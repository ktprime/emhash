// emhash7::HashMap for C++17/20
// version 2.2.6
// https://github.com/ktprime/emhash/blob/master/hash_table7.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2020-2026 Huang Yuanbing & bailuzhou AT 163.com
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

/// @file hash_table7.hpp
/// @brief No-tombstone linked-bucket open addressing hash map (emhash7)
/// @version 2.2.6
/// @copyright Copyright (c) 2020-2026 Huang Yuanbing

// From
// NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
// Quadratic collision resolution  1 - ln(1-L) - L/2     1/(1-L) - L - ln(1-L)
// Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
// separator chain resolution      1 + L / 2             exp(-L) + L

// -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
// QUADRATIC COLLISION RES.
//    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
//    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
// LINEAR COLLISION RES.
//    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
//    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0
// SEPARATE CHAN RES.
//    probes/successful lookup    1.05  1.25  1.3   1.25  1.4   1.45  1.50
//    probes/unsuccessful lookup  1.00  1.11  1.15  1.22  1.25  1.31  1.37
//    collision/unsuccessful lookup  1.01  1.25  1.36, 1.56, 1.64, 1.81, 1.97

/****************
    under random hashCodes, the frequency of nodes in bins follows a Poisson
distribution(http://en.wikipedia.org/wiki/Poisson_distribution) with a parameter of about 0.5
on average for the default resizing threshold of 0.75, although with a large variance because
of resizing granularity. Ignoring variance, the expected occurrences of list size k are
(exp(-0.5) * pow(0.5, k)/factorial(k)). The first values are:
0: 0.60653066
1: 0.30326533
2: 0.07581633
3: 0.01263606
4: 0.00157952
5: 0.00015795
6: 0.00001316
7: 0.00000094
8: 0.00000006

  ============== buckets size ratio ========
    1   1543981  0.36884964|0.36787944  36.885
    2    768655  0.36725597|0.36787944  73.611
    3    256236  0.18364065|0.18393972  91.975
    4     64126  0.06127757|0.06131324  98.102
    5     12907  0.01541710|0.01532831  99.644
    6      2050  0.00293841|0.00306566  99.938
    7       310  0.00051840|0.00051094  99.990
    8        49  0.00009365|0.00007299  99.999
    9         4  0.00000860|0.00000913  100.000
========== collision miss ratio ===========
  _num_filled aver_size k.v size_kv = 4185936, 1.58, x.x 24
  collision,poisson,cache_miss hit_find|hit_miss, load_factor = 36.73%,36.74%,31.31%  1.50|2.00, 1.00
============== buckets size ratio ========
*******************************************************/

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

#include <cstring>
#include <string>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>
#include <algorithm>
#include <memory>

// wyhash is now provided by config.hpp (emh_wyhash / wyhash alias)
// No need for external wyhash.h dependency

#ifdef EMH_NEW
#undef EMH_KEY
#undef EMH_VAL
#undef EMH_PKV
#undef EMH_NEW
#undef EMH_BUCKET
#endif

#ifndef EMH_BUCKET_INDEX
#define EMH_BUCKET_INDEX 1
#endif

#if EMH_BUCKET_INDEX == 0
#define EMH_KEY(p, n) p[n].second.first
#define EMH_VAL(p, n) p[n].second.second
#define EMH_BUCKET(p, n) p[n].first
#define EMH_PKV(p, n) p[n].second
#define EMH_NEW(key, val, bucket)                                                                                      \
    new (_pairs + bucket) PairT(bucket, value_type(key, val));                                                         \
    _num_filled++;                                                                                                     \
    emh_set(bucket)
#elif EMH_BUCKET_INDEX == 2
#define EMH_KEY(p, n) p[n].first.first
#define EMH_VAL(p, n) p[n].first.second
#define EMH_BUCKET(p, n) p[n].second
#define EMH_PKV(p, n) p[n].first
#define EMH_NEW(key, val, bucket)                                                                                      \
    new (_pairs + bucket) PairT(value_type(key, val), bucket);                                                         \
    _num_filled++;                                                                                                     \
    emh_set(bucket)
#else
#define EMH_KEY(p, n) p[n].first
#define EMH_VAL(p, n) p[n].second
#define EMH_BUCKET(p, n) p[n].bucket
#define EMH_PKV(p, n) p[n]
#define EMH_NEW(key, val, bucket)                                                                                      \
    new (_pairs + (bucket)) PairT(key, val, bucket);                                                                   \
    _num_filled++;                                                                                                     \
    emh_set(bucket)
#endif

// EMH_SET/EMH_CLS/EMH_EMPTY replaced by inline methods emh_set/emh_cls/emh_empty
// (see class body). This avoids macro UB from multiple evaluation of arguments.

#ifdef _WIN32
#include <intrin.h>
#endif

