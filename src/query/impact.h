#ifndef CGRAPH_IMPACT_H
#define CGRAPH_IMPACT_H
#include "graph/graph.h"
#include "query/traverse.h"

QueryResult query_impact(const Graph *g, uint32_t node_id);

#endif
