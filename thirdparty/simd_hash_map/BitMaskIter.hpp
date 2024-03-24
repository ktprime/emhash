
//Copyright Nathan Ward 2019.

/* This class is inspired by google's flash_hash_map's BitMask
 * see: github.com/abseil/abseil-cpp/absl/container
 */

#ifndef BIT_MASK_ITER_64_HPP
#define BIT_MASK_ITER_64_HPP

#include <cstdint>
#include <utility>

class BitMaskIter64 {
 public:
  explicit constexpr BitMaskIter64(uint32_t a, uint32_t b, uint32_t c,
                                   uint32_t d) noexcept
      : bits_{(static_cast<uint64_t>(((d << 16) | c)) << 32) |
              (static_cast<uint64_t>((b << 16) | a))} {}

  explicit constexpr BitMaskIter64(uint32_t a, uint32_t b) noexcept
      : bits_{(static_cast<uint64_t>(b) << 32) | a} {}

  explicit constexpr BitMaskIter64(uint64_t bits) noexcept : bits_{bits} {}

  explicit constexpr BitMaskIter64() noexcept : bits_{0} {}

  constexpr BitMaskIter64& operator++() noexcept {
    bits_ &= (bits_ - 1);
    return *this;
  }

  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return bits_ != 0x0000000000000000;
  }

 private:
  [[nodiscard]] constexpr auto countTrailingZeros() const noexcept {
    auto bits = bits_;
    auto count = 63;
    bits &= ~bits + 1;  // bit binary search
    // if (bits) --count;
    if (bits & 0x00000000FFFFFFFF) count -= 32;
    if (bits & 0x0000FFFF0000FFFF) count -= 16;
    if (bits & 0x00FF00FF00FF00FF) count -= 8;
    if (bits & 0x0F0F0F0F0F0F0F0F) count -= 4;
    if (bits & 0x3333333333333333) count -= 2;
    if (bits & 0x5555555555555555) count -= 1;
    return count;
  }

 public:
  [[nodiscard]] constexpr auto operator*() const noexcept {
    if (bits_ == 0) return -1;
    return countTrailingZeros();
  }

  constexpr friend bool operator==(const BitMaskIter64& lhs,
                                   const BitMaskIter64& rhs) noexcept {
    return lhs.bits_ == rhs.bits_;
  }

  constexpr friend bool operator!=(const BitMaskIter64& lhs,
                                   const BitMaskIter64& rhs) noexcept {
    return lhs.bits_ != rhs.bits_;
  }

  [[nodiscard]] constexpr BitMaskIter64 begin() const noexcept { return *this; }

  [[nodiscard]] constexpr BitMaskIter64 end() const noexcept {
    return BitMaskIter64{};
  }

  [[nodiscard]] constexpr bool getFirstBit() const noexcept {
    return ((bits_ >> 63) & 1);
  }

  [[nodiscard]] constexpr bool getLastBit() const noexcept {
    return (bits_ & 1);
  }

  [[nodiscard]] constexpr auto getFirstSetBit() const noexcept {
    if (bits_ == 0) return -1;
    return countTrailingZeros();
  }

 public:
  uint64_t bits_{0};
};

#endif //BIT_MASK_ITER_64_HPP
