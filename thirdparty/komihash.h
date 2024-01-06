/**
 * komihash.h version 5.8
 *
 * The inclusion file for the "komihash" hash function, "komirand" 64-bit
 * PRNG, and streamed "komihash" implementation.
 *
 * This function is named the way it is named is to honor the Komi Republic
 * (located in Russia), native to the author.
 *
 * Description is available at https://github.com/avaneev/komihash
 * E-mail: aleksey.vaneev@gmail.com or info@voxengo.com
 *
 * License
 *
 * Copyright (c) 2021-2023 Aleksey Vaneev
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

#ifndef KOMIHASH_INCLUDED
#define KOMIHASH_INCLUDED

#include <stdint.h>
#include <string.h>

// Endianness definition macro, can be used as a logical constant. Can be
// defined externally (e.g. =1, if endianness-correction is unnecessary in any
// case, to reduce its associated overhead).

#if !defined( KOMIHASH_LITTLE_ENDIAN )
	#if defined( __LITTLE_ENDIAN__ ) || defined( __LITTLE_ENDIAN ) || \
		defined( _LITTLE_ENDIAN ) || defined( _WIN32 ) || defined( i386 ) || \
		defined( __i386 ) || defined( __i386__ ) || defined( __x86_64__ ) || \
		( defined( __BYTE_ORDER__ ) && \
			__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )

		#define KOMIHASH_LITTLE_ENDIAN 1

	#elif defined( __BIG_ENDIAN__ ) || defined( __BIG_ENDIAN ) || \
		defined( _BIG_ENDIAN ) || defined( __SYSC_ZARCH__ ) || \
		defined( __zarch__ ) || defined( __s390x__ ) || defined( __sparc ) || \
		defined( __sparc__ ) || ( defined( __BYTE_ORDER__ ) && \
			__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ )

		#define KOMIHASH_LITTLE_ENDIAN 0

	#else // defined( __BIG_ENDIAN__ )

		#warning KOMIHASH: cannot determine endianness, assuming little-endian.

		#define KOMIHASH_LITTLE_ENDIAN 1

	#endif // defined( __BIG_ENDIAN__ )
#endif // !defined( KOMIHASH_LITTLE_ENDIAN )

// Macro that denotes availability of required GCC-style built-in functions.

#if defined( __GNUC__ ) || defined( __clang__ ) || \
	defined( __IBMC__ ) || defined( __IBMCPP__ ) || defined( __COMPCERT__ )

	#define KOMIHASH_GCC_BUILTINS

#endif // GCC built-ins.

// Macros that apply byte-swapping, used for endianness-correction.

#if KOMIHASH_LITTLE_ENDIAN

	#define KOMIHASH_EC32( v ) ( v )
	#define KOMIHASH_EC64( v ) ( v )

#else // KOMIHASH_LITTLE_ENDIAN

	#if defined( KOMIHASH_GCC_BUILTINS )

		#define KOMIHASH_EC32( v ) __builtin_bswap32( v )
		#define KOMIHASH_EC64( v ) __builtin_bswap64( v )

	#elif defined( _MSC_VER )

		#include <intrin.h>

		#define KOMIHASH_EC32( v ) _byteswap_ulong( v )
		#define KOMIHASH_EC64( v ) _byteswap_uint64( v )

	#else // defined( _MSC_VER )

		#define KOMIHASH_EC32( v ) ( \
			( v & 0xFF000000 ) >> 24 | \
			( v & 0x00FF0000 ) >> 8 | \
			( v & 0x0000FF00 ) << 8 | \
			( v & 0x000000FF ) << 24 )

		#define KOMIHASH_EC64( v ) ( \
			( v & 0xFF00000000000000 ) >> 56 | \
			( v & 0x00FF000000000000 ) >> 40 | \
			( v & 0x0000FF0000000000 ) >> 24 | \
			( v & 0x000000FF00000000 ) >> 8 | \
			( v & 0x00000000FF000000 ) << 8 | \
			( v & 0x0000000000FF0000 ) << 24 | \
			( v & 0x000000000000FF00 ) << 40 | \
			( v & 0x00000000000000FF ) << 56 )

	#endif // defined( _MSC_VER )

#endif // KOMIHASH_LITTLE_ENDIAN

// Likelihood macros that are used for manually-guided micro-optimization.

#if defined( KOMIHASH_GCC_BUILTINS )

	#define KOMIHASH_LIKELY( x ) __builtin_expect( x, 1 )
	#define KOMIHASH_UNLIKELY( x ) __builtin_expect( x, 0 )

#else // likelihood macros

	#define KOMIHASH_LIKELY( x ) ( x )
	#define KOMIHASH_UNLIKELY( x ) ( x )

#endif // likelihood macros

// Memory address prefetch macro (temporal locality=1, in case a collision
// resolution would be necessary).

#if defined( KOMIHASH_GCC_BUILTINS ) && !defined( __COMPCERT__ )

	#define KOMIHASH_PREFETCH( addr ) __builtin_prefetch( addr, 0, 1 )

#else // prefetch macro

	#define KOMIHASH_PREFETCH( addr )

#endif // prefetch macro

// Macro to force code inlining.

#if defined( KOMIHASH_GCC_BUILTINS )

	#define KOMIHASH_INLINE inline __attribute__((always_inline))

#else // defined( KOMIHASH_GCC_BUILTINS )

	#define KOMIHASH_INLINE inline

#endif // defined( KOMIHASH_GCC_BUILTINS )

/**
 * An auxiliary function that returns an unsigned 32-bit value created out of
 * a sequence of bytes in memory. This function is used to convert endianness
 * of in-memory 32-bit unsigned values, and to avoid unaligned memory
 * accesses.
 *
 * @param p Pointer to 4 bytes in memory. Alignment is unimportant.
 * @return Endianness-corrected 32-bit value from memory.
 */

