// High-intensity stress test for emhash5
// #define EMH_FIND_HIT 1
#include "emhash/hash_table5.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>

static int total_fail = 0;
static int64_t total_ops = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { total_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

#define CHECK_EQ(a, b, msg) do { \
    if ((size_t)(a) != (size_t)(b)) { total_fail++; printf("  FAIL: %s (got %lld expected %lld)\n", \
        msg, (long long)(size_t)(a), (long long)(size_t)(b)); } \
} while(0)

static double now_ms()
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// Test 1: Large scale random operations with 1M elements
static void test_large_random_1m()
{
    printf("=== Test 1: 1M random insert/erase/find ===\n");
    emhash5::HashMap<int64_t, int64_t> m;
    std::unordered_map<int64_t, int64_t> ref;
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<int64_t> keyd(0, 999999);

    const int N = 1000000;
    int64_t next_key = 1000000;

    double t0 = now_ms();
    for (int i = 0; i < N; i++) {
        int op = rng() & 7;  // 0-7
        if (op < 5) {  // 50% insert
            int64_t k = (rng() & 1) ? next_key++ : keyd(rng);
            int64_t v = rng();
            auto r1 = m.emplace(k, v);
            auto r2 = ref.emplace(k, v);
            if (r1.second != r2.second) {
                printf("  FAIL at i=%d: insert return mismatch\n", i);
                total_fail++;
                return;
            }
        } else if (op < 7) {  // 25% find
            int64_t k = keyd(rng);
            auto it1 = m.find(k);
            auto it2 = ref.find(k);
            if ((it1 == m.end()) != (it2 == ref.end())) {
                printf("  FAIL at i=%d: find existence mismatch key=%lld\n", i, (long long)k);
                total_fail++;
                return;
            }
            if (it1 != m.end() && it1->second != it2->second) {
                printf("  FAIL at i=%d: find value mismatch\n", i);
                total_fail++;
                return;
            }
        } else {  // 25% erase
            int64_t k = keyd(rng);
            auto r1 = m.erase(k);
            auto r2 = ref.erase(k);
            if ((size_t)r1 != r2) {
                printf("  FAIL at i=%d: erase return mismatch key=%lld\n", i, (long long)k);
                total_fail++;
                return;
            }
        }
    }
    double t1 = now_ms();
    total_ops += N;

    CHECK_EQ(m.size(), ref.size(), "final size");
    printf("  size=%zu, %.0f ops/ms (%.1f Mops total)\n",
        (size_t)m.size(), N / (t1 - t0), (double)total_ops / 1e6);

    // Full verify
    printf("  full verifying %zu elements...\n", (size_t)m.size());
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        if (it == m.end() || it->second != v) {
            printf("  FAIL: missing or wrong key=%lld\n", (long long)k);
            total_fail++;
            return;
        }
    }
    printf("  full verify OK\n\n");
}

// Test 2: Hash collision attacks (all keys hash to same bucket)
static void test_collision_attack()
{
    printf("=== Test 2: hash collision attack (small map) ===\n");
    // Use a hasher that returns constant
    struct const_hasher {
        size_t operator()(int /*k*/) const { return 0; }
    };
    emhash5::HashMap<int, int, const_hasher> m(8);
    std::unordered_map<int, int, const_hasher> ref;

    for (int i = 0; i < 1000; i++) {
        m[i] = i;
        ref[i] = i;
    }
    CHECK_EQ(m.size(), ref.size(), "size after collision insert");

    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        if (it == m.end() || it->second != v) {
            printf("  FAIL: collision key=%d not found\n", k);
            total_fail++;
            return;
        }
    }
    printf("  collision test OK (1000 keys, all hash to bucket 0)\n\n");
}

// Test 3: Erase during iteration
static void test_erase_during_iter()
{
    printf("=== Test 3: erase during iteration ===\n");
    emhash5::HashMap<int, int> m;
    for (int i = 0; i < 10000; i++) m[i] = i;

    int count = 0, erased = 0;
    for (auto it = m.begin(); it != m.end(); ) {
        count++;
        if ((it->first & 1) == 0) {
            it = m.erase(it);
            erased++;
        } else {
            ++it;
        }
    }
    CHECK_EQ(erased, 5000, "erased count");
    CHECK_EQ(count, 10000, "iterated count");
    CHECK_EQ(m.size(), 5000, "final size");
    printf("  erase during iter OK (5000 erased)\n\n");
}

