#pragma once
// version 1.4.2
// https://github.com/ktprime/ktprime/blob/master/hash_set4.hpp
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


#include <cstring>
#include <string>
#include <cmath>
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

// likely/unlikely
#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define EMH_LIKELY(condition) __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition) condition
#    define EMH_UNLIKELY(condition) condition
#endif

#if _WIN32
#include <intrin.h>
#if _WIN64
#pragma intrinsic(_umul128)
#endif
#endif

namespace emhash9 {

constexpr uint32_t MASK_BIT = sizeof(uint8_t) * 8;
constexpr uint32_t SIZE_BIT = sizeof(size_t) * 8;
constexpr uint32_t INACTIVE = 0xFFFFFFFF;

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

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashSet
{
public:
    typedef  HashSet<KeyT, HashT, EqT> htype;
    typedef  std::pair<KeyT, uint32_t> PairT;
    static constexpr bool bInCacheLine = sizeof(PairT) < 64 * 2 / 3;

    typedef size_t   size_type;
    typedef KeyT     value_type;
    typedef KeyT&    reference;
    typedef KeyT*    pointer;
    typedef const KeyT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef KeyT                      value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        iterator() { }
        iterator(htype* hash_set, uint32_t bucket) : _set(hash_set), _bucket(bucket) { init(); }
        void init()
        {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _set->bucket_count()) {
                _bmask = *(size_t*)((size_t*)_set->_bitmask + _from / SIZE_BIT);
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
            return {_set, old_index};
        }

        reference operator*() const
        {
            return _set->_pairs[_bucket].first;
        }

        pointer operator->() const
        {
            return &(_set->_pairs[_bucket].first);
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
#ifdef EMH_SAFE_ITER
            auto bitmask = _set->_bitmask;

            do {
                _bucket++;
            } while (bitmask[_bucket / MASK_BIT] & (1 << (_bucket % MASK_BIT)));
#else
            _bmask &= _bmask - 1;
            if (EMH_LIKELY(_bmask != 0)) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            while (_bmask == 0 && EMH_LIKELY(_from < _set->bucket_count()))
                _bmask = ~*(size_t*)((size_t*)_set->_bitmask + (_from += SIZE_BIT) / SIZE_BIT);

            _bucket = _bmask != 0 ? _from + CTZ(_bmask) : _set->bucket_count();
#endif
        }

    public:
        htype*   _set;
        size_t   _bmask;
        uint32_t _bucket;
        uint32_t _from;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef KeyT                      value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        const_iterator() { }
        const_iterator(const iterator& it) : _set(it._set), _bucket(it._bucket) { init(); }
        const_iterator(const htype* hash_set, uint32_t bucket) : _set(hash_set), _bucket(bucket) { init(); }

        void init()
        {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _set->bucket_count()) {
                _bmask = *(size_t*)((size_t*)_set->_bitmask + _from / SIZE_BIT);
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
            return {_set, old_index};
        }

        reference operator*() const
        {
            return _set->_pairs[_bucket].first;
        }

        pointer operator->() const
        {
            return &(_set->_pairs[_bucket].first);
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
#ifdef EMH_SAFE_ITER
            auto bitmask = _set->_bitmask;

            do {
                _bucket++;
            } while (bitmask[_bucket / MASK_BIT] & (1 << (_bucket % MASK_BIT)));
#else
            _bmask &= _bmask - 1;
            if (EMH_LIKELY(_bmask != 0)) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            while (_bmask == 0 && EMH_LIKELY(_from < _set->bucket_count()))
                _bmask = ~*(size_t*)((size_t*)_set->_bitmask + (_from += SIZE_BIT) / SIZE_BIT);

            _bucket = _bmask != 0 ? _from + CTZ(_bmask) : _set->bucket_count();
#endif
        }

    public:
        const htype* _set;
        size_t   _bmask;
        uint32_t _bucket;
        uint32_t _from;
    };

    // ------------------------------------------------------------------------

    void init(uint32_t bucket, float load_factor = 0.90f)
    {
        _num_buckets = 0;
        _mask = 0;
        _pairs = nullptr;
        _bitmask = nullptr;
        _num_filled = 0;
        max_load_factor(load_factor);
        reserve(bucket);
    }

    HashSet(uint32_t bucket = 4, float load_factor = 0.90f)
    {
        init(bucket, load_factor);
    }

