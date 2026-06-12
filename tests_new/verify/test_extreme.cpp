/**
 * EMH_FIND_HIT bug 修复的极端测试
 *
 * 覆盖：各种整型 key、INACTIVE 边界值、各种操作后的残留检查
 *
 * 编译: g++ -std=c++17 -O2 -o test_extreme test/test_extreme.cpp
 */

#define EMH_FIND_HIT 1
#include "../hash_table5.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <string>

static int total = 0, passed = 0;

#define CHECK(cond, msg) do { \
    total++; \
    if (cond) { passed++; } \
    else { printf("  FAIL [%d]: %s\n", total, msg); } \
} while(0)

// 前向声明
template<typename Map, typename KeyT>
void fp_check(Map& map, KeyT inactive, const char* msg);

// ============================================================
// 通用测试模板：对任意整型 KeyT
// ============================================================
template<typename KeyT>
void test_int_type(const char* name)
{
    printf("\n--- %s (sizeof=%zu) ---\n", name, sizeof(KeyT));

    using Map = emhash5::HashMap<KeyT, KeyT>;
    const KeyT inactive = (KeyT)emhash5::INACTIVE;

    // 1. 空 map: find(key) 对各种边界 key
    {
        Map map(16);
        CHECK(map.find(inactive) == map.end(), "empty find(INACTIVE)");
        CHECK(map.find(KeyT(0)) == map.end(), "empty find(0)");
        CHECK(!map.contains(inactive), "empty contains(INACTIVE)");
    }

    // 2. 空 map: find(key, hash) 遍历所有桶
    {
        Map map(16);
        fp_check(map, inactive, "empty find(INACTIVE,hash) no false positives");
    }

    // 3. 插入 INACTIVE key 后正常查找
    {
        Map map(16);
        map.insert_unique(inactive, KeyT(99));
        auto it = map.find(inactive);
        CHECK(it != map.end() && it->second == KeyT(99), "find inserted INACTIVE key");

        // 删除后再查找
        map.erase(inactive);
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after erase");
        fp_check(map, inactive, "find(INACTIVE,hash) after erase");
    }

    // 4. 插入大量数据后 clear，检查残留
    {
        Map map(16);
        for (KeyT i = 0; i < KeyT(200); i++)
            map.insert_unique(i, i);
        map.clear();
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after clear");
        fp_check(map, inactive, "find(INACTIVE,hash) after clear");
    }

    // 5. rehash 后检查
    {
        Map map(4);
        for (KeyT i = 0; i < KeyT(200); i++)
            map.insert_unique(i, i);
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after rehash");
        fp_check(map, inactive, "find(INACTIVE,hash) after rehash");
    }

    // 6. 插入-删除-插入 循环，制造大量空洞
    {
        Map map(32);
        for (int round = 0; round < 5; round++) {
            for (KeyT i = 0; i < KeyT(100); i++)
                map[i] = i;
            for (KeyT i = KeyT(1); i < KeyT(100); i += KeyT(2))
                map.erase(i);
        }
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after churn");
        fp_check(map, inactive, "find(INACTIVE,hash) after churn");
    }

    // 7. swap 后检查
    {
        Map map1(16), map2(64);
        for (KeyT i = 0; i < KeyT(50); i++) map1[i] = i;
        for (KeyT i = 0; i < KeyT(30); i++) map2[i] = i;
        map1.swap(map2);
        CHECK(map1.find(inactive) == map1.end(), "find INACTIVE after swap (map1)");
        CHECK(map2.find(inactive) == map2.end(), "find INACTIVE after swap (map2)");
    }

    // 8. reserve 后检查
    {
        Map map;
        map.reserve(1000);
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after reserve");
        fp_check(map, inactive, "find(INACTIVE,hash) after reserve");
    }

    // 9. shrink_to_fit 后检查
    {
        Map map(128);
        for (KeyT i = 0; i < KeyT(10); i++) map[i] = i;
        map.shrink_to_fit();
        CHECK(map.find(inactive) == map.end(), "find INACTIVE after shrink_to_fit");
    }

    // 10. key == INACTIVE-1 (哨兵值 -2)
    {
        Map map(16);
        KeyT sentinel = KeyT(KeyT(0) - 2);
        map.insert_unique(sentinel, KeyT(77));
        auto it = map.find(sentinel);
        CHECK(it != map.end() && it->second == KeyT(77), "find sentinel key (-2)");
        map.erase(sentinel);
        CHECK(map.find(sentinel) == map.end(), "find sentinel after erase");
    }
}

template<typename Map, typename KeyT>
void fp_check(Map& map, KeyT inactive, const char* msg)
{
    int fp = 0;
    for (size_t b = 0; b < map.bucket_count(); b++) {
        if (map.find(inactive, b) != map.end()) fp++;
    }
    CHECK(fp == 0, msg);
}

