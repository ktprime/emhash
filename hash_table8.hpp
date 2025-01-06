// emhash8::HashMap for C++14/17/20
// version 1.7.1
// https://github.com/ktprime/emhash/blob/master/hash_table8.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2025 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

#pragma once

#include <cstring>
#include <string>
#include <cstdlib>
#include <type_traits>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>
#include <algorithm>
#include <memory>
#include <limits>

// ---------------------------------
// Versie-checks C++17 / C++20
// ---------------------------------
#if __cplusplus >= 202002L
    #define EMH_CONSTEXPR_20 constexpr
    #include <ranges>
#else
    #define EMH_CONSTEXPR_20 inline
#endif

#if __cplusplus >= 201703L
    #define EMH_CONSTEXPR_17 constexpr
#else
    #define EMH_CONSTEXPR_17 inline
#endif

#if defined(_MSC_VER)
    #define EMH_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define EMH_FORCE_INLINE inline __attribute__((always_inline))
#else
    #define EMH_FORCE_INLINE inline
#endif

// Likely/unlikely hints
#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
    #define EMH_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define EMH_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define EMH_LIKELY(x)   (x)
    #define EMH_UNLIKELY(x) (x)
#endif

// Extra sentinel buckets
#ifndef EMH_EAD
    #define EMH_EAD 2
#endif

// Optional macros (kunnen worden gedefinieerd voordat deze header wordt geincludeerd)
#ifndef EMH_QUADRATIC
    #define EMH_QUADRATIC 1 // Schakel kwadratische probing in
#endif

#ifndef EMH_INT_HASH
    #define EMH_INT_HASH 1 // Eenvoudige integer hashing
#endif

#ifndef EMH_WYHASH_HASH
    #define EMH_WYHASH_HASH 1 // WyHash voor string hashing
#endif

#ifndef EMH_PREFETCH
    #define EMH_PREFETCH 1 // Schakel prefetching in
#endif

namespace emhash8 {

/// DefaultPolicy
struct DefaultPolicy {
    static constexpr float load_factor      = 0.80f;
    static constexpr float min_load_factor  = 0.20f;
    static constexpr size_t cacheline_size  = 64U;
};

// ----------------------------------------------------------------------------
// Ranges-helpers voor keys() en values()
// ----------------------------------------------------------------------------

// ValuesRange: Itereert over alleen de values
template<typename PairT>
class ValuesRange {
public:
    using pointer   = PairT*;
    using reference = typename std::conditional<
        std::is_const<PairT>::value,
        const typename PairT::second_type&,
        typename PairT::second_type&
    >::type;

    class iterator {
    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = typename PairT::second_type;
        using reference         = reference;
        using pointer           = PairT*;
        using iterator_category = std::random_access_iterator_tag;

        EMH_CONSTEXPR_17 iterator(pointer ptr) noexcept : _ptr(ptr) {}

        EMH_CONSTEXPR_17 reference operator*()  const noexcept { return _ptr->second; }
        EMH_CONSTEXPR_17 pointer   operator->() const noexcept { return _ptr; }

        EMH_CONSTEXPR_17 iterator& operator++()    noexcept { ++_ptr; return *this; }
        EMH_CONSTEXPR_17 iterator  operator++(int) noexcept { iterator tmp(*this); ++(*this); return tmp; }

        EMH_CONSTEXPR_17 iterator& operator--()    noexcept { --_ptr; return *this; }
        EMH_CONSTEXPR_17 iterator  operator--(int) noexcept { iterator tmp(*this); --(*this); return tmp; }

        EMH_CONSTEXPR_17 bool operator==(const iterator& o) const noexcept { return _ptr == o._ptr; }
        EMH_CONSTEXPR_17 bool operator!=(const iterator& o) const noexcept { return _ptr != o._ptr; }

        // Optioneel: random access
        EMH_CONSTEXPR_17 iterator& operator+=(std::ptrdiff_t n) noexcept { _ptr += n; return *this; }
        EMH_CONSTEXPR_17 iterator& operator-=(std::ptrdiff_t n) noexcept { _ptr -= n; return *this; }
        EMH_CONSTEXPR_17 iterator  operator+(std::ptrdiff_t n)  const noexcept { return iterator(_ptr + n); }
        EMH_CONSTEXPR_17 iterator  operator-(std::ptrdiff_t n)  const noexcept { return iterator(_ptr - n); }
        EMH_CONSTEXPR_17 std::ptrdiff_t operator-(const iterator& o) const noexcept { return _ptr - o._ptr; }

    private:
        pointer _ptr;
    };

    EMH_CONSTEXPR_17 ValuesRange(pointer b, pointer e) noexcept : _begin(b), _end(e) {}

    EMH_CONSTEXPR_17 iterator begin() const noexcept { return iterator(_begin); }
    EMH_CONSTEXPR_17 iterator end()   const noexcept { return iterator(_end);   }

    EMH_CONSTEXPR_17 std::size_t size() const noexcept { return static_cast<std::size_t>(_end - _begin); }
    EMH_CONSTEXPR_17 bool empty() const noexcept { return _begin == _end; }

private:
    pointer _begin;
    pointer _end;
};

// KeysRange: Itereert over alleen de keys
template<typename PairT>
class KeysRange {
public:
    using pointer   = PairT*;
    using reference = typename std::conditional<
        std::is_const<PairT>::value,
        const typename PairT::first_type&,
        typename PairT::first_type&
    >::type;

    class iterator {
    public:
        using difference_type   = std::ptrdiff_t;
        using value_type        = typename PairT::first_type;
        using reference         = reference;
        using pointer           = PairT*;
        using iterator_category = std::random_access_iterator_tag;

        EMH_CONSTEXPR_17 iterator(pointer ptr) noexcept : _ptr(ptr) {}

        EMH_CONSTEXPR_17 reference operator*()  const noexcept { return _ptr->first; }
        EMH_CONSTEXPR_17 pointer   operator->() const noexcept { return _ptr; }

