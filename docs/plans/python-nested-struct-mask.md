# 修复 Python 嵌套 struct 的掩码位错位

## 1. 目标与背景

### 背景

Python 后端为嵌套 struct 生成的 writer/reader **不使用传入的 `fm`**，
既不设置自己的掩码位，也不推进掩码位置。后果是后续字段的掩码位整体前移一位，
且嵌套 struct 自身在掩码中完全消失。

实测（schema `struct Outer { Inner in_; int32 y_; }`，其中 `Inner { int32 x_; bool b_; }`）：

```
                               C++ / C# / Go            Python
in_.x_=7, in_.b_=true, y_=3    01 c0 01 c0 07 00 …      01 80 01 c0 07 00 …
in_.x_=7, y_=0                 01 80 01 80 07 00 …      01 00 01 80 07 00 …
                                  ↑                        ↑
                            外层掩码 bit0 = in_       bit0 被 y_ 占用；
                            （嵌套 struct 恒为 1）     y_=0 时掩码成 00，
                                                     声称"无字段"但数据仍在
```

第二种情形最严重：掩码为 `00` 表示"两个字段都不存在"，而对端按此解析会完全错位。

**C++、C#、Go 三者已经一致**（本次实测确认），因此修复范围**只在 Python**。

### 根因

`PYGenerator.cpp` 生成的两个模块级函数忽略了 `fm`：

```python
def InnerWriter(b, v, fm):
    v.serialize(b)          # fm 未使用
def InnerReader(b, p, valMax, fm):
    v = Inner(); p = v.deserialize(b, p); return v, p   # fm 未使用
```

而运行时的 `write()` / `read()` 把这个 `fm` 原样透传给它们（非数组路径是
`return wtr(b, v, fm)`），所以掩码位既没被设置、位置也没推进。

### 目标

让 Python 的嵌套 struct 与其余三方一致：**写侧置位并推进，读侧消费该位**。

### 对齐基准

C++ 对 `FT_USER` 的处理（`CppGenerator.cpp`）：

- 写：`__fm__.writeBit(true);` —— **恒为 1**，无条件；
- 读：`if(__fm__.readBit()){ ...deserialize... }` —— 把该位当作守卫消费掉。

C# 与 Go 同构。注意读侧是**守卫**语义：位为 0 时跳过反序列化、字段保持默认值。

---

## 2. 受影响文件清单

| 文件 | 改动 |
|---|---|
| `compiler/PYGenerator.cpp` | 嵌套 struct 的 `XxxWriter` / `XxxReader` 使用 `fm` |
| `tests/` | 新增嵌套 struct 的字节级黄金向量 |
| `docs/knowledge-base.md` | §10、§12 更新 |

`compiler/CppGenerator.cpp`、`CSGenerator.cpp`、`GoGenerator.cpp`、四个 runtime、
schema 均**不需改动**。

---

## 3. 分步实现计划

### 步骤 A：修正 Python 的嵌套 writer / reader

- [x] **A1** `XxxWriter(b, v, fm)` —— 在 `v.serialize(b)` **之前**置位：
      ```python
      if fm != None:
          fm.set(True)
      v.serialize(b)
      ```
      必须在 `serialize` 之前：掩码位序按字段声明顺序，且 `fm.set()` 只推进外层掩码的
      位置，与嵌套 struct 内部的 `_fm_` 互不干扰（后者是 `serialize` 内部新建的独立对象）。

- [x] **A2** `XxxReader(b, p, valMax, fm)` —— 消费该位作为守卫：
      ```python
      if fm != None and not fm.get():
          return Xxx(), p
      v = Xxx()
      p = v.deserialize(b, p)
      return v, p
      ```

- [x] **A3** 注意 `fm` 可能是 `None`：**数组元素**走的是这条路径。
      运行时 `write()` / `read()` 在数组分支里显式传 `None`（数组的存在性已由外层的位表达），
      此时**不得**触碰 `fm`，否则会抛 `AttributeError`。

### 步骤 B：黄金向量

- [x] **B1** 新增 `tests/schema/Nested.rpc`（`Inner { int32 x_; bool b_; }` /
      `Outer { Inner in_; int32 y_; }`），在 `tests/CMakeLists.txt` 中生成 C++ 并加入
      `rpc_wire_format_tests`
- [x] **B2** 用例至少覆盖：
      - `in_.x_=7, in_.b_=true, y_=3` → `01 c0 01 c0 07 00 00 00 03 00 00 00`
      - `in_.x_=7, y_=0` → `01 80 01 80 07 00 00 00`（验证嵌套 struct 的位恒为 1）
      - `array<Inner>` 元素路径（`fm=None`）仍正常

> 现有 schema 中嵌套 struct 只出现在 `FullTest.rpc` 的 `StructBase.struct_`，
> 它是一个大结构体，定位失败原因时不够直观，因此单独建一份最小 schema。

