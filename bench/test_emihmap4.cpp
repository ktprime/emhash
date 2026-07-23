/**
 * Comprehensive correctness test for emihmap4 (single-allocation swiss table)
 */

#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cassert>
#include <random>
#include <algorithm>

#define TEST(name) printf("  TEST %-45s ", name)
#define PASS() printf("PASS\n")

using Map = emilib4::HashMap<int64_t, int64_t>;
using SMap = emilib4::HashMap<std::string, int64_t>;

static void test_basic_crud() {
    TEST("basic CRUD");
    Map m;
    assert(m.empty());
    assert(m.size() == 0);

    m[1] = 10;
    assert(!m.empty());
    assert(m.size() == 1);
    assert(m[1] == 10);
    assert(m.contains(1));
    assert(!m.contains(2));

    m[1] = 20;
    assert(m[1] == 20);
    assert(m.size() == 1);

    m[2] = 30;
    assert(m.size() == 2);
    assert(m.at(2) == 30);

    auto cnt = m.erase(1);
    assert(cnt == 1);
    assert(!m.contains(1));
    assert(m.size() == 1);

    cnt = m.erase(999);
    assert(cnt == 0);
    PASS();
}

static void test_insert_find_erase_large() {
    TEST("insert/find/erase 1M random int64");
    Map m;
    constexpr int N = 1000000;
    std::mt19937_64 rng(12345);
    std::vector<int64_t> keys(N);
    for (int i = 0; i < N; i++) keys[i] = rng();

    // Insert all
    for (auto k : keys) m[k] = k * 7;
    assert(m.size() == N);

    // Find all
    for (auto k : keys) {
        auto it = m.find(k);
        assert(it != m.end());
        assert(it->second == k * 7);
    }

    // Find miss
    for (int i = 0; i < 1000; i++) {
        auto k = rng();
        if (std::find(keys.begin(), keys.end(), k) == keys.end()) {
            assert(!m.contains(k));
        }
    }

    // Erase half
    std::shuffle(keys.begin(), keys.end(), rng);
    for (int i = 0; i < N / 2; i++) {
        assert(m.erase(keys[i]) == 1);
    }
    assert(m.size() == N / 2);

    // Check remaining
    for (int i = N / 2; i < N; i++) {
        assert(m.contains(keys[i]));
    }
    for (int i = 0; i < N / 2; i++) {
        assert(!m.contains(keys[i]));
    }
    PASS();
}

static void test_string_keys() {
    TEST("string key CRUD + erase");
    SMap m;
    constexpr int N = 50000;
    for (int i = 0; i < N; i++) {
        m["key_" + std::to_string(i)] = i;
    }
    assert(m.size() == N);

    for (int i = 0; i < N; i++) {
        auto k = "key_" + std::to_string(i);
        assert(m.contains(k));
        assert(m[k] == i);
    }

    // Erase every other
    for (int i = 0; i < N; i += 2) {
        m.erase("key_" + std::to_string(i));
    }
    assert(m.size() == N / 2);

    for (int i = 1; i < N; i += 2) {
        assert(m.contains("key_" + std::to_string(i)));
    }
    PASS();
}

static void test_copy_move() {
    TEST("copy and move semantics");
    Map m1;
    for (int i = 0; i < 1000; i++) m1[i] = i * 3;

    Map m2(m1); // copy
    assert(m2.size() == m1.size());
    for (int i = 0; i < 1000; i++) {
        assert(m2.contains(i));
        assert(m2[i] == i * 3);
    }

    Map m3(std::move(m1)); // move
    assert(m3.size() == 1000);
    assert(m1.size() == 0 || m1.empty());

    Map m4;
    m4 = m2; // copy assign
    assert(m4.size() == 1000);

    Map m5;
    m5 = std::move(m3); // move assign
    assert(m5.size() == 1000);
    PASS();
}

static void test_clear_reuse() {
    TEST("clear and reuse");
    Map m;
    for (int i = 0; i < 10000; i++) m[i] = i;
    assert(m.size() == 10000);
    m.clear();
    assert(m.size() == 0);
    assert(m.empty());

    // Reuse after clear
    for (int i = 0; i < 5000; i++) m[i + 100] = i;
    assert(m.size() == 5000);
    for (int i = 0; i < 5000; i++) {
        assert(m.contains(i + 100));
    }
    PASS();
}

