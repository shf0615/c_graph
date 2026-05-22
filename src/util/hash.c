#include "util/hash.h"
#include <stdlib.h>
#include <string.h>

static uint32_t fnv1a(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) {
        h ^= (uint8_t)*s;
        h *= 16777619u;
    }
    return h;
}

void hashmap_init(HashMap *m, size_t initial_capacity) {
    m->capacity = initial_capacity < 8 ? 8 : initial_capacity;
    m->count = 0;
    m->entries = calloc(m->capacity, sizeof(HashEntry));
}

static void hashmap_resize(HashMap *m) {
    size_t old_cap = m->capacity;
    HashEntry *old = m->entries;
    m->capacity *= 2;
    m->entries = calloc(m->capacity, sizeof(HashEntry));
    m->count = 0;
    for (size_t i = 0; i < old_cap; i++) {
        if (old[i].occupied) {
            hashmap_put(m, old[i].key, old[i].value);
            free(old[i].key);
        }
    }
    free(old);
}

void hashmap_put(HashMap *m, const char *key, uint32_t value) {
    if (m->count * 10 >= m->capacity * 7) {
        hashmap_resize(m);
    }
    uint32_t idx = fnv1a(key) % m->capacity;
    while (m->entries[idx].occupied) {
        if (strcmp(m->entries[idx].key, key) == 0) {
            m->entries[idx].value = value;
            return;
        }
        idx = (idx + 1) % m->capacity;
    }
    m->entries[idx].key = strdup(key);
    m->entries[idx].value = value;
    m->entries[idx].occupied = true;
    m->count++;
}

bool hashmap_get(const HashMap *m, const char *key, uint32_t *out_value) {
    uint32_t idx = fnv1a(key) % m->capacity;
    while (m->entries[idx].occupied) {
        if (strcmp(m->entries[idx].key, key) == 0) {
            *out_value = m->entries[idx].value;
            return true;
        }
        idx = (idx + 1) % m->capacity;
    }
    return false;
}

bool hashmap_remove(HashMap *m, const char *key) {
    uint32_t idx = fnv1a(key) % m->capacity;
    while (m->entries[idx].occupied) {
        if (strcmp(m->entries[idx].key, key) == 0) {
            free(m->entries[idx].key);
            m->entries[idx].occupied = false;
            m->count--;
            // Rehash subsequent entries
            uint32_t next = (idx + 1) % m->capacity;
            while (m->entries[next].occupied) {
                HashEntry e = m->entries[next];
                m->entries[next].occupied = false;
                m->count--;
                hashmap_put(m, e.key, e.value);
                free(e.key);
                next = (next + 1) % m->capacity;
            }
            return true;
        }
        idx = (idx + 1) % m->capacity;
    }
    return false;
}

void hashmap_destroy(HashMap *m) {
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].occupied) free(m->entries[i].key);
    }
    free(m->entries);
    m->entries = NULL;
    m->capacity = 0;
    m->count = 0;
}
