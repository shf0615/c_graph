# cgraph Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a CLI tool that parses C codebases via libclang, constructs a knowledge graph, and supports JSON queries + static HTML export.

**Architecture:** Single C binary, layered as parse → graph → query → export. Arena allocator for all graph memory. Adjacency list with hash index for O(1) symbol lookup. Binary serialization for persistence.

**Tech Stack:** C17, libclang, CMake, Unity test framework, D3.js (embedded in HTML export)

---

## Phase 1: Foundation (util + graph core)

### Task 1: Project scaffolding + CMake setup

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.c`
- Create: `src/util/arena.h`
- Create: `src/util/arena.c`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_arena.c`
- Create: `vendor/unity/unity.h`
- Create: `vendor/unity/unity.c`
- Create: `vendor/unity/unity_internals.h`

- [ ] **Step 1: Create top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(cgraph C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CGRAPH_BUILD_TESTS "Build tests" ON)

# Find libclang
find_package(Clang REQUIRED CONFIG)

add_subdirectory(src)

if(CGRAPH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: Create src/CMakeLists.txt**

```cmake
# src/CMakeLists.txt
add_library(cgraph_lib STATIC
    util/arena.c
)
target_include_directories(cgraph_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cgraph main.c)
target_link_libraries(cgraph PRIVATE cgraph_lib)
```

- [ ] **Step 3: Implement arena allocator**

```c
// src/util/arena.h
#ifndef CGRAPH_ARENA_H
#define CGRAPH_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t size;
    size_t used;
    uint8_t data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
    ArenaBlock *current;
    size_t default_block_size;
} Arena;

void arena_init(Arena *a, size_t default_block_size);
void *arena_alloc(Arena *a, size_t size);
char *arena_strdup(Arena *a, const char *s);
void arena_destroy(Arena *a);

#endif
```

```c
// src/util/arena.c
#include "util/arena.h"
#include <stdlib.h>
#include <string.h>

static ArenaBlock *block_new(size_t size) {
    ArenaBlock *b = malloc(sizeof(ArenaBlock) + size);
    if (!b) return NULL;
    b->next = NULL;
    b->size = size;
    b->used = 0;
    return b;
}

void arena_init(Arena *a, size_t default_block_size) {
    a->default_block_size = default_block_size ? default_block_size : (64 * 1024);
    a->head = block_new(a->default_block_size);
    a->current = a->head;
}

void *arena_alloc(Arena *a, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~(size_t)7;

    if (a->current->used + size > a->current->size) {
        size_t block_size = size > a->default_block_size ? size : a->default_block_size;
        ArenaBlock *b = block_new(block_size);
        if (!b) return NULL;
        a->current->next = b;
        a->current = b;
    }
    void *ptr = a->current->data + a->current->used;
    a->current->used += size;
    return ptr;
}

char *arena_strdup(Arena *a, const char *s) {
    size_t len = strlen(s) + 1;
    char *dst = arena_alloc(a, len);
    if (dst) memcpy(dst, s, len);
    return dst;
}

void arena_destroy(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
    a->current = NULL;
}
```

- [ ] **Step 4: Write arena tests**

```c
// tests/test_arena.c
#include "unity.h"
#include "util/arena.h"

void setUp(void) {}
void tearDown(void) {}

void test_arena_basic_alloc(void) {
    Arena a;
    arena_init(&a, 1024);
    void *p1 = arena_alloc(&a, 100);
    void *p2 = arena_alloc(&a, 200);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT(p2 > p1);
    arena_destroy(&a);
}

void test_arena_strdup(void) {
    Arena a;
    arena_init(&a, 1024);
    char *s = arena_strdup(&a, "hello");
    TEST_ASSERT_EQUAL_STRING("hello", s);
    arena_destroy(&a);
}

void test_arena_large_alloc(void) {
    Arena a;
    arena_init(&a, 64);
    // Alloc larger than block size
    void *p = arena_alloc(&a, 128);
    TEST_ASSERT_NOT_NULL(p);
    arena_destroy(&a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_basic_alloc);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_large_alloc);
    return UNITY_END();
}
```

- [ ] **Step 5: Create tests/CMakeLists.txt**

```cmake
add_library(unity STATIC ${CMAKE_SOURCE_DIR}/vendor/unity/unity.c)
target_include_directories(unity PUBLIC ${CMAKE_SOURCE_DIR}/vendor/unity)

add_executable(test_arena test_arena.c)
target_link_libraries(test_arena PRIVATE cgraph_lib unity)
target_include_directories(test_arena PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_arena COMMAND test_arena)
```

- [ ] **Step 6: Create minimal main.c**

```c
// src/main.c
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: cgraph <command> [options]\n");
        return 1;
    }
    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    return 1;
}
```

- [ ] **Step 7: Download Unity test framework, build and run tests**

```bash
# Download Unity (single-file test framework)
mkdir -p vendor/unity
curl -sL https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.h -o vendor/unity/unity.h
curl -sL https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.c -o vendor/unity/unity.c
curl -sL https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity_internals.h -o vendor/unity/unity_internals.h

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Test
cd build && ctest --output-on-failure
```

Expected: All 3 arena tests pass.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt src/ tests/ vendor/
git commit -m "feat: project scaffolding with arena allocator and tests"
```

---

### Task 2: Hash map implementation

**Files:**
- Create: `src/util/hash.h`
- Create: `src/util/hash.c`
- Create: `tests/test_hash.c`
- Modify: `src/CMakeLists.txt` (add hash.c)
- Modify: `tests/CMakeLists.txt` (add test_hash)

- [ ] **Step 1: Write hash map tests**

```c
// tests/test_hash.c
#include "unity.h"
#include "util/hash.h"

void setUp(void) {}
void tearDown(void) {}

void test_hashmap_put_get(void) {
    HashMap m;
    hashmap_init(&m, 16);
    hashmap_put(&m, "foo", 42);
    uint32_t val;
    TEST_ASSERT_TRUE(hashmap_get(&m, "foo", &val));
    TEST_ASSERT_EQUAL_UINT32(42, val);
    hashmap_destroy(&m);
}

void test_hashmap_missing_key(void) {
    HashMap m;
    hashmap_init(&m, 16);
    uint32_t val;
    TEST_ASSERT_FALSE(hashmap_get(&m, "missing", &val));
    hashmap_destroy(&m);
}

void test_hashmap_overwrite(void) {
    HashMap m;
    hashmap_init(&m, 16);
    hashmap_put(&m, "key", 1);
    hashmap_put(&m, "key", 2);
    uint32_t val;
    hashmap_get(&m, "key", &val);
    TEST_ASSERT_EQUAL_UINT32(2, val);
    hashmap_destroy(&m);
}

void test_hashmap_grow(void) {
    HashMap m;
    hashmap_init(&m, 4);
    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        hashmap_put(&m, key, i);
    }
    uint32_t val;
    TEST_ASSERT_TRUE(hashmap_get(&m, "key99", &val));
    TEST_ASSERT_EQUAL_UINT32(99, val);
    hashmap_destroy(&m);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hashmap_put_get);
    RUN_TEST(test_hashmap_missing_key);
    RUN_TEST(test_hashmap_overwrite);
    RUN_TEST(test_hashmap_grow);
    return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_hash
```

Expected: Build fails (hash.h not found).

- [ ] **Step 3: Implement hash map**

```c
// src/util/hash.h
#ifndef CGRAPH_HASH_H
#define CGRAPH_HASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *key;       // owned, strdup'd
    uint32_t value;
    bool occupied;
} HashEntry;

typedef struct {
    HashEntry *entries;
    size_t capacity;
    size_t count;
} HashMap;

void hashmap_init(HashMap *m, size_t initial_capacity);
void hashmap_put(HashMap *m, const char *key, uint32_t value);
bool hashmap_get(const HashMap *m, const char *key, uint32_t *out_value);
bool hashmap_remove(HashMap *m, const char *key);
void hashmap_destroy(HashMap *m);

#endif
```

```c
// src/util/hash.c
#include "util/hash.h"
#include <stdlib.h>
#include <string.h>

static uint32_t fnv1a(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) {
        h ^= (uint8_t)*s;
        h *= 16777619u;
    }
    return h;
}

void hashmap_init(HashMap *m, size_t initial_capacity) {
    m->capacity = initial_capacity < 8 ? 8 : initial_capacity;
    m->count = 0;
    m->entries = calloc(m->capacity, sizeof(HashEntry));
}

static void hashmap_resize(HashMap *m) {
    size_t old_cap = m->capacity;
    HashEntry *old = m->entries;
    m->capacity *= 2;
    m->entries = calloc(m->capacity, sizeof(HashEntry));
    m->count = 0;
    for (size_t i = 0; i < old_cap; i++) {
        if (old[i].occupied) {
            hashmap_put(m, old[i].key, old[i].value);
            free(old[i].key);
        }
    }
    free(old);
}

void hashmap_put(HashMap *m, const char *key, uint32_t value) {
    if (m->count * 10 >= m->capacity * 7) {
        hashmap_resize(m);
    }
    uint32_t idx = fnv1a(key) % m->capacity;
    while (m->entries[idx].occupied) {
        if (strcmp(m->entries[idx].key, key) == 0) {
            m->entries[idx].value = value;
            return;
        }
        idx = (idx + 1) % m->capacity;
    }
    m->entries[idx].key = strdup(key);
    m->entries[idx].value = value;
    m->entries[idx].occupied = true;
    m->count++;
}

bool hashmap_get(const HashMap *m, const char *key, uint32_t *out_value) {
    uint32_t idx = fnv1a(key) % m->capacity;
    while (m->entries[idx].occupied) {
        if (strcmp(m->entries[idx].key, key) == 0) {
            *out_value = m->entries[idx].value;
            return true;
        }
        idx = (idx + 1) % m->capacity;
    }
    return false;
}

bool hashmap_remove(HashMap *m, const char *key) {
    uint32_t idx = fnv1a(key) % m->capacity;
    while (m->entries[idx].occupied) {
        if (strcmp(m->entries[idx].key, key) == 0) {
            free(m->entries[idx].key);
            m->entries[idx].occupied = false;
            m->count--;
            // Rehash subsequent entries
            uint32_t next = (idx + 1) % m->capacity;
            while (m->entries[next].occupied) {
                HashEntry e = m->entries[next];
                m->entries[next].occupied = false;
                m->count--;
                hashmap_put(m, e.key, e.value);
                free(e.key);
                next = (next + 1) % m->capacity;
            }
            return true;
        }
        idx = (idx + 1) % m->capacity;
    }
    return false;
}

