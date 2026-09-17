# rpc 编译器与线格式

从 `.rpc` 模式文件出发的代码生成器与二进制协议的完整参考：四种目标语言、一条线格式，以及所有在实测中被确认会出问题的地方。

| | |
|---|---|
| 仓库 | `PHXStudio/rpc` |
| 分支 | `main` |
| 分析起点 | `923d720` |
| 后端 | `cpp` · `cs` · `py` · `go` |
| 编译器 | ~5.0k 行 |
| 运行时 | ~3.6k 行 |

> **关于可信度**
>
> 本文每条技术结论都标注了来源：**实测** 表示在本机重新构建编译器、生成代码并运行后得到的可复现结果；**源码** 表示来自对源码的直接阅读。实测所得均给出了原始字节输出，可自行复现。
>
> 初版分析基于 `main` 分支的 `923d720`；此后修复了 Go 整数宽度、Go 生成代码无法编译、
> C# 命名空间断裂，并移除了 `(skipcomp)` 标记。修复后的结论已重新实测。

## 目录

1. [项目概览](#1-项目概览)
2. [快速开始](#2-快速开始)
3. [.rpc 语言参考](#3-rpc-语言参考)
4. [编译器架构](#4-编译器架构)
5. [线格式规格](#5-线格式规格)
6. [FieldMask 与版本兼容](#6-fieldmask-与版本兼容)
7. [类型映射矩阵](#7-类型映射矩阵)
8. [各后端产物](#8-各后端产物)
9. [运行时 API](#9-运行时-api)
10. [跨语言兼容性](#10-跨语言兼容性)
11. [测试与构建](#11-测试与构建)
12. [已知问题清单](#12-已知问题清单)
13. [仓库地图与分支](#13-仓库地图与分支)

---

## 1. 项目概览

一个 schema 驱动的二进制序列化工具链：单一 IDL，四个代码生成后端，四份手写运行时。

`rpc` 是一个命令行编译器。它读入以 `.rpc` 为扩展名的模式文件，为其中定义的 **struct / enum / service** 生成目标语言代码，并配套提供该语言的二进制读写运行时。生成代码与运行时共同实现一套自定义的二进制线格式。

```
 schema.rpc          rpc                     ┌─ C++     .h + .cpp + Methods.h
   IDL 源文件  ──→  flex + bison 编译器  ──→  ├─ C#      .cs
                     │                       ├─ Python  .py
                     │                       └─ Go      .go
                     └── + runtime（4 语言读写实现）
```

### 这套协议的核心特征

- **无字段标签**。线格式里没有 protobuf 那样的 per-field tag、wire type、varint 或 zigzag。字段身份完全由收发双方的**声明顺序**保证。
- **定长小端整数**，唯一变长编码是长度前缀 `dynSize`。
- **FieldMask 位图**前置在每个结构体之前，负责表达"字段是否存在"，这是唯一的可选性机制，也是唯一的版本兼容手段。
- **零值即缺省**。没有 null，没有 presence 语义 —— `0`、`false`、空串、空数组都会被视为"未设置"而不占字节，与 protobuf proto3 的取舍一致。

运行时不是编译产物，而是四份独立手写的源码树（`runtime/cpp`、`runtime/cs`、`runtime/go`、`runtime/py`）。生成代码只调用这些运行时的公开 API，因此**线格式的一致性靠四份实现之间的约定维持**，而不是靠共享代码。这正是本文 §10、§12 里多数问题的根源。

---

## 2. 快速开始

构建、调用与验证的最短路径，含几个会让第一次使用失败的坑。

### 依赖与构建

```bash
# 依赖：CMake ≥ 3.16、C++11 编译器、bison、flex（rapidjson 由 FetchContent 自动拉取）
cmake -S . -B build
cmake --build build
# 产物：build/compiler/rpc
```

### 命令行

```
rpc -i <输入.rpc> -o <输出目录/> -g <cpp|cs|py|go>
```

| 选项 | 含义 | 备注 |
|---|---|---|
| `-i` | 模式文件路径 | 无默认值，缺失时无友好报错 |
| `-o` | 输出目录 | 末尾 `/` 或 `\` 会自动补全 |
| `-g` | 后端 | 未知值**静默回落**到 `cpp` |

程序没有 `--help`，也没有版本号输出；`rpc --help` 会静默退出并返回 0。

> **产物命名**（实测）
>
> 输出文件名取自 **schema 文件的主文件名**，而不是其中定义的结构体名。`CrossLangTest.rpc` 始终生成 `CrossLangTest.cpp` / `.h` / `.cs` / `.go` / `.py`，无论里面定义了多少个 struct。

### 端到端示例

```bash
mkdir -p out_cpp
./build/compiler/rpc -i tests/schema/FullTest.rpc -o out_cpp/ -g cpp
# → out_cpp/FullTest.h, out_cpp/FullTest.cpp, out_cpp/ServiceBaseMethods.h, ...
```

C++ 侧编译时需要三个 include 路径：生成目录、`runtime/cpp`、rapidjson 的 `include`。

```bash
g++ -std=c++11 -I out_cpp -I runtime/cpp -I _deps/rapidjson-src/include \
    your_code.cpp out_cpp/FullTest.cpp -o your_app
```

### 运行测试

```bash
cmake -S . -B build -DBUILD_TESTING=ON   # 必须显式传入，见 §12
cmake --build build
ctest --test-dir build --output-on-failure -L rpc
```

测试全部挂在同一个 `rpc` label 下，因此 `-L rpc` 等价于跑全量。依赖 .NET 的跨语言用例在找不到 `dotnet` 时会**跳过而非失败**。

---

## 3. .rpc 语言参考

一套刻意保持极小的 IDL：三种顶层定义、一个类型系统、没有字段修饰符。

### 顶层定义

模式文件由 `enum`、`struct`、`service` 三种定义组成，顺序无关（但引用必须先定义）。

```c
/* 枚举：成员自动编号 0..n-1，不支持显式赋值 */
enum EnumName
{
	EN1,
	EN2,
	EN3,
};

/* 结构体：可继承 */
struct StructBase
{
	int32	int32_;
	string	string_;
	array<int32>	int32Array_;
};

struct DerivedStruct : StructBase
{
	int32 aaa_;
};

/* 服务：方法即一次 RPC 调用，无返回值声明 */
service ServiceBase
{
	method1(string[32] username, string[32] password);
	method2(StructBase s);
};

service DerivedService : ServiceBase
{
	method9(DerivedStruct d);
	method10();          // 允许无参方法
};
```

### 类型系统

| 写法 | 含义 | 备注 |
|---|---|---|
| `int64` `uint64` `double` `float` | 64 位整数与浮点 | 全部为定长小端编码 |
| `int32` `uint32` `int16` `uint16` | 32 / 16 位整数 | 同上 |
| `int8` `uint8` `bool` | 8 位整数与布尔 | 同上 |
| `string` | 动态字符串 | `dynSize` 长度前缀 |
| `string[N]` | 带读取上界的字符串 | **不改变编码**，仅在读取时校验上限 |
| `array<T>` | 动态数组 | `dynSize` + 顺序元素 |
| `array[N]<T>` | 带读取上界的数组 | 同样不改变编码 |
| `bytes` / `bytes[N]` | 字节串 | 语法层完全等价于 `array<uint8>` |
| 标识符 | enum 或 struct 类型 | 查表决定是枚举还是用户类型；用 service 作类型会报错 |

### 结构体标记：已移除

早期版本支持一个 struct 标记 `(skipcomp)`，写在名字与父类之后、左花括号之前。
它的实际语义是「跳过与默认值比较」，即**无条件写出所有字段**，且只有 C++ 与 C# 生成器实现了它 ——
同一份 schema 会因此产生两种互不兼容的字节流。

该标记与语法**已被彻底移除**。现在 `struct Foo (anything) { ... }` 是语法错误：

```
Foo.rpc(2):syntax error, unexpected '(', expecting '{'
```

所有 struct 一律走「与默认值比较」的压缩路径，即 §6 描述的唯一编码。

### 注释

```c
// 单行注释
/* 多行注释 */
```

`#< ... #>` 是文件级 C++ 代码片段，原样插入生成的头文件，仅在根文件生效；`#{ ... #}` 是 struct 级 C++ 片段，插入到最近解析的结构体声明中。这两个转义**只对 C++ 后端有效**。

### `#import` 机制

导入在词法层完成：解析器维护一个文件栈，遇到 `#import` 就切换输入缓冲，`<<EOF>>` 时恢复。去重基于已导入文件集合，搜索路径以**主 schema 所在目录**为首选。

> **硬退出**（源码）
>
> 被导入文件找不到时，词法器直接 `exit(1)`，而不是向上报错。这意味着把编译器当作库使用时，一个坏路径会**直接终止宿主进程**。

所有导入文件中的定义会被**摊平**进同一份输出，以根文件的主文件名命名 —— 导入不会产生独立的头文件。

### 语言刻意不提供的东西

| 没有的语法 | 已有的语义检查 |
|---|---|
| 字段 ID / 序号（由声明顺序隐式决定） | 重复定义名 |
| `optional` / `required` / `repeated` | 重复字段名 / 重复参数名 |
| 字段默认值 | 字段名与已有定义冲突 |
| package / namespace 声明 | 非法父类型（未定义、自继承、用作类型的 service） |
| 常量、类型别名、前向引用 | 非法的父类型 / 未知类型引用 |

---

## 4. 编译器架构

经典的 flex + bison 前端，加上四个结构上高度雷同、彼此不共享代码的后端。

```
 rpc.l          →   rpc.y          →   Compiler        →   Generator
 flex 词法 222 行   bison 语法 439 行   单例 · 持有 AST     按 -g 选择
```

### AST

所有定义都以 `Definition` 为基类（`Definition.h`），向下分为 `Struct`、`Enum`、`Service`，字段由 `Field` 表示，结构体与方法的公共部分抽象为 `FieldContainer`。继承关系直接存在 `super_` 指针里。

解析过程中，编译器用 `Compiler` 单例上的 `curStruct_` / `curService_` / `curField_` / `curMethod_` 累积当前状态，在归约到完整定义时转成堆对象存入 `definitions_`。字段 ID 由 `FieldContainer` 在生成阶段按"父类字段数 + 自身序号"计算。

### 后端接口

```cpp
class CodeGenerator
{
public:
	virtual void generate() = 0;
};
```

只有一个纯虚函数，没有参数、没有返回值。四个后端各自从 `Compiler::inst()` 单例取所需的一切。

真正的生成逻辑是各 `.cpp` 里的 `static` 自由函数（`generateStruct`、`generateEnum`、`generateStub`、`generateProxy`…）。四个后端之间**没有任何共享头或公共基类** —— 同名函数在四个文件里各写一遍，是彻底的复制-分叉结构。这是理解"为什么某个后端会漏掉某个特性"的关键。

### 代码输出：CodeFile

`CodeFile` 极薄：维护一个制表符缩进深度，提供 `printf` 风格的 `output()`，并**直接写入 `FILE*`**，没有缓冲。因此生成过程中无法对已写内容做后处理 —— 这也是 Go 输出没有经过 `gofmt`、Python 输出没有经过格式化的原因。

### 编译主流程

```
main()                       Main.cpp
  └─ Args 解析 -i / -o / -g
  └─ Compiler::inst().compile()          Compiler.cpp
       ├─ yyparse()                       // 词法+语法，填充 definitions_
       ├─ 按 generator_ 选择后端对象
       └─ gen->generate()                 // try/catch(const char*)
```

---

## 5. 线格式规格

整个项目的契约核心。以下全部为实测确认的字节级行为。

### 整体布局

```
结构体（每个 FieldContainer 各一段，逐层递归）：
[ fmLen : uint8 ][ FieldMask : fmLen 字节 ][ 字段数据 … ]

RPC 调用：
[ methodId : uint16 ][ 参数字段容器段 … ]
```

> **方法载荷与结构体同构**
>
> 方法参数被当作一个普通 `FieldContainer` 编码，掩码语义与结构体完全一致：
> 参数等于默认值时不占字节，由掩码位表达；bool 参数不占字节。
>
> **无参方法不写掩码段**，载荷只有 `[methodId]`。实测 `method10()` → `09 00`（2 字节）。

实测的方法载荷（四种语言输出逐字节相同）：

```
method5(1, 2, true, EN2)   →  04 00 01 f0 01 02 01
                              └─┬─┘ │  │  │  └── enum EN2
                              pid=4 │  │  └───── u8 = 2
                                 fmLen=1  └────── i8 = 1
                                        mask=0xF0   （bool 不占字节）

> **关键区分**
>
> `fmLen` 是**裸 uint8**，不是 `dynSize`。这决定了单个结构体最多表达 255 字节掩码，即约 2040 个字段。

### 整数：定长、小端、补码

没有任何变长整数编码。各语言实现方式不同但语义一致：C++ 直接 `memcpy` 宿主内存，C# 用 `BitConverter`，Go 显式 `binary.LittleEndian`，Python 用 `struct.pack('<i')`。

```
int32 值 0x12345678

  78 56 34 12
  └──┬──┘
   4 字节，小端序，低字节在前
```

> **宿主端序依赖**（源码）
>
> C++ 与 C# 的实现直接使用宿主内存布局，只在**小端机器上正确**。`Config.h.in` 里有 `RPC_BIG_ENDIAN` 宏，但没有任何源文件 include 它 —— 大端平台会静默产生错误字节流。

### `dynSize`：唯一的变长编码

不是 varint，也不是 LEB128。规则：长度值以**大端**写出 1–4 字节；首字节的高 2 位记录**后续还有几字节**，低 6 位是长度的高 6 位。

| 取值范围 | 总字节 | 编码 |
|---|---|---|
| ≤ `0x3F` | 1 | `0nnnnnnn` |
| ≤ `0x3FFF` | 2 | `01nnnnnn nnnnnnnn` |
| ≤ `0x3FFFFF` | 3 | `10nnnnnn …` |
| ≤ `0x3FFFFFFF` | 4 | `11nnnnnn …` |

**实测：C++ 运行时与 Go 运行时对同一批值的输出，逐字节一致**

```
0x00000000 → 00
0x0000003F → 3F
0x00000040 → 40 40          ← 进位到 2 字节：高 2 位 01 + 高 6 位 0
0x00003FFF → 7F FF
0x00004000 → 80 40 00       ← 进位到 3 字节
0x003FFFFF → BF FF FF
0x00400000 → C0 40 00 00
0x3FFFFFFF → FF FF FF FF
```

超出 `0x3FFFFFFF` 时，C++ / C# / Python 都会**静默截断或丢弃数据**，只有 Go 返回错误。

### 字符串、布尔与枚举

| 类型 | 编码 | 说明 |
|---|---|---|
| `string` | `dynSize(len)` + 原始字节 | 无 NUL 终止符；读取侧强制 `maxlen` 上限 |
| `bool` | 1 字节，写侧归一化为 0/1 | 读侧接受任意非零为 true |
| `enum` | 1 字节 uint8 序号 | **枚举成员上限 256**；越界值导致整包解析失败而非忽略 |
| `float` / `double` | 4 / 8 字节 IEEE-754 小端 | — |
| 数组 | `dynSize(count)` + 元素顺序排布 | 元素无独立标签 |

字符编码不是协议的一部分。只有 **C# 运行时显式做 UTF-8**（`Encoding.UTF8`），C++ / Go / Python 按原始字节序列处理，编码由调用方负责。

### 继承的布局：每层一个掩码段

基类与派生类**不共享**一个 FieldMask。序列化时先递归调用基类，因此线格式是 `[基类段][派生段]`，继承链每增加一层就多一个 `[fmLen][fm]` 块。

```cpp
void Derived::serialize(ProtocolWriter* s) const
{
	Base::serialize(s);   // ← 先写基类那一整段
	// ← 再写自己这一段
}
```

### 消息边界

协议自身**不含长度或结束标记**。结构体解析的终止条件是"掩码中所有已知位处理完毕"，真实的消息边界必须由传输层帧来界定。这也解释了为什么 FieldMask 是必需的 —— 无法用 EOF 判断结束。

---

## 6. FieldMask 与版本兼容

唯一的可选性机制，也是唯一的演进手段 —— 但它的语义比名字看起来复杂得多。

### 位图布局

字段在结构体中的声明序号即位序号，采用 **MSB-first** 打包：位置 0 对应首字节的最高位。

```cpp
// C++ 参考语义
void writeBit(bool b) {
	if (b) masks_[pos_ >> 3] |= (128 >> (pos_ & 7));
	pos_++;
}
```

四份运行时实现逐位一致（Go 写作 `1 << (7 - pos%8)`，等价）。字节数为 `ceil(N/8)`。

### 位的语义

| 字段类型 | 位 = 1 的条件 | 位 = 0 时 |
|---|---|---|
| 数值 / 枚举 | `value != 0` | 不写字节，读侧取默认值 |
| `bool` | 值本身 | 不写字节，读侧 false |
| `string` | `length > 0` | 不写字节，读侧空串 |
| 数组 | `size > 0` | 不写字节，读侧空数组 |
| 嵌套 struct | **恒为 1** | 永不发生，总是递归写整块 |

因为位由"值是否等于默认值"决定，**"零值"与"未设置"在协议层不可区分**。

### 版本兼容的三步读法

读取侧的逻辑在四个后端中一致：

```
1. 读 fmLen（1 字节）
2. readFmLen = min(actualFmLen, 我自己需要的字节数)
3. 读 readFmLen 字节掩码
   ├─ actualFmLen > readFmLen  →  skip 掉多余字节       // 旧版本读新数据
   └─ actualFmLen < readFmLen  →  剩余位补 0            // 新版本读旧数据
```

由此得出的兼容性边界：**只能通过在结构体末尾追加字段来演进**。删除字段、改变字段顺序、改变字段类型都会让整个流无法解析 —— 因为没有 per-field tag 可用来对齐。

### 唯一编码：与默认值比较

**当前版本只有这一种编码。** 值等于默认值的字段不占字节，其存在性由掩码位表达。

**实测：`struct { int32 i32_; string s_; bytes b_; }`，值 `i32_=0`、`s_="hi"`、`b_=[1,2]`**

```
01 60 02 68 69 02 01 02          （8 字节）

01 60        fmLen=1；掩码 0x60 → bit1、bit2 置位，bit0（i32_）为 0
02 68 69     字符串 "hi"
02 01 02     数组 [1, 2]
              i32_ 等于默认值 0，不占任何字节
```

**同一结构，`i32_=5`：**

```
01 C0 05 00 00 00 02 68 69 02 01 02

01 C0        fmLen=1；掩码 0xC0 → bit0、bit1 置位
05 00 00 00  int32 小端，4 字节
02 68 69     "hi"
02 01 02     [1, 2]
```

C++、C#、Python、Go 四个后端对上述两组输入**输出逐字节相同**，且可互相读取。

> **历史说明**
>
> 早期版本另有一个 `(skipcomp)` 标记，语义是「跳过与默认值比较」，即无条件写出所有字段。
> 它只在 C++ 与 C# 生成器中实现，Go 与 Python 完全忽略，因此同一份 schema 会产生两种
> 互不兼容的字节流（实测 C++ 12 字节 vs Python 8 字节，互读失败）。
> 该标记与对应逻辑已从编译器中移除，语法一并删除 —— 旧 schema 会解析报错而非静默改变格式。

---

## 7. 类型映射矩阵

| IDL | C++ | C# | Python | Go |
|---|---|---|---|---|
| `int64` | `int64_t` | `long` | `int` | `int64` |
| `uint64` | `uint64_t` | `ulong` | `int` | `uint64` |
| `double` | `double` | `double` | `float` | `float64` |
| `float` | `float` | `float` | `float` | `float32` |
| `int32` | `int32_t` | `int` | `int` | `int32` |
| `uint32` | `uint32_t` | `uint` | `int` | `uint32` |
| `int16` | `int16_t` | `short` | `int` | `int16` |
| `uint16` | `uint16_t` | `ushort` | `int` | `uint16` |
| `int8` | `int8_t` | `sbyte` | `int` | `int8` |
| `uint8` | `uint8_t` | `byte` | `int` | `uint8` |
| `bool` | `bool` | `bool` | `bool` | `bool` |
| `string` | `std::string` | `string` | `str` | `string` |
| `array<T>` | `std::vector<T>` | `T[]` | `list` | `[]T` |
| `bytes` | `std::vector<uint8_t>` | `byte[]` | `list` | `[]uint8` |
| `enum X` | `X`（真枚举） | `X` | 普通 `int` | `X`（int32 命名类型） |
| `struct X` | `X` | `X` | `X` | `X` |

Python 完全不区分整数宽度（一律 `int`），宽度信息只体现在它选择的 `*Writer` 函数上。这本身没有问题 —— 只要序列化时选对函数，而 Python 确实选对了。

---

## 8. 各后端产物

同一份 schema，四种语言的输出形态差异很大。

| | C++ | C# | Python | Go |
|---|---|---|---|---|
| 输出文件 | `stem.h`<br>`stem.cpp`<br>`ServiceMethods.h` | `stem.cs` | `stem.py` | `stem.go` |
| 命名空间 | 无（全局） | 无（全局） | 模块名 = 文件名 | `package` = 小写 stem |
| 运行时引用 | `#include "ProtocolWriter.h"` | `rpc.ProtocolWriter` | `from rpc.writer import *` | `github.com/rpc/runtime` |
| 继承实现 | `struct X : public Base` | `class X : Base` + `new` 隐藏 | `class X(Base)` | 匿名内嵌基类 |
| 字段 ID | struct 内 `enum { FID_x, FIDMAX }` | 嵌套 `public enum FID` | 无 | 扁平常量 `FIDXxxYyy` |
| 逐字段 API | 无 | 有 `serializeField` / `deserializeField` | 无 | 无 |
| 构造函数 | 仅当存在标量默认值时生成 | 无（字段初始化器） | 恒生成 `__init__` | 恒生成 `NewX()` |
| JSON | 完整 `toJson` / `loadJson` | 无 | 无 | 仅 struct tag，无方法 |
| 错误模型 | `bool` 返回 | `bool` 返回 | 异常 + 位置式返回 | `error` 返回 |

### Service 三件套

每个 service 在各语言中生成 Stub（发送侧）、Proxy（接收侧）与分发器：

```cpp
// C++
class ServiceBaseStub  { void method1(...);  protected: virtual ProtocolWriter* methodBegin() = 0; };
class ServiceBaseProxy { virtual bool method1(...) = 0;  bool dispatch(ProtocolReader*); };
```

```csharp
// C#
public abstract class ServiceBaseStub { ... }
public interface ServiceBaseProxy { ... }
public static class ServiceBaseDispatcher { public static bool dispatch(...); }
```

```go
// Go
type ServiceBaseStub struct { ... }
type ServiceBaseProxy interface { ... }
type ServiceBaseDispatcher struct{}  // 方法名小写开头，包外不可见
```

方法 ID 按**基类优先**的顺序分配，四个后端一致。方法载荷与结构体编码**完全同构**：
`[uint16 methodId][fmLen][FieldMask][参数]`，掩码语义与结构体一致。唯一的例外是**无参方法不写掩码段**
（载荷只有 `[methodId]`）。四个后端自 `1e2f637` 起一致。

### C++ 的两个扩展点

C++ 后端是唯一支持代码注入的后端：`#< ... #>` 把片段插入文件级，`#{ ... #}` 插入结构体级。此外它还会为每个 service 额外生成一份 `<Service>Methods.h`，内含非纯虚的方法声明，便于把 handler 实现分散到多个编译单元。

---

## 9. 运行时 API

四份手写运行时，四套不同的 API 形态，共享一条线格式。

### C++ · 虚基类 + 重载

- `ProtocolWriter` / `ProtocolReader` 抽象基类
- `writeType(T)` 重载覆盖全部标量
- `readType(T&)` 全部返回 `bool`
- `writeDynSize` / `readDynSize`
- `skip(size_t)` —— 版本兼容用
- 实现：`ProtocolMemWriter`（固定缓冲）、`ProtocolBytesWriter`（可增长 vector）

### C# · 接口 + 静态类

- `IWriter` / `IReader` 接口
- `ProtocolWriter` / `ProtocolReader` 静态类，首参为接口
- `readType(r, out T, maxlen)` 形态
- `Skip(uint)` 在 `IReader` 上
- 只有 `MemWriter`（内部 `List<byte>`，**可增长**）与 `MemReader`

### Go · interface + 方法爆炸

- `rpc.ProtocolWriter` / `rpc.ProtocolReader` 接口
- 每个类型一个方法（Go 无重载）
- 统一 `(T, error)` 返回
- 具名 sentinel：`ErrArrayTooLong`、`ErrUnknownMethod`…
- 附 `FieldMask`、`EnumInfo` 全局注册表

### Python · 自由函数

- 没有读写对象 —— 缓冲是 `list`，位置是整数
- `int32Writer` / `stringReader` 一族函数
- `write(wtr, isArray, buf, v, fm)` 调度器
- `skipReader(b, p, n)`
- 生成代码形如 `self.f, _p_ = read(int32Reader, _b_, _p_, 0, 0, _fm_)`
- 包名为 `rpc`（`runtime/py/rpc/`），`pip install ./runtime/py` 即可装入；
  生成代码的 `from rpc.writer import *` 直接可用

### Mem* 与 Bytes* 的语义漂移

> **同名不同义**（源码）
>
> 在 C++ 里 `ProtocolMemWriter` 写入**外部固定缓冲区**，容量不足时 `write()` 直接 `return` —— 因为是 `void`，**溢出会静默丢字节且无法感知**。而在 C# 与 Go 里，`MemWriter` 内部的缓冲**会增长**，实际扮演的是 C++ 里 `BytesWriter` 的角色。"Mem" 这个名字在两个语言族里含义相反，选型时极易误判。

### EnumInfo

运行时的枚举元数据表，承担三重职责：二进制用序号、JSON 用字符串名之间的转换；提供读取侧的取值范围校验（`valMax = items_.size()`）；以及 `ENUM(X)` 这类查询入口。C++ 用构造期回调填充，Go 用带 `sync.RWMutex` 的全局注册表。

---

## 10. 跨语言兼容性

实测结果。这是使用本项目前最需要知道的一张表。

**四种语言在已知的全部维度上均已一致，无未解决项。**

| 维度 | 状态 |
|---|---|
| 基本类型（标量 / 字符串 / 数组 / bool / 枚举） | 一致 |
| 非 64 位整数按声明宽度写出 | 一致 |
| 服务方法载荷（`[methodId][fmLen][fmask][参数]`） | 一致 |
| 嵌套 struct（占父级一位，恒为 1） | 一致 |
| 嵌套 struct 作为数组元素（元素侧无掩码） | 一致 |

**已验证的方式**：同一 schema、同一组字段值，四方输出逐字节相同且可互相读取。
C++ ↔ C# 由跨语言文件交换用例覆盖（`rpc_serialization_tests` / `rpc_full_crosslang_tests`），
其余组合由字节级黄金向量（§11）与 Python 一致性测试守护。

> `skipcomp` 一列已随该标记的移除而作废，见 §6。

### 历史：Python 的嵌套 struct 掩码错位（已修复）

**Python 嵌套 struct 的掩码位错位**

`实测` · 已修复 · `compiler/PYGenerator.cpp`

Python 为嵌套 struct 生成的 writer 曾忽略传入的 `fm`，既不置位也不推进位置，
导致后续字段掩码整体前移一位，且嵌套 struct 自身在掩码中完全消失：

```
                            C++ / C# / Go          Python（修复前）
in_.x_=7, in_.b_=true, y_=3  01 c0 01 c0 07 00 …   01 80 01 c0 07 00 …
in_.x_=7, y_=0               01 80 01 80 07 00 …   01 00 01 80 07 00 …
```

第二种情形最严重：掩码为 `00` 表示"两个字段都不存在"，数据却仍在。

修法（`PYGenerator.cpp`）：writer 在 `v.serialize(b)` 之前 `fm.set(True)`
（嵌套 struct 恒占一位且恒为 1），reader 消费该位作为守卫。**数组元素路径传 `fm=None`**，
此时两侧都不得触碰掩码。

（实测）Python 后端为嵌套 struct 生成的 `XxxWriter(b, v, fm)` 函数体只有一句 `v.serialize(b)`，**既不设置自己的掩码位，也不推进掩码位置**。于是后续字段会占用本该属于嵌套 struct 的那一位，整个掩码**前移一位**。

schema：`struct Outer { Inner in_; int32 y_; }`，值 `in_.x_=7`

C++ 输出，`y_ = 3`：

```
01 C0 01 80 07 00 00 00 03 00 00 00
   └┬┘
   bit0（嵌套 struct，恒为 1）+ bit1（y_ ≠ 0）
```

Python 输出，`y_ = 3` —— 同样是 12 字节，掩码差一位：

```
01 80 01 80 07 00 00 00 03 00 00 00
   └┬┘
   嵌套 struct 没占位，y_ 的位前移到了 bit0
```

当 `y_` 取默认值 0 时后果最清晰：C++ 写出 `01 80 01 80 07 00 00 00`（8 字节，合法），Python 去读它会抛 `struct.error: unpack requires a buffer of 4 bytes` —— 因为错位的掩码让它去找一个 C++ 根本没写的 `y_`。

---

## 11. 测试与构建

七个 GoogleTest 目标、一套文件交换式跨语言验证，以及若干未接线的资产。

### 测试组织

全部用例通过 `gtest_discover_tests` 注册，且只有**一个 label**：`rpc`。因此 `ctest -L rpc` 就是全量测试，没有更细的筛选维度。

| 目标 | 覆盖内容 | 依赖外部工具 |
|---|---|---|
| `rpc_serialization_tests` | 内存往返 + C++↔C# 文件交换 | 是（可跳过） |
| `rpc_full_schema_tests` | 全类型 schema 往返 | 否 |
| `rpc_full_crosslang_tests` | 跨语言（含 enum / 数组） | 是（可跳过） |
| `rpc_compiler_tests` | 调用编译器并检查产物文本；含 `Edge.rpc` 的代码注入与多字节掩码 | 否 |
| `rpc_compiler_negative_tests` | **编译器负例**：坏 schema 必须非零退出；`enum : 类型` 的两种失效形态 | 否 |
| `rpc_import_tests` | `#import`：定义摊平、无悬空引用、缺失导入致命 | 否 |
| `rpc_runtime_edge_tests` | `skip` 边界、版本兼容读路径、`dynSize`/整数/浮点字节 golden、MemWriter 溢出 | 否 |
| `rpc_service_tests` | Stub / Proxy 与报文分发 | 否 |
| `rpc_json_tests` | JSON 序列化与反序列化；含 `JsonGolden.*` 的**硬编码 JSON 文本** | 否 |
| `rpc_go_generator_tests` | Go 产物**文本断言**（不编译） | 否 |
| `rpc_wire_format_tests` | **字节级黄金向量**（见下） | 否 |
| `rpc_interop_crosslang_tests` | InteropFull 向量交给 C# 解码并重编码 | 是（可跳过） |
| `GoInteropGolden` | **编译并运行** Go 产物，断言同一向量 | 需 Go 工具链 |
| `PythonInteropGolden` | 生成 Python 断言同一向量 | 需 Python 解释器 |
| `PythonWireFormatGolden` | 嵌套 struct 掩码向量 | 需 Python 解释器 |

### 字节级黄金向量

其余用例都只断言「序列化再反序列化得到相同的值」—— 这个条件**任何自洽的编码都能满足**，
包括错误的编码。`rpc_wire_format_tests` 因此改为直接固定字节：

```
CrossLangPayload{i32_=0, s_="hi", b_={1,2}}  →  01 60 02 68 69 02 01 02
CrossLangPayload{i32_=1, ...}                →  01 e0 01 00 00 00 02 68 69 02 01 02
FullCrossLangPayload{全默认值}                →  01 00
FullCrossLangPayload{全字段非默认}            →  01 fe 01 00 00 00 02 00 00 00 01 02 68 69 02 01 02 02 03 00 00 00 04 00 00 00
FullCrossLangPayload{bool_=true}             →  01 20        ← bool 不占载荷字节
```

方法载荷的黄金向量在 `tests/runtime/service_test.cpp` 的 `MethodPayloadGolden.*` 中，
固定 `[methodId][fmLen][fmask][参数]` 的确切字节，含无参方法（无掩码段）与非默认值省略两种情形。
嵌套 struct 的向量在 `wire_format_golden_test.cpp` 的 `WireFormatGolden.Nested*` 中。

**`InteropFull` 向量是四端共享的契约**（`WireFormatGolden.Interop*`）。此前
`CrossLangTest`（3 字段）与 `FullCrossLang`（7 字段）合计从未覆盖 `int8/16/64`、`float/double`、
嵌套 struct 与字符串数组 —— 任何后端放宽整数宽度或丢掉浮点，跨语言用例都仍然全绿。
`tests/schema/InteropFull.rpc` 补齐这些维度，同一组字节由四个后端各自断言：

| 后端 | 断言位置 | 是否真正运行产物 |
|---|---|---|
| C++ | `WireFormatGolden.InteropPayloadAllFieldsPresent` | 是 |
| C# | `rpc_interop_crosslang_tests` → `InteropVerifier.cs` | 是 |
| Python | `PythonInteropGolden` → `tests/py/interop_test.py` | 是 |
| Go | `GoInteropGolden` → `tests/go/interop_golden_test.go` | 是 |

四者都固定同一串手工推导的 hex（74 字节），而不是互相传值往返 —— 后者任何自洽编码都能通过。

**C++ 黄金向量抓不到 Python 专有的回归** —— 而嵌套 struct 掩码错位恰恰只存在于
Python 后端。因此另有 `tests/py/wire_format_test.py`（CTest 中名为 `PythonWireFormatGolden`），
用**生成的 Python 代码**跑同一批字节断言。找不到解释器时该用例跳过，
与 .NET 用例的处理方式一致。

每个期望值都先按格式规范手工推导、再与生成器输出核对。这是防止线格式被无意改动的唯一回归网。

### 跨语言验证的工作方式

C++ 与 C# 的互操作通过**临时二进制文件交换**验证，而不是链接在一起：

- 构建期用 `add_custom_command` 对同一份 schema 同时生成 C++ 与 C#。
- 构建期用 `dotnet build` 预编译一个独立的验证器可执行文件 —— 刻意**不用 `dotnet run`**，避免每次触发完整 MSBuild 评估。
- 运行期 C++ 侧写临时 `.bin` 文件（用 PID 隔离并发），再通过 `dotnet exec` 启动验证器进程，以退出码判定结果。
- 字符串与字节数组以**十六进制**传参，规避跨平台参数编码差异。

找不到 `dotnet` 时，对应用例编译成 `GTEST_SKIP()`，**跳过而非失败**，并且
**仍以真实用例名注册** —— 早先用一个 `SkippedNoDotnet` 占位名代替，会让这些
用例从 `ctest -N` 里彻底消失，只看汇总行与「已覆盖」无法区分。

Python 与 Go 的两条外部工具链同理：工具链缺失时用例仍然注册，只是报告为
`Skipped`（CMake 侧用 `SKIP_REGULAR_EXPRESSION`）。这一条曾经很要命 ——
`PythonWireFormatGolden` 在容器里根本不注册，`ctest` 却显示 100% 通过。

> **Go 与 Python 的接入方式**
>
> `runtime/go/` 自身的测试（约 160 个用例/子测试）需要手动 `go test ./...`；CMake 侧的
> `rpc_go_generator_tests` 仍只是 C++ 写的**文本断言**，不编译生成的 Go 代码。
> 真正的 Go 行为验证由 `GoInteropGolden` 承担：它把生成的代码与测试文件放进一个
> 构建期模块（`tests/go/go.mod.in` + `replace` 指向 `runtime/go`）后 `go test`，
> 因此 Go 产物同时被**编译**和**运行**。
>
> Python 侧由 `PythonWireFormatGolden`（嵌套掩码）与 `PythonInteropGolden`（InteropFull 向量）
> 覆盖，两者都在找不到解释器时**不注册**（连 skip 记录都没有），且 Dockerfile 的 tester
> 阶段未安装 python3 —— 在容器里跑测试时这两项不会执行。

### 依赖获取

- **rapidjson** —— 根 `CMakeLists.txt` 通过 FetchContent 拉取，但 tag 写的是 `master` 而非固定版本，构建不完全可复现。
- **GoogleTest** —— 先 `find_package`，找不到才 FetchContent 拉 `v1.14.0`。
- **bison / flex** —— 强制 required，Windows 需自行安装 WinFlexBison。

### Docker

`Dockerfile` 是一个多阶段构建：Linux 构建、Windows 交叉编译（mingw-w64）、以及一个额外安装 .NET 6 用于跑测试的 tester 阶段。`scripts/` 下有 8 个对应的 sh / ps1 脚本。`docker build` 的上下文需要注意：`.dockerignore` 没有排除 `_deps/`，会把完整的依赖克隆打进上下文。

---

## 12. 已知问题清单

按影响排序。每一条都经过验证，标注了验证方式。

> **已修复**：Go 整数宽度、Go 生成代码无法编译、C# 命名空间断裂、
> `(skipcomp)` 造成的跨语言不兼容（该标记已从语法中移除）、
> Go/Python 方法载荷缺少 FieldMask、Python 嵌套 struct 掩码错位、
> 以及 `rpc.l` 引用已改名的 `bin.tab.hpp` 导致干净构建失败。
>
> **2026-09-17 修复**（均经实测确认，非推断）：
>
> | 问题 | 根因 | 验证 |
> |---|---|---|
> | Python `stringWriter`/`boolWriter` 追加 `str`，`b''.join(buf)` 抛 `TypeError` | 写入器未编码 | 四端 InteropFull 向量一致 |
> | Python 空 struct 生成 `__init__(self):` 空函数体 → `IndentationError`，整个模块不可导入 | 生成器缺 `pass` 守卫 | `rpc_import_tests` 与 `Edge`/`InteropEmpty` 用例 |
> | Python 空 struct 多写 1 字节掩码，与其余三端线格式不一致 | 同上的掩码块无守卫 | `InteropEmptyWritesNothing`（四端均 0 字节） |
> | Python `boolReader` 用 `b[p] == '\000'`（int 比 str，恒假）→ `array<bool>` 全读成 `true` | Py2 遗留比较 | `PythonInteropGolden` 往返检查 |
> | Python `stringReader` 返回 `bytes` 而 writer 收 `str`；三处裸 `raise` | 读写不对称 + Py2 裸 raise | 同上 |
> | C# 派生 struct 的嵌套 `FID` 枚举未加 `new` → `CS0108` | 生成器遗漏修饰符 | 接入编译验证后暴露 |
> | C# 空 struct 生成空 `switch` → `CS1522` | 同上 | 同上 |
> | `#import` **四个后端全部不可用**（定义未摊平 + 悬空引用） | 定义循环的 `getFile()` 过滤 | `rpc_import_tests`，四端产物均编译通过 |
> | Go 测试套件因 `fm.Pos undefined` 整包编译失败（约 40 用例不执行） | `FieldMask` 缺 `Pos()` | `go test ./...` 全绿 |
> | Go 测试的 `dynSize`/`int16` 期望值是**测试数据错**（漏掉长度标记 / 写成大端） | 测试写错，实现对 | 逐条对照 §5 规范 |
> | `BUILD_TESTING` 判定顺序错误，测试默认不配置 | `include(CTest)` 顺序 | 默认配置即注册测试 |
> | `version_*` 三个孤儿文件（越界 `skip` 负断言唯一来源） | 从未接入构建 | 断言迁入 `rpc_runtime_edge_tests` 后删除 |
> | `compiler_test` 中两个指向已删除 `bin/` 的必然失败用例 | 路径未随重命名更新 | 由 `rpc_import_tests` 取代 |
>
> **四种语言在已知的全部维度上已一致**（见 §10），
> 由 §11 的黄金向量用例与 Python 一致性测试守护。
> 下列条目均为独立于跨语言互通的其余问题。

### 中等

#### `enum Name : <底层类型>` 是不可用的语法

`实测` · `compiler/rpc.y` 的 `enumeration:` 规则

该分支有两个问题，合起来使这条语法成为死路：

1. 它**不接受成员列表**。`enum E : int64 { A, B };` 报
   `unexpected '{', expecting ';'` —— 带 `{ enum_items }` 的是**另一个**分支，
   而那个分支不处理 `: super`。
2. 它的 action 只做重名检查与 `curEnum_` 初始化，**从不把枚举加入
   `definitions_`**。因此 `enum E : int64;` 能编译通过（退出码 0），
   但该枚举不出现在任何后端的产物里，也不报任何警告。

想带成员写不出来，不带成员则被静默丢弃。四个后端行为一致（都丢弃），
所以不影响跨语言互通，但这条语法目前没有任何可用形态。

覆盖：`CompilerNegative.EnumUnderlyingTypeAcceptsNoMembers` 与
`EnumUnderlyingTypeIsSilentlyDropped` 把当前行为钉住，将来补全或移除
该特性时必须同步更新它们。

#### Go 生成代码的 FieldMask 跳过被类型断言限死

`源码` · `compiler/GoGenerator.cpp:850`

版本兼容的 `Skip` 调用写作 `if reader, ok := reader.(*rpc.MemReader); ok` —— 只有内存读取器会执行跳过。换成流式 / socket 读取器时，**跳过被静默忽略**，后续所有字段偏移全错且不报错。C++ 与 C# 通过接口虚函数完成，没有这个限制。

### 轻微

#### C++ 运行时用 `c_str()` 去 const 写入

`源码` · `runtime/cpp/ProtocolReader.h:82`

```cpp
v.resize(len);
return read((void*)v.c_str(), len);
```

通过 `c_str()` 拿到 const 指针后强制转换并写入，形式上属未定义行为。实测所有 C++ 用例均正常工作，因此这是**代码质量问题而非已观测到的故障**；正确写法是 `&v[0]` 或 `v.data()`。

#### C++ MemWriter 溢出静默丢字节

`源码` · `runtime/cpp/ProtocolMemWriter.h:17`

```cpp
if (space_ < wtptr_ + len) return;
```

`write()` 返回 `void`，缓冲区不足时既不报错也不返回状态，产生被截断的消息。对不可信输入是安全隐患。

#### `build/` 里的二进制可能落后于源码

`实测` · `build/compiler/rpc`

改动生成器后若不重新构建，`build/` 中的 `rpc` 会产出与源码不符的代码。这不是假设：仓库曾长期携带一个构建于命名空间统一提交**之前**的二进制，生成的仍是旧 `arpc` 引用。**任何生成器改动后都必须 `cmake --build build --target rpc`**（CLAUDE.md 规则六）。

#### 未接线的遗留资产

`源码` · `conn/` · `Config.h.in`

`conn/` 是一套依赖 **ACE 框架**的 TCP 连接 / 多路复用库，**不在任何构建中**，仓库里也没有 ACE 依赖，且缺少顶层 `conn/CMakeLists.txt`（只有 `conn/src/` 下的），即使想启用也无法 `add_subdirectory`。`Config.h` 生成后无任何源文件 include。

---

## 13. 仓库地图与分支

| 路径 | 职责 | 参与构建 |
|---|---|---|
| `compiler/` | 编译器：词法、语法、AST、四个后端 | 是 |
| `runtime/cpp/` | C++ 运行时头文件（仅头文件） | 仅 include 路径 |
| `runtime/cs/` | C# 运行时源码 | 由 csproj 引用 |
| `runtime/go/` | Go 运行时包 `rpc` | 由 go.mod 引用 |
| `runtime/py/` | Python 运行时（包名 `rpc`） | 否 |
| `tests/` | GoogleTest 用例、示例 schema、.NET 验证器 | 需显式开启 |
| `conn/` | ACE 连接库遗留 | 否（死代码） |
| `scripts/` | Docker 构建脚本、CSV→schema 转换工具 | — |

> **runtime 不是 CMake 目标**
>
> 根 `CMakeLists.txt` 里 `add_subdirectory(runtime)` 是**被注释掉的**。四个语言运行时都通过各自生态的方式被消费：C++ 靠 include 路径、C# 靠 csproj 的 `<Compile Include>`、Go 靠 module path、Python 靠手工路径。

### 分支拓扑

仓库有两条各自演进了很久的线，共同祖先是 2018 年的初始提交 `11df9de`，之后**再无合并**：

| 分支 | 定位 | 特征 |
|---|---|---|
| `main` ← 当前 | 统一命名后的主线 | 四个后端（cpp/cs/py/go）、`rpc.l`/`rpc.y`、FieldMask 版本兼容、完整测试体系 |
| `master` | 面向 Godot 的分支 | 含 `GodotCppGenerator` 与 `runtime/godot/`，但**没有 Go / Python 后端**，文件名仍是旧的 `bin.l`/`bin.y` |

`master` 并不是 `main` 的旧版本，而是**另一条产品线** —— 它保留了 Godot 游戏引擎方向的生成后端。两者互不包含，切换分支会看到完全不同的编译器。远程指向 `github.com:PHXStudio/rpc.git`，标签 `v0.0.1` 打在 `main` 线的 Go 测试提交上。

### 项目沿革

初始提交（2018-07-11）时项目名为 **bintalk**，带有 ActionScript 与 Erlang 运行时；2026 年 3 月起经历了一轮现代化重写：改为 `rpc`、删除 AS3/Erlang 后端、加入 Go 后端与完整测试、引入 FieldMask 版本兼容，最后在 `923d720` 把散落的 `bin` / `arpc` 命名统一为 `rpc`。那次统一改了运行时却没改完生成器，是 §12 中若干问题的直接来源。

---

*初版基于 `main` 分支 `923d720`（2026-09-17）。同日对测试套件做了一轮修复与补全：
`#import` 四端修复、Python 运行时的字符串/布尔/空 struct 缺陷、C# 的 `CS0108`/`CS1522`、
Go 测试套件恢复可编译、`dynSize`/整数/浮点/JSON 的字节 golden、以及 `InteropFull`
四端共享黄金向量；工具链缺失时用例改为注册后跳过，Docker 的 tester 阶段补齐 python3 与 Go。
测试用例从 126 个（124 通过 / 2 失败）增至 **163 个全部通过**。
所有标注 **实测** 的结论均在重新构建编译器后复现，原始字节输出已列在对应段落中。*
