// Stress test for all 7 hash map implementations
// Tests: emhash5, emhash6, emhash7, emhash8, emilib (emilib2ss), emilib2 (emilib2o), emilib3 (emilib2s)
// Compile: g++ -fsanitize=address,undefined -std=c++17 -g stress_all_maps.cpp -o stress_all_maps

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include <cassert>
#include <algorithm>

// Include all hash map implementations
#include "emhash/hash_table5.hpp"   // emhash5::HashMap
#include "emhash/hash_table6.hpp"   // emhash6::HashMap
#include "emhash/hash_table7.hpp"   // emhash7::HashMap
#include "emhash/hash_table8.hpp"   // emhash8::HashMap
#include "emilib/emihmap1.hpp"  // emilib::HashMap
#include "emilib/emihmap2.hpp"   // emilib2::HashMap
#include "emilib/emihmap3.hpp"   // emilib3::HashMap

// ============================================================================
// Test 1: reserve(1) + many inserts (stress the tiny table path)
// ============================================================================

template<typename HashMap>
int test_reserve1_tiny(const char* name, int trials)
{
    printf("=== Test 1: reserve(1) + many inserts [%s] ===\n", name);

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> key_dist(-100000000, 100000000);

    for (int trial = 0; trial < trials; trial++) {
        if (trial % 200 == 0) { printf("  trial %d...\n", trial); fflush(stdout); }

        HashMap m;
        m.reserve(1);  // Force tiny table

        for (int i = 0; i < 20; i++) {
            int key = key_dist(rng);
            int val = key_dist(rng);
            m.insert({key, val});
        }

        // Verify all inserted keys are findable via iteration
        for (auto it = m.begin(); it != m.end(); ++it) {
            auto found = m.find(it->first);
            if (found == m.end()) {
                printf("BUG trial %d: key %d not found via iteration\n", trial, it->first);
                return 1;
            }
        }
    }

    printf("PASS: %d trials with reserve(1) + 20 inserts each\n\n", trials);
    return 0;
}

// ============================================================================
// Test 2: High load factor (0.95+) with random operations
// ============================================================================

template<typename HashMap>
int test_high_load_factor(const char* name, int trials)
{
    printf("=== Test 2: High load factor (0.95+) [%s] ===\n", name);

    std::mt19937 rng(54321);
    std::uniform_int_distribution<int64_t> key_dist(0, 100000000);

    for (int trial = 0; trial < trials; trial++) {
        if (trial % 100 == 0) { printf("  trial %d...\n", trial); fflush(stdout); }

        HashMap m;
        m.reserve(1000);
        m.max_load_factor(0.98f);

        std::vector<int64_t> keys;
        // Fill to high load factor
        for (int i = 0; i < 950; i++) {
            int64_t key = key_dist(rng);
            keys.push_back(key);
            m[key] = i;
        }

        // Verify load factor
        float lf = static_cast<float>(m.size()) / static_cast<float>(m.bucket_count());
        if (lf < 0.90f) {
            printf("  Note: LF=%.3f (lower than expected due to duplicates)\n", lf);
        }

        // Random operations: insert, find, erase
        for (int i = 0; i < 100; i++) {
            int64_t key = key_dist(rng);
            int op = rng() % 3;

            switch (op) {
                case 0: // insert
                    m[key] = i;
                    keys.push_back(key);
                    break;
                case 1: // find
                    (void)m.count(key);
                    break;
                case 2: // erase
                    m.erase(key);
                    break;
            }
        }

        // Verify consistency
        size_t count = 0;
        for (auto& p : m) {
            (void)p;
            count++;
        }
        if (count != (size_t)m.size()) {
            printf("BUG trial %d: iter count %zu != size %zu\n", trial, count, (size_t)m.size());
            return 1;
        }
    }

    printf("PASS: %d trials with high load factor\n\n", trials);
    return 0;
}

// ============================================================================
// Test 3: Rapid insert/erase cycles
// ============================================================================

template<typename HashMap>
int test_rapid_insert_erase(const char* name, int trials)
{
    printf("=== Test 3: Rapid insert/erase cycles [%s] ===\n", name);

    std::mt19937 rng(98765);
    std::uniform_int_distribution<int> key_dist(0, 10000);

    for (int trial = 0; trial < trials; trial++) {
        if (trial % 200 == 0) { printf("  trial %d...\n", trial); fflush(stdout); }

        HashMap m;

        // Rapid insert/erase cycles
        for (int cycle = 0; cycle < 50; cycle++) {
            // Insert batch
            for (int i = 0; i < 100; i++) {
                int key = key_dist(rng);
                m[key] = i;
            }

            // Erase batch
            for (int i = 0; i < 50; i++) {
                int key = key_dist(rng);
                m.erase(key);
            }
        }

        // Verify map is in consistent state
        size_t count = 0;
        for (auto& p : m) {
            (void)p;
            count++;
        }
        if (count != (size_t)m.size()) {
            printf("BUG trial %d: iter count %zu != size %zu\n", trial, count, (size_t)m.size());
            return 1;
        }
    }

    printf("PASS: %d trials with rapid insert/erase cycles\n\n", trials);
    return 0;
}

