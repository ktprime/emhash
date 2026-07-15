// unit/test_emihmap4.cpp
// Comprehensive correctness test for emilib4::HashMap (swiss table design).
// Validates: CRUD, iterators, copy/move, rehash, string keys, edge cases, erase_if, etc.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "emilib/emihmap4.hpp"
#include "emilib/emihmap2.hpp"
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <random>

using MapII = emilib4::HashMap<int, int>;
using MapSI = emilib4::HashMap<std::string, int>;
using MapI64 = emilib4::HashMap<int64_t, int64_t>;

// ─── 1. Empty state ───────────────────────────────────────────────────

TEST_CASE("empty state") {
    MapII m;
    CHECK(m.empty());
    CHECK(m.size() == 0);
    CHECK(m.begin() == m.end());
    CHECK(m.cbegin() == m.cend());
    CHECK(m.find(1) == m.end());
    CHECK(!m.contains(1));
    CHECK(m.count(1) == 0);
}

// ─── 2. Basic CRUD ────────────────────────────────────────────────────

TEST_CASE("operator[] insert and read") {
    MapII m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;
    CHECK(m.size() == 3);
    CHECK(m[1] == 10);
    CHECK(m[2] == 20);
    CHECK(m[3] == 30);
}

TEST_CASE("operator[] overwrite") {
    MapII m;
    m[1] = 10;
    m[1] = 100;
    CHECK(m.size() == 1);
    CHECK(m[1] == 100);
}

TEST_CASE("operator[] default construct") {
    MapII m;
    m[1]; // default-constructs value (0 for int)
    CHECK(m.size() == 1);
    CHECK(m[1] == 0);
}

TEST_CASE("insert pair") {
    MapII m;
    auto [it1, ok1] = m.insert({1, 10});
    CHECK(ok1);
    CHECK(it1->first == 1);
    CHECK(it1->second == 10);

    auto [it2, ok2] = m.insert({1, 20}); // duplicate
    CHECK(!ok2);
    CHECK(it2->second == 10); // original unchanged
}

TEST_CASE("emplace") {
    MapII m;
    auto [it, ok] = m.emplace(5, 50);
    CHECK(ok);
    CHECK(it->first == 5);
    CHECK(it->second == 50);
}

TEST_CASE("try_emplace") {
    MapII m;
    m[1] = 10;
    auto [it1, ok1] = m.try_emplace(1, 999);
    CHECK(!ok1);
    CHECK(it1->second == 10);

    auto [it2, ok2] = m.try_emplace(2, 20);
    CHECK(ok2);
    CHECK(it2->second == 20);
}

TEST_CASE("at() read and modify") {
    MapII m;
    m[1] = 10;
    CHECK(m.at(1) == 10);
    m.at(1) = 100;
    CHECK(m.at(1) == 100);
}

TEST_CASE("at() throws for missing key") {
    MapII m;
    CHECK_THROWS_AS(m.at(999), std::out_of_range);
}

TEST_CASE("contains and count") {
    MapII m;
    m[1] = 10;
    CHECK(m.contains(1));
    CHECK(!m.contains(2));
    CHECK(m.count(1) == 1);
    CHECK(m.count(2) == 0);
}

TEST_CASE("find") {
    MapII m;
    m[1] = 10;
    auto it = m.find(1);
    REQUIRE(it != m.end());
    CHECK(it->first == 1);
    CHECK(it->second == 10);
    CHECK(m.find(2) == m.end());
}

// ─── 3. Erase ─────────────────────────────────────────────────────────

TEST_CASE("erase by key") {
    MapII m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;
    CHECK(m.erase(2) == 1);
    CHECK(m.size() == 2);
    CHECK(!m.contains(2));
    CHECK(m.erase(999) == 0);
}

TEST_CASE("erase by iterator") {
    MapII m;
    m[1] = 10;
    m[2] = 20;
    auto it = m.find(1);
    REQUIRE(it != m.end());
    m.erase(it);
    CHECK(!m.contains(1));
    CHECK(m.size() == 1);
}

TEST_CASE("erase_if") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i;
    auto erased = m.erase_if([](auto& p) { return p.second % 2 == 0; });
    CHECK(erased == 50);
    CHECK(m.size() == 50);
    for (auto& [k, v] : m)
        CHECK(v % 2 != 0);
}

TEST_CASE("erase all then reinsert") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i;
    for (int i = 0; i < 100; i++)
        m.erase(i);
    CHECK(m.empty());
    CHECK(m.size() == 0);
    // Reinsert after full erase
    m[42] = 420;
    CHECK(m.size() == 1);
    CHECK(m[42] == 420);
}

