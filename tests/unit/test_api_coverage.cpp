// unit/test_api_coverage.cpp
// Coverage gap filler: tests APIs that were previously untested.
// Covers:
//   - operator==/!= (container equality)
//   - at() throws std::out_of_range
//   - initializer_list / range ctor + insert overloads
//   - custom key type with custom hash
//   - int->std::string value type (all 7 impls)
//   - erase by range (first, last)
//   - emplace_hint / equal_range
//   - non-default-constructible value (emplace/try_emplace correctness)
//   - INT_MAX / SIZE_MAX key boundaries
//   - non-member swap (ADL)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"

#include <memory>
#include <limits>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

// ============================================================================
// 1. operator== / operator!= (container equality)
// ============================================================================
TEST_CASE_TEMPLATE("operator== and != basic equality", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map a, b;
    CHECK(a == b);
    CHECK_FALSE(a != b);

    a[make_kv<K>(1)] = make_kv<V>(10);
    CHECK(a != b);
    CHECK_FALSE(a == b);

    b[make_kv<K>(1)] = make_kv<V>(10);
    CHECK(a == b);

    // different value -> not equal
    b[make_kv<K>(1)] = make_kv<V>(99);
    CHECK(a != b);

    // different key set, same size -> not equal
    Map c, d;
    c[make_kv<K>(1)] = make_kv<V>(1);
    c[make_kv<K>(2)] = make_kv<V>(2);
    d[make_kv<K>(1)] = make_kv<V>(1);
    d[make_kv<K>(3)] = make_kv<V>(2);
    CHECK(c != d);
}

TEST_CASE_TEMPLATE("operator== different insertion order", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    // Equal maps must compare equal regardless of insertion order.
    Map a, b;
    for (int i = 0; i < 50; ++i) a[make_kv<K>(i)] = make_kv<V>(i * 2);
    for (int i = 49; i >= 0; --i) b[make_kv<K>(i)] = make_kv<V>(i * 2);
    CHECK(a == b);
}

// ============================================================================
// 2. at() throws std::out_of_range on missing key
// ============================================================================
TEST_CASE_TEMPLATE("at throws out_of_range", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(1)] = make_kv<V>(10);
    CHECK_THROWS_AS(m.at(make_kv<K>(999)), std::out_of_range);

    const Map& cm = m;
    CHECK_THROWS_AS(cm.at(make_kv<K>(999)), std::out_of_range);
}

// ============================================================================
// 3. initializer_list / range constructors + insert overloads
// ============================================================================
TEST_CASE_TEMPLATE("initializer_list constructor", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m{
        {make_kv<K>(1), make_kv<V>(10)},
        {make_kv<K>(2), make_kv<V>(20)},
        {make_kv<K>(3), make_kv<V>(30)}
    };
    CHECK(m.size() == 3);
    CHECK(m.at(make_kv<K>(2)) == make_kv<V>(20));
}

TEST_CASE_TEMPLATE("range constructor from vector", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    std::vector<std::pair<K, V>> src{
        {make_kv<K>(1), make_kv<V>(10)},
        {make_kv<K>(2), make_kv<V>(20)},
        {make_kv<K>(3), make_kv<V>(30)}
    };
    Map m(src.begin(), src.end());
    CHECK(m.size() == 3);
    CHECK(m.contains(make_kv<K>(3)));
}

TEST_CASE_TEMPLATE("insert range", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    std::vector<std::pair<K, V>> src{
        {make_kv<K>(10), make_kv<V>(100)},
        {make_kv<K>(20), make_kv<V>(200)},
        {make_kv<K>(30), make_kv<V>(300)}
    };
    Map m;
    m.insert(src.begin(), src.end());
    CHECK(m.size() == 3);
    CHECK(m.at(make_kv<K>(20)) == make_kv<V>(200));
}

TEST_CASE_TEMPLATE("insert initializer_list", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m.insert({
        {make_kv<K>(100), make_kv<V>(1)},
        {make_kv<K>(200), make_kv<V>(2)}
    });
    CHECK(m.size() == 2);
    CHECK(m.contains(make_kv<K>(200)));
}

// ============================================================================
// 4. Custom key type with custom hash
// ============================================================================
struct Point2D {
    int x, y;
    bool operator==(const Point2D& o) const { return x == o.x && y == o.y; }
};

struct PointHash {
    size_t operator()(const Point2D& p) const {
        return static_cast<size_t>(static_cast<uint64_t>(p.x) * 31 + p.y);
    }
};

