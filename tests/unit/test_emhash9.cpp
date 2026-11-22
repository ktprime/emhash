// unit/test_emhash9.cpp
// Tests for emhash9 (group index): CRUD, differential fuzzing against
// std::unordered_map, churn, bad hashes, iterators, copy/move, allocators.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "emhash/hash_table9.hpp"

namespace {

struct constant_hash {
    size_t operator()(int) const noexcept { return 0x5bd1e995u; }
};

struct identity_hash {
    size_t operator()(uint64_t k) const noexcept { return static_cast<size_t>(k); }
};

using Map = emhash9::HashMap<int, int>;

} // namespace

// ---------------------------------------------------------------------------
// basic CRUD
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: basic crud") {
    Map m;
    CHECK(m.empty());
    CHECK(m.size() == 0);
    CHECK(m.find(1) == m.end());
    CHECK_FALSE(m.contains(1));
    CHECK(m.count(1) == 0);

    m[1] = 10;
    m[2] = 20;
    m.insert({3, 30});
    CHECK(m.size() == 3);
    CHECK(m[1] == 10);
    CHECK(m.at(2) == 20);
    CHECK(m.contains(3));
    CHECK(m.count(3) == 1);

    auto r = m.insert({3, 999});
    CHECK_FALSE(r.second);
    CHECK(m[3] == 30);

    auto r2 = m.insert_or_assign(3, 31);
    CHECK_FALSE(r2.second);
    CHECK(m[3] == 31);

    CHECK(m.erase(2) == 1);
    CHECK(m.erase(2) == 0);
    CHECK(m.find(2) == m.end());
    CHECK(m.size() == 2);

    m.clear();
    CHECK(m.empty());
    CHECK(m.begin() == m.end());
}

TEST_CASE("emhash9: at throws on missing key") {
    Map m;
    CHECK_THROWS_AS(m.at(42), std::out_of_range);
    m[42] = 1;
    CHECK_NOTHROW(m.at(42));
}

TEST_CASE("emhash9: try_emplace does not overwrite") {
    Map m;
    auto a = m.try_emplace(7, 70);
    CHECK(a.second);
    CHECK(a.first->second == 70);
    auto b = m.try_emplace(7, 71);
    CHECK_FALSE(b.second);
    CHECK(m[7] == 70);
}

TEST_CASE("emhash9: equal_range") {
    Map m;
    m[5] = 50;
    auto range = m.equal_range(5);
    CHECK(range.first != m.end());
    CHECK(range.second == range.first + 1);
    auto missing = m.equal_range(6);
    CHECK(missing.first == missing.second);
}

// ---------------------------------------------------------------------------
// growth / reserve / rehash
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: grows and keeps every element") {
    Map m;
    constexpr int N = 20000;
    for (int i = 0; i < N; ++i)
        m[i] = i * 3;
    CHECK(m.size() == static_cast<size_t>(N));
    CHECK(m.load_factor() <= m.max_load_factor() + 1e-6f);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        REQUIRE(it != m.end());
        CHECK(it->second == i * 3);
    }

    // erase every third key
    for (int i = 0; i < N; i += 3)
        CHECK(m.erase(i) == 1);
    CHECK(m.size() == static_cast<size_t>(N - (N + 2) / 3));
    for (int i = 0; i < N; ++i) {
        const bool expected = (i % 3) != 0;
        CHECK(m.contains(i) == expected);
    }
}

TEST_CASE("emhash9: reserve then rehash") {
    Map m;
    m.reserve(1000);
    const auto buckets = m.bucket_count();
    CHECK(buckets >= 1000);
    for (int i = 0; i < 900; ++i)
        m[i] = i;
    CHECK(m.bucket_count() == buckets); // no growth needed after reserve
    m.rehash(0);
    CHECK(m.size() == 900);
    for (int i = 0; i < 900; ++i)
        CHECK(m.at(i) == i);
}

// ---------------------------------------------------------------------------
// differential fuzzing against std::unordered_map
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: differential fuzz vs std::unordered_map") {
    std::mt19937 rng(1234567);
    emhash9::HashMap<int, int> m;
    std::unordered_map<int, int> ref;

    auto check_key = [&](int k) {
        auto a = m.find(k);
        auto b = ref.find(k);
        REQUIRE((a != m.end()) == (b != ref.end()));
        if (b != ref.end())
            CHECK(a->second == b->second);
    };

    for (int step = 0; step < 200000; ++step) {
        const int key = static_cast<int>(rng() % 4096);
        switch (rng() % 100) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7: {
            // insert / assign
            const int value = static_cast<int>(rng());
            m[key] = value;
            ref[key] = value;
            break;
        }
        case 8:
        case 9:
        case 10:
        case 11: {
            // erase
            const auto a = m.erase(key);
            const auto b = ref.erase(key);
            CHECK(a == b);
            break;
        }
        case 12:
        case 13: {
            // try_emplace
            const int value = static_cast<int>(rng());
            const auto a = m.try_emplace(key, value);
            const auto b = ref.try_emplace(key, value);
            CHECK(a.second == b.second);
            break;
        }
        case 14: {
            // iterator erase
            auto it = m.find(key);
            if (it != m.end()) {
                m.erase(it);
                ref.erase(key);
            }
            break;
        }
        case 15: {
            if (step % 5000 == 0)
                m.rehash(static_cast<size_t>(rng() % 5000));
            break;
        }
        default: {
            check_key(key);
            break;
        }
        }
        REQUIRE(m.size() == ref.size());
    }

    CHECK(m.size() == ref.size());
    CHECK(m.load_factor() <= m.max_load_factor() + 1e-6f);
    for (const auto& kv : ref) {
        auto it = m.find(kv.first);
        REQUIRE(it != m.end());
        CHECK(it->second == kv.second);
    }
    size_t count = 0;
    for (const auto& kv : m) {
        CHECK(ref.at(kv.first) == kv.second);
        ++count;
    }
    CHECK(count == m.size());
}

