// unit/test_hashset.cpp
// HashSet API coverage for all 5 set implementations (emhash2/4/8 + emihset2/3).
// Covers: insert/find/erase/contains/count, iteration, copy/move, reserve/clear,
//         insert_unique, merge, erase_if, shrink_to_fit.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

#include <vector>
#include <algorithm>

// ============================================================================
// Universal API: insert / find / erase / contains / count (all 5 sets)
// ============================================================================
TEST_CASE_TEMPLATE("set insert + find + contains + count", Set, AllIntSets) {
    Set s;
    CHECK(s.empty());
    CHECK(s.size() == 0);

    auto r1 = s.insert(1);
    CHECK(r1.second);
    auto r2 = s.insert(1);
    CHECK(!r2.second);

    s.insert(2);
    s.insert(3);
    CHECK(s.size() == 3);

    CHECK(s.contains(1));
    CHECK(s.contains(2));
    CHECK(!s.contains(99));

    CHECK(s.count(1) == 1);
    CHECK(s.count(99) == 0);

    CHECK(s.find(1) != s.end());
    CHECK(s.find(99) == s.end());
}

TEST_CASE_TEMPLATE("set erase by key and iterator", Set, AllIntSets) {
    Set s;
    for (int i = 0; i < 10; ++i)
        s.insert(i);
    CHECK(s.size() == 10);

    CHECK(s.erase(5) == 1);
    CHECK(!s.contains(5));
    CHECK(s.erase(99) == 0);
    CHECK(s.size() == 9);

    auto it = s.find(3);
    CHECK(it != s.end());
    s.erase(it);
    CHECK(!s.contains(3));
    CHECK(s.size() == 8);
}

TEST_CASE_TEMPLATE("set emplace returns pair", Set, AllIntSets) {
    Set s;
    auto r = s.emplace(42);
    CHECK(r.second);
    CHECK(s.contains(42));
    auto r2 = s.emplace(42);
    CHECK(!r2.second);
}

// ============================================================================
// Iteration (all 5 sets)
// ============================================================================
TEST_CASE_TEMPLATE("set iteration visits all keys", Set, AllIntSets) {
    Set s;
    for (int i = 0; i < 100; ++i)
        s.insert(i);

    int count = 0;
    std::vector<bool> found(100, false);
    for (auto key : s) {
        if (key >= 0 && key < 100)
            found[key] = true;
        ++count;
    }
    CHECK(count == 100);
    for (int i = 0; i < 100; ++i)
        CHECK(found[i]);
}

TEST_CASE_TEMPLATE("set empty iteration begin==end", Set, AllIntSets) {
    Set s;
    CHECK(s.begin() == s.end());
    int count = 0;
    for (auto k : s) {
        (void)k;
        ++count;
    }
    CHECK(count == 0);
}

// ============================================================================
// Copy / move semantics (all 5 sets)
// ============================================================================
TEST_CASE_TEMPLATE("set copy ctor and assign", Set, AllIntSets) {
    Set s;
    for (int i = 0; i < 50; ++i)
        s.insert(i);

    Set s2(s);
    CHECK(s2.size() == 50);
    CHECK(s2.contains(25));

    Set s3;
    s3 = s;
    CHECK(s3.size() == 50);
    CHECK(s3.contains(10));

    // Modify copy should not affect original
    s3.insert(999);
    CHECK(!s.contains(999));
    CHECK(s3.contains(999));
}

TEST_CASE_TEMPLATE("set move ctor and assign", Set, AllIntSets) {
    Set s;
    for (int i = 0; i < 50; ++i)
        s.insert(i);

    Set s2(std::move(s));
    CHECK(s2.size() == 50);
    CHECK(s2.contains(25));

    Set s3;
    s3 = std::move(s2);
    CHECK(s3.size() == 50);
    CHECK(s3.contains(25));
}

TEST_CASE_TEMPLATE("set swap", Set, AllIntSets) {
    Set a;
    Set b;
    for (int i = 0; i < 10; ++i)
        a.insert(i);
    for (int i = 100; i < 110; ++i)
        b.insert(i);

    a.swap(b);
    CHECK(a.size() == 10);
    CHECK(b.size() == 10);
    CHECK(a.contains(105));
    CHECK(b.contains(5));
}

