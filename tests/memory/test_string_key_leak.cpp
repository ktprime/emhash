// memory/test_string_key_leak.cpp
// LeakTracker-based leak detection: constructor/destructor balance across
// all map + set implementations with non-trivial key types.
// Note: intermediate s_alive checks are omitted because implicit conversion
// from LeakTracker to std::string creates temporaries that die immediately.
// The key invariant is: s_alive == 0 AND constructed == destructed at scope exit.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/trackers.hpp"

#include <string>

// ============================================================================
// 1. LeakTracker balance after insert + erase.
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: insert + erase", Map, AllStringMaps) {
    LeakTracker::reset();
    {
        Map m;
        for (int i = 0; i < 200; ++i)
            m[LeakTracker("lt_" + std::to_string(i))] = i;
        CHECK(m.size() == 200);

        for (int i = 0; i < 100; ++i)
            m.erase(LeakTracker("lt_" + std::to_string(i)));
        CHECK(m.size() == 100);
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}

// ============================================================================
// 2. LeakTracker copy / assignment balance.
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: copy ctor + assign", Map, AllStringMaps) {
    LeakTracker::reset();
    {
        Map m;
        for (int i = 0; i < 100; ++i)
            m[LeakTracker("ck_" + std::to_string(i))] = i;

        { Map m2(m); CHECK(m2.size() == 100); }
        { Map m3; m3[LeakTracker("temp")] = 0; m3 = m; CHECK(m3.size() == 100); }
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}

// ============================================================================
// 3. LeakTracker cross-bucket copy (sentinel leak regression).
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: cross-bucket copy", Map, AllStringMaps) {
    LeakTracker::reset();
    {
        Map src;
        for (int i = 0; i < 50; ++i)
            src[LeakTracker("xs_" + std::to_string(i))] = i;

        {
            Map dst;
            for (int i = 0; i < 200; ++i)
                dst[LeakTracker("xd_" + std::to_string(i))] = i;
            dst = src;
            CHECK(dst.size() == 50);
        }

        {
            Map dst;
            dst[LeakTracker("xd_empty")] = 0;
            dst = src;
            CHECK(dst.size() == 50);
        }

        { Map a(src); Map b(a); Map c(b); CHECK(c.size() == 50); }
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}

// ============================================================================
// 4. LeakTracker empty / single-element copy.
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: empty + single copy", Map, AllStringMaps) {
    LeakTracker::reset();
    {
        Map empty;
        { Map dst = empty; CHECK(dst.size() == 0); }

        Map one;
        one[LeakTracker("one")] = 42;
        { Map dst = one; CHECK(dst.size() == 1); }

        { Map dst = one; dst = dst; CHECK(dst.size() == 1); }
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}

// ============================================================================
// 5. LeakTracker rehash balance.
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: rehash", Map, AllStringMaps) {
    LeakTracker::reset();
    {
        Map m;
        for (int i = 0; i < 500; ++i)
            m[LeakTracker("rh_" + std::to_string(i))] = i;
        CHECK(m.size() == 500);
        m.clear();
        for (int i = 0; i < 500; ++i)
            m[LeakTracker("rh2_" + std::to_string(i))] = i;
        CHECK(m.size() == 500);
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}

// ============================================================================
// 6. LeakTracker stress (heavy churn).
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: stress 5000 entries", Map, AllStringMaps) {
    LeakTracker::reset();
    {
        Map m;
        for (int i = 0; i < 5000; ++i)
            m[LeakTracker("s_" + std::to_string(i))] = i;
        CHECK(m.size() == 5000);

        for (int i = 0; i < 5000; i += 2)
            m.erase(LeakTracker("s_" + std::to_string(i)));
        CHECK(m.size() == 2500);

        for (int i = 0; i < 5000; i += 2)
            m[LeakTracker("s_" + std::to_string(i))] = i;
        CHECK(m.size() == 5000);
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}

// ============================================================================
// 7. LeakTracker move semantics.
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: move ctor + assign", Map, AllStringMaps) {
    LeakTracker::reset();
    {
        Map m;
        for (int i = 0; i < 200; ++i)
            m[LeakTracker("mv_" + std::to_string(i))] = i;

        Map dst(std::move(m));
        CHECK(dst.size() == 200);

        Map dst2;
        dst2[LeakTracker("old")] = 0;
        dst2 = std::move(dst);
        CHECK(dst2.size() == 200);
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}

// ============================================================================
// 8. LeakTracker for HashSet (string keys).
// ============================================================================
TEST_CASE_TEMPLATE("LeakTracker balance: set string keys", Set, AllStringSets) {
    LeakTracker::reset();
    {
        Set s;
        for (int i = 0; i < 200; ++i)
            s.insert(LeakTracker("set_" + std::to_string(i)));
        CHECK(s.size() == 200);

        for (int i = 0; i < 100; ++i)
            s.erase(LeakTracker("set_" + std::to_string(i)));
        CHECK(s.size() == 100);

        Set s2(s);
        CHECK(s2.size() == 100);
    }
    CHECK(LeakTracker::s_alive == 0);
    CHECK(LeakTracker::s_constructed == LeakTracker::s_destructed);
}
