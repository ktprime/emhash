// emhash6::HashMap for C++11/14/17
// version 1.6.7
// https://github.com/ktprime/ktprime/blob/master/hash_table6.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Huang Yuanbing & bailuzhou AT 163.com
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

#pragma once

#include <cstdint>
#include <functional>
#include <cstring>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cassert>
#include <utility>
#include <iterator>
#include <type_traits>

#ifdef __has_include
    #if __has_include("wyhash.h")
    #include "wyhash.h"
    #endif
#elif EMH_WY_HASH
    #include "wyhash.h"
#endif

#ifdef EMH_KEY
    #undef  EMH_KEY
    #undef  EMH_VAL
    #undef  EMH_PKV
    #undef  EMH_NEW
    #undef  EMH_BUCKET
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMH_LIKELY(condition)   __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition)   condition
#    define EMH_UNLIKELY(condition) condition
#endif

#ifndef EMH_BUCKET_INDEX
    #define EMH_BUCKET_INDEX 1
#endif
#if EMH_CACHE_LINE_SIZE < 32
    #define EMH_CACHE_LINE_SIZE 64
#endif

#if EMH_BUCKET_INDEX == 0
    #define EMH_KEY(p,n)     p[n].second.first
    #define EMH_VAL(p,n)     p[n].second.second
    #define EMH_BUCKET(p,n) p[n].first / 2
    #define ADDR_BUCKET(p,n) p[n].first
    #define ISEMPTY_BUCKET(p,n) (int)p[n].first < 0
    #define EMH_PKV(p,n)     p[n].second
    #define EMH_NEW(key, value, bucket, next) new(_pairs + bucket) PairT(next, value_type(key, value)), _num_filled ++; EM_SET(bucket)
#elif EMH_BUCKET_INDEX == 2
    #define EMH_KEY(p,n)     p[n].first.first
    #define EMH_VAL(p,n)     p[n].first.second
    #define EMH_BUCKET(p,n) p[n].second / 2
    #define ADDR_BUCKET(p,n) p[n].second
    #define ISEMPTY_BUCKET(p,n) (int)p[n].second < 0
    #define EMH_PKV(p,n)     p[n].first
    #define EMH_NEW(key, value, bucket, next) new(_pairs + bucket) PairT(value_type(key, value), next), _num_filled ++; EM_SET(bucket)
#else
    #define EMH_KEY(p,n)     p[n].first
    #define EMH_VAL(p,n)     p[n].second
    #define EMH_BUCKET(p,n) p[n].bucket / 2
    #define ADDR_BUCKET(p,n) p[n].bucket
    #define ISEMPTY_BUCKET(p,n) 0 > (int)p[n].bucket
    #define EMH_PKV(p,n)     p[n]
    #define EMH_NEW(key, value, bucket, next) new(_pairs + bucket) PairT(key, value, next), _num_filled ++; EM_SET(bucket)
#endif

#define EM_SET(bucket)   _bitmask[bucket / MASK_BIT] &= ~(1 << (bucket % MASK_BIT))
#define EM_EMPTY(bucket) _bitmask[bucket / MASK_BIT] & (1 << (bucket % MASK_BIT))

#if _WIN32
    #include <intrin.h>
#if _WIN64
    #pragma intrinsic(_umul128)
#endif
#endif

namespace emhash6 {

constexpr uint32_t MASK_BIT = sizeof(uint32_t) * 8;
constexpr uint32_t INACTIVE = (0 - 1u);
constexpr uint32_t BIT_PACK = sizeof(uint64_t) * 2 + sizeof(uint8_t);
constexpr uint32_t SIZE_BIT = sizeof(size_t) * 8;
static_assert(INACTIVE % 2 == 1, "INACTIVE must be even and < 0(to int)");
static_assert((int)INACTIVE < 0, "INACTIVE must be even and < 0(to int)");

inline static uint32_t CTZ(size_t n)
{
#if defined(__x86_64__) || defined(_WIN32) || (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#elif __BIG_ENDIAN__ || (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    n = __builtin_bswap64(n);
#else
    static uint32_t endianness = 0x12345678;
    const auto is_big = *(const char *)&endianness == 0x12;
    if (is_big)
    n = __builtin_bswap64(n);
#endif

#if _WIN32
    unsigned long index;
    #if defined(_WIN64)
    _BitScanForward64(&index, n);
    #else
    _BitScanForward(&index, n);
    #endif
#elif defined (__LP64__) || (SIZE_MAX == UINT64_MAX) || defined (__x86_64__)
    uint32_t index = __builtin_ctzll(n);
#elif 1
    uint32_t index = __builtin_ctzl(n);
#else
    #if defined (__LP64__) || (SIZE_MAX == UINT64_MAX) || defined (__x86_64__)
    uint32_t index;
    __asm__("bsfq %1, %0\n" : "=r" (index) : "rm" (n) : "cc");
    #else
    uint32_t index;
    __asm__("bsf %1, %0\n" : "=r" (index) : "rm" (n) : "cc");
    #endif
#endif

    return (uint32_t)index;
}

template <typename First, typename Second>
struct entry {

    entry(const First& key, const Second& value, uint32_t ibucket) :second(value),first(key) { bucket = ibucket; }
    entry(First&& key, Second&& value, uint32_t ibucket) :second(std::move(value)), first(std::move(key)) { bucket = ibucket; }

