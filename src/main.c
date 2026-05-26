#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph/graph.h"
#include "graph/serialize.h"
#include "graph/metrics.h"
#include "parse/compdb.h"
#include "parse/parser.h"
#include "parse/scan.h"
#include "query/traverse.h"
#include "query/impact.h"
#include "query/quality.h"
#include "query/concurrency.h"
#include "query/perf.h"
#include "query/platform.h"
#include "query/lockorder.h"
#include "export/json.h"
#include "export/html.h"

typedef struct {
    const char *db_path;
    const char *project_root;
    const char *module;
    int depth;
    int top_n;
    int min_lines;
} CliOptions;

static void parse_opts(int argc, char **argv, CliOptions *opts) {
    opts->db_path = "cgraph.db";
    opts->project_root = NULL;
    opts->module = NULL;
    opts->depth = 3;
    opts->top_n = 10;
    opts->min_lines = 100;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) opts->db_path = argv[++i];
        else if (strcmp(argv[i], "--project-root") == 0 && i + 1 < argc) opts->project_root = argv[++i];
        else if (strcmp(argv[i], "--module") == 0 && i + 1 < argc) opts->module = argv[++i];
        else if (strcmp(argv[i], "--depth") == 0 && i + 1 < argc) opts->depth = atoi(argv[++i]);
        else if (strcmp(argv[i], "--top") == 0 && i + 1 < argc) opts->top_n = atoi(argv[++i]);
        else if (strcmp(argv[i], "--min-lines") == 0 && i + 1 < argc) opts->min_lines = atoi(argv[++i]);
    }
}

/* Find first positional arg (not starting with --) */
static const char *find_arg(int argc, char **argv, int skip) {
    int found = 0;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0) { i++; continue; }
        if (found == skip) return argv[i];
        found++;
    }
    return NULL;
}

static bool load_graph(Graph *g, const char *path) {
    if (!graph_deserialize(g, path)) {
        fprintf(stderr, "Error: cannot load %s\n", path);
        return false;
    }
    return true;
}

static int cmd_build(int argc, char **argv) {
    const char *compdb_path = NULL;
    const char *src_dir = NULL;
    const char *output = "cgraph.db";
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--compile-commands") == 0 && i + 1 < argc) compdb_path = argv[++i];
        else if (strcmp(argv[i], "--src-dir") == 0 && i + 1 < argc) src_dir = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output = argv[++i];
    }

    if (!compdb_path && !src_dir) {
        fprintf(stderr, "Error: provide --compile-commands or --src-dir\n");
        return 1;
    }

    Graph g;
    graph_init(&g);

    if (compdb_path) {
        CompDB compdb;
        if (!compdb_parse(&compdb, compdb_path)) {
            fprintf(stderr, "Error: cannot parse %s\n", compdb_path);
            graph_destroy(&g);
            return 1;
        }
        fprintf(stderr, "Parsing %u translation units...\n", compdb.count);
        ParseResult r = parser_parse_project(&g, &compdb);
        if (!r.success) fprintf(stderr, "Warning: %s\n", r.error);
        fprintf(stderr, "Found %u functions, %u call edges\n", r.functions_found, r.calls_found);
        compdb_destroy(&compdb);
    } else {
        ScanResult sr;
        if (!scan_directory(&sr, src_dir)) {
            fprintf(stderr, "Error: cannot scan directory %s\n", src_dir);
            graph_destroy(&g);
            return 1;
        }
        fprintf(stderr, "Scanning %u .c files from %s\n", sr.file_count, src_dir);

        uint32_t arg_count = sr.include_count * 2;
        const char **parse_args = malloc((arg_count + 1) * sizeof(const char *));
        uint32_t idx = 0;
        for (uint32_t i = 0; i < sr.include_count; i++) {
            parse_args[idx++] = "-I";
            parse_args[idx++] = sr.includes[i];
        }
        parse_args[idx] = NULL;

        uint32_t total_funcs = 0, total_calls = 0;
        for (uint32_t i = 0; i < sr.file_count; i++) {
            ParseResult r = parser_parse_file(&g, sr.files[i], parse_args);
            if (!r.success) {
                fprintf(stderr, "Warning: %s: %s\n", sr.files[i], r.error);
            }
            total_funcs += r.functions_found;
            total_calls += r.calls_found;
        }
        fprintf(stderr, "Found %u functions, %u call edges\n", total_funcs, total_calls);
        free(parse_args);
        scan_result_destroy(&sr);
    }

    metrics_compute_fan(&g);
    fprintf(stderr, "Graph: %u nodes, %u edges\n", g.node_count, g.edge_count);

    if (!graph_serialize(&g, output)) {
        fprintf(stderr, "Error: cannot write %s\n", output);
        graph_destroy(&g);
        return 1;
    }
    fprintf(stderr, "Written: %s\n", output);
    graph_destroy(&g);
    return 0;
}

