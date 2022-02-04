#pragma once

#include <qc-core/matrix.hpp>
#include <qc-core/vector-ext.hpp>

namespace qc
{
    //
    // ...
    //
    template <typename T, int n> std::ostream & operator<<(std::ostream & os, const mat<T, n> & m);

    //
    // ...
    //
    template <typename T, int n> bool isIdentity(const mat<T, n> & m);

    //
    // ...
    //
    template <typename T, int n> mat<T, n> transpose(const mat<T, n> & m);

    //
    // ...
    //
    template <typename T, int n> mat<T, n> cofactor(const mat<T, n> & m);

    //
    // ...
    //
    template <typename T, int n> mat<T, n> adjoint(const mat<T, n> & m);

    //
    // ...
    //
    template <typename T, int n> T determinant(const mat<T, n> & m);

    //
    // ...
    //
    template <typename T, int n> mat<T, n> inverse(const mat<T, n> & m);

    //
    // ...
    //
    template <typename T, int n> mat<T, n + 1> translate(const vec<T, n> & delta);
    template <typename T, int mn, int vn> requires (mn == vn + 1) mat<T, mn> & translate(mat<T, mn> & mat, const vec<T, vn> & delta);

    //
    // ...
    //
    template <typename T, int n> mat<T, n> scale(const vec<T, n> & scale);
    template <typename T, int mn, int vn> mat<T, mn> & scale(mat<T, mn> & mat, const vec<T, vn> & scale);

    //
    // ...
    //
    template <typename T> mat2<T> rotate(T angle);

    //
    // ...
    //
    template <typename T> mat3<T> rotateX(T angle);

    //
    // ...
    //
    template <typename T> mat3<T> rotateY(T angle);

    //
    // ...
    //
    template <typename T> mat3<T> rotateZ(T angle);

    //
    // ...
    //
    template <typename T> mat3<T> rotate(vec3<T> axis, T sinTheta, T cosTheta);
    template <typename T> mat3<T> rotate_n(vec3<T> axis, T sinTheta, T cosTheta);

    //
    // ...
    //
    template <typename T> mat3<T> rotate(vec3<T> axis, T angle);
    template <typename T> mat3<T> rotate_n(vec3<T> axis, T angle);

    //
    // ...
    // theta: thumb points up, phi: right, psi: forward
    //
    template <typename T> mat3<T> euler(vec3<T> forward, vec3<T> up, T theta, T phi, T psi);
    template <typename T> mat3<T> euler_n(vec3<T> forward, vec3<T> up, T theta, T phi, T psi);

    //
    // ...
    //
    template <typename T, int n> mat<T, n> align(const vec<T, n> & v1, const vec<T, n> & v2);
    template <typename T, int n> mat<T, n> align_n(const vec<T, n> & v1, const vec<T, n> & v2);

    //
    // ...
    // expects orthogonal vectors
    //
    template <typename T> mat3<T> align(vec3<T> forward1, vec3<T> up1, vec3<T> forward2, vec3<T> up2);
    template <typename T> mat3<T> align_n(vec3<T> forward1, vec3<T> up1, vec3<T> forward2, vec3<T> up2);

    //
    // ...
    // _o variants (orthogonal) usable when the transformation matrix, T, from basis
    // A to B is orthogonal i.e. A's basis vectors don't need to be orthogonal, nor
    // B's, but the angles between A's basis vectors must be the same as B's
    //
    template <typename T> mat2<T> map(vec2<T> x1, vec2<T> y1, vec2<T> x2, vec2<T> y2);
    template <typename T> mat2<T> map_o(vec2<T> x1, vec2<T> y1, vec2<T> x2, vec2<T> y2);
    template <typename T> mat3<T> map(vec3<T> x1, vec3<T> y1, vec3<T> z1, vec3<T> x2, vec3<T> y2, vec3<T> z2);
    template <typename T> mat3<T> map_o(vec3<T> x1, vec3<T> y1, vec3<T> z1, vec3<T> x2, vec3<T> y2, vec3<T> z2);

