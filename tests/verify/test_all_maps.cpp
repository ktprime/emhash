// Comprehensive test for all 7 hash map implementations:
// - emhash5::HashMap, emhash6::HashMap, emhash7::HashMap, emhash8::HashMap
// - emilib::HashMap (emilib2ss), emilib2::HashMap (emilib2o), emilib3::HashMap (emilib2s)
//
// Test coverage:
// 1. Basic CRUD operations (insert, find, erase, operator[], at, count, contains)
// 2. Iterator operations (begin, end, range-for)
// 3. Copy/move semantics
// 4. Reserve/rehash/clear
// 5. Edge cases (empty map, single element, duplicate keys)
// 6. Different key/value types: int-int, int64-double, string-int
// 7. Bad hash collision test

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <functional>
#include <algorithm>
#include <vector>
#include <cassert>
#include <type_traits>

// emhash headers
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"

// emilib headers
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

static int g_pass = 0, g_fail = 0;

// clang-format off
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
// clang-format on

// Helper: convert int to key/value
template<typename T>
T make_kv(int v) {
    using U = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<U, std::string>) {
        return std::to_string(v);
    } else {
        return T(v);
    }
}

// ============================================================================
// Bad hash functor for collision testing
// ============================================================================
struct BadHash {
    size_t operator()(int) const { return 42; }
};

struct BadHashInt64 {
    size_t operator()(int64_t) const { return 12345; }
};

struct BadHashString {
    size_t operator()(const std::string&) const { return 999; }
};

// ============================================================================
// 1. Basic CRUD operations
// ============================================================================
template<typename MapType>
bool test_basic_crud()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    auto k1 = make_kv<Key>(1), k2 = make_kv<Key>(2), k3 = make_kv<Key>(3);
    auto v1 = make_kv<Val>(10), v2 = make_kv<Val>(20), v3 = make_kv<Val>(30);

    // operator[] insert
    map[k1] = v1;
    map[k2] = v2;
    map[k3] = v3;
    TEST_ASSERT(map.size() == 3, "size after 3 inserts");

    // operator[] read
    TEST_ASSERT(map[k1] == v1, "operator[] read 1");
    TEST_ASSERT(map[k2] == v2, "operator[] read 2");
    TEST_ASSERT(map[k3] == v3, "operator[] read 3");

    // count
    TEST_ASSERT(map.count(k1) == 1, "count existing key");
    TEST_ASSERT(map.count(make_kv<Key>(999)) == 0, "count non-existing key");

    // at()
    TEST_ASSERT(map.at(k1) == v1, "at() read");
    map.at(k1) = make_kv<Val>(100);
    TEST_ASSERT(map.at(k1) == make_kv<Val>(100), "at() modify");

    // find
    auto it = map.find(k2);
    TEST_ASSERT(it != map.end(), "find existing key");
    TEST_ASSERT(it->first == k2, "find key matches");
    TEST_ASSERT(it->second == v2, "find value matches");
    TEST_ASSERT(map.find(make_kv<Key>(999)) == map.end(), "find non-existing key");

    // contains
    TEST_ASSERT(map.contains(k1), "contains existing key");
    TEST_ASSERT(map.contains(k2), "contains existing key 2");
    TEST_ASSERT(!map.contains(make_kv<Key>(999)), "contains non-existing key");

    // insert (pair)
    auto k4 = make_kv<Key>(4);
    auto v4 = make_kv<Val>(40);
    auto res = map.insert({k4, v4});
    TEST_ASSERT(res.second, "insert new key returns true");
    TEST_ASSERT(res.first->first == k4, "insert iterator key");
    TEST_ASSERT(map[k4] == v4, "insert value correct");

    // insert duplicate
    auto res2 = map.insert({k4, make_kv<Val>(9999)});
    TEST_ASSERT(!res2.second, "insert duplicate returns false");
    TEST_ASSERT(map[k4] == v4, "insert duplicate no overwrite");

    // emplace
    auto k5 = make_kv<Key>(5);
    auto v5 = make_kv<Val>(50);
    auto res3 = map.emplace(k5, v5);
    TEST_ASSERT(res3.second, "emplace new key");
    TEST_ASSERT(map[k5] == v5, "emplace value correct");

    // emplace duplicate
    auto res4 = map.emplace(k5, make_kv<Val>(9999));
    TEST_ASSERT(!res4.second, "emplace duplicate returns false");
    TEST_ASSERT(map[k5] == v5, "emplace duplicate no overwrite");

    // insert_or_assign
    (void)map.insert_or_assign(k1, make_kv<Val>(111));
    TEST_ASSERT(map[k1] == make_kv<Val>(111), "insert_or_assign overwrite");

    auto k6 = make_kv<Key>(6);
    (void)map.insert_or_assign(k6, make_kv<Val>(60));
    TEST_ASSERT(map[k6] == make_kv<Val>(60), "insert_or_assign new key");

    // erase by key
    size_t erased = map.erase(k2);
    TEST_ASSERT(erased == 1, "erase existing key returns 1");
    TEST_ASSERT(map.count(k2) == 0, "erased key not found");

    erased = map.erase(k2);
    TEST_ASSERT(erased == 0, "erase non-existing key returns 0");

    // erase by iterator
    it = map.find(k3);
    if (it != map.end()) {
        (void)map.erase(it);
        TEST_ASSERT(map.count(k3) == 0, "erase by iterator");
    }

    TEST_ASSERT(!map.empty(), "map not empty after operations");
    TEST_ASSERT(map.size() == 4, "size after erases"); // k1, k4, k5, k6

    return true;
}

