// emhash5::HashMap for C++11
// version 1.5.8
// https://github.com/ktprime/ktprime/blob/master/hash_table5.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2022 Huang Yuanbing & bailuzhou AT 163.com
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

#include <cstring>
#include <string>
#include <cstdlib>
#include <type_traits>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>
#include <algorithm>

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
    #undef  EMH_KV
    #undef  EMH_BUCKET
    #undef  EMH_NEW
    #undef  EMH_EMPTY
    #undef  EMH_PREVET
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMH_LIKELY(condition)   __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition)   condition
#    define EMH_UNLIKELY(condition) condition
#endif

#if EMH_CACHE_LINE_SIZE < 32
    #define EMH_CACHE_LINE_SIZE 64
#endif

#ifndef EMH_DEFAULT_LOAD_FACTOR
#define EMH_DEFAULT_LOAD_FACTOR 0.88f
#endif

#define EMH_KEY(p,n)     p[n].first
#define EMH_VAL(p,n)     p[n].second
#define EMH_KV(p,n)      p[n]

#define EMH_INDEX(i,n)   i[n]
#define EMH_BUCKET(i,n)  i[n].bucket
#define EMH_HSLOT(i,n)   i[n].slot
#define EMH_SLOT(i,n)    (i[n].slot & _mask)
#define EMH_PREVET(i,n)  i[n].slot

#define EMH_EQHASH(n, key_hash) ((size_type)key_hash & ~_mask) == (_index[n].slot & ~_mask)
#undef  EMH_NEW
#define EMH_NEW(key, value, bucket, key_hash) new(_pairs + _num_filled) PairT(key, value); _index[bucket] = {bucket, _num_filled++ | (key_hash & ~_mask)}

#define EMH_EMPTY(i, n) (0 > (int)i[n].bucket)

namespace emhash8 {
#if SLOT_BITS < 10 || SLOT_BITS >= 30
constexpr uint32_t SLOT_BITS  = 28;
#endif
constexpr uint32_t SLOT_KMASK = ~((1 << SLOT_BITS) - 1);
constexpr uint32_t SLOT_SMASK = ~SLOT_KMASK;

constexpr uint32_t INACTIVE = 0xAAAAAAAA;
constexpr uint32_t END      = 0-1;
constexpr uint32_t EAD      = 2;

/// A cache-friendly hash table with open addressing, linear/qua probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
public:
    typedef HashMap<KeyT, ValueT, HashT, EqT> htype;
    typedef std::pair<KeyT,ValueT>            value_type;
    typedef std::pair<KeyT, ValueT>           PairT;

public:
    typedef KeyT   key_type;
    typedef ValueT val_type;
    typedef ValueT mapped_type;

    typedef uint32_t     size_type;
    typedef PairT&       reference;
    typedef const PairT& const_reference;

    struct Index
    {
        size_type bucket;
        size_type slot;
    };

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t       difference_type;
        typedef PairT                value_type;

        typedef PairT*               pointer;
        typedef PairT&               reference;

        iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _slot(bucket) { }

        iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            auto old_slot = _slot;
            goto_next_element();
            return {_map, old_slot};
        }

        iterator& operator--()
        {
            _slot ++;
            return *this;
        }

        iterator operator--(int)
        {
            return {_map, ++_slot};
        }

        reference operator*() const
        {
            return _map->EMH_KV(_pairs, _slot);
        }

        pointer operator->() const
        {
            return &(_map->EMH_KV(_pairs, _slot));
        }

        bool operator==(const iterator& rhs) const
        {
            return _slot == rhs._slot;
        }

        bool operator!=(const iterator& rhs) const
        {
            return _slot != rhs._slot;
        }

        size_type bucket() const
        {
            return _slot;
        }

    private:
        inline void goto_next_element()
        {
            _slot --;
        }

    public:
        const htype* _map;
        size_type _slot;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t       difference_type;
        typedef PairT                value_type;

        typedef PairT*               pointer;
        typedef PairT&               reference;

        const_iterator(const iterator& proto) : _map(proto._map), _slot(proto._slot) { }
        const_iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _slot(bucket) { }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            auto old_slot = _slot;
            goto_next_element();
            return {_map, old_slot};
        }

        const_iterator& operator--()
        {
            _slot ++;
            return *this;
        }

        const_iterator operator--(int)
        {
            return {_map, ++_slot};
        }

        reference operator*() const
        {
            return _map->EMH_KV(_pairs, _slot);
        }

        pointer operator->() const
        {
            return &(_map->EMH_KV(_pairs, _slot));
        }

        bool operator==(const const_iterator& rhs) const
        {
            return _slot == rhs._slot;
        }

        bool operator!=(const const_iterator& rhs) const
        {
            return _slot != rhs._slot;
        }

        size_type bucket() const
        {
            return _slot;
        }

    private:
        inline void goto_next_element()
        {
            _slot --;
        }

    public:
        const htype* _map;
        size_type _slot;
    };

    void init(size_type bucket, float lf = EMH_DEFAULT_LOAD_FACTOR)
    {
        _pairs = nullptr;
        _index = nullptr;
        _mask  = _num_buckets = 0;
        _num_filled = 0;
        _ehead = 0;
        max_load_factor(lf);
        reserve(bucket, true);
    }

    HashMap(size_type bucket = 2, float lf = EMH_DEFAULT_LOAD_FACTOR)
    {
        init(bucket, lf);
    }

    HashMap(const HashMap& other)
    {
        _pairs = alloc_bucket(other._num_buckets);
//        _index = alloc_index(other._num_buckets);
        _index = (Index*)(_pairs + other._num_buckets);
        clone(other);
    }

    HashMap(HashMap&& other)
    {
        init(0);
        *this = std::move(other);
    }

    HashMap(std::initializer_list<value_type> ilist)
    {
        init((uint32_t)ilist.size());
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            do_insert(it->first, it->second);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this == &other)
            return *this;

        if (is_triviall_destructable())
            clearkv();

        if (_num_buckets < other._num_buckets || _num_buckets > 2 * other._num_buckets) {
            free(_pairs);
            _pairs = alloc_bucket(other._num_buckets);
//            free(_index); _index = alloc_index(other._num_buckets);
            _index = (Index*)(_pairs + other._num_buckets);
        }

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
        _num_filled  = other._num_filled;
        _loadlf      = other._loadlf;
        _last        = other._last;
        _mask        = other._mask;
        _ehead       = other._ehead;

        auto opairs  = other._pairs;
        memcpy(_index, other._index, (_num_buckets + EAD) * sizeof(Index));

        if (is_copy_trivially()) {
            memcpy(_pairs, opairs, _num_filled * sizeof(PairT));
        } else {
            for (size_type slot = 0; slot < _num_filled; slot++)
                new(_pairs + slot) PairT(opairs[slot]);
        }
    }

    void swap(HashMap& other)
    {
        //      std::swap(_eq, other._eq);
        std::swap(_hasher, other._hasher);
        std::swap(_pairs, other._pairs);
        std::swap(_index, other._index);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_loadlf, other._loadlf);
        std::swap(_last, other._last);
        std::swap(_mask, other._mask);
        std::swap(_ehead, other._ehead);
    }

    // -------------------------------------------------------------
    inline iterator first() const
    {
        if (_num_filled == 0)
            return end();

        return {this, 0};
    }

    inline iterator last() const
    {
        return {this, _num_filled - 1};
    }

    iterator begin()
    {
        return last();
    }

    const_iterator cbegin() const
    {
        return last();
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    inline iterator end()
    {
        return {this, END};
    }

    inline const_iterator cend() const
    {
        return {this, END};
    }

    const_iterator end() const
    {
        return cend();
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
        if (value < 1.0-1e-4 && value > 0.2f)
            _loadlf = (uint32_t)((1 << 27) / value);
    }

    constexpr size_type max_size() const
    {
        return (1ull << (sizeof(size_type) * 8 - 2));
    }

    constexpr size_type max_bucket_count() const
    {
        return max_size();
    }

#ifdef EMH_STATIS
    //Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key);
        const auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0)
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        return hash_main(bucket) + 1;
    }

    //Returns the number of elements in bucket n.
    size_type bucket_size(const size_type bucket) const
    {
        auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0)
            return 0;

        next_bucket = hash_main(bucket);
        size_type ibucket_size = 1;

        //iterator each item in current main bucket
        while (true) {
            const auto nbucket = EMH_BUCKET(_index, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    size_type get_main_bucket(const size_type bucket) const
    {
        auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0)
            return INACTIVE;

        return hash_main(bucket);
    }

    size_type get_diss(size_type bucket, size_type next_bucket, const size_type slots) const
    {
        auto pbucket = reinterpret_cast<uint64_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<uint64_t>(&_pairs[next_bucket]);
        if (pbucket / EMH_CACHE_LINE_SIZE == pnext / EMH_CACHE_LINE_SIZE)
            return 0;
        size_type diff = pbucket > pnext ? (pbucket - pnext) : (pnext - pbucket);
        if (diff / EMH_CACHE_LINE_SIZE < slots - 1)
            return diff / EMH_CACHE_LINE_SIZE + 1;
        return slots - 1;
    }

    int get_bucket_info(const size_type bucket, size_type steps[], const size_type slots) const
    {
        auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0)
            return -1;

        const auto main_bucket = hash_main(bucket);
        if (next_bucket == main_bucket)
            return 1;
        else if (main_bucket != bucket)
            return 0;

        steps[get_diss(bucket, next_bucket, slots)] ++;
        size_type ibucket_size = 2;
        //find a new empty and linked it to tail
        while (true) {
            const auto nbucket = EMH_BUCKET(_index, next_bucket);
            if (nbucket == next_bucket)
                break;

            steps[get_diss(nbucket, next_bucket, slots)] ++;
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return (int)ibucket_size;
    }

    void dump_statics() const
    {
        const uint32_t slots = 128;
        size_type buckets[slots + 1] = {0};
        size_type steps[slots + 1]   = {0};
        for (size_type bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, slots);
            if (bsize > 0)
                buckets[bsize] ++;
        }

        size_type sumb = 0, collision = 0, sumc = 0, finds = 0, sumn = 0;
        puts("============== buckets size ration =========");
        for (size_type i = 0; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            const auto bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;
            sumb += bucketsi;
            sumn += bucketsi * i;
            collision += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            printf("  %2u  %8u  %2.2lf|  %.2lf\n", i, bucketsi, bucketsi * 100.0 * i / _num_filled, sumn * 100.0 / _num_filled);
        }

        puts("========== collision miss ration ===========");
        for (size_type i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
            sumc += steps[i];
            if (steps[i] <= 2)
                continue;
            printf("  %2u  %8u  %.2lf  %.2lf\n", i, steps[i], steps[i] * 100.0 / collision, sumc * 100.0 / collision);
        }

        if (sumb == 0)  return;
        printf("    _num_filled/bucket_size/packed collision/cache_miss/hit_find = %u/%.2lf/%zd/ %.2lf%%/%.2lf%%/%.2lf\n",
                _num_filled, _num_filled * 1.0 / sumb, sizeof(PairT), (collision * 100.0 / _num_filled), (collision - steps[0]) * 100.0 / _num_filled, finds * 1.0 / _num_filled);
        assert(sumn == _num_filled);
        assert(sumc == collision);
        puts("============== buckets size end =============");
    }