    //
    // ...
    //
    template <typename T> mat2<T> mapTo(vec2<T> x, vec2<T> y);
    template <typename T> mat2<T> mapTo_o(vec2<T> x, vec2<T> y);
    template <typename T> mat3<T> mapTo(vec3<T> x, vec3<T> y, vec3<T> z);
    template <typename T> mat3<T> mapTo_o(vec3<T> x, vec3<T> y, vec3<T> z);

    //
    // ...
    //
    template <typename T> mat2<T> mapFrom(vec2<T> x, vec2<T> y);
    template <typename T> mat3<T> mapFrom(vec3<T> x, vec3<T> y, vec3<T> z);

    //
    // ...
    // If `depth0to1` is true, the resulting z will be in [0, 1], else [-1, 1]
    //
    template <bool depth0To1, typename T> mat4<T> orthoProj(T width, T height, T near, T far);

    //
    // ...
    // If `depth0to1` is true, the resulting z will be in [0, 1], else [-1, 1]
    // `vfov` is the full vertical field of view
    // `aspect` is the "screen" height divided by width
    //
    template <bool depth0To1, typename T> mat4<T> perspProj(T vfov, T aspect, T near, T far);

    //
    // ...
    // `camPos` and `lookAt` must not be the same point.
    // The camera must not be looking parallel to `up`.
    //
    template <typename T> mat4<T> view(vec3<T> camPos, vec3<T> lookAt, vec3<T> up);

    //
    // ...
    //
    template <typename T> mat4<T> view(vec3<T> camPos, vec3<T> camU, vec3<T> camV, vec3<T> camW);
    template <typename T> mat4<T> view_n(vec3<T> camPos, vec3<T> camU, vec3<T> camV, vec3<T> camW);
    // basis vectors are orthonormal (optimization)
    template <typename T> mat4<T> view_o(vec3<T> camPos, vec3<T> camU, vec3<T> camV, vec3<T> camW);
    template <typename T> mat4<T> view_on(vec3<T> camPos, vec3<T> camU, vec3<T> camV, vec3<T> camW);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <typename T, int n>
    inline std::ostream & operator<<(std::ostream & os, const mat<T, n> & m)
    {
        os << "[";
        if constexpr (n >= 1) { os << m.c1; }
        if constexpr (n >= 2) { os << m.c2; }
        if constexpr (n >= 3) { os << m.c3; }
        if constexpr (n >= 4) { os << m.c4; }
        os << "]";
        return os;
    }

    template <typename T, int n>
    inline bool isIdentity(const mat<T, n> & m)
    {
        if constexpr (n == 2) {
            return
                m.c1.x == T(1.0) && m.c2.y == T(1.0) &&
                m.c2.x == T(0.0) &&
                m.c1.y == T(0.0);
        }

        if constexpr (n == 3) {
            return
                m.c1.x == T(1.0) && m.c2.y == T(1.0) && m.c3.z == T(1.0) &&
                m.c3.x == T(0.0) && m.c3.y == T(0.0) &&
                m.c1.y == T(0.0) && m.c1.z == T(0.0) &&
                m.c2.x == T(0.0) && m.c2.z == T(0.0);
        }

        if constexpr (n == 4) {
            return
                m.c1.x == T(1.0) && m.c2.y == T(1.0) && m.c3.z == T(1.0) && m.c4.w == T(1.0) &&
                m.c4.x == T(0.0) && m.c4.y == T(0.0) && m.c4.z == T(0.0) &&
                m.c1.y == T(0.0) && m.c1.z == T(0.0) && m.c1.w == T(0.0) &&
                m.c2.x == T(0.0) && m.c2.z == T(0.0) && m.c2.w == T(0.0) &&
                m.c3.x == T(0.0) && m.c3.y == T(0.0) && m.c3.w == T(0.0);
        }
    }

    template <typename T, int n>
    inline mat<T, n> transpose(const mat<T, n> & m)
    {
        if constexpr (n == 2) return {m.template row<0>(), m.template row<1>()};
        if constexpr (n == 3) return {m.template row<0>(), m.template row<1>(), m.template row<2>()};
        if constexpr (n == 4) return {m.template row<0>(), m.template row<1>(), m.template row<2>(), m.template row<3>()};
    }

