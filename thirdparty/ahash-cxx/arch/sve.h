#pragma once

#include <ahash-cxx/arch/config.h>

#if AHASH_CXX_HAS_SVE_ACCELERATION
#  include <ahash-cxx/common.h>
namespace ahash
{
namespace sve
{
class ExtendedShuffleTable
{
  alignas(64) uint8_t bytes[256]{};

public:
  constexpr
  ExtendedShuffleTable ()
  {
    for (uint8_t i = 0x00; i <= 0x0F; ++i)
      {
        for (uint8_t j = 0x00; j <= 0x0F; ++j)
          {
            bytes[(i << 4) | j] = SHUFFLE_TABLE[j];
          }
      }
  }
  uint8_t constexpr
  operator[] (size_t i) const
  {
    return bytes[i];
  }
  const uint8_t * // NOLINT(google-runtime-operator)
  operator& () const
  {
    return bytes;
  }
};
constexpr ExtendedShuffleTable EXTENDED_SHUFFLE_TABLE{};

struct VectorOperator
{
  using VecType = svuint8_t;

  AHASH_CXX_ALWAYS_INLINE static size_t
  lanes (VecType data)
  {
    return svlen_u8 (data);
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  broadcast_from (uint8x16_t data)
  {
    return svld1rq_u8 (svptrue_b8 (),
                       reinterpret_cast<const uint8_t *> (&data));
  }

  AHASH_CXX_ALWAYS_INLINE static uint8x16_t
  downcast (VecType data, size_t idx)
  {
    uint8x16_t result[1];
    const auto l = svwhilege_b8_u64 (0, idx * 16);
    const auto r = svwhilelt_b8_u64 (0, (idx + 1) * 16);
    const auto range = svand_b_z (svptrue_b8 (), l, r);
    svst1_u8 (range, reinterpret_cast<uint8_t *> (&result[0] - idx), data);
    return result[0];
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  encode (VecType x, VecType y)
  {
    return sveor_u8_x(svptrue_b8 (), svaesmc_u8 (svaese_u8 (x, svdup_n_u8(0))), y);
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  add_by_64s (VecType x, VecType y)
  {
    return svreinterpret_u8_u64(svadd_u64_x(svptrue_b8 (), svreinterpret_u64_u8(x), svreinterpret_u64_u8(y)));
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  shuffle (VecType x, VecType table)
  {
    return svtbl_u8 (x, table);
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  shuffle_and_add (VecType x, VecType y)
  {
    return add_by_64s (shuffle (x, shuffle_mask ()), y);
  }

  AHASH_CXX_ALWAYS_INLINE static VecType
  unaligned_load (const void *target)
  {
    return svld1_u8 (svptrue_b8 (),
                     reinterpret_cast<const uint8_t *> (target));
  }

  AHASH_CXX_ALWAYS_INLINE
  __attribute__ ((const)) static VecType
  shuffle_mask ()
  {
    return svld1_u8 (svptrue_b8 (), &EXTENDED_SHUFFLE_TABLE);
  }
};
}
using WideVectorOperator = sve::VectorOperator;
}
#endif
