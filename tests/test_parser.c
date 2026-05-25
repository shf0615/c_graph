#include "unity.h"
#include "graph/graph.h"
#include "parse/parser.h"

void setUp(void) {}
void tearDown(void) {}

void test_parser_extracts_functions(void) {
    Graph g;
    graph_init(&g);
    const char *args[] = {"-I" FIXTURE_PATH "/sample_project", NULL};
    ParseResult r = parser_parse_file(&g, FIXTURE_PATH "/sample_project/main.c", args);
    TEST_ASSERT_TRUE(r.success);
    uint32_t id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "main", &id));
    TEST_ASSERT_TRUE(graph_find_node(&g, "local_func", &id));
    graph_destroy(&g);
}

void test_parser_extracts_calls(void) {
    Graph g;
    graph_init(&g);
    const char *args[] = {"-I" FIXTURE_PATH "/sample_project", NULL};
    parser_parse_file(&g, FIXTURE_PATH "/sample_project/main.c", args);
    parser_parse_file(&g, FIXTURE_PATH "/sample_project/util.c", args);

    uint32_t main_id, add_id;
    TEST_ASSERT_TRUE(graph_find_node(&g, "main", &main_id));
    TEST_ASSERT_TRUE(graph_find_node(&g, "add", &add_id));

    EdgeList edges = graph_edges_from(&g, main_id);
    bool found_call_to_add = false;
    for (uint32_t i = 0; i < edges.count; i++) {
        if (edges.edges[i].to == add_id && edges.edges[i].type == EDGE_CALLS)
            found_call_to_add = true;
    }
    TEST_ASSERT_TRUE(found_call_to_add);
    free(edges.edges);
    graph_destroy(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_extracts_functions);
    RUN_TEST(test_parser_extracts_calls);
    return UNITY_END();
}
