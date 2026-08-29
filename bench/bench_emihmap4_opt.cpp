/**
 * Benchmark: emihmap4 (original) vs emihmap4_opt (optimized) vs boost::unordered_flat_map
 *
 * Tests: Insert, FindHit, FindMiss, Erase
 * Key types: int64_t, std::string
 * Data sizes: 1K, 10K, 100K, 1M, 5M
 * Key distributions: sequential, random
 * Multiple compilers: g++-11, g++-16, clang++-20
 *
 * Usage:
 *   #!/bin/bash
 *   for CC in g++-11 g++-16 clang++-20; do
 *     $CC -std=c++17 -O3 -march=native -msse2 -DNOMINMAX \
 *       -I../include -I../thirdparty \
 *       bench_emihmap4_opt.cpp -o bench_emihmap4_opt_${CC}
 *     ./bench_emihmap4_opt_${CC}
 *   done
 */

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO646_REMOVED_WARNING

#include "emilib/emihmap4.hpp"
#include "emilib/emihmap4_opt.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdio>
#include <cstdint>
#include <chrono>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

// ─── timing ──────────────────────────────────────────────────────────

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ─── WyRand PRNG (deterministic, fast) ───────────────────────────────

class WyRand {
public:
    WyRand(uint64_t seed = 42) : wyrng_state(seed) {}
    uint64_t operator()() {
        wyrng_state += 0xa0761d6478bd642full;
        uint64_t t = wyrng_state ^ 0xe7037ed1a0b428dbull;
        uint64_t r = t ^ (t >> 33);
        r *= (t >> 31 | t << 33) ^ 0x74743c1bu;
        return r ^ (r >> 33);
    }
private:
    uint64_t wyrng_state;
};

// ─── benchmark operations ────────────────────────────────────────────

template<typename Map, typename Keys, typename Val>
int64_t bench_insert(Map& m, const Keys& keys, const Val& val) {
    auto t0 = now_us();
    for (auto& k : keys)
        m.emplace(k, val);
    return now_us() - t0;
}

template<typename Map, typename Keys>
int64_t bench_find_hit(const Map& m, const Keys& keys) {
    volatile size_t sink = 0;
    auto t0 = now_us();
    for (auto& k : keys) {
        auto it = m.find(k);
        if (it != m.end()) sink += it->second;
    }
    (void)sink;
    return now_us() - t0;
}

template<typename Map, typename Keys>
int64_t bench_find_miss(const Map& m, const Keys& miss_keys) {
    volatile size_t sink = 0;
    auto t0 = now_us();
    for (auto& k : miss_keys) {
        if (m.find(k) == m.end()) sink++;
    }
    (void)sink;
    return now_us() - t0;
}

template<typename Map, typename Keys>
int64_t bench_erase(Map& m, const Keys& keys) {
    auto t0 = now_us();
    for (auto& k : keys)
        m.erase(k);
    return now_us() - t0;
}

