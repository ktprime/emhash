#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <cmath>

#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

template<typename Map>
double bench_find_hit(const std::vector<int>& keys, size_t iters) {
    Map m;
    m.reserve(keys.size() * 2);
    for (auto k : keys) m[k] = k;

    size_t sum = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
        for (auto k : keys) {
            auto it = m.find(k);
            if (it != m.end()) sum += it->second;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    printf("  sum=%zu (prevent opt), %.2f ns/op\n", sum, ns / (iters * keys.size()));
    return ns;
}

template<typename Map>
double bench_find_miss(const std::vector<int>& keys, const std::vector<int>& miss_keys, size_t iters) {
    Map m;
    m.reserve(keys.size() * 2);
    for (auto k : keys) m[k] = k;

    size_t sum = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
        for (auto k : miss_keys) {
            auto it = m.find(k);
            if (it != m.end()) sum += it->second;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    printf("  misses sum=%zu, %.2f ns/op\n", sum, ns / (iters * miss_keys.size()));
    return ns;
}

template<typename Map>
double bench_insert(const std::vector<int>& keys, size_t iters) {
    size_t total = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i) {
        Map m;
        m.reserve(keys.size());
        for (auto k : keys) m[k] = k;
        total += m.size();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    printf("  total=%zu, %.2f ns/op\n", total, ns / (iters * keys.size()));
    return ns;
}

int main() {
    std::mt19937 rng(42);
    const size_t N = 100000;
    std::vector<int> keys(N);
    std::uniform_int_distribution<int> dist(1, 100000000);
    for (auto& k : keys) k = dist(rng);

    std::vector<int> miss_keys(N);
    for (auto& k : miss_keys) k = dist(rng) + 200000000;

    printf("=== emilib4::HashMap find HIT (N=%zu) ===\n", N);
    auto e4_hit = bench_find_hit<emilib4::HashMap<int, int>>(keys, 20);

    printf("=== boost::unordered_flat_map find HIT (N=%zu) ===\n", N);
    auto b_hit = bench_find_hit<boost::unordered_flat_map<int, int>>(keys, 20);

    printf("=== emilib4::HashMap find MISS (N=%zu) ===\n", N);
    auto e4_miss = bench_find_miss<emilib4::HashMap<int, int>>(keys, miss_keys, 20);

    printf("=== boost::unordered_flat_map find MISS (N=%zu) ===\n", N);
    auto b_miss = bench_find_miss<boost::unordered_flat_map<int, int>>(keys, miss_keys, 20);

    printf("=== emilib4::HashMap insert (N=%zu) ===\n", N);
    auto e4_ins = bench_insert<emilib4::HashMap<int, int>>(keys, 20);

    printf("=== boost::unordered_flat_map insert (N=%zu) ===\n", N);
    auto b_ins = bench_insert<boost::unordered_flat_map<int, int>>(keys, 20);

    printf("\n=== RATIO (emilib4/boost) ===\n");
    printf("  FindHit:   %.2fx\n", e4_hit / b_hit);
    printf("  FindMiss:  %.2fx\n", e4_miss / b_miss);
    printf("  Insert:    %.2fx\n", e4_ins / b_ins);

    return 0;
}
