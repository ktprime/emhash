// Hash Attack Benchmark for All 7 HashMap Implementations
// Tests hash collision attacks on:
//   - emhash5::HashMap, emhash6::HashMap, emhash7::HashMap, emhash8::HashMap
//   - emilib::HashMap (emihmap1), emilib2::HashMap (emihmap2), emilib3::HashMap (emihmap3)
//
// Attack scenarios:
//   1. Constant hash: all keys map to bucket 0 (worst-case collision)
//   2. Small-range hash: keys map to few buckets (partial collision)
//   3. Linear hash: keys map to contiguous buckets
//
// Build: g++ -O2 -std=c++17 -o hash_attack_all hash_attack_all.cpp
//

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include <iomanip>

// ============================================================================
// Hash Functions for Attack Simulation
// ============================================================================

// 1. Constant hasher: all keys -> bucket 0 (worst-case collision)
struct const_hasher {
    size_t operator()(int /*k*/) const { return 0; }
};

// 2. Small-range hasher: keys -> first 4 buckets only (partial collision)
struct range4_hasher {
    size_t operator()(int k) const { return (size_t)(k & 3); }
};

// 3. Linear hasher: keys -> contiguous buckets
struct linear_hasher {
    size_t operator()(int k) const { return (size_t)k; }
};

// ============================================================================
// Timing Utility
// ============================================================================