static KOMIHASH_INLINE uint32_t kh_lu32ec( const uint8_t* const p )
{
	uint32_t v;
	memcpy( &v, p, 4 );

	return( KOMIHASH_EC32( v ));
}

/**
 * An auxiliary function that returns an unsigned 64-bit value created out of
 * a sequence of bytes in memory. This function is used to convert endianness
 * of in-memory 64-bit unsigned values, and to avoid unaligned memory
 * accesses.
 *
 * @param p Pointer to 8 bytes in memory. Alignment is unimportant.
 * @return Endianness-corrected 64-bit value from memory.
 */

static KOMIHASH_INLINE uint64_t kh_lu64ec( const uint8_t* const p )
{
	uint64_t v;
	memcpy( &v, p, 8 );

	return( KOMIHASH_EC64( v ));
}

/**
 * Function builds an unsigned 64-bit value out of remaining bytes in a
 * message, and pads it with the "final byte". This function can only be
 * called if less than 8 bytes are left to read. The message should be "long",
 * permitting Msg[ -3 ] reads.
 *
 * @param Msg Message pointer, alignment is unimportant.
 * @param MsgLen Message's remaining length, in bytes; can be 0.
 * @return Final byte-padded value from the message.
 */

static KOMIHASH_INLINE uint64_t kh_lpu64ec_l3( const uint8_t* const Msg,
	const size_t MsgLen )
{
	const int ml8 = (int) ( MsgLen * 8 );

	if( MsgLen < 4 )
	{
		const uint8_t* const Msg3 = Msg + MsgLen - 3;
		const uint64_t m = (uint64_t) Msg3[ 0 ] | (uint64_t) Msg3[ 1 ] << 8 |
			(uint64_t) Msg3[ 2 ] << 16;

		return( (uint64_t) 1 << ml8 | m >> ( 24 - ml8 ));
	}

	const uint64_t mh = kh_lu32ec( Msg + MsgLen - 4 );
	const uint64_t ml = kh_lu32ec( Msg );

	return( (uint64_t) 1 << ml8 | ml | ( mh >> ( 64 - ml8 )) << 32 );
}