        EMH_CONSTEXPR_17 iterator& operator++()    noexcept { ++_ptr; return *this; }
        EMH_CONSTEXPR_17 iterator  operator++(int) noexcept { iterator tmp(*this); ++(*this); return tmp; }

        EMH_CONSTEXPR_17 iterator& operator--()    noexcept { --_ptr; return *this; }
        EMH_CONSTEXPR_17 iterator  operator--(int) noexcept { iterator tmp(*this); --(*this); return tmp; }

        EMH_CONSTEXPR_17 bool operator==(const iterator& o) const noexcept { return _ptr == o._ptr; }
        EMH_CONSTEXPR_17 bool operator!=(const iterator& o) const noexcept { return _ptr != o._ptr; }

        // Optioneel: random access
        EMH_CONSTEXPR_17 iterator& operator+=(std::ptrdiff_t n) noexcept { _ptr += n; return *this; }
        EMH_CONSTEXPR_17 iterator& operator-=(std::ptrdiff_t n) noexcept { _ptr -= n; return *this; }
        EMH_CONSTEXPR_17 iterator  operator+(std::ptrdiff_t n)  const noexcept { return iterator(_ptr + n); }
        EMH_CONSTEXPR_17 iterator  operator-(std::ptrdiff_t n)  const noexcept { return iterator(_ptr - n); }
        EMH_CONSTEXPR_17 std::ptrdiff_t operator-(const iterator& o) const noexcept { return _ptr - o._ptr; }

    private:
        pointer _ptr;
    };

    EMH_CONSTEXPR_17 KeysRange(pointer b, pointer e) noexcept : _begin(b), _end(e) {}

    EMH_CONSTEXPR_17 iterator begin() const noexcept { return iterator(_begin); }
    EMH_CONSTEXPR_17 iterator end()   const noexcept { return iterator(_end);   }

    EMH_CONSTEXPR_17 std::size_t size() const noexcept { return static_cast<std::size_t>(_end - _begin); }
    EMH_CONSTEXPR_17 bool empty() const noexcept { return _begin == _end; }

private:
    pointer _begin;
    pointer _end;
};

// ----------------------------------------------------------------------------
// HashMap
// ----------------------------------------------------------------------------
template <
    typename KeyT,
    typename ValueT,
    typename HashT      = std::hash<KeyT>,
    typename EqT        = std::equal_to<KeyT>,
    typename Allocator  = std::allocator<std::pair<KeyT, ValueT>>,
    typename Policy     = DefaultPolicy
>
class HashMap {
private:
#ifndef EMH_DEFAULT_LOAD_FACTOR
    static constexpr float EMH_DEFAULT_LOAD_FACTOR = 0.80f;
#endif

    static constexpr float    EMH_MIN_LOAD_FACTOR   = 0.25f; //< 0.5
    static constexpr uint32_t EMH_CACHE_LINE_SIZE  = 64; // Debug only

public:
#ifdef EMH_SMALL_TYPE
    using size_type = uint16_t;
#elif EMH_SIZE_TYPE == 0
    using size_type = uint32_t;
#else
    using size_type = size_t;
#endif

    using key_type      = KeyT;
    using mapped_type   = ValueT;
    using value_type    = std::pair<KeyT, ValueT>;
    using hasher        = HashT;
    using key_equal     = EqT;

private:
    // Elk bucket heeft:
    //   next: == INACTIVE => empty, anders => index van volgend bucket in chain
    //   slot: index in _pairs array + top bits van de hash
    struct Index {
        size_type next;
        size_type slot;
    };

    static constexpr size_type INACTIVE = (std::numeric_limits<size_type>::max)();
    static constexpr size_type EAD      = EMH_EAD;

    // Forward-declare
    class const_iterator;

public:
    // -----------------------------------------------
    // Iterator
    // -----------------------------------------------
    class iterator {
    public:
        using difference_type       = std::ptrdiff_t;
        using value_type            = HashMap::value_type;
        using reference             = value_type&;
        using pointer               = value_type*;
        using iterator_category     = std::bidirectional_iterator_tag;
        using iterator_concept      = std::bidirectional_iterator_tag;

        EMH_CONSTEXPR_17 iterator() noexcept
            : _kv(nullptr), _end(nullptr)
        {}

        EMH_CONSTEXPR_17 iterator(const iterator&) noexcept = default;
        EMH_CONSTEXPR_17 iterator& operator=(const iterator&) noexcept = default;

        // Van const_iterator naar iterator
        EMH_CONSTEXPR_17 iterator(const const_iterator& rhs) noexcept
            : _kv(rhs._kv), _end(rhs._end)
        {}

        EMH_CONSTEXPR_17 iterator(pointer ptr, pointer end) noexcept
            : _kv(ptr), _end(end)
        {}

        EMH_CONSTEXPR_17 iterator& operator++() noexcept {
            ++_kv;
            return *this;
        }

        EMH_CONSTEXPR_17 iterator operator++(int) noexcept {
            iterator old(*this);
            ++(*this);
            return old;
        }

        EMH_CONSTEXPR_17 iterator& operator--() noexcept {
            --_kv;
            return *this;
        }

        EMH_CONSTEXPR_17 iterator operator--(int) noexcept {
            iterator old(*this);
            --(*this);
            return old;
        }

        EMH_CONSTEXPR_17 reference operator*()  const noexcept { return *_kv; }
        EMH_CONSTEXPR_17 pointer   operator->() const noexcept { return _kv; }

        EMH_CONSTEXPR_17 bool operator==(const iterator& rhs) const noexcept { return _kv == rhs._kv; }
        EMH_CONSTEXPR_17 bool operator!=(const iterator& rhs) const noexcept { return _kv != rhs._kv; }

        EMH_CONSTEXPR_17 bool operator==(const const_iterator& rhs) const noexcept { return _kv == rhs._kv; }
        EMH_CONSTEXPR_17 bool operator!=(const const_iterator& rhs) const noexcept { return _kv != rhs._kv; }

    private:
        friend class const_iterator;
        pointer _kv;
        pointer _end;
    };

