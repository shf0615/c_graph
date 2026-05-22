#include "util/arena.h"
#include <stdlib.h>
#include <string.h>

static ArenaBlock *block_new(size_t size) {
    ArenaBlock *b = malloc(sizeof(ArenaBlock) + size);
    if (!b) return NULL;
    b->next = NULL;
    b->size = size;
    b->used = 0;
    return b;
}

void arena_init(Arena *a, size_t default_block_size) {
    a->default_block_size = default_block_size ? default_block_size : (64 * 1024);
    a->head = block_new(a->default_block_size);
    a->current = a->head;
}

void *arena_alloc(Arena *a, size_t size) {
    size = (size + 7) & ~(size_t)7;

    if (a->current->used + size > a->current->size) {
        size_t block_size = size > a->default_block_size ? size : a->default_block_size;
        ArenaBlock *b = block_new(block_size);
        if (!b) return NULL;
        a->current->next = b;
        a->current = b;
    }
    void *ptr = a->current->data + a->current->used;
    a->current->used += size;
    return ptr;
}

char *arena_strdup(Arena *a, const char *s) {
    size_t len = strlen(s) + 1;
    char *dst = arena_alloc(a, len);
    if (dst) memcpy(dst, s, len);
    return dst;
}

void arena_destroy(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
    a->current = NULL;
}
