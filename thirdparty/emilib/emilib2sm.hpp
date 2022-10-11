// By Emil Ernerfeldt 2014-2017
// LICENSE:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.

#pragma once

#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>
#include <cassert>

#ifdef _MSC_VER
#  include <intrin.h>
#ifndef __clang__
#  include <zmmintrin.h>
#endif
#else
#  include <x86intrin.h>
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMH_LIKELY(condition)   __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition)   condition
#    define EMH_UNLIKELY(condition) condition
#endif

namespace emilib2 {

    enum State : uint8_t
    {
        EFILLED   = 0,
        EDELETE   = 3,
        EEMPTY    = 1,
        PACK_STAT = EDELETE + EEMPTY,
    };

#ifndef AVX2_EHASH
    const static auto simd_empty  = _mm_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm_set1_epi8(EDELETE);
    const static auto simd_filled = _mm_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm_set1_epi8
    #define LOAD_UEPI8     _mm_load_si128
    #define LOAD_EMPTY(u)  _mm_and_si128(_mm_load_si128(u), simd_empty)
    #define MOVEMASK_EPI8  _mm_movemask_epi8
    #define CMPEQ_EPI8     _mm_cmpeq_epi8
#elif 1
    const static auto simd_empty  = _mm256_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm256_set1_epi8(EDELETE);
    const static auto simd_filled = _mm256_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm256_set1_epi8
    #define LOAD_UEPI8     _mm256_loadu_si256
    #define LOAD_EMPTY(u)  _mm256_and_si256(_mm256_loadu_si256(u), simd_empty)
    #define MOVEMASK_EPI8  _mm256_movemask_epi8
    #define CMPEQ_EPI8     _mm256_cmpeq_epi8
#elif AVX512_EHASH
    const static auto simd_empty  = _mm512_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm512_set1_epi8(EDELETE);
    const static auto simd_filled = _mm512_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm512_set1_epi8
    #define LOAD_UEPI8     _mm512_loadu_si512
    #define MOVEMASK_EPI8  _mm512_movemask_epi8 //avx512 error
    #define CMPEQ_EPI8     _mm512_test_epi8_mask
#else
    //TODO sse2neon
#endif

//find filled or empty
constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);
constexpr static uint8_t stat_bytes = simd_bytes;

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
    if ((uint32_t)n)
        _BitScanForward(&index, (uint32_t)n);
    else
        {_BitScanForward(&index, n >> 32); index += 32; }
    #endif
#elif defined (__LP64__) || (SIZE_MAX == UINT64_MAX) || defined (__x86_64__)
    uint32_t index = __builtin_ctzll(n);
#elif 1
    uint32_t index = __builtin_ctzl(n);
#endif

    return (uint32_t)index;
}

#define EMH_PROBE(id) _ps[id / simd_bytes]._probe
#define EMH_STATE(id) _ps[id / simd_bytes]._stats[id % simd_bytes]
#define EMH_PAIRS(id) _ps[id / simd_bytes]._kvval[id % simd_bytes]


/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
private:
    using htype = HashMap<KeyT, ValueT, HashT, EqT>;

    using PairT = std::pair<KeyT, ValueT>;
public:
    using size_t          = uint32_t;
    using value_type      = PairT;
    using reference       = PairT&;
    using const_reference = const PairT&;
    typedef ValueT mapped_type;
    typedef ValueT val_type;
    typedef KeyT   key_type;

#ifdef EMH_H2
    #define key_2hash(key_hash, key) ((uint8_t)(key_hash >> 24)) << 1
    //#define key_2hash(key_hash, key) (((uint8_t)(key_hash >> 57)) << 1)
