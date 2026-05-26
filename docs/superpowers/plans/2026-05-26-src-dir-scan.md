# --src-dir Scan Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow `cgraph build --src-dir <path>` to build a graph by recursively scanning a directory for `.c`/`.h` files, without requiring a `compile_commands.json`.

**Architecture:** New `scan.c`/`scan.h` module in `src/parse/` that recursively walks a directory, collects `.c` files and auto-discovers `-I` paths from directories containing `.h` files. `cmd_build` gains a branch: if `--src-dir` is provided, use scanner instead of compdb.

**Tech Stack:** C17, POSIX `dirent.h`/`stat`, existing parser infrastructure, Unity test framework.

---

### Task 1: Create scan module header and implementation

**Files:**
- Create: `src/parse/scan.h`
- Create: `src/parse/scan.c`

- [ ] **Step 1: Write the failing test**

Create `tests/test_scan.c`:

```c
#include "unity.h"
#include "parse/scan.h"

void setUp(void) {}
void tearDown(void) {}

void test_scan_finds_c_files(void) {
    ScanResult sr;
    TEST_ASSERT_TRUE(scan_directory(&sr, FIXTURE_PATH "/sample_project"));
    TEST_ASSERT_TRUE(sr.file_count >= 2); /* main.c, util.c at minimum */
    /* Verify all entries end in .c */
    for (uint32_t i = 0; i < sr.file_count; i++) {
        const char *dot = strrchr(sr.files[i], '.');
        TEST_ASSERT_NOT_NULL(dot);
        TEST_ASSERT_EQUAL_STRING(".c", dot);
    }
    scan_result_destroy(&sr);
}

void test_scan_finds_include_dirs(void) {
    ScanResult sr;
    TEST_ASSERT_TRUE(scan_directory(&sr, FIXTURE_PATH "/sample_project"));
    /* Should have at least the root dir as include path */
    TEST_ASSERT_TRUE(sr.include_count >= 1);
    scan_result_destroy(&sr);
}

void test_scan_invalid_dir(void) {
    ScanResult sr;
    TEST_ASSERT_FALSE(scan_directory(&sr, "/nonexistent/path"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scan_finds_c_files);
    RUN_TEST(test_scan_finds_include_dirs);
    RUN_TEST(test_scan_invalid_dir);
    return UNITY_END();
}
```

- [ ] **Step 2: Write scan.h**

```c
#ifndef CGRAPH_SCAN_H
#define CGRAPH_SCAN_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char **files;         /* absolute paths to .c files */
    uint32_t file_count;
    uint32_t file_capacity;
    char **includes;      /* -I directories (dirs containing .h files) */
    uint32_t include_count;
    uint32_t include_capacity;
    char *root_dir;       /* canonical root path */
} ScanResult;

bool scan_directory(ScanResult *sr, const char *root_path);
void scan_result_destroy(ScanResult *sr);

#endif
```

- [ ] **Step 3: Write scan.c**

```c
#include "parse/scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

static void add_file(ScanResult *sr, const char *path) {
    if (sr->file_count >= sr->file_capacity) {
        sr->file_capacity *= 2;
        sr->files = realloc(sr->files, sr->file_capacity * sizeof(char *));
    }
    sr->files[sr->file_count++] = strdup(path);
}

static void add_include(ScanResult *sr, const char *dir) {
    /* Deduplicate */
    for (uint32_t i = 0; i < sr->include_count; i++) {
        if (strcmp(sr->includes[i], dir) == 0) return;
    }
    if (sr->include_count >= sr->include_capacity) {
        sr->include_capacity *= 2;
        sr->includes = realloc(sr->includes, sr->include_capacity * sizeof(char *));
    }
    sr->includes[sr->include_count++] = strdup(dir);
}

static bool has_suffix(const char *name, const char *suffix) {
    size_t nlen = strlen(name);
    size_t slen = strlen(suffix);
    if (nlen < slen) return false;
    return strcmp(name + nlen - slen, suffix) == 0;
}

static void scan_recurse(ScanResult *sr, const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) return;

    bool has_header = false;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_recurse(sr, path);
        } else if (S_ISREG(st.st_mode)) {
            if (has_suffix(ent->d_name, ".c")) {
                add_file(sr, path);
            } else if (has_suffix(ent->d_name, ".h")) {
                has_header = true;
            }
        }
    }
    closedir(d);

    if (has_header) {
        add_include(sr, dir_path);
    }
}

bool scan_directory(ScanResult *sr, const char *root_path) {
    struct stat st;
    if (stat(root_path, &st) != 0 || !S_ISDIR(st.st_mode)) return false;

    memset(sr, 0, sizeof(ScanResult));
    sr->file_capacity = 64;
    sr->files = malloc(sr->file_capacity * sizeof(char *));
    sr->include_capacity = 32;
    sr->includes = malloc(sr->include_capacity * sizeof(char *));

    char resolved[PATH_MAX];
    if (realpath(root_path, resolved)) {
        sr->root_dir = strdup(resolved);
    } else {
        sr->root_dir = strdup(root_path);
    }

    scan_recurse(sr, sr->root_dir);
    return true;
}

void scan_result_destroy(ScanResult *sr) {
    for (uint32_t i = 0; i < sr->file_count; i++) free(sr->files[i]);
    free(sr->files);
    for (uint32_t i = 0; i < sr->include_count; i++) free(sr->includes[i]);
    free(sr->includes);
    free(sr->root_dir);
    memset(sr, 0, sizeof(ScanResult));
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

In `src/CMakeLists.txt`, add `parse/scan.c` to the `cgraph_lib` sources list.

In `tests/CMakeLists.txt`, add:

```cmake
add_executable(test_scan test_scan.c)
target_link_libraries(test_scan PRIVATE cgraph_lib unity)
target_include_directories(test_scan PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_scan PRIVATE FIXTURE_PATH="${FIXTURE_PATH}")
add_test(NAME test_scan COMMAND test_scan)
```

- [ ] **Step 5: Add a .h fixture file**

Create `tests/fixtures/sample_project/util.h`:

```c
#ifndef UTIL_H
#define UTIL_H
int add(int a, int b);
#endif
```

This ensures the sample_project fixture has at least one `.h` file for include-dir detection.

- [ ] **Step 6: Build and run test**

Run:
```bash
cd /home/shf/workspace-ai/new_work_521/c_graph/build && cmake .. && make test_scan && ./tests/test_scan
```

Expected: 3 tests pass.

- [ ] **Step 7: Commit**

```bash
git add src/parse/scan.h src/parse/scan.c tests/test_scan.c tests/fixtures/sample_project/util.h src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add directory scan module for --src-dir mode"
```

---

### Task 2: Integrate --src-dir into cmd_build

**Files:**
- Modify: `src/main.c` (the `cmd_build` function, lines 64-100)

- [ ] **Step 1: Write end-to-end test**

Create `tests/test_scan_build.sh`:

```bash
#!/bin/bash
set -e
CGRAPH="$1"
FIXTURE="$2"

