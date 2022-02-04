#pragma once

//
// Column-major ordering
//
//  x1 x2 x3    00 03 06
//  y1 y2 y3    01 04 07
//  z1 z2 z3    02 05 08
//

#include <qc-core/vector.hpp>

namespace qc
{
    template <Floating T, int n> struct mat;

    inline namespace types
    {
        using qc::mat;

        template <Floating T> using mat2 = mat<T, 2>;
        template <Floating T> using mat3 = mat<T, 3>;
        template <Floating T> using mat4 = mat<T, 4>;

        template <int n> using fmat = mat< float, n>;
        template <int n> using dmat = mat<double, n>;

        using fmat2 = mat<float, 2>;
        using fmat3 = mat<float, 3>;
        using fmat4 = mat<float, 4>;
        using dmat2 = mat<double, 2>;
        using dmat3 = mat<double, 3>;
        using dmat4 = mat<double, 4>;

        template <typename T> concept Matrix = std::is_same_v<T, mat<typename T::Type, T::n>>;

        template <typename T> concept Matrix2 = Matrix<T> && T::n == 2;
        template <typename T> concept Matrix3 = Matrix<T> && T::n == 3;
        template <typename T> concept Matrix4 = Matrix<T> && T::n == 4;
    }

    template <Floating T> struct mat<T, 2>
    {
        using Type = T;
        static constexpr int n{2};

        vec2<T> c1{T(1.0), T(0.0)};
        vec2<T> c2{T(0.0), T(1.0)};

        constexpr mat() noexcept = default;
        constexpr mat(vec2<T> c1, vec2<T> c2) noexcept;
        constexpr mat(
            T x1, T y1,
            T x2, T y2
        ) noexcept;
        template <Floating U> constexpr explicit mat(const mat2<U> & m) noexcept;
        template <Floating U> constexpr explicit mat(const mat3<U> & m) noexcept;
        template <Floating U> constexpr explicit mat(const mat4<U> & m) noexcept;

        constexpr mat(const mat & m) noexcept = default;
        constexpr mat(mat && m) noexcept = default;

        mat & operator=(const mat & m) noexcept = default;
        mat & operator=(mat && m) noexcept = default;

        ~mat() noexcept = default;

        constexpr explicit operator bool() const noexcept;

        vec2<T> & col(int i);
        vec2<T> col(int i) const;

        vec2<T> row(int i) const;

        template <int i> constexpr vec2<T> row() const noexcept;

        template <int i> constexpr vec2<T> col() const noexcept;
    };

    template <Floating T> struct mat<T, 3>
    {
        using Type = T;
        static constexpr int n{3};

        vec3<T> c1{T(1.0), T(0.0), T(0.0)};
        vec3<T> c2{T(0.0), T(1.0), T(0.0)};
        vec3<T> c3{T(0.0), T(0.0), T(1.0)};

        constexpr mat() noexcept = default;
        constexpr mat(vec3<T> c1, vec3<T> c2, vec3<T> c3) noexcept;
        constexpr mat(
            T x1, T y1, T z1,
            T x2, T y2, T z2,
            T x3, T y3, T z3
        ) noexcept;
        template <Floating U> constexpr explicit mat(const mat2<U> & m) noexcept;
        template <Floating U> constexpr explicit mat(const mat3<U> & m) noexcept;
        template <Floating U> constexpr explicit mat(const mat4<U> & m) noexcept;

        constexpr mat(const mat & m) noexcept = default;
        constexpr mat(mat && m) noexcept = default;

        mat & operator=(const mat & m) noexcept = default;
        mat & operator=(mat && m) noexcept = default;
        mat & operator=(const mat2<T> & m) noexcept;

        ~mat() noexcept = default;

        constexpr explicit operator bool() const noexcept;

        vec3<T> & col(int i);
        vec3<T> col(int i) const;

        vec3<T> row(int i) const;

        template <int i> constexpr vec3<T> row() const noexcept;

        template <int i> constexpr vec3<T> col() const noexcept;
    };

