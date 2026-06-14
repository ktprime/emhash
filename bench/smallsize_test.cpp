// EMH_SMALL_SIZE bug verification test
// Compile: g++ -I. -Ithirdparty -O0 -g -std=c++17 -DEMH_SMALL_SIZE=16 bench/smallsize_test.cpp -o bench/smallsize_test.exe

#define EMH_SMALL_SIZE 16

#include "../hash_table5.hpp"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <string>

static int test_basic_small()
{
    printf("=== Basic small map test ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    std::unordered_map<int64_t, int> refmap;

    // Insert a few elements (should use _small buffer)
    for (int i = 0; i < 10; i++) {
        myhash[i] = i * 10;
        refmap[i] = i * 10;
    }

    printf("After insert: size=%zu, buckets=%zu, LF=%.4f\n",
        myhash.size(), myhash.bucket_count(), myhash.load_factor());

    assert(myhash.size() == refmap.size());
    for (auto& [k, v] : refmap) {
        assert(myhash[k] == v);
    }

    printf("Basic small map test PASSED\n\n");
    return 0;
}

// Test: grow beyond _small buffer
static int test_grow_beyond_small()
{
    printf("=== Grow beyond _small buffer ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    std::unordered_map<int64_t, int> refmap;

    // Start small, then grow beyond EMH_SMALL_SIZE
    for (int i = 0; i < 100; i++) {
        myhash[i] = i;
        refmap[i] = i;
    }

    printf("After grow: size=%zu, buckets=%zu, LF=%.4f\n",
        myhash.size(), myhash.bucket_count(), myhash.load_factor());

    assert(myhash.size() == refmap.size());
    for (auto& [k, v] : refmap) {
        assert(myhash[k] == v);
    }

    printf("Grow beyond _small test PASSED\n\n");
    return 0;
}

// Test: copy constructor with small map
static int test_copy_small()
{
    printf("=== Copy constructor test ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    for (int i = 0; i < 10; i++)
        myhash[i] = i * 10;

    auto copy = myhash;

    assert(copy.size() == myhash.size());
    for (int i = 0; i < 10; i++) {
        assert(copy.contains(i));
        assert(copy[i] == i * 10);
    }

    // Modify copy, original should be unchanged
    copy[100] = 1000;
    assert(!myhash.contains(100));
    assert(copy.contains(100));

    printf("Copy constructor test PASSED\n\n");
    return 0;
}

// Test: copy constructor with large map (beyond _small)
static int test_copy_large()
{
    printf("=== Copy large map test ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    for (int i = 0; i < 200; i++)
        myhash[i] = i;

    auto copy = myhash;

    assert(copy.size() == myhash.size());
    for (int i = 0; i < 200; i++) {
        assert(copy[i] == i);
    }

    printf("Copy large map test PASSED\n\n");
    return 0;
}

// Test: move constructor
static int test_move()
{
    printf("=== Move constructor test ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    for (int i = 0; i < 10; i++)
        myhash[i] = i * 10;

    auto moved = std::move(myhash);

    assert(moved.size() == 10);
    for (int i = 0; i < 10; i++) {
        assert(moved.contains(i));
        assert(moved[i] == i * 10);
    }

    printf("Move constructor test PASSED\n\n");
    return 0;
}

// Test: move large map
static int test_move_large()
{
    printf("=== Move large map test ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    for (int i = 0; i < 200; i++)
        myhash[i] = i;

    auto moved = std::move(myhash);

    assert(moved.size() == 200);
    for (int i = 0; i < 200; i++) {
        assert(moved[i] == i);
    }

    printf("Move large map test PASSED\n\n");
    return 0;
}

// Test: swap two small maps
static int test_swap_small()
{
    printf("=== Swap small maps test ===\n");

    emhash5::HashMap<int64_t, int> map1, map2;

    for (int i = 0; i < 5; i++) map1[i] = i;
    for (int i = 100; i < 105; i++) map2[i] = i;

    map1.swap(map2);

    assert(map1.size() == 5);
    assert(map2.size() == 5);

    for (int i = 100; i < 105; i++) {
        assert(map1.contains(i));
        assert(map1[i] == i);
    }
    for (int i = 0; i < 5; i++) {
        assert(map2.contains(i));
        assert(map2[i] == i);
    }

    printf("Swap small maps test PASSED\n\n");
    return 0;
}

// Test: swap small with large
static int test_swap_small_large()
{
    printf("=== Swap small with large test ===\n");

    emhash5::HashMap<int64_t, int> small_map, large_map;

    for (int i = 0; i < 5; i++) small_map[i] = i;
    for (int i = 0; i < 200; i++) large_map[i] = i + 1000;

    printf("Before swap: small size=%zu buckets=%zu, large size=%zu buckets=%zu\n",
        small_map.size(), small_map.bucket_count(),
        large_map.size(), large_map.bucket_count());

    small_map.swap(large_map);

    printf("After swap: small size=%zu buckets=%zu, large size=%zu buckets=%zu\n",
        small_map.size(), small_map.bucket_count(),
        large_map.size(), large_map.bucket_count());

    // Check what we actually have
    int small_found = 0, small_missing = 0;
    for (int i = 0; i < 200; i++) {
        if (small_map.contains(i)) {
            small_found++;
        } else {
            small_missing++;
            if (small_missing <= 5) printf("  small_map missing key=%d\n", i);
        }
    }

    int large_found = 0, large_missing = 0;
    for (int i = 0; i < 5; i++) {
        if (large_map.contains(i)) {
            large_found++;
        } else {
            large_missing++;
            printf("  large_map missing key=%d\n", i);
        }
    }

    printf("small_map: found=%d missing=%d, large_map: found=%d missing=%d\n",
        small_found, small_missing, large_found, large_missing);

    if (small_missing > 0 || large_missing > 0) {
        printf("BUG CONFIRMED: swap small/large loses data!\n\n");
        return 1;
    }

    printf("Swap small with large test PASSED\n\n");
    return 0;
}

// Test: copy assignment small to large and vice versa
static int test_copy_assignment()
{
    printf("=== Copy assignment test ===\n");

    emhash5::HashMap<int64_t, int> map1, map2;

    for (int i = 0; i < 5; i++) map1[i] = i;
    for (int i = 0; i < 200; i++) map2[i] = i + 1000;

    // Small = Large
    map1 = map2;
    assert(map1.size() == 200);
    for (int i = 0; i < 200; i++)
        assert(map1[i] == i + 1000);

    // Large = Small
    map2.clear();
    for (int i = 0; i < 5; i++) map2[i] = i;
    emhash5::HashMap<int64_t, int> map3;
    for (int i = 0; i < 200; i++) map3[i] = i;
    map3 = map2;
    assert(map3.size() == 5);
    for (int i = 0; i < 5; i++)
        assert(map3[i] == i);

    printf("Copy assignment test PASSED\n\n");
    return 0;
}

// Test: move assignment
static int test_move_assignment()
{
    printf("=== Move assignment test ===\n");

    emhash5::HashMap<int64_t, int> map1, map2;

    for (int i = 0; i < 5; i++) map1[i] = i;
    for (int i = 0; i < 200; i++) map2[i] = i + 1000;

    // Small = move(Large)
    map1 = std::move(map2);
    assert(map1.size() == 200);
    for (int i = 0; i < 200; i++)
        assert(map1[i] == i + 1000);

    printf("Move assignment test PASSED\n\n");
    return 0;
}

// Test: clear and reuse (should go back to _small)
static int test_clear_reuse()
{
    printf("=== Clear and reuse test ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    std::unordered_map<int64_t, int> refmap;

    // Grow large
    for (int i = 0; i < 200; i++) {
        myhash[i] = i;
        refmap[i] = i;
    }
    printf("After grow: size=%zu, buckets=%zu\n", myhash.size(), myhash.bucket_count());

    // Clear
    myhash.clear();
    refmap.clear();
    assert(myhash.size() == 0);

    // Reuse small
    for (int i = 0; i < 10; i++) {
        myhash[1000 + i] = i;
        refmap[1000 + i] = i;
    }

    assert(myhash.size() == refmap.size());
    for (auto& [k, v] : refmap) {
        assert(myhash[k] == v);
    }

    printf("Clear and reuse test PASSED\n\n");
    return 0;
}

// Test: shrink_to_fit with _small buffer
static int test_shrink_to_fit()
{
    printf("=== shrink_to_fit test ===\n");

    emhash5::HashMap<int64_t, int> myhash;

    // Start small
    for (int i = 0; i < 5; i++)
        myhash[i] = i;

    printf("Small: size=%zu, buckets=%zu\n", myhash.size(), myhash.bucket_count());

    // shrink_to_fit should be no-op when using _small
    myhash.shrink_to_fit();
    assert(myhash.size() == 5);
    for (int i = 0; i < 5; i++)
        assert(myhash[i] == i);

    // Grow large then erase most
    for (int i = 5; i < 200; i++)
        myhash[i] = i;
    for (int i = 0; i < 190; i++)
        myhash.erase(i);

    printf("After erase: size=%zu, buckets=%zu, LF=%.4f\n",
        myhash.size(), myhash.bucket_count(), myhash.load_factor());

    myhash.shrink_to_fit();

    printf("After shrink: size=%zu, buckets=%zu, LF=%.4f\n",
        myhash.size(), myhash.bucket_count(), myhash.load_factor());

    // Verify remaining
    for (int i = 190; i < 200; i++) {
        assert(myhash.contains(i));
        assert(myhash[i] == i);
    }

    printf("shrink_to_fit test PASSED\n\n");
    return 0;
}

// Test: non-trivially-copyable type (std::string)
static int test_non_trivial_type()
{
    printf("=== Non-trivial type test (std::string) ===\n");

    emhash5::HashMap<int64_t, std::string> myhash;
    std::unordered_map<int64_t, std::string> refmap;

    for (int i = 0; i < 10; i++) {
        myhash[i] = "value_" + std::to_string(i);
        refmap[i] = "value_" + std::to_string(i);
    }

    assert(myhash.size() == refmap.size());
    for (auto& [k, v] : refmap) {
        assert(myhash[k] == v);
    }

    // Copy
    auto copy = myhash;
    assert(copy.size() == myhash.size());
    for (int i = 0; i < 10; i++)
        assert(copy[i] == "value_" + std::to_string(i));

    // Swap
    emhash5::HashMap<int64_t, std::string> other;
    other[100] = "other";
    myhash.swap(other);
    assert(myhash.size() == 1);
    assert(other.size() == 10);
    assert(myhash[100] == "other");
    for (int i = 0; i < 10; i++)
        assert(other[i] == "value_" + std::to_string(i));

    printf("Non-trivial type test PASSED\n\n");
    return 0;
}

// Stress test: many insert/erase cycles
static int test_stress()
{
    printf("=== Stress test ===\n");

    emhash5::HashMap<int64_t, int> myhash;
    std::unordered_map<int64_t, int> refmap;

    int64_t next_key = 0;
    for (int round = 0; round < 100; round++) {
        // Insert 20
        for (int i = 0; i < 20; i++) {
            myhash[next_key] = (int)round;
            refmap[next_key] = (int)round;
            next_key++;
        }

        // Erase 10
        int erased = 0;
        std::vector<int64_t> to_erase;
        for (auto& [k, v] : refmap) {
            if (erased >= 10) break;
            to_erase.push_back(k);
            erased++;
        }
        for (auto k : to_erase) {
            myhash.erase(k);
            refmap.erase(k);
        }

        // Verify every 20 rounds
        if (round % 20 == 0) {
            if (myhash.size() != refmap.size()) {
                printf("ERROR at round %d: size=%zu != refsize=%zu\n",
                    round, myhash.size(), refmap.size());
                return 1;
            }

            size_t count = 0;
            for (auto it = myhash.begin(); it != myhash.end(); ++it) count++;
            if (count != myhash.size()) {
                printf("ERROR at round %d: iter=%zu != size=%zu\n",
                    round, count, myhash.size());
                return 1;
            }

            printf("  Round %3d: size=%zu, LF=%.4f [OK]\n",
                round, myhash.size(), myhash.load_factor());
        }
    }

    // Final verify
    for (auto& [k, v] : refmap) {
        auto it = myhash.find(k);
        assert(it != myhash.end() && it->second == v);
    }

    printf("Stress test PASSED\n\n");
    return 0;
}

// Test: rehash from _small back to _small (shrink)
static int test_rehash_small_to_small()
{
    printf("=== Rehash small to small test ===\n");

    emhash5::HashMap<int64_t, int> myhash(4, 0.95f);

    for (int i = 0; i < 3; i++)
        myhash[i] = i;

    printf("After insert: size=%zu, buckets=%zu\n", myhash.size(), myhash.bucket_count());

    // Erase and shrink
    myhash.erase(0);
    myhash.shrink_to_fit();

    printf("After shrink: size=%zu, buckets=%zu\n", myhash.size(), myhash.bucket_count());

    assert(myhash.contains(1) && myhash[1] == 1);
    assert(myhash.contains(2) && myhash[2] == 2);
    assert(!myhash.contains(0));

    printf("Rehash small to small test PASSED\n\n");
    return 0;
}

int main()
{
    int failures = 0;
    failures += test_basic_small();
    failures += test_grow_beyond_small();
    failures += test_copy_small();
    failures += test_copy_large();
    failures += test_move();
    failures += test_move_large();
    failures += test_swap_small();
    failures += test_swap_small_large();
    failures += test_copy_assignment();
    failures += test_move_assignment();
    failures += test_clear_reuse();
    failures += test_shrink_to_fit();
    failures += test_non_trivial_type();
    failures += test_stress();
    failures += test_rehash_small_to_small();

    if (failures == 0)
        printf("All EMH_SMALL_SIZE tests PASSED!\n");
    else
        printf("%d tests FAILED!\n", failures);

    return failures;
}
