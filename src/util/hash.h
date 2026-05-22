#ifndef CGRAPH_HASH_H
#define CGRAPH_HASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *key;
    uint32_t value;
    bool occupied;
} HashEntry;

typedef struct {
    HashEntry *entries;
    size_t capacity;
    size_t count;
} HashMap;

void hashmap_init(HashMap *m, size_t initial_capacity);
void hashmap_put(HashMap *m, const char *key, uint32_t value);
bool hashmap_get(const HashMap *m, const char *key, uint32_t *out_value);
bool hashmap_remove(HashMap *m, const char *key);
void hashmap_destroy(HashMap *m);

#endif