    template <Floating T> struct mat<T, 4>
    {
        using Type = T;
        static constexpr int n{4};

        vec4<T> c1{T(1.0), T(0.0), T(0.0), T(0.0)};
        vec4<T> c2{T(0.0), T(1.0), T(0.0), T(0.0)};
        vec4<T> c3{T(0.0), T(0.0), T(1.0), T(0.0)};
        vec4<T> c4{T(0.0), T(0.0), T(0.0), T(1.0)};

        constexpr mat() noexcept = default;
        constexpr mat(vec4<T> c1, vec4<T> c2, vec4<T> c3, vec4<T> c4) noexcept;
        constexpr mat(
            T x1, T y1, T z1, T w1,
            T x2, T y2, T z2, T w2,
            T x3, T y3, T z3, T w3,
            T x4, T y4, T z4, T w4
        ) noexcept;
        template <Floating U> constexpr explicit mat(const mat2<U> & m) noexcept;
        template <Floating U> constexpr explicit mat(const mat3<U> & m) noexcept;
        template <Floating U> constexpr explicit mat(const mat4<U> & m) noexcept;

        constexpr mat(const mat & m) noexcept = default;
        constexpr mat(mat && m) noexcept = default;

        mat & operator=(const mat & m) noexcept = default;
        mat & operator=(mat && m) noexcept = default;
        mat & operator=(const mat2<T> & m) noexcept;
        mat & operator=(const mat3<T> & m) noexcept;

        ~mat() noexcept = default;

        constexpr explicit operator bool() const noexcept;

        vec4<T> & col(int i);
        vec4<T> col(int i) const;

        vec4<T> row(int i) const;

        template <int i> constexpr vec4<T> row() const noexcept;

        template <int i> constexpr vec4<T> col() const noexcept;
    };

    template <typename T, int n> mat<T, n> & operator+=(mat<T, n> & m, T v);
    template <typename T, int n> mat<T, n> & operator+=(mat<T, n> & m1, const mat<T, n> & m2);

    template <typename T, int n> mat<T, n> & operator-=(mat<T, n> & m, T v);
    template <typename T, int n> mat<T, n> & operator-=(mat<T, n> & m1, const mat<T, n> & m2);

    template <typename T, int n> mat<T, n> & operator*=(mat<T, n> & m, T v);
    template <typename T, int n> mat<T, n> & operator*=(mat<T, n> & m1, const mat<T, n> & m2); // THIS IS EQUIVALENT TO m1 = m2 * m1 !!!
    template <typename T, int n> vec<T, n> & operator*=(vec<T, n> & v, const mat<T, n> & m);

    template <typename T, int n> mat<T, n> & operator/=(mat<T, n> & m, T v);

    template <typename T, int n> constexpr mat<T, n> operator+(const mat<T, n> & m);

    template <typename T, int n> constexpr mat<T, n> operator-(const mat<T, n> & m);

    template <typename T, int n> constexpr mat<T, n> operator+(const mat<T, n> & m1, const mat<T, n> & m2);
    template <typename T, int n> constexpr mat<T, n> operator+(const mat<T, n> & m, T v);
    template <typename T, int n> constexpr mat<T, n> operator+(T v, const mat<T, n> & m);

    template <typename T, int n> constexpr mat<T, n> operator-(const mat<T, n> & m1, const mat<T, n> & m2);
    template <typename T, int n> constexpr mat<T, n> operator-(const mat<T, n> & m, T v);
    template <typename T, int n> constexpr mat<T, n> operator-(T v, const mat<T, n> & m);

    template <typename T, int n> constexpr mat<T, n> operator*(const mat<T, n> & m1, const mat<T, n> & m2);
    template <typename T, int n> constexpr mat<T, n> operator*(const mat<T, n> & m, T v);
    template <typename T, int n> constexpr mat<T, n> operator*(T v, const mat<T, n> & m);
    template <typename T, int n> constexpr vec<T, n> operator*(const mat<T, n> & m, const vec<T, n> & v);

