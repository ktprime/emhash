#pragma once

///
/// Colorspaces
///
/// sRGB (standard RGB):
///   - Non-linear
///   - Gamma 2.2
///   - Components:
///     - R (Red)   : [0, 1]
///     - G (Green) : [0, 1]
///     - B (Blue)  : [0, 1]
///
/// lRGB (linear RGB):
///   - Components:
///     -  R (red)   : [0, inf)
///     -  G (green) : [0, inf)
///     -  B (blue)  : [0, inf)
///
/// HSL:
///   - Components:
///     - H (Hue)        : [0, 1]
///     - S (Saturation) : [0, 1]
///     - L (Lightness)  : [0, 1]
///
/// CIE XYZ:
///   - "foundational" linear color space
///   - Components:
///     - X : (-inf, inf)
///     - Y : (-inf, inf)
///     - Z : (-inf, inf)
///
/// CIE xyY:
///   - CIE XYZ normalized to uniform brightness
///   - Components:
///     - x : (-inf, inf)
///     - y : (-inf, inf)
///     - Y : (-inf, inf)
///
/// CIE LAB:
///   - Good perceptual uniformity in print and physical media
///   - Components:
///     - L : [0, 1]
///     - a : (-inf, inf)
///     - b : (-inf, inf)
///
/// CIE LUV:
///   - Good perceptual uniformity for light and digital displays
///   - Components:
///     - L : [0, 1]
///     - u : (-inf, inf)
///     - v : (-inf, inf)
///
/// CIE LCH (UV):
///   - Hue-chroma form of CIE LUV
///   - Components:
///     - L: [0, 1]
///     - C: [0, inf)
///     - h: [-pi, pi]
///

#include <qc-core/vector-ext.hpp>
#include <qc-core/matrix.hpp>

namespace qc::color
{
    template <Numeric T> inline const vec3<T> black    {transnorm<vec3<T>>(dvec3{0.00, 0.00, 0.00})};
    template <Numeric T> inline const vec3<T> darkGray {transnorm<vec3<T>>(dvec3{0.25, 0.25, 0.25})};
    template <Numeric T> inline const vec3<T> gray     {transnorm<vec3<T>>(dvec3{0.50, 0.50, 0.50})};
    template <Numeric T> inline const vec3<T> lightGray{transnorm<vec3<T>>(dvec3{0.75, 0.75, 0.75})};
    template <Numeric T> inline const vec3<T> white    {transnorm<vec3<T>>(dvec3{1.00, 1.00, 1.00})};

    template <Numeric T> inline const vec3<T> red      {transnorm<vec3<T>>(dvec3{1.00, 0.00, 0.00})};
    template <Numeric T> inline const vec3<T> yellow   {transnorm<vec3<T>>(dvec3{1.00, 1.00, 0.00})};
    template <Numeric T> inline const vec3<T> green    {transnorm<vec3<T>>(dvec3{0.00, 1.00, 0.00})};
    template <Numeric T> inline const vec3<T> cyan     {transnorm<vec3<T>>(dvec3{0.00, 1.00, 1.00})};
    template <Numeric T> inline const vec3<T> blue     {transnorm<vec3<T>>(dvec3{0.00, 0.00, 1.00})};
    template <Numeric T> inline const vec3<T> magenta  {transnorm<vec3<T>>(dvec3{1.00, 0.00, 1.00})};

    ///
    /// Converts from SRGB to HSL
    ///
    /// @param srgb the SRGB value to convert
    /// @return the converted HSL value
    template <Floating T>
    inline vec3<T> srgbToHsl(const vec3<T> & srgb) noexcept
    {
        vec3<T> hsl{};

        int maxI{srgb.y > srgb.x};
        int minI{1 - maxI};
        if (srgb.z > srgb[maxI]) maxI = 2;
        else if (srgb.z < srgb[minI]) minI = 2;
        const T maxComp{srgb[maxI]};
        const T minComp{srgb[minI]};

        // Lightness
        hsl.z = (minComp + maxComp) * T(0.5);

        if (hsl.z > T(0.0) && hsl.z < T(1.0)) {
            const T compRange{maxComp - minComp};

            // Saturation
            if (hsl.z > T(0.5)) {
                hsl.y = compRange / (T(2.0) - (hsl.z * T(2.0)));
            }
            else {
                hsl.y = compRange / (hsl.z * T(2.0));
            }

            if (hsl.y > T(0.0)) {
                // Hue
                // TODO: Test if this is acually faster than the two `% 3`s
                const T overflowRgb[5]{srgb.x, srgb.y, srgb.z, srgb.x, srgb.y};
                hsl.x = T(maxI * 2 + 6);
                hsl.x += (overflowRgb[maxI + 1] - overflowRgb[maxI + 2]) / compRange;
                hsl.x = fract(hsl.x * T(1.0 / 6.0));
            }
        }

        return hsl;
    }

