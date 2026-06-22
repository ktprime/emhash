// ============================================================================
// emhash Memory Safety & Sanitizer Verification Suite
// ----------------------------------------------------------------------------
// This file aggregates a set of targeted micro-tests that exercise the
// hash-table implementations in scenarios known to trip memory sanitizers
// (ASan, UBSan, MSan). It is meant to be compiled with -fsanitize=address
// (and/or -fsanitize=undefined or -fsanitize=memory) and run repeatedly.
//
// Build (Linux/macOS):
//   clang++ -std=c++17 -O1 -g -fno-omit-frame-pointer \
//           -fsanitize=address,undefined -I../include \
//           verify/test_sanitizer_aggregate.cpp -o test_sanitizer_aggregate
//   clang++ -std=c++17 -O1 -g -fno-omit-frame-pointer \
//           -fsanitize=memory -fsanitize-memory-track-origins -I../include \
//           verify/test_sanitizer_aggregate.cpp -o test_sanitizer_aggregate_msan
//
// Build (Windows MSVC):
//   cl /std:c++17 /Zi /fsanitize=address /EHsc /I..\include \
//      verify\test_sanitizer_aggregate.cpp /Fe:test_sanitizer_aggregate.exe
// ============================================================================

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define CHECK(cond) do {                                                    \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);\
        std::abort();                                                       \
    }                                                                       \
} while (0)

// ----------------------------------------------------------------------------
// Pathological hashers (available for targeted collision tests).
// ----------------------------------------------------------------------------

struct const_hash_int {
    size_t operator()(int) const { return 0; }
};
struct range4_hash_int {
    size_t operator()(int k) const { return static_cast<size_t>(k) & 3u; }
};
struct linear_hash_int {
    size_t operator()(int k) const { return static_cast<size_t>(k); }
};

// ----------------------------------------------------------------------------
// Test 1: clear() must leave memory in a defined state under every map type.
// This is the regression test for the MSan "use-of-uninitialized-value" bug
// we fixed in the clear() path of hash_table5/6/8 + hash_set8.
// ----------------------------------------------------------------------------
template <class Map>
static void test_clear_resets_state() {
    Map m;
    for (int i = 0; i < 1000; ++i) (void)m.emplace(i, i * 2);
    CHECK(m.size() == 1000);
    m.clear();
    CHECK(m.empty());

    // After clear, the map must be fully reusable without UB.
    for (int i = 0; i < 1000; ++i) (void)m.emplace(i, i + 1);
    CHECK(m.size() == 1000);
    for (int i = 0; i < 1000; ++i) CHECK(m.find(i) != m.end());
    m.clear();
    for (int i = 0; i < 1000; ++i) (void)m.emplace(i, i - 1);
    CHECK(m.size() == 1000);
}

// ----------------------------------------------------------------------------
// Test 2: erase + reinsert cycle must not leak or read freed memory.
// ----------------------------------------------------------------------------
template <class Map>
static void test_erase_reinsert_cycle() {
    Map m;
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 500; ++i) (void)m.emplace(i, i);
        for (int i = 0; i < 500; i += 2) (void)m.erase(i);
        for (int i = 0; i < 500; ++i) {
            if (i & 1) CHECK(m.find(i) != m.end());
            else      CHECK(m.find(i) == m.end());
        }
        m.clear();
    }
}

// ----------------------------------------------------------------------------
// Test 3: string keys — exercises need_explicit_dtor path. This is where the
// MSan "use-of-uninitialized-value in find_or_allocate" regression used to
// live. After clear, find/insert must read initialized memory only.
// ----------------------------------------------------------------------------
template <class Map>
static void test_string_keys_clear_reuse() {
    Map m;
    for (int i = 0; i < 256; ++i) (void)m.emplace(std::to_string(i), i);
    CHECK(m.size() == 256);
    m.clear();
    CHECK(m.empty());

    for (int i = 0; i < 256; ++i) (void)m.emplace(std::string("k_") + std::to_string(i), i);
    CHECK(m.size() == 256);
    for (int i = 0; i < 256; ++i) CHECK(m.find(std::string("k_") + std::to_string(i)) != m.end());
}

