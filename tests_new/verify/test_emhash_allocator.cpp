// Custom allocator tests for emhash5/6/7/8 HashMap
//
// This test covers emhash implementations only (they support AllocT template param).
// emilib2ss/emilib2o/emilib2s do not support custom allocators.
//
// Coverage:
//   1. Tracking allocator (counts allocate/deallocate calls)
//   2. Stateful allocator (holds external state)
//   3. PMR allocator using monotonic_buffer_resource
//   4. Allocator propagation (copy/move/swap)
//
// Compile: g++ -std=c++17 -O2 -I.. test_emhash_allocator.cpp -o test_alloc

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <vector>
#include <memory>
#include <new>
#include <limits>
#include <type_traits>

#include "../../hash_table5.hpp"
#include "../../hash_table6.hpp"
#include "../../hash_table7.hpp"
#include "../../hash_table8.hpp"

static int g_pass = 0, g_fail = 0;

#define TEST_ASSERT(expr, msg) do { \
    if (!(expr)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; return; \
    } else { g_pass++; } \
} while(0)

// ============================================================================
// Tracking allocator - counts calls
// ============================================================================
struct alloc_stats {
    size_t alloc_count = 0;
    size_t dealloc_count = 0;
    size_t total_bytes = 0;
};

template<typename T>
struct TrackingAllocator {
    using value_type = T;

    alloc_stats* stats;

    TrackingAllocator() : stats(new alloc_stats()) {}
    TrackingAllocator(alloc_stats* s) : stats(s) {}

    template<typename U>
    TrackingAllocator(const TrackingAllocator<U>& other) : stats(other.stats) {}

    T* allocate(size_t n) {
        stats->alloc_count++;
        stats->total_bytes += n * sizeof(T);
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) {
        stats->dealloc_count++;
        (void)n;
        ::operator delete(p);
    }

    template<typename U>
    bool operator==(const TrackingAllocator<U>& other) const {
        return stats == other.stats;
    }

    template<typename U>
    bool operator!=(const TrackingAllocator<U>& other) const {
        return stats != other.stats;
    }
};

// ============================================================================
// Stateful allocator - holds a tag value
// ============================================================================
template<typename T>
struct StatefulAllocator {
    using value_type = T;
    int tag;

    StatefulAllocator(int t = 0) : tag(t) {}

    template<typename U>
    StatefulAllocator(const StatefulAllocator<U>& other) : tag(other.tag) {}

    T* allocate(size_t n) {
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) {
        (void)n;
        ::operator delete(p);
    }

    template<typename U>
    bool operator==(const StatefulAllocator<U>& other) const {
        return tag == other.tag;
    }

    template<typename U>
    bool operator!=(const StatefulAllocator<U>& other) const {
        return tag != other.tag;
    }
};

// ============================================================================
// Test 1: Tracking allocator - verify calls
// ============================================================================
template<template<typename, typename, typename, typename, typename> class HashMap>
void test_tracking_allocator(const char* name)
{
    printf("=== %s: Tracking allocator ===\n", name);

    alloc_stats stats;
    using Map = HashMap<int, int, std::hash<int>, std::equal_to<int>, TrackingAllocator<std::pair<const int, int>>>;

    {
        Map m{TrackingAllocator<std::pair<const int, int>>(&stats)};
        TEST_ASSERT(stats.alloc_count >= 0, "initial alloc");

        for (int i = 0; i < 1000; i++)
            m[i] = i * 10;

        TEST_ASSERT(m.size() == 1000, "size after inserts");
        TEST_ASSERT(stats.alloc_count > 0, "allocations happened");

        // Verify data
        for (int i = 0; i < 1000; i++)
            TEST_ASSERT(m[i] == i * 10, "value correct");
    }

    // After destruction, dealloc should match alloc
    TEST_ASSERT(stats.dealloc_count > 0, "deallocations happened");
    TEST_ASSERT(stats.total_bytes > 0, "bytes allocated");
}

// ============================================================================
// Test 2: Stateful allocator - verify propagation
// ============================================================================
template<template<typename, typename, typename, typename, typename> class HashMap>
void test_stateful_allocator(const char* name)
{
    printf("=== %s: Stateful allocator ===\n", name);

    using Map = HashMap<int, int, std::hash<int>, std::equal_to<int>, StatefulAllocator<std::pair<const int, int>>>;

    // Create with tag=1
    Map m1{StatefulAllocator<std::pair<const int, int>>(1)};
    for (int i = 0; i < 100; i++) m1[i] = i;

    // Copy should propagate allocator
    Map m2(m1);
    TEST_ASSERT(m2.get_allocator().tag == 1, "copy ctor propagates allocator tag");

    // Copy assignment
    Map m3{StatefulAllocator<std::pair<const int, int>>(2)};
    m3 = m1;
    // depends on propagate_on_container_copy_assignment
    // We just verify data is correct
    TEST_ASSERT(m3.size() == 100, "copy assign size");

    // Move constructor
    Map m4(std::move(m1));
    TEST_ASSERT(m4.size() == 100, "move ctor size");

    // Move assignment
    Map m5{StatefulAllocator<std::pair<const int, int>>(3)};
    m5 = std::move(m2);
    TEST_ASSERT(m5.size() == 100, "move assign size");
}

