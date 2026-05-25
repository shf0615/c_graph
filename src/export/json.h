#ifndef CGRAPH_JSON_H
#define CGRAPH_JSON_H

#include "graph/graph.h"
#include "query/traverse.h"

typedef struct {
    const char *project_root;
} JsonOptions;

char *json_format_node(const Graph *g, uint32_t node_id, const JsonOptions *opts);
char *json_format_query_result(const Graph *g, const QueryResult *qr, const JsonOptions *opts);
char *json_format_info(const Graph *g);
char *json_format_node_list(const Graph *g, NodeType type, const JsonOptions *opts);

#endif
