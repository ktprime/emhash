// Hash Attack Benchmark for emhash7
// emhash7 uses Swiss Table style with 8-bit metadata
// Compare its robustness under hash attack
//
#include "../hash_table7.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <random>

// 1. Constant hasher
struct const_hasher {
    size_t operator()(int /*k*/) const { return 0; }
};

// 2. Small-range hasher
struct range4_hasher {
    size_t operator()(int k) const { return (size_t)(k & 3); }
};

// 3. Linear hasher
struct linear_hasher {
    size_t operator()(int k) const { return (size_t)k; }
};

static double now_ms() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// ---------------------------------------------------------------------------
// Correctness tests
// ---------------------------------------------------------------------------
static int test_attack_correctness_const()
{
    printf("=== Correctness: constant hash (all keys 鈫?bucket 0) ===\n");
    emhash7::HashMap<int, int, const_hasher> m(8);
    std::unordered_map<int, int, const_hasher> ref(8);

    const int N = 100000;
    for (int i = 0; i < N; i++) {
        m[i] = i;
        ref[i] = i;
    }
    int fail = 0;
    if (m.size() != (size_t)N) { printf("  FAIL: size %zu != %d\n", m.size(), N); fail++; }

    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        auto refit = ref.find(i);
        if (it == m.end() || it->second != i) {
            printf("  FAIL: find(%d)\n", i);
            fail++; if (fail > 5) return fail;
        }
        if (refit == ref.end() || refit->second != i) {
            printf("  FAIL: ref find(%d)\n", i);
            fail++;
        }
    }

    for (int i = 0; i < N; i += 2) {
        m.erase(i);
        ref.erase(i);
    }
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        auto refit = ref.find(i);
        if ((it == m.end()) != (refit == ref.end())) {
            printf("  FAIL: erase mismatch at %d\n", i);
            fail++; if (fail > 5) return fail;
        }
        if (it != m.end() && it->second != refit->second) {
            printf("  FAIL: post-erase value mismatch at %d\n", i);
            fail++; if (fail > 5) return fail;
        }
    }

    for (int i = 0; i < N; i += 2) m[i] = i;
    for (int i = 0; i < N; i++) {
        if (m[i] != i) { printf("  FAIL: reinsert %d\n", i); fail++; if (fail > 5) return fail; }
    }
    printf("  %s: N=%d, all operations correct (%d failures)\n", fail == 0 ? "PASS" : "FAIL", N, fail);
    return fail;
}

static int test_attack_correctness_range4()
{
    printf("\n=== Correctness: range-4 hash (keys 鈫?4 buckets) ===\n");
    emhash7::HashMap<int, int, range4_hasher> m(8);
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
            fail++; if (fail > 5) return fail;
        }
    }
    printf("  %s: N=%d, all operations correct\n", fail == 0 ? "PASS" : "FAIL", N);
    return fail;
}

static int test_attack_correctness_linear()
{
    printf("\n=== Correctness: linear hash (key == hash) ===\n");
    emhash7::HashMap<int, int, linear_hasher> m(8);
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
            fail++; if (fail > 5) return fail;
        }
    }
    printf("  %s: N=%d, all operations correct\n", fail == 0 ? "PASS" : "FAIL", N);
    return fail;
}

// ---------------------------------------------------------------------------
// Performance benchmarks
// ---------------------------------------------------------------------------
template <typename Hash>
static double bench_insert(int N, const char* name)
{
    emhash7::HashMap<int, int, Hash> m(8);
    auto t0 = now_ms();
    for (int i = 0; i < N; i++) m[i] = i;
    auto t1 = now_ms();
    printf("  %-30s insert N=%-7d 鈫?%.1f ms (%.0f ops/ms)\n",
        name, N, t1 - t0, N / (t1 - t0));
    return t1 - t0;
}

