#include "unity.h"
#include "util/arena.h"

void setUp(void) {}
void tearDown(void) {}

void test_arena_basic_alloc(void) {
    Arena a;
    arena_init(&a, 1024);
    void *p1 = arena_alloc(&a, 100);
    void *p2 = arena_alloc(&a, 200);
    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);
    TEST_ASSERT(p2 > p1);
    arena_destroy(&a);
}

void test_arena_strdup(void) {
    Arena a;
    arena_init(&a, 1024);
    char *s = arena_strdup(&a, "hello");
    TEST_ASSERT_EQUAL_STRING("hello", s);
    arena_destroy(&a);
}

void test_arena_large_alloc(void) {
    Arena a;
    arena_init(&a, 64);
    void *p = arena_alloc(&a, 128);
    TEST_ASSERT_NOT_NULL(p);
    arena_destroy(&a);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_arena_basic_alloc);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_large_alloc);
    return UNITY_END();
}
