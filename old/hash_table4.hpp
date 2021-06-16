// emhash4::HashMap for C++11
// version 1.4.2
// https://github.com/ktprime/ktprime/blob/master/hash_table4.hpp
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


// From
// NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
// Quadratic collision resolution   1 - ln(1-L) - L/2    1/(1-L) - L - ln(1-L)
// Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
//
// -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
// QUADRATIC COLLISION RES.
//    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
//    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
// LINEAR COLLISION RES.
//    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
//    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0

#pragma once

#include <cstring>
#include <string>
#include <cstdlib>
#include <type_traits>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>

#ifdef __has_include
    #if __has_include("wyhash.h")
    #include "wyhash.h"
    #endif
#elif EMH_WY_HASH
    #include "wyhash.h"
#endif

#ifdef  EMH_KEY
    #undef  EMH_KEY
    #undef  EMH_VAL
    #undef  EMH_BUCKET
    #undef  EMH_PKV
    #undef  hash_bucket
    #undef  EMH_NEW
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMH_LIKELY(condition) __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition) condition
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
    #define EMH_BUCKET(p,n) p[n].first
    #define EMH_PKV(p,n)     p[n].second
    #define EMH_NEW(key, value, bucket) new(_pairs + bucket) PairT(bucket, std::pair<KeyT, ValueT>(key, value)); _num_filled ++
#elif EMH_BUCKET_INDEX == 2
    #define EMH_KEY(p,n)     p[n].first.first
    #define EMH_VAL(p,n)     p[n].first.second
    #define EMH_BUCKET(p,n) p[n].second
    #define EMH_PKV(p,n)     p[n].first
    #define EMH_NEW(key, value, bucket) new(_pairs + bucket) PairT(std::pair<KeyT, ValueT>(key, value), bucket); _num_filled ++
#else
    #define EMH_KEY(p,n)     p[n].first
    #define EMH_VAL(p,n)     p[n].second
    #define EMH_BUCKET(p,n) p[n].bucket
    #define EMH_PKV(p,n)     p[n]
    #define EMH_NEW(key, value, bucket) new(_pairs + bucket) PairT(key, value, bucket), _num_filled ++
#endif

namespace emhash4 {

constexpr uint32_t INACTIVE = 0xFFFFFFFF;

template <typename First, typename Second>
struct entry {
    entry(const First& key, const Second& value, uint32_t ibucket)
        :second(value),first(key)
    {
        bucket = ibucket;
    }

    entry(First&& key, Second&& value, uint32_t ibucket)
        :second(std::move(value)), first(std::move(key))
    {
        bucket = ibucket;
    }

    template<typename K, typename V>
    entry(K&& key, V&& value, uint32_t ibucket)
        :second(std::forward<V>(value)), first(std::forward<K>(key))
    {
        bucket = ibucket;
    }

    entry(const std::pair<First,Second>& pair)
        :second(pair.second),first(pair.first)
    {
        bucket = INACTIVE;
    }

    entry(std::pair<First, Second>&& pair)
        :second(std::move(pair.second)),first(std::move(pair.first))
    {
        bucket = INACTIVE;
    }

    entry(const entry& pairT)
        :second(pairT.second),first(pairT.first)
    {
        bucket = pairT.bucket;
    }