// Test 4: Rehash under load
static void test_rehash_under_load()
{
    printf("=== Test 4: rehash under load ===\n");
    emhash5::HashMap<int, int> m;
    std::unordered_map<int, int> ref;
    std::mt19937 rng(42);

    // Insert 100K
    for (int i = 0; i < 100000; i++) {
        m[i] = i;
        ref[i] = i;
    }
    CHECK_EQ(m.size(), 100000, "after 100K insert");
    printf("  bucket_count=%zu LF=%.2f\n", (size_t)m.bucket_count(), m.load_factor());

    // Erase 80K
    for (int i = 0; i < 80000; i++) {
        (void)m.erase(i);
        ref.erase(i);
    }
    CHECK_EQ(m.size(), 20000, "after 80K erase");
    printf("  bucket_count=%zu LF=%.4f\n", (size_t)m.bucket_count(), m.load_factor());

    // Verify remaining
    int v_count = 0;
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        if (it == m.end() || it->second != v) {
            printf("  FAIL: missing key=%d after erase\n", k);
            total_fail++;
            return;
        }
        v_count++;
    }
    CHECK_EQ(v_count, 20000, "verify count");
    printf("  rehash under load OK\n\n");
}

// Test 5: String key churn
static void test_string_churn()
{
    printf("=== Test 5: string key churn (100K) ===\n");
    emhash5::HashMap<std::string, int> m;
    std::unordered_map<std::string, int> ref;

    char buf[32];
    for (int i = 0; i < 100000; i++) {
        snprintf(buf, sizeof(buf), "key_%d", i);
        std::string k(buf);
        m[k] = i;
        ref[k] = i;
    }
    CHECK_EQ(m.size(), 100000, "string size");

    // Erase half
    for (int i = 0; i < 100000; i += 2) {
        snprintf(buf, sizeof(buf), "key_%d", i);
        (void)m.erase(std::string(buf));
        ref.erase(std::string(buf));
    }
    CHECK_EQ(m.size(), 50000, "after erase half");

    // Verify odd keys
    for (int i = 1; i < 100000; i += 2) {
        snprintf(buf, sizeof(buf), "key_%d", i);
        auto it = m.find(std::string(buf));
        if (it == m.end() || it->second != i) {
            printf("  FAIL: string key=%d not found\n", i);
            total_fail++;
            return;
        }
    }
    printf("  string churn OK\n\n");
}

// Test 6: Bucket count changes
static void test_bucket_growth()
{
    printf("=== Test 6: bucket count growth ===\n");
    emhash5::HashMap<int, int> m;

    size_t last_bc = 0;
    for (int i = 0; i < 200000; i++) {
        m[i] = i;
        if (i % 10000 == 0) {
            size_t bc = m.bucket_count();
            if (bc != last_bc) {
                printf("  i=%d bucket_count=%zu LF=%.3f\n", i, bc, m.load_factor());
                last_bc = bc;
            }
        }
    }
    CHECK_EQ(m.size(), 200000, "final size");
    printf("  final bucket_count=%zu LF=%.3f\n\n",
        (size_t)m.bucket_count(), m.load_factor());
}

// Test 7: Copy/move heavy
static void test_copy_move_heavy()
{
    printf("=== Test 7: copy/move heavy ===\n");
    emhash5::HashMap<int, int> m;
    for (int i = 0; i < 1000; i++) m[i] = i;

    for (int round = 0; round < 100; round++) {
        auto copy = m;  // copy ctor
        CHECK_EQ(copy.size(), 1000, "copy size");

        // Move into another
        emhash5::HashMap<int, int> dest;
        dest = std::move(copy);
        CHECK_EQ(dest.size(), 1000, "moved size");
        // copy should be empty
        CHECK_EQ(copy.size(), 0, "moved-from size");

        // Self-assignment test
        dest = static_cast<decltype(dest)&>(dest);
        CHECK_EQ(dest.size(), 1000, "self-assign");

        // Verify all
        for (int i = 0; i < 1000; i++) {
            if (dest[i] != i) {
                printf("  FAIL: copy/move lost data at i=%d\n", i);
                total_fail++;
                return;
            }
        }
        m = std::move(dest);
        CHECK_EQ(m.size(), 1000, "after move back");
    }
    printf("  copy/move heavy OK (100 rounds)\n\n");
}

