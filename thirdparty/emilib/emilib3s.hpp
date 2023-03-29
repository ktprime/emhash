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

#undef EMH_LIKELY
#undef EMH_UNLIKELY

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
        EFILLED  = 0,
        EDELETE  = 3,
        EEMPTY   = 1,
        SENTINEL = EFILLED + EDELETE + EEMPTY + 0xE0,
    };

#ifndef AVX2_EHASH
    const static auto simd_empty  = _mm_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm_set1_epi8(EDELETE);
    const static auto simd_filled = _mm_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm_set1_epi8
    #define LOAD_UEPI8     _mm_load_si128
    #define LOAD_EMPTY(u)  _mm_and_si128(_mm_load_si128(u), simd_empty)
    #define LOAD_EMPTY2(u) _mm_slli_epi16(_mm_load_si128(u), 7)
    #define MOVEMASK_EPI8  _mm_movemask_epi8
    #define CMPEQ_EPI8     _mm_cmpeq_epi8
#elif 1
    const static auto simd_empty  = _mm256_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm256_set1_epi8(EDELETE);
    const static auto simd_filled = _mm256_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm256_set1_epi8
    #define LOAD_UEPI8     _mm256_loadu_si256
    #define LOAD_EMPTY(u)  _mm256_and_si256(_mm256_loadu_si256(u), simd_empty)
    #define LOAD_EMPTY2(u) _mm256_slli_epi32(_mm256_loadu_si256(u), 7)
    #define MOVEMASK_EPI8  _mm256_movemask_epi8
    #define CMPEQ_EPI8     _mm256_cmpeq_epi8
#elif AVX512_EHASH
    const static auto simd_empty  = _mm512_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm512_set1_epi8(EDELETE);
    const static auto simd_filled = _mm512_set1_epi8(EFILLED);
    #define LOAD_EMPTY(u)  _mm512_and_si512(_mm512_loadu_si512(u), simd_empty)
    #define LOAD_EMPTY2(u) _mm512_slli_epi64(_mm512_loadu_si512(u), 7)

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
#elif EMH_H3
    #define key_2hash(key_hash, key) (uint8_t)(key_hash & 0xFE)