/**
 * Function builds an unsigned 64-bit value out of remaining bytes in a
 * message, and pads it with the "final byte". This function can only be
 * called if less than 8 bytes are left to read. Can be used on "short"
 * messages, but MsgLen should be greater than 0.
 *
 * @param Msg Message pointer, alignment is unimportant.
 * @param MsgLen Message's remaining length, in bytes; cannot be 0.
 * @return Final byte-padded value from the message.
 */

static KOMIHASH_INLINE uint64_t kh_lpu64ec_nz( const uint8_t* const Msg,
	const size_t MsgLen )
{
	const int ml8 = (int) ( MsgLen * 8 );

	if( MsgLen < 4 )
	{
		uint64_t m = Msg[ 0 ];

		if( MsgLen > 1 )
		{
			m |= (uint64_t) Msg[ 1 ] << 8;

			if( MsgLen > 2 )
			{
				m |= (uint64_t) Msg[ 2 ] << 16;
			}
		}

		return( (uint64_t) 1 << ml8 | m );
	}

	const uint64_t mh = kh_lu32ec( Msg + MsgLen - 4 );
	const uint64_t ml = kh_lu32ec( Msg );

	return( (uint64_t) 1 << ml8 | ml | ( mh >> ( 64 - ml8 )) << 32 );
}

/**
 * Function builds an unsigned 64-bit value out of remaining bytes in a
 * message, and pads it with the "final byte". This function can only be
 * called if less than 8 bytes are left to read. The message should be "long",
 * permitting Msg[ -4 ] reads.
 *
 * @param Msg Message pointer, alignment is unimportant.
 * @param MsgLen Message's remaining length, in bytes; can be 0.
 * @return Final byte-padded value from the message.
 */

static KOMIHASH_INLINE uint64_t kh_lpu64ec_l4( const uint8_t* const Msg,
	const size_t MsgLen )
{
	const int ml8 = (int) ( MsgLen * 8 );

	if( MsgLen < 5 )
	{
		const uint64_t m = kh_lu32ec( Msg + MsgLen - 4 );

		return( (uint64_t) 1 << ml8 | m >> ( 32 - ml8 ));
	}

	const uint64_t m = kh_lu64ec( Msg + MsgLen - 8 );

	return( (uint64_t) 1 << ml8 | m >> ( 64 - ml8 ));
}

#if defined( _MSC_VER ) && ( defined( _M_ARM64 ) || \
	( defined( _M_X64 ) && defined( __INTEL_COMPILER )))

	#include <intrin.h>

	/**
	 * 64-bit by 64-bit unsigned multiplication.
	 *
	 * @param m1 Multiplier 1.
	 * @param m2 Multiplier 2.
	 * @param[out] rl The lower half of the 128-bit result.
	 * @param[in,out] rha The accumulator to receive the higher half of the
	 * 128-bit result.
	 */

	static KOMIHASH_INLINE void kh_m128( const uint64_t m1, const uint64_t m2,
		uint64_t* const rl, uint64_t* const rha )
	{
		*rl = m1 * m2;
		*rha += __umulh( m1, m2 );
	}

#elif defined( _MSC_VER ) && ( defined( _M_X64 ) || defined( _M_IA64 ))

	#include <intrin.h>
	#pragma intrinsic(_umul128)

	static KOMIHASH_INLINE void kh_m128( const uint64_t m1, const uint64_t m2,
		uint64_t* const rl, uint64_t* const rha )
	{
		uint64_t rh;
		*rl = _umul128( m1, m2, &rh );
		*rha += rh;
	}

#elif defined( __SIZEOF_INT128__ )

	static KOMIHASH_INLINE void kh_m128( const uint64_t m1, const uint64_t m2,
		uint64_t* const rl, uint64_t* const rha )
	{
		const unsigned __int128 r = (unsigned __int128) m1 * m2;

		*rha += (uint64_t) ( r >> 64 );
		*rl = (uint64_t) r;
	}

