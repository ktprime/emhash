#pragma once

#include <iostream>

#include <qc-core/vector.hpp>
#include <qc-core/core-ext.hpp>

namespace qc
{
    //
    // ...
    //
    template <Numeric  T, int n> constexpr T sum(const vec<T, n> & v);

    //
    // ...
    //
    template <Numeric T, int n> constexpr T product(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> constexpr T average(const vec<T, n> & v);

    //
    // ...
    //
    template <int n> constexpr bool any(bvec<n> v);

    //
    // ...
    //
    template <int n> constexpr bool all(bvec<n> v);

    //
    // ...
    //
    template <typename T, int n> std::ostream & operator<<(std::ostream & os, const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> vec<T, n> pow(const vec<T, n> & v, T p);
    template <Floating T, int n> vec<T, n> pow(const vec<T, n> & v, const vec<T, n> & p);

    //
    // ...
    //
    template <Floating T, int n> vec<T, n> exp(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> T magnitude(const vec<T, n> & v);

    //
    // ...
    //
    template <Numeric T, int n> T magnitude2(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> T distance(const vec<T, n> & v1, const vec<T, n> & v2);

    //
    // ...
    //
    template <SignedNumeric T, int n> T distance2(const vec<T, n> & v1, const vec<T, n> & v2);

    //
    // ...
    //
    template <Floating T, int n> vec<T, n> normalize(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> vec<T, n> & normalizeAssign(vec<T, n> & v);

    //
    // ...
    //
    template <Numeric T, int n> T dot(const vec<T, n> & v1, const vec<T, n> & v2);

    //
    // ...
    //
    template <Numeric T> T cross(vec2<T> v1, vec2<T> v2);
    template <Numeric T> vec3<T> cross(vec3<T> v1, vec3<T> v2);

    //
    // ...
    //
    template <Numeric T, int n> bool parallel(const vec<T, n> & v1, const vec<T, n> & v2);

    //
    // ...
    //
    template <Numeric T, int n> bool orthogonal(const vec<T, n> & v1, const vec<T, n> & v2);

    //
    // ...
    //
    template <SignedNumeric T> vec2<T> ortho(const vec2<T> & v);
    template <SignedNumeric T> vec3<T> ortho(const vec3<T> & v);

    //
    // ...
    //
    template <Floating T, int n> void orthogonalize(const vec<T, n> & v1, vec<T, n> & v2);
    template <Floating T> void orthogonalize(vec3<T> v1, vec3<T> & v2, vec3<T> & v3);
    template <Floating T> void orthogonalize_n(vec3<T> v1, vec3<T> & v2, vec3<T> & v3);

    //
    // ...
    //
    template <Floating T, int n> vec<T, n> reflect(const vec<T, n> & v, const vec<T, n> & norm);
    template <Floating T, int n> vec<T, n> reflect_n(const vec<T, n> & v, const vec<T, n> & norm);

    //
    // ...
    //
    template <Floating T, int n> T angle(const vec<T, n> & v1, const vec<T, n> & v2);
    template <Floating T, int n> T angle_n(const vec<T, n> & v1, const vec<T, n> & v2);

    //
    // ...
    //
    template <Numeric  T, int n> void sort(const vec<T, n> & v);

    //
    // ...
    //
    template <Numeric T, int n> constexpr vec<T, n> clamp(const vec<T, n> & v, T min, T max);
    template <Numeric T, int n> constexpr vec<T, n> clamp(const vec<T, n> & v, const vec<T, n> & min, const vec<T, n> & max);

    //
    // ...
    //
    template <Numeric T, int n> constexpr vec<T, n> abs(const vec<T, n> & v);

    //
    // ...
    //
    template <Numeric T, int n> bool isZero(const vec<T, n> & v, T e = std::numeric_limits<T>::epsilon());

    //
    // ...
    //
    template <Numeric T, int n> bool areEqual(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> bool areEqual_e(const vec<T, n> & v1, const vec<T, n> & v2, T e = std::numeric_limits<T>::epsilon());
    template <Floating T, int n> bool areEqual_e(const vec<T, n> & v, T e = std::numeric_limits<T>::epsilon());

    //
    // ...
    //
    template <Numeric T, int n> constexpr ivec<n> sign(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> vec<stype<T>, n> round(const vec<T, n> & v);
    template <Integral T, int n> vec<T, n> round(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> vec<stype<T>, n> floor(const vec<T, n> & v);
    template <Integral T, int n> vec<T, n> floor(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> vec<stype<T>, n> ceil(const vec<T, n> & v);
    template <Integral T, int n> vec<T, n> ceil(const vec<T, n> & v);

    //
    // ...
    //
    template <Floating T, int n> vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, T t);
    template <Floating T, int n> vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, vec2<T> weights);
    template <Floating T, int n> vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, const vec<T, n> & v3, vec3<T> weights);
    template <Floating T, int n> vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, const vec<T, n> & v3, const vec<T, n> & v4, vec4<T> weights);
    template <Floating T> T mix(T v1, T v2, vec2<T> weights);
    template <Floating T> T mix(T v1, T v2, T v3, vec3<T> weights);
    template <Floating T> T mix(T v1, T v2, T v3, T v4, vec4<T> weights);

    //
    // ...
    //
    template <Floating T, int n> vec<T, n> smoothstep(const vec<T, n> & v1, const vec<T, n> & v2, T t);

    //
    // Converts between normalized types.
    // Works with floats, signed, and unsigned integers.
    //
    template <Vector ToVec, Numeric From, int n> requires(ToVec::n == n) ToVec transnorm(const vec<From, n> & v);

    //
    // ...
    //
    template <UnsignedIntegral T, int n> constexpr int mipmaps(const vec<T, n> & size);

    //
    // ...
    //
    template <Numeric T, int n> vec<T, n> composite(const vec<T, n> & v1, const vec<T, n> & v2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <Numeric T, int n>
    inline constexpr T sum(const vec<T, n> & v)
    {
        if constexpr (n == 2) return v.x + v.y;
        if constexpr (n == 3) return v.x + v.y + v.z;
        if constexpr (n == 4) return v.x + v.y + v.z + v.w;
    }

    template <Numeric T, int n>
    inline constexpr T product(const vec<T, n> & v)
    {
        if constexpr (n == 2) return v.x * v.y;
        if constexpr (n == 3) return v.x * v.y * v.z;
        if constexpr (n == 4) return v.x * v.y * v.z * v.w;
    }

    template <Floating T, int n>
    inline constexpr T average(const vec<T, n> & v)
    {
        if constexpr (n == 2) return (v.x + v.y) * T(0.5);
        if constexpr (n == 3) return (v.x + v.y + v.z) * T(1.0 / 3.0);
        if constexpr (n == 4) return (v.x + v.y + v.z + v.w) * T(0.25);
    }

    template <int n>
    inline constexpr bool all(const bvec<n> v)
    {
        if constexpr (n == 2) return v.x && v.y;
        if constexpr (n == 3) return v.x && v.y && v.z;
        if constexpr (n == 4) return v.x && v.y && v.z && v.w;
    }

    template <int n>
    inline constexpr bool any(const bvec<n> v)
    {
        return bool(v);
    }

    template <typename T, int n>
    inline std::ostream & operator<<(std::ostream & os, const vec<T, n> & v)
    {
        os << "[";
        if constexpr (n >= 1) os << v.x << " ";
        if constexpr (n >= 2) os << " " << v.y;
        if constexpr (n >= 3) os << " " << v.z;
        if constexpr (n >= 4) os << " " << v.w;
        os << "]";
        return os;
    }

    template <Floating T, int n>
    inline vec<T, n> pow(const vec<T, n> & v, const T p)
    {
        if constexpr (n == 2) return {std::pow(v.x, p), std::pow(v.y, p)};
        if constexpr (n == 3) return {std::pow(v.x, p), std::pow(v.y, p), std::pow(v.z, p)};
        if constexpr (n == 4) return {std::pow(v.x, p), std::pow(v.y, p), std::pow(v.z, p), std::pow(v.w, p)};
    }

    template <Floating T, int n>
    inline vec<T, n> pow(const vec<T, n> & v, const vec<T, n> & p)
    {
        if constexpr (n == 2) return {std::pow(v.x, p.x), std::pow(v.y, p.y)};
        if constexpr (n == 3) return {std::pow(v.x, p.x), std::pow(v.y, p.y), std::pow(v.z, p.z)};
        if constexpr (n == 4) return {std::pow(v.x, p.x), std::pow(v.y, p.y), std::pow(v.z, p.z), std::pow(v.w, p.w)};
    }

    template <Floating T, int n>
    inline vec<T, n> exp(const vec<T, n> & v)
    {
        if constexpr (n == 2) return {std::exp(v.x), std::exp(v.y)};
        if constexpr (n == 3) return {std::exp(v.x), std::exp(v.y), std::exp(v.z)};
        if constexpr (n == 4) return {std::exp(v.x), std::exp(v.y), std::exp(v.z), std::exp(v.w)};
    }

    template <Floating T, int n>
    inline T magnitude(const vec<T, n> & v)
    {
        return std::sqrt(magnitude2(v));
    }

    template <Numeric T, int n>
    inline T magnitude2(const vec<T, n> & v)
    {
        if constexpr (n == 2) return v.x * v.x + v.y * v.y;
        if constexpr (n == 3) return v.x * v.x + v.y * v.y + v.z * v.z;
        if constexpr (n == 4) return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
    }

    template <Floating T, int n>
    inline T distance(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        return std::sqrt(distance2(v1, v2));
    }

    template <SignedNumeric T, int n>
    inline T distance2(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        return magnitude2(v2 - v1);
    }

    template <Floating T, int n>
    inline vec<T, n> normalize(const vec<T, n> & v)
    {
        const T m2{magnitude2(v)};
        if (isZero(m2)) {
            return {};
        }
        return v / std::sqrt(m2);
    }

    template <Floating T, int n>
    inline vec<T, n> & normalizeAssign(vec<T, n> & v)
    {
        const T m2{magnitude2(v)};
        if (isZero(m2)) {
            return v = {};
        }
        return v /= std::sqrt(m2);
    }

    template <Numeric T, int n>
    inline T dot(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        if constexpr (n == 2) return v1.x * v2.x + v1.y * v2.y;
        if constexpr (n == 3) return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
        if constexpr (n == 4) return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
    }

    template <Numeric T>
    inline T cross(const vec2<T> v1, const vec2<T> v2)
    {
        return v1.x * v2.y - v1.y * v2.x;
    }

    template <Numeric T>
    inline vec3<T> cross(const vec3<T> v1, const vec3<T> v2)
    {
        return {T(v1.y * v2.z - v1.z * v2.y), T(v1.z * v2.x - v1.x * v2.z), T(v1.x * v2.y - v1.y * v2.x)};
    }

    template <Numeric T, int n>
    inline bool parallel(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        const T d{dot(v1, v2)};
        return areEqual(d * d, magnitude2(v1) * magnitude2(v2));
    }

    template <Numeric T, int n>
    inline bool orthogonal(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        return isZero(dot(v1, v2));
    }

    template <SignedNumeric T>
    inline vec2<T> ortho(const vec2<T> & v)
    {
        return {T(-v.y), v.x};
    }

    template <SignedNumeric T>
    inline vec3<T> ortho(const vec3<T> & v)
    {
        const vec3<T> absV(abs(v));

        if (absV.x < absV.y && absV.x < absV.z) {
            return {T(0), T(-v.z), v.y}; // rotate around x
        }
        else if (absV.y < absV.z) {
            return {v.z, T(0), T(-v.x)}; // rotate around y
        }
        else {
            return {T(-v.y), v.x, T(0)}; // rotate around z
        }
    }

    template <Floating T, int n>
    inline void orthogonalize(const vec<T, n> & v1, vec<T, n> & v2)
    {
        v2 = normalize(v2 - dot(v1, v2) * v1);
    }

    // in order of priority
    template <Floating T>
    inline void orthogonalize(const vec3<T> v1, vec3<T> & v2, vec3<T> & v3)
    {
        orthogonalize_n(v1, v2, v3);
        v2 = normalize(v2);
        v3 = normalize(v3);
    }

    // in order of priority
    template <Floating T>
    inline void orthogonalize_n(const vec3<T> v1, vec3<T> & v2, vec3<T> & v3)
    {
        orthogonalize(v1, v2);
        v3 = cross(v2, v1);
    }

    template <Floating T, int n>
    inline vec<T, n> reflect(const vec<T, n> & v, const vec<T, n> & norm)
    {
        return reflect_n(v, normalize(norm));
    }

    template <Floating T, int n>
    inline vec<T, n> reflect_n(const vec<T, n> & v, const vec<T, n> & norm)
    {
        return (T(2.0) * dot(v, norm)) * norm - v;
    }

    template <Floating T, int n>
    inline T angle(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        return angle_n(normalize(v1), normalize(v2));
    }

    template <Floating T, int n>
    inline T angle_n(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        return T(std::acos(dot(v1, v2)));
    }

    template <Numeric T, int n>
    inline void sort(vec<T, n> & v)
    {
        if constexpr (n >= 2) {
            if (v.x > v.y) std::swap(v.x, v.y);
        }
        if constexpr (n >= 3) {
            if (v.y > v.z) std::swap(v.y, v.z);
            if (v.x > v.y) std::swap(v.x, v.y);
        }
        if constexpr (n >= 4) {
            if (v.z > v.w) std::swap(v.z, v.w);
            if (v.y > v.z) std::swap(v.y, v.z);
            if (v.x > v.y) std::swap(v.x, v.y);
        }
    }

    template <Numeric T, int n>
    inline constexpr vec<T, n> clamp(const vec<T, n> & v, const T min, const T max)
    {
        if constexpr (n == 2) return {clamp(v.x, min, max), clamp(v.y, min, max)};
        if constexpr (n == 3) return {clamp(v.x, min, max), clamp(v.y, min, max), clamp(v.z, min, max)};
        if constexpr (n == 4) return {clamp(v.x, min, max), clamp(v.y, min, max), clamp(v.z, min, max), clamp(v.w, min, max)};
    }

    template <Numeric T, int n>
    inline constexpr vec<T, n> clamp(const vec<T, n> & v, const vec<T, n> & min, const vec<T, n> & max)
    {
        if constexpr (n == 2) return {clamp(v.x, min.x, max.x), clamp(v.y, min.y, max.y)};
        if constexpr (n == 3) return {clamp(v.x, min.x, max.x), clamp(v.y, min.y, max.y), clamp(v.z, min.z, max.z)};
        if constexpr (n == 4) return {clamp(v.x, min.x, max.x), clamp(v.y, min.y, max.y), clamp(v.z, min.z, max.z), clamp(v.w, min.w, max.w)};
    }

    template <Numeric T, int n>
    inline constexpr vec<T, n> abs(const vec<T, n> & v)
    {
        if constexpr (n == 2) return {abs(v.x), abs(v.y)};
        if constexpr (n == 3) return {abs(v.x), abs(v.y), abs(v.z)};
        if constexpr (n == 4) return {abs(v.x), abs(v.y), abs(v.z), abs(v.w)};
    }

    template <Numeric T, int n>
    inline bool isZero(const vec<T, n> & v, T e)
    {
        if constexpr (n == 2) return isZero(v.x, e) && isZero(v.y, e);
        if constexpr (n == 3) return isZero(v.x, e) && isZero(v.y, e) && isZero(v.z, e);
        if constexpr (n == 4) return isZero(v.x, e) && isZero(v.y, e) && isZero(v.z, e) && isZero(v.w, e);
    }

    template <Numeric T, int n>
    inline bool areEqual(const vec<T, n> & v)
    {
        if constexpr (n == 2) return v.x == v.y;
        if constexpr (n == 3) return v.x == v.y && v.x == v.z;
        if constexpr (n == 4) return v.x == v.y && v.x == v.z && v.x == v.w;
    }

    template <Floating T, int n>
    inline bool areEqual_e(const vec<T, n> & v1, const vec<T, n> & v2, const T e)
    {
        return isZero(v1 - v2, e);
    }

    template <Floating T, int n>
    inline bool areEqual_e(const vec<T, n> & v, const T e)
    {
        return isZero(v - v.x, e);
    }

    template <Numeric T, int n>
    inline constexpr ivec<n> sign(const vec<T, n> & v)
    {
        if constexpr (n == 2) return {sign(v.x), sign(v.y)};
        if constexpr (n == 3) return {sign(v.x), sign(v.y), sign(v.z)};
        if constexpr (n == 4) return {sign(v.x), sign(v.y), sign(v.z), sign(v.w)};
    }

    template <Floating T, int n>
    inline vec<stype<T>, n> floor(const vec<T, n> & v)
    {
        if constexpr (n == 2) return {floor(v.x), floor(v.y)};
        if constexpr (n == 3) return {floor(v.x), floor(v.y), floor(v.z)};
        if constexpr (n == 4) return {floor(v.x), floor(v.y), floor(v.z), floor(v.w)};
    }

    template <Integral T, int n>
    inline vec<T, n> floor(const vec<T, n> & v)
    {
        return v;
    }

    template <Floating T, int n>
    inline vec<stype<T>, n> ceil(const vec<T, n> & v)
    {
        if constexpr (n == 2) return {ceil(v.x), ceil(v.y)};
        if constexpr (n == 3) return {ceil(v.x), ceil(v.y), ceil(v.z)};
        if constexpr (n == 4) return {ceil(v.x), ceil(v.y), ceil(v.z), ceil(v.w)};
    }

    template <Integral T, int n>
    inline vec<T, n> ceil(const vec<T, n> & v)
    {
        return v;
    }

    template <Floating T, int n>
    inline vec<stype<T>, n> round(const vec<T, n> & v)
    {
        if constexpr (n == 2) return {round(v.x), round(v.y)};
        if constexpr (n == 3) return {round(v.x), round(v.y), round(v.z)};
        if constexpr (n == 4) return {round(v.x), round(v.y), round(v.z), round(v.w)};
    }

    template <Integral T, int n>
    inline vec<T, n> round(const vec<T, n> & v)
    {
        return v;
    }

    template <Floating T, int n>
    inline vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, const T t)
    {
        return (T(1.0) - t) * v1 + t * v2;
    }

    template <Floating T, int n>
    inline vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, const vec2<T> weights)
    {
        return weights.x * v1 + weights.y * v2;
    }

    template <Floating T, int n>
    inline vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, const vec<T, n> & v3, const vec3<T> weights)
    {
        return weights.x * v1 + weights.y * v2 + weights.z * v3;
    }

    template <Floating T, int n>
    inline vec<T, n> mix(const vec<T, n> & v1, const vec<T, n> & v2, const vec<T, n> & v3, const vec<T, n> & v4, const vec4<T> weights)
    {
        return weights.x * v1 + weights.y * v2 + weights.z * v3 + weights.w * v4;
    }

    template <Floating T>
    inline T mix(const T v1, const T v2, const vec2<T> weights)
    {
        return weights.x * v1 + weights.y * v2;
    }

    template <Floating T>
    inline T mix(const T v1, const T v2, const T v3, const vec3<T> weights)
    {
        return weights.x * v1 + weights.y * v2 + weights.z * v3;
    }

    template <Floating T>
    inline T mix(const T v1, const T v2, const T v3, const T v4, const vec4<T> weights)
    {
        return weights.x * v1 + weights.y * v2 + weights.z * v3 + weights.w * v4;
    }

    template <Floating T, int n>
    inline vec<T, n> smoothstep(const vec<T, n> & v1, const vec<T, n> & v2, const T t)
    {
        return mix(v1, v2, t * t * (T(3.0) - T(2.0) * t));
    }

    template <Vector ToVec, Numeric From, int n>
    requires(ToVec::n == n)
    inline ToVec transnorm(const vec<From, n> & v)
    {
        using To = typename ToVec::Type;
        if constexpr (n == 2) return {transnorm<To>(v.x), transnorm<To>(v.y)};
        if constexpr (n == 3) return {transnorm<To>(v.x), transnorm<To>(v.y), transnorm<To>(v.z)};
        if constexpr (n == 4) return {transnorm<To>(v.x), transnorm<To>(v.y), transnorm<To>(v.z), transnorm<To>(v.w)};
    }

    template <UnsignedIntegral T, int n>
    inline constexpr int mipmaps(const vec<T, n> & size)
    {
        return mipmaps(max(size));
    }

    template <Numeric T, int n>
    inline vec<T, n> composite(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        vec<T, n> v;

        if constexpr (n >= 1) {
            if      (v1.x > T(0) && v2.x > T(0)) v.x = max(v1.x, v2.x);
            else if (v1.x < T(0) && v2.x < T(0)) v.x = min(v1.x, v2.x);
            else                                 v.x = v1.x + v2.x;
        }
        if constexpr (n >= 2) {
            if      (v1.y > T(0) && v2.y > T(0)) v.y = max(v1.y, v2.y);
            else if (v1.y < T(0) && v2.y < T(0)) v.y = min(v1.y, v2.y);
            else                                 v.y = v1.y + v2.y;
        }
        if constexpr (n >= 3) {
            if      (v1.z > T(0) && v2.z > T(0)) v.z = max(v1.z, v2.z);
            else if (v1.z < T(0) && v2.z < T(0)) v.z = min(v1.z, v2.z);
            else                                 v.z = v1.z + v2.z;
        }
        if constexpr (n >= 4) {
            if      (v1.w > T(0) && v2.w > T(0)) v.w = max(v1.w, v2.w);
            else if (v1.w < T(0) && v2.w < T(0)) v.w = min(v1.w, v2.w);
            else                                 v.w = v1.w + v2.w;
        }

        return v;
    }
}