namespace emhash7 {

#ifdef EMH_SIZE_TYPE_16BIT
using size_type = uint16_t;
static constexpr size_type INACTIVE = 0xFFFF;
#elif EMH_SIZE_TYPE_64BIT
using size_type = uint64_t;
static constexpr size_type INACTIVE = 0 - 0x1ull;
#else
using size_type = uint32_t;
static constexpr size_type INACTIVE = 0xFFFFFFFF;
#endif

#ifndef EMH_MALIGN
static constexpr uint32_t EMH_MALIGN = 16;
#endif
static_assert(EMH_MALIGN >= 16 && 0 == (EMH_MALIGN & (EMH_MALIGN - 1)));

#ifndef EMH_SIZE_TYPE_16BIT
static_assert(static_cast<int>(INACTIVE) < 0, "INACTIVE must negative (to int)");
#endif

// count the leading zero bit
static inline size_type CTZ(size_t n) {
#if defined(__x86_64__) || defined(_WIN32) || (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#elif __BIG_ENDIAN__ || (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    n = __builtin_bswap64(n);
#else
    // Portable endianness detection without strict aliasing violation
    uint32_t endianness = 0x12345678;
    unsigned char first_byte;
    std::memcpy(&first_byte, &endianness, 1);
    if (first_byte == 0x12)
        n = __builtin_bswap64(n);
#endif

#ifdef _WIN32
    unsigned long index;
#if defined(_WIN64)
    _BitScanForward64(&index, n);
#else
    _BitScanForward(&index, n);
#endif
#elif defined(__LP64__) || (SIZE_MAX == UINT64_MAX) || defined(__x86_64__)
    auto index = __builtin_ctzll(n);
#else
    auto index = __builtin_ctzl(n);
#endif

    return static_cast<size_type>(index);
}

template <typename First, typename Second> struct entry {
    using first_type = First;
    using second_type = Second;
    entry(const First& key, const Second& val, size_type ibucket) : second(val), first(key) { bucket = ibucket; }

    entry(First&& key, Second&& val, size_type ibucket) : second(std::move(val)), first(std::move(key)) {
        bucket = ibucket;
    }

    template <typename K, typename V>
    entry(K&& key, V&& val, size_type ibucket) : second(std::forward<V>(val)), first(std::forward<K>(key)) {
        bucket = ibucket;
    }

    entry(const std::pair<First, Second>& pair) : second(pair.second), first(pair.first) { bucket = INACTIVE; }

    entry(std::pair<First, Second>&& pair) : second(std::move(pair.second)), first(std::move(pair.first)) {
        bucket = INACTIVE;
    }

    entry(std::tuple<First, Second>&& tup) : second(std::move(std::get<1>(tup))), first(std::move(std::get<0>(tup))) {
        bucket = INACTIVE;
    }

    entry(const entry& rhs) : second(rhs.second), first(rhs.first) { bucket = rhs.bucket; }

    entry(entry&& rhs) noexcept : second(std::move(rhs.second)), first(std::move(rhs.first)) { bucket = rhs.bucket; }

    entry& operator=(entry&& rhs) noexcept {
        second = std::move(rhs.second);
        bucket = rhs.bucket;
        first = std::move(rhs.first);
        return *this;
    }

    entry& operator=(const entry& rhs) {
        second = rhs.second;
        bucket = rhs.bucket;
        first = rhs.first;
        return *this;
    }

    bool operator==(const entry<First, Second>& p) const { return first == p.first && second == p.second; }

    bool operator==(const std::pair<First, Second>& p) const { return first == p.first && second == p.second; }

    void swap(entry<First, Second>& o) noexcept {
        std::swap(second, o.second);
        std::swap(first, o.first);
    }

#if EMH_ORDER_KV || EMH_SIZE_TYPE_64BIT
    First first;
    size_type bucket;
    Second second;
#else
    Second second;
    size_type bucket;
    First first;
#endif
};

/// @brief Cache-friendly hash map with no-tombstone linked-bucket design.
///
/// emhash7 uses a linked-bucket layout with in-place chain repair on erase,
/// eliminating tombstones entirely. This provides stable performance in
/// insert/erase-heavy workloads without periodic rehashing.
///
/// Supports load factors from 0.80 to 0.999 natively (no EMH_HIGH_LOAD needed).
///
/// @tparam KeyT    Key type
/// @tparam ValueT  Mapped value type
/// @tparam HashT   Hash functor (default: std::hash<KeyT>)
/// @tparam EqT     Key equality functor (default: std::equal_to<KeyT>)
/// @tparam AllocT  Allocator type (default: std::allocator<std::pair<KeyT, ValueT>>)
///
/// @note Header-only: just `#include "emhash/hash_table7.hpp"` and use `emhash7::HashMap`.
/// @note Not thread-safe. Concurrent read-only access is safe.
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>,
          typename AllocT = std::allocator<std::pair<KeyT, ValueT>>>
class HashMap {
    static_assert(std::is_copy_constructible<KeyT>::value || std::is_move_constructible<KeyT>::value,
                  "KeyT must be copy-constructible or move-constructible");
    static_assert(std::is_copy_constructible<ValueT>::value || std::is_move_constructible<ValueT>::value,
                  "ValueT must be copy-constructible or move-constructible");
    static_assert(std::is_invocable_v<HashT, const KeyT&>, "HashT must be callable with const KeyT&");
    static_assert(std::is_invocable_v<EqT, const KeyT&, const KeyT&>,
                  "EqT must be callable with (const KeyT&, const KeyT&)");

#ifndef EMH_DEFAULT_LOAD_FACTOR
    constexpr static float EMH_DEFAULT_LOAD_FACTOR = 0.80f;
#endif
    constexpr static float EMH_MIN_LOAD_FACTOR = 0.25f;

public:
    using htype = HashMap<KeyT, ValueT, HashT, EqT, AllocT>;
    using value_type = std::pair<KeyT, ValueT>;
    using allocator_type = AllocT;

#if EMH_BUCKET_INDEX == 0
    using value_pair = value_type;
    using PairT = std::pair<size_type, value_type>;
#elif EMH_BUCKET_INDEX == 2
    using value_pair = value_type;
    using PairT = std::pair<value_type, size_type>;
#else
    using value_pair = entry<KeyT, ValueT>;
    using PairT = entry<KeyT, ValueT>;
#endif

    // Bitmask front layout requires EMH_MALIGN-aligned _pairs pointer.
    // alloc_bucket returns alignof(PairT)-aligned memory; bitmask_aligned_size
    // rounds up to EMH_MALIGN. If alignof(PairT) > EMH_MALIGN, _pairs may be
    // misaligned. This static_assert catches that at compile time.
    static_assert(alignof(PairT) <= EMH_MALIGN,
                  "PairT alignment exceeds EMH_MALIGN; increase EMH_MALIGN or use a type with smaller alignment");

    using key_type = KeyT;
    using val_type = ValueT;
    using mapped_type = ValueT;
    using hasher = HashT;
    using key_equal = EqT;
    using reference = PairT&;
    using const_reference = const PairT&;

    using PairAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<PairT>;
    using PairAllocTraits = std::allocator_traits<PairAlloc>;

    class const_iterator;
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = value_pair;

        using pointer = value_pair*;
        using reference = value_pair&;

        iterator() = default;
        iterator(const const_iterator& it) : _map(it._map), _bucket(it._bucket), _from(it._from), _bmask(it._bmask) {}
        // iterator(const htype* hash_map, size_type bucket, bool) : _map(hash_map), _bucket(bucket) { init(); }
#if EMH_ITER_SAFE
        iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket) { init(); }
#else
        iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket), _bmask(0) {
            _from = size_type(-1);
        }
#endif

        void init() {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _map->bucket_count()) {
                memcpy(&_bmask, _map->_bitmask + _from / SIZE_BIT * sizeof(size_t), sizeof(_bmask));
                _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        size_type bucket() const { return _bucket; }

        void clear(size_type bucket) {
            if (_bucket / SIZE_BIT == bucket / SIZE_BIT)
                _bmask &= ~(1ull << (bucket % SIZE_BIT));
        }

        iterator& next() {
            goto_next_element();
            return *this;
        }

        iterator& operator++() {
#ifndef EMH_ITER_SAFE
            if (_from == static_cast<size_type>(-1))
                init();
#endif
            _bmask &= _bmask - 1;
            goto_next_element();
            return *this;
        }

        iterator operator++(int) {
#ifndef EMH_ITER_SAFE
            if (_from == static_cast<size_type>(-1))
                init();
#endif
            iterator old = *this;
            _bmask &= _bmask - 1;
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->EMH_PKV(_pairs, _bucket); }

        pointer operator->() const { return &(_map->EMH_PKV(_pairs, _bucket)); }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

    private:
        void goto_next_element() {
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                memcpy(&_bmask, _map->_bitmask + (_from += SIZE_BIT) / SIZE_BIT * sizeof(size_t), sizeof(_bmask));
                _bmask = ~_bmask;
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype* _map;
        size_type _bucket;
        size_type _from;
        size_t _bmask = 0;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = value_pair;

        using pointer = const value_pair*;
        using reference = const value_pair&;

        const_iterator(const iterator& it) : _map(it._map), _bucket(it._bucket), _from(it._from), _bmask(it._bmask) {}
        // const_iterator(const htype* hash_map, size_type bucket, bool) : _map(hash_map), _bucket(bucket) { init(); }
#if EMH_ITER_SAFE
        const_iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket) { init(); }
#else
        const_iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket), _bmask(0) {
            _from = size_type(-1);
        }
