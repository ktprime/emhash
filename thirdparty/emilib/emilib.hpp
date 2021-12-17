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

namespace emilib {

    enum State : uint8_t
    {
        EFILLED  = 0x00,
        EEMPTY   = 0xFF, //0b1111'1111,
        EDELETE  = 0x80, //0b1000'0000,
        SENTINEL = 0x7E, //0b0111'1110
    };

    constexpr uint8_t  FILLED_MASK  = EDELETE;
    constexpr uint8_t  EMPTY_MASK   = EDELETE;
    constexpr uint64_t EFILLED_FIND = 0x7f7f7f7f7f7f7f7full;
    constexpr uint64_t EEMPTY_FIND  = 0x8080808080808080ull;

#ifndef AVX2_EHASH
    const static auto simd_empty  = _mm_set1_epi8(EEMPTY);
    const static auto simd_detele = _mm_set1_epi8(EDELETE);
    constexpr static uint8_t simd_gaps = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8      _mm_set1_epi8
    #define LOADU_EPI8     _mm_loadu_si128
    #define MOVEMASK_EPI8  _mm_movemask_epi8
    #define CMPEQ_EPI8     _mm_cmpeq_epi8
    #define CMPGT_EPI8     _mm_cmpgt_epi8
#elif __aarch64__ == 0
    const static auto simd_empty  = _mm256_set1_epi8(EEMPTY);
    const static auto simd_detele = _mm256_set1_epi8(EDELETE);
    constexpr static uint8_t simd_gaps = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8      _mm256_set1_epi8
    #define LOADU_EPI8     _mm256_loadu_si256
    #define MOVEMASK_EPI8  _mm256_movemask_epi8
    #define CMPEQ_EPI8     _mm256_cmpeq_epi8
    #define CMPGT_EPI8     _mm256_cmpgt_epi8
#elif AVX515
    const static auto simd_empty  = _mm512_set1_epi8(EEMPTY);
    const static auto simd_detele = _mm512_set1_epi8(EDELETE);
    constexpr static uint8_t simd_gaps = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8      _mm512_set1_epi8
    #define LOADU_EPI8     _mm512_loadu_si512
    #define MOVEMASK_EPI8  _mm512_movemask_epi8 //avx512 error
    #define CMPEQ_EPI8     _mm512_test_epi8_mask

#elif __aarch64__
    //https://github.com/DLTcollab/sse2neon/blob/master/sse2neon.h
    const static auto simd_empty  = vreinterpretq_s64_s8(vdupq_n_s8(EEMPTY));
    const static auto simd_detele = vreinterpretq_s64_s8(vdupq_n_s8(EDELETE));
    constexpr static uint8_t simd_gaps = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8(a)      vreinterpretq_s64_s8(vdupq_n_s8(a))
    #define LOADU_EPI8(a)     vreinterpretq_s64_s32(vld1q_s32((const int32_t *)a))
    int MOVEMASK_EPI8(__m128i _a)
    {
        uint8x16_t input = vreinterpretq_u8_s32(_a);
        static const int8_t xr[8] = { -7, -6, -5, -4, -3, -2, -1, 0 };
        uint8x8_t mask_and = vdup_n_u8(0x80);
        int8x8_t mask_shift = vld1_s8(xr);

        uint8x8_t lo = vget_low_u8(input);
        uint8x8_t hi = vget_high_u8(input);

        lo = vand_u8(lo, mask_and);
        lo = vshl_u8(lo, mask_shift);

        hi = vand_u8(hi, mask_and);
        hi = vshl_u8(hi, mask_shift);

        lo = vpadd_u8(lo, lo);
        lo = vpadd_u8(lo, lo);
        lo = vpadd_u8(lo, lo);

        hi = vpadd_u8(hi, hi);
        hi = vpadd_u8(hi, hi);
        hi = vpadd_u8(hi, hi);

        return ((hi[0] << 8) | (lo[0] & 0xFF));
    }

