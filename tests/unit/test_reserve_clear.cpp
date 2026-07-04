// unit/test_reserve_clear.cpp
// reserve / rehash / shrink_to_fit / clear + reuse / bucket_count.
// Consolidates scenarios from:
//   verify/test_all_maps.cpp (test_reserve_rehash_clear)
//   debug/debug_all_maps.cpp (test_edge_cases clear-reuse)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

TEST_CASE_TEMPLATE("reserve expands bucket_count", Map, AllIntMaps) {
    Map m;
    m.reserve(1000);
    CHECK(m.bucket_count() >= 1000);
    CHECK(m.empty());
}

TEST_CASE_TEMPLATE("insert after reserve", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m.reserve(1000);
    for (int i = 0; i < 500; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i);
    CHECK(m.size() == 500);
    CHECK(m[make_kv<K>(499)] == make_kv<V>(499));
}

TEST_CASE_TEMPLATE("rehash preserves content", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 200; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i * 2);

    m.rehash(4000);
    CHECK(m.bucket_count() >= 4000);
    CHECK(m.size() == 200);
    CHECK(m[make_kv<K>(100)] == make_kv<V>(200));
    CHECK(m[make_kv<K>(199)] == make_kv<V>(398));
}

TEST_CASE_TEMPLATE("clear resets to empty", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 100; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i);
    m.clear();
    CHECK(m.empty());
    CHECK(m.size() == 0);
    CHECK(m.begin() == m.end());
}

TEST_CASE_TEMPLATE("clear and reuse", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 50; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i);
    m.clear();

    // reuse with different keys
    for (int i = 0; i < 30; ++i)
        m[make_kv<K>(i + 1000)] = make_kv<V>(i);
    CHECK(m.size() == 30);
    CHECK(m[make_kv<K>(1000)] == make_kv<V>(0));
    CHECK_FALSE(m.contains(make_kv<K>(0)));
}

TEST_CASE_TEMPLATE("multiple reserve clear cycles", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m.reserve(5000);
    for (int i = 0; i < 1000; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i);
    CHECK(m.size() == 1000);
    m.clear();
    CHECK(m.empty());

    m.reserve(200);
    for (int i = 0; i < 50; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i);
    CHECK(m.size() == 50);
}

TEST_CASE_TEMPLATE("shrink_to_fit reduces buckets", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m.reserve(10000);
    for (int i = 0; i < 10; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i);

    auto before = m.bucket_count();
    m.shrink_to_fit();
    auto after = m.bucket_count();
    CHECK(after <= before);
    CHECK(m.size() == 10);
    CHECK(m[make_kv<K>(0)] == make_kv<V>(0));
}

TEST_CASE_TEMPLATE("load_factor and max_load_factor", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    CHECK(m.max_load_factor() > 0.0F);
    for (int i = 0; i < 100; ++i)
        m[make_kv<K>(i)] = make_kv<V>(i);
    CHECK(m.load_factor() <= m.max_load_factor());
}
