// stress/test_bad_hash.cpp
// Stress tests with pathological hashers (ConstHasher = all same bucket,
// Range4Hasher = only 4 buckets). Verifies correctness under extreme collision.
// Source: stress/test_bad_hash_stress.cpp (199 lines).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/hashers.hpp"

#include <unordered_map>
#include <vector>
#include <random>

// Type lists with bad hashers (only emhash5-8 used for determinism)
#define ConstIntMaps map5<int, int, ConstHasher>, map6<int, int, ConstHasher>, \
    map7<int, int, ConstHasher>, map8<int, int, ConstHasher>
#define Range4IntMaps map5<int, int, Range4Hasher>, map6<int, int, Range4Hasher>, \
    map7<int, int, Range4Hasher>, map8<int, int, Range4Hasher>

// ============================================================================
// ConstHasher stress: all keys collide into bucket 0
// ============================================================================
TEST_CASE_TEMPLATE("stress: ConstHasher all-collision", Map, ConstIntMaps) {
    std::mt19937 rng(42);
    std::unordered_map<int, int> oracle;

    {
        Map m;
        for (int i = 0; i < 1000; ++i) {
            int k = rng() % 5000;
            m[k] = i;
            oracle[k] = i;
        }
        CHECK(m.size() == oracle.size());
        for (auto& [k, v] : oracle) {
            auto it = m.find(k);
            CHECK(it != m.end());
            CHECK(it->second == v);
        }

        // erase half
        int erased = 0;
        for (auto& [k, v] : oracle) {
            if (erased % 2 == 0) { m.erase(k); }
            ++erased;
        }
        // verify remaining
        erased = 0;
        for (auto& [k, v] : oracle) {
            if (erased % 2 == 0) { CHECK(m.find(k) == m.end()); }
            else { auto it = m.find(k); CHECK(it != m.end()); CHECK(it->second == v); }
            ++erased;
        }
    }
}

// ============================================================================
// Range4Hasher stress: only 4 distinct buckets
// ============================================================================
TEST_CASE_TEMPLATE("stress: Range4Hasher 4-bucket concentration", Map, Range4IntMaps) {
    std::mt19937 rng(7);
    std::unordered_map<int, int> oracle;

    Map m;
    for (int i = 0; i < 2000; ++i) {
        int k = rng() % 10000;
        m[k] = i;
        oracle[k] = i;
    }
    CHECK(m.size() == oracle.size());

    // full verify
    for (auto& [k, v] : oracle) {
        auto it = m.find(k);
        CHECK(it != m.end());
        CHECK(it->second == v);
    }

    // churn: erase + reinsert
    std::vector<int> keys_to_erase;
    for (auto& [k, v] : oracle) keys_to_erase.push_back(k);
    for (int i = 0; i < static_cast<int>(keys_to_erase.size()) / 2; ++i) {
        m.erase(keys_to_erase[i]);
    }
    for (int i = 0; i < static_cast<int>(keys_to_erase.size()) / 2; ++i) {
        m[keys_to_erase[i]] = oracle[keys_to_erase[i]];
    }
    CHECK(m.size() == oracle.size());
}

// ============================================================================
// LinearHasher (identity) stress: sequential keys, no collision
// ============================================================================
TEST_CASE_TEMPLATE("stress: LinearHasher sequential keys", Map, AllIntMaps) {
    Map m;
    const int N = 5000;
    for (int i = 0; i < N; ++i) m[i] = i;
    CHECK(m.size() == static_cast<size_t>(N));

    for (int i = 0; i < N; ++i) {
        CHECK(m[i] == i);
    }

    for (int i = 0; i < N; i += 3) m.erase(i);
    for (int i = 0; i < N; ++i) {
        if (i % 3 == 0) CHECK(m.find(i) == m.end());
        else CHECK(m.find(i) != m.end());
    }
}
