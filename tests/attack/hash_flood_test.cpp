// Hash Flooding Attack Test for emhash7/8
//
// Realistic attack model: the attacker KNOWS the hash function used by the target
// (e.g., std::hash<int> is identity on most platforms), and crafts keys that
// collide under that specific hash function with power-of-2 bucket counts.
//
// Attack strategies tested:
//   1. Known-hash collision: attacker knows std::hash<int> is identity,
//      crafts keys that all map to the same bucket (multiples of bucket_count)
//   2. Adaptive probing: attacker discovers bucket_count via timing side-channel,
//      then crafts colliding keys
//   3. Multi-bucket concentration: attacker targets a few buckets to evade
//      naive detection while still causing O(N) degradation
//   4. Quadratic degradation proof: doubling N produces ~4x time (O(N^2))
//
// Build:
//   g++ -O2 -std=c++17 -I../../include -o hash_flood_test hash_flood_test.cpp

#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emhash/hash_set8.hpp"
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <vector>
#include <cassert>
#include <cstring>

static double now_ms() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Attack 1: Known-hash collision with std::hash<int> (identity on most platforms)
//
// Realistic scenario: Web server uses emhash<int, ...> for request tracking.
// Attacker knows: hash function is std::hash<int>, bucket count is power-of-2.
// Strategy: send request IDs that are multiples of a power-of-2, so they all
// hash to the same bucket under identity hash & mask.
//
// With identity hash: hash(k) = k, bucket = k & mask
// If mask = 1023 (bucket_count = 1024), then keys 0, 1024, 2048, ... all
// land in bucket 0.
// ============================================================================
template<typename HashMap>
static void attack_known_hash(const char* name, int N) {
    HashMap map;

    // Step 1: Attacker guesses a power-of-2 stride that matches the bucket count.
    // Even if the guess is slightly off, many keys will still collide.
    // A stride of 1024 works for bucket counts 512, 1024, 2048, etc.
    // because keys 0, 1024, 2048... all have the same low 10 bits (= 0).
    const int stride = 1024;

    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        map[i * stride] = i;
    auto t_attack_insert = now_ms() - t0;

    // Baseline: insert N random keys for comparison
    HashMap baseline;
    (void)baseline.reserve(N);
    t0 = now_ms();
    for (int i = 0; i < N; i++)
        baseline[i] = i;
    auto t_normal_insert = now_ms() - t0;

    // Find performance comparison
    t0 = now_ms();
    int found = 0;
    for (int i = 0; i < N; i++) {
        if (map.find(i * stride) != map.end()) found++;
    }
    auto t_attack_find = now_ms() - t0;

    t0 = now_ms();
    for (int i = 0; i < N; i++) {
        baseline.find(i);
    }
    auto t_normal_find = now_ms() - t0;

    printf("[%s] Known-hash attack N=%d (stride=%d):\n", name, N, stride);
    printf("  Insert: attack=%.2fms vs normal=%.2fms (%.1fx slower)\n",
           t_attack_insert, t_normal_insert,
           t_normal_insert > 0 ? t_attack_insert / t_normal_insert : 0.0);
    printf("  Find:   attack=%.2fms vs normal=%.2fms (%.1fx slower)\n",
           t_attack_find, t_normal_find,
           t_normal_find > 0 ? t_attack_find / t_normal_find : 0.0);
    printf("  load_factor=%.3f bucket_count=%u\n",
           (double)map.size() / map.bucket_count(), (unsigned)map.bucket_count());

    assert(found == N);
}