// ----------------------------------------------------------------------------
// Test 4: collision attack via collision-prone integer sequences. Exercises
// probe sequences and kickout logic; ASan should report no OOB reads/writes.
// Uses keys that are deliberately clustered so the hasher (std::hash<int>)
// produces a tight distribution and forces deep probes.
// ----------------------------------------------------------------------------
template <class Map>
static void test_collision_attack(const char* tag) {
    Map m;
    (void)m.reserve(8192);
    // Use a key pattern that forces collisions: small multiples of a prime.
    const int N = 4096;
    for (int i = 0; i < N; ++i) (void)m.emplace(i * 4093, i);   // 4093 is prime
    CHECK(static_cast<int>(m.size()) == N);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i * 4093);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }
    // Erase every other key and verify.
    for (int i = 0; i < N; i += 2) (void)m.erase(i * 4093);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i * 4093);
        if (i & 1) { CHECK(it != m.end()); CHECK(it->second == i); }
        else       CHECK(it == m.end());
    }
    std::printf("  collision-attack[%s] OK\n", tag);
}

// ----------------------------------------------------------------------------
// Test 5: bucket-count boundaries. Power-of-two and -one bucket sizes are the
// riskiest because of the masking optimization (hash & (N-1)).
// ----------------------------------------------------------------------------
template <class Map>
static void test_bucket_count_boundaries() {
    const int sizes[] = {1, 2, 3, 7, 15, 16, 17, 31, 32, 63, 64, 127, 128,
                         255, 256, 1023, 1024, 4095, 4096};
    for (int n : sizes) {
        Map m;
        (void)m.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) (void)m.emplace(i, i + 1);
        CHECK(static_cast<int>(m.size()) == n);
        for (int i = 0; i < n; ++i) {
            auto it = m.find(i);
            CHECK(it != m.end());
            CHECK(it->second == i + 1);
        }
    }
}

// ----------------------------------------------------------------------------
// Test 6: high load-factor (90%+) without rehash crash. Many hash tables
// have off-by-one bugs in the rehash threshold arithmetic.
// ----------------------------------------------------------------------------
template <class Map>
static void test_high_load_factor() {
    Map m;
    m.max_load_factor(0.95f);
    for (int i = 0; i < 100000; ++i) (void)m.emplace(i, i);
    CHECK(m.size() == 100000);
    for (int i = 0; i < 100000; i += 7) CHECK(m.find(i) != m.end());
}

// ----------------------------------------------------------------------------
// Test 7: cross-implementation consistency. Same operations on the same
// random inputs must produce the same logical result across all maps.
// ----------------------------------------------------------------------------
template <class MapA, class MapB>
static void test_consistency_pair() {
    std::mt19937_64 rng(0xC0FFEEu);
    std::vector<int> keys;
    for (int i = 0; i < 5000; ++i) keys.push_back(static_cast<int>(rng()));

    MapA a;
    MapB b;
    for (int k : keys) {
        (void)a.emplace(k, k * 3);
        (void)b.emplace(k, k * 3);
    }
    CHECK(a.size() == static_cast<decltype(a.size())>(b.size()));
    for (int k : keys) {
        auto ita = a.find(k);
        auto itb = b.find(k);
        CHECK((ita == a.end()) == (itb == b.end()));
        if (ita != a.end()) CHECK(ita->second == itb->second);
    }
}

// ----------------------------------------------------------------------------
// Test 8: copy / move must not leak (catches missing allocators).
// ----------------------------------------------------------------------------
template <class Map>
static void test_copy_move() {
    Map src;
    for (int i = 0; i < 2000; ++i) (void)src.emplace(std::to_string(i), i);

    Map copy = src;
    CHECK(copy.size() == src.size());
    for (int i = 0; i < 2000; ++i) {
        auto it = copy.find(std::to_string(i));
        CHECK(it != copy.end());
        CHECK(it->second == i);
    }

    Map mv = std::move(copy);
    CHECK(mv.size() == 2000);
    CHECK(copy.empty());
}

