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
#if (__GNUC__ >= 4 || __clang__) && _MSC_VER == 0
#    define EMH_LIKELY(condition)   __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition)   condition
#    define EMH_UNLIKELY(condition) condition
#endif

namespace emilib2 {

    enum State : int8_t
    {
        EFILLED  = -126, EDELETE = -127, EEMPTY = -128,
        SENTINEL = EFILLED,
    };

    constexpr static uint8_t EMPTY_OFFSET = 0;

#if EMH_ITERATOR_BITS < 16
    #define EMH_ITERATOR_BITS 16
#endif
#define EMH_MIN_OFFSET_CHECK  1

#ifndef AVX2_EHASH
    const static auto simd_empty  = _mm_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm_set1_epi8(EDELETE);
    const static auto simd_filled = _mm_set1_epi8(EFILLED);
    constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8      _mm_set1_epi8
    #define LOAD_UEPI8     _mm_loadu_si128
    #define MOVEMASK_EPI8  _mm_movemask_epi8
    #define CMPEQ_EPI8     _mm_cmpeq_epi8
    #define CMPLT_EPI8     _mm_cmplt_epi8
#elif SSE2_EMHASH == 0
    const static auto simd_empty  = _mm256_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm256_set1_epi8(EDELETE);
    constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8      _mm256_set1_epi8
    #define LOAD_UEPI8     _mm256_loadu_si256
    #define MOVEMASK_EPI8  _mm256_movemask_epi8
    #define CMPEQ_EPI8     _mm256_cmpeq_epi8
#elif AVX512_EHASH
    const static auto simd_empty  = _mm512_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm512_set1_epi8(EDELETE);
    constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);

    #define SET1_EPI8      _mm512_set1_epi8
    #define LOAD_UEPI8     _mm512_loadu_si512
    #define MOVEMASK_EPI8  _mm512_movemask_epi8 //avx512 error
    #define CMPEQ_EPI8     _mm512_test_epi8_mask
#else
    //TODO arm neon
