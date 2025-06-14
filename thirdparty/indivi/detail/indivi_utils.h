/**
 * Copyright 2024 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

#ifndef INDIVI_UTILS_H
#define INDIVI_UTILS_H

#include "indivi_defines.h"

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


namespace indivi
{
namespace detail
{
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
