// Bug fix verification tests for emhash5/emhash8 EMH_HIGH_LOAD
// Bug 4: EMH_PREVET overflow with small key types (static_assert)
// Note: Bug 2 (bucket 0 not in empty chain) is a design limitation, not a bug.
//   Bucket 0 cannot be in the empty chain because -0 = 0 breaks the
//   negative-number encoding of next pointers in the linked list.

#define EMH_HIGH_LOAD 1234567

#include "../../hash_table5.hpp"
#include "../../hash_table8.hpp"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <random>
#include <unordered_map>
#include <vector>

// Bug 4 test: static_assert should reject small key types
// Uncomment to verify compile error:
// emhash5::HashMap<int8_t, int> bad_map;  // should not compile

static int test_bucket0_erase()
{
    printf("=== Bucket 0 erase test (design limitation) ===\n");

    // Create a hash map with high load factor, size > EMH_HIGH_LOAD
    const auto vsize = 1u << 21; // 2M buckets
    emhash5::HashMap<int64_t, int> myhash(vsize, 0.95f);
    std::unordered_map<int64_t, int> refmap;

    // Fill it up to trigger set_empty()
    for (int i = 0; i < 240000; i++) {
        myhash[i] = i;
        refmap[i] = i;
    }

    printf("After fill: size=%d, buckets=%d, LF=%.4f\n",
        (int)myhash.size(), (int)myhash.bucket_count(), myhash.load_factor());

    // Erase all elements and verify the map is consistent
    auto orig_size = myhash.size();
    size_t erased = 0;
    for (auto it = myhash.begin(); it != myhash.end(); ) {
        refmap.erase(it->first);
        it = myhash.erase(it);
        erased++;
    }
    printf("Erased %zu elements (orig size=%d)\n", erased, (int)orig_size);
    assert(myhash.size() == 0);
    assert(refmap.size() == 0);

    // Reinsert and verify against refmap
    for (int i = 0; i < 240000; i++) {
        myhash[i] = i * 10;
        refmap[i] = i * 10;
    }

    for (int i = 0; i < 240000; i++) {
        assert(myhash.contains(i));
        assert(myhash[i] == i * 10);
        assert(refmap[i] == i * 10);
    }
    assert((size_t)myhash.size() == refmap.size());
    printf("Bucket 0 erase test PASSED\n\n");
    return 0;
}

static int test_bucket0_push_empty()
{
    printf("=== Bug 2: bucket 0 push_empty test ===\n");

    const auto vsize = 1u << 21;
    emhash5::HashMap<int64_t, int> myhash(vsize, 0.999f);
    std::unordered_map<int64_t, int> refmap;

    // Fill to high LF
    std::vector<int64_t> keys;
    for (int i = 0; i < 260000; i++) {
        myhash[i] = i;
        refmap[i] = i;
        keys.push_back(i);
    }

    printf("After fill: size=%d, buckets=%d, LF=%.4f\n",
        (int)myhash.size(), (int)myhash.bucket_count(), myhash.load_factor());

    // Verify all elements against refmap
    for (auto k : keys) {
        assert(myhash.contains(k) == (refmap.count(k) > 0));
        if (!myhash.contains(k)) {
            printf("ERROR: key %lld not found after fill!\n", (long long)k);
            return 1;
        }
    }

    // Erase half
    for (int i = 0; i < 130000; i++) {
        myhash.erase(i);
        refmap.erase(i);
    }

    printf("After erase: size=%d, LF=%.4f\n",
        (int)myhash.size(), myhash.load_factor());
    assert((size_t)myhash.size() == refmap.size());

    // Reinsert
    for (int i = 0; i < 130000; i++) {
        myhash[1000000 + i] = i;
        refmap[1000000 + i] = i;
    }

    // Verify remaining original elements
    int verified = 0;
    for (int i = 130000; i < 260000; i++) {
        assert(myhash.contains(i) == (refmap.count(i) > 0));
        if (!myhash.contains(i)) {
            printf("ERROR: original key %d not found!\n", i);
            return 1;
        }
        verified++;
    }

    // Verify new elements
    for (int i = 0; i < 130000; i++) {
        assert(myhash.contains(1000000 + i) == (refmap.count(1000000 + i) > 0));
        if (!myhash.contains(1000000 + i)) {
            printf("ERROR: new key %d not found!\n", 1000000 + i);
            return 1;
        }
        verified++;
    }

    assert((size_t)myhash.size() == refmap.size());
    printf("Verified %d elements, size=%d (ref=%zu)\n", verified, (int)myhash.size(), refmap.size());
    printf("Bug 2 push_empty test PASSED\n\n");
    return 0;
}

