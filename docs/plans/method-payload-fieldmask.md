# 为 Go / Python 的方法载荷补上 FieldMask

## 1. 目标与背景

### 背景

RPC 调用的载荷目前在各语言间**不一致**：

```
C++ / C#    [methodId : uint16][fmLen : uint8][fmask : N 字节][参数…]
Go / Python [methodId : uint16][参数…]
```

C++ 与 C# 直接把方法参数当作一个 `FieldContainer` 序列化（与 struct 完全同构），
因此带掩码段；Go 与 Python 则是逐个参数裸写，没有掩码。

两端各自自洽，但字节流对不上 —— **服务调用目前无法跨语言**。这是上一轮修复
（`452b7f3`）后仅存的两处互通缺口之一。

### 目标

让 Go 与 Python 的方法载荷补上 `[fmLen][fmask]` 段，与 C++ / C# 对齐，
使服务调用可在四种语言之间互通。

### 对齐基准：C++ / C# 的行为

以 `CppGenerator.cpp` 的 `generateFieldContainerSerialize(f, &m, "w")` 为准，语义与 struct 完全相同：

- **参数有默认值时该参数不占字节**，由掩码位表达（与 struct 字段一致）。
- **bool 参数不再占字节**，值由掩码位承载 —— 这一点会改变 Go 现有的 bool 参数编码。
- **方法没有参数时，完全不写掩码段**，载荷只有 `[methodId]`。
  见 `CppGenerator.cpp:493` 与 `CSGenerator.cpp` 的 `if(!fc->fields_.size()) return;`。
  `FullTest.rpc` 中的 `method10()` 正是这种情形。

### 兼容性影响

**破坏性**：Go 与 Python 的服务线格式改变，旧版对端无法互通。
C++ 与 C# 不受影响。四项改动中这是最后一个破坏性变更。

---

## 2. 受影响文件清单

| 文件 | 改动 |
|---|---|
| `compiler/PYGenerator.cpp` | `generateServiceStubMethod` 写掩码；`generateServiceProxy` 的 `dispatchID` 读掩码 |
| `compiler/GoGenerator.cpp` | `generateStubMethods` 写掩码；`generateMethodDispatch` 读掩码；`generateFieldSerialize` / `generateFieldDeserialize` 的 `isMethod` 语义收窄 |
| `tests/runtime/wire_format_golden_test.cpp` | 新增方法载荷的字节级黄金向量 |
| `docs/knowledge-base.md` | §5、§10、§12 更新 |

`compiler/CppGenerator.cpp`、`compiler/CSGenerator.cpp`、四个 runtime、三个 schema
均**不需改动** —— 本次是让 Go/Python 向既有基准靠拢。

---

## 3. 分步实现计划

### 步骤 A：Python

Python 的运行时函数 `write()` / `read()` **本来就支持掩码**（`fm` 参数非 `None` 时按位判断），
问题只在于方法路径传的是 `None`。改动很小。

- [x] **A1** `generateServiceStubMethod`（`PYGenerator.cpp:158`）——
      在 `uint16Writer(_b_, id, None)` 之后插入掩码段，写法与 struct 一致：
      ```python
      _b_.append(struct.pack('B', N))     # N = m.getFMByteNum()
      _fm_ = FieldMaskWriter(N)
      _pfm_ = len(_b_)
      _b_.append('')
      ```
      参数写出改用 `_fm_`（原为 `None`），并在末尾回填 `_b_[_pfm_] = _fm_.write()`
- [x] **A2** `generateServiceProxy` 的 `dispatchID`（`PYGenerator.cpp:211`）——
      在 `uint16Reader` 之后插入掩码读取（`_actual_fm_len_` / `min` / `FieldMaskReader` /
      `skipReader`），参数读入改用 `_fm_`（原为 `None`）
- [x] **A3** **无参方法**（`m.fields_.size() == 0`）两个方向都**不写也不读**掩码段

### 步骤 B：Go

Go 的改动比 Python 大：`isMethod` 目前同时控制**访问器**与**守卫**两件事，
需要把它收窄为只控制访问器。

- [x] **B1** 收窄 `isMethod` 语义（`generateFieldSerialize` / `generateFieldDeserialize`）：
      `isMethod` 此后**只**决定访问器 —— 值位置用局部变量名（方法参数），
      赋值目标同理；其余分支一律与 struct 字段统一：
      - 守卫 `if len(X) > 0` / `if X != ""` / `if X != 默认值` **无条件生成**
      - **bool 参数改为仅占掩码位**（删除 `if(isMethod) { WriteBool }` 分支）
      - 读侧 `if fm.ReadBit()` 守卫与 `s.X = fm.ReadBit()` 的 bool 特例同样无条件生成
      当前 `isMethod` 的 30 处分支中，约 20 处属于"守卫/收尾"会被删掉，
      约 10 处属于"赋值目标"会保留。
- [x] **B2** `generateStubMethods`（`GoGenerator.cpp:932`）——
      在 `WriteUint16(methodId)` 之后插入掩码写出，复用 struct 的写法
      （`fmLen := uint8((N-1)/8+1)` → `WriteUint8` → `rpc.NewFieldMask(N)` →
      逐参数 `fm.WriteBit(...)` → `writer.Write(fm.Bytes())`）。
      掩码长度直接内联常量，方法没有 `FID<Struct>Max` 那样的常量可用。
- [x] **B3** `generateMethodDispatch`（`GoGenerator.cpp:1085` 附近）——
      在 `ReadUint16()` 之后插入掩码读取
      （`actualFmLen` / `min` / `fmBytes` / `rpc.FieldMask` / `SetBytes`），
      并把已存在的"方法参数包裹块作用域"逻辑保留