    template <typename T, int n>
    inline mat<T, n> cofactor(const mat<T, n> & m)
    {
        if constexpr (n == 2) return {
            +m.c2.y, -m.c2.x,
            -m.c1.y, +m.c1.x
        };

        if constexpr (n == 3) return {
            +(m.c2.y * m.c3.z - m.c3.y * m.c2.z), -(m.c2.x * m.c3.z - m.c3.x * m.c2.z), +(m.c2.x * m.c3.y - m.c3.x * m.c2.y),
            -(m.c1.y * m.c3.z - m.c3.y * m.c1.z), +(m.c1.x * m.c3.z - m.c3.x * m.c1.z), -(m.c1.x * m.c3.y - m.c3.x * m.c1.y),
            +(m.c1.y * m.c2.z - m.c2.y * m.c1.z), -(m.c1.x * m.c2.z - m.c2.x * m.c1.z), +(m.c1.x * m.c2.y - m.c2.x * m.c1.y)
        };

        if constexpr (n == 4) {
            T yz12{m.c1.y * m.c2.z - m.c2.y * m.c1.z};
            T yz13{m.c1.y * m.c3.z - m.c3.y * m.c1.z};
            T yz14{m.c1.y * m.c4.z - m.c4.y * m.c1.z};
            T yw12{m.c1.y * m.c2.w - m.c2.y * m.c1.w};
            T yw13{m.c1.y * m.c3.w - m.c3.y * m.c1.w};
            T yw14{m.c1.y * m.c4.w - m.c4.y * m.c1.w};
            T yz23{m.c2.y * m.c3.z - m.c3.y * m.c2.z};
            T yz24{m.c2.y * m.c4.z - m.c4.y * m.c2.z};
            T yw23{m.c2.y * m.c3.w - m.c3.y * m.c2.w};
            T yw24{m.c2.y * m.c4.w - m.c4.y * m.c2.w};
            T zw12{m.c1.z * m.c2.w - m.c2.z * m.c1.w};
            T zw13{m.c1.z * m.c3.w - m.c3.z * m.c1.w};
            T zw14{m.c1.z * m.c4.w - m.c4.z * m.c1.w};
            T yz34{m.c3.y * m.c4.z - m.c4.y * m.c3.z};
            T yw34{m.c3.y * m.c4.w - m.c4.y * m.c3.w};
            T zw23{m.c2.z * m.c3.w - m.c3.z * m.c2.w};
            T zw24{m.c2.z * m.c4.w - m.c4.z * m.c2.w};
            T zw34{m.c3.z * m.c4.w - m.c4.z * m.c3.w};

            T xyz123{m.c1.x * yz23 - m.c2.x * yz13 + m.c3.x * yz12};
            T xyz124{m.c1.x * yz24 - m.c2.x * yz14 + m.c4.x * yz12};
            T xyz134{m.c1.x * yz34 - m.c3.x * yz14 + m.c4.x * yz13};
            T xyw123{m.c1.x * yw23 - m.c2.x * yw13 + m.c3.x * yw12};
            T xyw124{m.c1.x * yw24 - m.c2.x * yw14 + m.c4.x * yw12};
            T xyw134{m.c1.x * yw34 - m.c3.x * yw14 + m.c4.x * yw13};
            T xzw123{m.c1.x * zw23 - m.c2.x * zw13 + m.c3.x * zw12};
            T xzw124{m.c1.x * zw24 - m.c2.x * zw14 + m.c4.x * zw12};
            T xzw134{m.c1.x * zw34 - m.c3.x * zw14 + m.c4.x * zw13};
            T xyz234{m.c2.x * yz34 - m.c3.x * yz24 + m.c4.x * yz23};
            T xyw234{m.c2.x * yw34 - m.c3.x * yw24 + m.c4.x * yw23};
            T xzw234{m.c2.x * zw34 - m.c3.x * zw24 + m.c4.x * zw23};
            T yzw123{m.c1.y * zw23 - m.c2.y * zw13 + m.c3.y * zw12};
            T yzw124{m.c1.y * zw24 - m.c2.y * zw14 + m.c4.y * zw12};
            T yzw134{m.c1.y * zw34 - m.c3.y * zw14 + m.c4.y * zw13};
            T yzw234{m.c2.y * zw34 - m.c3.y * zw24 + m.c4.y * zw23};

            return {
                 yzw234, -xzw234,  xyw234, -xyz234,
                -yzw134,  xzw134, -xyw134,  xyz134,
                 yzw124, -xzw124,  xyw124, -xyz124,
                -yzw123,  xzw123, -xyw123,  xyz123
            };
        }
    }

