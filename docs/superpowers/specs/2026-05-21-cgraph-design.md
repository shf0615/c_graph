# cgraph - C 代码知识图谱工具

## 概述

cgraph 是一个面向 C 语言代码库的知识图谱 CLI 工具。通过 libclang 解析 AST 构建图谱，支持 CLI 指令查询（AI agent 使用）和静态 HTML 导出（人类查看）。

**目标规模：** 中型嵌入式项目（1-10万行，几百个文件）

**架构：** 单体 CLI 工具，内部分层（解析层、图存储层、查询层、导出层）

## 数据模型

### 节点类型

| 类型 | 关键属性 |
|------|----------|
| `file` | 路径、总行数、函数数量 |
| `function` | 名称、文件、起止行、圈复杂度、行数、扇入、扇出、是否线程入口、是否外部函数 |
| `struct` / `union` / `enum` | 名称、文件、成员列表 |
| `field` | 名称、类型、所属结构体 |
| `global_var` | 名称、类型、文件、是否共享资源 |
| `macro` | 名称、文件、是否条件编译宏 |
| `typedef` | 名称、底层类型 |
| `sync_primitive` | 名称、类型（mutex/semaphore/spinlock）、文件 |
| `module` | 名称（目录路径）、包含的文件列表 |

### 关系类型

| 关系 | 方向 | 说明 |
|------|------|------|
| `calls` | 函数→函数 | 直接调用 |
| `calls_fp` | 函数→函数 | 通过函数指针调用 |
| `includes` | 文件→文件 | #include |
| `defined_in` | 符号→文件 | 定义位置 |
| `references_type` | 函数→结构体 | 使用了该类型 |
| `accesses_field` | 函数→字段 | 读/写字段（带 r/w 属性） |
| `reads_global` | 函数→全局变量 | |
| `writes_global` | 函数→全局变量 | |
| `allocates` | 函数→函数 | 调用 malloc/calloc 等（边属性记录调用行号，目标为外部函数节点） |
| `frees` | 函数→函数 | 调用 free 等（边属性记录调用行号，目标为外部函数节点） |
| `guarded_by` | 全局变量→锁 | 该资源被该锁保护 |
| `acquires_lock` | 函数→锁 | |
| `creates_thread` | 函数→函数 | pthread_create 等的目标 |
| `ifdef_depends` | 符号→宏 | 该符号在 #ifdef X 块内 |
| `belongs_to` | 文件→模块 | |

## CLI 查询指令集

所有指令输出 JSON。支持通用选项：`--module <name>` 限定范围，`--db <path>` 指定 cgraph.db 位置（默认当前目录），`--project-root <path>` 指定项目根目录（用于将相对路径映射到实际文件）。

### 基础查询

| 指令 | 功能 |
|------|------|
| `cgraph info` | 图谱概览（文件数、函数数、类型数等统计） |
| `cgraph list <type>` | 列出某类节点（functions/files/structs/globals/macros/modules）。配合 `--module` 可过滤指定模块下的节点 |
| `cgraph show <name>` | 查看某个符号的详细信息及直接关系 |

### 调用关系

| 指令 | 功能 |
|------|------|
| `cgraph callers <func> [--depth N]` | 谁调用了该函数（向上追溯） |
| `cgraph callees <func> [--depth N]` | 该函数调用了谁（向下展开） |
| `cgraph callgraph <func> [--depth N]` | 双向调用图 |
| `cgraph path <func_a> <func_b>` | 两函数间的调用路径 |

### 依赖与影响分析

| 指令 | 功能 |
|------|------|
| `cgraph depends <symbol>` | 该符号依赖什么（向下） |
| `cgraph impact <symbol>` | 修改该符号会影响什么（向上传播） |
| `cgraph impact-field <struct.field>` | 修改某结构体字段的影响范围 |
| `cgraph includes <file>` | 该文件的 include 树 |
| `cgraph included-by <file>` | 谁 include 了该文件 |

### 并发分析

