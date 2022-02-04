#pragma once

#include <cmath>

#include <bit>
#include <numbers>

#include <qc-core/core.hpp>

namespace qc
{
    //
    // ...
    //
    template <Numeric T> constexpr T median(T v1, T v2, T v3);

    //
    // ...
    //
    template <Numeric T> constexpr T clamp(const T & v, const T & min, const T & max);

    //
    // ...
    //
    template <Numeric T> constexpr T abs(T v);

    //
    // ...
    //
    template <Numeric T> bool isZero(T v, T e = std::numeric_limits<T>::epsilon());

    //
    // ...
    //
    template <typename T> bool areEqual(const T & v1, const T & v2);
    template <typename T, typename... Ts> bool areEqual(const T & v1, const T & v2, const T & v3, const Ts &... vs);

    //
    // ...
    //
    template <Floating T> bool areEqual_e(T v1, T v2, T e = std::numeric_limits<T>::epsilon());

    //
    // ...
    //
    template <Numeric T> constexpr int sign(T v);

    //
    // Returns `v` stripped of any fractional part.
    // If `T` is a float, only works for values that can fit in an `s32`.
    // If `T` is a double, only works for values that can fit in a `s64`.
    // If `T` is integral, simply returns `v`.
    //
    template <Numeric T> T trunc(T v);

    //
    // ...
    // ~2x faster than std::floor
    // doesn't work with extremely large or extremely small floating point values
    //
    template <Floating T> stype<T> floor(T v);
    template <Integral T> T floor(T v);

    //
    // ...
    // ~2x faster than std::ceil
    // doesn't work with extremely large or extremely small floating point values
    //
    template <Floating T> stype<T> ceil(T v);
    template <Integral T> T ceil(T v);

    //
    // ...
    // ~12x faster than std::llround
    // Only works for "normal" values of absolute magnitude smaller than roughly one quadrillion for doubles or one million for floats
    // Any exact 0.5 value is not guaranteed to round up - it may go either way
    //
    s64 round(double v);
    s32 round(float v);
    template <Integral T> T round(T v);

    // Simple wrapper around std::pow
    template <Floating T> T pow(T v, T e);

    //
    // ...
    // ~2.15x faster than std::pow
    //
    template <Floating T> T pow(T v, int e);
    template <Floating T> T pow(T v, uint e);

    //
    // ...
    //
    template <UnsignedIntegral T> constexpr int log2Floor(T v);

    //
    // ...
    //
    template <UnsignedIntegral T> constexpr int mipmaps(T size);

    //
    // ...
    // ~3.3x faster than std::modf
    //
    template <Floating T> T fract(T v);

    //
    // ...
    //
    template <Floating T> std::pair<T, stype<T>> fract_i(T v);

    //
    // ...
    // ~2.5x faster than std::fmod
    // `d` must be > 0
    //
    template <Numeric T> constexpr T mod(T v, T d);

    //
    // ...
    // `d` must be > 0
    //
    template <Numeric T> constexpr std::pair<T, T> mod_q(T v, T d);

    //
    // ...
    //
    template <Floating T> T mix(T v1, T v2, T t);

    //
    // ...
    //
    template <Floating T> T unmix(T v1, T v2, T v);

    //
    // ...
    //
    template <Floating T> T smoothstep(T v1, T v2, T t);

    //
    // ...
    //
    template <typename T, typename... Args> constexpr T sum(const T & v, const Args &... args);

    //
    // ...
    //
    template <typename T, typename... Args> constexpr T product(const T & v, const Args &... args);

    //
    // ...
    //
    template <Floating T, Floating... Args> T average(T v, Args... args);

    //
    // ...
    //
    template <Floating T> T radians(T degrees);

    //
    // ...
    //
    template <Floating T> T degrees(T radians);

    //
    // Converts between normalized types.
    //
    template <Floating To, Floating From> To transnorm(From v);
    template <SignedIntegral To, Floating From> To transnorm(From v);
    template <UnsignedIntegral To, Floating From> To transnorm(From v);
    template <Floating To, SignedIntegral From> To transnorm(From v);
    template <Floating To, UnsignedIntegral From> To transnorm(From v);
    template <UnsignedIntegral To, UnsignedIntegral From> To transnorm(From v);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <Numeric T>
    inline constexpr T median(T v1, T v2, T v3)
    {
        if (v1 > v2) std::swap(v1, v2);
        if (v2 > v3) std::swap(v2, v3);
        return max(v1, v2);
    }

