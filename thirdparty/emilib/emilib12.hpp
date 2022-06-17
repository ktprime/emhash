// By Emil Ernerfeldt 2014-2017
// LICENSE:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.

#pragma once

#include <cstdlib>
#include <iterator>
#include <utility>
#include <cstring>

namespace emlib2 {

/// like std::equal_to but no need to #include <functional>
template<typename T>
struct HashMapEqualTo
{
    constexpr bool operator()(const T& lhs, const T& rhs) const
    {
        return lhs == rhs;
    }
};

constexpr uint32_t probe_limit = (1 << ((sizeof(uint16_t) * 8) - 2)) - 1;

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = HashMapEqualTo<KeyT>>
class HashMap
{
private:
    using MyType = HashMap<KeyT, ValueT, HashT, EqT>;

    using PairT = std::pair<KeyT, ValueT>;
public:
    using size_t          = uint32_t;
    using value_type      = PairT;
    using reference       = PairT&;
    using const_reference = const PairT&;
    typedef ValueT mapped_type;
    typedef ValueT val_type;
    typedef KeyT   key_type;

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::pair<KeyT, ValueT>;
        using pointer           = value_type*;
        using reference         = value_type&;

        iterator() { }

        iterator(MyType* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket)
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
            return _map->_pairs[_bucket];
        }