    entry(const std::pair<First,Second>& pair) :second(pair.second),first(pair.first) { bucket = INACTIVE; }
    entry(std::pair<First, Second>&& pair) :second(std::move(pair.second)),first(std::move(pair.first)) { bucket = INACTIVE; }

    entry(const entry& pairT) :second(pairT.second),first(pairT.first) { bucket = pairT.bucket; }
    entry(entry&& pairT) :second(std::move(pairT.second)),first(std::move(pairT.first)) { bucket = pairT.bucket; }

    template<typename K, typename V>
    entry(K&& key, V&& value, uint32_t ibucket)
        :second(std::forward<V>(value)), first(std::forward<K>(key))
    {
        bucket = ibucket;
    }

    entry& operator = (entry&& pairT)
    {
        second = std::move(pairT.second);
        bucket = pairT.bucket;
        first = std::move(pairT.first);
        return *this;
    }

    entry& operator = (entry& o)
    {
        second = o.second;
        bucket = o.bucket;
        first  = o.first;
        return *this;
    }

    void swap(entry<First, Second>& o)
    {
        std::swap(second, o.second);
        std::swap(first, o.first);
    }

    Second second;//int
    uint32_t bucket;
    First first; //long
};// __attribute__ ((packed));

/// A cache-friendly hash table with open addressing, linear/qua probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
public:
    typedef HashMap<KeyT, ValueT, HashT, EqT> htype;
    typedef std::pair<KeyT,ValueT>            value_type;

#if EMH_BUCKET_INDEX == 0
    typedef value_type                        value_pair;
    typedef std::pair<uint32_t, value_type>   PairT;
#elif EMH_BUCKET_INDEX == 2
    typedef value_type                        value_pair;
    typedef std::pair<value_type, uint32_t>   PairT;
#else
    typedef entry<KeyT, ValueT>             value_pair;
    typedef entry<KeyT, ValueT>             PairT;
#endif

    static constexpr bool bInCacheLine = sizeof(value_pair) < (2 * EMH_CACHE_LINE_SIZE) / 3;

public:
    typedef KeyT   key_type;
    typedef ValueT mapped_type;
    typedef HashT  hasher;
    typedef EqT    key_equal;
    typedef uint32_t     size_type;
    typedef PairT&       reference;
    typedef const PairT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef value_pair*               pointer;
        typedef value_pair&               reference;

        iterator() { }
        iterator(htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { init(); }

        void init()
        {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _map->bucket_count()) {
                _bmask = *(size_t*)((size_t*)_map->_bitmask + _from / SIZE_BIT);
                _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const
        {
            return _map->EMH_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->EMH_PKV(_pairs, _bucket));
        }

        bool operator==(const iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
#ifdef EM_SAFE_ITER
            auto _bitmask = _map->_bitmask;
            do {
                _bucket++;
            } while (EM_EMPTY(_bucket));
#else
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            while (_bmask == 0 && _from < _map->bucket_count())
                _bmask = ~*(size_t*)((size_t*)_map->_bitmask + (_from += SIZE_BIT) / SIZE_BIT);

            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
            } else
                _bucket = _map->bucket_count();
#endif
        }

    public:
        htype*   _map;
        size_t   _bmask;
        uint32_t _bucket;
        uint32_t _from;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef value_pair*               pointer;
        typedef value_pair&               reference;

        const_iterator() { }
        const_iterator(const iterator& it) : _map(it._map), _bucket(it._bucket) { init(); }
        const_iterator(const htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { init(); }

        void init()
        {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _map->bucket_count()) {
                _bmask = *(size_t*)((size_t*)_map->_bitmask + _from / SIZE_BIT);
                _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const
        {
            return _map->EMH_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->EMH_PKV(_pairs, _bucket));
        }

        bool operator==(const const_iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const const_iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
#ifdef EM_SAFE_ITER
            auto _bitmask = _map->_bitmask;
            do {
                _bucket++;
            } while (EM_EMPTY(_bucket));

#else
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            while (_bmask == 0 && _from < _map->bucket_count())
                _bmask = ~*(size_t*)((size_t*)_map->_bitmask + (_from += SIZE_BIT) / SIZE_BIT);

            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
            } else
                _bucket = _map->bucket_count();
#endif
        }