    template <Numeric T>
    inline constexpr T clamp(const T & v, const T & min, const T & max)
    {
        return qc::min(qc::max(v, min), max);
    }

    template <Numeric T>
    inline constexpr T abs(const T v)
    {
        if constexpr (UnsignedIntegral<T>) {
            return v;
        }
        else {
            return v < 0 ? -v : v;
        }
    }

    template <Numeric T>
    inline bool isZero(const T v, const T e)
    {
        if constexpr (Floating<T>) {
            return abs(v) <= e;
        }
        else {
            return !v;
        }
    }

    template <typename T>
    inline bool areEqual(const T & v1, const T & v2)
    {
        if constexpr (Floating<T>) {
            return isZero(v1 - v2);
        }
        else {
            return v1 == v2;
        }
    }

    template <typename T, typename... Ts>
    inline bool areEqual(const T & v1, const T & v2, const T & v3, const Ts &... vs)
    {
        return areEqual(v1, v2) && areEqual(v2, v3, vs...);
    }

    template <Floating T>
    inline bool areEqual_e(const T v1, const T v2, const T e)
    {
        return isZero(v1 - v2, e);
    }

    template <Numeric T>
    inline constexpr int sign(const T v)
    {
        if constexpr (UnsignedIntegral<T>) {
            return int(v > 0u);
        }
        else {
            return int(0 < v) - int(v < 0);
        }
    }

    template <Numeric T>
    inline T trunc(const T v)
    {
        if constexpr (Floating<T>) {
            return T(stype<T>(v));
        }
        else {
            return v;
        }
    }

    template <Floating T>
    inline stype<T> floor(const T v)
    {
        stype<T> i{stype<T>(v)};
        return i - (v < T(i));
    }

    template <Integral T>
    inline T floor(const T v)
    {
        return v;
    }

    template <Floating T>
    inline stype<T> ceil(const T v)
    {
        stype<T> i{stype<T>(v)};
        return i + (v > T(i));
    }

    template <Integral T>
    inline T ceil(const T v)
    {
        return v;
    }

    inline s64 round(double v)
    {
        // Right bit shift on signed integer must be arithmetic
        static_assert((s64(-1) >> 1) == s64(-1));

        return std::bit_cast<s64>(v + 6755399441055744.0) << 13 >> 13;
    }

    inline s32 round(float v)
    {
        // Right bit shift on signed integer must be arithmetic
        static_assert((s32(-1) >> 1) == s32(-1));

        return std::bit_cast<s32>(v + 12582912.0f) << 10 >> 10;
    }

    template <Integral T>
    inline T round(const T v)
    {
        return v;
    }

    template <Floating T>
    inline T pow(const T v, const T e)
    {
        return std::pow(v, e);
    }

    template <Floating T>
    inline T pow(const T v, const int e)
    {
        if (e >= 0) {
            return pow(v, uint(e));
        }
        else {
            return pow(T(1.0) / v, uint(-e));
        }
    }

    template <Floating T>
    inline T pow(T v, uint e)
    {
        T r{T(1.0)};

        do {
            if (e & 1u) r *= v; // exponent is odd
            e >>= 1;
            v *= v;
        } while (e);

        return r;
    }

    template <UnsignedIntegral T>
    inline constexpr int log2Floor(const T v)
    {
        // Old, non-std way of doing it
        //static_assert(sizeof(T) <= 8u);
        //
        //int log{0};
        //if constexpr (sizeof(T) >= 8u) if (v & 0xFFFFFFFF00000000) { v >>= 32; log += 32; }
        //if constexpr (sizeof(T) >= 4u) if (v & 0x00000000FFFF0000) { v >>= 16; log += 16; }
        //if constexpr (sizeof(T) >= 2u) if (v & 0x000000000000FF00) { v >>=  8; log +=  8; }
        //                               if (v & 0x00000000000000F0) { v >>=  4; log +=  4; }
        //                               if (v & 0x000000000000000C) { v >>=  2; log +=  2; }
        //                               if (v & 0x0000000000000002) {           log +=  1; }
        //return log;

        return v == T(0) ? 0 : int(std::bit_width(v)) - 1;
    }

