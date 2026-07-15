// unit/test_emihmap4.cpp
// Bug-focused correctness test for emilib4::HashMap (swiss table design).
// Strategy: minimize trivial API smoke tests (those belong in test_api_coverage),
//           maximize scenarios that historically break hash tables:
//           - rehash correctness (element preservation, tombstone handling)
//           - erase + reinsert (tombstone reuse, group boundary crossing)
//           - copy/move with non-trivial types (memcpy safety, allocator consistency)
//           - overflow probe chains (quadratic probing under collision)
//           - iterator stability across mutations
//           - size boundary around group/swiss-table group size (15 slots)
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
#include <unordered_map>

using MapII = emilib4::HashMap<int, int>;
using MapSI = emilib4::HashMap<std::string, int>;
using MapIS = emilib4::HashMap<int, std::string>;
using MapI64 = emilib4::HashMap<int64_t, int64_t>;

// ─── 1. Empty state + moved-from state ────────────────────────────────

TEST_CASE("empty and moved-from state") {
    MapII m;
    CHECK(m.empty());
    CHECK(m.size() == 0);
    CHECK(m.begin() == m.end());
    CHECK(m.find(1) == m.end());
    CHECK(!m.contains(1));

    MapII m2;
    m2[1] = 10;
    MapII m3(std::move(m2));
    // moved-from: valid but unspecified state; must not crash
    CHECK(m2.size() <= 1);
    m2.clear(); // must be callable
    CHECK(m2.empty());
    m2[2] = 20; // must be reusable
    CHECK(m2[2] == 20);
}

// ─── 2. Insert + duplicate handling ────────────────────────────────────

TEST_CASE("insert returns correct pair for new/duplicate keys") {
    MapII m;
    auto [it1, ok1] = m.insert({1, 10});
    CHECK(ok1);
    CHECK(it1->second == 10);

    auto [it2, ok2] = m.insert({1, 20});
    CHECK(!ok2);
    CHECK(it2->second == 10); // original unchanged

    // try_emplace must not overwrite
    auto [it3, ok3] = m.try_emplace(1, 999);
    CHECK(!ok3);
    CHECK(it3->second == 10);

    auto [it4, ok4] = m.try_emplace(2, 20);
    CHECK(ok4);
    CHECK(it4->second == 20);

    // insert_or_assign must overwrite
    auto [it5, ok5] = m.insert_or_assign(1, 99);
    CHECK(!ok5);
    CHECK(it5->second == 99);
    CHECK(m[1] == 99);

    auto [it6, ok6] = m.insert_or_assign(3, 30);
    CHECK(ok6);
    CHECK(m[3] == 30);
}

// ─── 3. Erase tombstone stress: erase half, reinsert, repeat ──────────

TEST_CASE("erase half + reinsert oscillation") {
    // This stresses tombstone handling in swiss tables.
    // Repeatedly erasing and reinserting creates many deleted slots,
    // testing whether probe chains remain correct.
    MapII m;
    const int N = 500;
    for (int i = 0; i < N; i++)
        m[i] = i;

    for (int round = 0; round < 10; round++) {
        // Erase even keys
        for (int i = 0; i < N; i += 2)
            CHECK(m.erase(i) == 1);
        CHECK(m.size() == static_cast<size_t>(N / 2));

        // Reinsert even keys with new values
        for (int i = 0; i < N; i += 2)
            m[i] = i * 100 + round;

        CHECK(m.size() == static_cast<size_t>(N));
        // Verify odd keys untouched
        for (int i = 1; i < N; i += 2)
            CHECK(m[i] == i);
    }
}

// ─── 4. Erase all then reinsert (tombstone cleanup via rehash) ───────

TEST_CASE("erase all then reinsert") {
    MapII m;
    for (int i = 0; i < 200; i++)
        m[i] = i;
    for (int i = 0; i < 200; i++)
        CHECK(m.erase(i) == 1);
    CHECK(m.empty());

    // After full erase, map must accept new inserts without corruption
    for (int i = 0; i < 200; i++)
        m[i + 1000] = i * 7;
    CHECK(m.size() == 200);
    for (int i = 0; i < 200; i++)
        CHECK(m[i + 1000] == i * 7);
}

// ─── 5. Rehash correctness: all elements must survive ─────────────────

