#include "parse/parser.h"
#include <clang-c/Index.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    Graph *graph;
    uint32_t current_func_id;
    bool in_function;
    uint32_t funcs_found;
    uint32_t calls_found;
    uint32_t complexity;  /* running complexity count for current function */
    const char *base_dir;
} VisitorCtx;

static const char *make_relative(const char *full_path, const char *base_dir) {
    if (!base_dir || !full_path) return full_path;
    size_t base_len = strlen(base_dir);
    if (strncmp(full_path, base_dir, base_len) == 0) {
        const char *rel = full_path + base_len;
        if (*rel == '/') rel++;
        return rel;
    }
    return full_path;
}

static uint32_t ensure_func_node(Graph *g, CXCursor cursor, const char *base_dir) {
    CXString name_cx = clang_getCursorSpelling(cursor);
    const char *name = clang_getCString(name_cx);

    if (!name || name[0] == '\0') {
        clang_disposeString(name_cx);
        return UINT32_MAX;
    }

    uint32_t existing;
    if (graph_find_node(g, name, &existing)) {
        clang_disposeString(name_cx);
        return existing;
    }

    CXSourceLocation loc = clang_getCursorLocation(cursor);
    CXFile file;
    unsigned line_start;
    clang_getFileLocation(loc, &file, &line_start, NULL, NULL);

    const char *file_path = NULL;
    CXString file_cx = clang_getFileName(file);
    if (clang_getCString(file_cx))
        file_path = make_relative(clang_getCString(file_cx), base_dir);

    CXSourceRange range = clang_getCursorExtent(cursor);
    CXSourceLocation end = clang_getRangeEnd(range);
    unsigned line_end;
    clang_getFileLocation(end, NULL, &line_end, NULL, NULL);

    uint32_t id = graph_add_node(g, NODE_FUNCTION, name, file_path, line_start, line_end);

    if (!clang_isCursorDefinition(cursor)) {
        g->nodes[id].is_external = true;
    }

    clang_disposeString(name_cx);
    clang_disposeString(file_cx);
    return id;
}

static bool is_alloc_func(const char *name) {
    return strcmp(name, "malloc") == 0 || strcmp(name, "calloc") == 0 ||
           strcmp(name, "realloc") == 0 || strcmp(name, "aligned_alloc") == 0;
}

static bool is_free_func(const char *name) {
    return strcmp(name, "free") == 0;
}

static bool is_thread_create_func(const char *name) {
    return strcmp(name, "pthread_create") == 0 ||
           strcmp(name, "xTaskCreate") == 0 ||
           strcmp(name, "osThreadNew") == 0;
}

static bool is_lock_func(const char *name) {
    return strstr(name, "mutex_lock") != NULL ||
           strstr(name, "spin_lock") != NULL ||
           strstr(name, "sem_wait") != NULL ||
           strcmp(name, "pthread_mutex_lock") == 0 ||
           strcmp(name, "xSemaphoreTake") == 0;
}

