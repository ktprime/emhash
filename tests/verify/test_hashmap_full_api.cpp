// Full API coverage test for all emhash HashMap versions
// Tests: at, try_emplace, insert_or_assign, insert_unique, emplace_unique,
//        merge, erase_if, shrink_to_fit, try_get/try_set (emhash8 only)

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <type_traits>

static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; return; } \
} while(0)

// ============================================================================
// at() tests
// ============================================================================
template<typename Map>
void test_at() {
    Map map;
    map[1] = 10;
    map[2] = 20;
    map[3] = 30;

    TEST_ASSERT(map.at(1) == 10, "at(1) should be 10");
    TEST_ASSERT(map.at(2) == 20, "at(2) should be 20");
    TEST_ASSERT(map.at(3) == 30, "at(3) should be 30");

    const auto& cmap = map;
    TEST_ASSERT(cmap.at(1) == 10, "const at(1) should be 10");
    TEST_ASSERT(cmap.at(2) == 20, "const at(2) should be 20");

    map.at(1) = 100;
    TEST_ASSERT(map.at(1) == 100, "at() should return mutable reference");

    g_pass++;
}

// ============================================================================
// try_emplace() tests
// ============================================================================
template<typename Map>
void test_try_emplace() {
    Map map;

    auto [it1, ins1] = map.try_emplace(1, 10);
    TEST_ASSERT(ins1, "try_emplace new key should insert");
    TEST_ASSERT(it1->first == 1 && it1->second == 10, "try_emplace value correct");

    auto [it2, ins2] = map.try_emplace(1, 999);
    TEST_ASSERT(!ins2, "try_emplace existing key should not insert");
    TEST_ASSERT(it2->second == 10, "try_emplace existing key should not change value");

    auto [it3, ins3] = map.try_emplace(2, 20);
    TEST_ASSERT(ins3, "try_emplace second key");
    TEST_ASSERT(it3->second == 20, "try_emplace second value");

    g_pass++;
}

// ============================================================================
// insert_or_assign() tests
// ============================================================================
template<typename Map>
void test_insert_or_assign() {
    Map map;

    auto [it1, ins1] = map.insert_or_assign(1, 10);
    TEST_ASSERT(ins1, "insert_or_assign new key should insert");
    TEST_ASSERT(it1->second == 10, "insert_or_assign new value");

    auto [it2, ins2] = map.insert_or_assign(1, 100);
    TEST_ASSERT(!ins2, "insert_or_assign existing key should not insert");
    TEST_ASSERT(it2->second == 100, "insert_or_assign should update value");

    g_pass++;
}

// ============================================================================
// insert_unique() tests
// Note: insert_unique assumes key does NOT exist (caller's responsibility).
// It always inserts, even for duplicate keys. Returns bucket index.
// ============================================================================
template<typename Map>
void test_insert_unique() {
    Map map;

    auto n1 = map.insert_unique(1, 10);
    TEST_ASSERT(n1 >= 0, "insert_unique new key should return valid bucket");

    (void)map.insert_unique(2, 20);
    (void)map.insert_unique(3, 30);
    TEST_ASSERT(map.size() == 3, "insert_unique size correct");

    g_pass++;
}

// ============================================================================
// emplace_unique() tests
// Note: Same as insert_unique - assumes key does NOT exist.
// ============================================================================
template<typename Map>
void test_emplace_unique() {
    Map map;

    auto n1 = map.emplace_unique(1, 10);
    TEST_ASSERT(n1 >= 0, "emplace_unique new key");

    (void)map.emplace_unique(2, 20);
    TEST_ASSERT(map.size() == 2, "emplace_unique size");

    g_pass++;
}

// ============================================================================
// merge() tests
// ============================================================================
template<typename Map>
void test_merge() {
    Map map1;
    map1[1] = 10;
    map1[2] = 20;

    Map map2;
    map2[2] = 200;
    map2[3] = 300;

    map1.merge(map2);

    TEST_ASSERT(map1.size() == 3, "merge should add new keys");
    TEST_ASSERT(map1.at(1) == 10, "merge should keep existing key 1");
    TEST_ASSERT(map1.at(2) == 20, "merge should not overwrite existing key 2");
    TEST_ASSERT(map1.at(3) == 300, "merge should add key 3 from source");

    g_pass++;
}

