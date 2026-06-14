// Benchmark: emilib2o vs emilib2o_opt vs emilib2o_v2
// Compare three optimization strategies:
// 1. emilib2o: Original version
// 2. emilib2o_opt: Bit Mixer (MurmurHash3 finalizer) - previous attempt, slower
// 3. emilib2o_v2: Aligned SIMD + Fast integer hash (golden ratio)

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <random>
#include <string>

#include "emilib2o.hpp"
#include "emilib2o_opt.hpp"
#include "emilib2o_v2.hpp"

using namespace std;

// Timer helper
template<typename Func>
double measure_time(Func f, int runs = 3) {
    double best = 1e9;
    for (int i = 0; i < runs; i++) {
        auto start = chrono::high_resolution_clock::now();
        f();
        auto end = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(end - start).count();
        if (ms < best) best = ms;
    }
    return best;
}

// Test 1: Sequential integer key insertion (tests hash distribution)
void test_sequential_insert(int N) {
    printf("\n=== Test 1: Sequential Insert (N=%d) ===\n", N);

    // Original emilib2o
    double t1 = measure_time([&]() {
        emilib2::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;
    });

    // Bit Mixer version (previous attempt)
    double t2 = measure_time([&]() {
        emilib2_opt::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;
    });

    // v2: Fast integer hash + aligned SIMD
    double t3 = measure_time([&]() {
        emilib2_v2::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;
    });

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec) [Bit Mixer]\n", t2, N / t2 / 1000);
    printf("  emilib2o_v2   : %.3f ms (%.1f M ops/sec) [FastHash+Align]\n", t3, N / t3 / 1000);
    printf("  v2 vs original: %.2fx (%.1f%% improvement)\n", t1 / t3, (1 - t3/t1) * 100);
}

// Test 2: Random key insertion (normal case)
void test_random_insert(int N) {
    printf("\n=== Test 2: Random Insert (N=%d) ===\n", N);

    vector<int> keys(N);
    mt19937 rng(42);
    for (int i = 0; i < N; i++)
        keys[i] = rng();

    double t1 = measure_time([&]() {
        emilib2::HashMap<int, int> map;
        for (int k : keys)
            map[k] = k;
    });

    double t2 = measure_time([&]() {
        emilib2_opt::HashMap<int, int> map;
        for (int k : keys)
            map[k] = k;
    });

    double t3 = measure_time([&]() {
        emilib2_v2::HashMap<int, int> map;
        for (int k : keys)
            map[k] = k;
    });

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec)\n", t2, N / t2 / 1000);
    printf("  emilib2o_v2   : %.3f ms (%.1f M ops/sec)\n", t3, N / t3 / 1000);
    printf("  v2 vs original: %.2fx\n", t1 / t3);
}

// Test 3: Sequential key lookup
void test_sequential_lookup(int N) {
    printf("\n=== Test 3: Sequential Lookup (N=%d) ===\n", N);

    // Build maps first
    emilib2::HashMap<int, int> map1;
    emilib2_opt::HashMap<int, int> map2;
    emilib2_v2::HashMap<int, int> map3;
    for (int i = 0; i < N; i++) {
        map1[i] = i;
        map2[i] = i;
        map3[i] = i;
    }

    // Lookup
    double t1 = measure_time([&]() {
        volatile int sum = 0;
        for (int i = 0; i < N; i++)
            sum += map1.at(i);
    });

    double t2 = measure_time([&]() {
        volatile int sum = 0;
        for (int i = 0; i < N; i++)
            sum += map2.at(i);
    });

    double t3 = measure_time([&]() {
        volatile int sum = 0;
        for (int i = 0; i < N; i++)
            sum += map3.at(i);
    });

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec)\n", t2, N / t2 / 1000);
    printf("  emilib2o_v2   : %.3f ms (%.1f M ops/sec)\n", t3, N / t3 / 1000);
    printf("  v2 vs original: %.2fx\n", t1 / t3);
}

// Test 4: Random lookup
void test_random_lookup(int N) {
    printf("\n=== Test 4: Random Lookup (N=%d) ===\n", N);

    vector<int> keys(N);
    mt19937 rng(42);
    for (int i = 0; i < N; i++)
        keys[i] = i;
    shuffle(keys.begin(), keys.end(), rng);

    // Build maps
    emilib2::HashMap<int, int> map1;
    emilib2_opt::HashMap<int, int> map2;
    emilib2_v2::HashMap<int, int> map3;
    for (int i = 0; i < N; i++) {
        map1[i] = i;
        map2[i] = i;
        map3[i] = i;
    }

    double t1 = measure_time([&]() {
        volatile int sum = 0;
        for (int k : keys)
            sum += map1.at(k);
    });

    double t2 = measure_time([&]() {
        volatile int sum = 0;
        for (int k : keys)
            sum += map2.at(k);
    });

    double t3 = measure_time([&]() {
        volatile int sum = 0;
        for (int k : keys)
            sum += map3.at(k);
    });

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec)\n", t2, N / t2 / 1000);
    printf("  emilib2o_v2   : %.3f ms (%.1f M ops/sec)\n", t3, N / t3 / 1000);
    printf("  v2 vs original: %.2fx\n", t1 / t3);
}

