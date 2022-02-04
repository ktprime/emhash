#pragma once

#include <qc-core/quaternion.hpp>
#include <qc-core/matrix-ext.hpp>

namespace qc
{
    //
    // ...
    //
    template <typename T> std::ostream & operator<<(std::ostream & os, const quat<T> & q);

    //
    // ...
    //
    template <typename T> T dot(const quat<T> & q1, const quat<T> & q2);

    //
    // ...
    //
    template <typename T> T magnitude(const quat<T> & q);

    //
    // ...
    //
    template <typename T> T magnitude2(const quat<T> & q);

    //
    // ...
    //
    template <typename T> quat<T> normalize(const quat<T> & q);

    //
    // ...
    //
    template <typename T> quat<T> inverse(const quat<T> & q);

    //
    // ...
    //
    template <typename T> T quatAngle(const quat<T> & q);

    //
    // ...
    //
    template <typename T> vec3<T> quatAxis(const quat<T> & q);
    template <typename T> vec3<T> quatAxis_n(const quat<T> & q);

    //
    // ...
    //
    template <typename T> quat<T> mix(const quat<T> & q1, const quat<T> & q2, T t);

    //
    // ...
    //
    //template <typename T> quat<T> pow(const quat<T> & q, T t);

    //
    // ...
    //
    template <typename T> quat<T> rotateQ(vec3<T> axis, T angle);
    template <typename T> quat<T> rotateQ_n(vec3<T> axis, T angle);

    //
    // ...
    //
    template <typename T> quat<T> alignQ(vec3<T> v1, vec3<T> v2);
    template <typename T> quat<T> alignQ_n(vec3<T> v1, vec3<T> v2);

    //
    // ...
    // expects orthogonal vectors
    //
    template <typename T> quat<T> alignQ(vec3<T> forward1, vec3<T> up1, vec3<T> forward2, vec3<T> up2);
    template <typename T> quat<T> alignQ_n(vec3<T> forward1, vec3<T> up1, vec3<T> forward2, vec3<T> up2);

    //
    // ...
    // theta: thumb points up, phi: right, psi: forward
    //
    template <typename T> quat<T> eulerQ(vec3<T> forward, vec3<T> up, T theta, T phi, T psi);
    template <typename T> quat<T> eulerQ_n(vec3<T> forward, vec3<T> up, T theta, T phi, T psi);

    //
    // ...
    //
    template <typename T> mat3<T> toMat(const quat<T> & q);

    //
    // ...
    // t is a "time" value between 0 and 1
    //
    template <typename T> quat<T> nlerp(const quat<T> & q1, const quat<T> & q2, T t);

    template <typename T> quat<T> slerp(const quat<T> & q1, const quat<T> & q2, T t);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <typename T>
    inline std::ostream & operator<<(std::ostream & os, const quat<T> & q)
    {
        return os << "[ " << q.a.x << " " << q.a.y << " " << q.a.z << " | " << q.w << " ]";
    }

    template <typename T>
    inline T dot(const quat<T> & q1, const quat<T> & q2)
    {
        return dot(q1.a, q2.a) + q1.w * q2.w;
    }

    template <typename T>
    inline T magnitude(const quat<T> & q)
    {
        return std::sqrt(magnitude2(q));
    }

    template <typename T>
    inline T magnitude2(const quat<T> & q)
    {
        return magnitude2(q.a) + q.w * q.w;
    }

    template <typename T>
    inline quat<T> normalize(const quat<T> & q)
    {
        const T mag2(magnitude2(q));
        if (isZero(mag2)) {
            return {};
        }
        const T invMag(T(1.0) / std::sqrt(mag2));
        return {q.a * invMag, q.w * invMag};
    }

    template <typename T>
    inline quat<T> inverse(const quat<T> & q)
    {
        return {-q.a, q.w};
    }

    template <typename T>
    inline T quatAngle(const quat<T> & q)
    {
        return std::acos(q.w) * T(2.0);
    }

    template <typename T>
    inline vec3<T> quatAxis(const quat<T> & q)
    {
        return quatAxis_n(normalize(q));
    }

    template <typename T>
    inline vec3<T> quatAxis_n(const quat<T> & q)
    {
        const T d2{T(1.0) - q.w * q.w};
        if (isZero(d2)) {
            return {};
        }
        return q.a * (T(1.0) / std::sqrt(d2));
    }

