// Append-only variant of emihmap1.
//
// Storage layout differs from emihmap1.hpp: keys/values live in an append-only
// pair array, while the bucket array only stores 1-byte SIMD states plus a
// 32-bit slot index into that pair array. Consequences:
//   - rehash rebuilds only states/slots; pairs never move
//   - resize (value array growth) is the only operation that moves pairs
//   - find adds one indirection (slot -> pair) but keeps the SIMD group scan
//   - iteration walks buckets in probe order like the original
//
// The pair array never shrinks on erase; dead value slots are reclaimed by
// compaction when they outnumber live values.

#pragma once

#include "config.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef _WIN32
#include <intrin.h>
#elif defined(__x86_64__) || defined(__amd64__) || defined(__i386__) || defined(__i686__) || defined(_M_IX86) ||       \
    defined(_M_X64)
#include <x86intrin.h>
#elif defined(__ARM_ARCH__) || defined(__aarch64__) || defined(__arm__)
#include "sse2neon.h"
#endif

// emihset3.hpp keys its SIMD block off EMILIB3_SIMD_DEFINED (defined by
// emihmap3.hpp) but still uses the plain LOAD_EPI8 macro that the regular
// emihmap1.hpp happens to define. Keep that macro contract when this header
// replaces emihmap1.hpp, otherwise emihset3.hpp fails to compile.
#ifndef LOAD_EPI8
#ifndef AVX2_EHASH
#define SET1_EPI8 _mm_set1_epi8
#define SET1_EPI32 _mm_set1_epi32
#define LOAD_EPI8 _mm_loadu_si128
#define MOVEMASK_EPI8 _mm_movemask_epi8
#define CMPEQ_EPI8 _mm_cmpeq_epi8
#define CMPGT_EPI8 _mm_cmpgt_epi8
#else
#define SET1_EPI8 _mm256_set1_epi8
#define SET1_EPI32 _mm256_set1_epi32
#define LOAD_EPI8 _mm256_loadu_si256
#define MOVEMASK_EPI8 _mm256_movemask_epi8
#define CMPEQ_EPI8 _mm256_cmpeq_epi8
#define CMPGT_EPI8 _mm256_cmpgt_epi8
#endif
#endif

#ifndef EMH_LIKELY
#if defined(__GNUC__) || defined(__clang__)
#define EMH_LIKELY(x) __builtin_expect(!!(x), 1)
#define EMH_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define EMH_LIKELY(x) (x)
#define EMH_UNLIKELY(x) (x)
#endif
#endif

namespace emilib {

enum State : int8_t {
    EEMPTY = -128,
    EDELETE = -127,
    EFILLED = -126,
};

#ifndef AVX2_EHASH
const static auto simd_empty = _mm_set1_epi8(EEMPTY);
const static auto simd_delete = _mm_set1_epi8(EDELETE);
constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);
#else
const static auto simd_empty = _mm256_set1_epi8(EEMPTY);
const static auto simd_delete = _mm256_set1_epi8(EDELETE);
constexpr static uint8_t simd_bytes = sizeof(simd_empty) / sizeof(uint8_t);
#endif

inline static uint32_t CTZ(uint32_t n) {
#ifdef _WIN32
    unsigned long index;
    _BitScanForward(&index, n);
    return static_cast<uint32_t>(index);
#else
    return static_cast<uint32_t>(__builtin_ctz(n));
#endif
}

inline static void prefetch_write(const void* p) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(p, 1);
#elif defined(_MSC_VER)
    _mm_prefetch(static_cast<const char*>(p), _MM_HINT_T0);
#else
    (void)p;
#endif
}

/// A cache-friendly hash table with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>,
          typename AllocT = std::allocator<std::pair<KeyT, ValueT>>>
class HashMap {
private:
    using htype = HashMap<KeyT, ValueT, HashT, EqT, AllocT>;

    using PairT = std::pair<const KeyT, ValueT>;
    using PairAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<PairT>;
    using PairAllocTraits = std::allocator_traits<PairAlloc>;
    using StateAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<int8_t>;
    using StateAllocTraits = std::allocator_traits<StateAlloc>;
    using SlotAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<uint32_t>;
    using SlotAllocTraits = std::allocator_traits<SlotAlloc>;

    static constexpr uint32_t MXLOAD_FACTOR = 5; // max_load = 5 / 6

public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
    using size_t = uint32_t;
#pragma GCC diagnostic pop

    using value_type = PairT;
    using reference = PairT&;
    using const_reference = const PairT&;

