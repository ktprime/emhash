// common/maps.hpp
// Unified type aliases and type lists for all emhash/emilib map & set implementations.
// Provides doctest::Types lists for TEST_CASE_TEMPLATE so one test body runs against
// every implementation, eliminating per-implementation duplication.
#pragma once

#include <cstddef>
#include <functional>
#include <string>

// emhash map headers
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"

// emilib map headers
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihmap4.hpp"

// emhash set headers
#include "emhash/hash_set2.hpp"
#include "emhash/hash_set4.hpp"
#include "emhash/hash_set8.hpp"

// emilib set headers
#include "emilib/emihset2.hpp"
#include "emilib/emihset3.hpp"

// doctest.h MUST be included by the .cpp before this header (macro type lists are used below).
// Convention: each test .cpp starts with:
//     #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
//     #include "doctest.h"
//     #include "common/maps.hpp"
#ifndef DOCTEST_LIBRARY_INCLUDED
#error "doctest.h must be included before common/maps.hpp (see file comment)"
#endif

// ============================================================================
// Unified map aliases (all HashMap<K, V, HashT, EqT[, AllocT[, Policy]>)
// ============================================================================
template <typename K, typename V, typename H = std::hash<K>> using map5 = emhash5::HashMap<K, V, H>;
template <typename K, typename V, typename H = std::hash<K>> using map6 = emhash6::HashMap<K, V, H>;
template <typename K, typename V, typename H = std::hash<K>> using map7 = emhash7::HashMap<K, V, H>;
template <typename K, typename V, typename H = std::hash<K>> using map8 = emhash8::HashMap<K, V, H>;
template <typename K, typename V, typename H = std::hash<K>> using imap1 = emilib::HashMap<K, V, H>;
template <typename K, typename V, typename H = std::hash<K>> using imap2 = emilib2::HashMap<K, V, H>;
template <typename K, typename V, typename H = std::hash<K>> using imap3 = emilib3::HashMap<K, V, H>;
template <typename K, typename V, typename H = std::hash<K>> using imap4 = emilib4::HashMap<K, V, H>;

// ============================================================================
// Unified set aliases (all HashSet<K, HashT, EqT>)
// ============================================================================
template <typename K, typename H = std::hash<K>> using set2 = emhash2::HashSet<K, H>;
template <typename K, typename H = std::hash<K>> using set4 = emhash4::HashSet<K, H>;
template <typename K, typename H = std::hash<K>> using set8 = emhash8::HashSet<K, H>;
template <typename K, typename H = std::hash<K>> using iset2 = emilib2::HashSet<K, H>;
template <typename K, typename H = std::hash<K>> using iset3 = emilib3::HashSet<K, H>;

// ============================================================================
// Type lists for TEST_CASE_TEMPLATE
// ============================================================================

// All 8 map implementations with int->int
#define AllIntMaps                                                                                                     \
    map5<int, int>, map6<int, int>, map7<int, int>, map8<int, int>, imap1<int, int>, imap2<int, int>, imap3<int, int>, \
        imap4<int, int>

// All 8 map implementations with std::string->int (covers emilib gap)
#define AllStringMaps                                                                                                  \
    map5<std::string, int>, map6<std::string, int>, map7<std::string, int>, map8<std::string, int>,                    \
        imap1<std::string, int>, imap2<std::string, int>, imap3<std::string, int>, imap4<std::string, int>

// All 5 set implementations with int
#define AllIntSets set2<int>, set4<int>, set8<int>, iset2<int>, iset3<int>

// All 5 set implementations with std::string
#define AllStringSets set2<std::string>, set4<std::string>, set8<std::string>, iset2<std::string>, iset3<std::string>

// Subset: only emhash5-8 (used where emilib lacks an API, e.g. some allocators)
#define EmhashIntMaps map5<int, int>, map6<int, int>, map7<int, int>, map8<int, int>

// Subset: only emhash2/4/8 sets (used where emilib sets lack an API, e.g. merge)
#define EmhashIntSets set2<int>, set4<int>, set8<int>
