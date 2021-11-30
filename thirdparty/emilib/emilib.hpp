// By Emil Ernerfeldt 2014-2017
// LICENSE:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.

#pragma once

#include <cstdlib>
#include <iterator>
#include <utility>


#ifdef _MSC_VER
#  include <intrin.h>
#  include <zmmintrin.h>
#else
#  include <x86intrin.h>
#endif

//#include <loguru.hpp>

namespace emilib {

#define KEYHASH_MASK(key_hash) (((key_hash >> 24) << 1) | State::FILLED)

#ifndef AVX2_EHASH
    const static auto simd_empty  = _mm_set1_epi8(1);
    const static auto simd_erased = _mm_set1_epi8(3);
    constexpr static uint8_t simd_gaps = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8      _mm_set1_epi8
    #define LOADU_EPI8     _mm_loadu_si128
    #define MOVEMASK_EPI8  _mm_movemask_epi8
    #define CMPEQ_EPI8     _mm_cmpeq_epi8
#elif 1
    const static auto simd_empty  = _mm256_set1_epi8(1);
    const static auto simd_erased = _mm256_set1_epi8(3);
    constexpr static uint8_t simd_gaps = sizeof(simd_empty) / sizeof(uint8_t);
    #define SET1_EPI8      _mm256_set1_epi8
    #define LOADU_EPI8     _mm256_loadu_si256
    #define MOVEMASK_EPI8  _mm256_movemask_epi8
    #define CMPEQ_EPI8     _mm256_cmpeq_epi8
#elif AVX512_EHASH
    const static auto simd_empty  = _mm512_set1_epi8(1);
    const static auto simd_erased = _mm512_set1_epi8(3);
    constexpr static uint8_t simd_gaps = sizeof(simd_empty) / sizeof(uint8_t);
    #define SET1_EPI8      _mm512_set1_epi8
    #define LOADU_EPI8     _mm512_loadu_si512
    #define MOVEMASK_EPI8  _mm512_movemask_epi8 //avx512 error
    #define CMPEQ_EPI8     _mm512_test_epi8_mask
#else
	//TODO arm neon
#endif

//find filled or empty
constexpr static uint8_t stat_bits = sizeof(uint8_t) * 8;
constexpr static uint8_t stat_gaps = sizeof(uint64_t) / sizeof(uint8_t);

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
    typedef ValueT mapped_type;
    typedef ValueT val_type;
    typedef KeyT   key_type;

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = size_t;
        using value_type        = std::pair<KeyT, ValueT>;
        using pointer           = value_type*;
        using reference         = value_type&;

//        iterator() { }

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
            _bucket = _map->find_filled_slot(_bucket + 1);
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
        using difference_type   = size_t;
        using value_type        = const std::pair<KeyT, ValueT>;
        using pointer           = value_type*;
        using reference         = value_type&;

//        const_iterator() { }

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
            _bucket = _map->find_filled_slot(_bucket + 1);
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
        insert_unique(other.cbegin(), other.cend());
    }

    HashMap(HashMap&& other)
    {
        if (this != &other) {
        *this = std::move(other);
        other.clear();
        }
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
        insert_unique(other.cbegin(), other.cend());
        }
        return *this;
    }

    void operator=(HashMap&& other)
    {
        if (this != &other) {
        swap(other);
        other.clear();
        }
    }

    ~HashMap()
    {
        if (!is_triviall_destructable() && _num_filled) {
        for (size_t bucket=0; bucket<_num_buckets; ++bucket) {
            if (_states[bucket] % 2 == State::FILLED) {
                _pairs[bucket].~PairT();
            }
        }
        }
        _num_filled = 0;
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
        std::swap(_max_probe_length, other._max_probe_length);
        std::swap(_mask,             other._mask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        if (_num_filled == 0)
            return { this, _num_buckets };

        size_t bucket = find_filled_slot(0);
        return { this, bucket };
    }

    const_iterator cbegin() const
    {
        if (_num_filled == 0)
            return { this, _num_buckets };

        size_t bucket = find_filled_slot(0);
        return { this, bucket };
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return { this, _num_buckets };
    }

    const_iterator cend() const
    {
        return { this, _num_buckets };
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
        return _num_filled / static_cast<float>(_num_buckets);
    }

    void max_load_factor(float lf)
    {
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
        return &_pairs[bucket].second;
    }

    /// Const version of the above
    template<typename KeyLike>
    ValueT* try_get(const KeyLike& k) const
    {
        auto bucket = find_filled_bucket(k);
        return &_pairs[bucket].second;
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

        const auto key_hash = _hasher(key);
        const auto bucket = find_or_allocate(key, key_hash);

        if (_states[bucket] % 2 == State::FILLED) {
            return { iterator(this, bucket), false };
        } else {
            _states[bucket] = KEYHASH_MASK(key_hash);
            new(_pairs + bucket) PairT(key, value); _num_filled++;
            return { iterator(this, bucket), true };
        }
    }

    std::pair<iterator, bool> insert(const KeyT& key, ValueT&& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_or_allocate(key, key_hash);

        if (_states[bucket] % 2 == State::FILLED) {
            return { iterator(this, bucket), false };
        }
        else {
            _states[bucket] = KEYHASH_MASK(key_hash);
            new(_pairs + bucket) PairT(key, std::move(value)); _num_filled++;
            return { iterator(this, bucket), true };
        }
    }

    std::pair<iterator, bool> emplace(const KeyT& key, const ValueT& value)
    {
        return insert(key, value);
    }

    std::pair<iterator, bool> emplace(const KeyT& key, ValueT&& value)
    {
        return insert(key, std::move(value));
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

    void insert_unique(const_iterator begin, const_iterator end)
    {
        // TODO: reserve space exactly once.
        for (; begin != end; ++begin) {
            insert_unique(begin->first, begin->second);
        }
    }

    /// Same as above, but contains(key) MUST be false
    void insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_empty_slot(key_hash & _mask, 0);

        _states[bucket] = KEYHASH_MASK(key_hash);
        new(_pairs + bucket) PairT(key, value); _num_filled++;
    }

    void insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_empty_slot(key_hash & _mask, 0);

        _states[bucket] = KEYHASH_MASK(key_hash);
        new(_pairs + bucket) PairT(std::move(key), std::move(value)); _num_filled++;
    }

    void insert_unique(std::pair<KeyT, ValueT>&& p)
    {
        insert_unique(std::move(p.first), std::move(p.second));
    }

    void insert_unique(std::pair<KeyT, ValueT>& p)
    {
        insert_unique(p.first, p.second);
    }

    void insert_or_assign(const KeyT& key, ValueT&& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_or_allocate(key, key_hash);

        // Check if inserting a new value rather than overwriting an old entry
        if (_states[bucket] % 2 == State::FILLED) {
            _pairs[bucket].second = value;
        } else {
            _states[bucket] = KEYHASH_MASK(key_hash);
            new(_pairs + bucket) PairT(key, value); _num_filled++;
        }
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& new_value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_or_allocate(key, key_hash);

        // Check if inserting a new value rather than overwriting an old entry
        if (_states[bucket] % 2 == State::FILLED) {
            ValueT old_value = _pairs[bucket].second;
            _pairs[bucket] = new_value.second;
            return old_value;
        } else {
            _states[bucket] = KEYHASH_MASK(key_hash);
            new(_pairs + bucket) PairT(key, new_value); _num_filled++;
            return ValueT();
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_or_allocate(key, key_hash);

        /* Check if inserting a new value rather than overwriting an old entry */
        if (_states[bucket] % 2 != State::FILLED) {
            _states[bucket] = KEYHASH_MASK(key_hash);
            new(_pairs + bucket) PairT(key, ValueT()); _num_filled++;
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
            _states[bucket] = State::ERASED;
            _pairs[bucket].~PairT();
            _num_filled -= 1;
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
        _states[it._bucket] = State::ERASED;
        _pairs[it._bucket].~PairT();
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
                if (_states[bucket] % 2 == State::FILLED) {
                    _pairs[bucket].~PairT();
                    _num_filled --;
                }
                _states[bucket] = State::EMPTY;
            }
        } else if (_num_filled)
            std::fill_n(_states, _num_buckets, State::EMPTY);

        _num_filled = 0;
        _max_probe_length = -1;
    }

    /// Make room for this many elements
    void reserve(size_t num_elems)
    {
        size_t required_buckets = num_elems + num_elems / 8 + 2;
        if (required_buckets <= _num_buckets) {
            return;
        }
        size_t num_buckets = 4;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto new_states = (uint8_t*)malloc((simd_gaps + num_buckets) * sizeof(uint8_t));
        auto new_pairs  = (PairT*)malloc((num_buckets + 1) * sizeof(PairT));

        auto old_num_filled  = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_states      = _states;
        auto old_pairs       = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = _num_buckets - 1;
        _states      = new_states;
        _pairs       = new_pairs;

        std::fill_n(_states, num_buckets, State::EMPTY);
        //find empty tombstone
        std::fill_n(_states + num_buckets, simd_gaps / 2, State::FILLED + 4);
        //find filled tombstone
        std::fill_n(_states + num_buckets + simd_gaps / 2, simd_gaps / 2, State::EMPTY + 4);
        //fill last packet zero
        memset(new_pairs + num_buckets, 0, sizeof(new_pairs[0]));

        _max_probe_length = -1;

        for (size_t src_bucket=0; _num_filled < old_num_filled; src_bucket++) {
            if (old_states[src_bucket] % 2 == State::FILLED) {
                auto& src_pair = old_pairs[src_bucket];

                const auto key_hash = _hasher(src_pair.first);
                const auto dst_bucket = find_empty_slot(key_hash & _mask, 0);

                _states[dst_bucket] = KEYHASH_MASK(key_hash);
                new(_pairs + dst_bucket) PairT(std::move(src_pair));
                _num_filled += 1;
                src_pair.~PairT();
            }
        }

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
    template<typename KeyLike>
    size_t find_filled_bucket(const KeyLike& key) const
    {
        const auto key_hash = _hasher(key);
        auto next_bucket = (size_t)(key_hash & _mask);
        if (0 && _max_probe_length < simd_gaps / 4) {
            for (int offset = 0; offset <= _max_probe_length; ) {
                if (_states[next_bucket] % 2 == State::FILLED) {
                    if (_eq(_pairs[next_bucket].first, key)) {
                        return next_bucket;
                    }
                }
                else if (_states[next_bucket] % 4 == State::EMPTY) {
                    return _num_buckets; // End of the chain!
                }
                next_bucket = (key_hash + ++offset) & _mask;
            }
        } else {
            const char keymask = KEYHASH_MASK(key_hash);
            const auto filled = SET1_EPI8(keymask);
            const auto round  = next_bucket + _max_probe_length;

            for (auto i = next_bucket; i <= round; i += simd_gaps) {
                const auto vec = LOADU_EPI8((decltype(&simd_empty))((char*)_states + next_bucket));
                auto mask1 = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));

                while (mask1 != 0) {
                    const auto fbucket = next_bucket + CTZ(mask1);
                    if (fbucket < _num_buckets && _eq(_pairs[fbucket].first, key))
                        return fbucket;
                    mask1 &= mask1 - 1;
                }

                if (_max_probe_length >= simd_gaps) {
                const auto mask2 = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
                if (mask2 != 0)
                    break;
                }
//
                next_bucket += simd_gaps;
                if (next_bucket >= _num_buckets) {
                    i -= next_bucket - _num_buckets;
                    next_bucket = 0;
                 }
            }
        }
        return _num_buckets;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    template<typename KeyLike>
    size_t find_or_allocate(const KeyLike& key, size_t key_hash)
    {
        size_t hole = (size_t)-1;
        int offset = 0;
        if (0 && _max_probe_length <= simd_gaps / 2) {
            for (; offset <= _max_probe_length; ++offset) {
                auto bucket = (key_hash + offset) & _mask;
                if (_states[bucket] % 2 == State::FILLED) {
                    if (_eq(_pairs[bucket].first, key)) {
                        return bucket;
                    }
                }
                else if (_states[bucket] % 4 == State::EMPTY) {
                    return bucket;
                }
                else if (hole == (size_t)-1) {
                    hole = bucket;
                }
            }

            if (hole != (size_t)-1)
                return hole;
            return find_empty_slot((key_hash + offset) & _mask, offset);
        }

        const char keymask = KEYHASH_MASK(key_hash);
        const auto filled = SET1_EPI8(keymask);
        const auto bucket = (size_t)(key_hash & _mask);
        const auto round  = bucket + _max_probe_length;
        auto next_bucket  = bucket, i = bucket;

        for (; i <= round; i += simd_gaps) {
            const auto vec  = LOADU_EPI8((decltype(&simd_empty))((char*)_states + next_bucket));
            auto mask1  = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));

            //1. find filled
            while (mask1 != 0) {
                const auto fbucket = next_bucket + CTZ(mask1);
                if (fbucket < _num_buckets && _eq(_pairs[fbucket].first, key))
                    return fbucket;
                mask1 &= mask1 - 1;
            }

            //2. find empty
            const auto mask2 = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (mask2 != 0) {
                const auto ebucket = next_bucket + CTZ(mask2);
                int diff = ebucket - bucket;
                if (diff > _max_probe_length)
                    _max_probe_length = diff;
                else if (diff < 0 && _num_buckets + diff > _max_probe_length)
                    _max_probe_length = _num_buckets + diff;
                return ebucket;
            }

            //3. find erased
            if (hole == -1) {
                const auto mask3 = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_erased));
                if (mask3 != 0)
                    hole = next_bucket + CTZ(mask3);
            }

            //4. next round
            next_bucket += simd_gaps;
            if (next_bucket >= _num_buckets) {
                i -= next_bucket - _num_buckets;
                next_bucket = 0;
            }
        }

        if (hole != (size_t)-1)
            return hole;
        return find_empty_slot(next_bucket, i - bucket);
    }

    size_t find_empty_slot(size_t next_bucket, int offset)
    {
        constexpr uint64_t EMPTY_MASK = stat_bits == 16 ? 0x0001000100010001ull : 0x0101010101010101ull;

        while (true) {
            const auto bmask = *(uint64_t*)(_states + next_bucket) & EMPTY_MASK;
            if (bmask != 0) {
                const auto probe = CTZ(bmask) / stat_bits;
                offset += probe;
                if (offset > _max_probe_length)
                    _max_probe_length = offset;
                const auto ebucket = next_bucket + probe;
                return ebucket;
            }
            next_bucket += stat_gaps;
            offset += stat_gaps;
            if (next_bucket >= _num_buckets) {
                offset -= (next_bucket - _num_buckets);
                next_bucket = 0;
            }
        }
        return 0;
    }

    size_t find_filled_slot(size_t next_bucket) const
    {
#if 0
        if (_num_filled > _num_buckets / 2) {
            while ((_states[next_bucket++] % 2 != State::FILLED));
            return next_bucket - 1;
        }
#endif

        constexpr uint64_t FILLED_MASK = 0xfefefefefefefefeull;
        while (next_bucket < _num_buckets) {
            const auto bmask = ~(*(uint64_t*)(_states + next_bucket) | FILLED_MASK);
            if (bmask != 0)
                return next_bucket + CTZ(bmask) / stat_bits;;
            next_bucket += stat_gaps;
        }
        return _num_buckets;
    }

private:
    enum State : uint8_t
    {
        FILLED   = 0, // Is set with key/value
        ERASED   = 3, // Is inside a search-chain, but is empty
        EMPTY    = 1, // Never been touched empty
    };

    HashT   _hasher;
    EqT     _eq;
    uint8_t*_states           = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      =  0;
    size_t  _num_filled       =  0;
    size_t  _mask             =  0;  // _num_buckets minus one
    int     _max_probe_length = -1; // Our longest bucket-brigade is this long. ONLY when we have zero elements is this ever negative (-1).
};

} // namespace emilib