    using mapped_type = ValueT;
    using val_type = ValueT;
    using key_type = KeyT;
    using hasher = HashT;
    using key_equal = EqT;
    using allocator_type = AllocT;

    static constexpr uint32_t NIL = 0xFFFFFFFFu;

    template <typename UType, typename std::enable_if<!std::is_integral<UType>::value, int8_t>::type = 0>
    inline int8_t hash_key2(size_t& main_bucket, const UType& key) const {
        const auto key_hash = static_cast<uint64_t>(_hasher(key));
        main_bucket = static_cast<size_t>(key_hash) & _mask;
        main_bucket -= main_bucket % simd_bytes;
        return static_cast<int8_t>(static_cast<size_t>(key_hash % 253) + static_cast<size_t>(EFILLED));
    }

    template <typename UType, typename std::enable_if<std::is_integral<UType>::value, int8_t>::type = 0>
    inline int8_t hash_key2(size_t& main_bucket, const UType& key) const {
        const auto key_hash = static_cast<uint64_t>(_hasher(key));
        main_bucket = static_cast<size_t>(key_hash) & _mask;
        main_bucket -= main_bucket % simd_bytes;
        return static_cast<int8_t>(static_cast<size_t>(key_hash % 253) + static_cast<size_t>(EFILLED));
    }

