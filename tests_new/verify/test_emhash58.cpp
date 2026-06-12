// Comprehensive test for emhash5, emhash6, emhash7, emhash8 (int-int only)

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <utility>
#include <cassert>

#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table7.hpp"
#include "../hash_table8.hpp"

static int g_pass = 0, g_fail = 0;

#define TEST_ASSERT(expr) do { \
    if (!(expr)) { \
        printf("    FAIL at line %d\n", __LINE__); \
        g_fail++; return false; \
    } else { g_pass++; } \
} while(0)

#define RUN_TEST(fn) do { \
    if (fn()) printf("  [PASS] %s\n", #fn); \
    else      printf("  [FAIL] %s\n", #fn); \
} while(0)

struct BadHash {
    size_t operator()(int) const { return 42; }
};

template<typename MapType>
bool test_basic_crud()
{
    MapType map;
    map[1] = 10;
    map[2] = 20;
    map[3] = 30;
    TEST_ASSERT(map.size() == 3);
    TEST_ASSERT(map[1] == 10);
    TEST_ASSERT(map.count(1) == 1);
    TEST_ASSERT(map.count(999) == 0);
    
    auto it = map.find(2);
    TEST_ASSERT(it != map.end());
    TEST_ASSERT(it->second == 20);
    TEST_ASSERT(map.find(999) == map.end());
    
    TEST_ASSERT(map.contains(1));
    TEST_ASSERT(!map.contains(999));
    
    map.erase(2);
    TEST_ASSERT(map.size() == 2);
    TEST_ASSERT(map.count(2) == 0);
    
    map.insert_or_assign(1, 100);
    TEST_ASSERT(map[1] == 100);
    map.insert_or_assign(4, 40);
    TEST_ASSERT(map[4] == 40);
    
    auto res = map.emplace(5, 50);
    TEST_ASSERT(res.second);
    TEST_ASSERT(map[5] == 50);
    
    return true;
}

template<typename MapType>
bool test_copy_move()
{
    MapType map;
    for (int i = 0; i < 100; i++) map[i] = i * 10;
    
    MapType map2(map);
    TEST_ASSERT(map2.size() == map.size());
    TEST_ASSERT(map2[50] == 500);
    
    MapType map3;
    map3 = map;
    TEST_ASSERT(map3.size() == map.size());
    
    MapType map4(std::move(map2));
    TEST_ASSERT(map4.size() == 100);
    
    MapType map5;
    map5 = std::move(map3);
    TEST_ASSERT(map5.size() == 100);
    
    MapType map6;
    map6[999] = 1;
    map5.swap(map6);
    TEST_ASSERT(map5.size() == 1);
    TEST_ASSERT(map6.size() == 100);
    
    return true;
}

template<typename MapType>
bool test_iterator()
{
    MapType map;
    for (int i = 0; i < 50; i++) map[i] = i;
    
    int count = 0;
    for (auto& p : map) { count++; (void)p; }
    TEST_ASSERT(count == 50);
    
    const auto& cmap = map;
    count = 0;
    for (auto& p : cmap) { count++; (void)p; }
    TEST_ASSERT(count == 50);
    
    count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) count++;
    TEST_ASSERT(count == 50);
    
    return true;
}

template<typename MapType>
bool test_rehash_reserve_clear()
{
    MapType map;
    map.reserve(1000);
    TEST_ASSERT(map.bucket_count() >= 1000);
    
    for (int i = 0; i < 500; i++) map[i] = i;
    TEST_ASSERT(map.size() == 500);
    
    map.rehash(2000);
    TEST_ASSERT(map.size() == 500);
    TEST_ASSERT(map[100] == 100);
    
    map.clear();
    TEST_ASSERT(map.empty());
    TEST_ASSERT(map.size() == 0);
    
    return true;
}

template<typename MapType>
bool test_edge_cases()
{
    MapType map;
    TEST_ASSERT(map.empty());
    TEST_ASSERT(map.size() == 0);
    TEST_ASSERT(map.find(1) == map.end());
    TEST_ASSERT(map.count(1) == 0);
    
    map[42] = 100;
    TEST_ASSERT(map.size() == 1);
    TEST_ASSERT(!map.empty());
    TEST_ASSERT(map.contains(42));
    
    MapType map2;
    map2[0] = 1;
    TEST_ASSERT(map2[0] == 1);
    
    map2[-1] = 2;
    TEST_ASSERT(map2[-1] == 2);
    
    map2[1] = 10;
    auto res = map2.insert({1, 20});
    TEST_ASSERT(!res.second);
    TEST_ASSERT(map2[1] == 10);
    
    return true;
}

template<typename MapType>
bool test_bad_hash()
{
    MapType map;
    const int N = 200;
    for (int i = 0; i < N; i++) map[i] = i * 7;
    
    TEST_ASSERT((int)map.size() == N);
    for (int i = 0; i < N; i++) TEST_ASSERT(map[i] == i * 7);
    
    for (int i = 0; i < N / 2; i++) map.erase(i);
    TEST_ASSERT((int)map.size() == N / 2);
    
    for (int i = 0; i < N / 2; i++) map[i] = i * 11;
    TEST_ASSERT((int)map.size() == N);
    
    return true;
}

template<typename MapType>
bool test_size_sweep()
{
    int sizes[] = {1, 2, 3, 5, 8, 13, 16, 20, 32, 50, 64, 100, 128, 200, 256, 500, 1000};
    
    for (int N : sizes) {
        MapType map;
        for (int i = 0; i < N; i++) map[i] = i * 3;
        TEST_ASSERT((int)map.size() == N);
        for (int i = 0; i < N; i++) TEST_ASSERT(map[i] == i * 3);
    }
    
    return true;
}

template<typename MapType>
bool test_high_load()
{
    MapType map(1000, 0.99);
    for (int i = 0; i < 990; i++) map[i] = i;
    TEST_ASSERT(map.size() == 990);
    TEST_ASSERT(map.load_factor() > 0.9);
    
    return true;
}

template<typename MapType>
void run_all_tests(const char* name)
{
    printf("\n=== %s ===\n", name);
    RUN_TEST((test_basic_crud<MapType>));
    RUN_TEST((test_copy_move<MapType>));
    RUN_TEST((test_iterator<MapType>));
    RUN_TEST((test_rehash_reserve_clear<MapType>));
    RUN_TEST((test_edge_cases<MapType>));
    RUN_TEST((test_size_sweep<MapType>));
    RUN_TEST((test_high_load<MapType>));
}

int main()
{
    run_all_tests<emhash5::HashMap<int, int>>("emhash5::HashMap<int,int>");
    run_all_tests<emhash6::HashMap<int, int>>("emhash6::HashMap<int,int>");
    run_all_tests<emhash7::HashMap<int, int>>("emhash7::HashMap<int,int>");
    run_all_tests<emhash8::HashMap<int, int>>("emhash8::HashMap<int,int>");
    
    printf("\n=== Bad hash tests ===\n");
    RUN_TEST((test_bad_hash<emhash5::HashMap<int, int, BadHash>>));
    RUN_TEST((test_bad_hash<emhash6::HashMap<int, int, BadHash>>));
    RUN_TEST((test_bad_hash<emhash7::HashMap<int, int, BadHash>>));
    RUN_TEST((test_bad_hash<emhash8::HashMap<int, int, BadHash>>));
    
    printf("\n============================================================\n");
    printf("  Total assertions: %d\n", g_pass + g_fail);
    printf("  Passed: %d\n", g_pass);
    printf("  Failed: %d\n", g_fail);
    printf("============================================================\n");
    
    return g_fail > 0 ? 1 : 0;
}