template<typename Map, typename Keys, typename MissKeys, typename Val>
void run_bench(const char* name, const Keys& keys, const MissKeys& miss_keys, const Val& val) {
    Map m;
    m.reserve(static_cast<size_t>(keys.size() * 1.2));

    auto t_ins = bench_insert(m, keys, val);
    auto t_hit = bench_find_hit(m, keys);
    auto t_miss = bench_find_miss(m, miss_keys);
    auto t_era = bench_erase(m, keys);

    printf("  %-30s %10lld %10lld %10lld %10lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

// ─── main ────────────────────────────────────────────────────────────

int main() {
    using KeyInt = uint64_t;
    using ValInt = uint64_t;

    printf("=== Compiler: %s ===\n",
#ifdef __clang__
        "clang++"
#elif defined(__GNUC__)
        "g++"
#else
        "unknown"
#endif
    );
#ifdef __GNUC__
    printf("    Version: %d.%d\n", __GNUC__, __GNUC_MINOR__);
#endif

    // ─── Sequential int64_t keys ────────────────────────────────────
    printf("\n=== Sequential int64_t keys (us) ===\n");
    printf("  %-30s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {1000, 10000, 100000, 1000000, 5000000}) {
        std::vector<KeyInt> keys(N);
        for (int64_t i = 0; i < N; i++) keys[i] = i + 1;

        std::vector<KeyInt> miss_keys(N);
        for (int64_t i = 0; i < N; i++) miss_keys[i] = N + i + 1;

        printf("\n  [N = %d]\n", N);
        run_bench<emilib4::HashMap<KeyInt, ValInt>>("emihmap4(orig)", keys, miss_keys, (ValInt)1);
        run_bench<emilib4_opt::HashMap<KeyInt, ValInt>>("emihmap4(opt)", keys, miss_keys, (ValInt)1);
        run_bench<boost::unordered_flat_map<KeyInt, ValInt>>("boost::flat_map", keys, miss_keys, (ValInt)1);
    }

    // ─── Random int64_t keys ────────────────────────────────────────
    printf("\n\n=== Random int64_t keys (us) ===\n");
    printf("  %-30s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {10000, 100000, 1000000, 5000000}) {
        WyRand rng(42);
        std::vector<KeyInt> keys(N);
        std::vector<KeyInt> miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = rng();
            miss_keys[i] = rng();
        }

        printf("\n  [N = %d]\n", N);
        run_bench<emilib4::HashMap<KeyInt, ValInt>>("emihmap4(orig)", keys, miss_keys, (ValInt)1);
        run_bench<emilib4_opt::HashMap<KeyInt, ValInt>>("emihmap4(opt)", keys, miss_keys, (ValInt)1);
        run_bench<boost::unordered_flat_map<KeyInt, ValInt>>("boost::flat_map", keys, miss_keys, (ValInt)1);
    }

    // ─── String keys ────────────────────────────────────────────────
    printf("\n\n=== String keys (us) ===\n");
    printf("  %-30s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {10000, 100000, 1000000}) {
        std::vector<std::string> keys(N);
        std::vector<std::string> miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = "key_" + std::to_string(i + 1);
            miss_keys[i] = "miss_" + std::to_string(i + 1);
        }

        printf("\n  [N = %d]\n", N);
        run_bench<emilib4::HashMap<std::string, ValInt>>("emihmap4(orig)", keys, miss_keys, (ValInt)1);
        run_bench<emilib4_opt::HashMap<std::string, ValInt>>("emihmap4(opt)", keys, miss_keys, (ValInt)1);
        run_bench<boost::unordered_flat_map<std::string, ValInt>>("boost::flat_map", keys, miss_keys, (ValInt)1);
    }

    // ─── Iterated insert/erase stress (anti-drift test) ─────────────
    printf("\n\n=== Iterated Insert/Erase stress (us) ===\n");
    printf("  %-30s %10s %10s %10s\n", "Map", "Ins+Era x5", "FindHit", "FinalSize");

    for (auto N : {100000, 1000000}) {
        WyRand rng(42);
        std::vector<KeyInt> keys(N);
        for (int i = 0; i < N; i++) keys[i] = rng();

        printf("\n  [N = %d]\n", N);

        // emihmap4 orig
        {
            emilib4::HashMap<KeyInt, ValInt> m;
            m.reserve(keys.size() * 2);
            int64_t total = 0;
            for (int round = 0; round < 5; round++) {
                auto t0 = now_us();
                for (auto& key : keys) m[key] = 1;
                for (auto& key : keys) m.erase(key);
                total += now_us() - t0;
            }
            for (auto& key : keys) m[key] = 1;
            volatile size_t sink = 0;
            auto t0 = now_us();
            for (auto& key : keys) { auto it = m.find(key); if (it != m.end()) sink += it->second; }
            auto t_hit = now_us() - t0;
            (void)sink;
            printf("  %-30s %10lld %10lld %10zu\n", "emihmap4(orig)", (long long)total, (long long)t_hit, m.size());
        }
        // emihmap4 opt
        {
            emilib4_opt::HashMap<KeyInt, ValInt> m;
            m.reserve(keys.size() * 2);
            int64_t total = 0;
            for (int round = 0; round < 5; round++) {
                auto t0 = now_us();
                for (auto& key : keys) m[key] = 1;
                for (auto& key : keys) m.erase(key);
                total += now_us() - t0;
            }
            for (auto& key : keys) m[key] = 1;
            volatile size_t sink = 0;
            auto t0 = now_us();
            for (auto& key : keys) { auto it = m.find(key); if (it != m.end()) sink += it->second; }
            auto t_hit = now_us() - t0;
            (void)sink;
            printf("  %-30s %10lld %10lld %10zu\n", "emihmap4(opt)", (long long)total, (long long)t_hit, m.size());
        }
        // boost
        {
            boost::unordered_flat_map<KeyInt, ValInt> m;
            m.reserve(keys.size() * 2);
            int64_t total = 0;
            for (int round = 0; round < 5; round++) {
                auto t0 = now_us();
                for (auto& key : keys) m[key] = 1;
                for (auto& key : keys) m.erase(key);
                total += now_us() - t0;
            }
            for (auto& key : keys) m[key] = 1;
            volatile size_t sink = 0;
            auto t0 = now_us();
            for (auto& key : keys) { auto it = m.find(key); if (it != m.end()) sink += it->second; }
            auto t_hit = now_us() - t0;
            (void)sink;
            printf("  %-30s %10lld %10lld %10zu\n", "boost::flat_map", (long long)total, (long long)t_hit, m.size());
        }
    }

    printf("\n=== Done ===\n");
    return 0;
}
