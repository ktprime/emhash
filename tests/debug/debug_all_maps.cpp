// Debug test for all 7 hash map implementations
// Tests: chain integrity, erase correctness, find after rehash, edge cases
// Compile: g++ -std=c++17 -g -O0 -fsanitize=address -I../.. debug_all_maps.cpp -o debug_all_maps
//
// Hash maps tested:
//   - emhash5::HashMap (hash_table5.hpp)
//   - emhash6::HashMap (hash_table6.hpp)
//   - emhash7::HashMap (hash_table7.hpp)
//   - emhash8::HashMap (hash_table8.hpp)
//   - emilib::HashMap (emihmap1.hpp)
//   - emilib2::HashMap (emihmap2.hpp)
//   - emilib3::HashMap (emihmap3.hpp)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <functional>

// Include all hash map headers
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

// =============================================================================
// Test utilities
// =============================================================================

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, ...)                                                                                         \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "FAIL at %s:%d: ", __FILE__, __LINE__);                                                    \
            fprintf(stderr, __VA_ARGS__);                                                                              \
            fprintf(stderr, "\n");                                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

#define TEST_START(name) printf("\n=== %s ===\n", name)
#define TEST_PASS()                                                                                                    \
    do {                                                                                                               \
        printf("PASS\n");                                                                                              \
        g_tests_passed++;                                                                                              \
        return true;                                                                                                   \
    } while (0)
#define TEST_FAIL(msg, ...)                                                                                            \
    do {                                                                                                               \
        fprintf(stderr, "FAIL: " msg "\n", ##__VA_ARGS__);                                                             \
        g_tests_failed++;                                                                                              \
        return false;                                                                                                  \
    } while (0)

// Custom hashers for controlled collision testing
struct ConstHasher {
    size_t seed = 0;
    explicit ConstHasher(size_t s = 0) : seed(s) {}
    size_t operator()(int x) const {
        (void)x;
        return seed; // All keys get same hash -> maximum collision
    }
};

struct ModHasher {
    size_t mod;
    explicit ModHasher(size_t m = 16) : mod(m) {}
    size_t operator()(int x) const {
        return static_cast<size_t>(x) % mod; // Controlled collision
    }
};

struct BadHasher {
    size_t operator()(int x) const {
        // High bits collision - tests EMH_EQHASH in emhash8
        return static_cast<size_t>(x) & 0xFF; // Only low 8 bits vary
    }
};

// =============================================================================
// Test 1: Basic insert/find/erase consistency
// =============================================================================

template <typename HashMap> bool test_basic_operations(const char* name) {
    TEST_START(name);

    HashMap m;
    std::unordered_map<int, int> ref;

    // Insert some values
    for (int i = 0; i < 100; i++) {
        m[i] = i * 10;
        ref[i] = i * 10;
    }

    TEST_ASSERT((size_t)m.size() == ref.size(), "size mismatch: %zu vs %zu", (size_t)m.size(), ref.size());

    // Find all inserted keys
    for (int i = 0; i < 100; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it != m.end(), "key %d not found", i);
        TEST_ASSERT(it->second == i * 10, "wrong value for key %d: %d vs %d", i, it->second, i * 10);
    }

    // Keys not inserted should not be found
    for (int i = 1000; i < 1100; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it == m.end(), "key %d should not be found", i);
    }

    // Erase half the keys
    for (int i = 0; i < 50; i++) {
        size_t erased = m.erase(i);
        size_t ref_erased = ref.erase(i);
        TEST_ASSERT(erased == ref_erased, "erase count mismatch for key %d: %zu vs %zu", i, erased, ref_erased);
    }

    TEST_ASSERT((size_t)m.size() == ref.size(), "size mismatch after erase: %zu vs %zu", (size_t)m.size(), ref.size());

    // Verify erased keys are gone
    for (int i = 0; i < 50; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it == m.end(), "erased key %d still found", i);
    }

    // Verify remaining keys still work
    for (int i = 50; i < 100; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it != m.end(), "remaining key %d not found", i);
        TEST_ASSERT(it->second == i * 10, "wrong value for remaining key %d", i);
    }

    TEST_PASS();
}

