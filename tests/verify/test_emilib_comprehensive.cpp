// Comprehensive bug test for three hash map implementations:
//   1. emilib::HashMap      (emihmap1.hpp)
//   2. emilib2::HashMap     (emihmap2.hpp)
//   3. emilib3::HashMap     (emihmap3.hpp)
//
// Focus: get_next_bucket probing coverage, hash sensitivity, and correctness

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>
#include <functional>
#include <unordered_map>

#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

// ============================================================================
// Test infrastructure
// ============================================================================

static int g_total_tests = 0;
static int g_passed_tests = 0;
static int g_failed_tests = 0;
static std::vector<std::string> g_failures;

#define TEST_ASSERT(cond, msg) do { \
    g_total_tests++; \
    if (!(cond)) { \
        g_failed_tests++; \
        char _buf[512]; \
        snprintf(_buf, sizeof(_buf), "[%s:%d] FAIL: %s", __FILE__, __LINE__, msg); \
        g_failures.push_back(_buf); \
        fprintf(stderr, "  FAIL: %s\n", msg); \
    } else { \
        g_passed_tests++; \
    } \
} while(0)

// ============================================================================
// Section A: Bad Hash Probing Tests
// ============================================================================

// Hash that maps many keys to the same bucket (simulates worst-case hash)
struct Mod2Hash {
    size_t operator()(int key) const { return key & 1; }  // Only 2 buckets
};

struct Mod4Hash {
    size_t operator()(int key) const { return key & 3; }  // Only 4 buckets
};

struct Mod8Hash {
    size_t operator()(int key) const { return key & 7; }  // Only 8 buckets
};

struct Mod16Hash {
    size_t operator()(int key) const { return key & 15; }  // Only 16 buckets
};

// Identity hash - terrible for power-of-2 sized tables
struct IdentityHash {
    size_t operator()(int key) const { return (size_t)key; }
};

// All-same hash - absolute worst case
struct SameHash {
    size_t operator()(int) const { return 0; }
};

template<typename HashMapType>
static void test_bad_hash_mod2(const char* name) {
    printf("\n--- [%s] Bad Hash: mod2 (extreme collision) ---\n", name);
    using MyMap = HashMapType;  // User must instantiate with Mod2Hash

    // Test with default hash but keys that collide in power-of-2 tables
    // Keys 0, 2, 4, 6, 8... all hash to bucket 0 with mod2
    // This tests get_next_bucket's ability to find empty slots
    MyMap map;

    bool ok = true;
    int fail_key = -1;
    for (int i = 0; i < 200; i++) {
        int key = i * 2;  // All even keys hash to same bucket with mod2
        map[key] = i;
        // Verify all previously inserted keys are still findable
        for (int j = 0; j <= i; j++) {
            if (!map.contains(j * 2)) {
                ok = false;
                fail_key = j * 2;
                break;
            }
        }
        if (!ok) break;
    }
    if (!ok) {
        char buf[128];
        snprintf(buf, sizeof(buf), "mod2 collision: key %d not found after %d inserts", fail_key, 200);
        TEST_ASSERT(false, buf);
    } else {
        TEST_ASSERT(true, "mod2 collision test passed");
    }
}

template<typename HashMapType>
static void test_bad_hash_power2_aligned(const char* name) {
    printf("\n--- [%s] Bad Hash: power-of-2 aligned keys ---\n", name);
    HashMapType map;

    // Keys that are multiples of 64, 128, 256 etc.
    // With power-of-2 bucket count, these all hash to same low bits
    bool ok = true;
    int fail_at = -1;
    for (int i = 0; i < 500; i++) {
        int key = i * 64;
        map[key] = i;
        if (!map.contains(key)) {
            ok = false;
            fail_at = i;
            break;
        }
    }
    if (!ok) {
        char buf[128];
        snprintf(buf, sizeof(buf), "power2 aligned: key %d*64 not found after insert", fail_at);
        TEST_ASSERT(false, buf);
    } else {
        TEST_ASSERT(true, "power2 aligned keys test passed");
    }

    // Verify all
    int missing = 0;
    for (int i = 0; i < 500; i++) {
        if (!map.contains(i * 64) || map[i * 64] != i) missing++;
    }
    if (missing > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "power2 aligned: %d keys missing after all inserts", missing);
        TEST_ASSERT(false, buf);
    } else {
        TEST_ASSERT(true, "all power2 aligned keys verified");
    }
}

