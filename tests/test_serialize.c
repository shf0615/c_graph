#include "unity.h"
#include "graph/graph.h"
#include "graph/serialize.h"
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

void test_serialize_roundtrip(void) {
    Graph g;
    graph_init(&g);
    uint32_t a = graph_add_node(&g, NODE_FUNCTION, "main", "main.c", 1, 20);
    uint32_t b = graph_add_node(&g, NODE_FUNCTION, "helper", "util.c", 5, 15);
    g.nodes[a].cyclomatic_complexity = 7;
    g.nodes[b].is_external = true;
    graph_add_edge(&g, a, b, EDGE_CALLS, 0);

    const char *path = "/tmp/test_cgraph.db";
    TEST_ASSERT_TRUE(graph_serialize(&g, path));
    graph_destroy(&g);

    Graph g2;
    TEST_ASSERT_TRUE(graph_deserialize(&g2, path));
    TEST_ASSERT_EQUAL_UINT32(2, g2.node_count);
    TEST_ASSERT_EQUAL_UINT32(1, g2.edge_count);
    TEST_ASSERT_EQUAL_STRING("main", g2.nodes[0].name);
    TEST_ASSERT_EQUAL_STRING("helper", g2.nodes[1].name);
    TEST_ASSERT_EQUAL_UINT32(7, g2.nodes[0].cyclomatic_complexity);
    TEST_ASSERT_TRUE(g2.nodes[1].is_external);
    TEST_ASSERT_EQUAL_UINT32(a, g2.edges[0].from);
    TEST_ASSERT_EQUAL_UINT32(b, g2.edges[0].to);

    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g2, "helper", &id));
    TEST_ASSERT_EQUAL_UINT32(1, id);

    graph_destroy(&g2);
    remove(path);
}

void test_serialize_empty_graph(void) {
    Graph g;
    graph_init(&g);
    const char *path = "/tmp/test_cgraph_empty.db";
    TEST_ASSERT_TRUE(graph_serialize(&g, path));
    graph_destroy(&g);

    Graph g2;
    TEST_ASSERT_TRUE(graph_deserialize(&g2, path));
    TEST_ASSERT_EQUAL_UINT32(0, g2.node_count);
    TEST_ASSERT_EQUAL_UINT32(0, g2.edge_count);
    graph_destroy(&g2);
    remove(path);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_roundtrip);
    RUN_TEST(test_serialize_empty_graph);
    return UNITY_END();
}
