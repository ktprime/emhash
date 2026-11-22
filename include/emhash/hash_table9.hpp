// version 0.1.0
// https://github.com/ktprime/emhash/blob/master/hash_table9.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2026 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

/// @file hash_table9.hpp
/// @brief Group index + dense pairs hash map (emhash9)
///
/// The index is a flat array of 88-byte blocks, each covering one group of 16
/// slots:
///
///     struct block {
///         uint8_t  fingerprints[16]; // low hash byte, 0 means empty
///         uint8_t  reach[8];         // one max probe step per hash class
///         uint32_t index[16];        // into the dense value vector
///     };
///
/// A lookup compares all sixteen fingerprints with one SIMD instruction.  The
/// reach array holds one byte per hash class (the low three bits of the hash):
/// the furthest step any key of that class homing in this group had to probe.
/// Once the walk passes the reach of its own class the key is absent.  A reach
/// only ever grows, so a stale value costs extra probes but can never hide a
/// key, and erase never has to repair it.  Both the fingerprints and the reaches
/// live in the first cache line, so a miss reads its bound without touching the
/// line that holds the value indices.
///
/// The blocks and the dense values share one allocation while their combined
/// size stays below EMH9_SPLIT_THRESHOLD (1 MiB).  A larger table allocates the
/// index and the values separately, so the (small) index does not share pages
/// with the (large) value buffer; every growth then costs one allocation each.
///
/// This is the "group index" described by ankerl::unordered_dense 5.0
/// (https://github.com/martinus/unordered_dense/blob/main/doc/design.md),
/// reimplemented on top of emhash's allocator and API conventions.
///
/// @note Header-only: just `#include "emhash/hash_table9.hpp"` and use `emhash9::HashMap`.
/// @note Not thread-safe. Concurrent read-only access is safe.

#pragma once

#ifdef __has_include
#if __has_include("config.hpp")
#include "config.hpp"
#elif __has_include("emhash/config.hpp")
#include "emhash/config.hpp"
#endif
#else
#include "config.hpp"
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <new>

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <emmintrin.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// Defined when this header provides hash()-precomputed lookup overloads
// (find/contains/count/erase taking a key hash from map.hash(key)).
#define EMH_HASH_LOOKUP 1

#ifndef EMH9_GROUP_SIZE
#define EMH9_GROUP_SIZE 16
#endif

// Fetch the second cache line of a group before its fingerprints are compared.
// The value index of a matching lane usually lives in that line, and issuing
// the load early hides its latency behind the SIMD compare.
#ifndef EMH9_PREFETCH_GROUP
#define EMH9_PREFETCH_GROUP 1
#endif

#ifndef EMH9_MAX_LOAD_FACTOR
#define EMH9_MAX_LOAD_FACTOR 0.80f
#endif

// Move an element back into its home group when a write-path lookup finds it
// in a later group and home has room.  Recovers most of the probe-length drift
// a churned table accumulates, at the cost of one load and two stores.
#ifndef EMH9_HOME_RELOCATE
#define EMH9_HOME_RELOCATE 1
#endif

// The probe loop must be inlined into find()/insert(): at -O2 a large TU
// otherwise emits an out-of-line call, which keeps the group index in memory
// and lengthens the dependent load chain measured by the lookup.
#if defined(_MSC_VER)
#define EMH9_FORCEINLINE __forceinline
#define EMH9_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define EMH9_FORCEINLINE __attribute__((always_inline)) inline
#define EMH9_NOINLINE __attribute__((noinline))
#else
#define EMH9_FORCEINLINE inline
#define EMH9_NOINLINE
#endif

namespace emhash9 {

/// One group of sixteen slots: the fingerprints, one probe reach byte per hash
/// class, and the value indices.  All sixteen slots hold entries.
struct group_block {
    uint8_t fingerprints[EMH9_GROUP_SIZE]; // 0 = empty, otherwise low hash byte (0 mapped to 8)
    uint8_t reach[8];                      // one max probe step per hash class
    uint32_t index[EMH9_GROUP_SIZE];       // position in the dense value vector
};

static_assert(sizeof(group_block) == 88, "group_block must stay a single 88-byte unit");

/// A reach of 255 means "a key of this class probed 15 groups or further
/// from its home group", so lookups scan until the group count is exhausted.
constexpr uint32_t REACH_UNBOUNDED = 255;

namespace detail {

/// Index of the lowest set bit. The argument must be non-zero.
inline uint32_t ctz32(uint32_t x) noexcept {
#if defined(_MSC_VER)
    unsigned long i;
    _BitScanForward(&i, x);
    return static_cast<uint32_t>(i);
#else
    return static_cast<uint32_t>(__builtin_ctz(x));
#endif
}

/// Bit i set when byte i of `bytes` equals `value`.
inline uint32_t match_byte(const uint8_t* bytes, uint8_t value) noexcept {
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    const __m128i haystack = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bytes));
    const __m128i needle = _mm_set1_epi8(static_cast<char>(value));
    return static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(haystack, needle)));
#else
    uint32_t mask = 0;
    for (uint32_t i = 0; i < EMH9_GROUP_SIZE; ++i)
        mask |= static_cast<uint32_t>(bytes[i] == value) << i;
    return mask;
#endif
}

/// Stored fingerprint: the low hash byte, with 0 remapped to 8 so that 0 can
/// mean "empty" while the low three bits (the hash class) stay unchanged.
inline uint8_t fingerprint(uint64_t hash) noexcept {
    const uint8_t fp = static_cast<uint8_t>(hash);
    return fp ? fp : 8;
}

/// The fingerprint of every hash byte replicated into all four bytes of a
/// 32-bit word.  Broadcasting that word costs one `movd` + `pshufd`, where
/// building it arithmetically costs a select plus three unpack/shuffle steps on
/// the critical path of every probe.
inline constexpr std::array<uint32_t, 256> make_fingerprint_words() noexcept {
    std::array<uint32_t, 256> words{};
    for (uint32_t i = 0; i < 256; ++i)
        words[i] = (i == 0 ? 8u : i) * 0x01010101u;
    return words;
}

inline constexpr std::array<uint32_t, 256> fingerprint_words = make_fingerprint_words();

/// Bit i set when byte i of `bytes` equals the repeated byte of `word`.
inline uint32_t match_fingerprint(const uint8_t* bytes, uint32_t word) noexcept {
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    const __m128i haystack = _mm_loadu_si128(reinterpret_cast<const __m128i*>(bytes));
    return static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(haystack, _mm_set1_epi32(static_cast<int>(word)))));
#else
    return match_byte(bytes, static_cast<uint8_t>(word));
#endif
}

/// Pull the second half of a group (from byte 64, i.e. index[10..15]) into L1
/// while the fingerprints are being compared.
inline void prefetch_group(const void* block) noexcept {
#if EMH9_PREFETCH_GROUP
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(static_cast<const char*>(block) + 64);
#elif defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
    _mm_prefetch(reinterpret_cast<const char*>(block) + 64, _MM_HINT_T0);
#else
    (void)block;
#endif
#else
    (void)block;
#endif
}

/// True when HashT declares `is_avalanching`, the marker absl, folly and
/// ankerl::unordered_dense use for "every input bit already reaches every
/// output bit".  Those hashes are used as-is.
template <typename H, typename = void> struct is_avalanching : std::false_type {};

template <typename H> struct is_avalanching<H, std::void_t<typename H::is_avalanching>> : std::true_type {};

template <typename H> inline constexpr bool is_avalanching_v = is_avalanching<H>::value;

/// MurmurHash3's 64-bit finalizer.  Two multiplies and three shifts turn a
/// structured hash into one whose every bit depends on every input bit, which
/// is what the fingerprint byte and the group index need.
inline uint64_t mix64(uint64_t x) noexcept {
    x ^= x >> 33;
    x *= UINT64_C(0xff51afd7ed558ccd);
    x ^= x >> 33;
    x *= UINT64_C(0xc4ceb9fe1a85ec53);
    x ^= x >> 33;
    return x;
}