#endif

        void init() {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _map->bucket_count()) {
                memcpy(&_bmask, _map->_bitmask + _from / SIZE_BIT * sizeof(size_t), sizeof(_bmask));
                _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        size_type bucket() const { return _bucket; }

        const_iterator& operator++() {
#ifndef EMH_ITER_SAFE
            if (_from == static_cast<size_type>(-1))
                init();
#endif
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int) {
#ifndef EMH_ITER_SAFE
            if (_from == static_cast<size_type>(-1))
                init();
#endif
            const_iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->EMH_PKV(_pairs, _bucket); }

        pointer operator->() const { return &(_map->EMH_PKV(_pairs, _bucket)); }

        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

    private:
        void goto_next_element() {
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                memcpy(&_bmask, _map->_bitmask + (_from += SIZE_BIT) / SIZE_BIT * sizeof(size_t), sizeof(_bmask));
                _bmask = ~_bmask;
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype* _map;
        size_type _bucket;
        size_type _from;
        size_t _bmask = 0;
    };

    void init(size_type bucket, float mlf = EMH_DEFAULT_LOAD_FACTOR) {
        _pairs = nullptr;
        _bitmask = nullptr;
        _num_buckets = _num_filled = 0;
        _mlf = static_cast<uint32_t>((1 << 28) / EMH_DEFAULT_LOAD_FACTOR);
        max_load_factor(mlf);
        rehash(bucket);
    }

    explicit HashMap(size_type bucket = 2, float mlf = EMH_DEFAULT_LOAD_FACTOR) noexcept { init(bucket, mlf); }

    explicit HashMap(const AllocT& alloc) noexcept : _alloc(PairAlloc(alloc)) { init(2, EMH_DEFAULT_LOAD_FACTOR); }

    HashMap(size_type bucket, float mlf, const AllocT& alloc) noexcept : _alloc(PairAlloc(alloc)) { init(bucket, mlf); }

    // Bitmask size rounded up to EMH_MALIGN boundary (for front layout alignment)
    static size_t bitmask_aligned_size(uint64_t num_buckets) {
        const auto raw = (num_buckets + 7) / 8 + BIT_PACK;
        return (raw + EMH_MALIGN - 1) & ~size_t(EMH_MALIGN - 1);
    }

    static size_t AllocSize(uint64_t num_buckets) {
        return bitmask_aligned_size(num_buckets) + (num_buckets + EPACK_SIZE) * sizeof(PairT);
    }

    static size_type alloc_count(size_type num_buckets) {
        return static_cast<size_type>((AllocSize(num_buckets) + sizeof(PairT) - 1) / sizeof(PairT));
    }

    PairT* alloc_bucket(size_type num_buckets) noexcept {
        auto count = alloc_count(num_buckets);
        return PairAllocTraits::allocate(_alloc, count);
    }

    void dealloc_bucket(PairT* pairs, size_type num_buckets) noexcept {
        if (pairs) {
            auto count = alloc_count(num_buckets);
            PairAllocTraits::deallocate(_alloc, pairs, count);
        }
    }

    HashMap(const HashMap& rhs) : _alloc(PairAllocTraits::select_on_container_copy_construction(rhs._alloc)) {
        if (rhs.load_factor() > EMH_MIN_LOAD_FACTOR) {
            auto* base = alloc_bucket(rhs._num_buckets);
            _bitmask = reinterpret_cast<bit_type*>(base);
            _pairs =
                reinterpret_cast<PairT*>(reinterpret_cast<uint8_t*>(base) + bitmask_aligned_size(rhs._num_buckets));
            clone(rhs);
        } else {
            init(rhs._num_filled + 2, rhs.max_load_factor());
            for (auto it = rhs.begin(); it != rhs.end(); ++it)
                static_cast<void>(insert_unique(it->first, it->second));
        }
    }

    HashMap(HashMap&& rhs) noexcept : _alloc(std::move(rhs._alloc)) {
#ifndef EMH_ZERO_MOVE
        init(4);
#else
        _num_buckets = _num_filled = _mask = 0;
        _pairs = nullptr;
#endif
        swap(rhs);
    }

    HashMap(std::initializer_list<value_type> ilist) {
        init(static_cast<size_type>(ilist.size()));
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            (void)do_insert(*it);
    }

    template <class InputIt> HashMap(InputIt first, InputIt last, size_type bucket_count = 4) {
        init(static_cast<size_type>(std::distance(first, last)) + bucket_count);
        for (; first != last; ++first)
            (void)emplace(*first);
    }

    HashMap& operator=(const HashMap& rhs) {
        if (this == &rhs)
            return *this;

        if constexpr (PairAllocTraits::propagate_on_container_copy_assignment::value)
            _alloc = rhs._alloc;

        if (rhs.load_factor() < EMH_MIN_LOAD_FACTOR) {
            clear();
            dealloc_bucket(reinterpret_cast<PairT*>(_bitmask), _num_buckets);
            _pairs = nullptr;
            _bitmask = nullptr;
            _num_buckets = 0;
            rehash(rhs._num_filled + 2);
            for (auto it = rhs.begin(); it != rhs.end(); ++it)
                static_cast<void>(insert_unique(it->first, it->second));
            return *this;
        }

        if (_num_filled)
            clearkv();

        if (_num_buckets != rhs._num_buckets) {
            dealloc_bucket(reinterpret_cast<PairT*>(_bitmask), _num_buckets);
            auto* base = alloc_bucket(rhs._num_buckets);
            _bitmask = reinterpret_cast<bit_type*>(base);
            _pairs =
                reinterpret_cast<PairT*>(reinterpret_cast<uint8_t*>(base) + bitmask_aligned_size(rhs._num_buckets));
        }

        clone(rhs);
        return *this;
    }

    HashMap& operator=(HashMap&& rhs) noexcept(PairAllocTraits::propagate_on_container_move_assignment::value ||
                                               std::is_nothrow_move_assignable<HashT>::value) {
        if (this != &rhs) {
            swap(rhs);
            rhs.clear();
        }
        return *this;
    }

    template <typename Con> bool operator==(const Con& rhs) const {
        if (size() != rhs.size())
            return false;

        for (auto it = begin(), last = end(); it != last; ++it) {
            auto oi = rhs.find(it->first);
            if (oi == rhs.end() || it->second != oi->second)
                return false;
        }
        return true;
    }

    template <typename Con> bool operator!=(const Con& rhs) const { return !(*this == rhs); }

    ~HashMap() noexcept {
        if (need_explicit_dtor() && _num_filled) {
            for (auto it = cbegin(); _num_filled; ++it) {
                _num_filled--;
                it->~value_pair();
            }
        }
        dealloc_bucket(reinterpret_cast<PairT*>(_bitmask), _num_buckets);
        _pairs = nullptr;
    }

    void clone(const HashMap& rhs) {
        _hasher = rhs._hasher;
        //_eq          = rhs._eq;

        _num_filled = rhs._num_filled;
        _mask = rhs._mask;
        _mlf = rhs._mlf;
        _num_buckets = rhs._num_buckets;

        auto* opairs = rhs._pairs;

        if (is_trivially_copyable())
            memcpy(reinterpret_cast<char*>(_bitmask), reinterpret_cast<char*>(rhs._bitmask), AllocSize(_num_buckets));
        else {
            // For non-trivially-copyable types, only init bucket field of tail sentinels
            // (memcpy of whole PairT would read uninitialized key — MSan UB).
            // Then placement-new each filled bucket. Finally copy bitmask bytes.
            const size_type zero_bucket = 0;
            for (size_type i = 0; i < EPACK_SIZE; ++i)
                std::memcpy(&EMH_BUCKET(_pairs, _num_buckets + i), &zero_bucket, sizeof(zero_bucket));
            for (auto it = rhs.cbegin(); it.bucket() < _num_buckets; ++it) {
                const auto bucket = it.bucket();
                new (_pairs + bucket) PairT(opairs[bucket]);
                EMH_BUCKET(_pairs, bucket) = EMH_BUCKET(opairs, bucket);
            }
            memcpy(reinterpret_cast<char*>(_bitmask), reinterpret_cast<char*>(rhs._bitmask),
                   (_num_buckets + 7) / 8 + BIT_PACK);
        }
    }

    void swap(HashMap& rhs) noexcept {
        std::swap(_hasher, rhs._hasher);
        std::swap(_eq, rhs._eq);
        std::swap(_alloc, rhs._alloc);
        std::swap(_pairs, rhs._pairs);
        std::swap(_num_buckets, rhs._num_buckets);
        std::swap(_num_filled, rhs._num_filled);
        std::swap(_mask, rhs._mask);
        std::swap(_mlf, rhs._mlf);
        std::swap(_bitmask, rhs._bitmask);
    }

    // -------------------------------------------------------------
    iterator begin() noexcept {
#ifdef EMH_ZERO_MOVE
        if (0 == _num_filled)
            return {this, _num_buckets};
#endif

        size_t bmask;
        memcpy(&bmask, _bitmask, sizeof(bmask));
        bmask = ~bmask;
        if (bmask != 0)
            return {this, static_cast<size_type>(CTZ(bmask))};

        iterator it(this, sizeof(bmask) * 8 - 1);
        it.init();
        return it.next();
    }

    const_iterator cbegin() const noexcept {
#ifdef EMH_ZERO_MOVE
        if (0 == _num_filled)
            return {this, _num_buckets};
#endif

        size_t bmask;
        memcpy(&bmask, _bitmask, sizeof(bmask));
        bmask = ~bmask;
        if (bmask != 0)
            return {this, static_cast<size_type>(CTZ(bmask))};

        iterator it(this, sizeof(bmask) * 8 - 1);
        it.init();
        return it.next();
    }

    [[nodiscard]] iterator last() const {
        if (_num_filled == 0)
            return end();

        auto bucket = _num_buckets - 1;
        while (emh_empty(bucket))
            bucket--;
        return {this, bucket};
    }

    inline const_iterator begin() const noexcept { return cbegin(); }

    inline iterator end() noexcept { return {this, _num_buckets}; }
    inline const_iterator cend() const { return {this, _num_buckets}; }
    inline const_iterator end() const { return cend(); }

    [[nodiscard]] inline size_type size() const noexcept { return _num_filled; }
    [[nodiscard]] inline bool empty() const noexcept { return _num_filled == 0; }

    [[nodiscard]] inline size_type bucket_count() const noexcept { return _num_buckets; }
    [[nodiscard]] inline float load_factor() const noexcept {
        return (static_cast<float>(_num_filled)) / (static_cast<float>(_mask) + 1.0f);
    }

    [[nodiscard]] inline const HashT& hash_function() const { return _hasher; }
    [[nodiscard]] inline const EqT& key_eq() const { return _eq; }
    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator_type(_alloc); }

    inline void max_load_factor(float mlf) {
        if (mlf <= 0.999f && mlf > EMH_MIN_LOAD_FACTOR)
            _mlf = static_cast<uint32_t>((1 << 28) / mlf);
    }

    [[nodiscard]] inline constexpr float max_load_factor() const {
        return static_cast<float>(1 << 28) / static_cast<float>(_mlf);
    }
    [[nodiscard]] constexpr uint64_t max_size() const { return 1ull << (sizeof(_num_buckets) * 8 - 1); }
    [[nodiscard]] constexpr uint64_t max_bucket_count() const { return max_size(); }

    [[nodiscard]] size_type bucket_main() const {
        size_type main_size = 0;
        for (size_type bucket = 0; bucket < _num_buckets; ++bucket) {
            if (EMH_BUCKET(_pairs, bucket) == bucket)
                main_size++;
        }
        return main_size;
    }