// ─── 4. Iterators ─────────────────────────────────────────────────────

TEST_CASE("iterate all elements") {
    MapII m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;
    int sum_v = 0, count = 0;
    for (auto& [k, v] : m) {
        sum_v += v;
        count++;
    }
    CHECK(count == 3);
    CHECK(sum_v == 60);
}

TEST_CASE("const iterator") {
    MapII m;
    m[1] = 10;
    const auto& cm = m;
    int count = 0;
    for (auto it = cm.cbegin(); it != cm.cend(); ++it)
        count++;
    CHECK(count == 1);
}

TEST_CASE("empty iteration") {
    MapII m;
    int count = 0;
    for (auto& [k, v] : m) {
        (void)k;
        (void)v;
        count++;
    }
    CHECK(count == 0);
}

// ─── 5. Copy / Move ───────────────────────────────────────────────────

TEST_CASE("copy constructor") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i * 10;
    MapII m2(m);
    CHECK(m2.size() == 100);
    for (int i = 0; i < 100; i++)
        CHECK(m2[i] == i * 10);
    // Original unchanged
    CHECK(m.size() == 100);
}

TEST_CASE("copy assignment") {
    MapII m;
    for (int i = 0; i < 50; i++)
        m[i] = i;
    MapII m2;
    m2 = m;
    CHECK(m2.size() == 50);
    CHECK(m2[49] == 49);
}

TEST_CASE("move constructor") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i * 10;
    size_t old_size = m.size();
    MapII m2(std::move(m));
    CHECK(m2.size() == old_size);
    CHECK(m2[50] == 500);
}

TEST_CASE("move assignment") {
    MapII m;
    for (int i = 0; i < 50; i++)
        m[i] = i;
    MapII m2;
    m2 = std::move(m);
    CHECK(m2.size() == 50);
}

TEST_CASE("self-assignment guard") {
    MapII m;
    m[1] = 10;
    m = m; // self-assignment
    CHECK(m.size() == 1);
    CHECK(m[1] == 10);
}

// ─── 6. Rehash / Reserve / Clear / Shrink ─────────────────────────────

TEST_CASE("reserve") {
    MapII m;
    m.reserve(10000);
    CHECK(m.bucket_count() >= 10000);
    for (int i = 0; i < 10000; i++)
        m[i] = i;
    CHECK(m.size() == 10000);
}

TEST_CASE("clear") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i;
    m.clear();
    CHECK(m.empty());
    CHECK(m.size() == 0);
    // Can reuse after clear
    m[42] = 420;
    CHECK(m[42] == 420);
}

TEST_CASE("shrink_to_fit") {
    MapII m;
    for (int i = 0; i < 1000; i++)
        m[i] = i;
    for (int i = 0; i < 900; i++)
        m.erase(i);
    m.shrink_to_fit();
    CHECK(m.size() == 100);
    CHECK(m.bucket_count() <= 500); // reasonable bound
}

// ─── 7. String keys ───────────────────────────────────────────────────

TEST_CASE("string key CRUD") {
    MapSI m;
    m["hello"] = 1;
    m["world"] = 2;
    CHECK(m.size() == 2);
    CHECK(m["hello"] == 1);
    CHECK(m.contains("world"));
    m.erase("hello");
    CHECK(!m.contains("hello"));
}

TEST_CASE("string key copy/move") {
    MapSI m1;
    m1["abc"] = 1;
    m1["def"] = 2;
    MapSI m2 = m1;
    CHECK(m2["abc"] == 1);
    MapSI m3 = std::move(m1);
    CHECK(m3["def"] == 2);
}

TEST_CASE("string key iteration") {
    MapSI m;
    m["a"] = 1;
    m["b"] = 2;
    m["c"] = 3;
    int sum = 0;
    for (auto& [k, v] : m)
        sum += v;
    CHECK(sum == 6);
}

// ─── 8. Edge cases ────────────────────────────────────────────────────

TEST_CASE("single element") {
    MapII m;
    m[1] = 42;
    CHECK(m.size() == 1);
    CHECK(m[1] == 42);
    CHECK(m.contains(1));
    m.erase(1);
    CHECK(m.empty());
}

TEST_CASE("erase during iteration via erase_if") {
    MapII m;
    for (int i = 0; i < 50; i++)
        m[i] = i;
    m.erase_if([](auto& p) { return p.first < 25; });
    CHECK(m.size() == 25);
}