    template <typename T>
    inline quat<T> mix(const quat<T> & q1, const quat<T> & q2, const T t)
    {
        const T s{T(1.0) - t};
        return {s * q1.a + t * q2.a, s * q1.w + t * q2.w};
    }

    //template <typename T>
    //inline quat<T> pow(const quat<T> & q, const T t)
    //{
    //    return angleAxis(angle(q) * t, axis(q));
    //}

    template <typename T>
    inline quat<T> rotateQ(const vec3<T> axis, const T angle)
    {
        return rotateQ_n(normalize(axis), angle);
    }

    template <typename T>
    inline quat<T> rotateQ_n(const vec3<T> axis, const T angle)
    {
        return {std::sin(angle * T(0.5)) * axis, std::cos(angle * T(0.5))};
    }

    template <typename T>
    inline quat<T> alignQ(const vec3<T> v1, const vec3<T> v2)
    {
        return alignQ_n(normalize(v1), normalize(v2));
    }

    template <typename T>
    inline quat<T> alignQ_n(const vec3<T> v1, const vec3<T> v2)
    {
        return rotateQ(cross(v1, v2), std::acos(dot(v1, v2)));
    }

    template <typename T>
    inline quat<T> alignQ(const vec3<T> forward1, const vec3<T> up1, const vec3<T> forward2, const vec3<T> up2)
    {
        return alignQ_n(normalize(forward1), normalize(up1), normalize(forward2), normalize(up2));
    }

    template <typename T>
    inline quat<T> alignQ_n(const vec3<T> forward1, const vec3<T> up1, const vec3<T> forward2, const vec3<T> up2)
    {
        quat<T> q(alignQ_n(forward1, forward2));
        return alignQ_n(q * up1, up2) * q;
    }

    template <typename T>
    inline quat<T> eulerQ(const vec3<T> forward, const vec3<T> up, const T theta, const T phi, const T psi)
    {
        return eulerQ_n(normalize(forward), normalize(up), theta, phi, psi);
    }

    template <typename T>
    inline quat<T> eulerQ_n(const vec3<T> forward, const vec3<T> up, const T theta, const T phi, const T psi)
    {
        return rotateQ_n(up, theta) * rotateQ_n(cross(forward, up), phi) * rotateQ_n(forward, psi);
    }

    template <typename T>
    inline mat3<T> toMat(const quat<T> & q)
    {
        const T wi{q.w   * q.a.x};
        const T wj{q.w   * q.a.y};
        const T wk{q.w   * q.a.z};
        const T ii{q.a.x * q.a.x};
        const T ij{q.a.x * q.a.y};
        const T ik{q.a.x * q.a.z};
        const T jj{q.a.y * q.a.y};
        const T jk{q.a.y * q.a.z};
        const T kk{q.a.z * q.a.z};

        return {
            T(1.0) - T(2.0) * (jj + kk), T(2.0) * (ij + wk), T(2.0) * (ik - wj),
            T(2.0) * (ij - wk), T(1.0) - T(2.0) * (ii + kk), T(2.0) * (jk + wi),
            T(2.0) * (ik + wj), T(2.0) * (jk - wi), T(1.0) - T(2.0) * (ii + jj)
        };
    }

    template <typename T>
    inline quat<T> nlerp(const quat<T> & q1, const quat<T> & q2, const T t)
    {
        return normalize(quat<T>(mix(q1, q2, t)));
    }

    template <typename T>
    inline quat<T> slerp(const quat<T> & q1, const quat<T> & q2_, const T t)
    {
        quat<T> q2(q2_);

        T cosHalfTheta{dot(q1, q2)};

        // Make sure to take the shorter route
        if (cosHalfTheta < T(0.0)) {
            cosHalfTheta = -cosHalfTheta;
            q2 = -q2;
        }

        // If parallel, no interpolation necessary
        if (areEqual(cosHalfTheta, T(1.0))) {
            return q1;
        }

        const T halfTheta{std::acos(cosHalfTheta)};
        const T sinHalfTheta{std::sqrt(T(1.0) - cosHalfTheta * cosHalfTheta)};

        return (q1 * std::sin((T(1.0) - t) * halfTheta) + q2 * std::sin(t * halfTheta)) * (T(1.0) / sinHalfTheta);
    }
}