    template <typename T, int n> constexpr mat<T, n> operator/(const mat<T, n> & m, T v);
    template <typename T, int n> constexpr mat<T, n> operator/(T v, const mat<T, n> & m);

    template <typename T, int n> constexpr bool operator==(const mat<T, n> & m1, const mat<T, n> & m2);

    template <typename T, int n> constexpr bool operator!=(const mat<T, n> & m1, const mat<T, n> & m2);

    //
    // ...
    //
    template <typename T, int n> constexpr mat<T, n> fullMat(T v);

    //
    // ...
    //
    template <typename T, int n> constexpr mat<T, n> nullMat();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <Floating T>
    inline constexpr mat<T, 2>::mat(const vec2<T> c1, const vec2<T> c2) noexcept :
        c1(c1),
        c2(c2)
    {}

    template <Floating T>
    inline constexpr mat<T, 2>::mat(
        const T x1, const T y1,
        const T x2, const T y2
    ) noexcept :
        c1(x1, y1),
        c2(x2, y2)
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 2>::mat(const mat2<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2)
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 2>::mat(const mat3<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2)
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 2>::mat(const mat4<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2)
    {}

    template <Floating T>
    inline constexpr mat<T, 2>::operator bool() const noexcept
    {
        return *this == mat{};
    }

    template <Floating T>
    inline vec2<T> & mat<T, 2>::col(const int i)
    {
        return *(&c1 + i);
    }

    template <Floating T>
    inline vec2<T> mat<T, 2>::col(const int i) const
    {
        return *(&c1 + i);
    }

    template <Floating T>
    inline vec2<T> mat<T, 2>::row(const int i) const
    {
        return {c1[i], c2[i]};
    }

    template <Floating T>
    template <int i>
    inline constexpr vec2<T> mat<T, 2>::col() const noexcept
    {
        if constexpr (i == 0) return c1;
        if constexpr (i == 1) return c2;
    }

    template <Floating T>
    template <int i>
    inline constexpr vec2<T> mat<T, 2>::row() const noexcept
    {
        return {c1.template at<i>(), c2.template at<i>()};
    }

    template <Floating T>
    inline constexpr mat<T, 3>::mat(const vec3<T> c1, const vec3<T> c2, const vec3<T> c3) noexcept :
        c1(c1),
        c2(c2),
        c3(c3)
    {}

    template <Floating T>
    inline constexpr mat<T, 3>::mat(
        const T x1, const T y1, const T z1,
        const T x2, const T y2, const T z2,
        const T x3, const T y3, const T z3
    ) noexcept :
        c1(x1, y1, z1),
        c2(x2, y2, z2),
        c3(x3, y3, z3)
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 3>::mat(const mat2<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2),
        c3(T(0.0), T(0.0), T(1.0))
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 3>::mat(const mat3<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2),
        c3(m.c3)
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 3>::mat(const mat4<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2),
        c3(m.c3)
    {}

    template <Floating T>
    inline mat3<T> & mat<T, 3>::operator=(const mat2<T> & m) noexcept
    {
        c1 = m.c1;
        c2 = m.c2;
        c3 = {T(0), T(0), T(1)};

        return *this;
    }

    template <Floating T>
    inline constexpr mat<T, 3>::operator bool() const noexcept
    {
        return *this == mat{};
    }

    template <Floating T>
    inline vec3<T> & mat<T, 3>::col(const int i)
    {
        return *(&c1 + i);
    }

    template <Floating T>
    inline vec3<T> mat<T, 3>::col(const int i) const
    {
        return *(&c1 + i);
    }

    template <Floating T>
    inline vec3<T> mat<T, 3>::row(const int i) const
    {
        return {c1[i], c2[i], c3[i]};
    }

    template <Floating T>
    template <int i>
    inline constexpr vec3<T> mat<T, 3>::col() const noexcept
    {
        if constexpr (i == 0) return c1;
        if constexpr (i == 1) return c2;
        if constexpr (i == 2) return c3;
    }