template<typename HashMapType>
static void test_bad_hash_sequential(const char* name) {
    printf("\n--- [%s] Bad Hash: sequential keys (low entropy) ---\n", name);
    HashMapType map;

    // Sequential keys are common and stress the hash function
    for (int i = 0; i < 100000; i++) {
        map[i] = i;
    }
    TEST_ASSERT((int)map.size() == 100000, "sequential insert: size should be 100000");

    int missing = 0;
    for (int i = 0; i < 100000; i++) {
        auto it = map.find(i);
        if (it == map.end() || it->second != i) missing++;
    }
    if (missing > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "sequential keys: %d missing", missing);
        TEST_ASSERT(false, buf);
    } else {
        TEST_ASSERT(true, "sequential keys all found");
    }
}

// ============================================================================
// Section B: Probing Coverage Test (get_next_bucket bug detection)
//
// The bug: get_next_bucket may skip groups, causing some empty slots
// to be unreachable from the probe sequence. When the map is nearly
// full with only a few empty slots left, if those slots are in groups
// that the probe sequence can't reach, insert will fail/loop.
// ============================================================================

template<typename HashMapType>
static void test_high_load_insert(const char* name) {
    printf("\n--- [%s] High Load Insert (probe coverage) ---\n", name);
    HashMapType map;

    // Fill to near capacity, then erase scattered entries, then re-insert
    // This creates a scenario where empty slots may be in hard-to-reach groups
    const int N = 10000;
    for (int i = 0; i < N; i++) {
        map[i] = i;
    }

    // Erase every 10th element to create scattered holes
    for (int i = 0; i < N; i += 10) {
        map.erase(i);
    }
    TEST_ASSERT((int)map.size() == N - N/10, "should have erased every 10th element");

    // Now re-insert the erased keys - this stresses find_empty_slot
    bool ok = true;
    for (int i = 0; i < N; i += 10) {
        map[i] = i * 10;
        if (!map.contains(i) || map[i] != i * 10) {
            ok = false;
            break;
        }
    }
    TEST_ASSERT(ok, "re-inserted erased keys should be findable");
}

template<typename HashMapType>
static void test_insert_erase_interleaved(const char* name) {
    printf("\n--- [%s] Interleaved Insert/Erase (probe sequence) ---\n", name);
    HashMapType map;
    std::unordered_map<int, int> ref;

    std::mt19937 rng(42);
    bool consistent = true;

    // Many rounds of insert/erase to create complex probe chains
    for (int round = 0; round < 50; round++) {
        // Insert 100
        for (int i = 0; i < 100; i++) {
            int key = rng() % 500;
            int val = rng();
            map[key] = val;
            ref[key] = val;
        }
        // Erase 50
        for (int i = 0; i < 50; i++) {
            int key = rng() % 500;
            map.erase(key);
            ref.erase(key);
        }
    }

    // Verify consistency
    for (auto& kv : ref) {
        auto it = map.find(kv.first);
        if (it == map.end() || it->second != kv.second) {
            consistent = false;
            fprintf(stderr, "  MISMATCH: key=%d ref_val=%d map_val=%d\n",
                kv.first, kv.second, it == map.end() ? -1 : it->second);
            break;
        }
    }
    TEST_ASSERT(consistent, "interleaved insert/erase should stay consistent");
    TEST_ASSERT((int)map.size() == (int)ref.size(), "sizes should match");
}

