# 测试套件修复与补全计划

> 状态：**阶段 0–1 与跨语言验证已完成**（2026-09-17）。阶段 2 部分完成，阶段 4 未开始。
> 已定决策：Python 字符串采用 **UTF-8 编解码**（与 C# 运行时对齐）；执行范围 **阶段 0–3**，阶段 4 工程链路另议。

## 执行结果

**CTest 用例：126 个（124 通过 / 2 失败）→ 150 个全部通过。**

完成情况：

| 阶段 | 状态 | 说明 |
|---|---|---|
| 0 前置缺陷修复 | ✅ | D1–D9 全部修复并实测确认；另发现并修复 Python `array<bool>` 读取（D8）与 `boolWriter`（D9） |
| 1 清理失效资产 | ✅ | 删除 4 个废弃文件与 6 个空目录；越界 `skip` 断言已先行迁入 `rpc_runtime_edge_tests` |
| — `#import` 修复 | ✅ | **计划外**：实测发现四端全部不可用，经确认后修复（定义摊平 + 移除悬空引用） |
| 2 补齐零覆盖 | ✅ | 空 struct、编译器负例、`#import`、`skip` 边界、`#< #>`/`#{ #}` 代码注入、多字节掩码、`dynSize`/整数/浮点字节 golden、JSON 文本 golden |
| 3 跨语言验证 | ✅ | `InteropFull.rpc` + 四端共享黄金向量（C++/C#/Python/Go），每端都真正运行产物 |
| 4 工程链路 | ✅ | Docker 补 python3 与 Go 1.21；工具链缺失时用例注册后跳过（实测确认报告为 Skipped 而非消失） |

**`enum Name : <底层类型>` 未按原计划"补测试"** —— 实测发现该语法没有任何可用形态
（不接受成员列表，且裸声明会被静默丢弃），已改为两条负例测试记录当前行为，并记入
知识库 §12。补全或移除该特性需要设计决策，不在本次范围。

新增测试目标：`rpc_compiler_negative_tests`、`rpc_import_tests`、`rpc_runtime_edge_tests`、
`rpc_interop_crosslang_tests`、`GoInteropGolden`、`PythonInteropGolden`。

修复过程中新增发现的缺陷（原计划未列）：C# `CS1522`（空 struct 空 switch）、
Python `array<bool>` 恒真、Python `boolWriter` 追加 `str`。

## 1. 目标与背景

### 背景

对 `main`（`3ae537f`）的测试套件做了一次完整清点与实测，结论是：**126 个 CTest 用例中 124 个通过，但「全绿」掩盖了真实缺陷**。

三类问题：

1. **测试替缺陷打掩护** —— 用例为了通过而绕开已知问题，导致缺陷长期不可见。
2. **结构性零覆盖** —— 若干已实现的功能一行测试都没有。
3. **后端覆盖极不均衡** —— 四语言「同一 schema 产出同一字节流」是项目核心契约（CLAUDE.md 规则一/二），但自动化验证只覆盖 C++↔C#，且仅限两个简单 schema。

### 本次实测确认的缺陷（非推断）

| # | 缺陷 | 证据 | 严重度 |
|---|---|---|---|
| D1 | Python `stringWriter` 往缓冲区追加 `str`，导致 `b''.join(buf)` 抛 `TypeError` | 实测 `StructBase{string_='hello'}` → 缓冲区含 `str`；`b"".join` 报 `sequence item 7: expected a bytes-like object` | **高**，任何含 `string` 的 schema 产物不可用 |
| D2 | Python 空 struct 生成 `__init__(self):` 空函数体 → `IndentationError`，整个文件不可导入 | 实测 `ast.parse` 失败；C++/Go/C# 同一声明均正常 | **高** |
| D3 | Python 空 struct 额外写 1 字节掩码，与其余三端（0 字节）线格式不一致 | 生成代码静态可见（`_b_.append(struct.pack('B', 0))`）；因 D2 无法动态验证 | **高**（线格式） |
| D4 | C# `DerivedStruct.FID` 隐藏 `StructBase.FID` → `CS0108` | 实测 `dotnet build FullTest.cs` 报 1 warning | 中 |
| D5 | `runtime/go/` 约 40 个测试因 `fm.Pos undefined` 整个包编译失败 | 实测 `go test ./...` → `build failed` | **高**（Go 运行时零覆盖） |
| D6 | Python `stringReader` 返回 `bytes` 而 `stringWriter` 收 `str` —— 读写类型不对称 | 源码 `return b[p:p+l]` vs `b.append(v)` | 中（与 D1 同源） |
| D7 | `stringReader`/`enumReader` 使用裸 `raise` | `runtime/py/rpc/reader.py:67,74` —— 实际抛 `RuntimeError: No active exception to re-raise` | 中 |