    template <Floating T>
    template <int i>
    inline constexpr vec3<T> mat<T, 3>::row() const noexcept
    {
        return {c1.template at<i>(), c2.template at<i>(), c3.template at<i>()};
    }

    template <Floating T>
    inline constexpr mat<T, 4>::mat(const vec4<T> c1, const vec4<T> c2, const vec4<T> c3, const vec4<T> c4) noexcept :
        c1(c1),
        c2(c2),
        c3(c3),
        c4(c4)
    {}

    template <Floating T>
    inline constexpr mat<T, 4>::mat(
        const T x1, const T y1, const T z1, const T w1,
        const T x2, const T y2, const T z2, const T w2,
        const T x3, const T y3, const T z3, const T w3,
        const T x4, const T y4, const T z4, const T w4
    ) noexcept :
        c1(x1, y1, z1, w1),
        c2(x2, y2, z2, w2),
        c3(x3, y3, z3, w3),
        c4(x4, y4, z4, w4)
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 4>::mat(const mat2<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2),
        c3(T(0.0), T(0.0), T(1.0), T(0.0)),
        c4(T(0.0), T(0.0), T(0.0), T(1.0))
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 4>::mat(const mat3<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2),
        c3(m.c3),
        c4(T(0.0), T(0.0), T(0.0), T(1.0))
    {}

    template <Floating T>
    template <Floating U>
    inline constexpr mat<T, 4>::mat(const mat4<U> & m) noexcept :
        c1(m.c1),
        c2(m.c2),
        c3(m.c3),
        c4(m.c4)
    {}

    template <Floating T>
    inline mat4<T> & mat<T, 4>::operator=(const mat2<T> & m) noexcept
    {
        c1 = m.c1;
        c2 = m.c2;
        c3 = {T(0), T(0), T(1), T(0)};
        c4 = {T(0), T(0), T(0), T(1)};

        return *this;
    }

    template <Floating T>
    inline mat4<T> & mat<T, 4>::operator=(const mat3<T> & m) noexcept
    {
        c1 = m.c1;
        c2 = m.c2;
        c3 = m.c3;
        c4 = {T(0), T(0), T(0), T(1)};

        return *this;
    }

    template <Floating T>
    inline constexpr mat<T, 4>::operator bool() const noexcept
    {
        return *this == mat{};
    }

    template <Floating T>
    inline vec4<T> & mat<T, 4>::col(const int i)
    {
        return *(&c1 + i);
    }

    template <Floating T>
    inline vec4<T> mat<T, 4>::col(const int i) const
    {
        return *(&c1 + i);
    }

    template <Floating T>
    inline vec4<T> mat<T, 4>::row(const int i) const
    {
        return {c1[i], c2[i], c3[i], c4[i]};
    }

    template <Floating T>
    template <int i>
    inline constexpr vec4<T> mat<T, 4>::col() const noexcept
    {
        if constexpr (i == 0) return c1;
        if constexpr (i == 1) return c2;
        if constexpr (i == 2) return c3;
        if constexpr (i == 3) return c4;
    }

    template <Floating T>
    template <int i>
    inline constexpr vec4<T> mat<T, 4>::row() const noexcept
    {
        return {c1.template at<i>(), c2.template at<i>(), c3.template at<i>(), c4.template at<i>()};
    }

    template <Floating T, int n>
    inline mat<T, n> & operator+=(mat<T, n> & m, const T v)
    {
        if constexpr (n >= 1) m.c1 += v;
        if constexpr (n >= 2) m.c2 += v;
        if constexpr (n >= 3) m.c3 += v;
        if constexpr (n >= 4) m.c4 += v;

        return m;
    }

    template <Floating T, int n>
    inline mat<T, n> & operator+=(mat<T, n> & m1, const mat<T, n> & m2)
    {
        if constexpr (n >= 1) m1.c1 += m2.c1;
        if constexpr (n >= 2) m1.c2 += m2.c2;
        if constexpr (n >= 3) m1.c3 += m2.c3;
        if constexpr (n >= 4) m1.c4 += m2.c4;

        return m1;
    }