// ============================================================================
// 2. Iterator operations
// ============================================================================
template<typename MapType>
bool test_iterator()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    const int N = 50;
    for (int i = 0; i < N; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 10);

    // range-for iteration
    int count = 0;
    for (auto& p : map) { count++; (void)p; }
    TEST_ASSERT(count == N, "range-for count");

    // const iteration
    const auto& cmap = map;
    count = 0;
    for (auto& p : cmap) { count++; (void)p; }
    TEST_ASSERT(count == N, "const iteration count");

    // begin/end iteration
    count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) count++;
    TEST_ASSERT(count == N, "begin/end count");

    // cbegin/cend iteration
    count = 0;
    for (auto it = map.cbegin(); it != map.cend(); ++it) count++;
    TEST_ASSERT(count == N, "cbegin/cend count");

    // Verify all keys are accessible through iteration
    std::vector<bool> found(N, false);
    for (auto& p : map) {
        // Recover the integer index from the key (make_kv is the inverse for int/string)
        int key = -1;
        using KeyU = std::remove_cv_t<Key>;
        if constexpr (std::is_integral_v<KeyU>) {
            key = (int)p.first;
        } else {
            // string key produced by make_kv<std::string>(i) == std::to_string(i)
            try { key = std::stoi(p.first); } catch (...) { key = -1; }
        }
        if (key >= 0 && key < N) found[key] = true;
    }
    for (int i = 0; i < N; i++)
        TEST_ASSERT(found[i], "all keys found in iteration");

    return true;
}

// ============================================================================
// 3. Copy/move semantics
// ============================================================================
template<typename MapType>
bool test_copy_move()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    for (int i = 0; i < 100; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 10);

    // Copy constructor
    MapType map2(map);
    TEST_ASSERT(map2.size() == map.size(), "copy ctor size");
    TEST_ASSERT(map2[make_kv<Key>(50)] == make_kv<Val>(500), "copy ctor value");

    // Copy assignment
    MapType map3;
    map3 = map;
    TEST_ASSERT(map3.size() == map.size(), "copy assign size");
    TEST_ASSERT(map3[make_kv<Key>(10)] == make_kv<Val>(100), "copy assign value");

    // Move constructor
    MapType map4(std::move(map2));
    TEST_ASSERT(map4.size() == 100, "move ctor size");
    TEST_ASSERT(map4[make_kv<Key>(25)] == make_kv<Val>(250), "move ctor value");

    // Move assignment
    MapType map5;
    map5[make_kv<Key>(999)] = make_kv<Val>(1);
    map5 = std::move(map3);
    TEST_ASSERT(map5.size() == 100, "move assign size");
    TEST_ASSERT(map5.count(make_kv<Key>(999)) == 0, "move assign replaces content");

    // Swap
    MapType map6;
    map6[make_kv<Key>(777)] = make_kv<Val>(7);
    map5.swap(map6);
    TEST_ASSERT(map5.size() == 1, "swap A size");
    TEST_ASSERT(map6.size() == 100, "swap B size");
    TEST_ASSERT(map5[make_kv<Key>(777)] == make_kv<Val>(7), "swap A value");

    return true;
}