    public:
        const htype* _map;
        size_t   _bmask;
        uint32_t _bucket;
        uint32_t _from;
    };

    void init(uint32_t bucket, float load_factor = 0.95f)
    {
        _num_buckets = 0;
#if EMH_SAFE_HASH
        _num_main = _hash_inter = 0;
#endif
        _mask = 0;
        _pairs = nullptr;
        _bitmask = nullptr;
        _num_filled = 0;
        max_load_factor(load_factor);
        reserve(bucket);
    }

    HashMap(uint32_t bucket = 4, float load_factor = 0.95f)
    {
        init(bucket, load_factor);
    }

    HashMap(const HashMap& other)
    {
        _pairs = (PairT*)malloc((2 + other._num_buckets) * sizeof(PairT) + other._num_buckets / 8 + BIT_PACK);
        clone(other);
    }

    HashMap(HashMap&& other)
    {
        _num_buckets = _num_filled = 0;
        _pairs = nullptr;
        swap(other);
    }

    HashMap(std::initializer_list<value_type> ilist)
    {
        init((uint32_t)ilist.size());
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin)
            do_insert(begin->first, begin->second);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this == &other)
            return *this;

        if (is_triviall_destructable())
            clearkv();

        if (_num_buckets != other._num_buckets) {
            free(_pairs);
            _pairs = (PairT*)malloc((2 + other._num_buckets) * sizeof(PairT) + other._num_buckets / 8 + BIT_PACK);
        }

        clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other)
    {
        if (this != &other)
            swap(other);
        return *this;
    }

    ~HashMap()
    {
        if (is_triviall_destructable())
            clearkv();
        free(_pairs);
    }

    void clone(const HashMap& other)
    {
        _hasher      = other._hasher;
//        _eq          = other._eq;
        _num_buckets = other._num_buckets;
#if EMH_SAFE_HASH
        _num_main    = other._num_main;
        _hash_inter  = other._hash_inter;
#endif
        _num_filled  = other._num_filled;
        _mask        = other._mask;
        _loadlf      = other._loadlf;
        _bitmask     = (uint32_t*)((char*)_pairs + ((char*)other._bitmask - (char*)other._pairs));
        auto opairs  = other._pairs;

#if __cplusplus >= 201103L || _MSC_VER > 1600 || __clang__
        if (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value)
#else
        if (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value)
#endif
            memcpy(_pairs, opairs, _num_buckets * sizeof(PairT));
        else {
            for (uint32_t bucket = 0; bucket < _num_buckets; bucket++) {
                auto next_bucket = ADDR_BUCKET(_pairs, bucket) = ADDR_BUCKET(opairs, bucket);
                if ((int)next_bucket >= 0)
                    new(_pairs + bucket) PairT(opairs[bucket]);
            }
        }
        memcpy(_pairs + _num_buckets, opairs + _num_buckets, 2 * sizeof(PairT) + _num_buckets / 8 + BIT_PACK);
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher, other._hasher);
//      std::swap(_eq, other._eq);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
#if EMH_SAFE_HASH
        std::swap(_num_main, other._num_main);
        std::swap(_hash_inter, other._hash_inter);
#endif
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
        std::swap(_loadlf, other._loadlf);
        std::swap(_bitmask, other._bitmask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        if (0 == _num_filled)
            return {this, _num_buckets};
        uint32_t bucket = 0;
        while (EM_EMPTY(bucket)) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator cbegin() const
    {
        if (0 == _num_filled)
            return {this, _num_buckets};
        uint32_t bucket = 0;
        while (EM_EMPTY(bucket)) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return {this, _num_buckets};
    }

    const_iterator cend() const
    {
        return {this, _num_buckets};
    }

    const_iterator end() const
    {
        return {this, _num_buckets};
    }

    size_type size() const
    {
        return _num_filled;
    }

    bool empty() const
    {
        return _num_filled == 0;
    }

    // Returns the number of buckets.
    size_type bucket_count() const
    {
        return _num_buckets;
    }

    /// Returns average number of elements per bucket.
    float load_factor() const
    {
        return static_cast<float>(_num_filled) / (_mask + 1);
    }

    HashT& hash_function() const
    {
        return _hasher;
    }

    EqT& key_eq() const
    {
        return _eq;
    }

    constexpr float max_load_factor() const
    {
        return (1 << 27) / (float)_loadlf;
    }

    void max_load_factor(float value)
    {
        if (value < 0.9999f && value > 0.2f)
            _loadlf = (uint32_t)((1 << 27) / value);
    }

    constexpr size_type max_size() const
    {
        return (1u << 31) / sizeof(PairT);
    }

    constexpr size_type max_bucket_count() const
    {
        return (1u << 31) / sizeof(PairT);
    }

#ifdef EMH_STATIS
    //Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key) & _mask;
        const auto next_bucket = ADDR_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0)
            return 0;
        else if (bucket == 2 * next_bucket)
            return bucket + 1;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        return (hash_bucket(bucket_key) & _mask) + 1;
    }

    //Returns the number of elements in bucket n.
    size_type bucket_size(const uint32_t bucket) const
    {
        auto next_bucket = ADDR_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0)
            return 0;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        next_bucket = hash_bucket(bucket_key) & _mask;
        uint32_t ibucket_size = 1;

        //iterator each item in current main bucket
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    size_type get_main_bucket(const uint32_t bucket) const
    {
        if (ISEMPTY_BUCKET(_pairs, bucket))
            return -1u;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key) & _mask;
        return main_bucket;
    }

    int get_cache_info(uint32_t bucket, uint32_t next_bucket) const
    {
        auto pbucket = reinterpret_cast<std::ptrdiff_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<std::ptrdiff_t>(&_pairs[next_bucket]);
        if (pbucket / 64 == pnext / 64)
            return 0;
        auto diff = pbucket > pnext ? (pbucket - pnext) : pnext - pbucket;
        if (diff < 127 * 64)
            return diff / 64 + 1;
        return 127;
    }

    int get_bucket_info(const uint32_t bucket, uint32_t steps[], const uint32_t slots) const
    {
        auto next_bucket = ADDR_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0)
            return -1;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key) & _mask;
        if (main_bucket != bucket)
            return 0;
        else if (next_bucket == bucket)
            return 1;

        steps[get_cache_info(bucket, next_bucket) % slots] ++;
        uint32_t ibucket_size = 2;
        //find a new empty and linked it to tail
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;

            steps[get_cache_info(nbucket, next_bucket) % slots] ++;
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    void dump_statics() const
    {
        uint32_t buckets[129] = {0};
        uint32_t steps[129]   = {0};
        for (uint32_t bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, 128);
            if (bsize > 0)
                buckets[bsize] ++;
        }

        uint32_t sumb = 0, collision = 0, sumc = 0, finds = 0, sumn = 0;
        puts("============== buckets size ration ========");
        for (uint32_t i = 0; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            const auto bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;
            sumb += bucketsi;
            sumn += bucketsi * i;
            collision += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            printf("  %2u  %8u  %0.8lf  %2.3lf\n", i, bucketsi, bucketsi * 1.0 * i / _num_filled, sumn * 100.0 / _num_filled);
        }

        puts("========== collision miss ration ===========");
        for (uint32_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
            sumc += steps[i];
            if (steps[i] <= 2)
                continue;
            printf("  %2u  %8u  %0.2lf  %.2lf\n", i, steps[i], steps[i] * 100.0 / collision, sumc * 100.0 / collision);
        }

        if (sumb == 0)  return;
        printf("    _num_filled/aver_size/packed collision/cache_miss/hit_find = %u/%.2lf/%zd/ %.2lf%%/%.2lf%%/%.2lf\n",
                _num_filled, _num_filled * 1.0 / sumb, sizeof(PairT), (collision * 100.0 / _num_filled), (collision - steps[0]) * 100.0 / _num_filled, finds * 1.0 / _num_filled);
        assert(sumn == _num_filled);
        assert(sumc == collision);
        puts("============== buckets size end =============");
    }