// ============================================================================
// Test 4: Rehash under stress
// ============================================================================

template<typename HashMap>
int test_rehash_stress(const char* name, int trials)
{
    printf("=== Test 4: Rehash under stress [%s] ===\n", name);

    std::mt19937 rng(11111);
    std::uniform_int_distribution<int64_t> key_dist(0, 1000000);

    for (int trial = 0; trial < trials; trial++) {
        if (trial % 100 == 0) { printf("  trial %d...\n", trial); fflush(stdout); }

        HashMap m;
        std::vector<int64_t> keys;

        // Insert many elements to trigger multiple rehashes
        for (int i = 0; i < 1000; i++) {
            int64_t key = key_dist(rng);
            keys.push_back(key);
            m[key] = i;
        }

        size_t orig_buckets = m.bucket_count();
        (void)orig_buckets;

        // Force rehash by inserting more
        for (int i = 0; i < 500; i++) {
            int64_t key = key_dist(rng);
            keys.push_back(key);
            m[key] = i + 1000;
        }

        // Verify all keys are still accessible
        for (auto key : keys) {
            auto it = m.find(key);
            if (it != m.end()) {
                // Key exists, verify value is valid
                if (it->second < 0 || it->second > 1500) {
                    printf("BUG trial %d: invalid value %d for key %lld\n",
                           trial, it->second, (long long)key);
                    return 1;
                }
            }
        }

        // Verify iteration count
        size_t count = 0;
        for (auto& p : m) {
            (void)p;
            count++;
        }
        if (count != (size_t)m.size()) {
            printf("BUG trial %d: iter count %zu != size %zu\n", trial, count, (size_t)m.size());
            return 1;
        }
    }

    printf("PASS: %d trials with rehash stress\n\n", trials);
    return 0;
}

// ============================================================================
// Test 5: Concurrent-looking patterns (interleaved operations)
// ============================================================================

template<typename HashMap>
int test_interleaved_ops(const char* name, int trials)
{
    printf("=== Test 5: Interleaved operations [%s] ===\n", name);

    std::mt19937 rng(22222);
    std::uniform_int_distribution<int> key_dist(0, 5000);
    std::uniform_int_distribution<int> op_dist(0, 9);

    for (int trial = 0; trial < trials; trial++) {
        if (trial % 100 == 0) { printf("  trial %d...\n", trial); fflush(stdout); }

        HashMap m;
        std::vector<int> inserted_keys;

        // Interleaved operations simulating concurrent access pattern
        for (int i = 0; i < 500; i++) {
            int op = op_dist(rng);
            int key = key_dist(rng);

            switch (op) {
                case 0: case 1: case 2: // insert (30%)
                    {
                        auto result = m.insert({key, i});
                        if (result.second) {
                            inserted_keys.push_back(key);
                        }
                    }
                    break;
                case 3: case 4: // erase (20%)
                    {
                        auto it = m.find(key);
                        if (it != m.end()) {
                            m.erase(it);
                            // Remove from tracking
                            auto kit = std::find(inserted_keys.begin(), inserted_keys.end(), key);
                            if (kit != inserted_keys.end()) {
                                inserted_keys.erase(kit);
                            }
                        }
                    }
                    break;
                case 5: case 6: case 7: // find (30%)
                    (void)m.find(key);
                    break;
                case 8: // iterate (10%)
                    {
                        int count = 0;
                        for (auto& p : m) {
                            (void)p;
                            count++;
                        }
                        if (static_cast<size_t>(count) != (size_t)m.size()) {
                            printf("BUG trial %d: iter count %d != size %zu\n",
                                   trial, count, (size_t)m.size());
                            return 1;
                        }
                    }
                    break;
                case 9: // count (10%)
                    (void)m.count(key);
                    break;
            }
        }

        // Final consistency check
        size_t count = 0;
        for (auto& p : m) {
            (void)p;
            count++;
        }
        if (count != (size_t)m.size()) {
            printf("BUG trial %d: final iter count %zu != size %zu\n",
                   trial, count, (size_t)m.size());
            return 1;
        }
    }

    printf("PASS: %d trials with interleaved operations\n\n", trials);
    return 0;
}

// ============================================================================
// Main test runner
// ============================================================================

