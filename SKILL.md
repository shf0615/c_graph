---
name: cgraph
description: C 代码知识图谱查询工具。用于理解 C 代码库结构、调用关系、影响分析、并发分析、代码质量扫描。触发条件：对 C 项目进行代码理解、影响分析、重构规划、bug 定位、架构审查时。
---

# cgraph - C 代码知识图谱 Skill

## 工具路径

```
CGRAPH=/home/shf/workspace-ai/new_work_521/c_graph/build/src/cgraph
```

所有命令格式：`$CGRAPH <command> [args] --db <project>/cgraph.db`

## 前置条件

项目目录下需存在 `cgraph.db`。如果没有，先构建：

```bash
# 1. 确保项目有 compile_commands.json（CMake: -DCMAKE_EXPORT_COMPILE_COMMANDS=ON，或用 bear）
# 2. 构建图谱
$CGRAPH build --compile-commands <project>/compile_commands.json --output <project>/cgraph.db
```

## 通用选项

- `--db <path>` — cgraph.db 路径（默认当前目录）
- `--project-root <path>` — 指定后输出绝对路径
- `--module <name>` — 限定范围到指定模块
- `--depth N` — 遍历深度（默认 3）
- `--top N` — 排名数量（默认 10）

## 输出格式

所有命令输出 JSON。直接 pipe 到 `python3 -m json.tool` 可格式化阅读。

---

## 场景指南

### 场景 1：理解代码库

首次接触一个 C 项目时，按顺序执行：

```bash
$CGRAPH info --db <db>                           # 整体规模
$CGRAPH list functions --db <db>                  # 所有函数
$CGRAPH list globals --db <db>                    # 全局变量
$CGRAPH callees <入口函数> --depth 2 --db <db>    # 从入口理解主流程
```

### 场景 2：Bug 定位

已知可疑函数或症状时：

```bash
$CGRAPH show <可疑函数> --db <db>                 # 函数详情（位置、复杂度、扇入扇出）
$CGRAPH callers <func> --depth 3 --db <db>        # 谁调用了它（向上追溯触发路径）
$CGRAPH callees <func> --depth 2 --db <db>        # 它调用了谁（向下查看副作用）
$CGRAPH path <func_a> <func_b> --db <db>          # 验证两函数间是否存在调用路径
$CGRAPH data-race-suspects --db <db>              # 如果怀疑并发问题
```

### 场景 3：变更影响分析

修改某个符号前，评估波及范围：

```bash
$CGRAPH impact <要修改的符号> --db <db>            # 修改该符号会影响哪些函数
$CGRAPH callers <func> --depth 3 --db <db>        # 调用者链
$CGRAPH impact-field <struct.field> --db <db>     # 如果修改结构体字段
$CGRAPH included-by <header.h> --db <db>          # 头文件修改波及哪些文件
```

### 场景 4：新增功能规划

决定在哪里加代码、需要了解哪些接口：

```bash
$CGRAPH show <相关接口函数> --db <db>              # 了解现有接口签名和位置
$CGRAPH callees <func> --depth 2 --db <db>        # 该接口的下游依赖
$CGRAPH depends <symbol> --db <db>                # 符号的完整依赖
$CGRAPH coupling --top 10 --db <db>               # 耦合度高的模块（避免加重）
```

### 场景 5：代码质量扫描

重构前摸底或 code review 辅助：

```bash
$CGRAPH complexity --top 20 --db <db>             # 圈复杂度最高的函数
$CGRAPH coupling --top 20 --db <db>               # 耦合度最高的模块
$CGRAPH large-functions --min-lines 80 --db <db>  # 过长函数
$CGRAPH dead-code --db <db>                       # 未被调用的函数
```

### 场景 6：并发安全审查

多线程项目审查：

```bash
$CGRAPH threads --db <db>                         # 所有线程入口函数
$CGRAPH shared-resources --db <db>                # 共享资源及其保护锁
$CGRAPH data-race-suspects --db <db>              # 访问共享资源但未持锁的函数
$CGRAPH lock-order --db <db>                      # 锁获取顺序（死锁检测）
```

### 场景 7：性能分析

定位性能瓶颈和内存问题：

```bash
$CGRAPH hotpath <入口> --depth 5 --db <db>        # 从入口展开的调用热路径
$CGRAPH alloc-sites --db <db>                     # 所有内存分配点
$CGRAPH alloc-pairs --db <db>                     # malloc/free 配对（检测泄漏）
$CGRAPH complexity --top 10 --db <db>             # 复杂度热点
```

### 场景 8：平台移植评估

评估代码的平台依赖：

```bash
$CGRAPH platform-deps --db <db>                   # 所有条件编译依赖
$CGRAPH platform-deps --ifdef STM32F4 --db <db>   # 特定平台的代码
$CGRAPH syscalls --db <db>                        # 系统调用/平台 API 列表
$CGRAPH compiler-builtins --db <db>               # 编译器内建函数
```

---

## 使用原则

1. **先 info，后深入** — 任何分析先跑 `info` 了解规模，避免盲目查询。
2. **组合使用** — 单条命令给出局部信息，多条组合才能得出结论。
3. **验证假设** — 用 `path` 验证"A 是否能调用到 B"，用 `impact` 验证"改 X 是否影响 Y"。
4. **输出即证据** — 将 cgraph 输出作为分析依据引用，不要凭记忆猜测调用关系。
