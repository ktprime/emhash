#pragma once

#if __ARM_NEON && __ARM_FEATURE_AES
#  include <arm_neon.h>
#  define AHASH_CXX_HAS_ASIMD_ACCELERATION 1
#endif

#if __ARM_FEATURE_SVE2_AES
#  include <arm_sve.h>
#  define AHASH_CXX_HAS_SVE_ACCELERATION 1
#endif

#if __SSSE3__ || __x86_64__ || __amd64__ || _M_X64

#  include <immintrin.h>

#  define AHASH_CXX_HAS_SSSE3_ACCELERATION 1
#endif

#if __VAES__
#  define AHASH_CXX_HAS_VAES_ACCELERATION 1
#endif

#if AHASH_CXX_HAS_ASIMD_ACCELERATION || AHASH_CXX_HAS_SSSE3_ACCELERATION
#  define AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION 1
#endif

#if AHASH_CXX_HAS_SVE_ACCELERATION || AHASH_CXX_HAS_VAES_ACCELERATION
#  define AHASH_CXX_HAS_WIDER_SIMD_ACCELERATION 1
#endif

#define AHASH_CXX_ALWAYS_INLINE __attribute__((always_inline))