#endif

    // ------------------------------------------------------------
    template<typename K>
    iterator find(const K& key, size_t hash_v) noexcept
    {
        return {this, find_filled_hash(key, hash_v)};
    }

    template<typename K>
    const_iterator find(const K& key, size_t hash_v) const noexcept
    {
        return {this, find_filled_hash(key, hash_v)};
    }

    iterator find(const KeyT& key) noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    const_iterator find(const KeyT& key) const noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    bool contains(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    size_type count(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets ? 1 : 0;
    }

    std::pair<iterator, iterator> equal_range(const KeyT& key) const noexcept
    {
        const auto found = find(key);
        if (found == end())
            return { found, found };
        else
            return { found, std::next(found) };
    }

    /// Returns false if key isn't found.
    bool try_get(const KeyT& key, ValueT& val) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        const auto found = bucket != _num_buckets;
        if (found) {
            val = EMH_VAL(_pairs, bucket);
        }
        return found;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& key) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Const version of the above
    ValueT* try_get(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Convenience function.
    ValueT get_or_return_default(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? ValueT() : EMH_VAL(_pairs, bucket);
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert(key, value);
    }

    std::pair<iterator, bool> insert(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert(std::move(key), std::move(value));
    }

    std::pair<iterator, bool> insert(const KeyT& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert(key, std::move(value));
    }

    std::pair<iterator, bool> insert(KeyT&& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert(std::move(key), value);
    }

    template<typename K, typename V>
    inline std::pair<iterator, bool> do_insert(K&& key, V&& value)
    {
        const auto bucket = find_or_allocate(key);
        const auto next   = bucket / 2;
        const auto found = ISEMPTY_BUCKET(_pairs, next);
        if (found) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), next, bucket);
        }
        return { {this, next}, found };
    }

    std::pair<iterator, bool> insert(const value_type& p)
    {
        check_expand_need();
        return do_insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(value_type && p)
    {
        check_expand_need();
        return do_insert(std::move(p.first), std::move(p.second));
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        reserve(ilist.size() + _num_filled);
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin)
            do_insert(begin->first, begin->second);
    }

#if 0
    template <typename Iter>
    void insert(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin)
            do_insert(*begin);
    }

    uint32_t try_insert_mainbucket(const KeyT& key, const ValueT& value)
    {
        const auto bucket = hash_bucket(key) & _mask;
        if (ISEMPTY_BUCKET(_pairs, bucket))
        {
#if EMH_SAFE_HASH
            _num_main ++;
#endif
            EMH_NEW(key, value, bucket, bucket * 2);
            return bucket;
        }

        return -1u;
    }

    template <typename Iter>
    void insert2(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            if ((int)try_insert_mainbucket(begin->first, begin->second) < 0) {
                std::swap(*begin, *citend++);
            }
        }

        for (; citbeg != citend; ++citbeg)
            insert(*citbeg);
    }

    template <typename Iter>
    void insert_unique(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin)
            do_insert_unqiue(*begin);
    }