#elif ( defined( __IBMC__ ) || defined( __IBMCPP__ )) && defined( __LP64__ )

	static KOMIHASH_INLINE void kh_m128( const uint64_t m1, const uint64_t m2,
		uint64_t* const rl, uint64_t* const rha )
	{
		*rl = m1 * m2;
		*rha += __mulhdu( m1, m2 );
	}

#else // defined( __IBMC__ )

	// _umul128() code for 32-bit systems, adapted from Hacker's Delight,
	// Henry S. Warren, Jr.

	#if defined( _MSC_VER ) && !defined( __INTEL_COMPILER )

		#include <intrin.h>
		#pragma intrinsic(__emulu)

		#define KOMIHASH_EMULU( x, y ) __emulu( x, y )

	#else // defined( _MSC_VER ) && !defined( __INTEL_COMPILER )

		#define KOMIHASH_EMULU( x, y ) ( (uint64_t) ( x ) * ( y ))

	#endif // defined( _MSC_VER ) && !defined( __INTEL_COMPILER )

	static inline void kh_m128( const uint64_t u, const uint64_t v,
		uint64_t* const rl, uint64_t* const rha )
	{
		*rl = u * v;

		const uint32_t u0 = (uint32_t) u;
		const uint32_t v0 = (uint32_t) v;
		const uint64_t w0 = KOMIHASH_EMULU( u0, v0 );
		const uint32_t u1 = (uint32_t) ( u >> 32 );
		const uint32_t v1 = (uint32_t) ( v >> 32 );
		const uint64_t t = KOMIHASH_EMULU( u1, v0 ) + (uint32_t) ( w0 >> 32 );
		const uint64_t w1 = KOMIHASH_EMULU( u0, v1 ) + (uint32_t) t;

		*rha += KOMIHASH_EMULU( u1, v1 ) + (uint32_t) ( w1 >> 32 ) +
			(uint32_t) ( t >> 32 );
	}

#endif // defined( __IBMC__ )

// Macro for common hashing round with 16-byte input.

#define KOMIHASH_HASH16( m ) \
	kh_m128( Seed1 ^ kh_lu64ec( m ), \
		Seed5 ^ kh_lu64ec( m + 8 ), &Seed1, &Seed5 ); \
	Seed1 ^= Seed5;

// Macro for common hashing round without input.

#define KOMIHASH_HASHROUND() \
	kh_m128( Seed1, Seed5, &Seed1, &Seed5 ); \
	Seed1 ^= Seed5;

// Macro for common hashing finalization round, with the final hashing input
// expected in the "r1h" and "r2h" temporary variables. The macro inserts the
// function return instruction.

#define KOMIHASH_HASHFIN() \
	kh_m128( r1h, r2h, &Seed1, &Seed5 ); \
	Seed1 ^= Seed5; \
	KOMIHASH_HASHROUND(); \
	return( Seed1 );

// Macro for a common 64-byte full-performance hashing loop. Expects Msg and
// MsgLen values (greater than 63), requires initialized Seed1-8 values.
//
// The "shifting" arrangement of Seed1-4 (below) does not increase individual
// SeedN's PRNG period beyond 2^64, but reduces a chance of any occassional
// synchronization between PRNG lanes happening. Practically, Seed1-4 together
// become a single "fused" 256-bit PRNG value, having 2^66 summary PRNG
// period.

