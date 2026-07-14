// Benchmark: bitmask layout (front vs back) for emhash7
// Focus on large data sizes that exceed cache levels

#include "emhash/hash_table7.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
#include <string>

using namespace emhash7;

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

template <typename KeyT>
std::vector<KeyT> make_keys(size_t n, uint64_t seed = 42);

template <>
std::vector<int64_t> make_keys<int64_t>(size_t n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<int64_t> keys(n);
    for (size_t i = 0; i < n; i++) keys[i] = static_cast<int64_t>(rng());
    return keys;
}

template <>
std::vector<std::string> make_keys<std::string>(size_t n, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<std::string> keys(n);
    for (size_t i = 0; i < n; i++) {
        char buf[24];
        snprintf(buf, sizeof(buf), "key_%016llx", (unsigned long long)rng());
        keys[i] = buf;
    }
    return keys;
}

template <typename KeyT>
static std::vector<KeyT> make_miss_keys(const std::vector<KeyT>& keys, uint64_t seed = 999) {
    auto miss = make_keys<KeyT>(keys.size(), seed);
    return miss;
}

// Returns: [insert, find_hit, find_miss, erase, iterate] in ms
template <typename KeyT>
static void bench(const std::vector<KeyT>& keys, const std::vector<KeyT>& miss_keys, int rounds) {
    const size_t N = keys.size();
    double t_insert = 0, t_find_hit = 0, t_find_miss = 0, t_erase = 0, t_iterate = 0;

    for (int r = 0; r < rounds; r++) {
        HashMap<KeyT, int> map;
        map.reserve(N);

        // Insert
        auto t0 = now_ns();
        for (auto& k : keys) map[k] = 1;
        t_insert += (now_ns() - t0) / 1e6;

        // FindHit
        volatile int64_t sink = 0;
        t0 = now_ns();
        for (auto& k : keys) sink += map.find(k) != map.end() ? 1 : 0;
        t_find_hit += (now_ns() - t0) / 1e6;

        // FindMiss
        t0 = now_ns();
        for (auto& k : miss_keys) sink += map.find(k) != map.end() ? 1 : 0;
        t_find_miss += (now_ns() - t0) / 1e6;

        // Iterate
        t0 = now_ns();
        for (auto& kv : map) sink += kv.second;
        t_iterate += (now_ns() - t0) / 1e6;

        // Erase
        t0 = now_ns();
        for (auto& k : keys) map.erase(k);
        t_erase += (now_ns() - t0) / 1e6;

        (void)sink;
    }

    printf("%7zu  %8.1f %8.1f %8.1f %8.1f %8.1f\n",
        N, t_insert/rounds, t_find_hit/rounds, t_find_miss/rounds, t_erase/rounds, t_iterate/rounds);
}

int main() {
    printf("\n=== emhash7 HashMap<int64_t, int> ===\n");
    printf("%7s  %8s %8s %8s %8s %8s\n", "N", "Insert", "FindHit", "FindMiss", "Erase", "Iterate");
    printf("%7s  %8s %8s %8s %8s %8s\n", "", "(ms)", "(ms)", "(ms)", "(ms)", "(ms)");

    // int64_t: 24B/entry, L3~30MB = ~1.25M entries
    // Test: 500K (fits L3) → 2M, 5M, 10M, 20M, 50M (well beyond L3)
    {
        auto k500k = make_keys<int64_t>(500'000);
        auto m500k = make_miss_keys(k500k);
        bench<int64_t>(k500k, m500k, 10);

        auto k2m = make_keys<int64_t>(2'000'000);
        auto m2m = make_miss_keys(k2m);
        bench<int64_t>(k2m, m2m, 8);

        auto k5m = make_keys<int64_t>(5'000'000);
        auto m5m = make_miss_keys(k5m);
        bench<int64_t>(k5m, m5m, 5);

        auto k10m = make_keys<int64_t>(10'000'000);
        auto m10m = make_miss_keys(k10m);
        bench<int64_t>(k10m, m10m, 5);

        auto k20m = make_keys<int64_t>(20'000'000);
        auto m20m = make_miss_keys(k20m);
        bench<int64_t>(k20m, m20m, 3);

        auto k50m = make_keys<int64_t>(50'000'000);
        auto m50m = make_miss_keys(k50m);
        bench<int64_t>(k50m, m50m, 3);
    }

    printf("\n=== emhash7 HashMap<string, int> ===\n");
    printf("%7s  %8s %8s %8s %8s %8s\n", "N", "Insert", "FindHit", "FindMiss", "Erase", "Iterate");
    printf("%7s  %8s %8s %8s %8s %8s\n", "", "(ms)", "(ms)", "(ms)", "(ms)", "(ms)");

    {
        auto k100k = make_keys<std::string>(100'000);
        auto m100k = make_miss_keys(k100k);
        bench<std::string>(k100k, m100k, 10);

        auto k500k = make_keys<std::string>(500'000);
        auto m500k = make_miss_keys(k500k);
        bench<std::string>(k500k, m500k, 8);

        auto k1m = make_keys<std::string>(1'000'000);
        auto m1m = make_miss_keys(k1m);
        bench<std::string>(k1m, m1m, 5);

        auto k2m = make_keys<std::string>(2'000'000);
        auto m2m = make_miss_keys(k2m);
        bench<std::string>(k2m, m2m, 5);

        auto k5m = make_keys<std::string>(5'000'000);
        auto m5m = make_miss_keys(k5m);
        bench<std::string>(k5m, m5m, 3);
    }

    return 0;
}