        pointer operator->() const
        {
            return _map->_pairs + _bucket;
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
            } while (_map->_states[_bucket].flag_probe.flag != State::FILLED);
        }

    //private:
    //    friend class MyType;
    public:
        MyType* _map;
        size_t  _bucket;
    };

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = const std::pair<KeyT, ValueT>;
        using pointer           = value_type*;
        using reference         = value_type&;

        const_iterator() { }

        const_iterator(iterator proto) : _map(proto._map), _bucket(proto._bucket)
        {
        }

        const_iterator(const MyType* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket)
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
            return _map->_pairs[_bucket];
        }

        pointer operator->() const
        {
            return _map->_pairs + _bucket;
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
            } while (_map->_states[_bucket].flag_probe.flag != State::FILLED);
        }

    //private:
    //    friend class MyType;
    public:
        const MyType* _map;
        size_t        _bucket;
    };

    // ------------------------------------------------------------------------

    HashMap() = default;

    HashMap(int n)
    {
        reserve(n);
    }

    HashMap(const HashMap& other)
    {
        reserve(other.size());
        insert(other.cbegin(), other.cend());
    }

    HashMap(HashMap&& other)
    {
        *this = std::move(other);
         other._num_filled = 0;
    }

    HashMap(std::initializer_list<std::pair<KeyT, ValueT>> il)
    {
        for (auto begin = il.begin(); begin != il.end(); ++begin)
            insert(*begin);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this != &other) {
        clear();
        reserve(other.size());
        insert(other.cbegin(), other.cend());
        }
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
        if (!is_triviall_destructable()) {
            for (size_t bucket=0; _num_filled; ++bucket) {
                if (is_filled(bucket)) {
                    _pairs[bucket].~PairT();
                    _num_filled --;
                }
            }
        }

        free(_states);
        free(_pairs);
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher,           other._hasher);
        std::swap(_eq,               other._eq);
        std::swap(_states,           other._states);
        std::swap(_pairs,            other._pairs);
        std::swap(_num_buckets,      other._num_buckets);
        std::swap(_num_filled,       other._num_filled);
        std::swap(_mask,             other._mask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        if (_num_filled == 0)
            return {this, _num_buckets};

        size_t bucket = 0;
        while (!is_filled(bucket)) {
            ++bucket;
        }
        return iterator(this, bucket);
    }

    const_iterator cbegin() const
    {
        if (_num_filled == 0)
            return {this, _num_buckets};

        size_t bucket = 0;
        while (!is_filled(bucket)) {
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
        return _num_filled / static_cast<float>(_mask + 1);
    }

    void max_load_factor(float lf)
    {
    }

    inline void set_empty(size_t bucket)
    {
        _states[bucket].flag_probe.flag = State::INACTIVE;
    }

    inline void set_filled(size_t bucket)
    {
        _states[bucket].flag_probe.flag = State::FILLED;
    }

    inline bool is_filled(size_t bucket) const
    {
        return _states[bucket].flag_probe.flag == State::FILLED;
    }

    // ------------------------------------------------------------

    template<typename KeyLike>
    iterator find(const KeyLike& key)
    {
        return iterator(this, find_filled_bucket(key));
    }

    template<typename KeyLike>
    const_iterator find(const KeyLike& key) const
    {
        return const_iterator(this, find_filled_bucket(key));
    }

    template<typename KeyLike>
    bool contains(const KeyLike& k) const
    {
        return find_filled_bucket(k) != _num_buckets;
    }

    template<typename KeyLike>
    size_t count(const KeyLike& k) const
    {
        return find_filled_bucket(k) != _num_buckets;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    template<typename KeyLike>
    ValueT* try_get(const KeyLike& k)
    {
        auto bucket = find_filled_bucket(k);
        if (bucket != _num_buckets) {
            return &_pairs[bucket].second;
        } else {
            return nullptr;
        }
    }

    /// Const version of the above
    template<typename KeyLike>
    ValueT* try_get(const KeyLike& k) const
    {
        auto bucket = find_filled_bucket(k);
        if (bucket != _num_buckets) {
            return &_pairs[bucket].second;
        } else {
            return nullptr;
        }
    }

    /// Convenience function.
    template<typename KeyLike>
    ValueT get_or_return_default(const KeyLike& k) const
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
        check_expand_need();

        auto bucket = find_or_allocate(key);

        if (is_filled(bucket)) {
            return { iterator(this, bucket), false };
        } else {
            set_filled(bucket);
            new(_pairs + bucket) PairT(key, value);
            _num_filled++;
            return { iterator(this, bucket), true };
        }
    }

    std::pair<iterator, bool> emplace(const KeyT& key, const ValueT& value)
    {
        return insert(key, value);
    }

    std::pair<iterator, bool> insert(const std::pair<KeyT, ValueT>& p)
    {
        return insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(iterator it, const std::pair<KeyT, ValueT>& p)
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
    void insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        auto bucket = find_empty_bucket(key);
        set_filled(bucket);
        new(_pairs + bucket) PairT(std::move(key), std::move(value));
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
        if (is_filled(bucket)) {
            _pairs[bucket].second = value;
        } else {
            set_filled(bucket);
            new(_pairs + bucket) PairT(key, value);
            _num_filled++;
        }
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& new_value)
    {
        check_expand_need();
        auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (is_filled(bucket)) {
            ValueT old_value = _pairs[bucket].second;
            _pairs[bucket] = new_value.second;
            return old_value;
        } else {
            set_filled(bucket);
            new(_pairs + bucket) PairT(key, new_value);
            _num_filled++;
            return ValueT();
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();

        auto bucket = find_or_allocate(key);

        /* Check if inserting a new value rather than overwriting an old entry */
        if (!is_filled(bucket)) {
            set_filled(bucket);
            new(_pairs + bucket) PairT(key, ValueT());
            _num_filled++;
        }

        return _pairs[bucket].second;
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    bool erase(const KeyT& key)
    {
        auto bucket = find_filled_bucket(key);
        if (bucket != _num_buckets) {
            _pairs[bucket].~PairT();
            _num_filled -= 1;
            if (_states[bucket].get_probe() == 1)
                _states[bucket].clear();
            else
                set_empty(bucket);
            return true;
        } else {
            return false;
        }
    }

    /// Erase an element using an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
        //DCHECK_EQ_F(it._map, this);
        //DCHECK_LT_F(it._bucket, _num_buckets);
        _pairs[it._bucket].~PairT();
        if (_states[it._bucket].get_probe() == 1)
            _states[it._bucket].clear();
        else
            set_empty(it._bucket);

        _num_filled -= 1;
        return ++it;
    }

    static constexpr bool is_triviall_destructable()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600 || __clang__
        return (std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (!is_triviall_destructable()) {
        for (size_t bucket=0; _num_filled; ++bucket) {
            if (is_filled(bucket)) {
                _pairs[bucket].~PairT();
                _num_filled --;
            }
        }
        }

        _num_filled = 0;
        memset(_states, 0, sizeof(_states[0]) * _num_buckets);
    }

    /// Make room for this many elements
    void reserve(size_t num_elems)
    {
        size_t required_buckets = num_elems + num_elems / 7 + 2;
        if (required_buckets <= _num_buckets) {
            return;
        }
        size_t num_buckets = 4;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto new_states = (State*)malloc((1 + num_buckets) * sizeof(State));
        auto new_pairs  = (PairT*)malloc(num_buckets * sizeof(PairT));

        if (!new_states || !new_pairs) {
            free(new_states);
            free(new_pairs);
            throw std::bad_alloc();
        }

        //auto old_num_filled  = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_states      = _states;
        auto old_pairs       = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = _num_buckets - 1;
        _states      = new_states;
        _pairs       = new_pairs;

        memset(_states, 0, num_buckets * sizeof(_states[0]));
        set_filled(num_buckets);

        for (size_t src_bucket=0; src_bucket<old_num_buckets; src_bucket++) {
            if (old_states[src_bucket].flag_probe.flag == State::FILLED) {
                auto& src_pair = old_pairs[src_bucket];

                auto dst_bucket = find_empty_bucket(src_pair.first);
                //DCHECK_NE_F(dst_bucket, (size_t)-1);
                //DCHECK_NE_F(_states[dst_bucket], State::FILLED);
                set_filled(dst_bucket);

                new(_pairs + dst_bucket) PairT(std::move(src_pair));
                _num_filled += 1;

                src_pair.~PairT();
            }
        }

        ////DCHECK_EQ_F(old_num_filled, _num_filled);

        free(old_states);
        free(old_pairs);
    }

private:
    // Can we fit another element?
    void check_expand_need()
    {
        reserve(_num_filled);
    }

    // Find the bucket with this key, or return (size_t)-1
    size_t find_filled_bucket(const KeyT& key) const
    {
        const size_t main_bucket = _hasher(key) & _mask;
        uint32_t offset = _states[main_bucket].get_probe();
        while (offset > 0) {
            const auto bucket = (main_bucket + --offset) & _mask;
            if (is_filled(bucket) && _eq(_pairs[bucket].first, key)) {
                return bucket;
            }
        }
        return _num_buckets;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    size_t find_or_allocate(const KeyT& key)
    {
        const size_t main_bucket = _hasher(key) & _mask;
        const uint32_t cur_probe = _states[main_bucket].get_probe();
        if (cur_probe == 0 && !is_filled(main_bucket)) {
            _states[main_bucket].set_probe(1);
            return main_bucket;
        }
#if 0
        if (cur_probe == 0) {
            assert (!is_filled(main_bucket));
            _states[main_bucket].set_probe(1);
            return main_bucket;
        } else if (_eq(_pairs[main_bucket].first, key))
            return main_bucket;
#endif

        size_t hole = (size_t)-1, offset = 0;
        for (; offset < cur_probe; ++offset) {
            const auto bucket = (main_bucket + offset) & _mask;
            if (is_filled(bucket)) {
                if (_eq(_pairs[bucket].first, key))
                    return bucket;
            } else if (hole == (size_t)-1)
                hole = bucket;
        }

        if (hole != (size_t)-1) {
            return hole;
        }

        while (true) {
            const auto bucket = (main_bucket + offset ++) & _mask;
            if (!is_filled(bucket)) {
                _states[main_bucket].set_probe(offset);
                return bucket;
            }
        }
    }

    // key is not in this map. Find a place to put it.
    size_t find_empty_bucket(const KeyT& key)
    {
        const size_t main_bucket = _hasher(key) & _mask;
        for (size_t offset=_states[main_bucket].get_probe(); ; ) {
            const auto bucket = (main_bucket + offset++) & _mask;
            if (!is_filled(bucket)) {
                _states[main_bucket].set_probe(offset);
                return bucket;
            }
        }
    }

private:

    struct FLAG_PROBE
    {
        uint8_t  flag : 1;
        uint16_t probe : 15;
    };

    struct State
    {
        enum uint8_t
        {
            INACTIVE = 0, // Never been touched
            FILLED = 1    // Is set with key/value
        };

        FLAG_PROBE flag_probe;
        void set_probe(uint32_t probe)
        {
            flag_probe.probe = probe;
        }

        uint32_t get_probe() const
        {
            return flag_probe.probe;
        }

        void clear()
        {
            flag_probe.flag = INACTIVE;
            flag_probe.probe = 0;
        }
    };


    HashT   _hasher;
    EqT     _eq;
    State*  _states           = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _num_filled       = 0;
    size_t  _mask             = 0;  // _num_buckets minus one
};

} // namespace emilib
