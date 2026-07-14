// Small data stress test (1-32 elements) for emhash8/emhash5/emhash6/emhash7
// Uses string keys to catch copy/move issues, tests all APIs thoroughly
// CI-optimized: fast execution, comprehensive coverage
// Each TEST_CASE creates limited local variables to avoid stack/heap pressure

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "common/doctest.h"
#include <string>
#include <vector>
#include <array>
#include <stdexcept>

#include "emhash/hash_table8.hpp"
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_set8.hpp"

// Helper to generate unique string keys
static std::string make_key(int i) {
    return "key_" + std::to_string(i);
}

static std::string make_value(int i) {
    return "val_" + std::to_string(i);
}

// Critical sizes: 1-3, 5, 7-9, 15-17, 31-32 (boundary values)
static std::array<int, 12> test_sizes() {
    return {1, 2, 3, 5, 7, 8, 9, 15, 16, 17, 31, 32};
}

// ============================================================================
// Test: Constructors and assignment
// ============================================================================
template <typename Map> void test_ctor_assignment(int n) {
    std::vector<std::string> keys;
    for (int i = 0; i < n; i++)
        keys.push_back(make_key(i));

    // Initializer_list constructor
    std::vector<std::pair<std::string, std::string>> init;
    for (int i = 0; i < n; i++)
        init.emplace_back(keys[i], make_value(i));

    Map m1(init.begin(), init.end());
    CHECK(m1.size() == static_cast<size_t>(n));

    // Copy constructor (CRITICAL: this was buggy for n=31)
    Map m2(m1);
    CHECK(m2.size() == m1.size());
    for (int i = 0; i < n; i++) {
        CHECK(m2.at(keys[i]) == make_value(i));
    }

    // Copy assignment (CRITICAL: this was buggy for n=31)
    Map m3;
    m3 = m1;
    CHECK(m3.size() == m1.size());
    for (int i = 0; i < n; i++) {
        CHECK(m3.at(keys[i]) == make_value(i));
    }

    // Move constructor
    Map m4(std::move(m1));
    CHECK(m4.size() == static_cast<size_t>(n));

    // Move assignment
    Map m5;
    m5 = std::move(m4);
    CHECK(m5.size() == static_cast<size_t>(n));

    // Self-assignment
    m5 = m5;
    CHECK(m5.size() == static_cast<size_t>(n));
}

TEST_CASE("emhash8 ctor/assignment small data") {
    for (int n : test_sizes())
        test_ctor_assignment<emhash8::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash5 ctor/assignment small data") {
    for (int n : test_sizes())
        test_ctor_assignment<emhash5::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash6 ctor/assignment small data") {
    for (int n : test_sizes())
        test_ctor_assignment<emhash6::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash7 ctor/assignment small data") {
    for (int n : test_sizes())
        test_ctor_assignment<emhash7::HashMap<std::string, std::string>>(n);
}

// ============================================================================
// Test: Insert, emplace, operator[]
// ============================================================================
template <typename Map> void test_insert(int n) {
    std::vector<std::string> keys;
    for (int i = 0; i < n; i++)
        keys.push_back(make_key(i));

    // operator[]
    Map m1;
    for (int i = 0; i < n; i++)
        m1[keys[i]] = make_value(i);
    CHECK(m1.size() == static_cast<size_t>(n));
    for (int i = 0; i < n; i++)
        CHECK(m1[keys[i]] == make_value(i));

    // insert
    Map m2;
    for (int i = 0; i < n; i++) {
        auto [it, inserted] = m2.insert({keys[i], make_value(i)});
        CHECK(inserted);
    }
    // insert duplicate
    for (int i = 0; i < n; i++) {
        auto [it, inserted] = m2.insert({keys[i], "dup"});
        CHECK(!inserted);
        CHECK(it->second == make_value(i));
    }

    // emplace
    Map m3;
    for (int i = 0; i < n; i++) {
        auto [it, inserted] = m3.emplace(keys[i], make_value(i));
        CHECK(inserted);
    }

    // try_emplace
    Map m4;
    for (int i = 0; i < n; i++)
        m4.try_emplace(keys[i], make_value(i));
    auto [it2, ins2] = m4.try_emplace(keys[0], "new");
    CHECK(!ins2);
    CHECK(m4[keys[0]] == make_value(0));

    // insert_or_assign
    Map m5;
    for (int i = 0; i < n; i++)
        m5.insert_or_assign(keys[i], make_value(i));
    m5.insert_or_assign(keys[0], "changed");
    CHECK(m5[keys[0]] == "changed");
}