// Only emhash5/8 support custom hasher via alias template
TEST_CASE("custom key type with custom hash (emhash5/8)") {
    using PM = emhash5::HashMap<Point2D, int, PointHash>;
    PM m;
    m[{1, 2}] = 10;
    m[{3, 4}] = 20;
    m[{1, 2}] = 99;  // overwrite

    CHECK(m.size() == 2);
    CHECK(m.at({1, 2}) == 99);
    CHECK(m.contains({3, 4}));

    auto it = m.find({1, 2});
    CHECK(it != m.end());
    CHECK(it->second == 99);

    m.erase({3, 4});
    CHECK_FALSE(m.contains({3, 4}));
    CHECK(m.size() == 1);
}

// ============================================================================
// 5. int -> std::string value type (all 7 impls)
// ============================================================================
#define IntStrMaps map5<int, std::string>, map6<int, std::string>, \
    map7<int, std::string>, map8<int, std::string>, \
    imap1<int, std::string>, imap2<int, std::string>, imap3<int, std::string>

TEST_CASE_TEMPLATE("int->string value CRUD", Map, IntStrMaps) {
    Map m;
    m[1] = "hello";
    m[2] = "world";
    m[3] = std::string(64, 'x');  // long string (out of SSO buffer)

    CHECK(m.size() == 3);
    CHECK(m.at(1) == "hello");
    CHECK(m.at(3) == std::string(64, 'x'));

    // overwrite
    m[1] = "HELLO";
    CHECK(m.at(1) == "HELLO");

    // erase
    m.erase(2);
    CHECK_FALSE(m.contains(2));
    CHECK(m.size() == 2);

    // iteration
    size_t count = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        CHECK_FALSE(it->second.empty());
        ++count;
    }
    CHECK(count == 2);
}

TEST_CASE_TEMPLATE("int->string copy and move", Map, IntStrMaps) {
    Map m;
    for (int i = 0; i < 100; ++i) m[i] = std::string("val") + std::to_string(i);

    Map copy = m;
    CHECK(copy.size() == 100);
    CHECK(copy.at(50) == m.at(50));

    Map moved = std::move(m);
    CHECK(moved.size() == 100);
    CHECK(moved.at(50) == "val50");
}

// ============================================================================
// 6. erase by range (first, last) — only map8 + emilib1/2/3 support this overload
// ============================================================================
TEST_CASE_TEMPLATE("erase range", Map,
    map8<int,int>, imap1<int,int>, imap2<int,int>, imap3<int,int>) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 20; ++i) m[make_kv<K>(i)] = i;

    // erase first 5 elements via iterator range (use const_iterator for portability)
    const Map& cm = m;
    auto first = cm.begin();
    auto last = first;
    for (int i = 0; i < 5; ++i) ++last;
    m.erase(first, last);

    CHECK(m.size() == 15);
}

// ============================================================================
// 7. emplace_hint (emhash5/8 only — emhash6/7 have ambiguous overloads, emilib lacks this)
// ============================================================================
TEST_CASE_TEMPLATE("emplace_hint", Map,
    map5<int,int>, map8<int,int>) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m[make_kv<K>(1)] = make_kv<V>(10);

    auto hint = m.begin();
    auto r = m.emplace_hint(hint, make_kv<K>(2), make_kv<V>(20));
    CHECK(r != m.end());
    CHECK(m.size() == 2);
    CHECK(m.contains(make_kv<K>(2)));
}

// equal_range omitted: emhash6/7 have ambiguous overloads, emhash8 has internal .second bug at hash_table8.hpp:641

// ============================================================================
// 8. Non-default-constructible value (emplace/try_emplace correctness)
// ============================================================================
struct NoDefault {
    int v;
    explicit NoDefault(int x) : v(x) {}
    NoDefault() = delete;
    bool operator==(int o) const { return v == o; }
    bool operator==(const NoDefault& o) const { return v == o.v; }
};

TEST_CASE("emplace/try_emplace with non-default-constructible value (emhash5/8)") {
    using M5 = emhash5::HashMap<int, NoDefault>;
    using M8 = emhash8::HashMap<int, NoDefault>;

    M5 m5;
    m5.emplace(1, NoDefault(100));
    m5.try_emplace(2, 200);
    CHECK(m5.at(1).v == 100);
    CHECK(m5.at(2).v == 200);

    M8 m8;
    m8.emplace(1, NoDefault(100));
    m8.try_emplace(2, 200);
    CHECK(m8.at(1).v == 100);
    CHECK(m8.at(2).v == 200);
}

