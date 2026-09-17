# 每次提交生成一套发行版本

## 1. 目标与背景

### 起因

希望 main 上的每次提交都自动产出一套可下载、可分发、**可区分**的 `rpc` 二进制。

### 现状盘点

| 项 | 现状 |
|---|---|
| CI | **完全没有**，仓库无 `.github/` |
| 远程 | `git@github.com:PHXStudio/rpc.git`，主干 `main` |
| 构建 | [Dockerfile](../../Dockerfile) 已有 `linux-builder` / `windows-builder` 两级；`output` / `extract` 两级已负责收集产物 |
| 脚本 | [scripts/docker-build.sh](../../scripts/docker-build.sh) → `dist/rpc`；[scripts/docker-build-windows.sh](../../scripts/docker-build-windows.sh) → `dist/rpc.exe` |
| 版本信息 | **二进制里一点都没有** |

最后一条是本计划的重点。上一轮已实测确认：

- `RPC_VERSION_MAJOR/MINOR` 由 [Config.h.in](../../Config.h.in) 经 `configure_file` 生成到 `build/Config.h`，
  但**全仓库没有任何 `.cpp`/`.h` include 它**，编译器也没把它编进去；
- [`--version`](../../compiler/Main.cpp) 被 `Args` 静默忽略，退出码 0，无任何输出。

也就是说，如果只加 CI 不改代码，每次提交产出的 `rpc` 二进制**无法回答「我是哪个提交构建的」**。
一堆同名文件里挑不出新旧，发行版本就失去意义。所以本计划分两半：**先让产物可标识，再让 CI 自动产出**。

### 已确认的设计决定

| 维度 | 决定 |
|---|---|
| 版本号 | `1.0.<提交数>-<短sha>`；HEAD 正好在 tag 上时以 tag 名为准 |
| 发布形态 | 滚动 `latest` 预发布 + 每次提交一份 workflow artifact（保留 90 天） |
| 触发 | 仅 `main` 的 push；发布前必须通过全量测试门禁 |
| 平台 | Linux + Windows + macOS |

### 规划期实测到的三个约束

这三条会改变实现方式，不是推测：

1. **`.dockerignore` 排除了 `.git/`**（[.dockerignore](../../.dockerignore) 第 2 行）。
   容器内拿不到任何 git 元数据，所以版本号**必须由外部经 `--build-arg` 注入**，
   容器内的 git 探测只会得到 `unknown`。

2. **本机 `bison` 是 GNU Bison 2.3**（macOS 自带的 `/usr/bin/bison`），项目用它构建成功。
   GitHub 的 macOS runner 同样是系统 `/usr/bin/bison` + `/usr/bin/flex`，
   因此 **macOS 构建预期不需要 `brew install`** —— 但这一条只能在 CI 首跑时证实，
   列入验证步骤（见 §4）。注意 Homebrew 的 bison 是 keg-only 的，真需要装时还得改 `PATH`。

3. **滚动 `latest` tag 会污染版本号推导**。若用 `git describe --tags --exact-match HEAD` 取 tag，
   而 `latest` 恰好总是指向 main 的最新提交，**每次都会匹配到字面量 `latest`**，
   版本号就变成 `rpc latest`。必须加 `--match 'v[0-9]*'` 把滚动 tag 排除在外。

---

## 2. 受影响文件清单

### 版本注入（编译器）

| 文件 | 改动 |
|---|---|
| [CMakeLists.txt](../../CMakeLists.txt) | 由 git 推导 `RPC_VERSION_STRING` / `RPC_BUILD_COMMIT`，允许 `-D` 覆盖 |
| [Config.h.in](../../Config.h.in) | 新增 `RPC_VERSION_STRING`、`RPC_BUILD_COMMIT` |
| [compiler/Main.cpp](../../compiler/Main.cpp) | `#include "Config.h"`；`-v` / `--version` 短路输出 |
| [compiler/Args.h](../../compiler/Args.h) | 新增 `Has()` 判定；构造函数接受长选项表 |
| [compiler/Args.cpp](../../compiler/Args.cpp) | 实现上面两项 |