#endif

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
        {_BitScanForward(&index, (uint32_t)(n >> 32)); index += 32; }
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

    using mapped_type     = ValueT;
    using val_type        = ValueT;
    using key_type        = KeyT;
    using hasher          = HashT;
    using key_equal       = EqT;

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value, int8_t>::type = 0>
    inline int8_t hash_key2(size_t& main_bucket, const UType& key) const
    {
        const auto key_hash = _hasher(key);
        main_bucket = (size_t)key_hash & _mask;
        return (int8_t)(key_hash % 251 - 125);
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, int8_t>::type = 0>
    inline int8_t hash_key2(size_t& main_bucket, const UType& key) const
    {
        const auto key_hash = _hasher(key);
        main_bucket = (size_t)key_hash & _mask;
//        return (int8_t)((uint32_t)((uint64_t)key * 0x9FB21C651E98DF25ull >> 32) % 251) - 125;
        return (int8_t)(key_hash % 251 - 125);
    }

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
        iterator(const const_iterator& it)
            : _map(it._map), _bucket(it._bucket), _bmask(it._bmask), _from(it._from) {}
        iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
#if EMH_ITER_SAFE
        iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { init(); }
#else
        iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { _bmask = 0; _from = -1; }
#endif

        void init()
        {
            _from = (_bucket / EMH_ITERATOR_BITS) * EMH_ITERATOR_BITS;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= ~((1ull << (_bucket % EMH_ITERATOR_BITS)) - 1);
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
#ifndef EMH_ITER_SAFE
            if (_from == (size_t)-1) init();
#endif
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
#ifndef EMH_ITER_SAFE
            if (_from == (size_t)-1) init();
#endif
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
                _bmask = _map->filled_mask(_from += EMH_ITERATOR_BITS);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype*  _map;
        size_t        _bmask;
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
            : _map(it._map), _bucket(it._bucket), _bmask(it._bmask), _from(it._from) { init(); }
        const_iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
//        const_iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { init(); }

        void init()
        {
            _from = (_bucket / EMH_ITERATOR_BITS) * EMH_ITERATOR_BITS;
            const auto bucket_count = _map->bucket_count();
            if (_bucket < bucket_count) {
                _bmask = _map->filled_mask(_from);
                _bmask &= ~((1ull << (_bucket % EMH_ITERATOR_BITS)) - 1);
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
                _bmask = _map->filled_mask(_from += EMH_ITERATOR_BITS);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype*  _map;
        size_t        _bmask;
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

        _pairs[_num_buckets].~PairT();
        _num_filled = 0;
        free(_states);
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
            memcpy(_pairs, other._pairs, (_num_buckets + 1) * sizeof(_pairs[0]));
        } else {
            for (auto it = other.cbegin(); it.bucket() <= _num_buckets; ++it)
                new(_pairs + it.bucket()) PairT(*it);
        }

        //assert(_num_buckets == other._num_buckets);
        _num_filled = other._num_filled;
        //memcpy(_offset, other._offset, _num_buckets * sizeof(_offset[0]));
        memcpy(_states, other._states, (2 * _num_buckets + simd_bytes) * sizeof(_states[0]));
    }

    void swap(HashMap& other) noexcept
    {
        std::swap(_hasher,      other._hasher);
        std::swap(_eq,          other._eq);
        std::swap(_states,      other._states);
        std::swap(_pairs,       other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled,  other._num_filled);
        std::swap(_offset,      other._offset);
        std::swap(_mask,        other._mask);
    }

    // -------------------------------------------------------------

    iterator begin() noexcept
    {
        if (_num_filled == 0)
            return {this, _num_buckets, false};
        return {this, find_first_slot(0), false};
    }

    const_iterator cbegin() const noexcept
    {
        if (_num_filled == 0)
            return {this, _num_buckets};
        return {this, find_first_slot(0)};
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
        return {this, _num_buckets};
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

    float max_load_factor(float lf = 7.0f / 8)
    {
        return 7.0f / 8;
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
        return {this, find_filled_bucket(key)};
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

    template<typename Key = KeyT>
    ValueT& at(const KeyT& key)
    {
        const auto bucket = find_filled_bucket(key);
        return _pairs[bucket].second;
    }

    template<typename Key = KeyT>
    const ValueT& at(const KeyT& key) const
    {
        const auto bucket = find_filled_bucket(key);
        return _pairs[bucket].second;
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

    template<typename Con>
    bool operator == (const Con& rhs) const
    {
        if (size() != rhs.size())
            return false;

        for (auto it = begin(), last = end(); it != last; ++it) {
            auto oi = rhs.find(it->first);
            if (oi == rhs.end() || it->second != oi->second)
                return false;
        }
        return true;
    }

    template<typename Con>
    bool operator != (const Con& rhs) const { return !(*this == rhs); }

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
                rhs.erase(rit++);
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

#if 0
    iterator insert(iterator hint, const value_type& value) noexcept
    {
        (void)hint;
        return do_insert(value).first;
    }
#endif

    template <typename Iter>
    void insert(Iter beginc, Iter endc)
    {
        reserve(endc - beginc + _num_filled);
        for (; beginc != endc; ++beginc)
            do_insert(beginc->first, beginc->second);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args)
    {
        check_expand_need();
        return do_insert(key, std::forward<Args>(args)...);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args)
    {
        check_expand_need();
        return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

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

        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        const auto bucket = find_empty_slot(main_bucket, main_bucket, 0);

        set_states(bucket, key_h2);
        new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        return bucket;
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const KeyT& key, M&& val) { return do_assign(key, std::forward<M>(val)); }
    template <class M>
    std::pair<iterator, bool> insert_or_assign(KeyT&& key, M&& val) { return do_assign(std::move(key), std::forward<M>(val)); }

    template<typename K, typename V>
    std::pair<iterator, bool> do_assign(K&& key, V&& val)
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);

        // Check if inserting a new val rather than overwriting an old entry
        if (bempty) {
            new(_pairs + bucket) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        } else {
            _pairs[bucket].second = std::forward<V>(val);
        }

        return { {this, bucket, false}, bempty };
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

    void erase(const_iterator cit) noexcept
    {
        _erase(cit._bucket);
    }

    void erase(iterator it) noexcept
    {
        _erase(it._bucket);
    }


    void _erase(size_t bucket) noexcept
    {
        _num_filled -= 1;
        if (is_triviall_destructable())
            _pairs[bucket].~PairT();
#if EMH_PSL_LINEAR
        set_states(bucket, _states[bucket + 1] == State::EEMPTY ? State::EEMPTY : State::EDELETE);
#else
        set_states(bucket, State::EDELETE);
#endif

#if EMH_PSL_ERASE && EMH_PSL_LINEAR
        if (_states[bucket] == State::EEMPTY) {
            _offset[bucket] = EMPTY_OFFSET; bucket = (bucket - 1) & _mask;
            while (_states[bucket] == State::EDELETE) {
                set_states(bucket, State::EEMPTY); _offset[bucket] = EMPTY_OFFSET;
                bucket = (bucket - 1) & _mask;
            }
        }
#else
        if (EMH_UNLIKELY(_num_filled == 0)) {
            std::fill_n(_states, _num_buckets, State::EEMPTY);
            std::fill_n(_offset, _num_buckets, EMPTY_OFFSET);
        }
#endif
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        auto iend = cend();
        auto next = first;
        for (; next != last && next != iend; )
            erase(next++);

        return {this, next.bucket()};
    }

    template<typename Pred>
    size_t erase_if(Pred pred)
    {
        auto old_size = size();
        for (auto it = begin(), last = end(); it != last; ) {
            if (pred(*it))
                erase(it);
            ++it;
        }
        return old_size - size();
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
                _num_filled --;
            }
        } else if (_num_filled) {
            std::fill_n(_states, _num_buckets, State::EEMPTY);
            std::fill_n(_offset, _num_buckets, EMPTY_OFFSET);
            _num_filled = 0;
        }
    }

    void shrink_to_fit()
    {
        rehash(_num_filled + 1);
    }

    bool reserve(size_t num_elems) noexcept
    {
        size_t required_buckets = num_elems + num_elems / 5;
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    void dump_statics() const
    {
        int off[256] = {0};
        for (int i = 0; i < _num_buckets; i++)
            off[_offset[i]]++;

        size_t total = 0, sums = 0;
        for (int i = 0; i < 256; i++) {
            if (off[i] != EMPTY_OFFSET) {
                total += off[i];
                sums  += (size_t)off[i] * (i + 1);
                printf("\n%3d %8d %.5lf%% %3.lf%%", i, off[i], 1.0 * off[i] / _num_buckets, 10000.0 * total / _num_buckets);
            }
        }
        printf(", average probe sequence length PSL = %.4lf\n", 1.0 * sums / total);
    }

//#define EMH_STATIS 2000'000
    /// Make room for this many elements
    void rehash(size_t num_elems) noexcept
    {
        const size_t required_buckets = num_elems;
        if (EMH_UNLIKELY(required_buckets < _num_filled))
            return;

#if EMH_STATIS
        if (_num_filled > EMH_STATIS)
            dump_statics();
#endif

        auto num_buckets = _num_filled > (1u << 16) ? (1u << 16) : simd_bytes;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        const auto pairs_size = (num_buckets + 1) * sizeof(PairT);
        const auto state_size = (simd_bytes + num_buckets);
        //assert(state_size % 8 == 0);

        const auto* new_data = (char*)malloc(pairs_size + state_size * 2 * sizeof(_states[0]));

        auto* new_state = (decltype(_states))new_data;
        _offset         = (decltype(_offset))(new_state + state_size);
        auto* new_pairs = (decltype(_pairs))(_offset + state_size);

        auto old_num_filled  = _num_filled;
        auto old_states      = _states;
        auto old_pairs       = _pairs;
        auto old_buckets     = _num_buckets;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _states      = new_state;
        _pairs       = new_pairs;

        //init empty
        std::fill_n(_states, num_buckets, State::EEMPTY);
        //set sentinel tombstone
        std::fill_n(_states + num_buckets, simd_bytes, State::SENTINEL);

        {
            //set last packet tombstone. not equal key h2
            new(_pairs + num_buckets) PairT(KeyT(), ValueT());
            //size_t main_bucket;
            //_states[num_buckets] = hash_key2(main_bucket, _pairs[num_buckets].first) + 2; //iterator end tombstone:
            if (old_buckets)
                old_pairs[old_buckets].~PairT();
        }

        //fill offset to 0
        std::fill_n(_offset, num_buckets, EMPTY_OFFSET);

        for (size_t src_bucket = old_buckets - 1; _num_filled < old_num_filled; --src_bucket) {
            if (old_states[src_bucket] >= State::EFILLED) {
                auto& src_pair = old_pairs[src_bucket];

                size_t main_bucket;
                const auto key_h2 = hash_key2(main_bucket, src_pair.first);
                const auto bucket = find_empty_slot(main_bucket, main_bucket, 0);

                set_states(bucket, key_h2);
                new(_pairs + bucket) PairT(std::move(src_pair));
                _num_filled ++;
                src_pair.~PairT();
            }
        }
        free(old_states);
    }

private:
    // Can we fit another element?
    void check_expand_need()
    {
        reserve(_num_filled);
    }

    static void prefetch_heap_block(char* ctrl)
    {
        // Prefetch the heap-allocated memory region to resolve potential TLB
        // misses.  This is intended to overlap with execution of calculating the hash for a key.
#if __linux__
        __builtin_prefetch(static_cast<const void*>(ctrl));
#elif _WIN32
        _mm_prefetch((const char*)ctrl, _MM_HINT_T0);
#endif
    }

    inline uint32_t get_offset(size_t main_bucket) const
    {
#if EMH_SAFE_PSL
        if (EMH_UNLIKELY(_offset[main_bucket] > 128))
            return (_offset[main_bucket] - 127) * 128;
#endif
        return _offset[main_bucket];
    }

    inline void set_offset(size_t main_bucket, uint32_t off)
    {
//        if (off < _offset[main_bucket])
#if EMH_SAFE_PSL
        assert(off < 127 * 128);
        _offset[main_bucket] = off <= 128 ? off : 128 + off / 128;
#else
        assert(off < 256);
        _offset[main_bucket] = off;
#endif
    }

    inline void set_states(size_t ebucket, int8_t key_h2)
    {
        _states[ebucket] = key_h2;
    }

    inline size_t get_next_bucket(size_t next_bucket, size_t offset) const
    {
#if EMH_PSL_LINEAR == 0
        next_bucket += offset < 8 ? 7 + simd_bytes * offset / 2 : _mask / 32 + 2;
        next_bucket += next_bucket >= _num_buckets;
#elif EMH_PSL_LINEAR == 1
        if (offset < 8)
            next_bucket += simd_bytes * 2 + offset;
        else
            next_bucket += _num_buckets / 32 + 1;
#else
        next_bucket += simd_bytes;
        if (next_bucket >= _num_buckets)
            next_bucket = offset;
#endif
        return next_bucket & _mask;
    }

    bool is_empty(size_t bucket) const
    {
        return _states[bucket] == State::EEMPTY;
    }

    // Find the main_bucket with this key, or return (size_t)-1
    template<typename K>
    size_t find_filled_bucket(const K& key) const noexcept
    {
        size_t main_bucket;
        const auto filled = SET1_EPI8(hash_key2(main_bucket, key));
        auto next_bucket = main_bucket;
        size_t offset = 0;

        if (0)
        {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));
            if (maskf)
                prefetch_heap_block((char*)&_pairs[next_bucket]);
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(_pairs[fbucket].first, key))
                    return fbucket;
                maskf &= maskf - 1;
            }
            const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (maske != 0)
                return _num_buckets;
            else if (0 == get_offset(main_bucket))
                return _num_buckets;

            next_bucket = get_next_bucket(next_bucket, ++offset);
        }

        while (true) {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));
            if (maskf)
                prefetch_heap_block((char*)&_pairs[next_bucket]);
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (EMH_LIKELY(_eq(_pairs[fbucket].first, key)))
                    return fbucket;
                maskf &= maskf - 1;
            }

            const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (maske != 0)
                break;

            offset += 1;
            if (offset > EMH_MIN_OFFSET_CHECK && offset > get_offset(main_bucket))
                break;
            next_bucket = get_next_bucket(next_bucket, offset);
        }

        return _num_buckets;
    }

    // Find the main_bucket with this key, or return a good empty main_bucket to place the key in.
    // In the latter case, the main_bucket is expected to be filled.
    template<typename K>
    size_t find_or_allocate(const K& key, bool& bnew) noexcept
    {
        check_expand_need();

        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        const auto filled = SET1_EPI8(key_h2);
        auto next_bucket = main_bucket, offset = 0u;
        constexpr size_t chole = (size_t)-1;
        size_t hole = chole;

        while (true) {
            const auto vec = LOAD_UEPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf  = MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled));
            prefetch_heap_block((char*)&_pairs[next_bucket]);

            //1. find filled
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (EMH_LIKELY(_eq(_pairs[fbucket].first, key))) {
                    bnew = false;
                    return fbucket;
                }
                maskf &= maskf - 1;
            }

            //2. find empty
            const auto maske = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty));
            if (maske) {
                auto ebucket = next_bucket + CTZ(maske);
                if (EMH_UNLIKELY(hole != chole))
                    ebucket = hole;
                set_states(ebucket, key_h2);
                return ebucket;
            }

            //3. find erased
            else if (hole == chole) {
                const auto maskd = MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_delete));
                if (maskd != 0)
                    hole = next_bucket + CTZ(maskd);
            }

            //4. next round
            next_bucket = get_next_bucket(next_bucket, ++offset);
            if (offset > EMH_MIN_OFFSET_CHECK && offset > get_offset(main_bucket))
                break;
        }

        if (hole != chole) {
            //set_offset(main_bucket, offset - 1);
            set_states(hole, key_h2);
            return hole;
        }

        const auto ebucket = find_empty_slot(main_bucket, next_bucket, offset);
        //prefetch_heap_block((char*)&_pairs[ebucket]);
        set_states(ebucket, key_h2);
        return ebucket;
    }

    inline uint64_t empty_delete(size_t gbucket) const noexcept
    {
        const auto vec = LOAD_UEPI8((decltype(&simd_empty))(&_states[gbucket]));
        return MOVEMASK_EPI8(CMPLT_EPI8(vec, simd_filled));
    }

    inline uint64_t filled_mask(size_t gbucket) const noexcept
    {
#if EMH_ITERATOR_BITS == 160
        const auto vec = _mm_slli_epi16(_mm_loadu_si128((__m128i const*) & _states[gbucket]), 7);
        return (uint32_t)~_mm_movemask_epi8(vec) & 0xFFFF;
#elif EMH_ITERATOR_BITS == 320
        const auto vec = _mm256_slli_epi32(_mm256_loadu_si256((__m256i const*) & _states[gbucket]), 7);
        return (uint32_t)~_mm256_movemask_epi8(vec);
#else
        const auto vec = LOAD_UEPI8((decltype(&simd_empty))(&_states[gbucket]));
        return MOVEMASK_EPI8(CMPLT_EPI8(simd_delete, vec));
#endif
    }

