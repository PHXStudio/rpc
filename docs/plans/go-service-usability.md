# Go service 可用性修复 + C#/Go 通信协议测试

## 1. 目标与背景

### 起因

排查「Go 的通信协议能不能用」时，用生成的 `FullTest.go` 实测了 Stub → Dispatcher 的完整收发，
发现**三个**各自独立、都影响 Go service 可用性的问题。前两个此前完全未知。

### 问题清单

#### 问题一：Go 的 `ProtocolReader` 接口缺 `Skip`，跨版本时静默错位（已知）

`compiler/GoGenerator.cpp:790` 与 `:1135` 生成的版本兼容代码写作：

```go
if reader, ok := reader.(*rpc.MemReader); ok {
    if err := reader.Skip(uint32(actualFmLen - readFmLen)); err != nil {
```

只有 `*rpc.MemReader` 会执行跳过；换成流式 / socket 读取器时断言失败，**跳过被静默丢弃**，
其后所有参数的偏移全错且不报错。

实测（对端 schema 更新、掩码 1 字节 → 2 字节）：

| 场景 | 结果 |
|---|---|
| 同版本 + `*rpc.MemReader` | ✅ `user` / `pass` |
| 同版本 + socket 形态读取器 | ✅ `user` / `pass` |
| 跨版本 + `*rpc.MemReader` | ✅ `user` / `pass` |
| **跨版本 + socket 形态读取器** | ❌ `""` / `"user"` —— 参数错位，无错误 |

根因在运行时而非生成器：`runtime/go/protocol.go` 的 `ProtocolReader` 接口上没有 `Skip`，
只有具体类型 `MemReader` 有，生成器于是用类型断言绕开。
C++ 的 `ProtocolReader::skip` 是纯虚函数、C# 的 `IReader` 接口上有 `Skip`，
所以另外两个后端在结构上不可能有这个缺陷。Python 是内存缓冲模型，不受影响。

#### 问题二：Go 生成的 service 方法全部未导出（本次新发现）

`compiler/GoGenerator.cpp:851` 用 `method.getNameC()` 原样输出方法名，schema 里写的是 `method1`，
于是 Stub 的方法与 Proxy 接口的方法名都是小写：

```go
func (s *ServiceBaseStub) method1(Username string, Password string) error
type ServiceBaseProxy interface { method1(Username string, Password string) error }
```

Go 的导出规则使**其他 package 既无法调用这些方法，也无法实现这个接口**。
也就是说 Go 的 service 只能在生成代码自己所在的 package 内使用，
对任何真实的客户端 / 服务端代码都是不可用的。同文件内的字段却经 `toGoFieldName` 首字母大写
（`Username string`），方法名与字段名规则不一致。

C++ / C# / Python 没有导出概念，不受影响。

#### 问题三：C# 与 Go 都没有任何 service 测试（本次新发现）

| 后端 | service 测试 |
|---|---|
| C++ | `tests/runtime/service_test.cpp`，含 `MethodPayloadGolden.*` 字节黄金向量 |
| Python | 无 |
| **C#** | **无** |
| **Go** | **无** |

C# 与 Go 的生成器都产出完整的 Stub / Proxy / Dispatcher，但从未被任何测试编译并执行过。
问题一与问题二正是这个缺口的直接后果——两个后端各自都有真实编译验证（`InteropVerifier`、
`GoInteropGolden`），但那些验证只覆盖 struct 序列化，不碰 service 路径。

### 目标

1. 修掉问题一与问题二，使 Go 的 service 在**跨 package、跨版本**的真实用法下可用。
2. 为 C# 与 Go 补上 service 通信测试，并把「跨版本 + 非内存读取器」变成常驻回归网。
3. 测试必须放在**外部测试包**（`package fulltest_test`），否则问题二在有测试的情况下依然不会被发现。

---

## 2. 受影响文件清单

### 修复