    HashSet(const HashSet& other)
    {
        _pairs = (PairT*)malloc((2 + other._num_buckets) * sizeof(PairT) + other._num_buckets / 8 + sizeof(size_t));
        clone(other);
    }

    HashSet(HashSet&& other)
    {
        _num_buckets = _num_filled = 0;
        _pairs = nullptr;
        swap(other);
    }

    HashSet(std::initializer_list<value_type> il, size_t n = 8)
    {
        init((uint32_t)il.size());
        for (auto begin = il.begin(); begin != il.end(); ++begin)
            insert(*begin);
    }

    HashSet& operator=(const HashSet& other)
    {
        if (this == &other)
            return *this;
//            htype(other).swap(*this);

        if (!std::is_trivial<KeyT>::value)
            clearkv();

        if (_num_buckets != other._num_buckets) {
            free(_pairs);
            _pairs = (PairT*)malloc((2 + other._num_buckets) * sizeof(PairT) + other._num_buckets / 8 + sizeof(size_t));
        }

        clone(other);
        return *this;
    }

    HashSet& operator=(HashSet&& other)
    {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashSet()
    {
        if (isno_triviall_destructable())
            clearkv();

        free(_pairs);
    }

    void clone(const HashSet& other)
    {
        _hasher      = other._hasher;
//        _eq          = other._eq;
        _num_buckets = other._num_buckets;
        _num_filled  = other._num_filled;
        _mask        = other._mask;
        _loadlf      = other._loadlf;
        _last        = other._last;
        _bitmask     = decltype(_bitmask)((uint8_t*)_pairs + ((uint8_t*)other._bitmask - (uint8_t*)other._pairs));
        auto opairs  = other._pairs;

#if __cplusplus >= 201402L || _MSC_VER > 1600 || __clang__
        if (std::is_trivially_copyable<KeyT>::value)
#else
        if (std::is_pod<KeyT>::value)
#endif
            memcpy(_pairs, opairs, _num_buckets * sizeof(PairT));
        else {
            for (uint32_t bucket = 0; bucket < _num_buckets; bucket++) {
                auto next_bucket = _pairs[bucket].second = opairs[bucket].second;
                if (next_bucket != INACTIVE)
                    new(_pairs + bucket) PairT(opairs[bucket]);
            }
        }
        memcpy(_pairs + _num_buckets, opairs + _num_buckets, 2 * sizeof(PairT) + _num_buckets / 8 + sizeof(size_t));
    }

    inline void swap(HashSet& other)
    {
        std::swap(_hasher, other._hasher);
//      std::swap(_eq, other._eq);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
        std::swap(_last, other._last);
        std::swap(_loadlf, other._loadlf);
        std::swap(_bitmask, other._bitmask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        if (empty())
            return {this, _num_buckets};

        uint32_t bucket = 0;
        while (_bitmask[bucket / MASK_BIT] & (1 << (bucket % MASK_BIT))) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator cbegin() const
    {
        if (empty())
            return {this, _num_buckets};

        uint32_t bucket = 0;
        while (_bitmask[bucket / MASK_BIT] & (1 << (bucket % MASK_BIT))) {
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

    size_type bucket_main() const
    {
        auto bucket_size = 0;
        for (uint32_t bucket = 0; bucket < _num_buckets; ++bucket) {
            if (_pairs[bucket].second == bucket)
                bucket_size ++;
        }
        return bucket_size;
    }

#ifdef EMH_STATIS
    //Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key) & _mask;
        const auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE)
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        const auto& bucket_key = _pairs[bucket].first;
        return (hash_bucket(bucket_key) & _mask) + 1;
    }

    //Returns the number of elements in bucket n.
    size_type bucket_size(const uint32_t bucket) const
    {
        auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE)
            return 0;

        next_bucket = hash_bucket(_pairs[bucket].first) & _mask;
        uint32_t bucket_size = 1;

        //iterator each item in current main bucket
        while (true) {
            const auto nbucket = _pairs[next_bucket].second;
            if (nbucket == next_bucket) {
                break;
            }
            bucket_size++;
            next_bucket = nbucket;
        }
        return bucket_size;
    }

    size_type get_main_bucket(const uint32_t bucket) const
    {
        auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto& bucket_key = _pairs[bucket].first;
        const auto main_bucket = hash_bucket(bucket_key) & _mask;
        return main_bucket;
    }

    int get_cache_info(uint32_t bucket, uint32_t next_bucket) const
    {
        auto pbucket = reinterpret_cast<size_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<size_t>(&_pairs[next_bucket]);
        if (pbucket / 64 == pnext / 64)
            return 0;
        auto diff = pbucket > pnext ? (pbucket - pnext) : pnext - pbucket;
        if (diff < 127 * 64)
            return diff / 64 + 1;
        return 127;
    }

    int get_bucket_info(const uint32_t bucket, uint32_t steps[], const uint32_t slots) const
    {
        auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE)
            return -1;

        const auto& bucket_key = _pairs[bucket].first;
        const auto main_bucket = hash_bucket(bucket_key) & _mask;
        if (main_bucket != bucket)
            return 0;
        else if (next_bucket == bucket)
            return 1;

        steps[get_cache_info(bucket, next_bucket) % slots] ++;
        uint32_t ibucket_size = 2;
        //find a new empty and linked it to tail
        while (true) {
            const auto nbucket = _pairs[next_bucket].second;
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
        puts("============== buckets size ration =========");
        for (uint32_t i = 0; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            const auto bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;
            sumb += bucketsi;
            sumn += bucketsi * i;
            collision += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            printf("  %2u  %8u  %.2lf  %.2lf\n", i, bucketsi, bucketsi * 100.0 * i / _num_filled, sumn * 100.0 / _num_filled);
        }

        puts("========== collision miss ration ===========");
        for (uint32_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
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
    }
#endif

    // ------------------------------------------------------------

    inline iterator find(const KeyT& key) noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    inline const_iterator find(const KeyT& key) const noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    inline bool contains(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    inline size_type count(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) == _num_buckets ? 0 : 1;
    }

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (_pairs[bucket].second == INACTIVE) {
            new_key(key, bucket);
            return { {this, bucket}, true };
        } else {
            return { {this, bucket}, false };
        }
    }

    std::pair<iterator, bool> insert(KeyT&& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (_pairs[bucket].second == INACTIVE) {
            new_key(std::move(key), bucket);
            return { {this, bucket}, true };
        } else {
            return { {this, bucket}, false };
        }
    }

#if 0
    template <typename Iter>
    inline void insert(Iter begin, Iter end)
    {
        reserve(end - begin + _num_filled);
        for (; begin != end; ++begin) {
            insert(*begin);
        }
    }
#endif

    void insert(std::initializer_list<value_type> ilist)
    {
        reserve((uint32_t)ilist.size() + _num_filled);
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin) {
            insert(*begin);
        }
    }

    template <typename Iter>
    inline void insert(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(end - begin + _num_filled);
        for (; begin != end; ++begin) {
            if (try_insert_mainbucket(*begin) == INACTIVE) {
                std::swap(*begin, *citend++);
            }
        }

        for (; citbeg != citend; ++citbeg) {
            auto& key = *citbeg;
            const auto bucket = find_or_allocate(key);
            if (_pairs[bucket].second == INACTIVE) {
                new_key(key, bucket);
            }
        }
    }

    template <typename Iter>
    inline void insert_unique(Iter begin, Iter end)
    {
        reserve(end - begin + _num_filled);
        for (; begin != end; ++begin) {
            insert_unique(*begin);
        }
    }

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(const KeyT& key)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        new_key(key, bucket);
        return bucket;
    }

