/**
 * Benchmark: emihmap4 with various well-known external hash functions
 *
 * Hash functions tested:
 *   std::hash    - STL default (identity for int, platform-specific for string)
 *   wyhash v4.2  - Wang Yi's fast hash (used by Lua, Zig, etc.)
 *   rapidhash v3 - Nicoshev's improved wyhash variant
 *   komihash 5.x - Aleksey Vaneev's hash (high quality, no multiply dependency)
 *   a5hash 5.x   - Aleksey Vaneev's latest (AVX-512 optimized)
 *   CityHash64   - Google's CityHash (used by various Google systems)
 *   FNV1a        - Classic non-cryptographic hash (baseline)
 *
 * Key types: int64_t (random + sequential), std::string
 * All hash functors marked as avalanching to skip emihmap4's internal mulx_mix
 */

#include "emilib/emihmap4.hpp"

// External hash headers — use specific include order to avoid conflicts
// emhash's config.hpp defines its own wyhash in emhash_detail namespace,
// so we need to wrap the third-party wyhash include

// Forward-declare to avoid conflict
namespace ext_hash {
#include "wyhash.h"
}

#include "rapidhash.h"
#include "komihash.h"
#include "a5hash.h"

// CityHash needs a .cpp — we'll inline a minimal CityHash64 for int keys
// and use a simple wrapper for string keys

#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <functional>
#include <cstring>

// ─── Hash functors (all marked as avalanching) ────────────────────────

struct StdHashAvalanching {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const { return std::hash<uint64_t>()(k); }
    size_t operator()(const std::string& s) const { return std::hash<std::string>()(s); }
};

struct WyHashFunc {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const { return ext_hash::wyhash64(k, 0); }
    size_t operator()(const std::string& s) const { return ext_hash::wyhash(s.data(), s.size(), 0); }
};

struct RapidHashFunc {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const { return rapidhash(&k, sizeof(k)); }
    size_t operator()(const std::string& s) const { return rapidhash(s.data(), s.size()); }
};

struct KomiHashFunc {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const { return komihash(&k, sizeof(k), 0); }
    size_t operator()(const std::string& s) const { return komihash(s.data(), s.size(), 0); }
};

struct A5HashFunc {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const { return a5hash(&k, sizeof(k), 0); }
    size_t operator()(const std::string& s) const { return a5hash(s.data(), s.size(), 0); }
};

// Minimal inline CityHash64 for 8-byte input
struct CityHashFunc {
    using is_avalanching = void;
    // For int64_t, use a simple but effective mixing
    static inline uint64_t cityhash64_int(uint64_t k) {
        // absl::Hash internal for integers: k * 0x9ddfea08eb382d69
        // We use a similar multiply-shift approach
        const uint64_t mul = 0x9ddfea08eb382d69ULL;
        uint64_t a = (k ^ 0x9e3779b97f4a7c15ULL) * mul;
        a ^= (a >> 47);
        return a;
    }
    size_t operator()(uint64_t k) const { return cityhash64_int(k); }
    size_t operator()(const std::string& s) const {
        // Use wyhash as fallback for string (CityHash needs compiled .c)
        return ext_hash::wyhash(s.data(), s.size(), 0);
    }
};

struct FNV1aHashFunc {
    using is_avalanching = void;
    size_t operator()(uint64_t k) const {
        size_t h = 0xcbf29ce484222325ull;
        for (int i = 0; i < 8; i++) {
            h ^= (k >> (i * 8)) & 0xFF;
            h *= 0x100000001b3ull;
        }
        return h;
    }
    size_t operator()(const std::string& s) const {
        size_t h = 0xcbf29ce484222325ull;
        for (char c : s) { h ^= (unsigned char)c; h *= 0x100000001b3ull; }
        return h;
    }
};

// ─── timing ──────────────────────────────────────────────────────────

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ─── benchmark runner ─────────────────────────────────────────────────

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

    printf("  %-28s %8lld %8lld %8lld %8lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

// ─── main ─────────────────────────────────────────────────────────────

int main() {
    using K = uint64_t;
    using V = uint64_t;
    using SK = std::string;

    printf("====================================================================\n");
    printf("  emihmap4: External Hash Function Performance Comparison\n");
    printf("  All hashes marked is_avalanching (skip internal mulx_mix)\n");
    printf("====================================================================\n");

    // ─── Random int64_t ─────────────────────────────────────────────
    printf("\n=== Random int64_t key (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Hash", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {1000000, 10000000}) {
        std::mt19937_64 rng(42);
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

        printf("\n  [N = %d]\n", N);

        run_bench<emilib4::HashMap<K, V, StdHashAvalanching>>("std::hash", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, WyHashFunc>>("wyhash v4.2", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, RapidHashFunc>>("rapidhash v3", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, KomiHashFunc>>("komihash 5.x", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, A5HashFunc>>("a5hash 5.x", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, CityHashFunc>>("cityhash64-like", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, FNV1aHashFunc>>("FNV1a", keys, miss_keys);
    }

    // ─── Sequential int64_t ────────────────────────────────────────
    printf("\n\n=== Sequential int64_t key (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Hash", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {1000000, 10000000}) {
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = N + i + 1; }

        printf("\n  [N = %d]\n", N);

        run_bench<emilib4::HashMap<K, V, StdHashAvalanching>>("std::hash", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, WyHashFunc>>("wyhash v4.2", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, RapidHashFunc>>("rapidhash v3", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, KomiHashFunc>>("komihash 5.x", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, A5HashFunc>>("a5hash 5.x", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, CityHashFunc>>("cityhash64-like", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, FNV1aHashFunc>>("FNV1a", keys, miss_keys);
    }

    // ─── String key ────────────────────────────────────────────────
    printf("\n\n=== String key (us) ===\n");
    printf("  %-28s %8s %8s %8s %8s\n", "Hash", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000}) {
        std::vector<std::string> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = "key_" + std::to_string(i + 1);
            miss_keys[i] = "miss_" + std::to_string(i + 1);
        }

        printf("\n  [N = %d]\n", N);

        run_bench<emilib4::HashMap<SK, V, StdHashAvalanching>>("std::hash", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, WyHashFunc>>("wyhash v4.2", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, RapidHashFunc>>("rapidhash v3", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, KomiHashFunc>>("komihash 5.x", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, A5HashFunc>>("a5hash 5.x", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, CityHashFunc>>("cityhash64-like(wy)", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, FNV1aHashFunc>>("FNV1a", keys, miss_keys);
    }

    return 0;
}