// ============================================================================
// Attack 2: Adaptive probing — discover bucket_count via timing, then attack
//
// Realistic scenario: Attacker sends a few probe requests, measures response
// time to infer bucket_count, then crafts maximally colliding keys.
// ============================================================================
template<typename HashMap>
static void attack_adaptive_probing(const char* name) {
    HashMap map;

    // Phase 1: Fill table to a known size to stabilize bucket_count
    const int fill_count = 1000;
    for (int i = 0; i < fill_count; i++)
        map[i] = i;

    const auto bucket_count = map.bucket_count();
    const auto mask = bucket_count - 1;
    printf("[%s] Adaptive attack: discovered bucket_count=%zu, mask=0x%zx\n",
           name, (size_t)bucket_count, (size_t)mask);

    // Phase 2: Craft keys that all hash to bucket 0
    // With identity hash: bucket = key & mask, so keys must be multiples of bucket_count
    const int attack_count = 5000;
    std::vector<int> attack_keys;
    for (int i = 0; i < attack_count; i++)
        attack_keys.push_back(i * (int)bucket_count);

    auto t0 = now_ms();
    for (auto k : attack_keys)
        map[k] = k;
    auto t_attack = now_ms() - t0;

    // Baseline: insert same number of random keys
    HashMap baseline;
    (void)baseline.reserve(fill_count + attack_count);
    for (int i = 0; i < fill_count; i++)
        baseline[i] = i;

    t0 = now_ms();
    for (int i = fill_count; i < fill_count + attack_count; i++)
        baseline[i] = i;
    auto t_normal = now_ms() - t0;

    printf("[%s] Adaptive attack insert %d colliding keys: %.2fms vs normal %.2fms (%.1fx)\n",
           name, attack_count, t_attack, t_normal,
           t_normal > 0 ? t_attack / t_normal : 0.0);

    // Verify find degradation
    t0 = now_ms();
    for (auto k : attack_keys)
        map.find(k);
    auto t_find_attack = now_ms() - t0;

    t0 = now_ms();
    for (int i = fill_count; i < fill_count + attack_count; i++)
        baseline.find(i);
    auto t_find_normal = now_ms() - t0;

    printf("[%s] Adaptive attack find: %.2fms vs normal %.2fms (%.1fx)\n",
           name, t_find_attack, t_find_normal,
           t_find_normal > 0 ? t_find_attack / t_find_normal : 0.0);
}

// ============================================================================
// Attack 3: Multi-bucket concentration (evasive attack)
//
// Instead of all keys in one bucket (easily detected), attacker spreads keys
// across a small number of buckets (e.g., 8 out of thousands) to evade naive
// "all-in-one-bucket" detection while still causing O(N/8) per lookup.
// ============================================================================
template<typename HashMap>
static void attack_multi_bucket(const char* name, int N, int target_buckets) {
    HashMap map;
    // With identity hash and bucket_count = power-of-2,
    // keys that share the same low bits will collide.
    // Targeting `target_buckets` buckets means we need keys whose
    // hash values differ only in the top bits.
    // E.g., with bucket_count=8192 (mask=0x1FFF), keys 0, 8192, 16384, ...
    // all map to bucket 0; keys 1, 8193, 16385, ... all map to bucket 1, etc.

    // First fill to get a stable bucket count
    for (int i = 0; i < 1000; i++)
        map[i] = i;

    const auto bucket_count = map.bucket_count();

    // Generate keys that hit only `target_buckets` distinct buckets
    auto t0 = now_ms();
    int inserted = 0;
    for (int i = 0; inserted < N; i++) {
        int key = i * (int)bucket_count + (i % target_buckets);
        map[key] = i;
        inserted++;
    }
    auto t_attack = now_ms() - t0;

    // Baseline
    HashMap baseline;
    (void)baseline.reserve(N + 1000);
    for (int i = 0; i < 1000; i++)
        baseline[i] = i;

    t0 = now_ms();
    for (int i = 1000; i < 1000 + N; i++)
        baseline[i] = i;
    auto t_normal = now_ms() - t0;

    printf("[%s] Multi-bucket attack N=%d target_buckets=%d/%zu: "
           "attack=%.2fms normal=%.2fms (%.1fx)\n",
           name, N, target_buckets, (size_t)bucket_count,
           t_attack, t_normal,
           t_normal > 0 ? t_attack / t_normal : 0.0);
}

