/**
 * Copyright 2024 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

#ifndef INDIVI_UTILS_H
#define INDIVI_UTILS_H

#include "indivi/detail/indivi_defines.h"

#include <cassert>
#include <type_traits>
#include <utility>


#if defined(INDIVI_GCC) || defined(INDIVI_CLANG)
  #define INDIVI_PREFETCH(p) __builtin_prefetch((const char*)(p))
#elif defined(INDIVI_SIMD_SSE2)
  #include <emmintrin.h>
  #define INDIVI_PREFETCH(p) _mm_prefetch((const char*)(p), _MM_HINT_T0)
#else
  #define INDIVI_PREFETCH(p) ((void)(p))
#endif

#ifdef INDIVI_MSVC
  #include <intrin.h> // for BitScanForward
#endif

namespace indivi
{
namespace detail
{
  static inline uint32_t round_up_pow2(uint32_t v) noexcept
  {
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    
    return v;
  }
  
  static inline uint64_t round_up_pow2(uint64_t v) noexcept
  {
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    ++v;
    
    return v;
  }

  static inline int first_bit_index(uint32_t v) noexcept
  {
    assert(v != 0);
  #ifdef INDIVI_MSVC
    unsigned long r;
    _BitScanForward(&r, (unsigned long)v);
    return (int)r;
  #else
    return __builtin_ctz((unsigned int)v);
  #endif
  }

  static inline int first_bit_index(uint64_t v) noexcept
  {
    assert(v != 0);
  #ifdef INDIVI_MSVC
    unsigned long r;
    _BitScanForward64(&r, (unsigned __int64)v);
    return (int)r;
  #else
    return __builtin_ctzll((unsigned long long)v);
  #endif
  }

  static inline int first_bit_index(int v) noexcept
  {
    return first_bit_index((unsigned int)v);
  }

  static inline int last_bit_index(int v) noexcept
  {
    assert(v != 0);
  #ifdef INDIVI_MSVC
    unsigned long r;
    _BitScanReverse(&r, (unsigned long)v);
    return (int)r;
  #else
    return 31 ^ __builtin_clz((unsigned int)v);
  #endif
  }

namespace traits
{
  template< typename... Ts >
  struct make_void { typedef void type; };

  template< typename... Ts >
  using void_t = typename make_void<Ts...>::type;

  template< class T >
  struct remove_cvref
  {
    using type = typename std::remove_cv< typename std::remove_reference<T>::type >::type;
  };

  template< class U, class V >
  using is_similar = std::is_same<typename remove_cvref<U>::type, typename remove_cvref<V>::type >;

  using std::swap;

  template< class T, class = void >
  struct is_nothrow_swappable_impl
  {
    constexpr static bool const value = false;
  };

  template< class T >
  struct is_nothrow_swappable_impl<T,
    void_t<decltype(swap(std::declval<T&>(), std::declval<T&>()))> >
  {
    constexpr static bool const value =
      noexcept(swap(std::declval<T&>(), std::declval<T&>()));
  };

  template< class T >
  struct is_nothrow_swappable
    : public std::integral_constant<bool, is_nothrow_swappable_impl<T>::value>
  {};

} // namespace traits
} // namespace detail
} // namespace indivi

#endif // INDIVI_UTILS_H
