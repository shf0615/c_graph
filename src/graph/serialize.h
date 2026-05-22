#ifndef CGRAPH_SERIALIZE_H
#define CGRAPH_SERIALIZE_H

#include "graph/graph.h"
#include <stdbool.h>

bool graph_serialize(const Graph *g, const char *path);
bool graph_deserialize(Graph *g, const char *path);

#endif