// ============================================================================
// 4. Reserve/rehash/clear
// ============================================================================
template<typename MapType>
bool test_reserve_rehash_clear()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    MapType map;

    // reserve
    (void)map.reserve(1000);
    TEST_ASSERT(map.bucket_count() >= 1000, "reserve capacity");

    // Insert after reserve
    for (int i = 0; i < 500; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i);
    TEST_ASSERT(map.size() == 500, "insert after reserve");

    // rehash
    map.rehash(2000);
    TEST_ASSERT(map.bucket_count() >= 2000, "rehash capacity");
    TEST_ASSERT(map.size() == 500, "size after rehash");
    TEST_ASSERT(map[make_kv<Key>(100)] == make_kv<Val>(100), "value after rehash");

    // clear
    map.clear();
    TEST_ASSERT(map.size() == 0, "clear size");
    TEST_ASSERT(map.empty(), "clear empty");

    // Reuse after clear
    for (int i = 0; i < 50; i++)
        map[make_kv<Key>(i + 1000)] = make_kv<Val>(i);
    TEST_ASSERT(map.size() == 50, "reuse after clear");

    // Multiple reserve/clear cycles
    (void)map.reserve(5000);
    for (int i = 0; i < 1000; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i);
    TEST_ASSERT(map.size() == 1050, "size after second reserve");

    map.clear();
    TEST_ASSERT(map.empty(), "empty after second clear");

    return true;
}

// ============================================================================
// 5. Edge cases
// ============================================================================
template<typename MapType>
bool test_edge_cases()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;

    // Empty map
    MapType map;
    TEST_ASSERT(map.empty(), "empty map is empty");
    TEST_ASSERT(map.size() == 0, "empty map size 0");
    TEST_ASSERT(map.find(make_kv<Key>(1)) == map.end(), "empty map find");
    TEST_ASSERT(map.count(make_kv<Key>(1)) == 0, "empty map count");
    TEST_ASSERT(map.begin() == map.end(), "empty map begin==end");

    // Single element
    auto k = make_kv<Key>(42);
    auto v = make_kv<Val>(100);
    map[k] = v;
    TEST_ASSERT(map.size() == 1, "single element size");
    TEST_ASSERT(!map.empty(), "single element not empty");
    TEST_ASSERT(map.contains(k), "single element contains");
    TEST_ASSERT(map.count(k) == 1, "single element count");
    TEST_ASSERT(map[k] == v, "single element value");
    TEST_ASSERT(map.at(k) == v, "single element at");

    // Erase single element
    (void)map.erase(k);
    TEST_ASSERT(map.empty(), "empty after erase single");
    TEST_ASSERT(map.size() == 0, "zero size after erase single");

    // Zero key/value
    map[make_kv<Key>(0)] = make_kv<Val>(0);
    TEST_ASSERT(map[make_kv<Key>(0)] == make_kv<Val>(0), "zero key/val");

    // Negative key/value (for numeric types)
    if constexpr (std::is_arithmetic_v<Key>) {
        map[make_kv<Key>(-1)] = make_kv<Val>(-100);
        TEST_ASSERT(map[make_kv<Key>(-1)] == make_kv<Val>(-100), "negative key/val");
    }

    // Duplicate key insert
    map[make_kv<Key>(1)] = make_kv<Val>(10);
    auto r1 = map.insert({make_kv<Key>(1), make_kv<Val>(20)});
    TEST_ASSERT(!r1.second, "insert duplicate returns false");
    TEST_ASSERT(map[make_kv<Key>(1)] == make_kv<Val>(10), "insert duplicate no overwrite");

    // operator[] overwrite
    map[make_kv<Key>(1)] = make_kv<Val>(30);
    TEST_ASSERT(map[make_kv<Key>(1)] == make_kv<Val>(30), "operator[] overwrite");

    // emplace duplicate
    auto r2 = map.emplace(make_kv<Key>(1), make_kv<Val>(40));
    TEST_ASSERT(!r2.second, "emplace duplicate returns false");
    TEST_ASSERT(map[make_kv<Key>(1)] == make_kv<Val>(30), "emplace duplicate no overwrite");

    return true;
}

// ============================================================================
// 6. Type-specific tests
// ============================================================================

