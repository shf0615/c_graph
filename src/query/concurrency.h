#ifndef CGRAPH_CONCURRENCY_H
#define CGRAPH_CONCURRENCY_H
#include "graph/graph.h"
#include "query/traverse.h"

QueryResult query_threads(const Graph *g);
QueryResult query_shared_resources(const Graph *g);
QueryResult query_data_race_suspects(const Graph *g);

#endif