template <typename Hash>
static double bench_find(int N, const char* name)
{
    emhash7::HashMap<int, int, Hash> m(8);
    for (int i = 0; i < N; i++) m[i] = i;
    auto t0 = now_ms();
    volatile int sink = 0;
    for (int round = 0; round < 10; round++)
        for (int i = 0; i < N; i++)
            sink += m.find(i) != m.end() ? 1 : 0;
    auto t1 = now_ms();
    double ops = (double)N * 10;
    printf("  %-30s find   N=%-7d 鈫?%.1f ms (%.0f ops/ms)\n",
        name, N, t1 - t0, ops / (t1 - t0));
    return t1 - t0;
}

template <typename Hash>
static double bench_erase(int N, const char* name)
{
    emhash7::HashMap<int, int, Hash> m(8);
    for (int i = 0; i < N; i++) m[i] = i;
    auto t0 = now_ms();
    for (int i = 0; i < N; i++) m.erase(i);
    auto t1 = now_ms();
    printf("  %-30s erase  N=%-7d 鈫?%.1f ms (%.0f ops/ms)\n",
        name, N, t1 - t0, N / (t1 - t0));
    return t1 - t0;
}

template <typename Hash>
static double bench_mixed(int N, const char* name)
{
    emhash7::HashMap<int, int, Hash> m(8);
    for (int i = 0; i < N; i++) m[i] = i;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> opd(0, 9);
    std::uniform_int_distribution<int> keyd(0, N - 1);

    auto t0 = now_ms();
    for (int i = 0; i < N; i++) {
        int op = opd(rng);
        int k = keyd(rng);
        if (op < 5) m[k] = k;
        else if (op < 8) m.find(k);
        else m.erase(k);
    }
    auto t1 = now_ms();
    printf("  %-30s mixed  N=%-7d 鈫?%.1f ms (%.0f ops/ms)\n",
        name, N, t1 - t0, N / (t1 - t0));
    return t1 - t0;
}

int main()
{
    printf("################################################################\n");
    printf("# Hash Attack Benchmark for emhash7 (Swiss Table style)\n");
    printf("# Attack scenarios: const hash / range-4 hash / linear hash\n");
    printf("################################################################\n\n");

    int total_fail = 0;
    total_fail += test_attack_correctness_const();
    total_fail += test_attack_correctness_range4();
    total_fail += test_attack_correctness_linear();

    printf("\n=== Performance: N=10000, 8-bucket initial ===\n\n");

    printf("--- emhash7 insert ---\n");
    bench_insert<const_hasher>(10000, "const");
    bench_insert<range4_hasher>(10000, "range4");
    bench_insert<linear_hasher>(10000, "linear");

    printf("\n--- emhash7 find (10 rounds) ---\n");
    bench_find<const_hasher>(10000, "const");
    bench_find<range4_hasher>(10000, "range4");
    bench_find<linear_hasher>(10000, "linear");

    printf("\n--- emhash7 erase ---\n");
    bench_erase<const_hasher>(10000, "const");
    bench_erase<range4_hasher>(10000, "range4");
    bench_erase<linear_hasher>(10000, "linear");

    printf("\n--- emhash7 mixed ---\n");
    bench_mixed<const_hasher>(10000, "const");
    bench_mixed<range4_hasher>(10000, "range4");
    bench_mixed<linear_hasher>(10000, "linear");

    // High load + attack
    printf("\n=== High load + attack: N=100K, MLF=0.99 ===\n");
    {
        emhash7::HashMap<int, int, const_hasher> m(1024, 0.99f);
        auto t0 = now_ms();
        for (int i = 0; i < 100000; i++) m[i] = i;
        auto t1 = now_ms();
        printf("  insert N=100K (const+high_load): %.1f ms (%.0f ops/ms)\n",
            t1 - t0, 100000.0 / (t1 - t0));
    }

    printf("\n################################################################\n");
    printf("# Total correctness failures: %d\n", total_fail);
    printf("################################################################\n");
    return total_fail > 0 ? 1 : 0;
}
