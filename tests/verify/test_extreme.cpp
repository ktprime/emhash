/**
 * EMH_FIND_HIT bug fix extreme tests
 *
 * Coverage: various integer key types, INACTIVE boundary values, residue checks after various operations
 *
 * Compile: g++ -std=c++17 -O2 -o test_extreme test/test_extreme.cpp
 */

#define EMH_FIND_HIT 1
#include "emhash/hash_table5.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <string>

static int total = 0, passed = 0;

#define CHECK(cond, msg) do { \
    total++; \
    if (cond) { passed++; } \
    else { printf("  FAIL [%d]: %s\n", total, msg); } \
} while(0)

// Forward declaration
template<typename Map, typename KeyT>
void fp_check(Map& map, KeyT inactive, const char* msg);

// ============================================================
// Generic test template: for any integer KeyT
// ============================================================
template<typename KeyT>
void test_int_type(const char* name)
{
    printf("\n--- %s (sizeof=%zu) ---\n", name, sizeof(KeyT));

    using Map = emhash5::HashMap<KeyT, KeyT>;
    const KeyT inactive = (KeyT)emhash5::INACTIVE;

    // 1. Empty map: find(key) for various boundary keys
    {
        Map map(16);
        CHECK(map.find(inactive) == map.end(), "empty find(INACTIVE)");
        CHECK(map.find(KeyT(0)) == map.end(), "empty find(0)");
        CHECK(!map.contains(inactive), "empty contains(INACTIVE)");
    }

    // 2. Empty map: find(key, hash) iterate all buckets
    {
        Map map(16);
        fp_check(map, inactive, "empty find(INACTIVE,hash) no false positives");
    }

    // 3. Insert INACTIVE key then normal find
    {
        Map map(16);
        (void)map.insert_unique(inactive, KeyT(99));
        auto it = map.find(inactive);
        CHECK(it != map.end() && it->second == KeyT(99), "find inserted INACTIVE key");

        // Find after erase
        (void)map.erase(inactive);
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after erase");
        fp_check(map, inactive, "find(INACTIVE,hash) after erase");
    }

    // 4. Insert large data then clear, check residue
    {
        Map map(16);
        for (KeyT i = 0; i < KeyT(200); i++)
            (void)map.insert_unique(i, i);
        map.clear();
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after clear");
        fp_check(map, inactive, "find(INACTIVE,hash) after clear");
    }

    // 5. Check after rehash
    {
        Map map(4);
        for (KeyT i = 0; i < KeyT(200); i++)
            (void)map.insert_unique(i, i);
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after rehash");
        fp_check(map, inactive, "find(INACTIVE,hash) after rehash");
    }

    // 6. Insert-delete-insert loop, create many holes
    {
        Map map(32);
        for (int round = 0; round < 5; round++) {
            for (KeyT i = 0; i < KeyT(100); i++)
                map[i] = i;
            for (KeyT i = KeyT(1); i < KeyT(100); i += KeyT(2))
                (void)map.erase(i);
        }
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after churn");
        fp_check(map, inactive, "find(INACTIVE,hash) after churn");
    }

    // 7. Check after swap
    {
        Map map1(16), map2(64);
        for (KeyT i = 0; i < KeyT(50); i++) map1[i] = i;
        for (KeyT i = 0; i < KeyT(30); i++) map2[i] = i;
        map1.swap(map2);
        CHECK(map1.find(inactive) == map1.end(), "find INACTIVE after swap (map1)");
        CHECK(map2.find(inactive) == map2.end(), "find INACTIVE after swap (map2)");
    }

    // 8. Check after reserve
    {
        Map map;
        (void)map.reserve(1000);
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after reserve");
        fp_check(map, inactive, "find(INACTIVE,hash) after reserve");
    }

    // 9. Check after shrink_to_fit
    {
        Map map(128);
        for (KeyT i = 0; i < KeyT(10); i++) map[i] = i;
        map.shrink_to_fit();
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after shrink_to_fit");
    }

    // 10. key == INACTIVE-1 (sentinel value -2)
    {
        Map map(16);
        KeyT sentinel = KeyT(KeyT(0) - 2);
        (void)map.insert_unique(sentinel, KeyT(77));
        auto it = map.find(sentinel);
        CHECK(it != map.end() && it->second == KeyT(77), "find sentinel key (-2)");
        (void)map.erase(sentinel);
        CHECK(map.find(sentinel) == map.end(), "find sentinel after erase");
    }
}

template<typename Map, typename KeyT>
void fp_check(Map& map, KeyT inactive, const char* msg)
{
    int fp = 0;
    for (size_t b = 0; b < (size_t)map.bucket_count(); b++) {
        if (map.find(inactive, b) != map.end()) fp++;
    }
    CHECK(fp == 0, msg);
}

