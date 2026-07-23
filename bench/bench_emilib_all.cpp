/**
 * Comprehensive emilib version comparison:
 *   emihmap1 (linear probing) vs emihmap2 (SIMD swiss-like) vs emihmap3 (SIMD aligned)
 *   vs emihmap4 (Boost-style swiss table) vs boost::flat_map (reference)
 *
 * Key types: random int64_t, sequential int64_t, string
 * Operations: Insert / FindHit / FindMiss / Erase
 * Multiple runs per test, reports median to filter WSL noise.
 */

#include "emilib/emihmap1.hpp"
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

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<typename Map, typename Keys, typename MissKeys>
static void run_bench(const char* name, const Keys& keys, const MissKeys& miss_keys, int reps) {
    std::vector<int64_t> t_ins, t_hits, t_misses, t_eras;
    for (int r = 0; r < reps; ++r) {
        Map m;
        m.rehash(keys.size() * 12 / 10);

        auto t0 = now_us();
        for (auto& k : keys) m[k] = 1;
        t_ins.push_back(now_us() - t0);

        volatile size_t sink = 0;
        t0 = now_us();
        for (auto& k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
        t_hits.push_back(now_us() - t0);

        t0 = now_us();
        for (auto& k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        t_misses.push_back(now_us() - t0);

        t0 = now_us();
        for (auto& k : keys) m.erase(k);
        t_eras.push_back(now_us() - t0);
    }
    std::sort(t_ins.begin(), t_ins.end());
    std::sort(t_hits.begin(), t_hits.end());
    std::sort(t_misses.begin(), t_misses.end());
    std::sort(t_eras.begin(), t_eras.end());
    // Use a single buffer + fputs to avoid any partial-write issues.
    char line[256];
    snprintf(line, sizeof(line), "  %-22s %8lld %8lld %8lld %8lld\n", name,
             (long long)t_ins[t_ins.size() / 2],
             (long long)t_hits[t_hits.size() / 2],
             (long long)t_misses[t_misses.size() / 2],
             (long long)t_eras[t_eras.size() / 2]);
    fputs(line, stdout);
    fflush(stdout);
    fprintf(stderr, "[done] %s N=%zu\n", name, keys.size());
}

template<typename K, typename V>
static void test_random_int(size_t N, int reps) {
    printf("\n=== Random int64_t  N=%zu  (median of %d, us) ===\n", N, reps);
    printf("  %-22s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    std::mt19937_64 rng(42);
    std::vector<K> keys(N), miss_keys(N);
    for (size_t i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }
    run_bench<emilib::HashMap<K, V>>("emihmap1", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V>>("emihmap2", keys, miss_keys, reps);
    run_bench<emilib3::HashMap<K, V>>("emihmap3", keys, miss_keys, reps);
    run_bench<emilib4::HashMap<K, V>>("emihmap4(swiss)", keys, miss_keys, reps);
    run_bench<boost::unordered_flat_map<K, V>>("boost::flat_map", keys, miss_keys, reps);
}

template<typename K, typename V>
static void test_sequential_int(size_t N, int reps) {
    printf("\n=== Sequential int64_t  N=%zu  (median of %d, us) ===\n", N, reps);
    printf("  %-22s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    std::vector<K> keys(N), miss_keys(N);
    for (size_t i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = K(-1) - i; }
    run_bench<emilib::HashMap<K, V>>("emihmap1", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V>>("emihmap2", keys, miss_keys, reps);
    run_bench<emilib3::HashMap<K, V>>("emihmap3", keys, miss_keys, reps);
    run_bench<emilib4::HashMap<K, V>>("emihmap4(swiss)", keys, miss_keys, reps);
    run_bench<boost::unordered_flat_map<K, V>>("boost::flat_map", keys, miss_keys, reps);
}

template<typename V>
static void test_string(size_t N, int reps) {
    printf("\n=== String  N=%zu  (median of %d, us) ===\n", N, reps);
    printf("  %-22s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    std::vector<std::string> keys(N), miss_keys(N);
    for (size_t i = 0; i < N; i++) {
        keys[i] = "key_" + std::to_string(i + 1);
        miss_keys[i] = "miss_" + std::to_string(i + 1);
    }
    run_bench<emilib::HashMap<std::string, V>>("emihmap1", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<std::string, V>>("emihmap2", keys, miss_keys, reps);
    run_bench<emilib3::HashMap<std::string, V>>("emihmap3", keys, miss_keys, reps);
    run_bench<emilib4::HashMap<std::string, V>>("emihmap4(swiss)", keys, miss_keys, reps);
    run_bench<boost::unordered_flat_map<std::string, V>>("boost::flat_map", keys, miss_keys, reps);
}

int main() {
    const int reps = 5;
    // Disable stdout buffering to avoid losing lines when redirected to a file.
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=====================================================================\n");
    printf("  emilib versions: emihmap1 vs emihmap2 vs emihmap3 vs emihmap4 vs boost\n");
    printf("=====================================================================");

    test_random_int<uint64_t, uint64_t>(1000000, reps);
    test_random_int<uint64_t, uint64_t>(10000000, reps);
    test_sequential_int<uint64_t, uint64_t>(1000000, reps);
    test_sequential_int<uint64_t, uint64_t>(10000000, reps);
    test_string<uint64_t>(1000000, reps);
    return 0;
}