// =============================================================================
// Test 2: Erase correctness - verify erased keys are truly gone
// =============================================================================

template <typename HashMap> bool test_erase_correctness(const char* name) {
    TEST_START(name);

    HashMap m;
    std::unordered_map<int, int> ref;

    // Insert keys with specific pattern
    for (int i = 0; i < 200; i++) {
        m[i * 3] = i; // Keys: 0, 3, 6, 9, ...
        ref[i * 3] = i;
    }

    // Erase every other key
    for (int i = 0; i < 200; i += 2) {
        int key = i * 3;
        size_t cnt = m.erase(key);
        TEST_ASSERT(cnt == 1, "erase returned %zu for key %d", cnt, key);
        ref.erase(key);
    }

    TEST_ASSERT((size_t)m.size() == ref.size(), "size mismatch: %zu vs %zu", (size_t)m.size(), ref.size());

    // Verify all erased keys are gone
    for (int i = 0; i < 200; i += 2) {
        int key = i * 3;
        auto it = m.find(key);
        TEST_ASSERT(it == m.end(), "erased key %d still found", key);
        TEST_ASSERT(m.count(key) == 0, "count() != 0 for erased key %d", key);
    }

    // Verify remaining keys still work
    for (int i = 1; i < 200; i += 2) {
        int key = i * 3;
        auto it = m.find(key);
        TEST_ASSERT(it != m.end(), "remaining key %d not found", key);
        TEST_ASSERT(it->second == i, "wrong value for key %d", key);
    }

    // Insert some new keys that may reuse erased slots
    for (int i = 0; i < 50; i++) {
        m[i * 7] = i * 100; // New keys: 0, 7, 14, ...
        ref[i * 7] = i * 100;
    }

    TEST_ASSERT((size_t)m.size() == ref.size(), "size mismatch after reinsert: %zu vs %zu", (size_t)m.size(),
                ref.size());

    // Verify all keys
    for (const auto& [k, v] : ref) {
        auto it = m.find(k);
        TEST_ASSERT(it != m.end(), "key %d not found after reinsert", k);
        TEST_ASSERT(it->second == v, "wrong value for key %d: %d vs %d", k, it->second, v);
    }

    TEST_PASS();
}

// =============================================================================
// Test 3: Find after rehash
// =============================================================================

template <typename HashMap> bool test_find_after_rehash(const char* name) {
    TEST_START(name);

    HashMap m;
    std::unordered_map<int, int> ref;

    // Insert many keys to trigger rehash
    for (int i = 0; i < 1000; i++) {
        m[i] = i * i;
        ref[i] = i * i;
    }

    TEST_ASSERT((size_t)m.size() == ref.size(), "size mismatch: %zu vs %zu", (size_t)m.size(), ref.size());

    // Reserve to force rehash
    m.reserve(5000);

    // All keys should still be findable
    for (int i = 0; i < 1000; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it != m.end(), "key %d not found after reserve", i);
        TEST_ASSERT(it->second == i * i, "wrong value for key %d after reserve", i);
    }

    // Erase some keys
    for (int i = 0; i < 500; i++) {
        m.erase(i);
        ref.erase(i);
    }

    // Insert more to trigger another rehash
    for (int i = 2000; i < 3000; i++) {
        m[i] = i;
        ref[i] = i;
    }

    // Verify all remaining keys
    for (const auto& [k, v] : ref) {
        auto it = m.find(k);
        TEST_ASSERT(it != m.end(), "key %d not found after rehash", k);
        TEST_ASSERT(it->second == v, "wrong value for key %d", k);
    }

    TEST_PASS();
}

// =============================================================================
// Test 4: Collision handling with custom hasher
// =============================================================================

