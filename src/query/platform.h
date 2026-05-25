#ifndef CGRAPH_PLATFORM_H
#define CGRAPH_PLATFORM_H
#include "graph/graph.h"
#include "query/traverse.h"

/* Symbols guarded by #ifdef MACRO */
QueryResult query_platform_deps(const Graph *g, const char *ifdef_macro);

/* Functions that call known syscall/platform APIs */
QueryResult query_syscalls(const Graph *g);

/* Functions that use compiler builtins (__builtin_*) */
QueryResult query_compiler_builtins(const Graph *g);

#endif
