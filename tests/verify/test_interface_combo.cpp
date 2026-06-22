// Comprehensive interface test for emilib2ss, emilib2o, emilib2s
// Covers various key/value type combinations and most public APIs

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <functional>
#include <algorithm>
#include <vector>
#include <cassert>
#include <type_traits>

#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

static int g_pass = 0, g_fail = 0;

#define TEST_ASSERT(expr, msg) do { \
    if (!(expr)) { \
        printf("    FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; return false; \
    } else { g_pass++; } \
} while(0)

#define RUN_TEST(fn) do { \
    if (fn()) printf("  [PASS] %s\n", #fn); \
    else      printf("  [FAIL] %s\n", #fn); \
} while(0)

// Helper: convert int to key/value
template<typename T>
T make_kv(int v) { return T(v); }
template<>
std::string make_kv<std::string>(int v) { return std::to_string(v); }

// ============================================================================
// Numeric key tests (int, int64_t)
// ============================================================================
template<typename MapType>
bool test_basic_crud()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    auto k1 = make_kv<Key>(1), k2 = make_kv<Key>(2), k3 = make_kv<Key>(3);
    auto v1 = make_kv<Val>(10), v2 = make_kv<Val>(20), v3 = make_kv<Val>(30);
    map[k1] = v1; map[k2] = v2; map[k3] = v3;

    TEST_ASSERT(map.size() == 3, "size after 3 inserts");
    TEST_ASSERT(map[k1] == v1, "operator[] read 1");
    TEST_ASSERT(map[k2] == v2, "operator[] read 2");
    TEST_ASSERT(map.count(k1) == 1, "count existing");
    TEST_ASSERT(map.count(make_kv<Key>(999)) == 0, "count non-existing");

    // at()
    TEST_ASSERT(map.at(k1) == v1, "at() read");
    map.at(k1) = make_kv<Val>(100);
    TEST_ASSERT(map.at(k1) == make_kv<Val>(100), "at() modify");

    // find
    auto it = map.find(k1);
    TEST_ASSERT(it != map.end(), "find existing");
    TEST_ASSERT(it->first == k1, "find key");
    TEST_ASSERT(map.find(make_kv<Key>(999)) == map.end(), "find non-existing");

    // contains
    TEST_ASSERT(map.contains(k1), "contains existing");
    TEST_ASSERT(!map.contains(make_kv<Key>(999)), "contains non-existing");

    // insert (pair)
    auto k4 = make_kv<Key>(4); auto v4 = make_kv<Val>(40);
    auto res = map.insert({k4, v4});
    TEST_ASSERT(res.second, "insert new key");
    TEST_ASSERT(res.first->first == k4, "insert iterator key");
    auto res2 = map.insert({k4, make_kv<Val>(9999)});
    TEST_ASSERT(!res2.second, "insert duplicate");
    TEST_ASSERT(map[k4] == v4, "insert duplicate no overwrite");

    // emplace
    auto k5 = make_kv<Key>(5); auto v5 = make_kv<Val>(50);
    auto res3 = map.emplace(k5, v5);
    TEST_ASSERT(res3.second, "emplace new");
    TEST_ASSERT(map[k5] == v5, "emplace value");

    // insert_or_assign
    (void)map.insert_or_assign(k1, make_kv<Val>(111));
    TEST_ASSERT(map[k1] == make_kv<Val>(111), "insert_or_assign overwrite");
    auto k6 = make_kv<Key>(6);
    (void)map.insert_or_assign(k6, make_kv<Val>(60));
    TEST_ASSERT(map[k6] == make_kv<Val>(60), "insert_or_assign new key");

    // erase by key
    size_t erased = map.erase(k2);
    TEST_ASSERT(erased == 1, "erase existing returns 1");
    TEST_ASSERT(map.count(k2) == 0, "erased key not found");
    erased = map.erase(k2);
    TEST_ASSERT(erased == 0, "erase non-existing returns 0");

    // erase by iterator
    it = map.find(k3);
    if (it != map.end()) {
        (void)map.erase(it);
        TEST_ASSERT(map.count(k3) == 0, "erase by iterator");
    }

    TEST_ASSERT(!map.empty(), "not empty");
    TEST_ASSERT(map.size() == 4, "size after erases"); // k1,k4,k5,k6

    return true;
}

template<typename MapType>
bool test_copy_move()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;
    for (int i = 0; i < 50; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 10);

    MapType map2(map);
    TEST_ASSERT(map2.size() == map.size(), "copy ctor size");
    TEST_ASSERT(map2[make_kv<Key>(25)] == make_kv<Val>(250), "copy ctor value");

    MapType map3; map3 = map;
    TEST_ASSERT(map3.size() == map.size(), "copy assign size");
    TEST_ASSERT(map3[make_kv<Key>(10)] == make_kv<Val>(100), "copy assign value");

    MapType map4(std::move(map2));
    TEST_ASSERT(map4.size() == 50, "move ctor size");
    TEST_ASSERT(map4[make_kv<Key>(25)] == make_kv<Val>(250), "move ctor value");

    MapType map5;
    map5[make_kv<Key>(999)] = make_kv<Val>(1);
    map5 = std::move(map3);
    TEST_ASSERT(map5.size() == 50, "move assign size");
    TEST_ASSERT(map5.count(make_kv<Key>(999)) == 0, "move assign replaces");

    MapType map6; map6[make_kv<Key>(777)] = make_kv<Val>(7);
    map5.swap(map6);
    TEST_ASSERT(map5.size() == 1, "swap A size");
    TEST_ASSERT(map6.size() == 50, "swap B size");

    return true;
}

template<typename MapType>
bool test_iterator()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;
    for (int i = 0; i < 30; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 10);

    int count = 0;
    for (auto& p : map) { count++; (void)p; }
    TEST_ASSERT(count == 30, "range-for count");

    const auto& cmap = map;
    count = 0;
    for (auto& p : cmap) { count++; (void)p; }
    TEST_ASSERT(count == 30, "const iteration count");

    count = 0;
    for (auto it = map.cbegin(); it != map.cend(); ++it) count++;
    TEST_ASSERT(count == 30, "cbegin/cend count");

    return true;
}

