// By Emil Ernerfeldt 2014-2017
// LICENSE:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.
//   http://www.ilikebigbits.com/2016_08_28_hash_table.html


#pragma once

#include <cstdlib>
#include <iterator>
#include <utility>
#include <cstring>


#ifdef  EMH_KEY
    #undef  EMH_KEY
    #undef  EMH_VAL
    #undef  BUCKET
    #undef  HOPS
#endif

#define HOPS         1
#define EMH_KEY(p,n) p[n].second.first
#define EMH_VAL(p,n) p[n].second.second
#define MAX_LEN(p,n) p[n].first
//#define BUCKET(key)  (int)(_hasher(key) & _mask)
#define BUCKET(key)  (_hasher(key) & (_mask / 2)) * 2

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
        FILLED = 0   // Is set with key/value
    };
private:
    typedef  HashMap<KeyT, ValueT, HashT, EqT> htype;
    typedef  std::pair<int, std::pair<KeyT, ValueT>> PairT;

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

        iterator(htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket)
        {
        }

        iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            size_t old_index = _bucket;
            goto_next_element();
            return iterator(_map, old_index);
        }

        reference operator*() const
        {
            return _map->_pairs[_bucket].second;
        }

        pointer operator->() const
        {
            return &(_map->_pairs[_bucket].second);
        }

        bool operator==(const iterator& rhs) const
        {
            //DCHECK_EQ_F(_map, rhs._map);
            return _bucket == rhs._bucket;
        }

        bool operator!=(const iterator& rhs) const
        {
            //DCHECK_EQ_F(_map, rhs._map);
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            //DCHECK_LT_F(_bucket, _map->_num_buckets);
            do {
                _bucket++;
            } while (_bucket < _map->_num_buckets && _map->MAX_LEN(_pairs,_bucket) == State::INACTIVE);
        }

    //private:
    //    friend class htype;
    public:
        htype* _map;
        size_t  _bucket;
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

        const_iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket)
        {
        }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            size_t old_index = _bucket;
            goto_next_element();
            return const_iterator(_map, old_index);
        }

        reference operator*() const
        {
            return _map->_pairs[_bucket].second;
        }

        pointer operator->() const
        {
            return &(_map->_pairs[_bucket].second);
        }

        bool operator==(const const_iterator& rhs) const
        {
            //DCHECK_EQ_F(_map, rhs._map);
            return _bucket == rhs._bucket;
        }

        bool operator!=(const const_iterator& rhs) const
        {
            //DCHECK_EQ_F(_map, rhs._map);
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            //DCHECK_LT_F(_bucket, _map->_num_buckets);
            do {
                _bucket++;
            } while (_bucket < _map->_num_buckets && _map->MAX_LEN(_pairs,_bucket) == State::INACTIVE);
        }

    //private:
    //    friend class htype;
    public:
        const htype* _map;
        size_t        _bucket;
    };

    // ------------------------------------------------------------------------

    void init()
    {
        _num_buckets = 0;
        _num_filled = 0;
        _mask = 0;  // _num_buckets minus one
        _pairs = nullptr;
        reserve(8);
    }

    HashMap()
    {
       init();
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
        for (size_t bucket=0; bucket<_num_buckets; ++bucket) {
            if (MAX_LEN(_pairs,bucket) != State::INACTIVE) {
                _pairs[bucket].~PairT();
            }
        }
        if (_pairs)
            free(_pairs);
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher,           other._hasher);
        std::swap(_eq,               other._eq);
        std::swap(_pairs,            other._pairs);
        std::swap(_num_buckets,      other._num_buckets);
        std::swap(_num_filled,       other._num_filled);
        std::swap(_mask,             other._mask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        size_t bucket = 0;
        while (bucket<_num_buckets && MAX_LEN(_pairs,bucket) == State::INACTIVE) {
            ++bucket;
        }
        return iterator(this, bucket);
    }

    const_iterator cbegin() const
    {
        size_t bucket = 0;
        while (bucket<_num_buckets && MAX_LEN(_pairs,bucket) == State::INACTIVE) {
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
        return _num_filled==0;
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
        if (bucket == State::INACTIVE)
        {
            return end();
        }
        return const_iterator(this, bucket);
    }

    bool contains(const KeyT& k) const
    {
        return find_filled_bucket(k) != State::INACTIVE;
    }

    size_t count(const KeyT& k) const
    {
        return find_filled_bucket(k) != State::INACTIVE ? 1 : 0;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& k)
    {
        auto bucket = find_filled_bucket(k);
        if (bucket != State::INACTIVE) {
            return &EMH_VAL(_pairs,bucket);
        } else {
            return nullptr;
        }
    }

    /// Const version of the above
    const ValueT* try_get(const KeyT& k) const
    {
        auto bucket = find_filled_bucket(k);
        if (bucket != State::INACTIVE) {
            return &EMH_VAL(_pairs,bucket);
        } else {
            return nullptr;
        }
    }

    /// Convenience function.
    const ValueT get_or_return_default(const KeyT& k) const
    {
        const ValueT* ret = try_get(k);
        if (ret) {
            return *ret;
        } else {
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
        if (MAX_LEN(_pairs,bucket) != State::INACTIVE) {
            return { iterator(this, bucket), false };
        } else {
            if (check_expand_need())
                bucket = find_or_allocate(key);

            new(_pairs + bucket) PairT(State::FILLED, std::pair<KeyT, ValueT>(key, value));
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
        //DCHECK_F(!contains(key));
        check_expand_need();
        auto bucket = find_main_bucket(key);
        new(_pairs + bucket) PairT(State::FILLED, std::pair<KeyT, ValueT>(key, value));
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
        if (MAX_LEN(_pairs,bucket) != State::INACTIVE) {
            EMH_VAL(_pairs,bucket) = value;
        } else {
            new(_pairs + bucket) PairT(State::FILLED, std::pair<KeyT, ValueT>(key, value));
            _num_filled++;
        }
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& new_value)
    {
        check_expand_need();
        auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (MAX_LEN(_pairs,bucket) != State::INACTIVE) {
            ValueT old_value = EMH_VAL(_pairs,bucket);
            EMH_VAL(_pairs,bucket) = new_value;
            return old_value;
        } else {
            new(_pairs + bucket) PairT(State::FILLED, std::pair<KeyT, ValueT>(key, new_value));
            _num_filled++;
            return ValueT();
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        auto bucket = find_or_allocate(key);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (MAX_LEN(_pairs,bucket) == State::INACTIVE) {
            if (check_expand_need())
                bucket = find_main_bucket(key);

            new(_pairs + bucket) PairT(State::FILLED, std::pair<KeyT, ValueT>(key, ValueT()));
            _num_filled++;
        }

        return EMH_VAL(_pairs,bucket);
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    bool erase(const KeyT& key)
    {
        const auto bucket = erase_bucket (key);
        if (bucket == State::INACTIVE) {
            return false;
        }

        MAX_LEN(_pairs,bucket) = State::INACTIVE;
        _pairs[bucket].~PairT();
        _num_filled -= 1;

#ifdef EIMLIB_AUTO_SHRINK
        if (_num_buckets > 256 && _num_buckets > 4 * _num_filled)
            reserve(_num_filled * 2);
#endif
        return true;
    }

    /// Erase an element typedef an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
        //DCHECK_EQ_F(it._map, this);
        //DCHECK_LT_F(it._bucket, _num_buckets);
        auto bucket = it._bucket;
        if (MAX_LEN(_pairs,bucket) > State::FILLED) {
             bucket = erase_bucket(it->first);
        }

        MAX_LEN(_pairs,bucket) = State::INACTIVE;
        _pairs[bucket].~PairT();
        _num_filled -= 1;
        if (bucket == it._bucket)
           ++it;

#ifdef EIMLIB_AUTO_SHRINK
        if (_num_buckets > 256 && _num_buckets > 4 * _num_filled)
            reserve(_num_filled * 2);
#endif
        return it;
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        for (size_t bucket=0; bucket<_num_buckets; ++bucket) {
            if (MAX_LEN(_pairs,bucket) != State::INACTIVE) {
                MAX_LEN(_pairs,bucket) = State::INACTIVE;
                _pairs[bucket].~PairT();
            }
        }
        _num_filled = 0;
    }

    /// Make room for this many elements
    inline bool reserve(size_t num_elems)
    {
        size_t required_buckets = num_elems + 2 + num_elems / 8;
        if (required_buckets <= _num_buckets) {
            return false;
        }
        rehash(required_buckets);
        return true;
    }

    void rehash(size_t required_buckets)
    {
        size_t num_buckets = 4;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto new_pairs  = (PairT*)malloc((1 + num_buckets) * sizeof(PairT));
        if (!new_pairs) {
            throw std::bad_alloc();
        }

        auto old_num_filled  = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_pairs       = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = _num_buckets - 1;
        _pairs       = new_pairs;

        for (size_t bucket = 0; bucket < num_buckets; bucket++)
            MAX_LEN(_pairs, bucket) = State::INACTIVE;
        MAX_LEN(_pairs, num_buckets) = State::INACTIVE;

        size_t collision = 0;
        //set all main bucket first
        for (size_t src_bucket=0; src_bucket < old_num_buckets && _num_filled < old_num_filled; src_bucket++) {
            if (MAX_LEN(old_pairs, src_bucket) == State::INACTIVE) {
                continue;
            }

            auto& src_pair = old_pairs[src_bucket];
            const auto main_bucket = BUCKET(src_pair.second.first);
            auto& max_probe_length = MAX_LEN(_pairs, main_bucket);
            if (max_probe_length == State::INACTIVE) {
                new(_pairs + main_bucket) PairT(std::move(src_pair)); src_pair.~PairT();
                max_probe_length = State::FILLED;
                _num_filled += 1;
            } else {
                //move collision bucket to head
                //new(old_pairs + collision ++) PairT(std::move(src_pair)); src_pair.~PairT();
                MAX_LEN(old_pairs, collision++) = (int)src_bucket;
            }
        }

        if (_num_filled > 100)
            printf("    num_buckets/bucket_ration = %zd/%.2lf%%, collision = %zd, collor_ration = %.2lf%%\n",
                    _num_filled, _num_filled * 100.0 / (num_buckets),  collision, (collision * 100.0 / (num_buckets + 1)));

        //reset all collisions bucket
        for (size_t src_bucket=0; src_bucket < collision; src_bucket++) {
            const auto bucket = MAX_LEN(old_pairs, src_bucket);
            auto& src_pair = old_pairs[bucket];
            const auto main_bucket = BUCKET(src_pair.second.first);
            auto& max_probe_length = MAX_LEN(_pairs, main_bucket);
            const auto new_bucket = find_empty_bucket(main_bucket);
            new(_pairs + new_bucket) PairT(std::move(src_pair)); src_pair.~PairT();

            const int offset = (int)(new_bucket - main_bucket);
            if (offset > max_probe_length)
                max_probe_length = offset;
            else if (offset < 0 && offset + (int)_num_buckets > max_probe_length)
                max_probe_length = (int)(_num_buckets + offset);

            MAX_LEN(_pairs, new_bucket) = State::FILLED;
            _num_filled += 1;
        }

        assert(old_num_filled == _num_filled);
        free(old_pairs);
    }

private:
    // Can we fit another element?
    inline bool check_expand_need()
    {
        return reserve(_num_filled);
    }

    int erase_bucket(const KeyT& key) const
    {
        //if (empty()) { return State::INACTIVE; } // Optimization
        const auto bucket = BUCKET(key);
        auto& max_probe_length = MAX_LEN(_pairs,bucket);
        if (max_probe_length == State::INACTIVE)
            return State::INACTIVE;
        else if (max_probe_length == State::FILLED) {
            if (EMH_KEY(_pairs,bucket) == key)
                return bucket;
            return State::INACTIVE;
        }
        else if (EMH_KEY(_pairs,bucket) == key) {
            //can not erase the main bucket if max_probe_length > 1
            //find last match bucket and swap to main bucket
            for (auto offset = max_probe_length; offset > 0; offset -= HOPS) {
                max_probe_length -= HOPS; if (max_probe_length < 0 && HOPS > 1) max_probe_length = 0;
                const auto nbucket = (bucket + offset) & _mask;
                if (MAX_LEN(_pairs,nbucket) != State::INACTIVE && bucket == (BUCKET(EMH_KEY(_pairs,nbucket)))) {
                    _pairs[bucket].second.swap(_pairs[nbucket].second);
                    return nbucket;
                }
            }
            return bucket;
        }

        for (auto offset = max_probe_length; offset > 0; offset -= HOPS) {
            const auto nbucket = (bucket + offset) & _mask;
            if (MAX_LEN(_pairs,nbucket) != State::INACTIVE && EMH_KEY(_pairs,nbucket) == key) {
                const auto last = (bucket + max_probe_length) & _mask;
                max_probe_length -= HOPS; if (max_probe_length < 0 && HOPS > 1) max_probe_length = 0;
                if  (last != nbucket && (BUCKET(EMH_KEY(_pairs, last))) == bucket) {
                    _pairs[last].swap(_pairs[nbucket]);
                    return last;
                }
                return nbucket;
            }
        }

        return State::INACTIVE;
    }

    // Find the bucket with this key, or return State::INACTIVE
    int find_filled_bucket(const KeyT& key) const
    {
        //if (empty()) { return State::INACTIVE; } // Optimization
        const auto bucket = BUCKET(key);
        const auto max_probe_length = MAX_LEN(_pairs,bucket);
#if 0
        if (max_probe_length == State::FILLED) {
            if (_eq(EMH_KEY(_pairs,bucket), key))
                return bucket;
            else
                return State::INACTIVE;
        }
        else if (max_probe_length == State::INACTIVE)
            return State::INACTIVE;
#elif 1
        if (max_probe_length == State::INACTIVE)
            return State::INACTIVE;
        else if (EMH_KEY(_pairs,bucket) == key)
            return bucket;
        else if (max_probe_length == State::FILLED)
            return State::INACTIVE;
#endif

        for (auto offset = 1; offset <= max_probe_length; offset += HOPS) {
            const auto nbucket = (bucket + offset) & _mask;
            if (MAX_LEN(_pairs,nbucket) != State::INACTIVE && EMH_KEY(_pairs,nbucket) == key) {
#if EMLIB_LRU_GET
                _pairs[nbucket].second.swap(_pairs[bucket].second);
                return bucket;
#else
                return nbucket;
#endif
            }
        }

        return State::INACTIVE;
    }

    int kickout_bucket(int main_bucket, int bucket)
    {
        const auto new_bucket = find_empty_bucket(main_bucket);
        auto& max_probe_length = MAX_LEN(_pairs, main_bucket);

        const int offset = (int)(new_bucket - main_bucket);
        if (offset > max_probe_length)
            max_probe_length = offset;
        else if (offset < 0 && offset + (int)_num_buckets > max_probe_length)
            max_probe_length = (int)(_num_buckets + offset);
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket])); _pairs[bucket].~PairT();
        MAX_LEN(_pairs, new_bucket) = State::FILLED;
        return new_bucket;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    int find_or_allocate(const KeyT& key)
    {
        const auto bucket = BUCKET(key);
        auto& max_probe_length = MAX_LEN(_pairs,bucket);
        const auto& bucket_key = EMH_KEY(_pairs,bucket);
        if (max_probe_length == State::INACTIVE || bucket_key == key)
            return bucket;

        //find exits
        auto offset = 1;
        for (; offset <= max_probe_length; offset += HOPS) {
            const auto nbucket = (bucket + offset) & _mask;
            if (EMH_KEY(_pairs, nbucket) == key && MAX_LEN(_pairs, nbucket) != State::INACTIVE) {
#if EMLIB_LRU_SET
                _pairs[nbucket].second.swap(_pairs[bucket].second);
                return bucket;
#else
                return nbucket;
#endif
            }
        }

        //find current bucket_key move it to man bucket
        const auto main_bucket = BUCKET(bucket_key);
        if (main_bucket != bucket) {
            kickout_bucket(main_bucket, bucket);
            max_probe_length = State::INACTIVE;
            return bucket;
        }

        //find a new empty
        for (; ; offset += HOPS) {
            const auto nbucket = (bucket + offset) & _mask;
            if (MAX_LEN(_pairs,nbucket) == State::INACTIVE) {
                max_probe_length = offset;
                return nbucket;
            }
#if 0
            else if (offset > 10) {
                const auto bucket1 = (bucket + offset*offset) & _mask;
                if (MAX_LEN(_pairs, bucket1) == State::INACTIVE) {
                    max_probe_length = offset * offset;
                    return bucket1;
                }
            }
#endif
        }
    }

    int find_main_bucket(const KeyT& key)
    {
        const auto bucket = BUCKET(key);
        auto& max_probe_length = MAX_LEN(_pairs,bucket);
        if (max_probe_length == State::INACTIVE)
            return bucket;

        //find main postion
        const auto& bucket_key = EMH_KEY(_pairs,bucket);
        const auto main_bucket = BUCKET(bucket_key);
        if (main_bucket != bucket) {
            kickout_bucket(main_bucket, bucket);
            max_probe_length = State::INACTIVE;
            return bucket;
        }

        for (int offset = max_probe_length + 1; ; offset += HOPS) {
            const auto nbucket = (bucket + offset) & _mask;
            if (MAX_LEN(_pairs,nbucket) == State::INACTIVE) {
                max_probe_length = offset;
                return nbucket;
            }
        }
    }

    // key is not in this map. Find a place to put it.
    inline int find_empty_bucket(int bucket_from)
    {
        for (auto offset = 1; ; offset += HOPS) {
            const auto nbucket = (bucket_from + offset) & _mask;
            if (MAX_LEN(_pairs, nbucket) == State::INACTIVE)
                return nbucket;
#if 0
            if (offset > 10 + 64 / sizeof (PairT)) {
                const auto bucket1 = (bucket_from + offset*offset) & _mask;
                if (MAX_LEN(_pairs, bucket1) == State::INACTIVE)
                    return bucket1;
            }
#endif
        }
    }

private:

    HashT   _hasher;
    EqT     _eq;
    PairT*  _pairs            ;
    size_t  _num_buckets      ;
    size_t  _num_filled       ;
    size_t  _mask             ;  // _num_buckets minus one
};

} // namespace emhash
