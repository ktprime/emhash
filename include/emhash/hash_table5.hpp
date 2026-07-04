// emhash5::HashMap for C++17/20
// version 2.1.2
// https://github.com/ktprime/emhash/blob/master/hash_table5.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2026 Huang Yuanbing & bailuzhou AT 163.com
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

/// @file hash_table5.hpp
/// @brief Three-way hybrid open addressing hash map (emhash5)
/// @version 2.1.2
/// @copyright Copyright (c) 2019-2026 Huang Yuanbing

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
#undef EMH_BUCKET
#undef EMH_NEW
#undef EMH_EMPTY
#undef EMH_PREVET
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
    _num_filled++
#elif EMH_BUCKET_INDEX == 2
#ifdef EMH_PREVET
#undef EMH_PREVET
#endif
#define EMH_KEY(p, n) p[n].first.first
#define EMH_VAL(p, n) p[n].first.second
#define EMH_BUCKET(p, n) p[n].second
#define EMH_PREVET(p, n) emh_prevet_get(p, n)
#define EMH_PKV(p, n) p[n].first
#define EMH_NEW(key, val, bucket)                                                                                      \
    new (_pairs + bucket) PairT(value_type(key, val), bucket);                                                         \
    _num_filled++
#else
#ifdef EMH_PREVET
#undef EMH_PREVET
#endif
#define EMH_KEY(p, n) p[n].first
#define EMH_VAL(p, n) p[n].second
#define EMH_BUCKET(p, n) p[n].bucket
#define EMH_PREVET(p, n) emh_prevet_get(p, n)
#define EMH_PKV(p, n) p[n]
#define EMH_NEW(key, val, bucket)                                                                                      \
    new (_pairs + (bucket)) PairT(key, val, bucket);                                                                   \
    _num_filled++;                                                                                                     \
    if ((bucket) < _first)                                                                                             \
    _first = bucket
#endif

#define EMH_EMPTY(p, b) (0 > static_cast<size_type>(EMH_BUCKET(p, b)))

namespace emhash5 {

#if EMH_SIZE_TYPE_64BIT
using size_type = int64_t;
static constexpr size_type INACTIVE = 0 - 0x1ull;
#elif EMH_SIZE_TYPE_16BIT
using size_type = int16_t;
static constexpr size_type INACTIVE = 0xFFFF;
#else
using size_type = int32_t;
static constexpr size_type INACTIVE = int32_t(0xFFFFFFFF);
#endif

#ifdef EMH_ALLOC
#ifndef EMH_MALIGN
static constexpr uint32_t EMH_MALIGN = 16;
#endif
#endif

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

    template <typename K, typename V>
    entry(K&& key, std::tuple<V> val, size_type ibucket) : second(std::get<0>(val)), first(std::forward<K>(key)) {
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
    First first; // long
    size_type bucket;
    Second second; // int
#else
    Second second;
    size_type bucket;
    First first;
#endif
};

/// @brief Safe access to prev-bucket field (avoids strict aliasing UB)
#if EMH_BUCKET_INDEX == 2
template <typename P> static inline size_type emh_prevet_get(P p, size_t n) {
    size_type result;
    std::memcpy(&result, &p[n].first.first, sizeof(result));
    return result;
}
template <typename P> static inline void emh_prevet_set(P p, size_t n, size_type val) {
    std::memcpy(&p[n].first.first, &val, sizeof(val));
}
#else
template <typename P> static inline size_type emh_prevet_get(P p, size_t n) {
    size_type result;
    std::memcpy(&result, &p[n].first, sizeof(result));
    return result;
}
template <typename P> static inline void emh_prevet_set(P p, size_t n, size_type val) {
    std::memcpy(&p[n].first, &val, sizeof(val));
}
#endif

/// @brief Three-way hybrid open addressing hash map (emhash5)
///
/// emhash5 uses a three-way probing strategy:
/// - Linear probing for short distances
/// - Quadratic probing for medium distances
/// - Bidirectional search for long probe chains
///
/// With EMH_HIGH_LOAD enabled, supports load factors up to 0.999.
///
/// @tparam KeyT    Key type
/// @tparam ValueT  Mapped value type
/// @tparam HashT   Hash functor (default: std::hash<KeyT>)
/// @tparam EqT     Key equality functor (default: std::equal_to<KeyT>)
/// @tparam AllocT  Allocator type (default: std::allocator<std::pair<KeyT, ValueT>>)
///
/// @note Header-only: just `#include "emhash/hash_table5.hpp"` and use `emhash5::HashMap`.
/// @note Not thread-safe. Concurrent read-only access is safe.
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>,
          typename AllocT = std::allocator<std::pair<KeyT, ValueT>>>
class HashMap {
    static_assert(std::is_copy_constructible<KeyT>::value || std::is_move_constructible<KeyT>::value,
                  "KeyT must be copy-constructible or move-constructible");
    static_assert(std::is_copy_constructible<ValueT>::value || std::is_move_constructible<ValueT>::value,
                  "ValueT must be copy-constructible or move-constructible");

#ifndef EMH_DEFAULT_LOAD_FACTOR
    constexpr static float EMH_DEFAULT_LOAD_FACTOR = 0.80f;
#endif
    constexpr static float EMH_MIN_LOAD_FACTOR = 0.25f; //< 0.5

public:
    using htype = HashMap<KeyT, ValueT, HashT, EqT, AllocT>;
    using value_type = std::pair<KeyT, ValueT>;

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

    using key_type = KeyT;
    using val_type = ValueT;
    using mapped_type = ValueT;
    using hasher = HashT;
    using key_equal = EqT;
    using allocator_type = AllocT;
    using PairAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<PairT>;
    using PairAllocTraits = std::allocator_traits<PairAlloc>;
#if EMH_HIGH_LOAD
    static_assert(
        sizeof(KeyT) >= sizeof(size_type),
        "EMH_HIGH_LOAD requires sizeof(KeyT) >= sizeof(size_type). Use a larger key type or disable EMH_HIGH_LOAD.");
#endif
    using reference = PairT&;
    using const_reference = const PairT&;

    class const_iterator;
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = value_pair;

        using pointer = value_pair*;
        using reference = value_pair&;

        iterator() {
            _map = nullptr;
            _bucket = -1;
        }
        iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket) {}

        iterator& operator++() {
            goto_next_element();
            return *this;
        }

        iterator operator++(int) {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const { return _map->EMH_PKV(_pairs, _bucket); }
        pointer operator->() const { return std::addressof(_map->EMH_PKV(_pairs, _bucket)); }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }

        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

        size_type bucket() const { return _bucket; }

    private:
        void goto_next_element() {
            while (static_cast<int>(_map->EMH_BUCKET(_pairs, ++_bucket)) < 0)
                ;
        }

    public:
        const htype* _map;
        size_type _bucket;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = value_pair;

        using pointer = const value_pair*;
        using reference = const value_pair&;

