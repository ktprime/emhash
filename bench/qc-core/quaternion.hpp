#pragma once

#include <qc-core/matrix.hpp>

namespace qc
{
    template <Floating T> struct quat;

    inline namespace types
    {
        using qc::quat;

        using fquat = quat<float>;
        using dquat = quat<double>;

        template <typename T> concept Quaternion = std::is_same_v<T, quat<typename T::Type>>;
    }

    template <Floating T> struct quat
    {
        using Type = T;

        vec3<T> a{};
        T w{T(1.0)};

        constexpr quat() noexcept = default;
        template <Floating U> constexpr explicit quat(const quat<U> & q) noexcept;
        constexpr quat(vec3<T> a, T w) noexcept;
        constexpr explicit quat(vec3<T> v) noexcept;
        constexpr explicit quat(vec4<T> v) noexcept;

        constexpr quat(const quat & q) noexcept = default;
        constexpr quat(quat && q) noexcept = default;

        quat & operator=(const quat & q) noexcept = default;
        quat & operator=(quat && q) noexcept = default;

        ~quat() noexcept = default;

        constexpr explicit operator bool() const noexcept;
    };

    template <Floating T> quat<T> & operator+=(quat<T> & q1, const quat<T> & q2);

    template <Floating T> quat<T> & operator-=(quat<T> & q1, const quat<T> & q2);

    template <Floating T> quat<T> & operator*=(quat<T> & q1, const quat<T> & q2);
    template <Floating T> quat<T> & operator*=(quat<T> & q, T v);

    template <Floating T> quat<T> & operator/=(quat<T> & q1, const quat<T> & q2);

    template <Floating T> constexpr quat<T> operator+(const quat<T> & q);

    template <Floating T> constexpr quat<T> operator-(const quat<T> & q);

    template <Floating T> constexpr quat<T> operator+(const quat<T> & q1, const quat<T> & q2);

    template <Floating T> constexpr quat<T> operator-(const quat<T> & q1, const quat<T> & q2);

    template <Floating T> constexpr quat<T> operator*(const quat<T> & q1, const quat<T> & q2);
    template <Floating T> constexpr quat<T> operator*(const quat<T> & q, T v);
    template <Floating T> constexpr quat<T> operator*(T v, const quat<T> & q);
    template <Floating T> constexpr vec3<T> operator*(const quat<T> & q, vec3<T> v);

    template <Floating T> constexpr quat<T> operator/(const quat<T> & q1, const quat<T> & q2);

    template <Floating T> constexpr bool operator==(const quat<T> & q1, const quat<T> & q2);

    template <Floating T> constexpr bool operator!=(const quat<T> & q1, const quat<T> & q2);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <Floating T>
    template <Floating U>
    inline constexpr quat<T>::quat(const quat<U> & q) noexcept :
        a(q.a), w(T(q.w))
    {}

    template <Floating T>
    inline constexpr quat<T>::quat(const vec3<T> a, T w) noexcept :
        a(a), w(w)
    {}

    template <Floating T>
    inline constexpr quat<T>::quat(const vec3<T> v) noexcept :
        a(v), w(T(0.0))
    {}

    template <Floating T>
    inline constexpr quat<T>::quat(const vec4<T> v) noexcept :
        a(v), w(v.w)
    {}

    template <Floating T>
    inline constexpr quat<T>::operator bool() const noexcept
    {
        return !a && w == T(1.0);
    }

    template <Floating T>
    inline quat<T> & operator+=(quat<T> & q1, const quat<T> & q2)
    {
        return q1 = q1 + q2;
    }

    template <Floating T>
    inline quat<T> & operator-=(quat<T> & q1, const quat<T> & q2)
    {
        return q1 = q1 - q2;
    }

    template <Floating T>
    inline quat<T> & operator*=(quat<T> & q1, const quat<T> & q2)
    {
        return q1 = q1 * q2;
    }

    template <Floating T>
    inline quat<T> & operator*=(quat<T> & q, const T v)
    {
        return q = q * v;
    }

    template <Floating T>
    inline quat<T> & operator/=(quat<T> & q1, const quat<T> & q2)
    {
        return q1 = q1 / q2;
    }

    template <Floating T>
    inline constexpr quat<T> operator+(const quat<T> & q)
    {
        return {+q.a, +q.w};
    }

    template <Floating T>
    inline constexpr quat<T> operator-(const quat<T> & q)
    {
        return {-q.a, -q.w};
    }

    template <Floating T>
    inline constexpr quat<T> operator+(const quat<T> & q1, const quat<T> & q2)
    {
        return {q1.a + q2.a, q1.w + q2.w};
    }

    template <Floating T>
    inline constexpr quat<T> operator-(const quat<T> & q1, const quat<T> & q2)
    {
        return {q1.a - q2.a, q1.w - q2.w};
    }

    template <Floating T>
    inline constexpr quat<T> operator*(const quat<T> & q1, const quat<T> & q2)
    {
        return {
            q1.w * q2.a + q2.w * q1.a + cross(q1.a, q2.a),
            q1.w * q2.w - dot(q1.a, q2.a)
        };
    }

    template <Floating T>
    inline constexpr quat<T> operator*(const quat<T> & q, const T v)
    {
        return {q.a * v, q.w * v};
    }

    template <Floating T>
    inline constexpr quat<T> operator*(const T v, const quat<T> & q)
    {
        return {v * q.a, v * q.w};
    }

    template <Floating T>
    inline constexpr vec3<T> operator*(const quat<T> & q, const vec3<T> v)
    {
        const vec3<T> t(T(2.0) * cross(q.a, v));
        return v + q.w * t + cross(q.a, t);
    }

    template <Floating T>
    inline constexpr quat<T> operator/(const quat<T> & q1, const quat<T> & q2)
    {
        return {q1.a * q2.w - q2.a * q1.w - cross(q1.a, q2.a), dot(q1, q2)};
    }

    template <Floating T>
    inline constexpr bool operator==(const quat<T> & q1, const quat<T> & q2)
    {
        return q1.a == q2.a && q1.w == q2.w;
    }

    template <Floating T>
    inline constexpr bool operator!=(const quat<T> & q1, const quat<T> & q2)
    {
        return q1.a != q2.a || q1.w != q2.w;
    }
}