#endif

    // ------------------------------------------------------------

    iterator find(const KeyT& key) noexcept
    {
        return {this, find_filled_slot(key)};
    }

    const_iterator find(const KeyT& key) const noexcept
    {
        return {this, find_filled_slot(key)};
    }

    ValueT& at(const KeyT& key)
    {
        const auto slot = find_filled_slot(key);
        //throw
        return EMH_VAL(_pairs, slot);
    }

    const ValueT& at(const KeyT& key) const
    {
        const auto slot = find_filled_slot(key);
        //throw
        return EMH_VAL(_pairs, slot);
    }

    bool contains(const KeyT& key) const noexcept
    {
        return find_filled_slot(key) != END;
    }

    size_type count(const KeyT& key) const noexcept
    {
        return find_filled_slot(key) == END ? 0 : 1;
        //return find_sorted_bucket(key) == END ? 0 : 1;
        //return find_hash_bucket(key) == END ? 0 : 1;
    }

    std::pair<iterator, iterator> equal_range(const KeyT& key)
    {
        const auto found = find(key);
        if (found.second == END)
            return { found, found };
        else
            return { found, std::next(found) };
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    bool try_get(const KeyT& key, ValueT& val) const
    {
        const auto slot = find_filled_slot(key);
        const auto found = slot != END;
        if (found) {
            val = EMH_VAL(_pairs, slot);
        }
        return found;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& key) noexcept
    {
        const auto slot = find_filled_slot(key);
        return slot != END ? &EMH_VAL(_pairs, slot) : nullptr;
    }

    /// Const version of the above
    ValueT* try_get(const KeyT& key) const noexcept
    {
        const auto slot = find_filled_slot(key);
        return slot != END ? &EMH_VAL(_pairs, slot) : nullptr;
    }

    /// set value if key exist
    bool try_set(const KeyT& key, const ValueT& value) noexcept
    {
        const auto slot = find_filled_slot(key);
        if (slot == END)
            return false;

        EMH_VAL(_pairs, slot) = value;
        return true;
    }

    /// set value if key exist
    bool try_set(const KeyT& key, ValueT&& value) noexcept
    {
        const auto slot = find_filled_slot(key);
        if (slot == END)
            return false;

        EMH_VAL(_pairs, slot) = std::move(value);
        return true;
    }

    /// Convenience function.
    ValueT get_or_return_default(const KeyT& key) const noexcept
    {
        const auto slot = find_filled_slot(key);
        return slot == END ? ValueT() : EMH_VAL(_pairs, slot);
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
        const auto key_hash = (size_type)hash_key(key);
        const auto bucket = find_or_allocate(key, key_hash);
        const auto empty = EMH_EMPTY(_index, bucket);
        if (empty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket, key_hash);
        }

        const auto slot = EMH_SLOT(_index, bucket);
        return { {this, slot}, empty };
    }

    template<typename K, typename V>
    inline std::pair<iterator, bool> do_assign(K&& key, V&& value)
    {
        const auto key_hash = (size_type)hash_key(key);
        const auto bucket = find_or_allocate(key, key_hash);
        const auto empty = EMH_EMPTY(_index, bucket);
        if (empty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket, key_hash);
        } else {
            EMH_VAL(_pairs, EMH_SLOT(_index, bucket)) = std::move(value);
        }

        const auto slot = EMH_SLOT(_index, bucket);
        return { {this, slot}, empty };
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
        reserve(ilist.size() + _num_filled, false);
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin)
            do_insert(begin->first, begin->second);
    }

#if 0
    template <typename Iter>
    void insert(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled, false);
        for (; begin != end; ++begin) {
            emplace(*begin);
        }
    }

    template <typename Iter>
    void insert2(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(std::distance(begin, end) + _num_filled, false);
        for (; begin != end; ++begin) {
            if (try_insert_mainbucket(begin->first, begin->second) < 0) {
                std::swap(*begin, *citend++);
            }
        }

        for (; citbeg != citend; ++citbeg)
            insert(*citbeg);
    }
