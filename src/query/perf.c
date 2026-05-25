#include "query/perf.h"
#include <stdlib.h>
#include <string.h>

QueryResult query_alloc_sites(const Graph *g, const char *func_filter) {
    QueryResult r = {0};
    uint32_t cap = 32;
    r.node_ids = malloc(cap * sizeof(uint32_t));

    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != NODE_FUNCTION) continue;
        if (func_filter && strcmp(g->nodes[i].name, func_filter) != 0) continue;

        bool allocates = false;
        for (uint32_t e = 0; e < g->edge_count; e++) {
            if (g->edges[e].from == i && g->edges[e].type == EDGE_ALLOCATES) {
                allocates = true;
                break;
            }
        }
        if (allocates) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}

QueryResult query_alloc_pairs(const Graph *g) {
    /* Find functions that allocate but don't free (potential leaks) */
    QueryResult r = {0};
    uint32_t cap = 32;
    r.node_ids = malloc(cap * sizeof(uint32_t));

    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != NODE_FUNCTION) continue;

        bool has_alloc = false, has_free = false;
        for (uint32_t e = 0; e < g->edge_count; e++) {
            if (g->edges[e].from != i) continue;
            if (g->edges[e].type == EDGE_ALLOCATES) has_alloc = true;
            if (g->edges[e].type == EDGE_FREES) has_free = true;
        }
        /* Report functions that allocate without freeing */
        if (has_alloc && !has_free) {
            if (r.count >= cap) { cap *= 2; r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t)); }
            r.node_ids[r.count++] = i;
        }
    }
    return r;
}

QueryResult query_hotpath(const Graph *g, uint32_t entry_id, uint32_t max_depth) {
    /* DFS from entry, collecting call chain (prefers non-external callees) */
    QueryResult r = {0};
    uint32_t cap = 64;
    r.node_ids = malloc(cap * sizeof(uint32_t));
    r.depths = malloc(cap * sizeof(uint32_t));

    uint32_t bitmap_size = (g->node_count + 31) / 32;
    uint32_t *visited = calloc(bitmap_size, sizeof(uint32_t));

    /* Iterative DFS with stack */
    typedef struct { uint32_t id; uint32_t depth; } Item;
    Item *stack = malloc((g->node_count + 1) * sizeof(Item));
    int top = 0;
    stack[top++] = (Item){entry_id, 0};
    visited[entry_id / 32] |= (1u << (entry_id % 32));

    while (top > 0) {
        Item item = stack[--top];

        if (r.count >= cap) {
            cap *= 2;
            r.node_ids = realloc(r.node_ids, cap * sizeof(uint32_t));
            r.depths = realloc(r.depths, cap * sizeof(uint32_t));
        }
        r.node_ids[r.count] = item.id;
        r.depths[r.count] = item.depth;
        r.count++;

        if (item.depth >= max_depth) continue;

        for (uint32_t e = 0; e < g->edge_count; e++) {
            if (g->edges[e].from != item.id) continue;
            EdgeType et = g->edges[e].type;
            if (et != EDGE_CALLS && et != EDGE_CALLS_FP && et != EDGE_ALLOCATES) continue;
            uint32_t next = g->edges[e].to;
            if (visited[next / 32] & (1u << (next % 32))) continue;
            visited[next / 32] |= (1u << (next % 32));
            stack[top++] = (Item){next, item.depth + 1};
        }
    }

    free(visited);
    free(stack);
    return r;
}
