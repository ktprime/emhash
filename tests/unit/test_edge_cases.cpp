// unit/test_edge_cases.cpp
// Boundary conditions: empty / single element / zero key / negative key /
// duplicate insert / max load. Consolidates the 5 places edge cases were
// previously tested (test_all_maps, edge_test, test_functional_edge,
// test_extreme, debug_all_maps).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

TEST_CASE_TEMPLATE("empty map operations", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    CHECK(m.empty());
    CHECK(m.size() == 0);
    CHECK(m.find(make_kv<K>(1)) == m.end());
    CHECK(m.count(make_kv<K>(1)) == 0);
    CHECK_FALSE(m.contains(make_kv<K>(1)));
    CHECK(m.begin() == m.end());
    CHECK(m.erase(make_kv<K>(1)) == 0);
}

TEST_CASE_TEMPLATE("single element then erase", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(42)] = make_kv<V>(100);
    CHECK(m.size() == 1);
    CHECK_FALSE(m.empty());
    CHECK(m.contains(make_kv<K>(42)));
    CHECK(m[make_kv<K>(42)] == make_kv<V>(100));
    m.erase(make_kv<K>(42));
    CHECK(m.empty());
}

TEST_CASE_TEMPLATE("zero key and value", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(0)] = make_kv<V>(0);
    CHECK(m[make_kv<K>(0)] == make_kv<V>(0));
    CHECK(m.contains(make_kv<K>(0)));
}

TEST_CASE_TEMPLATE("negative numeric keys", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    if constexpr (std::is_arithmetic_v<K>) {
        Map m;
        m[make_kv<K>(-1)] = make_kv<V>(-100);
        m[make_kv<K>(-99999)] = make_kv<V>(-1);
        CHECK(m[make_kv<K>(-1)] == make_kv<V>(-100));
        CHECK(m[make_kv<K>(-99999)] == make_kv<V>(-1));
        CHECK(m.size() == 2);
    }
}

TEST_CASE_TEMPLATE("duplicate insert does not overwrite", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(1)] = make_kv<V>(10);
    auto r = m.insert({make_kv<K>(1), make_kv<V>(20)});
    CHECK_FALSE(r.second);
    CHECK(m[make_kv<K>(1)] == make_kv<V>(10));

    m[make_kv<K>(1)] = make_kv<V>(30);  // operator[] overwrites
    CHECK(m[make_kv<K>(1)] == make_kv<V>(30));
}

TEST_CASE_TEMPLATE("erase missing key returns 0", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    m[make_kv<K>(1)] = 1;
    CHECK(m.erase(make_kv<K>(999)) == 0);
    CHECK(m.erase(make_kv<K>(-1)) == 0);
    CHECK(m.size() == 1);
}

TEST_CASE_TEMPLATE("large key range", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    // insert keys spanning a wide range to test bucket distribution
    for (int i = 0; i < 100; ++i) {
        m[make_kv<K>(i * 1000)] = make_kv<V>(i);
    }
    CHECK(m.size() == 100);
    for (int i = 0; i < 100; ++i) {
        CHECK(m[make_kv<K>(i * 1000)] == make_kv<V>(i));
    }
}

TEST_CASE_TEMPLATE("reinsert after erase", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(1)] = make_kv<V>(10);
    m.erase(make_kv<K>(1));
    CHECK_FALSE(m.contains(make_kv<K>(1)));
    m[make_kv<K>(1)] = make_kv<V>(20);
    CHECK(m[make_kv<K>(1)] == make_kv<V>(20));
    CHECK(m.size() == 1);
}