#endif

    template <typename Iter>
    void insert_unique(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled, false);
        for (; begin != end; ++begin) {
            insert_unique(*begin);
        }
    }

    /// Same as above, but contains(key) MUST be false
    size_type insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        const auto key_hash = (size_type)hash_key(key);
        auto bucket = find_unique_bucket(key, key_hash);
        EMH_NEW(key, value, bucket, key_hash);
        return bucket;
    }

    size_type insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        const auto key_hash = (size_type)hash_key(key);
        auto bucket = find_unique_bucket(key, key_hash);
        EMH_NEW(std::move(key), std::move(value), bucket, key_hash);
        return bucket;
    }

    size_type insert_unique(value_type&& pairT)
    {
        const auto key_hash = (size_type)hash_key(pairT.first);
        auto bucket = find_unique_bucket(pairT.first, key_hash);
        EMH_NEW(std::move(pairT.first), std::move(pairT.second), bucket, key_hash);
        return bucket;
    }

    inline size_type insert_unique(value_type& p)
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
        check_expand_need();
        return do_insert(std::forward<Args>(args)...).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args)
    {
        check_expand_need();
        return do_insert(k, std::forward<Args>(args)...);
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& value)
    {
        check_expand_need();
        return do_assign(key, std::move(value));
    }

    std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        return do_assign(std::move(key), std::move(value));
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        const auto key_hash = (size_type)hash_key(key);
        const auto bucket = find_or_allocate(key, key_hash);
        if (EMH_EMPTY(_index, bucket)) {
            EMH_NEW(key, value, bucket, key_hash);
            return ValueT();
        } else {
            const auto slot = EMH_SLOT(_index, bucket);
            ValueT old_value(value);
            std::swap(EMH_VAL(_pairs, slot), old_value);
            return old_value;
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();
        const auto key_hash = (size_type)hash_key(key);
        const auto bucket = find_or_allocate(key, key_hash);
        if (EMH_EMPTY(_index, bucket)) {
            /* Check if inserting a new value rather than overwriting an old entry */
            EMH_NEW(key, std::move(ValueT()), bucket, key_hash);
        }

        const auto slot = EMH_SLOT(_index, bucket);
        return EMH_VAL(_pairs, slot);
    }

    ValueT& operator[](KeyT&& key)
    {
        check_expand_need();
        const auto key_hash = (size_type)hash_key(key);
        const auto bucket = find_or_allocate(key, key_hash);
        if (EMH_EMPTY(_index, bucket)) {
            EMH_NEW(std::move(key), std::move(ValueT()), bucket, key_hash);
        }

        const auto slot = EMH_SLOT(_index, bucket);
        return EMH_VAL(_pairs, slot);
    }

    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key)
    {
        auto slot = find_filled_slot(key);
        if (slot == END)
            return 0;

        erase_slot(slot);
        return 1;
    }

    //iterator erase(const_iterator begin_it, const_iterator end_it)
    iterator erase(const const_iterator& cit)
    {
        const auto slot = erase_slot(cit._slot);
        iterator it(this, cit._slot);
        return ++it;
    }

    void _erase(const const_iterator& it)
    {
        erase_slot(it._slot);
    }

    static constexpr bool is_triviall_destructable()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    static constexpr bool is_copy_trivially()
    {
#if __cplusplus >= 201103L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clearkv()
    {
        while (_num_filled --)
            _pairs[_num_filled].~PairT();
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (_num_filled > 0 || _ehead > 0)
            memset(_index, INACTIVE, sizeof(_index[0]) * _num_buckets);

        if (is_triviall_destructable())
            clearkv();

        _last = _num_filled = 0;
        _ehead = 0;
    }

    void shrink_to_fit(const float min_factor = EMH_DEFAULT_LOAD_FACTOR / 4)
    {
        if (load_factor() < min_factor && bucket_count() > 10) //safe guard
            rehash(_num_filled);
    }

    void set_empty()
    {
        auto prev = 0;
        for (int32_t bucket = 1; bucket < _num_buckets; ++bucket) {
            if (EMH_EMPTY(_index, bucket)) {
                if (prev != 0) {
                    EMH_PREVET(_index, bucket) = prev;
                    EMH_BUCKET(_index, prev) = -bucket;
                }
                else
                    _ehead = bucket;
                prev = bucket;
            }
        }

        EMH_PREVET(_index, _ehead) = prev;
        EMH_BUCKET(_index, prev) = 0-_ehead;
        _ehead = 0-EMH_BUCKET(_index, _ehead);
    }

    void clear_empty()
    {
        auto prev = EMH_PREVET(_index, _ehead);
        while (prev != _ehead) {
            EMH_BUCKET(_index, prev) = INACTIVE;
            prev = EMH_PREVET(_index, prev);
        }
        EMH_BUCKET(_index, _ehead) = INACTIVE;
        _ehead = 0;
    }

    //prev-ehead->next
    uint32_t pop_empty(const uint32_t bucket)
    {
        const auto prev_bucket = EMH_PREVET(_index, bucket);
        const int next_bucket = 0-EMH_BUCKET(_index, bucket);

        EMH_PREVET(_index, next_bucket) = prev_bucket;
        EMH_BUCKET(_index, prev_bucket) = -next_bucket;

        _ehead = next_bucket;
        return bucket;
    }

    //ehead->bucket->next
    void push_empty(const int32_t bucket)
    {
        const int next_bucket = 0-EMH_BUCKET(_index, _ehead);
        assert(next_bucket > 0);

        EMH_PREVET(_index, bucket) = _ehead;
        EMH_BUCKET(_index, bucket) = -next_bucket;

        EMH_PREVET(_index, next_bucket) = bucket;
        EMH_BUCKET(_index, _ehead) = -bucket;
        //        _ehead = bucket;
    }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems, bool force)
    {
#if EMH_HIGH_LOAD == 0
        const auto required_buckets = (uint32_t)(num_elems * _loadlf >> 27);
        if (EMH_LIKELY(required_buckets < _mask)) // && !force
            return false;

#elif EMH_HIGH_LOAD
        const auto required_buckets = (uint32_t)(num_elems + num_elems * 1 / 9);
        if (EMH_LIKELY(required_buckets < _mask))
            return false;

        else if (_num_buckets < 16 && _num_filled < _num_buckets)
            return false;

        else if (_num_buckets > EMH_HIGH_LOAD) {
            if (_ehead == 0) {
                set_empty();
                return false;
            } else if (/*_num_filled + 100 < _num_buckets && */EMH_BUCKET(_index, _ehead) != 0-_ehead) {
                return false;
            }
        }
#endif

#if EMH_STATIS
        if (_num_filled > 1'000'000) dump_statics();
#endif

        //assert(required_buckets < max_size());
        rehash(required_buckets + 2);
        return true;
    }

    static inline PairT* alloc_bucket(size_type num_buckets)
    {
        auto new_pairs = (char*)malloc(num_buckets * sizeof(PairT) + (EAD + num_buckets) * sizeof(Index));
        return (PairT *)(new_pairs);
    }

    static inline Index* alloc_index(size_type num_buckets)
    {
        auto new_index = (char*)malloc((EAD + num_buckets) * sizeof(Index));
        return (Index *)(new_index);
    }

    bool reserve(uint32_t required_buckets)
    {
        if (_num_filled != required_buckets || _num_filled == 0)
            return reserve(required_buckets, true);

        _ehead = _last = 0;

        std::sort(_pairs, _pairs + _num_filled, [this](const PairT & l, const PairT & r) {
           const auto hashl = hash_key(l.first), hashr = hash_key(r.first);
           auto diff = int64_t((hashl & _mask) - (hashr & _mask));
           if (diff != 0)
              return diff < 0;
#if 0
           return hashl < hashr;
#else
           return l.first < r.first;
#endif
        });

        memset(_index, INACTIVE, sizeof(_index[0]) * _num_buckets);
        for (size_type slot = 0; slot < _num_filled; slot++) {
            const auto& key = EMH_KEY(_pairs, slot);
            const auto key_hash = (size_type)hash_key(key);
            const auto bucket = key_hash & _mask;
            auto& next_bucket = EMH_BUCKET(_index, bucket);
            if ((int)next_bucket < 0)
                EMH_INDEX(_index, bucket) = {1, slot | (key_hash & ~_mask)};
            else
               next_bucket ++;
        }
        return true;
    }

    void rehash(size_type required_buckets)
    {
        if (required_buckets < _num_filled)
            return;

        uint32_t num_buckets = _num_filled > (1u << 16) ? (1u << 16) : 4u;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto new_pairs = (PairT*)alloc_bucket(num_buckets);
        auto new_index = (Index*)(new_pairs + num_buckets);
        auto old_pairs = _pairs;
#if EMH_REHASH_LOG
        auto last = _last;
        size_type collision = 0;
#endif

        _ehead = 0;
        _last = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;

        memset(new_index, INACTIVE, sizeof(new_index[0]) * num_buckets);
        memset(new_index + num_buckets, 0, sizeof(new_index[0]) * EAD);

        //if (is_copy_trivially())
        //    memcpy(new_pairs, _pairs, _num_filled * sizeof(PairT));

#ifdef EMH_SORT
        std::sort(old_pairs, old_pairs + _num_filled, [this](const PairT & l, const PairT & r) {
            const auto hashl = hash_key(l.first), hashr = hash_key(r.first);
            auto diff = int64_t((hashl & _mask) - (hashr & _mask));
            if (diff != 0)
               return diff < 0;
            return hashl < hashr;
//          return l.first < r.first;
        });
#endif

        _pairs       = new_pairs;
        _index       = new_index;
        for (size_type slot = 0; slot < _num_filled; slot++) {
            const auto& key = EMH_KEY(old_pairs, slot);
            const auto key_hash = (size_type)hash_key(key); //? old_index[slot].slot
            const auto bucket = find_unique_bucket(key, key_hash);
            EMH_INDEX(_index, bucket) = {bucket, slot | (key_hash & ~_mask)};

            //if (!is_copy_trivially())
            new(_pairs + slot) PairT(std::move(old_pairs[slot]));
#if EMH_REHASH_LOG
            if (bucket != hash_main(bucket))
                collision ++;
#endif
            if (is_triviall_destructable())
                old_pairs[slot].~PairT();
        }

#if EMH_REHASH_LOG
        if (_num_filled > EMH_REHASH_LOG) {
            auto mbucket = _num_filled - collision;
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/aver_size/K.V/pack/collision|last = %u/%.2lf/%s.%s/%zd|%.2lf%%,%.2lf%%",
                    _num_filled, double (_num_filled) / mbucket, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(_pairs[0]), collision * 100.0 / _num_filled, last * 100.0 / _num_buckets);
#ifdef EMH_LOG
            static size_type ihashs = 0; EMH_LOG() << "hash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
#else
            puts(buff);
#endif
        }
