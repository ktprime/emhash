#ifndef BOOST_UNORDERED_DETAIL_MULX_HPP
#define BOOST_UNORDERED_DETAIL_MULX_HPP

// Copyright 2022 Peter Dimov.
// Copyright 2022 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt)

#include <climits>
#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER) && !defined(__clang__)
# include <intrin.h>
#endif

namespace boost {
namespace unordered {
namespace detail {

// Bit mixer based on the mulx primitive

#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__)

__forceinline std::uint64_t mulx64( std::uint64_t x, std::uint64_t y )
{
    std::uint64_t r2;
    std::uint64_t r = _umul128( x, y, &r2 );
    return r ^ r2;
}

#elif defined(_MSC_VER) && defined(_M_ARM64) && !defined(__clang__)

__forceinline std::uint64_t mulx64( std::uint64_t x, std::uint64_t y )
{
    std::uint64_t r = x * y;
    std::uint64_t r2 = __umulh( x, y );
    return r ^ r2;
}

#elif defined(__SIZEOF_INT128__)

inline std::uint64_t mulx64( std::uint64_t x, std::uint64_t y )
{
    __uint128_t r = (__uint128_t)x * y;
    return (std::uint64_t)r ^ (std::uint64_t)( r >> 64 );
}

#else

inline std::uint64_t mulx64( std::uint64_t x, std::uint64_t y )
{
    std::uint64_t x1 = (std::uint32_t)x;
    std::uint64_t x2 = x >> 32;

    std::uint64_t y1 = (std::uint32_t)y;
    std::uint64_t y2 = y >> 32;

    std::uint64_t r3 = x2 * y2;

    std::uint64_t r2a = x1 * y2;

    r3 += r2a >> 32;

    std::uint64_t r2b = x2 * y1;

    r3 += r2b >> 32;

    std::uint64_t r1 = x1 * y1;

    std::uint64_t r2 = (r1 >> 32) + (std::uint32_t)r2a + (std::uint32_t)r2b;

    r1 = (r2 << 32) + (std::uint32_t)r1;
    r3 += r2 >> 32;

    return r1 ^ r3;
}

#endif

inline std::uint32_t mulx32( std::uint32_t x, std::uint32_t y )
{
    std::uint64_t r = (std::uint64_t)x * y;

#if defined(__MSVC_RUNTIME_CHECKS)

    return (std::uint32_t)(r & UINT32_MAX) ^ (std::uint32_t)(r >> 32);

#else

    return (std::uint32_t)r ^ (std::uint32_t)(r >> 32);

#endif
}

#if defined(SIZE_MAX)
#if ((((SIZE_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define BOOST_UNORDERED_64B_ARCHITECTURE /* >64 bits assumed as 64 bits */
#endif
#elif defined(UINTPTR_MAX) /* used as proxy for std::size_t */
#if ((((UINTPTR_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define BOOST_UNORDERED_64B_ARCHITECTURE
#endif
#endif

inline std::size_t mulx( std::size_t x ) noexcept
{
#if defined(BOOST_UNORDERED_64B_ARCHITECTURE)

    // multiplier is phi
    return (std::size_t)mulx64( (std::uint64_t)x, 0x9E3779B97F4A7C15ull );

#else /* 32 bits assumed */

    // multiplier from https://arxiv.org/abs/2001.05304
    return mulx32( x, 0xE817FB2Du );

#endif
}

#ifdef BOOST_UNORDERED_64B_ARCHITECTURE
#undef BOOST_UNORDERED_64B_ARCHITECTURE
#endif

} // namespace detail
} // namespace unordered
} // namespace boost

#endif // #ifndef BOOST_UNORDERED_DETAIL_MULX_HPP
