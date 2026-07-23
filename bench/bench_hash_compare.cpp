/**
 * Benchmark emihmap4 vs boost::flat_map with different external hash functions
 * Hash functions: std::hash, wyhash, rapidhash, komihash, a5hash, FNV1a
 */

#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <functional>

// ─── Hash function wrappers ──────────────────────────────────────────

// 1. FNV1a (non-avalanching)
struct FNV1aHash {
    size_t operator()(uint64_t k) const noexcept {
        size_t h = 14695981039346656037ull;
        for (int i = 0; i < 8; i++) {
            h ^= (k >> (i * 8)) & 0xFF;
            h *= 1099511628211ull;
        }
        return h;
    }
    size_t operator()(const std::string& s) const noexcept {
        size_t h = 14695981039346656037ull;
        for (char c : s) { h ^= (size_t)c; h *= 1099511628211ull; }
        return h;
    }
};

// 2. wyhash (avalanching)
#include "ExcaliburHash/wyhash.h"
struct WyhashWrapper {
    using is_avalanching = void; // tell emihmap4 to skip mulx
    size_t operator()(uint64_t k) const noexcept { return wyhash(&k, sizeof(k), 0x9E3779B97F4A7C15ull); }
    size_t operator()(const std::string& s) const noexcept { return wyhash(s.data(), s.size(), 0x9E3779B97F4A7C15ull); }
};

// 3. rapidhash (avalanching)
#include "rapidhash/rapidhash.h"
struct RapidhashWrapper {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const noexcept { return rapidhash(&k, sizeof(k)); }
    size_t operator()(const std::string& s) const noexcept { return rapidhash(s.data(), s.size()); }
};

// 4. komihash (avalanching)
#include "komihash.h"
struct KomihashWrapper {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const noexcept { return komihash(&k, sizeof(k), 0x9E3779B97F4A7C15ull); }
    size_t operator()(const std::string& s) const noexcept { return komihash(s.data(), s.size(), 0x9E3779B97F4A7C15ull); }
};

// 5. a5hash (avalanching)
#include "a5hash.h"
struct A5hashWrapper {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const noexcept { return a5hash(&k, sizeof(k), 0x9E3779B97F4A7C15ull); }
    size_t operator()(const std::string& s) const noexcept { return a5hash(s.data(), s.size(), 0x9E3779B97F4A7C15ull); }
};

// 6. cityhash (from abseil, avalanching)
struct CityhashWrapper {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const noexcept;
    size_t operator()(const std::string& s) const noexcept;
};

// ─── cityhash impl ──────────────────────────────────────────────────
// Minimal CityHash64 for benchmarking
static inline uint64_t Uint128Low64(__uint128_t x) { return (uint64_t)x; }
static inline uint64_t Uint128High64(__uint128_t x) { return (uint64_t)(x >> 64); }
static inline uint64_t Hash128to64(__uint128_t x) {
    uint64_t a = Uint128Low64(x) * 0x9ddfea08eb382d69ull;
    uint64_t b = Uint128High64(x);
    a += b; b ^= a >> 47; a *= 0x9ddfea08eb382d69ull;
    b ^= a; b *= 0x9ddfea08eb382d69ull;
    return b ^ (a >> 31);
}
static inline uint64_t HashLen16(uint64_t u, uint64_t v) { return Hash128to64((__uint128_t)v << 64 | u); }
static inline uint64_t Fetch64(const char* p) { uint64_t r; memcpy(&r, p, 8); return r; }
static inline uint64_t k0 = 0xc3a5c85c97b5adddull;
static inline uint64_t k1 = 0xb492b66fbe98f273ull;
static inline uint64_t k2 = 0x9ae16a3b2f90404full;
static inline uint64_t HashLen0to16(const char* s, size_t len) {
    if (len >= 8) {
        uint64_t mul = k2 + len * 2;
        uint64_t a = Fetch64(s) + k2;
        uint64_t b = Fetch64(s + len - 8);
        uint64_t c = (b >> 37) * mul + a;
        uint64_t d = ((a >> 25 + b) * mul);
        return HashLen16(c, d);
    }
    if (len >= 4) {
        uint64_t mul = k2 + len * 2;
        uint64_t a = *(uint32_t*)s;
        return HashLen16(len + (a << 3), *(uint32_t*)(s + len - 4) * mul);
    }
    if (len > 0) return k2 + len * 2 + (uint8_t)s[0] + ((uint8_t)s[len > 1 ? 1 : 0] << 8);
    return k2;
}