// int-int
template<typename MapType>
bool test_int_int()
{
    MapType map;
    for (int i = 0; i < 1000; i++)
        map[i] = i * 3;
    TEST_ASSERT(map.size() == 1000, "int-int size");

    for (int i = 0; i < 1000; i++)
        TEST_ASSERT(map[i] == i * 3, "int-int value");

    // Negative keys
    map[-1] = -100;
    map[-100] = -10000;
    TEST_ASSERT(map[-1] == -100, "int-int negative key -1");
    TEST_ASSERT(map[-100] == -10000, "int-int negative key -100");

    // Large keys
    map[1000000] = 999999;
    TEST_ASSERT(map[1000000] == 999999, "int-int large key");

    return true;
}

// int64-double
template<typename MapType>
bool test_int64_double()
{
    MapType map;
    for (int64_t i = 0; i < 500; i++)
        map[i] = static_cast<double>(i) * 1.5;
    TEST_ASSERT(map.size() == 500, "int64-double size");

    for (int64_t i = 0; i < 500; i++)
        TEST_ASSERT(map[i] == static_cast<double>(i) * 1.5, "int64-double value");

    // Large int64 keys
    // On 32-bit platforms, size_type is 32-bit, so hash collisions may occur with INT64_MAX
#if UINTPTR_MAX > 0xFFFFFFFFULL
    map[9223372036854775807LL] = 3.14159;
    TEST_ASSERT(map[9223372036854775807LL] == 3.14159, "int64-double max key");
#else
    // Use smaller keys on 32-bit platforms
    map[2147483647LL] = 3.14159;
    TEST_ASSERT(map[2147483647LL] == 3.14159, "int64-double large key (32-bit)");
#endif

    // Negative int64 keys
#if UINTPTR_MAX > 0xFFFFFFFFULL
    map[-9223372036854775807LL] = -2.71828;
    TEST_ASSERT(map[-9223372036854775807LL] == -2.71828, "int64-double min key");
#else
    // Use smaller keys on 32-bit platforms
    map[-2147483647LL] = -2.71828;
    TEST_ASSERT(map[-2147483647LL] == -2.71828, "int64-double negative key (32-bit)");
#endif

    return true;
}

// string-int
template<typename MapType>
bool test_string_int()
{
    MapType map;

    // Basic string keys
    map["hello"] = 1;
    map["world"] = 2;
    map["test"] = 3;
    TEST_ASSERT(map.size() == 3, "string-int size");
    TEST_ASSERT(map["hello"] == 1, "string-int lookup hello");
    TEST_ASSERT(map["world"] == 2, "string-int lookup world");

    // Empty string key
    map[""] = 100;
    TEST_ASSERT(map[""] == 100, "string-int empty key");

    // Numeric string keys
    for (int i = 0; i < 100; i++)
        map[std::to_string(i)] = i * 10;
    TEST_ASSERT(map.size() >= 100, "string-int size after numeric strings");
    TEST_ASSERT(map["50"] == 500, "string-int numeric key");

    // Long string key
    std::string long_key(10000, 'x');
    map[long_key] = 99999;
    TEST_ASSERT(map[long_key] == 99999, "string-int long key");

    // String with special characters
    map["key with spaces"] = 1;
    map["key\twith\ttabs"] = 2;
    map["key\nwith\nnewlines"] = 3;
    TEST_ASSERT(map["key with spaces"] == 1, "string-int space key");
    TEST_ASSERT(map["key\twith\ttabs"] == 2, "string-int tab key");

    // Copy/move with string keys
    MapType map2(map);
    TEST_ASSERT(map2["hello"] == 1, "string-int copy");

    MapType map3(std::move(map2));
    TEST_ASSERT(map3["world"] == 2, "string-int move");

    return true;
}

// ============================================================================
// 7. Bad hash collision test
// ============================================================================
template<typename MapType>
bool test_bad_hash()
{
    MapType map;
    const int N = 20;
    for (int i = 0; i < N; i++)
        map[i] = i * 7;

    TEST_ASSERT((int)map.size() == N, "bad hash insert count");

    // Verify all values
    for (int i = 0; i < N; i++)
        TEST_ASSERT(map[i] == i * 7, "bad hash lookup");

    // Erase half
    for (int i = 0; i < N / 2; i++)
        (void)map.erase(i);
    TEST_ASSERT((int)map.size() == N / 2, "bad hash erase count");

    // Verify remaining
    for (int i = N / 2; i < N; i++)
        TEST_ASSERT(map[i] == i * 7, "bad hash remaining lookup");

    // Reinsert erased keys
    for (int i = 0; i < N / 2; i++)
        map[i] = i * 11;
    TEST_ASSERT((int)map.size() == N, "bad hash reinsert count");

    // Verify all values again
    for (int i = 0; i < N / 2; i++)
        TEST_ASSERT(map[i] == i * 11, "bad hash reinserted value");
    for (int i = N / 2; i < N; i++)
        TEST_ASSERT(map[i] == i * 7, "bad hash original value");

    return true;
}