void hashmap_destroy(HashMap *m) {
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].occupied) free(m->entries[i].key);
    }
    free(m->entries);
    m->entries = NULL;
    m->capacity = 0;
    m->count = 0;
}
```

- [ ] **Step 4: Update CMakeLists, build, run tests**

Add `util/hash.c` to `src/CMakeLists.txt`. Add `test_hash` target to `tests/CMakeLists.txt`.

```bash
cmake --build build && cd build && ctest --output-on-failure
```

Expected: All hash tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/util/hash.h src/util/hash.c tests/test_hash.c src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add hash map with open addressing and auto-resize"
```

---

### Task 3: Graph core data structure

**Files:**
- Create: `src/graph/graph.h`
- Create: `src/graph/graph.c`
- Create: `tests/test_graph.c`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write graph tests**

```c
// tests/test_graph.c
#include "unity.h"
#include "graph/graph.h"

void setUp(void) {}
void tearDown(void) {}

void test_graph_add_node(void) {
    Graph g;
    graph_init(&g);
    uint32_t id = graph_add_node(&g, NODE_FUNCTION, "main", "src/main.c", 10, 50);
    TEST_ASSERT_EQUAL_UINT32(0, id);
    Node *n = graph_get_node(&g, id);
    TEST_ASSERT_EQUAL_STRING("main", n->name);
    TEST_ASSERT_EQUAL(NODE_FUNCTION, n->type);
    graph_destroy(&g);
}

void test_graph_add_edge(void) {
    Graph g;
    graph_init(&g);
    uint32_t a = graph_add_node(&g, NODE_FUNCTION, "caller", "a.c", 1, 10);
    uint32_t b = graph_add_node(&g, NODE_FUNCTION, "callee", "b.c", 1, 5);
    graph_add_edge(&g, a, b, EDGE_CALLS, 0);
    EdgeList edges = graph_edges_from(&g, a);
    TEST_ASSERT_EQUAL_UINT32(1, edges.count);
    TEST_ASSERT_EQUAL_UINT32(b, edges.edges[0].to);
    TEST_ASSERT_EQUAL(EDGE_CALLS, edges.edges[0].type);
    graph_destroy(&g);
}

void test_graph_lookup_by_name(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "foo", "x.c", 1, 10);
    graph_add_node(&g, NODE_FUNCTION, "bar", "x.c", 11, 20);
    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "bar", &id));
    TEST_ASSERT_EQUAL_UINT32(1, id);
    TEST_ASSERT_FALSE(graph_find_node(&g, "baz", &id));
    graph_destroy(&g);
}

void test_graph_edges_to(void) {
    Graph g;
    graph_init(&g);
    uint32_t a = graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    uint32_t b = graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    uint32_t c = graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_edge(&g, a, c, EDGE_CALLS, 0);
    graph_add_edge(&g, b, c, EDGE_CALLS, 0);
    EdgeList edges = graph_edges_to(&g, c);
    TEST_ASSERT_EQUAL_UINT32(2, edges.count);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_graph_add_node);
    RUN_TEST(test_graph_add_edge);
    RUN_TEST(test_graph_lookup_by_name);
    RUN_TEST(test_graph_edges_to);
    return UNITY_END();
}
```

- [ ] **Step 2: Implement graph core**

```c
// src/graph/graph.h
#ifndef CGRAPH_GRAPH_H
#define CGRAPH_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include "util/arena.h"
#include "util/hash.h"

typedef enum {
    NODE_FILE,
    NODE_FUNCTION,
    NODE_STRUCT,
    NODE_UNION,
    NODE_ENUM,
    NODE_FIELD,
    NODE_GLOBAL_VAR,
    NODE_MACRO,
    NODE_TYPEDEF,
    NODE_SYNC_PRIMITIVE,
    NODE_MODULE,
} NodeType;

typedef enum {
    EDGE_CALLS,
    EDGE_CALLS_FP,
    EDGE_INCLUDES,
    EDGE_DEFINED_IN,
    EDGE_REFERENCES_TYPE,
    EDGE_ACCESSES_FIELD,
    EDGE_READS_GLOBAL,
    EDGE_WRITES_GLOBAL,
    EDGE_ALLOCATES,
    EDGE_FREES,
    EDGE_GUARDED_BY,
    EDGE_ACQUIRES_LOCK,
    EDGE_CREATES_THREAD,
    EDGE_IFDEF_DEPENDS,
    EDGE_BELONGS_TO,
} EdgeType;

typedef struct {
    uint32_t id;
    NodeType type;
    char *name;
    char *file;
    uint32_t line_start;
    uint32_t line_end;
    // Extended attributes (type-specific)
    uint32_t cyclomatic_complexity;
    uint32_t fan_in;
    uint32_t fan_out;
    bool is_thread_entry;
    bool is_external;
    bool is_shared;
} Node;

typedef struct {
    uint32_t from;
    uint32_t to;
    EdgeType type;
    uint8_t attrs;  // e.g., read/write flags
} Edge;

typedef struct {
    Edge *edges;
    uint32_t count;
} EdgeList;

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_capacity;

    Edge *edges;
    uint32_t edge_count;
    uint32_t edge_capacity;

    HashMap name_index;  // name -> node_id
    Arena arena;         // string storage
} Graph;

void graph_init(Graph *g);
void graph_destroy(Graph *g);

uint32_t graph_add_node(Graph *g, NodeType type, const char *name,
                        const char *file, uint32_t line_start, uint32_t line_end);
Node *graph_get_node(Graph *g, uint32_t id);
bool graph_find_node(const Graph *g, const char *name, uint32_t *out_id);

void graph_add_edge(Graph *g, uint32_t from, uint32_t to, EdgeType type, uint8_t attrs);
EdgeList graph_edges_from(const Graph *g, uint32_t node_id);
EdgeList graph_edges_to(const Graph *g, uint32_t node_id);

#endif
```

```c
// src/graph/graph.c
#include "graph/graph.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_NODES 256
#define INITIAL_EDGES 1024

void graph_init(Graph *g) {
    arena_init(&g->arena, 64 * 1024);
    g->node_count = 0;
    g->node_capacity = INITIAL_NODES;
    g->nodes = calloc(INITIAL_NODES, sizeof(Node));
    g->edge_count = 0;
    g->edge_capacity = INITIAL_EDGES;
    g->edges = calloc(INITIAL_EDGES, sizeof(Edge));
    hashmap_init(&g->name_index, 512);
}

void graph_destroy(Graph *g) {
    free(g->nodes);
    free(g->edges);
    hashmap_destroy(&g->name_index);
    arena_destroy(&g->arena);
}

uint32_t graph_add_node(Graph *g, NodeType type, const char *name,
                        const char *file, uint32_t line_start, uint32_t line_end) {
    if (g->node_count >= g->node_capacity) {
        g->node_capacity *= 2;
        g->nodes = realloc(g->nodes, g->node_capacity * sizeof(Node));
    }
    uint32_t id = g->node_count++;
    Node *n = &g->nodes[id];
    memset(n, 0, sizeof(Node));
    n->id = id;
    n->type = type;
    n->name = arena_strdup(&g->arena, name);
    n->file = file ? arena_strdup(&g->arena, file) : NULL;
    n->line_start = line_start;
    n->line_end = line_end;
    hashmap_put(&g->name_index, name, id);
    return id;
}

Node *graph_get_node(Graph *g, uint32_t id) {
    if (id >= g->node_count) return NULL;
    return &g->nodes[id];
}

bool graph_find_node(const Graph *g, const char *name, uint32_t *out_id) {
    return hashmap_get(&g->name_index, name, out_id);
}

void graph_add_edge(Graph *g, uint32_t from, uint32_t to, EdgeType type, uint8_t attrs) {
    if (g->edge_count >= g->edge_capacity) {
        g->edge_capacity *= 2;
        g->edges = realloc(g->edges, g->edge_capacity * sizeof(Edge));
    }
    Edge *e = &g->edges[g->edge_count++];
    e->from = from;
    e->to = to;
    e->type = type;
    e->attrs = attrs;
}

EdgeList graph_edges_from(const Graph *g, uint32_t node_id) {
    // Linear scan (fine for target scale; optimize later if needed)
    Edge *result = malloc(g->edge_count * sizeof(Edge));
    uint32_t count = 0;
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from == node_id) {
            result[count++] = g->edges[i];
        }
    }
    result = realloc(result, count * sizeof(Edge));
    return (EdgeList){.edges = result, .count = count};
}

EdgeList graph_edges_to(const Graph *g, uint32_t node_id) {
    Edge *result = malloc(g->edge_count * sizeof(Edge));
    uint32_t count = 0;
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == node_id) {
            result[count++] = g->edges[i];
        }
    }
    result = realloc(result, count * sizeof(Edge));
    return (EdgeList){.edges = result, .count = count};
}
```

- [ ] **Step 3: Update CMakeLists, build, run tests**

Add `graph/graph.c` to `src/CMakeLists.txt`. Add `test_graph` to `tests/CMakeLists.txt`.

```bash
cmake --build build && cd build && ctest --output-on-failure
```

Expected: All graph tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/graph/ tests/test_graph.c src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: graph core with nodes, edges, and name index"
```

---

### Task 4: Binary serialization

**Files:**
- Create: `src/graph/serialize.h`
- Create: `src/graph/serialize.c`
- Create: `tests/test_serialize.c`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write serialization tests**

```c
// tests/test_serialize.c
#include "unity.h"
#include "graph/graph.h"
#include "graph/serialize.h"
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

void test_serialize_roundtrip(void) {
    Graph g;
    graph_init(&g);
    uint32_t a = graph_add_node(&g, NODE_FUNCTION, "main", "main.c", 1, 20);
    uint32_t b = graph_add_node(&g, NODE_FUNCTION, "helper", "util.c", 5, 15);
    graph_add_edge(&g, a, b, EDGE_CALLS, 0);

    const char *path = "/tmp/test_cgraph.db";
    TEST_ASSERT_TRUE(graph_serialize(&g, path));
    graph_destroy(&g);

    Graph g2;
    TEST_ASSERT_TRUE(graph_deserialize(&g2, path));
    TEST_ASSERT_EQUAL_UINT32(2, g2.node_count);
    TEST_ASSERT_EQUAL_UINT32(1, g2.edge_count);
    TEST_ASSERT_EQUAL_STRING("main", g2.nodes[0].name);
    TEST_ASSERT_EQUAL_STRING("helper", g2.nodes[1].name);
    TEST_ASSERT_EQUAL_UINT32(a, g2.edges[0].from);
    TEST_ASSERT_EQUAL_UINT32(b, g2.edges[0].to);

    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g2, "helper", &id));
    TEST_ASSERT_EQUAL_UINT32(1, id);

    graph_destroy(&g2);
    remove(path);
}

