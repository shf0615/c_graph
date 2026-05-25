#include "unity.h"
#include "graph/graph.h"
#include "graph/metrics.h"
#include "query/traverse.h"
#include "query/impact.h"
#include "query/quality.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static Graph make_chain(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "d", "d.c", 1, 5);
    graph_add_edge(&g, 0, 1, EDGE_CALLS, 0);
    graph_add_edge(&g, 1, 2, EDGE_CALLS, 0);
    graph_add_edge(&g, 2, 3, EDGE_CALLS, 0);
    return g;
}

void test_callees_depth1(void) {
    Graph g = make_chain();
    QueryResult r = query_callees(&g, 0, 1);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_UINT32(1, r.node_ids[0]);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_callees_depth3(void) {
    Graph g = make_chain();
    QueryResult r = query_callees(&g, 0, 3);
    TEST_ASSERT_EQUAL_UINT32(3, r.count);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_callers_depth2(void) {
    Graph g = make_chain();
    QueryResult r = query_callers(&g, 3, 2);
    TEST_ASSERT_EQUAL_UINT32(2, r.count);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_path_found(void) {
    Graph g = make_chain();
    QueryResult r = query_path(&g, 0, 3);
    TEST_ASSERT_TRUE(r.count > 0);
    TEST_ASSERT_EQUAL_UINT32(0, r.node_ids[0]);
    TEST_ASSERT_EQUAL_UINT32(3, r.node_ids[r.count - 1]);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_path_not_found(void) {
    Graph g = make_chain();
    QueryResult r = query_path(&g, 3, 0);
    TEST_ASSERT_EQUAL_UINT32(0, r.count);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_impact(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "d", "d.c", 1, 5);
    graph_add_edge(&g, 0, 1, EDGE_CALLS, 0);
    graph_add_edge(&g, 1, 2, EDGE_CALLS, 0);
    graph_add_edge(&g, 3, 1, EDGE_CALLS, 0);

    QueryResult r = query_impact(&g, 2);
    TEST_ASSERT_EQUAL_UINT32(3, r.count);
    query_result_free(&r);
    graph_destroy(&g);
}

void test_fan_in_out(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "hub", "h.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_edge(&g, 1, 0, EDGE_CALLS, 0);
    graph_add_edge(&g, 2, 0, EDGE_CALLS, 0);
    graph_add_edge(&g, 0, 3, EDGE_CALLS, 0);

    metrics_compute_fan(&g);
    TEST_ASSERT_EQUAL_UINT32(2, g.nodes[0].fan_in);
    TEST_ASSERT_EQUAL_UINT32(1, g.nodes[0].fan_out);
    graph_destroy(&g);
}

void test_dead_code(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "used", "a.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "unused", "b.c", 1, 5);
    graph_add_node(&g, NODE_FUNCTION, "main", "m.c", 1, 10);
    graph_add_edge(&g, 2, 0, EDGE_CALLS, 0);
    metrics_compute_fan(&g);

    QueryResult r = query_dead_code(&g);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_STRING("unused", g.nodes[r.node_ids[0]].name);
    free(r.node_ids);
    graph_destroy(&g);
}

void test_large_functions(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "small", "a.c", 1, 10);
    graph_add_node(&g, NODE_FUNCTION, "big", "b.c", 1, 200);

    QueryResult r = query_large_functions(&g, 50);
    TEST_ASSERT_EQUAL_UINT32(1, r.count);
    TEST_ASSERT_EQUAL_STRING("big", g.nodes[r.node_ids[0]].name);
    free(r.node_ids);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_callees_depth1);
    RUN_TEST(test_callees_depth3);
    RUN_TEST(test_callers_depth2);
    RUN_TEST(test_path_found);
    RUN_TEST(test_path_not_found);
    RUN_TEST(test_impact);
    RUN_TEST(test_fan_in_out);
    RUN_TEST(test_dead_code);
    RUN_TEST(test_large_functions);
    return UNITY_END();
}