// Test 8: Mass erase + reinsert
static void test_mass_erase_reinsert()
{
    printf("=== Test 8: mass erase + reinsert ===\n");
    emhash5::HashMap<int, int> m;
    for (int i = 0; i < 50000; i++) m[i] = i;

    for (int round = 0; round < 50; round++) {
        // Erase all
        for (auto it = m.begin(); it != m.end(); it = m.erase(it));
        CHECK_EQ(m.size(), 0, "after erase all");

        // Re-insert
        for (int i = 0; i < 50000; i++) m[i] = i;
        CHECK_EQ(m.size(), 50000, "after reinsert");

        // Verify
        for (int i = 0; i < 50000; i++) {
            if (m[i] != i) {
                printf("  FAIL: data lost at round=%d i=%d\n", round, i);
                total_fail++;
                return;
            }
        }
    }
    printf("  mass erase+reinsert OK (50 cycles × 50K elements)\n\n");
}

// Test 9: INACTIVE key (-1) under heavy ops
static void test_inactive_heavy()
{
    printf("=== Test 9: INACTIVE key (-1) heavy ops ===\n");
    emhash5::HashMap<int, int> m;

    // Insert -1 and verify
    m[-1] = 999;
    CHECK_EQ(m.find(-1)->second, 999, "-1 find");

    // Insert 10000 more
    for (int i = 0; i < 10000; i++) m[i] = i;
    auto it = m.find(-1);
    CHECK(it != m.end() && it->second == 999, "-1 still findable after 10K inserts");

    // Erase -1, verify
    (void)m.erase(-1);
    CHECK(m.find(-1) == m.end(), "-1 erased");

    // Re-insert -1 with different value
    m[-1] = -999;
    CHECK_EQ(m.find(-1)->second, -999, "-1 re-inserted");

    // Delete all and verify -1 not false-positive
    for (int i = 0; i < 10000; i++) (void)m.erase(i);
    CHECK_EQ(m.find(-1)->second, -999, "-1 still there");
    (void)m.erase(-1);
    CHECK(m.find(-1) == m.end(), "-1 finally erased");

    // Empty map find(-1)
    emhash5::HashMap<int, int> m2;
    CHECK(m2.find(-1) == m2.end(), "empty find(-1)");

    // Mass find(-1) with different hashes
    int fps = 0;
    for (size_t b = 0; b < (size_t)m2.bucket_count(); b++) {
        if (m2.find(-1, b) != m2.end()) fps++;
    }
    CHECK_EQ(fps, 0, "no false positives");

    printf("  INACTIVE key heavy OK (0 false positives)\n\n");
}

// Test 10: Extreme high load (LF=0.99)
static void test_extreme_high_load()
{
    printf("=== Test 10: extreme high load LF=0.99 ===\n");
    emhash5::HashMap<int, int> m(1024, 0.99f);
    for (int i = 0; i < 1000; i++) m[i] = i;
    CHECK_EQ(m.size(), 1000, "high load size");
    printf("  bucket_count=%zu LF=%.3f\n", (size_t)m.bucket_count(), m.load_factor());

    for (int i = 0; i < 1000; i++) {
        if (m[i] != i) {
            printf("  FAIL: high load find at i=%d\n", i);
            total_fail++;
            return;
        }
    }
    printf("  extreme high load OK\n\n");
}

// Test 11: Rehash + clear + EMH_FIND_HIT regression
static void test_rehash_clear_regression()
{
    printf("=== Test 11: rehash + clear regression ===\n");
    emhash5::HashMap<int, int> m;

    // Force several rehashes
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 5000; i++) m[i + round * 100000] = i;
    }
    CHECK_EQ(m.size(), 50000, "after 10 rehash rounds");

    // Clear and verify -1 not findable
    m.clear();
    CHECK_EQ(m.size(), 0, "after clear");
    CHECK(m.find(-1) == m.end(), "find(-1) after clear");

    // Re-insert
    for (int i = 0; i < 100; i++) m[i] = i;
    for (int i = 0; i < 100; i++) {
        if (m[i] != i) {
            printf("  FAIL: reinsert data lost at i=%d\n", i);
            total_fail++;
            return;
        }
    }
    CHECK(m.find(-1) == m.end(), "find(-1) after reinsert");
    printf("  rehash+clear regression OK\n\n");
}

int main()
{
    double t0 = now_ms();

    test_large_random_1m();
    test_collision_attack();
    test_erase_during_iter();
    test_rehash_under_load();
    test_string_churn();
    test_bucket_growth();
    test_copy_move_heavy();
    test_mass_erase_reinsert();
    test_inactive_heavy();
    test_extreme_high_load();
    test_rehash_clear_regression();

    double t1 = now_ms();
    printf("=== Total: %d failures, %.1f Mops, %.1f seconds ===\n",
        total_fail, (double)total_ops / 1e6, (t1 - t0) / 1000.0);

    return total_fail > 0 ? 1 : 0;
}