size_t CityhashWrapper::operator()(uint64_t k) const noexcept {
    return HashLen16(k + k2, k * k0);
}
size_t CityhashWrapper::operator()(const std::string& s) const noexcept {
    return HashLen0to16(s.data(), s.size());
}

// ─── Benchmark harness ───────────────────────────────────────────────

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
    for (auto& k : keys) { auto it = m.find(k); if (it != m.end()) sink += 1; }
    auto t_hit = now_us() - t0;

    t0 = now_us();
    for (auto& k : miss_keys) { if (m.find(k) == m.end()) sink++; }
    auto t_miss = now_us() - t0;

    t0 = now_us();
    for (auto& k : keys) m.erase(k);
    auto t_era = now_us() - t0;

    printf("  %-34s %8lld %8lld %8lld %8lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

// ─── Main ────────────────────────────────────────────────────────────

int main() {
    using K = uint64_t;
    using SK = std::string;

    printf("================================================================\n");
    printf("  emihmap4 vs boost::flat_map with different hash functions\n");
    printf("================================================================\n");

    // ─── Random int64_t 10M ──────────────────────────────────────────
    printf("\n=== Random int64_t key, N=10M (us) ===\n");
    {
        constexpr int N = 10000000;
        std::mt19937_64 rng(42);
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

        printf("  %-34s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

        // std::hash
        run_bench<emilib4::HashMap<K, int, std::hash<K>>>("emihmap4 std::hash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, int, std::hash<K>>>("boost  std::hash", keys, miss_keys);

        // FNV1a
        run_bench<emilib4::HashMap<K, int, FNV1aHash>>("emihmap4 FNV1a", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, int, FNV1aHash>>("boost  FNV1a", keys, miss_keys);

        // wyhash
        run_bench<emilib4::HashMap<K, int, WyhashWrapper>>("emihmap4 wyhash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, int, WyhashWrapper>>("boost  wyhash", keys, miss_keys);

        // rapidhash
        run_bench<emilib4::HashMap<K, int, RapidhashWrapper>>("emihmap4 rapidhash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, int, RapidhashWrapper>>("boost  rapidhash", keys, miss_keys);

        // komihash
        run_bench<emilib4::HashMap<K, int, KomihashWrapper>>("emihmap4 komihash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, int, KomihashWrapper>>("boost  komihash", keys, miss_keys);

        // a5hash
        run_bench<emilib4::HashMap<K, int, A5hashWrapper>>("emihmap4 a5hash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, int, A5hashWrapper>>("boost  a5hash", keys, miss_keys);

        // cityhash
        run_bench<emilib4::HashMap<K, int, CityhashWrapper>>("emihmap4 cityhash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<K, int, CityhashWrapper>>("boost  cityhash", keys, miss_keys);
    }

    // ─── String key 1M ─────────────────────────────────────────────
    printf("\n=== String key, N=1M (us) ===\n");
    {
        constexpr int N = 1000000;
        std::vector<std::string> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = "key_" + std::to_string(i + 1);
            miss_keys[i] = "miss_" + std::to_string(i + 1);
        }

        printf("  %-34s %8s %8s %8s %8s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase");

        // std::hash
        run_bench<emilib4::HashMap<SK, int, std::hash<SK>>>("emihmap4 std::hash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, int, std::hash<SK>>>("boost  std::hash", keys, miss_keys);

        // FNV1a
        run_bench<emilib4::HashMap<SK, int, FNV1aHash>>("emihmap4 FNV1a", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, int, FNV1aHash>>("boost  FNV1a", keys, miss_keys);

        // wyhash
        run_bench<emilib4::HashMap<SK, int, WyhashWrapper>>("emihmap4 wyhash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, int, WyhashWrapper>>("boost  wyhash", keys, miss_keys);

        // rapidhash
        run_bench<emilib4::HashMap<SK, int, RapidhashWrapper>>("emihmap4 rapidhash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, int, RapidhashWrapper>>("boost  rapidhash", keys, miss_keys);

        // komihash
        run_bench<emilib4::HashMap<SK, int, KomihashWrapper>>("emihmap4 komihash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, int, KomihashWrapper>>("boost  komihash", keys, miss_keys);

        // a5hash
        run_bench<emilib4::HashMap<SK, int, A5hashWrapper>>("emihmap4 a5hash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, int, A5hashWrapper>>("boost  a5hash", keys, miss_keys);

        // cityhash
        run_bench<emilib4::HashMap<SK, int, CityhashWrapper>>("emihmap4 cityhash", keys, miss_keys);
        run_bench<boost::unordered_flat_map<SK, int, CityhashWrapper>>("boost  cityhash", keys, miss_keys);
    }

    return 0;
}
