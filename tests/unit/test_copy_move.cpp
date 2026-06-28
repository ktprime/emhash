// unit/test_copy_move.cpp
// Copy/move semantics: copy ctor, move ctor, copy assign, move assign,
// self-assignment, swap. Consolidates the 5 places this was previously
// duplicated (test_all_maps, test_string_key_leak, test_lifecycle_audit,
// debug_all_maps, test_sanitizer_aggregate).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

TEST_CASE_TEMPLATE("copy constructor deep copy", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = make_kv<V>(i * 10);

    Map m2(m);
    CHECK(m2.size() == m.size());
    CHECK(m2[make_kv<K>(50)] == make_kv<V>(500));

    // modifying copy does not affect original
    m2[make_kv<K>(0)] = make_kv<V>(-1);
    CHECK(m[make_kv<K>(0)] == make_kv<V>(0));
}

TEST_CASE_TEMPLATE("copy assignment", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 50; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    Map m2;
    m2[make_kv<K>(999)] = make_kv<V>(1);
    m2 = m;
    CHECK(m2.size() == m.size());
    CHECK(m2[make_kv<K>(10)] == make_kv<V>(10));
    CHECK_FALSE(m2.contains(make_kv<K>(999)));  // old content replaced
}

TEST_CASE_TEMPLATE("move constructor", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = make_kv<V>(i * 10);

    Map m2(std::move(m));
    CHECK(m2.size() == 100);
    CHECK(m2[make_kv<K>(25)] == make_kv<V>(250));
}

TEST_CASE_TEMPLATE("move assignment", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 80; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    Map m2;
    m2[make_kv<K>(999)] = make_kv<V>(1);
    m2 = std::move(m);
    CHECK(m2.size() == 80);
    CHECK_FALSE(m2.contains(make_kv<K>(999)));
    CHECK(m2[make_kv<K>(40)] == make_kv<V>(40));
}

TEST_CASE_TEMPLATE("self assignment", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 20; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    Map& ref = m;
    m = ref;            // copy self-assign
    CHECK(m.size() == 20);
    CHECK(m[make_kv<K>(10)] == make_kv<V>(10));

    m = std::move(ref); // move self-assign (should be safe)
    CHECK(m.size() == 20);
    CHECK(m[make_kv<K>(10)] == make_kv<V>(10));
}

TEST_CASE_TEMPLATE("swap member", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map a, b;
    for (int i = 0; i < 30; ++i) a[make_kv<K>(i)] = make_kv<V>(i);
    b[make_kv<K>(777)] = make_kv<V>(7);

    a.swap(b);
    CHECK(a.size() == 1);
    CHECK(b.size() == 30);
    CHECK(a[make_kv<K>(777)] == make_kv<V>(7));
    CHECK(b[make_kv<K>(15)] == make_kv<V>(15));
}

TEST_CASE_TEMPLATE("copy of empty map", Map, AllIntMaps) {
    Map m;
    Map m2(m);
    CHECK(m2.empty());
    Map m3 = std::move(m2);
    CHECK(m3.empty());
}

TEST_CASE_TEMPLATE("chained copy assign", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map a, b, c;
    for (int i = 0; i < 10; ++i) a[make_kv<K>(i)] = make_kv<V>(i);
    b = c = a;
    CHECK(b.size() == 10);
    CHECK(c.size() == 10);
    CHECK(b[make_kv<K>(5)] == make_kv<V>(5));
    CHECK(c[make_kv<K>(5)] == make_kv<V>(5));
}