        // const_iterator() { }
        const_iterator(const iterator& proto) : _map(proto._map), _bucket(proto._bucket) {}
        const_iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket) {}

        const_iterator& operator++() {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int) {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const { return _map->EMH_PKV(_pairs, _bucket); }
        pointer operator->() const { return &(_map->EMH_PKV(_pairs, _bucket)); }

        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

        size_type bucket() const { return _bucket; }

    private:
        void goto_next_element() {
            while (static_cast<int>(_map->EMH_BUCKET(_pairs, ++_bucket)) < 0)
                ;
        }

    public:
        const htype* _map;
        size_type _bucket;
    };

    void init(size_type bucket, float mlf = EMH_DEFAULT_LOAD_FACTOR) noexcept {
        _pairs = nullptr;
        _mask = _num_buckets = 0;
        _num_filled = 0;
#if EMH_HIGH_LOAD
        _ehead = 0;
#endif
        _mlf = static_cast<uint32_t>((1 << 27) / EMH_DEFAULT_LOAD_FACTOR);
        max_load_factor(mlf);
        rehash(static_cast<uint64_t>(bucket));
    }

    explicit HashMap(size_type bucket = 2, float mlf = EMH_DEFAULT_LOAD_FACTOR) noexcept { init(bucket, mlf); }

    HashMap(const HashMap& rhs) : _alloc(PairAllocTraits::select_on_container_copy_construction(rhs._alloc)) {
        if (rhs.load_factor() > EMH_MIN_LOAD_FACTOR) {
            _pairs = alloc_bucket(rhs._num_buckets);
            clone(rhs);
        } else {
            init(rhs._num_filled + 2, rhs.max_load_factor());
            for (auto it = rhs.begin(); it != rhs.end(); ++it)
                (void)insert_unique(it->first, it->second);
        }
    }

    HashMap(HashMap&& rhs) noexcept : _alloc(std::move(rhs._alloc)) {
        init(0);
        *this = std::move(rhs);
    }

    HashMap(std::initializer_list<value_type> ilist) noexcept {
        init(static_cast<size_type>(ilist.size()));
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            (void)do_insert(*it);
    }

    template <class InputIt> HashMap(InputIt first, InputIt last, size_type bucket_count = 4) noexcept {
        init(static_cast<size_type>(std::distance(first, last)) + bucket_count);
        for (; first != last; ++first)
            (void)emplace(*first);
    }

    explicit HashMap(const AllocT& alloc) noexcept : _alloc(PairAlloc(alloc)) { init(2, EMH_DEFAULT_LOAD_FACTOR); }

    HashMap(size_type bucket, float mlf, const AllocT& alloc) noexcept : _alloc(PairAlloc(alloc)) { init(bucket, mlf); }

    HashMap& operator=(const HashMap& rhs) {
        if (this == &rhs)
            return *this;
        if constexpr (PairAllocTraits::propagate_on_container_copy_assignment::value)
            _alloc = rhs._alloc;

        if (rhs.load_factor() < EMH_MIN_LOAD_FACTOR) {
            clear();
#if EMH_SMALL_SIZE
            if (_pairs != reinterpret_cast<PairT*>(_small))
#endif
                dealloc_bucket(_pairs, _num_buckets);
            _pairs = nullptr;

            rehash(static_cast<uint64_t>(rhs._num_filled) + 2);
            for (auto it = rhs.begin(); it != rhs.end(); ++it)
                (void)insert_unique(it->first, it->second);
            return *this;
        }

        clearkv();
        if (_num_buckets < rhs._num_buckets || _num_buckets > 2 * rhs._num_buckets) {
#if EMH_SMALL_SIZE
            if (_pairs != reinterpret_cast<PairT*>(_small))
#endif
                dealloc_bucket(_pairs, _num_buckets);
            _pairs = alloc_bucket(rhs._num_buckets);
        }

        clone(rhs);
        return *this;
    }

    HashMap& operator=(HashMap&& rhs) noexcept(PairAllocTraits::propagate_on_container_move_assignment::value ||
                                               std::is_nothrow_move_assignable<HashT>::value) {
        if (this == &rhs)
            return *this;

#if EMH_SMALL_SIZE
        if (_pairs == reinterpret_cast<PairT*>(_small) || rhs._pairs == reinterpret_cast<PairT*>(rhs._small)) {
            clear();
            if (rhs.empty())
                return *this;
            if (rhs._num_buckets > _num_buckets) {
                if (_pairs != reinterpret_cast<PairT*>(_small)) {
                    dealloc_bucket(_pairs, _num_buckets);
                    _pairs = reinterpret_cast<PairT*>(_small);
                }
                if (rhs._num_buckets > EMH_SMALL_SIZE)
                    _pairs = alloc_bucket(rhs._num_buckets);
            }
            clone(rhs);
            rhs.clear();
            return *this;
        }
#endif
        swap(rhs);
        rhs.clear();
        return *this;
    }

    template <typename Con> bool operator==(const Con& rhs) const noexcept {
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
        clearkv();
#if EMH_SMALL_SIZE
        if (_pairs != reinterpret_cast<PairT*>(_small))
#endif
            dealloc_bucket(_pairs, _num_buckets);
        _pairs = nullptr;
    }

    void clone(const HashMap& rhs) {
        _hasher = rhs._hasher;
        //        _eq          = rhs._eq;
        _num_buckets = rhs._num_buckets;
        _num_filled = rhs._num_filled;
        _mlf = rhs._mlf;
        _last = rhs._last;
        _first = rhs._first;
        _mask = rhs._mask;
#if EMH_HIGH_LOAD
        _ehead = rhs._ehead;
#endif

        auto opairs = rhs._pairs;

        if (is_trivially_copyable())
            memcpy(reinterpret_cast<char*>(_pairs), opairs, (static_cast<size_t>(_num_buckets) + 2u) * sizeof(PairT));
        else {
            // Zero-fill all buckets first so MSan sees them as initialized.
            // Occupied buckets will be overwritten by placement-new below.
            memset(reinterpret_cast<char*>(_pairs), 0, static_cast<size_t>(_num_buckets) * sizeof(PairT));
            for (size_type bucket = 0; bucket < _num_buckets; bucket++) {
                auto next_bucket = EMH_BUCKET(opairs, bucket);
                EMH_BUCKET(_pairs, bucket) = next_bucket;
                if (static_cast<int>(next_bucket) >= 0)
                    new (_pairs + bucket) PairT(opairs[bucket]);
#if EMH_HIGH_LOAD
                else if (next_bucket != INACTIVE)
                    emh_prevet_set(_pairs, bucket, emh_prevet_get(opairs, bucket));
#endif
            }
            // For non-trivially-copyable types, only init bucket field of tail sentinels
            // (memcpy of whole PairT would read uninitialized std::string key — MSan UB)
            const size_type zero_bucket = 0;
            for (size_type i = 0; i < 2; ++i)
                std::memcpy(&EMH_BUCKET(_pairs, _num_buckets + i), &zero_bucket, sizeof(zero_bucket));
        }
    }

    void swap(HashMap& rhs) noexcept {
#if EMH_SMALL_SIZE
        if (_pairs == reinterpret_cast<PairT*>(_small) || rhs._pairs == reinterpret_cast<PairT*>(rhs._small)) {
            if (is_trivially_copyable()) {
                char tmp[(EMH_SMALL_SIZE + 2) * sizeof(PairT)];
                memcpy(tmp, _small, sizeof(tmp));
                memcpy(_small, rhs._small, sizeof(tmp));
                memcpy(rhs._small, tmp, sizeof(tmp)); // copy once if only one small map

                // _small is embedded in each object, so _pairs must always point to its own _small
                // When both use _small: keep each pointing to its own _small (data already swapped by memcpy)
                // When only one uses _small: transfer heap pointer to the other, point the other to its own _small
                if (_pairs == reinterpret_cast<PairT*>(_small) && rhs._pairs == reinterpret_cast<PairT*>(rhs._small)) {
                    // both use _small, nothing to do
                } else if (_pairs == reinterpret_cast<PairT*>(_small)) {
                    // this uses _small, rhs uses heap
                    _pairs = rhs._pairs;                               // this gets the heap pointer
                    rhs._pairs = reinterpret_cast<PairT*>(rhs._small); // rhs points to its own _small
                } else {
                    // rhs uses _small, this uses heap
                    rhs._pairs = _pairs;                       // rhs gets the heap pointer
                    _pairs = reinterpret_cast<PairT*>(_small); // this points to its own _small
                }

                std::swap(_alloc, rhs._alloc);
                std::swap(_hasher, rhs._hasher);
                std::swap(_num_buckets, rhs._num_buckets);
                std::swap(_num_filled, rhs._num_filled);
                std::swap(_mask, rhs._mask);
                std::swap(_mlf, rhs._mlf);
                std::swap(_last, rhs._last);
                std::swap(_first, rhs._first);
#if EMH_HIGH_LOAD
                std::swap(_ehead, rhs._ehead);
#endif
                return;
            } else {
                HashMap tmp(*this);
                *this = rhs;
                rhs = tmp;
                return;
            }
        }
#endif
        std::swap(_eq, rhs._eq);
        std::swap(_alloc, rhs._alloc);
        std::swap(_hasher, rhs._hasher);
        std::swap(_pairs, rhs._pairs);
        std::swap(_num_buckets, rhs._num_buckets);
        std::swap(_num_filled, rhs._num_filled);
        std::swap(_mask, rhs._mask);
        std::swap(_mlf, rhs._mlf);
        std::swap(_last, rhs._last);
        std::swap(_first, rhs._first);
#if EMH_HIGH_LOAD
        std::swap(_ehead, rhs._ehead);
#endif
    }

    // -------------------------------------------------------------
    iterator begin() noexcept {
        if (_num_filled == 0)
            return end();

        size_type bucket = _first;
        while (EMH_EMPTY(_pairs, bucket)) {
            ++bucket;
        }
        _first = bucket;
        return {this, bucket};
    }

    const_iterator cbegin() const noexcept {
        if (_num_filled == 0)
            return end();

        size_type bucket = _first;
        while (EMH_EMPTY(_pairs, bucket)) {
            ++bucket;
        }
        return {this, bucket};
    }
    const_iterator begin() const noexcept { return cbegin(); }

    iterator end() noexcept { return {this, _num_buckets}; }
    const_iterator end() const noexcept { return cend(); }
    const_iterator cend() const noexcept { return {this, _num_buckets}; }

    size_type size() const noexcept { return _num_filled; }
    bool empty() const noexcept { return _num_filled == 0; }
    size_type bucket_count() const noexcept { return _num_buckets; }

    [[nodiscard]] HashT hash_function() const noexcept { return static_cast<const HashT&>(_hasher); }
    [[nodiscard]] EqT key_eq() const noexcept { return static_cast<const EqT&>(_eq); }
    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator_type(_alloc); }

    float load_factor() const noexcept {
        return _num_buckets ? static_cast<float>(_num_filled) / static_cast<float>(_num_buckets) : 0.0f;
    }
    float max_load_factor() const noexcept { return static_cast<float>(1 << 27) / static_cast<float>(_mlf); }
    void max_load_factor(float mlf) noexcept {
        if (mlf <= 0.999f && mlf > EMH_MIN_LOAD_FACTOR)
            _mlf = static_cast<uint32_t>((1 << 27) / mlf);
    }

    [[nodiscard]] constexpr uint64_t max_size() const { return 1ull << (sizeof(_num_buckets) * 8 - 1); }
    [[nodiscard]] constexpr uint64_t max_bucket_count() const { return max_size(); }

