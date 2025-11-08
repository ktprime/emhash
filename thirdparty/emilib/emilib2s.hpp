// LICENSE:
// version 1.0.3
// https://github.com/ktprime/emhash/blob/master/thirdparty/emilib/emiset2s.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2025 Huang Yuanbing & bailuzhou AT 163.com
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

#pragma once

#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>
#include <cassert>

#ifdef _WIN32
    #include <intrin.h>
#elif __x86_64__
    #include <x86intrin.h>
#else
    #include "sse2neon.h"
#endif

#undef EMH_LIKELY
#undef EMH_UNLIKELY

// likely/unlikely
#if defined(__GNUC__) && (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1) || defined(__clang__)
    #define EMH_LIKELY(condition)   __builtin_expect(!!(condition), 1)
    #define EMH_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1920)
    #define EMH_LIKELY(condition)   ((condition) ? ((void)__assume(condition), 1) : 0)
    #define EMH_UNLIKELY(condition) ((condition) ? 1 : ((void)__assume(!condition), 0))
#else
    #define EMH_LIKELY(condition)   (condition)
    #define EMH_UNLIKELY(condition) (condition)
#endif

namespace emilib3 {

    enum State : int8_t
    {
        EEMPTY = -128,
        EDELETE = EEMPTY + 1,
        EFILLED = EDELETE + 1,
        SENTINEL= 127,
        GROUP_INDEX = 15,//> 0
    };

#ifndef EMH_DEFAULT_LOAD_FACTOR
    constexpr static float EMH_DEFAULT_LOAD_FACTOR = 0.80f;
#endif
    constexpr static float EMH_MAX_LOAD_FACTOR = 0.999f;
    constexpr static float EMH_MIN_LOAD_FACTOR = 0.25f;

#ifndef AVX2_EHASH
    const static auto simd_empty  = _mm_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm_set1_epi8(EDELETE);
    const static auto simd_filled = _mm_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm_set1_epi8
    #define SET1_EPI32     _mm_set1_epi32
    #define LOAD_EPI8      _mm_load_si128
    #define MOVEMASK_EPI8  _mm_movemask_epi8
    #define CMPEQ_EPI8     _mm_cmpeq_epi8
    #define CMPGT_EPI8     _mm_cmpgt_epi8
#elif 1
    const static auto simd_empty  = _mm256_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm256_set1_epi8(EDELETE);
    const static auto simd_filled = _mm256_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm256_set1_epi8
    #define LOAD_EPI8      _mm256_loadu_si256
    #define MOVEMASK_EPI8  _mm256_movemask_epi8
    #define CMPEQ_EPI8     _mm256_cmpeq_epi8
    #define CMPGT_EPI8     _mm256_cmpgt_epi8
#elif AVX512_EHASH
    const static auto simd_empty  = _mm512_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm512_set1_epi8(EDELETE);
    const static auto simd_filled = _mm512_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm512_set1_epi8
    #define LOAD_EPI8      _mm512_loadu_si512
    #define MOVEMASK_EPI8  _mm512_movemask_epi8 //avx512 error
    #define CMPEQ_EPI8     _mm512_test_epi8_mask
#else
    //TODO sse2neon
#endif

//find filled or empty
constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);

inline static uint32_t CTZ(uint32_t n)
{
#if _WIN32
    unsigned long index;
    _BitScanForward(&index, n);
#else
    auto index = __builtin_ctzl((unsigned long)n);
#endif

    return (uint32_t)index;
}

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
private:
    using htype = HashMap<KeyT, ValueT, HashT, EqT>;

    using PairT = std::pair<const KeyT, ValueT>;