    template <Floating T, int n>
    inline mat<T, n> & operator-=(mat<T, n> & m, const T v)
    {
        if constexpr (n >= 1) m.c1 -= v;
        if constexpr (n >= 2) m.c2 -= v;
        if constexpr (n >= 3) m.c3 -= v;
        if constexpr (n >= 4) m.c4 -= v;

        return m;
    }

    template <Floating T, int n>
    inline mat<T, n> & operator-=(mat<T, n> & m1, const mat<T, n> & m2)
    {
        if constexpr (n >= 1) m1.c1 -= m2.c1;
        if constexpr (n >= 2) m1.c2 -= m2.c2;
        if constexpr (n >= 3) m1.c3 -= m2.c3;
        if constexpr (n >= 4) m1.c4 -= m2.c4;

        return m1;
    }

    template <Floating T, int n>
    inline mat<T, n> & operator*=(mat<T, n> & m, const T v)
    {
        if constexpr (n >= 1) m.c1 *= v;
        if constexpr (n >= 2) m.c2 *= v;
        if constexpr (n >= 3) m.c3 *= v;
        if constexpr (n >= 4) m.c4 *= v;

        return m;
    }

    template <Floating T, int n>
    inline mat<T, n> & operator*=(mat<T, n> & m1, const mat<T, n> & m2)
    {
        if constexpr (n >= 1) m1.c1 *= m2;
        if constexpr (n >= 2) m1.c2 *= m2;
        if constexpr (n >= 3) m1.c3 *= m2;
        if constexpr (n >= 4) m1.c4 *= m2;

        return m1;
    }

    template <Floating T, int n>
    inline vec<T, n> & operator*=(vec<T, n> & v, const mat<T, n> & m)
    {
        return v = m * v;
    }

