/**
 * Benchmark emihmap2 / emihmap4 with different hash functions.
 *
 * Integer hashers (10): identity, fib, mur3, mix, rrxm, squirrel3, crc32,
 *                       wyhash64, splitmix64, stafford_mix13 (default)
 * String  hashers (6):  std::hash, wyhash, rapidhash, komihash, a5hash, FNV1a
 *
 * Key types: random int64_t, sequential int64_t, string
 * Operations: Insert / FindHit / FindMiss / Erase
 * Multiple runs per test, reports median to filter WSL noise.
 */

#include "emilib/emihmap2.hpp"
#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cstring>
#include <cstdarg>

// ─── Integer hash function implementations (from util.h) ────────────
static inline uint64_t hash_identity(uint64_t key) { return key; }

static inline uint64_t hash_fib(uint64_t key) {
#if __SIZEOF_INT128__
    __uint128_t r = (__uint128_t)key * UINT64_C(11400714819323198485);
    return (uint64_t)(r >> 64) ^ (uint64_t)r;
#else
    uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
    return (r >> 32) + r;
#endif
}

static inline uint64_t hash_mur3(uint64_t key) {
    uint64_t h = key;
    h ^= h >> 33; h *= 0xff51afd7ed558ccd;
    h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

static inline uint64_t hashmix(uint64_t key) {
    auto ror  = (key >> 32) | (key << 32);
    auto low  = key * 0xA24BAED4963EE407ull;
    auto high = ror * 0x9FB21C651E98DF25ull;
    auto mix  = low + high;
    return (mix >> 32) | (mix << 32);
}

static inline uint64_t rrxmrrxmsx_0(uint64_t v) {
    v ^= (v << 39 | v >> 25) ^ (v << 14 | v >> 50);
    v *= UINT64_C(0xA24BAED4963EE407);
    v ^= (v << 40 | v >> 24) ^ (v << 15 | v >> 49);
    v *= UINT64_C(0x9FB21C651E98DF25);
    return v ^ v >> 28;
}

static inline uint64_t squirrel3(uint64_t at) {
    constexpr uint64_t N1 = 0x9E3779B185EBCA87ULL;
    constexpr uint64_t N2 = 0xC2B2AE3D27D4EB4FULL;
    constexpr uint64_t N3 = 0x27D4EB2F165667C5ULL;
    at *= N1; at ^= (at >> 8);
    at += N2; at ^= (at << 8);
    at *= N3; at ^= (at >> 8);
    return at;
}

static inline uint64_t hash_crc32(uint64_t x) {
#if defined(__SSE4_2__) || defined(_WIN32)
#  ifdef __x86_64__
    return _mm_crc32_u64(0xFFFFFFFFULL, x);
#  else
    uint64_t crc = 0xFFFFFFFFU;
    crc = _mm_crc32_u32(crc, (uint32_t)(x & 0xFFFFFFFFULL));
    return ((int64_t)crc << 32) | _mm_crc32_u32(crc, (uint32_t)(x >> 32));
#  endif
#elif defined(__aarch64__)
    return __crc32cd(0xFFFFFFFFU, x);
#else
    return x;
#endif
}

static inline uint64_t wyhash64(uint64_t A, uint64_t B) {
    A ^= 0xa0761d6478bd642full;
    B ^= 0xe7037ed1a0b428dbull;
    A *= B;
    uint64_t h;
    uint64_t a = A;
#if __SIZEOF_INT128__
    h = (uint64_t)(((__uint128_t)a * (__uint128_t)(A >> 32 | B << 32)) >> 64);
#else
    uint64_t ha = A >> 32, hb = B >> 32, la = (uint32_t)A, lb = (uint32_t)B;
    h = (ha * hb) + ((ha * lb + la * hb) << 32) + (la * lb);
    h = h >> 64;
#endif
    A ^= B; A ^= h;
    return A;
}

static inline uint64_t hash_wyhash64(uint64_t key) {
    return wyhash64(key, UINT64_C(11400714819323198485));
}

static inline uint64_t udb_splitmix64(uint64_t x) {
    uint64_t z = x += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline uint64_t stafford_mix13(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

// ─── Integer hash functors ──────────────────────────────────────────
struct H_identity   { size_t operator()(uint64_t k) const noexcept { return (size_t)hash_identity(k); } };
struct H_fib        { size_t operator()(uint64_t k) const noexcept { return (size_t)hash_fib(k); } };
struct H_mur3       { size_t operator()(uint64_t k) const noexcept { return (size_t)hash_mur3(k); } };
struct H_mix        { size_t operator()(uint64_t k) const noexcept { return (size_t)hashmix(k); } };
struct H_rrxm       { size_t operator()(uint64_t k) const noexcept { return (size_t)rrxmrrxmsx_0(k); } };
struct H_squirrel3  { size_t operator()(uint64_t k) const noexcept { return (size_t)squirrel3(k); } };
struct H_crc32      { size_t operator()(uint64_t k) const noexcept { return (size_t)hash_crc32(k); } };
struct H_wyhash64   { size_t operator()(uint64_t k) const noexcept { return (size_t)hash_wyhash64(k); } };
struct H_splitmix   { size_t operator()(uint64_t k) const noexcept { return (size_t)udb_splitmix64(k); } };
struct H_stafford   { size_t operator()(uint64_t k) const noexcept { return (size_t)stafford_mix13(k); } };

// ─── String hash functors ───────────────────────────────────────────
#include "ExcaliburHash/wyhash.h"
#include "rapidhash/rapidhash.h"
#include "komihash.h"
#include "a5hash.h"

struct S_stdhash { size_t operator()(const std::string& s) const noexcept { return std::hash<std::string>()(s); } };
struct S_wyhash  { size_t operator()(const std::string& s) const noexcept { return wyhash(s.data(), s.size(), 0x9E3779B97F4A7C15ull); } };
struct S_rapid   { size_t operator()(const std::string& s) const noexcept { return rapidhash(s.data(), s.size()); } };
struct S_komi    { size_t operator()(const std::string& s) const noexcept { return komihash(s.data(), s.size(), 0x9E3779B97F4A7C15ull); } };
struct S_a5      { size_t operator()(const std::string& s) const noexcept { return a5hash(s.data(), s.size(), 0x9E3779B97F4A7C15ull); } };
struct S_fnv1a {
    size_t operator()(const std::string& s) const noexcept {
        size_t h = 14695981039346656037ull;
        for (char c : s) { h ^= (size_t)c; h *= 1099511628211ull; }
        return h;
    }
};

// ─── Bench harness ──────────────────────────────────────────────────
static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Output buffer: accumulate in memory, write to file at end.
// Avoids WSL stdout buffering bug that drops lines at section boundaries.
static std::string g_out;
static FILE* g_trace = nullptr;

static void out_line(const char* fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_out.append(buf);
    if (g_trace) { fputs(buf, g_trace); fflush(g_trace); }
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
    out_line("  %-26s %8lld %8lld %8lld %8lld\n", name,
             (long long)t_ins[t_ins.size() / 2],
             (long long)t_hits[t_hits.size() / 2],
             (long long)t_misses[t_misses.size() / 2],
             (long long)t_eras[t_eras.size() / 2]);
    if (g_trace) fprintf(g_trace, "[done] %s N=%zu\n", name, keys.size());
}

// ─── Test wrappers ──────────────────────────────────────────────────
template<typename K, typename V>
static void test_random_int(size_t N, int reps) {
    out_line("\n=== Random int64_t  N=%zu  (median of %d, us) ===\n", N, reps);
    out_line("  %-26s %8s %8s %8s %8s\n", "Map+Hash", "Insert", "FindHit", "FindMiss", "Erase");
    std::mt19937_64 rng(42);
    std::vector<K> keys(N), miss_keys(N);
    for (size_t i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

    run_bench<emilib2::HashMap<K, V, H_identity>>("emihmap2 identity", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_fib>>("emihmap2 fib", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_mur3>>("emihmap2 mur3", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_mix>>("emihmap2 mix", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_rrxm>>("emihmap2 rrxm", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_squirrel3>>("emihmap2 squirrel3", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_crc32>>("emihmap2 crc32", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_wyhash64>>("emihmap2 wyhash64", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_splitmix>>("emihmap2 splitmix", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_stafford>>("emihmap2 stafford", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, std::hash<K>>>("emihmap2 std::hash", keys, miss_keys, reps);
    run_bench<boost::unordered_flat_map<K, V>>("boost std::hash", keys, miss_keys, reps);
}

template<typename K, typename V>
static void test_sequential_int(size_t N, int reps) {
    out_line("\n=== Sequential int64_t  N=%zu  (median of %d, us) ===\n", N, reps);
    out_line("  %-26s %8s %8s %8s %8s\n", "Map+Hash", "Insert", "FindHit", "FindMiss", "Erase");
    std::vector<K> keys(N), miss_keys(N);
    for (size_t i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = K(-1) - i; }

    run_bench<emilib2::HashMap<K, V, H_identity>>("emihmap2 identity", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_fib>>("emihmap2 fib", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_mur3>>("emihmap2 mur3", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_mix>>("emihmap2 mix", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_rrxm>>("emihmap2 rrxm", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_squirrel3>>("emihmap2 squirrel3", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_crc32>>("emihmap2 crc32", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_wyhash64>>("emihmap2 wyhash64", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_splitmix>>("emihmap2 splitmix", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, H_stafford>>("emihmap2 stafford", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<K, V, std::hash<K>>>("emihmap2 std::hash", keys, miss_keys, reps);
    run_bench<boost::unordered_flat_map<K, V>>("boost std::hash", keys, miss_keys, reps);
}

template<typename V>
static void test_string(size_t N, int reps) {
    out_line("\n=== String  N=%zu  (median of %d, us) ===\n", N, reps);
    out_line("  %-26s %8s %8s %8s %8s\n", "Map+Hash", "Insert", "FindHit", "FindMiss", "Erase");
    std::vector<std::string> keys(N), miss_keys(N);
    for (size_t i = 0; i < N; i++) {
        keys[i] = "key_" + std::to_string(i + 1);
        miss_keys[i] = "miss_" + std::to_string(i + 1);
    }

    run_bench<emilib2::HashMap<std::string, V, S_stdhash>>("emihmap2 std::hash", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<std::string, V, S_wyhash>>("emihmap2 wyhash", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<std::string, V, S_rapid>>("emihmap2 rapidhash", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<std::string, V, S_komi>>("emihmap2 komihash", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<std::string, V, S_a5>>("emihmap2 a5hash", keys, miss_keys, reps);
    run_bench<emilib2::HashMap<std::string, V, S_fnv1a>>("emihmap2 fnv1a", keys, miss_keys, reps);
    run_bench<emilib4::HashMap<std::string, V, S_stdhash>>("emihmap4 std::hash", keys, miss_keys, reps);
    run_bench<emilib4::HashMap<std::string, V, S_wyhash>>("emihmap4 wyhash", keys, miss_keys, reps);
    run_bench<emilib4::HashMap<std::string, V, S_rapid>>("emihmap4 rapidhash", keys, miss_keys, reps);
    run_bench<boost::unordered_flat_map<std::string, V>>("boost std::hash", keys, miss_keys, reps);
}

int main(int argc, char** argv) {
    // Output file path from argv[1] (default: /tmp/bench_hash_out.txt).
    const char* out_path = argc > 1 ? argv[1] : "/tmp/bench_hash_out.txt";
    // Trace file for progress (stderr by default).
    g_trace = stderr;

    out_line("=====================================================================\n");
    out_line("  Hash function comparison on emihmap2 (10 int + 6 string hashers)\n");
    out_line("=====================================================================\n");

    test_random_int<uint64_t, uint64_t>(1000000, 5);
    test_random_int<uint64_t, uint64_t>(10000000, 3);
    test_sequential_int<uint64_t, uint64_t>(1000000, 5);
    test_sequential_int<uint64_t, uint64_t>(10000000, 3);
    test_string<uint64_t>(1000000, 5);

    // Write entire output buffer to file with a single write.
    FILE* fp = fopen(out_path, "wb");
    if (!fp) { fprintf(stderr, "Cannot open %s\n", out_path); return 1; }
    fwrite(g_out.data(), 1, g_out.size(), fp);
    fclose(fp);
    fprintf(stderr, "[main] wrote %zu bytes to %s\n", g_out.size(), out_path);
    return 0;
}
