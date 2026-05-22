#include "unity.h"
#include "util/hash.h"
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

void test_hashmap_put_get(void) {
    HashMap m;
    hashmap_init(&m, 16);
    hashmap_put(&m, "foo", 42);
    uint32_t val;
    TEST_ASSERT_TRUE(hashmap_get(&m, "foo", &val));
    TEST_ASSERT_EQUAL_UINT32(42, val);
    hashmap_destroy(&m);
}

void test_hashmap_missing_key(void) {
    HashMap m;
    hashmap_init(&m, 16);
    uint32_t val;
    TEST_ASSERT_FALSE(hashmap_get(&m, "missing", &val));
    hashmap_destroy(&m);
}

void test_hashmap_overwrite(void) {
    HashMap m;
    hashmap_init(&m, 16);
    hashmap_put(&m, "key", 1);
    hashmap_put(&m, "key", 2);
    uint32_t val;
    hashmap_get(&m, "key", &val);
    TEST_ASSERT_EQUAL_UINT32(2, val);
    hashmap_destroy(&m);
}

void test_hashmap_grow(void) {
    HashMap m;
    hashmap_init(&m, 4);
    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        hashmap_put(&m, key, i);
    }
    uint32_t val;
    TEST_ASSERT_TRUE(hashmap_get(&m, "key99", &val));
    TEST_ASSERT_EQUAL_UINT32(99, val);
    hashmap_destroy(&m);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hashmap_put_get);
    RUN_TEST(test_hashmap_missing_key);
    RUN_TEST(test_hashmap_overwrite);
    RUN_TEST(test_hashmap_grow);
    return UNITY_END();
}
