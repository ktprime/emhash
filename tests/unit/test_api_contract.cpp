// unit/test_api_contract.cpp
// Compile-time API contract enforcement.
// Every map/set implementation must expose the documented surface; presence is
// checked with detection idioms so an API regression (method removed/renamed
// or a signature narrowing, issue #104 style) fails at build time instead of
// at user compile time.
//
// Documented deviations are asserted explicitly, so changing one requires a
// conscious decision (flip the assertion together with the documentation):
//   - only hash_table5 provides try_emplace(hint, ...)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <string>
#include <type_traits>
#include <utility>

// ============================================================================
// Detection idiom: EMH_COMPILES(name, expr) defines name<M>::value which is
// true when `expr` is a valid expression for an M& instance.
// ============================================================================
#define EMH_COMPILES(name, expr)                                                                                       \
    template <typename M, typename = void> struct name : std::false_type {};                                           \
    template <typename M> struct name<M, std::void_t<decltype(expr)>> : std::true_type {}

EMH_COMPILES(has_at, std::declval<M&>().at(0));
EMH_COMPILES(has_subscript, std::declval<M&>()[0]);
EMH_COMPILES(has_find, std::declval<M&>().find(0));
EMH_COMPILES(has_count, std::declval<M&>().count(0));
EMH_COMPILES(has_contains, std::declval<M&>().contains(0));
EMH_COMPILES(has_erase_key, std::declval<M&>().erase(0));
EMH_COMPILES(has_clear, std::declval<M&>().clear());
EMH_COMPILES(has_reserve, std::declval<M&>().reserve(16));
EMH_COMPILES(has_rehash, std::declval<M&>().rehash(16));
EMH_COMPILES(has_load_factor, std::declval<M&>().load_factor());
EMH_COMPILES(has_max_load_factor, std::declval<M&>().max_load_factor(0.8f));
EMH_COMPILES(has_iterators, std::declval<M&>().begin() != std::declval<M&>().end());
EMH_COMPILES(has_insert_value, std::declval<M&>().insert(typename M::value_type(0, 1)));
EMH_COMPILES(has_emplace_kv, std::declval<M&>().emplace(0, 1));
EMH_COMPILES(has_insert_or_assign, std::declval<M&>().insert_or_assign(0, 1));
EMH_COMPILES(has_try_emplace_0, std::declval<M&>().try_emplace(0));
EMH_COMPILES(has_try_emplace_1, std::declval<M&>().try_emplace(0, 1));
EMH_COMPILES(has_try_emplace_n, std::declval<M&>().try_emplace(0, 1, 2));
EMH_COMPILES(has_hint_try_emplace, std::declval<M&>().try_emplace(std::declval<M&>().begin(), 0));
EMH_COMPILES(has_insert_key, std::declval<M&>().insert(0));

// Multi-arg try_emplace needs a value type with a multi-argument constructor
// and a default constructor (for the zero-arg form).
struct Ctor2 {
    int v = 0;
    Ctor2() = default;
    explicit Ctor2(int x) : v(x) {}
    Ctor2(int x, int y) : v(x * 10 + y) {}
};

// ============================================================================
// Contracts
// ============================================================================

// Core STL-facing surface every emhash map (5-8) must provide.
template <typename M> void map_core_contract() {
    static_assert(has_at<M>::value, "map must provide at()");
    static_assert(has_subscript<M>::value, "map must provide operator[]");
    static_assert(has_find<M>::value, "map must provide find()");
    static_assert(has_count<M>::value, "map must provide count()");
    static_assert(has_contains<M>::value, "map must provide contains()");
    static_assert(has_erase_key<M>::value, "map must provide erase(key)");
    static_assert(has_clear<M>::value, "map must provide clear()");
    static_assert(has_reserve<M>::value, "map must provide reserve()");
    static_assert(has_rehash<M>::value, "map must provide rehash()");
    static_assert(has_load_factor<M>::value, "map must provide load_factor()");
    static_assert(has_max_load_factor<M>::value, "map must provide max_load_factor()");
    static_assert(has_iterators<M>::value, "map must provide begin()/end()");
    static_assert(has_insert_value<M>::value, "map must provide insert(value_type)");
    static_assert(has_emplace_kv<M>::value, "map must provide emplace(k, v)");
    static_assert(has_insert_or_assign<M>::value, "map must provide insert_or_assign()");
}

// try_emplace surface (issue #104): zero / one / multiple mapped-value args.
template <typename M> void try_emplace_contract() {
    static_assert(has_try_emplace_0<M>::value, "try_emplace(key) must compile (issue #104)");
    static_assert(has_try_emplace_1<M>::value, "try_emplace(key, v) must compile");
    static_assert(has_try_emplace_n<M>::value, "try_emplace(key, a, b) must compile (issue #104)");
}

// Basic surface every set (emhash2/4/8 + emilib2/3) must provide.
template <typename S> void set_core_contract() {
    static_assert(has_insert_key<S>::value, "set must provide insert(key)");
    static_assert(has_find<S>::value, "set must provide find()");
    static_assert(has_count<S>::value, "set must provide count()");
    static_assert(has_erase_key<S>::value, "set must provide erase(key)");
    static_assert(has_clear<S>::value, "set must provide clear()");
    static_assert(has_iterators<S>::value, "set must provide begin()/end()");
}

// ============================================================================
// Test cases (instantiate the contracts; the checks themselves are compile-time)
// ============================================================================
TEST_CASE("API contract: all 8 maps core surface") {
    map_core_contract<map5<int, int>>();
    map_core_contract<map6<int, int>>();
    map_core_contract<map7<int, int>>();
    map_core_contract<map8<int, int>>();
    map_core_contract<imap1<int, int>>();
    map_core_contract<imap2<int, int>>();
    map_core_contract<imap3<int, int>>();
    map_core_contract<imap4<int, int>>();
    CHECK(true);
}

TEST_CASE("API contract: try_emplace zero/one/multi-arg (issue #104)") {
    try_emplace_contract<map5<int, Ctor2>>();
    try_emplace_contract<map6<int, Ctor2>>();
    try_emplace_contract<map7<int, Ctor2>>();
    try_emplace_contract<map8<int, Ctor2>>();
    try_emplace_contract<imap1<int, Ctor2>>();
    try_emplace_contract<imap2<int, Ctor2>>();
    try_emplace_contract<imap3<int, Ctor2>>();
    try_emplace_contract<imap4<int, Ctor2>>();
    CHECK(true);
}

TEST_CASE("API contract: documented deviations") {
    // Only hash_table5 provides the try_emplace(hint, ...) overload.
    static_assert(has_hint_try_emplace<map5<int, int>>::value, "map5 must keep try_emplace(hint, ...)");
    static_assert(!has_hint_try_emplace<map6<int, int>>::value, "map6 has no hint overload (documented)");
    static_assert(!has_hint_try_emplace<map7<int, int>>::value, "map7 has no hint overload (documented)");
    static_assert(!has_hint_try_emplace<map8<int, int>>::value, "map8 has no hint overload (documented)");
    CHECK(true);
}

TEST_CASE("API contract: sets basic surface") {
    set_core_contract<set2<int>>();
    set_core_contract<set4<int>>();
    set_core_contract<set8<int>>();
    set_core_contract<iset2<int>>();
    set_core_contract<iset3<int>>();
    CHECK(true);
}
