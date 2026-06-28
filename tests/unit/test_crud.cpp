// unit/test_crud.cpp
// Basic CRUD operations: insert / find / erase / contains / count / operator[] / at / emplace.
// Consolidates CRUD scenarios previously duplicated in:
//   verify/test_all_maps.cpp (test_basic_crud)
//   verify/test_hashmap_full_api.cpp
//   debug/debug_all_maps.cpp (test_basic)
//   verify/edge_test.cpp
// Runs against all 7 map implementations via TEST_CASE_TEMPLATE.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

// ---------------------------------------------------------------------------
// insert + find + contains + count
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("insert + find + contains + count", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    CHECK(m.empty());
    CHECK(m.size() == 0);

    m[make_kv<K>(1)] = make_kv<V>(10);
    m[make_kv<K>(2)] = make_kv<V>(20);
    m[make_kv<K>(3)] = make_kv<V>(30);
    CHECK(m.size() == 3);

    CHECK(m[make_kv<K>(1)] == make_kv<V>(10));
    CHECK(m[make_kv<K>(2)] == make_kv<V>(20));
    CHECK(m[make_kv<K>(3)] == make_kv<V>(30));

    CHECK(m.count(make_kv<K>(1)) == 1);
    CHECK(m.count(make_kv<K>(999)) == 0);
    CHECK(m.contains(make_kv<K>(1)));
    CHECK_FALSE(m.contains(make_kv<K>(999)));

    auto it = m.find(make_kv<K>(2));
    REQUIRE(it != m.end());
    CHECK(it->first == make_kv<K>(2));
    CHECK(it->second == make_kv<V>(20));
    CHECK(m.find(make_kv<K>(999)) == m.end());
}

// ---------------------------------------------------------------------------
// operator[] default-constructs and overwrites
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("operator[] default construct and overwrite", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(1)];            // default-construct value
    CHECK(m.size() == 1);
    CHECK(m[make_kv<K>(1)] == V{});

    m[make_kv<K>(1)] = make_kv<V>(42);  // overwrite
    CHECK(m[make_kv<K>(1)] == make_kv<V>(42));
}

// ---------------------------------------------------------------------------
// at() read and modify
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("at read and modify", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(1)] = make_kv<V>(10);

    CHECK(m.at(make_kv<K>(1)) == make_kv<V>(10));
    m.at(make_kv<K>(1)) = make_kv<V>(100);
    CHECK(m.at(make_kv<K>(1)) == make_kv<V>(100));
}

// ---------------------------------------------------------------------------
// insert(pair) + insert duplicate (no overwrite)
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("insert pair and duplicate", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;

    auto r1 = m.insert({make_kv<K>(1), make_kv<V>(10)});
    CHECK(r1.second);
    CHECK(r1.first->first == make_kv<K>(1));

    auto r2 = m.insert({make_kv<K>(1), make_kv<V>(9999)});
    CHECK_FALSE(r2.second);
    CHECK(m[make_kv<K>(1)] == make_kv<V>(10));  // no overwrite
}

// ---------------------------------------------------------------------------
// emplace + emplace duplicate
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("emplace and duplicate", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;

    auto r1 = m.emplace(make_kv<K>(1), make_kv<V>(10));
    CHECK(r1.second);
    auto r2 = m.emplace(make_kv<K>(1), make_kv<V>(9999));
    CHECK_FALSE(r2.second);
    CHECK(m[make_kv<K>(1)] == make_kv<V>(10));
}

// ---------------------------------------------------------------------------
// insert_or_assign
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("insert_or_assign", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;

    m[make_kv<K>(1)] = make_kv<V>(10);
    m.insert_or_assign(make_kv<K>(1), make_kv<V>(111));
    CHECK(m[make_kv<K>(1)] == make_kv<V>(111));

    m.insert_or_assign(make_kv<K>(2), make_kv<V>(22));
    CHECK(m[make_kv<K>(2)] == make_kv<V>(22));
}

// ---------------------------------------------------------------------------
// erase by key and by iterator
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("erase by key and iterator", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(1)] = make_kv<V>(10);
    m[make_kv<K>(2)] = make_kv<V>(20);
    m[make_kv<K>(3)] = make_kv<V>(30);

    CHECK(m.erase(make_kv<K>(2)) == 1);
    CHECK(m.count(make_kv<K>(2)) == 0);
    CHECK(m.erase(make_kv<K>(2)) == 0);  // erase missing

    auto it = m.find(make_kv<K>(3));
    REQUIRE(it != m.end());
    m.erase(it);
    CHECK(m.count(make_kv<K>(3)) == 0);
    CHECK(m.size() == 1);
}

// ---------------------------------------------------------------------------
// erase all then empty
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("erase all then empty", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 10; ++i) m[make_kv<K>(i)] = make_kv<V>(i * 10);
    CHECK(m.size() == 10);
    for (int i = 0; i < 10; ++i) m.erase(make_kv<K>(i));
    CHECK(m.empty());
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("clear", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 5; ++i) m[make_kv<K>(i)] = make_kv<V>(i);
    CHECK(m.size() == 5);
    m.clear();
    CHECK(m.empty());
    CHECK(m.size() == 0);
    // reuse after clear
    m[make_kv<K>(100)] = make_kv<V>(1000);
    CHECK(m.size() == 1);
    CHECK(m[make_kv<K>(100)] == make_kv<V>(1000));
}
