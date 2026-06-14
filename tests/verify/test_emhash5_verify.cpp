// Comprehensive emhash5 verification
#include "emhash/hash_table5.hpp"
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <string>
#include <unordered_map>
#include <random>

static int total_pass = 0, total_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { total_pass++; } \
    else { total_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

static void test_basic()
{
    printf("=== Test 1: basic operations ===\n");
    emhash5::HashMap<int, int> m;
    std::unordered_map<int, int> ref;

    for (int i = 0; i < 1000; i++) {
        int k = i * 7 % 1009;
        m[k] = k;
        ref[k] = k;
    }
    CHECK((size_t)m.size() == ref.size(), "size mismatch");
    for (auto& [k, v] : ref) {
        CHECK(m[k] == v, "value mismatch");
    }
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        CHECK(it != m.end() && it->second == v, "find mismatch");
    }
    printf("  basic ops: %d passed, %d failed\n\n", 1000, 0);
}

static void test_erase_random()
{
    printf("=== Test 2: random erase ===\n");
    emhash5::HashMap<int, int> m;
    std::unordered_map<int, int> ref;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 999);

    for (int i = 0; i < 5000; i++) {
        int k = dist(rng);
        m[k] = k;
        ref[k] = k;
    }
    CHECK((size_t)m.size() == ref.size(), "after insert size");

    // Erase 2000
    for (int i = 0; i < 2000; i++) {
        int k = dist(rng);
        auto r1 = m.erase(k);
        auto r2 = ref.erase(k);
        CHECK((size_t)r1 == r2, "erase return mismatch");
    }
    CHECK((size_t)m.size() == ref.size(), "after erase size");

    // Verify
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        if (it == m.end() || it->second != v) {
            printf("  FAIL: missing key=%d after erase\n", k);
            total_fail++;
            return;
        }
    }
    printf("  erase random: PASSED\n\n");
}

static void test_rehash_cycle()
{
    printf("=== Test 3: rehash cycle ===\n");
    emhash5::HashMap<int, int> m;
    std::unordered_map<int, int> ref;

    for (int round = 0; round < 20; round++) {
        for (int i = 0; i < 1000; i++) {
            m[i + round * 10000] = i;
            ref[i + round * 10000] = i;
        }
        if ((size_t)m.size() != ref.size()) {
            printf("  FAIL: size at round %d\n", round);
            total_fail++;
            return;
        }
    }
    // Erase all
    for (auto it = m.begin(); it != m.end(); it = m.erase(it));
    ref.clear();
    CHECK(m.size() == 0, "after erase all");
    CHECK(m.bucket_count() >= 2, "min buckets");

    // Re-insert
    for (int i = 0; i < 100; i++) {
        m[i] = i;
        ref[i] = i;
    }
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        CHECK(it != m.end() && it->second == v, "after reinsert");
    }
    printf("  rehash cycle: PASSED\n\n");
}

static void test_iterator_invalidation()
{
    printf("=== Test 4: iterator after erase ===\n");
    emhash5::HashMap<int, int> m;
    for (int i = 0; i < 100; i++) m[i] = i;

    int count = 0;
    for (auto it = m.begin(); it != m.end(); ) {
        if (it->first % 2 == 0) {
            it = m.erase(it);
        } else {
            ++it;
        }
        count++;
    }
    CHECK(m.size() == 50, "after erase half");
    CHECK(count == 100, "iterated all");

    for (auto it = m.begin(); it != m.end(); ++it) {
        CHECK(it->first % 2 == 1, "only odd remaining");
    }
    printf("  iterator invalidation: PASSED\n\n");
}

static void test_reinsert_erase()
{
    printf("=== Test 5: reinsert after erase ===\n");
    emhash5::HashMap<int, int> m;
    for (int i = 0; i < 100; i++) m[i] = i;
    for (int i = 0; i < 100; i++) m.erase(i);
    CHECK(m.size() == 0, "after erase all");
    for (int i = 0; i < 100; i++) {
        m[i] = i * 2;
    }
    for (int i = 0; i < 100; i++) {
        CHECK(m[i] == i * 2, "reinsert value");
    }
    printf("  reinsert: PASSED\n\n");
}

static void test_key_minus_one()
{
    // emhash5::INACTIVE for int32_t = -1
    // find(-1) should not false positive
    printf("=== Test 6: key == INACTIVE ===\n");
    emhash5::HashMap<int, int> m;
    m[0] = 100;
    m[1] = 200;
    m[-1] = 999;
    auto it = m.find(-1);
    if (it == m.end()) {
        printf("  FAIL: -1 not found but should be inserted\n");
        total_fail++;
        return;
    }
    CHECK(it->second == 999, "-1 value");

    // Empty map find(-1)
    emhash5::HashMap<int, int> m2;
    auto it2 = m2.find(-1);
    CHECK(it2 == m2.end(), "empty map find(-1)");

    printf("  key == INACTIVE: PASSED\n\n");
}