    #define CMPEQ_EPI8(a, b)  vreinterpretq_s64_u8(vceqq_s8(vreinterpretq_s8_s64(a), vreinterpretq_s8_s64(b)))
    #define CMPGT_EPI8(a, b)  vreinterpretq_s64_u8(vcgtq_s8(vreinterpretq_s8_s64(a), vreinterpretq_s8_s64(b)))
#endif


//arm

//find filled or empty
constexpr static uint8_t maxf_gaps = simd_gaps * 3;
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
    #define hash2_key(key_hash, key) ((uint8_t)(key_hash >> 25)) >> 1
#else
    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value, uint8_t>::type = 0>
    inline uint8_t hash2_key(uint64_t key_hash, const UType& key) const
    {
        return (uint8_t)(key_hash >> 25) >> 1;
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, uint8_t>::type = 0>
    inline uint8_t hash2_key(uint64_t key_hash, const UType& key) const
    {
#if _WIN32
        return (uint8_t)(_byteswap_uint64(((uint64_t)key) * 0xA24BAED4963EE407)) >> 1;
#else
        return (uint8_t)(__builtin_bswap64(((uint64_t)key) * 0xA24BAED4963EE407)) >> 1;
#endif
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
            _from = (_bucket / stat_gaps) * stat_gaps;
            if (_bucket < _map->bucket_count()) {
                _bmask = *(uint64_t*)(_map->_states + _from) | EFILLED_FIND;
                _bmask |= (1ull << (_bucket % stat_gaps * stat_gaps)) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        size_t operator - (iterator& r) const
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
            return _map->_pairs[_bucket];
        }

        pointer operator->() const
        {
            return _map->_pairs + _bucket;
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
                _bucket = _from + CTZ(_bmask) / stat_bits;
                return;
            }

            do {
                _bmask = ~(*(uint64_t*)(_map->_states + (_from += stat_gaps)) | EFILLED_FIND);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask) / stat_bits;
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

        explicit const_iterator(iterator it)
            : _map(it._map), _bucket(it._bucket), _bmask(it._bmask), _from(it._from) {}
        const_iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        const_iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { _bmask = _from = 0; }

        void init()
        {
            _from = (_bucket / stat_gaps) * stat_gaps;
            if (_bucket < _map->bucket_count()) {
                _bmask = *(uint64_t*)(_map->_states + _from) | EFILLED_FIND;
                _bmask |= (1ull << (_bucket % stat_gaps * stat_gaps)) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        size_t bucket() const
        {
            return _bucket;
        }

        size_t operator - (const_iterator& r) const
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
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(_bmask) / stat_bits;
                return;
            }

            do {
                _bmask = ~(*(uint64_t*)(_map->_states + (_from += stat_gaps)) | EFILLED_FIND);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask) / stat_bits;
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

    HashMap(std::initializer_list<std::pair<KeyT, ValueT>> il)
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
        if (!is_triviall_destructable())
            clear();

        _num_filled = 0;
        free(_states);
    }

    static constexpr bool is_copy_trivially()
    {
#if __cplusplus >= 201103L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
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
            memcpy(_pairs,  other._pairs,  _num_buckets * sizeof(_pairs[0]));
        } else {
            clear();
            reserve(other._num_buckets / 2);
            for (auto it = other.cbegin();  it != other.cend(); ++it)
                new(_pairs + it.bucket()) PairT(*it);
        }
        assert(_num_buckets == other._num_buckets);
        _num_filled = other._num_filled;
        _max_probe_length = other._max_probe_length;
        memcpy(_states, other._states, (_num_buckets + simd_gaps) * sizeof(_states[0]));
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
            return { this, _num_buckets, false };
        return { this, find_filled_slot(0) };
    }

    const_iterator cbegin() const
    {
        if (_num_filled == 0)
            return { this, _num_buckets, false };
        return { this, find_filled_slot(0) };
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return { this, _num_buckets, false };
    }

    const_iterator cend() const
    {
        return { this, _num_buckets, false };
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
        return ret ? *ret : ValueT();
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

        if ((_states[bucket] & FILLED_MASK) == State::EFILLED) {
            return { iterator(this, bucket, false), false };
        } else {
            _states[bucket] = hash2_key(key_hash, key);
            new(_pairs + bucket) PairT(key, value); _num_filled++;
            return { iterator(this, bucket, false), true };
        }
    }

    std::pair<iterator, bool> insert(const KeyT& key, ValueT&& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_or_allocate(key, key_hash);

        if ((_states[bucket] & FILLED_MASK) == State::EFILLED) {
            return { iterator(this, bucket, false), false };
        }
        else {
            _states[bucket] = hash2_key(key_hash, key);
            new(_pairs + bucket) PairT(key, std::move(value)); _num_filled++;
            return { iterator(this, bucket, false), true };
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
        const auto bucket = find_empty_slot(key_hash & _mask, 0);

        _states[bucket] = hash2_key(key_hash, key);
        new(_pairs + bucket) PairT(key, value); _num_filled++;
    }

    void insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_empty_slot(key_hash & _mask, 0);

        _states[bucket] = hash2_key(key_hash, key);
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
        if ((_states[bucket] & FILLED_MASK) == State::EFILLED) {
            _pairs[bucket].second = value;
        } else {
            _states[bucket] = hash2_key(key_hash, key);
            new(_pairs + bucket) PairT(key, value); _num_filled++;
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();

        const auto key_hash = _hasher(key);
        const auto bucket = find_or_allocate(key, key_hash);

        /* Check if inserting a new value rather than overwriting an old entry */
        if ((_states[bucket] & FILLED_MASK) != State::EFILLED) {
            _states[bucket] = hash2_key(key_hash, key);
            new(_pairs + bucket) PairT(key, ValueT()); _num_filled++;
        }
        return _pairs[bucket].second;
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

    iterator erase(const_iterator cit)
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

    void _erase(iterator it)
    {
        _erase(it._bucket);
    }

    void _erase(size_t bucket)
    {
        _num_filled -= 1;
        if (is_triviall_destructable())
            _pairs[bucket].~PairT();

        _states[bucket] = State::EDELETE;
#if 0
        auto state = _states[bucket] = (_states[bucket + 1] & EMPTY_MASK) == State::EEMPTY ? State::EEMPTY : State::EDELETE;
        if (state == State::EEMPTY) {
            while (bucket > 1 && _states[--bucket] == State::EDELETE)
                _states[bucket] = State::EEMPTY;
        }
#endif

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
            for (auto it = cbegin(); _num_filled; ++it) {
                _states[it.bucket()] = State::EEMPTY;
                (*it).~PairT();
                _num_filled --;
            }
        } else if (_num_filled)
            memset(_states, 0-1u, _num_buckets);

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

        auto num_buckets = _num_filled > (1u << 16) ? (1u << 16) : stat_gaps;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        const auto pairs_size = (num_buckets + 1) * sizeof(PairT);
        auto state_size = (simd_gaps + num_buckets) * sizeof(State::EFILLED);
//        state_size += (simd_gaps - state_size % simd_gaps) % 8;
        assert(state_size % 8 == 0);

        const auto* new_data  = (char*)malloc(pairs_size + state_size);
#if 1
        auto* new_state = (decltype(_states))new_data;
        auto* new_pairs = (decltype(_pairs))(new_data + state_size);
#else
        auto* new_pairs = (decltype(_pairs))new_data;
        auto* new_state = (decltype(_states))(new_data + pairs_size);
#endif

        auto old_num_filled  = _num_filled;
        auto old_num_buckets = _num_buckets;
        auto old_states      = _states;
        auto old_pairs       = _pairs;
        auto max_probe_length = _max_probe_length;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _states      = new_state;
        _pairs       = new_pairs;

        //init last packet zero
        memset(_pairs + num_buckets, 0, sizeof(_pairs[0]));

        //init empty
        std::fill_n(_states, num_buckets, State::EEMPTY);
        //memset(_states, 0-1u, num_buckets);

        //set delete tombstone for some use.
        for (auto index = 0; index < _num_buckets; index += simd_gaps)
            _states[index] = _states[index + simd_gaps / 2] = State::EDELETE;

        //set sentinel to deal with iterator
        std::fill_n(_states + num_buckets, simd_gaps, State::SENTINEL);

#if 0
        if constexpr (std::is_integral<KeyT>::value) {
            auto keymask = (hash2_key(_hasher(0), (KeyT)(0))) - 1;
            if ((keymask & EEMPTY) == State::EEMPTY)
                keymask = State::SENTINEL;
            std::fill_n(_states + num_buckets, simd_gaps, keymask);
        }
#endif

        _max_probe_length = -1;
        auto collisions = 0;

        for (size_t src_bucket=0; _num_filled < old_num_filled; src_bucket++) {
            if ((old_states[src_bucket] & FILLED_MASK) == State::EFILLED) {
                auto& src_pair = old_pairs[src_bucket];
#ifdef EMH_NRB
                if (EMH_UNLIKELY(src_bucket >= old_num_buckets))
                    break;
                const auto key_hash = _hasher(src_pair.first);
                const auto dst_bucket = key_hash & _mask;
                if ((_states[dst_bucket] & FILLED_MASK) == State::EFILLED) {
                    old_states[collisions] = State::EFILLED;
                    new(old_pairs + collisions ++) PairT(std::move(src_pair));
                    src_pair.~PairT();
                    continue;
                }
#else
                const auto key_hash = _hasher(src_pair.first);
                const auto dst_bucket = find_empty_slot(key_hash & _mask, 0);
                collisions += (_states[key_hash & _mask] & FILLED_MASK) == State::EFILLED;
#endif
                _states[dst_bucket] = hash2_key(key_hash, src_pair.first);
                new(_pairs + dst_bucket) PairT(std::move(src_pair));
                if (is_triviall_destructable())
                    src_pair.~PairT();
                old_states[src_bucket] = State::EDELETE;
                _num_filled += 1;
            }
        }

        if (_max_probe_length < 0 && _num_filled > 0)
            _max_probe_length = 0;

        //TODO: use find find_empty slot
        for (size_t src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
            if ((old_states[src_bucket] & FILLED_MASK) == State::EFILLED) {
                auto& src_pair = old_pairs[src_bucket];
                const auto key_hash = _hasher(src_pair.first);
#if 0
                const auto dst_bucket = find_empty_slot(key_hash & _mask, 0);
#else
                auto bucket = key_hash & _mask; int offset = 0;
                auto dst_bucket = find_empty_slot2(bucket, offset);
                if (offset > _max_probe_length && offset >= maxf_gaps) {
                    auto mbucket = robin_shift(bucket, dst_bucket, offset);
                    if (mbucket != -1)
                        dst_bucket = mbucket;
                } else
                    check_offset(offset);
#endif
                _states[dst_bucket] = hash2_key(key_hash, src_pair.first);
                new(_pairs + dst_bucket) PairT(std::move(src_pair));
                if (is_triviall_destructable())
                    src_pair.~PairT();
                _num_filled += 1;
            }
        }

#if EMH_DUMP
        if (_num_filled > 1000000)
            printf("\t\t\tmax_probe_length/_max_probe_length = %d/%d, collsions = %d, collisions = %.2f%%\n",
                max_probe_length, _max_probe_length, collisions, collisions * 100.0f / _num_buckets);
#endif

        free(old_states);
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

        if (0) {
            for (int offset = 0; offset <= _max_probe_length; ) {
                if ((_states[next_bucket] & FILLED_MASK) == State::EFILLED) {
                    if (_eq(_pairs[next_bucket].first, key)) {
                        return next_bucket;
                    }
                }
                else if ((_states[next_bucket] & EMPTY_MASK) == State::EEMPTY) {
                    return _num_buckets; // End of the chain!
                }
                next_bucket = (key_hash + ++offset) & _mask;
            }
        } else {
            const char keymask = hash2_key(key_hash, key);
            const auto filled = SET1_EPI8(keymask);
            /*****
            1. start at bucket h1 (mod n)
            2. load the Group of bytes starting at the current bucket
            3. search the Group for your key's h2 in parallel (match_byte)
            4. for each match, check if the keys are equal, return if found
            5. search the Group for an EEMPTY bucket in parallel (match_empty)
            6. if there was any matches, return false
            7. otherwise (every entry was FULL or ERASEDD), probe (mod n) and GOTO 2 **/

#if 0
            const auto headsize = next_bucket % simd_gaps;
            if (1) {
                next_bucket -= headsize;
                const auto head_mask = ~((1 << headsize) - 1);
                const auto vec = LOADU_EPI8((decltype(&simd_empty))(_states + next_bucket));
                auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled)) & head_mask;
                while (maskf != 0) {
                    const auto fbucket = next_bucket + CTZ(maskf);
                    if (!std::is_integral<KeyLike>::value && EMH_UNLIKELY(fbucket >= _num_buckets))
                        break; //overflow
                    if (_eq(_pairs[fbucket].first, key))
                        return fbucket;
                    maskf &= maskf - 1;
                }

                const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty)) & head_mask;
                if (maske != 0)
                    return _num_buckets;
                next_bucket += simd_gaps;
            }

