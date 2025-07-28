/**
 * Copyright 2024 Guillaume AUJAY. All rights reserved.
 * Distributed under the Apache License Version 2.0
 */

#ifndef INDIVI_FLAT_UTABLE_H
#define INDIVI_FLAT_UTABLE_H

#include "../hash.h"
#include "indivi_defines.h"
#include "indivi_utils.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef INDIVI_SIMD_SSE2
  #include <emmintrin.h>
#elif defined(INDIVI_SIMD_L_ENDIAN_NEON)
  #include <arm_neon.h>
#else
  #error indivi flat unordered containers need SSE2 or little endian NEON support
#endif

#ifdef INDIVI_MSVC
  #include <intrin.h> // for BitScanForward
#endif

#if !defined(INDIVI_FLAT_U_OVERFLOW_SATURATED) && !defined(INDIVI_FLAT_U_NCHECK)
  #define INDIVI_FLAT_U_OVERFLOW_SATURATED // enabled by default (warns about very bad hash)
#endif
#ifdef INDIVI_FLAT_U_OVERFLOW_SATURATED  // assert on overflow counter saturation (i.e. bad hash)
  #include <cassert>
  #define INDIVI_UTABLE_OFLW_SAT_ASSERT(x) assert(x)
#else
  #define INDIVI_UTABLE_OFLW_SAT_ASSERT(x) ((void)(x))
#endif

#ifdef INDIVI_FLAT_U_DIST_SATURATED // assert on distance counter saturation (i.e. perfectible hash)
  #include <cassert>
  #define INDIVI_UTABLE_DIST_SAT_ASSERT(x) assert(x)
#else
  #define INDIVI_UTABLE_DIST_SAT_ASSERT(x) ((void)(x))
#endif

#ifdef INDIVI_FLAT_U_DEBUG
  #include <cassert>
  #define INDIVI_UTABLE_ASSERT(x) assert(x)
#else
  #define INDIVI_UTABLE_ASSERT(x) ((void)(x))
#endif

// use faster quadratic probing (at group level)
// else use better distributed double-hashing linear probing
#define INDIVI_FLAT_U_QUAD_PROB

namespace indivi
{
namespace detail
{
/*
 * Metadata struct for group of 16 buckets.
 * SIMD-accelerated, used by `flat_utable`.
 */
struct alignas(32) MetaGroup // actually aligned/init manually, see NewStorage
{
  alignas(16) unsigned char hfrags[16] = {}; // 1-Byte hash fragments (0 means empty)
  unsigned char oflws[8] = {}; // 1-Byte dual overflow counters (modulo 8, i.e. n and n+8 entries)
  unsigned char dists[8] = {}; // 4-bits distance counters from original bucket (low/high, modulo 8)
  
  // static
  static int uc_ctz(int v) noexcept
  {
    INDIVI_UTABLE_ASSERT(v != 0);
  #ifdef INDIVI_MSVC
    unsigned long r;
    _BitScanForward(&r, (unsigned long)v);
    return (int)r;
  #else
    return __builtin_ctz((unsigned int)v);
  #endif
  }
  
  static int uc_last_set(int v) noexcept
  {
    INDIVI_UTABLE_ASSERT(v != 0);
  #ifdef INDIVI_MSVC
    unsigned long r;
    _BitScanReverse(&r, (unsigned long)v);
    return (int)r;
  #else
    return 31 ^ __builtin_clz((unsigned int)v);
  #endif
  }
  
  static int match_word(std::size_t hash) noexcept
  {
    // map 0 to 8, so metadata modulo matches
    static constexpr uint32_t word[] = {
        0x08080808u,0x01010101u,0x02020202u,0x03030303u,0x04040404u,0x05050505u,
        0x06060606u,0x07070707u,0x08080808u,0x09090909u,0x0A0A0A0Au,0x0B0B0B0Bu,
        0x0C0C0C0Cu,0x0D0D0D0Du,0x0E0E0E0Eu,0x0F0F0F0Fu,0x10101010u,0x11111111u,
        0x12121212u,0x13131313u,0x14141414u,0x15151515u,0x16161616u,0x17171717u,
        0x18181818u,0x19191919u,0x1A1A1A1Au,0x1B1B1B1Bu,0x1C1C1C1Cu,0x1D1D1D1Du,
        0x1E1E1E1Eu,0x1F1F1F1Fu,0x20202020u,0x21212121u,0x22222222u,0x23232323u,
        0x24242424u,0x25252525u,0x26262626u,0x27272727u,0x28282828u,0x29292929u,
        0x2A2A2A2Au,0x2B2B2B2Bu,0x2C2C2C2Cu,0x2D2D2D2Du,0x2E2E2E2Eu,0x2F2F2F2Fu,
        0x30303030u,0x31313131u,0x32323232u,0x33333333u,0x34343434u,0x35353535u,
        0x36363636u,0x37373737u,0x38383838u,0x39393939u,0x3A3A3A3Au,0x3B3B3B3Bu,
        0x3C3C3C3Cu,0x3D3D3D3Du,0x3E3E3E3Eu,0x3F3F3F3Fu,0x40404040u,0x41414141u,
        0x42424242u,0x43434343u,0x44444444u,0x45454545u,0x46464646u,0x47474747u,
        0x48484848u,0x49494949u,0x4A4A4A4Au,0x4B4B4B4Bu,0x4C4C4C4Cu,0x4D4D4D4Du,
        0x4E4E4E4Eu,0x4F4F4F4Fu,0x50505050u,0x51515151u,0x52525252u,0x53535353u,
        0x54545454u,0x55555555u,0x56565656u,0x57575757u,0x58585858u,0x59595959u,
        0x5A5A5A5Au,0x5B5B5B5Bu,0x5C5C5C5Cu,0x5D5D5D5Du,0x5E5E5E5Eu,0x5F5F5F5Fu,
        0x60606060u,0x61616161u,0x62626262u,0x63636363u,0x64646464u,0x65656565u,
        0x66666666u,0x67676767u,0x68686868u,0x69696969u,0x6A6A6A6Au,0x6B6B6B6Bu,
        0x6C6C6C6Cu,0x6D6D6D6Du,0x6E6E6E6Eu,0x6F6F6F6Fu,0x70707070u,0x71717171u,
        0x72727272u,0x73737373u,0x74747474u,0x75757575u,0x76767676u,0x77777777u,
        0x78787878u,0x79797979u,0x7A7A7A7Au,0x7B7B7B7Bu,0x7C7C7C7Cu,0x7D7D7D7Du,
        0x7E7E7E7Eu,0x7F7F7F7Fu,0x80808080u,0x81818181u,0x82828282u,0x83838383u,
        0x84848484u,0x85858585u,0x86868686u,0x87878787u,0x88888888u,0x89898989u,
        0x8A8A8A8Au,0x8B8B8B8Bu,0x8C8C8C8Cu,0x8D8D8D8Du,0x8E8E8E8Eu,0x8F8F8F8Fu,
        0x90909090u,0x91919191u,0x92929292u,0x93939393u,0x94949494u,0x95959595u,
        0x96969696u,0x97979797u,0x98989898u,0x99999999u,0x9A9A9A9Au,0x9B9B9B9Bu,
        0x9C9C9C9Cu,0x9D9D9D9Du,0x9E9E9E9Eu,0x9F9F9F9Fu,0xA0A0A0A0u,0xA1A1A1A1u,
        0xA2A2A2A2u,0xA3A3A3A3u,0xA4A4A4A4u,0xA5A5A5A5u,0xA6A6A6A6u,0xA7A7A7A7u,
        0xA8A8A8A8u,0xA9A9A9A9u,0xAAAAAAAAu,0xABABABABu,0xACACACACu,0xADADADADu,
        0xAEAEAEAEu,0xAFAFAFAFu,0xB0B0B0B0u,0xB1B1B1B1u,0xB2B2B2B2u,0xB3B3B3B3u,
        0xB4B4B4B4u,0xB5B5B5B5u,0xB6B6B6B6u,0xB7B7B7B7u,0xB8B8B8B8u,0xB9B9B9B9u,
        0xBABABABAu,0xBBBBBBBBu,0xBCBCBCBCu,0xBDBDBDBDu,0xBEBEBEBEu,0xBFBFBFBFu,
        0xC0C0C0C0u,0xC1C1C1C1u,0xC2C2C2C2u,0xC3C3C3C3u,0xC4C4C4C4u,0xC5C5C5C5u,
        0xC6C6C6C6u,0xC7C7C7C7u,0xC8C8C8C8u,0xC9C9C9C9u,0xCACACACAu,0xCBCBCBCBu,
        0xCCCCCCCCu,0xCDCDCDCDu,0xCECECECEu,0xCFCFCFCFu,0xD0D0D0D0u,0xD1D1D1D1u,
        0xD2D2D2D2u,0xD3D3D3D3u,0xD4D4D4D4u,0xD5D5D5D5u,0xD6D6D6D6u,0xD7D7D7D7u,
        0xD8D8D8D8u,0xD9D9D9D9u,0xDADADADAu,0xDBDBDBDBu,0xDCDCDCDCu,0xDDDDDDDDu,
        0xDEDEDEDEu,0xDFDFDFDFu,0xE0E0E0E0u,0xE1E1E1E1u,0xE2E2E2E2u,0xE3E3E3E3u,
        0xE4E4E4E4u,0xE5E5E5E5u,0xE6E6E6E6u,0xE7E7E7E7u,0xE8E8E8E8u,0xE9E9E9E9u,
        0xEAEAEAEAu,0xEBEBEBEBu,0xECECECECu,0xEDEDEDEDu,0xEEEEEEEEu,0xEFEFEFEFu,
        0xF0F0F0F0u,0xF1F1F1F1u,0xF2F2F2F2u,0xF3F3F3F3u,0xF4F4F4F4u,0xF5F5F5F5u,
        0xF6F6F6F6u,0xF7F7F7F7u,0xF8F8F8F8u,0xF9F9F9F9u,0xFAFAFAFAu,0xFBFBFBFBu,
        0xFCFCFCFCu,0xFDFDFDFDu,0xFEFEFEFEu,0xFFFFFFFFu
    };
    return (int)word[(unsigned char)hash];
  }
  
