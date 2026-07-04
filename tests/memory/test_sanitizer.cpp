// memory/test_sanitizer.cpp
// Memory-safety verification for ASan/UBSan/MSan builds.
// 8 scenarios matching project memory constraints:
//   1. clear() resets state (MSan regression)
//   2. erase + reinsert cycle
//   3. string key clear+reuse
//   4. bucket-count boundaries (power-of-two masking)
//   5. high load factor (90%+)
//   6. cross-implementation consistency
//   7. copy/move leak detection
//   8. collision attack (probe + kickout)
// Source: verify/test_sanitizer_aggregate.cpp (298 lines).
// Build: cmake -DEMHASH_SANITIZER=address (or memory/undefined)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// 1. clear() must leave memory in a defined state (MSan regression).
//    Fixed in hash_table5/6/8 + hash_set8: reset bucket fields to INACTIVE.
// ============================================================================
TEST_CASE_TEMPLATE("sanitizer: clear resets state", Map, AllIntMaps) {
    Map m;
    for (int i = 0; i < 1000; ++i) m.emplace(i, i * 2);
    CHECK(m.size() == 1000);
    m.clear();
    CHECK(m.empty());

    for (int i = 0; i < 1000; ++i) m.emplace(i, i + 1);
    CHECK(m.size() == 1000);
    for (int i = 0; i < 1000; ++i) CHECK(m.find(i) != m.end());
    m.clear();
    for (int i = 0; i < 1000; ++i) m.emplace(i, i - 1);
    CHECK(m.size() == 1000);
}

TEST_CASE_TEMPLATE("sanitizer: clear resets state (string keys)", Map, AllStringMaps) {
    Map m;
    for (int i = 0; i < 256; ++i) m.emplace(std::to_string(i), i);
    CHECK(m.size() == 256);
    m.clear();
    CHECK(m.empty());

    for (int i = 0; i < 256; ++i) m.emplace(std::string("k_") + std::to_string(i), i);
    CHECK(m.size() == 256);
    for (int i = 0; i < 256; ++i)
        CHECK(m.find(std::string("k_") + std::to_string(i)) != m.end());
}

// ============================================================================
// 2. erase + reinsert cycle must not leak or read freed memory.
// ============================================================================
TEST_CASE_TEMPLATE("sanitizer: erase + reinsert cycle", Map, AllIntMaps) {
    Map m;
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 500; ++i) m.emplace(i, i);
        for (int i = 0; i < 500; i += 2) m.erase(i);
        for (int i = 0; i < 500; ++i) {
            if (i & 1) CHECK(m.find(i) != m.end());
            else       CHECK(m.find(i) == m.end());
        }
        m.clear();
    }
}

// ============================================================================
// 3. string key clear + reuse (need_explicit_dtor path, MSan regression).
// ============================================================================
TEST_CASE_TEMPLATE("sanitizer: string key reuse after clear", Map, AllStringMaps) {
    Map m;
    for (int i = 0; i < 256; ++i) m.emplace(std::to_string(i), i);
    m.clear();
    for (int i = 0; i < 256; ++i) m.emplace(std::string("v2_") + std::to_string(i), i * 2);
    CHECK(m.size() == 256);
    for (int i = 0; i < 256; ++i)
        CHECK(m.find(std::string("v2_") + std::to_string(i)) != m.end());
}

// ============================================================================
// 4. bucket-count boundaries (power-of-two masking risk).
// ============================================================================
TEST_CASE_TEMPLATE("sanitizer: bucket count boundaries", Map, AllIntMaps) {
    const int sizes[] = {1, 2, 3, 7, 15, 16, 17, 31, 32, 63, 64, 127, 128, // NOLINT(readability-identifier-naming)
                         255, 256, 1023, 1024, 4095, 4096};
    for (int n : sizes) {
        Map m;
        m.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) m.emplace(i, i + 1);
        CHECK(static_cast<int>(m.size()) == n);
        for (int i = 0; i < n; ++i) {
            auto it = m.find(i);
            CHECK(it != m.end());
            CHECK(it->second == i + 1);
        }
    }
}

// ============================================================================
// 5. high load factor (90%+) without rehash crash.
// ============================================================================
TEST_CASE_TEMPLATE("sanitizer: high load factor", Map, AllIntMaps) {
    Map m;
    m.max_load_factor(0.95F);
    for (int i = 0; i < 50000; ++i) m.emplace(i, i);
    CHECK(m.size() == 50000);
    for (int i = 0; i < 50000; i += 7) CHECK(m.find(i) != m.end());
}

// ============================================================================
// 6. cross-implementation consistency (same ops → same logical result).
// ============================================================================
TEST_CASE("sanitizer: cross-implementation consistency emhash5 vs emilib1") {
    using A = map5<int, int>;
    using B = imap1<int, int>;
    std::mt19937_64 rng(0xC0FFEEU); // NOLINT(cert-msc32-c,cert-msc51-cpp)
    std::vector<int> keys;
    keys.reserve(5000);
    for (int i = 0; i < 5000; ++i) keys.push_back(static_cast<int>(rng()));

    A a; B b;
    for (int k : keys) { a.emplace(k, k * 3); b.emplace(k, k * 3); }
    CHECK(a.size() == b.size());
    for (int k : keys) {
        auto ita = a.find(k);
        auto itb = b.find(k);
        CHECK((ita == a.end()) == (itb == b.end()));
        if (ita != a.end()) CHECK(ita->second == itb->second);
    }
}

TEST_CASE("sanitizer: cross-implementation consistency emhash8 vs emhash5") {
    using A = map8<int, int>;
    using B = map5<int, int>;
    std::mt19937_64 rng(0xBEEFU); // NOLINT(cert-msc32-c,cert-msc51-cpp)
    std::vector<int> keys;
    keys.reserve(3000);
    for (int i = 0; i < 3000; ++i) keys.push_back(static_cast<int>(rng()));

    A a; B b;
    for (int k : keys) { a.emplace(k, k); b.emplace(k, k); }
    CHECK(a.size() == b.size());
    for (int k : keys) {
        CHECK((a.find(k) == a.end()) == (b.find(k) == b.end()));
    }
}

// ============================================================================
// 7. copy/move must not leak (catches missing allocators).
// ============================================================================
TEST_CASE_TEMPLATE("sanitizer: copy/move no leak (string keys)", Map, AllStringMaps) {
    Map src;
    for (int i = 0; i < 2000; ++i) src.emplace(std::to_string(i), i);

    Map copy = src;
    CHECK(copy.size() == src.size());
    for (int i = 0; i < 2000; ++i) {
        auto it = copy.find(std::to_string(i));
        CHECK(it != copy.end());
        CHECK(it->second == i);
    }

    Map mv = std::move(copy);
    CHECK(mv.size() == 2000);
    CHECK(copy.empty()); // NOLINT(bugprone-use-after-move)
}

// ============================================================================
// 8. collision attack (probe sequences + kickout, ASan OOB detection).
//    Uses keys that are small multiples of a prime to force tight clustering.
// ============================================================================
TEST_CASE_TEMPLATE("sanitizer: collision attack (clustered keys)", Map, AllIntMaps) {
    Map m;
    m.reserve(8192);
    const int N = 4096;
    for (int i = 0; i < N; ++i) m.emplace(i * 4093, i);
    CHECK(static_cast<int>(m.size()) == N);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i * 4093);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }
    for (int i = 0; i < N; i += 2) m.erase(i * 4093);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i * 4093);
        if (i & 1) { CHECK(it != m.end()); CHECK(it->second == i); }
        else       CHECK(it == m.end());
    }
}
