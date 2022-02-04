#pragma once

#include <qc-core/vector.hpp>

namespace qc
{
    template <Numeric T, int n> struct span;

    inline namespace types
    {
        using qc::span;

        template <Numeric T> using span1 = span<T, 1>;
        template <Numeric T> using span2 = span<T, 2>;
        template <Numeric T> using span3 = span<T, 3>;
        template <Numeric T> using span4 = span<T, 4>;

        template <int n> using  fspan = span<f32, n>;
        template <int n> using  dspan = span<f64, n>;
        template <int n> using  cspan = span< s8, n>;
        template <int n> using ucspan = span< u8, n>;
        template <int n> using  sspan = span<s16, n>;
        template <int n> using usspan = span<u16, n>;
        template <int n> using  ispan = span<s32, n>;
        template <int n> using uispan = span<u32, n>;
        template <int n> using  lspan = span<s64, n>;
        template <int n> using ulspan = span<u64, n>;

        using  fspan1 = span1<f32>;
        using  fspan2 = span2<f32>;
        using  fspan3 = span3<f32>;
        using  fspan4 = span4<f32>;
        using  dspan1 = span1<f64>;
        using  dspan2 = span2<f64>;
        using  dspan3 = span3<f64>;
        using  dspan4 = span4<f64>;
        using  cspan1 = span1<s8>;
        using  cspan2 = span2<s8>;
        using  cspan3 = span3<s8>;
        using  cspan4 = span4<s8>;
        using ucspan1 = span1<u8>;
        using ucspan2 = span2<u8>;
        using ucspan3 = span3<u8>;
        using ucspan4 = span4<u8>;
        using  sspan1 = span1<s16>;
        using  sspan2 = span2<s16>;
        using  sspan3 = span3<s16>;
        using  sspan4 = span4<s16>;
        using usspan1 = span1<u16>;
        using usspan2 = span2<u16>;
        using usspan3 = span3<u16>;
        using usspan4 = span4<u16>;
        using  ispan1 = span1<s32>;
        using  ispan2 = span2<s32>;
        using  ispan3 = span3<s32>;
        using  ispan4 = span4<s32>;
        using uispan1 = span1<u32>;
        using uispan2 = span2<u32>;
        using uispan3 = span3<u32>;
        using uispan4 = span4<u32>;
        using  lspan1 = span1<s64>;
        using  lspan2 = span2<s64>;
        using  lspan3 = span3<s64>;
        using  lspan4 = span4<s64>;
        using ulspan1 = span1<u64>;
        using ulspan2 = span2<u64>;
        using ulspan3 = span3<u64>;
        using ulspan4 = span4<u64>;
    }

    template <Numeric T> struct span<T, 1>
    {
        T min{};
        T max{};

        constexpr span() noexcept = default;
        template <Numeric U> constexpr explicit span(const span1<U> & v) noexcept;
        template <Numeric U, int m> constexpr explicit span(const span<U, m> & v) noexcept;
        constexpr span(T v1, T v2) noexcept;

        constexpr span(const span & v) noexcept = default;
        constexpr span(span && v) noexcept = default;

        span & operator=(const span & v) noexcept = default;
        span & operator=(span && v) noexcept = default;

        ~span() noexcept = default;

        constexpr T size() const noexcept;
    };

    template <Numeric T> struct span<T, 2>
    {
        vec2<T> min;
        vec2<T> max;

        constexpr span() noexcept = default;
        template <Numeric U, int m> constexpr explicit span(const span<U, m> & v) noexcept;
        constexpr span(T v1, T v2) noexcept;
        constexpr span(vec2<T> v1, vec2<T> v2) noexcept;
        constexpr span(const span1<T> & v1, const span1<T> & v2) noexcept;

        constexpr span(const span & v) noexcept = default;
        constexpr span(span && v) noexcept = default;

        span & operator=(const span & v) noexcept = default;
        span & operator=(span && v) noexcept = default;

        ~span() noexcept = default;

        constexpr vec2<T> size() const noexcept;

        constexpr span1<T> x() const noexcept;
        constexpr span1<T> y() const noexcept;
    };

