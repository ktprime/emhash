/**
 * @file a5hash.h
 *
 * @version 5.16
 *
 * @brief The inclusion file for the "a5hash" 64-bit hash function,
 * "a5hash32" 32-bit hash function, "a5hash128" 128-bit hash function, and
 * "a5rand" 64-bit PRNG.
 *
 * The source code is written in ISO C99, with full C++ compliance enabled
 * conditionally and automatically, if compiled with a C++ compiler.
 *
 * Description is available at https://github.com/avaneev/a5hash
 *
 * E-mail: aleksey.vaneev@gmail.com or info@voxengo.com
 *
 * LICENSE:
 *
 * Copyright (c) 2025 Aleksey Vaneev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef A5HASH_INCLUDED
#define A5HASH_INCLUDED

#define A5HASH_VER_STR "5.16" ///< A5HASH source code version string.

/**
 * @def A5HASH_NS_CUSTOM
 * @brief If this macro is defined externally, all symbols will be placed into
 * the C++ namespace specified by the macro, and won't be exported to the
 * global namespace. WARNING: if the defined value of the macro is empty, the
 * symbols will be placed into the global namespace anyway.
 */

/**
 * @def A5HASH_U64_C( x )
 * @brief Macro that defines a numeric value as unsigned 64-bit value.
 *
 * @param x Value.
 */

/**
 * @def A5HASH_NOEX
 * @brief Macro that defines the "noexcept" function specifier for C++
 * environment.
 */

/**
 * @def A5HASH_NULL
 * @brief Macro that defines "nullptr" value, for C++ guidelines conformance.
 */

/**
 * @def A5HASH_NS
 * @brief Macro that defines an actual implementation namespace in C++
 * environment, with export of relevant symbols to the global namespace
 * (if @ref A5HASH_NS_CUSTOM is undefined).
 */

#if defined( __cplusplus )

	#include <cstring>

	#if __cplusplus >= 201103L

		#include <cstdint>

		#define A5HASH_U64_C( x ) UINT64_C( x )
		#define A5HASH_NOEX noexcept
		#define A5HASH_NULL nullptr

	#else // __cplusplus >= 201103L

		#include <stdint.h>

		#define A5HASH_U64_C( x ) (uint64_t) x
		#define A5HASH_NOEX throw()
		#define A5HASH_NULL NULL

	#endif // __cplusplus >= 201103L

	#if defined( A5HASH_NS_CUSTOM )
		#define A5HASH_NS A5HASH_NS_CUSTOM
	#else // defined( A5HASH_NS_CUSTOM )
		#define A5HASH_NS a5hash_impl
	#endif // defined( A5HASH_NS_CUSTOM )

#else // defined( __cplusplus )

	#include <string.h>
	#include <stdint.h>

	#define A5HASH_U64_C( x ) (uint64_t) x
	#define A5HASH_NOEX
	#define A5HASH_NULL 0

#endif // defined( __cplusplus )

#if defined( _MSC_VER )
	#include <intrin.h>
#endif // defined( _MSC_VER )

#define A5HASH_VAL10 A5HASH_U64_C( 0xAAAAAAAAAAAAAAAA ) ///< `10` bit-pairs.
#define A5HASH_VAL01 A5HASH_U64_C( 0x5555555555555555 ) ///< `01` bit-pairs.

/**
 * @def A5HASH_ICC_GCC
 * @brief Macro that denotes the use of the ICC classic compiler with
 * GCC-style built-in functions.
 */

#if defined( __INTEL_COMPILER ) && __INTEL_COMPILER >= 1300 && \
	!defined( _MSC_VER )

	#define A5HASH_ICC_GCC

#endif // ICC check

/**
 * @def A5HASH_GCC_BUILTINS
 * @brief Macro that denotes availability of GCC-style built-in functions.
 */

#if defined( __GNUC__ ) || defined( __clang__ ) || \
	defined( __IBMC__ ) || defined( __IBMCPP__ ) || defined( A5HASH_ICC_GCC )

	#define A5HASH_GCC_BUILTINS