void test_serialize_empty_graph(void) {
    Graph g;
    graph_init(&g);
    const char *path = "/tmp/test_cgraph_empty.db";
    TEST_ASSERT_TRUE(graph_serialize(&g, path));
    graph_destroy(&g);

    Graph g2;
    TEST_ASSERT_TRUE(graph_deserialize(&g2, path));
    TEST_ASSERT_EQUAL_UINT32(0, g2.node_count);
    TEST_ASSERT_EQUAL_UINT32(0, g2.edge_count);
    graph_destroy(&g2);
    remove(path);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_roundtrip);
    RUN_TEST(test_serialize_empty_graph);
    return UNITY_END();
}
```

- [ ] **Step 2: Implement serialization**

```c
// src/graph/serialize.h
#ifndef CGRAPH_SERIALIZE_H
#define CGRAPH_SERIALIZE_H

#include "graph/graph.h"
#include <stdbool.h>

// File format:
// [magic 4B "CGRP"] [version u32] [node_count u32] [edge_count u32] [string_table_size u32]
// [nodes: id, type, name_offset, file_offset, line_start, line_end, complexity, fan_in, fan_out, flags]
// [edges: from, to, type, attrs]
// [string_table: null-terminated strings packed]

bool graph_serialize(const Graph *g, const char *path);
bool graph_deserialize(Graph *g, const char *path);

#endif
```

```c
// src/graph/serialize.c
#include "graph/serialize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0x50524743  // "CGRP" little-endian
#define VERSION 1

typedef struct {
    char *data;
    uint32_t size;
    uint32_t capacity;
    HashMap offsets;  // string -> offset
} StringTable;

static void strtab_init(StringTable *st) {
    st->capacity = 4096;
    st->data = malloc(st->capacity);
    st->size = 0;
    hashmap_init(&st->offsets, 256);
}

static uint32_t strtab_add(StringTable *st, const char *s) {
    if (!s) return UINT32_MAX;
    uint32_t existing;
    if (hashmap_get(&st->offsets, s, &existing)) return existing;

    uint32_t offset = st->size;
    size_t len = strlen(s) + 1;
    while (st->size + len > st->capacity) {
        st->capacity *= 2;
        st->data = realloc(st->data, st->capacity);
    }
    memcpy(st->data + st->size, s, len);
    st->size += len;
    hashmap_put(&st->offsets, s, offset);
    return offset;
}

static void strtab_destroy(StringTable *st) {
    free(st->data);
    hashmap_destroy(&st->offsets);
}

bool graph_serialize(const Graph *g, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    StringTable st;
    strtab_init(&st);

    // Collect strings
    uint32_t *name_offsets = malloc(g->node_count * sizeof(uint32_t));
    uint32_t *file_offsets = malloc(g->node_count * sizeof(uint32_t));
    for (uint32_t i = 0; i < g->node_count; i++) {
        name_offsets[i] = strtab_add(&st, g->nodes[i].name);
        file_offsets[i] = strtab_add(&st, g->nodes[i].file);
    }

    // Header
    uint32_t magic = MAGIC, version = VERSION;
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&g->node_count, 4, 1, f);
    fwrite(&g->edge_count, 4, 1, f);
    fwrite(&st.size, 4, 1, f);

    // Nodes
    for (uint32_t i = 0; i < g->node_count; i++) {
        Node *n = &g->nodes[i];
        fwrite(&n->id, 4, 1, f);
        uint32_t type = n->type;
        fwrite(&type, 4, 1, f);
        fwrite(&name_offsets[i], 4, 1, f);
        fwrite(&file_offsets[i], 4, 1, f);
        fwrite(&n->line_start, 4, 1, f);
        fwrite(&n->line_end, 4, 1, f);
        fwrite(&n->cyclomatic_complexity, 4, 1, f);
        fwrite(&n->fan_in, 4, 1, f);
        fwrite(&n->fan_out, 4, 1, f);
        uint8_t flags = (n->is_thread_entry ? 1 : 0)
                      | (n->is_external ? 2 : 0)
                      | (n->is_shared ? 4 : 0);
        fwrite(&flags, 1, 1, f);
    }

    // Edges
    for (uint32_t i = 0; i < g->edge_count; i++) {
        Edge *e = &g->edges[i];
        fwrite(&e->from, 4, 1, f);
        fwrite(&e->to, 4, 1, f);
        uint32_t type = e->type;
        fwrite(&type, 4, 1, f);
        fwrite(&e->attrs, 1, 1, f);
    }

    // String table
    fwrite(st.data, 1, st.size, f);

    fclose(f);
    free(name_offsets);
    free(file_offsets);
    strtab_destroy(&st);
    return true;
}

bool graph_deserialize(Graph *g, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    uint32_t magic, version, node_count, edge_count, strtab_size;
    fread(&magic, 4, 1, f);
    fread(&version, 4, 1, f);
    if (magic != MAGIC || version != VERSION) { fclose(f); return false; }

    fread(&node_count, 4, 1, f);
    fread(&edge_count, 4, 1, f);
    fread(&strtab_size, 4, 1, f);

    graph_init(g);

    // Read nodes (raw)
    typedef struct { uint32_t id, type, name_off, file_off, ls, le, cc, fi, fo; uint8_t flags; } RawNode;
    RawNode *raw_nodes = malloc(node_count * sizeof(RawNode));
    for (uint32_t i = 0; i < node_count; i++) {
        fread(&raw_nodes[i].id, 4, 1, f);
        fread(&raw_nodes[i].type, 4, 1, f);
        fread(&raw_nodes[i].name_off, 4, 1, f);
        fread(&raw_nodes[i].file_off, 4, 1, f);
        fread(&raw_nodes[i].ls, 4, 1, f);
        fread(&raw_nodes[i].le, 4, 1, f);
        fread(&raw_nodes[i].cc, 4, 1, f);
        fread(&raw_nodes[i].fi, 4, 1, f);
        fread(&raw_nodes[i].fo, 4, 1, f);
        fread(&raw_nodes[i].flags, 1, 1, f);
    }

    // Read edges
    typedef struct { uint32_t from, to, type; uint8_t attrs; } RawEdge;
    RawEdge *raw_edges = malloc(edge_count * sizeof(RawEdge));
    for (uint32_t i = 0; i < edge_count; i++) {
        fread(&raw_edges[i].from, 4, 1, f);
        fread(&raw_edges[i].to, 4, 1, f);
        fread(&raw_edges[i].type, 4, 1, f);
        fread(&raw_edges[i].attrs, 1, 1, f);
    }

    // Read string table
    char *strtab = malloc(strtab_size);
    fread(strtab, 1, strtab_size, f);
    fclose(f);

    // Reconstruct nodes
    for (uint32_t i = 0; i < node_count; i++) {
        const char *name = raw_nodes[i].name_off < strtab_size ? strtab + raw_nodes[i].name_off : "";
        const char *file = raw_nodes[i].file_off < strtab_size ? strtab + raw_nodes[i].file_off : NULL;
        if (raw_nodes[i].file_off == UINT32_MAX) file = NULL;
        uint32_t id = graph_add_node(g, raw_nodes[i].type, name, file, raw_nodes[i].ls, raw_nodes[i].le);
        g->nodes[id].cyclomatic_complexity = raw_nodes[i].cc;
        g->nodes[id].fan_in = raw_nodes[i].fi;
        g->nodes[id].fan_out = raw_nodes[i].fo;
        g->nodes[id].is_thread_entry = raw_nodes[i].flags & 1;
        g->nodes[id].is_external = raw_nodes[i].flags & 2;
        g->nodes[id].is_shared = raw_nodes[i].flags & 4;
    }

    // Reconstruct edges
    for (uint32_t i = 0; i < edge_count; i++) {
        graph_add_edge(g, raw_edges[i].from, raw_edges[i].to, raw_edges[i].type, raw_edges[i].attrs);
    }

    free(raw_nodes);
    free(raw_edges);
    free(strtab);
    return true;
}
```

- [ ] **Step 3: Build and run tests**

```bash
cmake --build build && cd build && ctest --output-on-failure
```

Expected: All serialize tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/graph/serialize.h src/graph/serialize.c tests/test_serialize.c src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: binary serialization/deserialization for graph"
```

---

## Phase 2: libclang Parser

### Task 5: compile_commands.json parser

**Files:**
- Create: `src/parse/compdb.h`
- Create: `src/parse/compdb.c`
- Create: `tests/test_compdb.c`
- Create: `tests/fixtures/compile_commands.json`

- [ ] **Step 1: Create test fixture**

```json
// tests/fixtures/compile_commands.json
[
  {
    "directory": "/project",
    "command": "cc -I/project/include -DFOO=1 -c src/main.c -o build/main.o",
    "file": "src/main.c"
  },
  {
    "directory": "/project",
    "command": "cc -I/project/include -c src/util.c -o build/util.o",
    "file": "src/util.c"
  }
]
```

- [ ] **Step 2: Write tests**

```c
// tests/test_compdb.c
#include "unity.h"
#include "parse/compdb.h"

void setUp(void) {}
void tearDown(void) {}

void test_compdb_parse(void) {
    CompDB db;
    // Path relative to build dir - adjust in CMake
    TEST_ASSERT_TRUE(compdb_parse(&db, FIXTURE_PATH "/compile_commands.json"));
    TEST_ASSERT_EQUAL_UINT32(2, db.count);
    TEST_ASSERT_EQUAL_STRING("src/main.c", db.entries[0].file);
    TEST_ASSERT_EQUAL_STRING("/project", db.entries[0].directory);
    compdb_destroy(&db);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_compdb_parse);
    return UNITY_END();
}
```

- [ ] **Step 3: Implement compdb parser**

Uses a minimal JSON parser (just enough for compile_commands.json array-of-objects format).

```c
// src/parse/compdb.h
#ifndef CGRAPH_COMPDB_H
#define CGRAPH_COMPDB_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char *directory;
    char *file;
    char *command;    // full command string
    char **args;     // parsed args (NULL-terminated)
    uint32_t argc;
} CompDBEntry;

typedef struct {
    CompDBEntry *entries;
    uint32_t count;
    uint32_t capacity;
} CompDB;

bool compdb_parse(CompDB *db, const char *path);
void compdb_destroy(CompDB *db);

#endif
```

