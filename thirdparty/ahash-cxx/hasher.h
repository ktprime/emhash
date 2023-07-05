#pragma once

#include <ahash-cxx/arch/config.h>
#include <ahash-cxx/arch/vaes.h>
#include <ahash-cxx/arch/ssse3.h>
#include <ahash-cxx/arch/sve.h>
#include <ahash-cxx/arch/asimd.h>
#include <ahash-cxx/common.h>

namespace ahash {
#if AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION

    class VectorizedHasher {
        using VecType = VectorOperator::VecType;
        VecType key;
        VecType enc;
        VecType sum;

        void
        consume_state(const VecType &state) {
            enc = VectorOperator::encode(enc, state);
            sum = VectorOperator::shuffle_and_add(sum, state);
        }

        void
        consume_pair(const VecType &a, const VecType &b) {
            consume_state(a);
            consume_state(b);
        }

        template<typename Accelerator>
        AHASH_CXX_ALWAYS_INLINE bool
        vectorized_consume(const uint8_t *data, size_t length) {
            const auto duplicated_key = Accelerator::broadcast_from(key);
            const size_t num_lanes = Accelerator::lanes(duplicated_key);
            if (4 * num_lanes >= length) {
                return false;
            }
            // We have to fully expand these things because we may use scalable
            // vectors. scalable vectors can not be stored as arrays.
            const auto tail0
                    = Accelerator::unaligned_load(&data[length - 4 * num_lanes]);
            const auto tail1
                    = Accelerator::unaligned_load(&data[length - 3 * num_lanes]);
            const auto tail2
                    = Accelerator::unaligned_load(&data[length - 2 * num_lanes]);
            const auto tail3
                    = Accelerator::unaligned_load(&data[length - 1 * num_lanes]);
            auto current0 = Accelerator::encode(duplicated_key, tail0);
            auto current1 = Accelerator::encode(duplicated_key, tail1);
            auto current2 = Accelerator::encode(duplicated_key, tail2);
            auto current3 = Accelerator::encode(duplicated_key, tail3);
            auto sum0 = Accelerator::shuffle_and_add(
                    Accelerator::add_by_64s(duplicated_key, tail0), tail2);
            auto sum1 = Accelerator::shuffle_and_add(
                    Accelerator::add_by_64s(duplicated_key, tail1), tail3);

            while (length > 4 * num_lanes) {
                const auto head0 = Accelerator::unaligned_load(&data[0 * num_lanes]);
                const auto head1 = Accelerator::unaligned_load(&data[1 * num_lanes]);
                const auto head2 = Accelerator::unaligned_load(&data[2 * num_lanes]);
                const auto head3 = Accelerator::unaligned_load(&data[3 * num_lanes]);
                current0 = Accelerator::encode(current0, head0);
                current1 = Accelerator::encode(current1, head1);
                current2 = Accelerator::encode(current2, head2);
                current3 = Accelerator::encode(current3, head3);
                sum0 = Accelerator::shuffle_and_add(sum0, head0);
                sum1 = Accelerator::shuffle_and_add(sum1, head1);
                sum0 = Accelerator::shuffle_and_add(sum0, head2);
                sum1 = Accelerator::shuffle_and_add(sum1, head3);
                data += 4 * num_lanes;
                length -= 4 * num_lanes;
            }
            const auto encoded0 = Accelerator::encode(current0, current1);
            const auto encoded1 = Accelerator::encode(current2, current3);
            const auto total = Accelerator::add_by_64s(sum0, sum1);
            for (size_t i = 0; i < num_lanes / 16; ++i) {
                auto a = Accelerator::downcast(encoded0, i);
                auto b = Accelerator::downcast(encoded1, i);
                auto c = Accelerator::downcast(total, i);
                consume_pair(a, b);
                consume_state(c);
            }
            return true;
        }

    public:
        explicit VectorizedHasher(uint64_t seed) {
            enc = VectorOperator::from_u64x2(PI[0], PI[1]);
            sum = VectorOperator::from_u64x2(PI[2], PI[3]);
            key = enc ^ sum;
            consume_state(VectorOperator::from_u64x2(seed, 0));
            auto mix = [this](uint64_t x, uint64_t y) {
                VectorizedHasher h = *this;
                h.consume_state(VectorOperator::from_u64x2(x, 0));
                h.consume_state(VectorOperator::from_u64x2(y, 0));
                return h.finalize();
            };
            uint64_t S[] = {mix(PI2[0], PI2[2]), mix(PI2[1], PI2[3]),
                            mix(PI2[2], PI2[1]), mix(PI2[3], PI2[0])};
            enc = VectorOperator::from_u64x2(S[0], S[1]);
            sum = VectorOperator::from_u64x2(S[2], S[3]);
            key = enc ^ sum;
        }

        [[nodiscard]] uint64_t
        finalize() const {
            auto combined = VectorOperator::decode(sum, enc);
            auto result = VectorOperator::encode(
                    VectorOperator::encode(combined, key), combined);
            return VectorOperator::lower_half(result);
        }