TEST_CASE("emhash8 insert/emplace small data") {
    for (int n : test_sizes())
        test_insert<emhash8::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash5 insert/emplace small data") {
    for (int n : test_sizes())
        test_insert<emhash5::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash6 insert/emplace small data") {
    for (int n : test_sizes())
        test_insert<emhash6::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash7 insert/emplace small data") {
    for (int n : test_sizes())
        test_insert<emhash7::HashMap<std::string, std::string>>(n);
}

// ============================================================================
// Test: Find, contains, count, at
// ============================================================================
template <typename Map> void test_lookup(int n) {
    std::vector<std::string> keys;
    for (int i = 0; i < n; i++)
        keys.push_back(make_key(i));

    Map m;
    for (int i = 0; i < n; i++)
        m[keys[i]] = make_value(i);

    // find
    for (int i = 0; i < n; i++) {
        auto it = m.find(keys[i]);
        CHECK(it != m.end());
        CHECK(it->first == keys[i]);
    }
    CHECK(m.find("nonexistent") == m.end());

    // contains
    for (int i = 0; i < n; i++)
        CHECK(m.contains(keys[i]));
    CHECK(!m.contains("nonexistent"));

    // count
    for (int i = 0; i < n; i++)
        CHECK(m.count(keys[i]) == 1);
    CHECK(m.count("nonexistent") == 0);

    // at
    for (int i = 0; i < n; i++)
        CHECK(m.at(keys[i]) == make_value(i));
    CHECK_THROWS_AS(m.at("nonexistent"), std::out_of_range);
}

TEST_CASE("emhash8 lookup small data") {
    for (int n : test_sizes())
        test_lookup<emhash8::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash5 lookup small data") {
    for (int n : test_sizes())
        test_lookup<emhash5::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash6 lookup small data") {
    for (int n : test_sizes())
        test_lookup<emhash6::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash7 lookup small data") {
    for (int n : test_sizes())
        test_lookup<emhash7::HashMap<std::string, std::string>>(n);
}

// ============================================================================
// Test: Erase, clear, swap
// ============================================================================
template <typename Map> void test_erase(int n) {
    std::vector<std::string> keys;
    for (int i = 0; i < n; i++)
        keys.push_back(make_key(i));

    // Erase by key
    Map m1;
    for (int i = 0; i < n; i++)
        m1[keys[i]] = make_value(i);
    for (int i = 0; i < n; i++) {
        CHECK(m1.erase(keys[i]) == 1);
    }
    CHECK(m1.empty());
    CHECK(m1.erase("nonexistent") == 0);

    // Erase by iterator
    Map m2;
    for (int i = 0; i < n; i++)
        m2[keys[i]] = make_value(i);
    int erased = 0;
    auto it = m2.begin();
    while (it != m2.end()) {
        it = m2.erase(it);
        erased++;
    }
    CHECK(erased == n);
    CHECK(m2.empty());

    // clear
    Map m3;
    for (int i = 0; i < n; i++)
        m3[keys[i]] = make_value(i);
    m3.clear();
    CHECK(m3.empty());

    // swap
    Map m4;
    Map m5;
    for (int i = 0; i < n; i++)
        m5[keys[i]] = make_value(i);
    m4.swap(m5);
    CHECK(m4.size() == static_cast<size_t>(n));
    CHECK(m5.empty());
}

TEST_CASE("emhash8 erase/clear/swap small data") {
    for (int n : test_sizes())
        test_erase<emhash8::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash5 erase/clear/swap small data") {
    for (int n : test_sizes())
        test_erase<emhash5::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash6 erase/clear/swap small data") {
    for (int n : test_sizes())
        test_erase<emhash6::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash7 erase/clear/swap small data") {
    for (int n : test_sizes())
        test_erase<emhash7::HashMap<std::string, std::string>>(n);
}

// ============================================================================
// Test: Iterator, reserve, load_factor, equality
// ============================================================================
template <typename Map> void test_misc(int n) {
    std::vector<std::string> keys;
    for (int i = 0; i < n; i++)
        keys.push_back(make_key(i));

    Map m;
    for (int i = 0; i < n; i++)
        m[keys[i]] = make_value(i);

    // Iterator traversal
    int count = 0;
    for (auto it = m.begin(); it != m.end(); ++it)
        count++;
    CHECK(count == n);

    // reserve
    Map m2;
    m2.reserve(100);
    for (int i = 0; i < n; i++)
        m2[keys[i]] = make_value(i);
    CHECK(m2.size() == static_cast<size_t>(n));

    // load_factor
    float lf = m.load_factor();
    CHECK(lf >= 0.0f);
    CHECK(lf <= 1.0f);

    // bucket_count
    CHECK(m.bucket_count() >= static_cast<size_t>(n));

    // operator== and !=
    Map m3 = m;
    CHECK(m == m3);
    CHECK(!(m != m3));
    m3[keys[0]] = "different";
    CHECK(m != m3);

    // Multiple copies in sequence (regression test for 31-element bug)
    Map m4 = m;
    Map m5 = m4;
    Map m6 = m5;
    CHECK(m6.size() == static_cast<size_t>(n));
}