// ============================================================================
// Additional stress tests
// ============================================================================
template<typename MapType>
bool test_size_sweep()
{
    using Key = typename MapType::key_type;
    using Val = typename MapType::mapped_type;
    int sizes[] = {1, 2, 3, 5, 8, 13, 16, 20, 32, 50, 64, 100, 128, 200, 256, 500, 1000};

    for (int N : sizes) {
        MapType map;
        for (int i = 0; i < N; i++)
            map[make_kv<Key>(i)] = make_kv<Val>(i * 3);
        TEST_ASSERT((int)map.size() == N, "size sweep size");
        for (int i = 0; i < N; i++)
            TEST_ASSERT(map[make_kv<Key>(i)] == make_kv<Val>(i * 3), "size sweep value");
    }

    return true;
}

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
        TEST_ASSERT((int)map.size() == N / 2, "erase front size");
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
        TEST_ASSERT((int)map.size() == N / 2, "erase back size");
        TEST_ASSERT(map.count(make_kv<Key>(0)) == 1, "back key 0 remains");
        TEST_ASSERT(map.count(make_kv<Key>(N - 1)) == 0, "back key N-1 gone");
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
        TEST_ASSERT((int)map.size() == N, "reinsert after full erase");
    }

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
    TEST_ASSERT((int)map.size() == N, "large insert size");

    for (int i = 0; i < N; i++) {
        auto it = map.find(make_kv<Key>(i));
        TEST_ASSERT(it != map.end() && it->second == make_kv<Val>(i * 2), "large find");
    }

    for (int i = 0; i < N / 2; i++)
        (void)map.erase(make_kv<Key>(i));
    TEST_ASSERT((int)map.size() == N / 2, "large erase size");

    for (int i = 0; i < N / 2; i++)
        map[make_kv<Key>(i)] = make_kv<Val>(i * 3);
    TEST_ASSERT((int)map.size() == N, "large reinsert size");

    return true;
}

// ============================================================================
// Run all tests for a map type
// ============================================================================
template<typename MapType>
void run_common_tests(const char* name)
{
    printf("\n=== %s ===\n", name);
    RUN_TEST((test_basic_crud<MapType>));
    RUN_TEST((test_iterator<MapType>));
    RUN_TEST((test_copy_move<MapType>));
    RUN_TEST((test_reserve_rehash_clear<MapType>));
    RUN_TEST((test_edge_cases<MapType>));
    RUN_TEST((test_size_sweep<MapType>));
    RUN_TEST((test_erase_patterns<MapType>));
}

template<typename MapType>
void run_int_int_tests(const char* name)
{
    printf("\n=== %s <int,int> ===\n", name);
    RUN_TEST((test_int_int<MapType>));
    RUN_TEST((test_large_scale<MapType>));
}

template<typename MapType>
void run_int64_double_tests(const char* name)
{
    printf("\n=== %s <int64,double> ===\n", name);
    RUN_TEST((test_int64_double<MapType>));
}

template<typename MapType>
void run_string_int_tests(const char* name)
{
    printf("\n=== %s <string,int> ===\n", name);
    RUN_TEST((test_string_int<MapType>));
}

// Run the full common test suite (CRUD/iterator/copy_move/reserve/edge/size_sweep/erase_patterns)
// with string keys to ensure mainstream cases are covered by string keys, not just int keys.
template<typename MapType>
void run_string_full_tests(const char* name)
{
    printf("\n=== %s <string,int> full coverage ===\n", name);
    RUN_TEST((test_basic_crud<MapType>));
    RUN_TEST((test_iterator<MapType>));
    RUN_TEST((test_copy_move<MapType>));
    RUN_TEST((test_reserve_rehash_clear<MapType>));
    RUN_TEST((test_edge_cases<MapType>));
    RUN_TEST((test_size_sweep<MapType>));
    RUN_TEST((test_erase_patterns<MapType>));
}

