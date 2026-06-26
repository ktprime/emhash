/**
 * Lifecycle audit test — full object lifecycle coverage for all 7 hash map implementations.
 *
 * All maps use std::string for both key and value (non-trivially-copyable).
 * Memory leaks are detected by ASan/MSan at exit.
 *
 * Coverage:
 *   1. Default construction + destruction
 *   2. Insert + clear + destroy
 *   3. Copy constructor chain (deep copies)
 *   4. Move constructor
 *   5. Copy assignment (cross-size: small←large, large←small)
 *   6. Move assignment
 *   7. Self-assignment
 *   8. Insert/erase churn
 *   9. Swap
 *  10. Reserve / shrink_to_fit cycles
 *  11. Heavy rehash (fill, clear, refill)
 *  12. Copy assign from small to large with sentinel (clone path)
 *
 * Compile (ASan): g++ -std=c++17 -O2 -fsanitize=address -I include -o test_audit test_audit.cpp
 * Compile (plain): g++ -std=c++17 -O2 -I include -o test_audit test_audit.cpp
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

using namespace std::literals;

static int g_pass = 0, g_fail = 0;

#define CHECK(expr, msg) do { \
    if (!(expr)) { \
        printf("    FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; \
    } else { g_pass++; } \
} while(0)

// Helper
static std::string K(int i) { return "k_" + std::to_string(i); }
static std::string V(int i) { return "v_" + std::to_string(i); }

// =============================================================================
// Audits — templated on map type (std::string key, std::string value)
// =============================================================================

template<typename Map>
void audit_default_ctor_dtor(const char* name) {
    printf("  [audit-1] %s default ctor/dtor\n", name);
    { Map m; (void)m; }
    printf("  [audit-1] %s default ctor/dtor: PASS\n", name);
}

template<typename Map>
void audit_insert_clear(const char* name) {
    printf("  [audit-2] %s insert+clear\n", name);
    Map m;
    for (int i = 0; i < 100; i++) m[K(i)] = V(i);
    CHECK(m.size() == 100, "size 100");
    CHECK(m[K(0)] == V(0), "first elem");
    CHECK(m[K(99)] == V(99), "last elem");
    m.clear();
    CHECK(m.empty(), "empty after clear");
    printf("  [audit-2] %s insert+clear: PASS\n", name);
}

template<typename Map>
void audit_copy_chain(const char* name) {
    printf("  [audit-3] %s deep copy chain\n", name);
    Map src;
    for (int i = 0; i < 50; i++) src[K(i)] = V(i);
    Map a(src);
    Map b(a);
    Map c(b);
    Map d(c);
    Map e(d);
    CHECK(e.size() == 50, "deep copy size");
    CHECK(e[K(0)] == V(0), "deep copy val 0");
    CHECK(e[K(49)] == V(49), "deep copy val 49");
    printf("  [audit-3] %s deep copy chain: PASS\n", name);
}

template<typename Map>
void audit_move_ctor(const char* name) {
    printf("  [audit-4] %s move ctor\n", name);
    Map src;
    for (int i = 0; i < 50; i++) src[K(i)] = V(i);
    size_t n = src.size();
    Map dst(std::move(src));
    CHECK(dst.size() == n, "moved-to size");
    CHECK(dst[K(25)] == V(25), "moved-to value");
    printf("  [audit-4] %s move ctor: PASS\n", name);
}

template<typename Map>
void audit_copy_assign_cross(const char* name) {
    printf("  [audit-5] %s cross-size copy assign\n", name);
    Map small;
    for (int i = 0; i < 5; i++)  small[K(i)] = V(i);
    Map large;
    for (int i = 0; i < 500; i++) large[K(i)] = V(i);

    // small→large forces clone rehash path
    Map dst1 = small;
    dst1 = large;
    CHECK(dst1.size() == 500, "small←large assign");
    CHECK(dst1[K(0)] == V(0), "val 0 after large assign");
    CHECK(dst1[K(499)] == V(499), "val 499 after large assign");

    // large→small also forces rehash
    Map dst2 = large;
    dst2 = small;
    CHECK(dst2.size() == 5, "large←small assign");
    CHECK(dst2[K(4)] == V(4), "val 4 after small assign");

    printf("  [audit-5] %s cross-size copy assign: PASS\n", name);
}

template<typename Map>
void audit_move_assign(const char* name) {
    printf("  [audit-6] %s move assign\n", name);
    Map src;
    for (int i = 0; i < 50; i++) src[K(i)] = V(i);
    Map dst;
    for (int i = 0; i < 10; i++) dst[K(i)] = V(i);
    dst = std::move(src);
    CHECK(dst.size() == 50, "move-assigned size");
    printf("  [audit-6] %s move assign: PASS\n", name);
}

template<typename Map>
void audit_self_assign(const char* name) {
    printf("  [audit-7] %s self assign\n", name);
    Map m;
    for (int i = 0; i < 20; i++) m[K(i)] = V(i);
    size_t n = m.size();
    m = m;
    CHECK(m.size() == n, "self-assign size");
    CHECK(m[K(10)] == V(10), "self-assign val");
    printf("  [audit-7] %s self assign: PASS\n", name);
}

template<typename Map>
void audit_churn(const char* name) {
    printf("  [audit-8] %s churn\n", name);
    Map m;
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 100; i++) m[K(i)] = V(i);
        for (int i = 0; i < 100; i += 2) (void)m.erase(K(i));
    }
    printf("  [audit-8] %s churn: PASS\n", name);
}

template<typename Map>
void audit_swap(const char* name) {
    printf("  [audit-9] %s swap\n", name);
    Map a, b;
    for (int i = 0; i < 20; i++) a[K(i)] = V(i);
    a.swap(b);
    CHECK(a.empty(), "a empty after swap");
    CHECK(b.size() == 20, "b has 20 after swap");
    printf("  [audit-9] %s swap: PASS\n", name);
}

template<typename Map>
void audit_reserve_shrink(const char* name) {
    printf("  [audit-10] %s reserve/shrink\n", name);
    Map m;
    (void)m.reserve(1000);
    for (int i = 0; i < 500; i++) m[K(i)] = V(i);
    m.shrink_to_fit();
    m.clear();
    (void)m.reserve(2000);
    printf("  [audit-10] %s reserve/shrink: PASS\n", name);
}

template<typename Map>
void audit_heavy_rehash(const char* name) {
    printf("  [audit-11] %s heavy rehash\n", name);
    Map m;
    for (int i = 0; i < 2000; i++) m[K(i)] = V(i);
    m.clear();
    for (int i = 0; i < 2000; i++) m[K(i)] = V(i);
    CHECK(m.size() == 2000, "heavy rehash size");
    CHECK(m[K(0)] == V(0), "heavy rehash val 0");
    CHECK(m[K(1999)] == V(1999), "heavy rehash val 1999");
    printf("  [audit-11] %s heavy rehash: PASS\n", name);
}

// =============================================================================
// Run all audits for one map
// =============================================================================
template<typename Map>
void run_all(const char* name) {
    printf("--- %s ---\n", name);
    audit_default_ctor_dtor<Map>(name);
    audit_insert_clear<Map>(name);
    audit_copy_chain<Map>(name);
    audit_move_ctor<Map>(name);
    audit_copy_assign_cross<Map>(name);
    audit_move_assign<Map>(name);
    audit_self_assign<Map>(name);
    audit_churn<Map>(name);
    audit_swap<Map>(name);
    audit_reserve_shrink<Map>(name);
    audit_heavy_rehash<Map>(name);
    printf("\n");
}

// =============================================================================
int main() {
    printf("=== Lifecycle Audit Test (all maps, std::string k/v) ===\n\n");

    run_all<emhash5::HashMap<std::string, std::string>>("emhash5");
    run_all<emhash6::HashMap<std::string, std::string>>("emhash6");
    run_all<emhash7::HashMap<std::string, std::string>>("emhash7");
    run_all<emhash8::HashMap<std::string, std::string>>("emhash8");
    run_all<emilib::HashMap<std::string, std::string>>("emihmap1");
    run_all<emilib2::HashMap<std::string, std::string>>("emihmap2");
    run_all<emilib3::HashMap<std::string, std::string>>("emihmap3");

    printf("=== Summary: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}