- [x] **B4** **无参方法**两个方向都不写不读掩码段

### 步骤 C：字节级黄金向量

- [x] **C1** 在 `tests/runtime/wire_format_golden_test.cpp` 中新增方法载荷用例。
      方法载荷无法直接用 `ProtocolBytesWriter` 触发（需要 Stub 实例），
      因此改为**直接构造期望字节并与 C++ Stub 的实际输出比对** ——
      参考 `tests/runtime/service_test.cpp` 中已有的 Stub 用法（它有可用的
      `methodBegin`/`methodEnd` 测试桩）
- [x] **C2** 至少覆盖：全默认值参数（掩码全 0，无载荷字节）、非默认值参数、
      bool 参数（验证不占字节）、无参方法（验证没有掩码段）

---

## 4. 验证与测试步骤

### 主验收：四语言方法载荷逐字节比对

现有的 `service_test.cpp` 只做 C++ 自洽往返，**抓不到跨语言差异**。必须新增跨语言比对：

```bash
# 1. 重建
cmake --build build --target rpc

# 2. 用同一份带 service 的 schema 生成四语言代码
./build/compiler/rpc -i tests/schema/FullTest.rpc -o /tmp/gen/cpp/ -g cpp
./build/compiler/rpc -i tests/schema/FullTest.rpc -o /tmp/gen/go/  -g go
./build/compiler/rpc -i tests/schema/FullTest.rpc -o /tmp/gen/py/  -g py
./build/compiler/rpc -i tests/schema/FullTest.rpc -o /tmp/gen/cs/  -g cs

# 3. 同一方法、同一组参数值，四方输出的字节流应完全相同
#    重点向量：
#      method1("user","pass")      定长字符串参数
#      method3(1.5, 2.5, 8, 9)     四个标量
#      method5(1, 2, true, EN2)    含 bool 与枚举
#      method10()                  无参方法 —— 只应有 methodId，无掩码段
```

### 生成代码必须能编译

```bash
go build ./...                                   # Go
python3 -c "import ast; ast.parse(open('X.py').read())"   # Python 语法
cmake --build build && ctest --test-dir build -L rpc      # C++ / 回归
```

### 回归

```bash
ctest --test-dir build --output-on-failure -L rpc
```

预期：除两个既存失效用例（`Compiler.ImportRpcCpp` / `Compiler.ExampleRpcCpp`，
引用已删除的 `bin/` 目录）外全部通过；
新增的黄金向量用例通过。

---

## 5. 风险点

1. **Go 的 `isMethod` 收窄是本次最大的改动面**（30 处分支）。
   收窄过头会把方法参数的守卫也带进 struct 路径，或反之。
   缓解：改动后必须对 struct 路径重跑黄金向量 —— 那些向量是上一轮刚建立、已验证通过的，
   任何回归都会立刻暴露。

2. **无参方法容易被忽略**。`FullTest.rpc` 的 `method10()` 正是这种情形，
   若给它写了掩码段就会与 C++/C# 错位。

3. **bool 参数的编码会变**。Go 侧此前 bool 参数写一个字节，改后只占掩码位；
   这与 struct 字段的既有语义一致，但对已有的 Go 服务对端是破坏性的。

---

## 6. 完成后的文档同步

- `docs/knowledge-base.md` §5 —— 删除「服务方法载荷目前跨语言不一致」的提示框，改为规格描述
- §10 —— 兼容性表删除「服务方法载荷」一列，两处未解决项减为一处
- §12 —— 删除「服务方法载荷在语言之间不兼容」条目，仅保留 Python 嵌套 struct
- `CLAUDE.md` 规则五 —— 已知破坏互通的写法从两条减为一条

---

## 7. 完成记录

全部完成。实际改动比计划多出一处 —— **C# 的明确赋值缺陷**，
它由本次改动暴露（计划的风险点里没有预见到）：

C# 的方法参数在分发器里声明为裸局部变量（`double d;`），赋值只发生在
`if(__fm__.readBit())` 守卫内部。此前 C# 的方法分发**没有掩码守卫**
（走的是 skipComp 路径），所以参数总会被赋值；守卫改为无条件生成后，
C# 的明确赋值规则开始拒绝读取可能未赋值的局部变量，报 `CS0165`。

修法：`CSGenerator` 新增 `getFieldCsDefault`，方法参数声明时带初始化
（数组 `new T[0]`、字符串 `""`、struct `new T()`、enum `(T)0`、其余 `0`），
与 struct 字段的初始化器风格一致。

该缺陷在 `tests/cs` 下无覆盖（跨语言验证器不含 service），因此长期未被发现。

### 验证结果

四语言对同一方法、同一组参数值输出**逐字节一致**：

| 向量 | 四方共同输出 |
|---|---|
| `method5(1, 2, true, EN2)` | `04 00 01 f0 01 02 01` |
| `method5(0, 0, false, EN1)` | `04 00 01 00` |
| `method10()` 无参 | `09 00` |
| `method3(1.5, 2.5, 8, 9)` | `02 00 01 f0 00 00 00 00 00 00 f8 3f 00 00 20 40 08 00 00 00 00 00 00 00 09 00 00 00 00 00 00 00` |

生成代码编译：Go `go build` 通过、C# `dotnet build` 通过、Python 语法通过。

回归：121 个用例中 119 通过，仅剩两个既存失效用例
（`Compiler.ImportRpcCpp` / `Compiler.ExampleRpcCpp`，引用已删除的 `bin/`）。

新增 `MethodPayloadGolden.*` 四个黄金向量用例，固定方法载荷的确切字节。