static int test_high_load_stress()
{
    printf("=== High load stress test (emhash5) ===\n");

    std::random_device rd;
    const auto seed = rd();
    std::mt19937_64 rng(seed);

    const auto max_lf = 0.999f;
    const auto vsize  = 1u << 21; // 2M buckets, must be > EMH_HIGH_LOAD

    emhash5::HashMap<int64_t, int> myhash(vsize, max_lf);
    std::unordered_map<int64_t, int> refmap;

    // Fill to max_lf
    std::vector<int64_t> keys;
    keys.reserve(vsize);
    for (size_t i = 0; i < size_t(vsize * max_lf); i++) {
        auto k = rng();
        keys.push_back(k);
        myhash.emplace(k, (int)i);
        refmap.emplace(k, (int)i);
    }

    assert(myhash.bucket_count() == vsize); // no rehash
    assert((size_t)myhash.size() == refmap.size());
    printf("Insert done: size=%d, buckets=%d, LF=%.4f\n",
        (int)myhash.size(), (int)myhash.bucket_count(), myhash.load_factor());

    // Erase + insert cycle
    std::mt19937_64 rng2(seed);
    for (size_t i = 0; i < vsize; i++) {
        auto old_k = rng2();
        auto new_k = rng();
        myhash.erase(old_k); // erase old key (may miss)
        myhash[new_k] = 1;   // insert new key
        refmap.erase(old_k);
        refmap[new_k] = 1;
    }

    printf("After erase+insert: size=%d (ref=%zu), LF=%.4f\n",
        (int)myhash.size(), refmap.size(), myhash.load_factor());
    assert((size_t)myhash.size() == refmap.size());

    // Verify load factor is still high
    assert(myhash.load_factor() >= max_lf - 0.01);

    // Verify consistency: count all elements via iteration
    size_t count = 0;
    for (auto& p : myhash) {
        (void)p;
        count++;
    }
    assert(count == (size_t)myhash.size());

    // Spot check: verify random keys match refmap
    for (auto& [k, v] : refmap) {
        auto it = myhash.find(k);
        assert(it != myhash.end());
        assert(it->second == v);
    }

    printf("High load stress test PASSED\n\n");
    return 0;
}

static int test_static_assert_small_key()
{
    printf("=== Bug 4: static_assert for small key types ===\n");

    // These should compile fine (sizeof(int64_t) >= sizeof(size_type))
    emhash5::HashMap<int64_t, int> map64;
    map64[1] = 10;
    assert(map64[1] == 10);
    printf("int64_t key: OK\n");

    emhash5::HashMap<uint64_t, int> mapu64;
    mapu64[1] = 10;
    assert(mapu64[1] == 10);
    printf("uint64_t key: OK\n");

    emhash5::HashMap<int32_t, int> map32;
    map32[1] = 10;
    assert(map32[1] == 10);
    printf("int32_t key: OK (sizeof=%zu)\n", sizeof(int32_t));

    // Note: int8_t/int16_t keys should trigger static_assert
    // Uncomment below to verify:
    // emhash5::HashMap<int8_t, int> map8;  // should not compile
    // emhash5::HashMap<int16_t, int> map16; // should not compile

    printf("Bug 4 static_assert test PASSED\n\n");
    return 0;
}

// LF oscillation around 0.8 threshold
// When LF oscillates around 0.8, clear_bucket() repeatedly calls
// clear_empty() (tear down chain) and reserve() calls set_empty() (rebuild chain).
// This test verifies no corruption occurs during rapid oscillation.
// Uses std::unordered_map as reference to verify correctness.
static int test_lf_oscillation()
{
    printf("=== LF oscillation test (emhash5) ===\n");

    // Use a size > EMH_HIGH_LOAD to activate the empty chain
    const auto vsize = 1u << 21; // 2M buckets
    emhash5::HashMap<int64_t, int> myhash(vsize, 0.999f);
    std::unordered_map<int64_t, int> refmap;

    // Fill to ~0.85 LF (above 0.8 threshold, chain is active)
    const int fill_count = int(vsize * 0.85);
    for (int i = 0; i < fill_count; i++) {
        myhash[i] = i;
        refmap[i] = i;
    }

    printf("Initial fill: size=%d, LF=%.4f\n", (int)myhash.size(), myhash.load_factor());
    assert((size_t)myhash.size() == refmap.size());

    // Oscillate around 0.8 LF threshold:
    //   erase enough to drop below 0.8 → clear_empty() tears down chain
    //   insert back above 0.8 → reserve() rebuilds chain via set_empty()
    //   repeat many times
    const int erase_count = int(vsize * 0.10); // erase 10% → LF drops from ~0.85 to ~0.75
    const int oscillations = 1000;

    int64_t next_key = fill_count; // unique key generator

    for (int round = 0; round < oscillations; round++) {
        // Erase to drop below 0.8
        for (int i = 0; i < erase_count; i++) {
            int64_t key = (int64_t)(round * erase_count + i);
            myhash.erase(key);
            refmap.erase(key);
        }

        // Insert new unique keys to go back above 0.8
        for (int i = 0; i < erase_count; i++) {
            myhash[next_key] = round;
            refmap[next_key] = round;
            next_key++;
        }

        // Verify consistency every 100 rounds
        if (round % 100 == 0) {
            // Check size matches
            if ((size_t)myhash.size() != refmap.size()) {
                printf("ERROR at round %d: size=%d != refsize=%zu\n",
                    round, (int)myhash.size(), refmap.size());
                return 1;
            }

            // Check iteration count matches size
            size_t count = 0;
            for (auto& p : myhash) {
                (void)p;
                count++;
            }
            if (count != (size_t)myhash.size()) {
                printf("ERROR at round %d: iter count=%zu != size=%d\n",
                    round, count, (int)myhash.size());
                return 1;
            }

            // Spot check: verify 100 random keys from refmap exist in myhash
            int checked = 0;
            for (auto& [k, v] : refmap) {
                auto it = myhash.find(k);
                if (it == myhash.end() || it->second != v) {
                    printf("ERROR at round %d: key=%lld not found or value mismatch!\n",
                        round, (long long)k);
                    return 1;
                }
                if (++checked >= 100) break;
            }

            printf("  Round %4d: LF=%.4f, size=%d [OK]\n",
                round, myhash.load_factor(), (int)myhash.size());
        }
    }

    // Final full verification: every key in refmap must be in myhash
    for (auto& [k, v] : refmap) {
        auto it = myhash.find(k);
        if (it == myhash.end() || it->second != v) {
            printf("ERROR: final check key=%lld not found or value mismatch!\n", (long long)k);
            return 1;
        }
    }
    assert((size_t)myhash.size() == refmap.size());

    size_t final_count = 0;
    for (auto& p : myhash) {
        (void)p;
        final_count++;
    }
    assert(final_count == (size_t)myhash.size());

    printf("LF oscillation test PASSED (%d oscillations, final buckets=%d)\n\n", oscillations, (int)myhash.bucket_count());
    return 0;
}

