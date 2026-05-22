#ifndef CGRAPH_GRAPH_H
#define CGRAPH_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include "util/arena.h"
#include "util/hash.h"

typedef enum {
    NODE_FILE,
    NODE_FUNCTION,
    NODE_STRUCT,
    NODE_UNION,
    NODE_ENUM,
    NODE_FIELD,
    NODE_GLOBAL_VAR,
    NODE_MACRO,
    NODE_TYPEDEF,
    NODE_SYNC_PRIMITIVE,
    NODE_MODULE,
} NodeType;

typedef enum {
    EDGE_CALLS,
    EDGE_CALLS_FP,
    EDGE_INCLUDES,
    EDGE_DEFINED_IN,
    EDGE_REFERENCES_TYPE,
    EDGE_ACCESSES_FIELD,
    EDGE_READS_GLOBAL,
    EDGE_WRITES_GLOBAL,
    EDGE_ALLOCATES,
    EDGE_FREES,
    EDGE_GUARDED_BY,
    EDGE_ACQUIRES_LOCK,
    EDGE_CREATES_THREAD,
    EDGE_IFDEF_DEPENDS,
    EDGE_BELONGS_TO,
} EdgeType;

typedef struct {
    uint32_t id;
    NodeType type;
    char *name;
    char *file;
    uint32_t line_start;
    uint32_t line_end;
    uint32_t cyclomatic_complexity;
    uint32_t fan_in;
    uint32_t fan_out;
    bool is_thread_entry;
    bool is_external;
    bool is_shared;
} Node;

typedef struct {
    uint32_t from;
    uint32_t to;
    EdgeType type;
    uint8_t attrs;
} Edge;

typedef struct {
    Edge *edges;
    uint32_t count;
} EdgeList;

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_capacity;

    Edge *edges;
    uint32_t edge_count;
    uint32_t edge_capacity;

    HashMap name_index;
    Arena arena;
} Graph;

void graph_init(Graph *g);
void graph_destroy(Graph *g);

uint32_t graph_add_node(Graph *g, NodeType type, const char *name,
                        const char *file, uint32_t line_start, uint32_t line_end);
Node *graph_get_node(Graph *g, uint32_t id);
bool graph_find_node(const Graph *g, const char *name, uint32_t *out_id);

void graph_add_edge(Graph *g, uint32_t from, uint32_t to, EdgeType type, uint8_t attrs);
EdgeList graph_edges_from(const Graph *g, uint32_t node_id);
EdgeList graph_edges_to(const Graph *g, uint32_t node_id);

#endif