    template <typename T, int n>
    inline mat<T, n> adjoint(const mat<T, n> & m)
    {
        return transpose(cofactor(m));
    }

    template <typename T, int n>
    inline T determinant(const mat<T, n> & m)
    {
        if constexpr (n == 2) return
            + m.c1.x * m.c2.y
            - m.c2.x * m.c1.y;
        if constexpr (n == 3) return
            + m.c1.x * (m.c2.y * m.c3.z - m.c3.y * m.c2.z)
            - m.c2.x * (m.c1.y * m.c3.z - m.c3.y * m.c1.z)
            + m.c3.x * (m.c1.y * m.c2.z - m.c2.y * m.c1.z);
        if constexpr (n == 4) {
            T zw12 = m.c1.z * m.c2.w - m.c2.z * m.c1.w;
            T zw13 = m.c1.z * m.c3.w - m.c3.z * m.c1.w;
            T zw14 = m.c1.z * m.c4.w - m.c4.z * m.c1.w;
            T zw23 = m.c2.z * m.c3.w - m.c3.z * m.c2.w;
            T zw24 = m.c2.z * m.c4.w - m.c4.z * m.c2.w;
            T zw34 = m.c3.z * m.c4.w - m.c4.z * m.c3.w;
            return
                + m.c1.x * (m.c2.y * zw34 - m.c3.y * zw24 + m.c4.y * zw23)
                - m.c2.x * (m.c1.y * zw34 - m.c3.y * zw14 + m.c4.y * zw13)
                + m.c3.x * (m.c1.y * zw24 - m.c2.y * zw14 + m.c4.y * zw12)
                - m.c4.x * (m.c1.y * zw23 - m.c2.y * zw13 + m.c3.y * zw12);
        }
    }

    template <typename T, int n>
    inline mat<T, n> inverse(const mat<T, n> & m)
    {
        const T det{determinant(m)};
        if (isZero(det)) {
            return nullMat<T, n>();
        }

        return adjoint(m) / determinant(m);
    }

    template <typename T, int n>
    inline mat<T, n + 1> translate(const vec<T, n> & delta)
    {
        if constexpr (n == 2) {
            return {
                 T(1.0),  T(0.0), T(0.0),
                 T(0.0),  T(1.0), T(0.0),
                delta.x, delta.y, T(1.0)
            };
        }
        if constexpr (n == 3) {
            return {
                 T(1.0),  T(0.0),  T(0.0), T(0.0),
                 T(0.0),  T(1.0),  T(0.0), T(0.0),
                 T(0.0),  T(0.0),  T(1.0), T(0.0),
                delta.x, delta.y, delta.z, T(1.0)
            };
        }
    }

    template <typename T, int mn, int vn>
    requires (mn == vn + 1)
    inline mat<T, mn> & translate(mat<T, mn> & m, const vec<T, vn> & delta)
    {
        if constexpr (vn == 2 && mn == 3) {
            m.c1.x += delta.x * m.c1.z;
            m.c2.x += delta.x * m.c2.z;
            m.c3.x += delta.x * m.c3.z;
            m.c1.y += delta.y * m.c1.z;
            m.c2.y += delta.y * m.c2.z;
            m.c3.y += delta.y * m.c3.z;

            return m;
        }
        if constexpr (vn == 3 && mn == 4) {
            m.c1.x += delta.x * m.c1.w;
            m.c2.x += delta.x * m.c2.w;
            m.c3.x += delta.x * m.c3.w;
            m.c4.x += delta.x * m.c4.w;
            m.c1.y += delta.y * m.c1.w;
            m.c2.y += delta.y * m.c2.w;
            m.c3.y += delta.y * m.c3.w;
            m.c4.y += delta.y * m.c4.w;
            m.c1.z += delta.z * m.c1.w;
            m.c2.z += delta.z * m.c2.w;
            m.c3.z += delta.z * m.c3.w;
            m.c4.z += delta.z * m.c4.w;

            return m;
        }
    }

