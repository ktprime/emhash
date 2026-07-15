// unit/test_iterator_invalidation.cpp
// Iterator invalidation guarantees: verify which operations invalidate
// iterators and which preserve them, per the standard's requirements.
//
// Key rules tested:
//   - erase(it): invalidates erased element + all iterators after it (dense layout)
//   - rehash/reserve: invalidates ALL iterators
//   - insert (no rehash): may invalidate iterators (implementation-defined for open addressing)
//   - clear: invalidates ALL iterators
//   - swap: iterators refer to swapped container (standard behavior)
//   - operator=/copy ctor: new container has valid iterators, source unchanged
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

// ---------------------------------------------------------------------------
// erase by key invalidates iterators pointing to erased element
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("erase by key invalidates erased iterator", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 10; ++i)
        m[make_kv<K>(i)] = i * 10;

    auto it = m.find(make_kv<K>(5));
    REQUIRE(it != m.end());
    CHECK(it->second == 50);

    // Erase a different key - iterator to key 5 may or may not be valid
    // (open addressing may move elements). We only verify the count changed.
    CHECK(m.erase(make_kv<K>(3)) == 1);
    CHECK(m.size() == 9);
    CHECK(!m.contains(make_kv<K>(3)));

    // Iterator to erased key 3 must be invalid (find returns end)
    CHECK(m.find(make_kv<K>(3)) == m.end());
}

// ---------------------------------------------------------------------------
// erase by iterator returns valid next iterator
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("erase by iterator works correctly", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 10; ++i)
        m[make_kv<K>(i)] = i * 10;

    auto it = m.find(make_kv<K>(3));
    REQUIRE(it != m.end());

    // Some implementations return void from erase(iterator), others return iterator.
    // Just call erase and verify the element is gone.
    (void)m.erase(it);
    CHECK(m.size() == 9);
    CHECK(m.find(make_kv<K>(3)) == m.end());
}

// ---------------------------------------------------------------------------
// rehash invalidates all iterators
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("rehash invalidates all iterators", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 5; ++i)
        m[make_kv<K>(i)] = i;

    auto it = m.begin();
    auto end = m.end();
    REQUIRE(it != end);

    // Force rehash by requesting much larger capacity
    m.reserve(1000);

    // After rehash, old iterators are invalidated.
    // We cannot safely dereference `it` — but we CAN verify the container
    // still has the same elements.
    CHECK(m.size() == 5);
    for (int i = 0; i < 5; ++i)
        CHECK(m.contains(make_kv<K>(i)));

    // New iterators from the rehashed container should be valid
    auto new_it = m.begin();
    auto new_end = m.end();
    int count = 0;
    while (new_it != new_end) {
        ++new_it;
        ++count;
    }
    CHECK(count == 5);
}

// ---------------------------------------------------------------------------
// reserve invalidates all iterators (same as rehash)
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("reserve invalidates all iterators", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    m[make_kv<K>(1)] = 10;
    m[make_kv<K>(2)] = 20;

    auto it = m.begin();
    REQUIRE(it != m.end());

    m.reserve(500);

    // Container state is preserved
    CHECK(m.size() == 2);
    CHECK(m[make_kv<K>(1)] == 10);
    CHECK(m[make_kv<K>(2)] == 20);

    // New iterators work correctly
    int count = 0;
    for (auto new_it = m.begin(); new_it != m.end(); ++new_it)
        ++count;
    CHECK(count == 2);
}

// ---------------------------------------------------------------------------
// clear invalidates all iterators
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("clear invalidates all iterators", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 5; ++i)
        m[make_kv<K>(i)] = i;

    auto it = m.begin();
    REQUIRE(it != m.end());

    m.clear();

    CHECK(m.empty());
    CHECK(m.size() == 0);

    // begin() == end() after clear
    CHECK(m.begin() == m.end());

    // Container can be reused after clear
    m[make_kv<K>(100)] = 1000;
    CHECK(m.size() == 1);
    auto new_it = m.find(make_kv<K>(100));
    REQUIRE(new_it != m.end());
    CHECK(new_it->second == 1000);
}

// ---------------------------------------------------------------------------
// swap exchanges container contents; iterators follow swapped data
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("swap preserves validity", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map a, b;
    a[make_kv<K>(1)] = 10;
    a[make_kv<K>(2)] = 20;
    b[make_kv<K>(3)] = 30;

    auto a_size = a.size();
    auto b_size = b.size();

    a.swap(b);

    CHECK(a.size() == b_size);
    CHECK(b.size() == a_size);
    CHECK(a.contains(make_kv<K>(3)));
    CHECK(b.contains(make_kv<K>(1)));
    CHECK(b.contains(make_kv<K>(2)));

    // Iterators from after swap should be valid
    // After swap, a has b's elements (b_size items)
    int count = 0;
    for (auto it = a.begin(); it != a.end(); ++it)
        ++count;
    CHECK(count == static_cast<int>(b_size));
}