# Test: build using --src-dir (no compile_commands.json)
$CGRAPH build --src-dir "$FIXTURE/sample_project" --output /tmp/cgraph_scan_test.db
if [ ! -f /tmp/cgraph_scan_test.db ]; then
    echo "FAIL: cgraph.db not created"
    exit 1
fi

# Verify we can query the result
INFO=$($CGRAPH info --db /tmp/cgraph_scan_test.db)
echo "$INFO" | grep -q "node_count"
if [ $? -ne 0 ]; then
    echo "FAIL: info command failed"
    exit 1
fi

echo "PASS"
rm -f /tmp/cgraph_scan_test.db
```

- [ ] **Step 2: Modify cmd_build to support --src-dir**

Replace the `cmd_build` function in `src/main.c`:

```c
static int cmd_build(int argc, char **argv) {
    const char *compdb_path = NULL;
    const char *src_dir = NULL;
    const char *output = "cgraph.db";
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--compile-commands") == 0 && i + 1 < argc) compdb_path = argv[++i];
        else if (strcmp(argv[i], "--src-dir") == 0 && i + 1 < argc) src_dir = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output = argv[++i];
    }

    if (!compdb_path && !src_dir) {
        fprintf(stderr, "Error: provide --compile-commands or --src-dir\n");
        return 1;
    }

    Graph g;
    graph_init(&g);

    if (compdb_path) {
        /* Existing compile_commands.json path */
        CompDB compdb;
        if (!compdb_parse(&compdb, compdb_path)) {
            fprintf(stderr, "Error: cannot parse %s\n", compdb_path);
            graph_destroy(&g);
            return 1;
        }
        fprintf(stderr, "Parsing %u translation units...\n", compdb.count);
        ParseResult r = parser_parse_project(&g, &compdb);
        if (!r.success) fprintf(stderr, "Warning: %s\n", r.error);
        fprintf(stderr, "Found %u functions, %u call edges\n", r.functions_found, r.calls_found);
        compdb_destroy(&compdb);
    } else {
        /* Directory scan path */
        ScanResult sr;
        if (!scan_directory(&sr, src_dir)) {
            fprintf(stderr, "Error: cannot scan directory %s\n", src_dir);
            graph_destroy(&g);
            return 1;
        }
        fprintf(stderr, "Scanning %u .c files from %s\n", sr.file_count, src_dir);

        /* Build -I args array */
        uint32_t arg_count = sr.include_count * 2;
        const char **parse_args = malloc((arg_count + 1) * sizeof(const char *));
        uint32_t idx = 0;
        for (uint32_t i = 0; i < sr.include_count; i++) {
            parse_args[idx++] = "-I";
            parse_args[idx++] = sr.includes[i];
        }
        parse_args[idx] = NULL;

        uint32_t total_funcs = 0, total_calls = 0;
        for (uint32_t i = 0; i < sr.file_count; i++) {
            ParseResult r = parser_parse_file(&g, sr.files[i], parse_args);
            if (!r.success) {
                fprintf(stderr, "Warning: %s: %s\n", sr.files[i], r.error);
            }
            total_funcs += r.functions_found;
            total_calls += r.calls_found;
        }
        fprintf(stderr, "Found %u functions, %u call edges\n", total_funcs, total_calls);
        free(parse_args);
        scan_result_destroy(&sr);
    }

    metrics_compute_fan(&g);
    fprintf(stderr, "Graph: %u nodes, %u edges\n", g.node_count, g.edge_count);

    if (!graph_serialize(&g, output)) {
        fprintf(stderr, "Error: cannot write %s\n", output);
        graph_destroy(&g);
        return 1;
    }
    fprintf(stderr, "Written: %s\n", output);
    graph_destroy(&g);
    return 0;
}
```

Also add `#include "parse/scan.h"` at the top of `main.c` (it's already importing parse headers there).

- [ ] **Step 3: Build and run e2e test**

Run:
```bash
cd /home/shf/workspace-ai/new_work_521/c_graph/build && cmake .. && make cgraph
chmod +x ../tests/test_scan_build.sh
../tests/test_scan_build.sh ./src/cgraph ../tests/fixtures
```

Expected: "PASS"

- [ ] **Step 4: Run all existing tests to verify no regressions**

Run:
```bash
cd /home/shf/workspace-ai/new_work_521/c_graph/build && make && ctest --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/main.c tests/test_scan_build.sh
git commit -m "feat: integrate --src-dir scan mode into build command"
```