#else
    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value, uint8_t>::type = 0>
    inline uint8_t key_2hash(uint64_t key_hash, const UType& key) const
    {
        return (uint8_t)(key_hash >> 28) << 1;
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, uint8_t>::type = 0>
    inline uint8_t key_2hash(uint64_t key_hash, const UType& key) const
    {
        return (uint8_t)((uint64_t)key * 0x9FB21C651E98DF25ull >> 52) << 1;
    }
#endif

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<KeyT, ValueT>;
        using pointer           = value_type*;
        using reference         = value_type&;

        iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { _bmask = _from = 0; }

        void init()
        {
            _from = (_bucket / simd_bytes) * simd_bytes;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= ~((1ull << (_bucket % simd_bytes)) - 1);
            } else {
                _bmask = 0;
            }
        }

        size_t operator - (const iterator& r) const
        {
            return _bucket - r._bucket;
        }

        size_t bucket() const
        {
            return _bucket;
        }

        iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const
        {
            return _map->EMH_PAIRS(_bucket);
        }

        pointer operator->() const
        {
            return &(_map->EMH_PAIRS(_bucket));
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
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = _map->filled_mask(_from += simd_bytes);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype*  _map;
        uint64_t      _bmask;
        size_t        _bucket;
        size_t        _from;
    };

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = const std::pair<KeyT, ValueT>;
        using pointer           = value_type*;
        using reference         = value_type&;

        explicit const_iterator(const iterator& it)
            : _map(it._map), _bucket(it._bucket), _bmask(it._bmask), _from(it._from) {}
        const_iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        const_iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { _bmask = _from = 0; }

        void init()
        {
            _from = (_bucket / simd_bytes) * simd_bytes;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= ~((1ull << (_bucket % simd_bytes)) - 1);
            } else {
                _bmask = 0;
            }
        }

        size_t bucket() const
        {
            return _bucket;
        }

        size_t operator - (const const_iterator& r) const
        {
            return _bucket - r._bucket;
        }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            const_iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const
        {
            return _map->EMH_PAIRS(_bucket);
        }

        pointer operator->() const
        {
            return _map->EMH_PAIRS(_bucket);
        }

        bool operator==(const const_iterator& rhs) const
        {
            //DCHECK_EQ_F(_map, rhs._map);
            return _bucket == rhs._bucket;
        }

        bool operator!=(const const_iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = _map->filled_mask(_from += simd_bytes);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype*  _map;
        uint64_t      _bmask;
        size_t        _bucket;
        size_t        _from;
    };

    // ------------------------------------------------------------------------

    HashMap(size_t n = 4)
    {
        rehash(n);
    }

    HashMap(const HashMap& other)
    {
        clone(other);
    }

    HashMap(HashMap&& other)
    {
        if (this != &other) {
            swap(other);
        }
    }

    HashMap(std::initializer_list<value_type> il)
    {
        reserve(il.size());
        for (auto it = il.begin(); it != il.end(); ++it)
            insert(*it);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this != &other)
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
        if (is_triviall_destructable())
            clear();

        _num_filled = 0;
        free(_ps);
    }

    void clone(const HashMap& other)
    {
        if (other.size() == 0) {
            clear();
            return;
        }

        _hasher     = other._hasher;
        if (is_copy_trivially()) {
            _num_filled = _num_buckets = 0;
            reserve(other._num_buckets / 2);
            memcpy(_ps, other._ps, (_num_buckets / simd_bytes) * sizeof(Group_KVS) + simd_bytes);
        } else {
            clear();

            reserve(other._num_buckets / 2);
            for (auto it = other.cbegin(); it.bucket() != _num_buckets; ++it)
                new(&EMH_PAIRS(it.bucket())) PairT(*it);

            for (int i = 0; i < _num_buckets / simd_bytes; i++) {
                memcpy(_ps[i]._stats, other._ps[i]._stats, State::EEMPTY);
                //_ps[i]._probe = other._ps[i]._probe;
            }
        }

        _num_filled = other._num_filled;
        _max_probe_length = other._max_probe_length;
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher,           other._hasher);
        std::swap(_eq,               other._eq);
        std::swap(_ps,               other._ps);
        std::swap(_num_buckets,      other._num_buckets);
        std::swap(_num_filled,       other._num_filled);
        std::swap(_max_probe_length, other._max_probe_length);
        std::swap(_mask,             other._mask);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        if (_num_filled == 0)
            return {this, _num_buckets, false};
        return {this, find_filled_slot(0)};
    }

    const_iterator cbegin() const
    {
        if (_num_filled == 0)
            return {this, _num_buckets, false};
        return {this, find_filled_slot(0)};
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return {this, _num_buckets, false};
    }

    const_iterator cend() const
    {
        return {this, _num_buckets, false};
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

    void max_load_factor(float lf = 8.0f/9)
    {
    }

    // ------------------------------------------------------------

    template<typename KeyLike>
    iterator find(const KeyLike& key)
    {
        return iterator(this, find_filled_bucket(key), false);
    }

    template<typename KeyLike>
    const_iterator find(const KeyLike& key) const
    {
        return const_iterator(this, find_filled_bucket(key), false);
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
        return &(EMH_PAIRS(bucket).second);
    }

    /// Const version of the above
    template<typename KeyLike>
    ValueT* try_get(const KeyLike& k) const
    {
        auto bucket = find_filled_bucket(k);
        return &(EMH_PAIRS(bucket).second);
    }

    /// Convenience function.
    template<typename KeyLike>
    ValueT get_or_return_default(const KeyLike& k) const
    {
        const ValueT* ret = try_get(k);
        return ret ? *ret : ValueT();
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key, const ValueT& value)
    {
        check_expand_need();

        bool bnofind = true;
        const auto bucket = find_or_allocate(key, bnofind);
        if (bnofind) {
            new(&EMH_PAIRS(bucket)) PairT(key, value); _num_filled++;
        }

        return { iterator(this, bucket, false), bnofind };
    }

    std::pair<iterator, bool> insert(const KeyT& key, ValueT&& value)
    {
        check_expand_need();
        bool bnofind = true;
        const auto bucket = find_or_allocate(key, bnofind);

        if (bnofind) {
            new(&EMH_PAIRS(bucket)) PairT(key, std::move(value)); _num_filled++;
        }
        return { iterator(this, bucket, false), bnofind };
    }

    std::pair<iterator, bool> emplace(const KeyT& key, const ValueT& value)
    {
        return insert(key, value);
    }

    std::pair<iterator, bool> emplace(const KeyT& key, ValueT&& value)
    {
        return insert(key, std::move(value));
    }

    std::pair<iterator, bool> emplace(value_type&& v)
    {
        return insert(std::move(v.first), std::move(v.second));
    }

    std::pair<iterator, bool> emplace(const value_type& v)
    {
        return insert(v.first, v.second);
    }

    std::pair<iterator, bool> insert(const value_type& p)
    {
        return insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(iterator it, const value_type& p)
    {
        return insert(p.first, p.second);
    }

    void insert(const_iterator beginc, const_iterator endc)
    {
        reserve(endc - beginc + _num_filled);
        for (; beginc != endc; ++beginc) {
            insert(beginc->first, beginc->second);
        }
    }

    void insert_unique(const_iterator beginc, const_iterator endc)
    {
        reserve(endc - beginc + _num_filled);
        for (; beginc != endc; ++beginc) {
            insert_unique(beginc->first, beginc->second);
        }
    }

    /// Same as above, but contains(key) MUST be false
    void insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto gbucket = key_hash & _mask;
        const auto bucket = find_empty_slot(gbucket, gbucket, 0);
        EMH_STATE(bucket) = key_2hash(key_hash, key);
        new(&EMH_PAIRS(bucket)) PairT(key, value); _num_filled++;
    }

    void insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto gbucket = key_hash & _mask;
        const auto bucket = find_empty_slot(gbucket, gbucket, 0);

        EMH_STATE(bucket) = key_2hash(key_hash, key);
        new(&EMH_PAIRS(bucket)) PairT(std::move(key), std::move(value)); _num_filled++;
    }

    void insert_unique(value_type&& p)
    {
        insert_unique(std::move(p.first), std::move(p.second));
    }

    void insert_unique(const value_type & p)
    {
        insert_unique(p.first, p.second);
    }

    void insert_or_assign(const KeyT& key, ValueT&& value)
    {
        check_expand_need();

        bool bnofind = true;
        const auto bucket = find_or_allocate(key, bnofind);
        auto& pair = EMH_PAIRS(bucket);

        // Check if inserting a new value rather than overwriting an old entry
        if (bnofind) {
            new(&pair) PairT(key, value); _num_filled++;
        } else {
            pair.second = value;
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();
        bool bnofind = true;
        const auto bucket = find_or_allocate(key, bnofind);
        auto& pair = EMH_PAIRS(bucket);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bnofind) {
            new(&pair) PairT(key, ValueT()); _num_filled++;
        }
        return pair.second;
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    size_t erase(const KeyT& key)
    {
        auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            return 0;

        _erase(bucket);
        return 1;
    }

    iterator erase(const const_iterator& cit)
    {
        _erase(cit._bucket);
        iterator it(cit);
        return ++it;
    }

    iterator erase(iterator it)
    {
        _erase(it._bucket);
        return ++it;
    }

    void _erase(iterator& it)
    {
        _erase(it._bucket);
    }

    uint8_t group_mask(size_t gbucket) const
    {
        return _ps[gbucket / simd_bytes]._stats[simd_bytes - 1] % 4;
    }

    uint64_t empty_delete(size_t gbucket) const
    {
        const auto vec = LOAD_EMPTY((decltype(&simd_empty))(_ps[gbucket / simd_bytes]._stats));
        return MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
    }

    uint64_t filled_mask(size_t gbucket) const
    {
        const auto vec = LOAD_EMPTY((decltype(&simd_empty))(_ps[gbucket / simd_bytes]._stats));
        return MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_filled));
    }

    void _erase(size_t bucket)
    {
        _num_filled -= 1;
        if (is_triviall_destructable())
            EMH_PAIRS(bucket).~PairT();

        const auto gbucket = bucket - bucket % simd_bytes;
        EMH_STATE(bucket) = group_mask(gbucket) == State::EEMPTY ? State::EEMPTY : State::EDELETE;
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
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (is_triviall_destructable()) {
            for (auto it = begin();  it.bucket() != _num_buckets; ++it) {
                const auto bucket = it.bucket();
                EMH_STATE(bucket) = State::EEMPTY;
                EMH_PAIRS(bucket).~PairT();
            }
        } else {
            for (int i = 0; i < _num_buckets / simd_bytes; i++) {
                std::fill_n(_ps[i]._stats, simd_bytes, State::EEMPTY);
            }
        }

        _num_filled = 0;
        _max_probe_length = -1;
    }

    void shrink_to_fit()
    {
        rehash(_num_filled);
    }

    bool reserve(size_t num_elems)
    {
        size_t required_buckets = num_elems + num_elems / 8;
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    /// Make room for this many elements
    void rehash(size_t num_elems)
    {
        const size_t required_buckets = num_elems;
        if (EMH_UNLIKELY(required_buckets < _num_filled))
            return;

        auto num_buckets = _num_filled > (1u << 16) ? (1u << 16) : stat_bytes;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        const auto pairs_size = (num_buckets / simd_bytes) * sizeof(Group_KVS) + stat_bytes;
        auto* new_ps = (Group_KVS*)(malloc(pairs_size));

        auto old_num_filled  = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_ps          = _ps;
        auto max_probe_length= _max_probe_length;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _ps          = new_ps;

        //init empty tombstone
        for (int i = 0; i < _num_buckets / simd_bytes; i++) {
            std::fill_n(_ps[i]._stats, simd_bytes, State::EEMPTY);
        }

        std::fill_n(_ps[_num_buckets / simd_bytes]._stats, simd_bytes, State::EFILLED + PACK_STAT);

        _max_probe_length = -1;
        auto collision = 0;

        for (size_t src_bucket=0; _num_filled < old_num_filled; src_bucket++) {
            if (old_ps[src_bucket / simd_bytes]._stats[src_bucket % simd_bytes] % 2 == State::EFILLED) {
                auto& src_pair = old_ps[src_bucket / simd_bytes]._kvval[src_bucket % simd_bytes];
                const auto key_hash = _hasher(src_pair.first);
                const auto dst_bucket = find_empty_only(key_hash & _mask);

                EMH_STATE(dst_bucket) = key_2hash(key_hash, src_pair.first);
                new(&EMH_PAIRS(dst_bucket)) PairT(std::move(src_pair));
                _num_filled += 1;
                src_pair.~PairT();
            }
        }

#if EMH_DUMP
        if (_num_filled > 1000000)
            printf("\t\t\tmax_probe_length/_max_probe_length = %d/%d, collsions = %d, collision = %.2f%%\n",
                    max_probe_length, _max_probe_length, collision, collision * 100.0f / _num_buckets);
#endif

        free(old_ps);
    }

private:
    // Can we fit another element?
    void check_expand_need()
    {
        reserve(_num_filled);
    }

    // Find the bucket with this key, or return (size_t)-1
    template<typename KeyLike> size_t find_filled_bucket(const KeyLike& key) const
    {
        const auto key_hash = _hasher(key);
        auto next_bucket = (size_t)(key_hash & _mask);
        next_bucket -= next_bucket % simd_bytes;

        const auto filled = SET1_EPI8(key_2hash(key_hash, key));
        int i = _max_probe_length;

        auto* state = _ps[next_bucket / simd_bytes]._stats;

        for ( ; ; ) {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty)(state)));
            auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));

            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(EMH_PAIRS(fbucket).first, key))
                    return fbucket;
                maskf &= maskf - 1;
            }

            if (group_mask(next_bucket) == State::EEMPTY)
                break;
            else if (EMH_UNLIKELY((i -= simd_bytes) < 0))
                break;

            next_bucket = (next_bucket + simd_bytes);
            state += sizeof(Group_KVS);
            if (next_bucket >= _num_buckets)
                state = _ps[next_bucket = 0]._stats;
        }

        return _num_buckets;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    template<typename KeyLike> size_t find_or_allocate(const KeyLike& key, bool& bnew)
    {
        const auto key_hash = _hasher(key);
        const auto key_h2 = key_2hash(key_hash, key);
        auto bucket = (size_t)(key_hash & _mask);
        bucket -= bucket % simd_bytes;

        const auto filled = SET1_EPI8(key_h2);
        const auto round  = bucket + _max_probe_length;
        auto next_bucket  = bucket, i = bucket;
        size_t hole = (size_t)-1;

        auto* state = _ps[next_bucket / simd_bytes]._stats;

        for ( ; ; ) {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty))state);
            auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));

            //1. find filled
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(EMH_PAIRS(fbucket).first, key)) {
                    bnew = false;
                    return fbucket;
                }
                maskf &= maskf - 1;
            }

            //2. find empty
            //if (group_mask(next_bucket) == State::EEMPTY) {
            const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (maske != 0) {
                const auto ebucket = hole == -1 ? next_bucket + CTZ(maske) : hole;
                const int offset = (ebucket - bucket + _num_buckets) & _mask;
                if (EMH_UNLIKELY(offset > _max_probe_length))
                    _max_probe_length = offset;

                EMH_STATE(ebucket) = key_h2;
                return ebucket;
            }

            //3. find erased
            else if (hole == -1) {
                const auto maskd = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_delete));
                if (maskd != 0)
                    hole = next_bucket + CTZ(maskd);
            }

            if (EMH_UNLIKELY((i += simd_bytes) > round))
                break;

            //4. next round
            next_bucket = (next_bucket + simd_bytes);
            state += sizeof(Group_KVS);
            if (next_bucket >= _num_buckets)
                state = _ps[next_bucket = 0]._stats;
        }

        if (EMH_LIKELY(hole != (size_t)-1)) {
            EMH_STATE(hole) = key_h2;
            return hole;
        }

        const auto ebucket = find_empty_slot(bucket, next_bucket, i - bucket);
        EMH_STATE(ebucket) = key_h2;
        return ebucket;
    }

    size_t find_empty_slot(size_t gbucket, size_t next_bucket, int offset)
    {
        next_bucket -= next_bucket % simd_bytes;
        while (true) {
            const auto maske = empty_delete(next_bucket);
            if (EMH_LIKELY(maske != 0)) {
                const auto probe = CTZ(maske);
                offset += probe;
                if (EMH_UNLIKELY(offset > _max_probe_length))
                    _max_probe_length = offset;
                return next_bucket + probe;
            }
            offset += simd_bytes;
            next_bucket = (next_bucket + simd_bytes) & _mask;
        }
        return 0;
    }

    size_t find_empty_only(size_t next_bucket)
    {
        int offset = 0;

        auto gbucket = next_bucket - next_bucket % simd_bytes;
        next_bucket  = gbucket;

        while (true) {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty))(_ps[next_bucket / simd_bytes]._stats));
            const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (EMH_LIKELY(maske != 0)) {
                const auto probe = CTZ(maske);
                offset += probe;
                if (EMH_UNLIKELY(offset > _max_probe_length))
                    _max_probe_length = offset;
                return next_bucket + probe;
            }
            offset      += simd_bytes;
            next_bucket = (next_bucket + simd_bytes) & _mask;
        }
        return 0;
    }

    size_t find_filled_slot(size_t next_bucket) const
    {
        next_bucket -= next_bucket % simd_bytes;
        while (true) {
            auto maske = filled_mask(next_bucket);
            if (EMH_LIKELY(maske != 0))
                return next_bucket + CTZ(maske);
            next_bucket += simd_bytes;
        }
        return 0;
    }

private:

    HashT   _hasher;
    EqT     _eq;

    struct Group_KVS {
        uint8_t  _stats[simd_bytes];
        PairT    _kvval[simd_bytes];
    };

    Group_KVS *_ps            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _mask             = 0; // _num_buckets minus one
    size_t  _num_filled       = 0;
    int     _max_probe_length = -1; // Our longest bucket-brigade is this long. ONLY when we have zero elements is this ever negative (-1).
};

}; // namespace emilib
