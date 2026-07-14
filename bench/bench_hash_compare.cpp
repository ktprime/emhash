/**
 * Benchmark: emihmap4 with different hash functions
 * Tests: std::hash, wyhash, FNV1a, identity, mulx-only
 * Key types: int64_t, string
 */

#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <functional>

// ─── Hash function implementations ───────────────────────────────────

struct IdentityHash {
    size_t operator()(uint64_t k) const { return static_cast<size_t>(k); }
};

struct FNV1aHash {
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

// wyhash final3 (minimal)
struct WyHash {
    static inline uint64_t wyrot(uint64_t v) { return (v >> 33) | (v << 31); }
    static inline uint64_t wymix(uint64_t a, uint64_t b) {
        uint64_t r = a ^ 0x53c5ca59u; r *= b ^ 0x74743c1bu;
        return r ^ (r >> 33);
    }
    size_t operator()(uint64_t k) const {
        uint64_t s = 0xa0761d6478bd642full + k;
        return wymix(s ^ 0xe7037ed1a0b428dbull, wyrot(s));
    }
    size_t operator()(const std::string& s) const {
        uint64_t h = 0xa0761d6478bd642full;
        for (char c : s) {
            h = wymix(h ^ (unsigned char)c, 0xe7037ed1a0b428dbull);
        }
        return h;
    }
};

// Mulx-only hash (no std::hash, direct mulx mixing)
struct MulxHash {
    size_t operator()(uint64_t k) const {
        return emilib4::mulx_hash(k);
    }
};

// Mark which hashes are avalanching
struct IdentityHash_A : IdentityHash { using is_avalanching = void; };
struct FNV1aHash_A : FNV1aHash { using is_avalanching = void; };
struct WyHash_A : WyHash { using is_avalanching = void; };
struct MulxHash_A : MulxHash { using is_avalanching = void; };

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

    printf("  %-30s %8lld %8lld %8lld %8lld\n", name,
           (long long)t_ins, (long long)t_hit, (long long)t_miss, (long long)t_era);
}

// ─── main ─────────────────────────────────────────────────────────────

int main() {
    using K = uint64_t;
    using V = uint64_t;

    printf("=== emihmap4 hash function comparison: int64_t key (us) ===\n");
    printf("  %-30s %8s %8s %8s %8s\n", "Hash Function", "Insert", "FindHit", "FindMiss", "Erase");
    printf("  %s\n", std::string(78, '-').c_str());

    for (auto N : {1000000, 10000000}) {
        std::mt19937_64 rng(42);
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

        printf("\n  [Random int64, N = %d]\n", N);

        // Non-avalanching hashes (emihmap4 auto-applies mulx_mix)
        run_bench<emilib4::HashMap<K, V, std::hash<K>>>("std::hash (auto-mulx)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, IdentityHash>>("identity (auto-mulx)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, FNV1aHash>>("FNV1a (auto-mulx)", keys, miss_keys);

        // Avalanching hashes (no mulx mixing applied)
        run_bench<emilib4::HashMap<K, V, WyHash_A>>("wyhash (avalanching)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, FNV1aHash_A>>("FNV1a (avalanching)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, MulxHash_A>>("mulx (avalanching)", keys, miss_keys);
    }

    // Sequential key test
    printf("\n\n=== emihmap4 hash function comparison: SEQUENTIAL int64_t key (us) ===\n");
    printf("  %-30s %8s %8s %8s %8s\n", "Hash Function", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {1000000, 10000000}) {
        std::vector<K> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) { keys[i] = i + 1; miss_keys[i] = N + i + 1; }

        printf("\n  [Sequential int64, N = %d]\n", N);

        run_bench<emilib4::HashMap<K, V, std::hash<K>>>("std::hash (auto-mulx)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, IdentityHash>>("identity (auto-mulx)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, FNV1aHash>>("FNV1a (auto-mulx)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, WyHash_A>>("wyhash (avalanching)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, FNV1aHash_A>>("FNV1a (avalanching)", keys, miss_keys);
        run_bench<emilib4::HashMap<K, V, MulxHash_A>>("mulx (avalanching)", keys, miss_keys);
    }

    // String key test
    printf("\n\n=== emihmap4 hash function comparison: string key (us) ===\n");
    printf("  %-30s %8s %8s %8s %8s\n", "Hash Function", "Insert", "FindHit", "FindMiss", "Erase");

    for (auto N : {64000, 1000000}) {
        std::vector<std::string> keys(N), miss_keys(N);
        for (int i = 0; i < N; i++) {
            keys[i] = "key_" + std::to_string(i + 1);
            miss_keys[i] = "miss_" + std::to_string(i + 1);
        }

        printf("\n  [String, N = %d]\n", N);

        using SK = std::string;
        run_bench<emilib4::HashMap<SK, V, std::hash<SK>>>("std::hash (auto-mulx)", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, FNV1aHash>>("FNV1a (auto-mulx)", keys, miss_keys);
        run_bench<emilib4::HashMap<SK, V, WyHash_A>>("wyhash (avalanching)", keys, miss_keys);
    }

    return 0;
}