> `compiler/` 下的注释一律 ASCII 英文（`b62b649` 约定），制表符缩进。
> 本次改动**不触碰任何生成器，不触碰线格式**，四个后端完全不受影响 —— 规则一 / 规则二 / 规则三
> 在此不适用；规则六（全量测试）照常执行。

### 测试

| 文件 | 改动 |
|---|---|
| [tests/tests_config.h.in](../../tests/tests_config.h.in) | 新增 `RPC_TEST_VERSION_STRING`，供用例与二进制比对 |
| [tests/runtime/compiler_harness.h](../../tests/runtime/compiler_harness.h) | 新增 `runCompilerArgs()`：捕获子进程输出 |
| [tests/runtime/compiler_test.cpp](../../tests/runtime/compiler_test.cpp) | 新增 3 个用例 |

### 构建与 CI

| 文件 | 改动 |
|---|---|
| [Dockerfile](../../Dockerfile) | `linux-builder` / `windows-builder` 接受版本 `ARG` 并传给 CMake |
| [scripts/docker-build.sh](../../scripts/docker-build.sh) | 计算版本号并注入（否则本地产物退化为 `unknown`） |
| [scripts/docker-build-windows.sh](../../scripts/docker-build-windows.sh) | 同上 |
| `scripts/assert-test-report.sh` | **新增**：解析 ctest 输出，强制核对用例总数与「无 Skipped」 |
| `.github/workflows/release.yml` | **新增**：门禁 → 三平台构建 → 发布 |

### 文档

| 文件 | 改动 |
|---|---|
| [CLAUDE.md](../../CLAUDE.md) | §1 常用命令补 `--version`；规则六用例数 173 → 176 |
| [docs/knowledge-base.md](../knowledge-base.md) | §2 命令行补 `--version`；§11 补用例与发行说明；新增一节「版本与发行」 |
| [README.md](../../README.md) | 补「下载发行版本」一小节（含滚动 tag 强推的说明） |

---

## 3. 分步实现计划与 Checklist

### 阶段一：把版本编进二进制

- [ ] `CMakeLists.txt`：在现有 `RPC_VERSION_MAJOR/MINOR` 之后加入推导逻辑

  ```cmake
  # 版本号来源优先级：命令行注入 > git > 兜底。
  # .dockerignore 排除了 .git/，容器内与 CI 导出构建都必须靠注入。
  if(NOT RPC_VERSION_STRING OR NOT RPC_BUILD_COMMIT)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
      # --match 排除滚动 latest tag，否则每次都会匹配到字面量 "latest"
      execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match --match "v[0-9]*" HEAD
                      ... OUTPUT_VARIABLE _tag ERROR_QUIET RESULT_VARIABLE _rc)
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD ...)
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD ...)
    endif()
  endif()
  ```

- [ ] 版本串拼装：tag 命中则用 tag；否则 `1.0.<count>-<sha>`；两者都拿不到则 `1.0.0-unknown`
- [ ] 工作区有未提交改动时追加 `-dirty`（本机自建产物不应冒充某个干净提交）
- [ ] `Config.h.in` 增加两行宏；确认 `build/Config.h` 生成正确

### 阶段二：`-v` / `--version`

- [ ] **`Args` 需要一个「参数是否出现」的判定**。这是规划期发现的坑：
      `SetShortKey()` 会给 shortopts 里**每个字符**都建 Token，所以
      `GetToken('v')` **永远非 NULL**，不能拿它判存在；而 `GetCString('v')` 在
      `--version` 无值的情况下也永远返回 NULL。两者都不可用，必须新增：

      ```cpp
      bool Has(char shortkey);          // Token 存在且 Values 非空
      bool Has(const char* longkey);
      ```

- [ ] 构造函数增加第 4 个参数 `const char* longopts`（默认 `NULL`，不破坏既有调用）：
      形如 `";v=version;"`，配套新增 `void SetLongKey(const char* longopts)`。
      **长选项必须在 `Scan()` 之前注册** —— 现有构造函数是先 `SetShortKey` 再 `Scan`，
      而 `SetLongKey` 目前是**从未被调用过的死代码**，顺带让它派上用场。
