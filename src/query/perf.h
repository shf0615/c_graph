#ifndef CGRAPH_PERF_H
#define CGRAPH_PERF_H
#include "graph/graph.h"
#include "query/traverse.h"

/* Functions that call malloc/calloc/realloc */
QueryResult query_alloc_sites(const Graph *g, const char *func_filter);

/* Paired alloc/free analysis: functions that allocate but never free (or vice versa) */
QueryResult query_alloc_pairs(const Graph *g);

/* Hot path: expand call chain from entry, annotating alloc points */
QueryResult query_hotpath(const Graph *g, uint32_t entry_id, uint32_t max_depth);

#endif