public:
    using size_t          = uint32_t;
    using value_type      = PairT;
    using reference       = PairT&;
    using const_reference = const PairT&;

    using mapped_type     = ValueT;
    using val_type        = ValueT;
    using key_type        = KeyT;
    using hasher          = HashT;
    using key_equal       = EqT;

    static constexpr uint8_t hash_253_map[] = {
    0x3F, 0xD6, 0x61, 0x0C, 0xA8, 0x23, 0x97, 0x47, 0x02, 0xC5,
    0x6D, 0xE6, 0x30, 0xB5, 0x12, 0x7A, 0xCE, 0x27, 0xA9, 0x4A,
    0xF7, 0x69, 0xDF, 0x40, 0x96, 0x0A, 0xBE, 0x1B, 0xAD, 0x6E,
    0xD5, 0x39, 0x9D, 0x10, 0x58, 0xEC, 0x4E, 0xFD, 0x1F, 0x7E,
    0x92, 0x33, 0xB2, 0x18, 0xA1, 0x6C, 0xE5, 0x42, 0x9C, 0x2A,
    0xBC, 0x61, 0xF4, 0x53, 0xCB, 0x0E, 0xB0, 0x72, 0xDD, 0x36,
    0x9F, 0x4F, 0x00, 0x90, 0x21, 0xFA, 0x45, 0xA3, 0x13, 0xDB,
    0x5C, 0xC1, 0x66, 0xE6, 0x31, 0xB4, 0x04, 0x78, 0xCF, 0x28,
    0xA8, 0x4B, 0xF8, 0x6A, 0xE0, 0x40, 0x97, 0x0B, 0xBD, 0x1C,
    0xA4, 0x6F, 0xD6, 0x3A, 0x9E, 0x11, 0x59, 0xED, 0x4F, 0xFE,
    0x20, 0x7C, 0x91, 0x34, 0xB1, 0x19, 0xA2, 0x6D, 0xE3, 0x3F,
    0x96, 0x2B, 0xBF, 0x62, 0xF5, 0x54, 0xCA, 0x0F, 0xAC, 0x73,
    0xDE, 0x7F, 0x3B, 0xD3, 0x56, 0xE1, 0x48, 0xFB, 0x1D, 0x76,
    0xC0, 0x3D, 0x9C, 0x15, 0x5D, 0xE8, 0x37, 0xB8, 0xF6, 0x67,
    0xEF, 0x4C, 0xC2, 0x0A, 0xA7, 0x2E, 0x95, 0x17, 0xD8, 0x05,
    0x64, 0xE9, 0x09, 0xBE, 0x1E, 0xAF, 0x70, 0xDC, 0x35, 0x9E,
    0x50, 0x03, 0x8F, 0x20, 0xF9, 0x44, 0x9A, 0x12, 0xDC, 0x5B,
    0xC2, 0x65, 0xE9, 0x30, 0xB5, 0x03, 0x77, 0xCE, 0x27, 0xA9,
    0x4A, 0xF7, 0x69, 0xDF, 0x40, 0x96, 0x0A, 0xBE, 0x1B, 0xAD,
    0x6E, 0xD5, 0x39, 0x9D, 0x10, 0x58, 0xEC, 0x4E, 0xFD, 0x1F,
    0x7E, 0x92, 0x33, 0xB2, 0x18, 0xA1, 0x6C, 0xE5, 0x42, 0x9C,
    0x2A, 0xBC, 0x60, 0xF4, 0x52, 0xCB, 0x0D, 0xB0, 0x71, 0xDD,
    0x37, 0x9F, 0x4E, 0xFF, 0x90, 0x22, 0xF9, 0x45, 0xA3, 0x12,
    0xDB, 0x5D, 0xC0, 0x67, 0xEB, 0x2F, 0xB4, 0x05, 0x79, 0xC0,
    0x29, 0xA9, 0x4C, 0xF7, 0x6B, 0xE0, 0x41, 0x98, 0x0C, 0xBE,
    0x1D, 0xA7, 0x70, 0xD4, 0x3A, 0x7F
    };

    static_assert(sizeof(hash_253_map) == 256, "map error");

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value, int8_t>::type = 0>
    inline int8_t hash_key2(size_t& main_bucket, const UType& key) const
    {
        const auto key_hash = _hasher(key);
        main_bucket = size_t(key_hash & _mask);
        main_bucket -= main_bucket % simd_bytes;
        return (int8_t)((size_t)(key_hash % 253) + (size_t)EFILLED);
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, int8_t>::type = 0>
    inline int8_t hash_key2(size_t& main_bucket, const UType& key) const
    {
        const auto key_hash = _hasher(key);

#if __SIZEOF_INT128__ && 0
        __uint128_t r = key_hash;
        r *= UINT64_C(0x9E3779B97F4A7C15);
        key_hash = static_cast<uint64_t>(r) ^ static_cast<uint64_t>(r >> 64U);
#endif
        main_bucket = size_t(key_hash & _mask);
        main_bucket -= main_bucket % simd_bytes;
        //return hash_253_map[(uint8_t)key_hash];//(int8_t)(key_hash % 253) + EFILLED;
        return (int8_t)((size_t)(key_hash % 253) + (size_t)EFILLED);
    }

    class const_iterator;
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = PairT;
        using pointer           = value_type*;
        using reference         = value_type&;

        iterator() {}
       // iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { _bmask = _from = (size_t)-1; }

        void init()
        {
            _from = (_bucket / simd_bytes) * simd_bytes;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= (size_t) ~((1ull << (_bucket % simd_bytes)) - 1);
            } else {
                _bmask = 0;
            }
        }

        size_t operator - (const iterator& r) const
        {
            return _bucket - r._bucket;
        }

        size_t bucket() const
        {
            return _bucket;
        }

        iterator& operator++()
        {
#ifndef EMH_ITER_SAFE
            if (_from == (size_t)-1) init();
#endif
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
#ifndef EMH_ITER_SAFE
            if (_from == (size_t)-1) init();
#endif
            iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->_pairs[_bucket]; }
        pointer operator->() const { return _map->_pairs + _bucket; }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

    private:
        void goto_next_element()
        {
            _bmask &= _bmask - 1;
            if (_bmask) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = _map->filled_mask(_from += simd_bytes);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype*  _map;
        size_t        _bmask;
        size_t        _bucket;
        size_t        _from;
    };

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = const PairT;
        using pointer           = value_type*;
        using reference         = value_type&;

        explicit const_iterator(const iterator& it)
            : _map(it._map), _bucket(it._bucket), _bmask(it._bmask), _from(it._from) {}
        const_iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }

        void init()
        {
            _from = (_bucket / simd_bytes) * simd_bytes;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= (size_t) ~((1ull << (_bucket % simd_bytes)) - 1);
            } else {
                _bmask = 0;
            }
        }

        size_t bucket() const
        {
            return _bucket;
        }

        size_t operator - (const const_iterator& r) const
        {
            return _bucket - r._bucket;
        }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            const_iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->_pairs[_bucket]; }
        pointer operator->() const { return _map->_pairs + _bucket; }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

    private:
        void goto_next_element()
        {
            _bmask &= _bmask - 1;
            if (_bmask) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = _map->filled_mask(_from += simd_bytes);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype*  _map;
        size_t        _bmask;
        size_t        _bucket;
        size_t        _from;
    };

    // ------------------------------------------------------------------------

    HashMap(size_t n = 4, float lf = EMH_DEFAULT_LOAD_FACTOR) noexcept
    {
        _mlf = (uint32_t)((1 << 28) / lf);
        rehash(n);
    }

    HashMap(const HashMap& other) noexcept
    {
        clone(other);
    }

    HashMap(HashMap&& other) noexcept
    {
        rehash(1);
        if (this != &other) {
            swap(other);
        }
    }

    HashMap(std::initializer_list<value_type> il) noexcept
    {
        rehash((size_t)il.size());
        for (auto it = il.begin(); it != il.end(); ++it)
            insert(*it);
    }

    template<class InputIt>
    HashMap(InputIt first, InputIt last, size_t bucket_count = 4) noexcept
    {
        rehash((size_t)std::distance(first, last) + bucket_count);
        for (; first != last; ++first)
            insert(*first);
    }

    HashMap& operator=(const HashMap& other) noexcept
    {
        if (this != &other)
            clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept
    {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap() noexcept
    {
        clear_data();
        _num_filled = 0;
        free(_states);
        free(_pairs);
    }

    void clone(const HashMap& other) noexcept
    {
        if (other.size() == 0) {
            clear();
            return;
        }

        clear_data();

        if (other._num_buckets != _num_buckets) {
            _num_filled = _num_buckets = 0;
            rehash(other._num_buckets);
        }

        if (is_trivially_copyable()) {
            memcpy((char*)_pairs, other._pairs, _num_buckets * sizeof(_pairs[0]));
        } else {
            for (auto it = other.cbegin(); it.bucket() != _num_buckets; ++it)
                new(_pairs + it.bucket()) PairT(*it);
        }

        _num_filled = other._num_filled;
        _max_probe_length = other._max_probe_length;
        _mlf              = other._mlf;
        const auto state_size = (simd_bytes + _num_buckets) * sizeof(_states[0]);
        memcpy(_states, other._states, state_size);
    }

    void swap(HashMap& other) noexcept
    {
        std::swap(_hasher,           other._hasher);
        std::swap(_eq,               other._eq);
        std::swap(_states,           other._states);
        std::swap(_pairs,            other._pairs);
        std::swap(_num_buckets,      other._num_buckets);
        std::swap(_num_filled,       other._num_filled);
        std::swap(_max_probe_length, other._max_probe_length);
        std::swap(_mask,             other._mask);
        std::swap(_mlf,              other._mlf);
    }

    // -------------------------------------------------------------

    iterator begin() noexcept
    {
        return {this, find_filled_slot(0)};
    }

    const_iterator cbegin() const noexcept
    {
        return {this, find_filled_slot(0)};
    }

    const_iterator begin() const noexcept
    {
        return cbegin();
    }

    iterator end() noexcept
    {
        return {this, _num_buckets};
    }

    const_iterator cend() const noexcept
    {
        return {this, _num_buckets};
    }

    const_iterator end() const noexcept
    {
        return cend();
    }

    size_t size() const noexcept
    {
        return _num_filled;
    }

    bool empty() const noexcept
    {
        return _num_filled == 0;
    }

    // Returns the number of buckets.
    size_t bucket_count() const noexcept
    {
        return _num_buckets;
    }

    /// Returns average number of elements per bucket.
    float load_factor() const noexcept
    {
        return float(_num_filled) / float(_num_buckets);
    }

    inline constexpr float max_load_factor() const { return EMH_MAX_LOAD_FACTOR; }
    inline constexpr float min_load_factor() const { return EMH_MIN_LOAD_FACTOR; }
    inline constexpr void max_load_factor(float mlf) noexcept
    {
        if (mlf <= max_load_factor() && mlf > min_load_factor())
            _mlf = (uint32_t)((1 << 28) / mlf);
    }

    constexpr uint64_t max_size() const { return 1ull << (sizeof(_num_buckets) * 8 - 1); }
    constexpr uint64_t max_bucket_count() const { return max_size(); }

    // ------------------------------------------------------------

    template<typename K=KeyT>
    iterator find(const K& key) noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    template<typename K=KeyT>
    const_iterator find(const K& key) const noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    template<typename K=KeyT>
    bool contains(const K& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    template<typename K=KeyT>
    size_t count(const K& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    template<typename K=KeyT>
    ValueT& at(const K& key) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return _pairs[bucket].second;
    }

    template<typename K=KeyT>
    const ValueT& at(const K& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return _pairs[bucket].second;
    }

    template<typename K>
    ValueT* try_get(const K& key) noexcept
    {
        auto bucket = find_filled_bucket(key);
        return &_pairs[bucket].second;
    }

    template<typename K>
    ValueT* try_get(const K& key) const noexcept
    {
        auto bucket = find_filled_bucket(key);
        return &_pairs[bucket].second;
    }

    template<typename Con>
    bool operator == (const Con& rhs) const noexcept
    {
        if (size() != rhs.size())
            return false;

        for (auto it = begin(), last = end(); it != last; ++it) {
            auto oi = rhs.find(it->first);
            if (oi == rhs.end() || it->second != oi->second)
                return false;
        }
        return true;
    }

    template<typename Con>
    bool operator != (const Con& rhs) const noexcept { return !(*this == rhs); }

    void merge(HashMap& rhs) noexcept
    {
        if (empty()) {
            *this = std::move(rhs);
            return;
        }

        for (auto rit = rhs.begin(); rit != rhs.end(); ) {
            auto fit = find(rit->first);
            if (fit.bucket() > _mask) {
                insert_unique(rit->first, std::move(rit->second));
                rhs.erase(rit++);
            } else {
                ++rit;
            }
        }
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    template<typename K, typename V>
    std::pair<iterator, bool> do_insert(K&& key, V&& val) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);

        if (bempty) {
            new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        }
        return { {this, bucket}, bempty };
    }

    std::pair<iterator, bool> do_insert(const value_type& value) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        if (bempty) {
            new(_pairs + bucket) PairT(value); _num_filled++;
        }
        return { {this, bucket}, bempty };
    }

    std::pair<iterator, bool> do_insert(value_type&& value) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        if (bempty) {
            new(_pairs + bucket) PairT(std::move(value)); _num_filled++;
        }
        return { {this, bucket}, bempty };
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args) noexcept
    {
        return do_insert(std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert(value_type&& value) noexcept
    {
        return do_insert(std::move(value));
    }

    std::pair<iterator, bool> insert(const value_type& value) noexcept
    {
        return do_insert(value);
    }

#if 0
    iterator insert(iterator hint, const value_type& value) noexcept
    {
        (void)hint;
        return do_insert(value).first;
    }
#endif

    template <typename Iter>
    void insert(Iter beginc, Iter endc) noexcept
    {
        rehash(size_t(endc - beginc) + _num_filled);
        for (; beginc != endc; ++beginc)
            do_insert(beginc->first, beginc->second);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) noexcept
    {
        return do_insert(key, std::forward<Args>(args)...);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) noexcept
    {
        return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    void insert(std::initializer_list<value_type> ilist) noexcept
    {
        rehash(size_t(ilist.size()) + _num_filled);
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            do_insert(*it);
    }

    template<typename K, typename V>
    size_t insert_unique(K&& key, V&& val) noexcept
    {
        const auto required_buckets = ((uint64_t)_num_filled * _mlf >> 28);
        if (EMH_UNLIKELY(required_buckets >= _num_buckets))
            rehash(required_buckets + 2);

        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        prefetch_heap_block((char*)&_pairs[main_bucket]);
        const auto bucket = find_empty_slot(main_bucket, 0);

        set_states(bucket, key_h2);
        new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        return bucket;
    }

    template<typename K, typename V>
    size_t insert_unique2(K&& key, V&& val) noexcept
    {
        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        const auto bucket = find_empty_slot(main_bucket, 0);

        set_states(bucket, key_h2);
        new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        return bucket;
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const KeyT& key, M&& val) noexcept
    {
        return do_assign(key, std::forward<M>(val));
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(KeyT&& key, M&& val) noexcept
    {
        return do_assign(std::move(key), std::forward<M>(val));
    }

    template<typename K, typename V>
    std::pair<iterator, bool> do_assign(K&& key, V&& val) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);

        // Check if inserting a new val rather than overwriting an old entry
        if (bempty) {
            new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        } else {
            _pairs[bucket].second = std::forward<V>(val);
        }

        return { {this, bucket}, bempty };
    }

    bool set_get(const KeyT& key, const ValueT& val, ValueT& oldv) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bempty) {
            new(_pairs + bucket) PairT(key,val); _num_filled++;
        } else {
            oldv = _pairs[bucket].second;
        }
        return bempty;
    }

    ValueT& operator[](const KeyT& key) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bempty) {
            new(_pairs + bucket) PairT(key, std::move(ValueT())); _num_filled++;
        }

        return _pairs[bucket].second;
    }

    ValueT& operator[](KeyT&& key) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        if (bempty) {
            new(_pairs + bucket) PairT(std::move(key), std::move(ValueT())); _num_filled++;
        }

        return _pairs[bucket].second;
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    size_t erase(const KeyT& key) noexcept
    {
        auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            return 0;

        _erase(bucket);
        return 1;
    }

    void erase(const const_iterator& cit) noexcept
    {
        _erase(cit._bucket);
    }

    void erase(iterator it) noexcept
    {
        _erase(it._bucket);
    }

    void _erase(size_t bucket) noexcept
    {
        _num_filled -= 1;
        if (!is_trivially_destructible())
            _pairs[bucket].~PairT();
#if 1
        const auto gbucket = bucket / simd_bytes * simd_bytes;
        _states[bucket] = group_mask(gbucket) == State::EEMPTY ? State::EEMPTY : State::EDELETE;
#else
        _states[bucket] = State::EDELETE;
        if (EMH_UNLIKELY(_num_filled == 0))
            clear_meta();
#endif
    }

    iterator erase(const_iterator first, const_iterator last) noexcept
    {
        auto iend = cend();
        auto next = first;
        for (; next != last && next != iend; )
            erase(next++);

        return {this, next.bucket()};
    }

    template<typename Pred>
    size_t erase_if(Pred pred) noexcept
    {
        auto old_size = size();
        for (auto it = begin(), last = end(); it != last; ) {
            if (pred(*it))
                erase(it);
            ++it;
        }
        return old_size - size();
    }

    static constexpr bool is_trivially_destructible()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return (std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    static constexpr bool is_trivially_copyable()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clear_meta() noexcept
    {
        //init empty tombstone
        std::fill_n(_states, _num_buckets, State::EEMPTY);
        //set filled tombstone
        std::fill_n(_states + _num_buckets, simd_bytes, State::SENTINEL);
        _num_filled = 0;
        _max_probe_length = 0;
    }

    void clear_data() noexcept
    {
        if (!is_trivially_destructible() && _num_filled) {
            for (auto it = begin(); _num_filled; ++it) {
                const auto bucket = it.bucket();
                _pairs[bucket].~PairT();
                _num_filled -= 1;
            }
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear() noexcept
    {
        if (_num_filled) {
            clear_data();
            clear_meta();
        }
    }

    void shrink_to_fit() noexcept
    {
        rehash(_num_filled + 1);
    }

    bool reserve(size_t num_elems) noexcept
    {
        const auto required_buckets = ((uint64_t)num_elems * _mlf >> 28);
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    /// Make room for this many elements
    void rehash(uint64_t required_buckets) noexcept
    {
        if (required_buckets < _num_filled)
            return;

        uint64_t buckets = _num_filled > (1u << 16) ? (1u << 16) : simd_bytes;
        while (buckets < required_buckets) { buckets *= 2; }

        if (buckets > max_size() || buckets < _num_filled)
            std::abort(); //throw std::length_error("too large size");

        const auto num_buckets = (size_t)buckets;
        const auto pairs_size = (num_buckets + 1) * sizeof(PairT);
        const auto state_size = (simd_bytes + num_buckets) * sizeof(State);

        auto* new_state = (decltype(_states))malloc(state_size);
        auto* new_pairs = (decltype(_pairs)) malloc(pairs_size);

        auto old_num_filled  = _num_filled;
        auto old_states      = _states;
        auto old_pairs       = _pairs;
        auto old_buckets     = _num_buckets;
#if EMH_STATIS
        auto max_probe_length = _max_probe_length;
#endif

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _states      = new_state;
        _pairs       = new_pairs;

        //fill last packet zero
        memset((char*)(_pairs + num_buckets), 0, sizeof(_pairs[0]));
        clear_meta();

#if EMH_STATIS
        auto collision = 0;
#endif

        //for (size_t src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
        for (size_t src_bucket = old_buckets - 1; _num_filled < old_num_filled; --src_bucket) {
            if (old_states[src_bucket] >= State::EFILLED) {
                auto& src_pair = old_pairs[src_bucket];
                size_t main_bucket;
                const auto key_h2 = hash_key2(main_bucket, src_pair.first);
                const auto bucket = find_empty_slot(main_bucket, 0);

                set_states(bucket, key_h2);
                new(_pairs + bucket) PairT(std::move(src_pair));
                _num_filled ++;
                if (!is_trivially_destructible())
                    src_pair.~PairT();
            }
        }

#if EMH_STATIS
        if (_num_filled > 1000000)
            printf("\t\t\tmax_probe_length/_max_probe_length = %d/%d, collsions = %d, collision = %.2f%%\n",
                    max_probe_length, _max_probe_length, collision, collision * 100.0f / _num_buckets);
#endif

        free(old_states);
        free(old_pairs);
    }

private:
    // Can we fit another element?
    void check_expand_need() noexcept
    {
        reserve(_num_filled);
    }

    static void prefetch_heap_block(char* ctrl)
    {
        // Prefetch the heap-allocated memory region to resolve potential TLB
        // misses.  This is intended to overlap with execution of calculating the hash for a key.
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_prefetch((const char*)ctrl, _MM_HINT_T0);
#elif defined(_MSC_VER)
        _mm_prefetch((const char*)ctrl);
#elif defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(static_cast<const void*>(ctrl));
#endif
    }

    inline int8_t group_mask(size_t gbucket) const noexcept
    {
        return _states[gbucket + simd_bytes - 1];
    }

    void set_states(size_t ebucket, int8_t key_h2) noexcept
    {
        _states[ebucket] = key_h2;
    }

    inline void set_offset(size_t offset) noexcept
    {
        _max_probe_length = offset;
    }

    inline size_t get_next_bucket(size_t next_bucket, size_t offset) const noexcept
    {
#if EMH_PSL_LINEAR == 0
        if (offset < 7)// || _num_buckets < 32 * simd_bytes)
            next_bucket += simd_bytes * offset;
        else
            next_bucket += _num_buckets / 8 + simd_bytes;
#else
        next_bucket += 3 * simd_bytes;
        if (next_bucket >= _num_buckets)
            next_bucket += simd_bytes;
#endif
        return next_bucket & _mask;
    }

    // Find the bucket with this key, or return (size_t)-1
    template<typename K>
    size_t find_filled_bucket(const K& key) const noexcept
    {
        size_t main_bucket; size_t offset = 0;
        const auto key_h2 = hash_key2(main_bucket, key);
        const auto filled = SET1_EPI32(0x01010101u * (uint8_t)key_h2);
//        const auto filled = SET1_EPI8(hash_key2(main_bucket, key));
        auto next_bucket = main_bucket;

        do {
            const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf = (size_t)MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));
            if (maskf) {
                prefetch_heap_block((char*)&_pairs[next_bucket]);
                do {
                    const auto fbucket = next_bucket + CTZ(maskf);
                    if (EMH_LIKELY(_eq(_pairs[fbucket].first, key)))
                        return fbucket;
                } while (maskf &= maskf - 1);
            }

            if (group_mask(next_bucket) == State::EEMPTY)
                return _num_buckets;
            if (offset >= _max_probe_length)
                return _num_buckets;
            next_bucket = get_next_bucket(next_bucket, ++offset);
        } while (true);

        return _num_buckets;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the later case, the bucket is expected to be filled.
    template<typename K>
    size_t find_or_allocate(const K& key, bool& bnew) noexcept
    {
#if 0
        auto findit = find_filled_bucket(key);
        if (findit != _num_buckets) {
            bnew = false;
            return findit;
        }
#endif

        const auto required_buckets = ((uint64_t)_num_filled * _mlf >> 28);
        if (EMH_UNLIKELY(required_buckets >= _num_buckets))
          rehash(required_buckets + 2);

        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        prefetch_heap_block((char*)&_pairs[main_bucket]);
        const auto filled = SET1_EPI32(0x01010101u * (uint8_t)key_h2);
        auto next_bucket = main_bucket; size_t offset = 0u;
        constexpr size_t chole = (size_t)-1;
        size_t hole = chole;

        do {
            const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[next_bucket]));
#if 1
            auto maskf = (size_t)MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));
            //1. find filled
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(_pairs[fbucket].first, key)) {
                    bnew = false;
                    return fbucket;
                }
                maskf &= maskf - 1;
            }
