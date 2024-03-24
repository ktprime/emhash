#pragma once

// Fast hashtable (hash_set, hash_map) based on open addressing hashing for C++11 and up
// version 1.3.1
// https://github.com/hordi/hash
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022 Yurii Hordiienko
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
// SOFTWARE.

#include <functional>
#include <stdexcept>
#include <cstdint>
#include <string.h> //memcpy

#ifdef _MSC_VER
#  include <pmmintrin.h>
#  define HRD_ALWAYS_INLINE __forceinline
#  define HRD_LIKELY(condition) condition
#  define HRD_UNLIKELY(condition) condition
#  define HRD_ATTR_NOINLINE __declspec(noinline)
#  define HRD_ATTR_NORETURN __declspec(noreturn)
#else
#  include <x86intrin.h>
#  define HRD_ALWAYS_INLINE __attribute__((always_inline)) inline
#  define HRD_LIKELY(condition) __builtin_expect(condition, 1)
#  define HRD_UNLIKELY(condition) __builtin_expect(condition, 0)
#  define HRD_ATTR_NOINLINE __attribute__((noinline))
#  define HRD_ATTR_NORETURN __attribute__((noreturn))
#endif

namespace hrd6 {

class hash_utils {
public:
    typedef size_t size_type;

    template<typename T>
    struct hash_ : public std::hash<T> {
        HRD_ALWAYS_INLINE size_t operator()(const T& val) const noexcept {
            return hash_1<sizeof(T)>(&val);
        }
    };

    template<size_t SIZE>
    static uint32_t hash(const void* ptr) noexcept {
        return hash_1<SIZE>(ptr);
    }

    template <class Key, class Hasher, class KeyEql>
    class
#if defined(_MSC_VER) && _MSC_VER >= 1915
        __declspec (empty_bases)
#endif
    hash_eql: private Hasher, private KeyEql
    {
    public:
        hash_eql() {}
        hash_eql(const Hasher& h, const KeyEql& eql) : Hasher(h), KeyEql(eql) {}
        hash_eql(const hash_eql& r) : Hasher(r), KeyEql(r) {}
        hash_eql(hash_eql&& r) noexcept : Hasher(std::move(r)), KeyEql(std::move(r)) {}

        hash_eql& operator=(const hash_eql& r) {
            hash_eql(r).swap(*this);
            return *this;
        }

        hash_eql& operator=(hash_eql&& r) noexcept {
            const_cast<Hasher&>(hasher()) = std::move(r);
            const_cast<KeyEql&>(keyeql()) = std::move(r);
            return *this;
        }

        HRD_ALWAYS_INLINE size_t operator()(const Key& k) const { return static_cast<size_t>(hasher()(k)); }
        HRD_ALWAYS_INLINE bool operator()(const Key& k1, const Key& k2) const { return keyeql()(k1, k2); }

        void swap(hash_eql& r) noexcept {
            std::swap(const_cast<Hasher&>(hasher()), const_cast<Hasher&>(r.hasher()));
            std::swap(const_cast<KeyEql&>(keyeql()), const_cast<KeyEql&>(r.keyeql()));
        }

    private:
        HRD_ALWAYS_INLINE const Hasher& hasher() const noexcept { return *this; }
        HRD_ALWAYS_INLINE const KeyEql& keyeql() const noexcept { return *this; }
    };

protected:
    enum { DELETED_MARK = 0x1 };

    constexpr static const uint32_t OFFSET_BASIS = 2166136261;

    HRD_ALWAYS_INLINE static size_t roundup(size_t sz) noexcept
    {
#ifdef _MSC_VER
        unsigned long idx;
#  if defined(_WIN64) || defined(__LP64__)
        _BitScanReverse64(&idx, sz - 1);
#  else
        _BitScanReverse(&idx, sz - 1);
#  endif
#else
        size_t idx;
#  ifdef __LP64__
        __asm__("bsrq %0, %0" : "=r" (idx) : "0" (sz - 1));
#  else
        __asm__("bsr %0, %0" : "=r" (idx) : "0" (sz - 1));
#  endif
#endif
        return size_t(1) << (idx + 1);
    }

    HRD_ATTR_NOINLINE HRD_ATTR_NORETURN static void throw_bad_alloc() {
        throw std::bad_alloc();
    }

    HRD_ATTR_NOINLINE HRD_ATTR_NORETURN static void throw_length_error() {
        throw std::length_error("size exceeded");
    }

    //if any exception happens during any new-in-place call ::dtor
    template<typename this_type>
    class dtor_if_throw_constructible {
    public:
        inline dtor_if_throw_constructible(this_type& ref) noexcept { set(&ref, typename this_type::IS_NOTHROW_CONSTRUCTIBLE()); }
        inline ~dtor_if_throw_constructible() noexcept { clear(typename this_type::IS_NOTHROW_CONSTRUCTIBLE()); }

        inline void reset() noexcept { set(nullptr, typename this_type::IS_NOTHROW_CONSTRUCTIBLE()); }
    private:
        inline void set(this_type*, std::true_type) noexcept {}
        inline void set(this_type* ptr, std::false_type) noexcept { _this = ptr; }
        inline void clear(std::true_type) noexcept {}
        inline void clear(std::false_type) noexcept { if (_this) dtor(); }
        HRD_ATTR_NOINLINE void dtor() noexcept { _this->dtor(typename this_type::IS_TRIVIALLY_DESTRUCTIBLE()); }
        this_type* _this;
    };