  static MetaGroup* empty_group() noexcept
  {
    static constexpr MetaGroup sEmptyGroup{}; // dummy group for empty tables
    return const_cast<MetaGroup*>(&sEmptyGroup);
  }
  
  // members
#ifdef INDIVI_SIMD_SSE2
  __m128i load_hfrags() const noexcept
  {
    return _mm_load_si128(reinterpret_cast<const __m128i*>(hfrags));
  }
  
  int match_hfrag(std::size_t hash) const noexcept
  {
    return _mm_movemask_epi8(
        _mm_cmpeq_epi8(load_hfrags(), _mm_set1_epi32(match_word(hash))));
  }
  
  int match_empty() const noexcept
  {
    return _mm_movemask_epi8(
        _mm_cmpeq_epi8(load_hfrags(), _mm_setzero_si128()));
  }
#else // NEON
  static inline int mm_movemask_epi8(uint8x16_t v) noexcept
  {
    // mask with powers of 2, group pairs as u16 then accumulate
    static constexpr uint8_t mask[] = {
        1, 2, 4, 8, 16, 32, 64, 128,
        1, 2, 4, 8, 16, 32, 64, 128
    };

    uint8x16_t vmask = vandq_u8(vld1q_u8(mask), v);
    uint8x8x2_t vzip = vzip_u8(vget_low_u8(vmask), vget_high_u8(vmask));
    uint16x8_t vmix = vreinterpretq_u16_u8(vcombine_u8(vzip.val[0], vzip.val[1]));
    
  #ifdef INDIVI_ARCH_ARM64
    return vaddvq_u16(vmix);
  #else
    uint64x2_t vsum = vpaddlq_u32(vpaddlq_u16(vmix));
    return int(vgetq_lane_u64(vsum, 0)) + int(vgetq_lane_u64(vsum, 1));
  #endif
  }
  
  uint8x16_t load_hfrags() const noexcept
  {
    return vld1q_u8(reinterpret_cast<const uint8_t*>(hfrags));
  }
  
  int match_hfrag(std::size_t hash) const noexcept
  {
    return mm_movemask_epi8(
        vceqq_u8(load_hfrags(), vdupq_n_u8((uint8_t)match_word(hash))));
  }
  
  int match_empty() const noexcept
  {
    return mm_movemask_epi8(
        vceqq_u8(load_hfrags(), vdupq_n_u8(0)));
  }
#endif
  
  int match_set() const noexcept
  {
    return (~match_empty()) & 0xFFFF;
  }
  
  unsigned char get_overflow(std::size_t hash) const noexcept
  {
    std::size_t pos = hash & 0x07;
    return oflws[pos];
  }
  
  void inc_overflow(std::size_t hash) noexcept
  {
    std::size_t pos = hash & 0x07;
    if (oflws[pos] != 255) // not saturated
      ++oflws[pos];
    else
      INDIVI_UTABLE_OFLW_SAT_ASSERT(false
        && "Overflow counter saturated: tombstone will remain until rehash. Please check your hash quality.");
  }
  
  void dec_overflow(std::size_t hash) noexcept
  {
    std::size_t pos = hash & 0x07;
    if (oflws[pos] != 255) // not saturated
    {
      INDIVI_UTABLE_ASSERT(oflws[pos]);
      --oflws[pos];
    }
  }
  
  unsigned char get_distance(int pos) const noexcept
  {
    INDIVI_UTABLE_ASSERT(pos < 16);
    unsigned char dist = dists[pos & 0x07];
    dist >>= (pos & 0x08) ? 4 : 0;
    return dist & 0x0F;
  }
  
  void set_distance(int pos, uint32_t distance) noexcept
  {
    INDIVI_UTABLE_ASSERT(pos < 16);
    INDIVI_UTABLE_ASSERT((dists[pos & 0x07] & ((pos & 0x08) ? 0xF0 : 0x0F)) == 0u);
    if (distance)
    {
      INDIVI_UTABLE_DIST_SAT_ASSERT((distance < 15u)
        && "Distance counter saturated: key will be rehashed on erase(iter). Maybe check your hash quality.");
      unsigned char dist = (unsigned char)std::min<uint32_t>(distance, 15u);
      dist <<= (pos & 0x08) ? 4 : 0;
      dists[pos & 0x07] |= dist;
    }
  }
  
  void reset_distance(int pos) noexcept
  {
    INDIVI_UTABLE_ASSERT(pos < 16);
    dists[pos & 0x07] &= (pos & 0x08) ? 0x0F : 0xF0;
  }
  
  bool has_hfrag(int pos) const noexcept
  {
    INDIVI_UTABLE_ASSERT(pos < 16);
    return hfrags[pos];
  }
  
  unsigned char get_hfrag(int pos) const noexcept
  {
    INDIVI_UTABLE_ASSERT(pos < 16);
    return hfrags[pos];
  }
  
  void set_hfrag(int pos, std::size_t hash) noexcept
  {
    INDIVI_UTABLE_ASSERT(pos < 16);
    hfrags[pos] = (unsigned char)match_word(hash);
  }
  