// ============================================================
// Non-integer key (string) should not be affected
// ============================================================
void test_string_key()
{
    printf("\n--- std::string key ---\n");
    using Map = emhash5::HashMap<std::string, int>;
    Map map(16);
    CHECK(map.find("") == map.end(), "empty string find");
    CHECK(map.find(std::string(256, '\xFF')) == map.end(), "long FF string find");

    map["hello"] = 1;
    map[""] = 2;
    CHECK(map.find("hello") != map.end(), "find hello");
    CHECK(map.find("") != map.end(), "find empty string");
    (void)map.erase("");
    CHECK(map.find("") == map.end(), "find empty string after erase");
}

// ============================================================
// Extreme capacity tests
// ============================================================
void test_extreme_sizes()
{
    printf("\n--- Extreme sizes ---\n");
    using Map = emhash5::HashMap<int32_t, int32_t>;
    const int32_t inactive = (int32_t)emhash5::INACTIVE;

    // Minimal capacity
    {
        Map map(1);
        CHECK(map.find(inactive) == map.end(), "size=1 find INACTIVE");
    }

    // Large capacity empty map
    {
        Map map(1 << 20); // 1M buckets
        CHECK(map.find(inactive) == map.end(), "1M buckets find INACTIVE");
        // Spot check a few hash values
        bool ok = true;
        for (size_t h = 0; h < 100; h++) {
            if (map.find(inactive, h) != map.end()) { ok = false; break; }
        }
        CHECK(ok, "1M buckets find(INACTIVE,hash) spot check");
    }
}

// ============================================================
// Concurrent operation sequence: alternating insert/erase to create maximum holes
// ============================================================
void test_alternating_ops()
{
    printf("\n--- Alternating insert/erase ---\n");
    using Map = emhash5::HashMap<int32_t, int32_t>;
    const int32_t inactive = (int32_t)emhash5::INACTIVE;
    Map map(32);

    for (int round = 0; round < 10; round++) {
        for (int32_t i = 0; i < 1000; i++)
            map[i * 3] = i;
        for (int32_t i = 0; i < 1000; i += 2)
            (void)map.erase(i * 3);
    }
    CHECK(map.find(inactive) == map.end(), "find INACTIVE after alternating ops");

    int fp = 0;
    for (size_t b = 0; b < (size_t)map.bucket_count(); b++) {
        if (map.find(inactive, b) != map.end()) fp++;
    }
    CHECK(fp == 0, "find(INACTIVE,hash) after alternating ops");
}

// ============================================================
// INACTIVE boundary key value tests
// ============================================================
void test_inactive_boundary()
{
    printf("\n--- INACTIVE boundary keys ---\n");
    using Map = emhash5::HashMap<int32_t, int32_t>;
    const int32_t inactive = (int32_t)emhash5::INACTIVE;
    Map map(16);

    int32_t keys[] = {inactive - 3, inactive - 2, inactive - 1, inactive, inactive + 1, 0, 1, 2};
    for (auto k : keys)
        map[k] = k * 10;

    bool all_found = true;
    for (auto k : keys) {
        auto it = map.find(k);
        if (it == map.end() || it->second != k * 10) {
            all_found = false;
            printf("    key=%d: expected val=%d, ", k, k * 10);
            if (it == map.end()) printf("NOT FOUND\n");
            else printf("got val=%d\n", it->second);
        }
    }
    CHECK(all_found, "all boundary keys found correctly");

    // Erase one by one then verify
    for (auto k : keys) {
        (void)map.erase(k);
        if (map.find(k) != map.end()) {
            CHECK(false, "key still found after erase");
            return;
        }
    }
    CHECK(true, "all boundary keys erased correctly");

    // After erase, find(key, hash) should not have false positives
    int fp = 0;
    for (size_t b = 0; b < (size_t)map.bucket_count(); b++) {
        for (auto k : keys) {
            if (map.find(k, b) != map.end()) fp++;
        }
    }
    CHECK(fp == 0, "no false positives for boundary keys after erase");
}

int main()
{
    printf("=== EMH_FIND_HIT extreme bug fix test ===\n");
    printf("INACTIVE = 0x%llx, sizeof(size_type) = %zu\n",
           (unsigned long long)emhash5::INACTIVE, sizeof(emhash5::size_type));

    test_int_type<int8_t>("int8_t");
    test_int_type<int16_t>("int16_t");
    test_int_type<int32_t>("int32_t");
    test_int_type<int64_t>("int64_t");
    test_int_type<uint8_t>("uint8_t");
    test_int_type<uint16_t>("uint16_t");
    test_int_type<uint32_t>("uint32_t");
    test_int_type<uint64_t>("uint64_t");

    test_string_key();
    test_extreme_sizes();
    test_alternating_ops();
    test_inactive_boundary();

    printf("\n=== Results: %d/%d passed ===\n", passed, total);
    if (passed != total) {
        printf("*** SOME TESTS FAILED ***\n");
        return 1;
    }
    printf("*** ALL TESTS PASSED ***\n");
    return 0;
}