#endif // GCC built-ins check

/**
 * @def A5HASH_BMI2
 * @brief Macro that denotes availability of `mulx` intrinsic (MSVC-compatible
 * compilers only).
 */

#if defined( _MSC_VER )
	#if defined( __BMI2__ ) || ( !defined( A5HASH_GCC_BUILTINS ) && \
		defined( _M_AMD64 ) && defined( __AVX2__ ) && \
		( defined( __INTEL_COMPILER ) || _MSC_VER >= 1900 ))

		#include <immintrin.h>
		#define A5HASH_BMI2

	#else // BMI2

		#include <intrin.h>

	#endif // BMI2
#endif // defined( _MSC_VER )

/**
 * @def A5HASH_INLINE
 * @brief Macro that defines a function as inlinable at compiler's discretion.
 */

#if ( defined( __cplusplus ) && __cplusplus >= 201703L ) || \
	( defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 202311L )

	#define A5HASH_INLINE [[maybe_unused]] static inline

#else // defined( __cplusplus )

	#define A5HASH_INLINE static inline

#endif // defined( __cplusplus )

/**
 * @def A5HASH_INLINE_F
 * @brief Macro to force code inlining.
 */

#if defined( __LP64__ ) || defined( _LP64 ) || \
	!( SIZE_MAX <= 0xFFFFFFFFU ) || ( defined( UINTPTR_MAX ) && \
	!( UINTPTR_MAX <= 0xFFFFFFFFU )) || defined( __x86_64__ ) || \
	defined( __aarch64__ ) || defined( _M_AMD64 ) || defined( _M_ARM64 )

	#if defined( A5HASH_GCC_BUILTINS )

		#define A5HASH_INLINE_F A5HASH_INLINE __attribute__((always_inline))

	#elif defined( _MSC_VER )

		#define A5HASH_INLINE_F A5HASH_INLINE __forceinline

	#endif // defined( _MSC_VER )

#endif // 64-bit platform check

#if !defined( A5HASH_INLINE_F )
	#define A5HASH_INLINE_F A5HASH_INLINE
#endif // !defined( A5HASH_INLINE_F )

#if defined( A5HASH_NS )

namespace A5HASH_NS {

using std :: memcpy;
using std :: size_t;

#if __cplusplus >= 201103L

	using std :: uint32_t;
	using std :: uint64_t;
	using uint8_t = unsigned char; ///< For C++ type aliasing compliance.

#endif // __cplusplus >= 201103L

#endif // defined( A5HASH_NS )

/**
 * @{
 * @brief Load unsigned value of corresponding bit-size from memory.
 *
 * @param p Load address.
 */

A5HASH_INLINE_F uint32_t a5hash_lu32( const uint8_t* const p ) A5HASH_NOEX
{
	uint32_t v;
	memcpy( &v, p, 4 );

	return( v );
}

A5HASH_INLINE_F uint64_t a5hash_lu64( const uint8_t* const p ) A5HASH_NOEX
{
	uint64_t v;
	memcpy( &v, p, 8 );

	return( v );
}

/** @} */

/**
 * @brief 64-bit by 64-bit unsigned multiplication with 128-bit result.
 *
 * @param u Multiplier 1.
 * @param v Multiplier 2.
 * @param[out] rl The lower half of the 128-bit result.
 * @param[out] rh The higher half of the 128-bit result.
 */

A5HASH_INLINE_F void a5hash_umul128( const uint64_t u, const uint64_t v,
	uint64_t* const rl, uint64_t* const rh ) A5HASH_NOEX
{
#if defined( A5HASH_BMI2 )

	*rl = _mulx_u64( u, v, rh );

#elif defined( _MSC_VER ) && ( defined( _M_ARM64 ) || defined( _M_ARM64EC ) || \
	( defined( __INTEL_COMPILER ) && defined( _M_AMD64 )))