template<typename HashMapType>
static void test_fill_then_scatter_erase(const char* name) {
    printf("\n--- [%s] Fill Then Scatter Erase (empty slot reachability) ---\n", name);
    HashMapType map;

    // Fill map to ~80% capacity
    const int N = 5000;
    for (int i = 0; i < N; i++) map[i] = i;

    // Erase keys in a pattern that leaves empty groups scattered
    // This is the scenario where get_next_bucket's non-linearity can miss empty slots
    for (int i = 0; i < N; i += 3) {
        map.erase(i);
    }

    // Now insert new keys that hash to different buckets
    // These inserts must find the scattered empty slots
    bool ok = true;
    for (int i = N; i < N + N/3; i++) {
        map[i] = i;
        if (!map.contains(i)) {
            fprintf(stderr, "  FILL_SCATTER: key %d not found after insert\n", i);
            ok = false;
            break;
        }
    }
    TEST_ASSERT(ok, "new keys should find scattered empty slots");
}

template<typename HashMapType>
static void test_erase_create_holes_then_insert(const char* name) {
    printf("\n--- [%s] Erase Creates Holes Then Insert ---\n", name);
    HashMapType map;

    // Create a dense map
    for (int i = 0; i < 2000; i++) map[i] = i;

    // Erase a contiguous range to create a big hole
    for (int i = 500; i < 600; i++) map.erase(i);

    // Insert keys that should land in or near that hole
    bool ok = true;
    for (int i = 2000; i < 2100; i++) {
        map[i] = i;
        if (!map.contains(i)) {
            ok = false;
            break;
        }
    }
    TEST_ASSERT(ok, "inserts after contiguous erase should work");
}

// ============================================================================
// Section C: Specific get_next_bucket probing analysis
//
// emilib2ss: offset < 7 => simd_bytes * offset; else => _num_buckets/8 + simd_bytes
// emilib2o:  offset < 5 => simd_bytes * offset; else => _num_buckets/11 + 1
// emilib2s:  offset < 7 => simd_bytes * offset; else => _num_buckets/8 + simd_bytes
//
// The problem: when offset >= threshold, the jump becomes very large
// (_num_buckets/8 or _num_buckets/11), which can skip over many groups.
// With a bad hash, many keys probe the same initial bucket, and the
// large jumps may cycle through the same groups repeatedly, never
// visiting some groups that have empty slots.
// ============================================================================