    template<size_t SIZE>
    static uint32_t hash_1(const void* ptr) noexcept {
        return fnv_1a((const char*)ptr, SIZE);
    }

    constexpr static HRD_ALWAYS_INLINE uint32_t fnv_1a(const char* key, size_t len, uint32_t hash32 = OFFSET_BASIS) noexcept
    {
        constexpr const uint32_t PRIME = 1607;

        for (size_t cnt = len / sizeof(uint32_t); cnt--; key += sizeof(uint32_t))
            hash32 = (hash32 ^ (*(uint32_t*)key)) * PRIME;

        if (len & sizeof(uint16_t)) {
            hash32 = (hash32 ^ (*(uint16_t*)key)) * PRIME;
            key += sizeof(uint16_t);
        }
        if (len & 1)
            hash32 = (hash32 ^ (*key)) * PRIME;

        return hash32 ^ (hash32 >> 16);
    }

    HRD_ALWAYS_INLINE static uint32_t make_mark(size_t h) noexcept {
        auto n = static_cast<uint32_t>(h);
        return n > DELETED_MARK ? n : (DELETED_MARK + 1);
    }

#ifdef _MSC_VER
    HRD_ALWAYS_INLINE static uint64_t umul128(uint64_t a, uint64_t b) noexcept {
        uint64_t h, l = _umul128(a, b, &h);
        return l + h;
    }
#else
    HRD_ALWAYS_INLINE static uint64_t umul128(uint64_t a, uint64_t b) noexcept {
        typedef unsigned __int128 uint128_t;

        auto result = static_cast<uint128_t>(a)* static_cast<uint128_t>(b);
        return static_cast<uint64_t>(result) + static_cast<uint64_t>(result >> 64U);
    }
#endif

    size_type _size;
    size_type _capacity;
    void* _elements;
    size_type _erased;
};

template<>
HRD_ALWAYS_INLINE uint32_t hash_utils::hash_1<1>(const void* ptr) noexcept {
    uint32_t hash32 = (OFFSET_BASIS ^ (*(uint8_t*)ptr)) * 1607;
    return hash32 ^ (hash32 >> 16);
}

template<>
HRD_ALWAYS_INLINE uint32_t hash_utils::hash_1<2>(const void* ptr) noexcept {
    uint32_t hash32 = (OFFSET_BASIS ^ (*(uint16_t*)ptr)) * 1607;
    return hash32 ^ (hash32 >> 16);
}

template<>
HRD_ALWAYS_INLINE uint32_t hash_utils::hash_1<4>(const void* ptr) noexcept {
    return static_cast<uint32_t>(umul128(*(uint32_t*)ptr, 0xde5fb9d2630458e9ull));
    /*
        uint32_t hash32 = (OFFSET_BASIS ^ (*(uint32_t*)ptr)) * 1607;
        return hash32 ^ (hash32 >> 16);
    */
}

template<>
HRD_ALWAYS_INLINE uint32_t hash_utils::hash_1<8>(const void* ptr) noexcept {
    return static_cast<uint32_t>(umul128(*(uint64_t*)ptr, 0xde5fb9d2630458e9ull));
    /*
        uint32_t* key = (uint32_t*)ptr;
        uint32_t hash32 = (((OFFSET_BASIS ^ key[0]) * 1607) ^ key[1]) * 1607;
        return hash32 ^ (hash32 >> 16);
    */
}

template<>
HRD_ALWAYS_INLINE uint32_t hash_utils::hash_1<12>(const void* ptr) noexcept
{
    const uint32_t* key = reinterpret_cast<const uint32_t*>(ptr);

    const uint32_t PRIME = 1607;

    uint32_t hash32 = (OFFSET_BASIS ^ key[0]) * PRIME;
    hash32 = (hash32 ^ key[1]) * PRIME;
    hash32 = (hash32 ^ key[2]) * PRIME;

    return hash32 ^ (hash32 >> 16);
}

template<>
HRD_ALWAYS_INLINE uint32_t hash_utils::hash_1<16>(const void* ptr) noexcept
{
    const uint32_t* key = reinterpret_cast<const uint32_t*>(ptr);

    const uint32_t PRIME = 1607;

    uint32_t hash32 = (OFFSET_BASIS ^ key[0]) * PRIME;
    hash32 = (hash32 ^ key[1]) * PRIME;
    hash32 = (hash32 ^ key[2]) * PRIME;
    hash32 = (hash32 ^ key[3]) * PRIME;

    return hash32 ^ (hash32 >> 16);
}

template<>
struct hash_utils::hash_<std::string> {
    HRD_ALWAYS_INLINE size_t operator()(const std::string& val) const noexcept {
        return fnv_1a(val.c_str(), val.size());
    }
};

template<class Key, class T, class value, class Hash = hash_utils::hash_<Key>, class Pred = std::equal_to<Key>>
class hash_base : public hash_utils, protected hash_utils::hash_eql<Key, Hash, Pred>
{
public:
    typedef hash_base<Key, T, value, Hash, Pred>    this_type;
    typedef Key                                     key_type;
    typedef Hash                                    hasher;
    typedef Pred                                    key_equal;
    typedef value&                                  reference;
    typedef const value&                            const_reference;
    typedef value                                   value_type;

protected:
    typedef hash_eql<Key, Hash, Pred>   hash_pred;
    friend dtor_if_throw_constructible<this_type>;

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    typedef std::integral_constant<bool, std::is_trivially_copyable<key_type>::value && std::is_trivially_copyable<T>::value> IS_TRIVIALLY_COPYABLE;
    typedef std::integral_constant<bool, std::is_trivially_destructible<key_type>::value && std::is_trivially_destructible<T>::value> IS_TRIVIALLY_DESTRUCTIBLE;
    typedef std::integral_constant<bool, std::is_nothrow_constructible<key_type>::value && std::is_nothrow_constructible<T>::value> IS_NOTHROW_CONSTRUCTIBLE;
#else
    typedef std::integral_constant<bool, std::is_pod<key_type>::value && std::is_pod<mapped_type>::value> IS_TRIVIALLY_COPYABLE;
    typedef std::integral_constant<bool, std::is_pod<key_type>::value && std::is_pod<mapped_type>::value> IS_TRIVIALLY_DESTRUCTIBLE;
    typedef std::integral_constant<bool, std::is_pod<key_type>::value && std::is_pod<mapped_type>::value> IS_NOTHROW_CONSTRUCTIBLE;
#endif

#pragma pack(push, 4)
    struct storage_type
    {
        storage_type(storage_type&& r) : data(std::move(r.data)) { mark = r.mark; /*set mark after data-ctor call, if any exception happens nothing break old mark value - new-in-place*/ }
        storage_type(const storage_type& r) : data(r.data) { mark = r.mark; }