static int cmd_info(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    char *json = json_format_info(&g);
    puts(json);
    free(json);
    graph_destroy(&g);
    return 0;
}

static int cmd_list(int argc, char **argv, CliOptions *opts) {
    const char *type_str = find_arg(argc, argv, 0);
    if (!type_str) { fprintf(stderr, "Usage: cgraph list <functions|files|structs|globals>\n"); return 1; }

    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;

    NodeType type;
    if (strcmp(type_str, "functions") == 0) type = NODE_FUNCTION;
    else if (strcmp(type_str, "files") == 0) type = NODE_FILE;
    else if (strcmp(type_str, "structs") == 0) type = NODE_STRUCT;
    else if (strcmp(type_str, "globals") == 0) type = NODE_GLOBAL_VAR;
    else { fprintf(stderr, "Unknown type: %s\n", type_str); graph_destroy(&g); return 1; }

    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_node_list(&g, type, &jo);
    puts(json);
    free(json);
    graph_destroy(&g);
    return 0;
}

static int cmd_show(int argc, char **argv, CliOptions *opts) {
    const char *name = find_arg(argc, argv, 0);
    if (!name) { fprintf(stderr, "Usage: cgraph show <symbol>\n"); return 1; }

    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    uint32_t id;
    if (!graph_find_node(&g, name, &id)) {
        fprintf(stderr, "Symbol not found: %s\n", name);
        graph_destroy(&g);
        return 1;
    }
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_node(&g, id, &jo);
    puts(json);
    free(json);
    graph_destroy(&g);
    return 0;
}

static int cmd_callers(int argc, char **argv, CliOptions *opts) {
    const char *name = find_arg(argc, argv, 0);
    if (!name) { fprintf(stderr, "Usage: cgraph callers <func>\n"); return 1; }

    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    uint32_t id;
    if (!graph_find_node(&g, name, &id)) {
        fprintf(stderr, "Symbol not found: %s\n", name);
        graph_destroy(&g); return 1;
    }
    QueryResult r = query_callers(&g, id, opts->depth);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    query_result_free(&r);
    graph_destroy(&g);
    return 0;
}

static int cmd_callees(int argc, char **argv, CliOptions *opts) {
    const char *name = find_arg(argc, argv, 0);
    if (!name) { fprintf(stderr, "Usage: cgraph callees <func>\n"); return 1; }

    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    uint32_t id;
    if (!graph_find_node(&g, name, &id)) {
        fprintf(stderr, "Symbol not found: %s\n", name);
        graph_destroy(&g); return 1;
    }
    QueryResult r = query_callees(&g, id, opts->depth);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    query_result_free(&r);
    graph_destroy(&g);
    return 0;
}