TEST_CASE("rehash preserves all elements") {
    MapII m;
    for (int i = 0; i < 500; i++)
        m[i] = i * 3;

    // Force multiple rehashes
    m.reserve(100000);
    CHECK(m.size() == 500);
    for (int i = 0; i < 500; i++)
        CHECK(m[i] == i * 3);

    // Shrink back down
    m.shrink_to_fit();
    CHECK(m.size() == 500);
    for (int i = 0; i < 500; i++)
        CHECK(m[i] == i * 3);
}

TEST_CASE("rehash(0) after heavy erase") {
    MapII m;
    for (int i = 0; i < 1000; i++)
        m[i] = i;
    for (int i = 0; i < 950; i++)
        m.erase(i);
    m.rehash(0);
    CHECK(m.size() == 50);
    for (int i = 950; i < 1000; i++)
        CHECK(m[i] == i);
}

// ─── 6. Copy/move with string keys (non-trivially-copyable) ──────────

TEST_CASE("string key copy/move lifecycle") {
    // String keys stress the non-trivially-copyable path in clone().
    // A bug here leads to double-free or leaked memory.
    MapSI m1;
    for (int i = 0; i < 200; i++)
        m1["key_" + std::to_string(i)] = i;

    auto m2(m1); // copy ctor
    CHECK(m2.size() == 200);
    for (int i = 0; i < 200; i++)
        CHECK(m2["key_" + std::to_string(i)] == i);

    auto m3 = std::move(m1); // move ctor
    CHECK(m3.size() == 200);
    for (int i = 0; i < 200; i++)
        CHECK(m3.contains("key_" + std::to_string(i)));

    MapSI m4;
    m4["x"] = 99;
    m4 = m3; // copy assign
    CHECK(m4.size() == 200);
    CHECK(m4["key_0"] == 0);

    MapSI m5;
    m5 = std::move(m3); // move assign
    CHECK(m5.size() == 200);
}

TEST_CASE("string value copy/move lifecycle") {
    // Non-trivially-copyable values also stress clone()/clear_data().
    MapIS m1;
    for (int i = 0; i < 200; i++)
        m1[i] = "val_" + std::to_string(i);

    auto m2(m1);
    CHECK(m2.size() == 200);
    CHECK(m2[50] == "val_50");

    auto m3 = std::move(m1);
    CHECK(m3.size() == 200);
    CHECK(m3[99] == "val_99");
}

// ─── 7. Copy ctor _pairs_capacity bug: copy then mutate ──────────────

TEST_CASE("copy then mutate must not corrupt source") {
    MapII m1;
    for (int i = 0; i < 100; i++)
        m1[i] = i * 10;

    auto m2(m1);
    // Mutate copy — if _pairs_capacity was wrong, this may write out of bounds
    for (int i = 100; i < 200; i++)
        m2[i] = i;
    CHECK(m2.size() == 200);
    CHECK(m2[199] == 199);

    // Original must be untouched
    CHECK(m1.size() == 100);
    for (int i = 0; i < 100; i++)
        CHECK(m1[i] == i * 10);
}

// ─── 8. Self-assignment safety ────────────────────────────────────────

TEST_CASE("self-assignment guard") {
    MapII m;
    for (int i = 0; i < 100; i++)
        m[i] = i;
    m = m; // must not corrupt or free
    CHECK(m.size() == 100);
    CHECK(m[50] == 50);
}

// ─── 9. Group boundary sizes (swiss table has 15-slot groups) ─────────

TEST_CASE("sizes around group boundaries") {
    // Swiss table groups have 15 slots. Sizes near N*15 boundaries
    // stress group allocation, sentinel handling, and overflow probing.
    for (int sz : {1, 14, 15, 16, 29, 30, 31, 44, 45, 46, 59, 60, 61}) {
        MapII m;
        for (int i = 0; i < sz; i++)
            m[i] = i * 10;
        CHECK(m.size() == static_cast<size_t>(sz));

        // Verify all elements via find
        for (int i = 0; i < sz; i++) {
            auto it = m.find(i);
            REQUIRE(it != m.end());
            CHECK(it->second == i * 10);
        }

        // Erase half and verify survivors
        for (int i = 0; i < sz; i += 2)
            CHECK(m.erase(i) == 1);
        for (int i = 1; i < sz; i += 2) {
            auto it = m.find(i);
            REQUIRE(it != m.end());
            CHECK(it->second == i * 10);
        }
    }
}