| 文件 | 改动 |
|---|---|
| `runtime/go/protocol.go` | `ProtocolReader` 接口新增 `Skip(n uint32) error` |
| `compiler/GoGenerator.cpp:786-800` | struct 反序列化：去掉类型断言，直接调 `reader.Skip` |
| `compiler/GoGenerator.cpp:1132-1145` | 方法参数反序列化：同上 |
| `compiler/GoGenerator.cpp:851` | Stub 方法名走 `toGoFieldName` |
| `compiler/GoGenerator.cpp:1082,1088,1068` | `dispatch%s` → 导出化命名，与上一条一致 |
| `compiler/GoGenerator.cpp:1180` | `handler.%s(` 调用点跟随改名 |
| `compiler/GoGenerator.cpp:1574 附近` | Proxy 接口方法名导出化 |
| `compiler/GoGenerator.cpp:1002` | 接口方法签名拼装处跟随改名 |

### 新增测试

| 文件 | 内容 |
|---|---|
| `tests/go/service/service_golden_test.go` | Go service：方法载荷黄金向量、同版本往返、**跨版本 + 流式读取器**、未知方法号 |
| `tests/go/service/go.mod.in` | 该 module 的 go.mod 模板 |
| `tests/cs/ServiceVerifier.cs` | C# service：方法载荷黄金向量、Stub → dispatch 往返、跨版本跳过 |
| `tests/cs/ServiceVerifier.csproj` | 引用生成的 `FullTest.cs` + `runtime/cs/*.cs` |

### 构建

| 文件 | 改动 |
|---|---|
| `tests/CMakeLists.txt` | FullTest 生成扩展到 cs / go；新增 C# service verifier target；新增 Go service module 与 ctest 条目 |
| `CLAUDE.md` | 规则六的用例总数 171 → 173 |
| `docs/knowledge-base.md` | §12 更新问题一（补实测后果与触发条件）、新增问题二；§10 补 C#/Go service 覆盖说明 |

---

## 3. 分步实现计划与 Checklist

### 阶段一：修复 Go 运行时与生成器

- [x] `runtime/go/protocol.go`：`ProtocolReader` 加 `Skip(n uint32) error`，注释说明它是
      版本兼容路径的一部分，任何读取器都必须实现，不能靠类型断言可选提供
- [x] `compiler/GoGenerator.cpp` 两处生成代码去掉 `.(*rpc.MemReader)` 断言：
      ```go
      if err := reader.Skip(uint32(actualFmLen - readFmLen)); err != nil {
          return err
      }
      ```
- [x] 重新构建编译器，确认 `go build ./...` 通过（接口新增方法后，`MemReader` 已实现，无其他实现者）
- [x] 方法名导出化：Stub、Proxy 接口、`dispatch%s`、`handler.%s` 全部改走 `toGoFieldName`
- [x] 生成 `FullTest.go`，确认 `go vet` / `go build` 通过

### 阶段二：Go service 测试

- [x] 新建 `tests/go/service/`，含 `go.mod.in`（module `fulltest`）与 `service_golden_test.go`
- [x] 测试用 `package fulltest_test`（外部测试包）—— 这是问题二的回归守卫，
      同 package 测试永远发现不了未导出
- [x] 实现 `streamReader`：包装 `bytes.Reader` 的 `rpc.ProtocolReader`，
      **不是** `*rpc.MemReader`，模拟 socket 形态
- [x] 用例：
  - 方法载荷黄金向量，期望字节与 C++ `MethodPayloadGolden.*` 逐字节相同
    （`method5(1,2,true,EN2)` → `04 00 01 f0 01 02 01`；`method10()` → `09 00`；
    `method3(1.5,2.5,8,9)` → `02 00 01 f0 ...`）
  - Stub → Dispatcher 往返，覆盖标量 / 字符串 / struct / 数组
  - **跨版本 + `streamReader`**：对端掩码 2 字节、本端 1 字节，断言参数不串位
  - 同版本 + `streamReader`（对照组，证明只有跨版本路径曾出错）
  - 未知方法号 → `rpc.ErrUnknownMethod`