static int cmd_path(int argc, char **argv, CliOptions *opts) {
    const char *from = find_arg(argc, argv, 0);
    const char *to = find_arg(argc, argv, 1);
    if (!from || !to) { fprintf(stderr, "Usage: cgraph path <func_a> <func_b>\n"); return 1; }

    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    uint32_t from_id, to_id;
    if (!graph_find_node(&g, from, &from_id) || !graph_find_node(&g, to, &to_id)) {
        fprintf(stderr, "Symbol not found\n");
        graph_destroy(&g); return 1;
    }
    QueryResult r = query_path(&g, from_id, to_id);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    query_result_free(&r);
    graph_destroy(&g);
    return 0;
}

static int cmd_impact(int argc, char **argv, CliOptions *opts) {
    const char *name = find_arg(argc, argv, 0);
    if (!name) { fprintf(stderr, "Usage: cgraph impact <symbol>\n"); return 1; }

    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    uint32_t id;
    if (!graph_find_node(&g, name, &id)) {
        fprintf(stderr, "Symbol not found: %s\n", name);
        graph_destroy(&g); return 1;
    }
    QueryResult r = query_impact(&g, id);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    query_result_free(&r);
    graph_destroy(&g);
    return 0;
}

static int cmd_complexity(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    QueryResult r = query_complexity_top(&g, opts->top_n);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    free(r.node_ids);
    graph_destroy(&g);
    return 0;
}

static int cmd_dead_code(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    metrics_compute_fan(&g);
    QueryResult r = query_dead_code(&g);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    free(r.node_ids);
    graph_destroy(&g);
    return 0;
}

static int cmd_coupling(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    QueryResult r = query_coupling_top(&g, opts->top_n);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    free(r.node_ids);
    graph_destroy(&g);
    return 0;
}

static int cmd_large_functions(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    QueryResult r = query_large_functions(&g, opts->min_lines);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    free(r.node_ids);
    graph_destroy(&g);
    return 0;
}

static int cmd_threads(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    QueryResult r = query_threads(&g);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    free(r.node_ids);
    graph_destroy(&g);
    return 0;
}

static int cmd_shared_resources(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    QueryResult r = query_shared_resources(&g);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    free(r.node_ids);
    graph_destroy(&g);
    return 0;
}