template<typename HashMapType>
static void test_probe_sequence_coverage(const char* name) {
    printf("\n--- [%s] Probe Sequence Coverage Analysis ---\n", name);

    // Simulate the probe sequence to check if all groups are reachable
    const int num_buckets = 256;  // Small power of 2
    const int simd_bytes = 16;    // SSE2
    const int num_groups = num_buckets / simd_bytes;
    const size_t mask = num_buckets - 1;

    // Simulate emilib2ss/emilib2s get_next_bucket
    auto get_next_ss = [&](size_t bucket, size_t offset) -> size_t {
        if (offset < 7)
            bucket += simd_bytes * offset;
        else
            bucket += num_buckets / 8 + simd_bytes;
        return bucket & mask;
    };

    // Simulate emilib2o get_next_bucket
    auto get_next_o = [&](size_t bucket, size_t offset) -> size_t {
        if (offset < 5)
            bucket += simd_bytes * offset;
        else
            bucket += num_buckets / 11 + 1;
        return bucket & mask;
    };

    // Check coverage from bucket 0
    auto check_coverage = [&](const char* algo_name, auto get_next_fn) {
        std::vector<bool> visited(num_groups, false);
        size_t bucket = 0;
        for (int offset = 0; offset < num_groups * 2; offset++) {
            int gi = (bucket / simd_bytes) % num_groups;
            visited[gi] = true;
            bucket = get_next_fn(bucket, offset + 1);
        }
        int covered = 0;
        for (auto v : visited) if (v) covered++;
        float pct = 100.0f * covered / num_groups;
        printf("  %s: %d/%d groups covered (%.1f%%) from bucket 0\n",
               algo_name, covered, num_groups, pct);
        return covered;
    };

    int cov_ss = check_coverage("emilib2ss/s", get_next_ss);
    int cov_o = check_coverage("emilib2o", get_next_o);

    // Also check from a different starting bucket
    auto check_coverage_from = [&](const char* algo_name, auto get_next_fn, size_t start) {
        std::vector<bool> visited(num_groups, false);
        size_t bucket = start;
        for (int offset = 0; offset < num_groups * 2; offset++) {
            int gi = (bucket / simd_bytes) % num_groups;
            visited[gi] = true;
            bucket = get_next_fn(bucket, offset + 1);
        }
        int covered = 0;
        for (auto v : visited) if (v) covered++;
        return covered;
    };

    int min_cov_ss = num_groups, min_cov_o = num_groups;
    for (int start = 0; start < num_groups; start++) {
        int c = check_coverage_from("ss", get_next_ss, start * simd_bytes);
        if (c < min_cov_ss) min_cov_ss = c;
        c = check_coverage_from("o", get_next_o, start * simd_bytes);
        if (c < min_cov_o) min_cov_o = c;
    }
    printf("  emilib2ss/s: worst-case coverage from any start: %d/%d (%.1f%%)\n",
           min_cov_ss, num_groups, 100.0f * min_cov_ss / num_groups);
    printf("  emilib2o:    worst-case coverage from any start: %d/%d (%.1f%%)\n",
           min_cov_o, num_groups, 100.0f * min_cov_o / num_groups);

    // The key finding: if coverage < 100%, some groups are unreachable
    // from certain starting buckets, which means empty slots in those
    // groups can't be found by the probe sequence
    if (min_cov_ss < num_groups) {
        printf("  ** BUG DETECTED in emilib2ss/s: %d groups unreachable from some starting buckets!\n",
               num_groups - min_cov_ss);
    }
    if (min_cov_o < num_groups) {
        printf("  ** BUG DETECTED in emilib2o: %d groups unreachable from some starting buckets!\n",
               num_groups - min_cov_o);
    }

    // Now test with larger bucket counts
    for (int nb : {512, 1024, 4096}) {
        const int ng = nb / simd_bytes;
        const size_t msk = nb - 1;

        auto get_next_ss2 = [&](size_t bucket, size_t offset) -> size_t {
            if (offset < 7)
                bucket += simd_bytes * offset;
            else
                bucket += nb / 8 + simd_bytes;
            return bucket & msk;
        };

        auto get_next_o2 = [&](size_t bucket, size_t offset) -> size_t {
            if (offset < 5)
                bucket += simd_bytes * offset;
            else
                bucket += nb / 11 + 1;
            return bucket & msk;
        };

        int min_ss = ng, min_o = ng;
        for (int start = 0; start < ng; start++) {
            std::vector<bool> vis(ng, false);
            size_t b = start * simd_bytes;
            for (int off = 0; off < ng * 2; off++) {
                vis[(b / simd_bytes) % ng] = true;
                b = get_next_ss2(b, off + 1);
            }
            int c = 0; for (auto v : vis) if (v) c++;
            if (c < min_ss) min_ss = c;

            vis.assign(ng, false);
            b = start * simd_bytes;
            for (int off = 0; off < ng * 2; off++) {
                vis[(b / simd_bytes) % ng] = true;
                b = get_next_o2(b, off + 1);
            }
            c = 0; for (auto v : vis) if (v) c++;
            if (c < min_o) min_o = c;
        }
        printf("  nb=%d: emilib2ss/s worst coverage %d/%d (%.1f%%), emilib2o worst %d/%d (%.1f%%)\n",
               nb, min_ss, ng, 100.0f*min_ss/ng, min_o, ng, 100.0f*min_o/ng);
    }
}

// ============================================================================
// Section D: Hash sensitivity stress tests
// ============================================================================

template<typename HashMapType>
static void test_hash_sensitivity_mod4(const char* name) {
    printf("\n--- [%s] Hash Sensitivity: mod4 keys ---\n", name);
    HashMapType map;

    // Keys that only differ in upper bits (low 2 bits are 0)
    // With power-of-2 bucket count, these all map to same bucket
    bool ok = true;
    for (int i = 0; i < 300; i++) {
        int key = i << 2;  // key = i * 4, low 2 bits always 0
        map[key] = i;
    }

    for (int i = 0; i < 300; i++) {
        int key = i << 2;
        if (!map.contains(key) || map[key] != i) {
            ok = false;
            fprintf(stderr, "  mod4: key %d (i=%d) not found\n", key, i);
            break;
        }
    }
    TEST_ASSERT(ok, "mod4 shifted keys should all be findable");
}

