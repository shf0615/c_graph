#ifndef CGRAPH_QUALITY_H
#define CGRAPH_QUALITY_H
#include "graph/graph.h"
#include "query/traverse.h"

QueryResult query_complexity_top(const Graph *g, uint32_t top_n);
QueryResult query_coupling_top(const Graph *g, uint32_t top_n);
QueryResult query_dead_code(const Graph *g);
QueryResult query_large_functions(const Graph *g, uint32_t min_lines);

#endif