#if defined(EMH9_ERASE_STATS)
/// Filled by erase() when the header is built with -DEMH9_ERASE_STATS: how
/// often the iterator's coordinates were still valid, and how many slot
/// probes the whole run needed.  Declared only under that macro.
struct erase_stats {
    static inline uint64_t erases = 0;
    static inline uint64_t hint_hits = 0;
    static inline uint64_t hint_misses = 0;
    static inline uint64_t slot_probes = 0;
};
#endif

} // namespace detail

template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>,
          typename AllocT = std::allocator<std::pair<KeyT, ValueT>>>
class HashMap {
    static_assert(std::is_copy_constructible<KeyT>::value || std::is_move_constructible<KeyT>::value,
                  "KeyT must be copy-constructible or move-constructible");
    static_assert(std::is_copy_constructible<ValueT>::value || std::is_move_constructible<ValueT>::value,
                  "ValueT must be copy-constructible or move-constructible");
    static_assert(std::is_invocable_v<HashT, const KeyT&>, "HashT must be callable with const KeyT&");
    static_assert(std::is_invocable_v<EqT, const KeyT&, const KeyT&>,
                  "HashT must be callable with (const KeyT&, const KeyT&)");

public:
    using key_type = KeyT;
    using mapped_type = ValueT;
    using value_type = std::pair<KeyT, ValueT>;
    using size_type = uint32_t;
    using difference_type = std::ptrdiff_t;
    using hasher = HashT;
    using key_equal = EqT;
    using allocator_type = AllocT;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    static constexpr size_type INVALID = size_type(-1);
    static constexpr size_type GROUP_SIZE = EMH9_GROUP_SIZE;
    static constexpr uint64_t HASH_SEED = UINT64_C(0x9E3779B97F4A7C15);

    /// Iterator over the dense value array.
    ///
    /// A dense map cannot derive an element's index slot from its position, so
    /// an erase that only receives a pointer has to hash the key and probe for
    /// the slot it already found once.  The iterator therefore carries the
    /// coordinates of the slot when the operation that produced it knew them
    /// (find, insert, erase's return value), and `invalidate`s them as soon as
    /// the iterator is stepped, since neighbours live in unrelated slots.
    /// Erase uses them only after checking that the slot still holds this
    /// value index, so a stale hint costs one cached load, never correctness.
    template <bool Const> class iter {
        using elem_type = std::conditional_t<Const, const value_type, value_type>;

        elem_type* _ptr = nullptr;
        uint32_t _group = INVALID;
        uint32_t _slot = INVALID;

        friend class HashMap;

        constexpr iter(elem_type* ptr, uint32_t group, uint32_t slot) noexcept
            : _ptr(ptr), _group(group), _slot(slot) {}

    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = typename HashMap::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = elem_type*;
        using reference = elem_type&;

        constexpr iter() noexcept = default;

        /// iterator -> const_iterator
        template <bool C = Const, typename = std::enable_if_t<C>>
        constexpr iter(const iter<false>& other) noexcept
            : _ptr(other._ptr), _group(other._group), _slot(other._slot) {}

        constexpr reference operator*() const noexcept { return *_ptr; }
        constexpr pointer operator->() const noexcept { return _ptr; }
        constexpr reference operator[](difference_type n) const noexcept { return _ptr[n]; }

        iter& operator++() noexcept {
            ++_ptr;
            _group = _slot = INVALID;
            return *this;
        }
        iter operator++(int) noexcept {
            iter tmp = *this;
            ++*this;
            return tmp;
        }
        iter& operator--() noexcept {
            --_ptr;
            _group = _slot = INVALID;
            return *this;
        }
        iter operator--(int) noexcept {
            iter tmp = *this;
            --*this;
            return tmp;
        }
        iter& operator+=(difference_type n) noexcept {
            _ptr += n;
            _group = _slot = INVALID;
            return *this;
        }
        iter& operator-=(difference_type n) noexcept {
            _ptr -= n;
            _group = _slot = INVALID;
            return *this;
        }

        friend iter operator+(iter it, difference_type n) noexcept { return it += n; }
        friend iter operator+(difference_type n, iter it) noexcept { return it += n; }
        friend iter operator-(iter it, difference_type n) noexcept { return it -= n; }
        friend difference_type operator-(const iter& a, const iter& b) noexcept { return a._ptr - b._ptr; }

        friend bool operator==(const iter& a, const iter& b) noexcept { return a._ptr == b._ptr; }
        friend bool operator!=(const iter& a, const iter& b) noexcept { return a._ptr != b._ptr; }
        friend bool operator<(const iter& a, const iter& b) noexcept { return a._ptr < b._ptr; }
        friend bool operator>(const iter& a, const iter& b) noexcept { return a._ptr > b._ptr; }
        friend bool operator<=(const iter& a, const iter& b) noexcept { return a._ptr <= b._ptr; }
        friend bool operator>=(const iter& a, const iter& b) noexcept { return a._ptr >= b._ptr; }
    };

    using iterator = iter<false>;
    using const_iterator = iter<true>;

private:
    /// The group index and the value buffer share a single allocation while
    /// their combined byte size stays below EMH9_SPLIT_THRESHOLD: the index
    /// comes first, the values follow it.  One allocation per grow skips an
    /// allocator round trip, and both regions land on the same pages, so a
    /// probe that reads a group and then its value faults in half the pages a
    /// second allocation would have needed.  Above the threshold the two are
    /// allocated separately, which keeps the index out of the value buffer's
    /// pages and lets a large table free the two at their own sizes.
#ifndef EMH9_SPLIT_THRESHOLD
#define EMH9_SPLIT_THRESHOLD (1u << 20)
#endif

    static constexpr std::size_t kArenaAlign = alignof(value_type) > alignof(group_block) ? alignof(value_type)
                                                                                          : alignof(group_block);

    /// One unit of the single-allocation path, aligned for both the index and
    /// the values; the allocator is rebound to it so `allocate` honours that
    /// alignment.
    struct alignas(kArenaAlign) arena_unit {
        unsigned char bytes[kArenaAlign];
    };
    static_assert(sizeof(arena_unit) == kArenaAlign, "arena_unit must not carry padding");

    using PairAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<value_type>;
    using PairAllocTraits = std::allocator_traits<PairAlloc>;
    using IndexAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<group_block>;
    using IndexAllocTraits = std::allocator_traits<IndexAlloc>;
    using ArenaAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<arena_unit>;
    using ArenaAllocTraits = std::allocator_traits<ArenaAlloc>;

    PairAlloc _alloc;
    value_type* _pairs = nullptr;
    group_block* _blocks = nullptr;
    size_type _num_filled = 0;
    size_type _capacity = 0;   // value slots allocated in _pairs
    size_type _num_groups = 0; // power of two
    size_type _mask = 0;
    size_type _max_filled = 0; // groups * GROUP_SIZE * max_load_factor
    float _max_load_factor = EMH9_MAX_LOAD_FACTOR;
    mutable hasher _hasher;
    mutable key_equal _key_equal;

public:
    // ------------------------------------------------------------------
    // construction / destruction
    // ------------------------------------------------------------------
    HashMap() = default;

    explicit HashMap(const AllocT& alloc) : _alloc(alloc) {}

    /// Pre-allocate room for about `bucket_count` elements.
    explicit HashMap(size_type bucket_count) { reserve(bucket_count); }

    HashMap(const HashMap& other)
        : _alloc(PairAllocTraits::select_on_container_copy_construction(other._alloc)),
          _max_load_factor(other._max_load_factor), _hasher(other._hasher), _key_equal(other._key_equal) {
        clone_from(other);
    }