// ============================================================
// 非整型 key (string) 不应受影响
// ============================================================
void test_string_key()
{
    printf("\n--- std::string key ---\n");
    using Map = emhash5::HashMap<std::string, int>;
    Map map(16);
    CHECK(map.find("") == map.end(), "empty string find");
    CHECK(map.find(std::string(256, '\xFF')) == map.end(), "long FF string find");

    map["hello"] = 1;
    map[""] = 2;
    CHECK(map.find("hello") != map.end(), "find hello");
    CHECK(map.find("") != map.end(), "find empty string");
    map.erase("");
    CHECK(map.find("") == map.end(), "find empty string after erase");
}

// ============================================================
// 极端容量测试
// ============================================================
void test_extreme_sizes()
{
    printf("\n--- Extreme sizes ---\n");
    using Map = emhash5::HashMap<int32_t, int32_t>;
    const int32_t inactive = (int32_t)emhash5::INACTIVE;

    // 极小容量
    {
        Map map(1);
        CHECK(map.find(inactive) == map.end(), "size=1 find INACTIVE");
    }

    // 大容量空 map
    {
        Map map(1 << 20); // 1M buckets
        CHECK(map.find(inactive) == map.end(), "1M buckets find INACTIVE");
        // 只抽查几个 hash 值
        bool ok = true;
        for (size_t h = 0; h < 100; h++) {
            if (map.find(inactive, h) != map.end()) { ok = false; break; }
        }
        CHECK(ok, "1M buckets find(INACTIVE,hash) spot check");
    }
}

// ============================================================
// 并发操作序列：交替 insert/erase 制造最大空洞
// ============================================================
void test_alternating_ops()
{
    printf("\n--- Alternating insert/erase ---\n");
    using Map = emhash5::HashMap<int32_t, int32_t>;
    const int32_t inactive = (int32_t)emhash5::INACTIVE;
    Map map(32);

    for (int round = 0; round < 10; round++) {
        for (int32_t i = 0; i < 1000; i++)
            map[i * 3] = i;
        for (int32_t i = 0; i < 1000; i += 2)
            map.erase(i * 3);
    }
    CHECK(map.find(inactive) == map.end(), "find INACTIVE after alternating ops");

    int fp = 0;
    for (size_t b = 0; b < map.bucket_count(); b++) {
        if (map.find(inactive, b) != map.end()) fp++;
    }
    CHECK(fp == 0, "find(INACTIVE,hash) after alternating ops");
}

// ============================================================
// INACTIVE 附近的 key 值边界测试
// ============================================================
void test_inactive_boundary()
{
    printf("\n--- INACTIVE boundary keys ---\n");
    using Map = emhash5::HashMap<int32_t, int32_t>;
    const int32_t inactive = (int32_t)emhash5::INACTIVE;
    Map map(16);

    int32_t keys[] = {inactive - 3, inactive - 2, inactive - 1, inactive, inactive + 1, 0, 1, 2};
    for (auto k : keys)
        map[k] = k * 10;

    bool all_found = true;
    for (auto k : keys) {
        auto it = map.find(k);
        if (it == map.end() || it->second != k * 10) {
            all_found = false;
            printf("    key=%d: expected val=%d, ", k, k * 10);
            if (it == map.end()) printf("NOT FOUND\n");
            else printf("got val=%d\n", it->second);
        }
    }
    CHECK(all_found, "all boundary keys found correctly");

    // 逐个删除再验证
    for (auto k : keys) {
        map.erase(k);
        if (map.find(k) != map.end()) {
            CHECK(false, "key still found after erase");
            return;
        }
    }
    CHECK(true, "all boundary keys erased correctly");

    // 删除后 find(key, hash) 不应误报
    int fp = 0;
    for (size_t b = 0; b < map.bucket_count(); b++) {
        for (auto k : keys) {
            if (map.find(k, b) != map.end()) fp++;
        }
    }
    CHECK(fp == 0, "no false positives for boundary keys after erase");
}

int main()
{
    printf("=== EMH_FIND_HIT extreme bug fix test ===\n");
    printf("INACTIVE = 0x%llx, sizeof(size_type) = %zu\n",
           (unsigned long long)emhash5::INACTIVE, sizeof(emhash5::size_type));

    test_int_type<int8_t>("int8_t");
    test_int_type<int16_t>("int16_t");
    test_int_type<int32_t>("int32_t");
    test_int_type<int64_t>("int64_t");
    test_int_type<uint8_t>("uint8_t");
    test_int_type<uint16_t>("uint16_t");
    test_int_type<uint32_t>("uint32_t");
    test_int_type<uint64_t>("uint64_t");

    test_string_key();
    test_extreme_sizes();
    test_alternating_ops();
    test_inactive_boundary();

    printf("\n=== Results: %d/%d passed ===\n", passed, total);
    if (passed != total) {
        printf("*** SOME TESTS FAILED ***\n");
        return 1;
    }
    printf("*** ALL TESTS PASSED ***\n");
    return 0;
}
