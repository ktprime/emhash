/**
 * Comprehensive benchmark: emihmap3 vs emihmap3_flat
 * - Multiple compilers: g++-11, g++-16, clang++-20
 * - Multiple hash functions: std::hash, wyhash, FNV-1a, mulx
 * - Key types: uint64_t, std::string
 * - Data sizes: 10K, 100K, 1M, 5M
 * - Key distributions: sequential, random
 * - Operations: Insert, FindHit, FindMiss, Erase
 */
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO626_REMOVED_WARNING
#define NDEBUG

#include "emilib/emihmap3.hpp"
#include "emilib/emihmap3_flat.hpp"

#include <cstdio>
#include <cstdint>
#include <chrono>
#include <vector>
#include <string>
#include <functional>

// ─── Hash functors ───────────────────────────────────────────────────
struct WyHash64 {
    size_t operator()(uint64_t x) const noexcept {
        x = (x ^ 0x9E3779B97F4A7C15ULL) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 31)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }
    size_t operator()(const std::string& s) const noexcept {
        uint64_t h = 0xa0761d6478bd642fULL;
        for (size_t i = 0; i < s.size(); i++)
            h = (h ^ (uint64_t(s[i]) << (8 * (i & 7)))) * 0x9E3779B97F4A7C15ULL;
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
        return h;
    }
};

struct Fnv1aHash {
    size_t operator()(uint64_t x) const noexcept {
        size_t h = 0xcbf29ce484222325ULL;
        for (int i = 0; i < 8; i++) { h ^= (x >> (i*8)) & 0xFF; h *= 0x100000001b3ULL; }
        return h;
    }
    size_t operator()(const std::string& s) const noexcept {
        size_t h = 0xcbf29ce484222325ULL;
        for (auto c : s) { h ^= (size_t)c; h *= 0x100000001b3ULL; }
        return h;
    }
};

struct MulxHash {
    static inline uint64_t mulx(uint64_t x) noexcept {
        // Platform-independent mulx using __uint128_t or _umul128
#if defined(_MSC_VER) && defined(_M_X64)
        uint64_t hi; uint64_t lo = _umul128(x, 0x9e3779b97f4a7c15ULL, &hi);
        return hi ^ lo;
#elif defined(__SIZEOF_INT128__)
        __uint128_t r = (__uint128_t)x * (__uint128_t)0x9e3779b97f4a7c15ULL;
        return (uint64_t)(r >> 64) ^ (uint64_t)r;
#else
        // Fallback: FNV-like
        x = (x ^ 0x9e3779b97f4a7c15ULL) * 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 31; x *= 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
#endif
    }
    size_t operator()(uint64_t x) const noexcept {
        return mulx(x + 0x9e3779b97f4a7c15ULL);
    }
    size_t operator()(const std::string& s) const noexcept {
        return mulx(std::hash<std::string>()(s) + 0x9e3779b97f4a7c15ULL);
    }
};

// ─── Timing ──────────────────────────────────────────────────────────
static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ─── RNG ─────────────────────────────────────────────────────────────
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

// ─── Benchmark functions ─────────────────────────────────────────────
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
    printf("  %-36s %10lld %10lld %10lld %10lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

// ─── Main ────────────────────────────────────────────────────────────
int main() {
#ifdef __clang__
    const char* compiler = "clang++";
#elif defined(__GNUC__)
    const char* compiler = "g++";
#else
    const char* compiler = "unknown";
#endif

    using KI = uint64_t;
    using VI = uint64_t;
    using KS = std::string;

    printf("========================================\n");
    printf("  emihmap3 vs emihmap3_flat benchmark\n");
    printf("  Compiler: %s\n", compiler);
    printf("========================================\n\n");

    // ─── 1. Sequential int64, std::hash ─────────────────────────────
    printf("=== 1. Sequential int64, std::hash ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = N + i + 1; }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI>>("emihmap3", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI>>("emihmap3_flat", keys, miss_keys, (VI)1);
    }

    // ─── 2. Random int64, std::hash ─────────────────────────────────
    printf("\n\n=== 2. Random int64, std::hash ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        WyRand rng(42);
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI>>("emihmap3", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI>>("emihmap3_flat", keys, miss_keys, (VI)1);
    }

    // ─── 3. Random int64, WyHash ────────────────────────────────────
    printf("\n\n=== 3. Random int64, WyHash ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        WyRand rng(42);
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI, WyHash64>>("emihmap3[wy]", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI, WyHash64>>("emihmap3_flat[wy]", keys, miss_keys, (VI)1);
    }

    // ─── 4. Random int64, FNV-1a ────────────────────────────────────
    printf("\n\n=== 4. Random int64, FNV-1a ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        WyRand rng(42);
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI, Fnv1aHash>>("emihmap3[fnv]", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI, Fnv1aHash>>("emihmap3_flat[fnv]", keys, miss_keys, (VI)1);
    }

    // ─── 5. Random int64, MulxHash ──────────────────────────────────
    printf("\n\n=== 5. Random int64, MulxHash ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        WyRand rng(42);
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI, MulxHash>>("emihmap3[mulx]", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI, MulxHash>>("emihmap3_flat[mulx]", keys, miss_keys, (VI)1);
    }

    // ─── 6. String keys, std::hash ──────────────────────────────────
    printf("\n\n=== 6. String keys, std::hash ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000}) {
        std::vector<KS> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = "key_" + std::to_string(i + 1); miss_keys[i] = "miss_" + std::to_string(i + 1); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KS, VI>>("emihmap3", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KS, VI>>("emihmap3_flat", keys, miss_keys, (VI)1);
    }

    // ─── 7. String keys, WyHash ─────────────────────────────────────
    printf("\n\n=== 7. String keys, WyHash ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000}) {
        std::vector<KS> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = "key_" + std::to_string(i + 1); miss_keys[i] = "miss_" + std::to_string(i + 1); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KS, VI, WyHash64>>("emihmap3[wy]", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KS, VI, WyHash64>>("emihmap3_flat[wy]", keys, miss_keys, (VI)1);
    }

    // ─── 8. String keys, FNV-1a ─────────────────────────────────────
    printf("\n\n=== 8. String keys, FNV-1a ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000}) {
        std::vector<KS> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = "key_" + std::to_string(i + 1); miss_keys[i] = "miss_" + std::to_string(i + 1); }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KS, VI, Fnv1aHash>>("emihmap3[fnv]", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KS, VI, Fnv1aHash>>("emihmap3_flat[fnv]", keys, miss_keys, (VI)1);
    }

    // ─── 9. Sequential int64, WyHash ────────────────────────────────
    printf("\n\n=== 9. Sequential int64, WyHash ===\n");
    printf("  %-36s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");
    for (int N : {10000, 100000, 1000000, 5000000}) {
        std::vector<KI> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = N + i + 1; }
        printf("\n  [N = %d]\n", N);
        run_bench<emilib3::HashMap<KI, VI, WyHash64>>("emihmap3[wy]", keys, miss_keys, (VI)1);
        run_bench<emilib3_flat::HashMap<KI, VI, WyHash64>>("emihmap3_flat[wy]", keys, miss_keys, (VI)1);
    }

    printf("\n=== Done ===\n");
    return 0;
}