    template <typename T, int n>
    inline mat<T, n> scale(const vec<T, n> & scale)
    {
        if constexpr (n == 2) return {
            scale.x,  T(0.0),
             T(0.0), scale.y
        };
        if constexpr (n == 3) return {
            scale.x,  T(0.0),  T(0.0),
             T(0.0), scale.y,  T(0.0),
             T(0.0),  T(0.0), scale.z
        };
        if constexpr (n == 4) return {
            scale.x,  T(0.0),  T(0.0),  T(0.0),
             T(0.0), scale.y,  T(0.0),  T(0.0),
             T(0.0),  T(0.0), scale.z,  T(0.0),
             T(0.0),  T(0.0),  T(0.0), scale.w
        };
    }

    template <typename T, int mn, int vn>
    inline mat<T, mn> & scale(mat<T, mn> & m, const vec<T, vn> & scale)
    {
        if constexpr (vn == 2 && mn == 2) {
            m.c1.x *= scale.x;
            m.c2.x *= scale.x;
            m.c1.y *= scale.y;
            m.c2.y *= scale.y;

            return m;
        }
        if constexpr (vn == 2 && mn == 3) {
            m.c1.x *= scale.x;
            m.c2.x *= scale.x;
            m.c3.x *= scale.x;
            m.c1.y *= scale.y;
            m.c2.y *= scale.y;
            m.c3.y *= scale.y;

            return m;
        }
        if constexpr (vn == 3 && mn == 3) {
            m.c1.x *= scale.x;
            m.c2.x *= scale.x;
            m.c3.x *= scale.x;
            m.c1.y *= scale.y;
            m.c2.y *= scale.y;
            m.c3.y *= scale.y;
            m.c1.z *= scale.z;
            m.c2.z *= scale.z;
            m.c3.z *= scale.z;

            return m;
        }
        if constexpr (vn == 3 && mn == 4) {
            m.c1.x *= scale.x;
            m.c2.x *= scale.x;
            m.c3.x *= scale.x;
            m.c4.x *= scale.x;
            m.c1.y *= scale.y;
            m.c2.y *= scale.y;
            m.c3.y *= scale.y;
            m.c4.y *= scale.y;
            m.c1.z *= scale.z;
            m.c2.z *= scale.z;
            m.c3.z *= scale.z;
            m.c4.z *= scale.z;

            return m;
        }
    }

    template <typename T>
    inline mat2<T> rotate(const T angle)
    {
        const T s{std::sin(angle)};
        const T c{std::cos(angle)};

        return {
             c, s,
            -s, c
        };
    }

    template <typename T>
    inline mat3<T> rotateX(const T angle)
    {
        const T s{std::sin(angle)};
        const T c{std::cos(angle)};

        return {
            T(1.0), T(0.0), T(0.0),
            T(0.0),      c,      s,
            T(0.0),     -s,      c
        };
    }

    template <typename T>
    inline mat3<T> rotateY(const T angle)
    {
        const T s{std::sin(angle)};
        const T c{std::cos(angle)};

        return {
                 c, T(0.0),     -s,
            T(0.0), T(1.0), T(0.0),
                 s, T(0.0),      c
        };
    }

    template <typename T>
    inline mat3<T> rotateZ(const T angle)
    {
        const T s{std::sin(angle)};
        const T c{std::cos(angle)};

        return {
                 c,      s, T(0.0),
                -s,      c, T(0.0),
            T(0.0), T(0.0), T(1.0)
        };
    }

    template <typename T>
    inline mat3<T> rotate(const vec3<T> axis, const T sinTheta, const T cosTheta)
    {
        if (isZero(magnitude2(axis))) { //can't rotate around 0 length vector
            return {};
        }

        return rotate_n(normalize(axis), sinTheta, cosTheta);
    }

