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
    Edge *result = malloc(g->edge_count * sizeof(Edge));
    uint32_t count = 0;
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from == node_id) {
            result[count++] = g->edges[i];
        }
    }
    if (count == 0) {
        free(result);
        return (EdgeList){.edges = NULL, .count = 0};
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
    if (count == 0) {
        free(result);
        return (EdgeList){.edges = NULL, .count = 0};
    }
    result = realloc(result, count * sizeof(Edge));
    return (EdgeList){.edges = result, .count = count};
}
