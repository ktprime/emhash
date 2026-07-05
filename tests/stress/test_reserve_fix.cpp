// stress/test_reserve_fix.cpp
// Regression test for reserve(1) crash: tiny initial table + kickout path
// previously caused segfault in emhash8. Tests the exact crash sequence
// from the fuzzer plus randomized variations.
// Source: stress/stress_fix.cpp (75 lines).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <random>

// ============================================================================
// reserve(1) + 20 random inserts (original crash trigger)
// ============================================================================
TEST_CASE_TEMPLATE("stress: reserve(1) + random inserts", Map, AllIntMaps) {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(-100000000, 100000000);

    for (int trial = 0; trial < 1000; ++trial) {
        Map m;
        m.reserve(1);
        for (int i = 0; i < 20; ++i) {
            m.insert({dist(rng), dist(rng)});
        }
        // every iterated key must be findable
        for (auto it = m.begin(); it != m.end(); ++it) {
            CHECK(m.find(it->first) != m.end());
        }
    }
}

// ============================================================================
// Exact crash sequence: insert 2, erase random, iterate, count, insert 3rd
// ============================================================================
TEST_CASE_TEMPLATE("stress: reserve(1) crash sequence", Map, AllIntMaps) {
    std::mt19937 rng(98765);
    std::uniform_int_distribution<int> dist(-1000000, 1000000);

    for (int trial = 0; trial < 5000; ++trial) {
        Map m;
        m.reserve(1);

        int k1 = dist(rng), v1 = dist(rng);
        int k2 = dist(rng), v2 = dist(rng);
        m.insert({k1, v1});
        m.insert({k2, v2});

        m.erase(dist(rng));

        for (auto it = m.begin(); it != m.end(); ++it)
            (void)it->first;
        m.count(dist(rng));

        m.insert({dist(rng), dist(rng)});

        for (auto it = m.begin(); it != m.end(); ++it) {
            CHECK(m.find(it->first) != m.end());
        }
    }
}

// ============================================================================
// reserve(0) edge case (should not crash, may be treated as reserve(1))
// ============================================================================
TEST_CASE_TEMPLATE("stress: reserve(0) edge case", Map, AllIntMaps) {
    Map m;
    m.reserve(0);
    m[1] = 10;
    m[2] = 20;
    CHECK(m.size() == 2);
    CHECK(m[1] == 10);
}
