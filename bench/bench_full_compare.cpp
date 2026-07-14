/**
 * Comprehensive benchmark: emihmap2 vs emihmap3 vs emihmap4(swiss) vs boost::flat_map
 * Key types: int64_t (random, sequential), string
 * Operations: Insert, FindHit, FindMiss, Erase
 */

#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <random>

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<typename Map, typename Keys, typename MissKeys>
void run_bench(const char* name, const Keys& keys, const MissKeys& miss_keys) {
    Map m;
    m.reserve(keys.size() * 12 / 10);

    auto t0 = now_us();
    for (auto& k : keys) m[k] = 1;
    auto t_ins = now_us() - t0;

    volatile size_t sink = 0;
    t0 = now_us();
    for (auto& k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
    auto t_hit = now_us() - t0;

    t0 = now_us();
    for (auto& k : miss_keys) { if (m.find(k) == m.end()) sink++; }
    auto t_miss = now_us() - t0;

    t0 = now_us();
    for (auto& k : keys) m.erase(k);
    auto t_era = now_us() - t0;

    printf("  %-28s %8lld %8lld %8lld %8lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

int main() {
    using K = uint64_t;
    using V = uint64_t;
    using SK = std::string;

    printf("=====================================================================\n");
    printf("  emihmap2 vs emihmap3 vs emihmap4(swiss) vs boost::flat_map\n");
    printf("=====================================================================\n");

    // ─── Random int64_t ─────────────────────────────────────────────
    printf("\n=== Random int64_t key (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000, 10000000}) {
        std::mt19937_64 rng(42);
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

        printf("\n  [N = %d]\n", N);
        run_bench<emilib2::HashMap<K, V>>("emihmap2", keys, miss_keys);
        run_bench<emilib3::HashMap<K, V>>("emihmap3", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V>>("emihmap4(swiss)", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, V>>("boost::flat_map", keys, miss_keys);
    }

    // ─── Sequential int64_t ────────────────────────────────────────
    printf("\n\n=== Sequential int64_t key (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000, 10000000}) {
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = N + i + 1; }

        printf("\n  [N = %d]\n", N);
        run_bench<emilib2::HashMap<K, V>>("emihmap2", keys, miss_keys);
        run_bench<emilib3::HashMap<K, V>>("emihmap3", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V>>("emihmap4(swiss)", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, V>>("boost::flat_map", keys, miss_keys);
    }

    // ─── String key ────────────────────────────────────────────────
    printf("\n\n=== String key (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000}) {
        std::vector<std::string> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = "key_" + std::to_string(i + 1);
            miss_keys[i] = "miss_" + std::to_string(i + 1);
        }

        printf("\n  [N = %d]\n", N);
        run_bench<emilib2::HashMap<SK, V>>("emihmap2", keys, miss_keys);
        run_bench<emilib3::HashMap<SK, V>>("emihmap3", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V>>("emihmap4(swiss)", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, V>>("boost::flat_map", keys, miss_keys);
    }

    return 0;
}
