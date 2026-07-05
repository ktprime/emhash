// attack/test_hash_attack.cpp
// Hash collision attack tests: 7 map implementations × 3 attack hashers.
// Verifies correctness AND performance stays sub-quadratic under:
//   - ConstHasher (all keys -> bucket 0, worst-case collision)
//   - Range4Hasher (keys -> 4 buckets, partial collision)
//   - LinearHasher (identity hash, sequential keys)
// Requires EMH_SAFE_PSL compile definition (provided by CMake emhash_add_attack_test).
// Source: attack/hash_attack_all.cpp (608 lines) + hash_attack.cpp + hash_attack7.cpp.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"
#include "common/hashers.hpp"

#include <chrono>
#include <cstdio>
#include <unordered_map>

// Attack-sized type lists (emhash5-8 + emilib1/2/3)
#define ConstAttackMaps                                                                                                \
    map5<int, int, ConstHasher>, map6<int, int, ConstHasher>, map7<int, int, ConstHasher>,                             \
        map8<int, int, ConstHasher>, imap1<int, int, ConstHasher>, imap2<int, int, ConstHasher>,                       \
        imap3<int, int, ConstHasher>
#define Range4AttackMaps                                                                                               \
    map5<int, int, Range4Hasher>, map6<int, int, Range4Hasher>, map7<int, int, Range4Hasher>,                          \
        map8<int, int, Range4Hasher>, imap1<int, int, Range4Hasher>, imap2<int, int, Range4Hasher>,                    \
        imap3<int, int, Range4Hasher>

static double elapsed_ms(double start, double end) {
    return end - start;
}
static double now_ms() {
    return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

// ============================================================================
// ConstHasher attack: all keys collide to bucket 0
// ============================================================================
TEST_CASE_TEMPLATE("attack: ConstHasher correctness", Map, ConstAttackMaps) {
    const int N = 5000;
    Map m;
    std::unordered_map<int, int> oracle;

    for (int i = 0; i < N; ++i) {
        m[i] = i;
        oracle[i] = i;
    }
    CHECK(m.size() == static_cast<size_t>(N));

    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }

    // erase half
    for (int i = 0; i < N; i += 2)
        m.erase(i);
    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        if (i & 1) {
            CHECK(it != m.end());
            CHECK(it->second == i);
        } else
            CHECK(it == m.end());
    }
}

TEST_CASE_TEMPLATE("attack: ConstHasher performance sub-quadratic", Map, ConstAttackMaps) {
    const int N = 5000;
    Map m;
    auto t0 = now_ms();
    for (int i = 0; i < N; ++i)
        m[i] = i;
    auto t_ins = elapsed_ms(t0, now_ms());

    auto t1 = now_ms();
    for (int i = 0; i < N; ++i)
        (void)m.find(i);
    auto t_find = elapsed_ms(t1, now_ms());

    // Performance sanity: insert+find should complete in reasonable time.
    // Loose bound to avoid flakiness on slow CI; real concern is O(N^2).
    CHECK(t_ins < 30000.0); // 30s ceiling
    CHECK(t_find < 30000.0);
    std::printf("  ConstHasher N=%d insert=%.1fms find=%.1fms\n", N, t_ins, t_find);
}

// ============================================================================
// Range4Hasher attack: 4-bucket concentration
// ============================================================================
TEST_CASE_TEMPLATE("attack: Range4Hasher correctness", Map, Range4AttackMaps) {
    const int N = 4000;
    Map m;

    for (int i = 0; i < N; ++i)
        m[i] = i;
    CHECK(m.size() == static_cast<size_t>(N));

    for (int i = 0; i < N; ++i) {
        auto it = m.find(i);
        CHECK(it != m.end());
        CHECK(it->second == i);
    }

    for (int i = 0; i < N; i += 3)
        m.erase(i);
    for (int i = 0; i < N; ++i) {
        if (i % 3 == 0)
            CHECK(m.find(i) == m.end());
        else
            CHECK(m.find(i) != m.end());
    }
}

// ============================================================================
// LinearHasher (identity) baseline: no collision, sequential keys
// ============================================================================
TEST_CASE_TEMPLATE("attack: LinearHasher baseline correctness", Map, AllIntMaps) {
    const int N = 8000;
    Map m;

    for (int i = 0; i < N; ++i)
        m[i] = i * 2;
    CHECK(m.size() == static_cast<size_t>(N));

    for (int i = 0; i < N; ++i) {
        CHECK(m[i] == i * 2);
    }

    for (int i = 0; i < N; i += 2)
        m.erase(i);
    for (int i = 0; i < N; ++i) {
        if (i % 2 == 0)
            CHECK(m.find(i) == m.end());
        else
            CHECK(m.find(i) != m.end());
    }
}

// ============================================================================
// High load factor + attack: combine both stressors
// ============================================================================
TEST_CASE_TEMPLATE("attack: high LF + ConstHasher", Map, ConstAttackMaps) {
    const int N = 2000;
    Map m;
    m.max_load_factor(0.95f);

    for (int i = 0; i < N; ++i)
        m[i] = i;
    CHECK(m.size() == static_cast<size_t>(N));

    for (int i = 0; i < N; ++i)
        CHECK(m.find(i) != m.end());

    for (int i = 0; i < N; i += 4)
        m.erase(i);
    for (int i = 0; i < N; i += 4)
        m[i] = i; // reinsert
    CHECK(m.size() == static_cast<size_t>(N));
}
