/**
 * Copyright 2024 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

// Based on wyhash, identity for basic types and fallback on std::hash
// Only supports uint64_t version currently
// https://github.com/wangyi-fudan/wyhash
// Free and unencumbered software released into the public domain.

#ifndef INDIVI_HASH_H
#define INDIVI_HASH_H

#include "detail/indivi_defines.h"
#include "detail/indivi_utils.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <cstdint>
#include <cstring>

#ifdef INDIVI_MSVC
  #include <intrin.h> // for _umul128
#endif
#ifdef INDIVI_CPP17
  #include <cstddef>
  #include <string_view>
#endif

#ifdef INDIVI_ARCH_64
  #define INDIVI_PTR_SHIFT  3
#else
  #define INDIVI_PTR_SHIFT  2
#endif

namespace indivi
{
namespace detail
{
namespace wyhash
{
  inline void mum(uint64_t* a, uint64_t* b)
  {
  #if defined(__SIZEOF_INT128__)
    __uint128_t r = *a;
    r *= *b;
    *a = static_cast<uint64_t>(r);
    *b = static_cast<uint64_t>(r >> 64U);
  #elif defined(INDIVI_MSVC) && defined(_M_X64)
    *a = _umul128(*a, *b, b);
  #else
    uint64_t ha = *a >> 32U;
    uint64_t hb = *b >> 32U;
    uint64_t la = static_cast<uint32_t>(*a);
    uint64_t lb = static_cast<uint32_t>(*b);
    uint64_t hi{};
    uint64_t lo{};
    uint64_t rh = ha * hb;
    uint64_t rm0 = ha * lb;
    uint64_t rm1 = hb * la;
    uint64_t rl = la * lb;
    uint64_t t = rl + (rm0 << 32U);
    auto c = static_cast<uint64_t>(t < rl);
    lo = t + (rm1 << 32U);
    c += static_cast<uint64_t>(lo < t);
    hi = rh + (rm0 >> 32U) + (rm1 >> 32U) + c;
    *a = lo;
    *b = hi;
  #endif
  }

  inline uint64_t mix(uint64_t a, uint64_t b)
  {
    mum(&a, &b);
    return a ^ b;
  }

  inline uint32_t mix32(uint32_t a, uint32_t b)
  {
    uint64_t r = (uint64_t)a * (uint64_t)b;
    return (uint32_t)r ^ (uint32_t)(r >> 32);
  }

  // read functions (ignoring endianness = hash not cross platform)
  inline uint64_t r8(const uint8_t* p)
  {
    uint64_t v{};
    std::memcpy(&v, p, 8U);
    return v;
  }

  inline uint64_t r4(const uint8_t* p)
  {
    uint32_t v{};
    std::memcpy(&v, p, 4);
    return v;
  }

  inline uint64_t r3(const uint8_t* p, size_t k)
  {
    return (static_cast<uint64_t>(p[0]) << 16U)
      | (static_cast<uint64_t>(p[k >> 1U]) << 8U) | p[k - 1];
  }

  inline uint64_t hash(const void* key, size_t len)
  {
    static constexpr uint64_t secret[] { UINT64_C(0x2d358dccaa6c78a5),
                                         UINT64_C(0x8bb84b93962eacc9),
                                         UINT64_C(0x4b33a62ed433d4a3),
                                         UINT64_C(0x4d5a2da51de1aa47) };

    const uint8_t* p = static_cast<const uint8_t*>(key);
    uint64_t seed = secret[0];
    uint64_t a{};
    uint64_t b{};

    if (len <= 16)
    {
      if (len >= 4)
      {
        a = (r4(p) << 32U) | r4(p + ((len >> 3U) << 2U));
        b = (r4(p + len - 4) << 32U) | r4(p + len - 4 - ((len >> 3U) << 2U));
      }
      else if (len > 0)
      {
        a = r3(p, len);
        b = 0;
      }
      else
      {
        a = 0;
        b = 0;
      }
    }
    else
    {
      size_t i = len;
      if (i > 48)
      {
        uint64_t see1 = seed;
        uint64_t see2 = seed;
        do {
          seed = mix(r8(p     ) ^ secret[1], r8(p + 8) ^ seed);
          see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
          see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
          p += 48;
          i -= 48;
        }
        while (i > 48);

        seed ^= see1 ^ see2;
      }

      while (i > 16)
      {
        seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
        i -= 16;
        p += 16;
      }
      a = r8(p + i - 16);
      b = r8(p + i - 8);
    }

    return mix(secret[1] ^ len, mix(a ^ secret[1], b ^ seed));
  }

} // namespace wyhash


// Detect if Hash has avalanching trait
// i.e. if 'Hash::is_avalanching' type is present
// Default is false (triggers additional bit mixing)
template< typename Hash, typename = void >
struct hash_is_avalanching_impl
  : std::false_type {};

template< typename Hash >
struct hash_is_avalanching_impl<Hash, traits::void_t<typename Hash::is_avalanching>>
  : std::true_type {};

template< typename Hash >
struct hash_is_avalanching : hash_is_avalanching_impl<Hash>::type {};

} // namespace detail


// Fallback (non avalanching)
template< typename T, typename Enable = void >
struct hash
{
  uint64_t operator()(const T& obj) const
    noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<const T&>())))
  {
    return std::hash<T>{}(obj);
  }
};

// Specializations
template< typename CharT >
struct hash<std::basic_string<CharT>>
{
  using is_avalanching = void;
  uint64_t operator()(const std::basic_string<CharT>& str) const noexcept
  {
    return detail::wyhash::hash(str.data(), sizeof(CharT) * str.size());
  }
};

#ifdef INDIVI_CPP17
template< typename CharT >
struct hash<std::basic_string_view<CharT>>
{
  using is_avalanching = void;
  uint64_t operator()(const std::basic_string_view<CharT>& sv) const noexcept
  {
    return detail::wyhash::hash(sv.data(), sizeof(CharT) * sv.size());
  }
};
#endif

template< class T >
struct hash<T*>
{
  uint64_t operator()(T* ptr) const noexcept
  {
    uint64_t raw = (uint64_t)ptr;
    return (uint64_t)(raw + (raw >> INDIVI_PTR_SHIFT));
  }
};

template< class T >
struct hash<std::unique_ptr<T>>
{
  uint64_t operator()(const std::unique_ptr<T>& ptr) const noexcept
  {
    uint64_t raw = (uint64_t)ptr.get();
    return (uint64_t)(raw + (raw >> INDIVI_PTR_SHIFT));
  }
};

template< class T >
struct hash<std::shared_ptr<T>>
{
  uint64_t operator()(const std::shared_ptr<T>& ptr) const noexcept
  {
    uint64_t raw = (uint64_t)ptr.get();
    return (uint64_t)(raw + (raw >> INDIVI_PTR_SHIFT));
  }
};

template< typename Enum >
struct hash<Enum, typename std::enable_if<std::is_enum<Enum>::value>::type>
{
  uint64_t operator()(Enum e) const noexcept
  {
    return (uint64_t)e;
  }
};

// Identity (non avalanching)
#define INDIVI_HASH_IMPL(T)                               \
  template<>                                              \
  struct hash<T> {                                        \
      uint64_t operator()(const T& obj) const noexcept {  \
        return (uint64_t)obj;                             \
      }                                                   \
  }

#ifdef INDIVI_CPP17
INDIVI_HASH_IMPL(std::byte);
#endif
INDIVI_HASH_IMPL(bool);
INDIVI_HASH_IMPL(char);
INDIVI_HASH_IMPL(signed char);
INDIVI_HASH_IMPL(unsigned char);
INDIVI_HASH_IMPL(char16_t);
INDIVI_HASH_IMPL(char32_t);
INDIVI_HASH_IMPL(wchar_t);
INDIVI_HASH_IMPL(short);
INDIVI_HASH_IMPL(unsigned short);
INDIVI_HASH_IMPL(int);
INDIVI_HASH_IMPL(unsigned int);
INDIVI_HASH_IMPL(long);
INDIVI_HASH_IMPL(unsigned long);
INDIVI_HASH_IMPL(long long);
INDIVI_HASH_IMPL(unsigned long long);

} // namespace indivi

#undef INDIVI_HASH_IMPL
#undef INDIVI_PTR_SHIFT

#endif // INDIVI_HASH_H