// ─── 10. Overflow probing: all keys collide ────────────────────────────

struct Mod2Hash {
    size_t operator()(int k) const { return static_cast<size_t>(k % 2); }
};

TEST_CASE("all keys hash to 2 buckets — overflow probe chain") {
    // With only 2 hash values, all 500 keys must be placed via
    // quadratic probing into overflow groups. This tests:
    // - maybe_caused_overflow and mark_overflow correctness
    // - probe chain termination
    // - find_locator_at returning the right slot
    emilib4::HashMap<int, int, Mod2Hash> m;
    const int N = 500;
    for (int i = 0; i < N; i++)
        m[i] = i * 10;
    CHECK(m.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; i++)
        CHECK(m[i] == i * 10);
    // Erase every 3rd key and re-find the rest
    for (int i = 0; i < N; i += 3)
        CHECK(m.erase(i) == 1);
    for (int i = 0; i < N; i++) {
        if (i % 3 == 0)
            CHECK(!m.contains(i));
        else
            CHECK(m[i] == i * 10);
    }
}

struct IdentityHash {
    size_t operator()(int k) const { return static_cast<size_t>(k); }
};

TEST_CASE("identity hash — every key is unique but low bits repeat") {
    emilib4::HashMap<int, int, IdentityHash> m;
    const int N = 2000;
    for (int i = 0; i < N; i++)
        m[i] = i;
    CHECK(m.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; i++)
        CHECK(m[i] == i);
}

// ─── 11. Iterator correctness after erase ─────────────────────────────

TEST_CASE("erase_if then iterate — no skipped or double-visited elements") {
    MapII m;
    for (int i = 0; i < 200; i++)
        m[i] = i;
    m.erase_if([](auto& p) { return p.first % 3 == 0; });

    std::vector<int> visited;
    for (auto& [k, v] : m)
        visited.push_back(k);
    CHECK(visited.size() == m.size());
    // All visited keys must be non-divisible by 3
    for (int k : visited)
        CHECK(k % 3 != 0);
}

TEST_CASE("iteration visits exactly size() elements") {
    MapSI m;
    for (int i = 0; i < 100; i++)
        m["k" + std::to_string(i)] = i;
    int count = 0;
    for (auto& [k, v] : m) {
        (void)k;
        (void)v;
        count++;
    }
    CHECK(count == 100);
}

// ─── 12. Cross-implementation fuzz consistency ─────────────────────────

TEST_CASE("fuzz consistency vs emilib2 + std::unordered_map") {
    emilib2::HashMap<int, int> ref2;
    std::unordered_map<int, int> ref_std;
    MapII m;
    std::mt19937 rng(42);
    const int OPS = 20000;
    const int KEY_RANGE = 500;

    for (int i = 0; i < OPS; i++) {
        int k = rng() % KEY_RANGE;
        int v = static_cast<int>(rng());
        switch (rng() % 5) {
        case 0: // insert/assign
            ref2[k] = v;
            ref_std[k] = v;
            m[k] = v;
            break;
        case 1: // erase
            ref2.erase(k);
            ref_std.erase(k);
            m.erase(k);
            break;
        case 2: // contains
            CHECK(m.contains(k) == (ref_std.find(k) != ref_std.end()));
            break;
        case 3: // find + verify value
            if (ref_std.count(k)) {
                auto it = m.find(k);
                REQUIRE(it != m.end());
                CHECK(it->second == ref_std[k]);
            } else {
                CHECK(m.find(k) == m.end());
            }
            break;
        case 4: // size check
            CHECK(m.size() == ref_std.size());
            break;
        }
    }
    // Final full consistency check
    CHECK(m.size() == ref_std.size());
    for (auto& [k, v] : ref_std) {
        auto it = m.find(k);
        REQUIRE(it != m.end());
        CHECK(it->second == v);
    }
}

// ─── 13. Copy at high load factor ─────────────────────────────────────

TEST_CASE("copy map at high load factor then extend") {
    MapII m;
    for (int i = 0; i < 1000; i++)
        m[i] = i;
    auto m2(m);
    // Extend copy beyond original capacity — triggers rehash in m2
    for (int i = 1000; i < 2000; i++)
        m2[i] = i;
    CHECK(m2.size() == 2000);
    for (int i = 0; i < 2000; i++)
        CHECK(m2[i] == i);
    // Original unchanged
    CHECK(m.size() == 1000);
}