  void reset_hfrag(int pos) noexcept
  {
    INDIVI_UTABLE_ASSERT(pos < 16);
    hfrags[pos] = 0;
  }
};

/*
 * Underlying class of `flat_umap` and `flat_uset`.
 */
template<
  class Key,
  class T,
  class value_type,
  class item_type,
  class size_type,
  class Hash,
  class KeyEqual >
class flat_utable
{
public:
  using key_type = Key;
  using mapped_type = T;
  using difference_type = typename std::make_signed<size_type>::type;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using iter_reference = typename std::conditional<std::is_same<key_type, value_type>::value, const key_type&, value_type&>::type;
  using iter_const_reference = const value_type&; // for flat_uset, iter_reference is also const
  using iter_pointer = value_type*;
  using iter_const_pointer = const value_type*;

private:
  using init_type = item_type;
  using insert_type = std::pair<typename std::remove_const<Key>::type, const mapped_type>;
  using storage_type = typename std::aligned_storage<sizeof(item_type), alignof(item_type)>::type;

  static constexpr float MAX_LOAD_FACTOR{ 0.875f };
  static constexpr unsigned int MIN_CAPA{ 2u };

  static constexpr bool is_pow2(std::size_t v)  noexcept { return (v & (v - 1)) == 0u; }

  static_assert(MAX_LOAD_FACTOR > 0.f && MAX_LOAD_FACTOR <= 1.f, "flat_utable: MAX_LOAD_FACTOR must be > 0 and <= 1");
  static_assert(MIN_CAPA >= 2u, "flat_utable: MIN_CAPA must be >= 2");
  static_assert(is_pow2(MIN_CAPA), "flat_utable: MIN_CAPA must be a power of 2");

#ifdef INDIVI_FLAT_U_STATS
  struct MFindStats
  {
    std::size_t find_hit_count = 0;
    std::size_t find_miss_count = 0;
    std::size_t prob_hit_len = 0;
    std::size_t prob_hit_max = 0;
    std::size_t prob_miss_len = 0;
    std::size_t prob_miss_max = 0;
    std::size_t cmp_hit = 0;
    std::size_t cmp_hit_max = 0;
    std::size_t cmp_miss = 0;
    std::size_t cmp_miss_max = 0;
  };
  mutable MFindStats mStats;
#endif

  struct no_mix
  {
    template< typename H, typename V >
    static inline std::size_t mix(const H& hasher, const V& v)
    {
      return hasher(v);
    }
  };
  struct bit_mix
  {
    template< typename H, typename V >
    static inline std::size_t mix(const H& hasher, const V& v)
    {
      std::size_t hash = hasher(v);
    #ifdef INDIVI_ARCH_64
      constexpr uint64_t phi = UINT64_C(0x9E3779B97F4A7C15);
      return wyhash::mix(hash, phi);
    #else // 32-bits assumed
      // from https://arxiv.org/abs/2001.05304
      constexpr uint32_t multiplier = UINT32_C(0xE817FB2D);
      return wyhash::mix32(hash, multiplier);
    #endif
    }
  };
  using mixer = typename std::conditional<hash_is_avalanching<Hash>::value, no_mix, bit_mix>::type;

  struct Location
  {
    item_type* value;
    const MetaGroup* group;
    int subIndex;
  };

  struct Groups : Hash // empty base optim
  {
    MetaGroup* data = MetaGroup::empty_group(); // static const dummy

    Groups(const Hash& hash) : Hash(hash) {}
    Groups(const Groups& groups) : Hash(groups) {}
    Groups(Groups&& groups) noexcept(std::is_nothrow_move_constructible<Hash>::value)
      : Hash(std::move(groups.hash_move())), data(groups.data) {}

    Hash& hash() noexcept { return *this; }
    const Hash& hash() const noexcept { return *this; }
    Hash&& hash_move() noexcept { return std::move(hash()); }
  };

  struct Values : KeyEqual // empty base optim
  {
    item_type* data = nullptr;

    Values(const KeyEqual& equal) : KeyEqual(equal) {}
    Values(const Values& values) : KeyEqual(values) {}
    Values(Values&& values) noexcept(std::is_nothrow_move_constructible<KeyEqual>::value)
      : KeyEqual(std::move(values.equal_move())), data(values.data) {}

    KeyEqual& equal() noexcept { return *this; }
    const KeyEqual& equal() const noexcept { return *this; }
    KeyEqual&& equal_move() noexcept { return std::move(equal()); }

    value_type* cdata() const noexcept { return reinterpret_cast<value_type*>(data); }
  };

  // Members
  size_type mSize    = 0u;  // number of items
  size_type mGMask   = 0u;  // group index mask (as power-of-2 capacity - 1)
  size_type mMaxSize = 0u;  // max size before rehash is needed
  Groups mGroups;
  Values mValues;

  size_type group_capa() const noexcept { return mValues.data ? mGMask + 1u : 0u; }
  Hash& hash() noexcept { return mGroups.hash(); }
  const Hash& hash() const noexcept { return mGroups.hash(); }
  KeyEqual& equal() noexcept { return mValues.equal(); }
  const KeyEqual& equal() const noexcept { return mValues.equal(); }

public:
  template <typename Pointer, typename Reference>
  class Iterator
  {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = typename flat_utable::difference_type;
    using pointer = Pointer;
    using reference = Reference;

  private:
    friend flat_utable;

    int mSubIndex = 0u;
    const MetaGroup* mGroup = nullptr;
    const MetaGroup* mGroupFirst = nullptr;
    value_type* mValue = nullptr;

    Iterator(int subIndex, const MetaGroup* group, const MetaGroup* groupFirst, item_type* value)
      : mSubIndex(subIndex), mGroup(group), mGroupFirst(groupFirst), mValue(reinterpret_cast<value_type*>(value))
    {}

    static Iterator find_begin(const MetaGroup* groups, item_type* values, size_type gCapa)
    {
      return ++Iterator(0, groups + gCapa, groups, values + (gCapa * 16));
    }

  public:
    Iterator() = default;
    Iterator(const Iterator& other) = default;

    template <typename P, typename R,
      typename = std::enable_if<std::is_convertible<P, pointer>::value>>
      Iterator(const Iterator<P, R>& other)
      : mSubIndex(other.mSubIndex), mGroup(other.mGroup), mGroupFirst(other.mGroupFirst), mValue(other.mValue)
    {}

    Iterator& operator=(const Iterator& other) = default;

    Iterator& operator++() noexcept
    {
      // current group
      while (mSubIndex)
      {
        --mSubIndex;
        --mValue;
        if (mGroup->has_hfrag(mSubIndex))
          return *this;
      }
      // next group
      while (mGroup != mGroupFirst)
      {
        INDIVI_UTABLE_ASSERT(mGroup);
        --mGroup;
        int sets = mGroup->match_set();
        if (sets)
        {
          INDIVI_PREFETCH(mValue);
          int last = MetaGroup::uc_last_set(sets);
          mSubIndex = last;
          mValue -= 16 - last;
          return *this;
        }
        mValue -= 16;
      }
      // end
      mValue = nullptr;
      return *this;
    }

    Iterator operator++(int) noexcept
    {
      Iterator retval = *this;
      ++(*this);
      return retval;
    }

    bool operator==(const Iterator& other) const noexcept
    {
      return mValue == other.mValue;
    }

    bool operator!=(const Iterator& other) const noexcept
    {
      return mValue != other.mValue;
    }

    reference operator*() const noexcept
    {
      return *mValue;
    }

    pointer operator->() const noexcept
    {
      return mValue;
    }
  };

  using iterator = Iterator<iter_pointer, iter_reference>;
  using const_iterator = Iterator<iter_const_pointer, iter_const_reference>;

  // Ctr/Dtr
  flat_utable() : flat_utable(0)
  {}

  explicit flat_utable(size_type bucket_count, const Hash& hash = Hash(), const key_equal& equal = key_equal())
    : mGroups(hash)
    , mValues(equal)
  {
    rehash(bucket_count);
  }

  template< class InputIt >
  flat_utable(InputIt first, InputIt last, size_type bucket_count = 0, const Hash& hash = Hash(), const key_equal& equal = key_equal())
    : flat_utable(bucket_count, hash, equal)
  {
      insert(first, last);
  }