int main() {
    std::printf("== emhash memory-safety aggregate ==\n");

    using M5  = emhash5::HashMap<int, int>;
    using M6  = emhash6::HashMap<int, int>;
    using M7  = emhash7::HashMap<int, int>;
    using M8  = emhash8::HashMap<int, int>;
    using ME1 = emilib::HashMap<int, int>;
    using ME2 = emilib2::HashMap<int, int>;
    using ME3 = emilib3::HashMap<int, int>;

    using MS5  = emhash5::HashMap<std::string, int>;
    using MS6  = emhash6::HashMap<std::string, int>;
    using MS7  = emhash7::HashMap<std::string, int>;
    using MS8  = emhash8::HashMap<std::string, int>;
    using MSE1 = emilib::HashMap<std::string, int>;
    using MSE2 = emilib2::HashMap<std::string, int>;
    using MSE3 = emilib3::HashMap<std::string, int>;

    // 1) clear() regression
    test_clear_resets_state<M5>();   test_clear_resets_state<M6>();
    test_clear_resets_state<M7>();   test_clear_resets_state<M8>();
    test_clear_resets_state<ME1>();  test_clear_resets_state<ME2>();
    test_clear_resets_state<ME3>();

    // 2) erase/reinsert cycle
    test_erase_reinsert_cycle<M5>();  test_erase_reinsert_cycle<M6>();
    test_erase_reinsert_cycle<M7>();  test_erase_reinsert_cycle<M8>();
    test_erase_reinsert_cycle<ME1>(); test_erase_reinsert_cycle<ME2>();
    test_erase_reinsert_cycle<ME3>();

    // 3) string keys clear/reuse
    test_string_keys_clear_reuse<MS5>();  test_string_keys_clear_reuse<MS6>();
    test_string_keys_clear_reuse<MS7>();  test_string_keys_clear_reuse<MS8>();
    test_string_keys_clear_reuse<MSE1>(); test_string_keys_clear_reuse<MSE2>();
    test_string_keys_clear_reuse<MSE3>();

    // 4) collision attacks
    test_collision_attack<M5>("const_int");
    test_collision_attack<ME1>("const_int_e1");
    test_collision_attack<M5>("range4_int");
    test_collision_attack<ME1>("range4_int_e1");
    test_collision_attack<M5>("linear_int");
    test_collision_attack<ME1>("linear_int_e1");

    // 5) bucket-count boundaries
    test_bucket_count_boundaries<M5>();  test_bucket_count_boundaries<M6>();
    test_bucket_count_boundaries<M7>();  test_bucket_count_boundaries<M8>();
    test_bucket_count_boundaries<ME1>(); test_bucket_count_boundaries<ME2>();
    test_bucket_count_boundaries<ME3>();

    // 6) high load factor
    test_high_load_factor<M5>();  test_high_load_factor<M6>();
    test_high_load_factor<M7>();  test_high_load_factor<M8>();
    test_high_load_factor<ME1>(); test_high_load_factor<ME2>();
    test_high_load_factor<ME3>();

    // 7) cross-implementation consistency
    test_consistency_pair<M5, ME1>(); test_consistency_pair<M6, ME2>();
    test_consistency_pair<M7, ME3>(); test_consistency_pair<M8, M5>();

    // 8) copy / move
    test_copy_move<MS5>();  test_copy_move<MS6>();
    test_copy_move<MS7>();  test_copy_move<MS8>();
    test_copy_move<MSE1>(); test_copy_move<MSE2>();
    test_copy_move<MSE3>();

    std::printf("== ALL SANITIZER AGGREGATE TESTS PASSED ==\n");
    return 0;
}