#define KOMIHASH_HASHLOOP64() \
	do \
	{ \
		KOMIHASH_PREFETCH( Msg ); \
	\
		kh_m128( Seed1 ^ kh_lu64ec( Msg ), \
			Seed5 ^ kh_lu64ec( Msg + 32 ), &Seed1, &Seed5 ); \
	\
		kh_m128( Seed2 ^ kh_lu64ec( Msg + 8 ), \
			Seed6 ^ kh_lu64ec( Msg + 40 ), &Seed2, &Seed6 ); \
	\
		kh_m128( Seed3 ^ kh_lu64ec( Msg + 16 ), \
			Seed7 ^ kh_lu64ec( Msg + 48 ), &Seed3, &Seed7 ); \
	\
		kh_m128( Seed4 ^ kh_lu64ec( Msg + 24 ), \
			Seed8 ^ kh_lu64ec( Msg + 56 ), &Seed4, &Seed8 ); \
	\
		Msg += 64; \
		MsgLen -= 64; \
	\
		Seed2 ^= Seed5; \
		Seed3 ^= Seed6; \
		Seed4 ^= Seed7; \
		Seed1 ^= Seed8; \
	\
	} while( KOMIHASH_LIKELY( MsgLen > 63 ));

/**
 * The hashing epilogue function (for internal use).
 *
 * @param Msg Pointer to the remaining part of the message.
 * @param MsgLen Remaining part's length, can be zero.
 * @param Seed1 Latest Seed1 value.
 * @param Seed5 Latest Seed5 value.
 * @return 64-bit hash value.
 */

static KOMIHASH_INLINE uint64_t komihash_epi( const uint8_t* Msg,
	size_t MsgLen, uint64_t Seed1, uint64_t Seed5 )
{
	uint64_t r1h, r2h;

	if( KOMIHASH_LIKELY( MsgLen > 31 ))
	{
		KOMIHASH_HASH16( Msg );
		KOMIHASH_HASH16( Msg + 16 );

		Msg += 32;
		MsgLen -= 32;
	}

	if( MsgLen > 15 )
	{
		KOMIHASH_HASH16( Msg );

		Msg += 16;
		MsgLen -= 16;
	}

	if( MsgLen > 7 )
	{
		r2h = Seed5 ^ kh_lpu64ec_l4( Msg + 8, MsgLen - 8 );
		r1h = Seed1 ^ kh_lu64ec( Msg );
	}
	else
	{
		r1h = Seed1 ^ kh_lpu64ec_l4( Msg, MsgLen );
		r2h = Seed5;
	}

	KOMIHASH_HASHFIN();
}

/**
 * KOMIHASH hash function. Produces and returns a 64-bit hash value of the
 * specified message, string, or binary data block. Designed for 64-bit
 * hash-table and hash-map uses. Produces identical hashes on both big- and
 * little-endian systems.
 *
 * @param Msg0 The message to produce a hash from. The alignment of this
 * pointer is unimportant. It is valid to pass 0 when MsgLen==0 (assuming that
 * compiler's implementation of the address prefetch permits use of zero
 * address).
 * @param MsgLen Message's length, in bytes, can be zero.
 * @param UseSeed Optional value, to use instead of the default seed. To use
 * the default seed, set to 0. The UseSeed value can have any bit length and
 * statistical quality, and is used only as an additional entropy source. May
 * need endianness-correction if this value is shared between big- and
 * little-endian systems.
 * @return 64-bit hash of the input data.
 */