    // -----------------------------------------------
    // Const Iterator
    // -----------------------------------------------
    class const_iterator {
    public:
        using difference_type       = std::ptrdiff_t;
        using value_type            = HashMap::value_type;
        using reference             = const value_type&;
        using pointer               = const value_type*;
        using iterator_category     = std::bidirectional_iterator_tag;
        using iterator_concept      = std::bidirectional_iterator_tag;

        EMH_CONSTEXPR_17 const_iterator() noexcept
            : _kv(nullptr), _end(nullptr)
        {}

        EMH_CONSTEXPR_17 const_iterator(const const_iterator&) noexcept = default;
        EMH_CONSTEXPR_17 const_iterator& operator=(const const_iterator&) noexcept = default;

        // Van iterator naar const_iterator
        EMH_CONSTEXPR_17 const_iterator(const iterator& rhs) noexcept
            : _kv(rhs._kv), _end(rhs._end)
        {}

        EMH_CONSTEXPR_17 const_iterator(pointer ptr, pointer end) noexcept
            : _kv(ptr), _end(end)
        {}

        EMH_CONSTEXPR_17 const_iterator& operator++() noexcept {
            ++_kv;
            return *this;
        }

        EMH_CONSTEXPR_17 const_iterator operator++(int) noexcept {
            const_iterator old(*this);
            ++(*this);
            return old;
        }

        EMH_CONSTEXPR_17 const_iterator& operator--() noexcept {
            --_kv;
            return *this;
        }

        EMH_CONSTEXPR_17 const_iterator operator--(int) noexcept {
            const_iterator old(*this);
            --(*this);
            return old;
        }

        EMH_CONSTEXPR_17 reference operator*() const noexcept { return *_kv; }
        EMH_CONSTEXPR_17 pointer   operator->() const noexcept { return _kv; }

        EMH_CONSTEXPR_17 bool operator==(const const_iterator& rhs) const noexcept { return _kv == rhs._kv; }
        EMH_CONSTEXPR_17 bool operator!=(const const_iterator& rhs) const noexcept { return _kv != rhs._kv; }

        EMH_CONSTEXPR_17 bool operator==(const iterator& rhs) const noexcept { return _kv == rhs._kv; }
        EMH_CONSTEXPR_17 bool operator!=(const iterator& rhs) const noexcept { return _kv != rhs._kv; }

    private:
        friend class iterator;
        pointer _kv;
        pointer _end;
    };

public:
    // -----------------------------------------------
    // Constructors
    // -----------------------------------------------
    EMH_CONSTEXPR_20 explicit HashMap(size_type bucket_count = 2, float mlf = EMH_DEFAULT_LOAD_FACTOR) noexcept
        : _index(nullptr), _pairs(nullptr), _hasher(), _eq(),
          _mlf(0), _num_buckets(0), _mask(0), _num_filled(0), _last(0)
    {
        init(bucket_count, mlf);
    }

    EMH_CONSTEXPR_20 HashMap(const HashMap& rhs)
        : HashMap()
    {
        init(0, rhs.max_load_factor());
        operator=(rhs);
    }

    EMH_CONSTEXPR_20 HashMap(HashMap&& rhs) noexcept
        : HashMap()
    {
        init(0, rhs.max_load_factor());
        operator=(std::move(rhs));
    }

    template<typename It>
    EMH_CONSTEXPR_20 HashMap(It first, It last, size_type bucket_count = 4)
        : HashMap()
    {
        init(static_cast<size_type>(std::distance(first, last)) + bucket_count, EMH_DEFAULT_LOAD_FACTOR);
        for (; first != last; ++first) {
            emplace(*first);
        }
    }

    EMH_CONSTEXPR_20 HashMap(std::initializer_list<value_type> ilist)
        : HashMap()
    {
        init(static_cast<size_type>(ilist.size()), EMH_DEFAULT_LOAD_FACTOR);
        for (auto&& val : ilist) {
            emplace(val);
        }
    }

    EMH_CONSTEXPR_20 HashMap& operator=(const HashMap& rhs) {
        if (this == &rhs)
            return *this;

        clear();
        _eq      = rhs._eq;
        _hasher  = rhs._hasher;

        if (_num_buckets != rhs._num_buckets) {
            if (_pairs)  std::free(_pairs);
            if (_index)  std::free(_index);
            _pairs = nullptr;
            _index = nullptr;

            rebuild(rhs._num_buckets);
        }
        _num_filled  = rhs._num_filled;
        _mlf         = rhs._mlf;
        _last        = rhs._last;
        _mask        = rhs._mask;

        // Index kopiëren
        std::memcpy(_index, rhs._index, (_num_buckets + EAD) * sizeof(Index));

        // Pairs kopiëren
        if (is_copy_trivially()) {
            std::memcpy(_pairs, rhs._pairs, _num_filled * sizeof(value_type));
        } else {
            for (size_type i = 0; i < _num_filled; i++) {
                new (_pairs + i) value_type(rhs._pairs[i]);
            }
        }
        return *this;
    }

    EMH_CONSTEXPR_20 HashMap& operator=(HashMap&& rhs) noexcept {
        if (this != &rhs) {
            clearkv();
            if (_pairs) std::free(_pairs);
            if (_index) std::free(_index);

            _pairs       = rhs._pairs;
            _index       = rhs._index;
            _hasher      = std::move(rhs._hasher);
            _eq          = std::move(rhs._eq);
            _mlf         = rhs._mlf;
            _num_buckets = rhs._num_buckets;
            _mask        = rhs._mask;
            _num_filled  = rhs._num_filled;
            _last        = rhs._last;

            rhs._pairs       = nullptr;
            rhs._index       = nullptr;
            rhs._num_buckets = 0;
            rhs._mask        = 0;
            rhs._num_filled  = 0;
            rhs._last        = 0;
        }
        return *this;
    }

    EMH_CONSTEXPR_20 ~HashMap() noexcept {
        clearkv();
        if (_pairs) std::free(_pairs);
        if (_index) std::free(_index);
        _pairs = nullptr;
        _index = nullptr;
    }