    entry(entry&& pairT)
        :second(std::move(pairT.second)),first(std::move(pairT.first))
    {
        bucket = pairT.bucket;
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

#if EMH_BUCKET_INDEX == 0
    typedef std::pair<KeyT, ValueT>          value_pair;
    typedef std::pair<uint32_t, value_pair > PairT;
#elif EMH_BUCKET_INDEX == 2
    typedef std::pair<KeyT, ValueT>         value_pair;
    typedef std::pair<value_pair, uint32_t> PairT;
#else
    typedef entry<KeyT, ValueT>             PairT;
    typedef entry<KeyT, ValueT>             value_pair;
#endif

public:
    typedef KeyT   key_type;
    typedef ValueT mapped_type;

    typedef  size_t       size_type;
    typedef std::pair<KeyT,ValueT>        value_type;
    typedef  PairT&       reference;
    typedef  const PairT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef value_pair*               pointer;
        typedef value_pair&               reference;

        iterator() { }
        iterator(htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { }

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
            do {
                _bucket++;
            } while (_map->EMH_BUCKET(_pairs, _bucket) == INACTIVE);
        }

    public:
        htype* _map;
        uint32_t  _bucket;
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
        const_iterator(const iterator& proto) : _map(proto._map), _bucket(proto._bucket) { }
        const_iterator(const htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { }

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
            do {
                _bucket++;
            } while (_map->EMH_BUCKET(_pairs, _bucket) == INACTIVE);
        }

    public:
        const htype* _map;
        uint32_t  _bucket;
    };

    void init(uint32_t bucket, float load_factor = 0.88f)
    {
        _num_buckets = 0;
        _max_bucket = 0;
        _last_colls = 0;
        _mask = 0;
        _pairs = nullptr;
        _num_filled = 0;
        _hash_inter = 0;
        max_load_factor(load_factor);
        reserve(bucket);
    }

    HashMap(uint32_t bucket = 4, float load_factor = 0.88f)
    {
        init(bucket, load_factor);
    }

    HashMap(const HashMap& other)
    {
        _pairs = (PairT*)malloc((2 + other._max_bucket) * sizeof(PairT));
        clone(other);
    }

    HashMap(HashMap&& other)
    {
        init(0);
        *this = std::move(other);
    }

    HashMap(std::initializer_list<std::pair<KeyT, ValueT>> il)
    {
        init((uint32_t)il.size());
        for (auto begin = il.begin(); begin != il.end(); ++begin)
            insert(*begin);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this == &other)
            return *this;

        if (is_notriviall_destructable())
            clearkv();

        if (_num_buckets != other._num_buckets) {
            free(_pairs);
            _pairs = (PairT*)malloc((2 + other._max_bucket) * sizeof(PairT));
        }

        clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other)
    {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap()
    {
        if (is_notriviall_destructable())
            clearkv();

        free(_pairs);
    }

    void clone(const HashMap& other)
    {
        _hasher      = other._hasher;
        _eq          = other._eq;
        _num_buckets = other._num_buckets;
        _num_filled  = other._num_filled;
        _max_bucket  = other._max_bucket;
        _last_colls  = other._last_colls;
        _mask        = other._mask;
        _hash_inter       = other._hash_inter;
        _loadlf      = other._loadlf;
        auto opairs = other._pairs;

#if __cplusplus >= 201402L || _MSC_VER > 1600 || __clang__
        if (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value) {
#else
        if (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value) {
#endif
            memcpy(_pairs, opairs, other._max_bucket * sizeof(PairT));
        }
        else {
            for (uint32_t bucket = 0; bucket < _max_bucket; bucket++) {
                auto next_bucket = EMH_BUCKET(_pairs, bucket) = EMH_BUCKET(opairs, bucket);
                if (next_bucket != INACTIVE)
                    new(_pairs + bucket) PairT(opairs[bucket]);
            }
        }
        EMH_BUCKET(_pairs, _max_bucket) = EMH_BUCKET(_pairs, _max_bucket + 1) = 0;
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher, other._hasher);
        std::swap(_eq, other._eq);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_max_bucket, other._max_bucket);
        std::swap(_last_colls, other._last_colls);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
        std::swap(_loadlf, other._loadlf);
        std::swap(_hash_inter, other._hash_inter);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        uint32_t bucket = 0;
        while (EMH_BUCKET(_pairs, bucket) == INACTIVE) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator cbegin() const
    {
        uint32_t bucket = 0;
        while (EMH_BUCKET(_pairs, bucket) == INACTIVE) {
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
        return {this, _max_bucket};
    }

    const_iterator cend() const
    {
        return {this, _max_bucket};
    }

    const_iterator end() const
    {
        return {this, _max_bucket};
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
        return static_cast<float>(_num_filled) / (_max_bucket + 1);
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
        if (value < 0.99f && value > 0.2f)
            _loadlf = (uint32_t)((1 << 27) / value);
    }

    constexpr size_type max_size() const
    {
        return (1 << 31) / sizeof(PairT);
    }

    constexpr size_type max_bucket_count() const
    {
        return (1 << 31) / sizeof(PairT);
    }

#ifdef EMH_STATIS
    //Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key);
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        return hash_bucket(bucket_key) + 1;
    }

    //Returns the number of elements in bucket n.
    size_type bucket_size(const uint32_t bucket) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return 0;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        next_bucket = hash_bucket(bucket_key);
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
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key);
        return main_bucket;
    }

    size_type get_cache_info(uint32_t bucket, uint32_t next_bucket) const
    {
        auto pbucket = reinterpret_cast<std::uintptr_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<std::uintptr_t>(&_pairs[next_bucket]);
        if (pbucket / 64 == pnext / 64)
            return 0;
        auto diff = pbucket > pnext ? (pbucket - pnext) : pnext - pbucket;
        if (diff < 127 * 64)
            return diff / 64 + 1;
        return 127;
    }

    int get_bucket_info(const uint32_t bucket, uint32_t steps[], const uint32_t slots) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return -1;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key);
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
        return find_filled_bucket(key) != _max_bucket;
    }

    size_type count(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) == _max_bucket ? 0 : 1;
    }

    std::pair<iterator, iterator> equal_range(const KeyT& key) noexcept
    {
        iterator found = find(key);
        if (found == end())
            return { found, found };
        else
            return { found, std::next(found) };
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    bool try_get(const KeyT& key, ValueT& val) const
    {
        const auto bucket = find_filled_bucket(key);
        const auto find = bucket != _max_bucket;
        if (find) {
            val = EMH_VAL(_pairs, bucket);
        }
        return find;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& key) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _max_bucket ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Const version of the above
    ValueT* try_get(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _max_bucket ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Convenience function.
    ValueT get_or_return_default(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _max_bucket ? ValueT() : EMH_VAL(_pairs, bucket);
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key, const ValueT& value) noexcept
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        const auto find = EMH_BUCKET(_pairs, bucket) == INACTIVE;
        if (find) {
            EMH_NEW(key, value, bucket);
        }
        return { {this, bucket}, find };
    }

    template<typename K, typename V>
    inline std::pair<iterator, bool> do_assign(K&& key, V&& value)
    {
        const auto bucket = find_or_allocate(key);
        const auto empty = EMH_BUCKET(_pairs, bucket) == INACTIVE;
        if (empty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket);
        } else {
            EMH_VAL(_pairs, bucket) = std::move(value);
        }
        return { {this, bucket}, empty };
    }

//    std::pair<iterator, bool> insert(const value_pair& value) { return insert(value.first, value.second); }
    std::pair<iterator, bool> insert(KeyT&& key, ValueT&& value) noexcept
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        const auto find = EMH_BUCKET(_pairs, bucket) == INACTIVE;
        if (find) {
            EMH_NEW(std::move(key), std::move(value), bucket);
        }
        return { {this, bucket}, find };
    }

    inline std::pair<iterator, bool> insert(const std::pair<KeyT, ValueT>& p)
    {
        return insert(p.first, p.second);
    }

    inline std::pair<iterator, bool> insert(std::pair<KeyT, ValueT>&& p)
    {
        return insert(std::move(p.first), std::move(p.second));
    }

#if 0
    template <typename Iter>
    void insert(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            emplace(*begin);
        }
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        reserve(ilist.size() + _num_filled);
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin) {
            emplace(*begin);
        }
    }