template<typename MapType>
bool test_reserve_rehash_clear()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    (void)map.reserve(1000);
    TEST_ASSERT(map.bucket_count() >= 1000, "reserve capacity");

    for (int i = 0; i < 500; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i);
    TEST_ASSERT(map.size() == 500, "insert after reserve");

    map.rehash(2000);
    TEST_ASSERT(map.bucket_count() >= 2000, "rehash capacity");
    TEST_ASSERT(map.size() == 500, "size after rehash");
    TEST_ASSERT(map[make_kv<Key>(100)] == make_kv<Val>(100), "value after rehash");

    map.clear();
    TEST_ASSERT(map.size() == 0, "clear size");
    TEST_ASSERT(map.empty(), "clear empty");

    for (int i = 0; i < 50; i++)
        map[make_kv<Key>(i + 1000)] = make_kv<Val>(i);
    TEST_ASSERT(map.size() == 50, "reuse size");

    return true;
}

template<typename MapType>
bool test_erase_scenarios()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    for (int i = 0; i < 100; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 10);

    for (int i = 0; i < 100; i += 2)
        (void)map.erase(make_kv<Key>(i));
    TEST_ASSERT(map.size() == 50, "erase evens size");
    for (int i = 0; i < 100; i++) {
        if (i % 2 == 0)
            TEST_ASSERT(map.count(make_kv<Key>(i)) == 0, "even erased");
        else
            TEST_ASSERT(map.count(make_kv<Key>(i)) == 1, "odd remains");
    }

    for (int i = 0; i < 100; i += 2)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 20);
    TEST_ASSERT(map.size() == 100, "reinsert size");

    size_t e = map.erase(make_kv<Key>(999));
    TEST_ASSERT(e == 0, "erase non-existent");
    TEST_ASSERT(map.size() == 100, "size unchanged");

    return true;
}

