// Test hash map correctness across all sizes from 1 to 100,000
// Covers: power-of-2 sizes, sizes near resize boundaries, and sampled sizes

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <algorithm>

#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

static int g_pass = 0, g_fail = 0;

#define TEST_ASSERT(expr, msg) do { \
    if (!(expr)) { \
        printf("    FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; return false; \
    } else { g_pass++; } \
} while(0)

// Test a specific size N for a given MapType
template<typename MapType>
bool test_size_n(int N)
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    // Insert N keys
    for (int i = 0; i < N; i++)
        map[Key(i)] = Val(i * 3);

    TEST_ASSERT((int)map.size() == N, "size after insert");

    // Verify all keys
    for (int i = 0; i < N; i++) {
        auto it = map.find(Key(i));
        if (it == map.end() || it->second != Val(i * 3)) {
            printf("    FAIL: key %d not found or wrong value at N=%d\n", i, N);
            g_fail++;
            return false;
        }
    }
    g_pass += N;

    // Erase odd keys
    int erased = 0;
    for (int i = 1; i < N; i += 2) {
        map.erase(Key(i));
        erased++;
    }
    TEST_ASSERT((int)map.size() == N - erased, "size after erase odds");

    // Verify remaining (even keys)
    for (int i = 0; i < N; i++) {
        if (i % 2 == 0) {
            auto it = map.find(Key(i));
            if (it == map.end() || it->second != Val(i * 3)) {
                printf("    FAIL: even key %d lost after erase at N=%d\n", i, N);
                g_fail++;
                return false;
            }
        } else {
            if (map.count(Key(i)) != 0) {
                printf("    FAIL: odd key %d still present at N=%d\n", i, N);
                g_fail++;
                return false;
            }
        }
    }
    g_pass += N;

    // Reinsert erased keys with new values
    for (int i = 1; i < N; i += 2)
        map[Key(i)] = Val(i * 7);

    TEST_ASSERT((int)map.size() == N, "size after reinsert");

    // Verify all keys with updated values
    for (int i = 0; i < N; i++) {
        int expected = (i % 2 == 0) ? i * 3 : i * 7;
        if (map[Key(i)] != Val(expected)) {
            printf("    FAIL: key %d value mismatch at N=%d\n", i, N);
            g_fail++;
            return false;
        }
    }
    g_pass += N;

    // Clear and verify empty
    map.clear();
    TEST_ASSERT(map.empty(), "empty after clear");
    TEST_ASSERT(map.size() == 0, "zero size after clear");

    return true;
}

// Test with bad hash at specific sizes
struct BadHash {
    size_t operator()(int) const { return 42; }
};

template<typename MapType>
bool test_size_n_bad_hash(int N)
{
    MapType map;
    for (int i = 0; i < N; i++)
        map[i] = i * 3;

    if ((int)map.size() != N) {
        printf("    FAIL: bad hash size mismatch at N=%d (got %zu)\n", N, map.size());
        g_fail++;
        return false;
    }
    g_pass++;

    for (int i = 0; i < N; i++) {
        if (map[i] != i * 3) {
            printf("    FAIL: bad hash value mismatch at N=%d key=%d\n", N, i);
            g_fail++;
            return false;
        }
    }
    g_pass += N;

    return true;
}

template<typename MapType>
void run_size_sweep(const char* name)
{
    printf("\n=== %s size sweep ===\n", name);
    int tests = 0, fails = 0;

    // Size 1-100: every size
    for (int N = 1; N <= 100; N++) {
        if (!test_size_n<MapType>(N)) fails++;
        tests++;
    }

    // Size 101-1000: every 10th
    for (int N = 101; N <= 1000; N += 10) {
        if (!test_size_n<MapType>(N)) fails++;
        tests++;
    }

    // Size 1001-10000: every 100th
    for (int N = 1001; N <= 10000; N += 100) {
        if (!test_size_n<MapType>(N)) fails++;
        tests++;
    }

    // Size 10001-100000: every 1000th
    for (int N = 10001; N <= 100000; N += 1000) {
        if (!test_size_n<MapType>(N)) fails++;
        tests++;
    }

    // Key boundary sizes (near common resize thresholds)
    int boundaries[] = {
        12, 13, 24, 25, 48, 49, 96, 97,
        192, 193, 384, 385, 768, 769,
        1536, 1537, 3072, 3073, 6144, 6145,
        12288, 12289, 24576, 24577, 49152, 49153,
        98304, 98305
    };
    for (int N : boundaries) {
        if (N > 100000) continue;
        if (!test_size_n<MapType>(N)) fails++;
        tests++;
    }

    // Power-of-2 sizes
    for (int p = 0; p <= 16; p++) {
        int N = 1 << p;
        if (N > 100000) break;
        if (!test_size_n<MapType>(N)) fails++;
        tests++;
    }

    // Power-of-2 +/- 1
    for (int p = 1; p <= 16; p++) {
        int N = (1 << p) - 1;
        if (N > 100000) break;
        if (!test_size_n<MapType>(N)) fails++;
        tests++;
        N = (1 << p) + 1;
        if (N <= 100000) {
            if (!test_size_n<MapType>(N)) fails++;
            tests++;
        }
    }

    printf("  %s: %d sizes tested, %d failed\n", name, tests, fails);
}

template<typename MapType>
void run_bad_hash_sweep(const char* name)
{
    printf("\n=== %s bad hash size sweep ===\n", name);
    int tests = 0, fails = 0;

    // Small sizes with bad hash
    int sizes[] = {1, 2, 3, 5, 8, 13, 16, 20, 32, 50, 64, 100, 128, 200, 256, 500};
    for (int N : sizes) {
        if (!test_size_n_bad_hash<MapType>(N)) fails++;
        tests++;
    }

    printf("  %s bad hash: %d sizes tested, %d failed\n", name, tests, fails);
}

int main()
{
    run_size_sweep<emilib::HashMap<int, int>>("emilib2ss");
    run_size_sweep<emilib2::HashMap<int, int>>("emilib2o");
    run_size_sweep<emilib3::HashMap<int, int>>("emilib2s");

    run_bad_hash_sweep<emilib::HashMap<int, int, BadHash>>("emilib2ss");
    run_bad_hash_sweep<emilib2::HashMap<int, int, BadHash>>("emilib2o");
    run_bad_hash_sweep<emilib3::HashMap<int, int, BadHash>>("emilib2s");

    printf("\n============================================================\n");
    printf("  Total assertions: %d\n", g_pass + g_fail);
    printf("  Passed: %d\n", g_pass);
    printf("  Failed: %d\n", g_fail);
    printf("============================================================\n");

    return g_fail > 0 ? 1 : 0;
}