// Test 5: Mixed operations (insert + lookup + erase)
void test_mixed_ops(int N) {
    printf("\n=== Test 5: Mixed Operations (N=%d) ===\n", N);

    double t1 = measure_time([&]() {
        emilib2::HashMap<int, int> map;
        for (int i = 0; i < N; i++) map[i] = i;
        for (int i = 0; i < N/2; i++) map.erase(i);
        for (int i = N/2; i < N; i++) map.at(i);
    });

    double t2 = measure_time([&]() {
        emilib2_opt::HashMap<int, int> map;
        for (int i = 0; i < N; i++) map[i] = i;
        for (int i = 0; i < N/2; i++) map.erase(i);
        for (int i = N/2; i < N; i++) map.at(i);
    });

    double t3 = measure_time([&]() {
        emilib2_v2::HashMap<int, int> map;
        for (int i = 0; i < N; i++) map[i] = i;
        for (int i = 0; i < N/2; i++) map.erase(i);
        for (int i = N/2; i < N; i++) map.at(i);
    });

    printf("  emilib2o      : %.3f ms\n", t1);
    printf("  emilib2o_opt  : %.3f ms\n", t2);
    printf("  emilib2o_v2   : %.3f ms\n", t3);
    printf("  v2 vs original: %.2fx\n", t1 / t3);
}

// Test 6: String keys (non-integral type - should use std::hash)
void test_string_keys(int N) {
    printf("\n=== Test 6: String Keys (N=%d) ===\n", N);

    vector<string> keys(N);
    for (int i = 0; i < N; i++)
        keys[i] = "key_" + to_string(i);

    double t1 = measure_time([&]() {
        emilib2::HashMap<string, int> map;
        for (int i = 0; i < N; i++)
            map[keys[i]] = i;
    });

    double t2 = measure_time([&]() {
        emilib2_opt::HashMap<string, int> map;
        for (int i = 0; i < N; i++)
            map[keys[i]] = i;
    });

    double t3 = measure_time([&]() {
        emilib2_v2::HashMap<string, int> map;
        for (int i = 0; i < N; i++)
            map[keys[i]] = i;
    });

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec)\n", t2, N / t2 / 1000);
    printf("  emilib2o_v2   : %.3f ms (%.1f M ops/sec)\n", t3, N / t3 / 1000);
    printf("  v2 vs original: %.2fx\n", t1 / t3);
}

// Test 7: Correctness verification
void test_correctness() {
    printf("\n=== Test 7: Correctness Verification ===\n");

    const int N = 10000;
    bool pass = true;

    // Test v2 version
    {
        emilib2_v2::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i * 10;

        for (int i = 0; i < N; i++) {
            auto it = map.find(i);
            if (it == map.end() || it->second != i * 10) {
                printf("  FAIL: key %d not found or wrong value\n", i);
                pass = false;
                break;
            }
        }
    }

    // Test erase
    {
        emilib2_v2::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;

        for (int i = 0; i < N/2; i++)
            map.erase(i);

        if (map.size() != N/2) {
            printf("  FAIL: size mismatch after erase (expected %d, got %d)\n", N/2, (int)map.size());
            pass = false;
        }

        for (int i = 0; i < N/2; i++) {
            if (map.contains(i)) {
                printf("  FAIL: erased key %d still exists\n", i);
                pass = false;
                break;
            }
        }
    }

    // Test rehash
    {
        emilib2_v2::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;

        map.rehash(N * 4);

        for (int i = 0; i < N; i++) {
            if (!map.contains(i) || map.at(i) != i) {
                printf("  FAIL: key %d lost after rehash\n", i);
                pass = false;
                break;
            }
        }
    }

    // Test collision handling
    {
        emilib2_v2::HashMap<int, int> map;
        // Insert keys that may collide (powers of 2, but limited to avoid overflow)
        for (int i = 0; i < 30; i++)  // Only up to 30 to avoid int overflow
            map[1 << i] = i;

        for (int i = 0; i < 30; i++) {
            if (!map.contains(1 << i) || map.at(1 << i) != i) {
                printf("  FAIL: collision key %d not found\n", 1 << i);
                pass = false;
                break;
            }
        }
    }

    printf("  Result: %s\n", pass ? "PASS" : "FAIL");
}

int main() {
    printf("========================================\n");
    printf("emilib2o Optimization Comparison\n");
    printf("========================================\n");
    printf("Versions:\n");
    printf("  emilib2o      : Original\n");
    printf("  emilib2o_opt  : Bit Mixer (MurmurHash3)\n");
    printf("  emilib2o_v2   : FastHash + Aligned SIMD\n");
    printf("========================================\n");

    // Correctness first
    test_correctness();

    // Performance tests with different sizes
    test_sequential_insert(100000);
    test_sequential_insert(1000000);

    test_random_insert(100000);
    test_random_insert(1000000);

    test_sequential_lookup(100000);
    test_sequential_lookup(1000000);

    test_random_lookup(100000);

    test_mixed_ops(100000);

    test_string_keys(10000);

    printf("\n========================================\n");
    printf("Summary:\n");
    printf("v2 optimizations:\n");
    printf("  1. Fast integer hash (golden ratio multiplication)\n");
    printf("  2. Aligned memory allocation for SIMD\n");
    printf("  3. Reduced prefetch overhead\n");
    printf("========================================\n");

    return 0;
}