template<typename MapType>
bool test_large_scale()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;
    const int N = 30000;

    for (int i = 0; i < N; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 2);
    TEST_ASSERT(map.size() == N, "large insert size");

    for (int i = 0; i < N; i++) {
        auto it = map.find(make_kv<Key>(i));
        TEST_ASSERT(it != map.end() && it->second == make_kv<Val>(i * 2), "large find");
    }

    for (int i = 0; i < N / 2; i++)
        (void)map.erase(make_kv<Key>(i));
    TEST_ASSERT(map.size() == N / 2, "large erase size");

    for (int i = 0; i < N / 2; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 3);
    TEST_ASSERT(map.size() == N, "large reinsert size");

    return true;
}

template<typename MapType>
bool test_bucket_and_load()
{
    MapType map;
    for (int i = 0; i < 1000; i++)
        map[make_kv<typename MapType::key_type>(i)] = make_kv<typename MapType::mapped_type>(i);

    TEST_ASSERT(map.bucket_count() > 0, "bucket_count > 0");
    TEST_ASSERT(map.load_factor() > 0.0, "load_factor > 0");
    TEST_ASSERT(map.max_size() > 0, "max_size > 0");

    return true;
}

template<typename MapType>
bool test_single_and_empty()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;

    MapType map;
    TEST_ASSERT(map.empty(), "empty map");
    TEST_ASSERT(map.size() == 0, "empty size");
    TEST_ASSERT(map.begin() == map.end(), "empty begin==end");
    TEST_ASSERT(map.find(make_kv<Key>(1)) == map.end(), "empty find");

    auto k = make_kv<Key>(42); auto v = make_kv<Val>(100);
    map[k] = v;
    TEST_ASSERT(map.size() == 1, "single size");
    TEST_ASSERT(!map.empty(), "single not empty");
    TEST_ASSERT(map.contains(k), "single contains");
    TEST_ASSERT(map.count(k) == 1, "single count");
    TEST_ASSERT(map[k] == v, "single value");
    TEST_ASSERT(map.at(k) == v, "single at");

    (void)map.erase(k);
    TEST_ASSERT(map.empty(), "empty after erase single");
    TEST_ASSERT(map.size() == 0, "zero size after erase single");

    return true;
}

template<typename MapType>
bool test_duplicate_insert()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    auto k = make_kv<Key>(1);
    map[k] = make_kv<Val>(10);
    auto r1 = map.insert({k, make_kv<Val>(20)});
    TEST_ASSERT(!r1.second, "insert duplicate returns false");
    TEST_ASSERT(map[k] == make_kv<Val>(10), "insert duplicate no overwrite");

    map[k] = make_kv<Val>(30);
    TEST_ASSERT(map[k] == make_kv<Val>(30), "operator[] overwrite");

    auto r2 = map.emplace(k, make_kv<Val>(40));
    TEST_ASSERT(!r2.second, "emplace duplicate");
    TEST_ASSERT(map[k] == make_kv<Val>(30), "emplace duplicate no overwrite");

    return true;
}

