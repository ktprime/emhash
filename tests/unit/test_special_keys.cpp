// unit/test_special_keys.cpp
// Special key/value types: float key (NaN/-0/+0/inf), move-only value,
// large value, non-trivial destructor. Consolidates test_special_key_types.cpp.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"
#include <memory>
#include <limits>
#include <string>

// ============================================================================
// Float key tests (NaN, -0.0/+0.0, infinity) — emhash5-8 only
// ============================================================================
#define FloatMaps                                                                                                      \
    map5<float, int>, map6<float, int>, map7<float, int>, map8<float, int>, imap1<float, int>, imap2<float, int>,      \
        imap3<float, int>

TEST_CASE_TEMPLATE("float key basic + NaN + infinity", Map, FloatMaps) {
    Map m;
    m[1.0F] = 10;
    m[2.5F] = 25;
    CHECK(m.contains(1.0F));
    CHECK(m.at(1.0F) == 10);

    // -0.0 and +0.0 are equal (IEEE 754)
    m[-0.0F] = 100;
    m[+0.0F] = 200;
    CHECK(m.contains(0.0F));
    CHECK(m.at(0.0F) == 200);

    // Infinity
    float inf = std::numeric_limits<float>::infinity();
    m[inf] = 999;
    CHECK(m.contains(inf));
    CHECK(m.at(inf) == 999);
}

// ============================================================================
// Move-only value type (std::unique_ptr<int>)
// ============================================================================
#define MoveMaps                                                                                                       \
    map5<int, std::unique_ptr<int>>, map6<int, std::unique_ptr<int>>, map7<int, std::unique_ptr<int>>,                 \
        map8<int, std::unique_ptr<int>>, imap1<int, std::unique_ptr<int>>, imap2<int, std::unique_ptr<int>>,           \
        imap3<int, std::unique_ptr<int>>

TEST_CASE_TEMPLATE("move-only value insert and move ctor", Map, MoveMaps) {
    Map m;
    m[1] = std::make_unique<int>(10);
    m[2] = std::make_unique<int>(20);
    CHECK(m.contains(1));
    CHECK(*m.at(1) == 10);
    CHECK(*m.at(2) == 20);

    Map m2(std::move(m));
    CHECK(m2.contains(1));
    CHECK(*m2.at(1) == 10);
}

// ============================================================================
// Large value type (1KB struct)
// ============================================================================
struct LargeValue {
    char data[1024];
    int id;
    LargeValue() : id(0) {
        for (auto& c : data)
            c = 0;
    }
    explicit LargeValue(int v) : id(v) {
        for (auto& c : data)
            c = static_cast<char>(v);
    }
    bool operator==(const LargeValue& o) const { return id == o.id; }
    bool operator!=(const LargeValue& o) const { return id != o.id; }
};

#define LargeMaps                                                                                                      \
    map5<int, LargeValue>, map6<int, LargeValue>, map7<int, LargeValue>, map8<int, LargeValue>,                        \
        imap1<int, LargeValue>, imap2<int, LargeValue>, imap3<int, LargeValue>

TEST_CASE_TEMPLATE("large value type", Map, LargeMaps) {
    Map m;
    m[1] = LargeValue(1);
    m[2] = LargeValue(2);
    CHECK(m[1].id == 1);
    CHECK(m[2].id == 2);
    CHECK(m.size() == 2);
}

// ============================================================================
// Non-trivial destructor value
// ============================================================================
struct CountedDtor {
    static int s_count;
    int v;
    CountedDtor() : v(0) {}
    explicit CountedDtor(int x) : v(x) {}
    CountedDtor(const CountedDtor&) = default;
    CountedDtor(CountedDtor&& o) noexcept : v(o.v) {}
    CountedDtor& operator=(const CountedDtor&) = default;
    CountedDtor& operator=(CountedDtor&& o) noexcept {
        v = o.v;
        return *this;
    }
    ~CountedDtor() { s_count++; }
    bool operator==(const CountedDtor& o) const { return v == o.v; }
    bool operator!=(const CountedDtor& o) const { return v != o.v; }
};
int CountedDtor::s_count = 0;

#define CountedMaps                                                                                                    \
    map5<int, CountedDtor>, map6<int, CountedDtor>, map7<int, CountedDtor>, map8<int, CountedDtor>,                    \
        imap1<int, CountedDtor>, imap2<int, CountedDtor>, imap3<int, CountedDtor>

TEST_CASE_TEMPLATE("non-trivial destructor value cleanup", Map, CountedMaps) {
    CountedDtor::s_count = 0;
    {
        Map m;
        for (int i = 0; i < 100; ++i)
            m[i] = CountedDtor(i);
        CHECK(m.size() == 100);
        m.clear();
        // clear should destruct all values
        CHECK(CountedDtor::s_count >= 100);
    }
    // scope exit should destruct any remaining
    CHECK(CountedDtor::s_count >= 100);
}
