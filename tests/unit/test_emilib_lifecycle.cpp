// unit/test_emilib_lifecycle.cpp
// Focused lifecycle tests for emilib SIMD-accelerated maps (emihmap1/2/3/4).
//
// Tests copy(), rehash(), clear+reinsert, move semantics, and erase patterns —
// areas where emilib implementations have had historical bugs:
//   - emihmap1/3 clone() sentinel memory leak when _num_buckets differs
//   - Uninitialized find() return iterators due to bit_mask optimization
//   - emhash6/7's erase() iterator _bmask initialized after modifying _bitmask
//
// emilib API notes:
//   - clone() is void clone(const HashMap& other) — copies FROM other INTO this
//   - erase() returns void (not iterator) — cannot use it = map.erase(it) pattern
//
// These tests complement test_allocator.cpp (which excludes emilib because
// emilib does not support custom AllocT).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <string>
#include <vector>
#include <algorithm>

// ============================================================================
// Type aliases for emilib-only maps
// ============================================================================
using EM2 = imap2<int, int>;
using EM3 = imap3<int, int>;
using EM4 = imap4<int, int>;
using EMS2 = imap2<std::string, std::string>;
using EMS3 = imap3<std::string, std::string>;

// ============================================================================
// Copy constructor / clone() — historical bug: sentinel leak when bucket counts differ
// ============================================================================
TEST_CASE("emilib2 copy constructor preserves data") {
    for (int n : {1, 10, 100, 1000}) {
        EM2 orig;
        for (int i = 0; i < n; i++)
            orig[i] = i * 10;

        EM2 cloned(orig);
        CHECK(orig.size() == cloned.size());
        for (int i = 0; i < n; i++) {
            CHECK(cloned.at(i) == i * 10);
        }
    }
}

TEST_CASE("emilib3 clone() preserves data integrity") {
    for (int n : {1, 10, 100, 1000}) {
        EM3 orig;
        for (int i = 0; i < n; i++)
            orig[i] = i * 10;

        EM3 cloned;
        cloned.clone(orig);
        CHECK(orig.size() == cloned.size());
        for (int i = 0; i < n; i++) {
            CHECK(cloned.at(i) == i * 10);
        }
    }
}

TEST_CASE("emilib3 clone() with string keys") {
    EMS3 orig;
    for (int i = 0; i < 500; i++)
        orig["key_" + std::to_string(i)] = "val_" + std::to_string(i);

    EMS3 cloned;
    cloned.clone(orig);
    CHECK(orig.size() == cloned.size());
    for (int i = 0; i < 500; i++) {
        auto key = "key_" + std::to_string(i);
        CHECK(cloned.at(key) == "val_" + std::to_string(i));
    }
}

TEST_CASE("emilib clone() then modify original") {
    EM3 orig;
    for (int i = 0; i < 200; i++)
        orig[i] = i;

    EM3 cloned;
    cloned.clone(orig);
    orig.clear();
    for (int i = 0; i < 200; i++)
        orig[i] = i + 1000;

    // Cloned should be unaffected
    CHECK(cloned.size() == 200);
    for (int i = 0; i < 200; i++)
        CHECK(cloned.at(i) == i);
}

// ============================================================================
// Rehash() — historical bug: uninitialized find() iterators after rehash
// ============================================================================
TEST_CASE("emilib rehash preserves all elements") {
    EM3 map;
    for (int i = 0; i < 1000; i++)
        map[i] = i;

    map.rehash(5000);

    CHECK(map.size() == 1000);
    for (int i = 0; i < 1000; i++)
        CHECK(map.at(i) == i);
}

TEST_CASE("emilib rehash(0) forces rehash and preserves data") {
    EM2 map;
    for (int i = 0; i < 500; i++)
        map[i] = i * 2;

    map.rehash(0);

    CHECK(map.size() == 500);
    for (int i = 0; i < 500; i++)
        CHECK(map.at(i) == i * 2);
}

TEST_CASE("emilib4 find() returns valid iterator after rehash") {
    // Historical bug: find() returned uninitialized iterators due to
    // bit_mask optimization not being re-initialized after rehash
    EM4 map;
    for (int i = 0; i < 1000; i++)
        map[i] = i;

    map.rehash(4096);

    for (int i = 0; i < 1000; i++) {
        auto it = map.find(i);
        CHECK(it != map.end());
        CHECK(it->second == i);
    }

    // Find miss should also work
    for (int i = 1000; i < 1100; i++) {
        auto it = map.find(i);
        CHECK(it == map.end());
    }
}

// ============================================================================
// clear() + reinsert — historical bug: uninitialized bucket fields
// ============================================================================
TEST_CASE("emilib clear() then reinsert preserves correctness") {
    EM3 map;
    for (int i = 0; i < 500; i++)
        map[i] = i;

    map.clear();
    CHECK(map.size() == 0);

    // Reinsert with different keys to stress bucket reuse
    for (int i = 0; i < 500; i++)
        map[i + 10000] = i + 1;

    CHECK(map.size() == 500);
    for (int i = 0; i < 500; i++)
        CHECK(map.at(i + 10000) == i + 1);
}

TEST_CASE("emilib multiple clear+reinsert cycles") {
    EM2 map;
    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 100; i++)
            map[i] = cycle * 100 + i;
        CHECK(map.size() == 100);
        for (int i = 0; i < 100; i++)
            CHECK(map.at(i) == cycle * 100 + i);
        map.clear();
        CHECK(map.size() == 0);
    }
}

