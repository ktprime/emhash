/**
 * Controlled micro-benchmark: emihmap4_opt find vs boost find
 * Focus: isolate clang++-20 find path performance difference
 */
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO626_REMOVED_WARNING
#define NDEBUG

#include "emilib/emihmap4_opt.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdio>
#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>

static int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static uint64_t wyrand(uint64_t& s) {
    s += 0xa0761d6478bd642full;
    uint64_t t = s ^ 0xe7037ed1a0b428dbull;
    uint64_t r = t ^ (t >> 33);
    r *= (t >> 31 | t << 33) ^ 0x74743c1bu;
    return r ^ (r >> 33);
}

template<typename Map>
int64_t measure_find_hit(Map& m, const std::vector<uint64_t>& keys, int iters) {
    volatile size_t sink = 0;
    int64_t total = 0;
    for (int i = 0; i < iters; i++) {
        auto t0 = now_ns();
        size_t sum = 0;
        for (auto& k : keys) {
            auto it = m.find(k);
            if (it != m.end()) sum += it->second;
        }
        total += now_ns() - t0;
        sink += sum;
    }
    (void)sink;
    return total / iters;
}

template<typename Map>
int64_t measure_find_miss(Map& m, const std::vector<uint64_t>& miss_keys, int iters) {
    volatile size_t sink = 0;
    int64_t total = 0;
    for (int i = 0; i < iters; i++) {
        auto t0 = now_ns();
        size_t cnt = 0;
        for (auto& k : miss_keys) {
            if (m.find(k) == m.end()) cnt++;
        }
        total += now_ns() - t0;
        sink += cnt;
    }
    (void)sink;
    return total / iters;
}

// ─── Probe length measurement via instrumented find ──────────────────
// Count how many groups are probed per find by wrapping the find loop

struct ProbeStats {
    double avg_probes;
    int max_probes;
    int total_finds;
};

// Manual probe counting for emihmap4_opt
ProbeStats count_probes_emihmap4(const emilib4_opt::HashMap<uint64_t, uint64_t>& m, const std::vector<uint64_t>& keys) {
    // Can't access internals easily, so use a statistical approach:
    // Count the fraction of finds that need >1 group probe by measuring
    // timing differences. Instead, let's just report size/capacity info.
    ProbeStats s = {0, 0, (int)keys.size()};
    return s;
}

