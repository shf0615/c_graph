#include "unity.h"
#include "graph/graph.h"
#include "query/concurrency.h"
#include <stdlib.h>

void setUp(void) {}
void tearDown(void) {}

void test_find_thread_entries(void) {
    Graph g;
    graph_init(&g);
    uint32_t t = graph_add_node(&g, NODE_FUNCTION, "worker", "w.c", 1, 10);
    g.nodes[t].is_thread_entry = true;
    graph_add_node(&g, NODE_FUNCTION, "main", "m.c", 1, 10);

    QueryResult r = query_threads(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(t, r.node_ids[0]);
    free(r.node_ids);
    graph_destroy(&g);
}

void test_shared_resources(void) {
    Graph g;
    graph_init(&g);
    uint32_t gvar = graph_add_node(&g, NODE_GLOBAL_VAR, "shared_buf", "g.c", 1, 1);
    g.nodes[gvar].is_shared = true;
    graph_add_node(&g, NODE_GLOBAL_VAR, "local_buf", "g.c", 2, 2);

    QueryResult r = query_shared_resources(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(gvar, r.node_ids[0]);
    free(r.node_ids);
    graph_destroy(&g);
}

void test_data_race_suspect(void) {
    Graph g;
    graph_init(&g);
    uint32_t gvar = graph_add_node(&g, NODE_GLOBAL_VAR, "shared", "g.c", 1, 1);
    g.nodes[gvar].is_shared = true;
    uint32_t func = graph_add_node(&g, NODE_FUNCTION, "writer", "w.c", 1, 5);
    graph_add_edge(&g, func, gvar, EDGE_WRITES_GLOBAL, 0);

    QueryResult r = query_data_race_suspects(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(func, r.node_ids[0]);
    free(r.node_ids);
    graph_destroy(&g);
}

void test_no_race_when_locked(void) {
    Graph g;
    graph_init(&g);
    uint32_t gvar = graph_add_node(&g, NODE_GLOBAL_VAR, "shared", "g.c", 1, 1);
    g.nodes[gvar].is_shared = true;
    uint32_t lock = graph_add_node(&g, NODE_SYNC_PRIMITIVE, "mutex", "g.c", 2, 2);
    uint32_t func = graph_add_node(&g, NODE_FUNCTION, "safe_writer", "w.c", 1, 5);
    graph_add_edge(&g, func, lock, EDGE_ACQUIRES_LOCK, 0);
    graph_add_edge(&g, func, gvar, EDGE_WRITES_GLOBAL, 0);

    QueryResult r = query_data_race_suspects(&g);
    TEST_ASSERT_EQUAL_UINT32(0, r.count);
    free(r.node_ids);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_find_thread_entries);
    RUN_TEST(test_shared_resources);
    RUN_TEST(test_data_race_suspect);
    RUN_TEST(test_no_race_when_locked);
    return UNITY_END();
}