        uint32_t mark;
        value_type data;
    };
#pragma pack(pop)

    template<typename base>
    struct iterator_base
    {
        class const_iterator
        {
        public:
            typedef std::forward_iterator_tag iterator_category;
            typedef typename base::value_type value_type;
            typedef typename base::value_type* pointer;
            typedef typename base::value_type& reference;
            typedef std::ptrdiff_t difference_type;

            const_iterator() noexcept : _ptr(nullptr), _cnt(0) {}

            HRD_ALWAYS_INLINE const_iterator& operator++() noexcept
            {
                if (HRD_LIKELY(_cnt)) {
                    --_cnt;
                    while (HRD_LIKELY((++_ptr)->mark <= base::DELETED_MARK))
                        ;
                }
                else
                    _ptr = nullptr;
                return *this;
            }

            HRD_ALWAYS_INLINE const_iterator operator++(int) noexcept
            {
                const_iterator ret(*this);
                ++(*this);
                return ret;
            }

            bool operator== (const const_iterator& r) const noexcept { return _ptr == r._ptr; }
            bool operator!= (const const_iterator& r) const noexcept { return _ptr != r._ptr; }

            const typename base::value_type& operator*() const noexcept { return _ptr->data; }
            const typename base::value_type* operator->() const noexcept { return &_ptr->data; }

        protected:
            friend base;
            friend hash_base;
            const_iterator(typename base::storage_type* p, typename base::size_type cnt) noexcept : _ptr(p), _cnt(cnt) {}

            typename base::storage_type* _ptr;
            typename base::size_type _cnt;
        };

        class iterator : public const_iterator
        {
        public:
            using typename const_iterator::iterator_category;
            using typename const_iterator::value_type;
            using typename const_iterator::pointer;
            using typename const_iterator::reference;
            using typename const_iterator::difference_type;
            using const_iterator::operator*;
            using const_iterator::operator->;

            iterator() noexcept {}

            typename base::value_type& operator*() noexcept { return const_iterator::_ptr->data; }
            typename base::value_type* operator->() noexcept { return &const_iterator::_ptr->data; }

        private:
            friend base;
            friend hash_base;
            iterator(typename base::storage_type* p, typename base::size_type cnt = 0) : const_iterator(p, cnt) {}
        };
    };

    typedef typename iterator_base<this_type>::iterator iterator;
    typedef typename iterator_base<this_type>::const_iterator const_iterator;

    hash_base() noexcept {
        ctor_empty();
    }

    hash_base(const hash_base& r) :
        hash_pred(r)
    {
        ctor_copy(r, IS_TRIVIALLY_COPYABLE());
    }

    hash_base(hash_base&& r) noexcept :
        hash_pred(std::move(r))
    {
        ctor_move(std::move(r));
    }

    hash_base(const hasher& hf, const key_equal& eql) :
        hash_pred(hf, eql)
    {}