    HashMap(HashMap&& other) noexcept(std::is_nothrow_move_constructible<PairAlloc>::value)
        : _alloc(std::move(other._alloc)), _pairs(other._pairs), _blocks(other._blocks), _num_filled(other._num_filled),
          _capacity(other._capacity), _num_groups(other._num_groups), _mask(other._mask),
          _max_filled(other._max_filled), _max_load_factor(other._max_load_factor), _hasher(std::move(other._hasher)),
          _key_equal(std::move(other._key_equal)) {
        other._pairs = nullptr;
        other._blocks = nullptr;
        other._num_filled = other._capacity = other._num_groups = other._mask = other._max_filled = 0;
    }

    template <typename It, typename = std::enable_if_t<!std::is_integral_v<It>>> HashMap(It first, It last) {
        reserve(static_cast<size_type>(std::distance(first, last)));
        for (; first != last; ++first)
            insert(*first);
    }

    HashMap(std::initializer_list<value_type> list) {
        reserve(static_cast<size_type>(list.size()));
        for (const auto& v : list)
            insert(v);
    }

    ~HashMap() { destroy_all(); }

    HashMap& operator=(const HashMap& other) {
        if (this == &other)
            return *this;
        if constexpr (PairAllocTraits::propagate_on_container_copy_assignment::value) {
            if (_alloc != other._alloc) {
                destroy_all();
                _alloc = other._alloc;
            }
        }
        clear();
        _max_load_factor = other._max_load_factor;
        _hasher = other._hasher;
        _key_equal = other._key_equal;
        clone_from(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept(std::is_nothrow_move_assignable<PairAlloc>::value) {
        if (this == &other)
            return *this;
        destroy_all();
        if constexpr (PairAllocTraits::propagate_on_container_move_assignment::value)
            _alloc = std::move(other._alloc);
        _pairs = other._pairs;
        _blocks = other._blocks;
        _num_filled = other._num_filled;
        _capacity = other._capacity;
        _num_groups = other._num_groups;
        _mask = other._mask;
        _max_filled = other._max_filled;
        _max_load_factor = other._max_load_factor;
        _hasher = std::move(other._hasher);
        _key_equal = std::move(other._key_equal);
        other._pairs = nullptr;
        other._blocks = nullptr;
        other._num_filled = other._capacity = other._num_groups = other._mask = other._max_filled = 0;
        return *this;
    }

    HashMap& operator=(std::initializer_list<value_type> list) {
        clear();
        reserve(static_cast<size_type>(list.size()));
        for (const auto& v : list)
            insert(v);
        return *this;
    }

    allocator_type get_allocator() const { return allocator_type(_alloc); }

    // ------------------------------------------------------------------
    // iterators
    // ------------------------------------------------------------------
    iterator begin() noexcept { return iterator(_pairs, INVALID, INVALID); }
    iterator end() noexcept { return iterator(_pairs + _num_filled, INVALID, INVALID); }
    const_iterator begin() const noexcept { return const_iterator(_pairs, INVALID, INVALID); }
    const_iterator end() const noexcept { return const_iterator(_pairs + _num_filled, INVALID, INVALID); }
    const_iterator cbegin() const noexcept { return const_iterator(_pairs, INVALID, INVALID); }
    const_iterator cend() const noexcept { return const_iterator(_pairs + _num_filled, INVALID, INVALID); }

    // ------------------------------------------------------------------
    // capacity
    // ------------------------------------------------------------------
    [[nodiscard]] bool empty() const noexcept { return _num_filled == 0; }
    [[nodiscard]] size_type size() const noexcept { return _num_filled; }
    [[nodiscard]] size_type max_size() const noexcept { return INVALID - 1; }
    [[nodiscard]] size_type bucket_count() const noexcept { return static_cast<size_type>(_num_groups) * GROUP_SIZE; }
    [[nodiscard]] size_type group_count() const noexcept { return _num_groups; }

    [[nodiscard]] float load_factor() const noexcept {
        return _num_groups ? static_cast<float>(_num_filled) / static_cast<float>(bucket_count()) : 0.0f;
    }

    [[nodiscard]] float max_load_factor() const noexcept { return _max_load_factor; }

    void max_load_factor(float mlf) {
        if (mlf <= 0.1f)
            mlf = 0.1f;
        if (mlf > 0.999f)
            mlf = 0.999f;
        _max_load_factor = mlf;
        refresh_max_filled();
        if (_num_filled > max_filled())
            grow_groups(_num_groups ? _num_groups * 2 : 1);
    }

    void reserve(size_type n) {
        if (n <= _num_filled)
            return;
        if (n > max_size())
            throw std::length_error("emhash9::HashMap::reserve");
        const size_type groups = groups_for(n);
        if (groups > _num_groups)
            grow_groups(groups);
        else if (n > _capacity)
            reallocate_pairs(n);
    }

    void rehash(size_type n) {
        const size_type want = groups_for(n > _num_filled ? n : _num_filled);
        if (want == _num_groups)
            return;
        set_groups(want);
    }

    void rehash(size_type n, bool force) {
        if (force)
            set_groups(groups_for(n > _num_filled ? n : _num_filled));
        else
            rehash(n);
    }

    // ------------------------------------------------------------------
    // observers
    // ------------------------------------------------------------------
    [[nodiscard]] hasher hash_function() const { return _hasher; }
    [[nodiscard]] key_equal key_eq() const { return _key_equal; }

    /// Hash a key exactly as the map does, for use with the (hash, key) overloads.
    template <typename K> [[nodiscard]] uint64_t hash(const K& key) const { return hash_key(key); }

    // ------------------------------------------------------------------
    // lookup
    // ------------------------------------------------------------------
    [[nodiscard]] EMH9_FORCEINLINE iterator find(const key_type& key) {
        uint32_t g, i;
        const size_type idx = index_of(hash_key(key), key, g, i);
        return idx == INVALID ? end() : iterator(_pairs + idx, g, i);
    }

    [[nodiscard]] EMH9_FORCEINLINE const_iterator find(const key_type& key) const {
        uint32_t g, i;
        const size_type idx = index_of(hash_key(key), key, g, i);
        return idx == INVALID ? end() : const_iterator(_pairs + idx, g, i);
    }

    /// Heterogeneous lookup: enabled when HashT and EqT accept K.
    template <typename K, typename = std::enable_if_t<!std::is_same_v<std::decay_t<K>, key_type>>,
              typename = std::enable_if_t<std::is_invocable_v<const hasher&, const K&>>>
    [[nodiscard]] EMH9_FORCEINLINE iterator find(const K& key) {
        uint32_t g, i;
        const size_type idx = index_of(hash_key(key), key, g, i);
        return idx == INVALID ? end() : iterator(_pairs + idx, g, i);
    }

    template <typename K, typename = std::enable_if_t<!std::is_same_v<std::decay_t<K>, key_type>>,
              typename = std::enable_if_t<std::is_invocable_v<const hasher&, const K&>>>
    [[nodiscard]] EMH9_FORCEINLINE const_iterator find(const K& key) const {
        uint32_t g, i;
        const size_type idx = index_of(hash_key(key), key, g, i);
        return idx == INVALID ? end() : const_iterator(_pairs + idx, g, i);
    }

    /// Lookup with a hash already computed by hash(key).
    [[nodiscard]] EMH9_FORCEINLINE iterator find(uint64_t prehash, const key_type& key) {
        uint32_t g, i;
        const size_type idx = index_of(prehash, key, g, i);
        return idx == INVALID ? end() : iterator(_pairs + idx, g, i);
    }

    [[nodiscard]] EMH9_FORCEINLINE const_iterator find(uint64_t prehash, const key_type& key) const {
        uint32_t g, i;
        const size_type idx = index_of(prehash, key, g, i);
        return idx == INVALID ? end() : const_iterator(_pairs + idx, g, i);
    }

    [[nodiscard]] EMH9_FORCEINLINE bool contains(const key_type& key) const {
        return index_of(hash_key(key), key) != INVALID;
    }

    template <typename K, typename = std::enable_if_t<!std::is_same_v<std::decay_t<K>, key_type>>,
              typename = std::enable_if_t<std::is_invocable_v<const hasher&, const K&>>>
    [[nodiscard]] EMH9_FORCEINLINE bool contains(const K& key) const {
        return index_of(hash_key(key), key) != INVALID;
    }

    [[nodiscard]] EMH9_FORCEINLINE size_type count(const key_type& key) const {
        return index_of(hash_key(key), key) != INVALID ? 1 : 0;
    }

    template <typename K, typename = std::enable_if_t<!std::is_same_v<std::decay_t<K>, key_type>>,
              typename = std::enable_if_t<std::is_invocable_v<const hasher&, const K&>>>
    [[nodiscard]] EMH9_FORCEINLINE size_type count(const K& key) const {
        return index_of(hash_key(key), key) != INVALID ? 1 : 0;
    }

    [[nodiscard]] std::pair<iterator, iterator> equal_range(const key_type& key) {
        auto it = find(key);
        return it == end() ? std::make_pair(it, it) : std::make_pair(it, it + 1);
    }

    [[nodiscard]] std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        auto it = find(key);
        return it == end() ? std::make_pair(it, it) : std::make_pair(it, it + 1);
    }

    [[nodiscard]] mapped_type* try_get(const key_type& key) {
        auto it = find(key);
        return it == end() ? nullptr : &it->second;
    }

    [[nodiscard]] const mapped_type* try_get(const key_type& key) const {
        auto it = find(key);
        return it == end() ? nullptr : &it->second;
    }

    mapped_type& at(const key_type& key) {
        auto it = find(key);
        if (it == end())
            throw std::out_of_range("emhash9::HashMap::at");
        return it->second;
    }

    const mapped_type& at(const key_type& key) const {
        auto it = find(key);
        if (it == end())
            throw std::out_of_range("emhash9::HashMap::at");
        return it->second;
    }

    /// The subscript goes through the insert core directly: it needs the value,
    /// not the public pair<iterator, bool>, and that pair is too large to be
    /// returned in registers.
    EMH9_FORCEINLINE mapped_type& operator[](const key_type& key) {
        return insert_core<false>(hash_key(key), key,
                                  [&](value_type* p) {
                                      ::new (static_cast<void*>(p))
                                          value_type(std::piecewise_construct, std::forward_as_tuple(key),
                                                     std::forward_as_tuple());
                                  })
            ->second;
    }

    EMH9_FORCEINLINE mapped_type& operator[](key_type&& key) {
        return insert_core<false>(hash_key(key), key,
                                  [&](value_type* p) {
                                      ::new (static_cast<void*>(p))
                                          value_type(std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                                                     std::forward_as_tuple());
                                  })
            ->second;
    }

    // ------------------------------------------------------------------
    // modifiers
    // ------------------------------------------------------------------
    void clear() noexcept {
        for (size_type i = 0; i < _num_filled; ++i)
            _pairs[i].~value_type();
        _num_filled = 0;
        if (_blocks)
            std::memset(_blocks, 0, sizeof(group_block) * _num_groups);
    }

    std::pair<iterator, bool> insert(const value_type& value) { return insert_value(value); }

    std::pair<iterator, bool> insert(value_type&& value) { return insert_value(std::move(value)); }

    template <typename P, typename = std::enable_if_t<std::is_constructible_v<value_type, P&&>>>
    std::pair<iterator, bool> insert(P&& value) {
        return insert_value(value_type(std::forward<P>(value)));
    }

    template <typename It> void insert(It first, It last) {
        for (; first != last; ++first)
            insert(*first);
    }

    void insert(std::initializer_list<value_type> list) {
        for (const auto& v : list)
            insert(v);
    }

    template <typename... Args> std::pair<iterator, bool> emplace(Args&&... args) {
        return insert_value(value_type(std::forward<Args>(args)...));
    }

    template <typename... Args> iterator emplace_hint(const_iterator, Args&&... args) {
        return emplace(std::forward<Args>(args)...).first;
    }

    template <typename... Args>
    EMH9_FORCEINLINE std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
        const uint64_t h = hash_key(key);
        return insert_impl(h, key, [&](value_type* p) {
            ::new (static_cast<void*>(p)) value_type(std::piecewise_construct, std::forward_as_tuple(key),
                                                     std::forward_as_tuple(std::forward<Args>(args)...));
        });
    }