	*rl = u * v;
	*rh = __umulh( u, v );

#elif defined( _MSC_VER ) && ( defined( _M_AMD64 ) || defined( _M_IA64 ))

	*rl = _umul128( u, v, rh );

#elif defined( __SIZEOF_INT128__ ) || \
	( defined( A5HASH_ICC_GCC ) && defined( __x86_64__ ))

	__uint128_t r = u;
	r *= v;

	*rl = (uint64_t) r;
	*rh = (uint64_t) ( r >> 64 );

#elif ( defined( __IBMC__ ) || defined( __IBMCPP__ )) && defined( __LP64__ )

	*rl = u * v;
	*rh = __mulhdu( u, v );

#else // defined( __IBMC__ )

	// _umul128() code for 32-bit systems, adapted from Hacker's Delight,
	// Henry S. Warren, Jr.

	*rl = u * v;

	const uint32_t u0 = (uint32_t) u;
	const uint32_t v0 = (uint32_t) v;
	const uint64_t w0 = (uint64_t) u0 * v0;
	const uint32_t u1 = (uint32_t) ( u >> 32 );
	const uint32_t v1 = (uint32_t) ( v >> 32 );
	const uint64_t t = (uint64_t) u1 * v0 + (uint32_t) ( w0 >> 32 );
	const uint64_t w1 = (uint64_t) u0 * v1 + (uint32_t) t;

	*rh = (uint64_t) u1 * v1 + (uint32_t) ( w1 >> 32 ) +
		(uint32_t) ( t >> 32 );

#endif // defined( __IBMC__ )
}

/**
 * @brief A5HASH 64-bit hash function.
 *
 * Produces and returns a 64-bit hash value (digest) of the specified message,
 * string, or binary data block. Designed for string/small key data hash-map
 * and hash-table uses.
 *
 * @param Msg0 The message to produce a hash from. The alignment of this
 * pointer is unimportant. It is valid to pass 0 when `MsgLen` equals 0.
 * @param MsgLen Message's length, in bytes, can be zero.
 * @param UseSeed Optional value, to use instead of the default seed (0). This
 * value can have any number of significant bits and any statistical quality.
 * @return 64-bit hash of the input data.
 */

A5HASH_INLINE_F uint64_t a5hash( const void* const Msg0, size_t MsgLen,
	const uint64_t UseSeed ) A5HASH_NOEX
{
	const uint8_t* Msg = (const uint8_t*) Msg0;

	uint64_t val01 = A5HASH_VAL01;
	uint64_t val10 = A5HASH_VAL10;

	// The seeds are initialized to mantissa bits of PI.

	uint64_t Seed1 = A5HASH_U64_C( 0x243F6A8885A308D3 ) ^ MsgLen;
	uint64_t Seed2 = A5HASH_U64_C( 0x452821E638D01377 ) ^ MsgLen;
	uint64_t a, b;

	a5hash_umul128( Seed2 ^ ( UseSeed & val10 ),
		Seed1 ^ ( UseSeed & val01 ), &Seed1, &Seed2 );

	val10 ^= Seed2;

	if( MsgLen > 3 )
	{
		if( MsgLen > 16 )
		{
			val01 ^= Seed1;

			do
			{
				a5hash_umul128( a5hash_lu64( Msg ) ^ Seed1,
					a5hash_lu64( Msg + 8 ) ^ Seed2, &Seed1, &Seed2 );

				MsgLen -= 16;
				Msg += 16;

				Seed1 += val01;
				Seed2 += val10;

			} while( MsgLen > 16 );

			a = a5hash_lu64( Msg + MsgLen - 16 );
			b = a5hash_lu64( Msg + MsgLen - 8 );
		}
		else
		{
			const uint8_t* const Msg4 = Msg + MsgLen - 4;
			const size_t mo = MsgLen >> 3;

			a = (uint64_t) a5hash_lu32( Msg ) << 32 | a5hash_lu32( Msg4 );

			b = (uint64_t) a5hash_lu32( Msg + mo * 4 ) << 32 |
				a5hash_lu32( Msg4 - mo * 4 );
		}

	_fin:
		a5hash_umul128( a ^ Seed1, b ^ Seed2, &Seed1, &Seed2 );

		a5hash_umul128( val01 ^ Seed1, Seed2, &a, &b );

		return( a ^ b );
	}

	a = 0;
	b = 0;

	if( MsgLen != 0 )
	{
		a = Msg[ 0 ];

		if( MsgLen != 1 )
		{
			a |= (uint64_t) Msg[ 1 ] << 8;

			if( MsgLen != 2 )
			{
				a |= (uint64_t) Msg[ 2 ] << 16;
			}
		}
	}

	goto _fin;
}

