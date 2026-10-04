// unit/test_ht8_optimizations.cpp
// Correctness tests for the hash_table8.hpp optimizations borrowed from
// martinus/unordered_dense (Aug-Sep 2026):
//   1. shared empty-index sentinel + lazy allocation (default ctor: 0 allocations)
//   2. store-forwarding-friendly hash for 8/12/16-byte POD keys (EMH_POD_HASH)
//   3. ADL-findable swap
//   4. precomputed-hash lookups via hash() (find/contains/count/at/try_get/erase)
//
// Run under several build configs to cover the hash_key branches:
//   g++ -I include -I tests -I tests/common tests/unit/test_ht8_optimizations.cpp
//   ... plus -DEMH_POD_HASH=1 / -DEMH_INT_HASH=1 / -DEMH_WYHASH_HASH=1
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "emhash/hash_table8.hpp"

// ---------------------------------------------------------------------------
// fixed-size POD key types (byte-equality holds)
// ---------------------------------------------------------------------------
struct Coord12 { // 3 x int32, no padding
    int32_t x, y, z;
    bool operator==(const Coord12& r) const { return x == r.x && y == r.y && z == r.z; }
    bool operator!=(const Coord12& r) const { return !(*this == r); }
};
struct Coord16 { // 2 x uint64, no padding
    uint64_t a, b;
    bool operator==(const Coord16& r) const { return a == r.a && b == r.b; }
    bool operator!=(const Coord16& r) const { return !(*this == r); }
};
struct CoordHash { // works with and without EMH_POD_HASH (bypassed when =1)
    size_t operator()(const Coord12& c) const noexcept {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&c);
        uint64_t a, b;
        memcpy(&a, p, 8);
        memcpy(&b, p + 4, 8);
        a = a * UINT64_C(0x9E3779B97F4A7C15) ^ (b >> 31);
        a ^= a >> 33;
        a *= UINT64_C(0xff51afd7ed558ccd);
        a ^= a >> 33;
        return static_cast<size_t>(a);
    }
    size_t operator()(const Coord16& c) const noexcept {
        const uint64_t h = c.a * UINT64_C(0x9E3779B97F4A7C15) ^ c.b;
        auto x = h;
        x ^= x >> 33;
        x *= UINT64_C(0xff51afd7ed558ccd);
        x ^= x >> 33;
        return static_cast<size_t>(x);
    }
};

// ---------------------------------------------------------------------------
// allocation counting
// ---------------------------------------------------------------------------
static long g_allocs = 0;
static long g_frees = 0;