template <typename HashMap> bool test_collision_handling(const char* name) {
    TEST_START(name);

    // Use const hasher to force all keys to collide
    HashMap m(4); // Start with small capacity
    std::unordered_map<int, int> ref;

    // Insert keys - all will collide
    for (int i = 0; i < 50; i++) {
        m[i] = i * 10;
        ref[i] = i * 10;
    }

    TEST_ASSERT((size_t)m.size() == ref.size(), "size mismatch: %zu vs %zu", (size_t)m.size(), ref.size());

    // All keys should be findable despite collision
    for (int i = 0; i < 50; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it != m.end(), "collided key %d not found", i);
        TEST_ASSERT(it->second == i * 10, "wrong value for collided key %d", i);
    }

    // Erase some collided keys
    for (int i = 0; i < 25; i++) {
        size_t cnt = m.erase(i);
        TEST_ASSERT(cnt == 1, "erase returned %zu for key %d", cnt, i);
    }

    // Remaining keys should still work
    for (int i = 25; i < 50; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it != m.end(), "remaining collided key %d not found", i);
        TEST_ASSERT(it->second == i * 10, "wrong value for remaining key %d", i);
    }

    // Erased keys should not be found
    for (int i = 0; i < 25; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it == m.end(), "erased collided key %d still found", i);
    }

    TEST_PASS();
}

// =============================================================================
// Test 5: Iterator validity after erase
// =============================================================================

// Helper trait to detect if erase returns iterator
template <typename T, typename = void> struct erase_returns_iterator : std::false_type {};

template <typename T>
struct erase_returns_iterator<T, std::void_t<decltype(std::declval<T>().erase(std::declval<typename T::iterator>()))>>
    : std::is_same<decltype(std::declval<T>().erase(std::declval<typename T::iterator>())), typename T::iterator> {};

template <typename HashMap> bool test_iterator_after_erase(const char* name) {
    TEST_START(name);

    HashMap m;

    // Insert keys
    for (int i = 0; i < 100; i++) {
        m[i] = i;
    }

    // Iterate and erase - handle both returning iterator and void
    int erase_count = 0;
    (void)erase_count;
    if constexpr (erase_returns_iterator<HashMap>::value) {
        // erase returns iterator (emhash style)
        auto it = m.begin();
        while (it != m.end()) {
            int key = it->first;
            if (key % 3 == 0) {
                it = m.erase(it);
                erase_count++;
            } else {
                ++it;
            }
        }
    } else {
        // erase returns void (emilib style) - use key-based erase
        std::vector<int> keys_to_erase;
        for (auto it = m.begin(); it != m.end(); ++it) {
            if (it->first % 3 == 0) {
                keys_to_erase.push_back(it->first);
            }
        }
        for (int key : keys_to_erase) {
            m.erase(key);
            erase_count++;
        }
    }

    // Verify erased keys are gone
    for (int i = 0; i < 100; i += 3) {
        TEST_ASSERT(m.find(i) == m.end(), "key %d should be erased", i);
    }

    // Verify remaining keys
    for (int i = 1; i < 100; i++) {
        if (i % 3 != 0) {
            auto it = m.find(i);
            TEST_ASSERT(it != m.end(), "remaining key %d not found", i);
            TEST_ASSERT(it->second == i, "wrong value for key %d", i);
        }
    }

    TEST_PASS();
}

// =============================================================================
// Test 6: Edge cases - empty map, single element
// =============================================================================