            const int round = max_search_gap(next_bucket) - (simd_gaps - headsize) % simd_gaps;
            assert(next_bucket % simd_gaps == 0);
#endif
            int i = max_search_gap(next_bucket);
            //for (int i = round; i >= 0; i -= simd_gaps) {
            for ( ; ; ) {
                const auto vec = LOADU_EPI8((decltype(&simd_empty))(_states + next_bucket));
                auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));
                while (maskf != 0) {
                    const auto fbucket = next_bucket + CTZ(maskf);
                    if (EMH_UNLIKELY(fbucket >= _num_buckets))
                        break;
                    else if (EMH_LIKELY(_eq(_pairs[fbucket].first, key)))
                        return fbucket;
                    maskf &= maskf - 1;
                }

                const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
                if (maske != 0)
                    break;

                next_bucket += simd_gaps;
                if (EMH_UNLIKELY(next_bucket >= _num_buckets)) {
                    i += next_bucket - _num_buckets;
                    next_bucket = 0;
                }

                if (EMH_UNLIKELY((i -= simd_gaps) < 0))
                    break;
            }
        }
        return _num_buckets;
    }

    size_t robin_shift(size_t bucket, size_t next_bucket, int offset)
    {
        const size_t sbucket = bucket + _max_probe_length - 1;
        const size_t ebucket = next_bucket - _max_probe_length + 1;
        const size_t obucket = bucket + offset / 2 - simd_gaps / 4;
        for (size_t i = 0; i <= simd_gaps / 2; i++)
        {
            const auto mbucket = (obucket + i) & _mask;
            auto& mpairs = _pairs[mbucket];

            //TODO keep main bucket info or robin hood backshifts
            if ((_hasher(mpairs.first) & _mask) == mbucket) {
                new(_pairs + next_bucket) PairT(std::move(mpairs)); mpairs.~PairT();
                _states[next_bucket] = _states[mbucket]; //bugs
                _states[mbucket] = State::EEMPTY;

                //update new offset
                auto doffset1 = (mbucket - bucket + _num_buckets) & _mask;
                auto doffset2 = (next_bucket - mbucket + _num_buckets) & _mask;
                check_offset(std::max<size_t>(doffset1, doffset2));
                return mbucket;
            }
#if 1
            const auto kbucket = (sbucket - i) & _mask;
            auto& kpairs = _pairs[kbucket];

            //TODO keep main bucket info or robin hood backshifts
            if ((_hasher(kpairs.first) & _mask) == kbucket) {
                new(_pairs + next_bucket) PairT(std::move(kpairs)); kpairs.~PairT();
                _states[next_bucket] = _states[kbucket]; //bugs
                _states[kbucket] = State::EEMPTY;

                //update new offset
                const auto doffset = (next_bucket - kbucket + _num_buckets) & _mask;
                check_offset(doffset);
                return kbucket;
            }

            /*bucket ---> kbucket  nbucket ---> next_bucket */
            const auto nbucket = (ebucket + i) & _mask;
            auto& epairs = _pairs[nbucket];
            if ((_hasher(epairs.first) & _mask) == nbucket) {
                new(_pairs + next_bucket) PairT(std::move(epairs)); epairs.~PairT();
                _states[next_bucket] = _states[nbucket]; //bugs
                _states[nbucket] = State::EEMPTY;

                //update new offset
                const auto doffset = (nbucket - bucket + _num_buckets) & _mask;
                check_offset(doffset);
                return nbucket;
            }
#endif
        }
        return -1;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    template<typename KeyLike>
    size_t find_or_allocate(const KeyLike& key, uint64_t key_hash)
    {
        size_t hole = (size_t)-1;
        int offset = 0;
        if (0) {
            for (; offset <= _max_probe_length; ++offset) {
                auto bucket = (key_hash + offset) & _mask;
                if ((_states[bucket] & FILLED_MASK) == State::EFILLED) {
                    if (_eq(_pairs[bucket].first, key))
                        return bucket;
                }
                else if ((_states[bucket] & EMPTY_MASK) == State::EEMPTY)
                    return hole == -1 ? bucket : hole;
                else if (hole == (size_t)-1)
                    hole = bucket;
            }

            if (hole != (size_t)-1)
                return hole;
            return find_empty_slot((key_hash + offset) & _mask, offset);
        }

        //TODO:same
        const char keymask = hash2_key(key_hash, key);
        //else
        //const char keymask = hash2_key(key_hash, (KeyT&)key);

        const auto filled = SET1_EPI8(keymask);
        const auto bucket = (size_t)(key_hash & _mask);
        const auto round  = bucket + max_search_gap(bucket);
        auto next_bucket  = bucket, i = bucket;

        /*****
        https://gankra.github.io/blah/hashbrown-tldr/
        start at bucket h1 (mod n)
        1.load the Group of bytes starting at the current bucket
        2.search the Group for EEMPTY or ERASEDD in parallel (match_empty_or_deleted)
        3.if there was no match (unlikely), probe and GOTO 2
        4.otherwise, get the first match and enter the SMALL TABLE NASTY CORNER CASE ZONE
        5.check that the match (mod n) isn't FULL.  ***/
        for (; ;) {
            const auto vec = LOADU_EPI8((decltype(&simd_empty))(_states + next_bucket));

            auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));
            //1. find filled
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (EMH_UNLIKELY(fbucket >= _num_buckets))
                    break;
                else if (_eq(_pairs[fbucket].first, key))
                    return fbucket;
                maskf &= maskf - 1;
            }

            //2. find empty
            const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (maske != 0) {
                const auto ebucket = hole == -1 ? next_bucket + CTZ(maske) : hole;
                const int offset = (ebucket - bucket + _num_buckets) & _mask;
                check_offset(offset);
                return ebucket;
            }

            //3. find erased
            if (hole == -1) {
                const auto zero = MOVEMASK_EPI8(vec);
                if (zero != 0)
                    hole = next_bucket + CTZ(zero);
            }

            //4. next round
            next_bucket += simd_gaps;
            if (EMH_UNLIKELY(next_bucket >= _num_buckets)) {
                i -= next_bucket - _num_buckets;
                next_bucket = 0;
            }

            if (EMH_UNLIKELY((i += simd_gaps) >= round))
                break;
        }

        if (EMH_LIKELY(hole != (size_t)-1))
            return hole;


        offset = i - bucket;
