# cgraph - C 代码知识图谱工具

cgraph 是一个面向 C 语言代码库的静态分析工具，通过 libclang 解析 AST 构建知识图谱，支持调用关系查询、影响分析、并发分析、代码质量检查等功能。

**适用规模：** 中型嵌入式项目（1-10万行，几百个文件）

## 依赖

- CMake >= 3.16
- C17 编译器（GCC / Clang）
- libclang-dev（推荐 LLVM 18）

Ubuntu/Debian 安装依赖：

```bash
sudo apt install cmake build-essential libclang-18-dev
```

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

编译产物为 `build/src/cgraph`。

## 快速开始

### 1. 生成 compile_commands.json

对于 CMake 项目：

```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build
```

或使用 [Bear](https://github.com/rizsotto/Bear)：

```bash
bear -- make
```

### 2. 构建图谱

```bash
cgraph build --compile-commands compile_commands.json --output cgraph.db
```

### 3. 查询

```bash
# 图谱概览
cgraph info

# 列出所有函数
cgraph list functions

# 查看某个符号详情
cgraph show main

# 谁调用了该函数
cgraph callers my_func --depth 3

# 该函数调用了谁
cgraph callees my_func --depth 3

# 两函数间的调用路径
cgraph path func_a func_b

# 修改该符号的影响范围
cgraph impact my_func

# 圈复杂度排名
cgraph complexity --top 10

# 耦合度排名
cgraph coupling --top 10

# 过长函数
cgraph large-functions --min-lines 100

# 死代码检测
cgraph dead-code
```

### 4. 并发分析

```bash
# 列出线程入口
cgraph threads

# 共享资源及保护锁
cgraph shared-resources

# 锁获取顺序（死锁检测）
cgraph lock-order

# 疑似数据竞争
cgraph data-race-suspects
```

### 5. 性能分析

```bash
# 热路径分析
cgraph hotpath main --depth 5

# 内存分配点
cgraph alloc-sites

# malloc/free 配对
cgraph alloc-pairs
```

### 6. 平台移植分析

```bash
# 条件编译依赖
cgraph platform-deps --ifdef STM32F4

# 系统调用
cgraph syscalls
```

### 7. 导出 HTML 可视化

```bash
cgraph export-html --output ./report
```

## 通用选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--db <path>` | 指定 cgraph.db 路径 | `cgraph.db` |
| `--project-root <path>` | 项目根目录（输出绝对路径） | 无（输出相对路径） |
| `--module <name>` | 限定查询范围到指定模块 | 无 |
| `--depth N` | 遍历深度 | 3 |
| `--top N` | 排名数量 | 10 |
| `--min-lines N` | 最小行数阈值 | 100 |

## 输出格式

所有查询命令输出 JSON，便于脚本处理和 AI agent 集成。

## 运行测试

```bash
cd build
cmake .. -DCGRAPH_BUILD_TESTS=ON
make
ctest --output-on-failure
```

## 项目结构

```
src/
├── main.c              # CLI 入口
├── parse/              # libclang AST 解析
├── graph/              # 图存储、序列化、度量计算
├── query/              # 各类查询实现
├── export/             # JSON/HTML 导出
└── util/               # 内存池、哈希表等基础设施
```

## 许可证

MIT