  flat_utable(const std::initializer_list<value_type>& init, size_type bucket_count = 0, const Hash& hash = Hash(), const key_equal& equal = key_equal())
    : flat_utable(init.begin(), init.end(), bucket_count, hash, equal)
  {}

  flat_utable(const flat_utable& other)
    : mGroups(other.mGroups)
    , mValues(other.mValues)
  {
    copy_content(other);
  }

  flat_utable(flat_utable&& other)
    noexcept(std::is_nothrow_move_constructible<Groups>::value && std::is_nothrow_move_constructible<Values>::value)
    : mSize(other.mSize)
    , mGMask(other.mGMask)
    , mMaxSize(other.mMaxSize)
    , mGroups(std::move(other.mGroups))
    , mValues(std::move(other.mValues))
  {
    other.mSize = 0u;
    other.mGMask = 0u;
    other.mMaxSize = 0u;
    other.mGroups.data = MetaGroup::empty_group();
    other.mValues.data = nullptr;
  }

  ~flat_utable()
  {
    destroy();
  }

  // Assignment
  void operator=(const flat_utable& other)
  {
    if (this != std::addressof(other))
    {
      clear();

      hash()  = other.hash();
      equal() = other.equal(); // if this throws, state might be inconsistent

      copy_content(other);
    }
  }

  void operator=(flat_utable&& other) noexcept(std::is_nothrow_move_assignable<Hash>::value && std::is_nothrow_move_assignable<KeyEqual>::value)
  {
    if (this != std::addressof(other))
    {
      destroy();
      mSize = 0u;
      mGMask = 0u;
      mMaxSize = 0u;
      mGroups.data = MetaGroup::empty_group();
      mValues.data = nullptr;

      hash()  = std::move(other.hash());
      equal() = std::move(other.equal()); // if this throws, state might be inconsistent

      mSize = other.mSize;
      mGMask = other.mGMask;
      mMaxSize = other.mMaxSize;
      mGroups.data = other.mGroups.data;
      mValues.data = other.mValues.data;

      other.mSize = 0u;
      other.mGMask = 0u;
      other.mMaxSize = 0u;
      other.mGroups.data = MetaGroup::empty_group();
      other.mValues.data = nullptr;
    }
  }

  // Iterators
  iterator begin() noexcept { return iterator::find_begin(mGroups.data, mValues.data, group_capa()); }
  const_iterator begin() const noexcept { return const_iterator::find_begin(mGroups.data, mValues.data, group_capa()); }
  const_iterator cbegin() const noexcept { return const_iterator::find_begin(mGroups.data, mValues.data, group_capa()); }

  static iterator end() noexcept { return iterator(); }
  static const_iterator cend() noexcept { return const_iterator(); }

  // Capacity
  bool empty() const noexcept { return mSize == 0u; }
  size_type size() const noexcept { return mSize; }
  size_type max_size() const noexcept { return (size_type)((double)max_bucket_count() * max_load_factor()); }

  // Bucket interface
  size_type bucket_count() const noexcept { return (mMaxSize > 16u) ? (mGMask + 1u) * 16 : mMaxSize; }
  size_type max_bucket_count() const noexcept { return (size_type)std::numeric_limits<difference_type>::max(); }

  // Hash policy
  float load_factor() const noexcept { return mSize ? float((double)mSize / (double)bucket_count()) : (float)mSize; }
  float max_load_factor() const noexcept { return MAX_LOAD_FACTOR; }
  void max_load_factor(float) noexcept { /*for compatibility*/ }

  void rehash(size_type count)
  {
    INDIVI_UTABLE_ASSERT(count <= max_size());
    size_type minCapa = (size_type)std::ceil((float)size() / max_load_factor());
    count = std::max(count, minCapa);
    if (count)
    {
      count = std::max(count, (size_type)MIN_CAPA);
      count = std::min(count, max_bucket_count());
      count = round_up_pow2(count);
      if (count != bucket_count())
        rehash_impl(count);
    }
    else
    {
      destroy_empty();
      mSize = 0u;
      mGMask = 0u;
      mMaxSize = 0u;
      mGroups.data = MetaGroup::empty_group();
      mValues.data = nullptr;
    }
  }

  void reserve(size_type count)
  {
    INDIVI_UTABLE_ASSERT(count <= max_size());
    size_type bucketCount = (count > 16) ? (size_type)std::ceil((float)count / max_load_factor()) : count;
    rehash(bucketCount);
  }

  // Obervers
  hasher hash_function() const { return hash(); }
  key_equal key_eq() const { return equal(); }

  // Lookup
  mapped_type& at(const Key& key)
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    if (!loc.value)
      //throw std::out_of_range("flat_utable::at");
  
    return loc.value->second;
  }
  const mapped_type& at(const Key& key) const
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    if (!loc.value)
      //throw std::out_of_range("flat_utable::at");
  
    return loc.value->second;
  }

  mapped_type& operator[](const Key& key)
  {
    return try_emplace_key_impl(key);
  }
  mapped_type& operator[](Key&& key)
  {
    return try_emplace_key_impl(std::move(key));
  }

  size_type count(const Key& key) const
  {
    return contains(key);
  }

  bool contains(const Key& key) const
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    return loc.value != nullptr;
  }

  iterator find(const Key& key)
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    return { loc.subIndex, loc.group, mGroups.data, loc.value };
  }
  const_iterator find(const Key& key) const
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    return { loc.subIndex, loc.group, mGroups.data, loc.value };
  }

  // Modifiers
  void clear() noexcept
  {
    INDIVI_UTABLE_ASSERT(mValues.data || mMaxSize == 0u);
    if (mValues.data)
    {
      uc_for_each([&](item_type* pValue) {
        pValue->~item_type();
      });
      std::memset((void*)mGroups.data, 0, sizeof(MetaGroup) * (mGMask + 1u));
      mSize = 0u;
    }
  }

  template< class P >
  std::pair<iterator, bool> insert(P&& value)
  {
    return insert_impl(std::forward<P>(value));
  }

  std::pair<iterator, bool> insert(init_type&& value)
  {
    return insert_impl(std::move(value));
  }

  template< class InputIt >
  void insert(InputIt first, InputIt last)
  {
    for (; first != last; ++first)
      emplace(*first);
  }

  template <typename U>
  typename std::enable_if<
    std::is_same<U, value_type>::value || std::is_same<U, item_type>::value,
    void >::type
  insert_list(const std::initializer_list<U>& ilist)
  {
    insert(ilist.begin(), ilist.end());
  }

  template< class M >
  std::pair<iterator, bool> insert_or_assign(const Key& key, M&& obj)
  {
    return insert_or_assign_impl(key, std::forward<M>(obj));
  }

  template< class M >
  std::pair<iterator, bool> insert_or_assign(Key&& key, M&& obj)
  {
    return insert_or_assign_impl(std::move(key), std::forward<M>(obj));
  }

  template< class... Args >
  std::pair<iterator, bool> emplace(Args&&... args)
  {
    return try_insert_impl(item_type(std::forward<Args>(args)...));
  }

  template <typename P>
  typename std::enable_if<
    traits::is_similar<P, value_type>::value || traits::is_similar<P, init_type>::value || traits::is_similar<P, insert_type>::value,
    std::pair<iterator, bool> >::type
  emplace(P&& value)
  {
    return try_insert_impl(std::forward<P>(value));
  }

  template< class... Args >
  std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args)
  {
    return try_emplace_impl(key, std::forward<Args>(args)...);
  }

  template< class... Args >
  std::pair<iterator, bool> try_emplace(Key&& key, Args&&... args)
  {
    return try_emplace_impl(std::move(key), std::forward<Args>(args)...);
  }

  iterator erase_(iterator pos)
  {
    INDIVI_UTABLE_ASSERT(is_dereferenceable(pos));
    iterator res = pos;
    ++res;

    erase_impl(reinterpret_cast<item_type*>(pos.mValue), const_cast<MetaGroup*>(pos.mGroup), pos.mSubIndex);
    return res;
  }
  iterator erase_(const_iterator pos)
  {
    INDIVI_UTABLE_ASSERT(is_dereferenceable(pos));
    iterator res = pos;
    ++res;

    erase_impl(reinterpret_cast<item_type*>(pos.mValue), const_cast<MetaGroup*>(pos.mGroup), pos.mSubIndex);
    return res;
  }
  // non-standard, see `erase_()`
  void erase(iterator pos)
  {
    INDIVI_UTABLE_ASSERT(is_dereferenceable(pos));
    erase_impl(reinterpret_cast<item_type*>(pos.mValue), const_cast<MetaGroup*>(pos.mGroup), pos.mSubIndex);
  }
  void erase(const_iterator pos)
  {
    INDIVI_UTABLE_ASSERT(is_dereferenceable(pos));
    erase_impl(reinterpret_cast<item_type*>(pos.mValue), pos.mGroup, pos.mSubIndex);
  }

  size_type erase(const Key& key)
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    if (loc.value)
    {
      erase_impl(gIndex, hash, loc);
      return 1u;
    }
    return 0u;
  }

  void swap(flat_utable& other)
    noexcept(traits::is_nothrow_swappable<Hash>::value && traits::is_nothrow_swappable<key_equal>::value)
  {
    using std::swap;
    swap(hash(),  other.hash());
    swap(equal(), other.equal());

    swap(mSize,        other.mSize);
    swap(mGMask,       other.mGMask);
    swap(mMaxSize,     other.mMaxSize);
    swap(mGroups.data, other.mGroups.data);
    swap(mValues.data, other.mValues.data);
  }

  template <typename U>
  typename std::enable_if<
    !std::is_same<typename U::key_type, typename U::init_type>::value,
    bool >::type
  equal(const U& other) const
  {
    if (mSize != other.mSize)
      return false;

    auto itEnd = cend();
    auto otEnd = other.cend();
    for (auto it = cbegin(); it != itEnd; ++it)
    {
      auto ot = other.find(get_key(*it));
      if (ot == otEnd || it->second != ot->second)
        return false;
    }
    return true;
  }

  template <typename U>
  typename std::enable_if<
    std::is_same<typename U::key_type, typename U::init_type>::value,
    bool >::type
  equal(const U& other) const
  {
    if (mSize != other.mSize)
      return false;

    auto end_ = cend();
    for (auto it = cbegin(); it != end_; ++it)
    {
      if (!other.contains(*it))
        return false;
    }
    return true;
  }

  template< class Pred >
  size_type erase_if(Pred& pred)
  {
    size_type oldSize = mSize;
    iterator end_ = end();
    for (iterator it = begin(); it != end_; ++it)
    {
      if (pred(*it))
        erase(it);
    }
    return oldSize - mSize;
  }