TEST_CASE("emhash8 iterator/reserve/equality small data") {
    for (int n : test_sizes())
        test_misc<emhash8::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash5 iterator/reserve/equality small data") {
    for (int n : test_sizes())
        test_misc<emhash5::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash6 iterator/reserve/equality small data") {
    for (int n : test_sizes())
        test_misc<emhash6::HashMap<std::string, std::string>>(n);
}
TEST_CASE("emhash7 iterator/reserve/equality small data") {
    for (int n : test_sizes())
        test_misc<emhash7::HashMap<std::string, std::string>>(n);
}

// ============================================================================
// Test: Large value type (132 bytes) - regression for original bug
// ============================================================================
struct LargeValue {
    int a;
    double b;
    std::string c;
    unsigned char padding[128];

    LargeValue() : a(0), b(0.0), c() {}
    LargeValue(int i) : a(i), b(i * 1.5), c(std::to_string(i)) {}
    bool operator==(const LargeValue& o) const { return a == o.a && b == o.b && c == o.c; }
};

TEST_CASE("emhash8 LargeValue (132B) copy/move regression") {
    for (int n : test_sizes()) {
        emhash8::HashMap<std::string, LargeValue> m1;
        for (int i = 0; i < n; i++)
            m1[make_key(i)] = LargeValue(i);

        // Copy constructor (the bug case!)
        emhash8::HashMap<std::string, LargeValue> m2(m1);
        CHECK(m2.size() == m1.size());
        for (int i = 0; i < n; i++)
            CHECK(m2.at(make_key(i)).a == i);

        // Copy assignment (the bug case!)
        emhash8::HashMap<std::string, LargeValue> m3;
        m3 = m1;
        CHECK(m3.size() == m1.size());
        for (int i = 0; i < n; i++)
            CHECK(m3.at(make_key(i)).a == i);
    }
}

// ============================================================================
// Test: Int key -> Int value (edge case for capacity calculation)
// ============================================================================
TEST_CASE("emhash8 int->int small data") {
    for (int n : test_sizes()) {
        emhash8::HashMap<int, int> m1;
        for (int i = 0; i < n; i++)
            m1[i] = i * 10;

        emhash8::HashMap<int, int> m2(m1);
        for (int i = 0; i < n; i++)
            CHECK(m2.at(i) == i * 10);

        emhash8::HashMap<int, int> m3;
        m3 = m1;
        for (int i = 0; i < n; i++)
            CHECK(m3.at(i) == i * 10);
    }
}

// ============================================================================
// Test: emhash8::HashSet
// ============================================================================
TEST_CASE("emhash8::HashSet small data") {
    for (int n : test_sizes()) {
        emhash8::HashSet<std::string> s1;
        for (int i = 0; i < n; i++)
            CHECK(s1.insert(make_key(i)).second);
        CHECK(s1.size() == static_cast<size_t>(n));

        emhash8::HashSet<std::string> s2(s1);
        CHECK(s2.size() == s1.size());
        for (int i = 0; i < n; i++)
            CHECK(s2.contains(make_key(i)));

        emhash8::HashSet<std::string> s3;
        s3 = s1;
        CHECK(s3.size() == s1.size());
        for (int i = 0; i < n; i++)
            CHECK(s3.contains(make_key(i)));

        for (int i = 0; i < n; i++)
            CHECK(s3.erase(make_key(i)) == 1);
        CHECK(s3.empty());
    }
}

// ============================================================================
// Test: Exact regression for user-reported bug (size 31, unsigned int key, LargeValue)
// ============================================================================
TEST_CASE("emhash8 size 31 unsigned int key LargeValue regression") {
    using Map = emhash8::HashMap<unsigned int, LargeValue>;
    Map test;
    test = {{1, {1}},   {2, {2}},   {3, {3}},   {4, {4}},   {5, {5}},   {6, {6}},   {7, {7}},   {8, {8}},
            {9, {9}},   {10, {10}}, {11, {11}}, {12, {12}}, {13, {13}}, {14, {14}}, {15, {15}}, {16, {16}},
            {17, {17}}, {18, {18}}, {19, {19}}, {20, {20}}, {21, {21}}, {22, {22}}, {23, {23}}, {24, {24}},
            {25, {25}}, {26, {26}}, {27, {27}}, {28, {28}}, {29, {29}}, {30, {30}}, {31, {31}}};
    CHECK(test.size() == 31);

    Map test2;
    test2 = test; // The crash case!
    CHECK(test2.size() == 31);
    for (int i = 1; i <= 31; i++)
        CHECK(test2.at(i).a == i);
}