static inline uint64_t komihash( const void* const Msg0, size_t MsgLen,
	const uint64_t UseSeed )
{
	const uint8_t* Msg = (const uint8_t*) Msg0;

	// The seeds are initialized to the first mantissa bits of PI.

	uint64_t Seed1 = 0x243F6A8885A308D3 ^ ( UseSeed & 0x5555555555555555 );
	uint64_t Seed5 = 0x452821E638D01377 ^ ( UseSeed & 0xAAAAAAAAAAAAAAAA );
	uint64_t r1h, r2h;

	// The three instructions in the "KOMIHASH_HASHROUND" macro represent the
	// simplest constantless PRNG, scalable to any even-sized state
	// variables, with the `Seed1` being the PRNG output (2^64 PRNG period).
	// It passes `PractRand` tests with rare non-systematic "unusual"
	// evaluations.
	//
	// To make this PRNG reliable, self-starting, and eliminate a risk of
	// stopping, the following variant can be used, which adds a "register
	// checker-board", a source of raw entropy. The PRNG is available as the
	// komirand() function. Not required for hashing (but works for it) since
	// the input entropy is usually available in abundance during hashing.
	//
	// Seed5 += r2h + 0xAAAAAAAAAAAAAAAA;
	//
	// (the `0xAAAA...` constant should match register's size; essentially,
	// it is a replication of the `10` bit-pair; it is not an arbitrary
	// constant).

	KOMIHASH_PREFETCH( Msg );

	KOMIHASH_HASHROUND(); // Required for Perlin Noise.

	if( KOMIHASH_LIKELY( MsgLen < 16 ))
	{
		r1h = Seed1;
		r2h = Seed5;

		if( MsgLen > 7 )
		{
			// The following two XOR instructions are equivalent to mixing a
			// message with a cryptographic one-time-pad (bitwise modulo 2
			// addition). Message's statistics and distribution are thus
			// unimportant.

			r2h ^= kh_lpu64ec_l3( Msg + 8, MsgLen - 8 );
			r1h ^= kh_lu64ec( Msg );
		}
		else
		if( KOMIHASH_LIKELY( MsgLen != 0 ))
		{
			r1h ^= kh_lpu64ec_nz( Msg, MsgLen );
		}

		KOMIHASH_HASHFIN();
	}

	if( KOMIHASH_LIKELY( MsgLen < 32 ))
	{
		KOMIHASH_HASH16( Msg );

		if( MsgLen > 23 )
		{
			r2h = Seed5 ^ kh_lpu64ec_l4( Msg + 24, MsgLen - 24 );
			r1h = Seed1 ^ kh_lu64ec( Msg + 16 );
		}
		else
		{
			r1h = Seed1 ^ kh_lpu64ec_l4( Msg + 16, MsgLen - 16 );
			r2h = Seed5;
		}

		KOMIHASH_HASHFIN();
	}

	if( KOMIHASH_LIKELY( MsgLen > 63 ))
	{
		uint64_t Seed2 = 0x13198A2E03707344 ^ Seed1;
		uint64_t Seed3 = 0xA4093822299F31D0 ^ Seed1;
		uint64_t Seed4 = 0x082EFA98EC4E6C89 ^ Seed1;
		uint64_t Seed6 = 0xBE5466CF34E90C6C ^ Seed5;
		uint64_t Seed7 = 0xC0AC29B7C97C50DD ^ Seed5;
		uint64_t Seed8 = 0x3F84D5B5B5470917 ^ Seed5;

		KOMIHASH_HASHLOOP64();

		Seed5 ^= Seed6 ^ Seed7 ^ Seed8;
		Seed1 ^= Seed2 ^ Seed3 ^ Seed4;
	}

	return( komihash_epi( Msg, MsgLen, Seed1, Seed5 ));
}

/**
 * Simple, reliable, self-starting yet efficient PRNG, with 2^64 period.
 * 0.62 cycles/byte performance. Self-starts in 4 iterations, which is a
 * suggested "warming up" initialization before using its output.
 *
 * @param[in,out] Seed1 Seed value 1. Can be initialized to any value
 * (even 0). This is the usual "PRNG seed" value.
 * @param[in,out] Seed2 Seed value 2, a supporting variable. Best initialized
 * to the same value as Seed1.
 * @return The next uniformly-random 64-bit value.
 */

static KOMIHASH_INLINE uint64_t komirand( uint64_t* const Seed1,
	uint64_t* const Seed2 )
{
	uint64_t s1 = *Seed1;
	uint64_t s2 = *Seed2;

	kh_m128( s1, s2, &s1, &s2 );
	s2 += 0xAAAAAAAAAAAAAAAA;
	s1 ^= s2;

	*Seed2 = s2;
	*Seed1 = s1;

	return( s1 );
}

#if !defined( KOMIHASH_BUFSIZE )

	#define KOMIHASH_BUFSIZE 768 // Streamed hashing's buffer size in bytes,
		// must be a multiple of 64, and not less than 128.

#endif // !defined( KOMIHASH_BUFSIZE )