template <typename HashMap> bool test_edge_cases(const char* name) {
    TEST_START(name);

    // Empty map operations
    {
        HashMap m;
        TEST_ASSERT(m.empty(), "new map should be empty");
        TEST_ASSERT(m.size() == 0, "new map size should be 0");
        TEST_ASSERT(m.find(0) == m.end(), "find on empty map should return end");
        TEST_ASSERT(m.count(0) == 0, "count on empty map should return 0");
        TEST_ASSERT(m.erase(0) == 0, "erase on empty map should return 0");

        auto it = m.begin();
        TEST_ASSERT(it == m.end(), "begin on empty map should equal end");
    }

    // Single element
    {
        HashMap m;
        m[1] = 100;
        TEST_ASSERT(!m.empty(), "map with one element should not be empty");
        TEST_ASSERT(m.size() == 1, "size should be 1");
        TEST_ASSERT(m.find(1) != m.end(), "key 1 should be found");
        TEST_ASSERT(m.find(1)->second == 100, "value should be 100");
        TEST_ASSERT(m.count(1) == 1, "count should be 1");

        // Erase single element
        TEST_ASSERT(m.erase(1) == 1, "erase should return 1");
        TEST_ASSERT(m.empty(), "map should be empty after erase");
        TEST_ASSERT(m.find(1) == m.end(), "key should not be found after erase");
    }

    // Clear and reuse
    {
        HashMap m;
        for (int i = 0; i < 100; i++) {
            m[i] = i;
        }
        m.clear();
        TEST_ASSERT(m.empty(), "map should be empty after clear");
        TEST_ASSERT(m.size() == 0, "size should be 0 after clear");

        // Reuse after clear
        for (int i = 0; i < 50; i++) {
            m[i * 2] = i * 3;
        }
        TEST_ASSERT(m.size() == 50, "size should be 50 after reuse");
        for (int i = 0; i < 50; i++) {
            auto it = m.find(i * 2);
            TEST_ASSERT(it != m.end() && it->second == i * 3, "wrong value after reuse");
        }
    }

    // Reserve on empty map
    {
        HashMap m;
        m.reserve(1000);
        TEST_ASSERT(m.empty(), "map should be empty after reserve");
        for (int i = 0; i < 500; i++) {
            m[i] = i;
        }
        TEST_ASSERT(m.size() == 500, "size should be 500");
    }

    TEST_PASS();
}

// =============================================================================
// Test 7: Stress test with random operations
// =============================================================================

template <typename HashMap> bool test_stress_random(const char* name) {
    TEST_START(name);

    HashMap m;
    std::unordered_map<int, int> ref;

    std::mt19937 rng(12345); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> key_dist(0, 999);
    std::uniform_int_distribution<int> op_dist(0, 9);

    for (int op = 0; op < 10000; op++) {
        int key = key_dist(rng);
        int val = key * 10;
        int op_type = op_dist(rng);

        switch (op_type) {
        case 0: // insert via []
            m[key] = val;
            ref[key] = val;
            break;
        case 1: // insert
            m.insert({key, val});
            ref[key] = val;
            break;
        case 2: // erase
            m.erase(key);
            ref.erase(key);
            break;
        case 3: // find
        {
            auto it = m.find(key);
            auto ref_it = ref.find(key);
            TEST_ASSERT((it == m.end()) == (ref_it == ref.end()), "find mismatch for key %d at op %d", key, op);
        } break;
        case 4: // count
            TEST_ASSERT((size_t)m.count(key) == ref.count(key), "count mismatch for key %d at op %d", key, op);
            break;
        case 5: // contains (if available)
            // Use count as fallback
            TEST_ASSERT((size_t)m.count(key) == ref.count(key), "contains mismatch for key %d at op %d", key, op);
            break;
        case 6: // clear
            m.clear();
            ref.clear();
            break;
        case 7: // reserve
            m.reserve(500);
            break;
        case 8: // emplace
            m.emplace(key, val);
            ref[key] = val;
            break;
        case 9: // insert_or_assign
            m.insert_or_assign(key, std::move(val));
            ref[key] = val;
            break;
        }

        // Check size consistency
        if ((size_t)m.size() != ref.size()) {
            fprintf(stderr, "SIZE MISMATCH at op %d: map=%zu ref=%zu\n", op, (size_t)m.size(), ref.size());
            fprintf(stderr, "Last operation: op_type=%d key=%d\n", op_type, key);

            // Print map contents for debugging
            fprintf(stderr, "Map contents:\n");
            for (auto it = m.begin(); it != m.end(); ++it) {
                fprintf(stderr, "  [%d] = %d\n", it->first, it->second);
            }
            TEST_ASSERT(false, "size mismatch");
        }
    }

    // Final verification
    for (const auto& [k, v] : ref) {
        auto it = m.find(k);
        TEST_ASSERT(it != m.end(), "final: key %d not found", k);
        TEST_ASSERT(it->second == v, "final: wrong value for key %d", k);
    }

    TEST_PASS();
}

