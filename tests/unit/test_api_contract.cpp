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

#include <iterator>
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
EMH_COMPILES(has_try_get, std::declval<M&>().try_get(1, std::declval<typename M::mapped_type&>()));
EMH_COMPILES(has_set_get, std::declval<M&>().set_get(std::declval<const typename M::key_type&>(),
                                                     std::declval<const typename M::mapped_type&>()));
// 3-argument set_get(key, val, old_out) -> bool (emilib1-3 shape; emhash5/8 have it too)
EMH_COMPILES(has_set_get3, std::declval<M&>().set_get(std::declval<const typename M::key_type&>(),
                                                      std::declval<const typename M::mapped_type&>(),
                                                      std::declval<typename M::mapped_type&>()));
EMH_COMPILES(has_try_set, std::declval<M&>().try_set(std::declval<const typename M::key_type&>(),
                                                     std::declval<const typename M::mapped_type&>()));
EMH_COMPILES(has_emplace_hint, std::declval<M&>().emplace_hint(std::declval<M&>().begin(), 0, 1));
EMH_COMPILES(has_merge, std::declval<M&>().merge(std::declval<M&>()));
EMH_COMPILES(has_hetero_find, std::declval<M&>().find(std::declval<const std::string_view&>()));
EMH_COMPILES(has_hetero_erase, std::declval<M&>().erase(std::declval<const std::string_view&>()));
EMH_COMPILES(has_ptr_try_get, std::declval<M&>().try_get(0));
EMH_COMPILES(has_insert_key, std::declval<M&>().insert(0));
EMH_COMPILES(has_insert_unique, std::declval<M&>().insert_unique(0, 1));
EMH_COMPILES(has_hash_function, std::declval<const M&>().hash_function());
EMH_COMPILES(has_key_eq, std::declval<const M&>().key_eq());

// erase(it) return shape: emhash5-8 return the next iterator; emilib1-4 return void
template <typename M, typename = void> struct erase_it_returns_iterator : std::false_type {};
template <typename M>
struct erase_it_returns_iterator<M, std::void_t<decltype(std::declval<M&>().erase(std::declval<M&>().begin()))>>
    : std::is_same<decltype(std::declval<M&>().erase(std::declval<M&>().begin())), typename M::iterator> {};

// iterator category: only emhash8 map/set expose bidirectional iterators
template <typename M>
using iter_category_t = typename std::iterator_traits<typename M::iterator>::iterator_category;
template <typename M, typename = void> struct bidirectional_iterator : std::false_type {};
template <typename M>
struct bidirectional_iterator<M, std::void_t<iter_category_t<M>>>
    : std::is_same<iter_category_t<M>, std::bidirectional_iterator_tag> {};

// Generic predicate for member erase_if detection (no lambdas in unevaluated
// operands before C++20).
struct FalsePred {
    template <typename T> bool operator()(const T&) const { return false; }
};
EMH_COMPILES(has_erase_if, std::declval<M&>().erase_if(std::declval<FalsePred>()));

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