// ============================================================================
// Move semantics — verify no dangling pointers or double-free
// ============================================================================
TEST_CASE("emilib move constructor preserves data") {
    EM3 orig;
    for (int i = 0; i < 500; i++)
        orig[i] = i;

    EM3 moved = std::move(orig);
    CHECK(moved.size() == 500);
    for (int i = 0; i < 500; i++)
        CHECK(moved.at(i) == i);

    // Original should be in valid empty state
    CHECK(orig.size() == 0);
}

TEST_CASE("emilib move assignment then use moved-from") {
    EM3 map1, map2;
    for (int i = 0; i < 300; i++)
        map1[i] = i;

    map2 = std::move(map1);

    // Reuse moved-from object
    for (int i = 0; i < 200; i++)
        map1[i] = i * 2;

    CHECK(map2.size() == 300);
    CHECK(map1.size() == 200);
    for (int i = 0; i < 300; i++)
        CHECK(map2.at(i) == i);
    for (int i = 0; i < 200; i++)
        CHECK(map1.at(i) == i * 2);
}

TEST_CASE("emilib swap two maps") {
    EM3 map1, map2;
    for (int i = 0; i < 100; i++)
        map1[i] = i;
    for (int i = 0; i < 50; i++)
        map2[i + 1000] = i + 1;

    map1.swap(map2);

    CHECK(map1.size() == 50);
    CHECK(map2.size() == 100);
    for (int i = 0; i < 50; i++)
        CHECK(map1.at(i + 1000) == i + 1);
    for (int i = 0; i < 100; i++)
        CHECK(map2.at(i) == i);
}

// ============================================================================
// erase() patterns — historical bug: _bmask init after modifying _bitmask
// ============================================================================
TEST_CASE("emilib erase by key preserves remaining elements") {
    EM3 map;
    for (int i = 0; i < 1000; i++)
        map[i] = i;

    // Erase every 3rd element
    std::vector<int> to_erase;
    for (int i = 0; i < 1000; i += 3)
        to_erase.push_back(i);

    for (int key : to_erase)
        CHECK(map.erase(key) > 0);

    CHECK(map.size() == 1000 - static_cast<int>(to_erase.size()));

    // Verify remaining elements
    for (int i = 0; i < 1000; i++) {
        if (i % 3 == 0) {
            CHECK(map.find(i) == map.end());
        } else {
            CHECK(map.at(i) == i);
        }
    }
}

TEST_CASE("emilib erase all elements then verify empty") {
    EM2 map;
    for (int i = 0; i < 500; i++)
        map[i] = i;

    for (int i = 0; i < 500; i++)
        CHECK(map.erase(i) > 0);

    CHECK(map.size() == 0);
    CHECK(map.empty());

    // Map should still be usable
    map[42] = 999;
    CHECK(map.at(42) == 999);
}

TEST_CASE("emilib erase and reinsert at same keys") {
    EM3 map;
    for (int i = 0; i < 300; i++)
        map[i] = i;

    // Erase half
    for (int i = 0; i < 150; i++)
        CHECK(map.erase(i) > 0);

    // Reinsert with different values
    for (int i = 0; i < 150; i++)
        map[i] = i + 1000;

    CHECK(map.size() == 300);
    for (int i = 0; i < 150; i++)
        CHECK(map.at(i) == i + 1000);
    for (int i = 150; i < 300; i++)
        CHECK(map.at(i) == i);
}

// ============================================================================
// String key lifecycle — comprehensive coverage for non-trivial types
// ============================================================================
TEST_CASE("emilib string key erase and reinsert cycle") {
    EMS3 map;
    for (int i = 0; i < 500; i++)
        map["key_" + std::to_string(i)] = "val_" + std::to_string(i);

    // Erase odd keys
    for (int i = 1; i < 500; i += 2)
        CHECK(map.erase("key_" + std::to_string(i)) > 0);

    CHECK(map.size() == 250);

    // Reinsert with new values
    for (int i = 1; i < 500; i += 2)
        map["key_" + std::to_string(i)] = "new_val_" + std::to_string(i);

    CHECK(map.size() == 500);
    for (int i = 0; i < 500; i++) {
        auto key = "key_" + std::to_string(i);
        if (i % 2 == 0)
            CHECK(map.at(key) == "val_" + std::to_string(i));
        else
            CHECK(map.at(key) == "new_val_" + std::to_string(i));
    }
}

TEST_CASE("emilib string key clone() and modify") {
    EMS3 orig;
    for (int i = 0; i < 200; i++)
        orig["k" + std::to_string(i)] = "v" + std::to_string(i);

    EMS3 cloned;
    cloned.clone(orig);

    // Modify cloned, original should be unaffected
    cloned["k0"] = "modified";
    CHECK(orig.at("k0") == "v0");
    CHECK(cloned.at("k0") == "modified");
}

// ============================================================================
// Iterator validity after modifications
// ============================================================================
TEST_CASE("emilib iterator visits all elements after rehash") {
    EM3 map;
    for (int i = 0; i < 500; i++)
        map[i] = i;

    map.rehash(2000);

    std::vector<int> keys;
    for (auto& p : map)
        keys.push_back(p.first);
    std::sort(keys.begin(), keys.end());

    CHECK(keys.size() == 500);
    for (int i = 0; i < 500; i++)
        CHECK(keys[i] == i);
}

TEST_CASE("emilib iterator after erase by key") {
    EM2 map;
    for (int i = 0; i < 200; i++)
        map[i] = i;

    // Erase even keys
    for (int i = 0; i < 200; i += 2)
        map.erase(i);

    std::vector<int> keys;
    for (auto& p : map)
        keys.push_back(p.first);
    std::sort(keys.begin(), keys.end());

    CHECK(keys.size() == 100);
    for (size_t i = 0; i < keys.size(); i++)
        CHECK(keys[i] == static_cast<int>(i * 2 + 1));
}