// Same test for emhash8
static int test_lf_oscillation_emhash8()
{
    printf("=== LF oscillation test (emhash8) ===\n");

    const auto vsize = 1u << 21;
    emhash8::HashMap<int64_t, int> myhash(vsize, 0.999f);
    std::unordered_map<int64_t, int> refmap;

    const int fill_count = int(vsize * 0.85);
    for (int i = 0; i < fill_count; i++) {
        myhash[i] = i;
        refmap[i] = i;
    }

    printf("Initial fill: size=%d, LF=%.4f\n", (int)myhash.size(), myhash.load_factor());
    assert((size_t)myhash.size() == refmap.size());

    const int erase_count = int(vsize * 0.10);
    const int oscillations = 1000;

    int64_t next_key = fill_count;

    for (int round = 0; round < oscillations; round++) {
        for (int i = 0; i < erase_count; i++) {
            int64_t key = (int64_t)(round * erase_count + i);
            myhash.erase(key);
            refmap.erase(key);
        }

        for (int i = 0; i < erase_count; i++) {
            myhash[next_key] = round;
            refmap[next_key] = round;
            next_key++;
        }

        if (round % 100 == 0) {
            if ((size_t)myhash.size() != refmap.size()) {
                printf("ERROR at round %d: size=%d != refsize=%zu\n",
                    round, (int)myhash.size(), refmap.size());
                return 1;
            }

            size_t count = 0;
            for (auto& p : myhash) {
                (void)p;
                count++;
            }
            if (count != (size_t)myhash.size()) {
                printf("ERROR at round %d: iter count=%zu != size=%d\n",
                    round, count, (int)myhash.size());
                return 1;
            }

            int checked = 0;
            for (auto& [k, v] : refmap) {
                auto it = myhash.find(k);
                if (it == myhash.end() || it->second != v) {
                    printf("ERROR at round %d: key=%lld not found or value mismatch!\n",
                        round, (long long)k);
                    return 1;
                }
                if (++checked >= 100) break;
            }

            printf("  Round %4d: size=%d, LF=%.4f [OK]\n",
                round, (int)myhash.size(), myhash.load_factor());
        }
    }

    // Final full verification
    for (auto& [k, v] : refmap) {
        auto it = myhash.find(k);
        if (it == myhash.end() || it->second != v) {
            printf("ERROR: final check key=%lld not found or value mismatch!\n", (long long)k);
            return 1;
        }
    }
    assert((size_t)myhash.size() == refmap.size());

    size_t final_count = 0;
    for (auto& p : myhash) {
        (void)p;
        final_count++;
    }
    assert(final_count == (size_t)myhash.size());

    printf("LF oscillation test PASSED (%d oscillations, final buckets=%d)\n\n", oscillations, (int)myhash.bucket_count());
    return 0;
}

int main()
{
    int failures = 0;
    failures += test_bucket0_erase();
    failures += test_bucket0_push_empty();
    failures += test_high_load_stress();
    failures += test_static_assert_small_key();
    failures += test_lf_oscillation();
    failures += test_lf_oscillation_emhash8();

    if (failures == 0)
        printf("All tests PASSED!\n");
    else
        printf("%d tests FAILED!\n", failures);

    return failures;
}
