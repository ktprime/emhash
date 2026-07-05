// stress/test_stress_all.cpp
// Unified stress tests for all 7 map implementations.
// 5 categories × N trials, each verified against std::unordered_map oracle:
//   1. reserve(1) + tiny inserts (tiny-table kickout path)
//   2. high load factor (0.98) random ops
//   3. rapid insert/erase cycle
//   4. rehash stress (force many rehashes)
//   5. interleaved insert/find/erase
// Source: stress/stress_all_maps.cpp (426 lines).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

#include <random>
#include <unordered_map>
#include <vector>

static constexpr int TRIALS = 200; // keep CI fast

// ============================================================================
// 1. reserve(1) + 20 inserts (stress tiny-table kickout path)
// ============================================================================
TEST_CASE_TEMPLATE("stress: reserve(1) + tiny inserts", Map, AllIntMaps) {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(-100000000, 100000000);

    for (int trial = 0; trial < TRIALS; ++trial) {
        Map m;
        m.reserve(1);
        std::unordered_map<int, int> oracle;

        for (int i = 0; i < 20; ++i) {
            int k = dist(rng), v = dist(rng);
            m[k] = v;
            oracle[k] = v;
        }
        CHECK(m.size() == oracle.size());

        // every iterated key must be findable
        for (auto it = m.begin(); it != m.end(); ++it) {
            CHECK(m.find(it->first) != m.end());
            CHECK(oracle.count(it->first) == 1);
        }
    }
}

// ============================================================================
// 2. high load factor (0.98) with random ops
// ============================================================================
TEST_CASE_TEMPLATE("stress: high load factor random ops", Map, AllIntMaps) {
    std::mt19937 rng(54321);
    std::uniform_int_distribution<int> dist(0, 1000000);

    for (int trial = 0; trial < TRIALS; ++trial) {
        Map m;
        m.reserve(1000);
        m.max_load_factor(0.98f);
        std::unordered_map<int, int> oracle;

        for (int i = 0; i < 950; ++i) {
            int k = dist(rng);
            m[k] = i;
            oracle[k] = i;
        }
        for (int i = 0; i < 100; ++i) {
            int k = dist(rng);
            int op = rng() % 3;
            if (op == 0) {
                m[k] = i;
                oracle[k] = i;
            } else if (op == 1) {
                CHECK(m.count(k) == oracle.count(k));
            } else {
                auto me = m.erase(k);
                auto oe = oracle.erase(k);
                CHECK(me == oe);
            }
        }
        CHECK(m.size() == oracle.size());
    }
}

// ============================================================================
// 3. rapid insert/erase cycle
// ============================================================================
TEST_CASE_TEMPLATE("stress: rapid insert/erase cycle", Map, AllIntMaps) {
    std::mt19937 rng(99999);
    std::uniform_int_distribution<int> dist(0, 500);

    for (int trial = 0; trial < TRIALS; ++trial) {
        Map m;
        std::unordered_map<int, int> oracle;

        for (int i = 0; i < 500; ++i) {
            int k = dist(rng);
            m[k] = i;
            oracle[k] = i;
        }
        for (int i = 0; i < 250; ++i) {
            int k = dist(rng);
            m.erase(k);
            oracle.erase(k);
        }
        CHECK(m.size() == oracle.size());
        for (auto& [k, v] : oracle) {
            auto it = m.find(k);
            CHECK(it != m.end());
            CHECK(it->second == v);
        }
    }
}

// ============================================================================
// 4. rehash stress (force many rehashes via reserve + fill + clear + refill)
// ============================================================================
TEST_CASE_TEMPLATE("stress: rehash stress", Map, AllIntMaps) {
    for (int trial = 0; trial < 50; ++trial) {
        Map m;
        for (int cycle = 0; cycle < 5; ++cycle) {
            m.reserve(100 * (cycle + 1));
            for (int i = 0; i < 100 * (cycle + 1); ++i)
                m[i + cycle * 1000] = i;
            CHECK(m.size() == static_cast<size_t>(100 * (cycle + 1)));
            m.clear();
        }
    }
}

// ============================================================================
// 5. interleaved insert/find/erase (mixed workload)
// ============================================================================
TEST_CASE_TEMPLATE("stress: interleaved mixed workload", Map, AllIntMaps) {
    std::mt19937 rng(0xABCD);
    std::uniform_int_distribution<int> dist(0, 2000);

    for (int trial = 0; trial < TRIALS; ++trial) {
        Map m;
        std::unordered_map<int, int> oracle;

        for (int i = 0; i < 1000; ++i) {
            int k = dist(rng);
            int op = rng() % 4;
            if (op == 0) {
                m[k] = i;
                oracle[k] = i;
            } else if (op == 1) {
                CHECK(m.count(k) == oracle.count(k));
            } else if (op == 2) {
                CHECK(m.erase(k) == oracle.erase(k));
            } else {
                auto it = m.find(k);
                auto ot = oracle.find(k);
                CHECK((it == m.end()) == (ot == oracle.end()));
            }
        }
        CHECK(m.size() == oracle.size());
    }
}
