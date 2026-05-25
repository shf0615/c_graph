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

static QueryResult bfs(const Graph *g, uint32_t start, uint32_t max_depth, bool reverse) {
    QueryResult result = {0};
    uint32_t cap = 64;
    result.node_ids = malloc(cap * sizeof(uint32_t));
    result.depths = malloc(cap * sizeof(uint32_t));

    uint32_t bitmap_size = (g->node_count + 31) / 32;
    uint32_t *visited = calloc(bitmap_size, sizeof(uint32_t));
    visited[start / 32] |= (1u << (start % 32));

    typedef struct { uint32_t id; uint32_t depth; } QItem;
    QItem *queue = malloc((g->node_count + 1) * sizeof(QItem));
    uint32_t q_head = 0, q_tail = 0;
    queue[q_tail++] = (QItem){start, 0};

    while (q_head < q_tail) {
        QItem item = queue[q_head++];
        if (item.depth >= max_depth) continue;

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

    uint32_t *queue = malloc((g->node_count + 1) * sizeof(uint32_t));
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
        uint32_t path[1024];
        uint32_t len = 0;
        uint32_t cur = to_id;
        while (cur != from_id && len < 1024) {
            path[len++] = cur;
            cur = parent[cur];
        }
        path[len++] = from_id;
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