    template <typename T>
    inline mat3<T> rotate_n(const vec3<T> axis, const T s, const T c)
    {
        const T cm{T(1.0) - c};
        const T xs{axis.x * s};
        const T ys{axis.y * s};
        const T zs{axis.z * s};
        const T xcm{axis.x * cm};
        const T ycm{axis.y * cm};
        const T zcm{axis.z * cm};
        const T xycm{xcm * axis.y};
        const T yzcm{ycm * axis.z};
        const T zxcm{zcm * axis.x};

        return {
            xcm * axis.x + c, xycm + zs, zxcm - ys,
            xycm - zs, ycm * axis.y + c, yzcm + xs,
            zxcm + ys, yzcm - xs, zcm * axis.z + c
        };
    }

    template <typename T>
    inline mat3<T> rotate(const vec3<T> axis, const T angle)
    {
        return rotate(axis, std::sin(angle), std::cos(angle));
    }

    template <typename T>
    inline mat3<T> rotate_n(const vec3<T> axis, const T angle)
    {
        return rotate_n(axis, std::sin(angle), std::cos(angle));
    }

    template <typename T>
    inline mat3<T> euler(const vec3<T> forward, const vec3<T> up, const T theta, const T phi, const T psi)
    {
        return euler_n(normalize(forward), normalize(up), theta, phi, psi);
    }

    template <typename T>
    inline mat3<T> euler_n(const vec3<T> forward, const vec3<T> up, const T theta, const T phi, const T psi)
    {
        return rotate_n(up, theta) * rotate_n(cross(forward, up), phi) * rotate_n(forward, psi);
    }

    template <typename T, int n>
    inline mat<T, n> align(const vec<T, n> & v1, const vec<T, n> & v2)
    {
        return align_n(normalize(v1), normalize(v2));
    }

    template <typename T>
    inline mat2<T> align_n(const vec2<T> v1, const vec2<T> v2)
    {
        T c{cross(v1, v2)};
        T d{dot(v1, v2)};

        return rotate(c < T(0.0) ? -std::acos(d) : std::acos(d));
    }

    template <typename T>
    inline mat3<T> align_n(const vec3<T> v1, const vec3<T> v2)
    {
        const T d{dot(v1, v2)};
        if (areEqual(d, T(1.0))) { // already aligned, and would break rotation
            return {};
        }
        if (areEqual(d, T(-1.0))) { // opposite direction, pick arbitrary axis to rotate around
            return rotate_n(ortho(v1), std::numbers::pi_v<T>);
        }

        const vec3<T> c(cross(v1, v2));
        const T m{magnitude(c)};

        return rotate_n(c * (T(1.0) / m), m, d);
    }

    template <typename T>
    inline mat3<T> align(const vec3<T> forward1, const vec3<T> up1, const vec3<T> forward2, const vec3<T> up2)
    {
        return align_n(normalize(forward1), normalize(up1), normalize(forward2), normalize(up2));
    }

    template <typename T>
    inline mat3<T> align_n(const vec3<T> forward1, const vec3<T> up1, const vec3<T> forward2, const vec3<T> up2)
    {
        const mat3<T> m(align_n(forward1, forward2));
        return align_n(m * up1, up2) * m;
    }

    template <typename T>
    inline mat2<T> map(const vec2<T> x1, const vec2<T> y1, const vec2<T> x2, const vec2<T> y2)
    {
        const mat2<T> a(x1, y1);
        const mat2<T> b(x2, y2);

        return inverse(b) * a;
    }

    template <typename T>
    inline mat2<T> map_o(const vec2<T> x1, const vec2<T> y1, const vec2<T> x2, const vec2<T> y2)
    {
        const mat2<T> a(x1, y1);
        const mat2<T> b(x2, y2);

        return transpose(b) * a;
    }

    template <typename T>
    inline mat3<T> map(const vec3<T> x1, const vec3<T> y1, const vec3<T> z1, const vec3<T> x2, const vec3<T> y2, const vec3<T> z2)
    {
        const mat3<T> a(x1, y1, z1);
        const mat3<T> b(x2, y2, z2);

        return inverse(b) * a;
    }

