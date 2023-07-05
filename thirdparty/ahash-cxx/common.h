#pragma once

#include <cstdint>
#include <cstring>

namespace ahash {

    template<class T>
    T
    generic_load(const void *__restrict src) {
        T data;
#pragma push_macro("__has_builtin")
#ifndef __has_builtin
#  define __has_builtin(...) 0
#endif
#if __has_builtin(__builtin_memcpy_inline)
        __builtin_memcpy_inline (&data, src, sizeof (T));
#elif __has_builtin(__builtin_memcpy)
        __builtin_memcpy (&data, src, sizeof (T));
#else
        ::memcpy(&data, src, sizeof(T));
#endif
#pragma pop_macro("__has_builtin")
        return data;
    }

    class ShuffleTable {
        uint8_t bytes[16]{};

    public:
        constexpr
        ShuffleTable(uint64_t low, uint64_t high) {
            for (uint8_t i = 0x00; i <= 0x0F; ++i) {
                bytes[i] = (i < 8) ? (low >> (i * 8)) & 0xFF
                                   : (high >> ((i - 8) * 8)) & 0xFF;
            }
        }

        uint8_t constexpr
        operator[](size_t i) const {
            return bytes[i];
        }

        [[nodiscard]] const uint8_t * // NOLINT(google-runtime-operator)
        operator&() const {
            return bytes;
        }
    };

    constexpr ShuffleTable SHUFFLE_TABLE
            = {0x050F0d0806090B04ULL, 0x020A07000C01030EULL};

    class SmallData {
    public:
        uint64_t data[2]{};

        static SmallData
        load(const void *__restrict source, size_t length) {
            const auto *data = reinterpret_cast<const uint8_t *> (source);
            if (length >= 2) {
                if (length >= 4)
                    return {static_cast<uint64_t> (generic_load<uint32_t>(&data[0])),
                            static_cast<uint64_t> (
                                    generic_load<uint32_t>(&data[length - 4]))};
                else
                    return {static_cast<uint64_t> (generic_load<uint16_t>(&data[0])),
                            static_cast<uint64_t> (data[length - 1])};
            } else {
                if (length > 0)
                    return {static_cast<uint64_t> (data[0]),
                            static_cast<uint64_t> (data[0])};
                else
                    return {0, 0};
            }
        }

    private:
        SmallData(uint64_t a, uint64_t b) {
            data[0] = a;
            data[1] = b;
        }
    };

    constexpr static inline uint64_t PI[] = {
            0x243f6a8885a308d3,
            0x13198a2e03707344,
            0xa4093822299f31d0,
            0x082efa98ec4e6c89,
    };

    constexpr static inline uint64_t PI2[] = {
            0x452821e638d01377,
            0xbe5466cf34e90c6c,
            0xc0ac29b7c97c50dd,
            0x3f84d5b5b5470917,
    };

}