```c
// src/parse/compdb.c
#include "parse/compdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Minimal JSON parsing for compile_commands.json format
// Only handles the specific structure: [ { "directory": "...", "file": "...", "command": "..." }, ... ]

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    if (out_len) *out_len = len;
    return buf;
}

static const char *skip_ws(const char *p) {
    while (*p && isspace(*p)) p++;
    return p;
}

static const char *parse_string(const char *p, char **out) {
    if (*p != '"') return NULL;
    p++;
    const char *start = p;
    while (*p && *p != '"') {
        if (*p == '\\') p++;
        p++;
    }
    size_t len = p - start;
    *out = malloc(len + 1);
    memcpy(*out, start, len);
    (*out)[len] = '\0';
    if (*p == '"') p++;
    return p;
}

static void parse_command_to_args(CompDBEntry *entry) {
    // Split command by spaces (simple, doesn't handle quoted args)
    char *cmd = strdup(entry->command);
    uint32_t cap = 16;
    entry->args = malloc(cap * sizeof(char *));
    entry->argc = 0;
    char *tok = strtok(cmd, " ");
    while (tok) {
        if (entry->argc >= cap) {
            cap *= 2;
            entry->args = realloc(entry->args, cap * sizeof(char *));
        }
        entry->args[entry->argc++] = strdup(tok);
        tok = strtok(NULL, " ");
    }
    entry->args = realloc(entry->args, (entry->argc + 1) * sizeof(char *));
    entry->args[entry->argc] = NULL;
    free(cmd);
}

bool compdb_parse(CompDB *db, const char *path) {
    size_t len;
    char *json = read_file(path, &len);
    if (!json) return false;

    db->count = 0;
    db->capacity = 32;
    db->entries = calloc(db->capacity, sizeof(CompDBEntry));

    const char *p = skip_ws(json);
    if (*p != '[') { free(json); return false; }
    p = skip_ws(p + 1);

    while (*p && *p != ']') {
        if (*p == ',') { p = skip_ws(p + 1); continue; }
        if (*p != '{') break;
        p = skip_ws(p + 1);

        if (db->count >= db->capacity) {
            db->capacity *= 2;
            db->entries = realloc(db->entries, db->capacity * sizeof(CompDBEntry));
        }
        CompDBEntry *e = &db->entries[db->count];
        memset(e, 0, sizeof(CompDBEntry));

        while (*p && *p != '}') {
            if (*p == ',') { p = skip_ws(p + 1); continue; }
            char *key = NULL;
            p = parse_string(p, &key);
            if (!p) break;
            p = skip_ws(p);
            if (*p == ':') p = skip_ws(p + 1);
            char *val = NULL;
            p = parse_string(p, &val);
            if (!p) { free(key); break; }
            p = skip_ws(p);

            if (strcmp(key, "directory") == 0) e->directory = val;
            else if (strcmp(key, "file") == 0) e->file = val;
            else if (strcmp(key, "command") == 0) e->command = val;
            else free(val);
            free(key);
        }
        if (*p == '}') p = skip_ws(p + 1);

        if (e->command) parse_command_to_args(e);
        db->count++;
    }

    free(json);
    return true;
}

void compdb_destroy(CompDB *db) {
    for (uint32_t i = 0; i < db->count; i++) {
        free(db->entries[i].directory);
        free(db->entries[i].file);
        free(db->entries[i].command);
        if (db->entries[i].args) {
            for (uint32_t j = 0; j < db->entries[i].argc; j++)
                free(db->entries[i].args[j]);
            free(db->entries[i].args);
        }
    }
    free(db->entries);
}
```

- [ ] **Step 4: Build and test**

In `tests/CMakeLists.txt`, add:
```cmake
add_executable(test_compdb test_compdb.c)
target_link_libraries(test_compdb PRIVATE cgraph_lib unity)
target_include_directories(test_compdb PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_compdb PRIVATE FIXTURE_PATH="${CMAKE_SOURCE_DIR}/tests/fixtures")
add_test(NAME test_compdb COMMAND test_compdb)
```

```bash
cmake --build build && cd build && ctest --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/parse/compdb.h src/parse/compdb.c tests/test_compdb.c tests/fixtures/ src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: compile_commands.json parser"
```

---

### Task 6: libclang AST parser - function and call extraction

**Files:**
- Create: `src/parse/parser.h`
- Create: `src/parse/parser.c`
- Create: `tests/test_parser.c`
- Create: `tests/fixtures/sample_project/main.c`
- Create: `tests/fixtures/sample_project/util.c`
- Create: `tests/fixtures/sample_project/util.h`
- Create: `tests/fixtures/sample_project/compile_commands.json`

- [ ] **Step 1: Create test fixture C project**

```c
// tests/fixtures/sample_project/util.h
#ifndef UTIL_H
#define UTIL_H
int add(int a, int b);
void helper(void);
#endif
```

```c
// tests/fixtures/sample_project/util.c
#include "util.h"
int add(int a, int b) { return a + b; }
void helper(void) {}
```

```c
// tests/fixtures/sample_project/main.c
#include "util.h"
#include <stdio.h>

static void local_func(void) {
    helper();
}

int main(int argc, char **argv) {
    int x = add(1, 2);
    local_func();
    printf("%d\n", x);
    return 0;
}
```

```json
// tests/fixtures/sample_project/compile_commands.json
[
  {"directory": "FIXTURE_DIR", "file": "main.c", "command": "cc -I. -c main.c"},
  {"directory": "FIXTURE_DIR", "file": "util.c", "command": "cc -I. -c util.c"}
]
```

- [ ] **Step 2: Write parser tests**

```c
// tests/test_parser.c
#include "unity.h"
#include "graph/graph.h"
#include "parse/parser.h"

void setUp(void) {}
void tearDown(void) {}

void test_parser_extracts_functions(void) {
    Graph g;
    graph_init(&g);
    ParseResult r = parser_parse_file(&g, FIXTURE_PATH "/sample_project/main.c",
                                       (const char*[]){"-I" FIXTURE_PATH "/sample_project", NULL});
    TEST_ASSERT_TRUE(r.success);
    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "main", &id));
    TEST_ASSERT_TRUE(graph_find_node(&g, "local_func", &id));
    graph_destroy(&g);
}

void test_parser_extracts_calls(void) {
    Graph g;
    graph_init(&g);
    parser_parse_file(&g, FIXTURE_PATH "/sample_project/main.c",
                      (const char*[]){"-I" FIXTURE_PATH "/sample_project", NULL});
    parser_parse_file(&g, FIXTURE_PATH "/sample_project/util.c",
                      (const char*[]){"-I" FIXTURE_PATH "/sample_project", NULL});

    uint32_t main_id, add_id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "main", &main_id));
    TEST_ASSERT_TRUE(graph_find_node(&g, "add", &add_id));

    EdgeList edges = graph_edges_from(&g, main_id);
    bool found_call_to_add = false;
    for (uint32_t i = 0; i < edges.count; i++) {
        if (edges.edges[i].to == add_id && edges.edges[i].type == EDGE_CALLS)
            found_call_to_add = true;
    }
    TEST_ASSERT_TRUE(found_call_to_add);
    free(edges.edges);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_extracts_functions);
    RUN_TEST(test_parser_extracts_calls);
    return UNITY_END();
}
```

- [ ] **Step 3: Implement parser**

```c
// src/parse/parser.h
#ifndef CGRAPH_PARSER_H
#define CGRAPH_PARSER_H

#include "graph/graph.h"
#include "parse/compdb.h"
#include <stdbool.h>

typedef struct {
    bool success;
    uint32_t functions_found;
    uint32_t calls_found;
    char error[256];
} ParseResult;

// Parse a single translation unit
ParseResult parser_parse_file(Graph *g, const char *filename, const char **args);

// Parse entire project from compile_commands.json
ParseResult parser_parse_project(Graph *g, const CompDB *compdb);

#endif
```

```c
// src/parse/parser.c
#include "parse/parser.h"
#include <clang-c/Index.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    Graph *graph;
    uint32_t current_func_id;
    bool in_function;
    uint32_t funcs_found;
    uint32_t calls_found;
    const char *base_dir;
} VisitorCtx;

static const char *get_relative_path(const char *full_path, const char *base_dir) {
    if (!base_dir || !full_path) return full_path;
    size_t base_len = strlen(base_dir);
    if (strncmp(full_path, base_dir, base_len) == 0) {
        const char *rel = full_path + base_len;
        if (*rel == '/') rel++;
        return rel;
    }
    return full_path;
}

static uint32_t ensure_function_node(Graph *g, CXCursor cursor, const char *base_dir) {
    CXString name_cx = clang_getCursorSpelling(cursor);
    const char *name = clang_getCString(name_cx);

    uint32_t existing;
    if (graph_find_node(g, name, &existing)) {
        clang_disposeString(name_cx);
        return existing;
    }

    CXSourceLocation loc = clang_getCursorLocation(cursor);
    CXFile file;
    unsigned line_start;
    clang_getFileLocation(loc, &file, &line_start, NULL, NULL);

    CXString file_cx = clang_getFileName(file);
    const char *file_path = get_relative_path(clang_getCString(file_cx), base_dir);

    CXSourceRange range = clang_getCursorExtent(cursor);
    CXSourceLocation end = clang_getRangeEnd(range);
    unsigned line_end;
    clang_getFileLocation(end, NULL, &line_end, NULL, NULL);

    uint32_t id = graph_add_node(g, NODE_FUNCTION, name, file_path, line_start, line_end);

    // Check if external (no body)
    if (clang_Cursor_isNull(clang_getCursorDefinition(cursor)) ||
        !clang_equalCursors(cursor, clang_getCursorDefinition(cursor))) {
        g->nodes[id].is_external = true;
    }

    clang_disposeString(name_cx);
    clang_disposeString(file_cx);
    return id;
}

static enum CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData data) {
    VisitorCtx *ctx = (VisitorCtx *)data;
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_FunctionDecl) {
        // Only process definitions
        if (clang_isCursorDefinition(cursor)) {
            ctx->current_func_id = ensure_function_node(ctx->graph, cursor, ctx->base_dir);
            ctx->in_function = true;
            ctx->funcs_found++;
            // Visit children for call expressions
            clang_visitChildren(cursor, visitor, data);
            ctx->in_function = false;
            return CXChildVisit_Continue;
        }
    }

    if (kind == CXCursor_CallExpr && ctx->in_function) {
        CXCursor callee = clang_getCursorReferenced(cursor);
        if (!clang_Cursor_isNull(callee) && clang_getCursorKind(callee) == CXCursor_FunctionDecl) {
            uint32_t callee_id = ensure_function_node(ctx->graph, callee, ctx->base_dir);
            graph_add_edge(ctx->graph, ctx->current_func_id, callee_id, EDGE_CALLS, 0);
            ctx->calls_found++;
        }
    }

    return CXChildVisit_Recurse;
}

ParseResult parser_parse_file(Graph *g, const char *filename, const char **args) {
    ParseResult result = {0};
    CXIndex index = clang_createIndex(0, 0);

    // Count args
    int argc = 0;
    if (args) { while (args[argc]) argc++; }

    CXTranslationUnit tu = clang_parseTranslationUnit(
        index, filename, args, argc, NULL, 0, CXTranslationUnit_None);

    if (!tu) {
        snprintf(result.error, sizeof(result.error), "Failed to parse %s", filename);
        clang_disposeIndex(index);
        return result;
    }

    // Get base dir from filename
    char base_dir[1024] = {0};
    const char *last_slash = strrchr(filename, '/');
    if (last_slash) {
        size_t len = last_slash - filename;
        memcpy(base_dir, filename, len);
    }

    VisitorCtx ctx = {
        .graph = g,
        .in_function = false,
        .funcs_found = 0,
        .calls_found = 0,
        .base_dir = base_dir[0] ? base_dir : NULL,
    };

    CXCursor root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root, visitor, &ctx);

    result.success = true;
    result.functions_found = ctx.funcs_found;
    result.calls_found = ctx.calls_found;

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return result;
}

ParseResult parser_parse_project(Graph *g, const CompDB *compdb) {
    ParseResult total = {.success = true};
    for (uint32_t i = 0; i < compdb->count; i++) {
        CompDBEntry *e = &compdb->entries[i];
        // Build full path
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", e->directory, e->file);
        // Skip compiler name (args[0]), skip -c and -o pairs
        ParseResult r = parser_parse_file(g, path, (const char **)e->args + 1);
        if (!r.success) {
            total.success = false;
            memcpy(total.error, r.error, sizeof(total.error));
        }
        total.functions_found += r.functions_found;
        total.calls_found += r.calls_found;
    }
    return total;
}
```