- [ ] `Main.cpp`：`Args args(argc, argv, ";i;o;g;v;", ";v=version;");`
      随后**最先**处理版本分支：
      ```cpp
      if (args.Has('v')) {
          std::cout << "rpc " << RPC_VERSION_STRING << std::endl;
          return 0;
      }
      ```
      必须在参数校验与 `compile()` 之前短路 —— `--version` 不该需要 `-i`。
- [ ] 只注册 `version` 一个长选项。`--input` / `--output` / `--generator` 属于顺带可得但**不做**，
      避免混入无关的 CLI 面变更。

### 阶段三：测试（3 个新用例）

- [ ] `tests_config.h.in` 补 `RPC_TEST_VERSION_STRING`（`configure_file` 已存在，无需改 CMake）
- [ ] `compiler_harness.h` 新增 `runCompilerArgs(const std::string& args, std::string* output)`：
      `std::system()` 读不到子进程输出，需先重定向到文件再读回
- [ ] 用例：
  - [ ] `Compiler.VersionFlagIdentifiesTheBuild` —— `--version` 退出 0，且输出**逐字节等于**
        `rpc <RPC_TEST_VERSION_STRING>\n`
  - [ ] `Compiler.VersionShortFlagMatchesLongFlag` —— `-v` 与 `--version` 输出一致
  - [ ] `Compiler.VersionFlagSkipsCompilation` —— `--version -i /nonexistent.rpc` 仍退出 0，
        证明短路发生在读文件之前（若先编译会因文件不存在而失败）
- [ ] 先确认这三个用例在**改动前**会失败（否则它们不构成守卫）

### 阶段四：CI 工作流

- [ ] `scripts/assert-test-report.sh`：从 ctest 输出中强制核对
      **用例总数 == 176** 且 **无 `Skipped`**，否则非零退出。
      规则六两次点名「只看 `100% passed` 会漏掉整个后端」，这里把它变成机器判定。
- [ ] `.github/workflows/release.yml`：
      ```
      on: push(main) + workflow_dispatch
      concurrency: group=release-main, cancel-in-progress: false   # 中断发布可能留下半套产物
      permissions: contents: write

      job test      : scripts/docker-test.sh | tee report.txt → assert-test-report.sh
      job build-bin : needs test → buildx --target output --output type=local,dest=dist
      job build-mac : needs test → cmake 本机构建（macos-14，arm64）
      job publish   : needs [build-bin, build-mac] → artifact + 刷新 rolling latest
      ```
- [ ] **版本推导步骤**（每个构建 job 共用）：checkout 必须 **`fetch-depth: 0`** ——
      `git rev-list --count HEAD` 在浅克隆下恒为 1，版本号会静默变成 `1.0.1-xxxxxxx`
      且不报错。这正是本项目最忌讳的「错得很安静」。
- [ ] Linux/Windows 走 Docker，注入 `--build-arg RPC_VERSION_STRING=... RPC_BUILD_COMMIT=...`；
      macOS 直接传 `-DRPC_VERSION_STRING=... -DRPC_BUILD_COMMIT=...`
- [ ] 产物命名带版本：`rpc-linux-x86_64` / `rpc-windows-x86_64.exe` / `rpc-macos-arm64`，
      另附 `SHA256SUMS`
- [ ] 发布：
  - [ ] `actions/upload-artifact@v4`，`name: rpc-<version>`，`retention-days: 90`
  - [ ] 滚动预发布：先 `git tag -f latest && git push -f origin latest` 把 tag 移到本次提交
        （**必须在 upload 之前**，`gh release upload --clobber` 不会移动 tag），
        再 `gh release upload latest <all assets> --clobber` 一次性上传全部资产
  - [ ] 首次需 `gh release create latest --prerelease`；注意仓库 Settings → Actions → General
        的 workflow 权限需为 read/write，否则 `permissions: contents: write` 会被仓库设置压回只读

### 阶段五：文档

