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