TEST_CASE("large scale insert find erase") {
    MapI64 m;
    const int N = 100000;
    for (int i = 0; i < N; i++)
        m[i] = i * 10;
    CHECK(m.size() == N);
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        REQUIRE(it != m.end());
        CHECK(it->second == i * 10);
    }
    for (int i = 0; i < N; i++)
        CHECK(m.erase(i) == 1);
    CHECK(m.empty());
}

// ─── 9. Random key stress test ────────────────────────────────────────

TEST_CASE("random int64 key stress") {
    MapI64 m;
    std::mt19937_64 rng(42);
    std::vector<int64_t> keys;
    const int N = 50000;
    for (int i = 0; i < N; i++) {
        auto k = static_cast<int64_t>(rng());
        keys.push_back(k);
        m[k] = k;
    }
    // Verify all keys
    for (auto k : keys) {
        auto it = m.find(k);
        REQUIRE(it != m.end());
        CHECK(it->second == k);
    }
    // Erase all
    for (auto k : keys)
        CHECK(m.erase(k) == 1);
    CHECK(m.empty());
}

TEST_CASE("random string key stress") {
    MapSI m;
    std::mt19937 rng(123);
    std::vector<std::string> keys;
    const int N = 10000;
    for (int i = 0; i < N; i++) {
        auto s = "key_" + std::to_string(rng());
        keys.push_back(s);
        m[s] = i;
    }
    for (auto& k : keys)
        CHECK(m.contains(k));
    for (auto& k : keys)
        m.erase(k);
    CHECK(m.empty());
}

// ─── 10. Insert with different hash functions ─────────────────────────

struct IdentityHash {
    size_t operator()(int k) const { return static_cast<size_t>(k); }
};

struct BadModHash {
    size_t operator()(int k) const { return k % 7; }
};

TEST_CASE("identity hash (worst case for low-bit positioning)") {
    emilib4::HashMap<int, int, IdentityHash> m;
    for (int i = 0; i < 1000; i++)
        m[i] = i;
    CHECK(m.size() == 1000);
    for (int i = 0; i < 1000; i++)
        CHECK(m[i] == i);
    for (int i = 0; i < 1000; i++)
        m.erase(i);
    CHECK(m.empty());
}

TEST_CASE("bad mod hash") {
    emilib4::HashMap<int, int, BadModHash> m;
    for (int i = 0; i < 1000; i++)
        m[i] = i;
    CHECK(m.size() == 1000);
    for (int i = 0; i < 1000; i++)
        CHECK(m.contains(i));
}

// ─── 11. operator== / != ──────────────────────────────────────────────

TEST_CASE("equality") {
    MapII m1, m2;
    m1[1] = 10;
    m1[2] = 20;
    m2[1] = 10;
    m2[2] = 20;
    CHECK(m1 == m2);

    m2[3] = 30;
    CHECK(m1 != m2);
}

// ─── 12. Initializer list / range constructor ─────────────────────────

TEST_CASE("initializer_list constructor") {
    MapII m{{1, 10}, {2, 20}, {3, 30}};
    CHECK(m.size() == 3);
    CHECK(m[2] == 20);
}

TEST_CASE("range constructor") {
    std::vector<std::pair<int, int>> v = {{1, 10}, {2, 20}};
    MapII m(v.begin(), v.end());
    CHECK(m.size() == 2);
    CHECK(m[1] == 10);
}

TEST_CASE("insert initializer_list") {
    MapII m;
    m.insert({{1, 10}, {2, 20}});
    CHECK(m.size() == 2);
}

TEST_CASE("insert range") {
    MapII m;
    std::vector<std::pair<int, int>> v = {{3, 30}, {4, 40}};
    m.insert(v.begin(), v.end());
    CHECK(m[3] == 30);
}

// ─── 13. Swap ─────────────────────────────────────────────────────────

TEST_CASE("swap") {
    MapII m1, m2;
    m1[1] = 10;
    m2[2] = 20;
    m2[3] = 30;
    m1.swap(m2);
    CHECK(m1.size() == 2);
    CHECK(m2.size() == 1);
    CHECK(m1[2] == 20);
    CHECK(m2[1] == 10);
}

// ─── 14. Rehash preserves all elements ────────────────────────────────

TEST_CASE("rehash preserves elements") {
    MapII m;
    for (int i = 0; i < 200; i++)
        m[i] = i * 3;
    m.reserve(100000);
    CHECK(m.size() == 200);
    for (int i = 0; i < 200; i++)
        CHECK(m[i] == i * 3);
}

// ─── 15. Mixed erase + insert pattern ─────────────────────────────────