template <typename T> struct counting_allocator {
    using value_type = T;
    counting_allocator() = default;
    template <typename U> counting_allocator(const counting_allocator<U>&) noexcept {}
    T* allocate(size_t n) {
        ++g_allocs;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    void deallocate(T* p, size_t) noexcept {
        ++g_frees;
        ::operator delete(p);
    }
};
template <typename T, typename U> bool operator==(const counting_allocator<T>&, const counting_allocator<U>&) noexcept {
    return true;
}
template <typename T, typename U> bool operator!=(const counting_allocator<T>&, const counting_allocator<U>&) noexcept {
    return false;
}

using CountMap =
    emhash8::HashMap<int, int, std::hash<int>, std::equal_to<int>, counting_allocator<std::pair<int, int>>>;

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;
static uint64_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

// ---------------------------------------------------------------------------
// 1. empty-map sentinel state
// ---------------------------------------------------------------------------
TEST_CASE("empty map: sentinel lookups are safe and allocation-free") {
    g_allocs = g_frees = 0;
    {
        CountMap m;
        CHECK(m.empty());
        CHECK(m.bucket_count() == 0);
        CHECK(m.find(1) == m.end());
        CHECK_FALSE(m.contains(1));
        CHECK(m.count(1) == 0);
        CHECK(m.try_get(1) == nullptr);
        CHECK(m.erase(1) == 0);
        CHECK_THROWS_AS(m.at(1), std::out_of_range);
        CHECK(g_allocs == 0); // the lazy-allocation promise
    }
    CHECK(g_allocs == g_frees);

    // first insert allocates and the map behaves normally afterwards
    CountMap m;
    m[42] = 7;
    CHECK(m.bucket_count() >= 4);
    CHECK(m.at(42) == 7);
    CHECK(m.contains(42));
    CHECK(m.size() == 1);

    // clear + reuse
    m.clear();
    CHECK(m.empty());
    CHECK_FALSE(m.contains(42));
    m[43] = 8;
    CHECK(m.at(43) == 8);

    // reserve / shrink_to_fit around the empty state
    CountMap r;
    CHECK_NOTHROW(r.reserve(0));
    CHECK_NOTHROW(r.reserve(128));
    for (int i = 0; i < 200; ++i)
        r[i] = i;
    CHECK_NOTHROW(r.shrink_to_fit());
    CHECK(r.size() == 200);
    for (int i = 0; i < 200; ++i)
        CHECK(r.at(i) == i);
}

// ---------------------------------------------------------------------------
// 2. copy / move around the sentinel state
// ---------------------------------------------------------------------------
TEST_CASE("copy and move of empty maps are allocation-free") {
    CountMap e;
    g_allocs = g_frees = 0;
    {
        CountMap c = e; // copy of empty
        CHECK(c.empty());
        CHECK(c.bucket_count() == 0);
        CountMap mv = std::move(c); // move of empty
        CHECK(mv.empty());
    }
    CHECK(g_allocs == 0);
    CHECK(g_allocs == g_frees);
}

TEST_CASE("moves of non-empty maps steal without allocating") {
    CountMap src;
    for (int i = 0; i < 16; ++i)
        src[i] = i; // stable, load factor under the minimum
    g_allocs = g_frees = 0;
    {
        CountMap m2 = std::move(src); // move ctor steals: no allocation
        CHECK(m2.size() == 16);
        CHECK(m2.at(7) == 7);
        CHECK(src.empty());

        CountMap m3;
        m3 = std::move(m2); // move assignment swaps: no allocation
        CHECK(m3.size() == 16);
        CHECK(m2.empty());

        CountMap m4(std::move(m3), counting_allocator<std::pair<int, int>>());
        CHECK(m4.size() == 16);
        CHECK(m4.at(7) == 7);

        // the steal itself must not have allocated; the frees for memory
        // allocated before the reset land when m4's destructor runs below
        CHECK(g_allocs == 0);
    }

    src[9] = 90; // the moved-from map stays usable
    CHECK(src.at(9) == 90);
}

// ---------------------------------------------------------------------------
// 3. swap: member swap via ADL, including the std::swap resolution
// ---------------------------------------------------------------------------
TEST_CASE("swap via ADL and std::swap, allocation-free") {
    CountMap a, b;
    for (int i = 0; i < 100; ++i)
        a[i] = i;
    for (int i = 0; i < 10; ++i)
        b[i] = -i;

    g_allocs = 0;
    swap(a, b); // ADL finds the friend
    CHECK(a.size() == 10);
    CHECK(b.size() == 100);
    CHECK(a.at(5) == -5);
    CHECK(b.at(5) == 5);
    CHECK(g_allocs == 0);

    using std::swap;
    swap(a, b);
    CHECK(a.size() == 100);
    CHECK(b.size() == 10);
    CHECK(a.at(99) == 99);
    CHECK(g_allocs == 0);

    swap(a, a); // self swap
    CHECK(a.size() == 100);
    CHECK(a.at(0) == 0);

    // swap between a sentinel-state map and a real one
    CountMap empty_one;
    swap(a, empty_one);
    CHECK(a.empty());
    CHECK(empty_one.size() == 100);
    CHECK(empty_one.at(1) == 1);
    a[500] = 5;
    CHECK(a.at(500) == 5);
}

// ---------------------------------------------------------------------------
// 4. precomputed-hash lookups
// ---------------------------------------------------------------------------
TEST_CASE("hash() + precomputed-hash lookups agree with plain lookups") {
    emhash8::HashMap<std::string, int> m;
    for (int i = 0; i < 500; ++i)
        m.emplace("key_" + std::to_string(i), i);

    for (int i = 0; i < 500; ++i) {
        const std::string k = "key_" + std::to_string(i);
        const uint64_t h = m.hash(k);
        CAPTURE(i);
        CHECK(m.contains(k, h));
        CHECK(m.count(k, h) == 1);
        CHECK(m.find(k, h)->second == i);
        CHECK(m.at(k, h) == i);
        const int* pv = m.try_get(k, h);
        REQUIRE(pv != nullptr);
        CHECK(*pv == i);
        int out = 0;
        CHECK(m.try_get(k, out, h));
        CHECK(out == i);
    }

    SUBCASE("misses with a valid hash of an absent key") {
        const std::string absent = "definitely_absent";
        const uint64_t h = m.hash(absent);
        CHECK_FALSE(m.contains(absent, h));
        CHECK(m.count(absent, h) == 0);
        CHECK(m.find(absent, h) == m.end());
        CHECK(m.try_get(absent, h) == nullptr);
        CHECK_THROWS_AS(m.at(absent, h), std::out_of_range);
    }

    SUBCASE("erase with a precomputed hash") {
        const std::string k = "key_250";
        const uint64_t h = m.hash(k);
        CHECK(m.erase(k, h) == 1);
        CHECK(m.erase(k, h) == 0);
        CHECK_FALSE(m.contains(k));
        CHECK(m.size() == 499);
    }

    SUBCASE("wrong hash must not find the key (documented contract)") {
        const std::string k = "key_1";
        const uint64_t bad = m.hash(k) ^ 0xDEADBEEFDEADBEEFull;
        CHECK_FALSE(m.contains(k, bad));
    }

    SUBCASE("hash() agrees with the bucket the map uses") {
        const std::string k = "key_123";
        const auto it = m.find(k);
        const uint64_t h = m.hash(k);
        // the map's EMH_EQHASH fingerprint stores (hash & ~mask) with the slot;
        // cross-check via erase-by-hash then reinsert: element must be findable again
        const int old = it->second;
        CHECK(m.erase(k, h) == 1);
        m[k] = old;
        CHECK(m.at(k) == old);
    }
}

// ---------------------------------------------------------------------------
// 5. POD keys: write field by field, look up at once
// ---------------------------------------------------------------------------
TEST_CASE("12/16-byte POD keys survive write-then-lookup (store forwarding scenario)") {
    emhash8::HashMap<Coord12, int, CoordHash> m;
    for (int i = 0; i < 10000; ++i) {
        Coord12 c{i, i * 3, i * 7}; // three separate 4-byte stores
        m[c] = i;
    }
    CHECK(m.size() == 10000);
    for (int i = 0; i < 10000; ++i) {
        Coord12 c{i, i * 3, i * 7}; // written again right before the lookup
        auto it = m.find(c);
        REQUIRE(it != m.end());
        CHECK(it->second == i);
        CHECK(m.contains(c));
    }
    // miss path
    Coord12 miss{1, 3, 8};
    CHECK_FALSE(m.contains(miss));

    emhash8::HashMap<Coord16, int, CoordHash> m2;
    for (int i = 0; i < 5000; ++i) {
        Coord16 c{uint64_t(i), uint64_t(i) * 31};
        m2[c] = i;
    }
    for (int i = 0; i < 5000; ++i) {
        Coord16 c{uint64_t(i), uint64_t(i) * 31};
        CHECK(m2.contains(c));
    }
}

#if EMH_POD_HASH
TEST_CASE("EMH_POD_HASH: hasher-less map over fixed-size POD keys") {
    emhash8::HashMap<Coord12, int> m; // std::hash<Coord12> does not exist; the stub is used
    for (int i = 0; i < 1000; ++i) {
        Coord12 c{i, i * 5, i * 11};
        m[c] = i;
    }
    CHECK(m.size() == 1000);
    for (int i = 0; i < 1000; ++i) {
        Coord12 c{i, i * 5, i * 11};
        CHECK(m.at(c) == i);
    }
    CHECK(m.erase(Coord12{0, 0, 0}) == 1);
    CHECK(m.size() == 999);
}
#endif

// ---------------------------------------------------------------------------
// 6. randomized differential stress vs std::unordered_map (multi key types)
// ---------------------------------------------------------------------------
TEST_CASE("differential stress: int keys") {
    std::unordered_map<int, int> oracle;
    emhash8::HashMap<int, int> m;
    for (int op = 0; op < 60000; ++op) {
        const int k = (int)(rng() % 20000);
        const uint64_t r = rng();
        if (r % 3 == 0) {
            const int v = (int)(r >> 8);
            oracle[k] = v;
            m[k] = v;
        } else if (r % 3 == 1) {
            CHECK(m.erase(k) == oracle.erase(k));
        } else {
            auto it = m.find(k);
            auto oi = oracle.find(k);
            CHECK((it == m.end()) == (oi == oracle.end()));
            if (it != m.end())
                CHECK(it->second == oi->second);
        }
    }
    CHECK(m.size() == oracle.size());
    for (auto& kv : oracle) {
        auto it = m.find(kv.first);
        REQUIRE(it != m.end());
        CHECK(it->second == kv.second);
    }
}

TEST_CASE("differential stress: string keys and 12-byte POD keys") {
    std::unordered_map<std::string, int> oracle;
    emhash8::HashMap<std::string, int> m;
    for (int op = 0; op < 30000; ++op) {
        const auto k = "k" + std::to_string((rng() % 15000));
        const uint64_t r = rng();
        if (r % 3 == 0) {
            oracle[k] = (int)(r >> 8);
            m[k] = (int)(r >> 8);
        } else if (r % 3 == 1) {
            CHECK(m.erase(k) == oracle.erase(k));
        } else {
            CHECK(m.contains(k) == (oracle.count(k) != 0));
        }
    }
    CHECK(m.size() == oracle.size());

    // collision-heavy POD keys: hash depends on x only, forcing chain/kickout paths
    struct WeakHash {
        size_t operator()(const Coord12& c) const noexcept { return static_cast<size_t>(c.x); }
    };
    emhash8::HashMap<Coord12, int, WeakHash> w;
    for (int i = 0; i < 8000; ++i) {
        Coord12 c{i % 64, i, i * 3}; // 8000 keys over 64 hash values
        w[c] = i;
    }
    CHECK(w.size() == 8000);
    for (int i = 0; i < 8000; ++i) {
        Coord12 c{i % 64, i, i * 3};
        auto it = w.find(c);
        REQUIRE(it != w.end());
        CHECK(it->second == i);
    }
    for (int i = 0; i < 4000; ++i) {
        Coord12 c{i % 64, i * 2, i * 3};
        w.erase(c);
    }
    for (int i = 4000; i < 8000; ++i) {
        Coord12 c{i % 64, i, i * 3};
        CHECK(w.contains(c));
    }
}