#endif

            if (hole == chole) {
                //2. find empty
                const auto maskd = (size_t)MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec));
                if (group_mask(next_bucket) == State::EEMPTY) {
                    hole = next_bucket + CTZ(maskd);
                    set_states(hole, key_h2);
                    return hole;
                }
                else if (maskd != 0) {
                    hole = next_bucket + CTZ(maskd);
                }
            }

            //4. next round
            next_bucket = get_next_bucket(next_bucket, ++offset);

        } while (EMH_UNLIKELY(offset <= _max_probe_length));

        if (hole != chole) {
            set_states(hole, key_h2);
            return hole;
        }

        const auto ebucket = find_empty_slot(next_bucket, offset);
        //prefetch_heap_block((char*)&_pairs[ebucket]);
        set_states(ebucket, key_h2);

        return ebucket;
    }

    inline size_t empty_delete(size_t gbucket) const noexcept
    {
        const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[gbucket]));
        return (size_t)MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec));
    }

    inline size_t filled_mask(size_t gbucket) const noexcept
    {
        const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[gbucket]));
        return (size_t)MOVEMASK_EPI8(CMPGT_EPI8(vec, simd_delete));
    }

    size_t find_empty_slot(size_t next_bucket, size_t offset) noexcept
    {
        do {
            const auto maske = empty_delete(next_bucket);
            if (maske != 0) {
                const auto ebucket = CTZ(maske) + next_bucket;
                prefetch_heap_block((char*)&_pairs[ebucket]);
                if (offset > _max_probe_length)
                    set_offset(offset);
                return ebucket;
            }
            next_bucket = get_next_bucket(next_bucket, (size_t)++offset);
        } while (true);

        return 0;
    }

    size_t find_filled_slot(size_t next_bucket) const noexcept
    {
        if (EMH_UNLIKELY(_num_filled) == 0)
            return _num_buckets;
        //next_bucket -= next_bucket % simd_bytes;
        while (true) {
            const auto maske = filled_mask(next_bucket);
            if (maske)
                return next_bucket + CTZ(maske);
            next_bucket += simd_bytes;
        }
        return 0;
    }

private:

    HashT   _hasher;
    EqT     _eq;
    int8_t* _states           = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _mask             = 0;
    size_t  _num_filled       = 0;
    size_t  _max_probe_length = 0;
    uint32_t _mlf = (uint32_t)((1 << 28) / EMH_DEFAULT_LOAD_FACTOR);
};

}