#if EMH_STATIS
    // Returns the bucket number where the element with key k is located.
    size_type bucket_slot(const KeyT& key) const {
        const auto bucket = key_to_bucket(key);
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (static_cast<int>(next_bucket) < 0)
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        return hash_main(bucket) + 1;
    }

    // Returns the number of elements in bucket n.
    size_type bucket_size(const size_type bucket) const {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (static_cast<int>(next_bucket) < 0)
            return 0;

        next_bucket = hash_main(bucket);
        size_type ibucket_size = 1;

        // iterator each item in current main bucket
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            ibucket_size++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    size_type get_main_bucket(const uint32_t bucket) const {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (static_cast<int>(next_bucket) < 0)
            return INACTIVE;

        return hash_main(bucket);
    }

    size_type get_diss(uint32_t bucket, uint32_t next_bucket, const uint32_t slots) const {
        constexpr static uint32_t EMH_CACHE_LINE_SIZE = 64;
        auto pbucket = reinterpret_cast<uintptr_t>(&_pairs[bucket]);
        auto pnext = reinterpret_cast<uintptr_t>(&_pairs[next_bucket]);
        if (pbucket / EMH_CACHE_LINE_SIZE == pnext / EMH_CACHE_LINE_SIZE)
            return 0;
        uint32_t diff = pbucket > pnext ? (pbucket - pnext) : (pnext - pbucket);
        if (diff / EMH_CACHE_LINE_SIZE < slots - 1)
            return diff / EMH_CACHE_LINE_SIZE + 1;
        return slots - 1;
    }

    int get_bucket_info(const uint32_t bucket, uint32_t steps[], const uint32_t slots) const {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (static_cast<int>(next_bucket) < 0)
            return -1;

        const auto main_bucket = hash_main(bucket);
        if (next_bucket == main_bucket)
            return 1;
        else if (main_bucket != bucket)
            return 0;

        steps[get_diss(bucket, next_bucket, slots)]++;
        uint32_t ibucket_size = 2;
        // find a new empty and linked it to tail
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;

            steps[get_diss(nbucket, next_bucket, slots)]++;
            ibucket_size++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    void dump_statics() const {
        const int slots = 128;
        uint32_t buckets[slots + 1] = {0};
        uint32_t steps[slots + 1] = {0};
        for (uint32_t bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, slots);
            if (bsize > 0)
                buckets[bsize]++;
        }

        uint32_t sumb = 0, collision = 0, sumc = 0, finds = 0, sumn = 0;
        puts("============== buckets size ration =========");
        for (uint32_t i = 0; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            const auto bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;
            sumb += bucketsi;
            sumn += bucketsi * i;
            collision += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            printf("  %2u  %8u  %2.2lf|  %.2lf\n", i, bucketsi, bucketsi * 100.0 * i / _num_filled,
                   sumn * 100.0 / _num_filled);
        }

        puts("========== collision miss ration ===========");
        for (uint32_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
            sumc += steps[i];
            if (steps[i] <= 2)
                continue;
            printf("  %2u  %8u  %.2lf  %.2lf\n", i, steps[i], steps[i] * 100.0 / collision, sumc * 100.0 / collision);
        }

        if (sumb == 0)
            return;
        printf(
            "    _num_filled/bucket_size/packed collision/cache_miss/hit_find = %u/%.2lf/%d/ %.2lf%%/%.2lf%%/%.2lf\n",
            _num_filled, _num_filled * 1.0 / sumb, static_cast<int>(sizeof(PairT)), (collision * 100.0 / _num_filled),
            (collision - steps[0]) * 100.0 / _num_filled, finds * 1.0 / _num_filled);
        assert(sumc == collision);
        assert(sumn == _num_filled);
        puts("============== buckets size end =============");
    }
#endif

    // ------------------------------------------------------------
    template <typename K = KeyT> iterator find(const K& key) noexcept { return {this, find_filled_key(key)}; }

    template <typename K = KeyT> const_iterator find(const K& key) const noexcept {
        return {this, find_filled_key(key)};
    }

    template <typename K = KeyT> iterator find(const K& key, size_type key_hash) noexcept {
        const auto main_bucket = key_hash & _mask;
        return {this, find_hash_bucket(key, main_bucket)};
    }

    template <typename K = KeyT> const_iterator find(const K& key, size_type key_hash) const noexcept {
        const auto main_bucket = key_hash & _mask;
        return {this, find_hash_bucket(key, main_bucket)};
    }

    template <typename K = KeyT> ValueT& at(const K& key) {
        const auto bucket = find_filled_key(key);
        if (bucket == _num_buckets)
            throw std::out_of_range("emhash5::at(): key not found");
        return EMH_VAL(_pairs, bucket);
    }

    template <typename K = KeyT> const ValueT& at(const K& key) const {
        const auto bucket = find_filled_key(key);
        if (bucket == _num_buckets)
            throw std::out_of_range("emhash5::at(): key not found");
        return EMH_VAL(_pairs, bucket);
    }

    template <typename K = KeyT> ValueT& at(const K& key, size_type key_hash) {
        const auto main_bucket = key_hash & _mask;
        const auto bucket = find_hash_bucket(key, main_bucket);
        if (EMH_EMPTY(_pairs, bucket))
            throw std::out_of_range("emhash5::at(): key not found");
        return EMH_VAL(_pairs, bucket);
    }

    template <typename K = KeyT> const ValueT& at(const K& key, size_type key_hash) const {
        const auto main_bucket = key_hash & _mask;
        const auto bucket = find_hash_bucket(key, main_bucket);
        if (EMH_EMPTY(_pairs, bucket))
            throw std::out_of_range("emhash5::at(): key not found");
        return EMH_VAL(_pairs, bucket);
    }

    template <typename K = KeyT> bool contains(const K& key) const noexcept {
        return find_filled_key(key) != _num_buckets;
    }

    template <typename K = KeyT> [[nodiscard]] bool contains(const K& key, size_type key_hash) const noexcept {
        const auto main_bucket = key_hash & _mask;
        return find_hash_bucket(key, main_bucket) != _num_buckets;
    }

    template <typename K = KeyT> size_type count(const K& key) const noexcept {
        return find_filled_key(key) == _num_buckets ? 0 : 1;
    }

    template <typename K = KeyT> size_type count(const K& key, size_type key_hash) const noexcept {
        const auto main_bucket = key_hash & _mask;
        return find_hash_bucket(key, main_bucket) == _num_buckets ? 0 : 1;
    }

    template <typename K = KeyT> size_type count_hint(const K& key, size_type main_bucket) const noexcept {
        return find_hash_bucket(key, main_bucket) == _num_buckets ? 0 : 1;
    }

    template <typename K = KeyT> [[nodiscard]] std::pair<iterator, iterator> equal_range(const K& key) noexcept {
        const auto found = find(key);
        if (found.bucket() == _num_buckets)
            return {found, found};
        else
            return {found, std::next(found)};
    }

    template <typename K = KeyT>
    [[nodiscard]] std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
        const auto found = find(key);
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

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& val) {
        check_expand_need();
        const auto bucket = find_or_allocate(key);

        if (EMH_EMPTY(_pairs, bucket)) {
            EMH_NEW(key, val, bucket);
            return ValueT();
        } else {
            ValueT old_value(val);
            std::swap(EMH_VAL(_pairs, bucket), old_value);
            return old_value;
        }
    }