// ============================================================================
// String-specific tests
// ============================================================================
template<typename MapType>
bool test_string_key()
{
    MapType map;
    map["hello"] = 1;
    map["world"] = 2;
    map[""] = 5;
    map["a"] = 10;
    map["ab"] = 11;
    map["abc"] = 12;

    TEST_ASSERT(map.size() == 6, "string size");
    TEST_ASSERT(map["hello"] == 1, "string lookup");
    TEST_ASSERT(map[""] == 5, "empty string key");
    TEST_ASSERT(map["abc"] == 12, "longer key");

    auto it = map.find("world");
    TEST_ASSERT(it != map.end() && it->second == 2, "find string");

    (void)map.erase("a");
    TEST_ASSERT(map.count("a") == 0, "erased string");
    TEST_ASSERT(map.count("ab") == 1, "neighbor preserved");

    std::string key = "emplace_key";
    (void)map.emplace(key, 100);
    TEST_ASSERT(map[key] == 100, "emplace std::string");

    (void)map.insert_or_assign("hello", 999);
    TEST_ASSERT(map["hello"] == 999, "insert_or_assign string");

    // Copy/move
    MapType map2(map);
    TEST_ASSERT(map2.size() == map.size(), "string copy size");
    TEST_ASSERT(map2["world"] == 2, "string copy value");

    MapType map3(std::move(map2));
    TEST_ASSERT(map3.size() == map.size(), "string move size");

    // Iteration
    int count = 0;
    for (auto& p : map3) { (void)p; count++; }
    TEST_ASSERT(count == (int)map3.size(), "string iteration");

    // Reserve/rehash/clear
    (void)map3.reserve(10000);
    TEST_ASSERT(map3.bucket_count() >= 10000, "string reserve");
    map3.rehash(20000);
    TEST_ASSERT(map3["hello"] == 999, "string value after rehash");
    map3.clear();
    TEST_ASSERT(map3.empty(), "string clear");

    return true;
}

template<typename MapType>
bool test_string_val()
{
    MapType map;
    map[1] = "one";
    map[2] = "two";
    map[0] = "zero";
    map[-1] = "neg";

    TEST_ASSERT(map.size() == 4, "string val size");
    TEST_ASSERT(map[1] == "one", "string val lookup");
    TEST_ASSERT(map[-1] == "neg", "negative key string val");

    map[1] = "ONE";
    TEST_ASSERT(map[1] == "ONE", "overwrite string val");

    (void)map.erase(2);
    TEST_ASSERT(map.count(2) == 0, "erased string val key");
    map[2] = "TWO";
    TEST_ASSERT(map[2] == "TWO", "reinsert string val");

    // Copy/move
    MapType map2(map);
    TEST_ASSERT(map2[1] == "ONE", "string val copy");
    MapType map3(std::move(map2));
    TEST_ASSERT(map3[-1] == "neg", "string val move");

    return true;
}

template<typename MapType>
bool test_string_string()
{
    MapType map;
    map["key1"] = "val1";
    map["key2"] = "val2";
    map["a"] = "A";
    map["ab"] = "AB";

    TEST_ASSERT(map.size() == 4, "ss size");
    TEST_ASSERT(map["key1"] == "val1", "ss lookup");
    TEST_ASSERT(map["ab"] == "AB", "ss longer");

    (void)map.erase("a");
    TEST_ASSERT(map.count("a") == 0, "ss erased");
    TEST_ASSERT(map.count("ab") == 1, "ss neighbor");

    map.clear();
    TEST_ASSERT(map.empty(), "ss clear");
    map["new"] = "NEW";
    TEST_ASSERT(map.size() == 1, "ss reuse");

    // Large scale string
    MapType map2;
    for (int i = 0; i < 5000; i++)
        map2[std::to_string(i)] = std::to_string(i * 10);
    TEST_ASSERT(map2.size() == 5000, "ss large size");
    TEST_ASSERT(map2["2500"] == "25000", "ss large lookup");

    return true;
}