int main() {
    printf("=== clang++-20 find path investigation ===\n");
#ifdef __clang__
    printf("    Compiler: clang++ %d.%d\n", __clang_major__, __clang_minor__);
#elif defined(__GNUC__)
    printf("    Compiler: g++ %d.%d\n", __GNUC__, __GNUC_MINOR__);
#endif
    printf("\n");

    // ─── Random int64 find-hit ───────────────────────────────────────
    printf("=== Random int64 keys ===\n");
    printf("  %-12s %12s %12s %8s %12s %12s %8s\n",
           "N", "opt_hit(ns)", "boost_hit(ns)", "ratio", "opt_miss(ns)", "boost_miss(ns)", "ratio");
    printf("  %s\n", std::string(78, '-').c_str());

    for (auto N : {10000, 100000, 1000000, 5000000}) {
        uint64_t seed = 42;
        std::vector<uint64_t> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = wyrand(seed); miss_keys[i] = wyrand(seed); }

        emilib4_opt::HashMap<uint64_t, uint64_t> em;
        boost::unordered_flat_map<uint64_t, uint64_t> bm;
        em.reserve(N * 2);
        bm.reserve(N * 2);
        for (auto& k : keys) { em[k] = 1; bm[k] = 1; }

        // Warmup (1 pass)
        volatile size_t sink = 0;
        for (int i = 0; i < std::min((int)N, 1000); i++) {
            sink += (em.find(keys[i]) != em.end()) ? 1 : 0;
            sink += (bm.find(keys[i]) != bm.end()) ? 1 : 0;
        }
        (void)sink;

        int iters = (N <= 100000) ? 5 : 3;
        auto em_hit = measure_find_hit(em, keys, iters);
        auto bm_hit = measure_find_hit(bm, keys, iters);
        auto em_miss = measure_find_miss(em, miss_keys, iters);
        auto bm_miss = measure_find_miss(bm, miss_keys, iters);

        printf("  %-12d %12lld %12lld %7.2fx %12lld %12lld %7.2fx\n",
               N, (long long)em_hit, (long long)bm_hit, (double)em_hit / bm_hit,
               (long long)em_miss, (long long)bm_miss, (double)em_miss / bm_miss);
    }

    // ─── Sequential int64 find-hit ───────────────────────────────────
    printf("\n=== Sequential int64 keys ===\n");
    printf("  %-12s %12s %12s %8s\n", "N", "opt_hit(ns)", "boost_hit(ns)", "ratio");

    for (auto N : {100000, 1000000}) {
        std::vector<uint64_t> keys(N);
        for (int i = 0; i < N; i++) keys[i] = i + 1;

        emilib4_opt::HashMap<uint64_t, uint64_t> em;
        boost::unordered_flat_map<uint64_t, uint64_t> bm;
        em.reserve(N * 2);
        bm.reserve(N * 2);
        for (auto& k : keys) { em[k] = 1; bm[k] = 1; }

        auto em_hit = measure_find_hit(em, keys, 3);
        auto bm_hit = measure_find_hit(bm, keys, 3);
        printf("  %-12d %12lld %12lld %7.2fx\n", N, (long long)em_hit, (long long)bm_hit, (double)em_hit / bm_hit);
    }

    // ─── Memory layout comparison ────────────────────────────────────
    printf("\n=== Memory layout comparison (N=1M random) ===\n");
    {
        const int N = 1000000;
        uint64_t seed = 42;
        std::vector<uint64_t> keys(N);
        for (int i = 0; i < N; i++) keys[i] = wyrand(seed);

        emilib4_opt::HashMap<uint64_t, uint64_t> em;
        boost::unordered_flat_map<uint64_t, uint64_t> bm;
        em.reserve(N * 2);
        bm.reserve(N * 2);
        for (auto& k : keys) { em[k] = 1; bm[k] = 1; }

        printf("  emihmap4_opt: size=%zu, bucket_count=%zu, load_factor=%.3f\n",
               em.size(), em.bucket_count(), em.load_factor());
        printf("  boost:        size=%zu, bucket_count=%zu, load_factor=%.3f\n",
               bm.size(), bm.bucket_count(), bm.load_factor());

        // Test: does the load factor / capacity difference explain the find gap?
        // Boost might have a different group count at the same N
    }

    // ─── Critical test: identical capacity ────────────────────────────
    // Force both maps to the same capacity to rule out rehash/layout effects
    printf("\n=== Same capacity test (N=1M, reserve exactly) ===\n");
    {
        const int N = 1000000;
        uint64_t seed = 42;
        std::vector<uint64_t> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = wyrand(seed); miss_keys[i] = wyrand(seed); }

        // Create with exact same reserve
        emilib4_opt::HashMap<uint64_t, uint64_t> em;
        boost::unordered_flat_map<uint64_t, uint64_t> bm;

        // Reserve to same number of groups
        auto target_groups = (N / 15) * 2;  // ~2x groups for 50% load
        auto target_buckets = target_groups * 15;

        em.reserve(target_buckets);
        bm.reserve(target_buckets);

        for (auto& k : keys) { em[k] = 1; bm[k] = 1; }

        printf("  After insert:\n");
        printf("    emihmap4_opt: size=%zu, bucket_count=%zu, load_factor=%.4f\n",
               em.size(), em.bucket_count(), em.load_factor());
        printf("    boost:        size=%zu, bucket_count=%zu, load_factor=%.4f\n",
               bm.size(), bm.bucket_count(), bm.load_factor());

        volatile size_t sink = 0;
        for (int i = 0; i < 1000; i++) {
            sink += (em.find(keys[i]) != em.end()) ? 1 : 0;
            sink += (bm.find(keys[i]) != bm.end()) ? 1 : 0;
        }
        (void)sink;

        auto em_hit = measure_find_hit(em, keys, 3);
        auto bm_hit = measure_find_hit(bm, keys, 3);
        auto em_miss = measure_find_miss(em, miss_keys, 3);
        auto bm_miss = measure_find_miss(bm, miss_keys, 3);

        printf("  FindHit:  opt=%lldns  boost=%lldns  ratio=%.2fx\n",
               (long long)em_hit, (long long)bm_hit, (double)em_hit / bm_hit);
        printf("  FindMiss: opt=%lldns  boost=%lldns  ratio=%.2fx\n",
               (long long)em_miss, (long long)bm_miss, (double)em_miss / bm_miss);
    }

    // ─── Vary load factor to find crossover ──────────────────────────
    printf("\n=== Load factor sweep (N=100K) ===\n");
    printf("  %-8s %12s %12s %8s %12s %12s %8s\n",
           "LF", "opt_hit(ns)", "boost_hit(ns)", "ratio", "opt_miss(ns)", "boost_miss(ns)", "ratio");
    {
        const int N = 100000;
        uint64_t seed = 42;
        std::vector<uint64_t> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = wyrand(seed); miss_keys[i] = wyrand(seed); }

        for (auto reserve_mult : {1.2, 1.5, 2.0, 3.0, 4.0}) {
            emilib4_opt::HashMap<uint64_t, uint64_t> em;
            boost::unordered_flat_map<uint64_t, uint64_t> bm;
            em.reserve((size_t)(N * reserve_mult));
            bm.reserve((size_t)(N * reserve_mult));
            for (auto& k : keys) { em[k] = 1; bm[k] = 1; }

            auto em_hit = measure_find_hit(em, keys, 5);
            auto bm_hit = measure_find_hit(bm, keys, 5);
            auto em_miss = measure_find_miss(em, miss_keys, 5);
            auto bm_miss = measure_find_miss(bm, miss_keys, 5);

            double lf = (double)N / em.bucket_count();
            printf("  %-8.1f %12lld %12lld %7.2fx %12lld %12lld %7.2fx\n",
                   lf, (long long)em_hit, (long long)bm_hit, (double)em_hit / bm_hit,
                   (long long)em_miss, (long long)bm_miss, (double)em_miss / bm_miss);
        }
    }

    printf("\n=== Done ===\n");
    return 0;
}