/**
 * @brief 32-bit by 32-bit unsigned multiplication with 64-bit result.
 *
 * @param u Multiplier 1.
 * @param v Multiplier 2.
 * @param[out] rl The lower half of the 64-bit result.
 * @param[out] rh The higher half of the 64-bit result.
 */

A5HASH_INLINE_F void a5hash_umul64( const uint32_t u, const uint32_t v,
	uint32_t* const rl, uint32_t* const rh ) A5HASH_NOEX
{
	const uint64_t r = (uint64_t) u * v;

	*rl = (uint32_t) r;
	*rh = (uint32_t) ( r >> 32 );
}

/**
 * @brief A5HASH 32-bit hash function.
 *
 * Produces and returns a 32-bit hash value (digest) of the specified message,
 * string, or binary data block. Designed for string/small key data hash-map
 * and hash-table uses.
 *
 * This function works on 32-bit platforms natively, and is not subject to
 * 64-bit arithmetic penalty of the a5hash() function.
 *
 * @param Msg0 The message to produce a hash from. The alignment of this
 * pointer is unimportant. It is valid to pass 0 when `MsgLen` equals 0.
 * @param MsgLen Message's length, in bytes, can be zero.
 * @param UseSeed Optional 32-bit value, to use instead of the default seed
 * (0). This value can have any number of significant bits and any statistical
 * quality.
 * @return 32-bit hash of the input data.
 */