    template <UnsignedIntegral T>
    inline constexpr int mipmaps(const T size)
    {
        return int(std::bit_width(size));
    }

    template <Floating T>
    inline T fract(const T v)
    {
        return v - trunc(v);
    }

    template <Floating T>
    inline std::pair<T, stype<T>> fract_i(const T v)
    {
        const stype<T> i{stype<T>(v)};
        return {v - T(i), i};
    }

    template <Numeric T>
    inline constexpr T mod(const T v, const T d)
    {
        if constexpr (Floating<T>) {
            return fract(v / d) * d;
        }
        else {
            return T(v % d);
        }
    }

    template <Numeric T>
    inline constexpr std::pair<T, T> mod_q(const T v, const T d)
    {
        if constexpr (Floating<T>) {
            const T q{v / d};
            return {fract(q) * d, q};
        }
        else {
            const auto q{v / d};
            return {T(v - q * d), T(q)};
        }
    }

    template <Floating T>
    inline T mix(const T v1, const T v2, const T t)
    {
        return std::lerp(v1, v2, t);
    }

    template <Floating T>
    inline T unmix(const T v1, const T v2, const T v)
    {
        return (v - v1) / (v2 - v1);
    }

    template <Floating T>
    inline T smoothstep(const T v1, const T v2, const T t)
    {
        return mix(v1, v2, t * t * (T(3.0) - T(2.0) * t));
    }

    template <typename T, typename... Args>
    inline constexpr T sum(const T & v, const Args &... args)
    {
        return (v + ... + args);
    }

    template <typename T, typename... Args>
    inline constexpr T product(const T & v, const Args &... args)
    {
        return (v * ... * args);
    }

    template <Floating T, Floating... Args>
    inline T average(const T v, const Args... args)
    {
        return (v + ... + args) * (T(1.0) / T(1 + sizeof...(Args)));
    }

    template <Floating T>
    inline T radians(const T degrees)
    {
        return degrees * T(std::numbers::pi / 180.0);
    }

    template <Floating T>
    inline T degrees(const T radians)
    {
        return radians * T(180.0 / std::numbers::pi);
    }

    template <Floating To, Floating From>
    inline To transnorm(const From v)
    {
        return To(v);
    }

    template <SignedIntegral To, Floating From>
    inline To transnorm(From v)
    {
        if (v <= From(-1.0)) return -std::numeric_limits<To>::max();
        if (v >= From(1.0)) return std::numeric_limits<To>::max();
        return To(std::round(v * From(std::numeric_limits<To>::max())));
    }

    template <UnsignedIntegral To, Floating From>
    inline To transnorm(const From v)
    {
        if (v <= From(0.0)) return To(0u);
        if (v >= From(1.0)) return std::numeric_limits<To>::max();
        return To(std::round(v * From(std::numeric_limits<To>::max())));
    }

    template <Floating To, SignedIntegral From>
    inline To transnorm(const From v)
    {
        return max(To(v) * (To(1.0) / To(std::numeric_limits<From>::max())), To(-1.0));
    }

    template <Floating To, UnsignedIntegral From>
    inline To transnorm(const From v)
    {
        return To(v) * (To(1.0) / To(std::numeric_limits<From>::max()));
    }

    template <UnsignedIntegral To, UnsignedIntegral From>
    inline To transnorm(const From v)
    {
        if constexpr (sizeof(From) == sizeof(To)) {
            return v;
        }
        else if constexpr (sizeof(From) > sizeof(To)) {
            return To(v >> (8u * (sizeof(From) - sizeof(To))));
        }
        else {
            using NextFrom = typename sized<sizeof(From) * 2>::utype;
            using OpType = typename sized<max(sizeof(NextFrom), sizeof(decltype(v << 1)))>::utype;
            return transnorm<To>(NextFrom(v | (OpType(v) << (8u * sizeof(From)))));
        }
    }
}