#endif

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert_unqiue(std::move(key), std::move(value));
    }

    uint32_t insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert_unqiue(key, value);
    }

    uint32_t insert_unique(value_type&& p)
    {
        check_expand_need();
        return do_insert_unqiue(std::move(p.first), std::move(p.second));
    }

    uint32_t insert_unique(const value_type& p)
    {
        check_expand_need();
        return do_insert_unqiue(p.first, p.second);
    }

    template<typename K, typename V>
    inline uint32_t do_insert_unqiue(K&& key, V&& value)
    {
        auto bucket = find_unique_bucket(key);
        EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket / 2, bucket);
        return bucket;
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args)
    {
        check_expand_need();
        return do_insert(std::forward<Args>(args)...);
    }

    //no any optimize for position
    template <class... Args>
    iterator emplace_hint(const_iterator position, Args&&... args)
    {
        return insert(std::forward<Args>(args)...).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const key_type& k, Args&&... args) { return insert(k, std::forward<Args>(args)...).first; }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args) { return insert(std::move(k), std::forward<Args>(args)...).first; }

    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }


    std::pair<iterator, bool> insert_or_assign(const KeyT&& key, ValueT&& value) { return insert(key, std::move(value)); }
    std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& value) { return insert(std::move(key), std::move(value)); }

    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        const auto next   = bucket / 2;
        /* Check if inserting a new value rather than overwriting an old entry */
        if (ISEMPTY_BUCKET(_pairs, next)) {
            EMH_NEW(key, std::move(ValueT()), next, bucket);
        }

        //bugs here if return local reference rehash happens
        return EMH_VAL(_pairs, next);
    }

    ValueT& operator[](KeyT&& key)
    {
        auto bucket = find_or_allocate(key);
        auto next   = bucket / 2;
        if (ISEMPTY_BUCKET(_pairs, next)) {
            if (EMH_UNLIKELY(check_expand_need())) {
                bucket = find_unique_bucket(key);
                next = bucket / 2;
            }

            EMH_NEW(std::move(key), std::move(ValueT()), next, bucket);
        }

        return EMH_VAL(_pairs, next);
    }

    // -------------------------------------------------------
    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key)
    {
        const auto bucket = erase_key(key);
        if ((int)bucket < 0)
            return 0;

        clear_bucket(bucket);
        return 1;
    }

    //iterator erase(const_iterator begin_it, const_iterator end_it)
    iterator erase(const_iterator cit)
    {
        iterator it(this, cit._bucket);
        return erase(it);
    }

    /// Erase an element typedef an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
        //erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? ++it : it;
    }

    void _erase(const_iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
    }

    static constexpr bool is_triviall_destructable()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600 || __clang__
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    static constexpr bool is_copy_trivially()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600 || __clang__
        return !(std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clearkv()
    {
        for (uint32_t bucket = 0; _num_filled > 0; ++bucket) {
            if (ISEMPTY_BUCKET(_pairs, bucket))
                continue;
            clear_bucket(bucket);
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (is_triviall_destructable() || !bInCacheLine || _num_filled < _num_buckets / 4)
            clearkv();
        else {
            memset(_pairs, 0xFFFFFFFF, sizeof(_pairs[0]) * _num_buckets);
            memset(_bitmask, 0xFFFFFFFF, _num_buckets / 8);
        }
        _num_filled = 0;
#if EMH_SAFE_HASH
        _num_main = _hash_inter = 0;
#endif
    }

    void shrink_to_fit()
    {
        rehash(_num_filled);
    }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems)
    {
#if EMH_HIGH_LOAD
        const auto required_buckets = (uint32_t)num_elems;
#else
        const auto required_buckets = (uint32_t)(num_elems * _loadlf >> 27);
#endif
        if (EMH_LIKELY(required_buckets < _mask))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    ///three ways may incr rehash: bad hash function, load_factor is high, or need shrink
    void rehash(uint32_t required_buckets)
    {
        if (required_buckets < _num_filled)
            return;

        uint32_t num_buckets = _num_filled > 65536 ? (1u << 16) : 8u;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        //assert(num_buckets > _num_filled);
        auto old_num_buckets = _num_buckets;
        auto old_num_filled  = _num_filled;
        auto new_pairs = (PairT*)malloc((2 + num_buckets) * sizeof(PairT) + num_buckets / 8 + BIT_PACK);
#if 0
        if (EMH_UNLIKELY(!new_pairs))
            throw std::bad_alloc();
#else
        assert(!!new_pairs);
#endif

        auto old_pairs = _pairs;

        _bitmask = (uint32_t*)(new_pairs + 2 + num_buckets);
        const auto bitmask_pack = ((size_t)_bitmask) % sizeof(uint64_t);
        if (bitmask_pack != 0) {
            _bitmask = (uint32_t*)((char*)_bitmask + sizeof(uint64_t) - bitmask_pack);
            assert(0 == ((size_t)_bitmask) % sizeof(uint64_t));
        }

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _pairs       = new_pairs;

#if EMH_SAFE_HASH
        if (_hash_inter == 0 && old_num_filled > 100) {
            //adjust hash function if bad hash function, alloc more memory
            uint32_t mbucket = 0;
            for (uint32_t src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
                if (ADDR_BUCKET(old_pairs, src_bucket) % 2 == 0)
                    mbucket ++;
            }
            if (mbucket * 2 < old_num_filled) { _hash_inter = old_num_buckets / mbucket; }
        }
        _num_main = 0;
#endif

        if (bInCacheLine)
            memset((char*)_pairs, 0xFFFFFFFF, sizeof(_pairs[0]) * num_buckets);
        else
            for (uint32_t bucket = 0; bucket < num_buckets; bucket++)
                ADDR_BUCKET(_pairs, bucket) = INACTIVE;

        //pack tail two tombstones for fast iterator and find empty_bucket without checking overflow
        memset((char*)(_pairs + num_buckets), 0, sizeof(PairT) * 2);

        /***************** init bitmask ---------------------- ***********/
        memset(_bitmask, 0xFFFFFFFF, num_buckets / 8);
        memset((char*)_bitmask + num_buckets / 8, 0, sizeof(uint64_t) + sizeof(uint8_t));
        //pack last position to bit 0
        /**************** -------------------------------- *************/

        for (uint32_t src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
            if (ISEMPTY_BUCKET(old_pairs, src_bucket))
                continue;

            auto&& key = EMH_KEY(old_pairs, src_bucket);
            const auto bucket = find_unique_bucket(key);
            EMH_NEW(std::move(key), std::move(EMH_VAL(old_pairs, src_bucket)), bucket / 2, bucket);
            if (is_triviall_destructable())
                old_pairs[src_bucket].~PairT();
        }

#if EMH_REHASH_LOG
        if (_num_filled > EMH_REHASH_LOG) {
#ifndef EMH_SAFE_HASH
            auto _num_main = _num_filled/2;
            auto _hash_inter = 0;
#endif
            uint32_t collision = _num_filled - _num_main;
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/_hash_inter/aver_size/K.V/pack/collision = %u/%u/%.2lf/%s.%s/%zd/%.2lf%%",
                    _num_filled, _hash_inter, (double)_num_filled / _num_main, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(_pairs[0]), (collision * 100.0 / _num_buckets));
#ifdef EMH_LOG
            static uint32_t ihashs = 0;
            EMH_LOG() << "EMH_BUCKET_INDEX = " << EMH_BUCKET_INDEX << "|rhash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
#else
            puts(buff);
#endif
        }
#endif

        free(old_pairs);
        assert(old_num_filled == _num_filled);
    }

private:
    // Can we fit another element?
    inline bool check_expand_need()
    {
#if EMH_SAFE_HASH > 1
        if (_num_main * 2 < _num_filled && _num_filled > 100 && _hash_inter == 0) {
            rehash(_num_filled);
            return true;
        }
#endif
        return reserve(_num_filled);
    }

    void clear_bucket(uint32_t bucket)
    {
        _pairs[bucket].~PairT();
        ADDR_BUCKET(_pairs, bucket) = INACTIVE;
        _num_filled --;
        _bitmask[bucket / MASK_BIT] |= 1 << (bucket % MASK_BIT);
    }

    template<class K = uint32_t> typename std::enable_if <!bInCacheLine, K>::type
    erase_key(const KeyT& key)
    {
        const auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = ADDR_BUCKET(_pairs, bucket);
        if (next_bucket % 2 > 0)
            return INACTIVE;

        const auto eqkey = _eq(key, EMH_KEY(_pairs, bucket));
        if (next_bucket == bucket * 2) {
#if EMH_SAFE_HASH
            return eqkey ? (_num_main --, bucket) : INACTIVE;
#else
            return eqkey ? bucket : INACTIVE;
#endif
         } else if (eqkey) {
            next_bucket /= 2;
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (is_copy_trivially())
                EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
            else
                EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
            ADDR_BUCKET(_pairs, bucket) = next_bucket == nbucket ? bucket * 2 : nbucket * 2;
            return next_bucket;
        }

        next_bucket /= 2;
        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
                ADDR_BUCKET(_pairs, prev_bucket) = (nbucket == next_bucket ? prev_bucket * 2 : nbucket * 2) + (1 - (prev_bucket == bucket));
                return next_bucket;
            }

            if (nbucket == next_bucket)
                break;
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return INACTIVE;
    }

    template<class K = uint32_t> typename std::enable_if<bInCacheLine, K>::type
    erase_key(const KeyT& key)
    {
        const auto empty_bucket = INACTIVE;
        const auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = ADDR_BUCKET(_pairs, bucket);
        if (next_bucket % 2 > 0)
            return empty_bucket;
        else if (next_bucket == bucket * 2) //only one main bucket
#if EMH_SAFE_HASH
            return _eq(key, EMH_KEY(_pairs, bucket)) ? (_num_main --, bucket) : empty_bucket;
#else
            return _eq(key, EMH_KEY(_pairs, bucket)) ? bucket : empty_bucket;
#endif

        //find erase key and swap to last bucket
        uint32_t prev_bucket = bucket, find_bucket = empty_bucket;
        next_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
                find_bucket = next_bucket;
                if (nbucket == next_bucket) {
                    ADDR_BUCKET(_pairs, prev_bucket) = prev_bucket * 2 + 1 - (prev_bucket == bucket);
                    break;
                }
            }
            if (nbucket == next_bucket) {
                if ((int)find_bucket >= 0) {
                    EMH_PKV(_pairs, find_bucket).swap(EMH_PKV(_pairs, nbucket));
//                    EMH_PKV(_pairs, find_bucket) = EMH_PKV(_pairs, nbucket);
                    ADDR_BUCKET(_pairs, prev_bucket) = prev_bucket * 2 + 1 - (prev_bucket == bucket);
                    find_bucket = nbucket;
                }
                break;
            }
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return find_bucket;
    }

    uint32_t erase_bucket(const uint32_t bucket)
    {
        const auto main_bucket = hash_bucket(EMH_KEY(_pairs, bucket)) & _mask;
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (bucket == next_bucket) { //erase the last bucket
            if (bucket != main_bucket) {
                const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
                ADDR_BUCKET(_pairs, prev_bucket) = prev_bucket * 2 + 1 - (prev_bucket == main_bucket); //maybe only left main bucket
            }
#if EMH_SAFE_HASH
            else
                _num_main --;
#endif
            return bucket;
        }

        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, nbucket)); //swap the erase bucket to last bucket
                ADDR_BUCKET(_pairs, prev_bucket) = prev_bucket * 2 + (prev_bucket == main_bucket ? 0 : 1);
                return nbucket;
            }

            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }
    }

    // Find the bucket with this key, or return bucket size
    //1. next_bucket = INACTIVE, empty bucket
    //2. next_bucket % 2 == 0 is main bucket
    uint32_t find_filled_bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = ADDR_BUCKET(_pairs, bucket);

        if (next_bucket % 2 > 0)
            return _num_buckets;
        else if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket * 2)
            return _num_buckets;

        next_bucket /= 2;
        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        return _num_buckets;
    }

    //kick out bucket and find empty to occpuy
    //it will break the orgin link and relnik again.
    //before: main_bucket-->prev_bucket --> bucket   --> next_bucket
    //atfer : main_bucket-->prev_bucket --> (removed)--> new_bucket--> next_bucket
    uint32_t kickout_bucket(const uint32_t bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto main_bucket = hash_bucket(EMH_KEY(_pairs, bucket)) & _mask;
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket])); EM_SET(new_bucket);
        if (next_bucket == bucket)
            ADDR_BUCKET(_pairs, new_bucket) = new_bucket * 2 + 1;

        ADDR_BUCKET(_pairs, prev_bucket) += (new_bucket - bucket) * 2;
