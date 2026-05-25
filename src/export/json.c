#include "export/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Simple dynamic string buffer */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_init(Buf *b) { b->cap = 1024; b->data = malloc(b->cap); b->len = 0; b->data[0] = '\0'; }
static void buf_append(Buf *b, const char *s) {
    size_t sl = strlen(s);
    while (b->len + sl + 1 > b->cap) { b->cap *= 2; b->data = realloc(b->data, b->cap); }
    memcpy(b->data + b->len, s, sl + 1);
    b->len += sl;
}
static void buf_printf(Buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    buf_append(b, tmp);
}

static const char *node_type_str(NodeType t) {
    switch (t) {
        case NODE_FILE: return "file";
        case NODE_FUNCTION: return "function";
        case NODE_STRUCT: return "struct";
        case NODE_UNION: return "union";
        case NODE_ENUM: return "enum";
        case NODE_FIELD: return "field";
        case NODE_GLOBAL_VAR: return "global_var";
        case NODE_MACRO: return "macro";
        case NODE_TYPEDEF: return "typedef";
        case NODE_SYNC_PRIMITIVE: return "sync_primitive";
        case NODE_MODULE: return "module";
    }
    return "unknown";
}

static void format_file_path(Buf *b, const char *file, const JsonOptions *opts) {
    if (!file) { buf_append(b, "null"); return; }
    if (opts && opts->project_root) {
        buf_printf(b, "\"%s/%s\"", opts->project_root, file);
    } else {
        buf_printf(b, "\"%s\"", file);
    }
}

char *json_format_node(const Graph *g, uint32_t node_id, const JsonOptions *opts) {
    if (node_id >= g->node_count) return NULL;
    const Node *n = &g->nodes[node_id];
    Buf b;
    buf_init(&b);
    buf_append(&b, "{\n");
    buf_printf(&b, "  \"id\": %u,\n", n->id);
    buf_printf(&b, "  \"name\": \"%s\",\n", n->name);
    buf_printf(&b, "  \"type\": \"%s\",\n", node_type_str(n->type));
    buf_append(&b, "  \"file\": ");
    format_file_path(&b, n->file, opts);
    buf_append(&b, ",\n");
    buf_printf(&b, "  \"line_start\": %u,\n", n->line_start);
    buf_printf(&b, "  \"line_end\": %u,\n", n->line_end);
    buf_printf(&b, "  \"cyclomatic_complexity\": %u,\n", n->cyclomatic_complexity);
    buf_printf(&b, "  \"fan_in\": %u,\n", n->fan_in);
    buf_printf(&b, "  \"fan_out\": %u,\n", n->fan_out);
    buf_printf(&b, "  \"is_external\": %s\n", n->is_external ? "true" : "false");
    buf_append(&b, "}");
    return b.data;
}

char *json_format_query_result(const Graph *g, const QueryResult *qr, const JsonOptions *opts) {
    Buf b;
    buf_init(&b);
    buf_append(&b, "[\n");
    for (uint32_t i = 0; i < qr->count; i++) {
        if (i > 0) buf_append(&b, ",\n");
        const Node *n = &g->nodes[qr->node_ids[i]];
        buf_append(&b, "  {");
        buf_printf(&b, "\"id\": %u, \"name\": \"%s\", \"type\": \"%s\", \"file\": ",
                   n->id, n->name, node_type_str(n->type));
        format_file_path(&b, n->file, opts);
        buf_printf(&b, ", \"line_start\": %u, \"line_end\": %u", n->line_start, n->line_end);
        if (qr->depths) buf_printf(&b, ", \"depth\": %u", qr->depths[i]);
        buf_append(&b, "}");
    }
    buf_append(&b, "\n]");
    return b.data;
}

char *json_format_info(const Graph *g) {
    uint32_t funcs = 0, files = 0, structs = 0, globals = 0;
    for (uint32_t i = 0; i < g->node_count; i++) {
        switch (g->nodes[i].type) {
            case NODE_FUNCTION: funcs++; break;
            case NODE_FILE: files++; break;
            case NODE_STRUCT: case NODE_UNION: structs++; break;
            case NODE_GLOBAL_VAR: globals++; break;
            default: break;
        }
    }
    Buf b;
    buf_init(&b);
    buf_append(&b, "{\n");
    buf_printf(&b, "  \"total_nodes\": %u,\n", g->node_count);
    buf_printf(&b, "  \"total_edges\": %u,\n", g->edge_count);
    buf_printf(&b, "  \"functions\": %u,\n", funcs);
    buf_printf(&b, "  \"files\": %u,\n", files);
    buf_printf(&b, "  \"structs\": %u,\n", structs);
    buf_printf(&b, "  \"globals\": %u\n", globals);
    buf_append(&b, "}");
    return b.data;
}

char *json_format_node_list(const Graph *g, NodeType type, const JsonOptions *opts) {
    Buf b;
    buf_init(&b);
    buf_append(&b, "[\n");
    bool first = true;
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].type != type) continue;
        if (!first) buf_append(&b, ",\n");
        first = false;
        const Node *n = &g->nodes[i];
        buf_append(&b, "  {");
        buf_printf(&b, "\"id\": %u, \"name\": \"%s\", \"file\": ", n->id, n->name);
        format_file_path(&b, n->file, opts);
        buf_printf(&b, ", \"line_start\": %u, \"line_end\": %u", n->line_start, n->line_end);
        buf_append(&b, "}");
    }
    buf_append(&b, "\n]");
    return b.data;
}
