/**
 * Fair benchmark: all implementations use the same wyhash function.
 * Tests: int64_t (random + sequential) and string keys.
 */
#include "emhash/config.hpp"
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

// Unified hash functors using wyhash
struct WyHashI64 {
    size_t operator()(uint64_t key) const noexcept {
        return emh_wyhash64(key, 0);
    }
};
struct WyHashStr {
    size_t operator()(const std::string& key) const noexcept {
        return emh_wyhash(key.data(), key.size(), 0);
    }
};

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

    printf("  %-30s %8lld %8lld %8lld %8lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

int main() {
    using K = uint64_t;
    using V = uint64_t;

    printf("========================================================================\n");
    printf("  Fair benchmark (all using wyhash) - us\n");
    printf("========================================================================\n");

    // ─── Random int64_t ─────────────────────────────────────────────
    printf("\n=== Random int64_t key (wyhash) ===\n");
    printf("  %-30s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000, 10000000}) {
        std::mt19937_64 rng(42);
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

        printf("\n  [N = %d]\n", N);
        using M2 = emilib2::HashMap<K, V, WyHashI64>;
        using M3 = emilib3::HashMap<K, V, WyHashI64>;
        using M4 = emilib4::HashMap<K, V, WyHashI64>;
        using MB = boost::unordered_flat_map<K, V, WyHashI64>;
        // Reversed order: boost first, emihmap2 last
        run_bench<MB>("boost::flat_map", keys, miss_keys);
        run_bench<M4>("emihmap4(swiss)", keys, miss_keys);
        run_bench<M3>("emihmap3", keys, miss_keys);
        run_bench<M2>("emihmap2", keys, miss_keys);
    }

    // ─── Sequential int64_t ────────────────────────────────────────
    printf("\n\n=== Sequential int64_t key (wyhash) ===\n");
    printf("  %-30s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000, 10000000}) {
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = N + i + 1; }

        printf("\n  [N = %d]\n", N);
        using M2 = emilib2::HashMap<K, V, WyHashI64>;
        using M3 = emilib3::HashMap<K, V, WyHashI64>;
        using M4 = emilib4::HashMap<K, V, WyHashI64>;
        using MB = boost::unordered_flat_map<K, V, WyHashI64>;
        run_bench<MB>("boost::flat_map", keys, miss_keys);
        run_bench<M4>("emihmap4(swiss)", keys, miss_keys);
        run_bench<M3>("emihmap3", keys, miss_keys);
        run_bench<M2>("emihmap2", keys, miss_keys);
    }

    // ─── String key ────────────────────────────────────────────────
    printf("\n\n=== String key (wyhash) ===\n");
    printf("  %-30s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000}) {
        std::vector<std::string> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = "key_" + std::to_string(i + 1);
            miss_keys[i] = "miss_" + std::to_string(i + 1);
        }

        printf("\n  [N = %d]\n", N);
        using M2S = emilib2::HashMap<std::string, V, WyHashStr>;
        using M3S = emilib3::HashMap<std::string, V, WyHashStr>;
        using M4S = emilib4::HashMap<std::string, V, WyHashStr>;
        using MBS = boost::unordered_flat_map<std::string, V, WyHashStr>;
        run_bench<MBS>("boost::flat_map", keys, miss_keys);
        run_bench<M4S>("emihmap4(swiss)", keys, miss_keys);
        run_bench<M3S>("emihmap3", keys, miss_keys);
        run_bench<M2S>("emihmap2", keys, miss_keys);
    }

    return 0;
}