### 与既有文档的关系

- D5 与知识库 §12「Go 测试套件无法编译」一致，本次重新实测确认仍然成立。
- 知识库 §12「Python 运行时是 Python 2 风格」的描述**部分过时**：多数 writer 已改用 `struct.pack`，但**字符串路径仍是 `str`**（即 D1/D6），该条应细化而非删除。
- CLAUDE.md 规则二与知识库 §8 关于「Go/Python 方法载荷不带 FieldMask」的描述已过时（`1e2f637` 已修），本次一并更正。

### 目标

1. 删除失效、废弃、孤儿测试资产。
2. 补齐零覆盖的关键功能，优先补「能暴露 D1/D2/D3 这类缺陷」的用例。
3. 建立跨语言字节级验证，覆盖 **C++↔C#↔Python↔Go** 四个方向。
4. 让测试套件在「产物不可编译/不可运行」时报错，而不是静默跳过。

### 非目标

- 不重构编译器架构，不新增 IDL 语法特性。
- 不删除 `master` 分支相关资产（另一条产品线）。
- 不处理 `conn/`（ACE 遗留死代码，不在任何构建中）。

---

## 2. 受影响文件清单

### 新增

| 文件 | 用途 |
|---|---|
| `tests/schema/Imported.rpc` | `#import` 的被导入方 |
| `tests/schema/ImportRoot.rpc` | `#import` 的根文件 |
| `tests/schema/Edge.rpc` | 空 struct、`enum : 底层类型`、>8 字段（多字节掩码） |
| `tests/runtime/runtime_edge_test.cpp` | 运行时边界：`skip` 负例、`dynSize` 边界 golden、截断输入 |
| `tests/runtime/import_test.cpp` | `#import` 行为（替换 `compiler_test.cpp` 中的失效用例） |
| `tests/runtime/compiler_negative_test.cpp` | 编译器负例：语法错误、语义错误必须非零退出 |
| `tests/py/crosslang_py_test.py` | Python ↔ C++ 字节级互操作（读 C++ 产物 / 产出供 C++ 校验） |
| `tests/go/crosslang_go_test.go` | Go ↔ C++ 字节级互操作 |

### 修改

| 文件 | 变更 |
|---|---|
| `runtime/py/rpc/writer.py` | 修 D1：`stringWriter` 编码为 bytes |
| `runtime/py/rpc/reader.py` | 修 D6/D7：`stringReader` 返回 `str`、裸 `raise` 改为具名异常 |
| `compiler/PYGenerator.cpp` | 修 D2/D3：空 struct 守卫 |
| `compiler/CSGenerator.cpp` | 修 D4：嵌套 `FID` 加 `new` |
| `runtime/go/fieldmask_test.go` | 修 D5：`fm.Pos()` → `fm.pos` |
| `tests/CMakeLists.txt` | 挂载新用例；修 `BUILD_TESTING` 顺序；Go/Python 接入 CTest |
| `CMakeLists.txt` | `include(CTest)` 提到 `if(BUILD_TESTING)` 之前 |
| `tests/runtime/compiler_test.cpp` | 删除指向 `bin/` 的两个用例 |
| `tests/runtime/wire_format_golden_test.cpp` | 补 `dynSize`/`int16`/`uint16` golden |
| `tests/runtime/json_test.cpp` | 补硬编码 JSON golden |
| `Dockerfile` | `linux-tester` 阶段安装 `python3`、`golang` |
| `CLAUDE.md` | 更正规则二中已过时的「Go/Python 不带 FieldMask」表述 |
| `docs/knowledge-base.md` | §8/§11/§12 同步（按规则：每次变更后更新） |

### 删除

| 文件 | 理由 | 前置动作 |
|---|---|---|
| `tests/cpp_serialization_check.cpp` | 废弃，不在任何 CMakeLists，`#include` 不存在的 `output_cpp/` | 无（内容无独有覆盖） |
| `tests/version_compat_simple.cpp` | 废弃，从未构建 | **先抢救 `testSkipFunction` 的越界 skip 负断言** |
| `tests/version_compatibility_test.cpp` | 废弃，从未构建 | **先抢救 `testSkipFunctionality` 的版本兼容流程** |
| `tests/cs/csharp_serialization_check.cs` | 孤儿，无 csproj/CMake 引用；失败分支仍 `return 0` | 无 |
| `tests/csharp_test_data.bin` | 上述废弃程序跑出的残留物 | 无 |
| `tests/runtime/go_test/` | 空目录残留 | 无 |

> **删除的前提**：`version_compat_simple.cpp` / `version_compatibility_test.cpp` 包含**全套用例中唯一**的「越界 `skip` 必须失败」负向断言。必须先将其搬入 `tests/runtime/runtime_edge_test.cpp`，验证通过后再删文件。

