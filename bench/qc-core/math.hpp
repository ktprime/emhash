#pragma once

#include <cmath>

#include <qc-core/matrix-ext.hpp>

namespace qc
{
    template <Floating T>
    inline vec2<T> polarToCartesian(const T theta)
    {
        return {
            std::cos(theta),
            std::sin(theta)
        };
    }

    template <Floating T>
    inline vec2<T> polarToCartesian(const T r, const T theta)
    {
        return r * polarToCartesian(theta);
    }

    //
    // `v.x` is the radius
    // `v.y` is theta
    //
    template <Floating T>
    inline vec2<T> polarToCartesian(const vec2<T> v)
    {
        return polarToCartesian(v.x, v.y);
    }

    template <Floating T>
    inline vec2<T> cartesianToPolar(const vec2<T> v)
    {
        return {
            magnitude(v),
            std::atan2(v.y, v.x)
        };
    }

    //
    // `theta` is the angle on the xy plane
    // `phi` is the angle from the z axis
    //
    template <Floating T>
    inline vec3<T> sphericalToCartesian(const T theta, const T phi)
    {
        const T sinPhi{std::sin(phi)};
        return {
            sinPhi * std::cos(theta),
            sinPhi * std::sin(theta),
            std::cos(phi)
        };
    }

    //
    // `r` is the radius
    // `theta` is the angle on the xy plane
    // `phi` is the angle from the z axis
    //
    template <Floating T>
    inline vec3<T> sphericalToCartesian(const T r, const T theta, const T phi)
    {
        return r * sphericalToCartesian(theta, phi);
    }

    //
    // `v.x` is the radius
    // `v.y` is theta, or the angle on the xy plane
    // `v.z` is the angle from the z axis
    //
    template <Floating T>
    inline vec3<T> sphericalToCartesian(const vec3<T> v)
    {
        return sphericalToCartesian(v.y, v.z) * v.r;
    }

    template <Floating T>
    inline vec3<T> cartesianToSpherical(const vec3<T> v)
    {
        const T r{magnitude(v)};
        return {
            r,
            std::atan2(v.y, v.x),
            std::acos(v.z / r)
        };
    }

    template <Floating T>
    inline vec3<T> cylindricalToCartesian(const T r, const T theta, const T z)
    {
        return {
            polarToCartesian(r, theta),
            z
        };
    }

    template <Floating T>
    inline vec3<T> cylindricalToCartesian(const vec3<T> v)
    {
        return cylindricalToCartesian(v.x, v.y, v.z);
    }

    template <Floating T>
    inline vec3<T> cartesianToCylindrical(const vec3<T> v)
    {
        return {
            magnitude(v),
            std::atan2(v.y, v.x),
            v.z
        };
    }

    //a is distance from vertex A in range [0, 1] (can be outside range and outside triangle), AX, AY, and AZ define cartesian position of A
    inline fvec2 barycentricToCartesian(const fvec3 v, const fvec2 A, const fvec2 B, const fvec2 C)
    {
        return fvec2(
            v.x * A.x + v.y * B.x + v.z * C.x,
            v.x * A.y + v.y * B.y + v.z * C.y
        );
    }

    inline fvec3 cartesianToBarycentric(const fvec2 v, const fvec2 A, const fvec2 B, const fvec2 C)
    {
        fmat2 mat(
            A.x - C.x, A.y - C.y,
            B.x - C.x, B.y - C.y
        );
        mat = inverse(mat);
        fvec3 bary(mat * (v - C));
        bary.z = 1.0f - bary.x - bary.y;
        return bary;
    }

    //maps a point in cartesian space to the surface of a sphere and returns cartesian coordinates
    //the point's x and y components, along with thetaPerUnit, determine the distance in radians from the origin
    //the point's z component determines the radius
    inline fvec3 mapToSphere(const fvec3 v, const float thetaPerUnit)
    {
        const fvec2 perp(ortho(v.xy()));
        const float theta(magnitude(v.xy()) * thetaPerUnit);
        const fmat3 rot(rotate(fvec3(perp), theta));

        return rot * fvec3(0.0f, 0.0f, v.z);
    }

