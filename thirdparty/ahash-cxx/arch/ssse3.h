#pragma once

#include <ahash-cxx/arch/config.h>

#if AHASH_CXX_HAS_SSSE3_ACCELERATION

#  include <ahash-cxx/common.h>

namespace ahash {
    namespace ssse3 {
        struct VectorOperator {
            using VecType = __m128i;

            AHASH_CXX_ALWAYS_INLINE static constexpr size_t
            lanes(VecType) {
                return 16;
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            broadcast_from(__m128i data) {
                return data;
            }

            AHASH_CXX_ALWAYS_INLINE static __m128i
            downcast(VecType data, size_t) {
                return data;
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            encode(VecType x, VecType y) {
                return _mm_aesenc_si128(x, y);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            decode(VecType x, VecType y) {
                return _mm_aesdec_si128(x, y);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            add_by_64s(VecType x, VecType y) {
                return _mm_add_epi64(x, y);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            shuffle(VecType x, VecType table) {
                return _mm_shuffle_epi8(x, table);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            shuffle_and_add(VecType x, VecType y) {
                return add_by_64s(shuffle(x, shuffle_mask()), y);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            unaligned_load(const void *target) {
                return _mm_loadu_si128(reinterpret_cast<const VecType *> (target));
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            from_u64x2(uint64_t a, uint64_t b) {
                return _mm_set_epi64x(static_cast<int64_t>(a), static_cast<int64_t>(b));
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            add_extra_data(VecType x, uint64_t info) {
                return add_by_64s(x, from_u64x2(0, info));
            }

            AHASH_CXX_ALWAYS_INLINE
            __attribute__ ((const)) static VecType
            shuffle_mask() {
                return _mm_load_si128(reinterpret_cast<const __m128i *>(&SHUFFLE_TABLE));
            }

            AHASH_CXX_ALWAYS_INLINE
            static uint64_t
            lower_half(VecType x) {
                return _mm_extract_epi64(x, 0);
            }
        };
    } // namespace ssse3
    using VectorOperator = ssse3::VectorOperator;
} // namespace ahash
#endif