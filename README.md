# rpc

从 `.rpc` 模式文件生成 **C++ / C# / Python** 序列化代码的命令行编译器，并附带二进制协议读写运行时与测试。

## 依赖

| 组件 | 用途 |
|------|------|
| CMake ≥ 3.16 | 构建 |
| C++11 编译器 | `rpc` 可执行文件与测试 |
| **Bison**、**Flex** | 解析器生成（`compiler/bin.y`、`compiler/bin.l`） |
| GoogleTest | 测试（未安装时 CMake 会通过 FetchContent 拉取） |
| [.NET SDK](https://dotnet.microsoft.com/download)（可选） | 跨语言（C++ ↔ C#）集成测试 |

## 构建

```bash
cmake -S . -B build
cmake --build build
```

### Windows（Visual Studio）

1. 安装 **CMake**、带 C++ 工作负载的 **Visual Studio**，以及 [**WinFlexBison**](https://github.com/lexxmark/winflexbison/releases)（或同等 **Flex/Bison**，并确保在 `PATH` 中，或在 CMake 里设置 `BISON_EXECUTABLE` / `FLEX_EXECUTABLE`）。
2. 可选：安装 [.NET SDK](https://dotnet.microsoft.com/download)，用于跨语言测试；若仅编译 `rpc` 可忽略。
3. 生成与编译示例：

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -L rpc
```

说明：

- 多配置生成器（VS）下请为 **ctest** 指定 **`-C Debug`**（或 `Release`），与 `--configuration` 一致。
- **`-o` 输出目录**：程序会自动补全末尾路径分隔符；仍建议使用 `out\` 或 `out/`。
- 若 CMake 找不到 `dotnet`，可设置 `-DRPC_TEST_DOTNET="C:/Program Files/dotnet/dotnet.exe"` 后再配置。

安装编译器（可选）：

```bash
cmake --install build --prefix /your/prefix
```

生成的可执行文件名为 **`rpc`**。

## 用法

```text
rpc -i <输入文件> -o <输出目录/> -g <后端>
```

- **`-i`**：模式文件路径（例如 `schema.rpc`）。
- **`-o`**：输出目录；若省略末尾 **`/`** 或 **`\`**，程序会自动补上（便于 Windows 下使用）。
- **`-g`**：代码后端，当前支持 **`cpp`**、**`cs`**、**`py`**。

示例：

```bash
./build/compiler/rpc -i bin/Example.rpc -o ./out/ -g cpp
```

（具体可执行文件路径取决于生成器与是否安装；多配置工程下可能在 `build/compiler/Debug/rpc` 等位置。）

## 仓库布局

| 路径 | 说明 |
|------|------|
| `compiler/` | `rpc` 编译器（词法/语法、各语言生成器） |
| `runtime/cpp/` | C++ 协议读写头文件 |
| `runtime/cs/` | C# 运行时源码 |
| `tests/` | GoogleTest：内存序列化 + 可选的跨语言文件交换 |
| `bin/` | 示例模式（如 `Example.rpc`） |

## 测试

```bash
cd build
ctest --output-on-failure -L rpc
```

或使用构建工具的目标（如 `make test` / `ninja test`）。

- **内存序列化**等用例不依赖 .NET。
- **跨语言文件**用例在配置阶段能找到 `dotnet` 时启用：构建时会执行一次 `dotnet build` 生成 `tests/cs/CrossLangVerifier`，测试运行时通过 **`dotnet exec`** 调用已编译的 DLL（避免每次 `dotnet run` 触发完整 MSBuild 导致耗时或看似卡死）。
- 若 CMake 找不到 `dotnet`，对应用例会 **跳过**（不会失败）。
- 若在仅装有较新运行时（例如仅有 .NET 10）的机器上测试，项目已配置 **`RollForward`**，便于在缺少 .NET 6 共享框架时仍能运行验证程序。

指定 .NET 可执行文件（可选）：

```bash
cmake -S . -B build -DRPC_TEST_DOTNET=/path/to/dotnet
```

## 模式文件说明

- 可使用 **`#import`** 引入其他定义文件（示例见 `bin/Example.rpc`）。
- 可定义 **struct**、**service**、**enum** 等；具体语法以 `compiler/bin.y` 与示例为准。