A5HASH_INLINE_F uint32_t a5hash32( const void* const Msg0, size_t MsgLen,
	const uint32_t UseSeed ) A5HASH_NOEX
{
	const uint8_t* Msg = (const uint8_t*) Msg0;

	uint32_t val01 = (uint32_t) A5HASH_VAL01;
	uint32_t val10 = (uint32_t) A5HASH_VAL10;

	// The seeds are initialized to mantissa bits of PI.

	uint32_t Seed1 = 0x243F6A88 ^ (uint32_t) MsgLen;
	uint32_t Seed2 = 0x85A308D3 ^ (uint32_t) MsgLen;
	uint32_t Seed3, Seed4;
	uint32_t a, b, c, d;

	#if SIZE_MAX <= 0xFFFFFFFFU

		Seed3 = 0xFB0BD3EA;
		Seed4 = 0x0F58FD47;

	#else // SIZE_MAX <= 0xFFFFFFFFU

		a5hash_umul64( (uint32_t) ( MsgLen >> 32 ) ^ 0x452821E6,
			(uint32_t) ( MsgLen >> 32 ) ^ 0x38D01377, &Seed3, &Seed4 );

	#endif // SIZE_MAX <= 0xFFFFFFFFU

	a5hash_umul64( Seed2 ^ ( UseSeed & val10 ),
		Seed1 ^ ( UseSeed & val01 ), &Seed1, &Seed2 );

	if( MsgLen < 17 )
	{
		if( MsgLen > 3 )
		{
			const uint8_t* const Msg4 = Msg + MsgLen - 4;
			size_t mo;

			a = a5hash_lu32( Msg );
			b = a5hash_lu32( Msg4 );

			if( MsgLen < 9 )
			{
				goto _fin;
			}

			mo = MsgLen >> 3;

			c = a5hash_lu32( Msg + mo * 4 );
			d = a5hash_lu32( Msg4 - mo * 4 );
		}
		else
		{
			a = 0;
			b = 0;

			if( MsgLen != 0 )
			{
				a = Msg[ 0 ];

				if( MsgLen != 1 )
				{
					a |= (uint32_t) Msg[ 1 ] << 8;

					if( MsgLen != 2 )
					{
						a |= (uint32_t) Msg[ 2 ] << 16;
					}
				}
			}

			goto _fin;
		}
	}
	else
	{
		val01 ^= Seed1;
		val10 ^= Seed2;

		do
		{
			const uint32_t s1 = Seed1;
			const uint32_t s4 = Seed4;

			a5hash_umul64( a5hash_lu32( Msg ) + Seed1,
				a5hash_lu32( Msg + 4 ) + Seed2, &Seed1, &Seed2 );

			a5hash_umul64( a5hash_lu32( Msg + 8 ) + Seed3,
				a5hash_lu32( Msg + 12 ) + Seed4, &Seed3, &Seed4 );

			MsgLen -= 16;
			Msg += 16;

			Seed1 += val01;
			Seed2 += s4;
			Seed3 += s1;
			Seed4 += val10;

		} while( MsgLen > 16 );

		a = a5hash_lu32( Msg + MsgLen - 8 );
		b = a5hash_lu32( Msg + MsgLen - 4 );

		if( MsgLen < 9 )
		{
			goto _fin;
		}

		c = a5hash_lu32( Msg + MsgLen - 16 );
		d = a5hash_lu32( Msg + MsgLen - 12 );
	}

	a5hash_umul64( c + Seed3, d + Seed4, &Seed3, &Seed4 );

_fin:
	Seed1 ^= Seed3;
	Seed2 ^= Seed4;

	a5hash_umul64( a + Seed1, b + Seed2, &Seed1, &Seed2 );

	a5hash_umul64( val01 ^ Seed1, Seed2, &a, &b );

	return( a ^ b );
}

/**
 * @brief A5HASH 128-bit hash function.
 *
 * Produces and returns a 128-bit hash value (digest) of the specified
 * message, string, or binary data block. Designed for string/small key data
 * hash-map and hash-table uses.
 *
 * @param Msg0 The message to produce a hash from. The alignment of this
 * pointer is unimportant. It is valid to pass 0 when `MsgLen` equals 0.
 * @param MsgLen Message's length, in bytes, can be zero.
 * @param UseSeed Optional value, to use instead of the default seed (0). This
 * value can have any number of significant bits and any statistical quality.
 * @param[out] rh Pointer to 64-bit variable that receives higher 64 bits of
 * 128-bit hash. The alignment of this pointer is unimportant. Can be 0.
 * @return Lower 64 bits of 128-bit hash of the input data.
 */