- [ ] `CLAUDE.md`：§1 命令表补 `./build/compiler/rpc --version`；规则六用例数 173 → 176
- [ ] `docs/knowledge-base.md`：§2 命令行补 `--version` 与版本号构成；§11 补新用例；
      新增「版本与发行」一节，写明版本号来源、注入方式、滚动 tag 的强推语义
- [ ] `README.md`：补「下载发行版本」小节

---

## 4. 验证与测试步骤

```bash
# 1. 重编译（不要用 build/ 里的旧二进制）
cmake -S . -B build && cmake --build build --target rpc
./build/compiler/rpc --version          # 期望：rpc 1.0.<提交数>-<sha>
./build/compiler/rpc -v                 # 同一行
./build/compiler/rpc --version -i /nonexistent.rpc; echo $?   # 期望 0

# 2. 全量测试（规则六）
scripts/docker-test.sh
#    核对：总数 176、无 Skipped、三个新用例确实执行

# 3. 容器内版本号不是 unknown（证明 build-arg 注入通了）
scripts/docker-build.sh && ./dist/rpc --version

# 4. CI 首次验证（workflow 无法本地跑）
#    先在临时分支上把 on.push.branches 加上该分支，workflow_dispatch 触发，确认：
#    - macOS runner 上 /usr/bin/bison 2.3 + /usr/bin/flex 可用（约束 2）
#    - 版本号不是 1.0.1-xxxxxxx（证明 fetch-depth: 0 生效）
#    - 版本号不是 "latest"（证明 --match 'v[0-9]*' 生效）
#    - 四个产物可下载、latest release 已刷新
#    验证通过后再合入 main
```

**判定标准**

- 本机与 Docker 全量测试 176 用例全绿、无 `Skipped`
- `--version` 输出与 `RPC_TEST_VERSION_STRING` 逐字节一致
- 容器内 `dist/rpc --version` 不是 `unknown`
- CI 首跑拿到的三个平台产物，其版本号都指向**触发它的那个提交**

**未覆盖 / 不做**

- Python 运行时产物（`runtime/py/`）不单独发行，随仓库分发
- macOS 只出 arm64；通用二进制（`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`）留作后续可选项
- Windows 产物仍是 MinGW 交叉编译，**未经真实 Windows 运行验证** —— 与现状一致，本次不改变
- 不引入 `--help`

---

## 5. 风险与已知缺口

| # | 风险 | 说明 |
|---|---|---|
| 1 | **可复现性缺口** | [CMakeLists.txt](../../CMakeLists.txt) 的 rapidjson 走 FetchContent 且 `GIT_TAG master`，上游一动，**同一提交在不同时间构建会得到不同二进制**。发行版本若要求可复现，需把 rapidjson 固定到具体 commit。本次只标注，不改动 |
| 2 | 产物不可复现的另一个来源 | `__DATE__` / `__TIME__` 会使构建带上时间戳。本计划**刻意不引入**，版本串里只有提交信息 |
| 3 | 每个提交都发布 | main 上高频提交会持续刷新 `latest`；per-commit artifact 保留 90 天，注意仓库存储配额 |
| 4 | 滚动 tag 的语义 | `latest` 会被反复强推，对 `git fetch --tags` 的人可能造成困惑，需在 README 说明 |
| 5 | macOS 工具链 | 系统 bison 2.3 / flex 的可用性待 CI 首跑证实（约束 2） |
| 6 | 发布中断 | 若 publish 中途失败，`latest` 可能短暂处于「新 Linux + 旧 macOS」的混合状态；下一次提交会自愈 |

---

## 6. 执行结果

计划中的五个阶段全部完成。实施过程中暴露了**两个计划外缺陷**与**三处计划本身的错误**，
一并记录——后者尤其值得留下，它们都是"看起来对、实际错"的形态。

### 计划外缺陷（已修）

| # | 缺陷 | 性质 |
|---|---|---|
| 1 | `Has()` 用「Values 非空」判定参数出现 | **本计划自己设计错了** |
| 2 | `rpc`（无参数）与 `rpc --help` 段错误，exit 139 | 既有缺陷，实施时发现 |