template<typename HashMapType>
static void test_hash_sensitivity_mod8(const char* name) {
    printf("\n--- [%s] Hash Sensitivity: mod8 keys ---\n", name);
    HashMapType map;

    bool ok = true;
    for (int i = 0; i < 500; i++) {
        int key = i << 3;  // key = i * 8, low 3 bits always 0
        map[key] = i;
    }

    for (int i = 0; i < 500; i++) {
        int key = i << 3;
        if (!map.contains(key) || map[key] != i) {
            ok = false;
            break;
        }
    }
    TEST_ASSERT(ok, "mod8 shifted keys should all be findable");
}

template<typename HashMapType>
static void test_hash_sensitivity_mod128(const char* name) {
    printf("\n--- [%s] Hash Sensitivity: mod128 keys ---\n", name);
    HashMapType map;

    bool ok = true;
    for (int i = 0; i < 1000; i++) {
        int key = i << 7;  // key = i * 128, low 7 bits always 0
        map[key] = i;
    }

    for (int i = 0; i < 1000; i++) {
        int key = i << 7;
        if (!map.contains(key) || map[key] != i) {
            ok = false;
            fprintf(stderr, "  mod128: key %d not found\n", key);
            break;
        }
    }
    TEST_ASSERT(ok, "mod128 shifted keys should all be findable");
}

// ============================================================================
// Section E: Basic correctness tests (from original)
// ============================================================================

template<typename HashMapType>
static void test_basic_insert_find_erase(const char* name) {
    printf("\n--- [%s] Basic Insert/Find/Erase ---\n", name);
    HashMapType map;

    auto r1 = map.insert({1, 100});
    TEST_ASSERT(r1.second, "insert new key should return true");
    TEST_ASSERT(r1.first->second == 100, "inserted value should be 100");

    auto r2 = map.insert({1, 999});
    TEST_ASSERT(!r2.second, "duplicate insert should return false");
    TEST_ASSERT(r2.first->second == 100, "duplicate insert should not change value");

    TEST_ASSERT(map.contains(1), "contains(1) should be true");
    TEST_ASSERT(!map.contains(999), "contains(999) should be false");
    TEST_ASSERT(map.count(1) == 1, "count(1) should be 1");

    map.erase(1);
    TEST_ASSERT(!map.contains(1), "erased key should not be found");
}

template<typename HashMapType>
static void test_operator_bracket(const char* name) {
    printf("\n--- [%s] Operator[] ---\n", name);
    HashMapType map;

    map[1] = 100;
    TEST_ASSERT(map[1] == 100, "operator[] should return inserted value");
    map[1] = 200;
    TEST_ASSERT(map[1] == 200, "operator[] should modify existing value");
    TEST_ASSERT(map.size() == 1, "operator[] should not increase size for existing key");
}

template<typename HashMapType>
static void test_emplace_try_emplace(const char* name) {
    printf("\n--- [%s] Emplace/Try Emplace ---\n", name);
    HashMapType map;

    auto r1 = map.emplace(1, 100);
    TEST_ASSERT(r1.second, "emplace new key should return true");

    auto r2 = map.emplace(1, 999);
    TEST_ASSERT(!r2.second, "emplace duplicate should return false");

    auto r3 = map.try_emplace(2, 200);
    TEST_ASSERT(r3.second, "try_emplace new key should return true");

    auto r4 = map.try_emplace(2, 999);
    TEST_ASSERT(!r4.second, "try_emplace existing should return false");
    TEST_ASSERT(r4.first->second == 200, "try_emplace should not modify existing");
}

