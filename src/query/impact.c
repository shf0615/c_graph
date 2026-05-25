#include "query/impact.h"
#include <stdlib.h>

QueryResult query_impact(const Graph *g, uint32_t node_id) {
    QueryResult result = {0};
    uint32_t cap = 64;
    result.node_ids = malloc(cap * sizeof(uint32_t));

    uint32_t bitmap_size = (g->node_count + 31) / 32;
    uint32_t *visited = calloc(bitmap_size, sizeof(uint32_t));
    visited[node_id / 32] |= (1u << (node_id % 32));

    uint32_t *queue = malloc((g->node_count + 1) * sizeof(uint32_t));
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