    template <typename... Args> EMH9_FORCEINLINE std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
        const uint64_t h = hash_key(key);
        return insert_impl(h, key, [&](value_type* p) {
            ::new (static_cast<void*>(p)) value_type(std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                                                     std::forward_as_tuple(std::forward<Args>(args)...));
        });
    }

    template <typename M> std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
        auto r = try_emplace(key, std::forward<M>(obj));
        if (!r.second)
            r.first->second = std::forward<M>(obj);
        return r;
    }

    template <typename M> std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
        auto r = try_emplace(std::move(key), std::forward<M>(obj));
        if (!r.second)
            r.first->second = std::forward<M>(obj);
        return r;
    }

    EMH9_FORCEINLINE iterator erase(const_iterator it) {
        if (it == end())
            return end();
        const size_type idx = static_cast<size_type>(it._ptr - _pairs);
        const size_type last = _num_filled - 1;
        const uint64_t h = hash_key(_pairs[idx].first);

        // The coordinates are a hint: any mutation after this iterator was
        // produced may have swapped a different value into that slot, so they
        // are used only when the slot still holds this value index.  A dead
        // hint costs one cached load and falls back to the probe.
        uint32_t g = it._group;
        uint32_t i = it._slot;
        const bool hinted =
            g < _num_groups && i < GROUP_SIZE && _blocks[g].index[i] == idx && _blocks[g].fingerprints[i] != 0;
#if defined(EMH9_ERASE_STATS)
        ++detail::erase_stats::erases;
        if (hinted)
            ++detail::erase_stats::hint_hits;
        else
            ++detail::erase_stats::hint_misses;
#endif
        if (!hinted)
            slot_of_value(h, idx, g, i);

        uint32_t moved_g = INVALID, moved_i = INVALID;
        if (idx != last) {
            // The last key is hashed up front: that load is independent of the
            // probe above, so one stall pays for both.
            const uint64_t moved_hash = hash_key(_pairs[last].first);
            slot_of_value(moved_hash, last, moved_g, moved_i);
        }
        erase_slot(g, i, moved_g, moved_i);
        // The value that was last now lives at idx, and its coordinates are
        // known, so a caller that erases in a loop skips the next probe.
        return idx < _num_filled ? iterator(_pairs + idx, moved_g, moved_i)
                                 : iterator(_pairs + _num_filled, INVALID, INVALID);
    }

    size_type erase(const key_type& key) { return erase_key(hash_key(key), key); }

    template <typename K, typename = std::enable_if_t<!std::is_same_v<std::decay_t<K>, key_type>>,
              typename = std::enable_if_t<std::is_invocable_v<const hasher&, const K&>>>
    size_type erase(const K& key) {
        return erase_key(hash_key(key), key);
    }

    /// Void-returning erase by iterator: same work as erase(it), without
    /// computing the follow-up iterator.
    void _erase(const_iterator it) noexcept { (void)erase(it); }

    /// Erase every element for which `pred(value_type)` holds.
    /// @return the number of erased elements.
    template <typename Pred> size_type erase_if(Pred pred) {
        const size_type old_size = _num_filled;
        for (auto it = begin(); it != end();) {
            // erase() moves the last element into `it`, so `it` already points
            // at the next element to visit (or at end()).
            if (pred(*it))
                it = erase(it);
            else
                ++it;
        }
        return old_size - _num_filled;
    }

    /// Insert without checking whether the key is already present.
    /// @return the index of the new element in the value vector.
    /// @pre The key must NOT already exist; a duplicate corrupts the index.
    template <typename K, typename V> size_type insert_unique(K&& key, V&& val) {
        const uint64_t h = hash_key(key);
        return insert_new(h, [&](value_type* p) {
            ::new (static_cast<void*>(p)) value_type(std::forward<K>(key), std::forward<V>(val));
        });
    }

    size_type insert_unique(value_type&& value) {
        return insert_unique(std::move(value.first), std::move(value.second));
    }

    size_type insert_unique(const value_type& value) { return insert_unique(value.first, value.second); }

    /// Move every element of `rhs` that has no equivalent key here, erasing the
    /// moved ones from `rhs`.  Elements with a duplicate key stay in `rhs`.
    void merge(HashMap& rhs) {
        if (rhs.empty())
            return;
        if (empty()) {
            *this = std::move(rhs);
            return;
        }
        for (auto it = rhs.begin(); it != rhs.end();) {
            // Both branches are safe for a self-merge: every key is found.
            if (find(it->first) == end()) {
                (void)insert_unique(it->first, std::move(it->second));
                it = rhs.erase(it);
            } else {
                ++it;
            }
        }
    }

    /// Shrink the value buffer and the index when the table is sparsely filled.
    void shrink_to_fit(float min_factor = EMH9_MAX_LOAD_FACTOR / 4) {
        if (_num_groups <= 1 || load_factor() >= min_factor)
            return;
        const size_type groups = groups_for(_num_filled);
        const size_type cap = groups * GROUP_SIZE;
        if (groups >= _num_groups && cap >= _capacity)
            return;
        if (groups < _num_groups)
            relocate(groups, cap, false);
        else
            relocate(_num_groups, cap, true);
    }

    void swap(HashMap& other) noexcept(std::is_nothrow_swappable_v<PairAlloc>) {
        using std::swap;
        swap(_pairs, other._pairs);
        swap(_blocks, other._blocks);
        swap(_num_filled, other._num_filled);
        swap(_capacity, other._capacity);
        swap(_num_groups, other._num_groups);
        swap(_mask, other._mask);
        swap(_max_filled, other._max_filled);
        swap(_max_load_factor, other._max_load_factor);
        swap(_hasher, other._hasher);
        swap(_key_equal, other._key_equal);
        if constexpr (PairAllocTraits::propagate_on_container_swap::value)
            swap(_alloc, other._alloc);
    }

    friend void swap(HashMap& a, HashMap& b) noexcept(noexcept(a.swap(b))) { a.swap(b); }

    friend bool operator==(const HashMap& a, const HashMap& b) {
        if (a.size() != b.size())
            return false;
        for (const auto& kv : a) {
            auto it = b.find(kv.first);
            if (it == b.end() || !(it->second == kv.second))
                return false;
        }
        return true;
    }

    friend bool operator!=(const HashMap& a, const HashMap& b) { return !(a == b); }

