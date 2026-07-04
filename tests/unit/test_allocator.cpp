// unit/test_allocator.cpp
// Custom allocator support for emhash5/6/7/8 HashMap and emhash2/4/8 HashSet.
// emilib maps/sets do NOT support custom AllocT, so they are excluded.
// Covers: tracking allocator (call counts), stateful allocator (tag propagation),
//         PMR (monotonic_buffer_resource), string values with allocator, over-aligned values.
// Source: verify/test_emhash_allocator.cpp (321 lines) + verify/test_hashset_allocator.cpp (215 lines).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <memory>
#include <memory_resource>
#include <new>
#include <string>
#include <type_traits>
#include <cstdint>

// ============================================================================
// CountingAllocator: tracks allocate/deallocate calls via shared stats
// ============================================================================
struct AllocStats {
    int alloc_count = 0;
    int dealloc_count = 0;
    std::size_t total_bytes = 0;
    void reset() { alloc_count = dealloc_count = total_bytes = 0; }
};

template <typename T>
struct CountingAllocator {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    AllocStats* stats;

    CountingAllocator() : stats(nullptr) {}
    explicit CountingAllocator(AllocStats* s) : stats(s) {}

    template <typename U>
    CountingAllocator(const CountingAllocator<U>& o) noexcept : stats(o.stats) {}

    T* allocate(std::size_t n) {
        if (stats) { stats->alloc_count++; stats->total_bytes += n * sizeof(T); }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t n) {
        if (stats) { stats->dealloc_count++; (void)n; }
        ::operator delete(p);
    }

    template <typename U>
    bool operator==(const CountingAllocator<U>& o) const noexcept { return stats == o.stats; }
    template <typename U>
    bool operator!=(const CountingAllocator<U>& o) const noexcept { return stats != o.stats; }
};

// ============================================================================
// StatefulAllocator: carries a tag value to verify propagation
// ============================================================================
template <typename T>
struct StatefulAllocator {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    int tag;
    StatefulAllocator(int t = 0) : tag(t) {}
    template <typename U>
    StatefulAllocator(const StatefulAllocator<U>& o) noexcept : tag(o.tag) {}

    T* allocate(std::size_t n) { return static_cast<T*>(::operator new(n * sizeof(T))); }
    void deallocate(T* p, std::size_t) { ::operator delete(p); }

    template <typename U>
    bool operator==(const StatefulAllocator<U>& o) const noexcept { return tag == o.tag; }
    template <typename U>
    bool operator!=(const StatefulAllocator<U>& o) const noexcept { return tag != o.tag; }
};

// ============================================================================
// Helper: build a map type with a custom allocator
// ============================================================================
template <template <typename, typename, typename, typename, typename> class MapT,
          typename K, typename V, typename Alloc>
using MapWithAlloc = MapT<K, V, std::hash<K>, std::equal_to<K>, Alloc>;

// ============================================================================
// Tests: tracking allocator records allocations
// ============================================================================
TEST_CASE("tracking allocator: counts alloc/dealloc on emhash5-8") {
    auto run = [](auto map_template) { // NOLINT(performance-unnecessary-value-param)
        using Map = decltype(map_template);
        AllocStats stats;
        {
            Map m{typename Map::allocator_type{&stats}};
            for (int i = 0; i < 500; ++i) m[i] = i * 2;
            CHECK(m.size() == 500);
            CHECK(stats.alloc_count > 0);
            for (int i = 0; i < 500; ++i) CHECK(m[i] == i * 2);
        }
        CHECK(stats.dealloc_count > 0);
        CHECK(stats.alloc_count == stats.dealloc_count);
    };

    run(emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>,
                         CountingAllocator<std::pair<const int, int>>>{});
    run(emhash6::HashMap<int, int, std::hash<int>, std::equal_to<int>,
                         CountingAllocator<std::pair<const int, int>>>{});
    run(emhash7::HashMap<int, int, std::hash<int>, std::equal_to<int>,
                         CountingAllocator<std::pair<const int, int>>>{});
    run(emhash8::HashMap<int, int, std::hash<int>, std::equal_to<int>,
                         CountingAllocator<std::pair<const int, int>>>{});
}