A5HASH_INLINE uint64_t a5hash128( const void* const Msg0, size_t MsgLen,
	const uint64_t UseSeed, void* const rh ) A5HASH_NOEX
{
	const uint8_t* Msg = (const uint8_t*) Msg0;

	uint64_t val01 = A5HASH_VAL01;
	uint64_t val10 = A5HASH_VAL10;

	// The seeds are initialized to mantissa bits of PI.

	uint64_t Seed1 = A5HASH_U64_C( 0x243F6A8885A308D3 ) ^ MsgLen;
	uint64_t Seed2 = A5HASH_U64_C( 0x452821E638D01377 ) ^ MsgLen;
	uint64_t Seed3 = A5HASH_U64_C( 0xA4093822299F31D0 );
	uint64_t Seed4 = A5HASH_U64_C( 0xC0AC29B7C97C50DD );
	uint64_t a, b, c, d;

	a5hash_umul128( Seed2 ^ ( UseSeed & val10 ),
		Seed1 ^ ( UseSeed & val01 ), &Seed1, &Seed2 );

	if( MsgLen < 17 )
	{
		if( MsgLen > 3 )
		{
			const uint8_t* Msg4;
			size_t mo;

			Msg4 = Msg + MsgLen - 4;
			mo = MsgLen >> 3;

			a = (uint64_t) a5hash_lu32( Msg ) << 32 | a5hash_lu32( Msg4 );

			b = (uint64_t) a5hash_lu32( Msg + mo * 4 ) << 32 |
				a5hash_lu32( Msg4 - mo * 4 );

		_fin16:
			a5hash_umul128( a + Seed1, b + Seed2, &Seed1, &Seed2 );

			a5hash_umul128( val01 ^ Seed1, Seed2, &a, &b );

			a ^= b;

			if( rh != A5HASH_NULL )
			{
				a5hash_umul128( Seed1 ^ Seed3, Seed2 ^ Seed4, &Seed3, &Seed4 );

				Seed3 ^= Seed4;
				memcpy( rh, &Seed3, 8 );
			}

			return( a );
		}
		else
		{
			a = 0;
			b = 0;

			if( MsgLen != 0 )
			{
				a = Msg[ 0 ];

				if( MsgLen != 1 )
				{
					a |= (uint64_t) Msg[ 1 ] << 8;

					if( MsgLen != 2 )
					{
						a |= (uint64_t) Msg[ 2 ] << 16;
					}
				}
			}

			goto _fin16;
		}
	}

	if( MsgLen < 33 )
	{
		a = a5hash_lu64( Msg );
		b = a5hash_lu64( Msg + 8 );
		c = a5hash_lu64( Msg + MsgLen - 16 );
		d = a5hash_lu64( Msg + MsgLen - 8 );

	_fin_m:
		a5hash_umul128( c + Seed3, d + Seed4, &Seed3, &Seed4 );

	_fin:
		Seed1 ^= Seed3;
		Seed2 ^= Seed4;

		a5hash_umul128( a + Seed1, b + Seed2, &Seed1, &Seed2 );

		a5hash_umul128( val01 ^ Seed1, Seed2, &a, &b );

		a ^= b;

		if( rh != A5HASH_NULL )
		{
			a5hash_umul128( Seed1 ^ Seed3, Seed2 ^ Seed4, &Seed3, &Seed4 );

			Seed3 ^= Seed4;
			memcpy( rh, &Seed3, 8 );
		}

		return( a );
	}
	else
	{
		val01 ^= Seed1;
		val10 ^= Seed2;

		if( MsgLen > 64 )
		{
			uint64_t Seed5 = A5HASH_U64_C( 0x082EFA98EC4E6C89 );
			uint64_t Seed6 = A5HASH_U64_C( 0x3F84D5B5B5470917 );
			uint64_t Seed7 = A5HASH_U64_C( 0x13198A2E03707344 );
			uint64_t Seed8 = A5HASH_U64_C( 0xBE5466CF34E90C6C );

			do
			{
				const uint64_t s1 = Seed1;
				const uint64_t s3 = Seed3;
				const uint64_t s5 = Seed5;

				a5hash_umul128( a5hash_lu64( Msg ) + Seed1,
					a5hash_lu64( Msg + 32 ) + Seed2, &Seed1, &Seed2 );

				Seed1 += val01;
				Seed2 += Seed8;

				a5hash_umul128( a5hash_lu64( Msg + 8 ) + Seed3,
					a5hash_lu64( Msg + 40 ) + Seed4, &Seed3, &Seed4 );

				Seed3 += s1;
				Seed4 += val10;

				a5hash_umul128( a5hash_lu64( Msg + 16 ) + Seed5,
					a5hash_lu64( Msg + 48 ) + Seed6, &Seed5, &Seed6 );

				a5hash_umul128( a5hash_lu64( Msg + 24 ) + Seed7,
					a5hash_lu64( Msg + 56 ) + Seed8, &Seed7, &Seed8 );

				MsgLen -= 64;
				Msg += 64;

				Seed5 += s3;
				Seed6 += val10;
				Seed7 += s5;
				Seed8 += val10;

			} while( MsgLen > 64 );

			Seed1 ^= Seed5;
			Seed2 ^= Seed6;
			Seed3 ^= Seed7;
			Seed4 ^= Seed8;

			if( MsgLen > 32 )
			{
				goto _tail32;
			}
		}
		else
		{
			uint64_t s1;

		_tail32:
			s1 = Seed1;

			a5hash_umul128( a5hash_lu64( Msg ) + Seed1,
				a5hash_lu64( Msg + 8 ) + Seed2, &Seed1, &Seed2 );

			Seed1 += val01;
			Seed2 += Seed4;

			a5hash_umul128( a5hash_lu64( Msg + 16 ) + Seed3,
				a5hash_lu64( Msg + 24 ) + Seed4, &Seed3, &Seed4 );

			MsgLen -= 32;
			Msg += 32;

			Seed3 += s1;
			Seed4 += val10;
		}

		a = a5hash_lu64( Msg + MsgLen - 16 );
		b = a5hash_lu64( Msg + MsgLen - 8 );

		if( MsgLen < 17 )
		{
			goto _fin;
		}

		c = a5hash_lu64( Msg + MsgLen - 32 );
		d = a5hash_lu64( Msg + MsgLen - 24 );

		goto _fin_m;
	}
}

