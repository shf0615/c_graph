#include "unity.h"
#include "parse/compdb.h"

void setUp(void) {}
void tearDown(void) {}

void test_compdb_parse(void) {
    CompDB db;
    TEST_ASSERT_TRUE(compdb_parse(&db, FIXTURE_PATH "/sample_project/compile_commands.json"));
    TEST_ASSERT_EQUAL_UINT32(2, db.count);
    TEST_ASSERT_EQUAL_STRING("main.c", db.entries[0].file);
    TEST_ASSERT_EQUAL_STRING("util.c", db.entries[1].file);
    compdb_destroy(&db);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_compdb_parse);
    return UNITY_END();
}