- [ ] **Step 4: Link libclang, build, run tests**

Update `src/CMakeLists.txt`:
```cmake
add_library(cgraph_lib STATIC
    util/arena.c
    util/hash.c
    graph/graph.c
    graph/serialize.c
    parse/compdb.c
    parse/parser.c
)
target_include_directories(cgraph_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cgraph_lib PUBLIC clang)
```

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_parser
```

Expected: Parser tests pass (requires libclang installed).

- [ ] **Step 5: Commit**

```bash
git add src/parse/parser.h src/parse/parser.c tests/test_parser.c tests/fixtures/sample_project/
git commit -m "feat: libclang parser extracts functions and call edges"
```

---

### Task 7: Extended AST extraction (structs, globals, includes)

**Files:**
- Create: `src/parse/extract.h`
- Create: `src/parse/extract.c`
- Modify: `src/parse/parser.c` (integrate extract callbacks)
- Create: `tests/test_extract.c`
- Modify: `tests/fixtures/sample_project/` (add struct/global examples)

- [ ] **Step 1: Extend fixture**

```c
// tests/fixtures/sample_project/types.h
#ifndef TYPES_H
#define TYPES_H

typedef struct {
    int x;
    int y;
} Point;

extern int g_counter;

#endif
```

```c
// tests/fixtures/sample_project/types.c
#include "types.h"

int g_counter = 0;

void use_point(Point *p) {
    p->x = g_counter++;
}
```

- [ ] **Step 2: Write extraction tests**

```c
// tests/test_extract.c
#include "unity.h"
#include "graph/graph.h"
#include "parse/parser.h"

void setUp(void) {}
void tearDown(void) {}

void test_extracts_struct(void) {
    Graph g;
    graph_init(&g);
    parser_parse_file(&g, FIXTURE_PATH "/sample_project/types.c",
                      (const char*[]){"-I" FIXTURE_PATH "/sample_project", NULL});
    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "Point", &id));
    TEST_ASSERT_EQUAL(NODE_STRUCT, g.nodes[id].type);
    graph_destroy(&g);
}

void test_extracts_global(void) {
    Graph g;
    graph_init(&g);
    parser_parse_file(&g, FIXTURE_PATH "/sample_project/types.c",
                      (const char*[]){"-I" FIXTURE_PATH "/sample_project", NULL});
    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "g_counter", &id));
    TEST_ASSERT_EQUAL(NODE_GLOBAL_VAR, g.nodes[id].type);
    graph_destroy(&g);
}

void test_extracts_field_access(void) {
    Graph g;
    graph_init(&g);
    parser_parse_file(&g, FIXTURE_PATH "/sample_project/types.c",
                      (const char*[]){"-I" FIXTURE_PATH "/sample_project", NULL});
    uint32_t func_id, global_id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "use_point", &func_id));
    TEST_ASSERT_TRUE(graph_find_node(&g, "g_counter", &global_id));
    // Check writes_global edge exists
    EdgeList edges = graph_edges_from(&g, func_id);
    bool found = false;
    for (uint32_t i = 0; i < edges.count; i++) {
        if (edges.edges[i].to == global_id &&
            (edges.edges[i].type == EDGE_WRITES_GLOBAL || edges.edges[i].type == EDGE_READS_GLOBAL))
            found = true;
    }
    TEST_ASSERT_TRUE(found);
    free(edges.edges);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_extracts_struct);
    RUN_TEST(test_extracts_global);
    RUN_TEST(test_extracts_field_access);
    return UNITY_END();
}
```

- [ ] **Step 3: Implement extract.c**

Extend the visitor in parser.c to handle `CXCursor_StructDecl`, `CXCursor_VarDecl` (file-scope), `CXCursor_MemberRefExpr`, `CXCursor_DeclRefExpr` (for globals). This is an extension of the visitor pattern already in parser.c — add cases to the switch.

```c
// src/parse/extract.h
#ifndef CGRAPH_EXTRACT_H
#define CGRAPH_EXTRACT_H

#include "graph/graph.h"
#include <clang-c/Index.h>

// Extract struct/union/enum declaration
void extract_type_decl(Graph *g, CXCursor cursor, const char *base_dir);

// Extract global variable
void extract_global_var(Graph *g, CXCursor cursor, const char *base_dir);

// Extract global variable access within a function
void extract_global_access(Graph *g, CXCursor cursor, uint32_t func_id);

// Extract include relationship
void extract_include(Graph *g, CXCursor cursor, const char *base_dir);

#endif
```

Implementation adds handlers for each cursor kind in the visitor loop. Key additions to `parser.c` visitor:

```c
// In visitor function, add these cases:
if (kind == CXCursor_StructDecl && clang_isCursorDefinition(cursor)) {
    extract_type_decl(ctx->graph, cursor, ctx->base_dir);
}
if (kind == CXCursor_VarDecl && !ctx->in_function) {
    // File-scope variable
    if (clang_getCursorLinkage(cursor) == CXLinkage_External) {
        extract_global_var(ctx->graph, cursor, ctx->base_dir);
    }
}
if (kind == CXCursor_DeclRefExpr && ctx->in_function) {
    CXCursor ref = clang_getCursorReferenced(cursor);
    if (clang_getCursorKind(ref) == CXCursor_VarDecl &&
        clang_getCursorLinkage(ref) == CXLinkage_External) {
        extract_global_access(ctx->graph, cursor, ctx->current_func_id);
    }
}
```

- [ ] **Step 4: Build and test**

```bash
cmake --build build && cd build && ctest --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/parse/extract.h src/parse/extract.c tests/test_extract.c tests/fixtures/sample_project/types.h tests/fixtures/sample_project/types.c
git commit -m "feat: extract structs, globals, and access edges from AST"
```

---

## Phase 3: Query Layer

### Task 8: Graph traversal (BFS/DFS, callers, callees, path)

**Files:**
- Create: `src/query/traverse.h`
- Create: `src/query/traverse.c`
- Create: `tests/test_traverse.c`

- [ ] **Step 1: Write traversal tests**

```c
// tests/test_traverse.c
#include "unity.h"
#include "graph/graph.h"
#include "query/traverse.h"

void setUp(void) {}
void tearDown(void) {}

static Graph make_chain(void) {
    // a -> b -> c -> d
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "d", "d.c", 1, 5);
    graph_add_edge(&g, 0, 1, EDGE_CALLS, 0);
    graph_add_edge(&g, 1, 2, EDGE_CALLS, 0);
    graph_add_edge(&g, 2, 3, EDGE_CALLS, 0);
    return g;
}

