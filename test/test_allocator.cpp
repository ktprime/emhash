// Test custom allocator support for emhash HashMap/HashSet

#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table7.hpp"
#include "../hash_table8.hpp"
#include "../hash_set2.hpp"
#include "../hash_set3.hpp"
#include "../hash_set4.hpp"
#include "../hash_set8.hpp"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <memory_resource>

struct alignas(64) AlignedVal {
    int64_t data;
    bool operator==(const AlignedVal& o) const { return data == o.data; }
};

static void test_default_allocator()
{
    emhash5::HashMap<int, int> m5;
    emhash6::HashMap<int, int> m6;
    emhash7::HashMap<int, int> m7;
    emhash8::HashMap<int, int> m8;
    for (int i = 0; i < 100; i++) {
        m5[i] = i; m6[i] = i; m7[i] = i; m8[i] = i;
    }
    assert(m5.size() == 100);
    assert(m6.size() == 100);
    assert(m7.size() == 100);
    assert(m8.size() == 100);

    auto c5 = m5; assert(c5.size() == 100 && c5[50] == 50);
    auto c6 = m6; assert(c6.size() == 100 && c6[50] == 50);
    auto c7 = m7; assert(c7.size() == 100 && c7[50] == 50);
    auto c8 = m8; assert(c8.size() == 100 && c8[50] == 50);

    auto v5 = std::move(c5); assert(v5.size() == 100);
    auto v6 = std::move(c6); assert(v6.size() == 100);
    auto v7 = std::move(c7); assert(v7.size() == 100);
    auto v8 = std::move(c8); assert(v8.size() == 100);

    printf("test_default_allocator passed\n");
}

static void test_over_aligned()
{
    emhash5::HashMap<int, AlignedVal> m;
    for (int i = 0; i < 100; i++)
        m[i] = AlignedVal{i};
    for (int i = 0; i < 100; i++) {
        auto it = m.find(i);
        assert(it != m.end());
        assert(reinterpret_cast<uintptr_t>(&it->second) % alignof(AlignedVal) == 0);
    }
    printf("test_over_aligned passed (alignof=%zu)\n", alignof(AlignedVal));
}

static void test_hash_sets()
{
    emhash2::HashSet<int> s2;
    emhash7::HashSet<int> s3;
    emhash9::HashSet<int> s4;
    emhash8::HashSet<int> s8;
    for (int i = 0; i < 100; i++) {
        s2.insert(i); s3.insert(i); s4.insert(i); s8.insert(i);
    }
    assert(s2.size() == 100);
    assert(s3.size() == 100);
    assert(s4.size() == 100);
    assert(s8.size() == 100);

    auto c2 = s2; assert(c2.size() == 100);
    auto v2 = std::move(c2); assert(v2.size() == 100);

    printf("test_hash_sets passed\n");
}

static void test_pmr_allocator()
{
    using PmrAlloc = std::pmr::polymorphic_allocator<std::pair<int, int>>;

    // monotonic_buffer_resource
    {
        char buffer[1 << 20];
        std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));
        emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> m(2, 0.8f, PmrAlloc(&pool));
        for (int i = 0; i < 1000; i++)
            m[i] = i;
        assert(m.size() == 1000);
        assert(m[500] == 500);
    }

    // unsynchronized_pool_resource + copy
    {
        std::pmr::unsynchronized_pool_resource pool;
        emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> m(2, 0.8f, PmrAlloc(&pool));
        for (int i = 0; i < 1000; i++)
            m[i] = i;
        assert(m.size() == 1000);

        auto m2 = m;
        assert(m2.size() == 1000 && m2[999] == 999);
    }

    // emhash6
    {
        char buffer[1 << 20];
        std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));
        emhash6::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> m(2, 0.8f, PmrAlloc(&pool));
        for (int i = 0; i < 1000; i++)
            m[i] = i;
        assert(m.size() == 1000);
    }

    // emhash7
    {
        char buffer[1 << 20];
        std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));
        emhash7::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> m(2, 0.8f, PmrAlloc(&pool));
        for (int i = 0; i < 1000; i++)
            m[i] = i;
        assert(m.size() == 1000);
    }

    // emhash8
    {
        char buffer[1 << 20];
        std::pmr::monotonic_buffer_resource pool(buffer, sizeof(buffer));
        emhash8::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> m(2, 0.8f, PmrAlloc(&pool));
        for (int i = 0; i < 1000; i++)
            m[i] = i;
        assert(m.size() == 1000);
    }

    // rehash
    {
        std::pmr::unsynchronized_pool_resource pool;
        emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> m(2, 0.8f, PmrAlloc(&pool));
        for (int i = 0; i < 10000; i++)
            m[i] = i;
        assert(m.size() == 10000);
        m.rehash(m.size() * 4);
        for (int i = 0; i < 10000; i++)
            assert(m[i] == i);
    }

    printf("test_pmr_allocator passed\n");
}

static void test_pmr_allocator_propagation()
{
    std::pmr::unsynchronized_pool_resource pool_a;
    std::pmr::unsynchronized_pool_resource pool_b;
    using PmrAlloc = std::pmr::polymorphic_allocator<std::pair<int, int>>;

    // select_on_container_copy_construction: copy should use default_resource
    {
        emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> src(2, 0.8f, PmrAlloc(&pool_a));
        for (int i = 0; i < 100; i++) src[i] = i;

        auto copy = src;
        assert(copy.size() == 100 && copy[50] == 50);
        assert(copy.get_allocator().resource() == std::pmr::get_default_resource());
        assert(copy.get_allocator().resource() != &pool_a);
    }

    // propagate_on_container_copy_assignment: target keeps its allocator
    {
        emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> a(2, 0.8f, PmrAlloc(&pool_a));
        emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> b(2, 0.8f, PmrAlloc(&pool_b));
        for (int i = 0; i < 100; i++) a[i] = i;

        b = a;
        assert(b.size() == 100 && b[50] == 50);
        assert(b.get_allocator().resource() == &pool_b);
    }

    {
        emhash7::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> a(2, 0.8f, PmrAlloc(&pool_a));
        emhash7::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> b(2, 0.8f, PmrAlloc(&pool_b));
        for (int i = 0; i < 100; i++) a[i] = i;

        b = a;
        assert(b.size() == 100 && b[50] == 50);
        assert(b.get_allocator().resource() == &pool_b);
    }

    {
        emhash8::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> a(2, 0.8f, PmrAlloc(&pool_a));
        emhash8::HashMap<int, int, std::hash<int>, std::equal_to<int>, PmrAlloc> b(2, 0.8f, PmrAlloc(&pool_b));
        for (int i = 0; i < 100; i++) a[i] = i;

        b = a;
        assert(b.size() == 100 && b[50] == 50);
        assert(b.get_allocator().resource() == &pool_b);
    }

    printf("test_pmr_allocator_propagation passed\n");
}

int main()
{
    test_default_allocator();
    test_over_aligned();
    test_hash_sets();
    test_pmr_allocator();
    test_pmr_allocator_propagation();

    printf("\nAll allocator tests passed!\n");
    return 0;
}
