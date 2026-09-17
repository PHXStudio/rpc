# rpc

从 `.rpc` 模式文件生成 **C++ / C# / Python / Go** 序列化代码的命令行编译器，并附带四份手写的二进制读写运行时。

**四种语言在已知的全部维度上字节级一致**，可以互相通信：标量、字符串、数组、bool、枚举、服务方法载荷、嵌套 struct（含作为数组元素的情形）。结论与验证方式见 [docs/knowledge-base.md](docs/knowledge-base.md) §10。

## 下载发行版本

每次提交到 `main` 都会自动产出三份二进制，且只在全量测试通过后才产出：

| 平台 | 产物 |
|---|---|
| Linux x86_64 | `rpc-linux-x86_64` |
| Windows x86_64 | `rpc-windows-x86_64.exe` |
| macOS arm64 | `rpc-macos-arm64` |

**固定链接**是 [`latest` 预发布](https://github.com/PHXStudio/rpc/releases/tag/latest)，每次提交刷新，附 `SHA256SUMS`：

```bash
curl -fL -o rpc \
  https://github.com/PHXStudio/rpc/releases/download/latest/rpc-linux-x86_64
chmod +x rpc

# 确认它是哪个提交构建的——这是下载二进制后第一件该做的事
./rpc --version        # → rpc 1.0.43-a0a058e
```

仓库若为私有，上面的链接需要认证，改用 `gh release download latest -R PHXStudio/rpc`。

每次提交另有一份独立的 workflow artifact（名为 `rpc-<版本>`，保留 90 天），适合锁定某个具体提交。

> `latest` 是一个**被反复强推、始终指向 main 最新提交**的 tag。
> 如果你习惯 `git fetch --tags`，会看到它不断变化——这是滚动发布的设计，不是异常。
> 需要长期固定的版本请用 per-commit artifact，或自行打 `v*` tag。

## 依赖

| 组件 | 用途 |
|------|------|
| CMake ≥ 3.16 | 构建 |
| C++11 编译器 | `rpc` 可执行文件与测试 |
| **Bison**、**Flex** | 解析器生成（`compiler/rpc.y`、`compiler/rpc.l`） |
| GoogleTest | 测试（未安装时 CMake 会通过 FetchContent 拉取） |
| [.NET SDK](https://dotnet.microsoft.com/download)（可选） | 跨语言（C++ ↔ C#）集成测试 |
| Go 工具链（可选） | 编译并运行生成的 Go 代码 |
| Python 3（可选） | 运行生成的 Python 代码 |

macOS 自带的 `bison` 2.3 与 `flex` 即可构建，无需 Homebrew 版本。

## 构建

```bash
cmake -S . -B build
cmake --build build --target rpc
# 产物：build/compiler/rpc
```

`build/` 是完整构建目录；只要编译器本体时用上面的 `--target rpc`。

### Windows（Visual Studio）

1. 安装 **CMake**、带 C++ 工作负载的 **Visual Studio**，以及 [**WinFlexBison**](https://github.com/lexxmark/winflexbison/releases)（或同等 **Flex/Bison**，并确保在 `PATH` 中，或在 CMake 里设置 `BISON_EXECUTABLE` / `FLEX_EXECUTABLE`）。
2. 可选：安装 [.NET SDK](https://dotnet.microsoft.com/download)，用于跨语言测试；若仅编译 `rpc` 可忽略。
3. 生成与编译：

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure -L rpc
```

多配置生成器（VS）下请为 **ctest** 指定 **`-C`**（与 `--config` 一致）。

安装编译器（可选）：

```bash
cmake --install build --prefix /your/prefix
```

## 用法

```text
rpc -i <输入.rpc> -o <输出目录/> -g <cpp|cs|py|go>
rpc --version | -v
```

| 选项 | 含义 | 注意 |
|---|---|---|
| `-i` | 模式文件路径 | 必填 |
| `-o` | 输出目录 | 末尾 `/` 或 `\` 会自动补全；**目录必须已存在**，程序不会自建 |
| `-g` | 代码后端 | `cpp` / `cs` / `py` / `go`；**传未知值会静默按 `cpp` 生成**，不报错 |
| `-v`、`--version` | 打印版本后退出 | 不需要 `-i`；输出形如 `rpc 1.0.43-a0a058e` |

示例：

```bash
mkdir -p out_cpp
./build/compiler/rpc -i tests/schema/FullTest.rpc -o out_cpp/ -g cpp
# → out_cpp/FullTest.h, out_cpp/FullTest.cpp, out_cpp/ServiceBaseMethods.h, ...
```

一次生成全部四端：

```bash
for g in cpp cs py go; do
  mkdir -p out_$g
  ./build/compiler/rpc -i tests/schema/FullTest.rpc -o out_$g/ -g $g
done
```

（可执行文件路径取决于生成器与是否安装；多配置工程下可能在 `build/compiler/Release/rpc`。）

## 把生成的代码接上运行时

生成的代码需要各自语言的运行时（`runtime/` 下四份）。**这是接入时最容易卡住的一步**：

| 后端 | 产物对运行时的引用 | 你需要做的 |
|---|---|---|
| C++ | `#include "ProtocolWriter.h"` | 编译时加 `-I runtime/cpp` |
| C# | `namespace rpc`（`rpc.ProtocolWriter` 等） | 把 `runtime/cs/*.cs` 与生成文件一起编译 |
| Python | `from rpc.writer import *` | 让 `runtime/py` 在 `PYTHONPATH` 上（包目录是 `runtime/py/rpc/`） |
| Go | `import "github.com/rpc/runtime"` | 见下，需要一份 `go.mod` 并用 `replace` 指向 `runtime/go` |

Go 的完整接入方式（module 名必须是 `github.com/rpc/runtime`）：

```
yourpkg/
  go.mod          # module yourpkg
  FullTest.go     # 生成的产物
```

```
// go.mod
module yourpkg

go 1.21

require github.com/rpc/runtime v0.0.0

replace github.com/rpc/runtime => /path/to/rpc/runtime/go
```

```bash
go build ./...
```

生成的代码里有 `Service*Stub` / `Service*Proxy` / `Service*Dispatcher` 三件套（Stub 发送、Proxy 接收、Dispatcher 分发）；
C++ 后端还会为每个 service 额外生成一份 `<Service>Methods.h`，便于把 handler 实现分散到多个编译单元。

## 常见陷阱

排查问题时先看这里，能省下大量时间。以下现象都实测过：

- **`-o` 目录不存在时不会自建。** 只打印 `failed to open file ""`（消息里的文件名是空的，极具误导性）并返回 1。先 `mkdir -p`。
- **`-g` 传未知值静默回落 `cpp`**，不报错也不警告。生成结果不对时先确认后端名拼写。
- **产物文件名取自 schema 文件的主文件名**，不是其中定义的结构体名，而且**区分大小写**：`FullTest.rpc` 产出 `FullTest.go`。macOS 文件系统不区分大小写，
  所以在本机写错大小写毫无症状，到 Linux 才炸。
- **`build/` 里的 `rpc` 可能是过期的**。改过生成器后必须重新 `cmake --build build --target rpc`，否则得到与源码不符的产物。
- **`#import` 失败会直接 `exit(1)`**，而不是抛错——把编译器当库调用时，这会终止宿主进程。
- **生成的文件不要手工修改**，重新生成会覆盖；文件头的标识注释也请保留。
- **`bin` / `arpc` 是历史命名**（已在 `923d720` 统一为 `rpc`），新增代码、脚本、注释里不要重新引入。

## 仓库布局

| 路径 | 说明 |
|------|------|
| `compiler/` | `rpc` 编译器（词法/语法、四个后端生成器） |
| `runtime/cpp/` | C++ 协议读写头文件 |
| `runtime/cs/` | C# 运行时源码 |
| `runtime/py/rpc/` | Python 运行时包 |
| `runtime/go/` | Go 运行时（module `github.com/rpc/runtime`） |
| `tests/schema/` | 示例模式（如 `FullTest.rpc`） |
| `tests/` | 测试：内存往返、字节级黄金向量、跨语言交换、各后端真实编译运行 |
| `scripts/` | 构建 / 测试 / 发布脚本 |
| `.github/workflows/` | 按提交产出发行版本 |

## 测试

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure -L rpc
```

全部用例挂在同一个 `rpc` label 下，所以 `-L rpc` 就是全量（当前 **176** 个）。

**推荐用容器跑**，环境与 CI 一致，且不会因缺少工具链而静默少跑几个后端：

```bash
scripts/docker-test.sh
```

注意事项：

- **`100% passed` 不等于跑全了。** 缺少 python3 时两个 Python 用例**根本不注册**，`ctest` 照样报 100%；缺少 Go 工具链时对应用例报 `Skipped`，同样算通过。请核对**用例总数**并**确认没有 `Skipped`**。
- 依赖 .NET / Go / Python 的用例在工具链缺失时**跳过而非失败**，因此它们仍以真实用例名注册，便于识别。
- 若 CMake 找不到 `dotnet`，可用 `-DRPC_TEST_DOTNET=/path/to/dotnet` 指定；本项目已配置 `RollForward`，仅装了较新 .NET 运行时也能跑。
- 跨语言用例通过**临时二进制文件交换**验证（C++ 写、C# 读并重编码），刻意不链接在一起。

## 模式文件说明

- 可定义 **struct**、**service**、**enum**；可继承；支持定长数组与 `bytes`。
- 可使用 **`#import`** 引入其他定义文件。
- 语法以 `compiler/rpc.y` 与 `tests/schema/` 下的示例为准，完整的语言参考见 [docs/knowledge-base.md](docs/knowledge-base.md) §3。

## 文档

| 文件 | 内容 |
|---|---|
| [docs/knowledge-base.md](docs/knowledge-base.md) | 完整技术参考：线格式字节级规格、类型映射矩阵、跨语言兼容性实测、已知问题清单 |
| [CLAUDE.md](CLAUDE.md) | 研发规范（改生成器、改线格式、发布流程前必读） |