void test_callees_depth1(void) {
    Graph g = make_chain();
    QueryResult r = query_callees(&g, 0, 1);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(1, r.node_ids[0]);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_callees_depth3(void) {
    Graph g = make_chain();
    QueryResult r = query_callees(&g, 0, 3);
    TEST_ASSERT_EQUAL_UINT32(3, r.count);  // b, c, d
    query_result_free(&r);
    graph_destroy(&g);
}

void test_callers_depth2(void) {
    Graph g = make_chain();
    QueryResult r = query_callers(&g, 3, 2);
    TEST_ASSERT_EQUAL_UINT32(2, r.count);  // c, b
    query_result_free(&r);
    graph_destroy(&g);
}

void test_path(void) {
    Graph g = make_chain();
    QueryResult r = query_path(&g, 0, 3);
    TEST_ASSERT_TRUE(r.count > 0);
    TEST_ASSERT_EQUAL_UINT32(0, r.node_ids[0]);
    TEST_ASSERT_EQUAL_UINT32(3, r.node_ids[r.count - 1]);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_path_not_found(void) {
    Graph g = make_chain();
    QueryResult r = query_path(&g, 3, 0);  // no reverse path
    TEST_ASSERT_EQUAL_UINT32(0, r.count);
    query_result_free(&r);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_callees_depth1);
    RUN_TEST(test_callees_depth3);
    RUN_TEST(test_callers_depth2);
    RUN_TEST(test_path);
    RUN_TEST(test_path_not_found);
    return UNITY_END();
}
```

- [ ] **Step 2: Implement traversal**

```c
// src/query/traverse.h
#ifndef CGRAPH_TRAVERSE_H
#define CGRAPH_TRAVERSE_H

#include "graph/graph.h"

typedef struct {
    uint32_t *node_ids;
    uint32_t count;
    uint32_t *depths;  // depth of each result node
} QueryResult;

void query_result_free(QueryResult *r);

// Forward traversal (callees)
QueryResult query_callees(const Graph *g, uint32_t node_id, uint32_t max_depth);

// Reverse traversal (callers)
QueryResult query_callers(const Graph *g, uint32_t node_id, uint32_t max_depth);

// Shortest path (BFS)
QueryResult query_path(const Graph *g, uint32_t from_id, uint32_t to_id);

#endif
```

```c
// src/query/traverse.c
#include "query/traverse.h"
#include <stdlib.h>
#include <string.h>

void query_result_free(QueryResult *r) {
    free(r->node_ids);
    free(r->depths);
    r->node_ids = NULL;
    r->depths = NULL;
    r->count = 0;
}

// BFS with depth limit
static QueryResult bfs(const Graph *g, uint32_t start, uint32_t max_depth, bool reverse) {
    QueryResult result = {0};
    uint32_t cap = 64;
    result.node_ids = malloc(cap * sizeof(uint32_t));
    result.depths = malloc(cap * sizeof(uint32_t));

    // Visited set (bitmap)
    uint32_t bitmap_size = (g->node_count + 31) / 32;
    uint32_t *visited = calloc(bitmap_size, sizeof(uint32_t));
    visited[start / 32] |= (1u << (start % 32));

    // Queue: node_id + depth pairs
    typedef struct { uint32_t id; uint32_t depth; } QItem;
    QItem *queue = malloc(g->node_count * sizeof(QItem));
    uint32_t q_head = 0, q_tail = 0;
    queue[q_tail++] = (QItem){start, 0};

    while (q_head < q_tail) {
        QItem item = queue[q_head++];
        if (item.depth >= max_depth) continue;

        // Find edges
        for (uint32_t i = 0; i < g->edge_count; i++) {
            uint32_t neighbor;
            if (!reverse && g->edges[i].from == item.id &&
                (g->edges[i].type == EDGE_CALLS || g->edges[i].type == EDGE_CALLS_FP)) {
                neighbor = g->edges[i].to;
            } else if (reverse && g->edges[i].to == item.id &&
                       (g->edges[i].type == EDGE_CALLS || g->edges[i].type == EDGE_CALLS_FP)) {
                neighbor = g->edges[i].from;
            } else {
                continue;
            }

            if (visited[neighbor / 32] & (1u << (neighbor % 32))) continue;
            visited[neighbor / 32] |= (1u << (neighbor % 32));

            if (result.count >= cap) {
                cap *= 2;
                result.node_ids = realloc(result.node_ids, cap * sizeof(uint32_t));
                result.depths = realloc(result.depths, cap * sizeof(uint32_t));
            }
            result.node_ids[result.count] = neighbor;
            result.depths[result.count] = item.depth + 1;
            result.count++;

            queue[q_tail++] = (QItem){neighbor, item.depth + 1};
        }
    }

    free(visited);
    free(queue);
    return result;
}

QueryResult query_callees(const Graph *g, uint32_t node_id, uint32_t max_depth) {
    return bfs(g, node_id, max_depth, false);
}

QueryResult query_callers(const Graph *g, uint32_t node_id, uint32_t max_depth) {
    return bfs(g, node_id, max_depth, true);
}

QueryResult query_path(const Graph *g, uint32_t from_id, uint32_t to_id) {
    QueryResult result = {0};
    if (from_id == to_id) {
        result.node_ids = malloc(sizeof(uint32_t));
        result.node_ids[0] = from_id;
        result.count = 1;
        return result;
    }

    uint32_t bitmap_size = (g->node_count + 31) / 32;
    uint32_t *visited = calloc(bitmap_size, sizeof(uint32_t));
    uint32_t *parent = malloc(g->node_count * sizeof(uint32_t));
    memset(parent, 0xFF, g->node_count * sizeof(uint32_t));

    visited[from_id / 32] |= (1u << (from_id % 32));

    uint32_t *queue = malloc(g->node_count * sizeof(uint32_t));
    uint32_t q_head = 0, q_tail = 0;
    queue[q_tail++] = from_id;
    bool found = false;

    while (q_head < q_tail && !found) {
        uint32_t cur = queue[q_head++];
        for (uint32_t i = 0; i < g->edge_count; i++) {
            if (g->edges[i].from != cur) continue;
            if (g->edges[i].type != EDGE_CALLS && g->edges[i].type != EDGE_CALLS_FP) continue;
            uint32_t next = g->edges[i].to;
            if (visited[next / 32] & (1u << (next % 32))) continue;
            visited[next / 32] |= (1u << (next % 32));
            parent[next] = cur;
            if (next == to_id) { found = true; break; }
            queue[q_tail++] = next;
        }
    }

    if (found) {
        // Reconstruct path
        uint32_t path[1024];
        uint32_t len = 0;
        uint32_t cur = to_id;
        while (cur != from_id && len < 1024) {
            path[len++] = cur;
            cur = parent[cur];
        }
        path[len++] = from_id;
        // Reverse
        result.count = len;
        result.node_ids = malloc(len * sizeof(uint32_t));
        for (uint32_t i = 0; i < len; i++) {
            result.node_ids[i] = path[len - 1 - i];
        }
    }

    free(visited);
    free(parent);
    free(queue);
    return result;
}
```

- [ ] **Step 3: Build and test**

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_traverse
```

- [ ] **Step 4: Commit**

```bash
git add src/query/traverse.h src/query/traverse.c tests/test_traverse.c src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: BFS traversal for callers, callees, and path queries"
```

---

### Task 9: Impact analysis and metrics

**Files:**
- Create: `src/query/impact.h`
- Create: `src/query/impact.c`
- Create: `src/graph/metrics.h`
- Create: `src/graph/metrics.c`
- Create: `tests/test_impact.c`

- [ ] **Step 1: Write tests**

```c
// tests/test_impact.c
#include "unity.h"
#include "graph/graph.h"
#include "graph/metrics.h"
#include "query/impact.h"

void setUp(void) {}
void tearDown(void) {}

void test_impact_propagates_up(void) {
    // a -> b -> c; d -> b
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "d", "d.c", 1, 5);
    graph_add_edge(&g, 0, 1, EDGE_CALLS, 0);
    graph_add_edge(&g, 1, 2, EDGE_CALLS, 0);
    graph_add_edge(&g, 3, 1, EDGE_CALLS, 0);

    QueryResult r = query_impact(&g, 2);  // who is affected if c changes?
    // b calls c, a and d call b => impact = {b, a, d}
    TEST_ASSERT_EQUAL_UINT32(3, r.count);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_fan_in_out(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "hub", "h.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_edge(&g, 1, 0, EDGE_CALLS, 0);  // a -> hub
    graph_add_edge(&g, 2, 0, EDGE_CALLS, 0);  // b -> hub
    graph_add_edge(&g, 0, 3, EDGE_CALLS, 0);  // hub -> c

    metrics_compute_fan(&g);
    TEST_ASSERT_EQUAL_UINT32(2, g.nodes[0].fan_in);
    TEST_ASSERT_EQUAL_UINT32(1, g.nodes[0].fan_out);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_impact_propagates_up);
    RUN_TEST(test_fan_in_out);
    return UNITY_END();
}
```

- [ ] **Step 2: Implement impact and metrics**

```c
// src/query/impact.h
#ifndef CGRAPH_IMPACT_H
#define CGRAPH_IMPACT_H
#include "graph/graph.h"
#include "query/traverse.h"

// Reverse-propagate: who is affected if this symbol changes?
QueryResult query_impact(const Graph *g, uint32_t node_id);

// Impact of a specific field change
QueryResult query_impact_field(const Graph *g, const char *struct_field);

#endif
```

```c
// src/query/impact.c
#include "query/impact.h"
#include <stdlib.h>

QueryResult query_impact(const Graph *g, uint32_t node_id) {
    // Reverse BFS across all edge types (not just calls)
    QueryResult result = {0};
    uint32_t cap = 64;
    result.node_ids = malloc(cap * sizeof(uint32_t));

    uint32_t bitmap_size = (g->node_count + 31) / 32;
    uint32_t *visited = calloc(bitmap_size, sizeof(uint32_t));
    visited[node_id / 32] |= (1u << (node_id % 32));

    uint32_t *queue = malloc(g->node_count * sizeof(uint32_t));
    uint32_t q_head = 0, q_tail = 0;
    queue[q_tail++] = node_id;

    while (q_head < q_tail) {
        uint32_t cur = queue[q_head++];
        for (uint32_t i = 0; i < g->edge_count; i++) {
            if (g->edges[i].to != cur) continue;
            uint32_t from = g->edges[i].from;
            if (visited[from / 32] & (1u << (from % 32))) continue;
            visited[from / 32] |= (1u << (from % 32));
            if (result.count >= cap) {
                cap *= 2;
                result.node_ids = realloc(result.node_ids, cap * sizeof(uint32_t));
            }
            result.node_ids[result.count++] = from;
            queue[q_tail++] = from;
        }
    }

    free(visited);
    free(queue);
    return result;
}

QueryResult query_impact_field(const Graph *g, const char *struct_field) {
    // Find field node, then reverse-propagate via ACCESSES_FIELD edges
    uint32_t field_id;
    QueryResult empty = {0};
    if (!graph_find_node(g, struct_field, &field_id)) return empty;
    return query_impact(g, field_id);
}
```

```c
// src/graph/metrics.h
#ifndef CGRAPH_METRICS_H
#define CGRAPH_METRICS_H
#include "graph/graph.h"

void metrics_compute_fan(Graph *g);

#endif
```

```c
// src/graph/metrics.c
#include "graph/metrics.h"

void metrics_compute_fan(Graph *g) {
    // Reset
    for (uint32_t i = 0; i < g->node_count; i++) {
        g->nodes[i].fan_in = 0;
        g->nodes[i].fan_out = 0;
    }
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].type == EDGE_CALLS || g->edges[i].type == EDGE_CALLS_FP) {
            g->nodes[g->edges[i].from].fan_out++;
            g->nodes[g->edges[i].to].fan_in++;
        }
    }
}
```

- [ ] **Step 3: Build and test**

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_impact
```

- [ ] **Step 4: Commit**

```bash
git add src/query/impact.h src/query/impact.c src/graph/metrics.h src/graph/metrics.c tests/test_impact.c
git commit -m "feat: impact analysis and fan-in/fan-out metrics"
```

---

### Task 10: JSON output formatter

**Files:**
- Create: `src/export/json.h`
- Create: `src/export/json.c`
- Create: `tests/test_json.c`

- [ ] **Step 1: Write tests**

```c
// tests/test_json.c
#include "unity.h"
#include "graph/graph.h"
#include "export/json.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_json_node(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "main", "main.c", 1, 20);
    g.nodes[0].cyclomatic_complexity = 5;

    char *json = json_format_node(&g, 0, NULL);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"name\": \"main\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"type\": \"function\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"cyclomatic_complexity\": 5"));
    free(json);
    graph_destroy(&g);
}

