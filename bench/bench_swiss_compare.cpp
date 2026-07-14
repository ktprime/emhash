/**
 * Benchmark: emihmap2 vs emihmap3 vs emihmap4(swiss) vs boost::unordered_flat_map
 *
 * Compile (GCC):
 *   g++ -std=c++17 -O2 -march=native -I../include -I../thirdparty -DNOMINMAX bench_swiss_compare.cpp -o bench_swiss
 */

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO646_REMOVED_WARNING
#define FIB_HASH 0
#define TKey 1
#define STR_SIZE 15
#define ABSL_HMAP 0
#define A_HASH 0
#define QC_HASH 0

#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdio>
#include <cstdint>
#include <chrono>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

// ─── minimal timing and utility (no util.h to avoid conflicts) ────────

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

class WyRand {
public:
    WyRand(uint64_t seed = 42) { wyrng_state = seed; }
    uint64_t operator()() { return wyrand(&wyrng_state); }
private:
    static inline uint64_t wyrot(uint64_t v) { return (v >> 33) | (v << 31); }
    static inline uint64_t wymix(uint64_t a, uint64_t b) {
        uint64_t r = a ^ 0x53c5ca59u; r *= b ^ 0x74743c1bu;
        return r ^ (r >> 33);
    }
    static inline uint64_t wyrand(uint64_t *s) {
        *s += 0xa0761d6478bd642full;
        uint64_t t = *s ^ 0xe7037ed1a0b428dbull;
        return wymix(t, wyrot(t));
    }
    uint64_t wyrng_state;
};

// ─── benchmark a single operation ─────────────────────────────────────

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

// ─── run full benchmark for one map type ──────────────────────────────

template<typename Map, typename Keys, typename MissKeys, typename Val>
void run_bench(const char* name, const Keys& keys, const MissKeys& miss_keys, const Val& val) {
    Map m;
    m.reserve(static_cast<size_t>(keys.size() * 1.2));

    auto t_ins = bench_insert(m, keys, val);
    auto t_hit = bench_find_hit(m, keys);
    auto t_miss = bench_find_miss(m, miss_keys);
    auto t_era = bench_erase(m, keys);

    printf("  %-28s %8lld %8lld %8lld %8lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

// ─── main ─────────────────────────────────────────────────────────────

int main() {
    using KeyInt = uint64_t;
    using ValInt = uint64_t;

    printf("=== Sequential int64_t key benchmarks (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {4000, 64000, 1000000, 10000000}) {
        std::vector<KeyInt> keys(N);
        for (int64_t i = 0; i < N; i++) keys[i] = i + 1;

        std::vector<KeyInt> miss_keys(N);
        for (int64_t i = 0; i < N; i++) miss_keys[i] = N + i + 1;

        printf("\n  [N = %d]\n", N);

        run_bench<emilib2::HashMap<KeyInt, ValInt>>("emihmap2", keys, miss_keys, (ValInt)1);
        run_bench<emilib3::HashMap<KeyInt, ValInt>>("emihmap3", keys, miss_keys, (ValInt)1);
        run_bench<emilib4::HashMap<KeyInt, ValInt>>("emihmap4(swiss)", keys, miss_keys, (ValInt)1);
        run_bench<boost::unordered_flat_map<KeyInt, ValInt>>("boost::flat_map", keys, miss_keys, (ValInt)1);
    }

    // ─── String key benchmarks ────────────────────────────────────────

    printf("\n\n=== string key benchmarks (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {4000, 64000, 1000000}) {
        std::vector<std::string> keys(N);
        std::vector<std::string> miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = "key_" + std::to_string(i + 1);
            miss_keys[i] = "miss_" + std::to_string(i + 1);
        }

        printf("\n  [N = %d]\n", N);

        run_bench<emilib2::HashMap<std::string, ValInt>>("emihmap2", keys, miss_keys, (ValInt)1);
        run_bench<emilib3::HashMap<std::string, ValInt>>("emihmap3", keys, miss_keys, (ValInt)1);
        run_bench<emilib4::HashMap<std::string, ValInt>>("emihmap4(swiss)", keys, miss_keys, (ValInt)1);
        run_bench<boost::unordered_flat_map<std::string, ValInt>>("boost::flat_map", keys, miss_keys, (ValInt)1);
    }

    // ─── Random key benchmark ─────────────────────────────────────────

    printf("\n\n=== Random int64_t key benchmarks (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {1000000, 10000000}) {
        WyRand rng(42);
        std::vector<KeyInt> keys(N);
        std::vector<KeyInt> miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = rng();
            miss_keys[i] = rng();
        }

        printf("\n  [N = %d]\n", N);

        run_bench<emilib2::HashMap<KeyInt, ValInt>>("emihmap2", keys, miss_keys, (ValInt)1);
        run_bench<emilib3::HashMap<KeyInt, ValInt>>("emihmap3", keys, miss_keys, (ValInt)1);
        run_bench<emilib4::HashMap<KeyInt, ValInt>>("emihmap4(swiss)", keys, miss_keys, (ValInt)1);
        run_bench<boost::unordered_flat_map<KeyInt, ValInt>>("boost::flat_map", keys, miss_keys, (ValInt)1);
    }

    return 0;
}
