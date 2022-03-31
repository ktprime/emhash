// By Huang Yuanbing 2019
// bailuzhou@163.com
// https://github.com/ktprime/ktprime/blob/master/hash_table5.hpp

// LICENSE:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.


#pragma once

#include <cstdlib>
#include <iterator>
#include <utility>
#include <cstring>
#include <cassert>
#include <initializer_list>

#ifdef EMH_LOG
    #include "LogHelper.h"
#endif
#ifdef  EMH_KEY
    #undef  EMH_KEY
    #undef  BUCKET
    #undef  EMH_BUCKET
#endif

#if EMH_CACHE_LINE_SIZE < 32
    #define EMH_CACHE_LINE_SIZE  64
#endif


#define BUCKET(key)  int(_hasher(key) & _mask)
#define EMH_KEY(p,n)     p[n].first
#define EMH_BUCKET(p,n) p[n].second

namespace emhash5 {

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename HashT = std::hash<KeyT>>
class HashSet
{
    enum State
    {
        INACTIVE = -1, // Never been touched
    };
private:
    typedef  HashSet<KeyT, HashT> htype;
    typedef  std::pair<KeyT, int> PairT;

public:
    typedef size_t  size_type;
    typedef KeyT    value_type;
    typedef KeyT&   reference;
    typedef const KeyT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t                    difference_type;
        typedef size_t                    distance_type;
        typedef KeyT                      value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        iterator() { }

        iterator(htype* hash_set, uint32_t bucket) : _set(hash_set), _bucket(bucket)
        {
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
            return iterator(_set, old_index);
        }

        reference operator*() const
        {
            return _set->EMH_KEY(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_set->EMH_KEY(_pairs, _bucket));
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
            } while (_bucket < _set->_num_buckets && _set->EMH_BUCKET(_pairs, _bucket) == INACTIVE);
        }

    public:
        htype* _set;
        uint32_t  _bucket;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t                    difference_type;
        typedef size_t                    distance_type;
        typedef const KeyT                value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        const_iterator() { }

        const_iterator(iterator proto) : _set(proto._set), _bucket(proto._bucket)
        {
        }

        const_iterator(const htype* hash_set, uint32_t bucket) : _set(hash_set), _bucket(bucket)
        {
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
            return const_iterator(_set, old_index);
        }

