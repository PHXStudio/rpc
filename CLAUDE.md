# CLAUDE.md — 研发规范

`rpc` 是一个 schema 驱动的二进制序列化工具链：读入 `.rpc` 模式文件，为 **C++ / C# / Python / Go** 生成代码，并配套提供四份手写的二进制读写运行时。

> **深度资料见 [docs/knowledge-base.md](docs/knowledge-base.md)** —— 线格式字节级规格、类型映射矩阵、跨语言兼容性实测结论、已知问题清单。本文只写**规则**，不重复那里的内容。

## Planning & Task Execution
- 面对复杂功能、重构或多步骤任务时，禁止直接修改代码
- 必须先在 `docs/plans/` 目录下生成或更新对应的 Markdown 计划文档
- 计划文档应包含：
  1. 目标与背景
  2. 受影响文件清单
  3. 分步实现计划与 Checklist
  4. 验证与测试步骤
- 等待用户审查确认该 Plan 后，再逐步执行代码变更。
- 每次变更之后都要更新 `docs/knowledge-base.md`

---

## 1. 常用命令

```bash
# 构建编译器（唯一产物：build/compiler/rpc）
cmake -S . -B build
cmake --build build --target rpc

# 构建并运行测试（BUILD_TESTING 必须显式传，否则 tests/ 不会被配置）
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure -L rpc

# 生成代码
./build/compiler/rpc -i tests/schema/FullTest.rpc -o out/ -g <cpp|cs|py|go>
```

---

## 2. 研发规范

### 规则一：线格式是四份实现的共同契约

这是本项目最重要的一条。线格式**没有单一权威定义**，它由 4 个生成器与 4 个运行时各自实现、靠约定保持一致。

> **任何线格式变更，必须同时落在全部四个生成器与全部四个运行时上，并附带跨语言字节级验证。**

只改一侧就会产生「同一份 schema、两套字节流」，而且**不会有任何测试失败**——现有测试绝大多数是各语言的自洽往返。

### 规则二：后端功能必须对等

四个生成器是彻底的复制-分叉结构（`CppGenerator` / `CSGenerator` / `PYGenerator` / `GoGenerator`，彼此不共享代码）。同一个特性漏实现一两个后端是**已经发生过的事**：

- `(skipcomp)` 曾只在 C++ 与 C# 中实现，Go 与 Python 完全忽略它，同一份 schema 因此产生两种字节流（该标记已从语法中移除）。
- 服务方法载荷至今仍不一致：C++ 与 C# 带 FieldMask 段，Go 与 Python 不带。

改动序列化逻辑时，务必确认四个后端对同一份输入产出相同字节——**现有测试的往返断言抓不到这类问题**。

- 新增 IDL 特性时，四个后端一并实现；确实无法支持的，必须在本文档与本提交信息中**显式标注**。
- 修改任一生成器后，用同一份 schema 对四个后端各生成一次并逐一 diff 检查。

### 规则三：生成的代码必须能被目标编译器编译

现有的 `rpc_go_generator_tests` 只对产物做**文本断言**，不编译。Go 与 C# 后端因此长期存在语法错误却无人察觉。

> 改动任一生成器后，必须对产物跑一次真实编译。

```bash
go build ./...                       # Go：未使用导入、复合字面量、方法签名、接收者前缀
dotnet build tests/cs/CrossLangVerifier.csproj -c Debug \
    -p:GeneratedCsDir=<生成的 cs 目录>   # C#
g++ -std=c++11 -fsyntax-only ...     # C++
python3 -c "import ast; ast.parse(open('X.py').read())"   # Python 语法
```

**Go 的产物还需要生成一份 `go.mod` 指向 `runtime/go`**（module `github.com/rpc/runtime`）才能编译。

### 规则四：命名一律为 `rpc`

`bin` 与 `arpc` 是历史命名，已在 `923d720` 统一为 `rpc`。该次统一当时**未改完生成器**，导致 C# 链路长期无法编译（已于后续修复）。新增代码、生成模板、注释中**不要重新引入这两个名字**：

| 位置 | 正确写法 |
|---|---|
| C# 运行时与生成代码 | `namespace rpc` / `rpc.ProtocolWriter` |
| Go 包与模块 | `package rpc` / `github.com/rpc/runtime` |
| Python 导入 | `from rpc.writer import *` |