#ifdef INDIVI_FLAT_U_DEBUG
  bool is_cleared() const noexcept
  {
    if (mValues.data)
    {
      for (size_type i = 0; i < group_capa(); ++i)
      {
        const auto& group = mGroups.data[i];
        for (int j = 0; j < 16; ++j)
        {
          if (group.hfrags[j] != 0)
            return false;
        }
        for (int j = 0; j < 8; ++j)
        {
          if (group.oflws[j] != 0)
            return false;
          if (group.dists[j] != 0)
            return false;
        }
      }
    }
    return true;
  }
#endif
#ifdef INDIVI_FLAT_U_STATS
  struct GroupStats
  {
    float full_grp_avg = 0.f;
    float overflow_avg = 0.f;
    float overflow_nzero_avg = 0.f;
    int overflow_max = 0;
    int overflow_saturated = 0;
    float dist_avg = 0.f;
    float dist_nzero_avg = 0.f;
    int dist_max = 0;
    int dist_saturated = 0;
  };

  GroupStats get_group_stats() const noexcept
  {
    GroupStats stats;
    if (empty())
      return stats;

    std::size_t full_count    = 0u;
    int dist_max              = 0;
    int dist_sat              = 0;
    std::size_t dist_sum      = 0u;
    std::size_t dist_nz_sum   = 0u;
    std::size_t dist_nz_count = 0u;
    int oflw_max              = 0;
    int oflw_sat              = 0;
    std::size_t oflw_sum      = 0u;
    std::size_t oflw_nz_sum   = 0u;
    std::size_t oflw_nz_count = 0u;

    std::size_t grp_count = mGMask + 1;
    MetaGroup* pGroup = mGroups.data;
    MetaGroup* last = pGroup + grp_count;
    do {
      int grp_size = 0;
      for (int i = 0; i < 16; ++i)
      {
        if (pGroup->has_hfrag(i))
        {
          ++grp_size;
          unsigned char dist = pGroup->get_distance(i);
          dist_sum += dist;
          dist_nz_sum += (dist == 0) ? 0 : dist;
          dist_nz_count += (dist > 0);
          dist_max = (dist_max >= dist) ? dist_max : dist;
          dist_sat += (dist == 15);
        }

        unsigned char overflow = pGroup->get_overflow((std::size_t)i);
        oflw_sum += overflow;
        oflw_nz_sum += (overflow == 0) ? 0 : overflow;
        oflw_nz_count += (overflow > 0);
        oflw_max = (oflw_max >= overflow) ? oflw_max : overflow;
        oflw_sat += (overflow == 255);
      }
      full_count += (grp_size == 16);

      ++pGroup;
    }
    while (pGroup != last);

    // compute
    stats.full_grp_avg = (float)full_count / grp_count;
    stats.overflow_avg = (float)oflw_sum / mSize;
    stats.overflow_nzero_avg = oflw_nz_count ? (float)oflw_nz_sum / oflw_nz_count : 0.f;
    stats.overflow_max = oflw_max;
    stats.overflow_saturated = oflw_sat;
    stats.dist_avg = (float)dist_sum / mSize;
    stats.dist_nzero_avg = dist_nz_count ? (float)dist_nz_sum / dist_nz_count : 0.f;
    stats.dist_max = dist_max;
    stats.dist_saturated = dist_sat;

    return stats;
  }

  struct FindStats
  {
    std::size_t find_hit_count = 0;
    std::size_t find_miss_count = 0;
    float prob_len_hit_avg = 0.f;
    std::size_t prob_len_hit_max = 0;
    float prob_len_miss_avg = 0.f;
    std::size_t prob_len_miss_max = 0;
    float compare_hit_avg = 0.f;
    std::size_t compare_hit_max = 0;
    float compare_miss_avg = 0.f;
    std::size_t compare_miss_max = 0;
  };

  FindStats get_find_stats() const noexcept
  {
    FindStats stats;

    stats.find_hit_count = mStats.find_hit_count;
    stats.find_miss_count = mStats.find_miss_count;

    stats.prob_len_hit_avg = mStats.find_hit_count ? (float)mStats.prob_hit_len / mStats.find_hit_count : 0.f;
    stats.prob_len_hit_max = mStats.prob_hit_max;
    stats.prob_len_miss_avg = mStats.find_miss_count ? (float)mStats.prob_miss_len / mStats.find_miss_count : 0.f;
    stats.prob_len_miss_max = mStats.prob_miss_max;

    stats.compare_hit_avg = mStats.find_hit_count ? (float)mStats.cmp_hit / mStats.find_hit_count : 0.f;
    stats.compare_hit_max = mStats.cmp_hit_max;
    stats.compare_miss_avg = mStats.find_miss_count ? (float)mStats.cmp_miss / mStats.find_miss_count : 0.f;
    stats.compare_miss_max = mStats.cmp_miss_max;

    return stats;
  }

  void reset_find_stats() noexcept
  {
    mStats.find_hit_count = 0;
    mStats.find_miss_count = 0;

    mStats.prob_hit_len = 0;
    mStats.prob_hit_max = 0;
    mStats.prob_miss_len = 0;
    mStats.prob_miss_max = 0;

    mStats.cmp_hit = 0;
    mStats.cmp_hit_max = 0;
    mStats.cmp_miss = 0;
    mStats.cmp_miss_max = 0;
  }
