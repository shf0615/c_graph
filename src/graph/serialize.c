#include "graph/serialize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0x50524743  /* "CGRP" little-endian */
#define VERSION 1

typedef struct {
    char *data;
    uint32_t size;
    uint32_t capacity;
    HashMap offsets;
} StringTable;

static void strtab_init(StringTable *st) {
    st->capacity = 4096;
    st->data = malloc(st->capacity);
    st->size = 0;
    hashmap_init(&st->offsets, 256);
}

static uint32_t strtab_add(StringTable *st, const char *s) {
    if (!s) return UINT32_MAX;
    uint32_t existing;
    if (hashmap_get(&st->offsets, s, &existing)) return existing;

    uint32_t offset = st->size;
    size_t len = strlen(s) + 1;
    while (st->size + len > st->capacity) {
        st->capacity *= 2;
        st->data = realloc(st->data, st->capacity);
    }
    memcpy(st->data + st->size, s, len);
    st->size += (uint32_t)len;
    hashmap_put(&st->offsets, s, offset);
    return offset;
}

static void strtab_destroy(StringTable *st) {
    free(st->data);
    hashmap_destroy(&st->offsets);
}

bool graph_serialize(const Graph *g, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    StringTable st;
    strtab_init(&st);

    uint32_t *name_offsets = malloc(g->node_count * sizeof(uint32_t));
    uint32_t *file_offsets = malloc(g->node_count * sizeof(uint32_t));
    for (uint32_t i = 0; i < g->node_count; i++) {
        name_offsets[i] = strtab_add(&st, g->nodes[i].name);
        file_offsets[i] = strtab_add(&st, g->nodes[i].file);
    }

    uint32_t magic = MAGIC, version = VERSION;
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&g->node_count, 4, 1, f);
    fwrite(&g->edge_count, 4, 1, f);
    fwrite(&st.size, 4, 1, f);

    for (uint32_t i = 0; i < g->node_count; i++) {
        Node *n = &g->nodes[i];
        fwrite(&n->id, 4, 1, f);
        uint32_t type = n->type;
        fwrite(&type, 4, 1, f);
        fwrite(&name_offsets[i], 4, 1, f);
        fwrite(&file_offsets[i], 4, 1, f);
        fwrite(&n->line_start, 4, 1, f);
        fwrite(&n->line_end, 4, 1, f);
        fwrite(&n->cyclomatic_complexity, 4, 1, f);
        fwrite(&n->fan_in, 4, 1, f);
        fwrite(&n->fan_out, 4, 1, f);
        uint8_t flags = (n->is_thread_entry ? 1 : 0)
                      | (n->is_external ? 2 : 0)
                      | (n->is_shared ? 4 : 0);
        fwrite(&flags, 1, 1, f);
    }

    for (uint32_t i = 0; i < g->edge_count; i++) {
        Edge *e = &g->edges[i];
        fwrite(&e->from, 4, 1, f);
        fwrite(&e->to, 4, 1, f);
        uint32_t type = e->type;
        fwrite(&type, 4, 1, f);
        fwrite(&e->attrs, 1, 1, f);
    }

    fwrite(st.data, 1, st.size, f);

    fclose(f);
    free(name_offsets);
    free(file_offsets);
    strtab_destroy(&st);
    return true;
}

bool graph_deserialize(Graph *g, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    uint32_t magic, version, node_count, edge_count, strtab_size;
    fread(&magic, 4, 1, f);
    fread(&version, 4, 1, f);
    if (magic != MAGIC || version != VERSION) { fclose(f); return false; }

    fread(&node_count, 4, 1, f);
    fread(&edge_count, 4, 1, f);
    fread(&strtab_size, 4, 1, f);

    /* Read raw node data */
    typedef struct { uint32_t id, type, name_off, file_off, ls, le, cc, fi, fo; uint8_t flags; } RawNode;
    RawNode *raw_nodes = malloc(node_count * sizeof(RawNode));
    for (uint32_t i = 0; i < node_count; i++) {
        fread(&raw_nodes[i].id, 4, 1, f);
        fread(&raw_nodes[i].type, 4, 1, f);
        fread(&raw_nodes[i].name_off, 4, 1, f);
        fread(&raw_nodes[i].file_off, 4, 1, f);
        fread(&raw_nodes[i].ls, 4, 1, f);
        fread(&raw_nodes[i].le, 4, 1, f);
        fread(&raw_nodes[i].cc, 4, 1, f);
        fread(&raw_nodes[i].fi, 4, 1, f);
        fread(&raw_nodes[i].fo, 4, 1, f);
        fread(&raw_nodes[i].flags, 1, 1, f);
    }

    typedef struct { uint32_t from, to, type; uint8_t attrs; } RawEdge;
    RawEdge *raw_edges = malloc(edge_count * sizeof(RawEdge));
    for (uint32_t i = 0; i < edge_count; i++) {
        fread(&raw_edges[i].from, 4, 1, f);
        fread(&raw_edges[i].to, 4, 1, f);
        fread(&raw_edges[i].type, 4, 1, f);
        fread(&raw_edges[i].attrs, 1, 1, f);
    }

    char *strtab = malloc(strtab_size ? strtab_size : 1);
    if (strtab_size > 0) fread(strtab, 1, strtab_size, f);
    fclose(f);

    graph_init(g);

    for (uint32_t i = 0; i < node_count; i++) {
        const char *name = raw_nodes[i].name_off < strtab_size ? strtab + raw_nodes[i].name_off : "";
        const char *file = NULL;
        if (raw_nodes[i].file_off != UINT32_MAX && raw_nodes[i].file_off < strtab_size)
            file = strtab + raw_nodes[i].file_off;
        uint32_t id = graph_add_node(g, raw_nodes[i].type, name, file, raw_nodes[i].ls, raw_nodes[i].le);
        g->nodes[id].cyclomatic_complexity = raw_nodes[i].cc;
        g->nodes[id].fan_in = raw_nodes[i].fi;
        g->nodes[id].fan_out = raw_nodes[i].fo;
        g->nodes[id].is_thread_entry = raw_nodes[i].flags & 1;
        g->nodes[id].is_external = (raw_nodes[i].flags & 2) != 0;
        g->nodes[id].is_shared = (raw_nodes[i].flags & 4) != 0;
    }

    for (uint32_t i = 0; i < edge_count; i++) {
        graph_add_edge(g, raw_edges[i].from, raw_edges[i].to, raw_edges[i].type, raw_edges[i].attrs);
    }

    free(raw_nodes);
    free(raw_edges);
    free(strtab);
    return true;
}
