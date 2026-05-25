#ifndef CGRAPH_PARSER_H
#define CGRAPH_PARSER_H

#include "graph/graph.h"
#include "parse/compdb.h"
#include <stdbool.h>

typedef struct {
    bool success;
    uint32_t functions_found;
    uint32_t calls_found;
    char error[256];
} ParseResult;

ParseResult parser_parse_file(Graph *g, const char *filename, const char **args);
ParseResult parser_parse_project(Graph *g, const CompDB *compdb);

#endif