---

## 3. 分步实现计划与 Checklist

### 阶段 0 — 前置缺陷修复

> 补测试会立刻暴露 D1–D5，测试必红。**必须先修**，否则无法区分「测试写错」与「产品有问题」。

- [ ] **0.1 修 D5（Go 测试编译）** —— `runtime/go/fieldmask_test.go` 的 4 处 `fm.Pos()` 改为直接访问 `fm.pos`（该字段为小写）。改完 `go test ./...` 必须全绿；若仍有失败，逐个甄别是测试数据错还是实现错（知识库 §12 记载 `dynSize` 测试数据本身写错，`0x40` 的期望值应为 `40 40`）。
- [ ] **0.2 修 D1/D6/D7（Python 运行时）** —— `stringWriter` 将 `str` 按 **UTF-8** 编码为 `bytes`（与 C# 运行时一致：`runtime/cs/ProtocolWriter.cs` 用 `Encoding.UTF8`）；`stringReader` 对称地解码回 `str`；两处裸 `raise` 改为 `raise ValueError(...)`。写入 `bytes` 时保持原样透传，保证与 C++/Go 的「原始字节」语义兼容。
- [ ] **0.3 修 D2/D3（Python 生成器）** —— `PYGenerator` 的 struct 序列化块加 `if (fields_.size())` 守卫，与 C++/C#/Go 生成器对齐：空 struct 不写掩码段，且 `__init__` 至少生成 `pass`。
- [ ] **0.4 修 D4（C# 生成器）** —— 派生 struct 的嵌套 `FID` 枚举加 `new` 修饰符。
- [ ] **0.5 端到端确认** —— 用 `FullTest.rpc` 与新增 `Edge.rpc` 对四端各生成一次，编译 + 运行，比对字节。

### 阶段 1 — 清理失效资产

- [ ] **1.1** 新建 `tests/schema/Imported.rpc` 与 `ImportRoot.rpc`，在 `tests/runtime/import_test.cpp` 中覆盖：正常导入、被导入类型可用、输出摊平到根文件名、**导入文件缺失时进程退出（非抛错）**。
- [ ] **1.2** 从 `compiler_test.cpp` 删除 `ImportRpcCpp` / `ExampleRpcCpp`（指向已删除的 `bin/`）。
- [ ] **1.3** 建 `tests/runtime/runtime_edge_test.cpp`，先迁入两个 skip 断言（越界必须失败、版本兼容跳过多余掩码字节后仍能读到后续字段）。
- [ ] **1.4** 确认 1.3 通过后，删除 `version_compat_simple.cpp`、`version_compatibility_test.cpp`、`cpp_serialization_check.cpp`、`csharp_serialization_check.cs`、`csharp_test_data.bin`、`tests/runtime/go_test/`。
- [ ] **1.5** 移除 `tests/CMakeLists.txt` 中产出 `generated/go/fulltest.go` 但无人消费的 `add_custom_command`（Go 测试实际用的是运行期重新生成到 `compiler_output/go/` 的那份）。

### 阶段 2 — 补齐零覆盖

- [ ] **2.1 空 struct** —— `Edge.rpc` 加 0 字段 struct；四端生成 + 编译 + 断言产出 **0 字节**（守卫 D2/D3 回归）。
- [ ] **2.2 编译器负例** —— `compiler_negative_test.cpp`：语法错误（`struct Foo (x) {}`）、重复定义名、重复字段名、未知类型、自继承、用 service 作字段类型，**均须非零退出**。这是当前完全空白的一类。
- [ ] **2.3 `#< #>` / `#{ #}` 代码注入** —— 仅 C++ 后端支持，验证片段被原样插入头文件（含 struct 级与文件级两种）。
- [ ] **2.4 `enum X : 底层类型`** —— `Edge.rpc` 覆盖；记录四端行为（C++ 原样、C# `: long`、Go 硬编码 `int32`、Python 忽略），线上一律单字节。
- [ ] **2.5 运行时边界 golden** —— `runtime_edge_test.cpp` 钉死 `dynSize` 的 `0x3F/0x40/0x3FFF/0x4000` 精确字节（当前只做往返，`40 40` 与 `40 00` 都能量通过）；补 `int16`/`uint16` 的字节 golden；补截断/超长长度前缀等畸形输入。
- [ ] **2.6 JSON golden** —— `json_test.cpp` 现有 62 例全是自洽/子串断言，补硬编码期望 JSON 文本，覆盖字段名、double 精度、`uint64` 大值。

### 阶段 3 — 跨语言验证（重点）

> 目标：把「同一 schema 四个后端产出相同字节」从**人工约定**变成**自动化断言**，且四端互测。

