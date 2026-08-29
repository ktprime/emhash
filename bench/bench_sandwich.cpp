/**
 * Benchmark: emihmap3 (orig) vs emihmap3_flat vs emihmap3_sandwich vs emihmap4_opt vs boost
 * Compilers: g++-11, g++-16, clang++-20
 * Hash: std::hash, WyHash, FNV-1a
 * Keys: uint64_t (sequential + random), std::string
 * Sizes: 10K, 100K, 1M, 5M
 */
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO626_REMOVED_WARNING
#define NDEBUG

#include "emilib/emihmap3.hpp"
#include "emilib/emihmap3_flat.hpp"
#include "emilib/emihmap3_sandwich.hpp"
#include "emilib/emihmap4_opt.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdio>
#include <cstdint>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

class WyRand {
public:
    WyRand(uint64_t seed = 42) : s(seed) {}
    uint64_t operator()() {
        s += 0xa0761d6478bd642full;
        uint64_t t = s ^ 0xe7037ed1a0b428dbull;
        uint64_t r = t ^ (t >> 33);
        r *= (t >> 31 | t << 33) ^ 0x74743c1bu;
        return r ^ (r >> 33);
    }
private:
    uint64_t s;
};

template<typename Map, typename Keys, typename Val>
int64_t bench_insert(Map& m, const Keys& keys, const Val& val) {
    auto t0 = now_us();
    for (auto& k : keys) m.emplace(k, val);
    return now_us() - t0;
}

template<typename Map, typename Keys>
int64_t bench_find_hit(const Map& m, const Keys& keys) {
    volatile size_t sink = 0;
    auto t0 = now_us();
    for (auto& k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
    (void)sink;
    return now_us() - t0;
}

template<typename Map, typename Keys>
int64_t bench_find_miss(const Map& m, const Keys& miss_keys) {
    volatile size_t sink = 0;
    auto t0 = now_us();
    for (auto& k : miss_keys) { if (m.find(k) == m.end()) sink++; }
    (void)sink;
    return now_us() - t0;
}

template<typename Map, typename Keys>
int64_t bench_erase(Map& m, const Keys& keys) {
    auto t0 = now_us();
    for (auto& k : keys) m.erase(k);
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
    printf("  %-28s %10lld %10lld %10lld %10lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

using KI = uint64_t;
using VI = uint64_t;
using KS = std::string;

int main() {
#ifdef __clang__
    const char* compiler = "clang++";
#elif defined(__GNUC__)
    const char* compiler = "g++";
#else
    const char* compiler = "unknown";
#endif

    printf("========================================\n");
    printf("  emihmap3 layout comparison benchmark\n");
    printf("  Compiler: %s\n", compiler);
    printf("========================================\n\n");

    // ─── Sequential int64, std::hash ─────────────────────────────
    printf("=== Sequential int64, std::hash (us) ===\n");
    printf("  %-28s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = N + i + 1; }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI>>("emihmap3(orig)", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI>>("emihmap3_flat", keys, miss_keys, (VI)1);
        run_bench<emilib3_sw::HashMap<KI, VI>>("emihmap3_sandwich", keys, miss_keys, (VI)1);
        run_bench<emilib4_opt::HashMap<KI, VI>>("emihmap4_opt", keys, miss_keys, (VI)1);
        run_bench<boost::unordered_flat_map<KI, VI>>("boost", keys, miss_keys, (VI)1);
    }

    // ─── Random int64, std::hash ────────────────────────────────
    printf("\n\n=== Random int64, std::hash (us) ===\n");
    printf("  %-28s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        WyRand rng(42);
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI>>("emihmap3(orig)", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI>>("emihmap3_flat", keys, miss_keys, (VI)1);
        run_bench<emilib3_sw::HashMap<KI, VI>>("emihmap3_sandwich", keys, miss_keys, (VI)1);
        run_bench<emilib4_opt::HashMap<KI, VI>>("emihmap4_opt", keys, miss_keys, (VI)1);
        run_bench<boost::unordered_flat_map<KI, VI>>("boost", keys, miss_keys, (VI)1);
    }

    // ─── String keys, std::hash ─────────────────────────────────
    printf("\n\n=== String keys, std::hash (us) ===\n");
    printf("  %-28s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000}) {
        std::vector<KS> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = "key_" + std::to_string(i + 1); miss_keys[i] = "miss_" + std::to_string(i + 1); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KS, VI>>("emihmap3(orig)", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KS, VI>>("emihmap3_flat", keys, miss_keys, (VI)1);
        run_bench<emilib3_sw::HashMap<KS, VI>>("emihmap3_sandwich", keys, miss_keys, (VI)1);
        run_bench<emilib4_opt::HashMap<KS, VI>>("emihmap4_opt", keys, miss_keys, (VI)1);
        run_bench<boost::unordered_flat_map<KS, VI>>("boost", keys, miss_keys, (VI)1);
    }

    printf("\n=== Done ===\n");
    return 0;
}