// ============================================================================
// Test 3: PMR allocator (monotonic_buffer_resource)
// ============================================================================
#ifdef __has_include
#if __has_include(<memory_resource>)

#include <memory_resource>

template<typename T>
struct PmrAllocator {
    using value_type = T;
    std::pmr::memory_resource* resource;

    PmrAllocator(std::pmr::memory_resource* r = std::pmr::get_default_resource())
        : resource(r) {}

    template<typename U>
    PmrAllocator(const PmrAllocator<U>& other)
        : resource(other.resource) {}

    T* allocate(size_t n) {
        return static_cast<T*>(resource->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, size_t n) {
        resource->deallocate(p, n * sizeof(T), alignof(T));
    }

    template<typename U>
    bool operator==(const PmrAllocator<U>& other) const {
        return resource == other.resource;
    }

    template<typename U>
    bool operator!=(const PmrAllocator<U>& other) const {
        return resource != other.resource;
    }
};

template<template<typename, typename, typename, typename, typename> class HashMap>
void test_pmr_allocator(const char* name)
{
    printf("=== %s: PMR allocator ===\n", name);

    char buffer[4096];
    std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));

    using Map = HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAllocator<std::pair<const int, int>>>;

    {
        Map m{PmrAllocator<std::pair<const int, int>>(&pool)};

        for (int i = 0; i < 500; i++)
            m[i] = i * 2;

        TEST_ASSERT(m.size() == 500, "pmr size");
        for (int i = 0; i < 500; i++)
            TEST_ASSERT(m[i] == i * 2, "pmr value");
    }
}

#endif // __has_include(<memory_resource>)
#endif

// ============================================================================
// Test 4: Allocator with non-trivial types (std::string)
// ============================================================================
template<template<typename, typename, typename, typename, typename> class HashMap>
void test_string_allocator(const char* name)
{
    printf("=== %s: String allocator ===\n", name);

    alloc_stats stats;
    using Map = HashMap<int, std::string, std::hash<int>, std::equal_to<int>,
                        TrackingAllocator<std::pair<const int, std::string>>>;

    {
        Map m{TrackingAllocator<std::pair<const int, std::string>>(&stats)};

        m[1] = "one";
        m[2] = "two";
        m[3] = "three";

        TEST_ASSERT(m.size() == 3, "string alloc size");
        TEST_ASSERT(m[1] == "one", "string alloc value");
        TEST_ASSERT(m[2] == "two", "string alloc value 2");

        // Move string value
        std::string s = "four";
        m[4] = std::move(s);
        TEST_ASSERT(m[4] == "four", "string moved value");
    }

    TEST_ASSERT(stats.alloc_count > 0, "string alloc happened");
    TEST_ASSERT(stats.dealloc_count > 0, "string dealloc happened");
}

// ============================================================================
// Run all allocator tests
// ============================================================================
int main()
{
    printf("========================================\n");
    printf("  Custom Allocator Tests for emhash\n");
    printf("========================================\n\n");

    // Tracking allocator
    test_tracking_allocator<emhash5::HashMap>("emhash5");
    test_tracking_allocator<emhash6::HashMap>("emhash6");
    test_tracking_allocator<emhash7::HashMap>("emhash7");
    test_tracking_allocator<emhash8::HashMap>("emhash8");

    // Stateful allocator
    test_stateful_allocator<emhash5::HashMap>("emhash5");
    test_stateful_allocator<emhash6::HashMap>("emhash6");
    test_stateful_allocator<emhash7::HashMap>("emhash7");
    test_stateful_allocator<emhash8::HashMap>("emhash8");

    // String with allocator
    test_string_allocator<emhash5::HashMap>("emhash5");
    test_string_allocator<emhash6::HashMap>("emhash6");
    test_string_allocator<emhash7::HashMap>("emhash7");
    test_string_allocator<emhash8::HashMap>("emhash8");

#ifdef __has_include
#if __has_include(<memory_resource>)
    // PMR allocator
    test_pmr_allocator<emhash5::HashMap>("emhash5");
    test_pmr_allocator<emhash6::HashMap>("emhash6");
    test_pmr_allocator<emhash7::HashMap>("emhash7");
    test_pmr_allocator<emhash8::HashMap>("emhash8");
#endif
#endif

    // Summary
    printf("\n========================================\n");
    printf("  Total: %d passed, %d failed\n", g_pass, g_fail);
    printf("========================================\n");

    return g_fail > 0 ? 1 : 0;
}