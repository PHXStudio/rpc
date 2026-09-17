# 移除 skipcomp，修复跨语言互通的三个阻塞点

## 1. 目标与背景

### 背景

`(skipcomp)` 是 struct 的一个标记，其真实语义是「跳过与默认值的比较」，即**无条件写出所有字段**，
与默认的「值等于默认值就不写」压缩路径并存。它带来三个问题：

1. **跨语言不兼容**。只有 C++ 与 C# 生成器实现了该语义，Go 与 Python 生成器完全忽略它。
   同一个标记让两派产生长度不同的字节流（实测：C++ 12 字节 vs Python 8 字节，互读失败）。
2. **名实不符**。`skipcomp` 读起来像「跳过该字段」，实际是「跳过比较」，是持续的误读来源。
3. **仓库自己的两个跨语言 schema 恰好都声明了它**（`CrossLangTest.rpc`、`FullCrossLang.rpc`），
   使这两条本该验证互通的链路反而被它破坏。

排查过程中又确认了三个独立的互通阻塞点，均与 skipcomp 无关：

4. **C# 生成代码无法编译**。`CSGenerator.cpp` 输出 `bin.` 前缀，而 `runtime/cs/` 已是 `namespace rpc`。
5. **Go 整数宽度错误**。除 float/double 外所有整数一律按 8 字节写出，与其余三方错位。
6. **Go 的 bool 重复编码**。既写掩码位又写一个字节，而 C++/C#/Python 只写掩码位。

### 目标

- 移除 skipcomp 逻辑与语法，所有 struct 一律走「与默认值比较」的压缩路径。
- 修复上述三个阻塞点，使四种语言在默认模式下**真正互通**。

### 已确认的决策

`(skipcomp)` 语法**彻底移除**（旧 schema 解析报错，而非静默改变行为）。
理由是线格式本来就会变，报错比静默变换格式安全 —— 沉默的格式变更正是这个项目反复踩的坑。

---

## 2. 受影响文件清单

### 编译器（4 个文件）

| 文件 | 改动 |
|---|---|
| `compiler/rpc.y` | 删除 `opt_struct_flags` / `struct_flags` / `struct_flag` 三条规则，及 `structure` 规则中的引用 |
| `compiler/Struct.h` | 删除 `skipComp_` 成员与两处构造初始化 |
| `compiler/CppGenerator.cpp` | 删除 8 处函数签名中的 `skipComp` 形参及全部条件分支；**另需修复 bool 相关分支** |
| `compiler/CSGenerator.cpp` | 同上；**另需将 32 处 `bin.` 前缀改为 `rpc.`** |
| `compiler/GoGenerator.cpp` | 整数按声明宽度分派；**标量 bool 改为仅用掩码位** |

### Schema（3 个文件）

`tests/schema/CrossLangTest.rpc`、`FullCrossLang.rpc`、`FullTest.rpc` —— 删除 `(skipcomp)` 标记。

> ⚠️ 这三个 schema 必须与 `rpc.y` 同批修改，否则解析失败、全部测试挂掉。

### 测试（4 个文件）

- `tests/runtime/full_schema_test.cpp:171` —— 注释更新
- `tests/cs/CrossLangVerifier.cs:5`、`tests/cs/FullCrossLangVerifier.cs:5` —— `using bin;` → `using rpc;`
- `tests/csharp_serialization_check.cs:6` —— 同上（游离的历史验证程序）
- **新增** `tests/runtime/wire_format_golden_test.cpp` —— 见下方「为什么需要新测试」

### 文档（3 个文件）

`docs/knowledge-base.md`、`CLAUDE.md`、`tests/bug_fix_protocol_compat.md`（后者仅加过期声明）。

### 确认不受影响

`compiler/rpc.l`（无 skipcomp 关键字）、`compiler/Struct.cpp`（零引用）、
`compiler/PYGenerator.cpp` 与 `GoGenerator.cpp` 的 skipcomp（零引用）、
JSON 路径（`generateFieldSerializeJson` 等全程无 skipComp）、
四个 runtime 的 FieldMask API、`scripts/csv_to_rpc_schema.py`（从不生成 flags）、`README.md`。

---

## 3. 分步实现计划

### 步骤 A：移除 skipcomp

- [x] **A1** `compiler/rpc.y` —— 删除 `structure` 规则第 166 行的 `opt_struct_flags`，
      并删除 175–202 行的三条规则定义（含第 193 行误导性注释 `// skip serializtion compress.`）