    template <Numeric T> struct span<T, 3>
    {
        vec3<T> min;
        vec3<T> max;

        constexpr span() noexcept = default;
        template <Numeric U, int m> constexpr explicit span(const span<U, m> & v) noexcept;
        constexpr span(T v1, T v2) noexcept;
        constexpr span(vec3<T> v1, vec3<T> v2) noexcept;
        constexpr span(const span1<T> & v1, const span1<T> & v2, const span1<T> & v3) noexcept;
        constexpr span(const span2<T> & v1, const span1<T> & v2) noexcept;
        constexpr span(const span1<T> & v1, const span2<T> & v2) noexcept;

        constexpr span(const span & v) noexcept = default;
        constexpr span(span && v) noexcept = default;

        span & operator=(const span & v) noexcept = default;
        span & operator=(span && v) noexcept = default;

        ~span() noexcept = default;

        constexpr vec3<T> size() const noexcept;

        constexpr span1<T> x() const noexcept;
        constexpr span1<T> y() const noexcept;
        constexpr span1<T> z() const noexcept;

        constexpr span2<T> xy() const noexcept;
        constexpr span2<T> yz() const noexcept;
    };

    template <Numeric T> struct span<T, 4>
    {
        vec4<T> min;
        vec4<T> max;

        constexpr span() noexcept = default;
        template <Numeric U, int m> constexpr explicit span(const span<U, m> & v) noexcept;
        constexpr span(T v1, T v2) noexcept;
        constexpr span(vec4<T> v1, vec4<T> v2) noexcept;
        constexpr span(const span1<T> & v1, const span1<T> & v2, const span1<T> & v3, const span1<T> & v4) noexcept;
        constexpr span(const span2<T> & v1, const span1<T> & v2, const span1<T> & v3) noexcept;
        constexpr span(const span1<T> & v1, const span2<T> & v2, const span1<T> & v3) noexcept;
        constexpr span(const span1<T> & v1, const span1<T> & v2, const span2<T> & v3) noexcept;
        constexpr span(const span2<T> & v1, const span2<T> & v2) noexcept;
        constexpr span(const span3<T> & v1, const span1<T> & v2) noexcept;
        constexpr span(const span1<T> & v1, const span3<T> & v2) noexcept;

        constexpr span(const span & v) noexcept = default;
        constexpr span(span && v) noexcept = default;

        span & operator=(const span & v) noexcept = default;
        span & operator=(span && v) noexcept = default;

        ~span() noexcept = default;

        constexpr vec4<T> size() const noexcept;

        constexpr span1<T> x() const noexcept;
        constexpr span1<T> y() const noexcept;
        constexpr span1<T> z() const noexcept;
        constexpr span1<T> w() const noexcept;

        constexpr span2<T> xy() const noexcept;
        constexpr span2<T> yz() const noexcept;
        constexpr span2<T> zw() const noexcept;

        constexpr span3<T> xyz() const noexcept;
        constexpr span3<T> yzw() const noexcept;
    };

    namespace _minutia
    {
        // Prevents warnings for using a negative sign in a ternary for unsigned types
        template <typename T> struct FullSpanHelper;

        template <Floating T>
        struct FullSpanHelper<T>
        {
            static constexpr T minimum{-std::numeric_limits<T>::infinity()};
            static constexpr T maximum{+std::numeric_limits<T>::infinity()};
        };

        template <Integral T>
        struct FullSpanHelper<T>
        {
            static constexpr T minimum{std::numeric_limits<T>::min()};
            static constexpr T maximum{std::numeric_limits<T>::max()};
        };
    }

    template <Numeric T, int n> constexpr span<T, n> fullSpan{_minutia::FullSpanHelper<T>::minimum, _minutia::FullSpanHelper<T>::maximum};
    template <Numeric T, int n> constexpr span<T, n> nullSpan{fullSpan<T, n>.max, fullSpan<T, n>.min};