static enum CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData data) {
    VisitorCtx *ctx = (VisitorCtx *)data;
    enum CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_FunctionDecl && clang_isCursorDefinition(cursor)) {
        ctx->current_func_id = ensure_func_node(ctx->graph, cursor, ctx->base_dir);
        if (ctx->current_func_id == UINT32_MAX) return CXChildVisit_Continue;
        ctx->in_function = true;
        ctx->complexity = 1; /* base complexity */
        ctx->funcs_found++;
        clang_visitChildren(cursor, visitor, data);
        ctx->graph->nodes[ctx->current_func_id].cyclomatic_complexity = ctx->complexity;
        ctx->in_function = false;
        return CXChildVisit_Continue;
    }

    if (!ctx->in_function) return CXChildVisit_Recurse;

    /* Complexity: count decision points */
    switch (kind) {
        case CXCursor_IfStmt:
        case CXCursor_ForStmt:
        case CXCursor_WhileStmt:
        case CXCursor_DoStmt:
        case CXCursor_CaseStmt:
        case CXCursor_ConditionalOperator: /* ternary */
            ctx->complexity++;
            break;
        case CXCursor_BinaryOperator: {
            /* Count && and || as decision points */
            CXString tok = clang_getCursorSpelling(cursor);
            const char *s = clang_getCString(tok);
            if (s && (strcmp(s, "&&") == 0 || strcmp(s, "||") == 0))
                ctx->complexity++;
            clang_disposeString(tok);
            break;
        }
        default:
            break;
    }

    /* Call expressions */
    if (kind == CXCursor_CallExpr) {
        CXCursor callee = clang_getCursorReferenced(cursor);
        if (!clang_Cursor_isNull(callee) && clang_getCursorKind(callee) == CXCursor_FunctionDecl) {
            CXString callee_name = clang_getCursorSpelling(callee);
            const char *cname = clang_getCString(callee_name);

            uint32_t callee_id = ensure_func_node(ctx->graph, callee, ctx->base_dir);
            if (callee_id != UINT32_MAX) {
                if (is_alloc_func(cname)) {
                    graph_add_edge(ctx->graph, ctx->current_func_id, callee_id, EDGE_ALLOCATES, 0);
                } else if (is_free_func(cname)) {
                    graph_add_edge(ctx->graph, ctx->current_func_id, callee_id, EDGE_FREES, 0);
                } else if (is_lock_func(cname)) {
                    graph_add_edge(ctx->graph, ctx->current_func_id, callee_id, EDGE_ACQUIRES_LOCK, 0);
                } else if (is_thread_create_func(cname)) {
                    graph_add_edge(ctx->graph, ctx->current_func_id, callee_id, EDGE_CREATES_THREAD, 0);
                } else {
                    graph_add_edge(ctx->graph, ctx->current_func_id, callee_id, EDGE_CALLS, 0);
                }
                ctx->calls_found++;
            }
            clang_disposeString(callee_name);
        }
    }

    /* Global variable references */
    if (kind == CXCursor_DeclRefExpr) {
        CXCursor ref = clang_getCursorReferenced(cursor);
        if (!clang_Cursor_isNull(ref) && clang_getCursorKind(ref) == CXCursor_VarDecl) {
            enum CXLinkageKind linkage = clang_getCursorLinkage(ref);
            if (linkage == CXLinkage_External || linkage == CXLinkage_Internal) {
                /* Check if file-scope (not local) */
                CXCursor semantic_parent = clang_getCursorSemanticParent(ref);
                if (clang_getCursorKind(semantic_parent) == CXCursor_TranslationUnit) {
                    CXString var_name = clang_getCursorSpelling(ref);
                    const char *vname = clang_getCString(var_name);
                    if (vname && vname[0]) {
                        uint32_t var_id;
                        if (!graph_find_node(ctx->graph, vname, &var_id)) {
                            var_id = graph_add_node(ctx->graph, NODE_GLOBAL_VAR, vname, NULL, 0, 0);
                            if (linkage == CXLinkage_External)
                                ctx->graph->nodes[var_id].is_shared = true;
                        }
                        /* Determine read vs write based on parent context */
                        /* Simplified: treat all as reads, writes detected via assignment parent */
                        graph_add_edge(ctx->graph, ctx->current_func_id, var_id, EDGE_READS_GLOBAL, 0);
                    }
                    clang_disposeString(var_name);
                }
            }
        }
    }

    return CXChildVisit_Recurse;
}

ParseResult parser_parse_file(Graph *g, const char *filename, const char **args) {
    ParseResult result = {0};
    CXIndex index = clang_createIndex(0, 0);

    int argc = 0;
    if (args) { while (args[argc]) argc++; }

    CXTranslationUnit tu = clang_parseTranslationUnit(
        index, filename, args, argc, NULL, 0, CXTranslationUnit_None);

    if (!tu) {
        snprintf(result.error, sizeof(result.error), "Failed to parse %s", filename);
        clang_disposeIndex(index);
        return result;
    }

    /* Derive base_dir from filename */
    char base_dir[2048] = {0};
    const char *last_slash = strrchr(filename, '/');
    if (last_slash) {
        size_t len = last_slash - filename;
        memcpy(base_dir, filename, len);
    }

    VisitorCtx ctx = {
        .graph = g,
        .in_function = false,
        .funcs_found = 0,
        .calls_found = 0,
        .base_dir = base_dir[0] ? base_dir : NULL,
    };

    CXCursor root = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(root, visitor, &ctx);

    result.success = true;
    result.functions_found = ctx.funcs_found;
    result.calls_found = ctx.calls_found;

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return result;
}

ParseResult parser_parse_project(Graph *g, const CompDB *compdb) {
    ParseResult total = {.success = true};
    for (uint32_t i = 0; i < compdb->count; i++) {
        CompDBEntry *e = &compdb->entries[i];
        char path[2048];
        if (e->file[0] == '/') {
            snprintf(path, sizeof(path), "%s", e->file);
        } else {
            snprintf(path, sizeof(path), "%s/%s", e->directory, e->file);
        }

        /* Build args: skip compiler name (args[0]), remove -c, -o and its arg */
        const char *parse_args[128];
        int parse_argc = 0;
        for (uint32_t j = 1; j < e->argc && parse_argc < 126; j++) {
            if (strcmp(e->args[j], "-c") == 0) continue;
            if (strcmp(e->args[j], "-o") == 0) { j++; continue; }
            /* Skip the source file itself */
            if (strcmp(e->args[j], e->file) == 0) continue;
            parse_args[parse_argc++] = e->args[j];
        }
        parse_args[parse_argc] = NULL;

        ParseResult r = parser_parse_file(g, path, parse_args);
        if (!r.success) {
            total.success = false;
            memcpy(total.error, r.error, sizeof(total.error));
        }
        total.functions_found += r.functions_found;
        total.calls_found += r.calls_found;
    }
    return total;
}
