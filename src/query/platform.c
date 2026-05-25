#include "query/platform.h"
#include <stdlib.h>
#include <string.h>

QueryResult query_platform_deps(const Graph *g, const char *ifdef_macro) {
    QueryResult r = {0};
    uint32_t cap = 32;
    r.node_ids = malloc(cap * sizeof(uint32_t));

    /* Find the macro node */
    uint32_t macro_id;
    if (ifdef_macro) {
        if (!graph_find_node(g, ifdef_macro, &macro_id)) return r;
        /* Find all symbols with IFDEF_DEPENDS edge to this macro */
        for (uint32_t e = 0; e < g->edge_count; e++) {
            if (g->edges[e].type == EDGE_IFDEF_DEPENDS && g->edges[e].to == macro_id) {
                if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
                r.node_ids[r.count++] = g->edges[e].from;
            }
        }
    } else {
        /* List all symbols that have any IFDEF_DEPENDS edge */
        uint32_t bitmap_size = (g->node_count + 31) / 32;
        uint32_t *seen = calloc(bitmap_size, sizeof(uint32_t));
        for (uint32_t e = 0; e < g->edge_count; e++) {
            if (g->edges[e].type == EDGE_IFDEF_DEPENDS) {
                uint32_t id = g->edges[e].from;
                if (!(seen[id / 32] & (1u << (id % 32)))) {
                    seen[id / 32] |= (1u << (id % 32));
                    if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
                    r.node_ids[r.count++] = id;
                }
            }
        }
        free(seen);
    }
    return r;
}

/* Known syscall/platform API prefixes */
static bool is_syscall(const char *name) {
    static const char *prefixes[] = {
        "open", "close", "read", "write", "ioctl", "mmap", "munmap",
        "socket", "bind", "listen", "accept", "connect", "send", "recv",
        "fork", "exec", "wait", "kill", "signal", "sigaction",
        "pthread_", "sem_", "shm_", "mq_",
        "epoll_", "poll", "select",
        NULL
    };
    for (const char **p = prefixes; *p; p++) {
        if (strncmp(name, *p, strlen(*p)) == 0) return true;
    }
    return false;
}

QueryResult query_syscalls(const Graph *g) {
    QueryResult r = {0};
    uint32_t cap = 32;
    r.node_ids = malloc(cap * sizeof(uint32_t));

    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != NODE_FUNCTION) continue;
        if (!g->nodes[i].is_external) continue;
        if (is_syscall(g->nodes[i].name)) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}

QueryResult query_compiler_builtins(const Graph *g) {
    QueryResult r = {0};
    uint32_t cap = 16;
    r.node_ids = malloc(cap * sizeof(uint32_t));

    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != NODE_FUNCTION) continue;
        if (strncmp(g->nodes[i].name, "__builtin_", 10) == 0) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}
