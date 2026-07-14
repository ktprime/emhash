/**
 * Quick correctness + focused random-key benchmark for emihmap4
 */

#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>
#include <random>

int main() {
    using Map = emilib4::HashMap<int64_t, int64_t>;

    // ─── Correctness tests ────────────────────────────────────────────

    printf("=== Correctness tests ===\n");

    // Basic CRUD
    Map m;
    assert(m.empty());
    assert(m.size() == 0);

    m[1] = 10;
    m[2] = 20;
    m[3] = 30;
    assert(m.size() == 3);
    assert(m.contains(1));
    assert(m.contains(2));
    assert(m.contains(3));
    assert(!m.contains(4));
    assert(m[1] == 10);
    assert(m[2] == 20);
    assert(m[3] == 30);

    // Overwrite
    m[1] = 100;
    assert(m[1] == 100);

    // Erase
    assert(m.erase(2) == 1);
    assert(m.size() == 2);
    assert(!m.contains(2));
    assert(m.erase(999) == 0);

    // at()
    assert(m.at(1) == 100);
    assert(m.at(3) == 30);
    bool threw = false;
    try { m.at(999); } catch (std::out_of_range&) { threw = true; }
    assert(threw);

    // Iteration
    int count = 0;
    for (auto& [k, v] : m) { (void)k; (void)v; count++; }
    assert(count == 2);

    // Clear
    m.clear();
    assert(m.empty());
    assert(m.size() == 0);

    // Large insert + find + erase
    Map m2;
    const int N = 100000;
    for (int i = 0; i < N; i++) m2[i] = i * 10;
    assert(m2.size() == N);
    for (int i = 0; i < N; i++) {
        auto it = m2.find(i);
        assert(it != m2.end());
        assert(it->second == i * 10);
    }
    for (int i = 0; i < N; i++) assert(m2.erase(i) == 1);
    assert(m2.empty());

    // String keys
    emilib4::HashMap<std::string, int> ms;
    ms["hello"] = 1;
    ms["world"] = 2;
    assert(ms.size() == 2);
    assert(ms["hello"] == 1);
    assert(ms.find("hello") != ms.end());
    ms.erase("hello");
    assert(!ms.contains("hello"));

    // Copy/move
    Map m3;
    for (int i = 0; i < 1000; i++) m3[i] = i;
    Map m4 = m3;
    assert(m4.size() == 1000);
    assert(m4[500] == 500);
    Map m5 = std::move(m3);
    assert(m5.size() == 1000);
    assert(m5[500] == 500);

    // Insert many random
    Map m6;
    std::mt19937_64 rng(42);
    std::vector<int64_t> keys;
    const int M = 500000;
    for (int i = 0; i < M; i++) {
        auto k = static_cast<int64_t>(rng());
        keys.push_back(k);
        m6[k] = k;
    }
    assert(m6.size() == M);
    for (auto k : keys) {
        auto it = m6.find(k);
        assert(it != m6.end());
        assert(it->second == k);
    }

    printf("  All correctness tests passed!\n");

    // ─── Focused FindMiss benchmark ───────────────────────────────────

    printf("\n=== Focused FindMiss benchmark (random int64, 1M elements, us) ===\n");

    auto now_us = [] {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    };

    Map fm;
    std::mt19937_64 rng2(123);
    std::vector<int64_t> in_keys(1000000), out_keys(1000000);
    for (int i = 0; i < 1000000; i++) {
        in_keys[i] = static_cast<int64_t>(rng2());
        out_keys[i] = static_cast<int64_t>(rng2());
    }
    for (auto k : in_keys) fm[k] = k;

    // FindHit
    volatile size_t sink = 0;
    auto t0 = now_us();
    for (auto k : in_keys) { auto it = fm.find(k); if (it != fm.end()) sink += it->second; }
    auto t_hit = now_us() - t0;

    // FindMiss
    t0 = now_us();
    for (auto k : out_keys) { if (fm.find(k) == fm.end()) sink++; }
    auto t_miss = now_us() - t0;

    // Erase half, then find
    for (int i = 0; i < 500000; i++) fm.erase(in_keys[i]);
    t0 = now_us();
    for (int i = 500000; i < 1000000; i++) { auto it = fm.find(in_keys[i]); if (it != fm.end()) sink += it->second; }
    auto t_hit_half = now_us() - t0;

    (void)sink;

    printf("  FindHit(full):  %lld us\n", (long long)t_hit);
    printf("  FindMiss:       %lld us\n", (long long)t_miss);
    printf("  FindHit(half):  %lld us\n", (long long)t_hit_half);
    printf("  Load factor:    %.2f (full), %.2f (half)\n",
           1000000.0f / fm.bucket_count(),
           500000.0f / fm.bucket_count());

    return 0;
}