    // -----------------------------------------------
    // Iterators
    // -----------------------------------------------
    EMH_CONSTEXPR_17 iterator begin() noexcept {
        return iterator(_pairs, _pairs + _num_filled);
    }
    EMH_CONSTEXPR_17 iterator end() noexcept {
        return iterator(_pairs + _num_filled, _pairs + _num_filled);
    }
    EMH_CONSTEXPR_17 const_iterator begin() const noexcept {
        return const_iterator(_pairs, _pairs + _num_filled);
    }
    EMH_CONSTEXPR_17 const_iterator end() const noexcept {
        return const_iterator(_pairs + _num_filled, _pairs + _num_filled);
    }
    EMH_CONSTEXPR_17 const_iterator cbegin() const noexcept {
        return const_iterator(_pairs, _pairs + _num_filled);
    }
    EMH_CONSTEXPR_17 const_iterator cend() const noexcept {
        return const_iterator(_pairs + _num_filled, _pairs + _num_filled);
    }

    // -----------------------------------------------
    // Ranges-helpers voor keys() en values()
    // -----------------------------------------------
    // In C++17 kun je hiermee ook for-rangen: for (auto& v : map.values()) ...
    // In C++20 kun je hier ook std::ranges::for_each etc. op toepassen.
    EMH_CONSTEXPR_17 auto values() noexcept {
        return ValuesRange<value_type>(_pairs, _pairs + _num_filled);
    }
    EMH_CONSTEXPR_17 auto values() const noexcept {
        return ValuesRange<const value_type>(_pairs, _pairs + _num_filled);
    }

    EMH_CONSTEXPR_17 auto keys() noexcept {
        return KeysRange<value_type>(_pairs, _pairs + _num_filled);
    }
    EMH_CONSTEXPR_17 auto keys() const noexcept {
        return KeysRange<const value_type>(_pairs, _pairs + _num_filled);
    }

    // -----------------------------------------------
    // Capacity
    // -----------------------------------------------
    EMH_CONSTEXPR_17 bool      empty()       const noexcept { return _num_filled == 0; }
    EMH_CONSTEXPR_17 size_type size()        const noexcept { return _num_filled; }
    EMH_CONSTEXPR_17 size_type bucket_count() const noexcept { return _num_buckets; }

    EMH_CONSTEXPR_17 float load_factor() const noexcept {
        return (_mask == 0) ? 0.0f : (static_cast<float>(_num_filled) / static_cast<float>(_mask + 1));
    }

    EMH_CONSTEXPR_17 float max_load_factor() const noexcept {
        if (_mlf == 0) {
            return EMH_DEFAULT_LOAD_FACTOR;
        }
        return (static_cast<float>((1ULL << 27) / _mlf));
    }

    EMH_CONSTEXPR_20 void max_load_factor(float mlf) noexcept {
        if (mlf < 0.10f)   mlf = 0.10f;
        if (mlf > 0.995f)  mlf = 0.995f;

        _mlf = static_cast<uint32_t>((1ULL << 27) / mlf);
        if (_num_buckets > 0)
            reserve(_num_filled, true);
    }

    EMH_CONSTEXPR_17 size_type max_size() const noexcept {
        return (std::numeric_limits<size_type>::max)();
    }

    EMH_CONSTEXPR_17 size_type max_bucket_count() const noexcept {
        return max_size();
    }

    // -----------------------------------------------
    // Modifiers
    // -----------------------------------------------
    EMH_FORCE_INLINE void clear() noexcept {
        clearkv();
        if (_num_buckets > 0) {
            // Markeer alle buckets als INACTIVE
            std::memset(_index, 0xFF, _num_buckets * sizeof(Index));
        }
        _num_filled = 0;
        _last       = 0;
    }

    EMH_CONSTEXPR_20 size_type erase(const KeyT& key) noexcept {
        const auto key_hash  = hash_key(key);
        const auto sbucket   = find_filled_bucket(key, key_hash);
        if (sbucket == INACTIVE)
            return 0;

        const auto main_bucket = static_cast<size_type>(key_hash & _mask);
        erase_slot(sbucket, main_bucket);
        return 1;
    }

    EMH_CONSTEXPR_20 iterator erase(const const_iterator& cit) noexcept {
        if (cit == end())
            return end();

        const auto slot = static_cast<size_type>(cit._kv - _pairs);
        if (slot >= _num_filled)
            return end(); // Extra check

        size_type main_bucket;
        const auto sbucket = find_slot_bucket(slot, main_bucket);
        if (sbucket == INACTIVE)
            return end();

        erase_slot(sbucket, main_bucket);

        if (slot >= _num_filled)
            return end();
        return iterator(_pairs + slot, _pairs + _num_filled);
    }

    EMH_CONSTEXPR_20 iterator erase(const const_iterator& first, const const_iterator& last) noexcept {
        while (first != last && first != cend()) {
            // Erase the element pointed by 'first' and update 'first'
            first = erase(first);
        }
        return iterator(static_cast<value_type*>(first._kv), static_cast<value_type*>(first._end));
    }

    template<typename Pred>
    EMH_CONSTEXPR_20 size_type erase_if(Pred pred) noexcept {
        const size_type old_size = size();
        for (auto it = begin(); it != end(); ) {
            if (pred(*it)) it = erase(it);
            else           ++it;
        }
        return old_size - size();
    }

    EMH_CONSTEXPR_17 void swap(HashMap& rhs) noexcept {
        std::swap(_hasher,      rhs._hasher);
        std::swap(_eq,          rhs._eq);
        std::swap(_pairs,       rhs._pairs);
        std::swap(_index,       rhs._index);
        std::swap(_num_buckets, rhs._num_buckets);
        std::swap(_mask,        rhs._mask);
        std::swap(_num_filled,  rhs._num_filled);
        std::swap(_mlf,         rhs._mlf);
        std::swap(_last,        rhs._last);
    }

    EMH_CONSTEXPR_20 void shrink_to_fit(float min_factor = EMH_DEFAULT_LOAD_FACTOR / 4) noexcept {
        if (load_factor() < min_factor && bucket_count() > 10) {
            rehash(_num_filled + 1);
        }
    }