| 指令 | 功能 |
|------|------|
| `cgraph threads` | 列出所有线程入口函数 |
| `cgraph shared-resources` | 列出共享资源及其保护锁 |
| `cgraph lock-order` | 锁获取顺序（检测潜在死锁） |
| `cgraph data-race-suspects` | 访问共享资源但未持锁的函数 |

### 平台移植

| 指令 | 功能 |
|------|------|
| `cgraph platform-deps [--ifdef <MACRO>]` | 列出条件编译依赖的符号 |
| `cgraph syscalls` | 列出使用的系统调用/平台 API |
| `cgraph compiler-builtins` | 列出编译器内建函数使用 |

### 代码质量

| 指令 | 功能 |
|------|------|
| `cgraph complexity [--top N]` | 圈复杂度排名 |
| `cgraph coupling [--top N]` | 耦合度排名（扇入×扇出） |
| `cgraph large-functions [--min-lines N]` | 过长函数 |
| `cgraph dead-code` | 未被调用的函数 |

### 性能分析

| 指令 | 功能 |
|------|------|
| `cgraph hotpath <entry> [--depth N]` | 从入口展开的调用链（标注循环和分配点） |
| `cgraph alloc-sites [--func <name>]` | 内存分配点 |
| `cgraph alloc-pairs` | malloc/free 配对分析 |

### 导出

| 指令 | 功能 |
|------|------|
| `cgraph export-html [--output dir]` | 导出静态 HTML 可视化 |

## 构建流程

```
cgraph build --compile-commands compile_commands.json
```

输入：compile_commands.json（由构建系统生成，如 CMake `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` 或 Bear）。该文件精确定义了参与编译的翻译单元及其编译参数。

输出：默认在项目目录（compile_commands.json 所在目录）生成 `cgraph.db`，可通过 `--output <path>` 指定其他路径。

路径约定：图谱中所有文件路径均为相对于项目目录的相对路径，确保图谱可移植。

查询定位：通过 `--project-root <path>` 指定项目根目录。指定时，输出 JSON 中的文件路径为拼接后的绝对路径；不指定时，输出图谱中存储的相对路径。

步骤：
1. 解析 compile_commands.json，获取所有参与编译的 .c 文件及其编译参数
2. 通过 libclang 逐文件解析 AST（头文件通过 #include 关系自动纳入）
3. 遍历 AST 提取节点和关系，写入内存图
4. 计算派生属性（圈复杂度、扇入扇出、模块归属）
5. 序列化到 cgraph.db 二进制文件

## 内部架构

### 代码模块划分

```
src/
├── main.c              # CLI 入口、命令路由
├── parse/
│   ├── parser.c        # libclang AST 遍历
│   ├── extract.c       # 从 AST 节点提取图元素
│   └── ifdef.c         # 条件编译块识别
├── graph/
│   ├── graph.c         # 图结构（邻接表）、节点/边 CRUD
│   ├── serialize.c     # 二进制序列化/反序列化
│   └── metrics.c       # 复杂度、耦合度等计算
├── query/
│   ├── traverse.c      # BFS/DFS 遍历、路径搜索
│   ├── impact.c        # 影响分析（反向传播）
│   ├── concurrency.c   # 锁分析、竞态检测
│   ├── platform.c      # 平台依赖查询
│   └── quality.c       # 代码质量查询
├── export/
│   ├── json.c          # JSON 输出格式化
│   └── html.c          # 静态 HTML 生成
└── util/
    ├── hash.c          # 哈希表（符号名→节点快速查找）
    └── arena.c         # 内存池（统一分配、一次释放）
```

### 核心数据结构

```c
typedef struct {
    Node *nodes;         // 动态数组
    Edge *edges;         // 动态数组
    HashMap name_index;  // 符号名 → node_id 快速查找
    Arena arena;         // 所有节点/边的内存池
} Graph;

typedef struct {
    uint32_t id;
    NodeType type;       // FUNC / STRUCT / FILE / ...
    char *name;
    char *file;
    uint32_t line_start, line_end;
    union { ... };       // 各类型特有属性
} Node;

typedef struct {
    uint32_t from, to;
    EdgeType type;       // CALLS / INCLUDES / ...
    uint8_t attrs;       // 如 r/w 标记
} Edge;
```