static int cmd_data_race_suspects(int argc, char **argv, CliOptions *opts) {
    Graph g;
    if (!load_graph(&g, opts->db_path)) return 1;
    QueryResult r = query_data_race_suspects(&g);
    JsonOptions jo = {.project_root = opts->project_root};
    char *json = json_format_query_result(&g, &r, &jo);
    puts(json);
    free(json);
    free(r.node_ids);
    graph_destroy(&g);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: cgraph <command> [options]\n");
        fprintf(stderr, "Commands: build, info, list, show, callers, callees, path,\n");
        fprintf(stderr, "          impact, complexity, coupling, dead-code, large-functions\n");
        return 1;
    }

    const char *cmd = argv[1];
    int sub_argc = argc - 2;
    char **sub_argv = argv + 2;

    CliOptions opts;
    parse_opts(sub_argc, sub_argv, &opts);

    if (strcmp(cmd, "build") == 0) return cmd_build(sub_argc, sub_argv);
    if (strcmp(cmd, "info") == 0) return cmd_info(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "list") == 0) return cmd_list(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "show") == 0) return cmd_show(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "callers") == 0) return cmd_callers(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "callees") == 0) return cmd_callees(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "path") == 0) return cmd_path(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "impact") == 0) return cmd_impact(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "complexity") == 0) return cmd_complexity(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "coupling") == 0) return cmd_coupling(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "dead-code") == 0) return cmd_dead_code(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "large-functions") == 0) return cmd_large_functions(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "threads") == 0) return cmd_threads(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "shared-resources") == 0) return cmd_shared_resources(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "data-race-suspects") == 0) return cmd_data_race_suspects(sub_argc, sub_argv, &opts);
    if (strcmp(cmd, "lock-order") == 0) {
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        LockOrderResult r = query_lock_order(&g);
        printf("{\"has_cycle\": %s, \"pairs\": [\n", r.has_cycle ? "true" : "false");
        for (uint32_t i = 0; i < r.count; i++) {
            if (i > 0) printf(",\n");
            printf("  {\"lock_a\": \"%s\", \"lock_b\": \"%s\", \"function\": \"%s\"}",
                   g.nodes[r.pairs[i].lock_a].name,
                   g.nodes[r.pairs[i].lock_b].name,
                   g.nodes[r.pairs[i].func_id].name);
        }
        printf("\n]}\n");
        lock_order_result_free(&r);
        graph_destroy(&g);
        return 0;
    }
    if (strcmp(cmd, "alloc-sites") == 0) {
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        const char *func = find_arg(sub_argc, sub_argv, 0);
        QueryResult r = query_alloc_sites(&g, func);
        JsonOptions jo = {.project_root = opts.project_root};
        char *json = json_format_query_result(&g, &r, &jo);
        puts(json); free(json); free(r.node_ids);
        graph_destroy(&g); return 0;
    }
    if (strcmp(cmd, "alloc-pairs") == 0) {
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        QueryResult r = query_alloc_pairs(&g);
        JsonOptions jo = {.project_root = opts.project_root};
        char *json = json_format_query_result(&g, &r, &jo);
        puts(json); free(json); free(r.node_ids);
        graph_destroy(&g); return 0;
    }
    if (strcmp(cmd, "hotpath") == 0) {
        const char *name = find_arg(sub_argc, sub_argv, 0);
        if (!name) { fprintf(stderr, "Usage: cgraph hotpath <entry>\n"); return 1; }
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        uint32_t id;
        if (!graph_find_node(&g, name, &id)) { fprintf(stderr, "Symbol not found: %s\n", name); graph_destroy(&g); return 1; }
        QueryResult r = query_hotpath(&g, id, opts.depth);
        JsonOptions jo = {.project_root = opts.project_root};
        char *json = json_format_query_result(&g, &r, &jo);
        puts(json); free(json); query_result_free(&r);
        graph_destroy(&g); return 0;
    }
    if (strcmp(cmd, "platform-deps") == 0) {
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        const char *macro = NULL;
        for (int i = 0; i < sub_argc; i++) {
            if (strcmp(sub_argv[i], "--ifdef") == 0 && i + 1 < sub_argc) macro = sub_argv[++i];
        }
        QueryResult r = query_platform_deps(&g, macro);
        JsonOptions jo = {.project_root = opts.project_root};
        char *json = json_format_query_result(&g, &r, &jo);
        puts(json); free(json); free(r.node_ids);
        graph_destroy(&g); return 0;
    }
    if (strcmp(cmd, "syscalls") == 0) {
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        QueryResult r = query_syscalls(&g);
        JsonOptions jo = {.project_root = opts.project_root};
        char *json = json_format_query_result(&g, &r, &jo);
        puts(json); free(json); free(r.node_ids);
        graph_destroy(&g); return 0;
    }
    if (strcmp(cmd, "compiler-builtins") == 0) {
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        QueryResult r = query_compiler_builtins(&g);
        JsonOptions jo = {.project_root = opts.project_root};
        char *json = json_format_query_result(&g, &r, &jo);
        puts(json); free(json); free(r.node_ids);
        graph_destroy(&g); return 0;
    }
    if (strcmp(cmd, "export-html") == 0) {
        const char *outdir = "./cgraph-html";
        for (int i = 0; i < sub_argc; i++) {
            if (strcmp(sub_argv[i], "--output") == 0 && i + 1 < sub_argc) outdir = sub_argv[++i];
        }
        Graph g;
        if (!load_graph(&g, opts.db_path)) return 1;
        if (!html_export(&g, outdir)) { fprintf(stderr, "Error exporting HTML\n"); graph_destroy(&g); return 1; }
        fprintf(stderr, "Exported to %s/\n", outdir);
        graph_destroy(&g);
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    return 1;
}
