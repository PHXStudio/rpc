# 统一 Python 运行时的包名

## 1. 目标与背景

### 背景

Python 运行时的包名在三处互不一致：

| 位置 | 当前名字 |
|---|---|
| 目录 | `runtime/py/bin/` |
| `setup.py.in` | `name='bin'`、`packages=['bin']` |
| 生成代码 | `from rpc.writer import *` |

生成代码期望导入名为 `rpc` 的包，但运行时装出来的包叫 `bin`，两者对不上 ——
按 README 的流程使用生成的 Python 代码会直接 `ModuleNotFoundError`。

这是 `923d720` 命名统一（`bin` / `arpc` → `rpc`）的遗留：那次改了生成器的 import，
没改运行时目录与打包配置。

### 顺带发现的两个问题

1. **`setup.py.in` 从不被 CMake 处理**。全仓库只有两处 `configure_file`
   （根 `Config.h.in`、`tests/tests_config.h.in`），都不涉及它，
   因此 `@RPC_VERSION_MAJOR@` 占位符**永远不会被替换**，是死模板。
2. **`distutils` 在 Python 3.12 已被移除**。该文件用
   `from distutils.core import setup`，即便改好包名也无法运行。

### 目标

让 `pip install ./runtime/py` 装出名为 `rpc` 的包，且生成的代码能直接
`from rpc.writer import *`。

### 影响范围

**无破坏性**。全仓库只有 `setup.py.in` 自己引用 `bin`，没有任何其他依赖点，
改名不影响构建与测试（`runtime/` 不参与 CMake，Python 也没有测试接入 CTest）。

---

## 2. 受影响文件清单

| 文件 | 改动 |
|---|---|
| `runtime/py/bin/` → `runtime/py/rpc/` | 目录改名（`__init__.py`、`writer.py`、`reader.py`） |
| `runtime/py/setup.py.in` → `runtime/py/setup.py` | 改用 setuptools；包名改 `rpc`；版本写字面量 |
| `docs/knowledge-base.md` | §12 删除对应的轻微条目；§9 的路径引用同步 |

---

## 3. 分步实现计划

- [x] **A1** `git mv runtime/py/bin runtime/py/rpc`
- [x] **A2** `setup.py.in` 改名为 `setup.py`（去掉 `.in` 后缀 —— 它不再需要模板替换），内容改为：
      ```python
      from setuptools import setup

      # 与 CMakeLists.txt 的 RPC_VERSION_MAJOR/MINOR 保持一致
      setup(name='rpc', version='1.0', packages=['rpc'])
      ```
      - 用 `setuptools` 而非 `distutils`：后者在 Python 3.12 已移除
      - 版本写字面量：该文件没有任何自动生成路径，保留占位符只会得到字面量 `@...@`
- [x] **A3** 更新 `docs/knowledge-base.md`：
      - §9 中 `runtime/py/bin/` 的路径引用
      - §12 删除「Python 包名三处不一致」条目

> **不改动**：`compile/PYGenerator.cpp` 的 `from rpc.writer import *` 已是正确写法，
> 本次是让运行时向它靠拢。

---

## 4. 验证与测试步骤

```bash
# 1. 目录与包名已一致
ls runtime/py/rpc/{__init__.py,writer.py,reader.py}

# 2. 真的能装上、且装出来的包名是 rpc
python3 -m venv /tmp/pyenv && /tmp/pyenv/bin/pip install -q ./runtime/py
/tmp/pyenv/bin/python -c "import rpc.writer, rpc.reader; print(rpc.__file__)"

# 3. 端到端：用生成的代码在装好包的环境里跑通往返
./build/compiler/rpc -i tests/schema/CrossLangTest.rpc -o /tmp/pygen/ -g py
cd /tmp/pygen && /tmp/pyenv/bin/python -c "
from CrossLangTest import CrossLangPayload
v = CrossLangPayload(); v.i32_ = 0; v.s_ = 'hi'; v.b_ = [1, 2]
b = []; v.serialize(b)
print(b''.join(x if isinstance(x, bytes) else x.encode('latin-1') for x in b).hex(' '))
"
# 期望 01 60 02 68 69 02 01 02 —— 与 C++/C#/Go 一致

# 4. 构建与回归不受影响
cmake --build build && ctest --test-dir build -L rpc
```

---

## 5. 风险点

1. **旧路径 `runtime/py/bin` 会失效**。若有人在 `sys.path` 里硬编码了这个路径，
   需要同步更新。仓库内无此引用，仓库外无法确认 —— 但 `bin` 这个名字本身
   与生成代码的 import 对不上，任何依赖它的用法本来就是坏的。
2. **`pip install ./runtime/py`** 需要在有 `setuptools` 的环境（venv 自带）。

---

## 6. 完成后的文档同步

- `docs/knowledge-base.md` §9（运行时 API 的路径引用）、§12（删除该条目）
- `CLAUDE.md` 规则四的对照表补一行 Python 包名

---

## 7. 完成记录

全部完成并端到端验证。

`runtime/py/bin/` → `runtime/py/rpc/`（git 识别为重命名）；
`setup.py.in` → `setup.py`，改用 `setuptools`（`distutils` 在 Python 3.12 已被移除），
包名 `rpc`，版本写字面量 `1.0`。

验证（Python 3.12 venv）：

```
$ pip install ./runtime/py && pip show rpc
Name: rpc
Version: 1.0

$ python -c "import rpc.writer, rpc.reader"
rpc 包位于: /tmp/pyenv/lib/python3.12/site-packages/rpc/__init__.py
```

端到端：在装好包的环境里运行生成的代码，输出 `01 60 02 68 69 02 01 02`，
与 C++/C#/Go 基准逐字节一致。此前这条路径会直接 `ModuleNotFoundError`。

顺带确认：`setup.py.in` 从未被任何 CMake 处理（全仓库只有两处 `configure_file`，
都不涉及它），版本占位符永远不会被替换 —— 这也是改成字面量而非补一条
`configure_file` 的原因：产物会落在 build 目录、与源码包分离，`pip install` 反而找不到包。