static void test_reserve_shrink() {
    TEST("reserve and shrink_to_fit");
    Map m;
    m.reserve(100000);
    for (int i = 0; i < 1000; i++) m[i] = i;
    m.shrink_to_fit();
    assert(m.size() == 1000);
    for (int i = 0; i < 1000; i++) {
        assert(m.contains(i));
    }
    PASS();
}

static void test_iterator() {
    TEST("iterator traversal");
    Map m;
    constexpr int N = 5000;
    for (int i = 0; i < N; i++) m[i] = i;

    int count = 0;
    int64_t sum = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        count++;
        sum += it->first;
    }
    assert(count == N);
    assert(sum == (int64_t)N * (N - 1) / 2);
    PASS();
}

static void test_erase_if() {
    TEST("erase_if");
    Map m;
    for (int i = 0; i < 1000; i++) m[i] = i;
    auto erased = m.erase_if([](auto& p) { return p.first % 3 == 0; });
    assert(erased == 334); // 0,3,6,...,999
    assert(m.size() == 666);
    for (int i = 0; i < 1000; i++) {
        if (i % 3 == 0)
            assert(!m.contains(i));
        else
            assert(m.contains(i));
    }
    PASS();
}

static void test_at_out_of_range() {
    TEST("at() throws out_of_range");
    Map m;
    m[1] = 10;
    bool threw = false;
    try { m.at(999); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);
    PASS();
}

static void test_initializer_list() {
    TEST("initializer_list constructor");
    Map m{{1, 10}, {2, 20}, {3, 30}};
    assert(m.size() == 3);
    assert(m[1] == 10);
    assert(m[2] == 20);
    assert(m[3] == 30);
    PASS();
}

static void test_operator_eq() {
    TEST("operator== and !=");
    Map m1, m2;
    for (int i = 0; i < 100; i++) { m1[i] = i; m2[i] = i; }
    assert(m1 == m2);
    m2[50] = 999;
    assert(m1 != m2);
    PASS();
}

static void test_high_erase_reinsert() {
    TEST("high erase + reinsert (overflow byte stress)");
    Map m;
    m.reserve(1000);
    // Insert
    for (int i = 0; i < 800; i++) m[i] = i;
    // Erase most
    for (int i = 0; i < 750; i++) m.erase(i);
    assert(m.size() == 50);
    // Reinsert into same groups (tests overflow byte handling)
    for (int i = 0; i < 750; i++) m[i] = i * 2;
    assert(m.size() == 800);
    for (int i = 0; i < 800; i++) {
        assert(m.contains(i));
        if (i >= 750) assert(m[i] == i);
        else assert(m[i] == i * 2);
    }
    PASS();
}

static void test_sequential_keys() {
    TEST("sequential int keys (swiss table worst case)");
    Map m;
    constexpr int N = 500000;
    for (int i = 1; i <= N; i++) m[i] = i;
    assert(m.size() == N);
    for (int i = 1; i <= N; i++) {
        assert(m.contains(i));
        assert(m[i] == i);
    }
    for (int i = 1; i <= N / 2; i++) m.erase(i);
    assert(m.size() == N / 2);
    PASS();
}

static void test_empty_map_ops() {
    TEST("empty map operations");
    Map m;
    assert(m.begin() == m.end());
    assert(m.find(1) == m.end());
    assert(!m.contains(1));
    assert(m.size() == 0);
    m.clear(); // clear on empty
    assert(m.empty());
    PASS();
}

static void test_single_element() {
    TEST("single element edge case");
    Map m;
    m[42] = 100;
    assert(m.size() == 1);
    assert(m.contains(42));
    m.erase(42);
    assert(m.empty());
    m[42] = 200;
    assert(m[42] == 200);
    PASS();
}

int main() {
    printf("=== emihmap4 Correctness Tests ===\n");
    test_basic_crud();
    test_insert_find_erase_large();
    test_string_keys();
    test_copy_move();
    test_clear_reuse();
    test_reserve_shrink();
    test_iterator();
    test_erase_if();
    test_at_out_of_range();
    test_initializer_list();
    test_operator_eq();
    test_high_erase_reinsert();
    test_sequential_keys();
    test_empty_map_ops();
    test_single_element();
    printf("\nAll tests PASSED!\n");
    return 0;
}
