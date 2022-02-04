#pragma once
#pragma once

#include <iostream>

#include <qc-core/span.hpp>
#include <qc-core/vector-ext.hpp>

namespace qc
{
    //
    // ...
    //
    template <Numeric T, int n> std::ostream & operator<<(std::ostream & os, const span<T, n> & s);

    //
    // ...
    //
    template <Floating T, int n> span<stype<T>, n> round(const span<T, n> & v);
    template <Integral T, int n> span<T, n> round(const span<T, n> & v);

    //
    // ...
    //
    template <Numeric T, int n> vec<T, n> clamp(const vec<T, n> & v, const span<T, n> & span);
    template <Numeric T> T clamp(T v, const span1<T> & span);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <Numeric T, int n>
    inline std::ostream & operator<<(std::ostream & os, const span<T, n> & s)
    {
        os << "[";
        if constexpr (n == 1) os << "[";
        os << s.min;
        if constexpr (n == 1) os << "][";
        os << s.max;
        if constexpr (n == 1) os << "]";
        os << "]";
        return os;
    }

    template <Floating T, int n>
    inline span<stype<T>, n> round(const span<T, n> & v)
    {
        return {round(v.min), round(v.max)};
    }

    template <Integral T, int n>
    inline span<T, n> round(const span<T, n> & v)
    {
        return v;
    }

    template <Numeric T, int n>
    inline vec<T, n> clamp(const vec<T, n> & v, const span<T, n> & span)
    {
        return clamp(v, span.min, span.max);
    }

    template <Numeric T>
    inline T clamp(const T v, const span1<T> & span)
    {
        return clamp(v, span.min, span.max);
    }
}