void test_json_query_result(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    uint32_t ids[] = {0, 1};
    QueryResult qr = {.node_ids = ids, .count = 2};

    char *json = json_format_query_result(&g, &qr, NULL);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"a\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"b\""));
    free(json);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_json_node);
    RUN_TEST(test_json_query_result);
    return UNITY_END();
}
```

- [ ] **Step 2: Implement JSON formatter**

```c
// src/export/json.h
#ifndef CGRAPH_JSON_H
#define CGRAPH_JSON_H

#include "graph/graph.h"
#include "query/traverse.h"

typedef struct {
    const char *project_root;  // if set, prepend to file paths
} JsonOptions;

// Returns heap-allocated JSON string. Caller must free.
char *json_format_node(const Graph *g, uint32_t node_id, const JsonOptions *opts);
char *json_format_query_result(const Graph *g, const QueryResult *qr, const JsonOptions *opts);
char *json_format_info(const Graph *g);
char *json_format_node_list(const Graph *g, NodeType type, const JsonOptions *opts);

#endif
```

Implementation: Simple snprintf-based JSON builder (no dependency). Each function builds a dynamically growing string buffer.

- [ ] **Step 3: Build and test**

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_json
```

- [ ] **Step 4: Commit**

```bash
git add src/export/json.h src/export/json.c tests/test_json.c
git commit -m "feat: JSON output formatter for nodes and query results"
```

---

## Phase 4: CLI Commands

### Task 11: CLI command router + build command

**Files:**
- Modify: `src/main.c` (full rewrite — command dispatch + build subcommand)
- Create: `src/cli.h`

- [ ] **Step 1: Implement CLI router**

```c
// src/cli.h
#ifndef CGRAPH_CLI_H
#define CGRAPH_CLI_H

typedef struct {
    const char *db_path;        // --db, default "cgraph.db"
    const char *project_root;   // --project-root
    const char *module;         // --module
    int depth;                  // --depth, default 3
    int top_n;                  // --top, default 10
    int min_lines;              // --min-lines, default 100
} CliOptions;

int cmd_build(int argc, char **argv, CliOptions *opts);
int cmd_info(int argc, char **argv, CliOptions *opts);
int cmd_list(int argc, char **argv, CliOptions *opts);
int cmd_show(int argc, char **argv, CliOptions *opts);
int cmd_callers(int argc, char **argv, CliOptions *opts);
int cmd_callees(int argc, char **argv, CliOptions *opts);
int cmd_path(int argc, char **argv, CliOptions *opts);
int cmd_impact(int argc, char **argv, CliOptions *opts);
int cmd_complexity(int argc, char **argv, CliOptions *opts);
int cmd_dead_code(int argc, char **argv, CliOptions *opts);

#endif
```

```c
// src/main.c
#include <stdio.h>
#include <string.h>
#include "cli.h"

static void parse_global_opts(int *argc, char **argv, CliOptions *opts) {
    opts->db_path = "cgraph.db";
    opts->project_root = NULL;
    opts->module = NULL;
    opts->depth = 3;
    opts->top_n = 10;
    opts->min_lines = 100;

    for (int i = 1; i < *argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < *argc) {
            opts->db_path = argv[++i];
        } else if (strcmp(argv[i], "--project-root") == 0 && i + 1 < *argc) {
            opts->project_root = argv[++i];
        } else if (strcmp(argv[i], "--module") == 0 && i + 1 < *argc) {
            opts->module = argv[++i];
        } else if (strcmp(argv[i], "--depth") == 0 && i + 1 < *argc) {
            opts->depth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--top") == 0 && i + 1 < *argc) {
            opts->top_n = atoi(argv[++i]);
        }
    }
}

typedef struct { const char *name; int (*fn)(int, char**, CliOptions*); } Command;

static Command commands[] = {
    {"build",       cmd_build},
    {"info",        cmd_info},
    {"list",        cmd_list},
    {"show",        cmd_show},
    {"callers",     cmd_callers},
    {"callees",     cmd_callees},
    {"path",        cmd_path},
    {"impact",      cmd_impact},
    {"complexity",  cmd_complexity},
    {"dead-code",   cmd_dead_code},
    {NULL, NULL}
};

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: cgraph <command> [options]\n");
        fprintf(stderr, "Commands: build, info, list, show, callers, callees, path, impact, complexity, dead-code\n");
        return 1;
    }

    CliOptions opts;
    parse_global_opts(&argc, argv, &opts);

    const char *cmd_name = argv[1];
    for (Command *c = commands; c->name; c++) {
        if (strcmp(cmd_name, c->name) == 0) {
            return c->fn(argc - 2, argv + 2, &opts);
        }
    }

    fprintf(stderr, "Unknown command: %s\n", cmd_name);
    return 1;
}
```

- [ ] **Step 2: Implement cmd_build**

```c
// In a new file src/cmd_build.c or within main.c
int cmd_build(int argc, char **argv, CliOptions *opts) {
    const char *compdb_path = "compile_commands.json";
    const char *output = opts->db_path;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--compile-commands") == 0 && i + 1 < argc)
            compdb_path = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            output = argv[++i];
    }

    CompDB compdb;
    if (!compdb_parse(&compdb, compdb_path)) {
        fprintf(stderr, "Error: cannot parse %s\n", compdb_path);
        return 1;
    }
    fprintf(stderr, "Parsing %u translation units...\n", compdb.count);

    Graph g;
    graph_init(&g);
    ParseResult r = parser_parse_project(&g, &compdb);
    if (!r.success) {
        fprintf(stderr, "Error: %s\n", r.error);
        graph_destroy(&g);
        compdb_destroy(&compdb);
        return 1;
    }

    metrics_compute_fan(&g);

    fprintf(stderr, "Found %u functions, %u call edges\n", r.functions_found, r.calls_found);
    fprintf(stderr, "Graph: %u nodes, %u edges\n", g.node_count, g.edge_count);

    if (!graph_serialize(&g, output)) {
        fprintf(stderr, "Error: cannot write %s\n", output);
        graph_destroy(&g);
        compdb_destroy(&compdb);
        return 1;
    }
    fprintf(stderr, "Written: %s\n", output);

    graph_destroy(&g);
    compdb_destroy(&compdb);
    return 0;
}
```

- [ ] **Step 3: Implement remaining query commands (info, list, show, callers, callees, path, impact, complexity, dead-code)**

Each command follows the same pattern:
1. Load graph from `opts->db_path`
2. Run query
3. Format as JSON via `json_format_*`
4. Print to stdout
5. Destroy graph

- [ ] **Step 4: End-to-end test with fixture project**

```bash
cd tests/fixtures/sample_project
../../../build/cgraph build --compile-commands compile_commands.json --output /tmp/test.db
../../../build/cgraph info --db /tmp/test.db
../../../build/cgraph callers main --db /tmp/test.db
```

Expected: JSON output for each command.

- [ ] **Step 5: Commit**

```bash
git add src/main.c src/cli.h src/cmd_build.c
git commit -m "feat: CLI command router with build and query commands"
```

---

## Phase 5: Concurrency & Quality Queries

### Task 12: Concurrency analysis (threads, shared-resources, data-race-suspects)

**Files:**
- Create: `src/query/concurrency.h`
- Create: `src/query/concurrency.c`
- Create: `tests/test_concurrency.c`

- [ ] **Step 1: Write tests**

```c
// tests/test_concurrency.c
#include "unity.h"
#include "graph/graph.h"
#include "query/concurrency.h"

void setUp(void) {}
void tearDown(void) {}

void test_find_thread_entries(void) {
    Graph g;
    graph_init(&g);
    uint32_t t = graph_add_node(&g, NODE_FUNCTION, "worker", "w.c", 1, 10);
    g.nodes[t].is_thread_entry = true;
    graph_add_node(&g, NODE_FUNCTION, "main", "m.c", 1, 10);

    QueryResult r = query_threads(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(t, r.node_ids[0]);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_data_race_suspect(void) {
    Graph g;
    graph_init(&g);
    uint32_t gvar = graph_add_node(&g, NODE_GLOBAL_VAR, "shared", "g.c", 1, 1);
    g.nodes[gvar].is_shared = true;
    uint32_t func = graph_add_node(&g, NODE_FUNCTION, "writer", "w.c", 1, 5);
    g.nodes[func].is_thread_entry = true;
    graph_add_edge(&g, func, gvar, EDGE_WRITES_GLOBAL, 0);
    // No lock acquired

    QueryResult r = query_data_race_suspects(&g);
    TEST_ASSERT_TRUE(r.count > 0);
    query_result_free(&r);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_find_thread_entries);
    RUN_TEST(test_data_race_suspect);
    return UNITY_END();
}
```

- [ ] **Step 2: Implement**

```c
// src/query/concurrency.h
#ifndef CGRAPH_CONCURRENCY_H
#define CGRAPH_CONCURRENCY_H
#include "graph/graph.h"
#include "query/traverse.h"

QueryResult query_threads(const Graph *g);
QueryResult query_shared_resources(const Graph *g);
QueryResult query_data_race_suspects(const Graph *g);

#endif
```

```c
// src/query/concurrency.c
#include "query/concurrency.h"
#include <stdlib.h>

QueryResult query_threads(const Graph *g) {
    QueryResult r = {0};
    uint32_t cap = 16;
    r.node_ids = malloc(cap * sizeof(uint32_t));
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type == NODE_FUNCTION && g->nodes[i].is_thread_entry) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}

QueryResult query_shared_resources(const Graph *g) {
    QueryResult r = {0};
    uint32_t cap = 16;
    r.node_ids = malloc(cap * sizeof(uint32_t));
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type == NODE_GLOBAL_VAR && g->nodes[i].is_shared) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}

QueryResult query_data_race_suspects(const Graph *g) {
    // Functions that access shared globals without holding a lock
    QueryResult r = {0};
    uint32_t cap = 16;
    r.node_ids = malloc(cap * sizeof(uint32_t));

    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != NODE_FUNCTION) continue;

        bool accesses_shared = false;
        bool holds_lock = false;

        for (uint32_t e = 0; e < g->edge_count; e++) {
            if (g->edges[e].from != i) continue;
            if (g->edges[e].type == EDGE_ACQUIRES_LOCK) holds_lock = true;
            if (g->edges[e].type == EDGE_READS_GLOBAL || g->edges[e].type == EDGE_WRITES_GLOBAL) {
                uint32_t target = g->edges[e].to;
                if (target < g->node_count && g->nodes[target].is_shared)
                    accesses_shared = true;
            }
        }

        if (accesses_shared && !holds_lock) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}
```

