#pragma once

#include <ahash-cxx/arch/config.h>

#if AHASH_CXX_HAS_VAES_ACCELERATION

#  include <ahash-cxx/common.h>

namespace ahash {
    namespace vaes {

        struct VectorOperator {
            using VecType = __m256i;

            AHASH_CXX_ALWAYS_INLINE static constexpr size_t
            lanes(VecType) {
                return 32;
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            broadcast_from(__m128i data) {
                return _mm256_broadcastsi128_si256(data);
            }

            AHASH_CXX_ALWAYS_INLINE static __m128i
            downcast(VecType data, size_t idx) {
                if (idx == 0)
                    return _mm256_castsi256_si128(data);
                else
                    return _mm256_extracti128_si256(data, 1);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            encode(VecType x, VecType y) {
                return _mm256_aesenc_epi128(x, y);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            add_by_64s(VecType x, VecType y) {
                return _mm256_add_epi64(x, y);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            shuffle(VecType x, VecType table) {
                return _mm256_shuffle_epi8(x, table);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            shuffle_and_add(VecType x, VecType y) {
                return add_by_64s(shuffle(x, shuffle_mask()), y);
            }

            AHASH_CXX_ALWAYS_INLINE static VecType
            unaligned_load(const void *target) {
                return _mm256_loadu_si256(reinterpret_cast<const VecType *>(target));
            }

            AHASH_CXX_ALWAYS_INLINE
            __attribute__ ((const)) static VecType
            shuffle_mask() {
                auto mask = _mm_load_si128(reinterpret_cast<const __m128i *>(&SHUFFLE_TABLE));
                return _mm256_broadcastsi128_si256(mask);
            }
        };
    } // namespace vaes
    using WideVectorOperator = vaes::VectorOperator;
} // namespace ahash
#endif