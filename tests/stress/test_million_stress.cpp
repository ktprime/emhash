// stress/test_million_stress.cpp
// Large-scale stress test: N=1,000,000 elements CRUD + iteration.
// Validates behavior at scale across all map & set implementations:
//   - large-scale rehashing (growth from 0 to 1M)
//   - iterator correctness over a large range
//   - erase/reinsert consistency at scale
//   - no accumulated errors over many operations
//
// Configurable scale via -DEMH_MILLION_N (default 1,000,000).
// Use a smaller N (e.g. 100000) for sanitizer/coverage builds.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

#include <cstddef>

#ifndef EMH_MILLION_N
#define EMH_MILLION_N 1000000
#endif

// ============================================================================
// Map: N int->int insert / find / erase-half / reinsert / iterate / clear
// ============================================================================
TEST_CASE_TEMPLATE("stress: million int map CRUD cycle", Map, AllIntMaps) {
    const int N = EMH_MILLION_N;
    Map m;

    // Phase 1: insert N unique keys (forces multiple rehashes)
    for (int i = 0; i < N; ++i)
        m[i] = i * 2;
    CHECK(m.size() == static_cast<std::size_t>(N));

    // Phase 2: find all keys, verify value
    long long sum = 0;
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        sum += it->second;
    }
    // sum(i*2) for i=0..N-1 = N*(N-1)
    CHECK(sum == static_cast<long long>(N) * (N - 1));

    // Phase 3: erase all even keys (half the elements)
    std::size_t erased = 0;
    for (int i = 0; i < N; i += 2)
        erased += static_cast<std::size_t>(m.erase(i));
    const std::size_t expected_erased = static_cast<std::size_t>((N + 1) / 2);
    CHECK(erased == expected_erased);
    CHECK(m.size() == static_cast<std::size_t>(N) - expected_erased);

    // Phase 4: verify even keys gone, odd keys remain
    for (int i = 0; i < N; ++i) {
        if (i % 2 == 0) {
            CHECK(m.find(i) == m.end());
        } else {
            auto it = m.find(i);
            CHECK(it != m.end());
            CHECK(it->second == i * 2);
        }
    }

    // Phase 5: reinsert erased even keys
    for (int i = 0; i < N; i += 2)
        m[i] = i * 2;
    CHECK(m.size() == static_cast<std::size_t>(N));

    // Phase 6: iterate and count, verify each entry
    std::size_t count = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        CHECK(it->second == it->first * 2);
        ++count;
    }
    CHECK(count == static_cast<std::size_t>(N));

    // Phase 7: overwrite all values, verify update path
    for (int i = 0; i < N; ++i)
        m[i] = i * 3;
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it->second == i * 3);
    }

    // Phase 8: clear and verify empty
    m.clear();
    CHECK(m.size() == 0);
    CHECK(m.begin() == m.end());
    for (int i = 0; i < N; ++i)
        CHECK(m.find(i) == m.end());
}

// ============================================================================
// Set: N int insert / find / erase-half / reinsert / iterate / clear
// ============================================================================
TEST_CASE_TEMPLATE("stress: million int set CRUD cycle", Set, AllIntSets) {
    const int N = EMH_MILLION_N;
    Set s;

    // Phase 1: insert N unique keys
    for (int i = 0; i < N; ++i)
        s.insert(i);
    CHECK(s.size() == static_cast<std::size_t>(N));

    // Phase 2: find all keys
    for (int i = 0; i < N; ++i)
        CHECK(s.find(i) != s.end());

    // Phase 3: erase all even keys (half)
    std::size_t erased = 0;
    for (int i = 0; i < N; i += 2)
        erased += static_cast<std::size_t>(s.erase(i));
    const std::size_t expected_erased = static_cast<std::size_t>((N + 1) / 2);
    CHECK(erased == expected_erased);
    CHECK(s.size() == static_cast<std::size_t>(N) - expected_erased);

    // Phase 4: verify even gone, odd remain
    for (int i = 0; i < N; ++i) {
        if (i % 2 == 0)
            CHECK(s.find(i) == s.end());
        else
            CHECK(s.find(i) != s.end());
    }

    // Phase 5: reinsert erased keys
    for (int i = 0; i < N; i += 2)
        s.insert(i);
    CHECK(s.size() == static_cast<std::size_t>(N));

    // Phase 6: iterate and count
    std::size_t count = 0;
    for (auto it = s.begin(); it != s.end(); ++it)
        ++count;
    CHECK(count == static_cast<std::size_t>(N));

    // Phase 7: clear
    s.clear();
    CHECK(s.size() == 0);
    CHECK(s.begin() == s.end());
}