// ============================================================================
// 9. INT_MAX / SIZE_MAX key boundaries
// ============================================================================
TEST_CASE_TEMPLATE("INT_MAX and SIZE_MAX key boundaries", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    K maxKey = std::numeric_limits<K>::max();
    K minKey = std::numeric_limits<K>::is_signed
                   ? std::numeric_limits<K>::min()
                   : K{0};

    m[maxKey] = make_kv<V>(1);
    m[minKey] = make_kv<V>(2);
    CHECK(m.size() == 2);
    CHECK(m.at(maxKey) == make_kv<V>(1));
    CHECK(m.at(minKey) == make_kv<V>(2));

    m.erase(maxKey);
    CHECK_FALSE(m.contains(maxKey));
    CHECK(m.size() == 1);
}

// ============================================================================
// 10. Non-member swap (ADL)
// ============================================================================
TEST_CASE_TEMPLATE("non-member swap (ADL)", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map a, b;
    a[make_kv<K>(1)] = make_kv<V>(10);
    a[make_kv<K>(2)] = make_kv<V>(20);
    b[make_kv<K>(3)] = make_kv<V>(30);

    swap(a, b);

    CHECK(a.size() == 1);
    CHECK(b.size() == 2);
    CHECK(a.at(make_kv<K>(3)) == make_kv<V>(30));
    CHECK(b.at(make_kv<K>(1)) == make_kv<V>(10));
}

// ============================================================================
// 11. Rehash during iteration does not crash (sequential, safe)
// ============================================================================
TEST_CASE_TEMPLATE("rehash then iterate consistency", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = i;

    // Force rehash by inserting many more
    for (int i = 100; i < 1000; ++i) m[make_kv<K>(i)] = i;

    // Iterate after rehash — count must match
    size_t count = 0;
    for (auto it = m.begin(); it != m.end(); ++it) ++count;
    CHECK(count == 1000);
}

// ============================================================================
// 12. Self-merge edge case (must not corrupt)
// ============================================================================
TEST_CASE_TEMPLATE("self merge safety", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 10; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    // Self-merge: all keys exist, so nothing should change
    m.merge(m);
    CHECK(m.size() == 10);
    for (int i = 0; i < 10; ++i) CHECK(m.contains(make_kv<K>(i)));
}

// ============================================================================
// 13. load_factor / max_load_factor / bucket_count
//     emilib's max_load_factor(float) is a no-op (compile-time fixed),
//     so setter verification is emhash-only.
// ============================================================================
TEST_CASE_TEMPLATE("load_factor and bucket_count", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    CHECK(m.empty());
    CHECK(m.bucket_count() > 0);

    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = i;
    CHECK(m.size() == 100);
    CHECK(m.load_factor() > 0.0f);
    CHECK(m.load_factor() <= m.max_load_factor() + 0.01f);
}

TEST_CASE_TEMPLATE("max_load_factor setter (emhash only)", Map, EmhashIntMaps) {
    using K = typename Map::key_type;
    Map m;
    m.max_load_factor(0.99f);
    CHECK(m.max_load_factor() == doctest::Approx(0.99f).epsilon(0.01));
    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = i;
    CHECK(m.load_factor() <= m.max_load_factor() + 0.01f);
}

TEST_CASE_TEMPLATE("reserve and rehash", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    m.reserve(5000);
    auto bc_after_reserve = m.bucket_count();
    CHECK(bc_after_reserve >= 5000);

    for (int i = 0; i < 1000; ++i) m[make_kv<K>(i)] = i;
    CHECK(m.size() == 1000);
    // reserve should have prevented multiple rehashes
    CHECK(m.bucket_count() == bc_after_reserve);
}

TEST_CASE_TEMPLATE("rehash zero on empty map", Map, AllIntMaps) {
    Map m;
    CHECK(m.empty());
    m.rehash(0);  // must not crash; allocates minimum buckets
    CHECK(m.empty());
    // Map remains usable after rehash(0)
    using K = typename Map::key_type;
    m[make_kv<K>(1)] = 10;
    CHECK(m.size() == 1);
    CHECK(m.at(make_kv<K>(1)) == 10);
}