// ============================================================================
// Edge case: extreme numeric ranges
// ============================================================================
template<typename MapType>
bool test_numeric_edge_ranges()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    // Zero
    map[make_kv<Key>(0)] = make_kv<Val>(0);
    TEST_ASSERT(map[make_kv<Key>(0)] == make_kv<Val>(0), "zero key/val");

    // Negative
    map[make_kv<Key>(-1)] = make_kv<Val>(-1);
    map[make_kv<Key>(-100)] = make_kv<Val>(-1000);
    TEST_ASSERT(map[make_kv<Key>(-1)] == make_kv<Val>(-1), "negative key -1");
    TEST_ASSERT(map[make_kv<Key>(-100)] == make_kv<Val>(-1000), "negative key -100");

    // Large positive
    map[make_kv<Key>(100000)] = make_kv<Val>(999999);
    TEST_ASSERT(map[make_kv<Key>(100000)] == make_kv<Val>(999999), "large key");

    // INT_MIN / INT_MAX range
    map[make_kv<Key>(2147483647)] = make_kv<Val>(1);
    TEST_ASSERT(map[make_kv<Key>(2147483647)] == make_kv<Val>(1), "INT_MAX key");
    map[make_kv<Key>(-2147483647 - 1)] = make_kv<Val>(2);
    TEST_ASSERT(map[make_kv<Key>(-2147483647 - 1)] == make_kv<Val>(2), "INT_MIN key");

    // Sparse keys
    map[make_kv<Key>(7)] = make_kv<Val>(70);
    map[make_kv<Key>(70000)] = make_kv<Val>(700000);
    map[make_kv<Key>(700000)] = make_kv<Val>(7000000);
    TEST_ASSERT(map[make_kv<Key>(7)] == make_kv<Val>(70), "sparse small");
    TEST_ASSERT(map[make_kv<Key>(700000)] == make_kv<Val>(7000000), "sparse large");

    // Powers of 2 (common hash table sizes)
    for (int i = 0; i < 20; i++) {
        Key k = make_kv<Key>(1 << i);
        map[k] = make_kv<Val>(i);
    }
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT(map[make_kv<Key>(1 << i)] == make_kv<Val>(i), "power-of-2 key");
    }

    // Sequential dense range
    for (int i = 0; i < 1000; i++)
        map[make_kv<Key>(i + 1000000)] = make_kv<Val>(i);
    for (int i = 0; i < 1000; i++)
        TEST_ASSERT(map[make_kv<Key>(i + 1000000)] == make_kv<Val>(i), "dense sequential");

    return true;
}

// ============================================================================
// Edge case: string key ranges
// ============================================================================
template<typename MapType>
bool test_string_edge_ranges()
{
    MapType map;

    // Empty string
    map[""] = 0;
    TEST_ASSERT(map[""] == 0, "empty string key");

    // Single char
    for (char c = 'a'; c <= 'z'; c++) {
        std::string s(1, c);
        map[s] = (int)c;
    }
    TEST_ASSERT(map.size() >= 26, "single char keys");
    TEST_ASSERT(map["m"] == (int)'m', "single char lookup");

    // Very long string
    std::string long_key(10000, 'x');
    map[long_key] = 99999;
    TEST_ASSERT(map[long_key] == 99999, "long string key");

    // Strings with special chars
    map["key with spaces"] = 1;
    map["key\twith\ttabs"] = 2;
    map["key\nwith\nnewlines"] = 3;
    map["key\"with\"quotes"] = 4;
    TEST_ASSERT(map["key with spaces"] == 1, "space key");
    TEST_ASSERT(map["key\twith\ttabs"] == 2, "tab key");

    // Numeric strings
    for (int i = 0; i < 100; i++) {
        map[std::to_string(i)] = i * 10;
    }
    TEST_ASSERT(map["50"] == 500, "numeric string key");

    // Prefix collision test
    map["pre"] = 1;
    map["pref"] = 2;
    map["prefi"] = 3;
    map["prefix"] = 4;
    TEST_ASSERT(map["pre"] == 1, "prefix short");
    TEST_ASSERT(map["prefix"] == 4, "prefix long");

    // Same-length different strings
    map["abcd"] = 10;
    map["abce"] = 11;
    map["abcf"] = 12;
    TEST_ASSERT(map["abcd"] == 10, "same-len diff 1");
    TEST_ASSERT(map["abcf"] == 12, "same-len diff 3");

    return true;
}