    // -----------------------------------------------
    // Lookup / Access
    // -----------------------------------------------
    template<typename K = KeyT>
    EMH_CONSTEXPR_17 bool contains(const K& key) const noexcept {
        return find_filled_slot(key) != _num_filled;
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_17 size_type count(const K& key) const noexcept {
        return (find_filled_slot(key) == _num_filled) ? 0 : 1;
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_20 iterator find(const K& key) noexcept {
        const auto slot = find_filled_slot(key);
        return (slot != _num_filled) ? iterator(_pairs + slot, _pairs + _num_filled) : end();
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_17 const_iterator find(const K& key) const noexcept {
        const auto slot = find_filled_slot(key);
        return (slot != _num_filled) ? const_iterator(_pairs + slot, _pairs + _num_filled) : cend();
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_20 ValueT& at(const K& key) {
        const auto slot = find_filled_slot(key);
        // Geen out_of_range check hier, kan eventueel toegevoegd worden
        return _pairs[slot].second;
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_17 const ValueT& at(const K& key) const {
        const auto slot = find_filled_slot(key);
        return _pairs[slot].second;
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_20 ValueT* try_get(const K& key) noexcept {
        const auto slot = find_filled_slot(key);
        return (slot == _num_filled) ? nullptr : &_pairs[slot].second;
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_17 const ValueT* try_get(const K& key) const noexcept {
        const auto slot = find_filled_slot(key);
        return (slot == _num_filled) ? nullptr : &_pairs[slot].second;
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_20 bool try_get(const K& key, ValueT& outval) const noexcept {
        const auto slot = find_filled_slot(key);
        if (slot == _num_filled)
            return false;
        outval = _pairs[slot].second;
        return true;
    }

    template<typename K = KeyT>
    EMH_CONSTEXPR_20 ValueT get_or_return_default(const K& key) const noexcept {
        const auto slot = find_filled_slot(key);
        return (slot == _num_filled) ? ValueT() : _pairs[slot].second;
    }

    // -----------------------------------------------
    // Insert / Emplace
    // -----------------------------------------------
    std::pair<iterator, bool> insert(const value_type& value) {
        check_expand_need();
        return do_insert(value);
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        check_expand_need();
        return do_insert(std::move(value));
    }

    template<typename Iter>
    void insert(Iter first, Iter last) {
        reserve(static_cast<size_type>(_num_filled + std::distance(first, last)), false);
        for (; first != last; ++first) {
            do_insert(*first);
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        reserve(static_cast<size_type>(_num_filled + ilist.size()), false);
        for (auto&& val : ilist) {
            do_insert(val);
        }
    }

    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        check_expand_need();
        return do_insert(std::forward<Args>(args)...);
    }

    template<class... Args>
    iterator emplace_hint(const_iterator /*hint*/, Args&&... args) {
        check_expand_need();
        return do_insert(std::forward<Args>(args)...).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) {
        check_expand_need();
        return do_insert(key, std::forward<Args>(args)...);
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) {
        check_expand_need();
        return do_insert(std::move(key), std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& val) {
        return do_assign(key, std::forward<ValueT>(val));
    }

    std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& val) {
        return do_assign(std::move(key), std::forward<ValueT>(val));
    }

    ValueT set_get(const KeyT& key, const ValueT& val) {
        check_expand_need();
        const auto key_hash = hash_key(key);
        const auto bucket   = find_or_allocate(key, key_hash);
        if (is_empty(bucket)) {
            EMH_NEW(key, val, bucket, key_hash);
            return ValueT();
        } else {
            const auto slot = _index[bucket].slot & _mask;
            ValueT old = _pairs[slot].second;
            _pairs[slot].second = val;
            return old;
        }
    }

    // Operator[]
    ValueT& operator[](const KeyT& key) {
        check_expand_need();
        const auto key_hash = hash_key(key);
        const auto bucket   = find_or_allocate(key, key_hash);
        if (is_empty(bucket)) {
            EMH_NEW(key, ValueT(), bucket, key_hash);
        }
        const auto slot = _index[bucket].slot & _mask;
        return _pairs[slot].second;
    }

    ValueT& operator[](KeyT&& key) {
        check_expand_need();
        const auto key_hash = hash_key(key);
        const auto bucket   = find_or_allocate(key, key_hash);
        if (is_empty(bucket)) {
            EMH_NEW(std::move(key), ValueT(), bucket, key_hash);
        }
        const auto slot = _index[bucket].slot & _mask;
        return _pairs[slot].second;
    }

    // Merge
    void merge(HashMap& other) {
        if (other.empty())
            return;
        if (empty()) {
            swap(other);
            return;
        }
        for (auto it = other.begin(); it != other.end(); ) {
            auto found = find(it->first);
            if (found == end()) {
                insert_unique(std::move(it->first), std::move(it->second));
                it = other.erase(it);
            } else {
                ++it;
            }
        }
    }

    // -----------------------------------------------
    // Observers
    // -----------------------------------------------
    hasher&       hash_function()      noexcept { return _hasher; }
    const hasher& hash_function() const noexcept { return _hasher; }

    key_equal&       key_eq()      noexcept { return _eq; }
    const key_equal& key_eq() const noexcept { return _eq; }

    const value_type* values_data() const noexcept { return _pairs; }
    const Index*      index_data()  const noexcept { return _index; }

    // -----------------------------------------------
    // Rehashing / Reserve
    // -----------------------------------------------
    bool reserve(size_type num_elems, bool force) {
        const auto required_buckets = static_cast<size_type>((static_cast<uint64_t>(num_elems) * _mlf) >> 27);
        if (!force && required_buckets < _mask)
            return false;
        rehash(required_buckets + 2);
        return true;
    }

    void rehash(size_type required_buckets) {
        if (required_buckets < _num_filled)
            required_buckets = (_num_filled > 0) ? _num_filled : 2;

        size_type new_size = 1;
        while (new_size < required_buckets) {
            new_size <<= 1;
            if (new_size > max_size()) {
                new_size = max_size();
                break;
            }
        }
        if (new_size == _num_buckets)
            return;

        rebuild(new_size);

        _last = 0;
        for (size_type slot = 0; slot < _num_filled; ++slot) {
            const auto& key      = _pairs[slot].first;
            const auto key_hash  = hash_key(key);
            const auto bucket    = find_unique_bucket(key_hash);

            _index[bucket].slot = (slot | (static_cast<size_type>(key_hash) & ~_mask));
            _index[bucket].next = bucket;
        }
    }

private:
    // -----------------------------------------------
    // Implementation
    // -----------------------------------------------

    // Controleer of een bucket leeg is
    EMH_FORCE_INLINE bool is_empty(size_type bucket) const noexcept {
        return (_index[bucket].next == INACTIVE);
    }

    // Macro voor het invoegen van een nieuwe sleutel-waarde paar
#undef EMH_NEW
#define EMH_NEW(key, val, bucket, key_hash)                                   \
    do {                                                                      \
        new(_pairs + _num_filled) value_type((key), (val));                   \
        _index[bucket].slot = (_num_filled | ((size_type)(key_hash) & ~_mask)); \
        _index[bucket].next = bucket;                                         \
        _num_filled++;                                                         \
    } while(0)

    // Initialisatie van de hash map
    EMH_CONSTEXPR_20 void init(size_type bucket_count, float mlf) noexcept {
        _index        = nullptr;
        _pairs        = nullptr;
        _num_buckets  = 0;
        _num_filled   = 0;
        _mask         = 0;
        _last         = 0;

        // Standaard inverse load factor
        _mlf          = static_cast<uint32_t>((1ULL << 27) / mlf);
        max_load_factor(mlf);

        if (bucket_count)
            rehash(bucket_count);
    }

    // Herbouw de interne structuren bij herhashing
    EMH_CONSTEXPR_20 void rebuild(size_type new_numbuckets) noexcept {
        if (_index) std::free(_index);

        const auto new_array_size = static_cast<size_type>(static_cast<double>(new_numbuckets) * max_load_factor()) + 4;
        value_type* new_pairs = static_cast<value_type*>(std::malloc(new_array_size * sizeof(value_type)));
        assert(new_pairs != nullptr && "Memory allocation failed for _pairs");

        // Move/kopieer bestaande items
        if (_pairs) {
            if (is_copy_trivially()) {
                std::memcpy(new_pairs, _pairs, _num_filled * sizeof(value_type));
            } else {
                for (size_type i = 0; i < _num_filled; i++) {
                    new (&new_pairs[i]) value_type(std::move(_pairs[i]));
                }
            }
            std::free(_pairs);
        }
        _pairs = new_pairs;

        _index = static_cast<Index*>(std::malloc((new_numbuckets + EAD) * sizeof(Index)));
        assert(_index != nullptr && "Memory allocation failed for _index");
        std::memset(_index, 0xFF, new_numbuckets * sizeof(Index));
        std::memset(_index + new_numbuckets, 0, EAD * sizeof(Index));

        _num_buckets = new_numbuckets;
        _mask        = new_numbuckets - 1;
    }

    // Schoon de key-value paren op
    EMH_CONSTEXPR_20 void clearkv() noexcept {
        if (!is_triviall_destructable()) {
            while (_num_filled) {
                --_num_filled;
                _pairs[_num_filled].~value_type();
            }
        } else {
            _num_filled = 0;
        }
    }

    // Zoek of alloceer een bucket voor een gegeven sleutel
    EMH_CONSTEXPR_20 size_type find_or_allocate(const KeyT& key, uint64_t key_hash) noexcept {
        const auto bucket = static_cast<size_type>(key_hash & _mask);
        auto next_bucket  = _index[bucket].next;
        if (next_bucket == INACTIVE) {
            return bucket;
        }

        // Occupant
        const auto occupant_slot = _index[bucket].slot & _mask;
        const auto occupant_mbk  = hash_bucket(_pairs[occupant_slot].first);
        if (EMH_UNLIKELY(occupant_mbk != bucket)) {
            return kickout_bucket(occupant_mbk, bucket);
        }

        // Occupant zit correct
        if (next_bucket == bucket) {
            // Nog 1 occupant -> append
            const auto emptyb = find_empty_bucket(bucket, 1);
            return _index[bucket].next = emptyb;
        } else {
            // Chain
            while (true) {
                const auto eslot = _index[next_bucket].slot & _mask;
                if (EMH_LIKELY(eqhash(next_bucket, key_hash)) && _eq(key, _pairs[eslot].first)) {
                    return next_bucket; 
                }
                const auto nbucket = _index[next_bucket].next;
                if (nbucket == next_bucket) {
                    const auto emptyb = find_empty_bucket(nbucket, 2);
                    return _index[next_bucket].next = emptyb;
                }
                next_bucket = nbucket;
            }
        }
    }

    // Zoek de laatste bucket in een chain
    EMH_CONSTEXPR_20 size_type find_last_bucket(size_type main_bucket) const noexcept {
        auto nbucket = _index[main_bucket].next;
        if (nbucket == main_bucket)
            return main_bucket;
        while (true) {
            auto nn = _index[nbucket].next;
            if (nn == nbucket)
                return nbucket;
            nbucket = nn;
        }
    }

    // Zoek een unieke bucket voor insert_unique
    EMH_CONSTEXPR_20 size_type find_unique_bucket(uint64_t key_hash) noexcept {
        const auto bucket = static_cast<size_type>(key_hash & _mask);
        auto next_bucket  = _index[bucket].next;
        if (next_bucket == INACTIVE)
            return bucket;

        // Occupant
        const auto occupant_slot = _index[bucket].slot & _mask;
        const auto occupant_mbk  = hash_bucket(_pairs[occupant_slot].first);
        if (EMH_UNLIKELY(occupant_mbk != bucket)) {
            return kickout_bucket(occupant_mbk, bucket);
        }

        // Occupant zit goed
        if (EMH_UNLIKELY(next_bucket != bucket)) {
            next_bucket = find_last_bucket(next_bucket);
        }
        return _index[next_bucket].next = find_empty_bucket(next_bucket, 2);
    }

    // Kickout een bucket en zoek een lege plek
    EMH_CONSTEXPR_20 size_type kickout_bucket(size_type occupant_main, size_type bucket) noexcept {
        const auto next_bucket = _index[bucket].next;
        const auto new_bucket  = find_empty_bucket(next_bucket, 2);
        const auto prev_bucket = find_prev_bucket(occupant_main, bucket);

        const auto last  = (next_bucket == bucket) ? new_bucket : next_bucket;
        _index[new_bucket] = { last, _index[bucket].slot };

        _index[prev_bucket].next = new_bucket;
        _index[bucket].next = INACTIVE;

        return bucket;
    }

    // Zoek de vorige bucket in een chain
    EMH_CONSTEXPR_20 size_type find_prev_bucket(size_type main_bucket, size_type bucket) const noexcept {
        auto nbucket = _index[main_bucket].next;
        if (nbucket == bucket)
            return main_bucket;
        while (true) {
            auto nb2 = _index[nbucket].next;
            if (nb2 == bucket)
                return nbucket;
            nbucket = nb2;
        }
    }

    // Zoek een lege bucket met probing
    EMH_CONSTEXPR_20 size_type find_empty_bucket(size_type from, uint32_t probe_size) noexcept {
#if EMH_QUADRATIC
        // Kwadratische probing
        constexpr size_type limit = (2 * EMH_CACHE_LINE_SIZE) / sizeof(Index);
        for (size_type offset = 1, inc = 2; offset <= limit; ) {
            size_type nbucket = (from + offset) & _mask;
            if (is_empty(nbucket))
                return nbucket;
            offset += inc;
        }
#else
        // Lineaire probing
        constexpr size_type limit = 6;
        for (size_type offset = 1; offset <= limit; ++offset) {
            const size_type nbucket = (from + offset) & _mask;
            if (is_empty(nbucket))
                return nbucket;
        }
#endif
        while (true) {
            _last &= _mask;
            if (is_empty(_last))
                return _last++;
            _last++;
        }
    }

    // Verwijder een slot uit de hash map
    EMH_CONSTEXPR_20 void erase_slot(size_type sbucket, size_type main_bucket) noexcept {
        const auto slot = _index[sbucket].slot & _mask;
        const auto ebuck = erase_bucket(sbucket, main_bucket);

        const auto last_slot = --_num_filled;
        if (slot != last_slot) {
            _pairs[slot] = std::move(_pairs[last_slot]);
            const auto last_bucket = slot_to_bucket(last_slot);
            _index[last_bucket].slot = (slot | (_index[last_bucket].slot & ~_mask));
        }

        if (ebuck != INACTIVE) {
            _index[ebuck].next = INACTIVE;
            _index[ebuck].slot = 0;
        }

        if (!is_triviall_destructable()) {
            _pairs[last_slot].~value_type();
        }
    }

    // Verwijder een specifieke bucket
    EMH_CONSTEXPR_20 size_type erase_bucket(size_type bucket, size_type main_bucket) noexcept {
        const auto next_bucket = _index[bucket].next;
        if (bucket == main_bucket) {
            if (main_bucket != next_bucket) {
                const auto nn = _index[next_bucket].next;
                _index[main_bucket].next = (nn == next_bucket) ? main_bucket : nn;
                _index[main_bucket].slot = _index[next_bucket].slot;
            }
            return next_bucket;
        } else {
            const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
            _index[prev_bucket].next = (bucket == next_bucket) ? prev_bucket : next_bucket;
            return bucket;
        }
    }

    // Convert slot naar bucket
    EMH_CONSTEXPR_20 size_type slot_to_bucket(size_type slot) const noexcept {
        if (slot >= _num_filled)
            return INACTIVE;
        const auto key_hash = hash_key(_pairs[slot].first);
        return find_slot_bucket(slot, static_cast<size_type>(key_hash & _mask));
    }

    // Zoek de bucket voor een specifieke slot
    EMH_CONSTEXPR_20 size_type find_slot_bucket(size_type slot, size_type main_bucket) const noexcept {
        auto nbucket = _index[main_bucket].next;
        if (nbucket == INACTIVE)
            return INACTIVE;

        if ((_index[main_bucket].slot & _mask) == slot)
            return main_bucket;

        while (true) {
            if ((_index[nbucket].slot & _mask) == slot)
                return nbucket;
            auto nb2 = _index[nbucket].next;
            if (nb2 == nbucket)
                break;
            nbucket = nb2;
        }
        return INACTIVE;
    }

    // Zoek de bucket met een bepaalde key hash
    EMH_CONSTEXPR_20 size_type find_filled_bucket(const KeyT& key, uint64_t key_hash) const noexcept {
        const auto bucket = static_cast<size_type>(key_hash & _mask);
        auto next_bucket  = _index[bucket].next;
        if (next_bucket == INACTIVE)
            return INACTIVE;

        const auto slot = _index[bucket].slot & _mask;
        if (eqhash(bucket, key_hash)) {
            if (_eq(key, _pairs[slot].first))
                return bucket;
        }
        if (next_bucket == bucket)
            return INACTIVE;

        while (true) {
            if (eqhash(next_bucket, key_hash)) {
                const auto eslot = _index[next_bucket].slot & _mask;
                if (_eq(key, _pairs[eslot].first))
                    return next_bucket;
            }
            auto nb = _index[next_bucket].next;
            if (nb == next_bucket)
                return INACTIVE;
            next_bucket = nb;
        }
    }

    // Zoek een gevulde slot voor een gegeven key
    template<typename K = KeyT>
    EMH_CONSTEXPR_20 size_type find_filled_slot(const K& key) const noexcept {
        if (_num_filled == 0)
            return _num_filled;

        const auto key_hash = hash_key(key);
        const auto bucket   = static_cast<size_type>(key_hash & _mask);
        auto next_bucket    = _index[bucket].next;
        if (next_bucket == INACTIVE)
            return _num_filled;

        const auto slot = _index[bucket].slot & _mask;
        if (eqhash(bucket, key_hash)) {
            if (_eq(key, _pairs[slot].first))
                return slot;
        }
        if (next_bucket == bucket)
            return _num_filled;

        while (true) {
            if (eqhash(next_bucket, key_hash)) {
                const auto eslot = _index[next_bucket].slot & _mask;
                if (_eq(key, _pairs[eslot].first))
                    return eslot;
            }
            auto nb = _index[next_bucket].next;
            if (nb == next_bucket)
                return _num_filled;
            next_bucket = nb;
        }
    }

    // -----------------------------------------------
    // do_insert helpers
    // -----------------------------------------------
    std::pair<iterator, bool> do_insert(const value_type& val) noexcept {
        const auto key_hash = hash_key(val.first);
        const auto bucket   = find_or_allocate(val.first, key_hash);
        const bool is_new   = is_empty(bucket);
        if (is_new) {
            EMH_NEW(val.first, val.second, bucket, key_hash);
        }
        const auto slot = _index[bucket].slot & _mask;
        return { iterator(_pairs + slot, _pairs + _num_filled), is_new };
    }

    std::pair<iterator, bool> do_insert(value_type&& val) noexcept {
        const auto key_hash = hash_key(val.first);
        const auto bucket   = find_or_allocate(val.first, key_hash);
        const bool is_new   = is_empty(bucket);
        if (is_new) {
            EMH_NEW(std::move(val.first), std::move(val.second), bucket, key_hash);
        }
        const auto slot = _index[bucket].slot & _mask;
        return { iterator(_pairs + slot, _pairs + _num_filled), is_new };
    }

    template<typename K, typename V>
    std::pair<iterator, bool> do_insert(K&& key, V&& val) noexcept {
        const auto key_hash = hash_key(key);
        const auto bucket   = find_or_allocate(key, key_hash);
        const bool is_new   = is_empty(bucket);
        if (is_new) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket, key_hash);
        }
        const auto slot = _index[bucket].slot & _mask;
        return { iterator(_pairs + slot, _pairs + _num_filled), is_new };
    }

    template<typename K, typename V>
    std::pair<iterator, bool> do_assign(K&& key, V&& val) noexcept {
        check_expand_need();
        const auto key_hash = hash_key(key);
        const auto bucket   = find_or_allocate(key, key_hash);
        const bool is_new   = is_empty(bucket);
        if (is_new) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket, key_hash);
        } else {
            auto slot = _index[bucket].slot & _mask;
            _pairs[slot].second = std::forward<V>(val);
        }
        const auto slot = _index[bucket].slot & _mask;
        return { iterator(_pairs + slot, _pairs + _num_filled), is_new };
    }

    template<typename K, typename V>
    size_type insert_unique(K&& key, V&& val) noexcept {
        check_expand_need();
        const auto key_hash = hash_key(key);
        const auto bucket   = find_unique_bucket(key_hash);
        EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket, key_hash);
        return bucket;
    }

    EMH_CONSTEXPR_20 bool check_expand_need() {
        const auto required_buckets = static_cast<size_type>((static_cast<uint64_t>(_num_filled + 1) * _mlf) >> 27);
        if (EMH_UNLIKELY(required_buckets >= _mask)) {
            rehash(required_buckets + 2);
            return true;
        }
        return false;
    }

    // Vergelijk hash waarden
    EMH_CONSTEXPR_20 bool eqhash(size_type bucket, uint64_t key_hash) const noexcept {
        const uint64_t stored = (_index[bucket].slot & ~_mask);
        return (stored == (key_hash & ~_mask));
    }

#if EMH_INT_HASH
    // Integer hashing optimalisatie
    static EMH_FORCE_INLINE uint64_t hash64(uint64_t x) noexcept {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return x;
    }
#endif

#if EMH_WYHASH_HASH
    // WyHash-implementatie voor string hashing
    // TODO: Eventuele WyHash-string implementatie
#endif

    // Hashing
    template<typename UType,
             typename std::enable_if<std::is_integral<UType>::value, int>::type = 0>
    EMH_FORCE_INLINE uint64_t hash_key(UType x) const noexcept {
#if EMH_INT_HASH
        return hash64(static_cast<uint64_t>(x));
#else
        return static_cast<uint64_t>(_hasher(x));
#endif
    }

    template<typename UType,
             typename std::enable_if<std::is_same<UType, std::string>::value, int>::type = 0>
    EMH_FORCE_INLINE uint64_t hash_key(const UType& s) const noexcept {
#if EMH_WYHASH_HASH
        // TODO: Implementeer WyHash hier
        return static_cast<uint64_t>(_hasher(s));
#else
        return static_cast<uint64_t>(_hasher(s));
#endif
    }

    template<typename UType,
             typename std::enable_if<!std::is_integral<UType>::value &&
                                     !std::is_same<UType, std::string>::value, int>::type = 0>
    EMH_FORCE_INLINE uint64_t hash_key(const UType& key) const noexcept {
        return static_cast<uint64_t>(_hasher(key));
    }

    EMH_FORCE_INLINE size_type hash_bucket(const KeyT& k) const noexcept {
        return static_cast<size_type>(hash_key(k) & _mask);
    }

    // Controleer of types triviaal destructible zijn
    static constexpr bool is_triviall_destructable() noexcept {
#if __cplusplus >= 201402L
        return (std::is_trivially_destructible<KeyT>::value &&
                std::is_trivially_destructible<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    // Controleer of types triviaal copyable zijn
    static constexpr bool is_copy_trivially() noexcept {
#if __cplusplus >= 201402L
        return (std::is_trivially_copyable<KeyT>::value &&
                std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

private:
    Index*       _index;
    value_type*  _pairs;
    hasher       _hasher;
    key_equal    _eq;
    uint32_t     _mlf;         // Inverse load factor in vaste bits
    size_type    _num_buckets;
    size_type    _mask;
    size_type    _num_filled;
    size_type    _last;
};
} // namespace emhash8