#else
    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value, uint8_t>::type = 0>
    inline uint8_t key_2hash(uint64_t key_hash, const UType& key) const
    {
        (void)key;
        return (uint8_t)(key_hash >> 28) << 1;
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, uint8_t>::type = 0>
    inline uint8_t key_2hash(uint64_t key_hash, const UType& key) const
    {
        (void)key_hash;
        return (uint8_t)((uint64_t)key * 0x9FB21C651E98DF25ull >> 50) << 1;
    }
#endif

    class const_iterator;
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<KeyT, ValueT>;
        using pointer           = value_type*;
        using reference         = value_type&;

        iterator() {}
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

        reference operator*() const { return _map->_pairs[_bucket]; }
        pointer operator->() const { return _map->_pairs + _bucket; }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

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

        reference operator*() const { return _map->_pairs[_bucket]; }
        pointer operator->() const { return _map->_pairs + _bucket; }

        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

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

    HashMap(size_t n = 4) noexcept
    {
        rehash(n);
    }

    HashMap(const HashMap& other) noexcept
    {
        clone(other);
    }

    HashMap(HashMap&& other) noexcept
    {
        rehash(1);
        if (this != &other) {
            swap(other);
        }
    }

    HashMap(std::initializer_list<value_type> il) noexcept
    {
        reserve(il.size());
        for (auto it = il.begin(); it != il.end(); ++it)
            insert(*it);
    }

    template<class InputIt>
    HashMap(InputIt first, InputIt last, size_t bucket_count = 4) noexcept
    {
        reserve(std::distance(first, last) + bucket_count);
        for (; first != last; ++first)
            insert(*first);
    }

    HashMap& operator=(const HashMap& other) noexcept
    {
        if (this != &other)
            clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept
    {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap() noexcept
    {
        if (is_triviall_destructable())
            clear();

        _num_filled = 0;
        free(_states);
        free(_pairs);
    }

    void clone(const HashMap& other) noexcept
    {
        if (other.size() == 0) {
            clear();
            return;
        }
        if (is_triviall_destructable()) {
            clear();
        }

        if (other._num_buckets != _num_buckets) {
            _num_filled = _num_buckets = 0;
            reserve(other._num_buckets / 2);
        }

        if (is_copy_trivially()) {
            memcpy(_pairs, other._pairs, _num_buckets * sizeof(_pairs[0]));
        } else {
            for (auto it = other.cbegin(); it.bucket() != _num_buckets; ++it)
                new(_pairs + it.bucket()) PairT(*it);
        }

        //assert(_num_buckets == other._num_buckets);
        _num_filled = other._num_filled;
        memcpy(_states, other._states, (_num_buckets + simd_bytes) * sizeof(_states[0]));
    }

    void swap(HashMap& other) noexcept
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

    iterator begin() noexcept
    {
        if (_num_filled == 0)
            return {this, _num_buckets, false};
        return {this, find_filled_slot(0)};
    }

    const_iterator cbegin() const noexcept
    {
        if (_num_filled == 0)
            return {this, _num_buckets, false};
        return {this, find_filled_slot(0)};
    }

    const_iterator begin() const noexcept
    {
        return cbegin();
    }

    iterator end() noexcept
    {
        return {this, _num_buckets, false};
    }

    const_iterator cend() const noexcept
    {
        return {this, _num_buckets, false};
    }

    const_iterator end() const noexcept
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
        return _num_filled / static_cast<float>(_num_buckets);
    }

    float max_load_factor(float lf = 8.0f/9)
    {
        return 7/8.0f;
    }

    // ------------------------------------------------------------

    template<typename K>
    iterator find(const K& key) noexcept
    {
        return {this, find_filled_bucket(key), false};
    }

    template<typename K>
    const_iterator find(const K& key) const noexcept
    {
        return {this, find_filled_bucket(key), false};
    }

    template<typename K>
    bool contains(const K& k) const noexcept
    {
        return find_filled_bucket(k) != _num_buckets;
    }

    template<typename K>
    size_t count(const K& k) const noexcept
    {
        return find_filled_bucket(k) != _num_buckets;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    template<typename K>
    ValueT* try_get(const K& k)
    {
        auto bucket = find_filled_bucket(k);
        return &_pairs[bucket].second;
    }

    /// Const version of the above
    template<typename K>
    ValueT* try_get(const K& k) const
    {
        auto bucket = find_filled_bucket(k);
        return &_pairs[bucket].second;
    }

    void merge(HashMap& rhs)
    {
        if (empty()) {
            *this = std::move(rhs);
            return;
        }

        for (auto rit = rhs.begin(); rit != rhs.end(); ) {
            auto fit = find(rit->first);
            if (fit.bucket() > _mask) {
                insert_unique(rit->first, std::move(rit->second));
                rit = rhs.erase(rit);
            } else {
                ++rit;
            }
        }
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    template<typename K, typename V>
    std::pair<iterator, bool> do_insert(K&& key, V&& val) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);

        if (bempty) {
            new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        }
        return { {this, bucket, false}, bempty };
    }

    std::pair<iterator, bool> do_insert(const value_type& value) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        if (bempty) {
            new(_pairs + bucket) PairT(value); _num_filled++;
        }
        return { {this, bucket, false}, bempty };
    }

    std::pair<iterator, bool> do_insert(value_type&& value) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        if (bempty) {
            new(_pairs + bucket) PairT(std::move(value)); _num_filled++;
        }
        return { {this, bucket, false}, bempty };
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args) noexcept
    {
        return do_insert(std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert(value_type&& value) noexcept
    {
        return do_insert(std::move(value));
    }

    std::pair<iterator, bool> insert(const value_type& value) noexcept
    {
        return do_insert(value);
    }

    iterator insert(iterator hint, const value_type& value) noexcept
    {
        (void)hint;
        return do_insert(value).first;
    }

#if 0
    template <typename Iter>
    void insert(Iter beginc, Iter endc)
    {
        reserve(endc - beginc + _num_filled);
        for (; beginc != endc; ++beginc)
            do_insert(beginc->first, beginc->second);
    }
#endif

    void insert(std::initializer_list<value_type> ilist) noexcept
    {
        reserve(ilist.size() + _num_filled);
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            do_insert(*it);
    }

    template<typename K, typename V>
    size_t insert_unique(K&& key, V&& val) noexcept
    {
        check_expand_need();

        const auto key_hash = H1(key);
        const auto main_bucket = key_hash & _mask;

        const auto gbucket = main_bucket - main_bucket % simd_bytes;
        const auto bucket = find_empty_slot(gbucket, gbucket, 0);

        set_states(bucket, key_2hash(key_hash, key));
        new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        return bucket;
    }

    template<typename K, typename V>
    void insert_or_assign(K&& key, V&& val) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);

        // Check if inserting a new value rather than overwriting an old entry
        if (bempty) {
            new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        } else {
            _pairs[bucket].second = std::forward<V>(val);
        }
    }

    bool set_get(const KeyT& key, const ValueT& val, ValueT& oldv)
    {
        check_expand_need();

        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bempty) {
            new(_pairs + bucket) PairT(key,val); _num_filled++;
        } else
            oldv = _pairs[bucket].second;
        return bempty;
    }

    ValueT& operator[](const KeyT& key) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bempty) {
            new(_pairs + bucket) PairT(key, std::move(ValueT())); _num_filled++;
        }

        return _pairs[bucket].second;
    }

    ValueT& operator[](KeyT&& key) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        if (bempty) {
            new(_pairs + bucket) PairT(std::move(key), std::move(ValueT())); _num_filled++;
        }

        return _pairs[bucket].second;
    }

    // -------------------------------------------------------

    /// Erase an element from the hash table.
    /// return false if element was not found
    size_t erase(const KeyT& key) noexcept
    {
        auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            return 0;

        _erase(bucket);
        return 1;
    }

    iterator erase(const const_iterator& cit) noexcept
    {
        _erase(cit._bucket);
        iterator it(cit);
        return ++it;
    }

    iterator erase(iterator it) noexcept
    {
        _erase(it._bucket);
        return ++it;
    }

    void _erase(iterator& it) noexcept
    {
        _erase(it._bucket);
    }

    inline uint8_t group_mask(size_t gbucket) const noexcept
    {
        //assert(gbucket % simd_bytes == 0);
        return _states[gbucket + simd_bytes - 1] % 4;
    }

    inline uint8_t group_probe(size_t gbucket) const noexcept
    {
        return _states[gbucket + simd_bytes - 1] >> 2;
    }

    void set_probe(size_t gbucket, size_t probe)
    {
        assert(probe < 64);
        const auto pbucket = gbucket / simd_bytes * simd_bytes + simd_bytes - 1;
        if (EMH_LIKELY(probe > (_states[pbucket] >> 2)))
            _states[pbucket] = (probe << 2) | (_states[pbucket] % 4);
    }

    void set_states(size_t ebucket, uint8_t key_h2)
    {
        if (EMH_LIKELY(ebucket % simd_bytes != simd_bytes - 1))
            _states[ebucket] = key_h2;
        else
            _states[ebucket] &= 0b11111100;
    }

    void _erase(size_t bucket) noexcept
    {
        _num_filled -= 1;
        if (is_triviall_destructable())
            _pairs[bucket].~PairT();

        const auto gbucket = bucket / simd_bytes * simd_bytes;
        const auto is_empty = group_mask(gbucket) == State::EEMPTY;
        if (is_empty)
            _states[bucket] = State::EEMPTY;
        else if (bucket % simd_bytes != simd_bytes - 1)
            _states[bucket] = State::EDELETE;
        else
            _states[bucket] = (_states[bucket] & 0b11111100) | State::EDELETE;
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
    void clear() noexcept
    {
        if (is_triviall_destructable()) {
            for (auto it = begin(); it.bucket() != _num_buckets; ++it) {
                const auto bucket = it.bucket();
                _states[bucket] = State::EEMPTY;
                _pairs[bucket].~PairT();
            }
        } else if (_num_filled)
            std::fill_n(_states, _num_buckets, State::EEMPTY);

        _num_filled = 0;
    }

    void shrink_to_fit()
    {
        rehash(_num_filled + 1);
    }

    bool reserve(size_t num_elems) noexcept
    {
        size_t required_buckets = num_elems + num_elems / 8;
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    /// Make room for this many elements
    void rehash(size_t num_elems) noexcept
    {
        const size_t required_buckets = num_elems;
        if (EMH_UNLIKELY(required_buckets < _num_filled))
            return;

        auto num_buckets = _num_filled > (1u << 16) ? (1u << 16) : stat_bytes;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        const auto pairs_size = (num_buckets + 1) * sizeof(PairT);
        const auto state_size = (simd_bytes + num_buckets) * sizeof(State);

#if 1
        auto* new_state = (decltype(_states))malloc(state_size);
        auto* new_pairs = (decltype(_pairs)) malloc(pairs_size);
#endif

#if 0
        auto* new_state = (decltype(_states))new_data;
        auto* new_pairs = (decltype(_pairs))(new_data + state_size);
#elif 0
        auto* new_pairs = (decltype(_pairs))new_data;
        auto* new_state = (decltype(_states))(new_data + pairs_size);
#endif

        auto old_num_filled  = _num_filled;
        auto old_states      = _states;
        auto old_pairs       = _pairs;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _states      = new_state;
        _pairs       = new_pairs;

        //fill last packet zero
        memset((char*)(_pairs + num_buckets), 0, sizeof(_pairs[0]));

        //init empty tombstone
        std::fill_n(_states, num_buckets, State::EEMPTY);
        //set filled tombstone
        if (num_buckets >= simd_bytes)
            std::fill_n(_states + num_buckets, 1, State::SENTINEL);
        else
            std::fill_n(_states + num_buckets, simd_bytes - num_buckets + 1, State::SENTINEL);

        for (size_t src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
            if (old_states[src_bucket] % 2 == State::EFILLED) {
                auto& src_pair = old_pairs[src_bucket];
                const auto key_hash = H1(src_pair.first);
                const auto main_bucket = key_hash & _mask;
                const auto gbucket = main_bucket - main_bucket % simd_bytes;
                const auto bucket = find_empty_slot(gbucket, gbucket, 0);

                set_states(bucket, key_2hash(key_hash, src_pair.first));
                new(_pairs + bucket) PairT(std::move(src_pair));
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

    void prefetch_heap_block(char* ctrl) const
    {
        // Prefetch the heap-allocated memory region to resolve potential TLB
        // misses.  This is intended to overlap with execution of calculating the hash for a key.
#if EMH_PREFETCH
        __builtin_prefetch(static_cast<const void*>(ctrl), 0, 1);
#endif
    }

    // Find the bucket with this key, or return (size_t)-1
    template<typename K>
    size_t find_filled_bucket(const K& key) const noexcept
    {
        const auto key_hash = H1(key);
        auto next_bucket = (size_t)(key_hash & _mask);
        next_bucket -= next_bucket % simd_bytes;
        const auto gbucket = next_bucket;
        //prefetch_heap_block((char*)_states + next_bucket);

        const auto filled = SET1_EPI8(key_2hash(key_hash, key));
        size_t offset = 0;

        while (true) {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));

            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (EMH_LIKELY(_eq(_pairs[fbucket].first, key)))
                    return fbucket;
                maskf &= maskf - 1;
            }

            if (group_mask(next_bucket) == State::EEMPTY)
                break;

            const auto probe_bucket = next_bucket + simd_bytes - 1;
            if (EMH_UNLIKELY((_states[probe_bucket] % 2 == State::EFILLED)) && _eq(_pairs[probe_bucket].first, key))
                return probe_bucket;

            else if (group_probe(gbucket) < ++offset)
                break;
            next_bucket = (next_bucket + simd_bytes) & _mask;
        }

        return _num_buckets;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    template<typename K>
    size_t find_or_allocate(const K& key, bool& bnew) noexcept
    {
        check_expand_need();

        const auto key_hash = H1(key);
        const auto key_h2 = key_2hash(key_hash, key);
        auto bucket = (size_t)(key_hash & _mask);
        bucket -= bucket % simd_bytes;
        //prefetch_heap_block((char*)_states + bucket);

        const auto filled = SET1_EPI8(key_h2);
        auto next_bucket  = bucket, offset = 0u;
        size_t hole = (size_t)-1;

        while (true) {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));

            //1. find filled
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(_pairs[fbucket].first, key)) {
                    bnew = false;
                    return fbucket;
                }
                maskf &= maskf - 1;
            }

            //2. find empty
            const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (maske != 0) {
                const auto ebucket = hole == (size_t)-1 ? next_bucket + CTZ(maske) : hole;
                set_states(ebucket, key_h2);
                return ebucket;
            }

            //2. find probe_bucket
            const auto probe_bucket = next_bucket + simd_bytes - 1;
            if (EMH_UNLIKELY(_states[probe_bucket] % 2 == State::EFILLED) && _eq(_pairs[probe_bucket].first, key)) {
                bnew = false;
                return probe_bucket;
            }

            //3. find erased
            else if (hole == (size_t)-1) {
                const auto maskd = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_delete));
                if (maskd != 0)
                    hole = next_bucket + CTZ(maskd);
            }

            //4. next round
            offset += 1;
            next_bucket = (next_bucket + simd_bytes) & _mask;
            if (offset > group_probe(bucket)) {
                if (EMH_LIKELY(hole != (size_t)-1)) {
                    set_states(hole, key_h2);
                    return hole;
                }
                break;
            }
        }

        const auto ebucket = find_empty_slot(bucket, next_bucket, offset);
        set_states(ebucket, key_h2);
        return ebucket;
    }

    inline uint64_t empty_delete(size_t gbucket) const noexcept
    {
        const auto vec = LOAD_EMPTY2((decltype(&simd_empty))(&_states[gbucket]));
        return MOVEMASK_EPI8(vec);
    }

    inline uint64_t filled_mask(size_t gbucket) const noexcept
    {
        const auto vec = LOAD_EMPTY((decltype(&simd_empty))(&_states[gbucket]));
        return MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_filled));
    }

    size_t find_empty_slot(size_t main_bucket, size_t next_bucket, int offset) noexcept
    {
        while (true) {
            const auto maske = empty_delete(next_bucket);
            if (EMH_LIKELY(maske != 0)) {
                set_probe(main_bucket, offset);
                const auto probe = CTZ(maske);
                return next_bucket + probe;
            }
            offset      += 1;
            next_bucket = (next_bucket + simd_bytes) & _mask;
        }

        return 0;
    }

    size_t find_filled_slot(size_t next_bucket) const noexcept
    {
        next_bucket -= next_bucket % simd_bytes;
        while (true) {
            const auto maske = filled_mask(next_bucket);
            if (EMH_LIKELY(maske != 0))
                return next_bucket + CTZ(maske);
            next_bucket += simd_bytes;
        }
        return 0;
    }

    inline size_t H1(const KeyT& key) const noexcept
    {
#ifdef EMH_H3
        const auto key_hash = _hasher(key);
        return (key_hash >> 7) ^ (reinterpret_cast<uintptr_t>(_states) >> 12);
#else
        return (size_t)_hasher(key);
#endif
    }

private:

    HashT   _hasher;
    EqT     _eq;
    uint8_t*_states           = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _mask             = 0; // _num_buckets minus one
    size_t  _num_filled       = 0;
};

} // namespace emilib
