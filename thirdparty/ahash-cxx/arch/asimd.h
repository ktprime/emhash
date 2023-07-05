#pragma once

#include <ahash-cxx/arch/config.h>

#if AHASH_CXX_HAS_ASIMD_ACCELERATION
#  include <ahash-cxx/common.h>

namespace ahash
{
namespace asimd
{
struct VectorOperator
{
  using VecType = uint8x16_t;

  AHASH_CXX_ALWAYS_INLINE static constexpr size_t
  lanes (VecType)
  {
    return 16;
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  broadcast_from (uint8x16_t data)
  {
    return { data };
  }

  AHASH_CXX_ALWAYS_INLINE static uint8x16_t
  downcast (VecType data, size_t)
  {
    return data;
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  encode (VecType x, VecType y)
  {
    return vaesmcq_u8(vaeseq_u8(x, vdupq_n_u8(0))) ^ y;
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  decode (VecType x, VecType y)
  {
    return vaesimcq_u8(vaesdq_u8(x, vdupq_n_u8(0))) ^ y;
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  add_by_64s (VecType x, VecType y)
  {
    return vreinterpretq_u8_u64(
            vaddq_u64(vreinterpretq_u64_u8(x), vreinterpretq_u64_u8(y)));
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  shuffle (VecType x, VecType table)
  {
    return vqtbl1q_u8(x, table);
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  shuffle_and_add (VecType x, VecType y)
  {
    return add_by_64s (shuffle (x, shuffle_mask ()), y);
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  unaligned_load (const void *target)
  {
    return vld1q_u8 (reinterpret_cast<const uint8_t *> (target));
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  from_u64x2 (uint64_t a, uint64_t b)
  {
    return vreinterpretq_u8_u64 (uint64x2_t{ a, b });
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  add_extra_data (VecType x, uint64_t info)
  {
    const auto delta = from_u64x2 (info, 0);
    return add_by_64s (x, delta);
  }

  AHASH_CXX_ALWAYS_INLINE
  __attribute__ ((const)) static VecType
  shuffle_mask ()
  {
    return vld1q_u8 (&SHUFFLE_TABLE);
  }
  AHASH_CXX_ALWAYS_INLINE
  static uint64_t
  lower_half (VecType x)
  {
    return vreinterpretq_u64_u8 (x)[0];
  }
};
}
using VectorOperator = asimd::VectorOperator;
} // namespace ahash
#endif