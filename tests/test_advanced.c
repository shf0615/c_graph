#include "unity.h"
#include "graph/graph.h"
#include "query/perf.h"
#include "query/platform.h"
#include "query/lockorder.h"
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

void test_alloc_sites(void) {
    Graph g;
    graph_init(&g);
    uint32_t f = graph_add_node(&g, NODE_FUNCTION, "init", "i.c", 1, 10);
    uint32_t m = graph_add_node(&g, NODE_FUNCTION, "malloc", NULL, 0, 0);
    graph_add_edge(&g, f, m, EDGE_ALLOCATES, 0);
    graph_add_node(&g, NODE_FUNCTION, "other", "o.c", 1, 5);

    QueryResult r = query_alloc_sites(&g, NULL);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(f, r.node_ids[0]);
    free(r.node_ids);
    graph_destroy(&g);
}

void test_alloc_pairs_leak(void) {
    Graph g;
    graph_init(&g);
    uint32_t leaker = graph_add_node(&g, NODE_FUNCTION, "leaker", "l.c", 1, 5);
    uint32_t safe = graph_add_node(&g, NODE_FUNCTION, "safe", "s.c", 1, 5);
    uint32_t m = graph_add_node(&g, NODE_FUNCTION, "malloc", NULL, 0, 0);
    uint32_t fr = graph_add_node(&g, NODE_FUNCTION, "free", NULL, 0, 0);
    graph_add_edge(&g, leaker, m, EDGE_ALLOCATES, 0);
    graph_add_edge(&g, safe, m, EDGE_ALLOCATES, 0);
    graph_add_edge(&g, safe, fr, EDGE_FREES, 0);

    QueryResult r = query_alloc_pairs(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(leaker, r.node_ids[0]);
    free(r.node_ids);
    graph_destroy(&g);
}

void test_hotpath(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "main", "m.c", 1, 10);
    graph_add_node(&g, NODE_FUNCTION, "process", "p.c", 1, 20);
    graph_add_node(&g, NODE_FUNCTION, "malloc", NULL, 0, 0);
    graph_add_edge(&g, 0, 1, EDGE_CALLS, 0);
    graph_add_edge(&g, 1, 2, EDGE_ALLOCATES, 0);

    QueryResult r = query_hotpath(&g, 0, 3);
    TEST_ASSERT_TRUE(r.count >= 3);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_lock_order_cycle(void) {
    Graph g;
    graph_init(&g);
    uint32_t f1 = graph_add_node(&g, NODE_FUNCTION, "f1", "a.c", 1, 5);
    uint32_t f2 = graph_add_node(&g, NODE_FUNCTION, "f2", "b.c", 1, 5);
    uint32_t la = graph_add_node(&g, NODE_SYNC_PRIMITIVE, "lock_a", "l.c", 1, 1);
    uint32_t lb = graph_add_node(&g, NODE_SYNC_PRIMITIVE, "lock_b", "l.c", 2, 2);
    /* f1 acquires A then B; f2 acquires B then A */
    graph_add_edge(&g, f1, la, EDGE_ACQUIRES_LOCK, 0);
    graph_add_edge(&g, f1, lb, EDGE_ACQUIRES_LOCK, 0);
    graph_add_edge(&g, f2, lb, EDGE_ACQUIRES_LOCK, 0);
    graph_add_edge(&g, f2, la, EDGE_ACQUIRES_LOCK, 0);

    LockOrderResult r = query_lock_order(&g);
    TEST_ASSERT_TRUE(r.has_cycle);
    lock_order_result_free(&r);
    graph_destroy(&g);
}

void test_syscalls(void) {
    Graph g;
    graph_init(&g);
    uint32_t op = graph_add_node(&g, NODE_FUNCTION, "open", NULL, 0, 0);
    g.nodes[op].is_external = true;
    graph_add_node(&g, NODE_FUNCTION, "my_func", "m.c", 1, 5);

    QueryResult r = query_syscalls(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(op, r.node_ids[0]);
    free(r.node_ids);
    graph_destroy(&g);
}

void test_compiler_builtins(void) {
    Graph g;
    graph_init(&g);
    uint32_t b = graph_add_node(&g, NODE_FUNCTION, "__builtin_expect", NULL, 0, 0);
    graph_add_node(&g, NODE_FUNCTION, "normal", "n.c", 1, 5);

    QueryResult r = query_compiler_builtins(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(b, r.node_ids[0]);
    free(r.node_ids);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_alloc_sites);
    RUN_TEST(test_alloc_pairs_leak);
    RUN_TEST(test_hotpath);
    RUN_TEST(test_lock_order_cycle);
    RUN_TEST(test_syscalls);
    RUN_TEST(test_compiler_builtins);
    return UNITY_END();
}
