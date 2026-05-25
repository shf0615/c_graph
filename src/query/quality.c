#include "query/quality.h"
#include <stdlib.h>
#include <string.h>

QueryResult query_complexity_top(const Graph *g, uint32_t top_n) {
    uint32_t *ids = malloc(g->node_count * sizeof(uint32_t));
    uint32_t count = 0;
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type == NODE_FUNCTION && !g->nodes[i].is_external)
            ids[count++] = i;
    }
    /* Sort by complexity descending (insertion sort, fine for target scale) */
    for (uint32_t i = 1; i < count; i++) {
        uint32_t key = ids[i];
        int j = (int)i - 1;
        while (j >= 0 && g->nodes[ids[j]].cyclomatic_complexity < g->nodes[key].cyclomatic_complexity) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key;
    }
    uint32_t n = count < top_n ? count : top_n;
    return (QueryResult){.node_ids = ids, .count = n};
}

QueryResult query_coupling_top(const Graph *g, uint32_t top_n) {
    uint32_t *ids = malloc(g->node_count * sizeof(uint32_t));
    uint32_t count = 0;
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type == NODE_FUNCTION && !g->nodes[i].is_external)
            ids[count++] = i;
    }
    for (uint32_t i = 1; i < count; i++) {
        uint32_t key = ids[i];
        uint32_t key_c = g->nodes[key].fan_in * g->nodes[key].fan_out;
        int j = (int)i - 1;
        while (j >= 0 && g->nodes[ids[j]].fan_in * g->nodes[ids[j]].fan_out < key_c) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key;
    }
    uint32_t n = count < top_n ? count : top_n;
    return (QueryResult){.node_ids = ids, .count = n};
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