    template <typename T>
    inline mat3<T> map_o(const vec3<T> x1, const vec3<T> y1, const vec3<T> z1, const vec3<T> x2, const vec3<T> y2, const vec3<T> z2)
    {
        const mat3<T> a(x1, y1, z1);
        const mat3<T> b(x2, y2, z2);

        return transpose(b) * a;
    }

    template <typename T>
    inline mat2<T> mapTo(const vec2<T> x, const vec2<T> y)
    {
        return inverse(mat2<T>(x, y));
    }

    template <typename T>
    inline mat2<T> mapTo_o(const vec2<T> x, const vec2<T> y)
    {
        return transpose(mat2<T>(x, y));
    }

    template <typename T>
    inline mat3<T> mapTo(const vec3<T> x, const vec3<T> y, const vec3<T> z)
    {
        return inverse(mat3<T>(x, y, z));
    }

    template <typename T>
    inline mat3<T> mapTo_o(const vec3<T> x, const vec3<T> y, const vec3<T> z)
    {
        return transpose(mat3<T>(x, y, z));
    }

    template <typename T>
    inline mat2<T> mapFrom(const vec2<T> x, const vec2<T> y)
    {
        return {x, y};
    }

    template <typename T>
    inline mat3<T> mapFrom(const vec3<T> x, const vec3<T> y, const vec3<T> z)
    {
        return {x, y, z};
    }

    template <bool depth0To1, typename T>
    inline mat4<T> orthoProj(const T width, const T height, const T near, const T far)
    {
        const T nearMinusFar{near - far};
        return {
            T(2.0) / width, T(0.0), T(0.0), T(0.0),
            T(0.0), T(2.0) / height, T(0.0), T(0.0),
            T(0.0), T(0.0), (depth0To1 ? T(1.0) : T(2.0)) / nearMinusFar, T(0.0),
            T(0.0), T(0.0), (depth0To1 ? near : far + near) / nearMinusFar, T(1.0)
        };
    }

    template <bool depth0To1, typename T>
    inline mat4<T> perspProj(const T vfov, const T aspect, const T near, const T far)
    {
        const T invTop{T(1.0) / std::tan(vfov * T(0.5))};
        const T invNearMinusFar{T(1.0) / (near - far)};
        return {
            invTop * aspect, T(0.0), T(0.0), T(0.0),
            T(0.0), invTop, T(0.0), T(0.0),
            T(0.0), T(0.0), (depth0To1 ? far : far + near) * invNearMinusFar, T(-1.0),
            T(0.0), T(0.0), (depth0To1 ? far : T(2.0) * far) * near * invNearMinusFar, T(0.0)
        };
    }

    template <typename T>
    inline mat4<T> view(const vec3<T> camPos, const vec3<T> lookAt, const vec3<T> up)
    {
        const vec3<T> w(normalize(camPos - lookAt));
        const vec3<T> u(normalize(cross(up, w)));
        const vec3<T> v(cross(w, u));

        return view_on(camPos, u, v, w);
    }

    template <typename T>
    inline mat4<T> view(const vec3<T> camPos, const vec3<T> camU, const vec3<T> camV, const vec3<T> camW)
    {
        return view_n(camPos, normalize(camU), normalize(camV), normalize(camW));
    }

    template <typename T>
    inline mat4<T> view_n(const vec3<T> camPos, const vec3<T> camU, const vec3<T> camV, const vec3<T> camW)
    {
        return mat4<T>(mapTo(camU, camV, camW)) * translate(-camPos);
    }

    template <typename T>
    inline mat4<T> view_o(const vec3<T> camPos, const vec3<T> camU, const vec3<T> camV, const vec3<T> camW)
    {
        return view_on(camPos, normalize(camU), normalize(camV), normalize(camW));
    }

    template <typename T>
    inline mat4<T> view_on(const vec3<T> camPos, const vec3<T> camU, const vec3<T> camV, const vec3<T> camW)
    {
        const vec3<T> trans(-camPos);
        return {
                      camU.x,           camV.x,           camW.x, T(0.0),
                      camU.y,           camV.y,           camW.y, T(0.0),
                      camU.z,           camV.z,           camW.z, T(0.0),
            dot(camU, trans), dot(camV, trans), dot(camW, trans), T(1.0)
        };
    }
}