// ============================================================================
// erase_if() tests
// ============================================================================
template<typename Map>
void test_erase_if() {
    Map map;
    for (int i = 0; i < 10; i++)
        map[i] = i * 10;

    auto erased = map.erase_if([](const auto& p) { return p.second >= 50; });
    TEST_ASSERT(erased == 5, "erase_if should erase 5 elements");
    TEST_ASSERT(map.size() == 5, "erase_if remaining size");
    for (int i = 0; i < 5; i++)
        TEST_ASSERT(map.contains(i), "erase_if should keep elements < 5");

    g_pass++;
}

// ============================================================================
// shrink_to_fit() tests
// ============================================================================
template<typename Map>
void test_shrink_to_fit() {
    Map map;
    for (int i = 0; i < 1000; i++)
        map[i] = i;

    auto bc_before = map.bucket_count();
    for (int i = 0; i < 900; i++)
        (void)map.erase(i);

    map.shrink_to_fit();
    auto bc_after = map.bucket_count();
    TEST_ASSERT(bc_after <= bc_before, "shrink_to_fit should not increase buckets");
    TEST_ASSERT(map.size() == 100, "shrink_to_fit should not change size");

    // Verify remaining elements still correct
    for (int i = 900; i < 1000; i++)
        TEST_ASSERT(map.at(i) == i, "shrink_to_fit should preserve elements");

    g_pass++;
}

// ============================================================================
// try_get / try_set tests (emhash8 only)
// ============================================================================
template<typename Map>
void test_try_get_set() {
    Map map;
    map[1] = 10;
    map[2] = 20;

    // try_get with output parameter
    typename Map::mapped_type val = 0;
    TEST_ASSERT(map.try_get(1, val), "try_get existing key");
    TEST_ASSERT(val == 10, "try_get value correct");

    TEST_ASSERT(!map.try_get(99, val), "try_get non-existing key");

    // try_get returning pointer
    auto* ptr = map.try_get(2);
    TEST_ASSERT(ptr != nullptr, "try_get pointer existing");
    TEST_ASSERT(*ptr == 20, "try_get pointer value");

    TEST_ASSERT(map.try_get(99) == nullptr, "try_get pointer non-existing");

    // try_set (only sets if key exists)
    TEST_ASSERT(map.try_set(1, 999), "try_set existing key should succeed");
    TEST_ASSERT(map.at(1) == 999, "try_set should update existing");
    TEST_ASSERT(!map.try_set(3, 30), "try_set new key should fail");

    g_pass++;
}

// ============================================================================
// Run all tests for a specific map type (common API)
// ============================================================================
template<typename Map>
void run_full_api_tests(const char* name) {
    printf("Testing %s:\n", name);
    test_at<Map>();
    test_try_emplace<Map>();
    test_insert_or_assign<Map>();
    test_insert_unique<Map>();
    test_emplace_unique<Map>();
    test_merge<Map>();
    test_erase_if<Map>();
    test_shrink_to_fit<Map>();
    printf("  %s done\n\n", name);
}

int main() {
    printf("=== Full API Coverage Tests ===\n\n");

    run_full_api_tests<emhash5::HashMap<int, int>>("emhash5::HashMap");
    run_full_api_tests<emhash6::HashMap<int, int>>("emhash6::HashMap");
    run_full_api_tests<emhash7::HashMap<int, int>>("emhash7::HashMap");
    run_full_api_tests<emhash8::HashMap<int, int>>("emhash8::HashMap");

    // emhash8-only: try_get/try_set
    printf("Testing emhash8 try_get/try_set:\n");
    test_try_get_set<emhash8::HashMap<int, int>>();
    printf("  done\n\n");

    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
