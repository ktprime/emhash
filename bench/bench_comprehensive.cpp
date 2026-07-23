/**
 * Comprehensive benchmark covering diverse scenarios.
 * All implementations use the same wyhash for fairness.
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
#include <algorithm>

// ── Unified hash ──────────────────────────────────────────────────
struct WyHashI64 {
    size_t operator()(uint64_t key) const noexcept { return emh_wyhash64(key, 0); }
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

// ── Key generators ────────────────────────────────────────────────
enum class Dist { Random, Sequential, Reverse, Clustered };

std::vector<uint64_t> gen_keys(int N, Dist dist, uint64_t seed = 42) {
    std::vector<uint64_t> keys(N);
    switch (dist) {
        case Dist::Random: {
            std::mt19937_64 rng(seed);
            for (int i = 0; i < N; i++) keys[i] = rng();
            break;
        }
        case Dist::Sequential:
            for (int i = 0; i < N; i++) keys[i] = i + 1;
            break;
        case Dist::Reverse:
            for (int i = 0; i < N; i++) keys[i] = N - i;
            break;
        case Dist::Clustered: {
            // 80% keys in 20% range (hot region)
            std::mt19937_64 rng(seed);
            uint64_t hot_range = N / 5;
            for (int i = 0; i < N; i++) {
                if (i % 5 < 4) keys[i] = rng() % hot_range;
                else keys[i] = rng();
            }
            break;
        }
    }
    return keys;
}

// ── Scenario 1: Pure insert ──────────────────────────────────────
template<typename Map>
double bench_insert(const std::vector<uint64_t>& keys, int rounds = 5) {
    double best = 1e9;
    for (int r = 0; r < rounds; r++) {
        Map m;
        m.reserve(keys.size() * 12 / 10);
        auto t0 = now_us();
        for (auto k : keys) m[k] = 1;
        auto t1 = now_us();
        double ms = (t1 - t0) / 1000.0;
        if (ms < best) best = ms;
    }
    return best;
}

// ── Scenario 2: Find hit (pre-filled) ───────────────────────────
template<typename Map>
double bench_find_hit(const std::vector<uint64_t>& keys, int rounds = 5) {
    Map m;
    m.reserve(keys.size() * 12 / 10);
    for (auto k : keys) m[k] = 1;

    double best = 1e9;
    for (int r = 0; r < rounds; r++) {
        volatile size_t sink = 0;
        auto t0 = now_us();
        for (auto k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
        auto t1 = now_us();
        double ms = (t1 - t0) / 1000.0;
        if (ms < best) best = ms;
    }
    return best;
}

// ── Scenario 3: Find miss ────────────────────────────────────────
template<typename Map>
double bench_find_miss(const std::vector<uint64_t>& keys, int rounds = 5) {
    Map m;
    m.reserve(keys.size() * 12 / 10);
    for (auto k : keys) m[k] = 1;

    // Generate miss keys (different from inserted keys)
    std::vector<uint64_t> miss_keys(keys.size());
    std::mt19937_64 rng(999);
    for (size_t i = 0; i < keys.size(); i++) miss_keys[i] = rng() | (1ULL << 63);

    double best = 1e9;
    for (int r = 0; r < rounds; r++) {
        volatile size_t sink = 0;
        auto t0 = now_us();
        for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        auto t1 = now_us();
        double ms = (t1 - t0) / 1000.0;
        if (ms < best) best = ms;
    }
    return best;
}

// ── Scenario 4: Erase all ────────────────────────────────────────
template<typename Map>
double bench_erase(const std::vector<uint64_t>& keys, int rounds = 3) {
    double best = 1e9;
    for (int r = 0; r < rounds; r++) {
        Map m;
        m.reserve(keys.size() * 12 / 10);
        for (auto k : keys) m[k] = 1;

        auto t0 = now_us();
        for (auto k : keys) m.erase(k);
        auto t1 = now_us();
        double ms = (t1 - t0) / 1000.0;
        if (ms < best) best = ms;
    }
    return best;
}

// ── Scenario 5: Mixed workload (70R/20I/10E) ────────────────────
template<typename Map>
double bench_mixed(const std::vector<uint64_t>& keys, int rounds = 3) {
    int N = (int)keys.size();
    std::mt19937 rng(123);
    std::vector<int> ops(N);
    for (int i = 0; i < N; i++) {
        int r = rng() % 10;
        ops[i] = r < 7 ? 0 : (r < 9 ? 1 : 2); // 70% find, 20% insert, 10% erase
    }

    double best = 1e9;
    for (int round = 0; round < rounds; round++) {
        Map m;
        m.reserve(N * 12 / 10);
        // Pre-fill 50%
        for (int i = 0; i < N / 2; i++) m[keys[i]] = i;

        volatile size_t sink = 0;
        auto t0 = now_us();
        for (int i = 0; i < N; i++) {
            auto k = keys[i % N];
            if (ops[i] == 0) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
            else if (ops[i] == 1) m[k] = i;
            else m.erase(k);
        }
        auto t1 = now_us();
        double ms = (t1 - t0) / 1000.0;
        if (ms < best) best = ms;
    }
    return best;
}

// ── Scenario 6: Tombstone stress (insert all, erase half, insert again) ─
template<typename Map>
double bench_tombstone(const std::vector<uint64_t>& keys, int rounds = 3) {
    int N = (int)keys.size();
    double best = 1e9;
    for (int r = 0; r < rounds; r++) {
        Map m;
        m.reserve(N * 12 / 10);
        // Phase 1: insert all
        for (auto k : keys) m[k] = 1;
        // Phase 2: erase even indices
        for (int i = 0; i < N; i += 2) m.erase(keys[i]);
        // Phase 3: re-insert new keys at even positions
        auto t0 = now_us();
        for (int i = 0; i < N; i += 2) m[keys[i] + 1] = 2;
        auto t1 = now_us();
        double ms = (t1 - t0) / 1000.0;
        if (ms < best) best = ms;
    }
    return best;
}

// ── Scenario 7: Iteration ────────────────────────────────────────
template<typename Map>
double bench_iterate(const std::vector<uint64_t>& keys, int rounds = 5) {
    Map m;
    m.reserve(keys.size() * 12 / 10);
    for (auto k : keys) m[k] = 1;

    double best = 1e9;
    for (int r = 0; r < rounds; r++) {
        volatile size_t sink = 0;
        auto t0 = now_us();
        for (auto& kv : m) sink += kv.second;
        auto t1 = now_us();
        double ms = (t1 - t0) / 1000.0;
        if (ms < best) best = ms;
    }
    return best;
}

// ── Runner ────────────────────────────────────────────────────────
template<typename M2, typename M3, typename M4, typename MB>
void run_all(const char* title, const std::vector<uint64_t>& keys) {
    printf("\n--- %s (N=%zu, best of 3-5) ---\n", title, keys.size());
    printf("  %-20s %8s %8s %8s %8s %8s %8s %8s\n",
           "Map", "Insert", "FindHit", "FindMs", "Erase", "Mixed", "Tomb", "Iter");
    printf("  %-20s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f\n",
           "boost::flat_map",
           bench_insert<MB>(keys), bench_find_hit<MB>(keys), bench_find_miss<MB>(keys),
           bench_erase<MB>(keys), bench_mixed<MB>(keys), bench_tombstone<MB>(keys),
           bench_iterate<MB>(keys));
    printf("  %-20s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f\n",
           "emihmap4(swiss)",
           bench_insert<M4>(keys), bench_find_hit<M4>(keys), bench_find_miss<M4>(keys),
           bench_erase<M4>(keys), bench_mixed<M4>(keys), bench_tombstone<M4>(keys),
           bench_iterate<M4>(keys));
    printf("  %-20s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f\n",
           "emihmap2",
           bench_insert<M2>(keys), bench_find_hit<M2>(keys), bench_find_miss<M2>(keys),
           bench_erase<M2>(keys), bench_mixed<M2>(keys), bench_tombstone<M2>(keys),
           bench_iterate<M2>(keys));
}

int main() {
    using K = uint64_t;
    using V = uint64_t;
    using M2 = emilib2::HashMap<K, V, WyHashI64>;
    using M3 = emilib3::HashMap<K, V, WyHashI64>;
    using M4 = emilib4::HashMap<K, V, WyHashI64>;
    using MB = boost::unordered_flat_map<K, V, WyHashI64>;

    printf("======================================================================\n");
    printf("  Comprehensive benchmark (wyhash, best of rounds, ms)\n");
    printf("======================================================================\n");

    // ── Size sweep with random keys ──────────────────────────────
    for (int N : {1000, 4000, 16000, 64000, 256000, 1000000, 4000000, 10000000}) {
        auto keys = gen_keys(N, Dist::Random);
        char title[128];
        snprintf(title, sizeof(title), "Random N=%d", N);
        // Skip largest sizes for slower tests
        if (N >= 4000000) {
            printf("\n--- %s (N=%d, best of 3-5) ---\n", title, N);
            printf("  %-20s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMs", "Erase");
            printf("  %-20s %8.1f %8.1f %8.1f %8.1f\n", "boost::flat_map",
                bench_insert<MB>(keys), bench_find_hit<MB>(keys), bench_find_miss<MB>(keys), bench_erase<MB>(keys));
            printf("  %-20s %8.1f %8.1f %8.1f %8.1f\n", "emihmap4(swiss)",
                bench_insert<M4>(keys), bench_find_hit<M4>(keys), bench_find_miss<M4>(keys), bench_erase<M4>(keys));
            printf("  %-20s %8.1f %8.1f %8.1f %8.1f\n", "emihmap2",
                bench_insert<M2>(keys), bench_find_hit<M2>(keys), bench_find_miss<M2>(keys), bench_erase<M2>(keys));
        } else {
            run_all<M2, M3, M4, MB>(title, keys);
        }
    }

    // ── Distribution comparison at N=1M ──────────────────────────
    for (auto dist : {Dist::Sequential, Dist::Reverse, Dist::Clustered}) {
        auto keys = gen_keys(1000000, dist);
        const char* name = dist == Dist::Sequential ? "Sequential" :
                           dist == Dist::Reverse ? "Reverse" : "Clustered";
        char title[128];
        snprintf(title, sizeof(title), "%s N=1000000", name);
        run_all<M2, M3, M4, MB>(title, keys);
    }

    return 0;
}