- [x] 先确认新用例在**修复前**会失败（否则它不构成回归守卫）

### 阶段三：C# service 测试

- [x] 新建 `tests/cs/ServiceVerifier.cs` 与 `.csproj`（仿 `InteropVerifier`）
- [x] Stub 是 abstract：子类实现 `methodBegin()` / `methodEnd()`，捕获字节
- [x] Proxy 是 interface：实现类记录收到的参数
- [x] 用例与方法载荷黄金向量同 Go（同一批字节，第三份独立实现）
- [x] 跨版本跳过：`TestMemReader` 已是 `IReader` 实现且带 `Skip`，构造掩码更长的流，
      确认 C# 侧不串位（预期本就正确，作为对照固化）

### 阶段四：接线与文档

- [x] `tests/CMakeLists.txt`：`_full_gen_cs` / `_full_gen_go` 与对应生成命令；
      **Go 产物名为 `FullTest.go`（大小写敏感，规则六的教训）**
- [x] Go 测试仿 `GoInteropGolden`，无 Go 工具链时注册为 `SKIP` 而非缺席
- [x] C# 测试仿 `InteropVerifier`，无 dotnet 时 `SKIP`
- [x] 更新 `CLAUDE.md` 规则六的用例总数
- [x] 更新 `docs/knowledge-base.md` §12 与 §10

---

## 4. 验证与测试步骤

```bash
# 1. 编译器重建（不要用 build/ 里的既有二进制）
cmake -S . -B build && cmake --build build --target rpc

# 2. 生成四个后端并逐一真实编译
./build/compiler/rpc -i tests/schema/FullTest.rpc -o out/ -g go
cd out && go build ./...

# 3. 全量测试（规则六）
scripts/docker-test.sh

# 4. 核对：用例总数 173、无 Skipped、Go 与 C# 两个新条目确实执行
```

**判定标准**

- 修复前：`TestDispatchNewerPeer_StreamReader` 失败（参数串位）
- 修复后：全部通过；用例数 173；容器内无 `Skipped`
- Go 产物在**外部测试包**下编译通过 —— 这是问题二已修复的唯一证据

**未覆盖 / 不做**

- Python service 测试不在本次范围（Python 无此两类问题），留作后续
- 真实跨进程 socket 交换仍超出容器能力，`streamReader` 是形态模拟而非真实 socket

---

## 5. 执行结果

写测试的过程又暴露出**两个计划外的缺陷**，都在 Go 的 service 路径上，共修四个：

| # | 缺陷 | 发现方式 |
|---|---|---|
| 1 | FieldMask 跳过被 `reader.(*rpc.MemReader)` 类型断言限死 | 计划内（此前实测） |
| 2 | service 方法名照抄 IDL，其他包无法调用 / 无法实现 Proxy | 计划内（写外部测试包时确认） |
| 3 | **方法参数中的数组被整体丢弃**，handler 永远收到 nil | 新增的往返用例失败 |
| 4 | **派生 service 的 stub 无构造函数**，无法从包外构造 | 写 `NewDerivedServiceStub` 时发现它不存在 |

第 3 条的机制：生成器对数组参数写 `Ints := make(...)`，而 `:=` 在 `if fm.ReadBit()` 块内新建变量，
遮蔽了函数顶部的 `var Ints []int32`——元素写进一个立即离开作用域的切片，外层仍是 nil。
非数组参数全部用 `=`，只有这一个分支写错。

**回归守卫的有效性已实测**：把生成产物的 `Skip` 改回旧的类型断言形式后，
`TestDispatchNewerPeer_StreamReader` 报 `got ""/"user"`，而同一批次的
`TestDispatchNewerPeer_MemReader` 与 `TestDispatchSameVersion_StreamReader` **仍然通过** ——
精确复现了「只有跨版本 + 非内存读取器会出错」这一特征。

**最终状态**：173 个用例，本机与 Docker 全部通过，容器内无 `Skipped`。
