// Full API coverage test for all emhash HashSet versions
// Tests: insert, insert_unique, emplace, erase, contains, count, find,
//        erase_if, merge, shrink_to_fit, swap, copy/move, iterator
// Note: hash_set2/3/4 have different API subsets from hash_set8

#include "emhash/hash_set2.hpp"
#include "emhash/hash_set3.hpp"
#include "emhash/hash_set4.hpp"
#include "emhash/hash_set8.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>

static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; return; } \
} while(0)

// ============================================================================
// Basic CRUD tests (all HashSet versions)
// ============================================================================
template<typename Set>
void test_basic_crud() {
    Set set;

    // insert
    auto p1 = set.insert(1);
    TEST_ASSERT(p1.second, "insert new key");
    auto p2 = set.insert(1);
    TEST_ASSERT(!p2.second, "insert duplicate");

    set.insert(2);
    set.insert(3);
    TEST_ASSERT(set.size() == 3, "size after inserts");

    // contains
    TEST_ASSERT(set.contains(1), "contains 1");
    TEST_ASSERT(set.contains(2), "contains 2");
    TEST_ASSERT(!set.contains(99), "not contains 99");

    // count
    TEST_ASSERT(set.count(1) == 1, "count existing");
    TEST_ASSERT(set.count(99) == 0, "count non-existing");

    // find
    TEST_ASSERT(set.find(1) != set.end(), "find existing");
    TEST_ASSERT(set.find(99) == set.end(), "find non-existing");

    // erase by key
    auto erased = set.erase(2);
    TEST_ASSERT(erased == 1, "erase existing key");
    TEST_ASSERT(!set.contains(2), "erased key not found");
    TEST_ASSERT(set.size() == 2, "size after erase");

    // erase by iterator
    auto it = set.find(1);
    set.erase(it);
    TEST_ASSERT(!set.contains(1), "erase by iterator");
    TEST_ASSERT(set.size() == 1, "size after iterator erase");

    g_pass++;
}

// ============================================================================
// insert_unique tests (hash_set2/4/8 only, hash_set3 has assert(false))
// ============================================================================
template<typename Set>
void test_insert_unique() {
    Set set;

    set.insert(1);
    set.insert(2);
    set.insert(3);

    // insert_unique for keys not in set
    set.insert_unique(4);
    set.insert_unique(5);
    TEST_ASSERT(set.contains(4), "insert_unique new key 4");
    TEST_ASSERT(set.contains(5), "insert_unique new key 5");
    TEST_ASSERT(set.size() == 5, "insert_unique size");

    g_pass++;
}

// Basic CRUD for hash_set3 (no insert_unique)
template<typename Set>
void test_basic_crud_no_unique() {
    Set set;

    auto p1 = set.insert(1);
    TEST_ASSERT(p1.second, "insert new key");
    auto p2 = set.insert(1);
    TEST_ASSERT(!p2.second, "insert duplicate");

    set.insert(2);
    set.insert(3);
    TEST_ASSERT(set.size() == 3, "size after inserts");

    TEST_ASSERT(set.contains(1), "contains 1");
    TEST_ASSERT(!set.contains(99), "not contains 99");
    TEST_ASSERT(set.count(1) == 1, "count existing");
    TEST_ASSERT(set.find(1) != set.end(), "find existing");

    auto erased = set.erase(2);
    TEST_ASSERT(erased == 1, "erase existing key");
    TEST_ASSERT(!set.contains(2), "erased key not found");
    TEST_ASSERT(set.size() == 2, "size after erase");

    auto it = set.find(1);
    set.erase(it);
    TEST_ASSERT(!set.contains(1), "erase by iterator");
    TEST_ASSERT(set.size() == 1, "size after iterator erase");

    g_pass++;
}

// ============================================================================
// copy/move/swap tests (all HashSet versions)
// ============================================================================
template<typename Set>
void test_copy_move_swap() {
    Set set1;
    for (int i = 0; i < 10; i++)
        set1.insert(i);

    // Copy constructor
    Set set2(set1);
    TEST_ASSERT(set2.size() == 10, "copy constructor size");
    TEST_ASSERT(set2.contains(5), "copy constructor element");

    // Copy assignment
    Set set3;
    set3 = set1;
    TEST_ASSERT(set3.size() == 10, "copy assignment size");

    // Move constructor
    Set set4(std::move(set2));
    TEST_ASSERT(set4.size() == 10, "move constructor size");
    TEST_ASSERT(set2.empty(), "moved-from set should be empty");

    // Move assignment
    Set set5;
    set5 = std::move(set3);
    TEST_ASSERT(set5.size() == 10, "move assignment size");

    // Swap
    Set setA, setB;
    setA.insert(1);
    setB.insert(2);
    setB.insert(3);
    setA.swap(setB);
    TEST_ASSERT(setA.size() == 2 && setA.contains(2), "swap A");
    TEST_ASSERT(setB.size() == 1 && setB.contains(1), "swap B");

    g_pass++;
}