- [ ] **Step 3: Build and test**

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_concurrency
```

- [ ] **Step 4: Commit**

```bash
git add src/query/concurrency.h src/query/concurrency.c tests/test_concurrency.c
git commit -m "feat: concurrency analysis queries"
```

---

### Task 13: Quality queries (complexity, coupling, dead-code, large-functions)

**Files:**
- Create: `src/query/quality.h`
- Create: `src/query/quality.c`
- Create: `tests/test_quality.c`

- [ ] **Step 1: Write tests**

```c
// tests/test_quality.c
#include "unity.h"
#include "graph/graph.h"
#include "graph/metrics.h"
#include "query/quality.h"

void setUp(void) {}
void tearDown(void) {}

void test_dead_code(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "used", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "unused", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "main", "m.c", 1, 10);
    graph_add_edge(&g, 2, 0, EDGE_CALLS, 0);  // main -> used
    metrics_compute_fan(&g);

    QueryResult r = query_dead_code(&g);
    // "unused" has fan_in=0 and is not main
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_STRING("unused", g.nodes[r.node_ids[0]].name);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_large_functions(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "small", "a.c", 1, 10);
    graph_add_node(&g, NODE_FUNCTION, "big", "b.c", 1, 200);

    QueryResult r = query_large_functions(&g, 50);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_STRING("big", g.nodes[r.node_ids[0]].name);
    query_result_free(&r);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dead_code);
    RUN_TEST(test_large_functions);
    return UNITY_END();
}
```

- [ ] **Step 2: Implement**

```c
// src/query/quality.h
#ifndef CGRAPH_QUALITY_H
#define CGRAPH_QUALITY_H
#include "graph/graph.h"
#include "query/traverse.h"

QueryResult query_complexity_top(const Graph *g, uint32_t top_n);
QueryResult query_coupling_top(const Graph *g, uint32_t top_n);
QueryResult query_dead_code(const Graph *g);
QueryResult query_large_functions(const Graph *g, uint32_t min_lines);

#endif
```

```c
// src/query/quality.c
#include "query/quality.h"
#include <stdlib.h>
#include <string.h>

static int cmp_complexity_desc(const void *a, const void *b, void *ctx) {
    const Graph *g = ctx;
    uint32_t ia = *(const uint32_t *)a, ib = *(const uint32_t *)b;
    return (int)g->nodes[ib].cyclomatic_complexity - (int)g->nodes[ia].cyclomatic_complexity;
}

QueryResult query_complexity_top(const Graph *g, uint32_t top_n) {
    // Collect all functions, sort by complexity
    uint32_t *ids = malloc(g->node_count * sizeof(uint32_t));
    uint32_t count = 0;
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type == NODE_FUNCTION && !g->nodes[i].is_external)
            ids[count++] = i;
    }
    // Simple insertion sort (fine for target scale)
    for (uint32_t i = 1; i < count; i++) {
        uint32_t key = ids[i];
        int j = i - 1;
        while (j >= 0 && g->nodes[ids[j]].cyclomatic_complexity < g->nodes[key].cyclomatic_complexity) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key;
    }
    uint32_t n = count < top_n ? count : top_n;
    QueryResult r = {.node_ids = ids, .count = n};
    return r;
}

QueryResult query_coupling_top(const Graph *g, uint32_t top_n) {
    uint32_t *ids = malloc(g->node_count * sizeof(uint32_t));
    uint32_t count = 0;
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type == NODE_FUNCTION && !g->nodes[i].is_external)
            ids[count++] = i;
    }
    // Sort by fan_in * fan_out descending
    for (uint32_t i = 1; i < count; i++) {
        uint32_t key = ids[i];
        uint32_t key_coupling = g->nodes[key].fan_in * g->nodes[key].fan_out;
        int j = i - 1;
        while (j >= 0 && g->nodes[ids[j]].fan_in * g->nodes[ids[j]].fan_out < key_coupling) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key;
    }
    uint32_t n = count < top_n ? count : top_n;
    QueryResult r = {.node_ids = ids, .count = n};
    return r;
}

QueryResult query_dead_code(const Graph *g) {
    QueryResult r = {0};
    uint32_t cap = 32;
    r.node_ids = malloc(cap * sizeof(uint32_t));
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != NODE_FUNCTION) continue;
        if (g->nodes[i].is_external) continue;
        if (strcmp(g->nodes[i].name, "main") == 0) continue;
        if (g->nodes[i].fan_in == 0) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}

QueryResult query_large_functions(const Graph *g, uint32_t min_lines) {
    QueryResult r = {0};
    uint32_t cap = 32;
    r.node_ids = malloc(cap * sizeof(uint32_t));
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != NODE_FUNCTION) continue;
        uint32_t lines = g->nodes[i].line_end - g->nodes[i].line_start;
        if (lines >= min_lines) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}
```

- [ ] **Step 3: Build and test**

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_quality
```

- [ ] **Step 4: Commit**

```bash
git add src/query/quality.h src/query/quality.c tests/test_quality.c
git commit -m "feat: code quality queries (complexity, coupling, dead-code)"
```

---

## Phase 6: HTML Export

### Task 14: Static HTML export

**Files:**
- Create: `src/export/html.h`
- Create: `src/export/html.c`
- Create: `src/export/templates/index.html` (template string embedded in C)

- [ ] **Step 1: Implement HTML export**

The HTML export generates:
- `index.html` — single page app with embedded D3.js
- `data.json` — full graph data for client-side rendering

```c
// src/export/html.h
#ifndef CGRAPH_HTML_H
#define CGRAPH_HTML_H
#include "graph/graph.h"
#include <stdbool.h>

bool html_export(const Graph *g, const char *output_dir);

#endif
```

The implementation writes `data.json` (reuses json.c functions) and a self-contained `index.html` with inline D3.js, search, and force-directed graph visualization. The HTML/JS/CSS templates are stored as C string literals.

- [ ] **Step 2: Test by building fixture and exporting**

```bash
cd tests/fixtures/sample_project
../../../build/cgraph build --compile-commands compile_commands.json --output /tmp/test.db
../../../build/cgraph export-html --db /tmp/test.db --output /tmp/cgraph_html/
ls /tmp/cgraph_html/
```

Expected: `index.html` and `data.json` exist, `index.html` opens in browser.

- [ ] **Step 3: Commit**

```bash
git add src/export/html.h src/export/html.c
git commit -m "feat: static HTML export with D3.js visualization"
```

---

## Phase 7: Integration & Polish

### Task 15: End-to-end integration test

**Files:**
- Create: `tests/test_e2e.sh`

- [ ] **Step 1: Write integration test script**

```bash
#!/bin/bash
set -e
CGRAPH=./build/cgraph
FIXTURE=./tests/fixtures/sample_project

echo "=== Build ==="
$CGRAPH build --compile-commands $FIXTURE/compile_commands.json --output /tmp/e2e.db

echo "=== Info ==="
$CGRAPH info --db /tmp/e2e.db | python3 -m json.tool

echo "=== List functions ==="
$CGRAPH list functions --db /tmp/e2e.db | python3 -m json.tool

echo "=== Show main ==="
$CGRAPH show main --db /tmp/e2e.db | python3 -m json.tool

echo "=== Callers of add ==="
$CGRAPH callers add --db /tmp/e2e.db | python3 -m json.tool

echo "=== Callees of main ==="
$CGRAPH callees main --db /tmp/e2e.db | python3 -m json.tool

echo "=== Path main -> helper ==="
$CGRAPH path main helper --db /tmp/e2e.db | python3 -m json.tool

echo "=== Dead code ==="
$CGRAPH dead-code --db /tmp/e2e.db | python3 -m json.tool

echo "=== ALL PASSED ==="
```

- [ ] **Step 2: Run and fix any issues**

```bash
chmod +x tests/test_e2e.sh && ./tests/test_e2e.sh
```

- [ ] **Step 3: Commit**

```bash
git add tests/test_e2e.sh
git commit -m "test: end-to-end integration test script"
```

---

### Task 16: Conditional compilation analysis (ifdef)

**Files:**
- Create: `src/parse/ifdef.h`
- Create: `src/parse/ifdef.c`
- Create: `src/query/platform.h`
- Create: `src/query/platform.c`
- Create: `tests/test_platform.c`

- [ ] **Step 1: Implement ifdef tracking**

Uses libclang's `clang_getSkippedRanges` or preprocessor callbacks to identify `#ifdef` blocks and associate contained symbols with the guard macro via `EDGE_IFDEF_DEPENDS`.

- [ ] **Step 2: Implement platform queries**

```c
// src/query/platform.h
#ifndef CGRAPH_PLATFORM_H
#define CGRAPH_PLATFORM_H
#include "graph/graph.h"
#include "query/traverse.h"

QueryResult query_platform_deps(const Graph *g, const char *ifdef_macro);
QueryResult query_syscalls(const Graph *g);

#endif
```

- [ ] **Step 3: Test and commit**

```bash
cmake --build build && cd build && ctest --output-on-failure -R test_platform
git add src/parse/ifdef.h src/parse/ifdef.c src/query/platform.h src/query/platform.c tests/test_platform.c
git commit -m "feat: ifdef tracking and platform dependency queries"
```

---

## Summary

| Phase | Tasks | Delivers |
|-------|-------|----------|
| 1. Foundation | 1-4 | Arena, HashMap, Graph core, Serialization |
| 2. Parser | 5-7 | compile_commands parser, libclang AST extraction |
| 3. Query | 8-10 | Traversal, Impact, JSON output |
| 4. CLI | 11 | Command router, all subcommands |
| 5. Analysis | 12-13 | Concurrency + quality queries |
| 6. Export | 14 | Static HTML with D3.js |
| 7. Polish | 15-16 | E2E tests, ifdef/platform analysis |

**Total: 16 tasks, ~40 commits, incrementally testable at each phase.**