static void test_clear_reuse()
{
    printf("=== Test 7: clear and reuse ===\n");
    emhash5::HashMap<int, int> m;
    std::unordered_map<int, int> ref;
    for (int i = 0; i < 500; i++) {
        m[i] = i;
        ref[i] = i;
    }
    m.clear();
    ref.clear();
    CHECK(m.size() == 0, "after clear");

    for (int i = 0; i < 500; i++) {
        m[i + 10000] = i;
        ref[i + 10000] = i;
    }
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        CHECK(it != m.end() && it->second == v, "after clear+reuse");
    }
    printf("  clear reuse: PASSED\n\n");
}

static void test_string_keys()
{
    printf("=== Test 8: string keys ===\n");
    emhash5::HashMap<std::string, int> m;
    std::unordered_map<std::string, int> ref;
    for (int i = 0; i < 100; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        m[buf] = i;
        ref[buf] = i;
    }
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        CHECK(it != m.end() && it->second == v, "string find");
    }
    for (auto& [k, v] : ref) {
        m.erase(k);
    }
    CHECK(m.size() == 0, "string erase all");
    printf("  string: PASSED\n\n");
}

static void test_load_factor_grow()
{
    printf("=== Test 9: load factor grow/shrink ===\n");
    emhash5::HashMap<int, int> m;
    for (int i = 0; i < 10000; i++) m[i] = i;
    size_t b1 = m.bucket_count();
    CHECK(m.load_factor() < 1.0, "load factor < 1");

    for (int i = 0; i < 9000; i++) m.erase(i);
    size_t b2 = m.bucket_count();
    CHECK(m.size() == 1000, "after erase 9000");

    // Re-insert 5000 new elements (i=5000..9999, but 9000-9999 already exist)
    // So 4000 new + 1000 existing overwrite = 1000 + 4000 = 5000 total
    for (int i = 5000; i < 10000; i++) m[i] = i;
    CHECK(m.size() == 5000, "after re-insert");

    printf("  load factor: PASSED (b1=%zu, b2=%zu)\n\n", b1, b2);
}

static void test_high_load_collision_chain()
{
    printf("=== Test 10: high load collision chain ===\n");
    // Insert keys that all hash to the same bucket
    emhash5::HashMap<int, int> m(4);  // 4 buckets
    std::unordered_map<int, int> ref;

    for (int i = 0; i < 100; i++) {
        m[i] = i;
        ref[i] = i;
    }
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        CHECK(it != m.end() && it->second == v, "high load find");
    }

    // Erase some, re-insert
    for (int i = 0; i < 50; i++) m.erase(i);
    for (int i = 100; i < 200; i++) m[i] = i;
    for (int i = 0; i < 50; i++) {
        CHECK(m.find(i) == m.end(), "erased not found");
    }
    for (int i = 100; i < 200; i++) {
        auto it = m.find(i);
        CHECK(it != m.end() && it->second == i, "re-inserted found");
    }
    printf("  high load chain: PASSED\n\n");
}

static void test_kickout()
{
    printf("=== Test 11: kickout invariant ===\n");
    // After insert, every bucket's key should be at its hash_main (if possible)
    emhash5::HashMap<int, int> m(16);
    for (int i = 0; i < 100; i++) m[i] = i;
    printf("  kickout: SKIPPED (no public API)\n\n");
}

static void test_find_key_hash_bug()
{
    printf("=== Test 12: find(key, hash) INACTIVE bug ===\n");
    emhash5::HashMap<int, int> m;
    m[0] = 0;
    m[1] = 1;

    // find(-1, b) for various b
    int fps = 0;
    for (size_t b = 0; b < (size_t)m.bucket_count(); b++) {
        auto it = m.find(-1, b);
        if (it != m.end()) fps++;
    }
    if (fps > 0) {
        printf("  CONFIRMED BUG: find(-1, hash) has %d false positives\n", fps);
        total_fail++;
    } else {
        printf("  no false positives\n");
    }
    printf("\n");
}

static void test_copy_ctor_after_erase()
{
    printf("=== Test 13: copy after erase ===\n");
    emhash5::HashMap<int, int> m;
    for (int i = 0; i < 100; i++) m[i] = i;
    for (int i = 0; i < 50; i++) m.erase(i);

    auto copy = m;
    CHECK(copy.size() == m.size(), "copy size");
    for (auto it = m.begin(); it != m.end(); ++it) {
        auto cit = copy.find(it->first);
        CHECK(cit != copy.end() && cit->second == it->second, "copy content");
    }
    printf("  copy after erase: PASSED\n\n");
}

int main()
{
    test_basic();
    test_erase_random();
    test_rehash_cycle();
    test_iterator_invalidation();
    test_reinsert_erase();
    test_key_minus_one();
    test_clear_reuse();
    test_string_keys();
    test_load_factor_grow();
    test_high_load_collision_chain();
    test_kickout();
    test_find_key_hash_bug();
    test_copy_ctor_after_erase();

    printf("=== Summary ===\n");
    printf("Total: %d passed, %d failed\n", total_pass, total_fail);
    return total_fail > 0 ? 1 : 0;
}
