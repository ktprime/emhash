// stress/test_highload.cpp
// Load-factor oscillation stress: fill to 99.9% capacity, erase a few,
// refill, repeat. Exercises rehash threshold arithmetic edge cases.
// Source: stress/highload_test.cpp (448 lines).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <vector>
#include <cstdio>

// ============================================================================
// LF=0.999 oscillation: fill near max, erase small batch, refill
// ============================================================================
TEST_CASE_TEMPLATE("stress: LF=0.999 oscillation", Map, AllIntMaps) {
    const int N = 10000;
    for (int cycle = 0; cycle < 5; ++cycle) {
        Map m;
        m.max_load_factor(0.99f);
        m.reserve(static_cast<size_t>(N));

        for (int i = 0; i < N; ++i)
            m[i] = i;
        CHECK(m.size() == static_cast<size_t>(N));

        // erase 1% and refill — oscillates near rehash threshold
        for (int i = 0; i < N; i += 100)
            m.erase(i);
        CHECK(m.size() == static_cast<size_t>(N - (N / 100)));

        for (int i = 0; i < N; i += 100)
            m[i] = i;
        CHECK(m.size() == static_cast<size_t>(N));

        // verify all keys
        for (int i = 0; i < N; ++i) {
            auto it = m.find(i);
            CHECK(it != m.end());
            CHECK(it->second == i);
        }
    }
}

// ============================================================================
// Incremental fill to exactly hit load-factor boundary
// ============================================================================
TEST_CASE_TEMPLATE("stress: incremental fill to LF boundary", Map, AllIntMaps) {
    Map m;
    m.max_load_factor(0.95f);
    m.reserve(1000);

    // Insert one at a time, verifying consistency at each step
    for (int i = 0; i < 1000; ++i) {
        m[i] = i * 2;
        // spot check
        if (i % 100 == 0) {
            CHECK(m.size() == static_cast<size_t>(i + 1));
            CHECK(m[i] == i * 2);
        }
    }
    CHECK(m.size() == 1000);

    // erase one at a time, verify rest intact
    for (int i = 0; i < 500; ++i) {
        m.erase(i);
        CHECK(m.size() == static_cast<size_t>(1000 - i - 1));
    }
    for (int i = 500; i < 1000; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(it->second == i * 2);
    }
}
