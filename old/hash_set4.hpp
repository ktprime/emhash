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

#if TAF_LOG
    #include "LogHelper.h"
#endif
#ifdef  EMH_KEY
    #undef  EMH_KEY
    #undef  BUCKET
    #undef  EMH_BUCKET
#endif

#define BUCKET(key)  int(_hasher(key) & _mask)
#define EMH_KEY(p,n)     p[n].first
#define EMH_BUCKET(p,n) p[n].second

namespace emhash4 {

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

        iterator(htype* hash_set, unsigned int bucket) : _set(hash_set), _bucket(bucket)
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
        unsigned int  _bucket;
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

        const_iterator(const htype* hash_set, unsigned int bucket) : _set(hash_set), _bucket(bucket)
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
        unsigned int  _bucket;
    };

    // ------------------------------------------------------------------------

    void init()
    {
        _num_buckets = 0;
        _num_filled = 0;
        _mask = 0;
        _pairs = nullptr;
    }

    HashSet()
    {
        init();
        reserve(8);
    }

    HashSet(const HashSet& other)
    {
        init();
        reserve(other.size());
        insert(other.cbegin(), other.cend());
    }

    HashSet(HashSet&& other)
    {
        init();
        reserve(8);
        *this = std::move(other);
    }

    HashSet& operator=(const HashSet& other)
    {
        clear();
        reserve(other.size());
        insert(other.cbegin(), other.cend());
        return *this;
    }

    HashSet& operator=(HashSet&& other)
    {
        swap(other);
        return *this;
    }

    ~HashSet()
    {
        for (unsigned int bucket = 0; bucket < _num_buckets; ++bucket) {
            if (EMH_BUCKET(_pairs, bucket) != INACTIVE) {
                _pairs[bucket].~PairT();
            }
        }
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
        unsigned int bucket = 0;
        while (bucket < _num_buckets && EMH_BUCKET(_pairs, bucket) == INACTIVE) {
            ++bucket;
        }
        return iterator(this, bucket);
    }

    const_iterator cbegin() const
    {
        unsigned int bucket = 0;
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

    void insert(const_iterator begin, const_iterator end)
    {
        // TODO: reserve space exactly once.
        for (; begin != end; ++begin) {
            insert(*begin);
        }
    }

    /// Same as above, but contains(key) MUST be false
    void insert_unique(const KeyT& key)
    {
        //DCHECK_F(!contains(key));
        check_expand_need();
        auto bucket = find_main_bucket(key, true);
        new(_pairs + bucket) PairT(key, bucket);
        _num_filled++;
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
        for (unsigned int bucket = 0; bucket < _num_buckets; ++bucket) {
            if (EMH_BUCKET(_pairs, bucket) != INACTIVE) {
                EMH_BUCKET(_pairs, bucket) = INACTIVE;
                _pairs[bucket].~PairT();
                if ( _num_filled -- == 1)
                    break;
            }
        }
        _num_filled = 0;
    }

    /// Make room for this many elements
    inline bool reserve(unsigned int num_elems)
    {
        auto required_buckets = num_elems * 9 / 8 + 2;
        if (required_buckets <= _num_buckets)
            return false;

        rehash(required_buckets);
        return true;
    }

    /// Make room for this many elements
    void rehash(unsigned int required_buckets)
    {
        unsigned int num_buckets = 4;
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

        for (unsigned int bucket = 0; bucket < num_buckets; bucket++)
            EMH_BUCKET(_pairs, bucket) = INACTIVE;

        unsigned int collision = 0;
        //set all main bucket first
        for (unsigned int src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
            if (EMH_BUCKET(old_pairs, src_bucket) == INACTIVE) {
                continue;
            }

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
        for (unsigned int src_bucket = 0; src_bucket < collision; src_bucket++) {
            const auto bucket = EMH_BUCKET(old_pairs, src_bucket);
            auto new_bucket = find_main_bucket(EMH_KEY(old_pairs, bucket), false);
            auto& src_pair = old_pairs[bucket];
            new(_pairs + new_bucket) PairT(std::move(src_pair)); src_pair.~PairT();
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        }

#ifdef LOG_REHASH
        if (_num_filled > 0) {
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/packed/collision = %u/%zd/%.2lf%%", _num_filled, sizeof(PairT), (collision * 100.0 / _num_filled));
#if TAF_LOG
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
        const auto new_bucket  = find_empty_bucket(bucket);
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        EMH_BUCKET(_pairs, prev_bucket) = new_bucket;
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket])); _pairs[bucket].~PairT();
        if (next_bucket == bucket)
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        else
            EMH_BUCKET(_pairs, new_bucket) = next_bucket;

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
            EMH_BUCKET(_pairs, bucket) = INACTIVE;
            return bucket;
        }

        //find a new empty and linked it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    inline int find_empty_bucket(int bucket_from)
    {
        constexpr auto max_probe_length = (int)(128 / sizeof(PairT)) + 2;//cpu cache line 64 byte,2-3 cache line miss
        for (auto offset = 1; ; ++offset) {
            const auto bucket = (bucket_from + offset) & _mask;
            if (EMH_BUCKET(_pairs, bucket) == INACTIVE)
                return bucket;
            else if (offset > max_probe_length) {
                const int bucket1 = (bucket + offset * offset) & _mask;
                if (EMH_BUCKET(_pairs, bucket1) == INACTIVE)
                    return bucket1;

                const auto bucket2 = (bucket1 + 1) & _mask;
                if (EMH_BUCKET(_pairs, bucket2) == INACTIVE)
                    return bucket2;
#if Q3S
                const auto bucket3 = (bucket1 - 1) & _mask;
                if (EMH_BUCKET(_pairs, bucket3) == INACTIVE)
                    return bucket3;
#endif
            }
        }
    }

    inline int find_prev_bucket(int main_bucket, const int bucket)
    {
        while (true) {
            const auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
            if (next_bucket == bucket || next_bucket == main_bucket)
                return main_bucket;
            main_bucket = next_bucket;
        }
    }

    int find_main_bucket(const KeyT& key, bool check_main)
    {
        const auto bucket = BUCKET(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        if (next_bucket == INACTIVE)
            return bucket;
        else if (next_bucket == bucket && (BUCKET(bucket_key)) == bucket)
             return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

        if (check_main) {
            const auto main_bucket = BUCKET(bucket_key);
            //check current bucket_key is linked in main bucket
            if (main_bucket != bucket) {
                kickout_bucket(main_bucket, bucket);
                EMH_BUCKET(_pairs, bucket) = INACTIVE;
                return bucket;
            }
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

    inline int fibonacci_hash(int key) {
        return (key * 11400714819323198485llu) & _mask;
    }

private:

    HashT   _hasher;
    PairT*  _pairs;
    unsigned int  _num_buckets;
    unsigned int  _num_filled;
    unsigned int  _mask;  // _num_buckets minus one
};

} // namespace emhash