    hash_base(size_type hint_size, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        hash_pred(hf, eql)
    {
        // |1 to prevent 0-usage (produces _capacity = 0 finally)
        ctor_pow2(roundup((hint_size | 1) * 2));
    }

    template<typename Iter>
    hash_base(Iter first, Iter last, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        hash_pred(hf, eql)
    {
        ctor_iters(first, last, typename std::iterator_traits<Iter>::iterator_category());
    }

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    hash_base(std::initializer_list<value_type> lst, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        hash_pred(hf, eql)
    {
        ctor_init_list(lst);
    }
#endif

    hash_base(size_type pow2, bool) {
        ctor_pow2(pow2);
    }

    ~hash_base() noexcept {
        dtor(IS_TRIVIALLY_DESTRUCTIBLE());
    }

public:
    size_type size() const noexcept { return _size; }
    size_type capacity() const noexcept { return _capacity; }

    static constexpr size_type max_size() noexcept {
        return (size_type(1) << (sizeof(size_type) * 8 - 1)) / sizeof(storage_type);
    }

    bool empty() const noexcept { return !_size; }

    float load_factor() const noexcept {
        return (float)_size / (float)(_capacity + 1);
    }

    float max_load_factor() const noexcept {
        return 0.5f;
    }

    void max_load_factor(float) noexcept {
        //do nothing
    }

    void reserve(size_type hint) {
        hint *= 2;
        if (HRD_LIKELY(hint > _capacity))
            resize_pow2(hash_utils::roundup(hint));
    }

    void clear() {
        clear(IS_TRIVIALLY_DESTRUCTIBLE());
    }

    void shrink_to_fit()
    {
        if (HRD_LIKELY(_size)) {
            size_t pow2 = roundup(_size * 2);
            if (HRD_LIKELY(_erased || (_capacity + 1) != pow2))
                resize_pow2(pow2, IS_TRIVIALLY_COPYABLE());
        }
        else {
            clear(std::true_type());
        }
    }

    HRD_ALWAYS_INLINE iterator find(const key_type& k) noexcept {
        return iterator(find_(k), 0);
    }

    HRD_ALWAYS_INLINE const_iterator find(const key_type& k) const noexcept {
        return const_iterator(find_(k), 0);
    }

    HRD_ALWAYS_INLINE size_type count(const key_type& k) const noexcept {
        return find_(k) != nullptr;
    }

    template <class P>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> insert(P&& val) {
        return insert_(std::forward<P>(val), std::false_type());
    }

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    void insert(std::initializer_list<value_type> lst)
    {
        for (auto i = lst.begin(), e = lst.end(); i != e; ++i)
            insert_(*i, std::false_type());
    }
#endif

    /*! Can invalidate iterators.
    * \params it - Iterator pointing to a single element to be removed
    * \return return an iterator pointing to the position immediately following of the element erased
    */
    HRD_ALWAYS_INLINE iterator erase(const_iterator it) noexcept
    {
        iterator& ret = (iterator&)it;

        auto ee = reinterpret_cast<storage_type*>(_elements);
        auto e_next = ee + ((it._ptr + 1 - ee) & _capacity);

        if (HRD_LIKELY(!!it._ptr)) //valid
        {
            it._ptr->data.~value_type();
            _size--;

            //set DELETED_MARK only if next element not 0
            auto next_mark = e_next->mark;
            if (HRD_LIKELY(!next_mark))
                it._ptr->mark = 0;
            else {
                it._ptr->mark = DELETED_MARK;
                _erased++;
            }

            if (HRD_UNLIKELY(ret._cnt)) {
                for (--ret._cnt;;) {
                    if (HRD_UNLIKELY((++ret._ptr)->mark > DELETED_MARK))
                        return ret;
                }
            }
            it._ptr = nullptr;
        }
        return ret;
    }

    /*! Can invalidate iterators.
    * \params k - Key of the element to be erased
    * \return 1 - if element erased and zero otherwise
    */
    HRD_ALWAYS_INLINE size_type erase(const key_type& k) noexcept
    {
        auto ee = (storage_type*)_elements;
        const uint32_t mark = make_mark(hash_pred::operator()(k));

        for (size_t i = mark;;)
        {
            i &= _capacity;
            storage_type& r = ee[i++];
            auto h = r.mark;
            if (HRD_LIKELY(h == mark))
            {
                if (HRD_LIKELY(hash_pred::operator()(get_key(r.data), k))) //identical found
                {
                    r.data.~value_type();
                    _size--;

                    h = ee[i & _capacity].mark;

                    //set DELETED_MARK only if next element not 0
                    if (HRD_LIKELY(!h))
                        r.mark = 0;
                    else {
                        r.mark = DELETED_MARK;
                        _erased++;
                    }

                    return 1;
                }
            }
            else if (!h)
                return 0;
        }
    }

    HRD_ALWAYS_INLINE iterator begin() noexcept {
        auto pm = (storage_type*)_elements;
        if (auto cnt = _size) {
            for (--cnt;; ++pm) {
                if (HRD_UNLIKELY(pm->mark > DELETED_MARK))
                    return iterator(pm, cnt);
            }
        }
        return iterator();
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator cbegin() const noexcept {
        return const_cast<this_type*>(this)->begin();
    }

    iterator end() noexcept {
        return iterator();
    }

    const_iterator end() const noexcept {
        return cend();
    }

    inline const_iterator cend() const noexcept {
        return const_iterator();
    }

protected:
    HRD_ALWAYS_INLINE void swap(this_type& r)
    {
        __m128i mm0 = _mm_loadu_si128((__m128i*)this);
        __m128i r_mm0 = _mm_loadu_si128((__m128i*) & r);

#if defined(_WIN64) || defined(__LP64__)
        static_assert(sizeof(r) == 32, "must be sizeof(hash_base)==32");

        __m128i mm1 = _mm_loadu_si128((__m128i*)this + 1);
        __m128i r_mm1 = _mm_loadu_si128((__m128i*)&r + 1);

        _mm_storeu_si128((__m128i*)this, r_mm0);
        _mm_storeu_si128((__m128i*)this + 1, r_mm1);
        _mm_storeu_si128((__m128i*)&r, mm0);
        _mm_storeu_si128((__m128i*)&r + 1, mm1);
#else
        static_assert(sizeof(r) == 16, "must be sizeof(hash_base)==16");

        _mm_storeu_si128((__m128i*)this, r_mm0);
        _mm_storeu_si128((__m128i*)&r, mm0);
#endif
        if (!_capacity)
            _elements = &_size;
        if (!r._capacity)
            r._elements = &r._size;

        hash_pred::swap(r);
    }

    HRD_ALWAYS_INLINE void ctor_empty() noexcept
    {
        _size = 0;
        _capacity = 0;
        _elements = (storage_type*)&_size; //0-hash indicates empty element - use this trick to prevent redundant "is empty" check in find-function
        _erased = 0;
    }

    HRD_ALWAYS_INLINE void ctor_pow2(size_t pow2)
    {
        _size = 0;
        _capacity = pow2 - 1;
        _elements = (storage_type*)calloc(pow2, sizeof(storage_type));
        _erased = 0;
        if (HRD_UNLIKELY(!_elements))
            throw_bad_alloc();
    }

    HRD_ALWAYS_INLINE void dtor(std::true_type) noexcept
    {
        if (HRD_LIKELY(_capacity))
            free(_elements);
    }

    HRD_ALWAYS_INLINE void dtor(std::false_type) noexcept
    {
        if (auto cnt = _size)
        {
            for (storage_type* p = (storage_type*)_elements;; ++p)
            {
                if (HRD_UNLIKELY(p->mark > DELETED_MARK)) {
                    cnt--;
                    p->data.~value_type();
                    if (HRD_UNLIKELY(!cnt))
                        break;
                }
            }
        }
        else if (!_capacity)
            return;

        free(_elements);
    }

    HRD_ALWAYS_INLINE void clear(std::true_type) noexcept
    {
        if (HRD_LIKELY(_capacity)) {
            free(_elements);
            ctor_empty();
        }
    }

    HRD_ALWAYS_INLINE void clear(std::false_type) noexcept
    {
        dtor(std::false_type());
        ctor_empty();
    }

    //space must be allocated before
    HRD_ALWAYS_INLINE void insert_unique(const storage_type& rec)
    {
        for (size_t i = rec.mark;;)
        {
            i &= _capacity;
            auto& r = reinterpret_cast<storage_type*>(_elements)[i++];
            if (!r.mark) {
                new ((void*)&r) storage_type(rec);
                _size++;
                return;
            }
        }
    }

    void resize_pow2(size_t pow2, std::true_type) //IS_TRIVIALLY_COPYABLE
    {
        storage_type* elements = (storage_type*)calloc(pow2--, sizeof(storage_type));
        if (HRD_UNLIKELY(!elements))
            throw_bad_alloc();

        if (size_t cnt = _size)
        {
            for (storage_type* p = (storage_type*)_elements;; ++p)
            {
                if (HRD_UNLIKELY(p->mark > DELETED_MARK))
                {
                    for (size_t i = p->mark;;)
                    {
                        i &= pow2;
                        auto& r = elements[i++];
                        if (HRD_LIKELY(!r.mark)) {
                            memcpy(&r, p, sizeof(storage_type));
                            break;
                        }
                    }
                    if (!--cnt)
                        break;
                }
            }
        }

        if (_capacity)
            free(_elements);
        _capacity = pow2;
        _elements = elements;
        _erased = 0;
    }

    void resize_pow2(size_t pow2, std::false_type) //IS_TRIVIALLY_COPYABLE
    {
        this_type tmp(pow2--, false);
        if (HRD_LIKELY(_size)) //rehash
        {
            for (storage_type* p = (storage_type*)_elements;; ++p)
            {
                if (HRD_UNLIKELY(p->mark > DELETED_MARK)) {
                    value_type& r = p->data;
                    tmp.insert_unique(std::move(*p));
                    r.~value_type();

                    //next 2 lines to cover any exception that occurs during next tmp.insert_unique(std::move(r));
                    p->mark = DELETED_MARK;
                    _erased++;

                    if (!--_size)
                        break;
                }
            }
            _size = tmp._size;
            tmp._size = 0; //prevent elements dtor call
        }
        tmp._capacity = _capacity;
        _capacity = pow2;
        std::swap(_elements, tmp._elements);
        _erased = 0;
    }

    HRD_ALWAYS_INLINE void resize_pow2(size_t pow2) {
        resize_pow2(pow2, IS_TRIVIALLY_COPYABLE());
    }

    HRD_ALWAYS_INLINE void ctor_copy(const this_type& r, std::true_type) //IS_TRIVIALLY_COPYABLE == true
    {
        if (HRD_LIKELY(r._size))
        {
            size_t len = (r._capacity + 1) * sizeof(storage_type);
            _elements = malloc(len);
            if (HRD_LIKELY(!!_elements)) {
                memcpy(_elements, r._elements, len);
                _size = r._size;
                _capacity = r._capacity;
                _erased = r._erased;
            }
            else {
                throw_bad_alloc();
            }
        }
        else
            ctor_empty();
    }

    HRD_ALWAYS_INLINE void ctor_copy_1(const this_type& r, std::true_type) //IS_NOTHROW_CONSTRUCTIBLE == true
    {
        size_t cnt = r._size;
        for (const storage_type* p = (const storage_type*)r._elements;; ++p)
        {
            if (HRD_UNLIKELY(p->mark > DELETED_MARK)) {
                insert_unique(*p);
                if (HRD_UNLIKELY(!--cnt))
                    break;
            }
        }
    }

    HRD_ALWAYS_INLINE void ctor_copy_1(const this_type& r, std::false_type) //IS_NOTHROW_CONSTRUCTIBLE == false
    {
        dtor_if_throw_constructible<this_type> tmp(*reinterpret_cast<this_type*>(this));
        ctor_copy_1(r, std::true_type());

        tmp.reset();
    }

    HRD_ALWAYS_INLINE void ctor_copy(const this_type& r, std::false_type) //IS_TRIVIALLY_COPYABLE == false
    {
        if (HRD_LIKELY(r._size)) {
            ctor_pow2(r._capacity + 1);
            ctor_copy_1(r, IS_NOTHROW_CONSTRUCTIBLE());
        }
        else
            ctor_empty();
    }

    HRD_ALWAYS_INLINE void ctor_move(hash_base&& r) noexcept
    {
        memcpy(this, &r, sizeof(hash_base));
        if (HRD_LIKELY(r._capacity))
            r.ctor_empty();
        else
            _elements = &_size; //0-hash indicates empty element - use this trick to prevent redundant "is empty" check in find-function
    }

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    HRD_ALWAYS_INLINE void ctor_init_list(std::initializer_list<value_type> lst)
    {
        ctor_pow2(roundup((lst.size() | 1) * 2));
        dtor_if_throw_constructible<this_type> tmp(*this);
        ctor_insert_(lst.begin(), lst.end(), std::true_type());

        tmp.reset();
    }
#endif

    template<typename V>
    HRD_ALWAYS_INLINE void ctor_insert_(V&& val, std::true_type /*resized*/)
    {
        const uint32_t mark = make_mark(hash_pred::operator()(get_key(val)));
        for (size_t i = mark;;)
        {
            i &= _capacity;
            storage_type* r = (storage_type*)_elements + i++;
            auto h = r->mark;
            if (!h)
            {
                new ((void*)&r->data) value_type(std::forward<V>(val));
                r->mark = mark;
                _size++;
                return;
            }
            if (h == mark && HRD_LIKELY(hash_pred::operator()(get_key(r->data), get_key(val)))) //identical found
                return;
        }
    }

    template<typename V>
    HRD_ALWAYS_INLINE void ctor_insert_(V&& val, std::false_type /*not resized yet*/)
    {
        if (HRD_UNLIKELY((_capacity - _size) <= _size))
            resize_pow2(2 * (_capacity + 1));

        ctor_insert_(std::forward<V>(val), std::true_type());
    }

    template <typename Iter, typename SIZE_PREPARED>
    void ctor_insert_(Iter first, Iter last, SIZE_PREPARED) {
        for (; first != last; ++first)
            ctor_insert_(*first, SIZE_PREPARED());
    }

    template<typename Iter>
    HRD_ALWAYS_INLINE void ctor_iters(Iter first, Iter last, std::random_access_iterator_tag)
    {
        ctor_pow2(roundup((std::distance(first, last) | 1) * 2));
        dtor_if_throw_constructible<this_type> tmp(*this);
        ctor_insert_(first, last, std::true_type());

        tmp.reset();
    }

    template<typename Iter, typename XXX>
    HRD_ALWAYS_INLINE void ctor_iters(Iter first, Iter last, XXX)
    {
        dtor_if_throw_constructible<this_type> tmp(*this);
        ctor_empty();
        ctor_insert_(first, last, std::false_type());

        tmp.reset();
    }

    //all needed space should be allocated before
    template<typename V>
    std::pair<iterator, bool> insert_(V&& val, std::true_type)
    {
        storage_type* empty_spot = nullptr;
        uint32_t deleted_mark = DELETED_MARK;
        const uint32_t mark = make_mark(hash_pred::operator()(get_key(val)));

        for (size_t i = mark;;)
        {
            i &= _capacity;
            storage_type* r = reinterpret_cast<storage_type*>(_elements) + i++;
            auto h = r->mark;
            if (!h)
            {
                if (HRD_UNLIKELY(!!empty_spot)) r = empty_spot;

                std::pair<iterator, bool> ret(r, true);
                new ((void*)&r->data) value_type(std::forward<V>(val));
                r->mark = mark;
                _size++;
                if (HRD_UNLIKELY(!!empty_spot)) _erased--;
                return ret;
            }
            if (h == mark)
            {
                if (HRD_LIKELY(hash_pred::operator()(get_key(r->data), get_key(val)))) //identical found
                    return std::pair<iterator, bool>(r, false);
            }
            else if (h == deleted_mark)
            {
                deleted_mark = 0; //prevent additional empty_spot == null_ptr comparison
                empty_spot = r;
            }
        }
    }

    //probe available size
    template<typename V>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> insert_(V&& val, std::false_type)
    {
        size_t used = _erased + _size;
        if (HRD_UNLIKELY(_capacity - used <= used))
            resize_pow2(2 * (_capacity + 1));

        return insert_(std::forward<V>(val), std::true_type());
    }

    HRD_ALWAYS_INLINE storage_type* find_(const key_type& k) const noexcept
    {
        const uint32_t mark = make_mark(hash_pred::operator()(k));
        for (size_t i = mark;;)
        {
            i &= _capacity;
            storage_type* r = (storage_type*)_elements + i++;
            auto h = r->mark;
            if (HRD_LIKELY(h == mark))
            {
                if (HRD_LIKELY(hash_pred::operator()(get_key(r->data), k))) //identical found
                    return r;
            }
            else if (!h)
                return nullptr;
        }
    }

    template <typename Iter, typename SIZE_PREPARED>
    void insert_iters_(Iter first, Iter last, SIZE_PREPARED) {
        for (; first != last; ++first)
            insert_(*first, SIZE_PREPARED());
    }

    template<typename Iter, class this_type>
    HRD_ALWAYS_INLINE void insert_iters(Iter first, Iter last, std::random_access_iterator_tag)
    {
        size_t actual = std::distance(first, last) + _size;
        if ((_erased + actual) >= (_capacity / 2))
            resize_pow2(roundup((actual | 1) * 2));

        insert_iters_(first, last, std::true_type());
    }

    template<typename Iter, typename XXX>
    HRD_ALWAYS_INLINE void insert_iters(Iter first, Iter last, XXX) {
        insert_iters_(first, last, std::false_type());
    }

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    template<typename K, typename... Args>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> emplace_(K&& k, Args&&... args)
#else
    template<typename K, typename Args>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> emplace_(K&& k, Args&& args)
#endif //C++-11 support
    {
        size_t used = _erased + _size;
        if (HRD_UNLIKELY(_capacity - used <= used))
            resize_pow2(2 * (_capacity + 1));

        storage_type* empty_spot = nullptr;
        uint32_t deleted_mark = DELETED_MARK;
        const uint32_t mark = make_mark(hash_pred::operator()(k));

        for (size_t i = mark;;)
        {
            i &= _capacity;
            storage_type* r = reinterpret_cast<storage_type*>(_elements) + i++;
            auto h = r->mark;
            if (!h)
            {
                if (HRD_UNLIKELY(!!empty_spot)) r = empty_spot;

                std::pair<iterator, bool> ret(r, true);
#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
                new ((void*)&r->data) value_type(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(k)), std::forward_as_tuple(std::forward<Args>(args)...));
#else
                new ((void*)&r->data) value_type(std::forward<K>(k), std::forward<Args>(args));
#endif //C++-11 support
                r->mark = mark;
                _size++;
                if (HRD_UNLIKELY(!!empty_spot)) _erased--;
                return ret;
            }
            if (h == mark)
            {
                if (HRD_LIKELY(hash_pred::operator()(r->data.first, k))) //identical found
                    return std::pair<iterator, bool>(r, false);
            }
            else if (h == deleted_mark)
            {
                deleted_mark = 0; //prevent additional empty_spot == null_ptr comparison
                empty_spot = r;
            }
        }
    }

    HRD_ALWAYS_INLINE static const key_type& get_key(const key_type& k) noexcept {
        return k;
    }
    HRD_ALWAYS_INLINE static const key_type& get_key(const std::pair<const key_type&, T>& pr) noexcept {
        return pr.first;
    }
};

//----------------------------------------- hash_set -----------------------------------------

template<class Key, class Hash = hash_utils::hash_<Key>, class Pred = std::equal_to<Key>>
class hash_set : public hash_base<Key, Key, const Key, Hash, Pred>
{
    typedef hash_base<Key, Key, const Key, Hash, Pred> super_type;
    using super_type::storage_type;
public:
    typedef hash_set<Key, Hash, Pred>   this_type;
    typedef Key                         key_type;
    typedef Hash                        hasher;
    typedef Pred                        key_equal;
    typedef const key_type              value_type;
    typedef value_type&                 reference;
    typedef const value_type&           const_reference;
    typedef typename super_type::iterator iterator;
    typedef typename super_type::const_iterator const_iterator;
    using typename super_type::size_type;
    using super_type::insert;

    hash_set() {}

    hash_set(const this_type& r) :
        super_type(r)
    {}

    hash_set(this_type&& r) noexcept :
        super_type(std::move(r))
    {}

    hash_set(size_type hint_size, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        super_type(hint_size, hf, eql)
    {}

    template<typename Iter>
    hash_set(Iter first, Iter last, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        super_type(first, last, hf, eql)
    {}

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    hash_set(std::initializer_list<value_type> lst, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        super_type(lst, hf, eql)
    {}
#endif

    ~hash_set() {}

    void swap(hash_set& r) noexcept {
        super_type::swap(r);
    }

    HRD_ALWAYS_INLINE std::pair<iterator, bool> insert(const key_type& val) {
        return super_type::insert_(val, std::false_type());
    }

    template<typename Iter>
    HRD_ALWAYS_INLINE void insert(Iter first, Iter last) {
        super_type::insert_iters(first, last, typename std::iterator_traits<Iter>::iterator_category());
    }

    template<class K>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> emplace(K&& val) {
        return super_type::insert_(std::forward<K>(val), std::false_type());
    }

    HRD_ALWAYS_INLINE hash_set& operator=(const hash_set& r) {
        this_type(r).swap(*this);
        return *this;
    }

    HRD_ALWAYS_INLINE hash_set& operator=(hash_set&& r) noexcept {
        swap(r);
        return *this;
    }
};

//----------------------------------------- hash_map -----------------------------------------

template<class Key, class T, class Hash = hash_utils::hash_<Key>, class Pred = std::equal_to<Key>>
class hash_map : public hash_base<Key, T, std::pair<const Key, T>, Hash, Pred>
{
    typedef hash_base<Key, T, std::pair<const Key, T>, Hash, Pred> super_type;
    using typename super_type::storage_type;
public:
    typedef hash_map<Key, T, Hash, Pred>            this_type;
    typedef Key                                     key_type;
    typedef T                                       mapped_type;
    typedef Hash                                    hasher;
    typedef Pred                                    key_equal;
    typedef std::pair<const key_type, mapped_type>  value_type;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;
    typedef typename super_type::iterator           iterator;
    typedef typename super_type::const_iterator     const_iterator;
    using typename super_type::size_type;
    using super_type::insert;

    hash_map() {}

    hash_map(const hash_map& r) :
        super_type(r)
    {}

    hash_map(hash_map&& r) noexcept :
        super_type(std::move(r))
    {}

    hash_map(size_type hint_size, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        super_type(hint_size, hf, eql)
    {}

    template<typename Iter>
    hash_map(Iter first, Iter last, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        super_type(first, last, hf, eql)
    {}

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    hash_map(std::initializer_list<value_type> lst, const hasher& hf = hasher(), const key_equal& eql = key_equal()) :
        super_type(lst, hf, eql)
    {}
#endif

    ~hash_map() {}

    void swap(hash_map& r) noexcept {
        super_type::swap(r);
    }

    HRD_ALWAYS_INLINE std::pair<iterator, bool> insert(const value_type& val) {
        return super_type::insert_(val, std::false_type());
    }

    template<typename Iter>
    HRD_ALWAYS_INLINE void insert(Iter first, Iter last) {
        super_type::insert_iters(first, last, typename std::iterator_traits<Iter>::iterator_category());
    }

#if (__cplusplus >= 201402L || _MSC_VER > 1600 || __clang__)
    template<class... Args>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> emplace(const Key& key, Args&&... args) {
        return super_type::emplace_(key, std::forward<Args>(args)...);
    }

    template<class K, class... Args>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> emplace(K&& key, Args&&... args) {
        return super_type::emplace_(std::forward<K>(key), std::forward<Args>(args)...);
    }
#else
    template<class Args>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> emplace(const Key& key, Args&& args) {
        return super_type::emplace_(key, std::forward<Args>(args));
    }

    template<class K, class Args>
    HRD_ALWAYS_INLINE std::pair<iterator, bool> emplace(K&& key, Args&& args) {
        return super_type::emplace_(std::forward<K>(key), std::forward<Args>(args));
    }
#endif //C++-11 support

    HRD_ALWAYS_INLINE hash_map& operator=(const hash_map& r) {
        this_type(r).swap(*this);
        return *this;
    }

    HRD_ALWAYS_INLINE hash_map& operator=(hash_map&& r) noexcept {
        swap(r);
        return *this;
    }

    HRD_ALWAYS_INLINE mapped_type& operator[](const key_type& k) {
        return find_insert(k);
    }

    HRD_ALWAYS_INLINE mapped_type& operator[](key_type&& k) {
        return find_insert(std::move(k));
    }

private:
    template<typename V>
    HRD_ALWAYS_INLINE mapped_type& find_insert(V&& k)
    {
        size_type used = this->_erased + this->_size;
        if (HRD_UNLIKELY(this->_capacity - used <= used))
            super_type::resize_pow2(2 * (this->_capacity + 1));

        storage_type* empty_spot = nullptr;
        uint32_t deleted_mark = hash_utils::DELETED_MARK;
        const uint32_t mark = hash_utils::make_mark(super_type::hash_pred::operator()(k));

        for (size_t i = mark;;)
        {
            i &= this->_capacity;
            storage_type* r = reinterpret_cast<storage_type*>(this->_elements) + i++;
            auto h = r->mark;
            if (!h)
            {
                if (HRD_UNLIKELY(!!empty_spot)) r = empty_spot;

                new ((void*)&r->data) value_type(std::forward<V>(k), mapped_type());
                r->mark = mark;
                this->_size++;
                if (HRD_UNLIKELY(!!empty_spot)) this->_erased--;
                return r->data.second;
            }
            if (h == mark)
            {
                if (HRD_LIKELY(super_type::hash_pred::operator()(r->data.first, k))) //identical found
                    return r->data.second;
            }
            else if (h == deleted_mark)
            {
                deleted_mark = 0; //prevent additional empty_spot == null_ptr comparison
                empty_spot = r;
            }
        }
    }
};

} //namespace hrd6