// ─── 14. Large value type (non-trivially-copyable, 256-byte struct) ───

struct LargeValue {
    int data[64];
    LargeValue() { memset(data, 0, sizeof(data)); }
    explicit LargeValue(int v) {
        for (int i = 0; i < 64; i++)
            data[i] = v + i;
    }
    bool operator==(const LargeValue& o) const { return memcmp(data, o.data, sizeof(data)) == 0; }
};

TEST_CASE("large non-trivially-copyable value — copy, erase, reinsert") {
    emilib4::HashMap<int, LargeValue> m;
    for (int i = 0; i < 100; i++)
        m[i] = LargeValue(i);
    auto m2(m);
    CHECK(m2.size() == 100);
    CHECK(m2[50] == LargeValue(50));

    // Erase and reinsert
    for (int i = 0; i < 50; i++)
        m2.erase(i);
    for (int i = 0; i < 50; i++)
        m2[i] = LargeValue(i * 10);
    CHECK(m2.size() == 100);
    CHECK(m2[0] == LargeValue(0));
    CHECK(m2[99] == LargeValue(99));
}

// ─── 15. merge and self-merge ──────────────────────────────────────────

TEST_CASE("merge moves keys from source, self-merge is safe") {
    MapII src;
    for (int i = 0; i < 50; i++)
        src[i] = i;
    MapII dst;
    for (int i = 50; i < 80; i++)
        dst[i] = i;
    dst.merge(src);
    CHECK(dst.size() == 80);
    CHECK(src.empty());

    // Self-merge: all keys exist, nothing should change
    MapII m;
    m[1] = 10;
    m[2] = 20;
    m.merge(m);
    CHECK(m.size() == 2);
    CHECK(m[1] == 10);
}

// ─── 16. at() throws, operator==, swap, initializer_list ──────────────

TEST_CASE("at() throws, equality, swap, constructors") {
    MapII m{{1, 10}, {2, 20}, {3, 30}};
    CHECK(m.size() == 3);
    CHECK_THROWS_AS(m.at(999), std::out_of_range);

    MapII m2(m);
    CHECK(m == m2); // same content → equal
    m2[4] = 40;
    CHECK(m != m2); // different size → not equal

    MapII m3;
    m3[1] = 10;
    m3[2] = 20;
    m3[3] = 30;
    CHECK(m == m3); // same content, different insertion order → equal

    m.swap(m2);
    CHECK(m.size() == 4);
    CHECK(m2.size() == 3);
    CHECK(m[4] == 40);
}

// ─── 17. Clear + reuse lifecycle ──────────────────────────────────────

TEST_CASE("clear then reuse with different key range") {
    MapII m;
    for (int i = 0; i < 500; i++)
        m[i] = i;
    m.clear();
    CHECK(m.empty());
    // Reuse with completely different keys
    for (int i = 10000; i < 10500; i++)
        m[i] = i;
    CHECK(m.size() == 500);
    CHECK(m[10000] == 10000);
    CHECK(m.find(0) == m.end()); // old keys must not persist
}

// ─── 18. Random string key stress ──────────────────────────────────────

TEST_CASE("random string key insert/erase stress") {
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

// ─── 19. Reserve then fill exactly to capacity boundary ────────────────

TEST_CASE("reserve then fill to boundary") {
    MapII m;
    m.reserve(1000);
    auto bc = m.bucket_count();
    // Fill exactly to capacity
    for (size_t i = 0; i < bc; i++)
        m[static_cast<int>(i)] = static_cast<int>(i);
    CHECK(m.size() == bc);
    // One more insert must trigger rehash
    m[static_cast<int>(bc)] = static_cast<int>(bc);
    CHECK(m.size() == bc + 1);
    // All elements must survive rehash
    for (size_t i = 0; i <= bc; i++)
        CHECK(m[static_cast<int>(i)] == static_cast<int>(i));
}

// ─── 20. Million-scale stress ─────────────────────────────────────────

TEST_CASE("million int CRUD stress") {
    MapII m;
    const int N = 1000000;
    for (int i = 0; i < N; i++)
        m[i] = i;
    CHECK(m.size() == static_cast<size_t>(N));
    for (int i = 0; i < N; i += 100) // spot-check
        CHECK(m[i] == i);
    for (int i = 0; i < N; i++)
        CHECK(m.erase(i) == 1);
    CHECK(m.empty());
}
