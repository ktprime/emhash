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

#ifdef _WIN32
#  include <intrin.h>
#ifndef __clang__
//#  include <zmmintrin.h>
#endif
#elif __x86_64__
#  include <x86intrin.h>
#else
# include "sse2neon.h"
#endif

#undef EMH_LIKELY
#undef EMH_UNLIKELY
#undef bucket_to_slot

// likely/unlikely
#if defined(__GNUC__) && (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1) || defined(__clang__)
  #define EMH_LIKELY(condition)   __builtin_expect(!!(condition), 1)
  #define EMH_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1920)
  #define EMH_LIKELY(condition)   ((condition) ? ((void)__assume(condition), 1) : 0)
  #define EMH_UNLIKELY(condition) ((condition) ? 1 : ((void)__assume(!condition), 0))
#else
  #define EMH_LIKELY(condition)   (condition)
  #define EMH_UNLIKELY(condition) (condition)
#endif

namespace emilib {

enum State : int8_t
{
    EFILLED  = -126,
    EDELETE = -127,
    EEMPTY = -128,
    SENTINEL = 127,
    STATE_BITS  = 1,
};

#ifndef AVX2_EHASH
    const static auto simd_empty  = _mm_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm_set1_epi8(EDELETE);
    const static auto simd_filled = _mm_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm_set1_epi8
    #define LOAD_EPI8      _mm_load_si128
    #define MOVEMASK_EPI8  _mm_movemask_epi8
    #define CMPEQ_EPI8     _mm_cmpeq_epi8
    #define CMPGT_EPI8     _mm_cmpgt_epi8
#else
    const static auto simd_empty  = _mm256_set1_epi8(EEMPTY);
    const static auto simd_delete = _mm256_set1_epi8(EDELETE);
    const static auto simd_filled = _mm256_set1_epi8(EFILLED);

    #define SET1_EPI8      _mm256_set1_epi8
    #define LOAD_EPI8      _mm256_loadu_si256
    #define MOVEMASK_EPI8  _mm256_movemask_epi8
    #define CMPEQ_EPI8     _mm256_cmpeq_epi8
    #define CMPGT_EPI8     _mm256_cmpgt_epi8
#endif

//find filled or empty
constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);
constexpr static uint8_t slot_size  = simd_bytes - STATE_BITS;
constexpr static uint8_t group_index = simd_bytes - 1;//> 0
constexpr static uint32_t group_bmask = (1u << slot_size) - 1;

inline static uint32_t CTZ(uint32_t n)
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
    _BitScanForward(&index, n);
#elif 1
    auto index = __builtin_ctzl((unsigned long)n);
#endif

    return (uint32_t)index;
}

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
private:
    using htype = HashMap<KeyT, ValueT, HashT, EqT>;

    using PairT = std::pair<const KeyT, ValueT>;
    constexpr static uint8_t MXLOAD_FACTOR = 5; // max_load = LOAD_FACTOR / (LOAD_FACTOR + 1)

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
        main_bucket = size_t(key_hash & _mask);
        main_bucket -= main_bucket % simd_bytes;
        return (int8_t)((size_t)(key_hash % 253) + (size_t)EFILLED);
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, int8_t>::type = 0>
    inline int8_t hash_key2(size_t& main_bucket, const UType& key) const
    {
        const auto key_hash = _hasher(key);
        main_bucket = size_t(key_hash & _mask);
        main_bucket -= main_bucket % simd_bytes;
        return (int8_t)((size_t)(key_hash % 253) + (size_t)EFILLED);
    }

#if 1
    #define bucket_to_slot(bucket) bucket
#else
    static constexpr size_t bucket_to_slot(size_t bucket)
    {
        return bucket / simd_bytes * slot_size + bucket % simd_bytes;
    }