#if EMH_STATIS
    // Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const {
        const auto bucket = hash_key(key) & _mask;
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (emh_empty(bucket))
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        return (hash_key(bucket_key) & _mask) + 1;
    }

    // Returns the number of elements in bucket n.
    size_type bucket_size(const size_type bucket) const {
        if (emh_empty(bucket))
            return 0;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        next_bucket = hash_key(EMH_KEY(_pairs, bucket)) & _mask;
        size_type bucket_size = 1;

        // iterator each item in current main bucket
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            bucket_size++;
            next_bucket = nbucket;
        }
        return bucket_size;
    }

    size_type get_main_bucket(const size_type bucket) const {
        if (emh_empty(bucket))
            return INACTIVE;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        const auto main_bucket = hash_key(bucket_key) & _mask;
        return main_bucket;
    }

    size_type get_diss(size_type bucket, size_type next_bucket, const size_type slots) const {
        const int cahe_line_size = 64;
        auto pbucket = reinterpret_cast<uintptr_t>(&_pairs[bucket]);
        auto pnext = reinterpret_cast<uintptr_t>(&_pairs[next_bucket]);
        if (pbucket / cahe_line_size == pnext / cahe_line_size)
            return 0;
        size_type diff = pbucket > pnext ? (pbucket - pnext) : (pnext - pbucket);
        if (diff / cahe_line_size + 1 < slots)
            return (diff / cahe_line_size + 1);
        return slots - 1;
    }

    int get_bucket_info(const size_type bucket, size_type steps[], const size_type slots) const {
        if (emh_empty(bucket))
            return -1;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((hash_key(EMH_KEY(_pairs, bucket)) & _mask) != bucket)
            return 0;
        else if (next_bucket == bucket)
            return 1;

        steps[get_diss(bucket, next_bucket, slots)]++;
        size_type bucket_size = 2;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;

            steps[get_diss(nbucket, next_bucket, slots)]++;
            bucket_size++;
            next_bucket = nbucket;
        }

        return bucket_size;
    }

    void dump_statics(bool show_cache) const {
        const int slots = 128;
        size_type buckets[slots + 1] = {0};
        size_type steps[slots + 1] = {0};
        char buff[1024 * 8];
        for (size_type bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, slots);
            if (bsize >= 0)
                buckets[bsize]++;
        }

        size_type sumb = 0, sums = 0, sumn = 0;
        size_type miss = 0, finds = 0, bucket_coll = 0;
        double lf = load_factor(), fk = 1.0 / exp(lf), sum_poisson = 0;
        int bsize = snprintf(buff, 1024, "============== buckets size ratio ========\n");

        miss += _num_buckets - _num_filled;
        for (int i = 1, factorial = 1; i < static_cast<int>(sizeof(buckets) / sizeof(buckets[0])); i++) {
            double poisson = fk / factorial;
            factorial *= i;
            fk *= lf;
            if (poisson > 1e-13 && i < 20)
                sum_poisson += poisson * 100.0 * (i - 1) / i;

            const int64_t bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;

            sumb += bucketsi;
            sumn += bucketsi * i;
            bucket_coll += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            miss += bucketsi * i * i;
            auto errs = (bucketsi * 1.0 * i / _num_filled - poisson) * 100 / poisson;
            bsize += snprintf(buff + bsize, 1024 - bsize, "  %2d  %8ld  %0.8lf|%0.2lf%%  %2.3lf\n", i, bucketsi,
                              bucketsi * 1.0 * i / _num_filled, errs, sumn * 100.0 / _num_filled);
            if (sumn >= _num_filled)
                break;
        }

        bsize += snprintf(buff + bsize, 1024 - bsize, "========== collision miss ration ===========\n");
        for (size_type i = 0; show_cache && i < sizeof(steps) / sizeof(steps[0]); i++) {
            sums += steps[i];
            if (steps[i] == 0)
                continue;
            if (steps[i] > 10)
                bsize += snprintf(buff + bsize, 1024 - bsize, "  %2d  %8u  %0.2lf  %.2lf\n", static_cast<int>(i),
                                  steps[i], steps[i] * 100.0 / bucket_coll, sums * 100.0 / bucket_coll);
        }

        if (sumb == 0)
            return;

        bsize +=
            snprintf(buff + bsize, 1024 - bsize, "  _num_filled aver_size k.v size_kv = %u, %.2lf, %s.%s %zd\n",
                     _num_filled, _num_filled * 1.0 / sumb, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(PairT));

        bsize +=
            snprintf(buff + bsize, 1024 - bsize,
                     "  collision, poisson, cache_miss hit_find|hit_miss, load_factor = %.2lf%%,%.2lf%%,%.2lf%% "
                     "%.2lf|%.2lf, %.2lf\n",
                     (bucket_coll * 100.0 / _num_filled), sum_poisson, (bucket_coll - steps[0]) * 100.0 / _num_filled,
                     finds * 1.0 / _num_filled, miss * 1.0 / _num_buckets, _num_filled * 1.0 / _num_buckets);

        bsize += snprintf(buff + bsize, 1024 - bsize, "============== buckets size end =============\n");
        buff[bsize + 1] = 0;