TEST_CASE("stateful allocator: tag propagates on copy/move") {
    using Alloc = StatefulAllocator<std::pair<const int, int>>;
    using M5 = emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, Alloc>;

    M5 m1{Alloc{1}};
    for (int i = 0; i < 100; ++i) m1[i] = i;

    M5 m2(m1);
    CHECK(m2.get_allocator().tag == 1);
    CHECK(m2.size() == 100);

    M5 m3{Alloc{2}};
    m3 = m1;
    CHECK(m3.size() == 100);

    M5 m4(std::move(m1));
    CHECK(m4.size() == 100);

    M5 m5{Alloc{3}};
    m5 = std::move(m2);
    CHECK(m5.size() == 100);
}

TEST_CASE("PMR allocator: monotonic buffer works on emhash5") {
    char buffer[1 << 16];
    std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));
    using PmrAlloc = std::pmr::polymorphic_allocator<std::pair<const int, int>>;

    emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc>
        m(2, 0.8F, PmrAlloc{&pool});

    for (int i = 0; i < 1000; ++i) m[i] = i;
    CHECK(m.size() == 1000);
    CHECK(m[500] == 500);
    CHECK(m[999] == 999);
}

TEST_CASE("string value with tracking allocator") {
    using Alloc = CountingAllocator<std::pair<const int, std::string>>;
    using M8 = emhash8::HashMap<int, std::string, std::hash<int>, std::equal_to<int>, Alloc>;

    AllocStats stats;
    {
        M8 m{Alloc{&stats}};
        m[1] = "one";
        m[2] = "two";
        m[3] = "three";
        CHECK(m.size() == 3);
        CHECK(m[1] == "one");

        std::string s = "four";
        m[4] = std::move(s);
        CHECK(m[4] == "four");
    }
    CHECK(stats.alloc_count > 0);
    CHECK(stats.dealloc_count > 0);
}

TEST_CASE("over-aligned value: allocator respects alignment") {
    struct alignas(64) AlignedVal {
        int64_t data;
        bool operator==(const AlignedVal& o) const { return data == o.data; }
    };

    emhash5::HashMap<int, AlignedVal> m;
    for (int i = 0; i < 100; ++i) m[i] = AlignedVal{i};
    for (int i = 0; i < 100; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(reinterpret_cast<uintptr_t>(&it->second) % alignof(AlignedVal) == 0);
    }
}

// ============================================================================
// HashSet allocator tests (emhash2/4/8 only)
// ============================================================================
TEST_CASE("hash set with default allocator: CRUD + copy/move") {
    emhash2::HashSet<int> s2;
    emhash4::HashSet<int> s4;
    emhash8::HashSet<int> s8;

    for (int i = 0; i < 100; ++i) { s2.insert(i); s4.insert(i); s8.insert(i); }
    CHECK(s2.size() == 100);
    CHECK(s4.size() == 100);
    CHECK(s8.size() == 100);

    auto c2 = s2; CHECK(c2.size() == 100); CHECK(c2.contains(50));
    auto c4 = s4; CHECK(c4.size() == 100); CHECK(c4.contains(50));
    auto c8 = s8; CHECK(c8.size() == 100); CHECK(c8.contains(50));

    auto v8 = std::move(c8); CHECK(v8.size() == 100);
    auto v4 = std::move(c4); CHECK(v4.size() == 100);
    auto v2 = std::move(c2); CHECK(v2.size() == 100);
}

TEST_CASE("hash set PMR allocator (emhash8)") {
    using PmrAlloc = std::pmr::polymorphic_allocator<int>;

    char buffer[1 << 16];
    std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));

    emhash8::HashSet<int, std::hash<int>, std::equal_to<int>, PmrAlloc>
        s(2, 0.8F, PmrAlloc{&pool});

    for (int i = 0; i < 500; ++i) s.insert(i);
    CHECK(s.size() == 500);
    CHECK(s.contains(250));
}