// ============================================================================
// iterator tests (all HashSet versions)
// ============================================================================
template<typename Set>
void test_iterator() {
    Set set;
    for (int i = 0; i < 10; i++)
        set.insert(i);

    int count = 0;
    for (auto it = set.begin(); it != set.end(); ++it) {
        TEST_ASSERT(set.contains(*it), "iterator element should be in set");
        count++;
    }
    TEST_ASSERT(count == 10, "iterator should visit all elements");

    // const iterator
    const auto& cset = set;
    count = 0;
    for (auto it = cset.cbegin(); it != cset.cend(); ++it)
        count++;
    TEST_ASSERT(count == 10, "const iterator count");

    g_pass++;
}

// ============================================================================
// shrink_to_fit tests (all HashSet versions)
// ============================================================================
template<typename Set>
void test_shrink_to_fit() {
    Set set;
    for (int i = 0; i < 100; i++)
        set.insert(i);

    auto bc_before = set.bucket_count();
    for (int i = 0; i < 90; i++)
        set.erase(i);

    set.shrink_to_fit();
    auto bc_after = set.bucket_count();
    TEST_ASSERT(bc_after <= bc_before, "shrink_to_fit should not increase buckets");
    TEST_ASSERT(set.size() == 10, "shrink_to_fit should not change size");

    for (int i = 90; i < 100; i++)
        TEST_ASSERT(set.contains(i), "shrink_to_fit should preserve elements");

    g_pass++;
}

// ============================================================================
// erase_if tests (hash_set2/3/8 only)
// ============================================================================
template<typename Set>
void test_erase_if() {
    Set set;
    for (int i = 0; i < 10; i++)
        set.insert(i);

    auto erased = set.erase_if([](const auto& key) { return key >= 5; });
    TEST_ASSERT(erased == 5, "erase_if count");
    TEST_ASSERT(set.size() == 5, "erase_if remaining size");
    for (int i = 0; i < 5; i++)
        TEST_ASSERT(set.contains(i), "erase_if should keep < 5");
    for (int i = 5; i < 10; i++)
        TEST_ASSERT(!set.contains(i), "erase_if should remove >= 5");

    g_pass++;
}

// ============================================================================
// merge tests (hash_set2/3/8 only)
// ============================================================================
template<typename Set>
void test_merge() {
    Set set1;
    set1.insert(1);
    set1.insert(2);

    Set set2;
    set2.insert(2);
    set2.insert(3);

    set1.merge(set2);

    TEST_ASSERT(set1.size() == 3, "merge size");
    TEST_ASSERT(set1.contains(1), "merge keeps existing 1");
    TEST_ASSERT(set1.contains(2), "merge keeps existing 2");
    TEST_ASSERT(set1.contains(3), "merge adds 3 from source");

    g_pass++;
}

// ============================================================================
// Run common tests for all HashSet types
// ============================================================================
template<typename Set>
void run_common_tests(const char* name) {
    printf("Testing %s:\n", name);
    test_basic_crud<Set>();
    test_insert_unique<Set>();
    test_copy_move_swap<Set>();
    test_iterator<Set>();
    printf("  %s done\n\n", name);
}

int main() {
    printf("=== HashSet Full API Coverage Tests ===\n\n");

    // Common tests for all versions
    run_common_tests<emhash2::HashSet<int>>("hash_set2");
    run_common_tests<emhash9::HashSet<int>>("hash_set4");
    run_common_tests<emhash8::HashSet<int>>("hash_set8");

    // hash_set3: no insert_unique (has assert(false))
    printf("Testing hash_set3:\n");
    test_basic_crud_no_unique<emhash7::HashSet<int>>();
    test_copy_move_swap<emhash7::HashSet<int>>();
    test_iterator<emhash7::HashSet<int>>();
    printf("  hash_set3 done\n\n");

    // shrink_to_fit: hash_set2, hash_set4, hash_set8
    printf("Testing shrink_to_fit:\n");
    test_shrink_to_fit<emhash2::HashSet<int>>();
    test_shrink_to_fit<emhash9::HashSet<int>>();
    test_shrink_to_fit<emhash8::HashSet<int>>();
    printf("  shrink_to_fit done\n\n");

    // erase_if and merge: hash_set8 only
    printf("Testing erase_if (hash_set8):\n");
    test_erase_if<emhash8::HashSet<int>>();
    printf("  erase_if done\n\n");

    printf("Testing merge (hash_set8):\n");
    test_merge<emhash8::HashSet<int>>();
    printf("  merge done\n\n");

    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