#if EMH_SAFE_HASH
        _num_main ++;
#endif
        clear_bucket(bucket); _num_filled ++;
        return bucket * 2;
    }

/***
** inserts a new key into a hash table; first, check whether key's main
** bucket/position is free. If not, check whether colliding node/bucket is in its main
** position or not: if it is not, move colliding bucket to an empty place and
** put new key in its main position; otherwise (colliding bucket is in its main
** position), new key goes to an empty position. ***/

    uint32_t find_or_allocate(const KeyT& key)
    {
        auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = ADDR_BUCKET(_pairs, bucket);
#if EMH_SAFE_HASH
        if ((int)next_bucket < 0)
            return _num_main ++, bucket * 2;
        else if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket * 2;
#else
        if ((int)next_bucket < 0 || _eq(key, EMH_KEY(_pairs, bucket)))
            return bucket * 2;
#endif

        //check current bucket_key is in main bucket or not
        if (next_bucket == bucket * 2)
            return (ADDR_BUCKET(_pairs, bucket) = find_empty_bucket(bucket) * 2) + 1;
        else if (next_bucket % 2 > 0)
            return kickout_bucket(bucket);

        next_bucket /= 2;
        //find next linked bucket and check key
        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
#if EMH_LRU_SET
                EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, bucket));
                return bucket * 2;