- [x] **A2** `compiler/Struct.h` —— 删除第 37 行成员、第 19 与 24 行构造初始化
- [x] **A3** `compiler/CppGenerator.cpp`：
  - [x] 删除 8 处函数签名中的 `bool skipComp` 形参（`:334,:492,:650,:742` 等）
  - [x] 删除 `:529,:771` 的调用实参、`:813,:825` 的 `s->skipComp_` 实参
  - [x] 序列化侧：`:340,:381,:383,:396,:398,:402,:404` 的守卫改为无条件输出
  - [x] **删除整个 `FT_BOOL` 序列化分支 `:385-392`**（默认路径下它本就零输出，留空壳会误读）
  - [x] 反序列化侧：`:656,:701,:703,:707,:709,:725,:728,:732,:734` 同理
  - [x] `FT_BOOL` 反序列化 `:711-721`：删除 `else` 分支，只保留 `:715` 的 `x = __fm__.readBit();`
  - [x] **不动** `:497-525` 与 `:747-767` 的掩码块 —— 该块与 skipComp 无关，是协议契约
- [x] **A4** `compiler/CSGenerator.cpp`：同构改造（`:54,:99,:128,:140,:178,:220,:284,:296` 等）。
      **注意 C# 用 `f.indent()` / `f.recover()` 而非花括号**，删守卫时须保留配对的缩进调用
- [x] **A5** 三个 schema 删除 `(skipcomp)`；`full_schema_test.cpp:171` 注释更新

### 步骤 B：修复 C# 命名空间

- [x] **B1** `compiler/CSGenerator.cpp` —— 32 处 `bin.` → `rpc.`（纯字符串替换，不改变字节）
- [x] **B2** 三个 `.cs` 文件的 `using bin;` → `using rpc;`

### 步骤 C：修复 Go 整数宽度

- [x] **C1** `compiler/GoGenerator.cpp` —— 新增按 `EFieldType` 分派方法名的 static 辅助函数
      （与现有 `getFieldGoType` / `getFieldGoDefault` 并列）：`FT_INT64→WriteInt64`、
      `FT_UINT64→WriteUint64`、`FT_INT32→WriteInt32` … 读侧同理
- [x] **C2** 改造 4 个调用点：标量写出 `:400`、标量读入 `:636`、数组元素写出 `:294`、数组元素读入 `:531`
- [x] **C3** 读侧强转保留（`s.X = int32(v)`），类型与新方法返回类型匹配

> `uint64` 用 `WriteInt64(int64(v))` 会在超过 `MaxInt64` 时溢出变负，一并修正。

### 步骤 D：修复 Go 的 bool 编码

实测证据（schema `struct B { int32 n_; bool flag_; }`，值 `n_=1, flag_=true`）：

```
C++ : 01 c0 01 00 00 00                    (6 字节)
Go  : 01 c0 01 00 00 00 00 00 00 00 01     (11 字节，末尾多一个 bool 字节)
```

- [x] **D1** 标量 bool 写出（`GoGenerator.cpp:355`）—— 删除 `WriteBool` 调用，bool 仅由掩码位承载
- [x] **D2** 标量 bool 读入（`GoGenerator.cpp:586`）—— 由 `if fm.ReadBit() { ReadBool() }`
      改为 `s.X = fm.ReadBit()`。
      **注意**：不能用 `if fm.ReadBit()` 作守卫 —— 位本身就是值，值为 false 时守卫会跳过赋值
- [x] **D3** **数组元素 bool 不要动**（`:276` 写出、`:497` 读入）：C++ 侧 `array<bool>` 的掩码位
      表示 `size() > 0`，元素仍各占 1 字节，Go 现有行为是正确的

---

## 4. 验证与测试步骤

### 为什么需要新增测试

全仓库**没有任何测试断言 skipcomp / 非 skipcomp 的字节差异** ——
`full_schema_test.cpp`、`json_test.cpp`、`service_test.cpp` 等全部只做「序列化→反序列化→值相等」的往返断言。
这意味着本次改造**没有回归网**：如果改错了，现有测试仍会全绿。

因此新增 `tests/runtime/wire_format_golden_test.cpp`，固化字节级黄金向量：

- `struct B { int32 n_; bool flag_; }` 在 `n_=1, flag_=true` 下必须输出 `01 C0 01 00 00 00`
- 同结构在 `n_=0, flag_=false` 下必须输出 `01 00`
- 含嵌套 struct、数组、enum 的各一组，覆盖掩码位序与默认值省略

