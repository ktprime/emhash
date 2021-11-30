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

#if _WIN32
    #include <intrin.h>
#if _WIN64
    #pragma intrinsic(_umul128)
#endif
#endif

namespace emilib4 {

inline static uint32_t CTZ(uint64_t n)
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
#endif

    return (uint32_t)index;
}

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
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
            } while ((_map->_states[_bucket] & 1) == INACTIVE);
        }

    //private:
    //    friend class MyType;
    public:
        MyType* _map;
        size_t _bucket;
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
            } while ((_map->_states[_bucket] & 1) == INACTIVE);
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
        for (size_t bucket=0; bucket<_num_buckets; ++bucket) {
            if (is_filled(bucket)) {
                _pairs[bucket].~PairT();
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
        while (is_empty(bucket)) {
            ++bucket;
        }
        return iterator(this, bucket);
    }

    const_iterator cbegin() const
    {
        if (_num_filled == 0)
            return {this, _num_buckets};

        size_t bucket = 0;
        while (is_empty(bucket)) {
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

    inline void set_empty(size_t bucket) { _states[bucket] |= INACTIVE; }
    inline void set_filled(size_t bucket) { _states[bucket] &= (~INACTIVE); }
    inline bool is_filled(size_t bucket) const { return (_states[bucket] & 1) != INACTIVE; }
    inline bool is_empty(size_t bucket) const { return (_states[bucket] & 1) == INACTIVE; }

    void set_probe(uint32_t main_bucket, uint32_t probe)
    {
        auto& probe_flag = _states[main_bucket];
        if (probe < over_probe_)
            probe_flag = (probe << 1) + (probe_flag & 1);
        else {
            if (probe > max_probe_)
                max_probe_ = probe;
            probe_flag = (over_probe_ << 1) + (probe_flag & 1);
        }
    }

    inline uint32_t get_probe(uint32_t main_bucket) const
    {
        const auto probe = _states[main_bucket] >> 1;
        return (probe != over_probe_) ? probe : max_probe_;
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

    std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& value)
    {
        check_expand_need();
        auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (is_filled(bucket)) {
            _pairs[bucket].second = value;
            return { iterator(this, bucket), false };
        } else {
            set_filled(bucket);
            new(_pairs + bucket) PairT(key, value);
            _num_filled++;
            return { iterator(this, bucket), true };
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
        if (is_empty(bucket)) {
            set_filled(bucket);
            new(_pairs + bucket) PairT(key, ValueT());
            _num_filled++;
        }

        return _pairs[bucket].second;
    }

    // -------------------------------------------------------

    void clear_bucket(uint32_t bucket)
    {
        if (!is_triviall_destructable())
            _pairs[bucket].~PairT();
        set_empty(bucket);
        _num_filled --;
    }

    /// Erase an element from the hash table.
    /// return false if element was not found
    int erase(const KeyT& key)
    {
        const uint32_t main_bucket = _hasher(key) & _mask;
        uint32_t offset = get_probe(main_bucket);
        if (offset == 1) {
            if (_eq(_pairs[main_bucket].first, key)) {
                clear_bucket(main_bucket);
                _states[main_bucket] = INACTIVE;
                return 1;
            }
            return 0;
        } else if (offset == 0) {
            return 0;
        } else if (is_filled(main_bucket) && _eq(_pairs[main_bucket].first, key)) {
//            auto last_bucket = (main_bucket + offset - 1) & _mask;
//            std::swap(_pairs[main_bucket], _pairs[last_bucket])
//            clear_bucket(last_bucket); set_probe(offset - 1)
            clear_bucket(main_bucket);
            return 1;
        }

        auto last = offset - 1;
        while (offset > 1) {
            const auto bucket = (main_bucket + --offset) & _mask;
            if (is_filled(bucket) && _eq(_pairs[bucket].first, key)) {
                if (offset == last)
                    set_probe(main_bucket, offset);
                clear_bucket(bucket);
                return 1;
            }
        }

        return 0;
    }

    /// Erase an element using an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
        //DCHECK_EQ_F(it._map, this);
        //DCHECK_LT_F(it._bucket, _num_buckets);
        auto bucket = it._bucket;
        auto buckets = get_probe(bucket) - 1;
        if (buckets == 0)
            _states[bucket] = INACTIVE;
        else {
#if 0
            const auto main_bucket = _hasher(_pairs[bucket].first) & _mask;
            buckets = get_probe(main_bucket) - 1;
            const auto last_bucket = (main_bucket + buckets) & _mask;
            //last bucket
            if (last_bucket == bucket) {
                for (int i = buckets; i > 0; i--) {
                    set_probe(main_bucket,i);
                    const auto next_bucket = (main_bucket + i - 1) & _mask;
                    if (is_filled(next_bucket) && main_bucket == (_hasher(_pairs[next_bucket].first) & _mask))
                        break;
                }
            }
            /*****
            else if (buckets == 1) {
                std::swap(_pairs[bucket], _pairs[last_bucket]);
                bucket = last_bucket;
                set_probe(main_bucket,1);
            }
            ******/
#endif
            set_empty(bucket);
        }

        _pairs[bucket].~PairT();
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

        if (sizeof(_states[0]) == sizeof(uint8_t))
            memset(_states, 0x01010101, sizeof(_states[0]) * _num_buckets);
        else {
            for (size_t bucket=0; bucket<_num_buckets; bucket++)
                _states[bucket] = INACTIVE;
        }

        _num_filled = 0;
        max_probe_  = over_probe_;
    }

    /// Make room for this many elements
    void reserve(size_t num_elems)
    {
        size_t required_buckets = num_elems + num_elems / 7 + 2;
        if (required_buckets < _num_buckets) {
            return;
        }
        size_t num_buckets = 8;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto new_states = (uint8_t*)malloc((1 + num_buckets) * sizeof(_states[0]) + 2 * sizeof(uint64_t));
        auto new_pairs  = (PairT*)malloc((1 + num_buckets) * sizeof(PairT));

        if (!new_states || !new_pairs) {
            free(new_states);
            free(new_pairs);
            throw std::bad_alloc();
        }

        auto old_num_filled  = _num_filled;
        auto old_states      = _states;
        auto old_pairs       = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = _num_buckets - 1;
        _states      = new_states;
        _pairs       = new_pairs;
        max_probe_   = over_probe_;

        if (sizeof(_states[0]) == sizeof(uint8_t))
            memset(_states, 0x01010101, sizeof(_states[0]) * num_buckets);
        else {
            for (size_t bucket=0; bucket<_num_buckets; bucket++)
                _states[bucket] = INACTIVE;
        }

        for (size_t bucket=0; bucket < sizeof(uint64_t) / sizeof(_states[0]); bucket++)
            set_filled(bucket + num_buckets);

        for (size_t src_bucket=0; _num_filled < old_num_filled; src_bucket++) {
            if ((old_states[src_bucket] & 1) != INACTIVE) {
                auto& src_pair = old_pairs[src_bucket];

                auto dst_bucket = find_empty_bucket(src_pair.first);
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
    //
    size_t find_filled_bucket(const KeyT& key) const
    {
        const size_t main_bucket = _hasher(key) & _mask;
        uint32_t offset = get_probe(main_bucket);
        while (offset > 0) {
            const auto bucket = (main_bucket + --offset) & _mask;
            if (std::is_integral<KeyT>::value) {
                if (_eq(_pairs[bucket].first, key) && is_filled(bucket))
                    return bucket;
            } else if (is_filled(bucket) && _eq(_pairs[bucket].first, key)) {
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
        const uint32_t cur_probe = get_probe(main_bucket);
        if (cur_probe == 0) {
            if (is_empty(main_bucket)) {
                set_probe(main_bucket,1);
                return main_bucket;
            } else
                return find_empty_bucket(key);
        }

        size_t hole = (size_t)-1, offset = cur_probe - 1;
        for (; (int)offset >= 0; --offset) {
            const auto bucket = (main_bucket + offset) & _mask;
            if (std::is_integral<KeyT>::value) {
                if (_eq(_pairs[bucket].first, key) && is_filled(bucket)) {
                    return bucket;
                }// else if (is_empty(bucket))
                 //   hole = bucket;
            } else {
                if (is_filled(bucket) && _eq(_pairs[bucket].first, key)) {
                    return bucket;
                }// else if (hole == (size_t)-1)
                 //   hole = bucket;
            }
        }

//        if (hole != (size_t)-1) {
//            return hole;
//        }

        return find_empty_bucket(key);
    }

    // key is not in this map. Find a place to put it.
    size_t find_empty_bucket(const KeyT& key)
    {
        constexpr uint32_t STAT_BITS = sizeof(_states[0]) * 8;
        constexpr uint32_t STAT_SKIP = sizeof(uint64_t) / sizeof(_states[0]);
        constexpr uint64_t STAT_MASK = STAT_BITS == 16 ? 0x0001000100010001ull : 0x0101010101010101ull;

        const auto main_bucket = _hasher(key) & _mask;
        auto probe = get_probe(main_bucket);
        auto next_bucket = (main_bucket + probe) & _mask;

        for (; next_bucket % STAT_SKIP != 0; ) {
            if (is_empty(next_bucket)) {
                set_probe(main_bucket, probe + 1);
                return next_bucket;
            }
            next_bucket = (main_bucket + ++probe) & _mask;
        }

        while ( true ) {
            const auto bmask = *(uint64_t*)(_states + next_bucket) & STAT_MASK;
            if (bmask != 0) {
                probe += CTZ(bmask) / STAT_BITS;
                set_probe(main_bucket, probe + 1);
                return (main_bucket + probe) & _mask;
            }
            probe += STAT_SKIP;
            next_bucket = (next_bucket + STAT_SKIP) & _mask;
        }
    }

private:

    enum STAT
    {
        INACTIVE = 1, // Never been touched
    };

    HashT   _hasher;
    EqT     _eq;
    uint8_t* _states          = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _num_filled       = 0;
    size_t  _mask             = 0;  // _num_buckets minus one
    static constexpr uint32_t over_probe_ = (1 << (sizeof(_states[0]) * 8 - 1)) - 1; // 127
    uint32_t max_probe_       = over_probe_;
};

} // namespace emilib
