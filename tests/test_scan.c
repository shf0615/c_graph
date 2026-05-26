#include "unity.h"
#include "parse/scan.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_scan_finds_c_files(void) {
    ScanResult sr;
    TEST_ASSERT_TRUE(scan_directory(&sr, FIXTURE_PATH "/sample_project"));
    TEST_ASSERT_TRUE(sr.file_count >= 2); /* main.c, util.c at minimum */
    for (uint32_t i = 0; i < sr.file_count; i++) {
        const char *dot = strrchr(sr.files[i], '.');
        TEST_ASSERT_NOT_NULL(dot);
        TEST_ASSERT_EQUAL_STRING(".c", dot);
    }
    scan_result_destroy(&sr);
}

void test_scan_finds_include_dirs(void) {
    ScanResult sr;
    TEST_ASSERT_TRUE(scan_directory(&sr, FIXTURE_PATH "/sample_project"));
    TEST_ASSERT_TRUE(sr.include_count >= 1);
    scan_result_destroy(&sr);
}

void test_scan_invalid_dir(void) {
    ScanResult sr;
    TEST_ASSERT_FALSE(scan_directory(&sr, "/nonexistent/path"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scan_finds_c_files);
    RUN_TEST(test_scan_finds_include_dirs);
    RUN_TEST(test_scan_invalid_dir);
    return UNITY_END();
}
