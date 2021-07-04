// By Emil Ernerfeldt 2014-2017
// LICENSE:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.
//   http://www.ilikebigbits.com/2016_08_28_hash_table.html

//some others
//https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html

#pragma once

#include <cstdlib>
#include <iterator>
#include <utility>
#include <cstring>
#include <cassert>

#ifdef  EMH_KEY
    #undef  EMH_KEY
    #undef  EMH_VAL
    #undef  BUCKET
    #undef  EMH_BUCKET
    #undef  EMH_PKV
	#undef  EMH_NEW
#endif

#define BUCKET(key)  int(_hasher(key) & _mask)
//#define BUCKET(key)  (_hasher(key) & (_mask / 2)) * 2

#if ORDER_INDEX == 0
    #define EMH_KEY(p,n)     p[n].second.first
    #define EMH_VAL(p,n)     p[n].second.second
    #define EMH_BUCKET(p,n) p[n].first
    #define EMH_PKV(s,n)    s[n].second
    #define EMH_NEW(key, value, bucket)  new(_pairs + bucket) PairT(bucket, std::pair<KeyT, ValueT>(key, value))
#elif ORDER_INDEX == 1
    #define EMH_KEY(p,n)     p[n].first.first
    #define EMH_VAL(p,n)     p[n].first.second
    #define EMH_BUCKET(p,n) p[n].second
    #define EMH_PKV(s,n)    s[n].first
    #define EMH_NEW(key, value, bucket) new(_pairs + bucket) PairT(std::pair<KeyT, ValueT>(key, value), bucket);
#endif

namespace emhash3 {
/// like std::equal_to but no need to #include <functional>
template<typename T>
struct HashMapEqualTo
{
    constexpr bool operator()(const T& lhs, const T& rhs) const
    {
        return lhs == rhs;
    }
};

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = HashMapEqualTo<KeyT>>
class HashMap
{
    enum State
    {
        INACTIVE = -1, // Never been touched
    };
private:
    typedef  HashMap<KeyT, ValueT, HashT, EqT> htype;

#if ORDER_INDEX == 0
    typedef  std::pair<int, std::pair<KeyT, ValueT>> PairT;
#else
    typedef  std::pair<std::pair<KeyT, ValueT>, int> PairT;
#endif

public:
    typedef  size_t       size_type;
    typedef  PairT        value_type;
    typedef  PairT&       reference;
    typedef  const PairT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t                    difference_type;
        typedef size_t                    distance_type;
        typedef std::pair<KeyT, ValueT>   value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        iterator() { }