- [ ] **3.1 扩充 schema** —— 现有 `CrossLangTest.rpc` 仅 3 字段、`FullCrossLang.rpc` 仅 7 字段，且都缺 `int64`/`double`/`float`/嵌套 struct/服务方法。新建 `tests/schema/InteropFull.rpc`，覆盖全部标量宽度 + 字符串 + 数组 + enum + 嵌套 struct。
- [ ] **3.2 四端字节 golden** —— 对同一份数据，四端各自序列化，与**同一组硬编码 hex** 比对（而非互相传值往返）。期望值手工推导后再与实现核对。
- [ ] **3.3 Python ↔ C++** —— 新建 `tests/py/crosslang_py_test.py`：读 C++ 写出的 `.bin` 并校验；写出供 C++ 校验的 `.bin`。接入 CTest，**依赖解释器时用 `add_test` 的条件注册改为显式失败或明确标记**，不再静默消失。
- [ ] **3.4 Go ↔ C++** —— 新建 `tests/go/crosslang_go_test.go`，同样的双向文件交换；由 CTest 驱动 `go test`（需生成 `go.mod` 指向 `runtime/go`）。
- [ ] **3.5 C# 覆盖扩展** —— 现有 verifier 只验两个简单 schema；补 `InteropFull.cs` 的编译验证，防止 D4 这类「从未编译过」的问题重现。
- [ ] **3.6 服务方法载荷跨语言** —— 当前只有 C++ 有 `MethodPayloadGolden`；补 Python/Go 侧对 `[methodId][fmLen][fmask][参数]` 的字节断言。

### 阶段 4 — 工程链路

- [ ] **4.1** `CMakeLists.txt`：把 `include(CTest)` 移到 `if(BUILD_TESTING)` 之前，消除「不传 `-DBUILD_TESTING=ON` 则 126 个用例全部不配置」的陷阱。
- [ ] **4.2** `Dockerfile` 的 `linux-tester` 阶段安装 `python3` 与 `golang` —— 实测 `ubuntu:22.04` 基础镜像**不含 python3**，当前 Docker 路径下 `PythonWireFormatGolden` 连注册都不会，跑出来是「全绿但 Python 从未验证」。
- [ ] **4.3** .NET/Go/Python 工具链缺失时的行为统一：**显式 skip 并打印原因**，而不是 `#if` 编译期删除（编译期删除会让用例在 ctest 列表里彻底消失）。
- [ ] **4.4**（可选）加 GitHub Actions 工作流跑 `scripts/docker-test.sh`，让上述保障真正生效。

---

## 4. 验证与测试步骤

每个阶段结束都必须执行：

```bash
# 1. 重新构建（禁止复用 build/ 里的旧二进制）
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build

# 2. 全量测试
ctest --test-dir build --output-on-failure -L rpc
```

**分阶段验收标准**：

| 阶段 | 验收 |
|---|---|
| 0 | `ctest` 全绿（当前 2 个失败用例在阶段 1 才删除，此阶段可暂留）；四端用 `Edge.rpc` 生成并编译通过；Python 端 `b''.join(buf)` 不再抛异常 |
| 1 | 用例总数下降但**零失败**；`ctest -N` 中不再出现 `SkippedNoDotnet` 之外的占位用例 |
| 2 | 新增用例能**抓住故意注入的回归**（改坏生成器后对应用例必须变红）—— 逐条验证，而非只看绿灯 |
| 3 | 四端互测全部通过；且**故意改坏任一后端**，跨语言用例必须失败 |
| 4 | 干净容器内 `docker build --target linux-tester` 后 `ctest` 报出的用例数与本机一致 |

**规则六要求的端到端验证**（涉及生成器/运行时改动时）：

1. `cmake --build build --target rpc` 重新构建编译器
2. 用真实 schema 生成四端代码
3. 分别编译（`go build`、`dotnet build`、`g++ -fsyntax-only`、`python3 -m py_compile`）
4. 实际交换字节并比对

**文档同步**：每次变更后更新 `docs/knowledge-base.md`（§8 类型映射、§11 测试组织、§12 已知问题清单都要动），并更正 CLAUDE.md 规则二中已过时的表述。

---

## 5. 待确认事项

1. **D1/D6 的编码选择**：`stringWriter`/`stringReader` 统一按 UTF-8 编解码（与 C# 对齐）。若你希望 Python 侧保持「原始字节」语义（与 C++/Go 对齐、由调用方自行编解码），方案需相应调整 —— 这会影响线格式之外的用户 API 形态。
2. **阶段 4.4（CI）** 是否纳入本次范围。
3. **执行顺序**：阶段 0 是硬前置；阶段 1–4 之间相对独立，可按你的优先级重排或裁剪。