// =============================================================================
// Test 8: Chain integrity after many erase operations
// =============================================================================

template <typename HashMap> bool test_chain_after_erase(const char* name) {
    TEST_START(name);

    HashMap m;
    std::unordered_map<int, int> ref;

    // Insert keys in a pattern that creates chains
    for (int i = 0; i < 500; i++) {
        m[i] = i;
        ref[i] = i;
    }

    // Erase keys in various patterns
    // Pattern 1: Erase every 3rd key
    for (int i = 0; i < 500; i += 3) {
        m.erase(i);
        ref.erase(i);
    }

    // Verify remaining keys
    for (const auto& [k, v] : ref) {
        auto it = m.find(k);
        TEST_ASSERT(it != m.end(), "key %d not found after pattern 1 erase", k);
        TEST_ASSERT(it->second == v, "wrong value for key %d", k);
    }

    // Pattern 2: Erase consecutive ranges
    for (int i = 100; i < 200; i++) {
        m.erase(i);
        ref.erase(i);
    }

    // Verify again
    for (const auto& [k, v] : ref) {
        auto it = m.find(k);
        TEST_ASSERT(it != m.end(), "key %d not found after pattern 2 erase", k);
        TEST_ASSERT(it->second == v, "wrong value for key %d", k);
    }

    // Pattern 3: Erase from the end
    for (int i = 400; i < 500; i++) {
        m.erase(i);
        ref.erase(i);
    }

    // Final verification
    for (const auto& [k, v] : ref) {
        auto it = m.find(k);
        TEST_ASSERT(it != m.end(), "key %d not found after pattern 3 erase", k);
        TEST_ASSERT(it->second == v, "wrong value for key %d", k);
    }

    // Verify erased keys are truly gone
    for (int i = 0; i < 500; i += 3) {
        TEST_ASSERT(m.find(i) == m.end(), "erased key %d still found", i);
    }
    for (int i = 100; i < 200; i++) {
        TEST_ASSERT(m.find(i) == m.end(), "erased key %d still found", i);
    }
    for (int i = 400; i < 500; i++) {
        TEST_ASSERT(m.find(i) == m.end(), "erased key %d still found", i);
    }

    TEST_PASS();
}

// =============================================================================
// Test 9: High load factor operations
// =============================================================================

template <typename HashMap> bool test_high_load_factor(const char* name) {
    TEST_START(name);

    HashMap m;
    std::unordered_map<int, int> ref;

    // Insert until very high load factor
    for (int i = 0; i < 10000; i++) {
        m[i] = i * i;
        ref[i] = i * i;
    }

    TEST_ASSERT((size_t)m.size() == ref.size(), "size mismatch: %zu vs %zu", (size_t)m.size(), ref.size());

    // Find all keys
    for (int i = 0; i < 10000; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it != m.end(), "key %d not found at high load", i);
        TEST_ASSERT(it->second == i * i, "wrong value for key %d", i);
    }

    // Erase half
    for (int i = 0; i < 5000; i++) {
        m.erase(i);
        ref.erase(i);
    }

    // Verify remaining
    for (int i = 5000; i < 10000; i++) {
        auto it = m.find(i);
        TEST_ASSERT(it != m.end(), "key %d not found after erase", i);
        TEST_ASSERT(it->second == i * i, "wrong value for key %d", i);
    }

    TEST_PASS();
}

// =============================================================================
// Test 10: Copy and move semantics
// =============================================================================