    uint32_t insert_unique(KeyT&& key)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        new_key(std::move(key), bucket);
        return bucket;
    }

    //not
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
    std::pair<iterator, bool> try_emplace(const value_type& k)
    {
        return insert(k).first;
    }
    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    uint32_t try_insert_mainbucket(const KeyT& key)
    {
        auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE) {
            new_key(key, bucket);
            return bucket;
        } else if(_eq(key, _pairs[bucket].first))
            return bucket;

        return INACTIVE;
    }

    void insert_or_assign(const KeyT& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        // Check if inserting a new value rather than overwriting an old entry
        if (_pairs[bucket].second == INACTIVE) {
            new_key(key, bucket);
        } else {
            _pairs[bucket].second = bucket;
        }
    }

    template<typename T>
    inline void new_key(const T& key, uint32_t bucket)
    {
        if (!std::is_trivial<KeyT>::value)
            new(_pairs + bucket) PairT(key, bucket);
        else {
            _pairs[bucket].first  = key;
            _pairs[bucket].second = bucket;
        }
        _num_filled ++;
        _bitmask[bucket / MASK_BIT] &= ~(1 << (bucket % MASK_BIT));
    }

    template<typename T>
    inline void new_key(T&& key, uint32_t bucket)
    {
        if (!std::is_trivial<KeyT>::value)
            new(_pairs + bucket) PairT(std::forward<T>(key), bucket);
        else {
            _pairs[bucket].first  = key;
            _pairs[bucket].second = bucket;
        }
        _num_filled ++;
        _bitmask[bucket / MASK_BIT] &= ~(1 << (bucket % MASK_BIT));
    }

    void clear_bucket(uint32_t bucket)
    {
        if (isno_triviall_destructable())
            _pairs[bucket].~PairT();
        _pairs[bucket].second = INACTIVE;
        _num_filled --;
        _bitmask[bucket / MASK_BIT] |= 1 << (bucket % MASK_BIT);
    }

    // -------------------------------------------------------
    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key)
    {
        const auto bucket = erase_key(key);
        if (bucket == INACTIVE)
            return 0;

        clear_bucket(bucket);
        return 1;
    }

    iterator erase(const_iterator cit)
    {
        iterator it(this, cit._bucket);
        const auto bucket = erase_bucket(it._bucket);
        //move last bucket to current
        clear_bucket(bucket);

        //erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? ++it : it;
    }

    void _erase(const_iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
    }

    static constexpr bool isno_triviall_destructable()
    {
#if __cplusplus > 201103L || _MSC_VER > 1600 || __clang__
        return !(std::is_trivially_destructible<KeyT>::value);
#else
        return !(std::is_trivial<KeyT>::value);
#endif
    }

    void clearkv()
    {
        for (uint32_t bucket = 0; _num_filled > 0; ++bucket) {
            if (_pairs[bucket].second != INACTIVE)
                clear_bucket(bucket);
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (isno_triviall_destructable() || !bInCacheLine || _num_filled < _num_buckets / 4)
            clearkv();
        else {
            memset(_pairs, INACTIVE, sizeof(_pairs[0]) * _num_buckets);
            memset(_bitmask, INACTIVE, _num_buckets / 8);
        }
        _last = 0;
        _num_filled = 0;
    }

    void shrink_to_fit()
    {
        rehash(_num_filled);
    }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems)
    {
        const auto required_buckets = (uint32_t)(num_elems * _loadlf >> 27);
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

private:
    void rehash(uint32_t required_buckets)
    {
        if (required_buckets < _num_filled)
            return;

        uint32_t num_buckets = _num_filled > 65536 ? (1u << 16) : 8u;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        _mask        = num_buckets - 1;
#if EMH_HIGH_LOAD
        if (num_buckets % 64 == 0)
            num_buckets += num_buckets / 8;
#endif

        const auto num_byte = num_buckets / 8;
        auto new_pairs = (PairT*)malloc((2 + num_buckets) * sizeof(PairT) + num_byte + sizeof(size_t));
        //TODO: throwOverflowError
        auto old_num_filled  = _num_filled;
        auto old_pairs = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _last        = 0;

        if (bInCacheLine)
            memset(new_pairs, INACTIVE, sizeof(_pairs[0]) * num_buckets);
        else
            for (uint32_t bucket = 0; bucket < num_buckets; bucket++)
                new_pairs[bucket].second = INACTIVE;
        memset(new_pairs + num_buckets, 0, sizeof(PairT) * 2);

        //set bit mask
        _bitmask     = decltype(_bitmask)(new_pairs + 2 + num_buckets);
        memset(_bitmask, INACTIVE, num_byte);
        memset((char*)_bitmask + num_byte, 0, sizeof(size_t));

        _pairs       = new_pairs;
        for (uint32_t src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
            auto&& opair = old_pairs[src_bucket];
            if (opair.second == INACTIVE)
                continue;

            const auto bucket = find_unique_bucket(opair.first);
            new_key(std::move(opair.first), bucket);
            if (isno_triviall_destructable())
                opair.first.~KeyT();
        }

#if EMH_REHASH_LOG
        if (_num_filled > EMH_REHASH_LOG) {
            const auto mbucket = bucket_main();
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/type/sizeof/coll|load_factor = %u/%s/%zd/%.2lf%%|%.2f",
                    _num_filled, typeid(value_type).name(), sizeof(PairT), 100. - 100.0 * mbucket / _num_filled, load_factor());
#ifdef EMH_LOG
            static uint32_t ihashs = 0;
            EMH_LOG() << "|rhash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
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

    uint32_t erase_key(const KeyT& key)
    {
        const auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto eqkey = _eq(key, _pairs[bucket].first);
        if (next_bucket == bucket) {
            return eqkey ? bucket : INACTIVE;
         } else if (eqkey) {
            const auto nbucket = _pairs[next_bucket].second;
            if (std::is_trivial<KeyT>::value)
                _pairs[bucket].first = _pairs[next_bucket].first;
            else
                std::swap(_pairs[bucket].first, _pairs[next_bucket].first);
            _pairs[bucket].second = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        }/* else if (EMH_UNLIKELY(bucket != hash_bucket(_pairs[bucket].first)) & _mask)
            return INACTIVE;
**/
        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = _pairs[next_bucket].second;
            if (_eq(key, _pairs[next_bucket].first)) {
                _pairs[prev_bucket].second = (nbucket == next_bucket) ? prev_bucket : nbucket;
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
        const auto next_bucket = _pairs[bucket].second;
        const auto main_bucket = hash_bucket(_pairs[bucket].first) & _mask;
        if (bucket == main_bucket) {
            if (bucket != next_bucket) {
                const auto nbucket = _pairs[next_bucket].second;
                if (std::is_trivial<KeyT>::value)
                    _pairs[bucket].first = _pairs[next_bucket].first;
                else
                    std::swap(_pairs[bucket].first, _pairs[next_bucket].first);
                _pairs[bucket].second = (nbucket == next_bucket) ? bucket : nbucket;
            }
            return next_bucket;
        }

        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        _pairs[prev_bucket].second = (bucket == next_bucket) ? prev_bucket : next_bucket;
        return bucket;
    }

    // Find the bucket with this key, or return bucket size
    uint32_t find_filled_bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = _pairs[bucket].second;
        const auto& bucket_key = _pairs[bucket].first;
        if (next_bucket == INACTIVE)
            return _num_buckets;
//        else if (bucket != (hash_bucket(bucket_key) & _mask))
//            return _num_buckets;
        else if (_eq(key, bucket_key))
            return bucket;
        else if (next_bucket == bucket)
            return _num_buckets;

        while (true) {
            if (_eq(key, _pairs[next_bucket].first))
                return next_bucket;

            const auto nbucket = _pairs[next_bucket].second;
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
    uint32_t kickout_bucket(const uint32_t main_bucket, const uint32_t bucket)
    {
        const auto next_bucket = _pairs[bucket].second;
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket]));

        _bitmask[new_bucket / MASK_BIT] &= ~(1 << (new_bucket % MASK_BIT));

        _pairs[prev_bucket].second = new_bucket;
        if (next_bucket == bucket)
            _pairs[new_bucket].second = new_bucket;
        if (isno_triviall_destructable())
            _pairs[bucket].~PairT();
        _pairs[bucket].second = INACTIVE;
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
        const auto bucket = hash_bucket(key) & _mask;
        const auto& bucket_key = _pairs[bucket].first;
        auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE || _eq(key, bucket_key))
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(bucket_key) & _mask;
        if (main_bucket != bucket)
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket == bucket)
            return _pairs[next_bucket].second = find_empty_bucket(next_bucket);

        //find next linked bucket and check key
        while (true) {
            if (_eq(key, _pairs[next_bucket].first)) {
#if EMH_LRU_SET
                std::swap(_pairs[bucket].first, _pairs[next_bucket].first);
                return bucket;
#else
                return next_bucket;
#endif
            }

            const auto nbucket = _pairs[next_bucket].second;
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //find a new empty and link it to tail
//        auto find_bucket = next_bucket > main_bucket + 8 ? main_bucket + 2 : next_bucket;
        const auto new_bucket = find_empty_bucket(next_bucket);
        return _pairs[next_bucket].second = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    uint32_t find_empty_simple(uint32_t bucket_from) const noexcept
    {
        if (_pairs[++bucket_from].second == INACTIVE || _pairs[++bucket_from].second == INACTIVE)
            return bucket_from;

        //fibonacci an2 = an1 + an0 --> 1, 2, 3, 5, 8, 13, 21 ...
//        for (uint32_t last = 2, slot = 3; ; slot += last, last = slot - last) {
        for (uint32_t last = 2, slot = 2; ; slot += ++last ) {
            auto bucket1 = (bucket_from + slot) & _mask;
            if (_pairs[bucket1].second == INACTIVE || _pairs[++bucket1].second == INACTIVE)
                return bucket1;

            if (last > 4) {
                auto bucket3 = (bucket1 + _num_filled) & _mask;
                if (_pairs[bucket3].second == INACTIVE || _pairs[++bucket3].second == INACTIVE)
                    return bucket3;
            }
        }
    }

    // key is not in this map. Find a place to put it.
    uint32_t find_empty_bucket(const uint32_t bucket_from)
    {
#if __x86_64__ || _M_X64
        const auto boset = bucket_from % 8;
        const auto begin = (uint8_t*)_bitmask + bucket_from / 8;
#else
        const auto boset = bucket_from % SIZE_BIT;
        const auto begin = (size_t*)_bitmask + bucket_from / SIZE_BIT;
#endif

        const auto bmask = *(size_t*)(begin) >> boset;
        if (EMH_LIKELY(bmask != 0)) {
            const auto offset = CTZ(bmask);
//            if (EMH_UNLIKELY(offset > 8 + 256 / sizeof(PairT)) && begin[0] > 0)
//            return bucket_from - boset + CTZ(begin[0]);
            return bucket_from + offset;
        }

        const auto qmask = _mask / SIZE_BIT;
        {
            const auto next2 = (bucket_from + 2 * SIZE_BIT) & qmask;
            const auto bmask2 = *((size_t*)_bitmask + next2);
            if (EMH_LIKELY(bmask2 != 0))
                return next2 * SIZE_BIT + CTZ(bmask2);
        }

        for (; ;) {
            const auto bmask2 = *((size_t*)_bitmask + _last);
            if (bmask2 != 0)
                return _last * SIZE_BIT + CTZ(bmask2);

            const auto next1 = qmask - _last;
            const auto bmask1 = *((size_t*)_bitmask + next1);
            if (bmask1 != 0)
                return next1 * SIZE_BIT + CTZ(bmask1);

            _last = (_last + 1) & qmask;
        }

        return 0;
    }

    uint32_t find_last_bucket(uint32_t main_bucket) const
    {
        auto next_bucket = _pairs[main_bucket].second;
        if (next_bucket == main_bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = _pairs[next_bucket].second;
            if (nbucket == next_bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    uint32_t find_prev_bucket(uint32_t main_bucket, const uint32_t bucket) const
    {
        auto next_bucket = _pairs[main_bucket].second;
        if (next_bucket == bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = _pairs[next_bucket].second;
            if (nbucket == bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    uint32_t find_unique_bucket(const KeyT& key)
    {
        const auto bucket = hash_bucket(key) & _mask;
        auto next_bucket = _pairs[bucket].second;
        if (next_bucket == INACTIVE)
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(_pairs[bucket].first) & _mask;
        if (main_bucket != bucket)
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket != bucket)
            next_bucket = find_last_bucket(next_bucket);

        //find a new empty and link it to tail
        return _pairs[next_bucket].second = find_empty_simple(next_bucket);
    }

    static inline uint64_t hash64(uint64_t key)
    {
#if __SIZEOF_INT128__
        constexpr uint64_t k = UINT64_C(11400714819323198485);
        __uint128_t r = key; r *= k;
        return (uint32_t)(r >> 64) + (uint32_t)r;
#elif _WIN64
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
#elif EMH_IDENTITY_HASH
        return key + (key >> (sizeof(UType) * 4));
#else
        return _hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint32_t hash_bucket(const UType& key) const
    {
#ifdef WYHASH_LITTLE_ENDIAN
        return wyhash(key.c_str(), key.size(), key.size());
#elif EMH_BDKR_HASH
        uint32_t hash = 0;
        if (key.size() < 256) {
            for (const auto c : key)
                hash = c + hash * 131;
        } else {
            for (size_t i = 0, j = 1; i < key.size(); i += j++)
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
        return (_hasher(key) * 11400714819323198485ull);
#else
        return _hasher(key);
#endif
    }

private:

    //the first cache line packed
    PairT*    _pairs;
    uint8_t*  _bitmask;
    HashT     _hasher;
    EqT       _eq;
    uint32_t  _loadlf;

    uint32_t  _num_buckets;
    uint32_t  _mask;

    uint32_t  _last;
    uint32_t  _num_filled;
};
} // namespace emhash
#if __cplusplus >= 201103L
template <class Key, typename Hash = std::hash<Key>> using em_hash_set = emhash9::HashSet<Key, Hash>;
#endif

