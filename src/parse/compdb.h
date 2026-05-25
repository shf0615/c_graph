#ifndef CGRAPH_COMPDB_H
#define CGRAPH_COMPDB_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char *directory;
    char *file;
    char *command;
    char **args;
    uint32_t argc;
} CompDBEntry;

typedef struct {
    CompDBEntry *entries;
    uint32_t count;
    uint32_t capacity;
} CompDB;

bool compdb_parse(CompDB *db, const char *path);
void compdb_destroy(CompDB *db);

#endif
