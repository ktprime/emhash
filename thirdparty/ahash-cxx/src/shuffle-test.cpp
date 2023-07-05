#include <ahash-cxx/hasher.h>
#include <cstdlib>
#include <string_view>
#include <array>
#include <iostream>
#include <algorithm>
#include <random>
#include <iomanip>

#if AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION
using namespace ahash;
#define CHECK_EQ(a, b)                                                        \
  do                                                                          \
    {                                                                         \
      if ((a) != (b))                                                         \
        return false;                                                         \
    }                                                                         \
  while (0)

#define ASSERT(x)                                                        \
  do                                                                     \
    {                                                                    \
      if (!(x))                                                          \
        ::abort();                                                       \
    }                                                                    \
  while (0)

using Uint8x16Type = uint8_t __attribute__ ((__vector_size__ (16), __may_alias__));
using Uint64x2Type = uint64_t __attribute__ ((__vector_size__ (16), __may_alias__));
union VecUnion {
    VectorOperator::VecType vector;
    Uint8x16Type uint8x16;
    Uint64x2Type uint64x2;
    unsigned __int128 uint128;
};

bool
test_shuffle_does_not_collide_with_aes(VectorOperator::VecType mask) {
    VecUnion value{.vector = {}};
    const auto zero_mask_enc = VecUnion{.vector =  VectorOperator::encode(VectorOperator::VecType{},
                                                                          VectorOperator::VecType{})};
    const auto zero_mask_dec = VecUnion{.vector = VectorOperator::decode(VectorOperator::VecType{},
                                                                         VectorOperator::VecType{})};
    for (size_t index = 0; index < 16; ++index) {
        value.uint8x16[index] = 1;
        const auto excluded_positions_enc
                = VecUnion{.vector = VectorOperator::encode(value.vector, zero_mask_enc.vector)};
        const auto excluded_positions_dec
                = VecUnion{.vector = VectorOperator::decode(value.vector, zero_mask_dec.vector)};
        const auto actual_location
                = VecUnion{.vector = VectorOperator::shuffle(value.vector, mask)};
        for (size_t pos = 0; pos < 16; ++pos) {
            if (actual_location.uint8x16[pos] != 0) {
                CHECK_EQ (0, excluded_positions_enc.uint8x16[pos]);
                CHECK_EQ (0, excluded_positions_dec.uint8x16[pos]);
            }
        }
        value.uint8x16[index] = 0;
    }
    return true;
}

bool test_shuffle_contains_each_value(VectorOperator::VecType mask) {
    VecUnion value = {.uint64x2 =  {0x0001020304050607ULL, 0x08090A0B0C0D0E0FULL}};
    const auto shuffled
            = VecUnion{.vector = VectorOperator::shuffle(value.vector, mask)};

    for (size_t i = 0; i < 16; ++i) {
        bool found = false;
        for (size_t j = 0; j < 16; ++j) {
            if (shuffled.uint8x16[j] == i)
                found = true;
        }
        if (!found)
            return false;
    }
    return true;
}

bool test_shuffle_moves_every_value(VectorOperator::VecType mask) {
    VecUnion value = {.uint64x2 =  {0x0001020304050607ULL, 0x08090A0B0C0D0E0FULL}};
    const auto shuffled
            = VecUnion{.vector = VectorOperator::shuffle(value.vector, mask)};

    for (size_t i = 0; i < 16; ++i) {
        if (shuffled.uint8x16[i] == value.uint8x16[i]) {
            return false;
        }
    }
    return true;
}

bool test_shuffle_does_not_loop(VectorOperator::VecType mask) {
    VecUnion value = {.uint64x2 =  {0x0011223344556677ULL, 0x8899AABBCCDDEEFFULL}};
    auto shuffled = VecUnion{.vector = VectorOperator::shuffle(value.vector, mask)};
    for (size_t count = 0; count < 100; ++count) {
        if (value.uint128 == shuffled.uint128) {
            return false;
        }
        shuffled.vector = VectorOperator::shuffle(shuffled.vector, mask);
    }
    return true;
}

bool test_shuffle_moves_high_bits(VectorOperator::VecType mask) {
    if (VecUnion{.vector = VectorOperator::shuffle(VectorOperator::VecType{1}, mask)}.uint128 <=
        static_cast<unsigned __int128>(1) << 80) {
        return false;
    }

    {
        auto x = VecUnion{.uint128 = 1};
        x.uint128 <<= 58;
        auto y = VecUnion{.uint128 = 1};
        y.uint128 <<= 64;
        if (VecUnion{.vector = VectorOperator::shuffle(x.vector, mask)}.uint128 < y.uint128) {
            return false;
        }
    }

    {
        auto x = VecUnion{.uint128 = 1};
        x.uint128 <<= 58;
        auto y = VecUnion{.uint128 = 1};
        y.uint128 <<= 112;
        if (VecUnion{.vector = VectorOperator::shuffle(x.vector, mask)}.uint128 >= y.uint128) {
            return false;
        }
    }

    {
        auto x = VecUnion{.uint128 = 1};
        x.uint128 <<= 64;
        auto y = VecUnion{.uint128 = 1};
        y.uint128 <<= 64;
        if (VecUnion{.vector = VectorOperator::shuffle(x.vector, mask)}.uint128 >= y.uint128) {
            return false;
        }
    }

    {
        auto x = VecUnion{.uint128 = 1};
        x.uint128 <<= 64;
        auto y = VecUnion{.uint128 = 1};
        y.uint128 <<= 16;
        if (VecUnion{.vector = VectorOperator::shuffle(x.vector, mask)}.uint128 < y.uint128) {
            return false;
        }
    }

    {
        auto x = VecUnion{.uint128 = 1};
        x.uint128 <<= 120;
        auto y = VecUnion{.uint128 = 1};
        y.uint128 <<= 50;
        if (VecUnion{.vector = VectorOperator::shuffle(x.vector, mask)}.uint128 >= y.uint128) {
            return false;
        }
    }

    return true;
}

#else
#define ASSERT(...)
#endif

int main(int argc, const char **argv) {
    ASSERT(test_shuffle_does_not_collide_with_aes(VectorOperator::shuffle_mask()));
    ASSERT(test_shuffle_contains_each_value(VectorOperator::shuffle_mask()));
    ASSERT(test_shuffle_moves_every_value(VectorOperator::shuffle_mask()));
    ASSERT(test_shuffle_does_not_loop(VectorOperator::shuffle_mask()));
    ASSERT(test_shuffle_moves_high_bits(VectorOperator::shuffle_mask()));
#if AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION
    std::array<uint8_t, 16> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    std::random_device dev;
    std::default_random_engine engine(dev());
    if (argc > 1 && std::string_view{argv[1]} == "search") {
        {
            while (true) {
                auto mask = VectorOperator::unaligned_load(&data[0]);
                if (test_shuffle_does_not_collide_with_aes(mask) &&
                    test_shuffle_contains_each_value(mask) &&
                    test_shuffle_moves_every_value(mask) &&
                    test_shuffle_does_not_loop(mask) &&
                    test_shuffle_moves_high_bits(mask)) {
                    VecUnion x{.vector = mask};
                    std::cout << std::hex << "0x" << std::setw(16) << std::setfill('0') << x.uint64x2[0] << std::setw(0)
                              << ", 0x" << std::setw(16) << x.uint64x2[1] << std::endl;
                    break;
                }
                std::shuffle(data.begin(), data.end(), engine);
            }
        }
    }
#endif
}