#else
                return next_bucket * 2;
#endif
            }

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //find a new empty and link it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return ADDR_BUCKET(_pairs, next_bucket) = new_bucket * 2 + 1;
    }

    uint32_t find_cacheline_bucket(const uint32_t bucket_from) const
    {
        //try find a near in the same cache line
        auto empty_bucket = 0;
        for (int i = 6; i > 2; i--) {
#if 1
            const auto near_bucket = (bucket_from + i) & _mask;
            const auto next_bucket = ADDR_BUCKET(_pairs, near_bucket);
            if (next_bucket % 2 == 0)
                continue;

            //assert(ADDR_BUCKET(_pairs, near_bucket) != INACTIVE);
            const auto main_bucket = hash_bucket(EMH_KEY(_pairs, near_bucket)) & _mask;
            if (main_bucket + 10 > near_bucket)
                continue;

            const auto prev_bucket = find_prev_bucket(main_bucket, near_bucket);
            ADDR_BUCKET(_pairs, prev_bucket) += (empty_bucket - near_bucket) * 2;
            new(_pairs + empty_bucket) PairT(std::move(_pairs[near_bucket])); _pairs[near_bucket].~PairT();
            if (next_bucket / 2 == near_bucket)
                ADDR_BUCKET(_pairs, empty_bucket) = empty_bucket * 2 + 1;
            EM_SET(empty_bucket);

            empty_bucket = near_bucket;
            ADDR_BUCKET(_pairs, near_bucket) = INACTIVE;
            break;
#endif
        }

        return empty_bucket;
    }

    // key is not in this map. Find a place to put it.
    uint32_t find_empty_bucket(const uint32_t bucket_from)
    {
#if 0
        const auto bucket1 = bucket_from + 1;
        if (ISEMPTY_BUCKET(_pairs, bucket1))
            return bucket1;

        if (bInCacheLine) {
            const auto bucket2 = bucket_from + 2;
            if (ISEMPTY_BUCKET(_pairs, bucket2))
                return bucket2;
        }
#endif

        const auto boset = bucket_from % 8;
        const auto bmask = *(size_t*)((uint8_t*)_bitmask + bucket_from / 8) >> boset;
        if (EMH_LIKELY(bmask != 0))
            return bucket_from + CTZ(bmask);

#if 0
        const auto qmask = (64 + _num_buckets - 1) / 64 - 1;
        for (uint32_t last = 3, step = (bucket_from + _num_filled) & qmask; ;step = (step + ++last) & qmask) {
        //for (uint32_t last = 3, step = (bucket_from + _num_filled) & qmask; ;step = (step + last) & qmask) {
            const auto bmask2 = *((size_t*)_bitmask + step);
            if (bmask2 != 0)
                return step * SIZE_BIT + CTZ(bmask2);

            const auto next1 = step + 1;
            const auto bmask1 = *((size_t*)_bitmask + next1);
            if (bmask1 != 0)
                return next1 * SIZE_BIT + CTZ(bmask1);
        }
#endif
        const auto qmask = _mask / SIZE_BIT;
        for (uint32_t step = _last & qmask; ; step = ++_last & qmask) {
            const auto bmask = *((size_t*)_bitmask + step);
            if (bmask != 0)
                return step * SIZE_BIT + CTZ(bmask);
        }
        return 0;
    }

    uint32_t find_last_bucket(uint32_t main_bucket) const
    {
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

    uint32_t find_prev_bucket(uint32_t main_bucket, const uint32_t bucket) const
    {
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

    uint32_t find_unique_bucket(const KeyT& key)
    {
        const auto bucket = hash_bucket(key) & _mask;
        const auto next_bucket = ADDR_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0) {
#if EMH_SAFE_HASH
            _num_main ++;
#endif
            return bucket * 2;
        }

        //check current bucket_key is in main bucket or not
        if (next_bucket == bucket * 2)
            return (ADDR_BUCKET(_pairs, bucket) = find_empty_bucket(bucket) * 2) + 1;
        else if (next_bucket % 2 > 0)
            return kickout_bucket(bucket);


        const auto last_bucket = find_last_bucket(next_bucket / 2);
        //find a new empty and link it to tail
        return ADDR_BUCKET(_pairs, last_bucket) = find_empty_bucket(last_bucket) * 2 + 1;
    }

    static inline uint64_t hash64(uint64_t key)
    {
#if __SIZEOF_INT128__
        constexpr uint64_t k = UINT64_C(11400714819323198485);
        __uint128_t r = key; r *= k;
        return (uint32_t)(r >> 64) + (uint32_t)r;
#elif _WIN32
        uint64_t high;
        constexpr uint64_t k = UINT64_C(11400714819323198485);
        return _umul128(key, k, &high) + high;
#elif 1
        uint64_t const r = key * UINT64_C(0xca4bcaa75ec3f625);
        return (r >> 32) + r;
#elif 1
        //MurmurHash3Mixer
        uint64_t h = key;
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccd;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53;
        h ^= h >> 33;
        return h;
#elif 1
        uint64_t x = key;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
#endif
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, uint32_t>::type = 0>
    inline uint32_t hash_bucket(const UType key) const
    {
#ifdef EMH_FIBONACCI_HASH
        return hash64(key);
#elif EMH_SAFE_HASH
        if (_hash_inter > 0)
            return hash64(key);

        return _hasher(key);
#elif EMH_IDENTITY_HASH
        return key + (key >> (sizeof(UType) * 4));
#elif EMH_WYHASH64
        return wyhash64(key, _num_buckets);
#else
        return _hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint32_t hash_bucket(const UType& key) const
    {
#ifdef WYHASH_LITTLE_ENDIAN
        return wyhash(key.c_str(), key.size(), key.size() + UINT64_C(0x12345678abcedef));
#elif EMH_BKR_HASH
        uint32_t hash = 0;
        if (key.size() < 64) {
            for (const auto c : key)
                hash = c + hash * 131;
        } else {
            for (int i = 0, j = 1; i < key.size(); i += j++)
                hash = key[i] + hash * 131;
        }
        return hash;
#else
        return _hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value && !std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint32_t hash_bucket(const UType& key) const
    {
#ifdef EMH_FIBONACCI_HASH
        return _hasher(key) * 11400714819323198485ull;
#else
        return _hasher(key);
#endif
    }

private:
    PairT*    _pairs;
    uint32_t* _bitmask;
    //8 * 2 + 4 * 5 = 16 + 20 = 32
    HashT     _hasher;
    EqT       _eq;
    uint32_t  _mask;
    uint32_t  _num_buckets;

    uint32_t  _num_filled;
    uint32_t  _loadlf;

#if EMH_SAFE_HASH
    uint32_t  _num_main;
    uint32_t  _hash_inter;
#endif

    uint32_t  _last;
};
} // namespace emhash
#if __cplusplus >= 201103L
//template <class Key, class Val> using emihash = emhash2::HashMap<Key, Val, std::hash<Key>>;
#endif