    namespace _minutia
    {
        template <Floating T>
        inline vec3<T> hueToSrgb(const T hue, const T minComp, const T maxComp) noexcept
        {
            const auto[fraction, whole]{fract_i(hue * T(6.0))};
            const T midOffset{(maxComp - minComp) * fraction};
            switch (whole) {
                default: return {maxComp, minComp + midOffset, minComp};
                case 1: return {maxComp - midOffset, maxComp, minComp};
                case 2: return {minComp, maxComp, minComp + midOffset};
                case 3: return {minComp, maxComp - midOffset, maxComp};
                case 4: return {minComp + midOffset, minComp, maxComp};
                case 5: return {maxComp, minComp, maxComp - midOffset};
            }
        }
    }

    ///
    /// Get the sRGB color for a given hue
    ///
    /// @param hue a hue in range [0, 1]
    /// @return the full value/saturated sRGB corresponding to `hue`
    ///
    template <Floating T>
    inline vec3<T> hueToSrgb(const T hue) noexcept
    {
        return _minutia::hueToSrgb(hue, T(0.0), T(1.0));
    }

    ///
    /// Converts a color from HSL to sRGB
    ///
    /// There is a desmos graph for this
    ///
    /// @param hsl the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> hslToSrgb(const vec3<T> & hsl) noexcept
    {
        const T maxSpread{T(0.5) - qc::abs(hsl.z - T(0.5))};
        const T spread{maxSpread * hsl.y};
        const T minComp{hsl.z - spread};
        const T maxComp{hsl.z + spread};
        return _minutia::hueToSrgb(hsl.x, minComp, maxComp);
    }

    ///
    /// Return a sRGB color correpsonding to a "thermal" value, in range [0, 1]
    ///
    /// The spectrum goes roughly black -> blue -> purple -> magenta -> orange -> yellow -> white
    ///
    /// Curve fitted to match ironbow palette. See Desmos graph
    ///
    /// @param thermal the input "temperature" in range [0, 1]
    /// @return the gradient sRGB value
    ///
    template <Floating T>
    inline vec3<T> thermalToSrgb(const T thermal) noexcept
    {
        T r1{T(1.09) * (thermal - T(1.0))};
        r1 *= r1;
        constexpr T b0{T(0.885)};
        constexpr T b1{b0 * b0 * b0 * b0};
        T b2{T(3.7) * thermal - b0};
        b2 *= b2;

        return {
            T(1.0) - r1 * r1,
            T(1.6) * thermal - T(0.5),
            thermal < T(0.5) ? b1 - b2 * b2 : T(6.5) * thermal - T(5.5)
        };
    }

    ///
    /// Converts a color from sRGB to lRGB
    ///
    /// @param srgb the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> srgbToLrgb(const vec3<T> & srgb) noexcept
    {
        return pow(srgb, T(2.2));
    }

    ///
    /// Converts a color from lRGB to sRGB
    ///
    /// @param lrgb the color to convert, must be positive
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> lrgbToSrgb(const vec3<T> & lrgb) noexcept
    {
        return pow(lrgb, T(1.0 / 2.2));
    }

    ///
    /// D65, see: http://terathon.com/blog/rgb-xyz-conversion-matrix-accuracy/
    ///
    template <Floating T> inline constexpr mat3<T> lrgbToXyzMatrix{
        T(506752.0 / 1228815.0), T(87098.0 / 409605.0), T(7918.0 / 409605.0),
        T(87881.0 / 245763.0), T(175762.0 / 245763.0), T(87881.0 / 737289.0),
        T(12673.0 / 70218.0), T(12673.0 / 175545.0), T(1001167.0 / 1053270.0)
    };
    template <Floating T> inline constexpr mat3<T> xyzToLrgbMatrix{
        T(12831.0 / 3959.0), T(-851781.0 / 878810.0), T(705.0 / 12673.0),
        T(-329.0 / 214.0), T(1648619.0 / 878810.0), T(-2585.0 / 12673.0),
        T(-1974.0 / 3959.0), T(36519.0 / 878810.0), T(705.0 / 667.0)
    };

    ///
    /// The XYZ value of D65 white
    ///
    template <Floating T> inline constexpr vec3<T> xyzWhitePoint{lrgbToXyzMatrix<T> * vec3<T>{T(1.0)}};

    ///
    /// Converts a color from lRGB to XYZ
    ///
    /// @param lrgb the color to convert
    /// @return the converted color
    template <Floating T>
    inline vec3<T> lrgbToXyz(const vec3<T> & lrgb) noexcept
    {
        return lrgbToXyzMatrix<T> * lrgb;
    }

    ///
    /// Converts a color from XYZ to lRGB
    ///
    /// @param xyz the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> xyzToLrgb(const vec3<T> & xyz) noexcept
    {
        return xyzToLrgbMatrix<T> * xyz;
    }

    ///
    /// Converts a color from XYZ to xyY
    ///
    /// @param xyz the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> xyzToXyy(const vec3<T> & xyz) noexcept
    {
        const T temp{sum(xyz)};
        if (temp) {
            return {xyz.xy() / temp, xyz.y};
        }
        else {
            return {T(1.0 / 3.0), T(1.0 / 3.0), 0.0f};
        }
    }

