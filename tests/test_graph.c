#include "unity.h"
#include "graph/graph.h"

void setUp(void) {}
void tearDown(void) {}

void test_graph_add_node(void) {
    Graph g;
    graph_init(&g);
    uint32_t id = graph_add_node(&g, NODE_FUNCTION, "main", "src/main.c", 10, 50);
    TEST_ASSERT_EQUAL_UINT32(0, id);
    Node *n = graph_get_node(&g, id);
    TEST_ASSERT_EQUAL_STRING("main", n->name);
    TEST_ASSERT_EQUAL(NODE_FUNCTION, n->type);
    graph_destroy(&g);
}

void test_graph_add_edge(void) {
    Graph g;
    graph_init(&g);
    uint32_t a = graph_add_node(&g, NODE_FUNCTION, "caller", "a.c", 1, 10);
    uint32_t b = graph_add_node(&g, NODE_FUNCTION, "callee", "b.c", 1, 5);
    graph_add_edge(&g, a, b, EDGE_CALLS, 0);
    EdgeList edges = graph_edges_from(&g, a);
    TEST_ASSERT_EQUAL_UINT32(1, edges.count);
    TEST_ASSERT_EQUAL_UINT32(b, edges.edges[0].to);
    TEST_ASSERT_EQUAL(EDGE_CALLS, edges.edges[0].type);
    free(edges.edges);
    graph_destroy(&g);
}

void test_graph_lookup_by_name(void) {
    Graph g;
    graph_init(&g);
    graph_add_node(&g, NODE_FUNCTION, "foo", "x.c", 1, 10);
    graph_add_node(&g, NODE_FUNCTION, "bar", "x.c", 11, 20);
    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "bar", &id));
    TEST_ASSERT_EQUAL_UINT32(1, id);
    TEST_ASSERT_FALSE(graph_find_node(&g, "baz", &id));
    graph_destroy(&g);
}

void test_graph_edges_to(void) {
    Graph g;
    graph_init(&g);
    uint32_t a = graph_add_node(&g, NODE_FUNCTION, "a", "a.c", 1, 5);
    uint32_t b = graph_add_node(&g, NODE_FUNCTION, "b", "b.c", 1, 5);
    uint32_t c = graph_add_node(&g, NODE_FUNCTION, "c", "c.c", 1, 5);
    graph_add_edge(&g, a, c, EDGE_CALLS, 0);
    graph_add_edge(&g, b, c, EDGE_CALLS, 0);
    EdgeList edges = graph_edges_to(&g, c);
    TEST_ASSERT_EQUAL_UINT32(2, edges.count);
    free(edges.edges);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_graph_add_node);
    RUN_TEST(test_graph_add_edge);
    RUN_TEST(test_graph_lookup_by_name);
    RUN_TEST(test_graph_edges_to);
    return UNITY_END();
}