TEST_CASE("erase and reinsert pattern") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i;
    // Erase odd keys
    for (int i = 1; i < 100; i += 2)
        m.erase(i);
    CHECK(m.size() == 50);
    // Reinsert
    for (int i = 1; i < 100; i += 2)
        m[i] = i * 10;
    CHECK(m.size() == 100);
    for (int i = 0; i < 100; i++) {
        if (i % 2 == 0)
            CHECK(m[i] == i);
        else
            CHECK(m[i] == i * 10);
    }
}

// ─── 16. Cross-implementation consistency ─────────────────────────────

TEST_CASE("cross-implementation consistency with emihmap2") {
    emilib2::HashMap<int, int> ref;
    MapII m;
    std::mt19937 rng(42);
    for (int i = 0; i < 5000; i++) {
        int k = rng() % 1000;
        int v = rng();
        switch (rng() % 4) {
        case 0:
            ref[k] = v;
            m[k] = v;
            break;
        case 1:
            ref.erase(k);
            m.erase(k);
            break;
        case 2:
            CHECK(m.contains(k) == ref.contains(k));
            break;
        case 3: {
            auto a = m.find(k) != m.end();
            auto b = ref.find(k) != ref.end();
            CHECK(a == b);
            break;
        }
        }
    }
    CHECK(m.size() == ref.size());
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        REQUIRE(it != m.end());
        CHECK(it->second == v);
    }
}

// ─── 17. insert_or_assign ──────────────────────────────────────────────

TEST_CASE("insert_or_assign inserts new key") {
    MapII m;
    auto [it, ok] = m.insert_or_assign(1, 10);
    CHECK(ok);
    CHECK(it->first == 1);
    CHECK(it->second == 10);
}

TEST_CASE("insert_or_assign overwrites existing key") {
    MapII m;
    m[1] = 10;
    auto [it, ok] = m.insert_or_assign(1, 99);
    CHECK(!ok);
    CHECK(it->second == 99);
    CHECK(m[1] == 99);
}

TEST_CASE("insert_or_assign with move key") {
    MapSI m;
    std::string key = "hello";
    m.insert_or_assign(std::move(key), 42);
    CHECK(m["hello"] == 42);
}

// ─── 18. merge ────────────────────────────────────────────────────────

TEST_CASE("merge from another map") {
    MapII src;
    src[1] = 10;
    src[2] = 20;
    MapII dst;
    dst[3] = 30;
    dst.merge(src);
    CHECK(dst.size() == 3);
    CHECK(dst[1] == 10);
    CHECK(src.empty()); // merged-out keys erased from src
}

TEST_CASE("self merge safety") {
    MapII m;
    m[1] = 10;
    m[2] = 20;
    m.merge(m); // self-merge: all keys exist, nothing should change
    CHECK(m.size() == 2);
    CHECK(m[1] == 10);
    CHECK(m[2] == 20);
}

// ─── 19. max_load_factor API ──────────────────────────────────────────

TEST_CASE("max_load_factor is no-op but callable") {
    MapII m;
    // Swiss table has fixed load factor; setting is a no-op for API compat
    float old_mlf = m.max_load_factor();
    m.max_load_factor(0.5f);
    CHECK(m.max_load_factor() == old_mlf); // unchanged
}

// ─── 20. rehash(0) edge case ──────────────────────────────────────────

TEST_CASE("rehash(0) after erase") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i;
    for (int i = 0; i < 90; i++)
        m.erase(i);
    m.rehash(0);
    CHECK(m.size() == 10);
    for (int i = 90; i < 100; i++)
        CHECK(m[i] == i);
}

// ─── 21. operator== with different insertion order ────────────────────

TEST_CASE("equality with different insertion order") {
    MapII m1, m2;
    for (int i = 0; i < 50; i++)
        m1[i] = i * 10;
    for (int i = 49; i >= 0; i--)
        m2[i] = i * 10;
    CHECK(m1 == m2);
}

TEST_CASE("inequality different values") {
    MapII m1, m2;
    m1[1] = 10;
    m2[1] = 20;
    CHECK(m1 != m2);
}

// ─── 22. Non-trivially-copyable value type ────────────────────────────

TEST_CASE("string value CRUD") {
    emilib4::HashMap<int, std::string> m;
    m[1] = "hello";
    m[2] = "world";
    CHECK(m[1] == "hello");
    CHECK(m[2] == "world");
    m[1] = "updated";
    CHECK(m[1] == "updated");
    m.erase(2);
    CHECK(!m.contains(2));
}

