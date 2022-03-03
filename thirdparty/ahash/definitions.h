//
// Created by schrodinger on 12/15/20.
//

#ifndef AHASH_DEFINITIONS_H
#define AHASH_DEFINITIONS_H
#include <stddef.h>
#include <stdint.h>

#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

#if defined(__clang__) || defined(__GNUC__)
#  define AHASH_FAST_PATH inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#  include <intrin.h>
#  define AHASH_FAST_PATH inline __forceinline
#else
#  define AHASH_FAST_PATH inline
#endif
#if defined(__SSSE3__) && defined(__AES__)
#  define AHASH_x86_TARGET
#  include <immintrin.h>
#  include <wmmintrin.h>
#  ifdef __VAES__
typedef __m256i aes256_t;
#    ifdef __AVX512DQ__
typedef __m512i aes512_t;
#    endif
#  endif
typedef __m128i aes128_t;
#  define AES_OR(a, b) (_mm_or_si128(a, b))
#elif (defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)) && \
  defined(__ARM_NEON) && defined(__ARM_FEATURE_CRYPTO)
#  define AHASH_ARM_TARGET
#  ifdef _MSC_VER
#    include <arm64_neon.h>
#  else
#    include <arm_neon.h>
#  endif
typedef uint8x16_t aes128_t;
#  define AES_OR(a, b) (veorq_u8(a, b))
#else
#  define AHASH_USE_FALLBACK
#endif

#ifndef AHASH_USE_FALLBACK

#  if defined(__VAES__) && defined(AHASH_x86_TARGET)
static AHASH_FAST_PATH aes256_t shuffle2(aes256_t data)
{
  const aes256_t mask = _mm256_set_epi64x(
    0x020a07000c01030eull,
    0x050f0d0806090b04ull,
    0x020a07000c01030eull,
    0x050f0d0806090b04ull);
  return _mm256_shuffle_epi8(data, mask);
}
static AHASH_FAST_PATH aes256_t shuffle_add2(aes256_t x, aes256_t y)
{
  return _mm256_add_epi64(shuffle2(x), y);
}
static AHASH_FAST_PATH aes256_t add_by_64s2(aes256_t x, aes256_t y)
{
  return _mm256_add_epi64(x, y);
}
static AHASH_FAST_PATH aes256_t aes_encode2(aes256_t x, aes256_t y)
{
  return _mm256_aesenc_epi128(x, y);
}

#    ifdef __AVX512DQ__
static AHASH_FAST_PATH aes512_t shuffle4(aes512_t data)
{
  const aes512_t mask = _mm512_set_epi64(
    0x020a07000c01030eull,
    0x050f0d0806090b04ull,
    0x020a07000c01030eull,
    0x050f0d0806090b04ull,
    0x020a07000c01030eull,
    0x050f0d0806090b04ull,
    0x020a07000c01030eull,
    0x050f0d0806090b04ull);
  return _mm512_shuffle_epi8(data, mask);
}
static AHASH_FAST_PATH aes512_t shuffle_add4(aes512_t x, aes512_t y)
{
  return _mm512_add_epi64(shuffle4(x), y);
}
static AHASH_FAST_PATH aes512_t add_by_64s4(aes512_t x, aes512_t y)
{
  return _mm512_add_epi64(x, y);
}
static AHASH_FAST_PATH aes512_t aes_encode4(aes512_t x, aes512_t y)
{
  return _mm512_aesenc_epi128(x, y);
}
#    endif
#  endif

static AHASH_FAST_PATH aes128_t shuffle(aes128_t data)
{
#  ifdef AHASH_x86_TARGET
  const aes128_t mask =
    _mm_set_epi64x(0x020a07000c01030eull, 0x050f0d0806090b04ull);
  return _mm_shuffle_epi8(data, mask);
#  elif defined(AHASH_ARM_TARGET) && defined(_MSC_VER)
  static const unsigned long masks[2] = {
    0x020a07000c01030eull, 0x050f0d0806090b04ull};
  return vqtbl1q_p8(data, vld1q_u64(masks));
#  elif defined(AHASH_ARM_TARGET)
  return (aes128_t)vqtbl1q_p8(
    (poly8x16_t)data,
    (aes128_t)(
      ((__int128)(0x020a07000c01030eull) << 64ull) | 0x050f0d0806090b04ull));
#  elif __has_builtin(__builtin_shuffle)
  typedef uint8_t v16ui __attribute__((vector_size(16)));
  return (aes128_t)__builtin_shuffle(
    (v16ui)data,
    (v16ui)(
      ((__int128)(0x020a07000c01030eull) << 64ull) | 0x050f0d0806090b04ull));
#  endif
}

static AHASH_FAST_PATH aes128_t shuffle_add(aes128_t x, aes128_t y)
{
#  ifdef AHASH_x86_TARGET
  return _mm_add_epi64(shuffle(x), y);
#  elif defined(AHASH_ARM_TARGET) && defined(_MSC_VER)
  return vaddq_s64(shuffle(x), y);
#  elif defined(AHASH_ARM_TARGET)
  return (aes128_t)vaddq_s64((int64x2_t)shuffle(x), (int64x2_t)y);
#  elif
  typedef uint64_t v64i __attribute__((vector_size(16)));
  return (aes128_t)((v64i)(shuffle(x)) + (v64i)(y));
#  endif
}