// ============================================================================
// Edge case: string value ranges
// ============================================================================
template<typename MapType>
bool test_string_val_edge_ranges()
{
    MapType map;

    // Empty string value
    map[1] = "";
    TEST_ASSERT(map[1] == "", "empty string val");

    // Long string value
    std::string long_val(5000, 'y');
    map[2] = long_val;
    TEST_ASSERT(map[2] == long_val, "long string val");

    // Overwrite with different length
    map[3] = "short";
    map[3] = "a much longer string value than before";
    TEST_ASSERT(map[3] == "a much longer string value than before", "overwrite diff length");

    // Special chars in value
    map[4] = "val\twith\ttabs";
    map[5] = "val\nwith\nnewlines";
    TEST_ASSERT(map[4] == "val\twith\ttabs", "tab val");
    TEST_ASSERT(map[5] == "val\nwith\nnewlines", "newline val");

    return true;
}

// ============================================================================
// Dense collision test: many keys with same hash
// ============================================================================
struct SameHashInt {
    size_t operator()(int) const { return 42; }
};

template<typename MapType>
bool test_dense_collision()
{
    MapType map;
    const int N = 500;
    for (int i = 0; i < N; i++)
        map[i] = i * 7;
    TEST_ASSERT(map.size() == N, "collision insert count");

    for (int i = 0; i < N; i++)
        TEST_ASSERT(map[i] == i * 7, "collision lookup");

    // Erase half
    for (int i = 0; i < N / 2; i++)
        (void)map.erase(i);
    TEST_ASSERT(map.size() == N / 2, "collision erase count");

    // Reinsert
    for (int i = 0; i < N / 2; i++)
        map[i] = i * 11;
    TEST_ASSERT(map.size() == N, "collision reinsert count");

    return true;
}

// ============================================================================
// Sequential vs reverse vs random insert patterns
// ============================================================================
template<typename MapType>
bool test_insert_patterns()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    const int N = 5000;

    // Sequential insert
    {
        MapType map;
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        TEST_ASSERT(map.size() == N, "sequential size");
        for (int i = 0; i < N; i++)
            TEST_ASSERT(map[make_kv<Key>(i)] == make_kv<Val>(i), "sequential verify");
    }

    // Reverse insert
    {
        MapType map;
        for (int i = N - 1; i >= 0; i--)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        TEST_ASSERT(map.size() == N, "reverse size");
        for (int i = 0; i < N; i++)
            TEST_ASSERT(map[make_kv<Key>(i)] == make_kv<Val>(i), "reverse verify");
    }

    // Stride insert (even then odd)
    {
        MapType map;
        for (int i = 0; i < N; i += 2)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        for (int i = 1; i < N; i += 2)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        TEST_ASSERT(map.size() == N, "stride size");
    }

    // Large stride insert
    {
        MapType map;
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i * 127)] = make_kv<Val>(i);
        TEST_ASSERT(map.size() == N, "large-stride size");
        for (int i = 0; i < N; i++)
            TEST_ASSERT(map[make_kv<Key>(i * 127)] == make_kv<Val>(i), "large-stride verify");
    }

    return true;
}

// ============================================================================
// Erase patterns
// ============================================================================
template<typename MapType>
bool test_erase_patterns()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    const int N = 1000;

    // Erase from front
    {
        MapType map;
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        for (int i = 0; i < N / 2; i++)
            (void)map.erase(make_kv<Key>(i));
        TEST_ASSERT(map.size() == N / 2, "erase front size");
        TEST_ASSERT(map.count(make_kv<Key>(0)) == 0, "front key 0 gone");
        TEST_ASSERT(map.count(make_kv<Key>(N - 1)) == 1, "front key N-1 remains");
    }

    // Erase from back
    {
        MapType map;
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        for (int i = N - 1; i >= N / 2; i--)
            (void)map.erase(make_kv<Key>(i));
        TEST_ASSERT(map.size() == N / 2, "erase back size");
        TEST_ASSERT(map.count(make_kv<Key>(0)) == 1, "back key 0 remains");
        TEST_ASSERT(map.count(make_kv<Key>(N - 1)) == 0, "back key N-1 gone");
    }

    // Erase every 3rd
    {
        MapType map;
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        for (int i = 0; i < N; i += 3)
            (void)map.erase(make_kv<Key>(i));
        int expected = N - (N + 2) / 3;
        TEST_ASSERT((int)map.size() == expected, "erase every 3rd size");
    }

    // Erase all then reinsert
    {
        MapType map;
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i)] = make_kv<Val>(i);
        for (int i = 0; i < N; i++)
            (void)map.erase(make_kv<Key>(i));
        TEST_ASSERT(map.empty(), "erase all empty");
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i)] = make_kv<Val>(i * 2);
        TEST_ASSERT(map.size() == N, "reinsert after full erase");
    }

    return true;
}