### 规则五：跨语言兼容性是默认要求

新写或修改的 schema，默认应能在四种语言间互通。标量、字符串、数组、bool、枚举
以及服务方法载荷，均已验证四方输出逐字节一致，由 `rpc_wire_format_tests`
与 `service_test.cpp` 的 `MethodPayloadGolden.*` 守护。

已知**仍会破坏互通**的一处：

- **结构体中嵌套 struct 后由 Python 收发** —— Python 生成的嵌套 writer 不设置自己的掩码位，后续字段掩码整体前移一位。

改动线格式时，必须同步更新上述两组黄金向量。详情见知识库 §10 与 §12。

### 规则六：提交前必须端到端验证

单元测试全绿**不代表**链路可用。涉及生成器或运行时的改动，至少完成一次：

1. `cmake --build build --target rpc` 重新构建（**不要用 `build/` 里的既有二进制，它可能是过期的**）
2. 用真实 schema 生成目标语言代码
3. 编译生成的代码
4. 若涉及跨语言，实际交换一次字节流并比对

---

## 3. 代码风格

遵循 [.editorconfig](.editorconfig) 与 [.gitattributes](.gitattributes)：

- **编码 UTF-8，行尾 LF，文件末尾保留换行。**
- **C / C++ 源文件用制表符缩进**（`compiler/`、`runtime/cpp/` 一致遵循）。
- **`compiler/` 下的注释保持 ASCII-only**（英文），这是 `b62b649` 确立的约定，该目录目前 100% 遵守。
  `runtime/` 与 `tests/` 中已存在中文注释（如 `runtime/cpp/JsonHelper.h`），沿袭所在文件的现状即可。
- 文件头保留生成器写入的标识注释；**生成的文件不要手工修改**。

### 提交信息

采用 Conventional Commits：

```
feat:     新功能
fix:      缺陷修复
refactor: 重构（不改变行为）
test:     测试
chore:    构建、依赖、忽略规则
docs:     文档
```

正文用中文或英文均可，涉及协议变更时**必须说明兼容性影响**，例如：

```
fix: correct Go integer width in generated serialization

先前所有整数一律按 8 字节 int64 写出，与 C++/C#/Python 不兼容。
现改为按声明宽度分派。

BREAKING: Go 侧线格式变更，旧版 Go 对端无法互通。
```

---

## 4. 已知陷阱

排查问题时先看这里，能省下大量时间。

- **`build/` 里的 `rpc` 可能是过期的** —— 修改生成器后不重新构建，会得到与源码不符的产物。
- **`-g` 传未知值会静默回落到 `cpp`**，不会报错。生成结果不对时先确认后端名拼写。
- **产物文件名取自 schema 文件的主文件名**，不是其中定义的结构体名。
- **`ctest -N` 报 `Total Tests: 0`** 说明配置时没传 `-DBUILD_TESTING=ON`。
- **`#import` 失败会直接 `exit(1)`**，不是抛错——作为库调用时会导致宿主进程退出。
- **`tests/runtime/compiler_test.cpp` 中的 `ImportRpcCpp` / `ExampleRpcCpp` 必然失败**：它们引用的 `bin/` 目录已被删除。这是已知的失效用例，不是你的改动引入的。
- **`conn/` 是 ACE 遗留死代码**，不在任何构建中，不要试图启用。
- **`master` 分支是另一条产品线**（面向 Godot，没有 Go/Python 后端），不是 `main` 的历史版本；不要跨分支套用结论。

---

## 5. 文档

| 文件 | 内容 |
|---|---|
| [docs/knowledge-base.md](docs/knowledge-base.md) | 完整技术参考：线格式规格、类型映射、兼容性矩阵、已知问题清单（含原始字节证据） |
| [README.md](README.md) | 构建、安装、测试入门 |

关于文档的可靠性，有两点需要留意：

- README 描述的语言是 **C++ / C# / Python**，**未提及 Go**。`-g go` 后端已实现并有测试，属文档滞后而非功能缺失。
- README 的跨语言部分明确限定为 **C++ ↔ C#**，此描述与实际相符。但 `5125c61` 的提交信息宣称
  「Cross-language: ✅ All languages use same format」——该结论经实测**不成立**（见知识库 §10）。

**涉及四语言互通的判断，以知识库中的实测结论为准，不要采信提交信息或 README 之外的描述。**