---

## 4. 验证与测试步骤

### 主验收：四语言逐字节比对

```bash
cmake --build build --target rpc
for g in cpp cs py go; do
  ./build/compiler/rpc -i tests/schema/Nested.rpc -o /tmp/nested/$g/ -g $g
done
# 同一组字段值，四方输出必须完全相同
```

Python 侧用 `ServiceBaseStub` 之外的方式直接调 `serialize` 即可（无 service 依赖）。
Go 与 C# 需要各自的临时工程（见上一轮使用的探测方式），或在已有的黄金向量用例中比对 C++。

### 双向互读

除了字节相同，还要验证 **Python 能读 C++ 的字节、C++ 能读 Python 的字节** ——
上一轮修复前的失败模式正是"长度相同但互读失败"。

### 回归

```bash
cmake --build build && ctest --test-dir build --output-on-failure -L rpc
```

预期：除两个既存失效用例（引用已删除的 `bin/`）外全部通过。

---

## 5. 风险点

1. **数组元素路径传 `fm=None`**，若不加判空会直接抛异常。
   现有 `FullTest.rpc` 的 `array<StructType> structArray_` 覆盖这条路径，
   回归测试会立刻发现。

2. **只影响 Python，不会波及其余三方** —— 但仍需重跑全部黄金向量确认。

3. **`fm.set()` 的调用时机**。必须在外层字段的写入顺序位置上，
   即 `XxxWriter` 被调用时立即置位，而不是等 `serialize` 内部 —— 否则位序会错。

---

## 6. 完成后的文档同步

- `docs/knowledge-base.md` §10 —— 兼容性表最后一列转为「一致」，删除"仍未解决的一处"
- §12 —— 删除「Python 嵌套 struct 的掩码位错位」条目（届时该清单仅剩轻微项）
- `CLAUDE.md` 规则五 —— 删除最后一条"已知会破坏互通"的写法

---

## 7. 完成记录

全部完成并验证。实际做法比计划多了一处，理由见下。

### 改动

`compiler/PYGenerator.cpp` 的嵌套 writer / reader：

```python
def InnerWriter(b, v, fm):
    if fm != None:
        fm.set(True)      # 嵌套 struct 恒占一位且恒为 1
    v.serialize(b)

def InnerReader(b, p, valMax, fm):
    if fm != None and not fm.get():    # 消费该位作为守卫
        return Inner(), p
    v = Inner()
    p = v.deserialize(b, p)
    return v, p
```

新增 `tests/schema/Nested.rpc`（`Inner` / `Outer` / `Holder`，覆盖嵌套字段与数组元素两条路径）
并接入 CMake。

### 计划外新增：Python 一致性测试

计划里只写了 C++ 的黄金向量。但**这次的缺陷只存在于 Python 后端，C++ 黄金向量抓不到它** ——
把断言写在 C++ 侧，等于给一个 Python bug 建了一道拦不住它的网。

因此新增 `tests/py/wire_format_test.py`（CTest 中的 `PythonWireFormatGolden`），
用**生成的 Python 代码**跑同一批字节断言，并把运行时目录加入 `sys.path`
（这样导入的是源码树里的 `rpc` 包，而非已安装的副本）。找不到解释器时跳过，
与 .NET 用例的处理方式一致。

### 验证

**字节比对**（四方一致）：

| 向量 | 输出 |
|---|---|
| `in_.x_=7, in_.b_=true, y_=3` | `01 c0 01 c0 07 00 00 00 03 00 00 00` |
| `in_.x_=7, y_=0` | `01 80 01 80 07 00 00 00` |
| `Outer` 全默认 | `01 80 01 00` |
| `items_=[x=1,x=2], z_=5`（数组元素） | `01 c0 02 01 80 01 00 00 00 01 80 02 00 00 00 05 00 00 00` |
| `Holder` 全默认 | `01 00` |

每个期望值先按格式规范手工推导、再与生成器输出核对。

**双向互读**：Python 读 C++ 的字节、C++ 读 Python 的字节，字段值均正确。
（修复前的失败模式正是"长度相同但互读失败"，只比长度会漏掉。）

**回归测试有效性**：临时还原 `PYGenerator.cpp` 到修复前状态并重建，
`PythonWireFormatGolden` 立刻失败并报出精确差异：

```
FAIL: nested struct with a non-default sibling
     expected: 01 c0 01 c0 07 00 00 00 03 00 00 00
     actual:   01 80 01 c0 07 00 00 00 03 00 00 00
```

确认这道网真的拦得住，而非只是碰巧通过。

**回归**：126 个用例中 124 通过，仅剩两个引用已删除 `bin/` 的既存失效用例。
全新构建树同样通过，且 `PythonWireFormatGolden` 在全新树中正常执行。
