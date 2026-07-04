// memory/test_lifecycle_audit.cpp
// Full object lifecycle audit for all 7 map implementations.
// Uses std::string key+value (non-trivially-copyable) to maximize dtor coverage.
// Leaks detected by ASan/MSan at exit.
//
// 11 audits: default ctor/dtor, insert+clear, deep copy chain, move ctor,
//            cross-size copy assign, move assign, self-assign, churn, swap,
//            reserve/shrink, heavy rehash.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <string>

// String->String map type list for non-trivial key+value lifecycle stress.
#define StringStringMaps map5<std::string, std::string>, map6<std::string, std::string>, \
    map7<std::string, std::string>, map8<std::string, std::string>, \
    imap1<std::string, std::string>, imap2<std::string, std::string>, \
    imap3<std::string, std::string>

static std::string K(int i) { return "k_" + std::to_string(i); } // NOLINT(readability-identifier-naming)
static std::string V(int i) { return "v_" + std::to_string(i); } // NOLINT(readability-identifier-naming)

// ============================================================================
// 1. default ctor/dtor
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: default ctor/dtor", Map, StringStringMaps) {
    { Map m; (void)m; }
}

// ============================================================================
// 2. insert + clear
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: insert + clear", Map, StringStringMaps) {
    Map m;
    for (int i = 0; i < 50; ++i) m[K(i)] = V(i);
    CHECK(m.size() == 50);
    CHECK(m[K(0)] == V(0));
    m.clear();
    CHECK(m.empty());
}

// ============================================================================
// 3. deep copy chain (3 levels)
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: deep copy chain", Map, StringStringMaps) {
    Map src;
    for (int i = 0; i < 20; ++i) src[K(i)] = V(i);
    Map a(src);
    Map b(a); // NOLINT(performance-unnecessary-copy-initialization)
    Map c(b); // NOLINT(performance-unnecessary-copy-initialization)
    CHECK(c.size() == 20);
    CHECK(c[K(0)] == V(0));
}

// ============================================================================
// 4. move ctor
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: move ctor", Map, StringStringMaps) {
    Map src;
    for (int i = 0; i < 20; ++i) src[K(i)] = V(i);
    auto n = src.size();
    Map dst(std::move(src));
    CHECK(dst.size() == n);
    CHECK(dst[K(10)] == V(10));
}

// ============================================================================
// 5. cross-size copy assign (small<->large) — clone rehash path
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: cross-size copy assign", Map, StringStringMaps) {
    Map small;
    for (int i = 0; i < 5; ++i) small[K(i)] = V(i);
    Map large;
    for (int i = 0; i < 100; ++i) large[K(i)] = V(i);

    Map dst1 = small;
    dst1 = large;
    CHECK(dst1.size() == 100);

    Map dst2 = large;
    dst2 = small;
    CHECK(dst2.size() == 5);
}

// ============================================================================
// 6. move assign
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: move assign", Map, StringStringMaps) {
    Map src;
    for (int i = 0; i < 20; ++i) src[K(i)] = V(i);
    Map dst;
    for (int i = 0; i < 5; ++i) dst[K(i)] = V(i);
    dst = std::move(src);
    CHECK(dst.size() == 20);
}

// ============================================================================
// 7. self-assignment
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: self assignment", Map, StringStringMaps) {
    Map m;
    for (int i = 0; i < 10; ++i) m[K(i)] = V(i);
    auto n = m.size();
    m = m;
    CHECK(m.size() == n);
}

// ============================================================================
// 8. churn (insert/erase cycle)
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: churn", Map, StringStringMaps) {
    Map m;
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 50; ++i) m[K(i)] = V(i);
        for (int i = 0; i < 50; i += 2) m.erase(K(i));
    }
}

// ============================================================================
// 9. swap
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: swap", Map, StringStringMaps) {
    Map a;
    Map b;
    for (int i = 0; i < 10; ++i) a[K(i)] = V(i);
    a.swap(b);
    CHECK(a.empty());
    CHECK(b.size() == 10);
}

// ============================================================================
// 10. reserve / shrink_to_fit
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: reserve + shrink", Map, StringStringMaps) {
    Map m;
    m.reserve(200);
    for (int i = 0; i < 100; ++i) m[K(i)] = V(i);
    m.shrink_to_fit();
    m.clear();
    m.reserve(500);
    CHECK(m.empty());
}

// ============================================================================
// 11. heavy rehash (fill, clear, refill)
// ============================================================================
TEST_CASE_TEMPLATE("lifecycle: heavy rehash", Map, StringStringMaps) {
    Map m;
    for (int i = 0; i < 200; ++i) m[K(i)] = V(i);
    m.clear();
    for (int i = 0; i < 200; ++i) m[K(i)] = V(i);
    CHECK(m.size() == 200);
    CHECK(m[K(0)] == V(0));
}