    class const_iterator;
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = PairT;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() {}
        iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) { _bmask = _from = 0; }

        void init() {
            _from = (_bucket / simd_bytes) * simd_bytes;
            if (_bucket < _map->bucket_count()) {
                _bmask = _map->filled_mask(_from);
                _bmask &= static_cast<size_t>(~((1ull << (_bucket % simd_bytes)) - 1));
            } else {
                _bmask = 0;
            }
        }

        size_t operator-(const iterator& r) const { return _bucket - r._bucket; }
        size_t bucket() const { return _bucket; }

        iterator& operator++() {
            goto_next_element();
            return *this;
        }

        iterator operator++(int) {
            iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->_values[_map->_slots[_bucket]]; }
        pointer operator->() const { return _map->_values + _map->_slots[_bucket]; }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const;
        bool operator!=(const const_iterator& rhs) const;

    private:
        void goto_next_element() {
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(static_cast<uint32_t>(_bmask));
                return;
            }

            const auto num_buckets = _map->bucket_count();
            do {
                _from += simd_bytes;
            } while (_from < num_buckets && (_bmask = _map->filled_mask(_from)) == 0);

            _bucket = (_bmask == 0) ? num_buckets : _from + CTZ(static_cast<uint32_t>(_bmask));
        }

    public:
        const htype* _map;
        size_t _bmask = 0;
        size_t _bucket;
        size_t _from;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const PairT;
        using pointer = value_type*;
        using reference = value_type&;

        const_iterator(const iterator& it) : _map(it._map), _bmask(it._bmask), _bucket(it._bucket), _from(it._from) {}
        const_iterator(const htype* hash_map, size_t bucket) : _map(hash_map), _bucket(bucket) { init(); }
        const_iterator(const htype* hash_map, size_t bucket, bool) : _map(hash_map), _bucket(bucket) {
            _bmask = _from = 0;
        }

        void init() {
            _from = (_bucket / simd_bytes) * simd_bytes;
            if (_bucket < _map->bucket_count()) {
                _bmask = _map->filled_mask(_from);
                _bmask &= static_cast<size_t>(~((1ull << (_bucket % simd_bytes)) - 1));
            } else {
                _bmask = 0;
            }
        }

        size_t bucket() const { return _bucket; }
        size_t operator-(const const_iterator& r) const { return _bucket - r._bucket; }

        const_iterator& operator++() {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const { return _map->_values[_map->_slots[_bucket]]; }
        pointer operator->() const { return _map->_values + _map->_slots[_bucket]; }

        bool operator==(const iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const iterator& rhs) const { return _bucket != rhs._bucket; }
        bool operator==(const const_iterator& rhs) const { return _bucket == rhs._bucket; }
        bool operator!=(const const_iterator& rhs) const { return _bucket != rhs._bucket; }

    private:
        void goto_next_element() {
            _bmask &= _bmask - 1;
            if (_bmask != 0) {
                _bucket = _from + CTZ(static_cast<uint32_t>(_bmask));
                return;
            }

            const auto num_buckets = _map->bucket_count();
            do {
                _from += simd_bytes;
            } while (_from < num_buckets && (_bmask = _map->filled_mask(_from)) == 0);

            _bucket = (_bmask == 0) ? num_buckets : _from + CTZ(static_cast<uint32_t>(_bmask));
        }

    public:
        const htype* _map;
        size_t _bmask = 0;
        size_t _bucket;
        size_t _from;
    };

    explicit HashMap(size_t n = 4) noexcept { init(n); }

    explicit HashMap(const AllocT& alloc) noexcept : _alloc(alloc), _state_alloc(alloc), _slot_alloc(alloc) { init(4); }

    HashMap(size_t n, const AllocT& alloc) noexcept : _alloc(alloc), _state_alloc(alloc), _slot_alloc(alloc) {
        init(n);
    }

    HashMap(const HashMap& other)
        : _alloc(PairAllocTraits::select_on_container_copy_construction(other._alloc)),
          _state_alloc(StateAllocTraits::select_on_container_copy_construction(other._state_alloc)),
          _slot_alloc(SlotAllocTraits::select_on_container_copy_construction(other._slot_alloc)) {
        clone(other);
    }

    HashMap(const HashMap& other, const AllocT& alloc) : _alloc(alloc), _state_alloc(alloc), _slot_alloc(alloc) {
        clone(other);
    }

    HashMap(HashMap&& other) noexcept
        : _alloc(other._alloc), _state_alloc(other._state_alloc), _slot_alloc(other._slot_alloc) {
        swap(other);
    }

    HashMap(HashMap&& other, const AllocT& alloc) noexcept : _alloc(alloc), _state_alloc(alloc), _slot_alloc(alloc) {
        swap(other);
    }

    HashMap(std::initializer_list<value_type> il) noexcept {
        init(static_cast<size_t>(il.size()));
        for (auto it = il.begin(); it != il.end(); ++it)
            insert(*it);
    }

    template <class InputIt> HashMap(InputIt first, InputIt last, size_t bucket_count = 4) noexcept {
        init(static_cast<size_t>(std::distance(first, last)) + bucket_count);
        for (; first != last; ++first)
            insert(*first);
    }

    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            if constexpr (PairAllocTraits::propagate_on_container_copy_assignment::value) {
                _alloc = other._alloc;
                _state_alloc = other._state_alloc;
                _slot_alloc = other._slot_alloc;
            }
            clone(other);
        }
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap() noexcept {
        clear_data();
        deallocate_all();
    }

    [[nodiscard]] allocator_type get_allocator() const noexcept { return allocator_type(_alloc); }

    void clone(const HashMap& other) {
        if (this == &other)
            return;

        clear();
        if (other.empty())
            return;

        rehash(other._num_filled + 1);
        reserve_values(other._num_filled);

        for (auto it = other.begin(); it != other.end(); ++it)
            insert_unique(it->first, it->second);
    }

    void swap(HashMap& other) noexcept {
        std::swap(_hasher, other._hasher);
        std::swap(_eq, other._eq);
        std::swap(_alloc, other._alloc);
        std::swap(_state_alloc, other._state_alloc);
        std::swap(_slot_alloc, other._slot_alloc);
        std::swap(_values, other._values);
        std::swap(_states, other._states);
        std::swap(_slots, other._slots);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_value_size, other._value_size);
        std::swap(_value_cap, other._value_cap);
        std::swap(_num_deleted, other._num_deleted);
        std::swap(_free_head, other._free_head);
        std::swap(_mask, other._mask);
    }

    iterator begin() noexcept { return {this, first_filled(0)}; }
    const_iterator cbegin() const noexcept { return {this, first_filled(0)}; }
    const_iterator begin() const noexcept { return cbegin(); }

    iterator end() noexcept { return {this, _num_buckets, false}; }
    const_iterator cend() const noexcept { return {this, _num_buckets, false}; }
    const_iterator end() const noexcept { return cend(); }

    size_t size() const noexcept { return _num_filled; }
    bool empty() const noexcept { return _num_filled == 0; }
    size_t bucket_count() const noexcept { return _num_buckets; }

    float load_factor() const noexcept {
        return _num_buckets ? static_cast<float>(_num_filled) / static_cast<float>(_num_buckets) : 0.0f;
    }

    float max_load_factor() const noexcept { return static_cast<float>(MXLOAD_FACTOR) / (MXLOAD_FACTOR + 1); }
    void max_load_factor(float) noexcept {}

    hasher hash_function() const noexcept { return _hasher; }
    key_equal key_eq() const noexcept { return _eq; }

    constexpr uint64_t max_size() const { return NIL - 1; }
    constexpr uint64_t max_bucket_count() const { return NIL - 1; }

    template <typename K = KeyT> iterator find(const K& key) noexcept {
        const auto bucket = find_filled_bucket(key);
        return bucket == NIL ? end() : iterator(this, bucket);
    }

    template <typename K = KeyT> const_iterator find(const K& key) const noexcept {
        const auto bucket = find_filled_bucket(key);
        return bucket == NIL ? cend() : const_iterator(this, bucket);
    }

    template <typename K = KeyT> bool contains(const K& key) const noexcept { return find_filled_bucket(key) != NIL; }

    template <typename K = KeyT> size_t count(const K& key) const noexcept { return find_filled_bucket(key) != NIL; }

    template <typename K = KeyT> ValueT& at(const K& key) {
        const auto bucket = find_filled_bucket(key);
        if (bucket == NIL)
            throw std::out_of_range("emilib::HashMap::at(): key not found");
        return _values[_slots[bucket]].second;
    }

    template <typename K = KeyT> const ValueT& at(const K& key) const {
        const auto bucket = find_filled_bucket(key);
        if (bucket == NIL)
            throw std::out_of_range("emilib::HashMap::at(): key not found");
        return _values[_slots[bucket]].second;
    }

    template <typename K = KeyT> ValueT* try_get(const K& key) noexcept {
        const auto bucket = find_filled_bucket(key);
        return bucket == NIL ? nullptr : &_values[_slots[bucket]].second;
    }

    template <typename K = KeyT> const ValueT* try_get(const K& key) const noexcept {
        const auto bucket = find_filled_bucket(key);
        return bucket == NIL ? nullptr : &_values[_slots[bucket]].second;
    }

    template <typename K = KeyT>
    bool try_set(const K& key, const ValueT& val) noexcept(std::is_nothrow_copy_assignable<ValueT>::value) {
        const auto bucket = find_filled_bucket(key);
        if (bucket == NIL)
            return false;
        _values[_slots[bucket]].second = val;
        return true;
    }

    template <typename K = KeyT>
    bool try_set(const K& key, ValueT&& val) noexcept(std::is_nothrow_move_assignable<ValueT>::value) {
        const auto bucket = find_filled_bucket(key);
        if (bucket == NIL)
            return false;
        _values[_slots[bucket]].second = std::move(val);
        return true;
    }

    template <typename Con> bool operator==(const Con& rhs) const noexcept {
        if (size() != rhs.size())
            return false;

        for (auto it = begin(), last = end(); it != last; ++it) {
            auto oi = rhs.find(it->first);
            if (oi == rhs.end() || it->second != oi->second)
                return false;
        }
        return true;
    }

    template <typename Con> bool operator!=(const Con& rhs) const noexcept { return !(*this == rhs); }

    void merge(HashMap& rhs) noexcept {
        if (this == &rhs)
            return;
        if (empty()) {
            *this = std::move(rhs);
            return;
        }

        for (auto rit = rhs.begin(); rit != rhs.end();) {
            auto fit = find(rit->first);
            if (fit == end()) {
                insert_unique(rit->first, std::move(rit->second));
                rit = rhs.erase(rit);
            } else {
                ++rit;
            }
        }
    }

    template <typename K, typename V> std::pair<iterator, bool> do_insert(K&& key, V&& val) noexcept {
        bool bnew = true;
        const auto bucket = find_or_allocate(key, bnew);
        const auto slot = _slots[bucket];
        if (bnew) {
            new (_values + slot) PairT(std::forward<K>(key), std::forward<V>(val));
        }
        return {{this, bucket}, bnew};
    }

    std::pair<iterator, bool> do_insert(const value_type& value) noexcept {
        return do_insert(value.first, value.second);
    }

    std::pair<iterator, bool> do_insert(value_type&& value) noexcept {
        bool bnew = true;
        const auto bucket = find_or_allocate(value.first, bnew);
        const auto slot = _slots[bucket];
        if (bnew) {
            new (_values + slot) PairT(std::move(value));
        }
        return {{this, bucket}, bnew};
    }

    template <class... Args> inline std::pair<iterator, bool> emplace(Args&&... args) noexcept {
        return do_insert(std::forward<Args>(args)...);
    }

    template <class... Args> iterator emplace_hint(const_iterator, Args&&... args) noexcept {
        return emplace(std::forward<Args>(args)...).first;
    }

    std::pair<iterator, bool> insert(value_type&& value) noexcept { return do_insert(std::move(value)); }
    std::pair<iterator, bool> insert(const value_type& value) noexcept { return do_insert(value); }

    template <typename Iter, typename = decltype(*std::declval<Iter&>())> void insert(Iter beginc, Iter endc) noexcept {
        reserve(static_cast<size_t>(std::distance(beginc, endc)) + _num_filled);
        for (; beginc != endc; ++beginc)
            do_insert(beginc->first, beginc->second);
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) noexcept {
        return do_insert(key, std::forward<Args>(args)...);
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) noexcept {
        return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
    }

    void insert(std::initializer_list<value_type> ilist) noexcept {
        reserve(static_cast<size_t>(ilist.size()) + _num_filled);
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            (void)do_insert(*it);
    }

    template <typename K, typename V> size_t insert_unique(K&& key, V&& val) noexcept {
        maybe_compact();
        ensure_insert_capacity();

        const auto kh = static_cast<uint64_t>(_hasher(key));
        const auto bucket = probe_empty(_states, kh);
        const auto slot = alloc_slot(); // grow_values scans states, so mark after
        _states[bucket] = fingerprint_of(kh);
        _slots[bucket] = slot;
        _num_filled++;
        new (_values + slot) PairT(std::forward<K>(key), std::forward<V>(val));
        return bucket;
    }

    size_t insert_unique(const value_type& value) noexcept { return insert_unique(value.first, value.second); }

    template <class M> std::pair<iterator, bool> insert_or_assign(const KeyT& key, M&& val) {
        return do_assign(key, std::forward<M>(val));
    }

    template <class M> std::pair<iterator, bool> insert_or_assign(KeyT&& key, M&& val) {
        return do_assign(std::move(key), std::forward<M>(val));
    }

    template <typename K, typename V> std::pair<iterator, bool> do_assign(K&& key, V&& val) {
        bool bnew = true;
        const auto bucket = find_or_allocate(key, bnew);
        const auto slot = _slots[bucket];
        if (bnew) {
            new (_values + slot) PairT(std::forward<K>(key), std::forward<V>(val));
        } else {
            _values[slot].second = std::forward<V>(val);
        }
        return {{this, bucket}, bnew};
    }

    bool set_get(const KeyT& key, const ValueT& val, ValueT& oldv) {
        bool bnew = true;
        const auto bucket = find_or_allocate(key, bnew);
        const auto slot = _slots[bucket];
        if (bnew) {
            new (_values + slot) PairT(key, val);
        } else {
            oldv = _values[slot].second;
        }
        return bnew;
    }

    ValueT& operator[](const KeyT& key) noexcept {
        bool bnew = true;
        const auto bucket = find_or_allocate(key, bnew);
        const auto slot = _slots[bucket];
        if (bnew) {
            new (_values + slot) PairT(key, ValueT());
        }
        return _values[slot].second;
    }

    ValueT& operator[](KeyT&& key) noexcept {
        bool bnew = true;
        const auto bucket = find_or_allocate(key, bnew);
        const auto slot = _slots[bucket];
        if (bnew) {
            new (_values + slot) PairT(std::move(key), ValueT());
        }
        return _values[slot].second;
    }

    size_t erase(const KeyT& key) noexcept {
        const auto bucket = find_filled_bucket(key);
        if (bucket == NIL)
            return 0;

        _erase(bucket);
        return 1;
    }

    iterator erase(const const_iterator& cit) noexcept {
        iterator next(this, cit._bucket);
        ++next;
        _erase(cit._bucket);
        return next;
    }

    iterator erase(iterator it) noexcept {
        iterator next = it;
        ++next;
        _erase(it._bucket);
        return next;
    }

    // iterator-shaped fast erase (API parity with emihmap1.hpp)
    void _erase(const const_iterator& cit) noexcept { _erase(cit._bucket); }

    iterator erase(const_iterator first, const_iterator last) {
        auto next = first;
        while (next != last)
            next = erase(next);
        return iterator(this, next._bucket);
    }

    template <typename Pred> size_t erase_if(Pred pred) noexcept {
        auto old_size = size();
        for (auto it = begin(), last = end(); it != last;) {
            if (pred(*it)) {
                it = erase(it);
            } else {
                ++it;
            }
        }
        return old_size - size();
    }

    void clear_meta() noexcept {
        if (_states != nullptr)
            std::memset(_states, EEMPTY, _num_buckets);
        _num_filled = 0;
        _value_size = 0;
        _num_deleted = 0;
        _free_head = NIL;
    }

    void clear_data() noexcept {
        if (need_explicit_dtor()) {
            for (size_t b = 0; b < _num_buckets; ++b) {
                if (_states[b] > EDELETE)
                    _values[_slots[b]].~PairT();
            }
        }
    }

    void clear() noexcept {
        clear_data();
        clear_meta();
    }

    void shrink_to_fit() {
        compact();
        rehash(_num_filled + 1);
    }

    bool reserve(size_t num_elems) noexcept {
        const auto required_buckets = num_elems + num_elems / MXLOAD_FACTOR + 1;
        const bool changed = required_buckets >= _num_buckets;
        if (changed)
            rehash(num_elems + 1);
        reserve_values(num_elems);
        return changed;
    }

    void dump_statics() const {
        std::printf("append-only state=%.3f size=%u buckets=%u values=%u deleted=%u\n",
                    static_cast<double>(load_factor()), _num_filled, _num_buckets, _value_size, _num_deleted);
    }

    void rehash(size_t num_elems) noexcept {
        if (num_elems < _num_filled)
            return;

        auto num_buckets = static_cast<size_t>(simd_bytes);
        const auto required = static_cast<uint64_t>(num_elems) * (MXLOAD_FACTOR + 1) / MXLOAD_FACTOR + 1;
        while (static_cast<uint64_t>(num_buckets) < required)
            num_buckets <<= 1;

        assert(num_buckets > _num_filled);

        auto* new_states = StateAllocTraits::allocate(_state_alloc, num_buckets);
        auto* new_slots = SlotAllocTraits::allocate(_slot_alloc, num_buckets);
        std::memset(new_states, EEMPTY, num_buckets);

        auto* old_states = _states;
        auto* old_slots = _slots;
        const auto old_buckets = _num_buckets;

        _states = new_states;
        _slots = new_slots;
        _num_buckets = num_buckets;
        _mask = num_buckets - 1;
        _num_deleted = 0;

        if (old_states != nullptr) {
            for (size_t b = 0; b < old_buckets; ++b) {
                if (old_states[b] <= EDELETE)
                    continue;
                const auto slot = old_slots[b];
                const auto kh = static_cast<uint64_t>(_hasher(_values[slot].first));
                const auto nb = probe_empty(_states, kh);
                _states[nb] = fingerprint_of(kh);
                _slots[nb] = slot;
            }
        }

        if (old_states != nullptr)
            StateAllocTraits::deallocate(_state_alloc, old_states, old_buckets);
        if (old_slots != nullptr)
            SlotAllocTraits::deallocate(_slot_alloc, old_slots, old_buckets);
    }

