// unit/test_string_keys.cpp
// Full string-key scenarios for ALL 7 map implementations (including emilib1/2/3).
// FILLS THE GAP: test_all_maps.cpp:740-751 previously skipped emilib string keys.
// Covers: CRUD / iteration / size_sweep / erase_patterns / reserve + rehash.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/utilities.hpp"
#include <string>
#include <vector>

TEST_CASE_TEMPLATE("string key CRUD", Map, AllStringMaps) {
    Map m;
    m["alpha"] = 1;
    m["beta"] = 2;
    m["gamma"] = 3;
    m[""] = 0;  // empty string key

    CHECK(m.size() == 4);
    CHECK(m.contains("alpha"));
    CHECK(m.at("alpha") == 1);
    CHECK(m.at("") == 0);
    CHECK(m["beta"] == 2);
    CHECK(m.count("gamma") == 1);
    CHECK(m.count("missing") == 0);

    // overwrite
    m["alpha"] = 100;
    CHECK(m.at("alpha") == 100);

    // erase
    CHECK(m.erase("beta") == 1);
    CHECK_FALSE(m.contains("beta"));
    CHECK(m.erase("beta") == 0);
}

TEST_CASE_TEMPLATE("string key iteration", Map, AllStringMaps) {
    Map m;
    for (int i = 0; i < 50; ++i) m[std::to_string(i)] = i;

    int count = 0;
    std::vector<bool> found(50, false);
    for (auto& p : m) {
        try {
            int idx = std::stoi(p.first);
            if (idx >= 0 && idx < 50) found[idx] = true;
        } catch (...) {} // NOLINT(bugprone-empty-catch)
        ++count;
    }
    CHECK(count == 50);
    for (int i = 0; i < 50; ++i) CHECK(found[i]);
}

TEST_CASE_TEMPLATE("string key size sweep", Map, AllStringMaps) {
    // Test multiple sizes to exercise rehash boundaries
    const int sizes[] = {1, 7, 63, 255, 1023, 4095}; // NOLINT(readability-identifier-naming)
    for (int N : sizes) { // NOLINT(readability-identifier-naming)
        Map m;
        for (int i = 0; i < N; ++i) m[std::to_string(i)] = i;
        CHECK(m.size() == static_cast<size_t>(N));
        // verify all keys
        for (int i = 0; i < N; ++i) {
            CHECK(m.at(std::to_string(i)) == i);
        }
        m.clear();
        CHECK(m.empty());
    }
}

TEST_CASE_TEMPLATE("string key erase patterns", Map, AllStringMaps) {
    const int N = 100;
    // Pattern 1: erase from front
    {
        Map m;
        for (int i = 0; i < N; ++i) m[std::to_string(i)] = i;
        for (int i = 0; i < N; ++i) m.erase(std::to_string(i));
        CHECK(m.empty());
    }
    // Pattern 2: erase from back
    {
        Map m;
        for (int i = 0; i < N; ++i) m[std::to_string(i)] = i;
        for (int i = N - 1; i >= 0; --i) m.erase(std::to_string(i));
        CHECK(m.empty());
    }
    // Pattern 3: erase even, then reinsert
    {
        Map m;
        for (int i = 0; i < N; ++i) m[std::to_string(i)] = i;
        for (int i = 0; i < N; i += 2) m.erase(std::to_string(i));
        CHECK(m.size() == static_cast<size_t>(N / 2));
        for (int i = 0; i < N; i += 2) m[std::to_string(i)] = i;
        CHECK(m.size() == static_cast<size_t>(N));
    }
}

TEST_CASE_TEMPLATE("string key reserve and rehash", Map, AllStringMaps) {
    Map m;
    m.reserve(1000);
    for (int i = 0; i < 500; ++i) m[std::to_string(i)] = i;
    CHECK(m.size() == 500);

    m.rehash(4000);
    CHECK(m.size() == 500);
    CHECK(m.at("250") == 250);
    CHECK(m.at("499") == 499);
}

TEST_CASE_TEMPLATE("string key copy and move", Map, AllStringMaps) {
    Map m;
    for (int i = 0; i < 50; ++i) m[std::to_string(i)] = i;

    Map m2(m);
    CHECK(m2.size() == 50);
    CHECK(m2.at("25") == 25);

    Map m3(std::move(m2));
    CHECK(m3.size() == 50);
    CHECK(m3.at("25") == 25);
}

TEST_CASE_TEMPLATE("string key merge", Map, AllStringMaps) {
    SUBCASE("overlap") {
        Map a;
        Map b;
        for (int i = 0; i < 20; ++i) a[std::to_string(i)] = i;
        for (int i = 10; i < 30; ++i) b[std::to_string(i)] = i * 2;

        a.merge(b);
        CHECK(a.size() == 30);
        for (int i = 0; i < 30; ++i) CHECK(a.contains(std::to_string(i)));
        // overlap keys 10..19 stay in b
        CHECK(b.size() == 10);
        for (int i = 10; i < 20; ++i) CHECK(b.contains(std::to_string(i)));
    }

    SUBCASE("no overlap") {
        Map c;
        Map d;
        for (int i = 0; i < 10; ++i) c[std::to_string(i)] = i;
        for (int i = 10; i < 20; ++i) d[std::to_string(i)] = i;

        c.merge(d);
        CHECK(c.size() == 20);
        CHECK(d.empty());
        for (int i = 0; i < 20; ++i) CHECK(c.contains(std::to_string(i)));
    }

    SUBCASE("large scale 500 keys") {
        Map a;
        Map b;
        const int N = 500;
        for (int i = 0; i < N; ++i) a[std::to_string(i)] = i;
        for (int i = N / 2; i < N + N / 2; ++i) b[std::to_string(i)] = i * 3;

        a.merge(b);
        CHECK(a.size() == N + N / 2);
        CHECK(b.size() == N / 2);
        for (int i = 0; i < N + N / 2; ++i) CHECK(a.contains(std::to_string(i)));
        for (int i = N / 2; i < N; ++i) {
            CHECK(b.contains(std::to_string(i)));
            CHECK(a.at(std::to_string(i)) == i);
        }
    }
}

TEST_CASE_TEMPLATE("long string keys", Map, AllStringMaps) {
    Map m;
    std::string long_key(1000, 'x');
    m[long_key] = 42;
    CHECK(m.contains(long_key));
    CHECK(m.at(long_key) == 42);

    std::string very_long(100000, 'y');
    m[very_long] = 99;
    CHECK(m.at(very_long) == 99);
}
