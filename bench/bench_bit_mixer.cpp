// Benchmark: emilib2o vs emilib2o_opt (Bit Mixer optimization)
// Tests sequential key insertion performance (RAW dependency chain scenario)

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <random>

#include "emilib2o.hpp"
#include "emilib2o_opt.hpp"

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

// Test 1: Sequential integer key insertion (worst case for RAW dependency)
void test_sequential_insert(int N) {
    printf("\n=== Test 1: Sequential Insert (N=%d) ===\n", N);

    // Original emilib2o
    double t1 = measure_time([&]() {
        emilib2::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;
    });

    // Optimized emilib2o_opt (with Bit Mixer)
    double t2 = measure_time([&]() {
        emilib2_opt::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;
    });

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec)\n", t2, N / t2 / 1000);
    printf("  Improvement   : %.2fx faster\n", t1 / t2);
}

// Test 2: Random key insertion (normal case)
void test_random_insert(int N) {
    printf("\n=== Test 2: Random Insert (N=%d) ===\n", N);

    vector<int> keys(N);
    mt19937 rng(42);
    for (int i = 0; i < N; i++)
        keys[i] = rng();

    // Original
    double t1 = measure_time([&]() {
        emilib2::HashMap<int, int> map;
        for (int k : keys)
            map[k] = k;
    });

    // Optimized
    double t2 = measure_time([&]() {
        emilib2_opt::HashMap<int, int> map;
        for (int k : keys)
            map[k] = k;
    });

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec)\n", t2, N / t2 / 1000);
    printf("  Difference    : %.2fx\n", t1 / t2);
}

// Test 3: Sequential key lookup
void test_sequential_lookup(int N) {
    printf("\n=== Test 3: Sequential Lookup (N=%d) ===\n", N);

    // Build maps first
    emilib2::HashMap<int, int> map1;
    emilib2_opt::HashMap<int, int> map2;
    for (int i = 0; i < N; i++) {
        map1[i] = i;
        map2[i] = i;
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

    printf("  emilib2o      : %.3f ms (%.1f M ops/sec)\n", t1, N / t1 / 1000);
    printf("  emilib2o_opt  : %.3f ms (%.1f M ops/sec)\n", t2, N / t2 / 1000);
    printf("  Difference    : %.2fx\n", t1 / t2);
}

// Test 4: Mixed operations (insert + lookup + erase)
void test_mixed_ops(int N) {
    printf("\n=== Test 4: Mixed Operations (N=%d) ===\n", N);

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

    printf("  emilib2o      : %.3f ms\n", t1);
    printf("  emilib2o_opt  : %.3f ms\n", t2);
    printf("  Difference    : %.2fx\n", t1 / t2);
}

// Test 5: Correctness verification
void test_correctness() {
    printf("\n=== Test 5: Correctness Verification ===\n");

    const int N = 10000;
    bool pass = true;

    // Test insert + find
    {
        emilib2_opt::HashMap<int, int> map;
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
        emilib2_opt::HashMap<int, int> map;
        for (int i = 0; i < N; i++)
            map[i] = i;

        for (int i = 0; i < N/2; i++)
            map.erase(i);

        if (map.size() != N/2) {
            printf("  FAIL: size mismatch after erase\n");
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
        emilib2_opt::HashMap<int, int> map;
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

    printf("  Result: %s\n", pass ? "PASS" : "FAIL");
}

int main() {
    printf("========================================\n");
    printf("emilib2o vs emilib2o_opt Benchmark\n");
    printf("Bit Mixer optimization test\n");
    printf("========================================\n");

    // Correctness first
    test_correctness();

    // Performance tests with different sizes
    test_sequential_insert(100000);
    test_sequential_insert(1000000);

    test_random_insert(100000);
    test_random_insert(1000000);

    test_sequential_lookup(100000);

    test_mixed_ops(100000);

    printf("\n========================================\n");
    printf("Summary:\n");
    printf("Bit Mixer breaks RAW dependency chain\n");
    printf("by scattering sequential keys across table.\n");
    printf("Expected: 10-50x faster for sequential keys.\n");
    printf("========================================\n");

    return 0;
}