/**
 * @brief A5RAND 64-bit pseudo-random number generator.
 *
 * Simple, reliable, self-starting yet efficient PRNG, with 2^64 period.
 * 0.50 cycles/byte performance. Self-starts in 4 iterations, which is a
 * suggested "warm up" before using its output, if seeds were initialized with
 * arbitrary values.
 *
 * The `Seed1` and `Seed2` variables can be independently initialized with two
 * high-quality uniformly-random values (e.g., from operating system's
 * entropy, or a hash function's outputs). Such initialization reduces the
 * number of "warm up" iterations - that way the PRNG output will be valid
 * from the let go.
 *
 * @param[in,out] Seed1 Seed value 1. Can be initialized to any value
 * (even 0). Should not be used as PRNG value.
 * @param[in,out] Seed2 Seed value 2. In the simplest case, can be initialized
 * to the same value as `Seed1`. Should not be used as PRNG value.
 * @return The next uniformly-random 64-bit value.
 */

A5HASH_INLINE_F uint64_t a5rand( uint64_t* const Seed1,
	uint64_t* const Seed2 ) A5HASH_NOEX
{
	uint64_t s1 = *Seed1;
	uint64_t s2 = *Seed2;

	a5hash_umul128( s1 + A5HASH_VAL01, s2 + A5HASH_VAL10, &s1, &s2 );

	*Seed1 = s1;
	*Seed2 = s2;

	return( s1 ^ s2 );
}

#if defined( A5HASH_NS )

} // namespace A5HASH_NS

#if !defined( A5HASH_NS_CUSTOM )

namespace {

using A5HASH_NS :: a5hash_umul128;
using A5HASH_NS :: a5hash;
using A5HASH_NS :: a5hash32;
using A5HASH_NS :: a5hash128;
using A5HASH_NS :: a5rand;

} // namespace

#endif // !defined( A5HASH_NS_CUSTOM )

#endif // defined( A5HASH_NS )

// Defines for Doxygen.

#if !defined( A5HASH_NS_CUSTOM )
	#define A5HASH_NS_CUSTOM
#endif // !defined( A5HASH_NS_CUSTOM )

#undef A5HASH_NS_CUSTOM
#undef A5HASH_U64_C
#undef A5HASH_NOEX
#undef A5HASH_NULL
#undef A5HASH_VAL10
#undef A5HASH_VAL01
#undef A5HASH_ICC_GCC
#undef A5HASH_GCC_BUILTINS
#undef A5HASH_BMI2
#undef A5HASH_INLINE
#undef A5HASH_INLINE_F

#endif // A5HASH_INCLUDED