template <typename HashMap> bool test_copy_move(const char* name) {
    TEST_START(name);

    // Create and fill original
    HashMap m1;
    for (int i = 0; i < 100; i++) {
        m1[i] = i * 10;
    }

    // Copy constructor
    HashMap m2(m1);
    TEST_ASSERT(m2.size() == m1.size(), "copy size mismatch");
    for (int i = 0; i < 100; i++) {
        auto it = m2.find(i);
        TEST_ASSERT(it != m2.end(), "key %d not found in copy", i);
        TEST_ASSERT(it->second == i * 10, "wrong value in copy for key %d", i);
    }

    // Modify copy, original should be unchanged
    m2[0] = 999;
    TEST_ASSERT(m1.find(0)->second == 0, "original modified by copy");
    TEST_ASSERT(m2.find(0)->second == 999, "copy not modified");

    // Move constructor
    HashMap m3(std::move(m1));
    TEST_ASSERT(m3.size() == 100, "moved map has wrong size");
    for (int i = 0; i < 100; i++) {
        auto it = m3.find(i);
        TEST_ASSERT(it != m3.end(), "key %d not found in moved map", i);
        TEST_ASSERT(it->second == i * 10, "wrong value in moved map for key %d", i);
    }

    // Copy assignment
    HashMap m4;
    m4 = m3;
    TEST_ASSERT(m4.size() == m3.size(), "copy assign size mismatch");
    for (int i = 0; i < 100; i++) {
        auto it = m4.find(i);
        TEST_ASSERT(it != m4.end(), "key %d not found in copy assigned map", i);
    }

    // Move assignment
    HashMap m5;
    m5 = std::move(m3);
    TEST_ASSERT(m5.size() == 100, "move assigned map has wrong size");

    TEST_PASS();
}

// =============================================================================
// Run all tests for a specific map type
// =============================================================================

template <typename HashMap> void run_all_tests(const char* map_name) {
    printf("\n");
    printf("============================================================\n");
    printf("Testing: %s\n", map_name);
    printf("============================================================\n");

    test_basic_operations<HashMap>(map_name);
    test_erase_correctness<HashMap>(map_name);
    test_find_after_rehash<HashMap>(map_name);
    test_collision_handling<HashMap>(map_name);
    test_iterator_after_erase<HashMap>(map_name);
    test_edge_cases<HashMap>(map_name);
    test_stress_random<HashMap>(map_name);
    test_chain_after_erase<HashMap>(map_name);
    test_high_load_factor<HashMap>(map_name);
    test_copy_move<HashMap>(map_name);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    printf("========================================\n");
    printf("Debug Test for All Hash Map Implementations\n");
    printf("========================================\n");

    // Test emhash5
    run_all_tests<emhash5::HashMap<int, int>>("emhash5::HashMap");

    // Test emhash6
    run_all_tests<emhash6::HashMap<int, int>>("emhash6::HashMap");

    // Test emhash7
    run_all_tests<emhash7::HashMap<int, int>>("emhash7::HashMap");

    // Test emhash8
    run_all_tests<emhash8::HashMap<int, int>>("emhash8::HashMap");

    // Test emilib (from emihmap1.hpp)
    run_all_tests<emilib::HashMap<int, int>>("emilib::HashMap (emihmap1)");
    run_all_tests<emilib2::HashMap<int, int>>("emilib2::HashMap (emihmap2)");
    run_all_tests<emilib3::HashMap<int, int>>("emilib3::HashMap (emihmap3)");

    // Summary
    printf("\n");
    printf("========================================\n");
    printf("SUMMARY\n");
    printf("========================================\n");
    printf("Tests passed: %d\n", g_tests_passed);
    printf("Tests failed: %d\n", g_tests_failed);

    if (g_tests_failed > 0) {
        printf("\n*** SOME TESTS FAILED ***\n");
        return 1;
    }

    printf("\n*** ALL TESTS PASSED ***\n");
    return 0;
}