#ifdef EMH_LOG
        EMH_LOG << __FUNCTION__ << "|" << buff << std::endl;
#else
        puts(buff);
#endif
        assert(sumn == _num_filled);
        assert(sums == bucket_coll || !show_cache);
        assert(bucket_coll == buckets[0]);
    }
#endif

    // ------------------------------------------------------------
    template <typename Key = KeyT> inline iterator find(const Key& key, size_t key_hash) noexcept {
        return {this, find_filled_hash(key, key_hash)};
    }

    template <typename Key = KeyT> inline const_iterator find(const Key& key, size_t key_hash) const noexcept {
        return {this, find_filled_hash(key, key_hash)};
    }

    template <typename Key = KeyT> inline iterator find(const Key& key) noexcept {
        return {this, find_filled_bucket(key)};
    }

    template <typename Key = KeyT> inline const_iterator find(const Key& key) const noexcept {
        return {this, find_filled_bucket(key)};
    }

    template <typename Key = KeyT> ValueT& at(const KeyT& key) {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            throw std::out_of_range("emhash7::at(): key not found");
        return EMH_VAL(_pairs, bucket);
    }

    template <typename Key = KeyT> const ValueT& at(const KeyT& key) const {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            throw std::out_of_range("emhash7::at(): key not found");
        return EMH_VAL(_pairs, bucket);
    }

    template <typename Key = KeyT> [[nodiscard]] inline bool contains(const Key& key) const noexcept {
        return find_filled_bucket(key) != _num_buckets;
    }

    template <typename Key = KeyT> inline size_type count(const Key& key) const noexcept {
        return find_filled_bucket(key) != _num_buckets ? 1u : 0u;
    }

    template <typename Key = KeyT>
    [[nodiscard]] std::pair<iterator, iterator> equal_range(const Key& key) const noexcept {
        const auto found = {this, find_filled_bucket(key), true};
        if (found.bucket() == _num_buckets)
            return {found, found};
        else
            return {found, std::next(found)};
    }

    template <typename K = KeyT>
    [[nodiscard]] std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
        const auto found = {this, find_filled_bucket(key), true};
        if (found.bucket() == _num_buckets)
            return {found, found};
        else
            return {found, std::next(found)};
    }

    void merge(HashMap& rhs) {
        if (empty()) {
            *this = std::move(rhs);
            return;
        }

        for (auto rit = rhs.begin(); rit != rhs.end();) {
            auto fit = find(rit->first);
            if (fit == end()) {
                (void)insert_unique(rit->first, std::move(rit->second));
                rit = rhs.erase(rit);
            } else {
                ++rit;
            }
        }
    }