/**
 * Context structure that holds streamed hashing state. The komihash_init()
 * function should be called to initalize the structure before hashing. Note
 * that the default buffer size is modest, permitting placement of this
 * structure on stack. Seed[ 0 ] is used as UseSeed value storage.
 */

typedef struct {
	uint8_t fb[ 8 ]; ///< Stream's final byte (at [7]), array to avoid OOB.
	uint8_t Buf[ KOMIHASH_BUFSIZE ]; ///< Buffer.
	uint64_t Seed[ 8 ]; ///< Hashing state variables.
	size_t BufFill; ///< Buffer fill count (position), in bytes.
	size_t IsHashing; ///< 0 or 1, equals 1 if the actual hashing was started.
} komihash_stream_t;

/**
 * Function initializes the streamed "komihash" session.
 *
 * @param[out] ctx Pointer to the context structure.
 * @param UseSeed Optional value, to use instead of the default seed. To use
 * the default seed, set to 0. The UseSeed value can have any bit length and
 * statistical quality, and is used only as an additional entropy source. May
 * need endianness-correction if this value is shared between big- and
 * little-endian systems.
 */

static inline void komihash_stream_init( komihash_stream_t* const ctx,
	const uint64_t UseSeed )
{
	ctx -> Seed[ 0 ] = UseSeed;
	ctx -> BufFill = 0;
	ctx -> IsHashing = 0;
}

/**
 * Function updates the streamed hashing state with a new input data.
 *
 * @param[in,out] ctx Pointer to the context structure. The structure must be
 * initialized via the komihash_stream_init() function.
 * @param Msg0 The next part of the whole message being hashed. The alignment
 * of this pointer is unimportant. It is valid to pass 0 when MsgLen==0.
 * @param MsgLen Message's length, in bytes, can be zero.
 */

static inline void komihash_stream_update( komihash_stream_t* const ctx,
	const void* const Msg0, size_t MsgLen )
{
	const uint8_t* Msg = (const uint8_t*) Msg0;

	const uint8_t* SwMsg = 0;
	size_t SwMsgLen = 0;
	size_t BufFill = ctx -> BufFill;

	if( BufFill + MsgLen >= KOMIHASH_BUFSIZE && BufFill != 0 )
	{
		const size_t CopyLen = KOMIHASH_BUFSIZE - BufFill;
		memcpy( ctx -> Buf + BufFill, Msg, CopyLen );
		BufFill = 0;

		SwMsg = Msg + CopyLen;
		SwMsgLen = MsgLen - CopyLen;

		Msg = ctx -> Buf;
		MsgLen = KOMIHASH_BUFSIZE;
	}

	if( BufFill == 0 )
	{
		while( MsgLen > 127 )
		{
			uint64_t Seed1, Seed2, Seed3, Seed4;
			uint64_t Seed5, Seed6, Seed7, Seed8;

			if( ctx -> IsHashing )
			{
				Seed1 = ctx -> Seed[ 0 ];
				Seed2 = ctx -> Seed[ 1 ];
				Seed3 = ctx -> Seed[ 2 ];
				Seed4 = ctx -> Seed[ 3 ];
				Seed5 = ctx -> Seed[ 4 ];
				Seed6 = ctx -> Seed[ 5 ];
				Seed7 = ctx -> Seed[ 6 ];
				Seed8 = ctx -> Seed[ 7 ];
			}
			else
			{
				ctx -> IsHashing = 1;

				const uint64_t UseSeed = ctx -> Seed[ 0 ];
				Seed1 = 0x243F6A8885A308D3 ^ ( UseSeed & 0x5555555555555555 );
				Seed5 = 0x452821E638D01377 ^ ( UseSeed & 0xAAAAAAAAAAAAAAAA );

				KOMIHASH_HASHROUND();

				Seed2 = 0x13198A2E03707344 ^ Seed1;
				Seed3 = 0xA4093822299F31D0 ^ Seed1;
				Seed4 = 0x082EFA98EC4E6C89 ^ Seed1;
				Seed6 = 0xBE5466CF34E90C6C ^ Seed5;
				Seed7 = 0xC0AC29B7C97C50DD ^ Seed5;
				Seed8 = 0x3F84D5B5B5470917 ^ Seed5;
			}

			KOMIHASH_HASHLOOP64();

			ctx -> Seed[ 0 ] = Seed1;
			ctx -> Seed[ 1 ] = Seed2;
			ctx -> Seed[ 2 ] = Seed3;
			ctx -> Seed[ 3 ] = Seed4;
			ctx -> Seed[ 4 ] = Seed5;
			ctx -> Seed[ 5 ] = Seed6;
			ctx -> Seed[ 6 ] = Seed7;
			ctx -> Seed[ 7 ] = Seed8;

			if( SwMsgLen == 0 )
			{
				if( MsgLen == 0 )
				{
					ctx -> fb[ 7 ] = Msg[ -1 ];
					ctx -> BufFill = 0;
					return;
				}

				break;
			}

			Msg = SwMsg;
			MsgLen = SwMsgLen;
			SwMsgLen = 0;
		}
	}

	ctx -> BufFill = BufFill + MsgLen;
	uint8_t* op = ctx -> Buf + BufFill;

	while( MsgLen != 0 )
	{
		*op = *Msg;
		Msg++;
		op++;
		MsgLen--;
	}
}

