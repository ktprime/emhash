// Focused test: get_next_bucket probing bug with bad hash functions
// Compile: g++ -std=c++17 -O2 -I. -Ithirdparty -msse4.2 -o test_probe_bug test/test_probe_bug.cpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_map>

#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"

// ============================================================================
// Bad hash functions that expose get_next_bucket weaknesses
// ============================================================================

// All keys hash to same bucket - worst case
struct SameHash {
    size_t operator()(int) const { return 0; }
};

// Keys differ only in upper bits (low bits always 0)
// With power-of-2 bucket count, hash(key) = key, so all land in bucket 0
struct UpperBitsHash {
    size_t operator()(int key) const { return (size_t)key >> 4; }
};

// Hash with very small range
struct TinyHash {
    size_t operator()(int key) const { return (key * 2654435761u) >> 28; }
};

// Hash that produces only multiples of a large number
struct StepHash {
    size_t operator()(int key) const { return (size_t)(key * 127); }
};

// ============================================================================
// Test: insert many keys with bad hash, verify all are findable
// ============================================================================

template<typename MapType>
static int test_bad_hash(const char* name, int num_keys) {
    printf("\n[%s] Inserting %d keys with bad hash...\n", name, num_keys);
    MapType map;

    auto t0 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_keys; i++) {
        map[i] = i * 2;
    }

    auto t1 = std::chrono::high_resolution_clock::now();

    // Verify all keys
    int missing = 0;
    int wrong_val = 0;
    for (int i = 0; i < num_keys; i++) {
        auto it = map.find(i);
        if (it == map.end()) {
            missing++;
            if (missing <= 5) fprintf(stderr, "  MISSING key=%d\n", i);
        } else if (it->second != i * 2) {
            wrong_val++;
            if (wrong_val <= 5) fprintf(stderr, "  WRONG key=%d expected=%d got=%d\n", i, i*2, it->second);
        }
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    double insert_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double verify_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    printf("  Size: %u, Missing: %d, Wrong: %d, Insert: %.1fms, Verify: %.1fms\n",
           (unsigned)map.size(), missing, wrong_val, insert_ms, verify_ms);

    return missing + wrong_val;
}

// ============================================================================
// Probe sequence coverage analysis
// ============================================================================

