// Hash Attack Benchmark for emhash5
// Simulates adversarial hashing scenarios:
//   1. Constant hash: all keys map to bucket 0
//   2. Small-range hash: keys map to N different buckets (controlled attack)
//   3. Find/erase on already-attacked map
//
// Build: g++ -O2 -std=c++17 -o hash_attack hash_attack.cpp
//
#include "emhash/hash_table5.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <random>
#include <string>

// 1. Constant hasher: all keys → bucket 0
struct const_hasher {
    size_t operator()(int /*k*/) const { return 0; }
    bool operator()(int a, int b) const { return a == b; }
};

// 2. Small-range hasher: keys → first 4 buckets only
struct range4_hasher {
    size_t operator()(int k) const { return (size_t)(k & 3); }
};

// 3. Linear hasher: keys → contiguous buckets (forms long chain in emhash5)
struct linear_hasher {
    size_t operator()(int k) const { return (size_t)k; }
};

static double now_ms() {
    return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

// ---------------------------------------------------------------------------
// Correctness test: under attack, all operations must be correct
// ---------------------------------------------------------------------------
static int test_attack_correctness_const() {
    printf("=== Correctness: constant hash (all keys → bucket 0) ===\n");
    emhash5::HashMap<int, int, const_hasher> m(8);
    std::unordered_map<int, int, const_hasher> ref(8);

    const int N = 100000;

    // Insert
    for (int i = 0; i < N; i++) {
        m[i] = i;
        ref[i] = i;
    }
    int fail = 0;
    if (m.size() != (size_t)N) {
        printf("  FAIL: size %zu != %d\n", (size_t)m.size(), N);
        fail++;
    }

    // Find all
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        auto refit = ref.find(i);
        if (it == m.end() || it->second != i) {
            printf("  FAIL: find(%d)\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
        if (refit == ref.end() || refit->second != i) {
            printf("  FAIL: ref find(%d)\n", i);
            fail++;
        }
    }

    // Erase half
    for (int i = 0; i < N; i += 2) {
        m.erase(i);
        ref.erase(i);
    }
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        auto refit = ref.find(i);
        if ((it == m.end()) != (refit == ref.end())) {
            printf("  FAIL: erase mismatch at %d\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
        if (it != m.end() && it->second != refit->second) {
            printf("  FAIL: post-erase value mismatch at %d\n", i);
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
            printf("  FAIL: reinsert %d\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
    }
    printf("  %s: N=%d, all operations correct (%d failures)\n", fail == 0 ? "PASS" : "FAIL", N, fail);
    return fail;
}

static int test_attack_correctness_range4() {
    printf("\n=== Correctness: range-4 hash (keys → 4 buckets) ===\n");
    emhash5::HashMap<int, int, range4_hasher> m(8);
    std::unordered_map<int, int, range4_hasher> ref(8);

    const int N = 100000;
    for (int i = 0; i < N; i++) {
        m[i] = i;
        ref[i] = i;
    }
    int fail = 0;
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i) {
            printf("  FAIL: find(%d) under range-4\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
    }
    printf("  %s: N=%d, all operations correct\n", fail == 0 ? "PASS" : "FAIL", N);
    return fail;
}

static int test_attack_correctness_linear() {
    printf("\n=== Correctness: linear hash (key == hash) ===\n");
    emhash5::HashMap<int, int, linear_hasher> m(8);
    std::unordered_map<int, int, linear_hasher> ref(8);

    const int N = 100000;
    for (int i = 0; i < N; i++) {
        m[i] = i;
        ref[i] = i;
    }
    int fail = 0;
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i) {
            printf("  FAIL: find(%d) under linear\n", i);
            fail++;
            if (fail > 5)
                return fail;
        }
    }
    printf("  %s: N=%d, all operations correct\n", fail == 0 ? "PASS" : "FAIL", N);
    return fail;
}

// ---------------------------------------------------------------------------
// Performance benchmarks
// ---------------------------------------------------------------------------
template <typename Hash> static double bench_insert_const(int N, const char* name) {
    emhash5::HashMap<int, int, Hash> m(8);
    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        m[i] = i;
    auto t1 = now_ms();
    printf("  %-30s insert N=%-7d → %.1f ms (%.0f ops/ms), bucket_count=%zu, LF=%.2f\n", name, N, t1 - t0,
           N / (t1 - t0), (size_t)m.bucket_count(), m.load_factor());
    return t1 - t0;
}

template <typename Hash> static double bench_find_const(int N, const char* name) {
    emhash5::HashMap<int, int, Hash> m(8);
    for (int i = 0; i < N; i++)
        m[i] = i;
    auto t0 = now_ms();
    volatile int sink = 0;
    for (int round = 0; round < 10; round++)
        for (int i = 0; i < N; i++)
            sink += m[i];
    (void)sink;
    auto t1 = now_ms();
    double ops = (double)N * 10;
    printf("  %-30s find   N=%-7d → %.1f ms (%.0f ops/ms)\n", name, N, t1 - t0, ops / (t1 - t0));
    return t1 - t0;
}

template <typename Hash> static double bench_erase_const(int N, const char* name) {
    emhash5::HashMap<int, int, Hash> m(8);
    for (int i = 0; i < N; i++)
        m[i] = i;
    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        m.erase(i);
    auto t1 = now_ms();
    printf("  %-30s erase  N=%-7d → %.1f ms (%.0f ops/ms)\n", name, N, t1 - t0, N / (t1 - t0));
    return t1 - t0;
}

template <typename Hash> static double bench_mixed_const(int N, const char* name) {
    emhash5::HashMap<int, int, Hash> m(8);
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
            m.find(k); // 30% find
        else
            m.erase(k); // 20% erase
    }
    auto t1 = now_ms();
    printf("  %-30s mixed  N=%-7d → %.1f ms (%.0f ops/ms)\n", name, N, t1 - t0, N / (t1 - t0));
    return t1 - t0;
}

int main() {
    printf("################################################################\n");
    printf("# Hash Attack Benchmark for emhash5\n");
    printf("# Attack scenarios: const hash / range-4 hash / linear hash\n");
    printf("################################################################\n\n");

    // === Correctness tests ===
    int total_fail = 0;
    total_fail += test_attack_correctness_const();
    total_fail += test_attack_correctness_range4();
    total_fail += test_attack_correctness_linear();

    // === Performance tests at N=10000 ===
    printf("\n=== Performance: N=10000, 8-bucket initial ===\n\n");

    printf("--- emhash5 const_hasher ---\n");
    bench_insert_const<const_hasher>(10000, "const");
    bench_insert_const<range4_hasher>(10000, "range4");
    bench_insert_const<linear_hasher>(10000, "linear");

    printf("\n--- emhash5 find (10 rounds) ---\n");
    bench_find_const<const_hasher>(10000, "const");
    bench_find_const<range4_hasher>(10000, "range4");
    bench_find_const<linear_hasher>(10000, "linear");

    printf("\n--- emhash5 erase ---\n");
    bench_erase_const<const_hasher>(10000, "const");
    bench_erase_const<range4_hasher>(10000, "range4");
    bench_erase_const<linear_hasher>(10000, "linear");

    printf("\n--- emhash5 mixed (insert/find/erase) ---\n");
    bench_mixed_const<const_hasher>(10000, "const");
    bench_mixed_const<range4_hasher>(10000, "range4");
    bench_mixed_const<linear_hasher>(10000, "linear");

    // === Comparison with std::unordered_map ===
    printf("\n=== Comparison: emhash5 vs std::unordered_map (const hash) ===\n");
    {
        const int N = 10000;
        emhash5::HashMap<int, int, const_hasher> m(8);
        for (int i = 0; i < N; i++)
            m[i] = i;
        std::unordered_map<int, int, const_hasher> ref(8);
        for (int i = 0; i < N; i++)
            ref[i] = i;

        auto t0 = now_ms();
        for (int i = 0; i < N; i++)
            m.find(i);
        auto t1 = now_ms();
        auto t2 = now_ms();
        for (int i = 0; i < N; i++)
            ref.find(i);
        auto t3 = now_ms();
        printf("  const find:    emhash5=%.1f ms  std=%.1f ms  ratio=%.2fx\n", t1 - t0, t3 - t2,
               (t1 - t0) / (t3 - t2 + 1e-6));
    }

    // === Long chain (high load) ===
    printf("\n=== High load + attack: N=100K, MLF=0.99 ===\n");
    {
        emhash5::HashMap<int, int, const_hasher> m(1024, 0.99f);
        auto t0 = now_ms();
        for (int i = 0; i < 100000; i++)
            m[i] = i;
        auto t1 = now_ms();
        printf("  insert N=100K: %.1f ms (%.0f ops/ms), bucket_count=%zu, LF=%.2f\n", t1 - t0, 100000.0 / (t1 - t0),
               (size_t)m.bucket_count(), m.load_factor());
    }

    printf("\n################################################################\n");
    printf("# Total correctness failures: %d\n", total_fail);
    printf("################################################################\n");
    return total_fail > 0 ? 1 : 0;
}