static double now_ms() {
    return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

// ============================================================================
// Test Result Structure
// ============================================================================

struct TestResult {
    const char* name;
    int failures;
    double insert_time;
    double find_time;
    double erase_time;
    double mixed_time;
    size_t bucket_count;
    double load_factor;
};

// ============================================================================
// Template Test Functions for Any HashMap Type
// ============================================================================

template <typename HashMap> static int test_correctness_const(const char* map_name, int N) {
    printf("  [%s] Correctness: constant hash (all keys -> bucket 0), N=%d\n", map_name, N);
    HashMap m(8);
    std::unordered_map<int, int, const_hasher> ref(8);

    int fail = 0;

    // Insert
    for (int i = 0; i < N; i++) {
        m[i] = i;
        ref[i] = i;
    }
    if ((size_t)m.size() != (size_t)N) {
        printf("    FAIL: size %zu != %d\n", (size_t)m.size(), N);
        fail++;
    }

    // Find all
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        auto refit = ref.find(i);
        if (it == m.end() || it->second != i) {
            printf("    FAIL: find(%d)\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
        if (refit == ref.end() || refit->second != i) {
            printf("    FAIL: ref find(%d)\n", i);
            fail++;
        }
    }

    // Erase half
    for (int i = 0; i < N; i += 2) {
        (void)m.erase(i);
        ref.erase(i);
    }
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        auto refit = ref.find(i);
        if ((it == m.end()) != (refit == ref.end())) {
            printf("    FAIL: erase mismatch at %d\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
        if (it != m.end() && it->second != refit->second) {
            printf("    FAIL: post-erase value mismatch at %d\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
    }

    // Re-insert erased
    for (int i = 0; i < N; i += 2)
        m[i] = i;
    for (int i = 0; i < N; i++) {
        if (m[i] != i) {
            printf("    FAIL: reinsert %d\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
    }

    printf("    %s (%d failures)\n", fail == 0 ? "PASS" : "FAIL", fail);
    return fail;
}

template <typename HashMap> static int test_correctness_range4(const char* map_name, int N) {
    printf("  [%s] Correctness: range-4 hash (keys -> 4 buckets), N=%d\n", map_name, N);
    HashMap m(8);
    std::unordered_map<int, int, range4_hasher> ref(8);

    int fail = 0;
    for (int i = 0; i < N; i++) {
        m[i] = i;
        ref[i] = i;
    }
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i) {
            printf("    FAIL: find(%d) under range-4\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
    }
    printf("    %s\n", fail == 0 ? "PASS" : "FAIL");
    return fail;
}

template <typename HashMap> static int test_correctness_linear(const char* map_name, int N) {
    printf("  [%s] Correctness: linear hash (key == hash), N=%d\n", map_name, N);
    HashMap m(8);
    std::unordered_map<int, int, linear_hasher> ref(8);

    int fail = 0;
    for (int i = 0; i < N; i++) {
        m[i] = i;
        ref[i] = i;
    }
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i) {
            printf("    FAIL: find(%d) under linear\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
    }
    printf("    %s\n", fail == 0 ? "PASS" : "FAIL");
    return fail;
}

// ============================================================================
// Performance Benchmark Templates
// ============================================================================

template <typename HashMap, typename Hash>
static double bench_insert(const char* map_name, const char* hash_name, int N) {
    HashMap m(8);
    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        m[i] = i;
    auto t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-12s %-8s insert N=%-6d -> %8.1f ms (%.0f ops/ms), buckets=%zu, LF=%.2f\n", map_name, hash_name, N,
           elapsed, N / elapsed, (size_t)m.bucket_count(), m.load_factor());
    return elapsed;
}

template <typename HashMap, typename Hash>
static double bench_find(const char* map_name, const char* hash_name, int N) {
    HashMap m(8);
    for (int i = 0; i < N; i++)
        m[i] = i;

    auto t0 = now_ms();
    volatile int sink = 0;
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < N; i++) {
            sink += m.find(i) != m.end() ? 1 : 0;
        }
    }
    (void)sink;
    auto t1 = now_ms();
    double ops = (double)N * 10;
    double elapsed = t1 - t0;
    printf("  %-12s %-8s find   N=%-6d -> %8.1f ms (%.0f ops/ms)\n", map_name, hash_name, N, elapsed, ops / elapsed);
    return elapsed;
}

template <typename HashMap, typename Hash>
static double bench_erase(const char* map_name, const char* hash_name, int N) {
    HashMap m(8);
    for (int i = 0; i < N; i++)
        m[i] = i;

    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        (void)m.erase(i);
    auto t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-12s %-8s erase  N=%-6d -> %8.1f ms (%.0f ops/ms)\n", map_name, hash_name, N, elapsed, N / elapsed);
    return elapsed;
}

template <typename HashMap, typename Hash>
static double bench_mixed(const char* map_name, const char* hash_name, int N) {
    HashMap m(8);
    for (int i = 0; i < N; i++)
        m[i] = i;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> opd(0, 9);
    std::uniform_int_distribution<int> keyd(0, N - 1);

    auto t0 = now_ms();
    for (int i = 0; i < N; i++) {
        int op = opd(rng);
        int k = keyd(rng);
        if (op < 5)
            m[k] = k; // 50% insert
        else if (op < 8)
            (void)m.find(k); // 30% find
        else
            (void)m.erase(k); // 20% erase
    }
    auto t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-12s %-8s mixed  N=%-6d -> %8.1f ms (%.0f ops/ms)\n", map_name, hash_name, N, elapsed, N / elapsed);
    return elapsed;
}

// ============================================================================
// Test All Maps for a Given Hash Type
// ============================================================================

template <typename Hash> static void test_all_maps_correctness(const char* hash_name, int N) {
    printf("\n=== Correctness Test: %s hasher, N=%d ===\n", hash_name, N);
    int total_fail = 0;

    // emhash5
    total_fail += test_correctness_const<emhash5::HashMap<int, int, Hash>>("emhash5", N);
    total_fail += test_correctness_range4<emhash5::HashMap<int, int, Hash>>("emhash5", N);
    total_fail += test_correctness_linear<emhash5::HashMap<int, int, Hash>>("emhash5", N);

    // emhash6
    total_fail += test_correctness_const<emhash6::HashMap<int, int, Hash>>("emhash6", N);
    total_fail += test_correctness_range4<emhash6::HashMap<int, int, Hash>>("emhash6", N);
    total_fail += test_correctness_linear<emhash6::HashMap<int, int, Hash>>("emhash6", N);

    // emhash7
    total_fail += test_correctness_const<emhash7::HashMap<int, int, Hash>>("emhash7", N);
    total_fail += test_correctness_range4<emhash7::HashMap<int, int, Hash>>("emhash7", N);
    total_fail += test_correctness_linear<emhash7::HashMap<int, int, Hash>>("emhash7", N);

    // emhash8
    total_fail += test_correctness_const<emhash8::HashMap<int, int, Hash>>("emhash8", N);
    total_fail += test_correctness_range4<emhash8::HashMap<int, int, Hash>>("emhash8", N);
    total_fail += test_correctness_linear<emhash8::HashMap<int, int, Hash>>("emhash8", N);

    // emilib (emilib2ss)
    total_fail += test_correctness_const<emilib::HashMap<int, int, Hash>>("emilib", N);
    total_fail += test_correctness_range4<emilib::HashMap<int, int, Hash>>("emilib", N);
    total_fail += test_correctness_linear<emilib::HashMap<int, int, Hash>>("emilib", N);

    // emilib2 (emilib2o)
    total_fail += test_correctness_const<emilib2::HashMap<int, int, Hash>>("emilib2", N);
    total_fail += test_correctness_range4<emilib2::HashMap<int, int, Hash>>("emilib2", N);
    total_fail += test_correctness_linear<emilib2::HashMap<int, int, Hash>>("emilib2", N);

    // emilib3 (emilib2s)
    total_fail += test_correctness_const<emilib3::HashMap<int, int, Hash>>("emilib3", N);
    total_fail += test_correctness_range4<emilib3::HashMap<int, int, Hash>>("emilib3", N);
    total_fail += test_correctness_linear<emilib3::HashMap<int, int, Hash>>("emilib3", N);

    printf("\n  Total failures for %s hasher: %d\n", hash_name, total_fail);
}

// ============================================================================
// Performance Comparison for All Maps
// ============================================================================

template <typename Hash> static void bench_all_maps_insert(const char* hash_name, int N) {
    printf("\n--- Insert Performance: %s hasher, N=%d ---\n", hash_name, N);
    bench_insert<emhash5::HashMap<int, int, Hash>, Hash>("emhash5", hash_name, N);
    bench_insert<emhash6::HashMap<int, int, Hash>, Hash>("emhash6", hash_name, N);
    bench_insert<emhash7::HashMap<int, int, Hash>, Hash>("emhash7", hash_name, N);
    bench_insert<emhash8::HashMap<int, int, Hash>, Hash>("emhash8", hash_name, N);
    bench_insert<emilib::HashMap<int, int, Hash>, Hash>("emilib", hash_name, N);
    bench_insert<emilib2::HashMap<int, int, Hash>, Hash>("emilib2", hash_name, N);
    bench_insert<emilib3::HashMap<int, int, Hash>, Hash>("emilib3", hash_name, N);
}

template <typename Hash> static void bench_all_maps_find(const char* hash_name, int N) {
    printf("\n--- Find Performance (10 rounds): %s hasher, N=%d ---\n", hash_name, N);
    bench_find<emhash5::HashMap<int, int, Hash>, Hash>("emhash5", hash_name, N);
    bench_find<emhash6::HashMap<int, int, Hash>, Hash>("emhash6", hash_name, N);
    bench_find<emhash7::HashMap<int, int, Hash>, Hash>("emhash7", hash_name, N);
    bench_find<emhash8::HashMap<int, int, Hash>, Hash>("emhash8", hash_name, N);
    bench_find<emilib::HashMap<int, int, Hash>, Hash>("emilib", hash_name, N);
    bench_find<emilib2::HashMap<int, int, Hash>, Hash>("emilib2", hash_name, N);
    bench_find<emilib3::HashMap<int, int, Hash>, Hash>("emilib3", hash_name, N);
}

template <typename Hash> static void bench_all_maps_erase(const char* hash_name, int N) {
    printf("\n--- Erase Performance: %s hasher, N=%d ---\n", hash_name, N);
    bench_erase<emhash5::HashMap<int, int, Hash>, Hash>("emhash5", hash_name, N);
    bench_erase<emhash6::HashMap<int, int, Hash>, Hash>("emhash6", hash_name, N);
    bench_erase<emhash7::HashMap<int, int, Hash>, Hash>("emhash7", hash_name, N);
    bench_erase<emhash8::HashMap<int, int, Hash>, Hash>("emhash8", hash_name, N);
    bench_erase<emilib::HashMap<int, int, Hash>, Hash>("emilib", hash_name, N);
    bench_erase<emilib2::HashMap<int, int, Hash>, Hash>("emilib2", hash_name, N);
    bench_erase<emilib3::HashMap<int, int, Hash>, Hash>("emilib3", hash_name, N);
}

template <typename Hash> static void bench_all_maps_mixed(const char* hash_name, int N) {
    printf("\n--- Mixed Operations (50%% insert, 30%% find, 20%% erase): %s hasher, N=%d ---\n", hash_name, N);
    bench_mixed<emhash5::HashMap<int, int, Hash>, Hash>("emhash5", hash_name, N);
    bench_mixed<emhash6::HashMap<int, int, Hash>, Hash>("emhash6", hash_name, N);
    bench_mixed<emhash7::HashMap<int, int, Hash>, Hash>("emhash7", hash_name, N);
    bench_mixed<emhash8::HashMap<int, int, Hash>, Hash>("emhash8", hash_name, N);
    bench_mixed<emilib::HashMap<int, int, Hash>, Hash>("emilib", hash_name, N);
    bench_mixed<emilib2::HashMap<int, int, Hash>, Hash>("emilib2", hash_name, N);
    bench_mixed<emilib3::HashMap<int, int, Hash>, Hash>("emilib3", hash_name, N);
}

// ============================================================================
// High Load Attack Test
// ============================================================================

// Helper to create HashMap with high load factor (handles different constructor signatures)
template <typename HashMap> static HashMap create_high_load_map() {
    return HashMap(1024, 0.99f); // For emhash5/6/7/8, emilib2, emilib3
}

// Specialization for emilib (emilib2ss) which only supports single-arg constructor
static emilib::HashMap<int, int, const_hasher> create_high_load_map_emilib() {
    emilib::HashMap<int, int, const_hasher> m(1024);
    m.max_load_factor(0.99f);
    return m;
}

template <typename HashMap> static double test_high_load_attack(const char* map_name, int N) {
    HashMap m = create_high_load_map<HashMap>();
    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        m[i] = i;
    auto t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-12s insert N=%d (const+high_load): %8.1f ms (%.0f ops/ms), buckets=%zu, LF=%.2f\n", map_name, N,
           elapsed, N / elapsed, (size_t)m.bucket_count(), m.load_factor());
    return elapsed;
}

// Specialized function for emilib
static double test_high_load_attack_emilib(const char* map_name, int N) {
    auto m = create_high_load_map_emilib();
    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        m[i] = i;
    auto t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-12s insert N=%d (const+high_load): %8.1f ms (%.0f ops/ms), buckets=%zu, LF=%.2f\n", map_name, N,
           elapsed, N / elapsed, (size_t)m.bucket_count(), m.load_factor());
    return elapsed;
}

static void test_all_high_load(int N) {
    printf("\n=== High Load + Attack: N=%d, MLF=0.99 ===\n", N);
    test_high_load_attack<emhash5::HashMap<int, int, const_hasher>>("emhash5", N);
    test_high_load_attack<emhash6::HashMap<int, int, const_hasher>>("emhash6", N);
    test_high_load_attack<emhash7::HashMap<int, int, const_hasher>>("emhash7", N);
    test_high_load_attack<emhash8::HashMap<int, int, const_hasher>>("emhash8", N);
    test_high_load_attack_emilib("emilib", N);
    test_high_load_attack<emilib2::HashMap<int, int, const_hasher>>("emilib2", N);
    test_high_load_attack<emilib3::HashMap<int, int, const_hasher>>("emilib3", N);
}

// ============================================================================
// Stress Test: Verify No Crash or Hang
// ============================================================================

template <typename HashMap> static bool stress_test_no_crash(const char* map_name, int N) {
    printf("  [%s] Stress test (no crash/hang)... ", map_name);
    fflush(stdout);

    try {
        HashMap m(8);

        // Insert all keys with constant hash
        for (int i = 0; i < N; i++) {
            m[i] = i;
        }

        // Find all keys
        for (int i = 0; i < N; i++) {
            auto it = m.find(i);
            if (it == m.end() || it->second != i) {
                printf("FAIL (find mismatch at %d)\n", i);
                return false;
            }
        }

        // Erase and re-insert
        for (int i = 0; i < N; i += 2) {
            (void)m.erase(i);
        }
        for (int i = 0; i < N; i += 2) {
            m[i] = i;
        }

        // Verify all present
        for (int i = 0; i < N; i++) {
            if (m[i] != i) {
                printf("FAIL (value mismatch at %d)\n", i);
                return false;
            }
        }

        // Clear
        m.clear();
        if (m.size() != 0) {
            printf("FAIL (clear failed)\n");
            return false;
        }

        printf("PASS\n");
        return true;
    } catch (const std::exception& e) {
        printf("FAIL (exception: %s)\n", e.what());
        return false;
    } catch (...) {
        printf("FAIL (unknown exception)\n");
        return false;
    }
}

static void test_all_stress(int N) {
    printf("\n=== Stress Test (No Crash/Hang): N=%d ===\n", N);
    int passed = 0;
    passed += stress_test_no_crash<emhash5::HashMap<int, int, const_hasher>>("emhash5", N) ? 1 : 0;
    passed += stress_test_no_crash<emhash6::HashMap<int, int, const_hasher>>("emhash6", N) ? 1 : 0;
    passed += stress_test_no_crash<emhash7::HashMap<int, int, const_hasher>>("emhash7", N) ? 1 : 0;
    passed += stress_test_no_crash<emhash8::HashMap<int, int, const_hasher>>("emhash8", N) ? 1 : 0;
    passed += stress_test_no_crash<emilib::HashMap<int, int, const_hasher>>("emilib", N) ? 1 : 0;
    passed += stress_test_no_crash<emilib2::HashMap<int, int, const_hasher>>("emilib2", N) ? 1 : 0;
    passed += stress_test_no_crash<emilib3::HashMap<int, int, const_hasher>>("emilib3", N) ? 1 : 0;
    printf("  Passed: %d/7\n", passed);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    printf("################################################################\n");
    printf("# Hash Attack Benchmark for All 7 HashMap Implementations\n");
    printf("#\n");
    printf("# Maps tested:\n");
    printf("#   - emhash5::HashMap  (hash_table5.hpp)\n");
    printf("#   - emhash6::HashMap  (hash_table6.hpp)\n");
    printf("#   - emhash7::HashMap  (hash_table7.hpp)\n");
    printf("#   - emhash8::HashMap  (hash_table8.hpp)\n");
    printf("#   - emilib::HashMap   (emihmap1.hpp)\n");
    printf("#   - emilib2::HashMap  (emihmap2.hpp)\n");
    printf("#   - emilib3::HashMap  (emihmap3.hpp)\n");
    printf("#\n");
    printf("# Attack scenarios:\n");
    printf("#   1. const_hasher   - all keys -> bucket 0 (worst-case)\n");
    printf("#   2. range4_hasher  - keys -> first 4 buckets (partial)\n");
    printf("#   3. linear_hasher  - keys -> contiguous buckets\n");
    printf("################################################################\n\n");

    // ========================================
    // Part 1: Correctness Tests with Different Key Counts
    // ========================================
    printf("============================================================\n");
    printf("PART 1: CORRECTNESS TESTS\n");
    printf("============================================================\n");

    for (int N : {100, 1000, 10000}) {
        printf("\n--- Testing with N=%d keys ---\n", N);
        test_all_maps_correctness<const_hasher>("const", N);
    }

    // ========================================
    // Part 2: Performance Tests with Different Key Counts
    // ========================================
    printf("\n============================================================\n");
    printf("PART 2: PERFORMANCE TESTS\n");
    printf("============================================================\n");

    for (int N : {100, 1000, 10000}) {
        printf("\n============================================================\n");
        printf("Testing with N=%d keys\n", N);
        printf("============================================================\n");

        // Constant hasher (worst-case collision)
        printf("\n===== Attack Type: const_hasher (worst-case) =====\n");
        bench_all_maps_insert<const_hasher>("const", N);
        bench_all_maps_find<const_hasher>("const", N);
        bench_all_maps_erase<const_hasher>("const", N);
        bench_all_maps_mixed<const_hasher>("const", N);

        // Range4 hasher (partial collision)
        printf("\n===== Attack Type: range4_hasher (partial collision) =====\n");
        bench_all_maps_insert<range4_hasher>("range4", N);
        bench_all_maps_find<range4_hasher>("range4", N);
        bench_all_maps_erase<range4_hasher>("range4", N);
        bench_all_maps_mixed<range4_hasher>("range4", N);

        // Linear hasher
        printf("\n===== Attack Type: linear_hasher =====\n");
        bench_all_maps_insert<linear_hasher>("linear", N);
        bench_all_maps_find<linear_hasher>("linear", N);
        bench_all_maps_erase<linear_hasher>("linear", N);
        bench_all_maps_mixed<linear_hasher>("linear", N);
    }

    // ========================================
    // Part 3: High Load Attack Test
    // ========================================
    printf("\n============================================================\n");
    printf("PART 3: HIGH LOAD ATTACK TEST\n");
    printf("============================================================\n");
    test_all_high_load(10000);

    // ========================================
    // Part 4: Stress Test (No Crash/Hang)
    // ========================================
    printf("\n============================================================\n");
    printf("PART 4: STRESS TEST (NO CRASH/HANG)\n");
    printf("============================================================\n");
    test_all_stress(10000);

    printf("\n################################################################\n");
    printf("# Hash Attack Benchmark Complete\n");
    printf("################################################################\n");

    return 0;
}