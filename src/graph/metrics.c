#include "graph/metrics.h"

void metrics_compute_fan(Graph *g) {
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
