#ifndef CGRAPH_HTML_H
#define CGRAPH_HTML_H
#include "graph/graph.h"
#include <stdbool.h>

bool html_export(const Graph *g, const char *output_dir);

#endif
