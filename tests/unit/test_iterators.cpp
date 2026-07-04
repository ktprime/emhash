// unit/test_iterators.cpp
// Iterator operations: begin/end, cbegin/cend, range-for, erase-then-iterate.
// Consolidates iterator scenarios from:
//   verify/test_all_maps.cpp (test_iterator)
//   debug/debug_all_maps.cpp (test_iterator_after_erase)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

TEST_CASE_TEMPLATE("begin/end and range-for count", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 50; ++i) m[make_kv<K>(i)] = make_kv<V>(i * 10);

    int count = 0;
    for (auto& p : m) { (void)p; ++count; }
    CHECK(count == 50);

    count = 0;
    for (auto it = m.begin(); it != m.end(); ++it) ++count;
    CHECK(count == 50);

    count = 0;
    for (auto it = m.cbegin(); it != m.cend(); ++it) ++count;
    CHECK(count == 50);
}

TEST_CASE_TEMPLATE("const iteration", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 10; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    const auto& cmap = m;
    int count = 0;
    for (auto& p : cmap) { (void)p; ++count; }
    CHECK(count == 10);
}

TEST_CASE_TEMPLATE("iteration visits all keys", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    const int N = 30;
    for (int i = 0; i < N; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    std::vector<bool> found(N, false);
    for (auto& p : m) {
        int idx = -1;
        if constexpr (std::is_integral_v<K>) {
            idx = static_cast<int>(p.first);
        } else {
            try { idx = std::stoi(p.first); } catch (...) {} // NOLINT(bugprone-empty-catch)
        }
        if (idx >= 0 && idx < N) found[idx] = true;
    }
    for (int i = 0; i < N; ++i) CHECK(found[i]);
}

TEST_CASE_TEMPLATE("empty map begin equals end", Map, AllIntMaps) {
    Map m;
    CHECK(m.begin() == m.end());
    CHECK(m.cbegin() == m.cend());
}

TEST_CASE_TEMPLATE("iterate after erase", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 20; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    m.erase(make_kv<K>(5));
    m.erase(make_kv<K>(10));
    m.erase(make_kv<K>(15));

    int count = 0;
    for (auto& p : m) { (void)p; ++count; }
    CHECK(count == 17);

    // erased keys not visited
    CHECK_FALSE(m.contains(make_kv<K>(5)));
    CHECK_FALSE(m.contains(make_kv<K>(10)));
    CHECK_FALSE(m.contains(make_kv<K>(15)));
}

TEST_CASE_TEMPLATE("iterate after rehash", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    m.rehash(1000);

    int count = 0;
    for (auto& p : m) { (void)p; ++count; }
    CHECK(count == 100);
    CHECK(m[make_kv<K>(50)] == make_kv<V>(50));
}