#endif // INDIVI_FLAT_U_STATS

private:
  // static functions
  static constexpr uint32_t round_up_pow2(uint32_t v) noexcept
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

  static constexpr uint64_t round_up_pow2(uint64_t v) noexcept
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

  static constexpr size_type hash_position(uint32_t hash, size_type mask) noexcept
  {
    // hfrag uses low byte and bit_mix multiplication has more entropy in high bytes
    // so for position we use the following mix: 3210 -> 1032
    uint32_t position = (hash >> 16) | (hash << 16);

    return (size_type)(position & mask); // mod as pow2 mask
  }

  static constexpr size_type hash_position(uint64_t hash, size_type mask) noexcept
  {
    // hfrag uses low byte and bit_mix multiplication has more entropy in high bytes
    // so for position we use the following mix: 7654 3210 -> 3210 5476
    // it gives good balance between speed and randomness
    uint32_t high = (uint32_t)(hash >> 32);
    high = (high >> 16) | (high << 16);
    uint64_t position = (uint64_t)high | (hash << 32);

    return (size_type)position & mask; // mod as pow2 mask
  }
#ifndef INDIVI_FLAT_U_QUAD_PROB
  static constexpr size_type prob_delta(std::size_t hash)
  {
    // use double-hashing based on hfrag so erase can rewind
    // capacity is a power of 2 so all odd numbers are ok (coprime)
    hash &= 0xFF;
    hash = (hash != 0) ? hash : (std::size_t)8u;
    return (size_type)(hash | 0x01);
  }