static AHASH_FAST_PATH aes128_t add_by_64s(aes128_t x, aes128_t y)
{
#  ifdef AHASH_x86_TARGET
  return _mm_add_epi64(x, y);
#  elif defined(AHASH_ARM_TARGET) && defined(_MSC_VER)
  return vaddq_s64(x, y);
#  elif defined(AHASH_ARM_TARGET)
  return (aes128_t)vaddq_s64((int64x2_t)x, (int64x2_t)y);
#  elif
  typedef int64_t v64i __attribute__((vector_size(16)));
  return (aes128_t)((v64i)(x) + (v64i)(y));
#  endif
}

static AHASH_FAST_PATH aes128_t add_shuffle(aes128_t x, aes128_t y)
{
#  ifdef AHASH_x86_TARGET
  return shuffle(_mm_add_epi64(x, y));
#  elif defined(AHASH_ARM_TARGET) && defined(_MSC_VER)
  return shuffle(vaddq_s64(x, y));
#  elif defined(AHASH_ARM_TARGET)
  return shuffle((aes128_t)vaddq_s64((int64x2_t)(x), (int64x2_t)y));
#  elif
  typedef int64_t v64i __attribute__((vector_size(16)));
  return shuffle((aes128_t)((v64i)x + (v64i)y));
#  endif
}

static AHASH_FAST_PATH aes128_t aes_encode(aes128_t x, aes128_t y)
{
#  ifdef AHASH_x86_TARGET
  return _mm_aesenc_si128(x, y);
#  elif defined(AHASH_ARM_TARGET) && defined(_MSC_VER)
  static const unsigned long zero[2] = {0, 0};
  return veorq_u8(vaesmcq_u8(vaeseq_u8(x, vld1q_u64(zero))), y);
#  elif defined(AHASH_ARM_TARGET)
  return (aes128_t)vaesmcq_u8(vaeseq_u8((uint8x16_t)x, (uint8x16_t){})) ^ y;
#  endif
}

static AHASH_FAST_PATH aes128_t aes_decode(aes128_t x, aes128_t y)
{
#  ifdef AHASH_x86_TARGET
  return _mm_aesdec_si128(x, y);
#  elif defined(AHASH_ARM_TARGET) && defined(_MSC_VER)
  static const unsigned long zero[2] = {0, 0};
  return veorq_u8(vaesimcq_u8(vaesdq_u8(x, vld1q_u64(zero))), y);
#  elif defined(AHASH_ARM_TARGET)
  return (aes128_t)vaesimcq_u8(vaesdq_u8((uint8x16_t)x, (uint8x16_t){})) ^ y;
#  endif
}
#else
static AHASH_FAST_PATH uint64_t rotate_left(uint64_t x, uint8_t bit)
{
#  if __has_builtin(__builtin_rotateleft64)
  // this is bascially a clang builtin
  return __builtin_rotateleft64(x, bit);
#  elif defined(_MS_VER)
  return _rotl64(x, bit);
#  else
  // actually, both arm64 and x86_64 has good codegen
  // with the following on clang and gcc
  return (x >> (64 - bit)) | (x << bit);
#  endif
}

static AHASH_FAST_PATH void
emu_multiply(uint64_t op1, uint64_t op2, uint64_t* hi, uint64_t* lo)
{
  uint64_t u1 = (op1 & 0xffffffff);
  uint64_t v1 = (op2 & 0xffffffff);
  uint64_t t = (u1 * v1);
  uint64_t w3 = (t & 0xffffffff);
  uint64_t k = (t >> 32);

  op1 >>= 32;
  t = (op1 * v1) + k;
  k = (t & 0xffffffff);
  uint64_t w1 = (t >> 32);

  op2 >>= 32;
  t = (u1 * op2) + k;
  k = (t >> 32);

  *hi = (op1 * op2) + w1 + k;
  *lo = (t << 32) + w3;
}

static AHASH_FAST_PATH uint64_t folded_multiply(uint64_t s, uint64_t by)
{
#  if defined(__SIZEOF_INT128__)
  // if int128 is available, then use int128,
  // this should pass for most 64 bit machines
  __int128 result = (__int128)(s) * (__int128)(by);
  return (uint64_t)(result & 0xffffffffffffffff) ^ (uint64_t)(result >> 64);
#  elif defined(_MSC_VER) && _M_X64
  uint64_t high, low;
  low = _umul128(s, by, &high);
  return high ^ low;
#  else
  // fallback for 32bit machines, this generally do the same thing as clang's
  // TI integers for 32bit
  uint64_t high, low;
  emu_multiply(s, by, &high, &low);
  return high ^ low;
#  endif
}
#endif
#endif // AHASH_DEFINITIONS_H