#if 1
        next_bucket = find_empty_slot(next_bucket, offset);
#elif 1
        next_bucket = find_empty_slot2(next_bucket, offset);
#ifndef EMH_NRB
        if (_max_probe_length > maxf_gaps * 2) {
            //assert (EMH_UNLIKELY(offset > _max_probe_length));
            auto nbucket = robin_shift(bucket, next_bucket, offset);
            if (nbucket != (size_t)-1)
                return nbucket;
        }
#endif
        _max_probe_length = offset;
#endif
        return next_bucket;
    }

    size_t find_empty_slot2(size_t next_bucket, int& offset)
    {
        while (true) {
            const auto vec   = LOADU_EPI8((decltype(&simd_empty))(_states + next_bucket));
            const auto maske = MOVEMASK_EPI8(vec);
            if (EMH_LIKELY(maske != 0)) {
                const auto probe = CTZ(maske);
                offset      += probe;
                next_bucket += probe;
                return next_bucket;
            }
            next_bucket += simd_gaps;
            offset      += simd_gaps;
            if (next_bucket >= _num_buckets) {
                offset -= next_bucket - _num_buckets;
                next_bucket = 0;
            }
        }
        return 0;
    }

    size_t find_empty_slot(size_t next_bucket, int offset)
    {
        while (true) {
            const auto maske = *(uint64_t*)(_states + next_bucket) & EEMPTY_FIND;
            if (EMH_LIKELY(maske != 0)) {
                const auto probe = CTZ(maske) / stat_bits;
                offset      += probe;
                next_bucket += probe;
                check_offset(offset);
                return next_bucket;
            }
            next_bucket += stat_gaps;
            offset      += stat_gaps;
            if (next_bucket >= _num_buckets) {
                offset -= next_bucket - _num_buckets;
                next_bucket = 0;
            }
        }
        return 0;
    }

    size_t find_filled_slot(size_t next_bucket) const
    {
#if 0
        if (_num_filled * 4 > _num_buckets) {
            while ((_states[next_bucket++] & FILLED_MASK) != State::EFILLED);
            return next_bucket - 1;
        }
#endif
        while (true) {
            const auto maske = ~(*(uint64_t*)(_states + next_bucket) | EFILLED_FIND);
            if (EMH_LIKELY(maske != 0))
                return next_bucket + CTZ(maske) / stat_bits;
            next_bucket += stat_gaps;
        }
        return _num_buckets;
    }

    inline void check_offset(int offset)
    {
        if (EMH_UNLIKELY(offset > _max_probe_length))
            _max_probe_length = offset;
    }

    inline int max_search_gap(size_t bucket) const
    {
        return _max_probe_length;
    }

private:

    HashT   _hasher;
    EqT     _eq;
    uint8_t*_states           = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _mask             = 0; // _num_buckets minus one
    size_t  _num_filled       = 0;
    int     _max_probe_length = -1; // Our longest bucket-brigade is this long. ONLY when we have zero elements is this ever negative (-1).
};

} // namespace emilib