        AHASH_CXX_ALWAYS_INLINE void
        consume(const void *__restrict input, size_t length) {
            const auto *data = reinterpret_cast<const uint8_t *> (input);
            enc = VectorOperator::add_extra_data(enc, length);
            if (length <= 8) {
                auto value = SmallData::load(data, length);
                consume_state(
                        VectorOperator::from_u64x2(value.data[0], value.data[1]));
            } else {
                if (length > 32) {
                    if (length > 64) {
#  if AHASH_CXX_HAS_WIDER_SIMD_ACCELERATION
                        if (length > 128) {
                            if (vectorized_consume<WideVectorOperator>(data, length)) {
                                return;
                            }
                        }
#  endif
                        vectorized_consume<VectorOperator>(data, length);
                    } else {
                        auto h0 = VectorOperator::unaligned_load(&data[0]);
                        auto h1 = VectorOperator::unaligned_load(&data[16]);
                        auto t0 = VectorOperator::unaligned_load(&data[length - 32]);
                        auto t1 = VectorOperator::unaligned_load(&data[length - 16]);
                        consume_pair(h0, h1);
                        consume_pair(t0, t1);
                    }
                } else {
                    if (length > 16) {
                        auto x = VectorOperator::unaligned_load(&data[0]);
                        auto y = VectorOperator::unaligned_load(&data[length - 16]);
                        consume_pair(x, y);
                    } else {
                        auto x = generic_load<uint64_t>(&data[0]);
                        auto y = generic_load<uint64_t>(&data[length - 8]);
                        consume_state(VectorOperator::from_u64x2(x, y));
                    }
                }
            }
        }
    };

#endif

    template<uint64_t MULTIPLE, uint64_t ROT>
    class FallbackHasher {
        uint64_t buffer;
        uint64_t pad;
        uint64_t extra_keys[2]{};

        static uint64_t folded_multiply(uint64_t x, uint64_t y) {
            auto a = static_cast<unsigned __int128>(x);
            auto b = static_cast<unsigned __int128>(y);
            auto result = a * b;
            auto low = static_cast<uint64_t>(result);
            auto high = static_cast<uint64_t>(result >> 64);
            return low ^ high;
        }

        static uint64_t rotate_left(uint64_t x, uint64_t y) {
#pragma push_macro("__has_builtin")
#ifndef __has_builtin
#define __has_builtin(...) 0
#endif
#if __has_builtin(__builtin_rotateleft64)
            return __builtin_rotateleft64(x, y);
#else
            return (x << y) | (x >> (64 - y));
#endif
#pragma pop_macro("__has_builtin")
        }


        void consume(uint64_t low_addr, uint64_t high_addr) {
            uint64_t low = low_addr;
            uint64_t high = high_addr;
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
            uint64_t tmp = low;
            low = high;
            high = tmp;
#endif
            auto combined = folded_multiply(low ^ extra_keys[0], high ^ extra_keys[1]);
            buffer = rotate_left((buffer + pad) ^ combined, ROT);
        }

        void consume(unsigned __int128 t) {
            auto low = static_cast<uint64_t>(t);
            auto high = static_cast<uint64_t>(t >> 64);
            auto combined = folded_multiply(low ^ extra_keys[0], high ^ extra_keys[1]);
            buffer = rotate_left((buffer + pad) ^ combined, ROT);
        }

    public:
        explicit FallbackHasher(uint64_t seed) {
            buffer = PI[0];
            pad = PI[1];
            extra_keys[0] = PI[2];
            extra_keys[1] = PI[3];
            consume(seed);
            auto mix = [this](uint64_t x, uint64_t y) {
                FallbackHasher h = *this;
                h.consume(x);
                h.consume(y);
                return h.finalize();
            };
            uint64_t S[] = {mix(PI2[0], PI2[2]), mix(PI2[1], PI2[3]),
                            mix(PI2[2], PI2[1]), mix(PI2[3], PI2[0])};
            buffer = S[0];
            pad = S[1];
            extra_keys[0] = S[2];
            extra_keys[1] = S[3];
        }

        void
        consume(const void *__restrict input, size_t length) {
            const auto *data = reinterpret_cast<const uint8_t *> (input);
            buffer = (buffer + length) * MULTIPLE;

            if (length > 8) {
                if (length > 16) {
                    const auto tail = generic_load<unsigned __int128>(&data[length - 16]);
                    consume(tail);
                    while (length > 16) {
                        const auto head = generic_load<unsigned __int128>(data);
                        consume(head);
                        data += 16;
                        length -= 16;
                    }
                } else {
                    const auto x = generic_load<uint64_t>(&data[0]);
                    const auto y = generic_load<uint64_t>(&data[length - 8]);
                    consume(x, y);
                }
            } else {
                const auto value = SmallData::load(data, length);
                consume(value.data[0], value.data[1]);
            }
        }

        [[nodiscard]] uint64_t
        finalize() const {
            const auto rot = buffer & 63;
            return rotate_left(folded_multiply(buffer, pad), rot);
        }
    };

    constexpr uint64_t DEFAULT_MULTIPLE = 6364136223846793005;
    constexpr uint64_t DEFAULT_ROT = 23;
#if AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION
    using Hasher = VectorizedHasher;
#else
    using Hasher = FallbackHasher<DEFAULT_MULTIPLE, DEFAULT_ROT>;
#endif
}