#endif
  template< typename P >
  const Key& get_key(const P& value) const noexcept
  {
    return value.first;
  }

  const Key& get_key(const Key& value) const noexcept
  {
    return value;
  }

  // member functions
  bool is_dereferenceable(iterator pos) noexcept
  {
    INDIVI_UTABLE_ASSERT(mSize != 0u);
    INDIVI_UTABLE_ASSERT(mValues.data);
    INDIVI_UTABLE_ASSERT(pos.mGroup);
    INDIVI_UTABLE_ASSERT(pos.mValue);
    INDIVI_UTABLE_ASSERT(pos.mSubIndex >= 0 && pos.mSubIndex < 16);
    INDIVI_UTABLE_ASSERT(pos.mGroupFirst == mGroups.data);
    INDIVI_UTABLE_ASSERT(pos.mValue >= mValues.cdata() && pos.mValue < mValues.cdata() + bucket_count());
    return true;
  }

  std::size_t get_hash(const Key& key) const
  {
    return mixer::mix(hash(), key);
  }

  template< typename F >
  void uc_for_each(F fct) const
  {
    INDIVI_UTABLE_ASSERT(mValues.data);
    item_type* pValue = mValues.data;

    MetaGroup* pGroup = mGroups.data;
    MetaGroup* last = pGroup + mGMask + 1;
    do {
      int idx = 0;
      int sets = pGroup->match_set();
      while (sets)
      {
        if (sets & 0x01)
          fct(&pValue[idx]);

        sets >>= 1;
        ++idx;
      }
      ++pGroup;
      pValue += 16;
    }
    while (pGroup != last);
  }

  template< typename F >
  void uc_each_while(F fct) const
  {
    INDIVI_UTABLE_ASSERT(mValues.data);
    item_type* pValue = mValues.data;

    MetaGroup* pGroup = mGroups.data;
    MetaGroup* last = pGroup + mGMask + 1;
    do {
      int idx = 0;
      int sets = pGroup->match_set();
      while (sets)
      {
        if (sets & 0x01)
        {
          if (!fct(&pValue[idx]))
            return;
        }
        sets >>= 1;
        ++idx;
      }
      ++pGroup;
      pValue += 16;
    }
    while (pGroup != last);
  }

  void destroy()
  {
    if (mValues.data)
    {
      uc_for_each([&](item_type* pValue) {
        pValue->~item_type();
      });
      delete[] reinterpret_cast<storage_type*>(mValues.data);
    }
  }
  
  void destroy_empty()
  {
    INDIVI_UTABLE_ASSERT(empty());
    if (mValues.data)
      delete[] reinterpret_cast<storage_type*>(mValues.data);
  }

  Location find_impl(std::size_t hash, size_type gIndex, const Key& key) const
  {
  #ifdef INDIVI_FLAT_U_STATS
    std::size_t probLen = 1;
    std::size_t cmpCount = 0;
  #endif
  #ifdef INDIVI_FLAT_U_QUAD_PROB
    size_type delta = 0u;
  #endif
    do {
      const auto& group = mGroups.data[gIndex];
      int matchs = group.match_hfrag(hash);
      if (matchs)
      {
        item_type* pValue = &mValues.data[gIndex * 16];
        INDIVI_PREFETCH(pValue);
        do {
        #ifdef INDIVI_FLAT_U_STATS
          ++cmpCount;
        #endif
          int idx = MetaGroup::uc_ctz(matchs);
          if (equal()(key, get_key(pValue[idx]))) // found
          {
          #ifdef INDIVI_FLAT_U_STATS
            mStats.prob_hit_len += probLen;
            mStats.prob_hit_max = (mStats.prob_hit_max >= probLen) ? mStats.prob_hit_max : probLen;
            mStats.cmp_hit += cmpCount;
            mStats.cmp_hit_max = (mStats.cmp_hit_max >= cmpCount) ? mStats.cmp_hit_max : cmpCount;
            ++mStats.find_hit_count;
          #endif
            return { pValue + idx, std::addressof(group), idx };
          }
          matchs &= matchs - 1; // remove match
        }
        while (matchs);
      }
      // not found
      if (!group.get_overflow(hash))
      {
      #ifdef INDIVI_FLAT_U_STATS
        mStats.prob_miss_len += probLen;
        mStats.prob_miss_max = (mStats.prob_miss_max >= probLen) ? mStats.prob_miss_max : probLen;
        mStats.cmp_miss += cmpCount;
        mStats.cmp_miss_max = (mStats.cmp_miss_max >= cmpCount) ? mStats.cmp_miss_max : cmpCount;
        ++mStats.find_miss_count;
      #endif
        return { nullptr, nullptr, 0 };
      }
    #ifdef INDIVI_FLAT_U_STATS
      ++probLen;
    #endif
    #ifdef INDIVI_FLAT_U_QUAD_PROB
      gIndex = (gIndex + (++delta)) & mGMask;
    #else
      gIndex += prob_delta(hash);
      gIndex &= mGMask;
    #endif
    }
    while (gIndex <= mGMask); // non-infinite loop helps optimization

    return { nullptr, nullptr, 0 };
  }

  template< typename U >
  Location unchecked_insert(std::size_t hash, size_type gIndex, U&& value)
  {
    uint32_t step = 0u;
  #ifdef INDIVI_FLAT_U_QUAD_PROB
    size_type delta = 0u;
  #endif
    do {
      auto& group = mGroups.data[gIndex];
      int empties = group.match_empty();
      if (empties) // not full
      {
        int idx = MetaGroup::uc_ctz(empties);
        item_type* pValue = &mValues.data[(gIndex * 16) + idx];
        ::new (pValue) item_type(std::forward<U>(value));
        ++mSize;

        group.set_hfrag(idx, hash);
        group.set_distance(idx, step);
        return { pValue, std::addressof(group), idx};
      }
      // set overflow flag
      group.inc_overflow(hash);

    #ifdef INDIVI_FLAT_U_QUAD_PROB
      gIndex = (gIndex + (++delta)) & mGMask;
    #else
      gIndex += prob_delta(hash);
      gIndex &= mGMask;
    #endif
      ++step;
    }
    while (true);
  }

  template< typename U, class... Args >
  Location unchecked_emplace(std::size_t hash, size_type gIndex, U&& key, Args&&... args)
  {
    uint32_t step = 0u;
  #ifdef INDIVI_FLAT_U_QUAD_PROB
    size_type delta = 0u;
  #endif
    do {
      auto& group = mGroups.data[gIndex];
      int empties = group.match_empty();
      if (empties) // not full
      {
        int idx = MetaGroup::uc_ctz(empties);
        item_type* pValue = &mValues.data[(gIndex * 16) + idx];
        ::new (pValue) item_type(std::piecewise_construct,
                                 std::forward_as_tuple(std::forward<U>(key)),
                                 std::forward_as_tuple(std::forward<Args>(args)...));
        ++mSize;

        group.set_hfrag(idx, hash);
        group.set_distance(idx, step);
        return { pValue, std::addressof(group), idx};
      }
      // set overflow flag
      group.inc_overflow(hash);

    #ifdef INDIVI_FLAT_U_QUAD_PROB
      gIndex = (gIndex + (++delta)) & mGMask;
    #else
      gIndex += prob_delta(hash);
      gIndex &= mGMask;
    #endif
      ++step;
    }
    while (true);
  }

  std::pair<iterator, bool> insert_impl(const init_type& value) { return try_insert_impl(value); }

  std::pair<iterator, bool> insert_impl(init_type&& value) { return try_insert_impl(std::move(value)); }

  template< typename = void > // resolve ambiguities with init_type
  std::pair<iterator, bool> insert_impl(const value_type& value) { return try_insert_impl(value); }

  template< typename = void >
  std::pair<iterator, bool> insert_impl(value_type&& value) { return try_insert_impl(std::move(value)); }

  template< typename U >
  std::pair<iterator, bool> try_insert_impl(U&& value)
  {
    std::size_t hash = get_hash(get_key(value));
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, get_key(value));
    if (loc.value != nullptr) // already exist
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), false };

    if (mSize < mMaxSize)
    {
      loc = unchecked_insert(hash, gIndex, std::forward<U>(value));
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), true };
    }
    else // need rehash
    {
      loc = grow_with_insert(hash, std::forward<U>(value));
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), true };
    }
  }

  template< typename U, class... Args >
  std::pair<iterator, bool> try_emplace_impl(U&& key, Args&&... args)
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    if (loc.value != nullptr) // already exist
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), false };

    if (mSize < mMaxSize)
    {
      loc = unchecked_emplace(hash, gIndex, std::forward<U>(key), std::forward<Args>(args)...);
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), true };
    }
    else // need rehash
    {
      loc = grow_with_emplace(hash, std::forward<U>(key), std::forward<Args>(args)...);
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), true };
    }
  }

  template< typename U >
  mapped_type& try_emplace_key_impl(U&& key)
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    if (loc.value != nullptr) // already exist
      return loc.value->second;

    if (mSize < mMaxSize)
    {
      loc = unchecked_emplace(hash, gIndex, std::forward<U>(key));
      return loc.value->second;
    }
    else // need rehash
    {
      loc = grow_with_emplace(hash, std::forward<U>(key));
      return loc.value->second;
    }
  }

  template< typename U, class M >
  std::pair<iterator, bool> insert_or_assign_impl(U&& key, M&& obj)
  {
    std::size_t hash = get_hash(key);
    size_type gIndex = hash_position(hash, mGMask);

    Location loc = find_impl(hash, gIndex, key);
    if (loc.value != nullptr) // already exist
    {
      loc.value->second = std::forward<M>(obj);
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), false };
    }

    if (mSize < mMaxSize)
    {
      loc = unchecked_emplace(hash, gIndex, std::forward<U>(key), std::forward<M>(obj));
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), true };
    }
    else // need rehash
    {
      loc = grow_with_emplace(hash, std::forward<U>(key), std::forward<M>(obj));
      return { iterator(loc.subIndex, loc.group, mGroups.data, loc.value), true };
    }
  }

  template< typename U >
  void insert_unique(MetaGroup* groups, item_type* values, size_type gMask, U&& value)
  {
    std::size_t hash = get_hash(get_key(value));
    size_type gIndex = hash_position(hash, gMask);

    uint32_t step = 0u;
  #ifdef INDIVI_FLAT_U_QUAD_PROB
    size_type delta = 0u;
  #endif
    do {
      auto& group = groups[gIndex];
      int empties = group.match_empty();
      if (empties) // not full
      {
        int idx = MetaGroup::uc_ctz(empties);
        item_type* pValue = &values[(gIndex * 16) + idx];
        ::new (pValue) item_type(std::forward<U>(value));

        group.set_hfrag(idx, hash);
        group.set_distance(idx, step);
        return;
      }
      // full
      group.inc_overflow(hash);

    #ifdef INDIVI_FLAT_U_QUAD_PROB
      gIndex = (gIndex + (++delta)) & gMask;
    #else
      gIndex += prob_delta(hash);
      gIndex &= gMask;
    #endif
      ++step;
    }
    while (true);
  }

  template< typename U >
  void insert_unique(U&& value)
  {
    insert_unique(mGroups.data, mValues.data, mGMask, std::forward<U>(value));
  }

  template< typename U >
  Location insert_first(MetaGroup* groups, item_type* values, size_type gMask, std::size_t hash, U&& value)
  {
    size_type gIndex = hash_position(hash, gMask);

    auto& group = groups[gIndex];
    item_type* pValue = &values[gIndex * 16];
    ::new (pValue) item_type(std::forward<U>(value));

    group.set_hfrag(0, hash);
    return { pValue, std::addressof(group), 0 };
  }

  template< typename U, class... Args >
  Location emplace_first(MetaGroup* groups, item_type* values, size_type gMask, std::size_t hash, U&& key, Args&&... args)
  {
    size_type gIndex = hash_position(hash, gMask);

    auto& group = groups[gIndex];
    item_type* pValue = &values[gIndex * 16];
    ::new (pValue) item_type(std::piecewise_construct,
                             std::forward_as_tuple(std::forward<U>(key)),
                             std::forward_as_tuple(std::forward<Args>(args)...));

    group.set_hfrag(0, hash);
    return { pValue, std::addressof(group), 0 };
  }

  void erase_impl(size_type gIndex, std::size_t hash, const Location& loc)
  {
  #ifdef INDIVI_FLAT_U_QUAD_PROB
    size_type delta = 0u;
  #endif
    do {
      auto& group = mGroups.data[gIndex];
      if (&group == loc.group)
      {
        group.reset_hfrag(loc.subIndex);
        group.reset_distance(loc.subIndex);

        loc.value->~item_type();
        --mSize;

        return;
      }
      group.dec_overflow(hash);

    #ifdef INDIVI_FLAT_U_QUAD_PROB
      gIndex = (gIndex + (++delta)) & mGMask;
    #else
      gIndex += prob_delta(hash);
      gIndex &= mGMask;
    #endif
    }
    while (true);
  }

  void erase_impl(item_type* value, MetaGroup* group, int subIndex)
  {
    unsigned int dist = group->get_distance(subIndex);
    if (dist < 15u) // valid distance
    {
      // erase entry
      std::size_t hfrag = group->get_hfrag(subIndex);
      group->reset_hfrag(subIndex);

      value->~item_type();
      --mSize;

      // decrement overflow counters
      if (dist > 0u)
      {
        size_type gIndex = (size_type)(group - mGroups.data);
        group->reset_distance(subIndex);
      #ifndef INDIVI_FLAT_U_QUAD_PROB
        size_type delta = prob_delta(hfrag);
      #endif
        do {
        #ifdef INDIVI_FLAT_U_QUAD_PROB
          gIndex -= dist;
        #else
          gIndex -= delta;
        #endif
          gIndex &= mGMask;
          mGroups.data[gIndex].dec_overflow(hfrag);
        }
        while (--dist);
      }
    }
    else // saturated distance
    {
      // fallback: recompute hash
      std::size_t hash = get_hash(get_key(*value));
      size_type gIndex = hash_position(hash, mGMask);

      erase_impl(gIndex, hash, { value, group, subIndex });
    }
  }

  void fast_copy(const flat_utable& other)
  {
    INDIVI_UTABLE_ASSERT(empty());
    INDIVI_UTABLE_ASSERT(!other.empty());
    INDIVI_UTABLE_ASSERT(mValues.data);
    INDIVI_UTABLE_ASSERT(mGMask == other.mGMask);
    INDIVI_UTABLE_ASSERT(bucket_count() == other.bucket_count());
    size_type bucketCount = bucket_count();

    if (std::is_trivially_copy_constructible<item_type>::value)
    {
      std::memcpy((void*)mValues.data, other.mValues.data, sizeof(item_type) * bucketCount);
      std::memcpy((void*)mGroups.data, other.mGroups.data, sizeof(MetaGroup) * (mGMask + 1u));
      mSize = other.mSize;
    }
    else
    {
      size_type ctr_count = 0u;
        other.uc_for_each([&](const item_type* otValue) {
          ::new (mValues.data + (otValue - other.mValues.data)) item_type(*otValue);
          ++ctr_count;
        });
      INDIVI_UTABLE_ASSERT(ctr_count == other.mSize);

      std::memcpy(mGroups.data, other.mGroups.data, sizeof(MetaGroup) * (mGMask + 1u));
      mSize = other.mSize;
    }
  }

  void copy_content(const flat_utable& other)
  {
    INDIVI_UTABLE_ASSERT(empty());
    if (other.empty())
      return;

    reserve(other.mSize);
    
      if (mMaxSize == other.mMaxSize) // same bucket count
      {
        fast_copy(other);
      }
      else
      {
        other.uc_for_each([&](const item_type* pValue) {
          insert_unique(*pValue);
          ++mSize;
        });
      }
  }

  void move_to(MetaGroup* newGroups, item_type* newValues, size_type newGMask)
  {
      uc_for_each([&](item_type* pValue) {
        insert_unique(newGroups, newValues, newGMask, std::move(*pValue));
        pValue->~item_type();
      });
  }

  struct NewStorage // exception-safe helper
  {
    size_type itemsCapa;
    std::unique_ptr<storage_type[]> data; // contains items then 32-aligned groups

    NewStorage(size_type itemsCapa_, size_type groupsCapa_)
      : itemsCapa(itemsCapa_)
      , data(new storage_type[storageCapa(itemsCapa_, groupsCapa_)])
    {
      void* ptr = (void*)(data.get() + itemsCapa_);
      std::memset(ptr, 0, sizeof(MetaGroup) * groupsCapa_ + 31); // init MetaGroups manually
    }

    static size_type storageCapa(size_type itemsCapa, size_type groupsCapa)
    {
      size_type grpsAsItemCapa = sizeof(MetaGroup) * groupsCapa + 31; // padding for alignment
      grpsAsItemCapa = (grpsAsItemCapa + sizeof(item_type) - 1) / sizeof(item_type); // round-up
      return itemsCapa + grpsAsItemCapa; // storage uses item_type element size
    }

    void release() noexcept { data.release(); }

    item_type* values() const noexcept { return reinterpret_cast<item_type*>(data.get()); }
    MetaGroup* groups() const noexcept
    {
      std::size_t space = 31 + sizeof(MetaGroup);
      void* ptr = (void*)(data.get() + itemsCapa);
      void* aligned = std::align(32, sizeof(MetaGroup), ptr, space);
      INDIVI_UTABLE_ASSERT(aligned);
      return reinterpret_cast<MetaGroup*>(aligned);
    }
  };

  void rehash_impl(size_type newCapa)
  {
    INDIVI_UTABLE_ASSERT(newCapa >= MIN_CAPA);
    INDIVI_UTABLE_ASSERT(is_pow2(newCapa));

    size_type newGCapa = newCapa / 16;
    newGCapa = std::max(newGCapa, (size_type)1u);
    size_type newGMask = newGCapa - 1u;

    if (mValues.data)
    {
      NewStorage newStorage(newCapa, newGCapa);
      MetaGroup* newGroups = newStorage.groups();
      item_type* newValues = newStorage.values();

      // move existing
      move_to(newGroups, newValues, newGMask);

      // delete old and update
      delete[] reinterpret_cast<storage_type*>(mValues.data);
      mGroups.data = newGroups;
      mValues.data = newValues;
      newStorage.release();

      mGMask = newGMask;
      mMaxSize = (mGMask != 0) ? (size_type)((double)newCapa * MAX_LOAD_FACTOR) : newCapa;
    }
    else // first time
    {
      INDIVI_UTABLE_ASSERT(mSize == 0u);
      INDIVI_UTABLE_ASSERT(mGMask == 0u);

      NewStorage newStorage(newCapa, newGCapa);
      mGroups.data = newStorage.groups();
      mValues.data = newStorage.values();
      newStorage.release();

      mGMask = newGMask;
      mMaxSize = (mGMask != 0) ? (size_type)((double)newCapa * MAX_LOAD_FACTOR) : newCapa;
    }
  }

  template< typename U >
  Location grow_with_insert(std::size_t hash, U&& value)
  {
    size_type newCapa = bucket_count() * 2;
    newCapa = std::max(newCapa, (size_type)MIN_CAPA);
    INDIVI_UTABLE_ASSERT(is_pow2(newCapa));

    size_type newGCapa = newCapa / 16;
    newGCapa = std::max(newGCapa, (size_type)1u);
    size_type newGMask = newGCapa - 1u;

    NewStorage newStorage(newCapa, newGCapa);
    MetaGroup* newGroups = newStorage.groups();
    item_type* newValues = newStorage.values();

    // insert new value first (strong exception guarantee)
    Location loc = insert_first(newGroups, newValues, newGMask, hash, std::forward<U>(value));

    if (mValues.data)
    {
      // move existing
      move_to(newGroups, newValues, newGMask);

      // delete old
      delete[] reinterpret_cast<storage_type*>(mValues.data);
    }
    // update
    mGroups.data = newGroups;
    mValues.data = newValues;
    newStorage.release();

    mGMask = newGMask;
    mMaxSize = (mGMask != 0) ? (size_type)(newCapa * MAX_LOAD_FACTOR) : newCapa;
    ++mSize;

    return loc;
  }

  template< typename U, class... Args >
  Location grow_with_emplace(std::size_t hash, U&& key, Args&&... args)
  {
    size_type newCapa = bucket_count() * 2;
    newCapa = std::max(newCapa, (size_type)MIN_CAPA);
    INDIVI_UTABLE_ASSERT(is_pow2(newCapa));

    size_type newGCapa = newCapa / 16;
    newGCapa = std::max(newGCapa, (size_type)1u);
    size_type newGMask = newGCapa - 1u;

    NewStorage newStorage(newCapa, newGCapa);
    MetaGroup* newGroups = newStorage.groups();
    item_type* newValues = newStorage.values();

    // insert new value first (strong exception guarantee)
    Location loc = emplace_first(newGroups, newValues, newGMask, hash, std::forward<U>(key), std::forward<Args>(args)...);

    if (mValues.data)
    {
      // move existing
      move_to(newGroups, newValues, newGMask);

      // delete old
      delete[] reinterpret_cast<storage_type*>(mValues.data);
    }
    // update
    mGroups.data = newGroups;
    mValues.data = newValues;
    newStorage.release();

    mGMask = newGMask;
    mMaxSize = (mGMask != 0) ? (size_type)((double)newCapa * MAX_LOAD_FACTOR) : newCapa;
    ++mSize;

    return loc;
  }
};

} // namespace detail
} // namespace indivi

#endif // INDIVI_FLAT_UTABLE_H