    template <Floating T, int n>
    inline mat<T, n> & operator/=(mat<T, n> & m, const T v)
    {
        return m *= T(1.0) / v;
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator+(const mat<T, n> & m)
    {
        return m;
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator-(const mat<T, n> & m)
    {
        if constexpr (n == 2) return {-m.c1, -m.c2};
        if constexpr (n == 3) return {-m.c1, -m.c2, -m.c3};
        if constexpr (n == 4) return {-m.c1, -m.c2, -m.c3, -m.c4};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator+(const mat<T, n> & m1, const mat<T, n> & m2)
    {
        if constexpr (n == 2) return {m1.c1 + m2.c1, m1.c2 + m2.c2};
        if constexpr (n == 3) return {m1.c1 + m2.c1, m1.c2 + m2.c2, m1.c3 + m2.c3};
        if constexpr (n == 4) return {m1.c1 + m2.c1, m1.c2 + m2.c2, m1.c3 + m2.c3, m1.c4 + m2.c4};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator+(const mat<T, n> & m, const T v)
    {
        if constexpr (n == 2) return {m.c1 + v, m.c2 + v};
        if constexpr (n == 3) return {m.c1 + v, m.c2 + v, m.c3 + v};
        if constexpr (n == 4) return {m.c1 + v, m.c2 + v, m.c3 + v, m.c4 + v};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator+(const T v, const mat<T, n> & m)
    {
        return m + v;
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator-(const mat<T, n> & m1, const mat<T, n> & m2)
    {
        if constexpr (n == 2) return {m1.c1 - m2.c1, m1.c2 - m2.c2};
        if constexpr (n == 3) return {m1.c1 - m2.c1, m1.c2 - m2.c2, m1.c3 - m2.c3};
        if constexpr (n == 4) return {m1.c1 - m2.c1, m1.c2 - m2.c2, m1.c3 - m2.c3, m1.c4 - m2.c4};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator-(const mat<T, n> & m, const T v)
    {
        if constexpr (n == 2) return {m.c1 - v, m.c2 - v};
        if constexpr (n == 3) return {m.c1 - v, m.c2 - v, m.c3 - v};
        if constexpr (n == 4) return {m.c1 - v, m.c2 - v, m.c3 - v, m.c4 - v};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator-(const T v, const mat<T, n> & m)
    {
        if constexpr (n == 2) return {v - m.c1, v - m.c2};
        if constexpr (n == 3) return {v - m.c1, v - m.c2, v - m.c3};
        if constexpr (n == 4) return {v - m.c1, v - m.c2, v - m.c3, v - m.c4};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator*(const mat<T, n> & m1, const mat<T, n> & m2)
    {
        if constexpr (n == 2) return {
            m1.c1.x * m2.c1.x + m1.c2.x * m2.c1.y,
            m1.c1.y * m2.c1.x + m1.c2.y * m2.c1.y,

            m1.c1.x * m2.c2.x + m1.c2.x * m2.c2.y,
            m1.c1.y * m2.c2.x + m1.c2.y * m2.c2.y
        };
        if constexpr (n == 3) return {
            m1.c1.x * m2.c1.x + m1.c2.x * m2.c1.y + m1.c3.x * m2.c1.z,
            m1.c1.y * m2.c1.x + m1.c2.y * m2.c1.y + m1.c3.y * m2.c1.z,
            m1.c1.z * m2.c1.x + m1.c2.z * m2.c1.y + m1.c3.z * m2.c1.z,

            m1.c1.x * m2.c2.x + m1.c2.x * m2.c2.y + m1.c3.x * m2.c2.z,
            m1.c1.y * m2.c2.x + m1.c2.y * m2.c2.y + m1.c3.y * m2.c2.z,
            m1.c1.z * m2.c2.x + m1.c2.z * m2.c2.y + m1.c3.z * m2.c2.z,

            m1.c1.x * m2.c3.x + m1.c2.x * m2.c3.y + m1.c3.x * m2.c3.z,
            m1.c1.y * m2.c3.x + m1.c2.y * m2.c3.y + m1.c3.y * m2.c3.z,
            m1.c1.z * m2.c3.x + m1.c2.z * m2.c3.y + m1.c3.z * m2.c3.z
        };
        if constexpr (n == 4) return {
            m1.c1.x * m2.c1.x + m1.c2.x * m2.c1.y + m1.c3.x * m2.c1.z + m1.c4.x * m2.c1.w,
            m1.c1.y * m2.c1.x + m1.c2.y * m2.c1.y + m1.c3.y * m2.c1.z + m1.c4.y * m2.c1.w,
            m1.c1.z * m2.c1.x + m1.c2.z * m2.c1.y + m1.c3.z * m2.c1.z + m1.c4.z * m2.c1.w,
            m1.c1.w * m2.c1.x + m1.c2.w * m2.c1.y + m1.c3.w * m2.c1.z + m1.c4.w * m2.c1.w,

            m1.c1.x * m2.c2.x + m1.c2.x * m2.c2.y + m1.c3.x * m2.c2.z + m1.c4.x * m2.c2.w,
            m1.c1.y * m2.c2.x + m1.c2.y * m2.c2.y + m1.c3.y * m2.c2.z + m1.c4.y * m2.c2.w,
            m1.c1.z * m2.c2.x + m1.c2.z * m2.c2.y + m1.c3.z * m2.c2.z + m1.c4.z * m2.c2.w,
            m1.c1.w * m2.c2.x + m1.c2.w * m2.c2.y + m1.c3.w * m2.c2.z + m1.c4.w * m2.c2.w,

            m1.c1.x * m2.c3.x + m1.c2.x * m2.c3.y + m1.c3.x * m2.c3.z + m1.c4.x * m2.c3.w,
            m1.c1.y * m2.c3.x + m1.c2.y * m2.c3.y + m1.c3.y * m2.c3.z + m1.c4.y * m2.c3.w,
            m1.c1.z * m2.c3.x + m1.c2.z * m2.c3.y + m1.c3.z * m2.c3.z + m1.c4.z * m2.c3.w,
            m1.c1.w * m2.c3.x + m1.c2.w * m2.c3.y + m1.c3.w * m2.c3.z + m1.c4.w * m2.c3.w,

            m1.c1.x * m2.c4.x + m1.c2.x * m2.c4.y + m1.c3.x * m2.c4.z + m1.c4.x * m2.c4.w,
            m1.c1.y * m2.c4.x + m1.c2.y * m2.c4.y + m1.c3.y * m2.c4.z + m1.c4.y * m2.c4.w,
            m1.c1.z * m2.c4.x + m1.c2.z * m2.c4.y + m1.c3.z * m2.c4.z + m1.c4.z * m2.c4.w,
            m1.c1.w * m2.c4.x + m1.c2.w * m2.c4.y + m1.c3.w * m2.c4.z + m1.c4.w * m2.c4.w
        };
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator*(const mat<T, n> & m, const T v)
    {
        if constexpr (n == 2) return {m.c1 * v, m.c2 * v};
        if constexpr (n == 3) return {m.c1 * v, m.c2 * v, m.c3 * v};
        if constexpr (n == 4) return {m.c1 * v, m.c2 * v, m.c3 * v, m.c4 * v};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator*(const T v, const mat<T, n> & m)
    {
        return m * v;
    }

    template <Floating T, int n>
    inline constexpr vec<T, n> operator*(const mat<T, n> & m, const vec<T, n> & v)
    {
        if constexpr (n == 2) return {
            m.c1.x * v.x + m.c2.x * v.y,
            m.c1.y * v.x + m.c2.y * v.y
        };
        if constexpr (n == 3) return {
            m.c1.x * v.x + m.c2.x * v.y + m.c3.x * v.z,
            m.c1.y * v.x + m.c2.y * v.y + m.c3.y * v.z,
            m.c1.z * v.x + m.c2.z * v.y + m.c3.z * v.z
        };
        if constexpr (n == 4) return {
            m.c1.x * v.x + m.c2.x * v.y + m.c3.x * v.z + m.c4.x * v.w,
            m.c1.y * v.x + m.c2.y * v.y + m.c3.y * v.z + m.c4.y * v.w,
            m.c1.z * v.x + m.c2.z * v.y + m.c3.z * v.z + m.c4.z * v.w,
            m.c1.w * v.x + m.c2.w * v.y + m.c3.w * v.z + m.c4.w * v.w
        };
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator/(const mat<T, n> & m, const T v)
    {
        return m * (T(1.0) / v);
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> operator/(const T v, const mat<T, n> & m)
    {
        if constexpr (n == 2) return {v / m.c1, v / m.c2};
        if constexpr (n == 3) return {v / m.c1, v / m.c2, v / m.c3};
        if constexpr (n == 4) return {v / m.c1, v / m.c2, v / m.c3, v / m.c4};
    }

    template <Floating T, int n>
    inline constexpr bool operator==(const mat<T, n> & m1, const mat<T, n> & m2)
    {
        if constexpr (n == 2) return m1.c1 == m2.c1 && m1.c2 == m2.c2;
        if constexpr (n == 3) return m1.c1 == m2.c1 && m1.c2 == m2.c2 && m1.c3 == m2.c3;
        if constexpr (n == 4) return m1.c1 == m2.c1 && m1.c2 == m2.c2 && m1.c3 == m2.c3 && m1.c4 == m2.c4;
    }

    template <Floating T, int n>
    inline constexpr bool operator!=(const mat<T, n> & m1, const mat<T, n> & m2)
    {
        return !(m1 == m2);
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> fullMat(const T v)
    {
        if constexpr (n == 2) return {vec2<T>(v), vec2<T>(v)};
        if constexpr (n == 3) return {vec3<T>(v), vec3<T>(v), vec3<T>(v)};
        if constexpr (n == 4) return {vec4<T>(v), vec4<T>(v), vec4<T>(v), vec4<T>(v)};
    }

    template <Floating T, int n>
    inline constexpr mat<T, n> nullMat()
    {
        return fullMat<T, n>(T(0.0));
    }
}
