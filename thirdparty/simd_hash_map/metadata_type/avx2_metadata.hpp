    
//Copyright Nathan Ward 2019.

#ifndef AVX2_METADATA_HPP
#define AVX2_METADATA_HPP

#include <immintrin.h>
#include <cstdint>
#include "../BitMaskIter.hpp"
#include "../metadata.hpp"

struct avx2_metadata {
  using bit_mask = BitMaskIter64;

  explicit avx2_metadata(metadata const* const& md) noexcept {
    group1_ = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(md));
    group2_ = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(md + 32));
  }

  [[nodiscard]] bit_mask Match(const metadata md) const noexcept {
    const auto match = _mm256_set1_epi8(md);
    return bit_mask{static_cast<uint32_t>(_mm256_movemask_epi8(
                        _mm256_cmpeq_epi8(match, group1_))),
                    static_cast<uint32_t>(_mm256_movemask_epi8(
                        _mm256_cmpeq_epi8(match, group2_)))};
  }

  [[nodiscard]] constexpr int getFirstOpenBucket() const noexcept {
    return *getFirstBits();
  }

  [[nodiscard]] bit_mask getFirstBits() const noexcept {
    return bit_mask{static_cast<uint32_t>(_mm256_movemask_epi8(group1_)),
                    static_cast<uint32_t>(_mm256_movemask_epi8(group2_))};
  }

  __m256i group1_;
  __m256i group2_;
};

#endif  // AVX2_METADATA_HPP