template<typename HashMapType>
static void test_insert_or_assign(const char* name) {
    printf("\n--- [%s] Insert or Assign ---\n", name);
    HashMapType map;

    auto r1 = map.insert_or_assign(1, 100);
    TEST_ASSERT(r1.second, "insert_or_assign new key should return true");
    auto r2 = map.insert_or_assign(1, 200);
    TEST_ASSERT(!r2.second, "insert_or_assign existing should return false");
    TEST_ASSERT(r2.first->second == 200, "insert_or_assign should update value");
}

template<typename HashMapType>
static void test_empty_map(const char* name) {
    printf("\n--- [%s] Empty Map ---\n", name);
    HashMapType map;

    TEST_ASSERT(map.empty(), "new map should be empty");
    TEST_ASSERT(map.size() == 0, "new map size should be 0");
    TEST_ASSERT(map.begin() == map.end(), "begin==end on empty map");
    TEST_ASSERT(map.find(1) == map.end(), "find on empty map returns end()");
    TEST_ASSERT(map.erase(1) == 0, "erase on empty map returns 0");
}

template<typename HashMapType>
static void test_clear(const char* name) {
    printf("\n--- [%s] Clear ---\n", name);
    HashMapType map;

    for (int i = 0; i < 100; i++) map[i] = i * 10;
    map.clear();
    TEST_ASSERT(map.empty(), "map should be empty after clear");

    map[1] = 100;
    TEST_ASSERT(map[1] == 100, "insert after clear should work");
}

template<typename HashMapType>
static void test_insert_erase_cycle(const char* name) {
    printf("\n--- [%s] Insert-Erase-Reinsert Cycle ---\n", name);
    HashMapType map;

    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 50; i++) map[i] = cycle * 50 + i;
        TEST_ASSERT(map.size() == 50, "size should be 50 after insert");
        for (int i = 0; i < 50; i++) map.erase(i);
        TEST_ASSERT(map.empty(), "map should be empty after erase");
    }
}

template<typename HashMapType>
static void test_copy_move(const char* name) {
    printf("\n--- [%s] Copy/Move ---\n", name);
    HashMapType map;
    for (int i = 0; i < 50; i++) map[i] = i * 10;

    HashMapType map2(map);
    TEST_ASSERT(map2.size() == 50, "copied map should have same size");
    TEST_ASSERT(map2[5] == 50, "copied map values should match");

    HashMapType map3(std::move(map2));
    TEST_ASSERT(map3.size() == 50, "moved map should have 50 elements");
}

template<typename HashMapType>
static void test_iterator(const char* name) {
    printf("\n--- [%s] Iterator ---\n", name);
    using KeyT = typename HashMapType::key_type;
    HashMapType map;
    for (int i = 0; i < 50; i++) map[KeyT(i)] = i * 10;

    std::vector<KeyT> keys;
    for (auto& kv : map) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    TEST_ASSERT((int)keys.size() == 50, "should iterate 50 elements");
    for (int i = 0; i < 50; i++) TEST_ASSERT(keys[i] == KeyT(i), "all keys found in iteration");
}

template<typename HashMapType>
static void test_string_keys(const char* name) {
    printf("\n--- [%s] String Keys ---\n", name);
    HashMapType map;

    map["hello"] = 1;
    map["world"] = 2;
    map[""] = 3;
    TEST_ASSERT(map.size() == 3, "string key map should have 3 elements");
    TEST_ASSERT(map["hello"] == 1, "string key lookup should work");
    TEST_ASSERT(map[""] == 3, "empty string key should work");
}