#ifdef EMH_EXT
    /// Returns the matching ValueT or nullptr if k isn't found.
    [[nodiscard]] bool try_get(const KeyT& key, ValueT& val) const {
        const auto bucket = find_filled_key(key);
        const auto found = bucket != _num_buckets;
        if (found) {
            val = EMH_VAL(_pairs, bucket);
        }
        return found;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    [[nodiscard]] ValueT* try_get(const KeyT& key) {
        const auto bucket = find_filled_key(key);
        return bucket != _num_buckets ? &EMH_VAL(_pairs, bucket) : nullptr;
    }

    /// Const version of the above
    [[nodiscard]] const ValueT* try_get(const KeyT& key) const {
        const auto bucket = find_filled_key(key);
        return bucket != _num_buckets ? &EMH_VAL(_pairs, bucket) : nullptr;
    }

    /// set value if key exist
    [[nodiscard]] bool try_set(const KeyT& key, const ValueT& val) {
        const auto bucket = find_filled_key(key);
        if (bucket == _num_buckets)
            return false;

        EMH_VAL(_pairs, bucket) = val;
        return true;
    }

    /// set value if key exist
    [[nodiscard]] bool try_set(const KeyT& key, ValueT&& val) {
        const auto bucket = find_filled_key(key);
        if (bucket == _num_buckets)
            return false;

        EMH_VAL(_pairs, bucket) = std::move(val);
        return true;
    }

    /// Convenience function.
    [[nodiscard]] ValueT get_or_return_default(const KeyT& key) const {
        const auto bucket = find_filled_key(key);
        return bucket == _num_buckets ? ValueT() : EMH_VAL(_pairs, bucket);
    }
#endif

    // -----------------------------------------------------
#if EMH_BUCKET_INDEX == 1
    [[nodiscard]] std::pair<iterator, bool> do_insert(const value_pair& value) {
        const auto bucket = find_or_allocate(value.first);
        const auto bempty = EMH_EMPTY(_pairs, bucket);
        if (bempty) {
            EMH_NEW(value.first, value.second, bucket);
        }
        return {{this, bucket}, bempty};
    }
#endif

    [[nodiscard]] std::pair<iterator, bool> do_insert(const value_type& value) {
        const auto bucket = find_or_allocate(value.first);
        const auto bempty = EMH_EMPTY(_pairs, bucket);
        if (bempty) {
            EMH_NEW(value.first, value.second, bucket);
        }
        return {{this, bucket}, bempty};
    }

    [[nodiscard]] std::pair<iterator, bool> do_insert(value_type&& value) {
        const auto bucket = find_or_allocate(value.first);
        const auto bempty = EMH_EMPTY(_pairs, bucket);
        if (bempty) {
            EMH_NEW(std::move(value.first), std::move(value.second), bucket);
        }
        return {{this, bucket}, bempty};
    }

    template <typename K = KeyT, typename V> std::pair<iterator, bool> do_insert(K&& key, V&& val) {
        const auto bucket = find_or_allocate(key);
        const auto bempty = EMH_EMPTY(_pairs, bucket);
        if (bempty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
        }
        return {{this, bucket}, bempty};
    }

    template <typename K = KeyT, typename V> std::pair<iterator, bool> do_assign(K&& key, V&& val) {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        const auto bempty = EMH_EMPTY(_pairs, bucket);
        if (bempty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
        } else {
            EMH_VAL(_pairs, bucket) = std::forward<V>(val);
        }
        return {{this, bucket}, bempty};
    }

    std::pair<iterator, bool> insert(const value_type& value) {
        check_expand_need();
        return do_insert(value);
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        check_expand_need();
        return do_insert(std::move(value));
    }

    template <typename P> std::pair<iterator, bool> insert(P&& value) {
        check_expand_need();
        return do_insert(std::forward<P>(value));
    }

    iterator insert(const_iterator hint, const value_type& value) {
        if (hint.bucket() != _num_buckets && hint->first == value.first) {
            return {this, hint.bucket()};
        }

        check_expand_need();
        return do_insert(value).first;
    }

    iterator insert(const_iterator hint, value_type&& value) {
        if (hint.bucket() != _num_buckets && hint->first == value.first) {
            return {this, hint.bucket()};
        }

        check_expand_need();
        return do_insert(std::move(value)).first;
    }

    void insert(std::initializer_list<value_type> ilist) {
        reserve(ilist.size() + _num_filled);
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            (void)do_insert(*it);
    }

    template <typename Iter> void insert(Iter first, Iter last) {
        reserve(std::distance(first, last) + _num_filled);
        for (; first != last; ++first)
            (void)emplace(*first);
    }

    // Returns a pointer to the value if key is at bucket, otherwise nullptr.
    ValueT* find_hint(const KeyT& key, size_type bucket) {
        if (!EMH_EMPTY(_pairs, bucket) && EMH_KEY(_pairs, bucket) == key)
            return &EMH_VAL(_pairs, bucket);
        return nullptr;
    }

    template <typename K, typename V> size_type insert_unique(K&& key, V&& val) {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
        return bucket;
    }

    size_type insert_unique(value_type&& value) {
        return insert_unique(std::move(value.first), std::move(value.second));
    }

    size_type insert_unique(const value_type& value) { return insert_unique(value.first, value.second); }

    template <class... Args> size_type emplace_unique(Args&&... args) {
        return insert_unique(std::forward<Args>(args)...);
    }

    template <class... Args> std::pair<iterator, bool> emplace(Args&&... args) {
        check_expand_need();
        return do_insert(std::forward<Args>(args)...);
    }

    template <class... Args> iterator emplace_hint(const_iterator hint, Args&&... args) {
        value_type value(std::forward<Args>(args)...);
        auto bucket = hint.bucket();
        if (bucket != _num_buckets && hint->first == value.first) {
            return {this, bucket};
        }

        check_expand_need();
        return do_insert(std::move(value)).first;
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) {
        check_expand_need();
        return do_insert(key, std::forward<Args>(args)...);
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) {
        check_expand_need();
        return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    template <class... Args> iterator try_emplace(const_iterator hint, const KeyT& key, Args&&... args) {
        (void)hint;
        return try_emplace(key, std::forward<Args>(args)...).first;
    }

    template <class... Args> iterator try_emplace(const_iterator hint, KeyT&& key, Args&&... args) {
        (void)hint;
        return try_emplace(std::move(key), std::forward<Args>(args)...).first;
    }

    template <class M> std::pair<iterator, bool> insert_or_assign(const KeyT& key, M&& val) {
        return do_assign(key, std::forward<M>(val));
    }
    template <class M> std::pair<iterator, bool> insert_or_assign(KeyT&& key, M&& val) {
        return do_assign(std::move(key), std::forward<M>(val));
    }

    template <class M> iterator insert_or_assign(const_iterator hint, const KeyT& key, M&& val) {
        auto bucket = hint.bucket();
        if (bucket != _num_buckets && hint->first == key) {
            hint->second = std::forward<M>(val);
            return {this, bucket};
        }

        return do_assign(key, std::forward<M>(val)).first;
    }

    template <class M> iterator insert_or_assign(const_iterator hint, KeyT&& key, M&& val) {
        auto bucket = hint.bucket();
        if (bucket != _num_buckets && hint->first == key) {
            EMH_VAL(_pairs, bucket) = std::forward<M>(val);
            return {this, bucket};
        }

        return do_assign(std::move(key), std::forward<M>(val)).first;
    }

    /// Like std::map<KeyT, ValueT>::operator[].
    ValueT& operator[](const KeyT& key) {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (EMH_EMPTY(_pairs, bucket)) {
            /* Check if inserting a new value rather than overwriting an old entry */
            EMH_NEW(key, std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    ValueT& operator[](KeyT&& key) {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (EMH_EMPTY(_pairs, bucket)) {
            EMH_NEW(std::move(key), std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    // -------------------------------------------------------
    /// return 0 if not erase
    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key) {
        const auto bucket = erase_key(key);
        if (bucket == INACTIVE)
            return 0;

        clear_bucket(bucket);
        return 1;
    }

    // iterator erase(const_iterator begin_it, const_iterator end_it)
    iterator erase(const_iterator cit) {
        const auto bucket = erase_bucket(cit._bucket);
        clear_bucket(bucket);

        iterator it(this, cit._bucket);
        // erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? ++it : it;
    }

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
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#endif
    }

    [[nodiscard]] static constexpr bool is_trivially_copyable() {
#if __cplusplus >= 201103L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#endif
    }

    void clearkv() noexcept {
        if (need_explicit_dtor()) {
            for (size_type bucket = 0; _num_filled > 0; ++bucket) {
                if (!EMH_EMPTY(_pairs, bucket))
                    clear_bucket(bucket, false);
            }
        }
    }

#if EMH_FIND_HIT
    void reset_bucket(size_type bucket) {
        if constexpr (std::is_integral<KeyT>::value) {
            auto& key = EMH_KEY(_pairs, bucket);
            key = KeyT(0 - 2);
            while (key_to_bucket(key) == bucket)
                key += 1610612741;
        }
    }
#endif

    /// Remove all elements, keeping full capacity.
    void clear() noexcept {
#if EMH_HIGH_LOAD
        if (_ehead > 0)
            clear_empty();
        clearkv();
#else
        if (need_explicit_dtor())
            clearkv();
        else if (_num_filled)
            memset(reinterpret_cast<char*>(_pairs), static_cast<int>(INACTIVE),
                   sizeof(_pairs[0]) * static_cast<size_t>(_num_buckets));
#endif
#if EMH_FIND_HIT
        if constexpr (std::is_integral<KeyT>::value)
            reset_bucket(hash_main(0));
#endif

        _last = _num_buckets / 4;
        _num_filled = 0;
        _first = _num_buckets;
    }

    void shrink_to_fit(const float min_factor = EMH_DEFAULT_LOAD_FACTOR / 4) noexcept {
#if EMH_SMALL_SIZE
        if (_pairs == reinterpret_cast<PairT*>(_small))
            return;
#endif
        if (load_factor() < min_factor) // safe guard
            rehash(static_cast<uint64_t>(_num_filled) + 1);
    }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems) noexcept {
#if EMH_HIGH_LOAD < 1000
        const auto required_buckets = (num_elems * _mlf >> 27);
        if (EMH_LIKELY(required_buckets < static_cast<uint64_t>(_mask)))
            return false;
#else
        const auto required_buckets = (num_elems + num_elems * 1 / 8);
        if (EMH_LIKELY(required_buckets < static_cast<uint64_t>(_mask)))
            return false;
        else if (_num_buckets > EMH_HIGH_LOAD) {
            if (_ehead == 0) {
                set_empty();
                return false;
            } else if (/*_num_filled + 100 < _num_buckets && */ EMH_BUCKET(_pairs, _ehead) != 0 - _ehead) {
                return false;
            }
        } else if (_num_buckets < 16 && _num_filled < _num_buckets)
            return false;
#endif

#if EMH_STATIS
        if (_num_filled > EMH_STATIS)
            dump_statics();
#endif

        rehash(required_buckets + 2);
        return true;
    }

    void rehash(uint64_t required_buckets) noexcept {
        if (required_buckets < static_cast<uint64_t>(_num_filled))
            return;

#if EMH_SMALL_SIZE
        static_assert(EMH_SMALL_SIZE >= 2);
#endif
        uint64_t buckets = _num_filled > static_cast<size_type>(1u << 16) ? (1u << 16) : 2;

        while (buckets < required_buckets) {
            buckets *= 2;
        }
        assert(buckets < max_size());

        auto num_buckets = static_cast<size_type>(buckets);
        auto old_num_filled = _num_filled;
        auto* old_pairs = _pairs;
        auto old_buckets = _num_buckets;

#if EMH_REHASH_LOG
        auto omask = _mask;
        auto last = _last;
        size_type collision = 0;
#endif
#if EMH_HIGH_LOAD
        _ehead = 0;
#endif

        _num_filled = 0;
        _mask = num_buckets - 1;
        _last = num_buckets / 4;
        _first = 0;

#if EMH_PACK_TAIL > 1 && EMH_PACK_TAIL <= 100
        _last = _mask;
        num_buckets += num_buckets * EMH_PACK_TAIL / 100; // add more 5-10%
#endif
        _num_buckets = num_buckets;

#if EMH_SMALL_SIZE
        if (num_buckets <= EMH_SMALL_SIZE && old_pairs != reinterpret_cast<PairT*>(_small))
            _pairs = reinterpret_cast<PairT*>(_small);
        else
#endif
            _pairs = reinterpret_cast<PairT*>(alloc_bucket(num_buckets));

        // Initialize all buckets: set every byte to INACTIVE so MSan sees them as
        // initialized.  For non-trivial types, the key/value fields are dead bytes
        // that will be overwritten by placement-new when the bucket is filled.
        memset(reinterpret_cast<char*>(_pairs), static_cast<int>(INACTIVE),
               sizeof(_pairs[0]) * static_cast<size_t>(num_buckets));

        // Initialize tail sentinels (bucket=0 so iterator stops)
        if (need_explicit_dtor()) {
            // Only init the bucket field; key/value of sentinels are never read.
            const size_type zero_bucket = 0;
            for (size_type i = 0; i < 2; ++i)
                std::memcpy(&EMH_BUCKET(_pairs, num_buckets + i), &zero_bucket, sizeof(zero_bucket));
        } else {
            memset(reinterpret_cast<char*>(_pairs + num_buckets), 0, sizeof(PairT) * 2u);
        }

#if EMH_FIND_HIT
        if constexpr (std::is_integral<KeyT>::value)
            reset_bucket(hash_main(0));
#endif

        for (size_type src_bucket = old_buckets - 1; _num_filled < old_num_filled; src_bucket--) {
            if (EMH_EMPTY(old_pairs, src_bucket))
                continue;
#if EMH_REHASH_LOG
            else if (src_bucket != EMH_BUCKET(old_pairs, src_bucket))
                collision++;
#endif

            const auto& key = EMH_KEY(old_pairs, src_bucket);
            const auto bucket = find_unique_bucket(key);
            new (_pairs + bucket) PairT(std::move(old_pairs[src_bucket]));
            _num_filled++;
            EMH_BUCKET(_pairs, bucket) = bucket;
            if (need_explicit_dtor())
                old_pairs[src_bucket].~PairT();

            if (src_bucket == 0)
                break;
        }

#if EMH_REHASH_LOG
        if (_num_filled > EMH_REHASH_LOG) {
            auto mbucket = _num_filled - collision;
            char buff[255] = {0};
            snprintf(buff, sizeof(buff),
                     "    _num_filled/aver_size/K.V/pack/collision|last = %u/%.2lf/%s.%s/%zd|%.2lf%%,%.2lf%%",
                     _num_filled, static_cast<double>(_num_filled) / mbucket, typeid(KeyT).name(),
                     typeid(ValueT).name(), sizeof(_pairs[0]), collision * 100.0 / _num_filled, last * 100.0 / omask);
#ifdef EMH_LOG
            static uint32_t ihashs = 0;
            EMH_LOG() << "hash_nums = " << ihashs++ << "|" << __FUNCTION__ << "|" << buff << std::endl;
#else
            puts(buff);
#endif
        }
#endif
#if EMH_SMALL_SIZE
        if (old_pairs != reinterpret_cast<PairT*>(_small))
#endif
            dealloc_bucket(old_pairs, old_buckets);
        assert(old_num_filled == _num_filled);
    }

private:
    PairT* alloc_bucket(size_type num_buckets) noexcept {
        return PairAllocTraits::allocate(_alloc, 2 + static_cast<size_t>(num_buckets));
    }

    void dealloc_bucket(PairT* pairs, size_type num_buckets) noexcept {
        if (pairs)
            PairAllocTraits::deallocate(_alloc, pairs, 2 + static_cast<size_t>(num_buckets));
    }

#if EMH_HIGH_LOAD
    void set_empty() {
        auto prev = 0;
        for (int32_t bucket = 1; bucket < _num_buckets; ++bucket) {
            if (EMH_EMPTY(_pairs, bucket)) {
                if (prev != 0) {
                    emh_prevet_set(_pairs, bucket, prev);
                    EMH_BUCKET(_pairs, prev) = -bucket;
                } else
                    _ehead = bucket;
                prev = bucket;
            }
        }

        if (prev == 0) {
            _ehead = 0;
            return;
        } // no empty bucket
        emh_prevet_set(_pairs, _ehead, prev);
        EMH_BUCKET(_pairs, prev) = 0 - _ehead;
        _ehead = 0 - EMH_BUCKET(_pairs, _ehead);
    }

    void clear_empty() {
        auto prev = emh_prevet_get(_pairs, _ehead);
        while (prev != _ehead) {
            EMH_BUCKET(_pairs, prev) = INACTIVE;
            prev = emh_prevet_get(_pairs, prev);
        }
        EMH_BUCKET(_pairs, _ehead) = INACTIVE;
        _ehead = 0;
    }

    // prev-ehead->next
    size_type pop_empty(const size_type bucket) {
        const auto prev_bucket = emh_prevet_get(_pairs, bucket);
        const auto next_bucket = static_cast<size_type>(0 - EMH_BUCKET(_pairs, bucket));
        assert(next_bucket > 0 && _ehead > 0);
        assert(next_bucket <= _mask && prev_bucket <= _mask);

        emh_prevet_set(_pairs, next_bucket, prev_bucket);
        EMH_BUCKET(_pairs, prev_bucket) = -next_bucket;

        _ehead = next_bucket;
        return bucket;
    }

    // ehead->bucket->next
    void push_empty(const size_type bucket) {
        const int next_bucket = 0 - EMH_BUCKET(_pairs, _ehead);
        assert(next_bucket > 0);

        emh_prevet_set(_pairs, bucket, _ehead);
        EMH_BUCKET(_pairs, bucket) = -next_bucket;

        emh_prevet_set(_pairs, next_bucket, bucket);
        EMH_BUCKET(_pairs, _ehead) = -bucket;
        //        _ehead = bucket;
    }
#endif

    // Can we fit another element?
    inline bool check_expand_need() noexcept { return reserve(static_cast<uint64_t>(_num_filled)); }

    void clear_bucket(size_type bucket, bool bclear = true) noexcept {
        if (need_explicit_dtor()) {
            // EMH_BUCKET(_pairs, bucket) = INACTIVE; //loop call in destructor
            _pairs[bucket].~PairT();
        }
        EMH_BUCKET(_pairs, bucket) = INACTIVE; // the status is reset by destructor by some compiler
        _num_filled--;
        (void)bclear;
#if EMH_HIGH_LOAD
        if (_ehead && bclear) {
            if (10 * _num_filled < 8 * _num_buckets)
                clear_empty();
            else if (bucket)
                push_empty(bucket);
        }
#endif
#if EMH_FIND_HIT
        reset_bucket(bucket);
#endif
    }

    template <typename K = KeyT> size_type erase_key(const K& key) {
        const auto bucket = key_to_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (EMH_UNLIKELY(static_cast<int>(next_bucket) < 0))
            return INACTIVE;

        const auto equalk = _eq(key, EMH_KEY(_pairs, bucket));
        if (next_bucket == bucket)
            return equalk ? bucket : INACTIVE;
        else if (equalk) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            EMH_PKV(_pairs, bucket) = std::move(EMH_PKV(_pairs, next_bucket));
            EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        }

        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
#ifndef EMH_RNEXT
                EMH_BUCKET(_pairs, prev_bucket) = (nbucket == next_bucket) ? prev_bucket : nbucket;
                return next_bucket;
#else
                if (nbucket == next_bucket) {
                    EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
                    return next_bucket;
                }

                const auto last = EMH_BUCKET(_pairs, nbucket);
                EMH_PKV(_pairs, next_bucket) = std::move(EMH_PKV(_pairs, nbucket));
                EMH_BUCKET(_pairs, next_bucket) = (nbucket == last) ? next_bucket : last;
                return nbucket;
#endif
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
        if (EMH_LIKELY(next_bucket == bucket)) {
            const auto main_bucket = hash_main(bucket);
            if (main_bucket != bucket) {
                const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
                EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
            }
        } else {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            EMH_PKV(_pairs, bucket) = std::move(EMH_PKV(_pairs, next_bucket));
            EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
        }

        return next_bucket;
    }

    template <typename K = KeyT> size_type find_filled_key(const K& key) const noexcept {
        const auto main_bucket = key_to_bucket(key);
#if EMH_FIND_HIT
        if constexpr (std::is_integral<KeyT>::value) {
            auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
            if (_eq(key, EMH_KEY(_pairs, main_bucket)))
                return main_bucket;
            if (static_cast<int>(next_bucket) < 0)
                return _num_buckets;
        }
#endif
        return find_hash_bucket(key, main_bucket);
    }

    // Traverse the collision chain starting at the main bucket to find key.
    // Each bucket stores the next link in its slot field; a self-loop marks
    // the chain tail. Returns _num_buckets if the key is not found.
    template <typename K = KeyT> size_type find_hash_bucket(const K& key, size_type bucket) const noexcept {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);

        if (static_cast<int>(next_bucket) < 0)
            return _num_buckets;
        else if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;

        if (next_bucket == bucket)
            return _num_buckets;

        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            if (nbucket == next_bucket)
                return _num_buckets;

#if !defined(_MSC_VER) && defined(__GNUC__)
            __builtin_prefetch(&_pairs[nbucket], 0, 1);
#endif
            next_bucket = nbucket;
        }
    }

    // Relocate a "guest" bucket (one that doesn't sit at its own main position)
    // to make room for the key whose main position it occupies. This is the
    // "kickout" step adapted from Lua's table design: it guarantees the chain
    // head always lives at its hash-computed main bucket, so lookups hit on
    // the first probe.
    //
    // before: kmain --> ... --> prev --> kbucket --> next
    // after : kmain --> ... --> prev --> new_bucket --> next , kbucket freed
    size_type kickout_bucket(const size_type kmain, const size_type kbucket) noexcept {
        const auto next_bucket = EMH_BUCKET(_pairs, kbucket);
        const auto new_bucket = find_empty_bucket(next_bucket, 2);
        const auto prev_bucket = find_prev_bucket(kmain, kbucket);
        EMH_BUCKET(_pairs, prev_bucket) = new_bucket;
        new (_pairs + new_bucket) PairT(std::move(_pairs[kbucket]));
        if (new_bucket < _first)
            _first = new_bucket;
        if (next_bucket == kbucket)
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;

        clear_bucket(kbucket, false);
        _num_filled++;
        return kbucket;
    }

    // Check if the main bucket is empty (ready for insertion) or occupied by
    // a guest that should be kicked out. Returns the bucket if ready, INACTIVE
    // if the caller must walk the chain to find/append.
    template <typename K = KeyT> size_type find_or_kickout(const K& key, size_type bucket) noexcept {
        (void)key;
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (static_cast<int>(next_bucket) < 0) {
#if EMH_HIGH_LOAD
            if (next_bucket != INACTIVE)
                pop_empty(bucket);
#endif
            return bucket;
        }

        const auto kmain = hash_main(bucket);
        if (EMH_UNLIKELY(kmain != bucket))
            return kickout_bucket(kmain, bucket);

        return INACTIVE;
    }

    // Core insert/lookup dispatcher using separate chaining with kickout:
    //  1. If the main bucket is empty, use it directly.
    //  2. If the main bucket holds a guest (key whose hash maps elsewhere),
    //     kick the guest to an empty slot and claim the main bucket.
    //  3. Otherwise walk the collision chain: return the bucket if the key
    //     matches, or append a new empty slot at the chain tail if not found.
    template <typename K = KeyT> size_type find_or_allocate(const K& key) noexcept {
        const auto bucket = key_to_bucket(key);
        auto result = find_or_kickout(key, bucket);
        if (result != INACTIVE)
            return result;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        if (_eq(key, bucket_key))
            return bucket;
        else if (next_bucket == bucket)
            return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket, 1);

        uint32_t csize = 0;
#if EMH_LRU_SET
        auto prev_bucket = bucket;
#endif
        // find next linked bucket and check key
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
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

            csize += 1;
            if (nbucket == next_bucket)
                break;

#if !defined(_MSC_VER) && defined(__GNUC__)
            __builtin_prefetch(&_pairs[nbucket], 0, 1);
#endif
            next_bucket = nbucket;
        }

        // find a new empty and link it to tail
        const auto new_bucket = find_empty_bucket(next_bucket, csize);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    template <typename K = KeyT> size_type find_unique_bucket(const K& key) noexcept {
        const auto bucket = key_to_bucket(key);
        auto result = find_or_kickout(key, bucket);
        if (result != INACTIVE)
            return result;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (EMH_UNLIKELY(next_bucket != bucket))
            next_bucket = find_last_bucket(next_bucket);

        // find a new empty and link it to tail
        return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket, 2);
    }

    size_type move_unique_bucket(size_type old_bucket, size_type bucket) noexcept {
        (void)old_bucket;
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (static_cast<int>(next_bucket) < 0)
            return bucket;

        next_bucket = find_last_bucket(next_bucket);

        // find a new empty and link it to tail
        return EMH_BUCKET(_pairs, next_bucket) = find_unique_empty(next_bucket);
    }

    /// 3-way empty-slot search for collision resolution.
    ///
    /// Linear probing alone degrades badly at high load factors. This function
    /// uses a 3-way strategy that stays fast even above 90% load:
    ///   1. **Local bidirectional probing** — search ±N slots around the
    ///      collision point. Keeps chain members in the same cache line as
    ///      their head, minimizing cache misses during chain walks.
    ///   2. **Roaming cursor (`_last`)** — a monotonically advancing cursor
    ///      that scans the table from where the last empty slot was found,
    ///      amortizing probe length across insertions.
    ///   3. **Mid-point cursor** — a second probe at `(_mask/4 + _last)`,
    ///      providing a pseudo-random jump to escape local clustering.
    EMH_INLINE size_type find_empty_bucket(const size_type bucket_from, uint32_t csize) noexcept {
#if EMH_HIGH_LOAD
        if (_ehead)
            return pop_empty(_ehead);
#endif

        (void)csize;
        auto bucket = bucket_from;
        if (EMH_EMPTY(_pairs, ++bucket) || EMH_EMPTY(_pairs, ++bucket))
            return bucket;

        // Bidirectional local probing: search outward in both directions.
        // This keeps kicked-out chain members close to their chain head,
        // dramatically reducing cache misses during chain traversal.
        constexpr size_type max_local_probes = 10; // ±8 from bucket_from after +1/+2
        for (size_type step = 2; step < max_local_probes; step++) {
            auto forward = (bucket_from + step + 1) & _mask;
            if (EMH_EMPTY(_pairs, forward))
                return forward;

            auto backward = (bucket_from - (step - 1)) & _mask;
            if (EMH_EMPTY(_pairs, backward))
                return backward;
        }

        // Fallback: global roaming when local area is saturated
        while (true) {
#if EMH_PACK_TAIL
            _last &= _mask;
            auto slot = _last++;
            if (EMH_EMPTY(_pairs, slot))
                return slot;

            if (EMH_UNLIKELY(_last >= _num_buckets))
                _last = 0;

            auto medium = (_mask / 4 + _last) & _mask;
            if (EMH_EMPTY(_pairs, medium))
                return medium;
#else
            if (EMH_EMPTY(_pairs, ++_last))
                return _last++;
            _last &= _mask;
            auto medium = (_num_buckets / 2 + _last) & _mask;
            if (EMH_EMPTY(_pairs, medium)) // && EMH_EMPTY(_pairs, ++medium))
                return _last = medium;
#endif
        }
    }

    size_type find_unique_empty(const size_type bucket_from) noexcept {
        auto bucket = bucket_from;
        if (EMH_EMPTY(_pairs, ++bucket) || EMH_EMPTY(_pairs, ++bucket))
            return bucket;

        for (size_type slot = bucket + 2, step = 2;; slot += ++step) {
            auto nbucket = slot & _mask;
            if (EMH_EMPTY(_pairs, nbucket) || EMH_EMPTY(_pairs, ++nbucket))
                return nbucket;
        }
    }

    size_type find_last_bucket(size_type main_bucket) const noexcept {
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

    size_type find_prev_bucket(const size_type main_bucket, const size_type bucket) const noexcept {
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

    template <typename K = KeyT> EMH_INLINE size_type key_to_bucket(const K& key) const noexcept {
        return static_cast<size_type>(hash_key(key)) & _mask;
    }

    EMH_INLINE size_type hash_main(const size_type bucket) const noexcept {
        return static_cast<size_type>(hash_key(EMH_KEY(_pairs, bucket))) & _mask;
    }

#if EMH_INT_HASH
    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    static uint64_t hash64(uint64_t key) {
#if __SIZEOF_INT128__ && EMH_INT_HASH == 1
        __uint128_t r = key;
        r *= KC;
        return static_cast<uint64_t>(r >> 64) + static_cast<uint64_t>(r);
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
        return _umul128(key, KC, &high) + high;
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
        return wyhash64(key, KC);
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
        return static_cast<size_type>(wyhash(key.data(), key.size(), 0));
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

private:
    PairT* _pairs;
#if EMH_SMALL_SIZE
    char _small[(EMH_SMALL_SIZE + 2) * sizeof(PairT)];
#endif

    HashT _hasher;
    EqT _eq;
    PairAlloc _alloc;
    uint32_t _mlf;
    size_type _mask;
    size_type _num_buckets;
    size_type _num_filled;
    size_type _last;
    size_type _first;
#if EMH_HIGH_LOAD
    size_type _ehead;
#endif
};
} // namespace emhash5
