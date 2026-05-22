#ifndef CGRAPH_ARENA_H
#define CGRAPH_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t size;
    size_t used;
    uint8_t data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
    ArenaBlock *current;
    size_t default_block_size;
} Arena;

void arena_init(Arena *a, size_t default_block_size);
void *arena_alloc(Arena *a, size_t size);
char *arena_strdup(Arena *a, const char *s);
void arena_destroy(Arena *a);

#endif