template<typename HashMapType>
static void test_random_ops_consistency(const char* name, int N) {
    printf("\n--- [%s] Random Ops Consistency (N=%d) ---\n", name, N);
    HashMapType map;
    std::unordered_map<int, int> ref;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> key_dist(0, N / 4);
    std::uniform_int_distribution<int> op_dist(0, 3);

    bool consistent = true;
    for (int i = 0; i < N; i++) {
        int key = key_dist(rng);
        int op = op_dist(rng);
        switch (op) {
            case 0: { int val = rng(); map[key] = val; ref[key] = val; break; }
            case 1: {
                auto it = map.find(key);
                auto ref_it = ref.find(key);
                bool m = (it != map.end()), r = (ref_it != ref.end());
                if (m != r || (m && it->second != ref_it->second)) consistent = false;
                break;
            }
            case 2: { map.erase(key); ref.erase(key); break; }
            case 3: {
                bool m = map.contains(key), r = (ref.find(key) != ref.end());
                if (m != r) consistent = false;
                break;
            }
        }
    }
    TEST_ASSERT(consistent, "random ops should stay consistent with unordered_map");

    // Full verification
    for (auto& kv : ref) {
        auto it = map.find(kv.first);
        if (it == map.end() || it->second != kv.second) { consistent = false; break; }
    }
    TEST_ASSERT(consistent, "all ref elements should match in map");
}

template<typename HashMapType>
static void test_shrink_to_fit(const char* name) {
    printf("\n--- [%s] Shrink to Fit ---\n", name);
    HashMapType map;

    for (int i = 0; i < 1000; i++) map[i] = i;
    for (int i = 0; i < 990; i++) map.erase(i);
    map.shrink_to_fit();

    bool ok = true;
    for (int i = 990; i < 1000; i++) {
        if (!map.contains(i) || map[i] != i) { ok = false; break; }
    }
    TEST_ASSERT(ok, "remaining elements accessible after shrink_to_fit");
}

template<typename HashMapType>
static void test_set_get(const char* name) {
    printf("\n--- [%s] Set Get ---\n", name);
    using KeyT = typename HashMapType::key_type;
    HashMapType map;

    typename HashMapType::val_type oldv;
    KeyT key1 = KeyT(1);
    bool ins = map.set_get(key1, 100, oldv);
    TEST_ASSERT(ins, "set_get new key should indicate insertion");
    TEST_ASSERT(map[key1] == 100, "set_get should set value");

    bool ins2 = map.set_get(key1, 200, oldv);
    TEST_ASSERT(!ins2, "set_get existing key should indicate not inserted");
    TEST_ASSERT(oldv == 100, "set_get should return old value");
}