    template <Numeric T, int n> span<T, n> & operator+=(span<T, n> & v1, T v2);
    template <Numeric T, int n> span<T, n> & operator+=(span<T, n> & v1, const vec<T, n> & v2);

    template <Numeric T, int n> span<T, n> & operator-=(span<T, n> & v1, T v2);
    template <Numeric T, int n> span<T, n> & operator-=(span<T, n> & v1, const vec<T, n> & v2);

    template <Numeric T, int n> span<T, n> & operator*=(span<T, n> & v1, T v2);
    template <Numeric T, int n> span<T, n> & operator*=(span<T, n> & v1, const vec<T, n> & v2);

    template <Numeric T, int n> span<T, n> & operator/=(span<T, n> & v1, T v2);
    template <Numeric T, int n> span<T, n> & operator/=(span<T, n> & v1, const vec<T, n> & v2);

    template <Numeric T, int n> span<T, n> & operator&=(span<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> span<T, n> & operator|=(span<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> operator+(const span<T, n> & v1, T v2);
    template <Numeric T, int n> constexpr span<T, n> operator+(T v1, const span<T, n> & v2);
    template <Numeric T, int n> constexpr span<T, n> operator+(const span<T, n> & v1, const vec<T, n> & v2);
    template <Numeric T, int n> constexpr span<T, n> operator+(const vec<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> operator-(const span<T, n> & v1, T v2);
    template <Numeric T, int n> constexpr span<T, n> operator-(T v1, const span<T, n> & v2);
    template <Numeric T, int n> constexpr span<T, n> operator-(const span<T, n> & v1, const vec<T, n> & v2);
    template <Numeric T, int n> constexpr span<T, n> operator-(const vec<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> operator*(const span<T, n> & v1, T v2);
    template <Numeric T, int n> constexpr span<T, n> operator*(T v1, const span<T, n> & v2);
    template <Numeric T, int n> constexpr span<T, n> operator*(const span<T, n> & v1, const vec<T, n> & v2);
    template <Numeric T, int n> constexpr span<T, n> operator*(const vec<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> operator/(const span<T, n> & v1, T v2);
    template <Numeric T, int n> constexpr span<T, n> operator/(const span<T, n> & v1, const vec<T, n> & v2);

    template <typename T, int n> constexpr bool operator==(const span<T, n> & v1, const span<T, n> & v2);

    template <typename T, int n> constexpr bool operator!=(const span<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> operator&(const span<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> operator|(const span<T, n> & v1, const span<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> min(const span<T, n> & v1, T v2);
    template <Numeric T, int n> constexpr span<T, n> min(const span<T, n> & v1, const vec<T, n> & v2);

    template <Numeric T, int n> constexpr span<T, n> max(const span<T, n> & v1, T v2);
    template <Numeric T, int n> constexpr span<T, n> max(const span<T, n> & v1, const vec<T, n> & v2);

    template <Numeric T, int n> span<T, n> & minify(span<T, n> & v1, T v2);
    template <Numeric T, int n> span<T, n> & minify(span<T, n> & v1, const vec<T, n> & v2);

    template <Numeric T, int n> span<T, n> & maxify(span<T, n> & v1, T v2);
    template <Numeric T, int n> span<T, n> & maxify(span<T, n> & v1, const vec<T, n> & v2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <Numeric T>
    template <Numeric U>
    inline constexpr span<T, 1>::span(const span1<U> & v) noexcept :
        min(T(v.min)),
        max(T(v.max))
    {}

    template <Numeric T>
    template <Numeric U, int m>
    inline constexpr span<T, 1>::span(const span<U, m> & v) noexcept :
        min(T(v.min.x)),
        max(T(v.max.x))
    {}

    template <Numeric T>
    inline constexpr span<T, 1>::span(const T v1, const T v2) noexcept :
        min(v1),
        max(v2)
    {}

    template <Numeric T>
    inline constexpr T span<T, 1>::size() const noexcept
    {
        return max - min;
    }

    template <Numeric T>
    template <Numeric U, int m>
    inline constexpr span<T, 2>::span(const span<U, m> & v) noexcept :
        min(v.min),
        max(v.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 2>::span(const T v1, const T v2) noexcept :
        min(v1),
        max(v2)
    {}

    template <Numeric T>
    inline constexpr span<T, 2>::span(const vec2<T> v1, const vec2<T> v2) noexcept :
        min(v1),
        max(v2)
    {}

    template <Numeric T>
    inline constexpr span<T, 2>::span(const span1<T> & v1, const span1<T> & v2) noexcept :
        min(v1.min, v2.min),
        max(v1.max, v2.max)
    {}

    template <Numeric T>
    inline constexpr vec2<T> span<T, 2>::size() const noexcept
    {
        return max - min;
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 2>::x() const noexcept
    {
        return {min.x, max.x};
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 2>::y() const noexcept
    {
        return {min.y, max.y};
    }

    template <Numeric T>
    template <Numeric U, int m>
    inline constexpr span<T, 3>::span(const span<U, m> & v) noexcept :
        min(v.min),
        max(v.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 3>::span(const T v1, const T v2) noexcept :
        min(v1),
        max(v2)
    {}

    template <Numeric T>
    inline constexpr span<T, 3>::span(const vec3<T> v1, const vec3<T> v2) noexcept :
        min(v1),
        max(v2)
    {}

    template <Numeric T>
    inline constexpr span<T, 3>::span(const span1<T> & v1, const span1<T> & v2, const span1<T> & v3) noexcept :
        min(v1.min, v2.min, v3.min),
        max(v1.max, v2.max, v3.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 3>::span(const span2<T> & v1, const span1<T> & v2) noexcept :
        min(v1.min, v2.min),
        max(v1.max, v2.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 3>::span(const span1<T> & v1, const span2<T> & v2) noexcept :
        min(v1.min, v2.min),
        max(v1.max, v2.max)
    {}

    template <Numeric T>
    inline constexpr vec3<T> span<T, 3>::size() const noexcept
    {
        return max - min;
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 3>::x() const noexcept
    {
        return {min.x, max.x};
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 3>::y() const noexcept
    {
        return {min.y, max.y};
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 3>::z() const noexcept
    {
        return {min.z, max.z};
    }

    template <Numeric T>
    inline constexpr span2<T> span<T, 3>::xy() const noexcept
    {
        return {min.xy(), max.xy()};
    }

    template <Numeric T>
    inline constexpr span2<T> span<T, 3>::yz() const noexcept
    {
        return {min.yz(), max.yz()};
    }

    template <Numeric T>
    template <Numeric U, int m>
    inline constexpr span<T, 4>::span(const span<U, m> & v) noexcept :
        min(v.min),
        max(v.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const T v1, const T v2) noexcept :
        min(v1),
        max(v2)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const vec4<T> v1, const vec4<T> v2) noexcept :
        min(v1),
        max(v2)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const span1<T> & v1, const span1<T> & v2, const span1<T> & v3, const span1<T> & v4) noexcept :
        min(v1.min, v2.min, v3.min, v4.min),
        max(v1.max, v2.max, v3.max, v4.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const span2<T> & v1, const span1<T> & v2, const span1<T> & v3) noexcept :
        min(v1.min, v2.min, v3.min),
        max(v1.max, v2.max, v3.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const span1<T> & v1, const span2<T> & v2, const span1<T> & v3) noexcept :
        min(v1.min, v2.min, v3.min),
        max(v1.max, v2.max, v3.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const span1<T> & v1, const span1<T> & v2, const span2<T> & v3) noexcept :
        min(v1.min, v2.min, v3.min),
        max(v1.max, v2.max, v3.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const span2<T> & v1, const span2<T> & v2) noexcept :
        min(v1.min, v2.min),
        max(v1.max, v2.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const span3<T> & v1, const span1<T> & v2) noexcept :
        min(v1.min, v2.min),
        max(v1.max, v2.max)
    {}

    template <Numeric T>
    inline constexpr span<T, 4>::span(const span1<T> & v1, const span3<T> & v2) noexcept :
        min(v1.min, v2.min),
        max(v1.max, v2.max)
    {}

    template <Numeric T>
    inline constexpr vec4<T> span<T, 4>::size() const noexcept
    {
        return max - min;
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 4>::x() const noexcept
    {
        return {min.x, max.x};
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 4>::y() const noexcept
    {
        return {min.y, max.y};
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 4>::z() const noexcept
    {
        return {min.z, max.z};
    }

    template <Numeric T>
    inline constexpr span1<T> span<T, 4>::w() const noexcept
    {
        return {min.w, max.w};
    }

    template <Numeric T>
    inline constexpr span2<T> span<T, 4>::xy() const noexcept
    {
        return {min.xy(), max.xy()};
    }

    template <Numeric T>
    inline constexpr span2<T> span<T, 4>::yz() const noexcept
    {
        return {min.yz(), max.yz()};
    }

    template <Numeric T>
    inline constexpr span2<T> span<T, 4>::zw() const noexcept
    {
        return {min.zw(), max.zw()};
    }

    template <Numeric T>
    inline constexpr span3<T> span<T, 4>::xyz() const noexcept
    {
        return {min.xyz(), max.xyz()};
    }

    template <Numeric T>
    inline constexpr span3<T> span<T, 4>::yzw() const noexcept
    {
        return {min.yzw(), max.yzw()};
    }

    template <Numeric T, int n>
    inline span<T, n> & operator+=(span<T, n> & v1, const T v2)
    {
        v1.min += v2;
        v1.max += v2;
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & operator+=(span<T, n> & v1, const vec<T, n> & v2)
    {
        v1.min += v2;
        v1.max += v2;
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & operator-=(span<T, n> & v1, const T v2)
    {
        v1.min -= v2;
        v1.max -= v2;
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & operator-=(span<T, n> & v1, const vec<T, n> & v2)
    {
        v1.min -= v2;
        v1.max -= v2;
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & operator*=(span<T, n> & v1, const T v2)
    {
        v1.min *= v2;
        v1.max *= v2;
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & operator*=(span<T, n> & v1, const vec<T, n> & v2)
    {
        v1.min *= v2;
        v1.max *= v2;
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & operator/=(span<T, n> & v1, const T v2)
    {
        if constexpr (Floating<T>) {
            return v1 *= (T(1.0) / v2);
        }
        else {
            v1.min /= v2;
            v1.max /= v2;
            return v1;
        }
    }

    template <Numeric T, int n>
    inline span<T, n> & operator/=(span<T, n> & v1, const vec<T, n> & v2)
    {
        if constexpr (Floating<T>) {
            return v1 *= T(1.0) / v2;
        }
        else {
            v1.min /= v2;
            v1.max /= v2;
            return v1;
        }
    }

    template <Numeric T, int n>
    inline span<T, n> & operator&=(span<T, n> & v1, const span<T, n> & v2)
    {
        maxify(v1.min, v2.min);
        minify(v1.max, v2.max);
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & operator|=(span<T, n> & v1, const span<T, n> & v2)
    {
        minify(v1.min, v2.min);
        maxify(v1.max, v2.max);
        return v1;
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator+(const span<T, n> & v1, const T v2)
    {
        if constexpr (n == 1) {
            return {T(v1.min + v2), T(v1.max + v2)};
        }
        else {
            return {v1.min + v2, v1.max + v2};
        }
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator+(const T v1, const span<T, n> & v2)
    {
        return v2 + v1;
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator+(const span<T, n> & v1, const vec<T, n> & v2)
    {
        return {v1.min + v2, v1.max + v2};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator+(const vec<T, n> & v1, const span<T, n> & v2)
    {
        return v2 + v1;
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator-(const span<T, n> & v1, const T v2)
    {
        if constexpr (n == 1) {
            return {T(v1.min - v2), T(v1.max - v2)};
        }
        else {
            return {v1.min - v2, v1.max - v2};
        }
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator-(const T v1, const span<T, n> & v2)
    {
        if constexpr (n == 1) {
            return {T(v1 - v2.min), T(v1 - v2.max)};
        }
        else {
            return {v1 - v2.min, v1 - v2.max};
        }
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator-(const span<T, n> & v1, const vec<T, n> & v2)
    {
        return {v1.min - v2, v1.max - v2};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator-(const vec<T, n> & v1, const span<T, n> & v2)
    {
        return {v1 - v2.min, v1 - v2.max};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator*(const span<T, n> & v1, const T v2)
    {
        if constexpr (n == 1) {
            return {T(v1.min * v2), T(v1.max * v2)};
        }
        else {
            return {v1.min * v2, v1.max * v2};
        }
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator*(const T v1, const span<T, n> & v2)
    {
        if constexpr (n == 1) {
            return {T(v1 * v2.min), T(v1 * v2.max)};
        }
        else {
            return {v1 * v2.min, v1 * v2.max};
        }
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator*(const span<T, n> & v1, const vec<T, n> & v2)
    {
        return {v1.min * v2, v1.max * v2};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator*(const vec<T, n> & v1, const span<T, n> & v2)
    {
        return {v1 * v2.min, v1 * v2.max};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator/(const span<T, n> & v1, const T v2)
    {
        if constexpr (Floating<T>) {
            return v1 * (T(1.0) / v2);
        }
        else {
            if constexpr (n == 1) {
                return {T(v1.min / v2), T(v1.max / v2)};
            }
            else {
                return {v1.min / v2, v1.max / v2};
            }
        }
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator/(const span<T, n> & v1, const vec<T, n> & v2)
    {
        if constexpr (Floating<T>) {
            return v1 * (T(1.0) / v2);
        }
        else {
            return {v1.min / v2, v1.max / v2};
        }
    }

    template <typename T, int n>
    inline constexpr bool operator==(const span<T, n> & v1, const span<T, n> & v2)
    {
        return v1.min == v2.min && v1.max == v2.max;
    }

    template <typename T, int n>
    inline constexpr bool operator!=(const span<T, n> & v1, const span<T, n> & v2)
    {
        return !(v1 == v2);
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator&(const span<T, n> & v1, const span<T, n> & v2)
    {
        return {max(v1.min, v2.min), min(v1.max, v2.max)};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> operator|(const span<T, n> & v1, const span<T, n> & v2)
    {
        return {min(v1.min, v2.min), max(v1.max, v2.max)};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> min(const span<T, n> & v1, const T v2)
    {
        return {min(v1.min, v2), min(v1.max, v2)};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> min(const span<T, n> & v1, const vec<T, n> & v2)
    {
        return {min(v1.min, v2), min(v1.max, v2)};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> max(const span<T, n> & v1, const T v2)
    {
        return {max(v1.min, v2), max(v1.max, v2)};
    }

    template <Numeric T, int n>
    inline constexpr span<T, n> max(const span<T, n> & v1, const vec<T, n> & v2)
    {
        return {max(v1.min, v2), max(v1.max, v2)};
    }

    template <Numeric T, int n>
    inline span<T, n> & minify(span<T, n> & v1, const T v2)
    {
        minify(v1.min, v2);
        minify(v1.max, v2);
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & minify(span<T, n> & v1, const vec<T, n> & v2)
    {
        minify(v1.min, v2);
        minify(v1.max, v2);
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & maxify(span<T, n> & v1, const T v2)
    {
        maxify(v1.min, v2);
        maxify(v1.max, v2);
        return v1;
    }

    template <Numeric T, int n>
    inline span<T, n> & maxify(span<T, n> & v1, const vec<T, n> & v2)
    {
        maxify(v1.min, v2);
        maxify(v1.max, v2);
        return v1;
    }
}