// ============================================================================
// Reserve / clear / reuse (all 5 sets)
// ============================================================================
TEST_CASE_TEMPLATE("set reserve and clear", Set, AllIntSets) {
    Set s;
    s.reserve(500);

    for (int i = 0; i < 200; ++i)
        s.insert(i);
    CHECK(s.size() == 200);

    s.clear();
    CHECK(s.empty());

    // Reuse after clear
    for (int i = 0; i < 50; ++i)
        s.insert(i + 1000);
    CHECK(s.size() == 50);
    CHECK(s.contains(1049));
}

TEST_CASE_TEMPLATE("set load factor and bucket count", Set, AllIntSets) {
    Set s;
    s.reserve(100);
    for (int i = 0; i < 80; ++i)
        s.insert(i);

    CHECK(s.bucket_count() > 0);
    float lf = s.load_factor();
    CHECK(lf >= 0.0F);
    CHECK(lf < 1.0F);
    float max_lf = s.max_load_factor();
    CHECK(max_lf > 0.0F);
    (void)lf;
    (void)max_lf;
}

// ============================================================================
// Extended API: insert_unique, merge, erase_if, shrink_to_fit
// ============================================================================
TEST_CASE_TEMPLATE("set insert_unique", Set, AllIntSets) {
    Set s;
    s.insert(1);
    s.insert(2);

    s.insert_unique(3);
    s.insert_unique(4);
    CHECK(s.size() == 4);
    CHECK(s.contains(3));
    CHECK(s.contains(4));
}

TEST_CASE_TEMPLATE("set merge", Set, AllIntSets) {
    Set a;
    Set b;
    for (int i = 0; i < 20; ++i)
        a.insert(i);
    for (int i = 10; i < 30; ++i)
        b.insert(i);

    a.merge(b);
    // a should now contain 0..29
    CHECK(a.size() == 30);
    for (int i = 0; i < 30; ++i)
        CHECK(a.contains(i));
}

TEST_CASE_TEMPLATE("set merge empty dst", Set, AllIntSets) {
    // Coverage: merge on empty set triggers *this = std::move(rhs)
    Set a; // empty
    Set b;
    for (int i = 0; i < 20; ++i)
        b.insert(i);
    CHECK(a.empty());
    CHECK(b.size() == 20);

    a.merge(b);
    CHECK(a.size() == 20);
    CHECK(b.empty()); // b was moved
    for (int i = 0; i < 20; ++i)
        CHECK(a.contains(i));
}

TEST_CASE_TEMPLATE("set copy from empty", Set, AllIntSets) {
    // Coverage: copy/clone from empty source triggers clear() or equivalent
    Set a;
    for (int i = 0; i < 10; ++i)
        a.insert(i);
    CHECK(a.size() == 10);

    Set b; // empty
    a = b; // copy from empty
    CHECK(a.empty());
    CHECK(b.empty());
}

TEST_CASE_TEMPLATE("set erase_if", Set, AllIntSets) {
    Set s;
    for (int i = 0; i < 100; ++i)
        s.insert(i);

    auto erased = s.erase_if([](int v) { return v % 2 == 0; });
    CHECK(erased == 50);
    CHECK(s.size() == 50);
    for (int i = 0; i < 100; ++i) {
        if (i % 2 == 0)
            CHECK(!s.contains(i));
        else
            CHECK(s.contains(i));
    }
}

TEST_CASE_TEMPLATE("set shrink_to_fit", Set, AllIntSets) {
    Set s;
    for (int i = 0; i < 1000; ++i)
        s.insert(i);
    for (int i = 0; i < 900; ++i)
        s.erase(i);

    auto bc_before = s.bucket_count();
    s.shrink_to_fit();
    CHECK(s.bucket_count() <= bc_before);
    CHECK(s.size() == 100);
    for (int i = 900; i < 1000; ++i)
        CHECK(s.contains(i));
}