// ============================================================================
// Attack 4: Quadratic degradation proof
//
// With N colliding keys, each find is O(N), so N finds = O(N^2).
// Doubling N should produce ~4x the time.
// ============================================================================
template<typename HashMap>
static void attack_quadratic_proof(const char* name) {
    printf("[%s] Quadratic degradation proof:\n", name);

    double prev_time = 0;
    for (int N : {2000, 4000, 8000}) {
        HashMap map;

        // Insert colliding keys: multiples of a large power-of-2
        const int stride = 4096;
        for (int i = 0; i < N; i++)
            map[i * stride] = i;

        auto t0 = now_ms();
        for (int i = 0; i < N; i++)
            map.find(i * stride);
        auto elapsed = now_ms() - t0;

        double ratio = (prev_time > 0 && elapsed > 0) ? elapsed / prev_time : 0.0;
        printf("  N=%-6d find_all=%.2fms  ratio=%.2fx %s\n",
               N, elapsed, ratio,
               ratio > 2.5 ? "<-- O(N^2) confirmed" :
               ratio > 1.5 ? "<-- super-linear" : "");

        prev_time = elapsed;
    }
}

// ============================================================================
// Baseline comparison: normal keys with default hash (no attack)
// ============================================================================
template<typename HashMap>
static void baseline_normal(const char* name, int N) {
    HashMap map;
    (void)map.reserve(N);

    auto t0 = now_ms();
    for (int i = 0; i < N; i++)
        map[i] = i;
    auto t_insert = now_ms() - t0;

    t0 = now_ms();
    for (int i = 0; i < N; i++)
        map.find(i);
    auto t_find = now_ms() - t0;

    printf("[%s] Baseline (random keys) N=%d: insert=%.2fms find=%.2fms\n",
           name, N, t_insert, t_find);
}

int main() {
    printf("=== Hash Flooding Attack Test ===\n");
    printf("Attack model: attacker KNOWS the hash function (std::hash<int> = identity)\n");
    printf("               and crafts keys that collide under hash & mask.\n\n");

    // --- Attack 1: Known-hash collision ---
    printf("--- Attack 1: Known-hash collision (stride=1024) ---\n");
    attack_known_hash<emhash7::HashMap<int, int>>("emhash7", 5000);
    attack_known_hash<emhash8::HashMap<int, int>>("emhash8", 5000);
    printf("\n");

    // --- Attack 2: Adaptive probing ---
    printf("--- Attack 2: Adaptive probing (discover bucket_count, then attack) ---\n");
    attack_adaptive_probing<emhash7::HashMap<int, int>>("emhash7");
    attack_adaptive_probing<emhash8::HashMap<int, int>>("emhash8");
    printf("\n");

    // --- Attack 3: Multi-bucket concentration ---
    printf("--- Attack 3: Multi-bucket concentration (evasive) ---\n");
    attack_multi_bucket<emhash7::HashMap<int, int>>("emhash7", 5000, 8);
    attack_multi_bucket<emhash8::HashMap<int, int>>("emhash8", 5000, 8);
    printf("\n");

    // --- Attack 4: Quadratic degradation proof ---
    printf("--- Attack 4: Quadratic degradation proof ---\n");
    attack_quadratic_proof<emhash7::HashMap<int, int>>("emhash7");
    attack_quadratic_proof<emhash8::HashMap<int, int>>("emhash8");
    printf("\n");

    // --- Baseline for comparison ---
    printf("--- Baseline: normal keys (no attack) ---\n");
    baseline_normal<emhash7::HashMap<int, int>>("emhash7", 5000);
    baseline_normal<emhash8::HashMap<int, int>>("emhash8", 5000);

    printf("\n=== Summary ===\n");
    printf("Attack vector: std::hash<int> is identity on most platforms.\n");
    printf("With power-of-2 buckets, keys like k*stride all collide (hash & mask = 0).\n");
    printf("This causes O(N) per lookup, O(N^2) for N operations.\n\n");
    printf("Mitigation options:\n");
    printf("  1. Compile with -DEMH_INT_HASH=1 (golden-ratio bit mixing)\n");
    printf("  2. Use emilib2ss with EMH_SAFE_PSL to cap probe length\n");
    printf("  3. Use a randomized hash seed per instance\n");
    printf("  4. Validate/sanitize untrusted keys before insertion\n");

    return 0;
}