**第 1 条值得单独说**：计划的 §3 阶段二**已经写明**「`GetCString()` 在 `--version` 无值时永远返回
NULL，不可用」，却仍然把 `Has()` 设计成基于 `Values`——而 `--version` 恰恰是无值 flag，
于是 `Values` 恒空，`Has('v')` 恒假，`--version` 完全不生效。
真正需要的是一枚**由 `Scan()` 在匹配到选项时置位**的 `Seen` 标志。
**识别出陷阱不等于绕开了陷阱**——这是本次最该记住的一条。

**第 2 条的根因**：`Main.cpp` 把 `args.GetCString()` 返回的 `NULL` 直接赋给 `std::string`
（`generator_` / `inputFileName_` / `outDir` 三处，`Compiler.h:236-242`），未定义行为。
用 `git stash` 构建未修改的版本复现确认是**既有问题**，非本次引入。已加 NULL 守卫；
无参数时现在走到 `fopen("")` 失败并返回 1。

### 计划本身的错误（已纠正）

| # | 计划里写的 | 实际 |
|---|---|---|
| 1 | 「`--version` 静默退出 0」 | **exit 139 段错误**。上一轮测量时命令写在管道里，`$?` 取的是 `head` 的退出码。已更正知识库并记下「测退出码不要经过管道」 |
| 2 | 「本机验证容器产物版本号」 | 容器产物是 **Linux ELF，macOS 上 `exec format error`**。必须在 Linux 容器内执行：`docker run --rm -v "$PWD/dist:/d" rpc-linux /d/rpc --version` |
| 3 | 未预见 | `version.sh` 不加 `-dirty`，导致**同一工作区、同一「版本号」，本机构建带 `-dirty` 而 Docker 构建不带** —— 一个版本号对应两种产物，正是机制本身要防的事。已让 `version.sh` 也判脏，两条路径实测一致 |

### 实施中新增的两处守卫

- **`version.sh` 拒绝在浅克隆下输出**：`rev-list --count` 在浅克隆里恒为 1，
  版本号会安静地变成 `1.0.1-<sha>`。现在直接报错退出，把静默错误变成显式失败。
- **CI 的 `arch` 断言**：产物名硬编码 `x86_64`，若 runner 换了架构会**发出一个标着 x86_64 的
  arm64 二进制**。现在 `uname -m` 不匹配就失败退出。

### 另一个可移植性坑（已修）

门禁脚本起初用 `sed 's/.*out of \([0-9]\+\).*/\1/p'` 取用例总数。
GNU sed 接受 BRE 里的 `\+`，**BSD sed（macOS）不接受**，于是同一行在 Linux 正确、
在 macOS 上静默匹配不到——脚本会把一份完全正常的报告判为失败。
已改用 bash 内建 `[[ =~ ]]`。这类"换个平台就安静地给出不同答案"的写法在本项目里反复出现。

### 最终状态

| 验证项 | 结果 |
|---|---|
| 本机 `ctest -L rpc` | **176 用例全部通过** |
| `scripts/docker-test.sh` | **176 用例全部通过，无 `Skipped`** |
| 五个工具链用例（Go×2 / Python×2 / C#×1） | 均实际执行，非跳过 |
| `scripts/assert-test-report.sh` 真报告 | OK；总数不符 / 含 Skipped / 无汇总行三种情况均正确失败 |
| 容器构建产物的版本 | `rpc 1.0.43-a0a058e-dirty`，与本机构建**逐字一致** |
| `--version` / `-v` / 无参数 / 正常生成 | 四条路径退出码与输出均符合预期 |

用例数 173 → **176**（新增三个 `Compiler.Version*`）。

**未做**：CI 尚未实跑过——workflow 只能在推送后验证。首跑需重点确认三件事：
macOS runner 上系统 `bison` 2.3 / `flex` 可用、版本号不是 `1.0.1-xxxxxxx`（`fetch-depth: 0` 生效）、
版本号不是 `latest`（`--match` 生效）。验证方式见 §4。