// Extended API surface (see README Feature Matrix): all of these must exist
// on every map implementation unless explicitly pinned as a deviation below.
// Note: try_get/try_set/set_get availability differs (EMH_EXT-gated on
// emhash5-7); shapes are pinned in the deviations test case below.
template <typename M> void extended_map_contract() {
    static_assert(has_erase_if<M>::value, "map must provide member erase_if(pred)");
    static_assert(has_merge<M>::value, "map must provide merge(other)");
    static_assert(has_insert_unique<M>::value, "map must provide insert_unique(key)");
    static_assert(has_try_emplace_0<M>::value && has_try_emplace_1<M>::value && has_try_emplace_n<M>::value,
                  "try_emplace surface (issue #104)");
    static_assert(has_insert_or_assign<M>::value, "map must provide insert_or_assign()");
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

TEST_CASE("API contract: extended surface (README Feature Matrix)") {
    extended_map_contract<map5<int, int>>();
    extended_map_contract<map6<int, int>>();
    extended_map_contract<map7<int, int>>();
    extended_map_contract<map8<int, int>>();
    extended_map_contract<imap1<int, int>>();
    extended_map_contract<imap2<int, int>>();
    extended_map_contract<imap3<int, int>>();
    extended_map_contract<imap4<int, int>>();
    CHECK(true);
}

TEST_CASE("API contract: extended deviations (try_set/set_get/try_get/hetero)") {
    // Default config: EMH_EXT is NOT defined, so emhash5-7 compile their
    // try_get/try_set blocks out entirely (see #ifdef EMH_EXT in headers).
    // set_get: emhash5 + emhash8 + emilib1-3 only
    static_assert(has_set_get<map5<int, int>>::value, "map5 must keep set_get");
    static_assert(has_set_get<map8<int, int>>::value && has_try_set<map8<int, int>>::value,
                  "map8 must keep try_set/set_get");
    static_assert(!has_set_get<map6<int, int>>::value && !has_try_set<map6<int, int>>::value,
                  "map6 has no try_set/set_get by default (documented)");
    static_assert(!has_set_get<map7<int, int>>::value && !has_try_set<map7<int, int>>::value,
                  "map7 has no try_set/set_get by default (documented)");
    static_assert(has_set_get3<imap1<int, int>>::value && has_try_set<imap1<int, int>>::value,
                  "emilib1 must keep try_set/set_get (3-arg set_get shape)");
    static_assert(!has_set_get<imap4<int, int>>::value && !has_try_set<imap4<int, int>>::value,
                  "emilib4 has no try_set/set_get (documented)");
    static_assert(!has_try_set<map5<int, int>>::value, "map5 try_set requires EMH_EXT (documented)");

    // try_get shapes: emhash8 -> bool try_get(key, out&); emilib -> ValueT* try_get(key);
    // emhash5-7 -> EMH_EXT only. Overload presence is pinned via member-pointer
    // casts (plain try_get(1, out) is ambiguous on map8<int, int> between
    // (key, out) and the precomputed-hash (key, uint64_t) overload - see the
    // note at the bottom of this test case), and semantics verified at runtime.
    static_assert(!has_try_get<map5<int, int>>::value, "map5 try_get requires EMH_EXT (documented)");
    static_assert(!has_try_get<map6<int, int>>::value, "map6 try_get requires EMH_EXT (documented)");
    static_assert(!has_try_get<map7<int, int>>::value, "map7 try_get requires EMH_EXT (documented)");
    {
        using Map8 = map8<int, int>;
        using TryGetBoolFn = bool (Map8::*)(const int&, int&) const;
        const auto fn = static_cast<TryGetBoolFn>(&Map8::try_get);

        Map8 m8;
        int out = 0;
        CHECK_FALSE((m8.*fn)(1, out));
        m8[1] = 42;
        CHECK((m8.*fn)(1, out));
        CHECK(out == 42);

        imap1<int, int> mi1;
        CHECK(mi1.try_get(1) == nullptr);
        mi1[1] = 42;
        CHECK(mi1.try_get(1) != nullptr);
        CHECK(*mi1.try_get(1) == 42);
    }

    // emplace_hint: emhash5-8 only
    static_assert(has_emplace_hint<map5<int, int>>::value, "map5 must keep emplace_hint");
    static_assert(!has_emplace_hint<imap1<int, int>>::value, "emilib has no emplace_hint (documented)");
    static_assert(!has_emplace_hint<imap4<int, int>>::value, "emilib4 has no emplace_hint (documented)");

    // heterogeneous erase: only emhash8 with a transparent hasher+equality.
    // (emhash6/7 open-template find/erase appear viable to decltype but their
    // bodies fail with non-transparent equality - body failures are not
    // SFINAE-observable. emhash5's erase is a plain overload set, so its
    // negative is properly detectable.)
    using HeteroMap8 = emhash8::HashMap<std::string, int, emhash8::string_hash, std::equal_to<>>;
    static_assert(has_hetero_find<HeteroMap8>::value && has_hetero_erase<HeteroMap8>::value,
                  "emhash8 + string_hash must keep heterogeneous lookup/erase");
    static_assert(!has_hetero_erase<map5<std::string, int, emhash8::string_hash>>::value,
                  "emhash5 has no heterogeneous erase even with a transparent hasher (documented)");

    // API hazard note (pre-existing, not enforced): emhash8's
    // try_get(key, uint64_t key_hash) overload makes plain m.try_get(1, out)
    // ambiguous for int keys in multi-header builds; in single-header builds
    // GCC silently selects the hash overload (out converted to a hash).
    // Callers should pass a const key lvalue or use operator[]/find instead.
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

TEST_CASE("API contract: inspectors, erase shape and iterator category") {
    // hash_function()/key_eq() accessors: emhash5-8 maps and emhash2/4/8 sets only
    static_assert(has_hash_function<map5<int, int>>::value && has_key_eq<map5<int, int>>::value,
                  "map5 must keep hash_function/key_eq");
    static_assert(has_hash_function<map6<int, int>>::value && has_key_eq<map6<int, int>>::value,
                  "map6 must keep hash_function/key_eq");
    static_assert(has_hash_function<map7<int, int>>::value && has_key_eq<map7<int, int>>::value,
                  "map7 must keep hash_function/key_eq");
    static_assert(has_hash_function<map8<int, int>>::value && has_key_eq<map8<int, int>>::value,
                  "map8 must keep hash_function/key_eq");
    static_assert(!has_hash_function<imap1<int, int>>::value, "emilib1 has no hash_function/key_eq (documented)");
    static_assert(!has_hash_function<imap2<int, int>>::value, "emilib2 has no hash_function/key_eq (documented)");
    static_assert(!has_hash_function<imap3<int, int>>::value, "emilib3 has no hash_function/key_eq (documented)");
    static_assert(!has_hash_function<imap4<int, int>>::value, "emilib4 has no hash_function/key_eq (documented)");
    static_assert(has_hash_function<set2<int>>::value && has_key_eq<set2<int>>::value,
                  "set2 must keep hash_function/key_eq");
    static_assert(has_hash_function<set4<int>>::value && has_key_eq<set4<int>>::value,
                  "set4 must keep hash_function/key_eq");
    static_assert(has_hash_function<set8<int>>::value && has_key_eq<set8<int>>::value,
                  "set8 must keep hash_function/key_eq");
    static_assert(!has_hash_function<iset2<int>>::value, "emilib2 set has no hash_function/key_eq (documented)");
    static_assert(!has_hash_function<iset3<int>>::value, "emilib3 set has no hash_function/key_eq (documented)");

    // erase(it) return shape: emhash5-8 maps return the next iterator; emilib maps return void.
    // Sets: emhash2/4/8 and emilib2 return iterator; emilib3 returns void.
    static_assert(erase_it_returns_iterator<map5<int, int>>::value, "map5 erase(it) must return iterator");
    static_assert(erase_it_returns_iterator<map6<int, int>>::value, "map6 erase(it) must return iterator");
    static_assert(erase_it_returns_iterator<map7<int, int>>::value, "map7 erase(it) must return iterator");
    static_assert(erase_it_returns_iterator<map8<int, int>>::value, "map8 erase(it) must return iterator");
    static_assert(!erase_it_returns_iterator<imap1<int, int>>::value, "emilib1 erase(it) returns void (documented)");
    static_assert(!erase_it_returns_iterator<imap2<int, int>>::value, "emilib2 erase(it) returns void (documented)");
    static_assert(!erase_it_returns_iterator<imap3<int, int>>::value, "emilib3 erase(it) returns void (documented)");
    static_assert(!erase_it_returns_iterator<imap4<int, int>>::value, "emilib4 erase(it) returns void (documented)");
    static_assert(erase_it_returns_iterator<set2<int>>::value, "set2 erase(it) must return iterator");
    static_assert(erase_it_returns_iterator<set4<int>>::value, "set4 erase(it) must return iterator");
    static_assert(erase_it_returns_iterator<set8<int>>::value, "set8 erase(it) must return iterator");
    static_assert(erase_it_returns_iterator<iset2<int>>::value, "emilib2 set erase(it) returns iterator");
    static_assert(!erase_it_returns_iterator<iset3<int>>::value, "emilib3 set erase(it) returns void (documented)");

    // iterator category: bidirectional only on emhash8 map/set, forward elsewhere
    static_assert(bidirectional_iterator<map8<int, int>>::value, "map8 iterators must stay bidirectional");
    static_assert(!bidirectional_iterator<map5<int, int>>::value, "map5 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<map6<int, int>>::value, "map6 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<map7<int, int>>::value, "map7 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<imap1<int, int>>::value, "emilib1 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<imap2<int, int>>::value, "emilib2 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<imap3<int, int>>::value, "emilib3 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<imap4<int, int>>::value, "emilib4 iterators are forward (documented)");
    static_assert(bidirectional_iterator<set8<int>>::value, "set8 iterators must stay bidirectional");
    static_assert(!bidirectional_iterator<set2<int>>::value, "set2 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<set4<int>>::value, "set4 iterators are forward (documented)");
    static_assert(!bidirectional_iterator<iset2<int>>::value, "emilib2 set iterators are forward (documented)");
    static_assert(!bidirectional_iterator<iset3<int>>::value, "emilib3 set iterators are forward (documented)");
    CHECK(true);
}
