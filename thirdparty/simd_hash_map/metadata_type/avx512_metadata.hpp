//Copyright Nathan Ward 2019.

#ifndef AVX512_METADATA_HPP
#define AVX512_METADATA_HPP

#include <immintrin.h>
#include <cstdint>
#include "../BitMaskIter.hpp"
#include "../metadata.hpp"

extern unsigned long long _cvtmask64_u64(__mmask64 a);

struct avx512_metadata {
  using bit_mask = BitMaskIter64;

  explicit avx512_metadata(metadata const* const& md) noexcept {
    group_ = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(md));
  }

  [[nodiscard]] bit_mask Match(const metadata md) const noexcept {
    const auto match = _mm512_set1_epi8(md);
    return bit_mask(static_cast<uint64_t>(
        _cvtmask64_u64(_mm512_cmpeq_epi8_mask(match, group_))));
  }

  [[nodiscard]] constexpr int getFirstOpenBucket() const noexcept {
    return *getFirstBits();
  }

  [[nodiscard]] bit_mask getFirstBits() const noexcept {
    return bit_mask(
        static_cast<uint64_t>(_cvtmask64_u64(_mm512_movepi8_mask(group_))));
  }

  __m512i group_;

};  // avx512_metadata

#endif  // AVX512_METADATA_HPP