static void analyze_probe_coverage() {
    printf("\n============================================================\n");
    printf("  PROBE SEQUENCE COVERAGE ANALYSIS\n");
    printf("============================================================\n\n");

    const int simd_bytes = 16;  // SSE2

    // Test various bucket counts
    for (int num_buckets : {128, 256, 512, 1024, 2048, 4096, 8192, 16384}) {
        const int num_groups = num_buckets / simd_bytes;
        const size_t mask = num_buckets - 1;

        // emilib2ss (new dual-probe): offset < 7 => simd_bytes * offset; 7-13 => secondary step; 14+ => tertiary
        auto get_next_ss = [&](size_t bucket, size_t offset) -> size_t {
            if (offset < 7)
                bucket += simd_bytes * offset;
            else if (offset < 14)
                bucket += (num_buckets / 7 + simd_bytes * 3) & ~(simd_bytes - 1);
            else
                bucket += num_buckets / 8 + simd_bytes;
            return bucket & mask;
        };

        // emilib2o (new dual-probe): offset < 5 => simd_bytes * offset; 5-9 => secondary; 10+ => tertiary
        auto get_next_o = [&](size_t bucket, size_t offset) -> size_t {
            if (offset < 5)
                bucket += simd_bytes * offset;
            else if (offset < 10)
                bucket += (num_buckets / 7 + simd_bytes * 3) & ~(simd_bytes - 1);
            else
                bucket += (num_buckets / 11 + 1) & ~(simd_bytes - 1);
            return bucket & mask;
        };

        // Check worst-case coverage from any starting group
        int min_cov_ss = num_groups, min_cov_o = num_groups;
        int worst_start_ss = 0, worst_start_o = 0;

        for (int start = 0; start < num_groups; start++) {
            // emilib2ss/s
            {
                std::vector<bool> visited(num_groups, false);
                size_t bucket = start * simd_bytes;
                for (int step = 0; step < num_groups * 3; step++) {
                    int gi = (bucket / simd_bytes) % num_groups;
                    visited[gi] = true;
                    bucket = get_next_ss(bucket, step + 1);
                }
                int covered = 0;
                for (auto v : visited) if (v) covered++;
                if (covered < min_cov_ss) {
                    min_cov_ss = covered;
                    worst_start_ss = start;
                }
            }

            // emilib2o
            {
                std::vector<bool> visited(num_groups, false);
                size_t bucket = start * simd_bytes;
                for (int step = 0; step < num_groups * 3; step++) {
                    int gi = (bucket / simd_bytes) % num_groups;
                    visited[gi] = true;
                    bucket = get_next_o(bucket, step + 1);
                }
                int covered = 0;
                for (auto v : visited) if (v) covered++;
                if (covered < min_cov_o) {
                    min_cov_o = covered;
                    worst_start_o = start;
                }
            }
        }

        float pct_ss = 100.0f * min_cov_ss / num_groups;
        float pct_o = 100.0f * min_cov_o / num_groups;

        printf("  nb=%5d: emilib2ss/s worst %3d/%3d (%5.1f%%, start=%3d)  emilib2o worst %3d/%3d (%5.1f%%, start=%3d)",
               num_buckets, min_cov_ss, num_groups, pct_ss, worst_start_ss,
               min_cov_o, num_groups, pct_o, worst_start_o);

        if (min_cov_ss < num_groups || min_cov_o < num_groups) {
            printf("  ** BUG: unreachable groups!");
        }
        printf("\n");
    }

    // Detailed analysis: show which groups are unreachable for a specific case
    printf("\n--- Detailed unreachable groups for nb=4096, emilib2o ---\n");
    {
        const int num_buckets = 4096;
        const int num_groups = num_buckets / simd_bytes;
        const size_t mask = num_buckets - 1;

        auto get_next_o = [&](size_t bucket, size_t offset) -> size_t {
            if (offset < 5)
                bucket += simd_bytes * offset;
            else if (offset < 10)
                bucket += (num_buckets / 7 + simd_bytes * 3) & ~(simd_bytes - 1);
            else
                bucket += (num_buckets / 11 + 1) & ~(simd_bytes - 1);
            return bucket & mask;
        };

        // Find worst starting group
        int worst_start = 0, min_cov = num_groups;
        for (int start = 0; start < num_groups; start++) {
            std::vector<bool> visited(num_groups, false);
            size_t bucket = start * simd_bytes;
            for (int step = 0; step < num_groups * 3; step++) {
                visited[(bucket / simd_bytes) % num_groups] = true;
                bucket = get_next_o(bucket, step + 1);
            }
            int covered = 0;
            for (auto v : visited) if (v) covered++;
            if (covered < min_cov) { min_cov = covered; worst_start = start; }
        }

        // Show unreachable groups
        std::vector<bool> visited(num_groups, false);
        size_t bucket = worst_start * simd_bytes;
        for (int step = 0; step < num_groups * 3; step++) {
            visited[(bucket / simd_bytes) % num_groups] = true;
            bucket = get_next_o(bucket, step + 1);
        }

        int unreachable_count = 0;
        printf("  Unreachable groups from start=%d: ", worst_start);
        for (int i = 0; i < num_groups; i++) {
            if (!visited[i]) {
                if (unreachable_count < 20) printf("%d ", i);
                unreachable_count++;
            }
        }
        if (unreachable_count > 20) printf("... (%d total)", unreachable_count);
        printf("\n");

        // Show the probe sequence for first 30 steps
        printf("  Probe sequence from start=%d (first 30 steps): ", worst_start);
        bucket = worst_start * simd_bytes;
        for (int step = 0; step < 30; step++) {
            printf("%d ", (int)(bucket / simd_bytes) % num_groups);
            bucket = get_next_o(bucket, step + 1);
        }
        printf("...\n");
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("============================================================\n");
    printf("  Focused Test: get_next_bucket probing bug\n");
    printf("============================================================\n");

    // 1. Probe coverage analysis
    analyze_probe_coverage();

    // 2. Test with bad hash functions
    printf("\n============================================================\n");
    printf("  BAD HASH FUNCTION TESTS\n");
    printf("============================================================\n");

    // Test emilib2ss with SameHash
    {
        using BadMap = emilib::HashMap<int, int, SameHash>;
        int errors = test_bad_hash<BadMap>("emilib2ss + SameHash", 500);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }

    // Test emilib2o with SameHash
    {
        using BadMap = emilib2::HashMap<int, int, SameHash>;
        int errors = test_bad_hash<BadMap>("emilib2o + SameHash", 500);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }

    // Test emilib2s with SameHash
    {
        using BadMap = emilib3::HashMap<int, int, SameHash>;
        int errors = test_bad_hash<BadMap>("emilib2s + SameHash", 500);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }

    // Test with StepHash (produces values that are multiples of 127)
    {
        using BadMap = emilib::HashMap<int, int, StepHash>;
        int errors = test_bad_hash<BadMap>("emilib2ss + StepHash", 1000);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }
    {
        using BadMap = emilib2::HashMap<int, int, StepHash>;
        int errors = test_bad_hash<BadMap>("emilib2o + StepHash", 1000);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }
    {
        using BadMap = emilib3::HashMap<int, int, StepHash>;
        int errors = test_bad_hash<BadMap>("emilib2s + StepHash", 1000);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }

    // Test with TinyHash
    {
        using BadMap = emilib::HashMap<int, int, TinyHash>;
        int errors = test_bad_hash<BadMap>("emilib2ss + TinyHash", 1000);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }
    {
        using BadMap = emilib2::HashMap<int, int, TinyHash>;
        int errors = test_bad_hash<BadMap>("emilib2o + TinyHash", 1000);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }
    {
        using BadMap = emilib3::HashMap<int, int, TinyHash>;
        int errors = test_bad_hash<BadMap>("emilib2s + TinyHash", 1000);
        printf("  Result: %s\n\n", errors == 0 ? "PASS" : "FAIL");
    }

    // 3. Stress test: insert/erase with bad hash
    printf("\n============================================================\n");
    printf("  STRESS: Insert/Erase with SameHash\n");
    printf("============================================================\n");

    {
        using BadMap = emilib2::HashMap<int, int, SameHash>;
        BadMap map;
        std::unordered_map<int, int> ref;
        bool ok = true;

        for (int round = 0; round < 20; round++) {
            // Insert 50
            for (int i = 0; i < 50; i++) {
                int key = round * 50 + i;
                map[key] = key * 2;
                ref[key] = key * 2;
            }
            // Erase 25
            for (int i = 0; i < 25; i++) {
                int key = round * 25 + i;
                map.erase(key);
                ref.erase(key);
            }
        }

        // Verify
        for (auto& kv : ref) {
            auto it = map.find(kv.first);
            if (it == map.end() || it->second != kv.second) {
                ok = false;
                fprintf(stderr, "  MISMATCH: key=%d\n", kv.first);
                break;
            }
        }
        printf("  emilib2o + SameHash stress: %s (map_size=%u ref_size=%u)\n",
               ok ? "PASS" : "FAIL", (unsigned)map.size(), (unsigned)ref.size());
    }

    printf("\n============================================================\n");
    printf("  ANALYSIS COMPLETE\n");
    printf("============================================================\n");
    return 0;
}