    //draw a line from v to A; gets the angle of this line w/ respect to the A bisector
    //possible angles range from -1 (along AB side) to 1 (along AC side), w/ linear-ness
    inline float baryToAngleA(const fvec3 v)
    {
        return (v.z - v.y) / v.x;
    }

    inline float baryToAngleB(const fvec3 v)
    {
        return (v.x - v.z) / v.y;
    }

    inline float baryToAngleC(const fvec3 v)
    {
        return (v.y - v.x) / v.z;
    }

    //given a bary angle, return point along given a corresponding to angle
    inline fvec3 baryFromAngleA(const float angle, const float a)
    {
        fvec3 v;
        v.x = a;
        v.z = (angle - 1.0f) * (a - 1.0f);
        v.y = 1.0f - v.x - v.z;
        return v;
    }

    inline fvec3 baryFromAngleB(const float angle, const float b)
    {
        fvec3 v;
        v.y = b;
        v.x = (angle - 1.0f) * (b - 1.0f);
        v.z = 1.0f - v.y - v.x;
        return v;
    }

    inline fvec3 baryFromAngleC(const float angle, const float c)
    {
        fvec3 v;
        v.z = c;
        v.y = (angle - 1.0f) * (c - 1.0f);
        v.x = 1.0f - v.z - v.y;
        return v;
    }

    //
    // Returns a uniformly distributed point on the perimeter of the unit circle given the random input `v`.
    // `v` should have a linear distribution in range [0, 1].
    //
    template <Floating T>
    inline vec2<T> discPoint(const T v)
    {
        return polarToCartesian((T(2.0) * std::numbers::pi_v<T>) * v);
    }

    //
    // Returns a uniformly distributed point in the unit circle given the random input `v`.
    // `v` should have a linear distribution in range [0, 1].
    //
    template <Floating T>
    inline vec2<T> circlePoint(const vec2<T> v)
    {
        return std::sqrt(v.x) * discPoint(v.y);
    }

    //
    // Returns a uniformly distributed point on the surface of the unit sphere given the random input `v`.
    // `v` should have a linear distribution in range [0, 1].
    //
    template <Floating T>
    inline vec3<T> spherePoint(const vec2<T> v)
    {
        return sphericalToCartesian(
            (T(2.0) * std::numbers::pi_v<T>) * v.x,
            std::acos(T(1.0) - T(2.0) * v.y)
        );
    }

    //
    // Returns a uniformly distributed point in the unit sphere given the random input `v`.
    // `v` should have a linear distribution in range [0, 1].
    //
    template <Floating T>
    inline vec3<T> ballPoint(const vec3<T> v)
    {
        return std::cbrt(v.x) * spherePoint(v.yz());
    }

    template <Floating T>
    inline vec2<T> circlePointFibonacci(const int i, const int n)
    {
        const T p{T(i + 1) / T(n + 1)};
        return circlePoint(vec2<T>(p, std::numbers::phi_v<T> * T(i)));
    }

    template <Floating T>
    inline vec3<T> spherePointFibonacci(const int i, const int n)
    {
        const T p{T(i + 1) / T(n + 1)};
        return spherePoint(vec2<T>(std::numbers::phi_v<T> * T(i), p));
    }

    template <Floating T>
    struct Dampener
    {
        const T angularFreq, dampingRatio, dt;

        Dampener(const T angularFreq, const T dampingRatio, const T dt) :
            angularFreq(angularFreq),
            dampingRatio(dampingRatio),
            dt(dt)
        {}

        virtual ~Dampener() = default;
    };

    template <Floating T>
    struct OverDampener : public Dampener<T>
    {
        const T za, zb, z0, z1, z2, expTerm1, expTerm2;

        OverDampener(const T angularFreq, const T dampingRatio, const T dt) :
            Dampener<T>(angularFreq, max(dampingRatio, T(1.001)), dt),
            za(-this->angularFreq * this->dampingRatio),
            zb(this->angularFreq * std::sqrt(this->dampingRatio * this->dampingRatio - T(1.0))),
            z0(T(1.0) / (T(-2.0) * zb)),
            z1(za - zb),
            z2(za + zb),
            expTerm1(std::exp(z1 * this->dt)),
            expTerm2(std::exp(z2 * this->dt))
        {}

