// Special key type tests for all emhash HashMap versions
// Tests: float key (NaN, -0.0/+0.0, infinity), string key,
//        move-only value, non-trivial destructor, large value, negative key

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <memory>
#include <limits>

static int g_pass = 0;
static int g_fail = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; return; } \
} while(0)

// ============================================================================
// Float key tests (NaN, -0.0/+0.0, infinity)
// ============================================================================
template<typename Map>
void test_float_key() {
    Map map;

    // Basic float key
    map[1.0f] = 10;
    map[2.5f] = 25;
    TEST_ASSERT(map.contains(1.0f), "float key contains");
    TEST_ASSERT(map.at(1.0f) == 10, "float key at");

    // -0.0 and +0.0 should be same key (IEEE 754: -0 == +0)
    map[-0.0f] = 100;
    map[+0.0f] = 200;
    TEST_ASSERT(map.contains(0.0f), "float zero key exists");
    TEST_ASSERT(map.at(0.0f) == 200, "float +0.0 overwrites -0.0");

    // Infinity
    float inf = std::numeric_limits<float>::infinity();
    map[inf] = 999;
    TEST_ASSERT(map.contains(inf), "float infinity key");

    g_pass++;
}

// ============================================================================
// String key tests
// ============================================================================
template<typename Map>
void test_string_key() {
    Map map;

    map[std::string("hello")] = 1;
    map[std::string("world")] = 2;
    map[std::string("")] = 3;  // empty string key

    TEST_ASSERT(map.contains(std::string("hello")), "string key contains");
    TEST_ASSERT(map.at(std::string("hello")) == 1, "string key at");
    TEST_ASSERT(map.at(std::string("")) == 3, "empty string key");

    // Long string
    std::string long_key(1000, 'x');
    map[long_key] = 42;
    TEST_ASSERT(map.contains(long_key), "long string key");
    TEST_ASSERT(map.at(long_key) == 42, "long string value");

    g_pass++;
}

// ============================================================================
// Move-only value tests
// ============================================================================
template<typename Map>
void test_move_only_value() {
    Map map;

    map[1] = std::make_unique<int>(10);
    map[2] = std::make_unique<int>(20);

    TEST_ASSERT(map.contains(1), "move-only value key exists");
    TEST_ASSERT(*map.at(1) == 10, "move-only value at");
    TEST_ASSERT(*map.at(2) == 20, "move-only value at 2");

    // Move construct
    Map map2(std::move(map));
    TEST_ASSERT(map2.contains(1), "moved map contains 1");
    TEST_ASSERT(*map2.at(1) == 10, "moved map value");

    g_pass++;
}

// ============================================================================
// Non-trivial destructor value tests
// ============================================================================
static int g_dtor_count = 0;

struct DtorCounter {
    int val;
    DtorCounter() : val(0) {}
    DtorCounter(int v) : val(v) {}
    ~DtorCounter() { g_dtor_count++; }
    DtorCounter(const DtorCounter& o) : val(o.val) {}
    DtorCounter(DtorCounter&& o) noexcept : val(o.val) { o.val = 0; }
    DtorCounter& operator=(const DtorCounter& o) { val = o.val; return *this; }
    DtorCounter& operator=(DtorCounter&& o) noexcept { val = o.val; o.val = 0; return *this; }
};

template<typename Map>
void test_dtor_value() {
    g_dtor_count = 0;
    {
        Map map;
        map[1] = DtorCounter(10);
        map[2] = DtorCounter(20);
        map[3] = DtorCounter(30);

        // Erase one
        map.erase(2);
    }
    // All should be destroyed when map goes out of scope
    TEST_ASSERT(g_dtor_count > 0, "destructors should be called");

    g_pass++;
}

// ============================================================================
// Large value tests (cache line boundary)
// ============================================================================
struct LargeValue {
    int data[64]; // 256 bytes
    LargeValue() { memset(data, 0, sizeof(data)); }
    LargeValue(int v) { for (auto& d : data) d = v; }
    int value() const { return data[0]; }
};

template<typename Map>
void test_large_value() {
    Map map;
    map[1] = LargeValue(10);
    map[2] = LargeValue(20);

    TEST_ASSERT(map.contains(1), "large value key exists");
    TEST_ASSERT(map.at(1).value() == 10, "large value at 1");
    TEST_ASSERT(map.at(2).value() == 20, "large value at 2");

    // Copy
    Map map2 = map;
    TEST_ASSERT(map2.at(1).value() == 10, "large value copy at 1");

    g_pass++;
}

// ============================================================================
// Negative key tests
// ============================================================================
template<typename Map>
void test_negative_key() {
    Map map;
    map[-1] = 10;
    map[-2] = 20;
    map[0] = 0;
    map[1] = 1;

    TEST_ASSERT(map.at(-1) == 10, "negative key -1");
    TEST_ASSERT(map.at(-2) == 20, "negative key -2");
    TEST_ASSERT(map.at(0) == 0, "key 0");
    TEST_ASSERT(map.at(1) == 1, "key 1");

    g_pass++;
}

// ============================================================================
// Run all tests for a specific map type
// ============================================================================
template<typename Map>
void run_special_key_tests(const char* name) {
    printf("Testing %s:\n", name);
    test_negative_key<Map>();
    printf("  %s done\n\n", name);
}

int main() {
    printf("=== Special Key Type Tests ===\n\n");

    // Float key tests
    printf("Testing float key:\n");
    test_float_key<emhash5::HashMap<float, int>>();
    test_float_key<emhash6::HashMap<float, int>>();
    test_float_key<emhash7::HashMap<float, int>>();
    test_float_key<emhash8::HashMap<float, int>>();
    printf("  float key done\n\n");

    // String key tests
    printf("Testing string key:\n");
    test_string_key<emhash5::HashMap<std::string, int>>();
    test_string_key<emhash6::HashMap<std::string, int>>();
    test_string_key<emhash7::HashMap<std::string, int>>();
    test_string_key<emhash8::HashMap<std::string, int>>();
    printf("  string key done\n\n");

    // Move-only value tests
    printf("Testing move-only value:\n");
    test_move_only_value<emhash5::HashMap<int, std::unique_ptr<int>>>();
    test_move_only_value<emhash8::HashMap<int, std::unique_ptr<int>>>();
    printf("  move-only value done\n\n");

    // Non-trivial destructor tests
    printf("Testing non-trivial destructor:\n");
    test_dtor_value<emhash5::HashMap<int, DtorCounter>>();
    test_dtor_value<emhash6::HashMap<int, DtorCounter>>();
    test_dtor_value<emhash7::HashMap<int, DtorCounter>>();
    test_dtor_value<emhash8::HashMap<int, DtorCounter>>();
    printf("  non-trivial destructor done\n\n");

    // Large value tests
    printf("Testing large value:\n");
    test_large_value<emhash5::HashMap<int, LargeValue>>();
    test_large_value<emhash8::HashMap<int, LargeValue>>();
    printf("  large value done\n\n");

    // Negative key tests
    printf("Testing negative key:\n");
    test_negative_key<emhash5::HashMap<int, int>>();
    test_negative_key<emhash6::HashMap<int, int>>();
    test_negative_key<emhash7::HashMap<int, int>>();
    test_negative_key<emhash8::HashMap<int, int>>();
    printf("  negative key done\n\n");

    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