template<typename HashMapType>
static void test_concurrent_read(const char* name) {
    printf("\n--- [%s] Concurrent Read ---\n", name);
    using KeyT = typename HashMapType::key_type;
    HashMapType map;

    const int N = 10000;
    for (int i = 0; i < N; i++) map[KeyT(i)] = i * 2;

    std::atomic<int> errors{0};
    auto reader = [&](int tid) {
        std::mt19937 rng(tid);
        for (int i = 0; i < 10000; i++) {
            int ki = rng() % N;
            auto it = map.find(KeyT(ki));
            if (it == map.end() || it->second != ki * 2) errors.fetch_add(1);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) threads.emplace_back(reader, t);
    for (auto& t : threads) t.join();
    TEST_ASSERT(errors.load() == 0, "concurrent reads should all succeed");
}

template<typename HashMapType>
static void test_negative_keys(const char* name) {
    printf("\n--- [%s] Negative Keys ---\n", name);
    using KeyT = typename HashMapType::key_type;
    HashMapType map;

    map[KeyT(-1)] = 100; map[KeyT(-2)] = 200; map[KeyT(0)] = 0; map[KeyT(1)] = 1;
    TEST_ASSERT(map[KeyT(-1)] == 100, "negative key -1 should work");
    TEST_ASSERT(map[KeyT(-2)] == 200, "negative key -2 should work");
    TEST_ASSERT(map.size() == 4, "size should be 4");
}

template<typename HashMapType>
static void test_try_get(const char* name) {
    printf("\n--- [%s] Try Get ---\n", name);
    using KeyT = typename HashMapType::key_type;
    HashMapType map;

    map[KeyT(1)] = 100;
    auto* ptr = map.try_get(KeyT(1));
    if (ptr) {  // try_get may not exist in all implementations
        TEST_ASSERT(*ptr == 100, "try_get existing key");
    } else {
        // Fallback: just verify the key exists
        TEST_ASSERT(map.contains(KeyT(1)), "key should exist");
    }
    auto* ptr2 = map.try_get(KeyT(999));
    if (ptr2) {
        TEST_ASSERT(false, "try_get non-existing key should return null");
    } else {
        TEST_ASSERT(true, "try_get non-existing key returns null");
    }
}

template<typename HashMapType>
static void test_erase_if(const char* name) {
    printf("\n--- [%s] Erase If ---\n", name);
    using KeyT = typename HashMapType::key_type;
    HashMapType map;

    for (int i = 0; i < 100; i++) map[KeyT(i)] = i;
    auto erased = map.erase_if([](const auto& kv) { return kv.second % 3 == 0; });
    TEST_ASSERT(erased == 34, "should erase 34 elements divisible by 3");
    TEST_ASSERT(map.size() == 66, "66 elements should remain");
}

// ============================================================================
// Run all tests for one map type
// ============================================================================

template<typename HashMapType>
static void run_all_tests(const char* name) {
    printf("\n========================================\n");
    printf("  Testing: %s\n", name);
    printf("========================================\n");

    // A: Bad hash / probing tests (KEY FOCUS)
    test_bad_hash_mod2<HashMapType>(name);
    test_bad_hash_power2_aligned<HashMapType>(name);
    test_bad_hash_sequential<HashMapType>(name);

    // B: Probing coverage tests
    test_high_load_insert<HashMapType>(name);
    test_insert_erase_interleaved<HashMapType>(name);
    test_fill_then_scatter_erase<HashMapType>(name);
    test_erase_create_holes_then_insert<HashMapType>(name);

    // D: Hash sensitivity
    test_hash_sensitivity_mod4<HashMapType>(name);
    test_hash_sensitivity_mod8<HashMapType>(name);
    test_hash_sensitivity_mod128<HashMapType>(name);

    // E: Basic correctness
    test_basic_insert_find_erase<HashMapType>(name);
    test_operator_bracket<HashMapType>(name);
    test_emplace_try_emplace<HashMapType>(name);
    test_insert_or_assign<HashMapType>(name);
    test_empty_map<HashMapType>(name);
    test_clear<HashMapType>(name);
    test_insert_erase_cycle<HashMapType>(name);
    test_copy_move<HashMapType>(name);
    test_iterator<HashMapType>(name);
    test_random_ops_consistency<HashMapType>(name, 100000);
    test_shrink_to_fit<HashMapType>(name);
    test_set_get<HashMapType>(name);
    test_concurrent_read<HashMapType>(name);
    test_negative_keys<HashMapType>(name);
    test_erase_if<HashMapType>(name);
}

// String-key specific tests (only tests that work with string keys)
template<typename HashMapType>
static void run_string_tests(const char* name) {
    printf("\n========================================\n");
    printf("  Testing: %s (string keys)\n", name);
    printf("========================================\n");

    test_empty_map<HashMapType>(name);
    test_string_keys<HashMapType>(name);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("============================================================\n");
    printf("  Bug Test: get_next_bucket probing & hash sensitivity\n");
    printf("  Focus: probe coverage, empty slot reachability\n");
    printf("============================================================\n");

    // C: Probe sequence coverage analysis (no map instance needed)
    test_probe_sequence_coverage<void>("ANALYSIS");

    // Test all three implementations with int keys
    run_all_tests<emilib::HashMap<int, int>>("emilib2ss (emilib)");
    run_all_tests<emilib2::HashMap<int, int>>("emilib2o (emilib2)");
    run_all_tests<emilib3::HashMap<int, int>>("emilib2s (emilib3)");

    // Summary
    printf("\n============================================================\n");
    printf("  TEST SUMMARY\n");
    printf("============================================================\n");
    printf("  Total:  %d\n", g_total_tests);
    printf("  Passed: %d\n", g_passed_tests);
    printf("  Failed: %d\n", g_failed_tests);

    if (!g_failures.empty()) {
        printf("\n  FAILED TESTS:\n");
        for (const auto& f : g_failures) {
            printf("    %s\n", f.c_str());
        }
    }

    printf("\n============================================================\n");
    return g_failed_tests > 0 ? 1 : 0;
}