### 执行顺序

```bash
# 1. 重建编译器（不要用 build/ 里的既有二进制）
cmake --build build --target rpc

# 2. 三个 schema × 四个后端全部生成，确认无解析错误
for s in CrossLangTest FullCrossLang FullTest; do
  for g in cpp cs py go; do
    ./build/compiler/rpc -i tests/schema/$s.rpc -o /tmp/gen/$g/ -g $g || echo "FAIL $s $g"
  done
done

# 3. 生成代码必须能真编译（此前 Go 与 C# 均必然失败）
go build ./...
dotnet build tests/cs/CrossLangVerifier.csproj

# 4. 跨语言字节比对 —— 本次改造的核心验收
#    同一 schema、同样的字段值，C++ / Python / Go 三方输出应逐字节相同
#    向量须含：非零 int32、默认值字段、bool 字段（覆盖三条修复路径）

# 5. 回归
cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build
ctest --test-dir build --output-on-failure -L rpc
```

### 预期结果

| 检查项 | 改造前 | 改造后 |
|---|---|---|
| `CrossLangTest.rpc` C++ vs Python | 12 字节 vs 8 字节，互读失败 | 逐字节一致 |
| 同 schema 加入 Go | 长度不同 | 逐字节一致 |
| Go 生成代码 `go build` | 必然失败 | 通过 |
| C# 生成代码 `dotnet build` | 必然失败 | 通过 |
| `struct B` 的 bool 编码 | C++ 6 字节 vs Go 11 字节 | 三方一致 |

---

## 5. 兼容性影响

| 改动 | 线格式 | 说明 |
|---|---|---|
| A. 移除 skipcomp | **破坏性** | 声明过 `(skipcomp)` 的 struct 编码改变；因语法一并移除，旧 schema 会解析报错 |
| B. C# 命名空间 | 无 | 仅编译期符号 |
| C. Go 整数宽度 | **破坏性** | Go 侧线格式变更，旧版 Go 对端无法互通；其余三方不受影响 |
| D. Go bool 编码 | **破坏性** | 同上，含 bool 的 struct 受影响 |

三处破坏性变更都需在提交信息的 `BREAKING:` 段落中写明。

---

## 6. 文档同步

变更完成后按规范更新 `docs/knowledge-base.md`：

- §3 —— 删除「修饰符 `(skipcomp)`」小节
- §6 —— 「`(skipcomp)` 的真实语义」整节改写为「已移除」，保留其字节流对照作为历史说明
- §8 —— C# 行的 `bin.` 断裂标记移除
- §10 —— 兼容性矩阵重测：删除 skipcomp 列，整数与 bool 两行转为「一致」
- §12 —— 删除已修复的四条，保留 Python 嵌套 struct 掩码错位（本次不在范围内）

`CLAUDE.md`：规则二换例，规则四补记 C# 命名空间已修复，规则五删除 skipcomp 条目并补充 bool 说明。
`tests/bug_fix_protocol_compat.md`：历史记录不改写，文首加一行过期声明。

---

## 7. 完成记录

全部完成并验证。实际改动比计划多出三处，均为「让产物真能编译」所必需：

1. **Go 的 bool 重复编码**（计划已含，步骤 D）—— 实测确认。
2. **Go 服务相关的四处缺陷**：枚举元数据未加 `rpc.` 包前缀、方法分发器临时变量 `:=` 重复声明、
   Proxy 接口方法缺少返回值、派生服务分发器未委托基类。这些在修复前使 Go 的 service 产物根本无法编译。
3. **`tests/cs/TestMemReader.cs` 缺少 `Skip` 实现** —— 该方法是 FieldMask 提交引入接口时漏改的。

### 验证结果

| 检查项 | 改动前 | 改动后 |
|---|---|---|
| Go 产物 `go build`（三个 schema） | 必然失败 | 全部通过 |
| C# 产物 `dotnet build` | 必然失败 | 通过 |
| C++ / Python / Go 字节比对（CrossLangPayload） | 12 vs 8 字节，互读失败 | **逐字节一致**，可互读 |
| bool 编码（`struct { int32; bool }`） | C++ 6 字节 vs Go 11 字节 | **逐字节一致** |
| 回归测试 | — | 115/117 通过（2 个为既存失效用例） |

新增 `rpc_wire_format_tests`：字节级黄金向量，期望值先手工推导再与生成器核对。