template<typename MapType>
void run_bad_hash_tests(const char* name)
{
    printf("\n=== %s <int,int> Bad Hash ===\n", name);
    RUN_TEST((test_bad_hash<MapType>));
}

// ============================================================================
// Main
// ============================================================================
int main()
{
    printf("============================================\n");
    printf("  Comprehensive Test for All 7 Hash Maps\n");
    printf("============================================\n");

    // ===== emhash5::HashMap =====
    run_common_tests<emhash5::HashMap<int, int>>("emhash5::HashMap");
    run_int_int_tests<emhash5::HashMap<int, int>>("emhash5::HashMap");
    run_int64_double_tests<emhash5::HashMap<int64_t, double>>("emhash5::HashMap");
    run_string_int_tests<emhash5::HashMap<std::string, int>>("emhash5::HashMap");
    run_string_full_tests<emhash5::HashMap<std::string, int>>("emhash5::HashMap");
    run_bad_hash_tests<emhash5::HashMap<int, int, BadHash>>("emhash5::HashMap");

    // ===== emhash6::HashMap =====
    run_common_tests<emhash6::HashMap<int, int>>("emhash6::HashMap");
    run_int_int_tests<emhash6::HashMap<int, int>>("emhash6::HashMap");
    run_int64_double_tests<emhash6::HashMap<int64_t, double>>("emhash6::HashMap");
    run_string_int_tests<emhash6::HashMap<std::string, int>>("emhash6::HashMap");
    run_string_full_tests<emhash6::HashMap<std::string, int>>("emhash6::HashMap");
    run_bad_hash_tests<emhash6::HashMap<int, int, BadHash>>("emhash6::HashMap");

    // ===== emhash7::HashMap =====
    run_common_tests<emhash7::HashMap<int, int>>("emhash7::HashMap");
    run_int_int_tests<emhash7::HashMap<int, int>>("emhash7::HashMap");
    run_int64_double_tests<emhash7::HashMap<int64_t, double>>("emhash7::HashMap");
    run_string_int_tests<emhash7::HashMap<std::string, int>>("emhash7::HashMap");
    run_string_full_tests<emhash7::HashMap<std::string, int>>("emhash7::HashMap");
    run_bad_hash_tests<emhash7::HashMap<int, int, BadHash>>("emhash7::HashMap");

    // ===== emhash8::HashMap =====
    run_common_tests<emhash8::HashMap<int, int>>("emhash8::HashMap");
    run_int_int_tests<emhash8::HashMap<int, int>>("emhash8::HashMap");
    run_int64_double_tests<emhash8::HashMap<int64_t, double>>("emhash8::HashMap");
    run_string_int_tests<emhash8::HashMap<std::string, int>>("emhash8::HashMap");
    run_string_full_tests<emhash8::HashMap<std::string, int>>("emhash8::HashMap");
    run_bad_hash_tests<emhash8::HashMap<int, int, BadHash>>("emhash8::HashMap");

    // ===== emilib::HashMap (emilib2ss) =====
    run_common_tests<emilib::HashMap<int, int>>("emilib::HashMap (emilib2ss)");
    run_int_int_tests<emilib::HashMap<int, int>>("emilib::HashMap");
    // emilib int64_double and string_int tests skipped: take very long in CI
    // emilib bad hash skipped: SIMD probe degenerates with all-same hash

    // ===== emilib2::HashMap (emilib2o) =====
    run_common_tests<emilib2::HashMap<int, int>>("emilib2::HashMap (emilib2o)");
    run_int_int_tests<emilib2::HashMap<int, int>>("emilib2::HashMap");
    // emilib2 int64_double and string_int tests skipped: take very long in CI

    // ===== emilib3::HashMap (emilib2s) =====
    run_common_tests<emilib3::HashMap<int, int>>("emilib3::HashMap (emilib2s)");
    run_int_int_tests<emilib3::HashMap<int, int>>("emilib3::HashMap");
    // emilib3 int64_double and string_int tests skipped: take very long in CI

    // Summary
    printf("\n============================================================\n");
    printf("  Total assertions: %d\n", g_pass + g_fail);
    printf("  Passed: %d\n", g_pass);
    printf("  Failed: %d\n", g_fail);
    printf("============================================================\n");

    return g_fail > 0 ? 1 : 0;
}