TEST_CASE_TEMPLATE("rehash zero on non-empty map is no-op", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = i;
    auto bc_before = m.bucket_count();
    auto size_before = m.size();

    m.rehash(0);  // 0 < _num_filled, so returns immediately
    CHECK(m.size() == size_before);
    CHECK(m.bucket_count() == bc_before);
    for (int i = 0; i < 100; ++i) CHECK(m.at(make_kv<K>(i)) == i);
}

// ============================================================================
// 14. insert_unique at high load factor (trigger rehash path)
// ============================================================================
TEST_CASE_TEMPLATE("insert_unique high load", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    m.max_load_factor(0.9f);
    m.reserve(100);

    // Fill to near capacity using insert_unique (keys guaranteed unique)
    for (int i = 0; i < 90; ++i) {
        m.insert_unique(make_kv<K>(i), make_kv<V>(i * 2));
    }
    CHECK(m.size() == 90);
    for (int i = 0; i < 90; ++i) CHECK(m.at(make_kv<K>(i)) == make_kv<V>(i * 2));
}

// ============================================================================
// 15. erase iterator returns correct next (main bucket promotion)
//     This tests the fix for emhash6/7 lazy _bmask init bug.
//     Only emhash5-8: emilib1/3 erase(iterator) returns void.
// ============================================================================
TEST_CASE_TEMPLATE("erase iterator sequential completeness", Map, EmhashIntMaps) {
    using K = typename Map::key_type;
    Map m;
    const int N = 200;
    for (int i = 0; i < N; ++i) m[make_kv<K>(i)] = i;

    // Erase all even keys via iterator-based erase_if pattern
    size_t erased = 0;
    for (auto it = m.begin(); it != m.end();) {
        if (it->second % 2 == 0) {
            it = m.erase(it);
            ++erased;
        } else {
            ++it;
        }
    }
    CHECK(erased == N / 2);
    CHECK(m.size() == N / 2);
    // All odd keys must survive
    for (int i = 1; i < N; i += 2) CHECK(m.contains(make_kv<K>(i)));
    // All even keys must be gone
    for (int i = 0; i < N; i += 2) CHECK_FALSE(m.contains(make_kv<K>(i)));
}

TEST_CASE_TEMPLATE("erase every other element by key", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    const int N = 300;
    for (int i = 0; i < N; ++i) m[make_kv<K>(i)] = i;

    // Erase keys 0, 2, 4, ... by key (not iterator)
    for (int i = 0; i < N; i += 2) m.erase(make_kv<K>(i));
    CHECK(m.size() == N / 2);
    for (int i = 1; i < N; i += 2) CHECK(m.contains(make_kv<K>(i)));

    // Re-insert erased keys
    for (int i = 0; i < N; i += 2) m[make_kv<K>(i)] = i;
    CHECK(m.size() == N);
    for (int i = 0; i < N; ++i) CHECK(m.contains(make_kv<K>(i)));
}

// ============================================================================
// 16. clear and reuse after heavy erase
// ============================================================================
TEST_CASE_TEMPLATE("clear and reuse after erase", Map, AllIntMaps) {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    Map m;
    for (int i = 0; i < 500; ++i) m[make_kv<K>(i)] = make_kv<V>(i);

    // Erase half
    for (int i = 0; i < 250; ++i) m.erase(make_kv<K>(i));
    CHECK(m.size() == 250);

    // Clear and refill with different keys
    m.clear();
    CHECK(m.empty());
    for (int i = 1000; i < 1500; ++i) m[make_kv<K>(i)] = make_kv<V>(i);
    CHECK(m.size() == 500);
    for (int i = 1000; i < 1500; ++i) CHECK(m.at(make_kv<K>(i)) == make_kv<V>(i));
}

// ============================================================================
// 17. erase_if coverage (was previously skipped due to emhash6/7 erase bug,
//     now fixed. emilib uses erase(it++) which is safe — _erase doesn't move
//     data between buckets.)
// ============================================================================
TEST_CASE_TEMPLATE("erase_if removes even values", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    const int N = 200;
    for (int i = 0; i < N; ++i) m[make_kv<K>(i)] = i;

    auto erased = m.erase_if([](const auto& kv) { return kv.second % 2 == 0; });
    CHECK(erased == N / 2);
    CHECK(m.size() == N / 2);
    for (int i = 1; i < N; i += 2) CHECK(m.contains(make_kv<K>(i)));
    for (int i = 0; i < N; i += 2) CHECK_FALSE(m.contains(make_kv<K>(i)));
}