private:
    // ------------------------------------------------------------------
    // hashing
    // ------------------------------------------------------------------
    template <typename K> [[nodiscard]] uint64_t hash_key(const K& key) const {
        const uint64_t h = static_cast<uint64_t>(_hasher(key));
        // A murmur3-style finalizer costs two multiplies and buys the property
        // the index depends on: every key bit reaches every hash bit.  Without
        // it, keys that differ only in bits 16..31 (multiples of 65536, say)
        // would share a fingerprint byte, and each probe would compare all
        // sixteen slots.  A hasher that already avalanches is left alone.
        if constexpr (detail::is_avalanching_v<HashT>)
            return h;
        return detail::mix64(h);
    }

    [[nodiscard]] static uint64_t home_of(uint64_t hash, size_type mask) noexcept {
        return static_cast<uint32_t>(hash >> 32) & static_cast<uint32_t>(mask);
    }

    // ------------------------------------------------------------------
    // capacity helpers
    // ------------------------------------------------------------------
    /// The insert path tests this once per element, so the value is kept
    /// precomputed instead of being derived from the float load factor there.
    [[nodiscard]] size_type max_filled() const noexcept { return _max_filled; }

    void refresh_max_filled() noexcept {
        const uint32_t scale = static_cast<uint32_t>(_max_load_factor * 65536.0f);
        _max_filled = static_cast<size_type>((static_cast<uint64_t>(_num_groups) * GROUP_SIZE * scale) >> 16);
    }

    [[nodiscard]] size_type groups_for(size_type n) const {
        size_type groups = 1;
        const double needed = static_cast<double>(n) / static_cast<double>(_max_load_factor);
        constexpr size_type kMaxGroups = size_type(1) << 27;
        while (static_cast<double>(groups) * static_cast<double>(GROUP_SIZE) < needed && groups < kMaxGroups)
            groups <<= 1;
        return groups;
    }

    static constexpr std::size_t round_up(std::size_t n, std::size_t align) noexcept {
        return (n + align - 1) / align * align;
    }

    /// Bytes the index occupies inside the arena, padded so the value buffer
    /// behind it starts on a `value_type` boundary.
    static constexpr std::size_t index_bytes(size_type groups) noexcept {
        return round_up(sizeof(group_block) * static_cast<std::size_t>(groups), alignof(value_type));
    }

    /// Furthest step a key of hash class `cls` homing in this group had to
    /// probe.  Zero means every key of the class sits in this group, so a miss
    /// may stop immediately.
    static EMH9_FORCEINLINE uint8_t reach_of(const group_block& block, uint8_t cls) noexcept {
        return block.reach[cls];
    }

    /// Record that a key of `cls` homing in `block` was placed `step` groups away.
    static EMH9_FORCEINLINE void grow_reach(group_block& block, uint8_t cls, uint32_t step) noexcept {
        const uint32_t cur = block.reach[cls];
        if (cur == REACH_UNBOUNDED)
            return;
        const uint32_t next = step >= REACH_UNBOUNDED ? REACH_UNBOUNDED : (step > cur ? step : cur);
        block.reach[cls] = static_cast<uint8_t>(next);
    }

    /// Probe bound implied by a reach byte; the unbounded marker widens to the
    /// table size so the walk cannot stop early.
    static EMH9_FORCEINLINE uint32_t reach_guard(uint8_t reach) noexcept {
        return reach == REACH_UNBOUNDED ? static_cast<uint32_t>(INVALID) : static_cast<uint32_t>(reach);
    }

    static constexpr std::size_t arena_units(size_type groups, size_type cap) noexcept {
        const std::size_t bytes = index_bytes(groups) + sizeof(value_type) * static_cast<std::size_t>(cap);
        return (bytes + sizeof(arena_unit) - 1) / sizeof(arena_unit);
    }

    static value_type* pairs_in(char* base, size_type groups) noexcept {
        return reinterpret_cast<value_type*>(base + index_bytes(groups));
    }

    /// True when the index and the values are allocated separately.  It is a
    /// pure function of the stored shape, so deallocation can recover the mode
    /// from `_num_groups` and `_capacity`; a threshold of 0 forces the split.
    static constexpr bool split_storage(size_type groups, size_type cap) noexcept {
        return EMH9_SPLIT_THRESHOLD == 0 || index_bytes(groups) + sizeof(value_type) * static_cast<std::size_t>(cap) >
                                                static_cast<std::size_t>(EMH9_SPLIT_THRESHOLD);
    }

    group_block* allocate_index(size_type groups) {
        IndexAlloc alloc(_alloc);
        return IndexAllocTraits::allocate(alloc, groups);
    }

    void deallocate_index(group_block* blocks, size_type groups) noexcept {
        if (blocks) {
            IndexAlloc alloc(_alloc);
            IndexAllocTraits::deallocate(alloc, blocks, groups);
        }
    }

    value_type* allocate_pairs(size_type cap) {
        PairAlloc alloc(_alloc);
        return PairAllocTraits::allocate(alloc, cap);
    }

    void deallocate_pairs(value_type* pairs, size_type cap) noexcept {
        if (pairs) {
            PairAlloc alloc(_alloc);
            PairAllocTraits::deallocate(alloc, pairs, cap);
        }
    }

    char* allocate_arena(size_type groups, size_type cap) {
        ArenaAlloc alloc(_alloc);
        return reinterpret_cast<char*>(ArenaAllocTraits::allocate(alloc, arena_units(groups, cap)));
    }

    void deallocate_arena(char* base, size_type groups, size_type cap) noexcept {
        if (base) {
            ArenaAlloc alloc(_alloc);
            ArenaAllocTraits::deallocate(alloc, reinterpret_cast<arena_unit*>(base), arena_units(groups, cap));
        }
    }

    void release_storage(group_block* blocks, value_type* pairs, size_type groups, size_type cap) noexcept {
        if (split_storage(groups, cap)) {
            deallocate_index(blocks, groups);
            deallocate_pairs(pairs, cap);
        } else {
            deallocate_arena(reinterpret_cast<char*>(blocks), groups, cap);
        }
    }

    void destroy_all() noexcept {
        if (_pairs) {
            for (size_type i = 0; i < _num_filled; ++i)
                _pairs[i].~value_type();
        }
        release_storage(_blocks, _pairs, _num_groups, _capacity);
        _pairs = nullptr;
        _blocks = nullptr;
        _num_filled = _capacity = _num_groups = _mask = _max_filled = 0;
    }

    /// Clone the storage of `other` verbatim.  The index is self-contained: the
    /// fingerprints come from the (copied) hasher and the stored indices are
    /// positions inside the value buffer, so no element has to be hashed or
    /// probed again.  This is what keeps `copy = O(n) memcpy` instead of
    /// `O(n)` re-insertions.
    void clone_from(const HashMap& other) {
        if (other._num_filled == 0) {
            destroy_all();
            return;
        }

        // Reuse this table's buffers when they already have the right shape:
        // repeated assignments then skip the allocator and the page faults.
        const bool reuse = _pairs != nullptr && _blocks != nullptr && _num_groups == other._num_groups &&
                           _capacity >= other._num_filled;
        if (reuse) {
            for (size_type i = 0; i < _num_filled; ++i)
                _pairs[i].~value_type();
            _num_filled = 0;
        } else {
            destroy_all();
        }

        const size_type groups = other._num_groups;
        const size_type cap = other._capacity;
        const bool split = split_storage(groups, cap);
        char* base = nullptr;
        group_block* blocks = _blocks;
        value_type* pairs = _pairs;
        if (!reuse) {
            if (split) {
                blocks = allocate_index(groups);
                try {
                    pairs = allocate_pairs(cap);
                } catch (...) {
                    deallocate_index(blocks, groups);
                    throw;
                }
            } else {
                base = allocate_arena(groups, cap);
                blocks = reinterpret_cast<group_block*>(base);
                pairs = pairs_in(base, groups);
            }
        }
        PairAlloc alloc(_alloc);
        size_type built = 0;
        try {
            for (; built < other._num_filled; ++built)
                PairAllocTraits::construct(alloc, pairs + built, other._pairs[built]);
        } catch (...) {
            for (size_type i = 0; i < built; ++i)
                pairs[i].~value_type();
            _num_filled = 0;
            if (!reuse) {
                if (split) {
                    deallocate_index(blocks, groups);
                    deallocate_pairs(pairs, cap);
                } else {
                    deallocate_arena(base, groups, cap);
                }
            }
            throw;
        }

        if (!reuse) {
            _blocks = blocks;
            _pairs = pairs;
            _capacity = cap;
            _num_groups = groups;
        }
        std::memcpy(_blocks, other._blocks, sizeof(group_block) * groups);
        _mask = other._mask;
        _num_filled = other._num_filled;
        refresh_max_filled();
    }

    /// Move the whole table into fresh storage.  `groups` and `cap` describe
    /// the new shape; `keep_index` copies the unchanged index (only the value
    /// buffer changed size) instead of rebuilding it from the moved values.
    void relocate(size_type groups, size_type cap, bool keep_index) {
        const bool split = split_storage(groups, cap);
        char* const base = split ? nullptr : allocate_arena(groups, cap);
        group_block* const nb = split ? allocate_index(groups) : reinterpret_cast<group_block*>(base);
        value_type* const np = split ? allocate_pairs(cap) : pairs_in(base, groups);

        for (size_type i = 0; i < _num_filled; ++i)
            ::new (static_cast<void*>(np + i)) value_type(std::move_if_noexcept(_pairs[i]));

        const size_type nmask = groups - 1;
        if (keep_index) {
            if (_num_groups)
                std::memcpy(nb, _blocks, sizeof(group_block) * _num_groups);
        } else {
            std::memset(nb, 0, sizeof(group_block) * groups);
            for (size_type idx = 0; idx < _num_filled; ++idx)
                place_index(nb, groups, nmask, hash_key(np[idx].first), idx);
        }

        if (_pairs) {
            for (size_type i = 0; i < _num_filled; ++i)
                _pairs[i].~value_type();
        }
        release_storage(_blocks, _pairs, _num_groups, _capacity);

        _blocks = nb;
        _pairs = np;
        _capacity = cap;
        _num_groups = groups;
        _mask = nmask;
        refresh_max_filled();
    }

    /// Same group count, different value capacity: the index stays valid, only
    /// its storage moves, so it is copied rather than rebuilt.
    void reallocate_pairs(size_type cap) { relocate(_num_groups, cap, true); }

    /// Rebuild the index for a different group count.  The values move with it.
    void rebuild_at(size_type groups) { relocate(groups, _capacity, false); }

    void grow_groups(size_type groups) {
        if (groups <= _num_groups)
            return;
        set_groups(groups);
    }

    /// Switch to `groups` groups, growing the value buffer to match.  One
    /// relocation covers both the index and the values.
    void set_groups(size_type groups) {
        const size_type needed = groups * GROUP_SIZE;
        relocate(groups, needed > _capacity ? needed : _capacity, false);
    }

    /// Insert `idx` into a freshly zeroed index and widen the home group's reach
    /// for the key's class to the distance the key had to travel.
    static void place_index(group_block* blocks, size_type groups, size_type mask, uint64_t hash, size_type idx) {
        const uint8_t fp = detail::fingerprint(hash);
        const uint8_t cls = static_cast<uint8_t>(hash) & 7;
        const uint32_t home = static_cast<uint32_t>(home_of(hash, mask));
        uint32_t g = home;
        uint32_t step = 0;
        for (size_type visited = 0; visited < groups; ++visited) {
            group_block& b = blocks[g];
            const uint32_t empties = detail::match_byte(b.fingerprints, 0);
            if (empties) {
                const uint32_t i = detail::ctz32(empties);
                b.fingerprints[i] = fp;
                b.index[i] = static_cast<uint32_t>(idx);
                grow_reach(blocks[home], cls, step);
                return;
            }
            g = (g + (++step)) & static_cast<uint32_t>(mask);
        }
    }

    // ------------------------------------------------------------------
    // lookup core
    // ------------------------------------------------------------------
    /// Lookup that returns the dense value index (or INVALID).  Separate from
    /// find_slot so find() loads the index exactly once and the whole probe can
    /// be inlined; find_slot is kept for erase, which needs (group, slot).
    template <typename K> EMH9_FORCEINLINE size_type index_of(uint64_t hash, const K& key) const {
        uint32_t group, slot;
        return index_of(hash, key, group, slot);
    }

    /// As above, and also returns the coordinates of the index slot holding the
    /// value, so that a following erase does not have to probe for it again.
    /// Callers that only need the value index let the two stores be removed.
    template <typename K>
    EMH9_FORCEINLINE size_type index_of(uint64_t hash, const K& key, uint32_t& group_out, uint32_t& slot_out) const {
        if (EMH_UNLIKELY(_num_groups == 0))
            return INVALID;

        const uint32_t fw = detail::fingerprint_words[static_cast<uint8_t>(hash)];
        const uint32_t home = static_cast<uint32_t>(home_of(hash, _mask));

        // Hit path: probe the home group before touching the probe bound, so a
        // key that never overflowed costs neither the reach load nor its shift.
        const group_block& hb = _blocks[home];
        detail::prefetch_group(&hb);
        uint32_t mask = detail::match_fingerprint(hb.fingerprints, fw);
        while (mask) {
            const uint32_t i = detail::ctz32(mask);
            mask &= mask - 1;
            const size_type idx = hb.index[i];
            if (EMH_LIKELY(_key_equal(_pairs[idx].first, key))) {
                group_out = home;
                slot_out = i;
                return idx;
            }
        }

        // Miss in the home group: the key can only be one of the overflows this
        // group recorded for its hash class.
        const uint32_t guard = reach_guard(reach_of(hb, static_cast<uint8_t>(hash) & 7));
        if (guard == 0)
            return INVALID;

        uint32_t g = home;
        uint32_t step = 0;
        for (size_type visited = 1; visited < _num_groups; ++visited) {
            g = (g + (++step)) & static_cast<uint32_t>(_mask);
            const group_block& b = _blocks[g];
            detail::prefetch_group(&b);
            mask = detail::match_fingerprint(b.fingerprints, fw);
            while (mask) {
                const uint32_t i = detail::ctz32(mask);
                mask &= mask - 1;
                const size_type idx = b.index[i];
                if (EMH_LIKELY(_key_equal(_pairs[idx].first, key))) {
                    group_out = g;
                    slot_out = i;
                    return idx;
                }
            }
            // No key of this class homing here probed past this step.
            if (step >= guard)
                return INVALID;
        }
        return INVALID;
    }

    template <typename K>
    EMH9_FORCEINLINE bool find_slot(uint64_t hash, const K& key, uint32_t& group_out, uint32_t& slot_out) const {
        if (EMH_UNLIKELY(_num_groups == 0))
            return false;

        const uint32_t fw = detail::fingerprint_words[static_cast<uint8_t>(hash)];
        const uint32_t home = static_cast<uint32_t>(home_of(hash, _mask));

        // Hit path: home group only, without the probe bound.
        const group_block& hb = _blocks[home];
        detail::prefetch_group(&hb);
        uint32_t mask = detail::match_fingerprint(hb.fingerprints, fw);
        while (mask) {
            const uint32_t i = detail::ctz32(mask);
            mask &= mask - 1;
            const size_type idx = hb.index[i];
            if (EMH_LIKELY(_key_equal(_pairs[idx].first, key))) {
                group_out = home;
                slot_out = i;
                return true;
            }
        }

        const uint32_t guard = reach_guard(reach_of(hb, static_cast<uint8_t>(hash) & 7));
        if (guard == 0)
            return false;

        uint32_t g = home;
        uint32_t step = 0;
        for (size_type visited = 1; visited < _num_groups; ++visited) {
            g = (g + (++step)) & static_cast<uint32_t>(_mask);
            const group_block& b = _blocks[g];
            detail::prefetch_group(&b);
            mask = detail::match_fingerprint(b.fingerprints, fw);
            while (mask) {
                const uint32_t i = detail::ctz32(mask);
                mask &= mask - 1;
                const size_type idx = b.index[i];
                if (EMH_LIKELY(_key_equal(_pairs[idx].first, key))) {
                    group_out = g;
                    slot_out = i;
                    return true;
                }
            }
            // No key of this class homing here probed past this step.
            if (step >= guard)
                return false;
        }
        return false;
    }

    /// Locate the index slot holding value index `idx`, comparing index entries
    /// only.  The dense array is never touched, so a probe costs no second
    /// cache line.  Used by erase, where the value index is already known.
    EMH9_FORCEINLINE void slot_of_value(uint64_t hash, size_type value_idx, uint32_t& group_out,
                                        uint32_t& slot_out) const {
#if defined(EMH9_ERASE_STATS)
        ++detail::erase_stats::slot_probes;
#endif
        const uint32_t fw = detail::fingerprint_words[static_cast<uint8_t>(hash)];
        const uint32_t home = static_cast<uint32_t>(home_of(hash, _mask));

        // Hit path: home group only, without the probe bound.
        const group_block& hb = _blocks[home];
        uint32_t mask = detail::match_fingerprint(hb.fingerprints, fw);
        while (mask) {
            const uint32_t i = detail::ctz32(mask);
            mask &= mask - 1;
            if (hb.index[i] == value_idx) {
                group_out = home;
                slot_out = i;
                return;
            }
        }

        const uint32_t guard = reach_guard(reach_of(hb, static_cast<uint8_t>(hash) & 7));
        if (guard == 0) {
            group_out = INVALID;
            return;
        }

        uint32_t g = home;
        uint32_t step = 0;
        for (size_type visited = 1; visited < _num_groups; ++visited) {
            g = (g + (++step)) & static_cast<uint32_t>(_mask);
            const group_block& b = _blocks[g];
            mask = detail::match_fingerprint(b.fingerprints, fw);
            while (mask) {
                const uint32_t i = detail::ctz32(mask);
                mask &= mask - 1;
                if (b.index[i] == value_idx) {
                    group_out = g;
                    slot_out = i;
                    return;
                }
            }
            if (step >= guard)
                break;
        }
        group_out = INVALID; // unreachable for a live value index
    }

    // ------------------------------------------------------------------
    // insert core
    // ------------------------------------------------------------------
    /// Where an element ended up: its position in the value array, the index
    /// slot that points at it, and whether it was inserted (as opposed to found
    /// on the overflow probe).  Sixteen bytes, so it comes back in registers.
    struct slot_pos {
        uint32_t group;
        uint32_t slot;
        size_type idx;
        bool inserted;
    };

    /// Insert or find.  The home group is probed inline because that is where
    /// almost every hit, and almost every free slot, is; only a key that has
    /// overflowed leaves the small hot path.
    ///
    /// WithPos selects the result: `true` builds the iterator of the public
    /// insert API, `false` returns just the element, which keeps operator[]
    /// from materialising a 24-byte pair that has to travel through memory.
    template <bool WithPos, typename K, typename Ctor>
    EMH9_FORCEINLINE auto insert_core(uint64_t hash, const K& key, Ctor&& construct_value)
        -> std::conditional_t<WithPos, std::pair<iterator, bool>, value_type*> {
        using result_type = std::conditional_t<WithPos, std::pair<iterator, bool>, value_type*>;

        // Grows before probing, which is also what gives an empty table its
        // first group: max_filled() is zero while there are none.
        if (EMH_UNLIKELY(_num_filled + 1 > max_filled()))
            grow_groups(_num_groups ? _num_groups * 2 : 1);

        const uint32_t home = static_cast<uint32_t>(home_of(hash, _mask));
        const uint8_t fp = detail::fingerprint(hash);
        const uint8_t cls = static_cast<uint8_t>(hash) & 7;

        const group_block& hb = _blocks[home];
        detail::prefetch_group(&hb);
        // The insert path stores `fp` anyway, so broadcast that byte instead of
        // loading the 4-byte table entry: the compare no longer waits on a load.
        uint32_t mask = detail::match_byte(hb.fingerprints, fp);
        while (mask) {
            const uint32_t i = detail::ctz32(mask);
            mask &= mask - 1;
            const size_type idx = hb.index[i];
            if (EMH_LIKELY(_key_equal(_pairs[idx].first, key))) {
                if constexpr (WithPos)
                    return result_type(iterator(_pairs + idx, home, i), false);
                else
                    return _pairs + idx;
            }
        }

        // No key of this class homing here probed past the group, so the key is
        // absent.
        if (reach_of(hb, cls) == 0) {
            const uint32_t empties = detail::match_byte(hb.fingerprints, 0);
            if (empties) {
                const uint32_t slot = detail::ctz32(empties);
                construct_value(_pairs + _num_filled);
                group_block& t = _blocks[home];
                t.fingerprints[slot] = fp;
                t.index[slot] = _num_filled;
                ++_num_filled;
                if constexpr (WithPos)
                    return result_type(iterator(_pairs + (_num_filled - 1), home, slot), true);
                else
                    return _pairs + (_num_filled - 1);
            }
        }

        const slot_pos p = insert_impl_far(hash, home, fp, key, construct_value);
        if constexpr (WithPos)
            return result_type(iterator(_pairs + p.idx, p.group, p.slot), p.inserted);
        else
            return _pairs + p.idx;
    }

    template <typename K, typename Ctor>
    EMH9_FORCEINLINE std::pair<iterator, bool> insert_impl(uint64_t hash, const K& key, Ctor&& construct_value) {
        return insert_core<true>(hash, key, std::forward<Ctor>(construct_value));
    }

    /// Cold half of insert_core: the key overflowed its home group.
    template <typename K, typename Ctor>
    EMH9_NOINLINE slot_pos insert_impl_far(uint64_t hash, uint32_t home, uint8_t fp, const K& key,
                                           Ctor& construct_value) {
        const uint8_t cls = static_cast<uint8_t>(hash) & 7;
        const uint32_t guard = reach_guard(reach_of(_blocks[home], cls));
        uint32_t g = home;
        uint32_t step = 0;
        for (size_type visited = 0; visited < _num_groups; ++visited) {
            const group_block& b = _blocks[g];
            detail::prefetch_group(&b);
            uint32_t mask = detail::match_byte(b.fingerprints, fp);
            while (mask) {
                const uint32_t i = detail::ctz32(mask);
                mask &= mask - 1;
                const size_type idx = b.index[i];
                if (_key_equal(_pairs[idx].first, key)) {
#if EMH9_HOME_RELOCATE
                    if (g != home) {
                        const uint32_t lane = relocate_home(home, g, i);
                        if (lane != static_cast<uint32_t>(INVALID))
                            return {home, lane, idx, false};
                    }
#endif
                    return {g, i, idx, false};
                }
            }
            // No key of this class homing here probed past this step, so the key
            // cannot be further along the sequence.
            if (step >= guard)
                break;
            g = (g + (++step)) & static_cast<uint32_t>(_mask);
        }

        // Known absent: place it at the first group with a free slot, counting
        // the full groups it probes past.
        uint32_t placed_g = 0, placed_i = 0;
        const size_type idx = insert_new(hash, construct_value, placed_g, placed_i);
        return {placed_g, placed_i, idx, true};
    }

    template <typename V> std::pair<iterator, bool> insert_value(V&& value) {
        const uint64_t h = hash_key(value.first);
        return insert_impl(h, value.first,
                           [&](value_type* p) { ::new (static_cast<void*>(p)) value_type(std::forward<V>(value)); });
    }

    /// Place an element whose key is known to be absent: probe from the home
    /// group to the first group with a free slot, recording how far its home
    /// group has to reach.  No key comparison is done, so a duplicate would
    /// break the index.
    template <typename Ctor> size_type insert_new(uint64_t hash, Ctor&& construct_value) {
        uint32_t group, slot;
        return insert_new(hash, std::forward<Ctor>(construct_value), group, slot);
    }

    /// As above, and also reports the coordinates of the slot it filled.
    template <typename Ctor>
    size_type insert_new(uint64_t hash, Ctor&& construct_value, uint32_t& group_out, uint32_t& slot_out) {
        if (_num_filled + 1 > max_filled())
            grow_groups(_num_groups ? _num_groups * 2 : 1);

        const uint8_t fp = detail::fingerprint(hash);
        const uint8_t cls = static_cast<uint8_t>(hash) & 7;
        const uint32_t home = static_cast<uint32_t>(home_of(hash, _mask));
        uint32_t g = home;
        uint32_t step = 0;
        for (size_type visited = 0; visited < _num_groups; ++visited) {
            group_block& b = _blocks[g];
            const uint32_t empties = detail::match_byte(b.fingerprints, 0);
            if (empties) {
                const uint32_t i = detail::ctz32(empties);
                construct_value(_pairs + _num_filled);
                b.fingerprints[i] = fp;
                b.index[i] = _num_filled;
                grow_reach(_blocks[home], cls, step);
                group_out = g;
                slot_out = i;
                return _num_filled++;
            }
            g = (g + (++step)) & static_cast<uint32_t>(_mask);
        }
        return INVALID; // unreachable: the load factor stays below 1
    }

    // ------------------------------------------------------------------
    // erase core
    // ------------------------------------------------------------------
    template <typename K> size_type erase_key(uint64_t hash, const K& key) {
        uint32_t g, i;
        if (!find_slot(hash, key, g, i))
            return 0;
        const size_type last = _num_filled - 1;
        uint32_t moved_g = 0, moved_i = 0;
        if (_blocks[g].index[i] != last)
            slot_of_value(hash_key(_pairs[last].first), last, moved_g, moved_i);
        erase_slot(g, i, moved_g, moved_i);
        return 1;
    }

    /// Free the index slot at (g, i), swap the last value into the hole and
    /// repoint it.  The caller has already located the slot of the moved value
    /// (moved_g, moved_i); it is unused when the removed value already is the
    /// last one.
    EMH9_FORCEINLINE void erase_slot(uint32_t g, uint32_t i, uint32_t moved_g, uint32_t moved_i) {
        group_block* const blocks = _blocks;
        const size_type idx = blocks[g].index[i];
        const size_type last = _num_filled - 1;

        blocks[g].fingerprints[i] = 0;

        if (idx != last) {
            _pairs[idx] = std::move(_pairs[last]);
            blocks[moved_g].index[moved_i] = idx;
        }
        _pairs[last].~value_type();
        --_num_filled;
    }