#ifdef EMH_EXT
    [[nodiscard]] bool try_get(const KeyT& key, ValueT& val) const noexcept {
        const auto bucket = find_filled_bucket(key);
        const auto found = bucket != _num_buckets;
        if (found) {
            val = EMH_VAL(_pairs, bucket);
        }
        return found;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    [[nodiscard]] ValueT* try_get(const KeyT& key) noexcept {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Const version of the above
    [[nodiscard]] const ValueT* try_get(const KeyT& key) const noexcept {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Convenience function.
    [[nodiscard]] ValueT get_or_return_default(const KeyT& key) const noexcept {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? ValueT() : EMH_VAL(_pairs, bucket);
    }
#endif

    // -----------------------------------------------------
    template <typename K = KeyT, typename V = ValueT> std::pair<iterator, bool> do_assign(K&& key, V&& val) {
        static_cast<void>(reserve(_num_filled));

        bool isempty;
        const auto bucket = find_or_allocate(key, isempty);
        if (isempty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
        } else {
            EMH_VAL(_pairs, bucket) = std::move(val);
        }
        return {{this, bucket}, isempty};
    }

    std::pair<iterator, bool> do_insert(const value_type& value) {
        bool isempty;
        const auto bucket = find_or_allocate(value.first, isempty);
        if (isempty) {
            EMH_NEW(value.first, value.second, bucket);
        }
        return {{this, bucket}, isempty};
    }

    std::pair<iterator, bool> do_insert(value_type&& value) {
        bool isempty;
        const auto bucket = find_or_allocate(value.first, isempty);
        if (isempty) {
            EMH_NEW(std::move(value.first), std::move(value.second), bucket);
        }
        return {{this, bucket}, isempty};
    }

    template <typename K = KeyT, typename V = ValueT> std::pair<iterator, bool> do_insert(K&& key, V&& val) {
        bool isempty;
        const auto bucket = find_or_allocate(key, isempty);
        if (isempty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
        }
        return {{this, bucket}, isempty};
    }

    std::pair<iterator, bool> insert(const value_type& value) {
        check_expand_need();
        return do_insert(value);
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        check_expand_need();
        return do_insert(std::move(value));
    }

    void insert(std::initializer_list<value_type> ilist) {
        reserve(ilist.size() + _num_filled);
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            (void)do_insert(*it);
    }

    template <typename Iter> void insert(Iter first, Iter last) {
        reserve(std::distance(first, last) + _num_filled);
        for (auto it = first; it != last; ++it)
            do_insert(it->first, it->second);
    }

    template <typename K, typename V> inline size_type insert_unique(K&& key, V&& val) {
        return do_insert_unique(std::forward<K>(key), std::forward<V>(val));
    }

    inline size_type insert_unique(value_type&& value) {
        return do_insert_unique(std::move(value.first), std::move(value.second));
    }

    inline size_type insert_unique(const value_type& value) { return do_insert_unique(value.first, value.second); }

    template <typename K, typename V> inline size_type do_insert_unique(K&& key, V&& val) {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
        return bucket;
    }

    std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& val) {
        return do_assign(key, std::forward<ValueT>(val));
    }
    std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& val) {
        return do_assign(std::move(key), std::forward<ValueT>(val));
    }

    template <typename... Args> std::pair<iterator, bool> emplace(Args&&... args) {
        check_expand_need();
        return do_insert(std::forward<Args>(args)...);
    }

    template <class... Args> iterator emplace_hint(const_iterator hint, Args&&... args) {
        static_cast<void>(hint);
        check_expand_need();
        return do_insert(std::forward<Args>(args)...).first;
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) {
        check_expand_need();
        return do_insert(key, std::forward<Args>(args)...);
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) {
        check_expand_need();
        return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    template <class... Args> size_type emplace_unique(Args&&... args) {
        return insert_unique(std::forward<Args>(args)...);
    }

    /* Check if inserting a new value rather than overwriting an old entry */
    ValueT& operator[](const KeyT& key) {
        check_expand_need();

        bool isempty;
        const auto bucket = find_or_allocate(key, isempty);
        if (isempty) {
            EMH_NEW(key, std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    ValueT& operator[](KeyT&& key) {
        check_expand_need();

        bool isempty;
        const auto bucket = find_or_allocate(key, isempty);
        if (isempty) {
            EMH_NEW(std::move(key), std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    // -------------------------------------------------------
    /// Erase an element from the hash table.
    /// return 0 if element was not found
    template <typename Key = KeyT> size_type erase(const Key& key) {
        const auto bucket = erase_key(key);
        if (bucket == INACTIVE)
            return 0;

        clear_bucket(bucket);
        return 1;
    }

    // iterator erase const_iterator
    iterator erase(const_iterator cit) {
        iterator it(cit);
        return erase(it);
    }

    /// Erase an element typedef an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it) {
        // Ensure _bmask is initialized before we modify _bitmask.
        // If _from == -1 (lazy init), ++it would trigger init() AFTER
        // clear_bucket() — loading _bmask from the already-updated
        // _bitmask where the current bucket is empty, causing the
        // lowest _bmask bit to be the NEXT element, which ++it then
        // incorrectly clears. Pre-init makes ++it clear only the
        // current bucket's bit.
#ifndef EMH_ITER_SAFE
        if (it._from == static_cast<size_type>(-1))
            it.init();
#endif
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
        if (bucket == it._bucket) {
            return ++it;
        } else {
            it.clear(bucket);
            return it;
        }
    }

    /// Erase an element typedef an iterator without return next iterator
    void _erase(const_iterator it) {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
    }

    template <typename Pred> size_type erase_if(Pred pred) {
        auto old_size = size();
        for (auto it = begin(), last = end(); it != last;) {
            if (pred(*it))
                it = erase(it);
            else
                ++it;
        }
        return old_size - size();
    }

    [[nodiscard]] static constexpr bool need_explicit_dtor() {
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
    }

    [[nodiscard]] static constexpr bool is_trivially_copyable() {
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
    }

    static void prefetch_heap_block(char* ctrl) {
        // Prefetch the heap-allocated memory region to resolve potential TLB
        // misses.  This is intended to overlap with execution of calculating the hash for a key.
#ifndef EMH_NO_READ_PREFETCH
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_prefetch(static_cast<const char*>(ctrl), _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(static_cast<const void*>(ctrl));
#endif
#endif
    }

    void clearkv() {
        if (need_explicit_dtor()) {
            auto it = cbegin();
            it.init();
            for (; _num_filled; ++it)
                clear_bucket(it.bucket());
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear() {
        if (!need_explicit_dtor() && _num_filled) {
            memset(_bitmask, static_cast<int>(0xFFFFFFFF), (_num_buckets + 7) / 8);
            if (_num_buckets < 8 * sizeof(_bitmask[0]))
                _bitmask[0] = static_cast<bit_type>((1u << _num_buckets) - 1);
        } else if (_num_filled)
            clearkv();

        // EMH_BUCKET(_pairs, _num_buckets) = 0; //_last
        _num_filled = 0;
    }

    void shrink_to_fit() noexcept { rehash(_num_filled + 1); }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems) noexcept {
        const auto required_buckets = (num_elems * _mlf >> 28);
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

#if EMH_HIGH_LOAD
        if (required_buckets < 64 && _num_filled < _num_buckets)
            return false;
#endif

#if EMH_STATIS
        if (_num_filled > EMH_STATIS)
            dump_statics(true);
#endif
        rehash(required_buckets + 2);
        return true;
    }

    void rehash(uint64_t required_buckets) noexcept {
        if (required_buckets < _num_filled)
            return;

        uint64_t buckets = _num_filled > (1u << 16) ? (1u << 16) : 2u;
        while (buckets < required_buckets) {
            buckets *= 2;
        }
        assert(buckets < static_cast<uint64_t>(max_size()));

        auto num_buckets = static_cast<size_type>(buckets);
        auto old_num_filled = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_mask = _num_buckets - 1;
        auto old_pairs = _pairs;
        auto* obmask = _bitmask;

        _num_filled = 0;
        _num_buckets = num_buckets;
        _mask = num_buckets - 1;

        auto* _alloc_base = alloc_bucket(_num_buckets);
        _bitmask = reinterpret_cast<bit_type*>(_alloc_base);
        _pairs = reinterpret_cast<PairT*>(reinterpret_cast<uint8_t*>(_alloc_base) + bitmask_aligned_size(_num_buckets));
        if (is_trivially_copyable()) {
            memset(reinterpret_cast<char*>(_pairs + _num_buckets), 0, sizeof(PairT) * EPACK_SIZE);
        } else {
            // For non-trivially-copyable types, only zero the bucket field of tail sentinels.
            const size_type zero_bucket = 0;
            for (size_type i = 0; i < EPACK_SIZE; ++i)
                std::memcpy(&EMH_BUCKET(_pairs, _num_buckets + i), &zero_bucket, sizeof(zero_bucket));
        }

        const auto mask_byte = (num_buckets + 7) / 8;
        memset(_bitmask, static_cast<unsigned char>(0xFF), mask_byte);
        memset(reinterpret_cast<char*>(_bitmask) + mask_byte, 0, BIT_PACK);
        if (num_buckets < 8 * sizeof(_bitmask[0]))
            _bitmask[0] = static_cast<bit_type>((1u << num_buckets) - 1);

        for (size_type src_bucket = old_mask; _num_filled < old_num_filled; src_bucket--) {
            if (obmask[src_bucket / MASK_BIT] & (1 << (src_bucket % MASK_BIT)))
                continue;

            auto& key = EMH_KEY(old_pairs, src_bucket);
            const auto bucket = find_unique_bucket(key);
            EMH_NEW(std::move(key), std::move(EMH_VAL(old_pairs, src_bucket)), bucket);
            if (need_explicit_dtor())
                old_pairs[src_bucket].~PairT();

            if (src_bucket == 0)
                break;
        }

#if EMH_REHASH_LOG
        if (_num_filled > EMH_REHASH_LOG) {
            auto mbucket = bucket_main();
            char buff[255] = {0};
            snprintf(buff, sizeof(buff), "    _num_filled/collision/main/K.V/pack/ = %u/%.2lf%%(%.2lf%%)/%s.%s/%zd",
                     _num_filled, 200.0f * (_num_filled - mbucket) / _mask, 100.0f * mbucket / _mask,
                     typeid(KeyT).name(), typeid(ValueT).name(), sizeof(_pairs[0]));
#ifdef EMH_LOG
            static size_t ihashs = 0;
            EMH_LOG << "rhash_nums = " << ihashs++ << "|" << __FUNCTION__ << "|" << buff << std::endl;
#else
            puts(buff);
#endif
        }
#endif

        dealloc_bucket(reinterpret_cast<PairT*>(obmask), old_num_buckets);
        assert(old_num_filled == _num_filled);
    }

private:
    // Can we fit another element?
    inline bool check_expand_need() noexcept { return reserve(_num_filled); }

    void clear_bucket(size_type bucket) {
        emh_cls(bucket);
        _num_filled--;
        if (need_explicit_dtor())
            _pairs[bucket].~PairT();
    }

    // template<typename UType, typename std::enable_if<std::is_integral<UType>::value, size_type>::type = 0>
    template <typename UType> size_type erase_key(const UType& key) {
        const auto bucket = hash_key(key) & _mask;
        if (emh_empty(bucket))
            return INACTIVE;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto eqkey = _eq(key, EMH_KEY(_pairs, bucket));
        if (eqkey) {
            if (next_bucket == bucket)
                return bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (is_trivially_copyable())
                EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
            else
                EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));

            EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        } else if (next_bucket == bucket)
            return INACTIVE;
        /* else if (EMH_UNLIKELY(bucket != hash_key(EMH_KEY(_pairs, bucket)) & _mask))
            return INACTIVE;
        */

        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
                EMH_BUCKET(_pairs, prev_bucket) = (nbucket == next_bucket) ? prev_bucket : nbucket;
                return next_bucket;
            }

            if (nbucket == next_bucket)
                break;
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return INACTIVE;
    }

    size_type erase_bucket(const size_type bucket) {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto main_bucket = hash_key(EMH_KEY(_pairs, bucket)) & _mask;
        if (bucket == main_bucket) {
            if (bucket != next_bucket) {
                const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
                if (is_trivially_copyable())
                    EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
                else
                    EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
                EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            }
            return next_bucket;
        }

        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        EMH_BUCKET(_pairs, prev_bucket) = (bucket == next_bucket) ? prev_bucket : next_bucket;
        return bucket;
    }

    // Find the bucket with this key, or return bucket size
    template <typename K = KeyT> size_type find_filled_hash(const K& key, const size_t key_hash) const {
        const auto bucket = size_type(key_hash & _mask);
        if (emh_empty(bucket))
            return _num_buckets;

        prefetch_heap_block(reinterpret_cast<char*>(&_pairs[bucket]));

        auto next_bucket = bucket;
        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            prefetch_heap_block(reinterpret_cast<char*>(&_pairs[nbucket]));
            next_bucket = nbucket;
        }

        return _num_buckets;
    }

    // Find the bucket with this key, or return bucket size
    template <typename K = KeyT> EMH_INLINE size_type find_filled_bucket(const K& key) const {
        const auto bucket = size_type(hash_key(key) & _mask);
        if (emh_empty(bucket))
            return _num_buckets;

        prefetch_heap_block(reinterpret_cast<char*>(&_pairs[bucket]));
        auto next_bucket = bucket;

        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                return _num_buckets;
            prefetch_heap_block(reinterpret_cast<char*>(&_pairs[nbucket]));
            next_bucket = nbucket;
        }

        return _num_buckets;
    }

    // Relocate a "guest" bucket (occupying another key's main position) to an
    // empty slot, freeing the main bucket for its rightful owner. Adapted from
    // Lua's table design so the chain head always sits at its hash position.
    //
    // before: kmain --> ... --> prev --> kbucket --> next
    // after : kmain --> ... --> prev --> new_bucket --> next , kbucket freed
    size_type kickout_bucket(const size_type kmain, const size_type kbucket) {
        const auto next_bucket = EMH_BUCKET(_pairs, kbucket);
        const auto new_bucket = find_empty_bucket(next_bucket, kbucket);
        const auto prev_bucket = find_prev_bucket(kmain, kbucket);
        new (_pairs + new_bucket) PairT(std::move(_pairs[kbucket]));
        if (need_explicit_dtor())
            _pairs[kbucket].~PairT();

        if (next_bucket == kbucket)
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        EMH_BUCKET(_pairs, prev_bucket) = new_bucket;

        emh_set(new_bucket);
        return kbucket;
    }

    // Core insert/lookup dispatcher using separate chaining with kickout:
    //  1. If the main bucket is empty, use it directly.
    //  2. If it holds a guest (key whose hash maps elsewhere), kick the guest
    //     out and claim the main bucket.
    //  3. Otherwise walk the collision chain: return the matching bucket, or
    //     append a new empty slot at the chain tail for a fresh insertion.
    // isempty is set to false if the key already exists, true if a new slot
    // was allocated.
    template <typename K = KeyT> size_type find_or_allocate(const K& key, bool& isempty) {
        const auto bucket = hash_key(key) & _mask;
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        if (emh_empty(bucket)) {
            isempty = true;
            return bucket;
        } else if (_eq(key, bucket_key)) {
            isempty = false;
            return bucket;
        }

        isempty = true;
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        // check current bucket_key is in main bucket or not
        const auto kmain_bucket = hash_key(bucket_key) & _mask;
        if (kmain_bucket != bucket)
            return kickout_bucket(kmain_bucket, bucket);
        else if (next_bucket == bucket)
            return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket, bucket);

#if EMH_LRU_SET
        auto prev_bucket = bucket;
#endif
        // find next linked bucket and check key, if lru is set then swap current key with prev_bucket
        while (true) {
            if (EMH_UNLIKELY(_eq(key, EMH_KEY(_pairs, next_bucket)))) {
                isempty = false;
#if EMH_LRU_SET
                EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, prev_bucket));
                return prev_bucket;
#else
                return next_bucket;
#endif
            }

#if EMH_LRU_SET
            prev_bucket = next_bucket;
#endif

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            prefetch_heap_block(reinterpret_cast<char*>(&_pairs[nbucket]));
            next_bucket = nbucket;
        }

        // find a new empty and link it to tail, TODO link after main bucket?
        const auto new_bucket = find_empty_bucket(next_bucket, bucket); // : find_empty_bucket(next_bucket);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    size_type find_empty_bucket(const size_type bucket_from, const size_type main_bucket) {
        assert(_num_filled < _num_buckets); // must have empty slots
#if EMH_ITER_SAFE
        const auto boset = bucket_from % 8;
        auto* const align = reinterpret_cast<uint8_t*>(_bitmask) + bucket_from / 8;
        static_cast<void>(main_bucket);
        size_t bmask;
        memcpy(&bmask, align + 0, sizeof(bmask));
        bmask >>= boset; // bmask |= ((size_t)align[8] << (SIZE_BIT - boset));
        if (EMH_LIKELY(bmask != 0))
            return bucket_from + CTZ(bmask);
#else
        const auto boset = bucket_from % 8;
        auto* const align = reinterpret_cast<uint8_t*>(_bitmask) + bucket_from / 8;
        static_cast<void>(main_bucket);
        size_t bmask;
        memcpy(&bmask, align, sizeof(bmask));
        bmask >>= boset;
        if (EMH_LIKELY(bmask != 0))
            return bucket_from + CTZ(bmask);
#endif

        const auto qmask = _mask / SIZE_BIT;
        auto& last = EMH_BUCKET(_pairs, _num_buckets);
        for (;;) {
            last &= qmask;
            size_t bmask2;
            memcpy(&bmask2, _bitmask + last * sizeof(size_t), sizeof(bmask2));
            if (bmask2 != 0)
                return last * SIZE_BIT + CTZ(bmask2);
            const auto next1 = (qmask / 2 + last) & qmask;
            size_t bmask1;
            memcpy(&bmask1, _bitmask + next1 * sizeof(size_t), sizeof(bmask1));
            if (bmask1 != 0) {
                last = next1;
                return next1 * SIZE_BIT + CTZ(bmask1);
            }
            last += 1;
        }
    }

    // key is not in this map. Find a place to put it.
    size_type find_unique_empty(const size_type bucket_from) {
        const auto boset = bucket_from % 8;
        auto* const align = reinterpret_cast<uint8_t*>(_bitmask) + bucket_from / 8;

#if EMH_ITER_SAFE
        size_t bmask;
        memcpy(&bmask, align + 0, sizeof(bmask));
        bmask >>= boset;
#else
        size_t bmask;
        memcpy(&bmask, align, sizeof(bmask));
        bmask >>= boset;
#endif
        if (EMH_LIKELY(bmask != 0))
            return bucket_from + CTZ(bmask);

        const auto qmask = _mask / SIZE_BIT;
        for (auto last = (bucket_from + _mask) & qmask;;) {
            size_t bmask2;
            memcpy(&bmask2, _bitmask + last * sizeof(size_t), sizeof(bmask2));
            if (EMH_LIKELY(bmask2 != 0))
                return last * SIZE_BIT + CTZ(bmask2);
            last = (last + 1) & qmask;
        }

        return 0;
    }

    size_type find_last_bucket(size_type main_bucket) const {
        auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
        if (next_bucket == main_bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    size_type find_prev_bucket(size_type main_bucket, const size_type bucket) const {
        auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
        if (next_bucket == bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    size_type find_unique_bucket(const KeyT& key) {
        const size_type bucket = hash_key(key) & _mask;
        if (emh_empty(bucket))
            return bucket;

        // check current bucket_key is in main bucket or not
        const auto kmain_bucket = hash_key(EMH_KEY(_pairs, bucket)) & _mask;
        if (EMH_UNLIKELY(kmain_bucket != bucket))
            return kickout_bucket(kmain_bucket, bucket);

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket != bucket)
            next_bucket = find_last_bucket(next_bucket);

        // find a new empty and link it to tail
        return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket, bucket);
    }

#if EMH_INT_HASH
    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    inline static uint64_t hash64(uint64_t key) {
#if __SIZEOF_INT128__ && EMH_INT_HASH == 1
        __uint128_t r = key;
        r *= KC;
        return static_cast<uint64_t>(r >> 64) ^ static_cast<uint64_t>(r);
#elif EMH_INT_HASH == 2
        // MurmurHash3Mixer
        uint64_t h = key;
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccd;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53;
        h ^= h >> 33;
        return h;
#elif _WIN64 && EMH_INT_HASH == 1
        uint64_t high;
        return _umul128(key, KC, &high) ^ high;
#elif EMH_INT_HASH == 3
        auto ror = (key >> 32) | (key << 32);
        auto low = key * 0xA24BAED4963EE407ull;
        auto high = ror * 0x9FB21C651E98DF25ull;
        auto mix = low + high;
        return mix;
#elif EMH_INT_HASH == 1
        uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
        return (r >> 32) + r;
#elif EMH_WYHASH64
        return emh_wyhash64(key, KC);
#else
        uint64_t x = key;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
#endif
    }
#endif

    template <typename UType, typename std::enable_if<std::is_integral<UType>::value, size_type>::type = 0>
    EMH_INLINE size_type hash_key(const UType key) const {
#if EMH_INT_HASH
        return static_cast<size_type>(hash64(key));
#elif EMH_IDENTITY_HASH
        return static_cast<size_type>(key + (key >> 24));
#else
        return static_cast<size_type>(_hasher(key));
#endif
    }

    template <typename UType, typename std::enable_if<std::is_same<UType, std::string>::value, size_type>::type = 0>
    EMH_INLINE size_type hash_key(const UType& key) const {
        EMH_MSAN_UNPOISON(&key, sizeof(key));
        EMH_MSAN_UNPOISON(key.data(), key.size());
#if EMH_WY_HASH
        return static_cast<size_type>(emh_wyhash(key.data(), key.size(), 0));
#else
        return static_cast<size_type>(_hasher(key));
#endif
    }

    template <typename UType,
              typename std::enable_if<!std::is_integral<UType>::value && !std::is_same<UType, std::string>::value,
                                      size_type>::type = 0>
    EMH_INLINE size_type hash_key(const UType& key) const {
        return static_cast<size_type>(_hasher(key));
    }

    // Safe inline replacements for EMH_SET/EMH_CLS/EMH_EMPTY macros:
    // evaluate the bucket index exactly once, avoiding UB on side-effecting args.
    EMH_INLINE void emh_set(const size_type n) {
        _bitmask[n / MASK_BIT] &= static_cast<bit_type>(~(1 << (n % MASK_BIT)));
    }
    EMH_INLINE void emh_cls(const size_type n) { _bitmask[n / MASK_BIT] |= static_cast<bit_type>(1 << (n % MASK_BIT)); }
    EMH_INLINE bool emh_empty(const size_type n) const {
        return (_bitmask[n / MASK_BIT] & static_cast<bit_type>(1 << (n % MASK_BIT))) != 0;
    }

private:
    using bit_type = uint8_t; // uint8_t uint16_t, uint32_t.
    bit_type* _bitmask;
    PairT* _pairs;
    HashT _hasher;
    EqT _eq;
    PairAlloc _alloc;
    size_type _mask;
    size_type _num_buckets;

    size_type _num_filled;
    uint32_t _mlf;

private:
    static constexpr uint32_t BIT_PACK = sizeof(uint64_t);
    static constexpr uint32_t MASK_BIT = sizeof(_bitmask[0]) * 8;
    static constexpr uint32_t SIZE_BIT = sizeof(size_t) * 8;
    static constexpr uint32_t EPACK_SIZE = sizeof(PairT) >= sizeof(size_t) ? 2 : 1; // > 1
};
} // namespace emhash7
// namespace emhash7
#if __cplusplus >= 201103L
// template <class Key, class Val> using ehmap7 = emhash7::HashMap<Key, Val, std::hash<Key>, std::equal_to<Key>>;
#endif

// 1. improve rehash and find miss performance(reduce peak memory)
// 2. load_factor > 1.0 && add grow ration