#endif

    class const_iterator;
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = PairT;
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
                _bmask &= (size_t) ~((1ull << (_bucket % simd_bytes)) - 1);
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

        reference operator*() const { return _map->_pairs[bucket_to_slot(_bucket)]; }
        pointer operator->() const { return _map->_pairs + bucket_to_slot(_bucket); }

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
        size_t        _bmask;
        size_t        _bucket;
        size_t        _from;
    };

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = const PairT;
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
                _bmask &= (size_t) ~((1ull << (_bucket % simd_bytes)) - 1);
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

        reference operator*() const { return _map->_pairs[bucket_to_slot(_bucket)]; }
        pointer operator->() const { return _map->_pairs + bucket_to_slot(_bucket); }

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
        rehash((size_t)il.size());
        for (auto it = il.begin(); it != il.end(); ++it)
            insert(*it);
    }

    template<class InputIt>
    HashMap(InputIt first, InputIt last, size_t bucket_count = 4) noexcept
    {
        rehash((size_t)std::distance(first, last) + bucket_count);
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
        clear_data();
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

        clear_data();

        if (other._num_buckets != _num_buckets) {
            _num_filled = _num_buckets = 0;
            rehash(other._num_buckets);
        }

        if (is_trivially_copyable()) {
            const auto pairs_size = (1 + bucket_to_slot(_num_buckets)) * sizeof(PairT);
            memcpy((char*)_pairs, other._pairs, pairs_size);
        } else {
            for (auto it = other.cbegin(); it.bucket() != _num_buckets; ++it)
                new(_pairs + bucket_to_slot(it.bucket())) PairT(*it);
        }

        const auto state_size = (simd_bytes + _num_buckets) * sizeof(State);
        //assert(_num_buckets == other._num_buckets);
        _num_filled = other._num_filled;
        memcpy(_states, other._states, state_size);
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
        return {this, find_filled_slot(0)};
    }

    const_iterator cbegin() const noexcept
    {
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
        return static_cast<float>(_num_filled) / (float)bucket_to_slot(_num_buckets);
    }

    float max_load_factor(float lf = 7.0f / 8)
    {
        (void)lf;
        return 7.0f / 8;
    }

    constexpr uint64_t max_size() const { return 1ull << (sizeof(_num_buckets) * 8 - 1); }
    constexpr uint64_t max_bucket_count() const { return max_size(); }

    // ------------------------------------------------------------

    template<typename K=KeyT>
    iterator find(const K& key) noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    template<typename K=KeyT>
    const_iterator find(const K& key) const noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    template<typename K=KeyT>
    bool contains(const K& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    template<typename K=KeyT>
    size_t count(const K& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    template<typename K=KeyT>
    ValueT& at(const K& key)
    {
        const auto bucket = find_filled_bucket(key);
        return _pairs[bucket].second;
    }

    template<typename K=KeyT>
    const ValueT& at(const K& key) const
    {
        const auto bucket = find_filled_bucket(key);
        return _pairs[bucket].second;
    }

#if 0
    /// Returns the matching ValueT or nullptr if k isn't found.
    template<typename K>
    ValueT* try_get(const K& key)
    {
        auto bucket = find_filled_bucket(key);
        return &_pairs[bucket].second;
    }

    /// Const version of the above
    template<typename K>
    ValueT* try_get(const K& key) const
    {
        auto bucket = find_filled_bucket(key);
        return &_pairs[bucket].second;
    }
#endif

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
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new(_pairs + slot) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        }
        return { {this, bucket, false}, bempty };
    }

    std::pair<iterator, bool> do_insert(const value_type& value) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new(_pairs + slot) PairT(value); _num_filled++;
        }
        return { {this, bucket, false}, bempty };
    }

    std::pair<iterator, bool> do_insert(value_type&& value) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(value.first, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new(_pairs + slot) PairT(std::move(value)); _num_filled++;
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
        rehash(size_t(endc - beginc) + _num_filled);
        for (; beginc != endc; ++beginc)
            do_insert(beginc->first, beginc->second);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args)
    {
        //check_expand_need();
        return do_insert(key, std::forward<Args>(args)...);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args)
    {
        //check_expand_need();
        return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    void insert(std::initializer_list<value_type> ilist) noexcept
    {
        rehash(size_t(ilist.size()) + _num_filled);
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
        new(_pairs + bucket_to_slot(bucket)) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
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
        const auto slot = bucket_to_slot(bucket);
        // Check if inserting a new val rather than overwriting an old entry
        if (bempty) {
            new(_pairs + slot) PairT(std::forward<K>(key), std::forward<V>(val)); _num_filled++;
        } else {
            _pairs[slot].second = std::forward<V>(val);
        }

        return { {this, bucket, false}, bempty };
    }

    bool set_get(const KeyT& key, const ValueT& val, ValueT& oldv)
    {
        //check_expand_need();

        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bempty) {
            new(_pairs + slot) PairT(key,val); _num_filled++;
        } else
            oldv = _pairs[slot].second;
        return bempty;
    }

    ValueT& operator[](const KeyT& key) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        /* Check if inserting a new value rather than overwriting an old entry */
        if (bempty) {
            new(_pairs + slot) PairT(key, std::move(ValueT())); _num_filled++;
        }
        return _pairs[slot].second;
    }

    ValueT& operator[](KeyT&& key) noexcept
    {
        bool bempty = true;
        const auto bucket = find_or_allocate(key, bempty);
        const auto slot = bucket_to_slot(bucket);
        if (bempty) {
            new(_pairs + slot) PairT(std::move(key), std::move(ValueT())); _num_filled++;
        }
        return _pairs[slot].second;
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

    void erase(const const_iterator& cit) noexcept
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
        if (!is_trivially_destructible()) {
            const auto slot = bucket_to_slot(bucket);
            _pairs[slot].~PairT();
        }
#if 1
        _states[bucket] = group_has_empty(bucket) ? State::EEMPTY : State::EDELETE;
#else
        _states[bucket] = State::EDELETE;
        if (EMH_UNLIKELY(_num_filled == 0))
            clear_meta();
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

    static constexpr bool is_trivially_destructible()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return (std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    static constexpr bool is_trivially_copyable()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clear_meta()
    {
        //init empty tombstone
        std::fill_n(_states, _num_buckets, State::EEMPTY);
        //set filled tombstone
        std::fill_n(_states + _num_buckets, simd_bytes, State::SENTINEL);
        assert(_num_buckets >= simd_bytes);
        for (size_t src_bucket = group_index; src_bucket < _num_buckets; src_bucket += simd_bytes)
            _states[src_bucket] = _states[src_bucket - STATE_BITS + 1] = 0;
        _num_filled = 0;
    }

    void clear_data()
    {
        if (!is_trivially_destructible()) {
            for (auto it = begin(); _num_filled; ++it) {
                const auto bucket = it.bucket();
                _pairs[bucket_to_slot(bucket)].~PairT();
                _num_filled -= 1;
            }
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear() noexcept
    {
        if (_num_filled) {
            clear_data();
            clear_meta();
        }
        _num_filled = 0;
    }

    void shrink_to_fit()
    {
        rehash(_num_filled + 1);
    }

    bool reserve(size_t num_elems) noexcept
    {
        size_t required_buckets = num_elems + num_elems / MXLOAD_FACTOR;
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

        rehash(required_buckets + 2);
        return true;
    }

    void dump_statics() const
    {
        size_t off[256] = {0};
        for (size_t i = 0; i < _num_buckets; i += simd_bytes)
            off[group_probe(i)]++;

        size_t total = 0, sums = 0;
        for (size_t i = 0; i < 256; i++) {
            if (off[i] != 0) {
                total += off[i];
                sums  += (size_t)off[i] * (i + 1);
                printf("\n%3d %8d %.5lf %3.3lf%%",
                    i, off[i], 1.0 * off[i] / (_num_buckets / simd_bytes), 100.0 * total / (_num_buckets / simd_bytes));
            }
        }
        printf(", 2ss load_factor = %.3f average probe group length PGL = %.4lf\n", load_factor(), 1.0 * sums / total);
    }

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

        const auto pairs_size = (1 + bucket_to_slot(num_buckets)) * sizeof(PairT);
        const auto state_size = (simd_bytes + num_buckets) * sizeof(State);

        auto* new_pairs = (decltype(_pairs)) malloc(pairs_size);
        auto* new_state = (decltype(_states))malloc(state_size);

        auto old_num_filled  = _num_filled;
        auto old_states      = _states;
        auto old_pairs       = _pairs;
        auto old_buckets     = _num_buckets;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;
        _states      = new_state;
        _pairs       = new_pairs;

        //fill last packet zero
        memset((char*)(_pairs + pairs_size / sizeof(PairT) - 1), 0, sizeof(_pairs[0]));

        clear_meta();

        //for (size_t src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
        for (size_t src_bucket = old_buckets - 1; _num_filled < old_num_filled; --src_bucket) {
            if (old_states[src_bucket] >= State::EFILLED && src_bucket % simd_bytes < slot_size) {
                auto& src_pair = old_pairs[bucket_to_slot(src_bucket)];

                size_t main_bucket;
                const auto key_h2 = hash_key2(main_bucket, src_pair.first);
                const auto bucket = find_empty_slot(main_bucket, main_bucket, 0);
                set_states(bucket, key_h2);
                const auto slot = bucket_to_slot(bucket);
                new(_pairs + slot) PairT(std::move(src_pair));
                _num_filled ++;
                if (!is_trivially_destructible())
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

    static void prefetch_heap_block(char* ctrl)
    {
        // Prefetch the heap-allocated memory region to resolve potential TLB
        // misses.  This is intended to overlap with execution of calculating the hash for a key.
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        _mm_prefetch((const char*)ctrl, _MM_HINT_T0);
#elif defined(__GNUC__)
        __builtin_prefetch(static_cast<const void*>(ctrl));
#endif
    }

    inline bool group_has_empty(size_t bucket) const noexcept
    {
        const auto gbucket = bucket / simd_bytes * simd_bytes;
        return _states[gbucket + slot_size - 1] == State::EEMPTY;
    }

    inline int group_probe(size_t gbucket) const noexcept
    {
        const auto offset = (uint8_t)_states[gbucket + group_index];
#if EMH_SAFE_PSL
        if (EMH_UNLIKELY(offset > 128))
            return (offset - 127) * 128;
#endif
        return offset;
    }

    inline void set_group_probe(size_t gbucket, int group_offset) noexcept
    {
#if EMH_SAFE_PSL
        _states[gbucket + group_index] = group_offset <= 128 ? group_offset : 128 + group_offset / 128;
#else
        _states[gbucket + group_index] = (int8_t)group_offset;
#endif
    }

    inline void set_states(size_t ebucket, int8_t key_h2) noexcept
    {
        //assert(_states[ebucket] < EFILLED && key_h2 <= EFILLED);
        _states[ebucket] = key_h2;
    }

    inline size_t get_next_bucket(size_t next_bucket, size_t offset) const noexcept
    {
#if EMH_PSL_LINEAR == 0
        if (offset < 8)// || _num_buckets < 32 * simd_bytes)
            next_bucket += simd_bytes * offset;
        else
            next_bucket += _num_buckets / 8 + simd_bytes;
#else
        next_bucket += 3 * simd_bytes;
        if (next_bucket >= _num_buckets)
            next_bucket += simd_bytes;
#endif
        return next_bucket & _mask;
    }

    // Find the bucket with this key, or return (size_t)-1
    template<typename K>
    size_t find_filled_bucket(const K& key) const noexcept
    {
        size_t main_bucket, offset = 0;
        const auto filled = SET1_EPI8(hash_key2(main_bucket, key));
        auto next_bucket = main_bucket;

        while (true) {
            const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf = (uint32_t)MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled)) & group_bmask;
            if (maskf) {
                prefetch_heap_block((char*)&_pairs[bucket_to_slot(next_bucket)]);
                do {
                    const auto fbucket = next_bucket + CTZ(maskf);
                    const auto slot = bucket_to_slot(fbucket);
                    if (EMH_LIKELY(_eq(_pairs[slot].first, key)))
                        return fbucket;
                    maskf &= maskf - 1;
                } while (maskf != 0);
            }

            if ((int)++offset > group_probe(main_bucket))
                return _num_buckets;
//            if (group_has_empty(next_bucket))
//                break;
            next_bucket = get_next_bucket(next_bucket, offset);
        }

        return 0;
    }

    // Find the bucket with this key, or return a good empty bucket to place the key in.
    // In the latter case, the bucket is expected to be filled.
    template<typename K>
    size_t find_or_allocate(const K& key, bool& bnew) noexcept
    {
        size_t required_buckets = _num_filled + _num_filled / MXLOAD_FACTOR;
        if (EMH_LIKELY(required_buckets >= _num_buckets))
            rehash(required_buckets + 2);

        size_t main_bucket;
        const auto key_h2 = hash_key2(main_bucket, key);
        prefetch_heap_block((char*)&_pairs[bucket_to_slot(main_bucket)]);
        const auto filled = SET1_EPI8(key_h2);
        auto next_bucket = main_bucket; int offset = 0u;
        constexpr size_t chole = (size_t)-1;
        size_t hole = chole;

        do {
            const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[next_bucket]));
            auto maskf = (uint32_t)MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled)) & group_bmask;

            //1. find filled
            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                const auto slot = bucket_to_slot(fbucket);
                if (_eq(_pairs[slot].first, key)) {
                    bnew = false;
                    return fbucket;
                }
                maskf &= maskf - 1;
            }

            if (hole == chole) {
                //2. find empty/deleted
                const auto maskd = (size_t)MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec)) & group_bmask;
                if (group_has_empty(next_bucket)) {
                    hole = next_bucket + CTZ(maskd);
                    set_states(hole, key_h2);
                    return hole;
                }
                else if (maskd != 0) {
                    hole = next_bucket + CTZ(maskd);
                }
            }
            //3. next round
            next_bucket = get_next_bucket(next_bucket, (size_t)++offset);
        }  while (offset <= group_probe(main_bucket));

        if (hole != chole) {
            set_states(hole, key_h2);
            return hole;
        }

        const auto ebucket = find_empty_slot(main_bucket, next_bucket, offset);
        set_states(ebucket, key_h2);
        //if (EMH_UNLIKELY(reserve(_num_filled + 1)))
        //    return find_filled_bucket(key);
        return ebucket;
    }

    inline size_t empty_delete(size_t gbucket) const noexcept
    {
        const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[gbucket]));
        return (size_t)MOVEMASK_EPI8(CMPGT_EPI8(simd_filled, vec));
    }

    inline size_t filled_mask(size_t gbucket) const noexcept
    {
        const auto vec = LOAD_EPI8((decltype(&simd_empty))(&_states[gbucket]));
        return (size_t)MOVEMASK_EPI8(CMPGT_EPI8(vec, simd_delete)) & group_bmask;
    }

    //gbucket--->kbucket--->next_bucket|  kick_bucket--->next_bucket--->gbucket
    size_t find_empty_slot(size_t gbucket, size_t next_bucket, int offset) noexcept
    {
        while (true) {
            const auto maske = empty_delete(next_bucket) & group_bmask;
            if (maske != 0) {
                const auto probe = CTZ(maske) + next_bucket;
                prefetch_heap_block((char*)&_pairs[probe]);
                set_group_probe(gbucket, offset);//bugs for unique
                return probe;
            }
            next_bucket = get_next_bucket(next_bucket, (size_t)++offset);
        }

        return 0;
    }

    size_t find_filled_slot(size_t next_bucket) const noexcept
    {
        if (EMH_UNLIKELY(_num_filled) == 0)
            return _num_buckets;
        //next_bucket -= next_bucket % simd_bytes;
        while (true) {
            const auto maske = filled_mask(next_bucket);
            if (maske != 0)
                return next_bucket + CTZ(maske);
            next_bucket += simd_bytes;
        }
        return 0;
    }

private:

    HashT   _hasher;
    EqT     _eq;
    int8_t* _states           = nullptr;
    PairT*  _pairs            = nullptr;
    size_t  _num_buckets      = 0;
    size_t  _mask             = 0; // _num_buckets minus one
    size_t  _num_filled       = 0;
};

} // namespace emilib