#endif

        free(old_pairs);
    }

private:
    // Can we fit another element?
    inline bool check_expand_need()
    {
        return reserve(_num_filled, false);
    }

    size_type slot_to_bucket(const size_type slot) const
    {
        const auto& key = EMH_KEY(_pairs, slot);
        return find_filled_bucket(key); //TODO
    }

    size_type erase_slot(const size_type slot)
    {
        const auto sbucket = slot_to_bucket(slot);
        const auto ebucket = erase_bucket(sbucket);
        const auto last_slot = --_num_filled;
        if (slot != last_slot) {
            const auto last_bucket = slot_to_bucket(last_slot);
            if (is_triviall_destructable())
                 std::swap(EMH_KV(_pairs, slot), EMH_KV(_pairs, last_slot));
            else
                EMH_KV(_pairs, slot) = EMH_KV(_pairs, last_slot);
            //EMH_SLOT(_index, last_bucket) = slot;
            EMH_HSLOT(_index, last_bucket) = slot | (EMH_HSLOT(_index, last_bucket) & ~_mask);
        }

        if (is_triviall_destructable())
            _pairs[last_slot].~PairT();

        EMH_INDEX(_index, ebucket) = {INACTIVE, END};

#if EMH_HIGH_LOAD
        if (_ehead) {
            if (10 * _num_filled < 8 * _num_buckets)
                clear_empty();
            else if (ebucket)
                push_empty(ebucket);
        }
#endif

        return last_slot;
    }

    size_type erase_bucket(const size_type bucket)
    {
        const auto next_bucket = EMH_BUCKET(_index, bucket);
        const auto main_bucket = hash_main(bucket);
        if (bucket == main_bucket) {
            if (main_bucket != next_bucket) {
                const auto nbucket = EMH_BUCKET(_index, next_bucket);
                EMH_INDEX(_index, main_bucket) = {
                    (nbucket == next_bucket) ? main_bucket : nbucket,
                    EMH_HSLOT(_index, next_bucket)
                };
            }
            return next_bucket;
        }

        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        EMH_BUCKET(_index, prev_bucket) = (bucket == next_bucket) ? prev_bucket : next_bucket;
        return bucket;
    }

    // Find the slot with this key, or return bucket size
    size_type find_filled_bucket(const KeyT& key) const
    {
        const auto key_hash = hash_key(key);
        const auto bucket = key_hash & _mask;
        auto next_bucket  = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0)
            return END;

        if (EMH_EQHASH(bucket, key_hash)) {
            const auto slot = EMH_SLOT(_index, bucket);
            if (EMH_LIKELY(_eq(key, EMH_KEY(_pairs, slot))))
            return bucket;
        }
        else if (next_bucket == bucket)
            return END;

        while (true) {
            if (EMH_EQHASH(next_bucket, key_hash)) {
                const auto slot = EMH_SLOT(_index, next_bucket);
                if (EMH_LIKELY(_eq(key, EMH_KEY(_pairs, slot))))
                return next_bucket;
            }

            const auto nbucket = EMH_BUCKET(_index, next_bucket);
            if (nbucket == next_bucket)
                return END;
            next_bucket = nbucket;
        }
        return 0;
    }

    // Find the slot with this key, or return bucket size
    size_type find_filled_slot(const KeyT& key) const
    {
        const auto key_hash = hash_key(key);
        const auto bucket = key_hash & _mask;
        auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0)
            return END;

        if (EMH_EQHASH(bucket, key_hash)) {
            const auto slot = EMH_SLOT(_index, bucket);
            if (EMH_LIKELY(_eq(key, EMH_KEY(_pairs, slot))))
            return slot;
        } else if (next_bucket == bucket)
            return END;

        while (true) {
            if (EMH_EQHASH(next_bucket, key_hash)) {
                const auto slot = EMH_SLOT(_index, next_bucket);
                if (EMH_LIKELY(_eq(key, EMH_KEY(_pairs, slot))))
                return slot;
            }

            const auto nbucket = EMH_BUCKET(_index, next_bucket);
            if (nbucket == next_bucket)
                return END;
            next_bucket = nbucket;
        }
        return 0;
    }

    size_type find_hash_bucket(const KeyT& key) const
    {
        const auto hashk = hash_key(key);
        const auto bucket = hashk & _mask;
        const auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0)
            return END;

        auto slot = EMH_SLOT(_index, bucket);
        if (_eq(key, EMH_KEY(_pairs, slot++)))
            return slot;
        else if (next_bucket == bucket)
            return END;

        while (true) {
            const auto& okey = EMH_KEY(_pairs, slot++);
            if (_eq(key, okey))
                return slot;

            const auto hasho = hash_key(okey);
            if (hasho > hashk)
               break;
            else if ((hasho & _mask) != bucket)
               break;
            else if (slot >= _num_filled)
               break;
        }

        return END;
    }

    size_type find_sorted_bucket(const KeyT& key) const
    {
        const auto hashk = hash_key(key);
        const auto bucket = hashk & _mask;
        auto slot  = (int)(EMH_SLOT(_index, bucket)); //TODO
        if (slot < 0 /**|| key < EMH_KEY(_pairs, slot)*/)
            return END;

        const auto slots = EMH_BUCKET(_index, bucket);
        if (_eq(key, EMH_KEY(_pairs, slot)))
            return slot;
        else if (slots == 1)
            return END;

#if 1
        if (key < EMH_KEY(_pairs, slot) || key > EMH_KEY(_pairs, slots + slot - 1))
            return END;
#endif

        for (int i = 0; i < slots; i++) {
            const auto& okey = EMH_KEY(_pairs, slot + i);
            if (_eq(key, okey))
                return slot;
#if 0
            else if (okey > key)
                return END;
#endif
        }

        return END;
    }

    //kick out bucket and find empty to occpuy
    //it will break the orgin link and relnik again.
    //before: main_bucket-->prev_bucket --> bucket   --> next_bucket
    //atfer : main_bucket-->prev_bucket --> (removed)--> new_bucket--> next_bucket
    size_type kickout_bucket(const size_type obmain, const size_type bucket)
    {
        const auto next_bucket = EMH_BUCKET(_index, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(obmain, bucket);

        const auto oslot = EMH_HSLOT(_index, bucket);
        if (next_bucket == bucket)
            EMH_INDEX(_index, new_bucket) = {new_bucket, oslot};
        else
            EMH_INDEX(_index, new_bucket) = {next_bucket, oslot};

        EMH_BUCKET(_index, prev_bucket) = new_bucket;
        EMH_BUCKET(_index, bucket) = INACTIVE;

        return bucket;
    }

/*
** inserts a new key into a hash table; first, check whether key's main
** bucket/position is free. If not, check whether colliding node/bucket is in its main
** position or not: if it is not, move colliding bucket to an empty place and
** put new key in its main position; otherwise (colliding bucket is in its main
** position), new key goes to an empty position.
*/
    size_type find_or_allocate(const KeyT& key, uint64_t key_hash)
    {
        const auto bucket = key_hash & _mask;
        auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0) {
#if EMH_HIGH_LOAD
            if (next_bucket != INACTIVE)
                pop_empty(bucket);
#endif
            return bucket;
        }

        const auto slot = EMH_SLOT(_index, bucket);
        const auto& bucket_key = EMH_KEY(_pairs, slot);
        if (EMH_EQHASH(bucket, key_hash))
            if (EMH_LIKELY(_eq(key, bucket_key)))
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto obmain = hash_bucket(bucket_key);
        if (obmain != bucket)
            return kickout_bucket(obmain, bucket);
        else if (next_bucket == bucket)
            return EMH_BUCKET(_index, next_bucket) = find_empty_bucket(next_bucket);

        //find next linked bucket and check key
        while (true) {
            const auto slot = EMH_SLOT(_index, next_bucket);
            if (EMH_UNLIKELY(EMH_EQHASH(next_bucket, key_hash))) {
                if (EMH_LIKELY(_eq(key, EMH_KEY(_pairs, slot))))
                return next_bucket;
            }

            const auto nbucket = EMH_BUCKET(_index, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //find a new empty and link it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return EMH_BUCKET(_index, next_bucket) = new_bucket;
    }

    size_type find_unique_bucket(const KeyT& key, uint64_t key_hash)
    {
        const auto bucket = key_hash & _mask;
        auto next_bucket = EMH_BUCKET(_index, bucket);
        if ((int)next_bucket < 0) {
#if EMH_HIGH_LOAD
            if (next_bucket != INACTIVE)
                pop_empty(bucket);
#endif
            return bucket;
        }

        //check current bucket_key is in main bucket or not
        const auto obmain = hash_main(bucket);
        if (EMH_UNLIKELY(obmain != bucket))
            return kickout_bucket(obmain, bucket);
        else if (EMH_UNLIKELY(next_bucket != bucket))
            next_bucket = find_last_bucket(next_bucket);

        //find a new empty and link it to tail
        return EMH_BUCKET(_index, next_bucket) = find_empty_bucket(next_bucket);
    }

/***
  Different probing techniques usually provide a trade-off between memory locality and avoidance of clustering.
Since Robin Hood hashing is relatively resilient to clustering (both primary and secondary), linear probing¡ªthe most cache-friendly alternative¡ªis typically used.

    It's the core algorithm of this hash map with highly optimization/benchmark.
normaly linear probing is inefficient with high load factor, it use a new 3-way linear
probing strategy to search empty slot. from benchmark even the load factor > 0.9, it's more 2-3 timer fast than
one-way seach strategy.

1. linear or quadratic probing a few cache line for less cache miss from input slot "bucket_from".
2. the first  seach  slot from member variant "_last", init with 0
3. the second search slot from calculated pos "(_num_filled + _last) & _mask", it's like a rand value
*/
    // key is not in this map. Find a place to put it.
    size_type find_empty_bucket(const size_type bucket_from)
    {
#if EMH_HIGH_LOAD
        if (_ehead)
            return pop_empty(_ehead);
#endif
        auto bucket = bucket_from;
        if (EMH_EMPTY(_index, ++bucket) || EMH_EMPTY(_index, ++bucket))
            return bucket;

#if 0
        constexpr auto linear_probe_length = std::max((unsigned int)(128 / sizeof(PairT)) + 2, 4u);//cpu cache line 64 byte,2-3 cache line miss
        auto offset = 2u;

#ifdef EMH_QUADRATIC
        for (; offset < linear_probe_length; offset += 2) {
            auto bucket1 = (bucket + offset) & _mask;
            if (EMH_EMPTY(_index, bucket1) || EMH_EMPTY(_index, ++bucket1))
                return bucket1;
        }
#else
        for (auto next = offset; offset < linear_probe_length; next += ++offset) {
            auto bucket1 = (bucket + next) & _mask;
            if (EMH_EMPTY(_index, bucket1) || EMH_EMPTY(_index, ++bucket1))
                return bucket1;
        }
#endif

        while (true) {
            _last &= _mask;
            if (EMH_EMPTY(_index, _last++) || EMH_EMPTY(_index, _last++))
                return _last++ - 1;

#if EMH_LINEAR3 || 1
            auto tail = _mask - (_last & _mask);
            if (EMH_EMPTY(_index, tail) || EMH_EMPTY(_index, ++tail))
                return tail;
#endif
            auto medium = (_num_filled + _last) & _mask;
            if (EMH_EMPTY(_index, medium) || EMH_EMPTY(_index, ++medium))
                return medium;
        }
#else
        constexpr auto linear_probe_length = sizeof(value_type) > EMH_CACHE_LINE_SIZE ? 3 : 4;
        for (size_type step = 2, slot = bucket + 1; ;slot += ++step) {
            auto bucket1 = slot & _mask;
            if (EMH_EMPTY(_index, bucket1) || EMH_EMPTY(_index, ++bucket1))
                return bucket1;

            if (step > linear_probe_length) {
                _last &= _mask;
                if (EMH_EMPTY(_index, ++_last) || EMH_EMPTY(_index, ++_last))
                    return _last ++;

#if 1
                auto tail = (_mask / 2 + _last) & _mask;
                if (EMH_EMPTY(_index, tail) || EMH_EMPTY(_index, ++tail))
                    return tail;
#endif
                auto medium = (_num_filled + _last) & _mask;
                if (EMH_EMPTY(_index, medium) || EMH_EMPTY(_index, ++medium))
                    return medium;
            }
        }
#endif
        return 0;
    }

    size_type find_last_bucket(size_type main_bucket) const
    {
        auto next_bucket = EMH_BUCKET(_index, main_bucket);
        if (next_bucket == main_bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_index, next_bucket);
            if (nbucket == next_bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    size_type find_prev_bucket(const size_type main_bucket, const size_type bucket) const
    {
        auto next_bucket = EMH_BUCKET(_index, main_bucket);
        if (next_bucket == bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_index, next_bucket);
            if (nbucket == bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    inline size_type hash_bucket(const KeyT& key) const
    {
        return (size_type)hash_key(key) & _mask;
    }

    inline size_type hash_main(const size_type bucket) const
    {
        const auto slot = EMH_SLOT(_index, bucket);
        return (size_type)hash_key(EMH_KEY(_pairs, slot)) & _mask;
    }

    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    static uint64_t hash64(uint64_t key)
    {
#if __SIZEOF_INT128__
        __uint128_t r = key; r *= KC;
        return (uint64_t)(r >> 64) + (uint64_t)r;
#elif _WIN64
        uint64_t high;
        return _umul128(key, KC, &high) + high;
#elif 1
        auto low  =  key;
        auto high = (key >> 32) | (key << 32);
        auto mix  = (0x94d049bb133111ebull * low + 0xbf58476d1ce4e5b9ull * high);
        return mix >> 32;
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
    inline uint64_t hash_key(const UType key) const
    {
#ifdef EMH_FIBONACCI_HASH
        return hash64(key);
#elif EMH_IDENTITY_HASH
        return (key + (key >> (sizeof(UType) * 4)));
#elif EMH_WYHASH64
        return wyhash64(key, KC);
#else
        return _hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint64_t hash_key(const UType& key) const
    {
#if EMH_BDKR_HASH
        uint64_t hash = 0; size_t i = 0;
        for (; i + sizeof(uint64_t) < key.size(); i += sizeof(uint64_t))
            hash += *(uint64_t*)(&key[i]) * KC;
        auto diff = int(key.size() - i) * 8;
        if (diff != 0)
            hash += ((*(uint64_t*)(&key[i]) & ((1ull << diff) - 1))) * KC;
        return hash;
#elif WYHASH_LITTLE_ENDIAN
        return wyhash(key.c_str(), key.size(), key.size());
#else
        return _hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value && !std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint64_t hash_key(const UType& key) const
    {
#ifdef EMH_FIBONACCI_HASH
        return _hasher(key) * KC;
#else
        return _hasher(key);
#endif
    }

private:
    PairT*    _pairs;
    Index*    _index;

    HashT     _hasher;
    EqT       _eq;

    size_type _num_buckets;
    size_type _loadlf;
    size_type _mask;
    size_type _num_filled;
    size_type _last;
    size_type _ehead;
};
} // namespace emhash
#if __cplusplus > 199711
//template <class Key, class Val> using emhash5 = ehmap<Key, Val, std::hash<Key>>;
#endif