        template <Floating U>
        void dampen(U & pos, U & vel, const U & targetPos) const {
            const U dp{pos - targetPos};
            const U c1{(vel - dp * z2) * z0};
            const U c2{dp - c1};
            pos = targetPos + c1 * expTerm1 + c2 * expTerm2;
            vel = c1 * z1 * expTerm1 + c2 * z2 * expTerm2;
        }
    };

    template <Floating T>
    struct CriticalDampener : public Dampener<T>
    {
        const T expTerm;

        CriticalDampener(const T angularFreq, const T dt) :
            Dampener<T>(angularFreq, T(1.0), dt),
            expTerm(std::exp(-this->angularFreq * this->dt))
        {}

        template <Floating U>
        void dampen(U & pos, U & vel, const U & targetPos) const {
            const U dp{pos - targetPos};
            const U c1{vel + this->angularFreq * dp};
            const U c2{(c1 * this->dt + dp) * expTerm};
            pos = targetPos + c2;
            vel = c1 * expTerm - c2 * this->angularFreq;
        }
    };

    template <Floating T, typename U>
    inline void dampen(U & r_pos, const U & targetPos, U & r_vel, const T angularFreq, const T dt)
    {
        const U dist{r_pos - targetPos};
        dampen(dist, r_vel, angularFreq, dt);
        r_pos = targetPos - dist;
    }

    template <Floating T, typename U>
    inline void dampen(U & r_dist, U & r_vel, const T angularFreq, const T dt)
    {
        const T expTerm{std::exp(-angularFreq * dt)};
        const U c1{r_vel + angularFreq * r_dist};
        const U c2{(c1 * dt + r_dist) * expTerm};
        r_dist = -c2;
        r_vel = c1 * expTerm - c2 * angularFreq;
    }

    template <Floating T>
    struct UnderDampener : public Dampener<T>
    {
        const T omegaZeta, alpha, expTerm, cosTerm, sinTerm;

        UnderDampener(const T angularFreq, const T dampingRatio, const T dt) :
            Dampener<T>(angularFreq, min(dampingRatio, T(0.999)), dt),
            omegaZeta(this->angularFreq * this->dampingRatio),
            alpha(this->angularFreq * std::sqrt(T(1.0) - this->dampingRatio * this->dampingRatio)),
            expTerm(std::exp(-omegaZeta * dt)),
            cosTerm(std::cos(alpha * this->dt)),
            sinTerm(std::sin(alpha * this->dt))
        {}

        template <Floating U>
        void dampen(U & pos, U & vel, const U & targetPos) const
        {
            const U dp{pos - targetPos};
            const U c{(vel + omegaZeta * dp) / alpha};
            pos = targetPos + expTerm * (dp * cosTerm + c * sinTerm);
            vel = -expTerm * ((dp * omegaZeta - c * alpha) * cosTerm + (dp * alpha + c * omegaZeta) * sinTerm);
        }
    };

    // Calculates the area of a non self-intersecting polygon
    // Points should be the vertices of the polygon given in order without duplicates
    template <Floating T>
    inline T areaOfPoly(const size_t n, const vec2<T> * const points)
    {
        T a{};
        for (size_t i{0u}; i < n - 1u; ++i) {
            const vec2<T> v1(points[i]), v2(points[i + 1u]);
            a += v1.x * v2.y - v2.x * v1.y;
        }
        const vec2<T> v1(points[n - 1u]), v2(points[0]);
        a += v1.x * v2.y - v2.x * v1.y;
        return T(0.5) * a;
    }

    // Calculates the centroid, or center of mass, of a non self-intersecting polygon
    // Points should be the vertices of the polygon given in order without duplicates
    template <Floating T>
    inline vec2<T> centroidOfPoly(const size_t n, const vec2<T> * const points)
    {
        T a{};
        vec2<T> c;
        for (size_t i{0u}; i < n - 1u; ++i) {
            const vec2<T> v1(points[i]), v2(points[i + 1u]);
            T temp{v1.x * v2.y - v2.x * v1.y};
            a += temp;
            c += (v1 + v2) * temp;
        }
        const vec2<T> v1(points[n - 1u]), v2(points[0]);
        const T temp{v1.x * v2.y - v2.x * v1.y};
        a += temp;
        c += (v1 + v2) * temp;
        return c / (T(3.0) * a);
    }
}