private:
    void init(size_t n) noexcept {
        rehash(n);
        reserve_values(n);
    }

    void deallocate_all() noexcept {
        if (_values != nullptr)
            PairAllocTraits::deallocate(_alloc, _values, _value_cap);
        if (_states != nullptr)
            StateAllocTraits::deallocate(_state_alloc, _states, _num_buckets);
        if (_slots != nullptr)
            SlotAllocTraits::deallocate(_slot_alloc, _slots, _num_buckets);

        _values = nullptr;
        _states = nullptr;
        _slots = nullptr;
        _value_cap = 0;
    }

    static constexpr bool need_explicit_dtor() { return !std::is_trivially_destructible<PairT>::value; }

    inline size_t filled_mask(size_t gbucket) const noexcept {
        const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(_states + gbucket));
        return static_cast<size_t>(MOVEMASK_EPI8(CMPGT_EPI8(vec, simd_delete)));
    }

    size_t first_filled(size_t bucket) const noexcept {
        while (bucket < _num_buckets) {
            const auto mask = filled_mask(bucket);
            if (mask != 0)
                return bucket + CTZ(static_cast<uint32_t>(mask));
            bucket += simd_bytes;
        }
        return _num_buckets;
    }

    static int8_t fingerprint_of(uint64_t key_hash) noexcept {
        return static_cast<int8_t>(static_cast<size_t>(key_hash % 253) + static_cast<size_t>(EFILLED));
    }

    template <typename K> size_t find_filled_bucket(const K& key) const noexcept {
        if (_num_buckets == 0)
            return NIL;

        const auto key_hash = static_cast<uint64_t>(_hasher(key));
        const auto raw = static_cast<size_t>(key_hash) & _mask;
        auto next_bucket = raw - raw % simd_bytes;
        auto skip = static_cast<uint32_t>(raw % simd_bytes);
        const auto filled = SET1_EPI8(fingerprint_of(key_hash));

        for (;;) {
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(_states + next_bucket));
            const auto valid = static_cast<uint32_t>(~((1u << skip) - 1));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & valid;
            const auto maske = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty))) & valid;
            const auto stop = maske ? CTZ(maske) : static_cast<uint32_t>(simd_bytes);
            maskf &= (stop >= simd_bytes) ? ~0u : ((1u << stop) - 1);

            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (EMH_LIKELY(_eq(_values[_slots[fbucket]].first, key)))
                    return fbucket;
                maskf &= maskf - 1;
            }

            if (stop < simd_bytes)
                return NIL;

            skip = 0;
            next_bucket = (next_bucket + simd_bytes) & _mask;
        }
    }

    // Probe for the first empty state at/after the key's home position. Works
    // on an arbitrary state array so rehash/compact can build fresh bucket
    // arrays without touching the live one.
    size_t probe_empty(int8_t* states, uint64_t key_hash) const noexcept {
        const auto raw = static_cast<size_t>(key_hash) & _mask;
        auto next_bucket = raw - raw % simd_bytes;
        auto skip = static_cast<uint32_t>(raw % simd_bytes);

        for (;;) {
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(states + next_bucket));
            auto maske = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty)));
            maske &= static_cast<uint32_t>(~((1u << skip) - 1));
            if (maske != 0)
                return next_bucket + CTZ(maske);
            skip = 0;
            next_bucket = (next_bucket + simd_bytes) & _mask;
        }
    }

    template <typename K> size_t find_or_allocate(const K& key, bool& bnew) noexcept {
        maybe_compact();
        ensure_insert_capacity();

        constexpr size_t chole = NIL;
        const auto key_hash = static_cast<uint64_t>(_hasher(key));
        const auto raw = static_cast<size_t>(key_hash) & _mask;
        auto next_bucket = raw - raw % simd_bytes;
        auto skip = static_cast<uint32_t>(raw % simd_bytes);
        const auto key_h2 = fingerprint_of(key_hash);
        const auto filled = SET1_EPI8(key_h2);
        size_t hole = chole;

        prefetch_write(_slots + raw);
        prefetch_write(_values + _value_size);

        for (;;) {
            const auto vec = LOAD_EPI8(reinterpret_cast<decltype(&simd_empty)>(_states + next_bucket));
            const auto valid = static_cast<uint32_t>(~((1u << skip) - 1));
            auto maskf = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, filled))) & valid;

            while (maskf != 0) {
                const auto fbucket = next_bucket + CTZ(maskf);
                if (_eq(_values[_slots[fbucket]].first, key)) {
                    bnew = false;
                    return fbucket;
                }
                maskf &= maskf - 1;
            }

            const auto maske = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_empty))) & valid;
            const auto maskd = static_cast<uint32_t>(MOVEMASK_EPI8(CMPEQ_EPI8(vec, simd_delete))) & valid;

            if (maske != 0) {
                size_t bucket;
                bool reuse_deleted = false;
                if (hole != chole) {
                    bucket = hole;
                    reuse_deleted = true;
                } else if (maskd != 0 && CTZ(maskd) < CTZ(maske)) {
                    bucket = next_bucket + CTZ(maskd);
                    reuse_deleted = true;
                } else {
                    bucket = next_bucket + CTZ(maske);
                }

                const auto slot = alloc_slot(); // grow_values scans states, so mark after
                _states[bucket] = key_h2;
                _slots[bucket] = slot;
                _num_filled++;
                if (reuse_deleted)
                    _num_deleted--;
                bnew = true;
                return bucket;
            }

            if (maskd != 0 && hole == chole) {
                hole = next_bucket + CTZ(maskd);
            }

            skip = 0;
            next_bucket = (next_bucket + simd_bytes) & _mask;
        }
    }

    void ensure_insert_capacity() noexcept {
        if (static_cast<uint64_t>(_num_filled + _num_deleted + 1) * (MXLOAD_FACTOR + 1) >
            static_cast<uint64_t>(_num_buckets) * MXLOAD_FACTOR) {
            rehash(_num_filled + 1);
        }
    }

    void maybe_compact() noexcept {
        // Dead slots are normally recycled through the free list in _erase, so
        // compaction only has to reclaim memory when dead slots clearly
        // outnumber live ones. A tighter ratio thrashes on erase/reinsert.
        if (static_cast<uint64_t>(_value_size) > static_cast<uint64_t>(_num_filled) * 2 + 4096)
            compact();
    }

    // The free list is threaded through the dead slot storage itself: no extra
    // array, and erase + reinsert reuses a slot instead of forcing compaction.
    static void set_free_link(PairT* values, uint32_t slot, uint32_t next) noexcept {
        std::memcpy(static_cast<void*>(values + slot), &next, sizeof(next));
    }

    static uint32_t get_free_link(const PairT* values, uint32_t slot) noexcept {
        uint32_t next;
        std::memcpy(&next, static_cast<const void*>(values + slot), sizeof(next));
        return next;
    }

    uint32_t alloc_slot() noexcept {
        if (_free_head != NIL) {
            const auto slot = _free_head;
            _free_head = get_free_link(_values, slot);
            return slot;
        }
        if (_value_size == _value_cap)
            grow_values(_value_cap ? _value_cap * 2 : static_cast<size_t>(simd_bytes));
        return _value_size++;
    }

    void reserve_values(size_t n) noexcept {
        if (n > _value_cap)
            grow_values(n > _value_cap * 2 ? n : _value_cap * 2);
    }

    void grow_values(size_t new_cap) noexcept {
        auto* new_values = PairAllocTraits::allocate(_alloc, new_cap);

        for (size_t b = 0; b < _num_buckets; ++b) {
            if (_states[b] <= EDELETE)
                continue;
            const auto slot = _slots[b];
            new (new_values + slot) PairT(std::move(_values[slot]));
        }

        if (need_explicit_dtor()) {
            for (size_t b = 0; b < _num_buckets; ++b) {
                if (_states[b] > EDELETE)
                    _values[_slots[b]].~PairT();
            }
        }

        if (_values != nullptr) {
            // free-list links live in the dead slot storage, so carry them over
            for (auto s = _free_head; s != NIL;) {
                const auto next = get_free_link(_values, s);
                set_free_link(new_values, s, next);
                s = next;
            }
            PairAllocTraits::deallocate(_alloc, _values, _value_cap);
        }

        _values = new_values;
        _value_cap = new_cap;
    }

    void compact() noexcept {
        if (_num_filled == 0) {
            clear_data();
            clear_meta();
            return;
        }

        const auto new_cap = _num_filled + _num_filled / 2 + 1;
        auto* new_values = PairAllocTraits::allocate(_alloc, new_cap);
        auto* new_states = StateAllocTraits::allocate(_state_alloc, _num_buckets);
        auto* new_slots = SlotAllocTraits::allocate(_slot_alloc, _num_buckets);
        std::memset(new_states, EEMPTY, _num_buckets);

        auto* old_values = _values;
        auto* old_states = _states;
        auto* old_slots = _slots;
        const auto old_buckets = _num_buckets;

        size_t dst = 0;
        for (size_t b = 0; b < old_buckets; ++b) {
            if (old_states[b] <= EDELETE)
                continue;

            const auto oslot = old_slots[b];
            new (new_values + dst) PairT(std::move(old_values[oslot]));

            const auto kh = static_cast<uint64_t>(_hasher(new_values[dst].first));
            const auto nb = probe_empty(new_states, kh);
            new_states[nb] = fingerprint_of(kh);
            new_slots[nb] = static_cast<uint32_t>(dst);
            ++dst;
        }

        if (need_explicit_dtor()) {
            for (size_t b = 0; b < old_buckets; ++b) {
                if (old_states[b] > EDELETE)
                    old_values[old_slots[b]].~PairT();
            }
        }

        if (old_values != nullptr)
            PairAllocTraits::deallocate(_alloc, old_values, _value_cap);
        StateAllocTraits::deallocate(_state_alloc, old_states, old_buckets);
        SlotAllocTraits::deallocate(_slot_alloc, old_slots, old_buckets);

        _values = new_values;
        _states = new_states;
        _slots = new_slots;
        _value_cap = new_cap;
        _value_size = dst;
        _num_deleted = 0;
        _free_head = NIL;
    }

    // raw-bucket fast erase (private: an int key argument would silently
    // convert to a bucket index and erase the wrong element)
    void _erase(size_t bucket) noexcept {
        const auto slot = _slots[bucket];
        if (need_explicit_dtor())
            _values[slot].~PairT();

        set_free_link(_values, slot, _free_head);
        _free_head = slot;

        _states[bucket] = EDELETE;
        _num_filled--;
        _num_deleted++;
    }

private:
    PairT* _values = nullptr;
    int8_t* _states = nullptr;
    uint32_t* _slots = nullptr;

    size_t _num_buckets = 0;
    size_t _num_filled = 0;
    size_t _value_size = 0;
    size_t _value_cap = 0;
    size_t _num_deleted = 0;
    uint32_t _free_head = NIL;
    size_t _mask = 0;

    HashT _hasher;
    EqT _eq;
    PairAlloc _alloc;
    StateAlloc _state_alloc;
    SlotAlloc _slot_alloc;
};

template <typename K, typename V, typename H, typename E, typename A>
inline bool HashMap<K, V, H, E, A>::iterator::operator==(const const_iterator& rhs) const {
    return _bucket == rhs._bucket;
}

template <typename K, typename V, typename H, typename E, typename A>
inline bool HashMap<K, V, H, E, A>::iterator::operator!=(const const_iterator& rhs) const {
    return _bucket != rhs._bucket;
}

} // namespace emilib