/**
 * Function finalizes the streamed hashing session, and returns the resulting
 * hash value of the previously hashed data. This value is equal to the value
 * returned by the komihash() function for the same provided data.
 *
 * Note that since this function is non-destructive to the context structure,
 * the function can be used to obtain intermediate hashes of the data stream
 * being hashed, and the hashing can then be resumed.
 *
 * @param[in] ctx Pointer to the context structure. The structure must be
 * initialized via the komihash_stream_init() function.
 * @return 64-bit hash value.
 */

static inline uint64_t komihash_stream_final( komihash_stream_t* const ctx )
{
	const uint8_t* Msg = ctx -> Buf;
	size_t MsgLen = ctx -> BufFill;

	if( ctx -> IsHashing == 0 )
	{
		return( komihash( Msg, MsgLen, ctx -> Seed[ 0 ]));
	}

	ctx -> fb[ 4 ] = 0;
	ctx -> fb[ 5 ] = 0;
	ctx -> fb[ 6 ] = 0;

	uint64_t Seed1 = ctx -> Seed[ 0 ];
	uint64_t Seed2 = ctx -> Seed[ 1 ];
	uint64_t Seed3 = ctx -> Seed[ 2 ];
	uint64_t Seed4 = ctx -> Seed[ 3 ];
	uint64_t Seed5 = ctx -> Seed[ 4 ];
	uint64_t Seed6 = ctx -> Seed[ 5 ];
	uint64_t Seed7 = ctx -> Seed[ 6 ];
	uint64_t Seed8 = ctx -> Seed[ 7 ];

	if( MsgLen > 63 )
	{
		KOMIHASH_HASHLOOP64();
	}

	Seed5 ^= Seed6 ^ Seed7 ^ Seed8;
	Seed1 ^= Seed2 ^ Seed3 ^ Seed4;

	return( komihash_epi( Msg, MsgLen, Seed1, Seed5 ));
}

/**
 * FOR TESTING PURPOSES ONLY - use the komihash() function instead.
 *
 * @param Msg The message to produce a hash from.
 * @param MsgLen Message's length, in bytes.
 * @param UseSeed Seed to use.
 * @return 64-bit hash value.
 */

static inline uint64_t komihash_stream_oneshot( const void* const Msg,
	const size_t MsgLen, const uint64_t UseSeed )
{
	komihash_stream_t ctx;

	komihash_stream_init( &ctx, UseSeed );
	komihash_stream_update( &ctx, Msg, MsgLen );

	return( komihash_stream_final( &ctx ));
}

#endif // KOMIHASH_INCLUDED