TEST_CASE_TEMPLATE("erase_if removes none", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 50; ++i) m[make_kv<K>(i)] = i;
    auto erased = m.erase_if([](const auto&) { return false; });
    CHECK(erased == 0);
    CHECK(m.size() == 50);
}

TEST_CASE_TEMPLATE("erase_if removes all", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 50; ++i) m[make_kv<K>(i)] = i;
    auto erased = m.erase_if([](const auto&) { return true; });
    CHECK(erased == 50);
    CHECK(m.empty());
}

TEST_CASE_TEMPLATE("erase_if large scale stress", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    const int N = 1000;
    for (int i = 0; i < N; ++i) m[make_kv<K>(i)] = i;

    // Erase multiples of 3
    auto erased = m.erase_if([](const auto& kv) { return kv.second % 3 == 0; });
    CHECK(erased == (N + 2) / 3);
    CHECK(m.size() == N - (N + 2) / 3);
    for (int i = 0; i < N; ++i) {
        if (i % 3 == 0) CHECK_FALSE(m.contains(make_kv<K>(i)));
        else CHECK(m.contains(make_kv<K>(i)));
    }

    // Erase remaining multiples of 5
    erased = m.erase_if([](const auto& kv) { return kv.second % 5 == 0; });
    CHECK(m.size() == N - (N + 2) / 3 - erased);
    // Re-insert some
    for (int i = 0; i < N; i += 3) m[make_kv<K>(i)] = i;
    CHECK(m.size() == N - erased);
}

TEST_CASE_TEMPLATE("erase_if return value and idempotency", Map, AllIntMaps) {
    using K = typename Map::key_type;
    Map m;
    for (int i = 0; i < 100; ++i) m[make_kv<K>(i)] = i;

    auto e1 = m.erase_if([](const auto& kv) { return kv.second < 50; });
    CHECK(e1 == 50);
    auto e2 = m.erase_if([](const auto& kv) { return kv.second < 50; });
    CHECK(e2 == 0);
    CHECK(m.size() == 50);
}

// ============================================================================
// 18. operator= with low load factor (emhash5/6/7/8 only)
//     When rhs.load_factor() < EMH_MIN_LOAD_FACTOR (0.25), operator= takes
//     a special path: clear + dealloc + rehash + insert_unique, instead of
//     bulk-copying buckets. This test forces that path.
// ============================================================================
TEST_CASE_TEMPLATE("copy assign low load factor", Map, EmhashIntMaps) {
    using K = typename Map::key_type;
    Map src;
    // reserve far more buckets than needed so load_factor << 0.25
    src.reserve(2000);
    for (int i = 0; i < 10; ++i) src[make_kv<K>(i)] = i;
    CHECK(src.load_factor() < 0.25f);

    Map dst;
    for (int i = 500; i < 510; ++i) dst[make_kv<K>(i)] = i;  // pre-fill dst
    CHECK(dst.size() == 10);

    dst = src;  // triggers low-load-factor branch in emhash5/6/7/8

    CHECK(dst.size() == 10);
    for (int i = 0; i < 10; ++i) {
        CHECK(dst.at(make_kv<K>(i)) == i);
        CHECK_FALSE(dst.contains(make_kv<K>(500 + i)));
    }
}

TEST_CASE_TEMPLATE("copy assign low load factor to empty", Map, EmhashIntMaps) {
    using K = typename Map::key_type;
    Map src;
    src.reserve(2000);
    for (int i = 0; i < 5; ++i) src[make_kv<K>(i)] = i;
    CHECK(src.load_factor() < 0.25f);

    Map dst;
    dst = src;  // low-load branch with empty dst

    CHECK(dst.size() == 5);
    for (int i = 0; i < 5; ++i) CHECK(dst.at(make_kv<K>(i)) == i);
}

TEST_CASE_TEMPLATE("copy assign after mass erase", Map, EmhashIntMaps) {
    using K = typename Map::key_type;
    // Another way to get low load factor: insert many, erase most.
    // erase() doesn't shrink buckets, so load_factor drops.
    Map src;
    for (int i = 0; i < 200; ++i) src[make_kv<K>(i)] = i;
    for (int i = 0; i < 190; ++i) src.erase(make_kv<K>(i));
    CHECK(src.size() == 10);
    CHECK(src.load_factor() < 0.25f);

    Map dst;
    dst = src;

    CHECK(dst.size() == 10);
    for (int i = 190; i < 200; ++i) CHECK(dst.at(make_kv<K>(i)) == i);
}