    ///
    /// Converts a color from xyY to XYZ
    ///
    /// @param xyy the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> xyyToXyz(const vec3<T> & xyy) noexcept
    {
        const T temp{xyy.z / xyy.y};
        return {temp * xyy.x, xyy.z, temp * (T(1.0) - xyy.x - xyy.y)};
    }


    ///
    /// Converts a color from XYZ to LAB
    ///
    /// @param xyz the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> xyzToLab(const vec3<T> & xyz) noexcept
    {
        static const vec3<T> invWhitePoint{T(1.0) / xyzWhitePoint<T>};

        // Not doing piecewise approximation
        const vec3<T> normalized{xyz * invWhitePoint};
        const T cbrtNY{std::cbrt(normalized.y)};
        return {
            cbrtNY,
            T(5.0) * (std::cbrt(normalized.x) - cbrtNY),
            T(2.0) * (cbrtNY - std::cbrt(normalized.z))
        };
    }

    ///
    /// Converts a color from LAB to XYZ
    ///
    /// @param lab the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> labToXyz(const vec3<T> & lab) noexcept
    {
        // Not doing piecewise approximation
        const vec3<T> xyz{
            lab.x + lab.y * T(0.2),
            lab.x,
            lab.x - lab.z * T(0.5)
        };
        return xyz * xyz * xyz * xyzWhitePoint<T>;
    }

    ///
    /// Converts a color from XYZ to LUV
    ///
    /// @param xyz the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> xyzToLuv(const vec3<T> & xyz) noexcept
    {
        static constexpr T u_v_nDivisor{xyzWhitePoint<T>.x + T(15.0) * xyzWhitePoint<T>.y + T(3.0) * xyzWhitePoint<T>.z};
        static constexpr T u_n{T(4.0) * xyzWhitePoint<T>.x / u_v_nDivisor};
        static constexpr T v_n{T(9.0) * xyzWhitePoint<T>.y / u_v_nDivisor};
        static constexpr T invYn{T(1.0) / xyzWhitePoint<T>.y};

        // Not doing piecewise approximation
        if (xyz) {
            const T l{std::cbrt(xyz.y * invYn)};
            const T _13l{T(13.0) * l};
            const T uvFactor{T(1.0) / (xyz.x + T(15.0) * xyz.y + T(3.0) * xyz.z)};
            const T u_{T(4.0) * xyz.x * uvFactor};
            const T v_{T(9.0) * xyz.y * uvFactor};
            const T u{_13l * (u_ - u_n)};
            const T v{_13l * (v_ - v_n)};
            return {l, u, v};
        }
        else {
            return {};
        }
    }

    ///
    /// Converts a color from LUV to XYZ
    ///
    /// @param luv the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> luvToXyz(const vec3<T> & luv) noexcept
    {
        static constexpr T u_v_nDivisor{xyzWhitePoint<T>.x + T(15.0) * xyzWhitePoint<T>.y + T(3.0) * xyzWhitePoint<T>.z};
        static constexpr T u_n{T(4.0) * xyzWhitePoint<T>.x / u_v_nDivisor};
        static constexpr T v_n{T(9.0) * xyzWhitePoint<T>.y / u_v_nDivisor};

        // Not doing piecewise approximation
        if (luv.x) {
            const T inv13L{T(1.0) / (T(13.0) * luv.x)};
            const T u_{luv.y * inv13L + u_n};
            const T v_{luv.z * inv13L + v_n};
            const T y{xyzWhitePoint<T>.y * luv.x * luv.x * luv.x};
            const T inv4v_{T(1.0) / (T(4.0) * v_)};
            const T x{y * T(9.0) * u_ * inv4v_};
            const T z{y * (T(12.0) - T(3.0) * u_ - T(20.0) * v_) * inv4v_};
            return {x, y, z};
        }
        else {
            return {};
        }
    }

    ///
    /// Converts a color from LUV to LCH
    ///
    /// @param luv the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> luvToLch(const vec3<T> & luv) noexcept
    {
        return {
            luv.x,
            magnitude(luv.yz()),
            std::atan2(luv.z, luv.y)
        };
    }

    ///
    /// Converts a color from LCH to LUV
    ///
    /// @param lch the color to convert
    /// @return the converted color
    ///
    template <Floating T>
    inline vec3<T> lchToLuv(const vec3<T> & lch) noexcept
    {
        return {
            lch.x,
            std::cos(lch.z) * lch.y,
            std::sin(lch.z) * lch.y
        };
    }

    // GPU (saved for later)
    /*vec3 hsv2rgb(vec3 c)
    {
        vec4 K = vec4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0f - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }
    vec3 hue2rgb(float h)
    {
        vec3 p = abs(fract(h + vec3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f)) * 6.0f - 3.0f);
        return clamp(p - 1.0f, 0.0, 1.0);
    }*/

} // namespace qc::color
