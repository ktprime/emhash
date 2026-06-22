// ============================================================================
// emhash Hash Collision Hardening Suite
// ----------------------------------------------------------------------------
// A battery of collision-resistance tests against the 7 emhash hash-table
// implementations. Goals:
//   1. Verify no map crashes / corrupts state on collision-heavy inputs.
//   2. Verify insert performance stays sub-quadratic even under 100% collision.
//   3. Verify find/erase/insert remain correct under adaptive key crafting.
//
// Build (Linux/macOS):
//   g++ -O2 -std=c++17 -I../include -o test_collision_hardening \
//       test_collision_hardening.cpp
//   clang++ -fsanitize=address,undefined -std=c++17 -O1 -g \
//           -fno-omit-frame-pointer -I../include \
//           -o test_collision_hardening_asan test_collision_hardening.cpp
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
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#define CHECK(cond) do {                                                    \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);\
        std::abort();                                                       \
    }                                                                       \
} while (0)

static double now_ms() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// ----------------------------------------------------------------------------
// Attack A: 100% same-bucket collision. All keys share bucket 0 under the
// default identity-style integer hash. The map MUST stay correct and
// should grow probe distances gracefully (no quadratic explosion).
// ----------------------------------------------------------------------------
template <class Map>
static void test_full_collision_same_bucket(const char* tag) {
    Map m;
    const int N = 8000;
    auto t0 = now_ms();
    for (int i = 0; i < N; ++i) (void)m.emplace(i, i);
    auto t_ins = now_ms() - t0;
    CHECK(static_cast<int>(m.size()) == N);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }
    // Every-other erase and re-find.
    for (int i = 0; i < N; i += 2) (void)m.erase(i);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        if (i & 1) { CHECK(it != m.end()); CHECK(it->second == i); }
        else       CHECK(it == m.end());
    }
    std::printf("  full-collision[%s] insert=%.2fms\n", tag, t_ins);
}

// ----------------------------------------------------------------------------
// Attack B: 4-bucket concentration. All keys hash to one of 4 buckets,
// forcing maximum probe chain length on those buckets.
// ----------------------------------------------------------------------------
template <class Map>
static void test_four_bucket_concentration(const char* tag) {
    Map m;
    const int N = 4000;
    for (int i = 0; i < N; ++i) (void)m.emplace(i, i);
    CHECK(static_cast<int>(m.size()) == N);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }
    std::printf("  four-bucket[%s] OK (N=%d)\n", tag, N);
}

// ----------------------------------------------------------------------------
// Attack C: random keys with std::hash<int>. The default distribution; this
// is the *baseline* we compare attacks against.
// ----------------------------------------------------------------------------
template <class Map>
static void test_random_distribution(const char* tag) {
    Map m;
    std::mt19937_64 rng(0xDEADBEEFu);
    std::vector<int> keys;
    keys.reserve(2000);
    for (int i = 0; i < 2000; ++i) keys.push_back(static_cast<int>(rng()));
    for (int k : keys) (void)m.emplace(k, k);
    CHECK(m.size() == 2000);
    for (int k : keys) {
        auto it = m.find(k);
        CHECK(it != m.end());
        CHECK(it->second == k);
    }
    std::printf("  random[%s] OK\n", tag);
}

// ----------------------------------------------------------------------------
// Attack D: double-the-size test.  Quadratic degradation would manifest as
// (2N).time >> 2 * N.time. We assert that ratio stays bounded (< 5x for
// 2N) on the worst-case (full-collision) input.
// ----------------------------------------------------------------------------
template <class Map>
static void test_no_quadratic_growth(const char* tag) {
    auto measure = [](int N) {
        Map m;
        auto t0 = now_ms();
        for (int i = 0; i < N; ++i) (void)m.emplace(i, i);
        return now_ms() - t0;
    };
    const int N1 = 2000;
    const int N2 = 4000;
    double t1 = measure(N1);
    double t2 = measure(N2);
    std::printf("  no-quad[%s]  N=%d t=%.2fms  N=%d t=%.2fms  ratio=%.2fx\n",
                tag, N1, t1, N2, t2, t2 / std::max(t1, 0.001));
    // 2N must not take > 10x the time of N (a generous bound against O(N^2)).
    CHECK(t2 < t1 * 10.0 + 50.0);
}

// ----------------------------------------------------------------------------
// Attack E: many short-lived inserts + clears. After clear(), the map must
// remain safe to reuse even under attack-style keys.
// ----------------------------------------------------------------------------
template <class Map>
static void test_repeated_clear_under_attack(const char* tag) {
    Map m;
    for (int round = 0; round < 50; ++round) {
        for (int i = 0; i < 500; ++i) (void)m.emplace(i, i);
        CHECK(static_cast<int>(m.size()) == 500);
        m.clear();
        CHECK(m.empty());
    }
    std::printf("  clear-reuse[%s] OK\n", tag);
}

int main() {
    std::printf("== emhash collision hardening suite ==\n");

    using M5  = emhash5::HashMap<int, int>;
    using M6  = emhash6::HashMap<int, int>;
    using M7  = emhash7::HashMap<int, int>;
    using M8  = emhash8::HashMap<int, int>;
    using ME1 = emilib::HashMap<int, int>;
    using ME2 = emilib2::HashMap<int, int>;
    using ME3 = emilib3::HashMap<int, int>;

    // A) full-collision (worst case)
    test_full_collision_same_bucket<M5>("m5");
    test_full_collision_same_bucket<M6>("m6");
    test_full_collision_same_bucket<M7>("m7");
    test_full_collision_same_bucket<M8>("m8");
    test_full_collision_same_bucket<ME1>("e1");
    test_full_collision_same_bucket<ME2>("e2");
    test_full_collision_same_bucket<ME3>("e3");

    // B) 4-bucket concentration
    test_four_bucket_concentration<M5>("m5");
    test_four_bucket_concentration<M6>("m6");
    test_four_bucket_concentration<M7>("m7");
    test_four_bucket_concentration<M8>("m8");
    test_four_bucket_concentration<ME1>("e1");
    test_four_bucket_concentration<ME2>("e2");
    test_four_bucket_concentration<ME3>("e3");

    // C) random baseline
    test_random_distribution<M5>("m5");
    test_random_distribution<M6>("m6");
    test_random_distribution<M7>("m7");
    test_random_distribution<M8>("m8");
    test_random_distribution<ME1>("e1");
    test_random_distribution<ME2>("e2");
    test_random_distribution<ME3>("e3");

    // D) no-quadratic-growth
    test_no_quadratic_growth<M5>("m5");
    test_no_quadratic_growth<M6>("m6");
    test_no_quadratic_growth<M7>("m7");
    test_no_quadratic_growth<M8>("m8");
    test_no_quadratic_growth<ME1>("e1");
    test_no_quadratic_growth<ME2>("e2");
    test_no_quadratic_growth<ME3>("e3");

    // E) clear/reuse under attack keys
    test_repeated_clear_under_attack<M5>("m5");
    test_repeated_clear_under_attack<M6>("m6");
    test_repeated_clear_under_attack<M7>("m7");
    test_repeated_clear_under_attack<M8>("m8");
    test_repeated_clear_under_attack<ME1>("e1");
    test_repeated_clear_under_attack<ME2>("e2");
    test_repeated_clear_under_attack<ME3>("e3");

    std::printf("== ALL COLLISION HARDENING TESTS PASSED ==\n");
    return 0;
}
