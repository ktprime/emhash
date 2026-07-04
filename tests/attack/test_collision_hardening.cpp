// attack/test_collision_hardening.cpp
// Collision hardening tests: verify no crash / corruption / quadratic blowup
// under worst-case collision patterns.
//   1. 100% same-bucket collision (all keys -> bucket 0)
//   2. 4-bucket concentration (keys -> 4 buckets)
//   3. Adaptive key craft (sequential keys forcing probe chain growth)
// Requires EMH_SAFE_PSL compile definition.
// Source: attack/test_collision_hardening.cpp (211 lines).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/hashers.hpp"

#include <chrono>
#include <cstdio>
#include <random>
#include <unordered_set>
#include <vector>

// ============================================================================
// 1. 100% same-bucket collision (ConstHasher)
// ============================================================================
TEST_CASE_TEMPLATE("hardening: 100% same-bucket collision", Map,
    map5<int,int,ConstHasher>, map6<int,int,ConstHasher>,
    map7<int,int,ConstHasher>, map8<int,int,ConstHasher>) {
    const int N = 8000;
    Map m;

    for (int i = 0; i < N; ++i) (void)m.emplace(i, i);
    CHECK(static_cast<int>(m.size()) == N);

    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }

    // every-other erase + re-find
    for (int i = 0; i < N; i += 2) m.erase(i);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        if (i & 1) { CHECK(it != m.end()); CHECK(it->second == i); }
        else       CHECK(it == m.end());
    }

    // reinsert erased keys
    for (int i = 0; i < N; i += 2) (void)m.emplace(i, i * 10);
    CHECK(static_cast<int>(m.size()) == N);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        if (i & 1) CHECK(it->second == i);
        else       CHECK(it->second == i * 10);
    }
}

// ============================================================================
// 2. 4-bucket concentration (Range4Hasher)
// ============================================================================
TEST_CASE_TEMPLATE("hardening: 4-bucket concentration", Map,
    map5<int,int,Range4Hasher>, map6<int,int,Range4Hasher>,
    map7<int,int,Range4Hasher>, map8<int,int,Range4Hasher>) {
    const int N = 4000;
    Map m;

    for (int i = 0; i < N; ++i) (void)m.emplace(i, i);
    CHECK(static_cast<int>(m.size()) == N);

    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }

    // erase + churn
    for (int round = 0; round < 3; ++round) {
        for (int i = round; i < N; i += 3) m.erase(i);
        for (int i = round; i < N; i += 3) (void)m.emplace(i, i + round);
    }
    CHECK(static_cast<int>(m.size()) == N);
}

// ============================================================================
// 3. Adaptive key craft: keys that force probe chain growth
//    Use keys = small multiples of bucket_count to cluster them.
// ============================================================================
TEST_CASE("hardening: adaptive key craft (emhash5)") {
    using Map = map5<int, int>;
    const int N = 6000;
    Map m;
    m.reserve(static_cast<size_t>(N) * 2);

    // Insert keys that are multiples of the (power-of-two) bucket count,
    // forcing them to share the same hash slot.
    const auto bc = m.bucket_count();
    for (int i = 0; i < N; ++i) {
        (void)m.emplace(static_cast<int>(i * bc), i);
    }
    CHECK(static_cast<int>(m.size()) == N);

    for (int i = 0; i < N; ++i) {
        auto it = m.find(static_cast<int>(i * bc));
        CHECK(it != m.end());
        CHECK(it->second == i);
    }

    for (int i = 0; i < N; i += 2) m.erase(static_cast<int>(i * bc));
    for (int i = 0; i < N; ++i) {
        auto it = m.find(static_cast<int>(i * bc));
        if (i & 1) CHECK(it != m.end());
        else       CHECK(it == m.end());
    }
}

// ============================================================================
// 4. Random-key baseline (default hash) — correctness reference
// ============================================================================
TEST_CASE_TEMPLATE("hardening: random key baseline", Map, AllIntMaps) {
    const int N = 5000;
    Map m;
    std::mt19937_64 rng(0xDEADBEEFU);
    std::unordered_set<int> keys;

    for (int i = 0; i < N; ++i) {
        int k = static_cast<int>(rng());
        keys.insert(k);
        (void)m.emplace(k, i);
    }
    CHECK(m.size() == keys.size());

    for (int k : keys) {
        auto it = m.find(k);
        CHECK(it != m.end());
    }
}