#if EMH_PSL
    //unlike robin hood, only move large offset once
    size_t update_offset(size_t gbucket, size_t new_bucket, size_t offset) noexcept
    {
        const auto kdiff  = offset / 2;
        const auto kprobe = offset - kdiff;
        const auto kgroup = (new_bucket - kdiff * simd_bytes) & _mask;
        for (int i = 0; i < 8 * simd_bytes; i++) {
            const auto kbucket = (kgroup + i) & _mask;
            if (_offset[kbucket] == 0)
                continue;

            size_t kmain_bucket;
            hash_key2(kmain_bucket, _pairs[kbucket].first);
            if (kmain_bucket != kbucket)
                continue;

            //move kbucket to new_bucket, update offset
            set_states(new_bucket, _states[kbucket]);
            new(_pairs + new_bucket) PairT(std::move(_pairs[kbucket]));
            _pairs[kbucket].~PairT();

            if (kdiff + 1 - i / simd_bytes > get_offset(kmain_bucket))
                set_offset(kmain_bucket, kdiff + 1 - i / simd_bytes);
            if (kprobe + i / simd_bytes >= get_offset(gbucket))
                set_offset(gbucket, kprobe + 1 + i / simd_bytes);
            return kbucket;
        }

        return -1;
    }
#endif

    size_t find_empty_slot(size_t main_bucket, size_t next_bucket, size_t offset) noexcept
    {
        while (true) {
            size_t ebucket;
            const auto maske = empty_delete(next_bucket);
            if (maske == 0)
                goto JNEXT_BLOCK;

            ebucket = next_bucket + CTZ(maske);
            if (EMH_UNLIKELY(ebucket >= _num_buckets))
                goto JNEXT_BLOCK;

            else if (offset <= get_offset(main_bucket))
                return ebucket;
#if EMH_PSL > 8 && EMH_PSL_LINEAR
            else if (EMH_UNLIKELY(offset >= EMH_PSL)) {
                const auto kbucket = update_offset(main_bucket, ebucket, offset);
                if (kbucket != (size_t)-1)
                    return kbucket;
            }
#endif
            set_offset(main_bucket, offset);
            return ebucket;

JNEXT_BLOCK:
            next_bucket = get_next_bucket(next_bucket, ++offset);
        }

        return 0;
    }

    size_t find_first_slot(size_t next_bucket) const noexcept
    {
        while (true) {
            const auto maske = filled_mask(next_bucket);
            if (EMH_LIKELY(maske != 0))
                return next_bucket + CTZ(maske);
            next_bucket += EMH_ITERATOR_BITS;
        }
        return 0;
    }

private:

    HashT   _hasher;
    EqT     _eq;
    int8_t* _states           = nullptr;
    uint8_t*_offset           = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _mask             = 0; // _num_buckets minus one
    size_t  _num_filled       = 0;
};

} // namespace emilib