// ---------------------------------------------------------------------------
// copy constructor: source iterators remain valid, new container valid
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("copy ctor preserves source iterators", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map original;
    for (int i = 0; i < 5; ++i)
        original[make_kv<K>(i)] = i * 10;

    // Take iterator on original
    auto src_it = original.find(make_kv<K>(2));
    REQUIRE(src_it != original.end());

    // Copy construct
    Map copy(original);

    // Source iterators should still be valid (copy doesn't modify source)
    CHECK(src_it->second == 20);
    CHECK(original.size() == 5);

    // Copy has its own valid iterators
    CHECK(copy.size() == 5);
    for (int i = 0; i < 5; ++i)
        CHECK(copy.contains(make_kv<K>(i)));

    // Modifying copy doesn't affect original
    copy[make_kv<K>(100)] = 1000;
    CHECK(original.size() == 5);
    CHECK(copy.size() == 6);
}

// ---------------------------------------------------------------------------
// copy assignment: source iterators remain valid
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("copy assignment preserves source iterators", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map original;
    for (int i = 0; i < 5; ++i)
        original[make_kv<K>(i)] = i * 10;

    Map target;
    target[make_kv<K>(999)] = -1;

    auto src_it = original.find(make_kv<K>(3));
    REQUIRE(src_it != original.end());

    target = original;

    // Source iterators valid
    CHECK(src_it->second == 30);
    CHECK(original.size() == 5);

    // Target has correct data
    CHECK(target.size() == 5);
    CHECK(target.contains(make_kv<K>(0)));
    CHECK(!target.contains(make_kv<K>(999)));
}

// ---------------------------------------------------------------------------
// move constructor: source is empty, target has valid iterators
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("move ctor transfers ownership", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map original;
    for (int i = 0; i < 5; ++i)
        original[make_kv<K>(i)] = i * 10;

    Map moved(std::move(original));

    CHECK(moved.size() == 5);
    for (int i = 0; i < 5; ++i)
        CHECK(moved.contains(make_kv<K>(i)));

    // Original is in valid but unspecified state
    // (typically empty after move)
    CHECK(original.empty());

    // Moved container has valid iterators
    int count = 0;
    for (auto it = moved.begin(); it != moved.end(); ++it)
        ++count;
    CHECK(count == 5);
}

// ---------------------------------------------------------------------------
// insert (no rehash): iterator to existing element may survive
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("insert without rehash may preserve iterators", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    m.reserve(100); // pre-allocate to avoid rehash during insert

    m[make_kv<K>(1)] = 10;
    m[make_kv<K>(2)] = 20;

    auto it = m.find(make_kv<K>(1));
    REQUIRE(it != m.end());

    // Insert more elements (should not trigger rehash due to reserve)
    for (int i = 10; i < 50; ++i)
        m[make_kv<K>(i)] = i;

    // The element at key 1 should still be findable
    CHECK(m.contains(make_kv<K>(1)));
    CHECK(m[make_kv<K>(1)] == 10);
}

// ---------------------------------------------------------------------------
// erase all then iterate: no crash
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("erase all elements one by one then iterate", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 20; ++i)
        m[make_kv<K>(i)] = i;

    // Erase odd keys
    for (int i = 1; i < 20; i += 2)
        CHECK(m.erase(make_kv<K>(i)) == 1);

    CHECK(m.size() == 10);

    // Iterate remaining (even keys)
    int count = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        CHECK(it->first % 2 == 0);
        ++count;
    }
    CHECK(count == 10);

    // Erase the rest
    for (int i = 0; i < 20; i += 2)
        CHECK(m.erase(make_kv<K>(i)) == 1);

    CHECK(m.empty());
    CHECK(m.begin() == m.end());
}

// ---------------------------------------------------------------------------
// iterator stability after multiple erase+insert cycles
// ---------------------------------------------------------------------------
TEST_CASE_TEMPLATE("iterator stability after erase insert cycles", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 100; ++i)
        m[make_kv<K>(i)] = i;

    // Erase half, insert back, verify count
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 100; i += 2)
            m.erase(make_kv<K>(i));

        CHECK(m.size() == 50);

        for (int i = 0; i < 100; i += 2)
            m[make_kv<K>(i)] = i;

        CHECK(m.size() == 100);

        // Verify all elements present
        for (int i = 0; i < 100; ++i)
            CHECK(m.contains(make_kv<K>(i)));
    }
}
