#include "query/lockorder.h"
#include <stdlib.h>
#include <string.h>

void lock_order_result_free(LockOrderResult *r) {
    free(r->pairs);
    r->pairs = NULL;
    r->count = 0;
}

LockOrderResult query_lock_order(const Graph *g) {
    LockOrderResult result = {0};
    uint32_t cap = 32;
    result.pairs = malloc(cap * sizeof(LockOrderPair));

    /* For each function, find all ACQUIRES_LOCK edges and record their order.
     * If function acquires lock A then lock B (based on edge order which
     * reflects source order), record (A, B). */

    for (uint32_t func = 0; func < g->node_count; func++) {
        if (g->nodes[func].type != NODE_FUNCTION) continue;

        /* Collect locks acquired by this function in edge order */
        uint32_t locks[64];
        uint32_t lock_count = 0;

        for (uint32_t e = 0; e < g->edge_count && lock_count < 64; e++) {
            if (g->edges[e].from == func && g->edges[e].type == EDGE_ACQUIRES_LOCK) {
                locks[lock_count++] = g->edges[e].to;
            }
        }

        /* Record all ordered pairs */
        for (uint32_t i = 0; i < lock_count; i++) {
            for (uint32_t j = i + 1; j < lock_count; j++) {
                if (result.count >= cap) {
                    cap *= 2;
                    result.pairs = realloc(result.pairs, cap * sizeof(LockOrderPair));
                }
                result.pairs[result.count++] = (LockOrderPair){
                    .lock_a = locks[i],
                    .lock_b = locks[j],
                    .func_id = func
                };
            }
        }
    }

    /* Check for cycles: if (A,B) and (B,A) both exist, we have a potential deadlock */
    for (uint32_t i = 0; i < result.count && !result.has_cycle; i++) {
        for (uint32_t j = i + 1; j < result.count; j++) {
            if (result.pairs[i].lock_a == result.pairs[j].lock_b &&
                result.pairs[i].lock_b == result.pairs[j].lock_a) {
                result.has_cycle = true;
                break;
            }
        }
    }

    return result;
}
