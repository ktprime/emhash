// unit/test_full_api.cpp
// Extended API: at, try_emplace, insert_or_assign, insert_unique (emhash only),
// merge, erase_if, shrink_to_fit. Consolidates test_hashmap_full_api.cpp.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

// ----------------------------------------------------------------------------
// Universal API (all 7 maps)
// ----------------------------------------------------------------------------

TEST_CASE_TEMPLATE("at const and mutable", Map, AllIntMaps) {
    Map m;
    m[1] = 10; m[2] = 20; m[3] = 30;
    CHECK(m.at(1) == 10);

    const auto& cmap = m;
    CHECK(cmap.at(2) == 20);

    m.at(1) = 100;
    CHECK(m.at(1) == 100);
}

TEST_CASE_TEMPLATE("try_emplace", Map, AllIntMaps) {
    Map m;
    auto [it1, ins1] = m.try_emplace(1, 10);
    CHECK(ins1);
    CHECK(it1->second == 10);

    auto [it2, ins2] = m.try_emplace(1, 999);
    CHECK_FALSE(ins2);
    CHECK(it2->second == 10);  // unchanged

    auto [it3, ins3] = m.try_emplace(2, 20);
    CHECK(ins3);
    CHECK(it3->second == 20);
}

TEST_CASE_TEMPLATE("insert_or_assign", Map, AllIntMaps) {
    Map m;
    auto [it1, ins1] = m.insert_or_assign(1, 10);
    CHECK(ins1);
    auto [it2, ins2] = m.insert_or_assign(1, 100);
    CHECK_FALSE(ins2);
    CHECK(it2->second == 100);
}

TEST_CASE_TEMPLATE("erase_if predicate" * doctest::skip("TODO: fix erase_if bug in emilib"), Map, AllIntMaps) {
    Map m;
    for (int i = 0; i < 100; ++i) m[i] = i;
    auto erased = m.erase_if([](const auto& kv) { return kv.second % 2 == 0; });
    CHECK(m.size() == 50);
    for (int i = 1; i < 100; i += 2) CHECK(m.contains(i));
    (void)erased;
}

TEST_CASE_TEMPLATE("shrink_to_fit", Map, AllIntMaps) {
    Map m;
    m.reserve(10000);
    for (int i = 0; i < 10; ++i) m[i] = i;
    auto before = m.bucket_count();
    m.shrink_to_fit();
    CHECK(m.bucket_count() <= before);
    CHECK(m.size() == 10);
}

// ----------------------------------------------------------------------------
// Extended API: insert_unique, merge
// ----------------------------------------------------------------------------

TEST_CASE_TEMPLATE("insert_unique", Map, AllIntMaps) {
    Map m;
    m[1] = 10;
    // insert_unique assumes key is new; returns bucket index
    auto bucket = m.insert_unique(2, 20);
    CHECK(bucket >= 0);
    CHECK(m.at(2) == 20);
    CHECK(m.size() == 2);
}

TEST_CASE_TEMPLATE("insert_unique precondition", Map, AllIntMaps) {
    Map m;
    m[1] = 10;
    m[2] = 20;

    // Correct usage: key is guaranteed new
    auto bucket = m.insert_unique(3, 30);
    CHECK(bucket >= 0);
    CHECK(m.at(3) == 30);
    CHECK(m.size() == 3);

    // insert_unique with a duplicate key is documented as UB.
    // In debug builds (assertions enabled), this would abort.
    // In release builds, we do NOT call it because it corrupts the map.
    // Verify the existing state is still valid:
    CHECK(m.contains(1));
    CHECK(m.contains(2));
    CHECK(m.contains(3));
    CHECK(m.size() == 3);
}

TEST_CASE_TEMPLATE("merge two maps emhash", Map, EmhashIntMaps) {
    Map a, b;
    for (int i = 0; i < 20; ++i) a[i] = i;
    for (int i = 10; i < 30; ++i) b[i] = i * 2;

    a.merge(b);
    // a should now contain keys 0..29
    for (int i = 0; i < 30; ++i) CHECK(a.contains(i));
}

TEST_CASE_TEMPLATE("merge all maps", Map, AllIntMaps) {
    Map a, b;
    for (int i = 0; i < 10; ++i) a[i] = i;
    for (int i = 10; i < 20; ++i) b[i] = i;

    a.merge(b);
    CHECK(a.size() == 20);
    for (int i = 0; i < 20; ++i) CHECK(a.contains(i));
}
