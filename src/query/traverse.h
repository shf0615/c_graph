#ifndef CGRAPH_TRAVERSE_H
#define CGRAPH_TRAVERSE_H

#include "graph/graph.h"

typedef struct {
    uint32_t *node_ids;
    uint32_t count;
    uint32_t *depths;
} QueryResult;

void query_result_free(QueryResult *r);
QueryResult query_callees(const Graph *g, uint32_t node_id, uint32_t max_depth);
QueryResult query_callers(const Graph *g, uint32_t node_id, uint32_t max_depth);
QueryResult query_path(const Graph *g, uint32_t from_id, uint32_t to_id);

#endif