#if EMH9_HOME_RELOCATE
    /// Move a found element back into its (now non-full) home group.
    /// Rare path: the key was found past its home group and home has a free
    /// slot.  Kept out of line so the inlined probe stays small.  Returns the
    /// slot it moved the entry to, or INVALID when nothing moved.
    EMH9_NOINLINE uint32_t relocate_home(uint32_t home, uint32_t g, uint32_t i) {
        if (g == home || _num_groups <= 1)
            return INVALID;
        group_block* const blocks = _blocks;
        group_block& hb = blocks[home];
        const uint32_t empties = detail::match_byte(hb.fingerprints, 0);
        if (!empties)
            return INVALID;
        const uint32_t e = detail::ctz32(empties);
        // Read both fields before either store, for the same aliasing reason
        // as in erase_slot(): the group base would otherwise be reloaded.
        const uint8_t fp = blocks[g].fingerprints[i];
        const size_type idx = blocks[g].index[i];
        hb.fingerprints[e] = fp;
        hb.index[e] = idx;
        blocks[g].fingerprints[i] = 0;
        // `reach` is left as it was: it may overestimate, never underestimate.
        return e;
    }
#endif
};

template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>,
          typename AllocT = std::allocator<std::pair<KeyT, ValueT>>>
using HashTable9 = HashMap<KeyT, ValueT, HashT, EqT, AllocT>;

} // namespace emhash9