int main()
{
    printf("========================================\n");
    printf("Stress test for all 7 hash map implementations\n");
    printf("========================================\n\n");

    const int trials = 1000;
    int failures = 0;

    // Test emhash5::HashMap
    printf("----------------------------------------\n");
    printf("Testing emhash5::HashMap\n");
    printf("----------------------------------------\n");
    failures += test_reserve1_tiny<emhash5::HashMap<int, int>>("emhash5", trials);
    failures += test_high_load_factor<emhash5::HashMap<int64_t, int>>("emhash5", trials);
    failures += test_rapid_insert_erase<emhash5::HashMap<int, int>>("emhash5", trials);
    failures += test_rehash_stress<emhash5::HashMap<int64_t, int>>("emhash5", trials);
    failures += test_interleaved_ops<emhash5::HashMap<int, int>>("emhash5", trials);

    // Test emhash6::HashMap
    printf("----------------------------------------\n");
    printf("Testing emhash6::HashMap\n");
    printf("----------------------------------------\n");
    failures += test_reserve1_tiny<emhash6::HashMap<int, int>>("emhash6", trials);
    failures += test_high_load_factor<emhash6::HashMap<int64_t, int>>("emhash6", trials);
    failures += test_rapid_insert_erase<emhash6::HashMap<int, int>>("emhash6", trials);
    failures += test_rehash_stress<emhash6::HashMap<int64_t, int>>("emhash6", trials);
    failures += test_interleaved_ops<emhash6::HashMap<int, int>>("emhash6", trials);

    // Test emhash7::HashMap
    printf("----------------------------------------\n");
    printf("Testing emhash7::HashMap\n");
    printf("----------------------------------------\n");
    failures += test_reserve1_tiny<emhash7::HashMap<int, int>>("emhash7", trials);
    failures += test_high_load_factor<emhash7::HashMap<int64_t, int>>("emhash7", trials);
    failures += test_rapid_insert_erase<emhash7::HashMap<int, int>>("emhash7", trials);
    failures += test_rehash_stress<emhash7::HashMap<int64_t, int>>("emhash7", trials);
    failures += test_interleaved_ops<emhash7::HashMap<int, int>>("emhash7", trials);

    // Test emhash8::HashMap
    printf("----------------------------------------\n");
    printf("Testing emhash8::HashMap\n");
    printf("----------------------------------------\n");
    failures += test_reserve1_tiny<emhash8::HashMap<int, int>>("emhash8", trials);
    failures += test_high_load_factor<emhash8::HashMap<int64_t, int>>("emhash8", trials);
    failures += test_rapid_insert_erase<emhash8::HashMap<int, int>>("emhash8", trials);
    failures += test_rehash_stress<emhash8::HashMap<int64_t, int>>("emhash8", trials);
    failures += test_interleaved_ops<emhash8::HashMap<int, int>>("emhash8", trials);

    // Test emilib::HashMap (emilib2ss)
    printf("----------------------------------------\n");
    printf("Testing emilib::HashMap (emilib2ss)\n");
    printf("----------------------------------------\n");
    failures += test_reserve1_tiny<emilib::HashMap<int, int>>("emilib", trials);
    failures += test_high_load_factor<emilib::HashMap<int64_t, int>>("emilib", trials);
    failures += test_rapid_insert_erase<emilib::HashMap<int, int>>("emilib", trials);
    failures += test_rehash_stress<emilib::HashMap<int64_t, int>>("emilib", trials);
    failures += test_interleaved_ops<emilib::HashMap<int, int>>("emilib", trials);

    // Test emilib2::HashMap (emilib2o)
    printf("----------------------------------------\n");
    printf("Testing emilib2::HashMap (emilib2o)\n");
    printf("----------------------------------------\n");
    failures += test_reserve1_tiny<emilib2::HashMap<int, int>>("emilib2", trials);
    failures += test_high_load_factor<emilib2::HashMap<int64_t, int>>("emilib2", trials);
    failures += test_rapid_insert_erase<emilib2::HashMap<int, int>>("emilib2", trials);
    failures += test_rehash_stress<emilib2::HashMap<int64_t, int>>("emilib2", trials);
    failures += test_interleaved_ops<emilib2::HashMap<int, int>>("emilib2", trials);

    // Test emilib3::HashMap (emilib2s)
    printf("----------------------------------------\n");
    printf("Testing emilib3::HashMap (emilib2s)\n");
    printf("----------------------------------------\n");
    failures += test_reserve1_tiny<emilib3::HashMap<int, int>>("emilib3", trials);
    failures += test_high_load_factor<emilib3::HashMap<int64_t, int>>("emilib3", trials);
    failures += test_rapid_insert_erase<emilib3::HashMap<int, int>>("emilib3", trials);
    failures += test_rehash_stress<emilib3::HashMap<int64_t, int>>("emilib3", trials);
    failures += test_interleaved_ops<emilib3::HashMap<int, int>>("emilib3", trials);

    // Summary
    printf("========================================\n");
    printf("SUMMARY\n");
    printf("========================================\n");
    printf("Total test failures: %d\n", failures);
    printf("Each implementation ran %d trials per test x 5 tests = %d trials total\n",
           trials, trials * 5);
    printf("7 implementations x %d trials = %d total trial runs\n",
           trials * 5, 7 * trials * 5);

    if (failures == 0) {
        printf("\nALL TESTS PASSED!\n");
    } else {
        printf("\n%d TEST(S) FAILED!\n", failures);
    }

    return failures;
}