        iterator(htype* hash_map, unsigned int bucket) : _map(hash_map), _bucket(bucket)
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
            return iterator(_map, old_index);
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
            } while (_bucket < _map->_num_buckets && _map->EMH_BUCKET(_pairs, _bucket) == State::INACTIVE);
        }

        //private:
        //    friend class htype;
    public:
        htype* _map;
        unsigned int  _bucket;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t                    difference_type;
        typedef size_t                    distance_type;
        typedef std::pair<KeyT, ValueT>   value_type;
        typedef value_type*               pointer;
        typedef value_type&               reference;

        const_iterator() { }

        const_iterator(iterator proto) : _map(proto._map), _bucket(proto._bucket)
        {
        }

        const_iterator(const htype* hash_map, unsigned int bucket) : _map(hash_map), _bucket(bucket)
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
            return const_iterator(_map, old_index);
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
            } while (_bucket < _map->_num_buckets && _map->EMH_BUCKET(_pairs, _bucket) == State::INACTIVE);
        }

        //private:
        //    friend class htype;
    public:
        const htype* _map;
        unsigned int  _bucket;
    };

    // ------------------------------------------------------------------------

    void init()
    {
        _num_buckets = 0;
        _num_filled = 0;
        _mask = 0;  // _num_buckets minus one
        _pairs = nullptr;
    }

    HashMap()
    {
        init();
        reserve(8);
    }

    HashMap(const HashMap& other)
    {
        init();
        reserve(other.size());
        insert(other.cbegin(), other.cend());
    }

    HashMap(HashMap&& other)
    {
        init();
        reserve(8);
        *this = std::move(other);
    }

    HashMap& operator=(const HashMap& other)
    {
        clear();
        reserve(other.size());
        insert(other.cbegin(), other.cend());
        return *this;
    }

    HashMap& operator=(HashMap&& other)
    {
        swap(other);
        return *this;
    }

    ~HashMap()
    {
        for (unsigned int bucket = 0; bucket < _num_buckets; ++bucket) {
            if (EMH_BUCKET(_pairs, bucket) != State::INACTIVE) {
                _pairs[bucket].~PairT();
            }
        }
        if (_pairs)
            free(_pairs);
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher, other._hasher);
//        std::swap(_eq, other._eq);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        unsigned int bucket = 0;
        while (bucket < _num_buckets && EMH_BUCKET(_pairs, bucket) == State::INACTIVE) {
            ++bucket;
        }
        return iterator(this, bucket);
    }

    const_iterator cbegin() const
    {
        unsigned int bucket = 0;
        while (bucket < _num_buckets && EMH_BUCKET(_pairs, bucket) == State::INACTIVE) {
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
        if (bucket == State::INACTIVE) {
            return end();
        }
        return iterator(this, bucket);
    }

    const_iterator find(const KeyT& key) const
    {
        auto bucket = find_filled_bucket(key);
        if (bucket == State::INACTIVE) {
            return end();
        }
        return const_iterator(this, bucket);
    }

    bool contains(const KeyT& key) const
    {
        return find_filled_bucket(key) != State::INACTIVE;
    }

    size_t count(const KeyT& key) const
    {
        return find_filled_bucket(key) != State::INACTIVE ? 1 : 0;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& key)
    {
        auto bucket = find_filled_bucket(key);
        if (bucket != State::INACTIVE) {
            return &EMH_VAL(_pairs, bucket);
        }
        else {
            return nullptr;
        }
    }

    /// Const version of the above
    const ValueT* try_get(const KeyT& key) const
    {
        auto bucket = find_filled_bucket(key);
        if (bucket != State::INACTIVE) {
            return &EMH_VAL(_pairs, bucket);
        }
        else {
            return nullptr;
        }
    }

    /// Convenience function.
    const ValueT get_or_return_default(const KeyT& key) const
    {
        const ValueT* ret = try_get(key);
        if (ret) {
            return *ret;
        }
        else {
            return ValueT();
        }
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key, const ValueT& value)
    {
        auto bucket = find_or_allocate(key);
        if (EMH_BUCKET(_pairs, bucket) != State::INACTIVE) {
            return { iterator(this, bucket), false };
        }
        else {
            if (check_expand_need())
                bucket = find_main_bucket(key, true);

            EMH_NEW(key, value, bucket);
            _num_filled++;
            return { iterator(this, bucket), true };
        }
    }

    std::pair<iterator, bool> insert(const std::pair<KeyT, ValueT>& p)
    {
        return insert(p.first, p.second);
    }

    void insert(const_iterator begin, const_iterator end)
    {
        // TODO: reserve space exactly once.
        for (; begin != end; ++begin) {
            insert(begin->first, begin->second);
        }
    }

    /// Same as above, but contains(key) MUST be false
    void insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        auto bucket = find_main_bucket(key, true);
        EMH_NEW(key, value, bucket);
        _num_filled++;
    }

    void insert_unique(std::pair<KeyT, ValueT>&& p)
    {
        insert_unique(std::move(p.first), std::move(p.second));
    }

    void insert_or_assign(const KeyT& key, ValueT&& value)
    {
        check_expand_need();

        auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (EMH_BUCKET(_pairs, bucket) != State::INACTIVE) {
            EMH_VAL(_pairs, bucket) = value;
        }
        else {
            EMH_NEW(key, value, bucket);
            _num_filled++;
        }
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& new_value)
    {
        check_expand_need();

        auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (EMH_BUCKET(_pairs, bucket) != State::INACTIVE) {
            ValueT old_value = EMH_VAL(_pairs, bucket);
            EMH_VAL(_pairs, bucket) = new_value;
            return old_value;
        }
        else {
            EMH_NEW(key, new_value, bucket);
            _num_filled++;
            return ValueT();
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        auto bucket = find_or_allocate(key);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (EMH_BUCKET(_pairs, bucket) == State::INACTIVE) {
            if (check_expand_need())
                bucket = find_main_bucket(key, true);

            EMH_NEW(key, ValueT(), bucket);
            _num_filled++;
        }

        return EMH_VAL(_pairs, bucket);
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    bool erase(const KeyT& key)
    {
        auto bucket = erase_bucket(key);
        if (bucket == State::INACTIVE) {
            return false;
        }

        EMH_BUCKET(_pairs, bucket) = State::INACTIVE;
        _pairs[bucket].~PairT();
        _num_filled -= 1;

#ifdef EIMLIB_AUTO_SHRINK
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

        EMH_BUCKET(_pairs, bucket) = State::INACTIVE;
        _pairs[bucket].~PairT();
        _num_filled -= 1;
        if (bucket == it._bucket)
            it++;

#ifdef EIMLIB_AUTO_SHRINK
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
            if (EMH_BUCKET(_pairs, bucket) != State::INACTIVE) {
                EMH_BUCKET(_pairs, bucket) = State::INACTIVE;
                _pairs[bucket].~PairT();
            }
        }
        _num_filled = 0;
    }

    /// Make room for this many elements
    inline bool reserve(unsigned int num_elems)
    {
        auto required_buckets = num_elems * 9 / 8 + 2;
        if (required_buckets <= _num_buckets) {
            return false;
        }
        rehash(required_buckets);
        return true;
    }

    /// Make room for this many elements
    void rehash(unsigned int required_buckets)
    {
        unsigned int num_buckets = 4;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto new_pairs = (PairT*)malloc(num_buckets * sizeof(PairT));
        if (!new_pairs) {
            throw std::bad_alloc();
        }

        assert(num_buckets > _num_filled);
        auto old_num_filled  = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_pairs = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _pairs       = new_pairs;

        for (unsigned int bucket = 0; bucket < num_buckets; bucket++)
            EMH_BUCKET(_pairs, bucket) = State::INACTIVE;

        unsigned int collision = 0;
        //set all main bucket first
        for (unsigned int src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
            if (EMH_BUCKET(old_pairs, src_bucket) == State::INACTIVE) {
                continue;
            }

            auto& src_pair = old_pairs[src_bucket];
            const auto main_bucket = BUCKET(EMH_KEY(old_pairs, src_bucket));
            auto& next_bucket = EMH_BUCKET(_pairs, main_bucket);
            if (next_bucket == State::INACTIVE) {
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
            //memcpy(&_pairs[new_bucket], &src_pair, sizeof(src_pair));
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        }

        if (_num_filled > 1024 * 16)
            printf("    _num_filled/packed/collision = %u/%zd/%.2lf%%\n", _num_filled, sizeof(PairT),  (collision * 100.0 / _num_filled));

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
        if (next_bucket == State::INACTIVE)
            return State::INACTIVE;
        else if (next_bucket == bucket) {
           if (EMH_KEY(_pairs, bucket) == key)
               return bucket;
           return State::INACTIVE;
        }
        else if (EMH_KEY(_pairs, bucket) == key) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
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

        return State::INACTIVE;
    }

    // Find the bucket with this key, or return State::INACTIVE
    int find_filled_bucket(const KeyT& key) const
    {
        //if (empty()) { return State::INACTIVE; } // Optimization
        const auto bucket = BUCKET(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
#if 0
        if (next_bucket != State::INACTIVE) {
             if (EMH_KEY(_pairs, bucket) == key)
                return bucket;
             else if (next_bucket == bucket)
                 return State::INACTIVE;
        }
        else
            return State::INACTIVE;
#elif 1
        if (next_bucket == State::INACTIVE)
            return State::INACTIVE;
        else if (EMH_KEY(_pairs, bucket) == key)
            return bucket;
        else if (next_bucket == bucket)
            return State::INACTIVE;
#endif
        //find next linked bucket
        while (true) {
            if (EMH_KEY(_pairs, next_bucket) == key) {
#if EMLIB_LRU_GET
                  EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, bucket));
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

        return State::INACTIVE;
    }

    int kickout_bucket(const int main_bucket, const int bucket)
    {
        //TODO:find parent/prev bucket
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
        if (next_bucket == State::INACTIVE || bucket_key == key)
             return bucket;
        else if (next_bucket == bucket && (BUCKET(bucket_key)) == bucket)
             return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

        //find next linked bucket and check key
        while (true) {
            if (EMH_KEY(_pairs, next_bucket) == key) {
#if EMLIB_LRU_SET
                EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, bucket));
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
            EMH_BUCKET(_pairs, bucket) = State::INACTIVE;
            return bucket;
        }

        //find a new empty and linked it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    inline int find_empty_bucket(int bucket_from)
    {
        constexpr int max_probe_length = (int)(128 / sizeof(PairT)) + 2;//cpu cache line 64 byte,2-3 cache line miss
        for (auto offset = 1; ; ++offset) {
            const auto bucket = (bucket_from + offset) & _mask;
            if (EMH_BUCKET(_pairs, bucket) == State::INACTIVE)
                return bucket;
            else if (offset > max_probe_length) {
                const auto bucket1 = (bucket + offset * offset + 0) & _mask;
                if (EMH_BUCKET(_pairs, bucket1) == State::INACTIVE)
                    return bucket1;

                const auto bucket2 = (bucket1 + 1) & _mask;
                if (EMH_BUCKET(_pairs, bucket2) == State::INACTIVE)
                    return bucket2;
#if Q3S
                const auto bucket3 = (bucket1 - 1) & _mask;
                if (EMH_BUCKET(_pairs, bucket3) == State::INACTIVE)
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
        if (next_bucket == State::INACTIVE)
            return bucket;
        else if (next_bucket == bucket && (BUCKET(bucket_key)) == bucket)
             return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

        if (check_main) {
            const auto main_bucket = BUCKET(bucket_key);
            //check current bucket_key is linked in main bucket
            if (main_bucket != bucket) {
                kickout_bucket(main_bucket, bucket);
                EMH_BUCKET(_pairs, bucket) = State::INACTIVE;
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

private:

    HashT   _hasher;
//    EqT     _eq;
    PairT*  _pairs;
    unsigned int  _num_buckets;
    unsigned int  _num_filled;
    unsigned int  _mask;  // _num_buckets minus one
};

} // namespace emhash