        reference operator*() const
        {
            return _set->EMH_KEY(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_set->EMH_KEY(_pairs, _bucket));
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
            } while (_bucket < _set->_num_buckets && _set->EMH_BUCKET(_pairs, _bucket) == INACTIVE);
        }

    public:
        const htype* _set;
        uint32_t  _bucket;
    };

    // ------------------------------------------------------------------------

    void init()
    {
        _num_buckets = 0;
        _num_filled = 0;
        _mask = 0;
        _pairs = nullptr;
    }

    HashSet(int n = 4)
    {
        init();
        reserve(n);
    }

    HashSet(const HashSet& other)
    {
        init();
        reserve(other.size());
        for (auto begin = other.cbegin(); begin != other.cend(); ++begin)
            insert_unique(*begin);
    }

    HashSet(HashSet&& other)
    {
        init();
        reserve(1);
        *this = std::move(other);
    }

    HashSet(std::initializer_list<KeyT> il)
    {
        init();
        reserve(il.size());
        for (auto begin = il.begin(); begin != il.end(); ++begin)
            insert(*begin);
    }

    HashSet& operator=(const HashSet& other)
    {
        clear();
        reserve(other.size());
        for (auto begin = other.cbegin(); begin != other.cend(); ++begin)
            insert_unique(*begin);
        return *this;
    }

    HashSet& operator=(HashSet&& other)
    {
        swap(other);
        return *this;
    }

    ~HashSet()
    {
        clear();
        if (_pairs)
            free(_pairs);
    }

    void swap(HashSet& other)
    {
        std::swap(_hasher, other._hasher);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        uint32_t bucket = 0;
        while (bucket < _num_buckets && EMH_BUCKET(_pairs, bucket) == INACTIVE) {
            ++bucket;
        }
        return iterator(this, bucket);
    }

    const_iterator cbegin() const
    {
        uint32_t bucket = 0;
        while (bucket < _num_buckets && EMH_BUCKET(_pairs, bucket) == INACTIVE) {
            ++bucket;
        }
        return const_iterator(this, bucket);
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return iterator(this, _num_buckets);
    }

    const_iterator cend() const
    {
        return const_iterator(this, _num_buckets);
    }

    const_iterator end() const
    {
        return cend();
    }

    size_t size() const
    {
        return _num_filled;
    }

    bool empty() const
    {
        return _num_filled == 0;
    }

    // Returns the number of buckets.
    size_t bucket_count() const
    {
        return _num_buckets;
    }

    /// Returns average number of elements per bucket.
    float load_factor() const
    {
        return static_cast<float>(_num_filled) / static_cast<float>(_num_buckets);
    }

    void max_load_factor(float lf) const
    {
    }

    // ------------------------------------------------------------

    iterator find(const KeyT& key)
    {
        auto bucket = find_filled_bucket(key);
        if (bucket == INACTIVE) {
            return end();
        }
        return iterator(this, bucket);
    }

    const_iterator find(const KeyT& key) const
    {
        auto bucket = find_filled_bucket(key);
        if (bucket == INACTIVE) {
            return end();
        }
        return const_iterator(this, bucket);
    }

    bool contains(const KeyT& key) const
    {
        return find_filled_bucket(key) != INACTIVE;
    }

    size_t count(const KeyT& key) const
    {
        return find_filled_bucket(key) != INACTIVE ? 1 : 0;
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key)
    {
        auto bucket = find_or_allocate(key);
        if (EMH_BUCKET(_pairs, bucket) != INACTIVE) {
            return { iterator(this, bucket), false };
        }
        else {
            if (check_expand_need())
                bucket = find_main_bucket(key, true);

            new(_pairs + bucket) PairT(key, bucket);
            _num_filled++;
            return { iterator(this, bucket), true };
        }
    }

    /// Insert an element, unless it already exists.
    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(KeyT&& key)
    {
        check_expand_need();

        auto bucket = find_or_allocate(key);

        if (EMH_BUCKET(_pairs, bucket) != INACTIVE) {
            return { iterator(this, bucket), false };
        } else {
            new(_pairs + bucket) PairT(std::move(key), bucket);
            _num_filled++;
            return { iterator(this, bucket), true };
        }
    }

    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(KeyT(std::forward<Args>(args)...));
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    template <typename Iter>
    inline void insert(Iter begin, Iter end)
    {
        reserve(end - begin + _num_filled);
        for (; begin != end; ++begin) {
            auto key = *begin;
            const auto bucket = find_or_allocate(key);
            if (EMH_BUCKET(_pairs, bucket) == INACTIVE) {
                new(_pairs + bucket) PairT(key, bucket);
                _num_filled++;
            }
        }
    }

    inline void insert(const_iterator begin, const_iterator end)
    {
        for (; begin != end; ++begin) {
            insert(*begin);
        }
    }

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(const KeyT& key)
    {
        check_expand_need();
        auto bucket = find_main_bucket(key, true);
        new(_pairs + bucket) PairT(key, bucket);
        _num_filled++;
        return bucket;
    }

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(const KeyT&& key)
    {
        check_expand_need();
        auto bucket = find_main_bucket(key, true);
        new(_pairs + bucket) PairT(std::move(key), bucket);
        _num_filled++;
        return bucket;
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    bool erase(const KeyT& key)
    {
        auto bucket = erase_bucket(key);
        if (bucket == INACTIVE) {
            return false;
        }

        EMH_BUCKET(_pairs, bucket) = INACTIVE;
        _pairs[bucket].~PairT();
        _num_filled -= 1;

#ifdef EMH_AUTO_SHRINK
        if (_num_buckets > 256 && _num_buckets > 4 * _num_filled)
            rehash(_num_filled * 9 / 8 + 2);
#endif
        return true;
    }

    /// Erase an element typedef an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
        auto bucket = it._bucket;
        bucket = erase_bucket(it->first);

        EMH_BUCKET(_pairs, bucket) = INACTIVE;
        _pairs[bucket].~PairT();
        _num_filled -= 1;
        if (bucket == it._bucket)
            it++;

#ifdef EMH_AUTO_SHRINK
        if (_num_buckets > 256 && _num_buckets > 4 * _num_filled) {
            rehash(_num_filled * 9 / 8 + 2);
            it = begin();
        }
#endif
        return it;
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        for (uint32_t bucket = 0; _num_filled > 0; ++bucket) {
            if (EMH_BUCKET(_pairs, bucket) != INACTIVE) {
                EMH_BUCKET(_pairs, bucket) = INACTIVE;
                _pairs[bucket].~PairT();
                _num_filled -= 1;
            }
        }
        _num_filled = 0;
    }

    /// Make room for this many elements
    inline bool reserve(uint32_t num_elems)
    {
        auto required_buckets = num_elems * 10 / 8 + 2;
        if (required_buckets <= _num_buckets)
            return false;

        rehash(required_buckets);
        return true;
    }

    /// Make room for this many elements
    void rehash(uint32_t required_buckets)
    {
        uint32_t num_buckets = 8;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        assert(num_buckets > _num_filled);
        auto new_pairs = (PairT*)malloc(num_buckets * sizeof(PairT));
        if (!new_pairs) {
            throw std::bad_alloc();
        }

        auto old_num_filled = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_pairs = _pairs;

        _num_filled = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _pairs = new_pairs;

        for (uint32_t bucket = 0; bucket < num_buckets; bucket++)
            EMH_BUCKET(_pairs, bucket) = INACTIVE;

        uint32_t collision = 0;
        //set all main bucket first
        for (uint32_t src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
            if (EMH_BUCKET(old_pairs, src_bucket) == INACTIVE)
                continue;


            const auto main_bucket = BUCKET(EMH_KEY(old_pairs, src_bucket));
            auto& next_bucket = EMH_BUCKET(_pairs, main_bucket);
            if (next_bucket == INACTIVE) {
                auto& src_pair = old_pairs[src_bucket];
                new(_pairs + main_bucket) PairT(std::move(src_pair)); src_pair.~PairT();
                next_bucket = main_bucket;
            }
            else {
                //move collision bucket to head
                EMH_BUCKET(old_pairs, collision++) = (int)src_bucket;
            }
            _num_filled += 1;
            if (_num_filled >= old_num_filled)
                break ;
        }

        //reset all collisions bucket
        for (uint32_t src_bucket = 0; src_bucket < collision; src_bucket++) {
            const auto bucket = EMH_BUCKET(old_pairs, src_bucket);
            auto new_bucket = find_main_bucket(EMH_KEY(old_pairs, bucket), false);
            auto& src_pair = old_pairs[bucket];
            new(_pairs + new_bucket) PairT(std::move(src_pair)); src_pair.~PairT();
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        }

#ifdef REHASH_LOG
        if (_num_filled > 0) {
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/packed/collision = %u/%zd/%.2lf%%", _num_filled, sizeof(PairT), (collision * 100.0 / _num_filled));
#ifdef EMH_LOG
            static int ihashs = 0;
            EMH_LOG() << "SORDER_INDEX = " << ORDER_INDEX << "|rhash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
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

    int erase_bucket(const KeyT& key) const
    {
        const auto bucket = BUCKET(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;
        else if (next_bucket == bucket) {
           if (EMH_KEY(_pairs, bucket) == key)
               return bucket;
           return INACTIVE;
        }
        else if (EMH_KEY(_pairs, bucket) == key) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            std::swap(EMH_KEY(_pairs, next_bucket), EMH_KEY(_pairs, bucket));
            if (nbucket == next_bucket)
                EMH_BUCKET(_pairs, bucket) = bucket;
            else
                EMH_BUCKET(_pairs, bucket) = nbucket;
            return next_bucket;
        }

        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (EMH_KEY(_pairs, next_bucket) == key) {
                if (nbucket == next_bucket)
                    EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
                else
                    EMH_BUCKET(_pairs, prev_bucket) = nbucket;
                return next_bucket;
            }
            if (nbucket == next_bucket)
                break;
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return INACTIVE;
    }

    // Find the bucket with this key, or return INACTIVE
    int find_filled_bucket(const KeyT& key) const
    {
        const auto bucket = BUCKET(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return INACTIVE;
        else if (EMH_KEY(_pairs, bucket) == key)
            return bucket;
        else if (next_bucket == bucket)
            return INACTIVE;

        //find next linked bucket
        while (true) {
            if (EMH_KEY(_pairs, next_bucket) == key) {
#if EMH_LRU_GET
                  std::swap(EMH_KEY(_pairs, next_bucket), EMH_KEY(_pairs, bucket));
                  return bucket;
#else
                  return next_bucket;
#endif
            }
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        return INACTIVE;
    }

    int kickout_bucket(const int main_bucket, const int bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        EMH_BUCKET(_pairs, prev_bucket) = new_bucket;
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket])); _pairs[bucket].~PairT();
        if (next_bucket == bucket)
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        else
            EMH_BUCKET(_pairs, new_bucket) = next_bucket;

        EMH_BUCKET(_pairs, bucket) = INACTIVE;
        return new_bucket;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    int find_or_allocate(const KeyT& key)
    {
        const auto bucket = BUCKET(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        if (next_bucket == INACTIVE || bucket_key == key)
             return bucket;
        else if (next_bucket == bucket && bucket == BUCKET(bucket_key))
             return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

        //find next linked bucket and check key
        while (true) {
            if (EMH_KEY(_pairs, next_bucket) == key) {
#if EMH_LRU_SET
                std::swap(EMH_KEY(_pairs, next_bucket), EMH_KEY(_pairs, bucket));
                return bucket;
#else
                return next_bucket;
#endif
            }

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //check current bucket_key is linked in main bucket
        const auto main_bucket = BUCKET(bucket_key);
        if (main_bucket != bucket) {
            kickout_bucket(main_bucket, bucket);
            return bucket;
        }

        //find a new empty and linked it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    inline int find_empty_bucket(int bucket_from)
    {
        const auto bucket = (++bucket_from) & _mask;
        if (EMH_BUCKET(_pairs, bucket) == INACTIVE)
            return bucket;

        bucket_from = (++bucket_from) & _mask;
        if (EMH_BUCKET(_pairs, bucket_from) == INACTIVE)
            return bucket_from;

#if 1
        constexpr auto max_probe_length = (int)(64 / sizeof(PairT)) + 2;//cpu cache line 64 byte,2-3 cache line miss
        for (auto slot = 1; ; ++slot) {
            const auto bucket = (bucket_from + slot) & _mask;
            if (EMH_BUCKET(_pairs, bucket) == INACTIVE)
                return bucket;
            else if (slot > max_probe_length) {
                const auto bucket1 = (bucket + slot * slot) & _mask; //switch to square search
                if (EMH_BUCKET(_pairs, bucket1) == INACTIVE)
                    return bucket1;

                const auto bucket2 = (bucket1 + 1) & _mask;
                if (EMH_BUCKET(_pairs, bucket2) == INACTIVE)
                    return bucket2;
                else if (slot > 6 /*  || _num_filled * 10 > 7 * _mask */)
                    bucket_from += _num_buckets / 2;
            }
        }
#else
        //0  1  1  2
        //1  2  3  4
        //3  3  6  7
        //6  4 10 11
        //10 5 15 16
        //15 6 21 22
        //21 7 28 29
        //28 8 36 37
        for (auto slot = 1; ; ++slot) {
            const auto bucket1 = (bucket_from + slot) & _mask;
            if (EMH_BUCKET(_pairs, bucket1) == INACTIVE)
                return bucket1;
#if 1
            //const auto cache_slot = reinterpret_cast<size_t>(&EMH_BUCKET(_pairs, bucket1)) % 64;
            //if (pro_bucket2 && cache_slot + sizeof(PairT) < 64) {
                const auto bucket2 = (bucket1 + 1) & _mask;
                if (EMH_BUCKET(_pairs, bucket2) == INACTIVE)
                    return bucket2;
            //}
#endif
            bucket_from += slot;
            if (slot > 6) {
                bucket_from += _num_buckets / 2;
                slot = 1;
            }
        }
#endif
    }

    int find_prev_bucket(int main_bucket, const int bucket)
    {
        auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
        if (next_bucket == bucket || next_bucket == main_bucket)
            return main_bucket;

        while (true) {
            auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    int find_main_bucket(const KeyT& key, bool check_main)
    {
        const auto bucket = BUCKET(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return bucket;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        const auto main_bucket = BUCKET(bucket_key);
        if (main_bucket == bucket) {
            if (next_bucket == bucket)
                return EMH_BUCKET(_pairs, bucket) = find_empty_bucket(next_bucket);
        } else if (check_main) {
            //check current bucket_key is linked in main bucket

            kickout_bucket(main_bucket, bucket);
            return bucket;
        }

        //find a new empty and linked it to tail
        int last_bucket = next_bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                last_bucket = nbucket;
                break;
            }
            next_bucket = nbucket;
        }

        return EMH_BUCKET(_pairs, last_bucket) = find_empty_bucket(last_bucket);
    }

private:

    HashT   _hasher;
    PairT*  _pairs;
    uint32_t  _num_buckets;
    uint32_t  _num_filled;
    uint32_t  _mask;  // _num_buckets minus one
};

} // namespace emhash
