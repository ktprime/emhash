#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>
#include <chrono>

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<typename Map>
void run(const char* name, const std::vector<uint64_t>& keys, const std::vector<uint64_t>& miss_keys) {
    Map m;
    m.reserve(keys.size() * 12 / 10);

    auto t0 = now_us();
    for (auto k : keys) m.emplace(k, k);
    auto t_ins = now_us() - t0;

    volatile size_t sink = 0;
    t0 = now_us();
    for (auto k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
    auto t_hit = now_us() - t0;

    t0 = now_us();
    for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
    auto t_miss = now_us() - t0;

    t0 = now_us();
    for (auto k : keys) m.erase(k);
    auto t_era = now_us() - t0;

    printf("  %-28s %8lld %8lld %8lld %8lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

int main() {
    using K = uint64_t;
    using V = uint64_t;

    printf("=== Random int64_t key ONLY (clean run, no sequential pollution) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {1000000, 10000000}) {
        std::mt19937_64 rng(42);
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

        printf("\n  [N = %d]\n", N);

        run<emilib2::HashMap<K, V>>("emihmap2", keys, miss_keys);
        run<emilib3::HashMap<K, V>>("emihmap3", keys, miss_keys);
        run<emilib4::HashMap<K, V>>("emihmap4(swiss)", keys, miss_keys);
        run<boost::unordered_flat_map<K, V>>("boost::flat_map", keys, miss_keys);
    }

    return 0;
}