### 序列化格式

简单二进制：header（magic + version + 节点数 + 边数）→ 节点数组 → 边数组 → 字符串表。加载时一次读入内存。

### 依赖

- 构建时：libclang（系统安装）
- 运行时：仅解析阶段依赖 libclang 动态库；查询/导出阶段零外部依赖

## 静态 HTML 导出

### 生成方式

`cgraph export-html --output ./html/` 生成自包含目录：

```
html/
├── index.html          # 入口页面
├── data.json           # 完整图谱数据
├── app.js              # 交互逻辑（搜索、过滤）
└── style.css           # 样式
```

浏览器直接打开 index.html，无需服务器（file:// 协议可用）。

### 页面内容

- **首页/概览：** 项目统计、模块列表
- **模块视图：** 文件和函数列表、模块间依赖关系力导向图
- **函数视图：** 属性、调用关系图（上下游各 2 层）、访问的全局变量和类型
- **全局视图：** 文件 include 关系图、调用图全局缩略

### 技术选型

- 图可视化：D3.js force-directed（内嵌，无 CDN 依赖）
- 搜索：前端 JSON 内存过滤
- 所有资源内联或本地，确保离线可用

## AI skill.md 场景指南

skill.md 按场景组织，指导 AI 在每个场景下该执行哪些指令组合：

### 理解代码库
1. `cgraph info` — 获取整体规模
2. `cgraph list modules` — 了解模块划分
3. `cgraph list functions --module <name>` — 逐模块查看核心函数
4. `cgraph callgraph <entry_func> --depth 2` — 从入口理解主流程

### 生成架构文档
1. `cgraph info` — 项目概览数据
2. `cgraph list modules` — 模块列表
3. `cgraph includes <核心文件>` — 依赖关系
4. `cgraph coupling --top 10` — 关键耦合点

### Bug 定位
1. `cgraph show <可疑函数>` — 查看函数详情
2. `cgraph callers <func> --depth 3` — 谁触发了它
3. `cgraph impact-field <struct.field>` — 如果怀疑数据问题
4. `cgraph data-race-suspects` — 如果怀疑并发问题
5. `cgraph path <func_a> <func_b>` — 验证执行路径假设

### 新增需求影响分析
1. `cgraph list modules` — 确定涉及的模块
2. `cgraph show <相关接口函数>` — 了解现有接口
3. `cgraph callees <func> --depth 2` — 该接口下游依赖
4. `cgraph depends <symbol>` — 新增点的依赖

### 变更需求影响分析
1. `cgraph impact <要修改的符号>` — 直接影响范围
2. `cgraph impact-field <struct.field>` — 字段级影响
3. `cgraph included-by <相关头文件>` — 头文件变更波及
4. `cgraph callers <func> --depth 3` — 调用者影响

### 代码质量扫描
1. `cgraph complexity --top 20` — 最复杂的函数
2. `cgraph coupling --top 20` — 最耦合的模块
3. `cgraph large-functions --min-lines 100` — 过长函数
4. `cgraph dead-code` — 死代码

### 平台移植
1. `cgraph platform-deps` — 全部平台依赖
2. `cgraph platform-deps --ifdef <目标平台宏>` — 特定平台代码
3. `cgraph syscalls` — 系统调用列表
4. `cgraph compiler-builtins` — 编译器内建函数

### 性能分析
1. `cgraph hotpath <入口> --depth 5` — 热路径
2. `cgraph alloc-sites` — 内存分配点
3. `cgraph alloc-pairs` — 分配释放配对
4. `cgraph complexity --top 10` — 复杂热点

### 并发分析
1. `cgraph threads` — 所有线程入口
2. `cgraph shared-resources` — 共享资源列表
3. `cgraph data-race-suspects` — 潜在竞态
4. `cgraph lock-order` — 锁序检查