TEST_CASE("string value copy/move") {
    emilib4::HashMap<int, std::string> m1;
    m1[1] = "hello";
    m1[2] = "world";
    auto m2 = m1;
    CHECK(m2[1] == "hello");
    CHECK(m2.size() == 2);
    auto m3 = std::move(m1);
    CHECK(m3[2] == "world");
}

TEST_CASE("int key + string value copy/move") {
    emilib4::HashMap<int, std::string> m1;
    for (int i = 0; i < 100; i++)
        m1[i] = "val_" + std::to_string(i);
    auto m2(m1);
    CHECK(m2.size() == 100);
    CHECK(m2[50] == "val_50");
    auto m3 = std::move(m2);
    CHECK(m3.size() == 100);
    CHECK(m3[99] == "val_99");
}

// ─── 23. Size sweep (1–32 elements) ──────────────────────────────────

TEST_CASE("size sweep: int keys") {
    for (int sz : {1, 2, 3, 5, 7, 8, 9, 15, 16, 17, 31, 32}) {
        MapII m;
        for (int i = 0; i < sz; i++)
            m[i] = i * 10;
        CHECK(m.size() == static_cast<size_t>(sz));
        for (int i = 0; i < sz; i++) {
            auto it = m.find(i);
            REQUIRE(it != m.end());
            CHECK(it->second == i * 10);
        }
        // erase all
        for (int i = 0; i < sz; i++)
            CHECK(m.erase(i) == 1);
        CHECK(m.empty());
    }
}

TEST_CASE("size sweep: string keys") {
    for (int sz : {1, 2, 3, 5, 7, 8, 9, 15, 16, 17, 31, 32}) {
        MapSI m;
        for (int i = 0; i < sz; i++)
            m["k" + std::to_string(i)] = i;
        CHECK(m.size() == static_cast<size_t>(sz));
        for (int i = 0; i < sz; i++)
            CHECK(m.contains("k" + std::to_string(i)));
        for (int i = 0; i < sz; i++)
            m.erase("k" + std::to_string(i));
        CHECK(m.empty());
    }
}

// ─── 24. Large struct value ───────────────────────────────────────────

struct LargeValue {
    int data[64];
    LargeValue() { memset(data, 0, sizeof(data)); }
    LargeValue(int v) {
        for (int i = 0; i < 64; i++)
            data[i] = v + i;
    }
    bool operator==(const LargeValue& o) const { return memcmp(data, o.data, sizeof(data)) == 0; }
};

TEST_CASE("large non-trivially-copyable value") {
    emilib4::HashMap<int, LargeValue> m;
    m[1] = LargeValue(100);
    m[2] = LargeValue(200);
    CHECK(m[1] == LargeValue(100));
    CHECK(m[2] == LargeValue(200));
    auto m2 = m;
    CHECK(m2[1] == LargeValue(100));
    CHECK(m2.size() == 2);
}

// ─── 25. Erase by iterator range (erase_if covers) ──────────────────

TEST_CASE("erase_if complex predicate") {
    MapII m;
    for (int i = 0; i < 200; i++)
        m[i] = i;
    // Erase keys in range [50, 150)
    auto erased = m.erase_if([](auto& p) { return p.first >= 50 && p.first < 150; });
    CHECK(erased == 100);
    CHECK(m.size() == 100);
    CHECK(m.contains(49));
    CHECK(!m.contains(50));
    CHECK(!m.contains(149));
    CHECK(m.contains(150));
}

// ─── 26. HashMap with custom key + custom hash ───────────────────────

struct PairKey {
    int a, b;
    bool operator==(const PairKey& o) const { return a == o.a && b == o.b; }
};

struct PairKeyHash {
    size_t operator()(const PairKey& k) const { return static_cast<size_t>(k.a) * 31 + k.b; }
};

TEST_CASE("custom key type with custom hash") {
    emilib4::HashMap<PairKey, int, PairKeyHash> m;
    m[{1, 2}] = 12;
    m[{3, 4}] = 34;
    CHECK(m[{1, 2}] == 12);
    CHECK(m.contains({3, 4}));
    CHECK(!m.contains({1, 3}));
}

// ─── 27. Million-scale stress ────────────────────────────────────────

TEST_CASE("million int CRUD stress") {
    MapII m;
    const int N = 1000000;
    for (int i = 0; i < N; i++)
        m[i] = i;
    CHECK(m.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; i++) {
        auto it = m.find(i);
        REQUIRE(it != m.end());
        CHECK(it->second == i);
    }
    for (int i = 0; i < N; i++)
        CHECK(m.erase(i) == 1);
    CHECK(m.empty());
}