// ============================================================================
// Multiple rehash cycles
// ============================================================================
template<typename MapType>
bool test_multiple_rehash()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    // Grow phase
    for (int i = 0; i < 10000; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i);

    // Force rehash to larger
    map.rehash(50000);
    TEST_ASSERT(map.size() == 10000, "rehash up size");
    for (int i = 0; i < 10000; i++)
        TEST_ASSERT(map[make_kv<Key>(i)] == make_kv<Val>(i), "rehash up value");

    // Force rehash to smaller (but still fits)
    map.rehash(15000);
    TEST_ASSERT(map.size() == 10000, "rehash down size");
    for (int i = 0; i < 10000; i++)
        TEST_ASSERT(map[make_kv<Key>(i)] == make_kv<Val>(i), "rehash down value");

    // Reserve and verify
    (void)map.reserve(100000);
    for (int i = 10000; i < 20000; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i);
    TEST_ASSERT(map.size() == 20000, "reserve then grow size");

    return true;
}

// ============================================================================
// uint32_t key type
// ============================================================================
template<typename MapType>
bool test_uint32_keys()
{
    MapType map;
    // Full uint32_t range spot checks
    map[0u] = 0;
    map[1u] = 1;
    map[255u] = 255;
    map[256u] = 256;
    map[65535u] = 65535;
    map[65536u] = 65536;
    map[0xFFFFFFFFu] = 999;
    map[0x80000000u] = 888;

    TEST_ASSERT(map[0u] == 0, "uint32 zero");
    TEST_ASSERT(map[255u] == 255, "uint32 255");
    TEST_ASSERT(map[65535u] == 65535, "uint32 65535");
    TEST_ASSERT(map[0xFFFFFFFFu] == 999, "uint32 max");
    TEST_ASSERT(map[0x80000000u] == 888, "uint32 high bit");

    // Dense range
    for (uint32_t i = 0; i < 10000; i++)
        map[i] = i * 3;
    TEST_ASSERT(map.size() >= 10000, "uint32 dense size");
    TEST_ASSERT(map[5000u] == 15000, "uint32 dense lookup");

    return true;
}

// ============================================================================
// Run all tests
// ============================================================================
template<typename MapType>
void run_common_tests(const char* name)
{
    printf("\n=== %s ===\n", name);
    RUN_TEST((test_basic_crud<MapType>));
    RUN_TEST((test_copy_move<MapType>));
    RUN_TEST((test_iterator<MapType>));
    RUN_TEST((test_reserve_rehash_clear<MapType>));
    RUN_TEST((test_erase_scenarios<MapType>));
    RUN_TEST((test_large_scale<MapType>));
    RUN_TEST((test_bucket_and_load<MapType>));
    RUN_TEST((test_single_and_empty<MapType>));
    RUN_TEST((test_duplicate_insert<MapType>));
    RUN_TEST((test_numeric_edge_ranges<MapType>));
    RUN_TEST((test_insert_patterns<MapType>));
    RUN_TEST((test_erase_patterns<MapType>));
    RUN_TEST((test_multiple_rehash<MapType>));
}