// ---------------------------------------------------------------------------
// continuous churn (erase + insert at constant size): probe drift must not
// break correctness, and a full scan after every round must stay exact.
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: churn keeps the index exact") {
    constexpr int N = 3000;
    std::mt19937 rng(20261121);
    emhash9::HashMap<uint64_t, uint64_t> m;
    m.reserve(N);

    for (uint64_t i = 0; i < N; ++i)
        m[i] = i;

    uint64_t next_insert = N;
    for (int round = 0; round < 200000; ++round) {
        const uint64_t victim = rng() % next_insert;
        if (m.erase(victim) != 0)
            m[next_insert++] = victim;
        if ((round & 1023) == 0) {
            CHECK(m.size() == N);
            size_t seen = 0;
            for (auto it = m.begin(); it != m.end(); ++it) {
                CHECK(m.at(it->first) == it->second);
                ++seen;
            }
            CHECK(seen == m.size());
        }
    }
}

// ---------------------------------------------------------------------------
// weak / adversarial hashes
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: constant hasher still stores every key") {
    emhash9::HashMap<int, int, constant_hash> m;
    constexpr int N = 4000;
    for (int i = 0; i < N; ++i)
        CHECK(m.insert({i, i * 2}).second);
    CHECK(m.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        REQUIRE(it != m.end());
        CHECK(it->second == i * 2);
    }
    // erase a slice and verify the rest
    for (int i = 0; i < N; i += 2)
        CHECK(m.erase(i) == 1);
    for (int i = 0; i < N; ++i)
        CHECK(m.contains(i) == ((i % 2) != 0));
    CHECK(m.size() == static_cast<size_t>(N / 2));
}

TEST_CASE("emhash9: identity hasher on sequential keys") {
    emhash9::HashMap<uint64_t, uint64_t, identity_hash> m;
    constexpr int N = 50000;
    for (int i = 0; i < N; ++i)
        m[static_cast<uint64_t>(i)] = static_cast<uint64_t>(i);
    CHECK(m.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; ++i)
        CHECK(m.at(static_cast<uint64_t>(i)) == static_cast<uint64_t>(i));
}

// ---------------------------------------------------------------------------
// string keys
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: string keys") {
    emhash9::HashMap<std::string, int> m;
    constexpr int N = 5000;
    for (int i = 0; i < N; ++i)
        m["key_" + std::to_string(i)] = i;
    CHECK(m.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        auto it = m.find("key_" + std::to_string(i));
        REQUIRE(it != m.end());
        CHECK(it->second == i);
    }
    for (int i = 0; i < N; i += 5)
        CHECK(m.erase("key_" + std::to_string(i)) == 1);
    for (int i = 0; i < N; ++i)
        CHECK(m.contains("key_" + std::to_string(i)) == ((i % 5) != 0));
}

// ---------------------------------------------------------------------------
// copy / move / swap
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: copy and move") {
    emhash9::HashMap<std::string, std::string> a;
    for (int i = 0; i < 2000; ++i)
        a["k" + std::to_string(i)] = "v" + std::to_string(i);

    auto b = a;
    CHECK(b == a);
    b["k1"] = "changed";
    CHECK(b != a);
    CHECK(a.at("k1") == "v1");

    auto c = std::move(a);
    CHECK(c.size() == 2000);
    CHECK(c.at("k1") == "v1");
    CHECK(a.empty()); // NOLINT(bugprone-use-after-move) - documented moved-from state

    emhash9::HashMap<std::string, std::string> d;
    d["only"] = "one";
    swap(c, d);
    CHECK(c.size() == 1);
    CHECK(d.size() == 2000);
    CHECK(d.at("k1") == "v1");
}

// ---------------------------------------------------------------------------
// move-only mapped type (erase has to move elements around)
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: move-only mapped type") {
    emhash9::HashMap<int, std::unique_ptr<int>> m;
    for (int i = 0; i < 500; ++i)
        m.emplace(i, std::make_unique<int>(i));
    CHECK(m.size() == 500);
    for (int i = 0; i < 500; i += 3)
        CHECK(m.erase(i) == 1);
    for (int i = 0; i < 500; ++i) {
        if (i % 3 == 0) {
            CHECK_FALSE(m.contains(i));
        } else {
            auto it = m.find(i);
            REQUIRE(it != m.end());
            CHECK(*it->second == i);
        }
    }
}

// ---------------------------------------------------------------------------
// precomputed-hash lookup
// ---------------------------------------------------------------------------
TEST_CASE("emhash9: precomputed hash lookup") {
    Map m;
    for (int i = 0; i < 1000; ++i)
        m[i] = i * 7;
    const auto h = m.hash(123);
    auto it = m.find(h, 123);
    REQUIRE(it != m.end());
    CHECK(it->second == 861);
    CHECK(m.find(m.hash(2000), 2000) == m.end());
}