#endif

    template <typename Iter>
    void insert2(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            if (try_insert_mainbucket(begin->first, begin->second) == INACTIVE) {
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
        for (; begin != end; ++begin) {
            insert_unique(*begin);
        }
    }

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        EMH_NEW(key, value, bucket);
        return bucket;
    }

    uint32_t insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        EMH_NEW(std::move(key), std::move(value), bucket);
        return bucket;
    }

    inline uint32_t insert_unique(std::pair<KeyT, ValueT>&& p)
    {
        return insert_unique(std::move(p.first), std::move(p.second));
    }

    inline uint32_t insert_unique(std::pair<KeyT, ValueT>& p)
    {
        return insert_unique(p.first, p.second);
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(std::forward<Args>(args)...);
    }

    //no any optimize for position
    template <class... Args>
    iterator emplace_hint(const_iterator position, Args&&... args)
    {
        return insert(std::forward<Args>(args)...).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args)
    {
        return insert(k, std::forward<Args>(args)...).first;
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    uint32_t try_insert_mainbucket(const KeyT& key, const ValueT& value)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket != INACTIVE)
            return INACTIVE;

        EMH_NEW(key, value, bucket);
        return bucket;
    }

    std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& value) { return do_assign(key, std::move(value)); };
    std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& value) { return do_assign(std::move(key), std::move(value)); }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (EMH_BUCKET(_pairs, bucket) == INACTIVE) {
            EMH_NEW(key, value, bucket);
            return ValueT();
        } else {
            ValueT old_value(value);
            std::swap(EMH_VAL(_pairs, bucket), old_value);
            return old_value;
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        auto bucket = find_or_allocate(key);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (EMH_BUCKET(_pairs, bucket) == INACTIVE) {
            if (EMH_UNLIKELY(check_expand_need()))
                bucket = find_unique_bucket(key);

            EMH_NEW(key, std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    ValueT& operator[](KeyT&& key)
    {
        auto bucket = find_or_allocate(key);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (EMH_BUCKET(_pairs, bucket) == INACTIVE) {
            if (EMH_UNLIKELY(check_expand_need()))
                bucket = find_unique_bucket(key);

            EMH_NEW(std::move(key), std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    // -------------------------------------------------------
    //reset last_bucket collision bucket to bucket
    void erase_coll(uint32_t bucket)
    {
        if (_last_colls < _num_buckets) {
            _last_colls = _num_buckets;
        }

        const auto last_bucket = ++ _last_colls;
        if (bucket == last_bucket)
            return;

        const auto last_next = EMH_BUCKET(_pairs, last_bucket);
        auto& key = EMH_KEY(_pairs, last_bucket);
        const auto main_bucket = hash_bucket(key);
        const auto prev_bucket = find_prev_bucket(main_bucket, last_bucket);

        new(_pairs + bucket) PairT(_pairs[last_bucket]);
        EMH_BUCKET(_pairs, bucket) = last_next != last_bucket ? last_next : bucket;
        EMH_BUCKET(_pairs, prev_bucket) = bucket;

        if (is_notriviall_destructable())
            _pairs[last_bucket].~PairT();
        EMH_BUCKET(_pairs, last_bucket) = INACTIVE;
    }

    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key) noexcept
    {
        const auto bucket = erase_key(key);
        if (bucket == INACTIVE)
            return 0;

        clear_bucket(bucket);
#if EMH_HIGH_LOAD
        if (bucket > _last_colls)
            erase_coll(bucket);
#endif
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
        //move last bucket to current
#if EMH_HIGH_LOAD
        if (bucket > _last_colls)
            erase_coll(bucket);
#endif

        //erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? ++it : it;
    }

    constexpr bool is_notriviall_destructable()
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
        return !(std::is_trivially_copy_assignable<KeyT>::value && std::is_trivially_copy_assignable<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clearkv()
    {
        for (uint32_t bucket = 0; _num_filled > 0; ++bucket) {
            if (EMH_BUCKET(_pairs, bucket) != INACTIVE) {
                clear_bucket(bucket);
            }
        }
        _last_colls = _max_bucket - 1;
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (is_notriviall_destructable() || sizeof(PairT) > EMH_CACHE_LINE_SIZE || _num_filled < _max_bucket / 4)
            clearkv();
        else
            memset(_pairs, INACTIVE, sizeof(_pairs[0]) * _max_bucket);
        _num_filled = 0;
        _last_colls = _max_bucket - 1;
    }

    void shrink_to_fit()
    {
        rehash(_num_filled);
    }

    /// Make room for this many elements
    bool reserve(size_t num_elems)
    {
        const auto required_buckets = (uint32_t)(num_elems * _loadlf >> 27);
#if EMH_HIGH_LOAD
        if (EMH_LIKELY(required_buckets + 1 < _max_bucket))
#else
        if (EMH_LIKELY(required_buckets < _mask))
#endif
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    /// Make room for this many elements
    void rehash(uint32_t required_buckets)
    {
        if (required_buckets < _num_filled)
            return ;

        uint32_t num_buckets = _num_filled > 65536 ? (1 << 16) : 8;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        const auto old_max_bucket = _max_bucket;

#ifdef EMH_HIGH_LOAD
        _max_bucket = num_buckets * 5 / 4;
#else
        _max_bucket = num_buckets;
#endif

        //assert(num_buckets > _num_filled);
        auto new_pairs = (PairT*)malloc((2 + _max_bucket) * sizeof(PairT));
        auto old_num_filled  = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_pairs = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _last_colls  = num_buckets;
        _pairs       = new_pairs;

#ifdef EMH_HIGH_LOAD
        _last_colls = _max_bucket - 1;
#endif

#if EMH_SAFE_HASH
        if (_hash_inter == 0 && old_num_filled > 10000) {
            //adjust hash function if bad hash function, alloc more memory
            uint32_t mbucket = 0;
            for (uint32_t src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
                if (EMH_BUCKET(old_pairs, src_bucket) == src_bucket)
                    mbucket ++;
            }
            if (mbucket * 2 < old_num_filled) {
                _hash_inter = 1;
                while (mbucket < (old_num_filled >> _hash_inter))
                    _hash_inter ++;
            }
        }
#endif

        if (sizeof(PairT) <= EMH_CACHE_LINE_SIZE / 2) {
            memset(_pairs, INACTIVE, sizeof(_pairs[0]) * _max_bucket);
        } else {
            for (uint32_t bucket = 0; bucket < _max_bucket; bucket++)
                EMH_BUCKET(_pairs, bucket) = INACTIVE;
        }
        EMH_BUCKET(_pairs, _max_bucket) = EMH_BUCKET(_pairs, _max_bucket + 1) = 0;

        uint32_t collision = 0;
        //set all main bucket first
        for (uint32_t src_bucket = 0; src_bucket < old_max_bucket && old_num_filled > 0; src_bucket++) {
            auto bucket = EMH_BUCKET(old_pairs, src_bucket);
            if (bucket == INACTIVE)
                continue;

            const auto& key = EMH_KEY(old_pairs, src_bucket);
#if 0
            if (try_insert_mainbucket(key, EMH_VAL(old_pairs, src_bucket)) == INACTIVE) {
                EMH_BUCKET(old_pairs, collision++) = src_bucket;
            }
#else
            auto&& old_pair = old_pairs[src_bucket];
            const auto main_bucket = hash_bucket(key);
            auto& next_bucket = EMH_BUCKET(_pairs, main_bucket);
            if (next_bucket == INACTIVE) {
                new(_pairs + main_bucket) PairT(std::move(old_pair));
                if (is_notriviall_destructable())
                    old_pair.~PairT();
                next_bucket = main_bucket;
                _num_filled ++;
            } else {
                //move collision bucket to head for better cache performance
                EMH_BUCKET(old_pairs, collision++) = src_bucket;
            }
#endif
        }

        _num_filled += collision;
        //reset all collisions bucket
        for (uint32_t colls = 0; colls < collision; colls++) {
            const auto src_bucket = EMH_BUCKET(old_pairs, colls);
            const auto main_bucket = hash_bucket(EMH_KEY(old_pairs, src_bucket));
            auto&& old_pair = old_pairs[src_bucket];

            auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
            //check current bucket_key is in main bucket or not
            if (next_bucket != main_bucket)
                next_bucket = find_last_bucket(next_bucket);
            //find a new empty and link it to tail
            auto new_bucket = EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);
            new(_pairs + new_bucket) PairT(std::move(old_pair));
            if (is_notriviall_destructable())
                old_pair.~PairT();
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        }

#if EMH_REHASH_LOG
        if (_num_filled > 100000 && _hash_inter > 0) {
            auto mbucket = _num_filled - collision;
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/_hash_inter/aver_size/K.V/pack/collision = %u/%u/%.2lf/%s.%s/%zd/%.2lf%%",
                    _num_filled, _hash_inter, (double)_num_filled / mbucket, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(_pairs[0]), (collision * 100.0 / _num_filled));
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
        return reserve(_num_filled);
    }

    void clear_bucket(uint32_t bucket)
    {
        if (is_notriviall_destructable())
            _pairs[bucket].~PairT();
        EMH_BUCKET(_pairs, bucket) = INACTIVE;
        _num_filled --;
    }

    uint32_t erase_key(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto eqkey = _eq(key, EMH_KEY(_pairs, bucket));
        if (next_bucket == bucket) {
            return eqkey ? bucket : INACTIVE;
         } else if (eqkey) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (is_copy_trivially())
                EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
            else
                EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);

            EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        }/* else if (EMH_UNLIKELY(bucket != hash_bucket(EMH_KEY(_pairs, bucket))))
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

    uint32_t erase_bucket(const uint32_t bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto main_bucket = hash_bucket(EMH_KEY(_pairs, bucket));
        if (bucket == main_bucket) {
            if (bucket != next_bucket) {
                const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
                if (is_copy_trivially())
                    EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
                else
                    EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
                EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            }
            return next_bucket;
        }

        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        EMH_BUCKET(_pairs, prev_bucket) = (bucket == next_bucket) ? prev_bucket : next_bucket;
        return bucket;
    }

    // Find the bucket with this key, or return bucket size
    uint32_t find_filled_bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);

        if (next_bucket == INACTIVE)
            return _max_bucket;
        else if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket)
            return _max_bucket;
        //else if (EMH_UNLIKELY(bucket != (hash_bucket(bucket_key))))
        //    return _max_bucket;

        //find next from linked bucket
        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        return _max_bucket;
    }

    //main --> prev --> bucekt -->next --> new
    uint32_t kickout_bucket(const uint32_t main_bucket, const uint32_t bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket]));
        if (is_notriviall_destructable())
            _pairs[bucket].~PairT();
        if (next_bucket == bucket)
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        EMH_BUCKET(_pairs, bucket) = INACTIVE;
        EMH_BUCKET(_pairs, prev_bucket) = new_bucket;
        return bucket;
    }

/*
** inserts a new key into a hash table; first, check whether key's main
** bucket/position is free. If not, check whether colliding node/bucket is in its main
** position or not: if it is not, move colliding bucket to an empty place and
** put new key in its main position; otherwise (colliding bucket is in its main
** position), new key goes to an empty position.
*/
    uint32_t find_or_allocate(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE || _eq(key, bucket_key))
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(bucket_key);
        if (main_bucket != bucket)
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket == bucket)
            return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

#if EMH_LRU_SET
        auto prev_bucket = bucket;
#endif
        //find next linked bucket and check key
        while (true) {
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

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //find a new empty and link it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    uint32_t find_empty_bucket(uint32_t bucket_from)
    {
        const auto bucket1 = (bucket_from + 1) & _mask;
        if (EMH_BUCKET(_pairs, bucket1) == INACTIVE)
            return bucket1;

        const auto bucket2 = bucket1 + 1;
        if (EMH_BUCKET(_pairs, bucket2) == INACTIVE)
            return bucket2;

        //constexpr bool is_big = sizeof(PairT) > EMH_CACHE_LINE_SIZE / 2;
        //if (bucket_from > _num_buckets && INACTIVE == EMH_BUCKET(_pairs, _last_colls)) return _last_colls --;
#ifdef EMH_HIGH_LOAD
        if (INACTIVE == EMH_BUCKET(_pairs, _last_colls))
            return _last_colls --;
#endif

        //fibonacci an2 = an1 + an0 --> 1, 2, 3, 5, 8, 13, 21 ...
        //for (uint32_t last = 2, slot = 3; ; slot += last, last = slot - last) {
        for (uint32_t last = 3, step = (bucket_from + last) & _mask; ;step = (step + ++last) & _mask) {
            const auto bucket = step;
            if (EMH_BUCKET(_pairs, bucket) == INACTIVE)
                return bucket;

            const auto bucket2 = bucket + 1;
            if (EMH_BUCKET(_pairs, bucket2) == INACTIVE)
                return bucket2;

            if (last > 4) {
                const auto bucket3 = (step + _num_filled) & _mask;
                if (EMH_BUCKET(_pairs, bucket3) == INACTIVE)
                    return bucket3;
            }
        }
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
        const auto bucket = hash_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(EMH_KEY(_pairs, bucket));
        if (main_bucket != bucket)
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket != bucket)
            next_bucket = find_last_bucket(next_bucket);

        //find a new empty and link it to tail
        return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);
    }

    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    static inline uint64_t hash64(uint64_t key)
    {
#if __SIZEOF_INT128__
        __uint128_t r = key; r *= KC;
        return (uint64_t)(r >> 64) + (uint64_t)r;
#elif _WIN64
        uint64_t high;
        return _umul128(key, KC, &high) + high;
#elif 1
        uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
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
        return (uint32_t)hash64(key) & _mask;
#elif EMH_SAFE_HASH
        if (_hash_inter > 0) {
                return uint32_t(hash64(key) & _mask);
        }
        return _hasher(key) & _mask;
#elif EMH_IDENTITY_HASH
        return (key + (key >> (sizeof(UType) * 4))) & _mask;
#elif EMH_WYHASH64
        return wyhash64(key, KC);
#else
        return _hasher(key) & _mask;
#endif
    }

    template<typename UType, typename std::enable_if<std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint32_t hash_bucket(const UType& key) const
    {
#ifdef WYHASH_LITTLE_ENDIAN
        return wyhash(key.c_str(), key.size(),0x12345678) & _mask;
#elif EMH_BDKR_HASH
        uint32_t hash = 0;
        for (const auto c : key)
            hash = c + hash * 131;
        return hash & _mask;
#else
        return _hasher(key) & _mask;
#endif
    }

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value && !std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint32_t hash_bucket(const UType& key) const
    {
#ifdef EMH_FIBONACCI_HASH
        return _hasher(key) * KC & _mask;
#else
        return _hasher(key) & _mask;
#endif
    }

private:

    PairT*    _pairs;
    HashT     _hasher;
    EqT       _eq;
    uint32_t  _loadlf;
    uint32_t  _num_buckets;
    uint32_t  _mask;

    uint32_t  _max_bucket;
    uint32_t  _last_colls;
    uint32_t  _num_filled;
    uint32_t  _hash_inter;
};
} // namespace emhash
#if __cplusplus > 199711
//template <class Key, class Val> using emihash = emhash4::HashMap<Key, Val, std::hash<Key>>;
#endif