int main()
{
    // ---- emilib2ss ----
    run_common_tests<emilib::HashMap<int, int>>("emilib2ss <int,int>");
    run_common_tests<emilib::HashMap<int64_t, double>>("emilib2ss <int64,double>");

    printf("\n=== emilib2ss <uint32,int> ===\n");
    RUN_TEST((test_uint32_keys<emilib::HashMap<uint32_t, int>>));

    printf("\n=== emilib2ss <string,int> ===\n");
    RUN_TEST((test_string_key<emilib::HashMap<std::string, int>>));
    RUN_TEST((test_string_edge_ranges<emilib::HashMap<std::string, int>>));

    printf("\n=== emilib2ss <int,string> ===\n");
    RUN_TEST((test_string_val<emilib::HashMap<int, std::string>>));
    RUN_TEST((test_string_val_edge_ranges<emilib::HashMap<int, std::string>>));

    printf("\n=== emilib2ss <string,string> ===\n");
    RUN_TEST((test_string_string<emilib::HashMap<std::string, std::string>>));

    printf("\n=== emilib2ss <int,int> collision ===\n");
    RUN_TEST((test_dense_collision<emilib::HashMap<int, int, SameHashInt>>));

    // ---- emilib2o ----
    run_common_tests<emilib2::HashMap<int, int>>("emilib2o <int,int>");
    run_common_tests<emilib2::HashMap<int64_t, double>>("emilib2o <int64,double>");

    printf("\n=== emilib2o <uint32,int> ===\n");
    RUN_TEST((test_uint32_keys<emilib2::HashMap<uint32_t, int>>));

    printf("\n=== emilib2o <string,int> ===\n");
    RUN_TEST((test_string_key<emilib2::HashMap<std::string, int>>));
    RUN_TEST((test_string_edge_ranges<emilib2::HashMap<std::string, int>>));

    printf("\n=== emilib2o <int,string> ===\n");
    RUN_TEST((test_string_val<emilib2::HashMap<int, std::string>>));
    RUN_TEST((test_string_val_edge_ranges<emilib2::HashMap<int, std::string>>));

    printf("\n=== emilib2o <string,string> ===\n");
    RUN_TEST((test_string_string<emilib2::HashMap<std::string, std::string>>));

    printf("\n=== emilib2o <int,int> collision ===\n");
    RUN_TEST((test_dense_collision<emilib2::HashMap<int, int, SameHashInt>>));

    // ---- emilib2s ----
    run_common_tests<emilib3::HashMap<int, int>>("emilib2s <int,int>");
    run_common_tests<emilib3::HashMap<int64_t, double>>("emilib2s <int64,double>");

    printf("\n=== emilib2s <uint32,int> ===\n");
    RUN_TEST((test_uint32_keys<emilib3::HashMap<uint32_t, int>>));

    printf("\n=== emilib2s <string,int> ===\n");
    RUN_TEST((test_string_key<emilib3::HashMap<std::string, int>>));
    RUN_TEST((test_string_edge_ranges<emilib3::HashMap<std::string, int>>));

    printf("\n=== emilib2s <int,string> ===\n");
    RUN_TEST((test_string_val<emilib3::HashMap<int, std::string>>));
    RUN_TEST((test_string_val_edge_ranges<emilib3::HashMap<int, std::string>>));

    printf("\n=== emilib2s <string,string> ===\n");
    RUN_TEST((test_string_string<emilib3::HashMap<std::string, std::string>>));

    printf("\n=== emilib2s <int,int> collision ===\n");
    RUN_TEST((test_dense_collision<emilib3::HashMap<int, int, SameHashInt>>));

    printf("\n============================================================\n");
    printf("  Total:  %d\n", g_pass + g_fail);
    printf("  Passed: %d\n", g_pass);
    printf("  Failed: %d\n", g_fail);
    printf("============================================================\n");

    return g_fail > 0 ? 1 : 0;
}
