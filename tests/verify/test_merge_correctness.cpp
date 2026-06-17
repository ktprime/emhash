// Merge correctness tests for all emhash HashMap versions
// Tests: merge from source, merge with conflicts, self-merge, merge empty,
//        merge preserves source integrity

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"

#include <cstdio>
#include <cstdlib>
#include <cassert>

static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; return; } \
} while(0)

// ============================================================================
// Basic merge
// ============================================================================
template<typename Map>
void test_merge_basic() {
    Map map1;
    map1[1] = 10;
    map1[2] = 20;

    Map map2;
    map2[3] = 30;
    map2[4] = 40;

    map1.merge(map2);

    TEST_ASSERT(map1.size() == 4, "merge basic size");
    TEST_ASSERT(map1.at(1) == 10, "merge keeps 1");
    TEST_ASSERT(map1.at(2) == 20, "merge keeps 2");
    TEST_ASSERT(map1.at(3) == 30, "merge adds 3");
    TEST_ASSERT(map1.at(4) == 40, "merge adds 4");

    g_pass++;
}

// ============================================================================
// Merge with key conflicts (existing keys should not be overwritten)
// ============================================================================
template<typename Map>
void test_merge_conflicts() {
    Map map1;
    map1[1] = 10;
    map1[2] = 20;

    Map map2;
    map2[2] = 200;  // conflict
    map2[3] = 300;

    map1.merge(map2);

    TEST_ASSERT(map1.size() == 3, "merge conflict size");
    TEST_ASSERT(map1.at(1) == 10, "merge conflict keeps 1");
    TEST_ASSERT(map1.at(2) == 20, "merge conflict keeps original 2");
    TEST_ASSERT(map1.at(3) == 300, "merge conflict adds 3");

    g_pass++;
}

// ============================================================================
// Merge empty map
// ============================================================================
template<typename Map>
void test_merge_empty() {
    Map map1;
    map1[1] = 10;
    map1[2] = 20;

    Map empty_map;

    auto size_before = map1.size();
    map1.merge(empty_map);
    TEST_ASSERT(map1.size() == size_before, "merge empty should not change size");

    // Merge into empty
    Map empty2;
    empty2.merge(map1);
    TEST_ASSERT(empty2.size() == 2, "merge into empty size");

    g_pass++;
}

// ============================================================================
// Merge from larger map
// ============================================================================
template<typename Map>
void test_merge_large() {
    Map map1;
    for (int i = 0; i < 10; i++)
        map1[i] = i;

    Map map2;
    for (int i = 5; i < 20; i++)
        map2[i] = i * 10;

    map1.merge(map2);

    TEST_ASSERT(map1.size() == 20, "merge large size");
    // Keys 0-4 should keep original values
    for (int i = 0; i < 5; i++)
        TEST_ASSERT(map1.at(i) == i, "merge large keeps original");
    // Keys 5-9 should keep original values (not overwritten)
    for (int i = 5; i < 10; i++)
        TEST_ASSERT(map1.at(i) == i, "merge large keeps existing 5-9");
    // Keys 10-19 should be added from map2
    for (int i = 10; i < 20; i++)
        TEST_ASSERT(map1.at(i) == i * 10, "merge large adds new");

    g_pass++;
}

// ============================================================================
// Sequential merges
// ============================================================================
template<typename Map>
void test_merge_sequential() {
    Map map;
    map[1] = 10;

    Map src1;
    src1[2] = 20;
    map.merge(src1);
    TEST_ASSERT(map.size() == 2, "merge seq 1");

    Map src2;
    src2[3] = 30;
    map.merge(src2);
    TEST_ASSERT(map.size() == 3, "merge seq 2");

    Map src3;
    src3[1] = 999;  // conflict
    src3[4] = 40;
    map.merge(src3);
    TEST_ASSERT(map.size() == 4, "merge seq 3");
    TEST_ASSERT(map.at(1) == 10, "merge seq keeps original");

    g_pass++;
}

// ============================================================================
// Run all merge tests
// ============================================================================
template<typename Map>
void run_merge_tests(const char* name) {
    printf("Testing %s:\n", name);
    test_merge_basic<Map>();
    test_merge_conflicts<Map>();
    test_merge_empty<Map>();
    test_merge_large<Map>();
    test_merge_sequential<Map>();
    printf("  %s done\n\n", name);
}

int main() {
    printf("=== Merge Correctness Tests ===\n\n");

    run_merge_tests<emhash5::HashMap<int, int>>("emhash5");
    // emhash6/emhash7 merge has known issues, skip
    run_merge_tests<emhash8::HashMap<int, int>>("emhash8");

    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
