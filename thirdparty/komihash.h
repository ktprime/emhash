/**
 * komihash.h version 4.3
 *
 * The inclusion file for the "komihash" hash function.
 *
 * Description is available at https://github.com/avaneev/komihash
 *
 * License
 *
 * Copyright (c) 2021 Aleksey Vaneev
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

// Macros that apply byte-swapping.

#if defined( __GNUC__ ) || defined( __clang__ ) || \
	( defined( __GNUC__ ) && defined( __INTEL_COMPILER ))

	#define KOMIHASH_BYTESW32( v ) __builtin_bswap32( v )
	#define KOMIHASH_BYTESW64( v ) __builtin_bswap64( v )

#elif defined( _MSC_VER )

	#define KOMIHASH_BYTESW32( v ) _byteswap_ulong( v )
	#define KOMIHASH_BYTESW64( v ) _byteswap_uint64( v )

#else // defined( _MSC_VER )

	#define KOMIHASH_BYTESW32( v ) ( \
		( v & 0xFF000000 ) >> 24 | \
		( v & 0x00FF0000 ) >> 8 | \
		( v & 0x0000FF00 ) << 8 | \
		( v & 0x000000FF ) << 24 )

	#define KOMIHASH_BYTESW64( v ) ( \
		( v & 0xFF00000000000000 ) >> 56 | \
		( v & 0x00FF000000000000 ) >> 40 | \
		( v & 0x0000FF0000000000 ) >> 24 | \
		( v & 0x000000FF00000000 ) >> 8 | \
		( v & 0x00000000FF000000 ) << 8 | \
		( v & 0x0000000000FF0000 ) << 24 | \
		( v & 0x000000000000FF00 ) << 40 | \
		( v & 0x00000000000000FF ) << 56 )

#endif // defined( _MSC_VER )

// Endianness-definition macro, can be defined externally (e.g. =1, if
// endianness-correction is unnecessary in any case, to reduce its associated
// overhead).

#if !defined( KOMIHASH_LITTLE_ENDIAN )
	#if defined( _WIN32 ) || defined( __LITTLE_ENDIAN__ ) || \
		( defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )

		#define KOMIHASH_LITTLE_ENDIAN 1

	#elif defined( __BIG_ENDIAN__ ) || \
		( defined( __BYTE_ORDER__ ) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ )

		#define KOMIHASH_LITTLE_ENDIAN 0

	#else // defined( __BIG_ENDIAN__ )

		#warning KOMIHASH: cannot determine endianness, assuming little-endian.

		#define KOMIHASH_LITTLE_ENDIAN 1

	#endif // defined( __BIG_ENDIAN__ )
#endif // !defined( KOMIHASH_LITTLE_ENDIAN )

// Macros that apply byte-swapping, used for endianness-correction.

#if KOMIHASH_LITTLE_ENDIAN

	#define KOMIHASH_EC32( v ) ( v )
	#define KOMIHASH_EC64( v ) ( v )

#else // KOMIHASH_LITTLE_ENDIAN

	#define KOMIHASH_EC32( v ) KOMIHASH_BYTESW32( v )
	#define KOMIHASH_EC64( v ) KOMIHASH_BYTESW64( v )

#endif // KOMIHASH_LITTLE_ENDIAN

// Likelihood macros that are used for manually-guided micro-optimization.

#if defined( __GNUC__ ) || defined( __clang__ ) || \
	( defined( __GNUC__ ) && defined( __INTEL_COMPILER ))

	#define KOMIHASH_LIKELY( x )  __builtin_expect( x, 1 )
	#define KOMIHASH_UNLIKELY( x )  __builtin_expect( x, 0 )

#else // likelihood macros

	#define KOMIHASH_LIKELY( x ) ( x )
	#define KOMIHASH_UNLIKELY( x ) ( x )

#endif // likelihood macros

// In-memory data prefetch macro (temporal locality=1, in case a collision
// resolution would be necessary).

#if defined( __GNUC__ ) || defined( __clang__ ) || \
	( defined( __GNUC__ ) && defined( __INTEL_COMPILER ))

	#define KOMIHASH_PREFETCH( addr ) __builtin_prefetch( addr, 0, 1 )

#else // prefetch macro

	#define KOMIHASH_PREFETCH( addr )

#endif // prefetch macro

/**
 * An auxiliary function that returns an unsigned 32-bit value created out of
 * a sequence of bytes in memory. This function is used to convert endianness
 * of in-memory 32-bit unsigned values, and to avoid unaligned memory
 * accesses.
 *
 * @param p Pointer to 4 bytes in memory. Alignment is unimportant.
 */

static inline uint32_t kh_lu32ec( const uint8_t* const p )
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
 */

static inline uint64_t kh_lu64ec( const uint8_t* const p )
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
 * @param fb Final byte used for padding.
 */

static inline uint64_t kh_lpu64ec_l3( const uint8_t* const Msg,
	const size_t MsgLen, uint64_t fb )
{
	if( MsgLen < 4 )
	{
		const uint8_t* const Msg3 = Msg + MsgLen - 3;
		const int ml8 = (int) ( MsgLen << 3 );
		const uint64_t m = (uint64_t) Msg3[ 0 ] | (uint64_t) Msg3[ 1 ] << 8 |
			(uint64_t) Msg3[ 2 ] << 16;

		return( fb << ml8 | m >> ( 24 - ml8 ));
	}

	const int ml8 = (int) ( MsgLen << 3 );
	const uint64_t mh = kh_lu32ec( Msg + MsgLen - 4 );
	const uint64_t ml = kh_lu32ec( Msg );

	return( fb << ml8 | ml | ( mh >> ( 64 - ml8 )) << 32 );
}

/**
 * Function builds an unsigned 64-bit value out of remaining bytes in a
 * message, and pads it with the "final byte". This function can only be
 * called if less than 8 bytes are left to read. Can be used on "short"
 * messages, but MsgLen should be greater than 0.
 *
 * @param Msg Message pointer, alignment is unimportant.
 * @param MsgLen Message's remaining length, in bytes; cannot be 0.
 * @param fb Final byte used for padding.
 */

static inline uint64_t kh_lpu64ec_nz( const uint8_t* const Msg,
	const size_t MsgLen, uint64_t fb )
{
	if( MsgLen < 4 )
	{
		fb <<= ( MsgLen << 3 );
		uint64_t m = Msg[ 0 ];

		if( MsgLen > 1 )
		{
			m |= (uint64_t) Msg[ 1 ] << 8;

			if( MsgLen > 2 )
			{
				m |= (uint64_t) Msg[ 2 ] << 16;
			}
		}

		return( fb | m );
	}

	const int ml8 = (int) ( MsgLen << 3 );
	const uint64_t mh = kh_lu32ec( Msg + MsgLen - 4 );
	const uint64_t ml = kh_lu32ec( Msg );

	return( fb << ml8 | ml | ( mh >> ( 64 - ml8 )) << 32 );
}

/**
 * Function builds an unsigned 64-bit value out of remaining bytes in a
 * message, and pads it with the "final byte". This function can only be
 * called if less than 8 bytes are left to read. The message should be "long",
 * permitting Msg[ -4 ] reads.
 *
 * @param Msg Message pointer, alignment is unimportant.
 * @param MsgLen Message's remaining length, in bytes; can be 0.
 * @param fb Final byte used for padding.
 */

static inline uint64_t kh_lpu64ec_l4( const uint8_t* const Msg,
	const size_t MsgLen, uint64_t fb )
{
	if( MsgLen < 5 )
	{
		const int ml8 = (int) ( MsgLen << 3 );

		return( fb << ml8 |
			(uint64_t) kh_lu32ec( Msg + MsgLen - 4 ) >> ( 32 - ml8 ));
	}
	else
	{
		const int ml8 = (int) ( MsgLen << 3 );

		return( fb << ml8 | kh_lu64ec( Msg + MsgLen - 8 ) >> ( 64 - ml8 ));
	}
}

#if defined( __SIZEOF_INT128__ )

	/**
	 * 64-bit by 64-bit unsigned multiplication.
	 *
	 * @param m1 Multiplier 1.
	 * @param m2 Multiplier 2.
	 * @param[out] rl The lower half of the 128-bit result.
	 * @param[out] rh The higher half of the 128-bit result.
	 */

	static inline void kh_m128( const uint64_t m1, const uint64_t m2,
		uint64_t* const rl, uint64_t* const rh )
	{
		const __uint128_t r = (__uint128_t) m1 * m2;

		*rl = (uint64_t) r;
		*rh = (uint64_t) ( r >> 64 );
	}

#elif defined( _MSC_VER ) && defined( _M_X64 )

	#include <intrin.h>

	static inline void kh_m128( const uint64_t m1, const uint64_t m2,
		uint64_t* const rl, uint64_t* const rh )
	{
		*rl = _umul128( m1, m2, rh );
	}

#else // defined( _MSC_VER )

	// _umul128() code for 32-bit systems, from
	// https://github.com/simdjson/simdjson
	// (from file: /include/simdjson/generic/jsoncharutils.h).
	// Licensed under Apache-2.0 license.

	static inline uint64_t kh__emulu( const uint32_t x, const uint32_t y )
	{
		return( x * (uint64_t) y );
	}

	static inline void kh_m128( const uint64_t ab, const uint64_t cd,
		uint64_t* const rl, uint64_t* const rh )
	{
		uint64_t ad = kh__emulu( (uint32_t) ( ab >> 32 ), (uint32_t) cd );
		uint64_t bd = kh__emulu( (uint32_t) ab, (uint32_t) cd );
		uint64_t adbc = ad + kh__emulu( (uint32_t) ab,
			(uint32_t) ( cd >> 32 ));

		uint64_t adbc_carry = !!( adbc < ad );
		uint64_t lo = bd + ( adbc << 32 );

		*rh = kh__emulu( (uint32_t) ( ab >> 32 ), (uint32_t) ( cd >> 32 )) +
			( adbc >> 32 ) + ( adbc_carry << 32 ) + !!( lo < bd );

		*rl = lo;
	}

#endif // defined( _MSC_VER )

// Common hashing round with 16-byte input, using the "r1l" and "r1h"
// temporary variables.

#define KOMIHASH_HASH16( m ) \
	kh_m128( Seed1 ^ kh_lu64ec( m ), \
		Seed5 ^ kh_lu64ec( m + 8 ), &r1l, &r1h ); \
	Seed5 += r1h; \
	Seed1 = Seed5 ^ r1l;

// Common hashing round without input, using the "r2l" and "r2h" temporary
// variables.

#define KOMIHASH_HASHROUND() \
	kh_m128( Seed1, Seed5, &r2l, &r2h ); \
	Seed5 += r2h; \
	Seed1 = Seed5 ^ r2l;

// Common hashing finalization round, with the final hashing input expected in
// the "r2l" and "r2h" temporary variables.

#define KOMIHASH_HASHFIN() \
	kh_m128( r2l, r2h, &r1l, &r1h ); \
	Seed5 += r1h; \
	Seed1 = Seed5 ^ r1l; \
	KOMIHASH_HASHROUND();

/**
 * KOMIHASH hash function. Produces and returns a 64-bit hash value of the
 * specified message, string, or binary data block. Designed for 64-bit
 * hash-table and hash-map uses. Produces identical hashes on both big- and
 * little-endian systems.
 *
 * @param Msg0 The message to produce a hash from. The alignment of this
 * pointer is unimportant.
 * @param MsgLen Message's length, in bytes.
 * @param UseSeed Optional value, to use instead of the default seed. To use
 * the default seed, set to 0. The UseSeed value can have any bit length and
 * statistical quality, and is used only as an additional entropy source. May
 * need endianness-correction if this value is shared between big- and
 * little-endian systems.
 */

static inline uint64_t komihash( const void* const Msg0, size_t MsgLen,
	const uint64_t UseSeed )
{
	const uint8_t* Msg = (const uint8_t*) Msg0;

	// The seeds are initialized to the first mantissa bits of PI.

	uint64_t Seed1 = 0x243F6A8885A308D3 ^ ( UseSeed & 0x5555555555555555 );
	uint64_t Seed5 = 0x452821E638D01377 ^ ( UseSeed & 0xAAAAAAAAAAAAAAAA );
	uint64_t r1l, r1h, r2l, r2h;

	// The three instructions in the "KOMIHASH_HASHROUND" macro represent the
	// simplest constant-less PRNG, scalable to any even-sized state
	// variables, with the `Seed1` being the PRNG output (2^64 PRNG period).
	// It passes `PractRand` tests with rare non-systematic "unusual"
	// evaluations.
	//
	// To make this PRNG reliable, self-starting, and eliminate a risk of
	// stopping, the following variant can be used, which is a "register
	// checker-board", a source of raw entropy. The PRNG is available as the
	// komirand() function. Not required for hashing (but works for it) since
	// the input entropy is usually available in abundance during hashing.
	//
	// Seed5 += r2h + 0xAAAAAAAAAAAAAAAA;
	//
	// (the `0xAAAA...` constant should match register's size; essentially,
	// it is a replication of the `10` bit-pair; it is not an arbitrary
	// constant).

	KOMIHASH_HASHROUND(); // Required for PerlinNoise.

	if( KOMIHASH_LIKELY( MsgLen < 16 ))
	{
		KOMIHASH_PREFETCH( Msg );

		r2l = Seed1;
		r2h = Seed5;

		if( MsgLen > 7 )
		{
			// The following two XOR instructions are equivalent to mixing a
			// message with a cryptographic one-time-pad (bitwise modulo 2
			// addition). Message's statistics and distribution are thus
			// unimportant.

			r2h ^= kh_lpu64ec_l3( Msg + 8, MsgLen - 8,
				1 << ( Msg[ MsgLen - 1 ] >> 7 ));

			r2l ^= kh_lu64ec( Msg );
		}
		else
		if( KOMIHASH_LIKELY( MsgLen != 0 ))
		{
			r2l ^= kh_lpu64ec_nz( Msg, MsgLen,
				1 << ( Msg[ MsgLen - 1 ] >> 7 ));
		}

		KOMIHASH_HASHFIN();

		return( Seed1 );
	}

	if( KOMIHASH_LIKELY( MsgLen < 32 ))
	{
		KOMIHASH_PREFETCH( Msg );

		KOMIHASH_HASH16( Msg );

		const uint64_t fb = 1 << ( Msg[ MsgLen - 1 ] >> 7 );

		if( MsgLen > 23 )
		{
			r2h = Seed5 ^ kh_lpu64ec_l4( Msg + 24, MsgLen - 24, fb );
			r2l = Seed1 ^ kh_lu64ec( Msg + 16 );
		}
		else
		{
			r2l = Seed1 ^ kh_lpu64ec_l4( Msg + 16, MsgLen - 16, fb );
			r2h = Seed5;
		}

		KOMIHASH_HASHFIN();

		return( Seed1 );
	}

	if( MsgLen > 63 )
	{
		uint64_t Seed2 = 0x13198A2E03707344 ^ Seed1;
		uint64_t Seed3 = 0xA4093822299F31D0 ^ Seed1;
		uint64_t Seed4 = 0x082EFA98EC4E6C89 ^ Seed1;
		uint64_t Seed6 = 0xBE5466CF34E90C6C ^ Seed5;
		uint64_t Seed7 = 0xC0AC29B7C97C50DD ^ Seed5;
		uint64_t Seed8 = 0x3F84D5B5B5470917 ^ Seed5;
		uint64_t r3l, r3h, r4l, r4h;

		do
		{
			KOMIHASH_PREFETCH( Msg );

			kh_m128( Seed1 ^ kh_lu64ec( Msg ),
				Seed5 ^ kh_lu64ec( Msg + 8 ), &r1l, &r1h );

			kh_m128( Seed2 ^ kh_lu64ec( Msg + 16 ),
				Seed6 ^ kh_lu64ec( Msg + 24 ), &r2l, &r2h );

			kh_m128( Seed3 ^ kh_lu64ec( Msg + 32 ),
				Seed7 ^ kh_lu64ec( Msg + 40 ), &r3l, &r3h );

			kh_m128( Seed4 ^ kh_lu64ec( Msg + 48 ),
				Seed8 ^ kh_lu64ec( Msg + 56 ), &r4l, &r4h );

			Msg += 64;
			MsgLen -= 64;

			// Such "shifting" arrangement (below) does not increase
			// individual SeedN's PRNG period beyond 2^64, but reduces a
			// chance of any occassional synchronization between PRNG lanes
			// happening. Practically, Seed1-4 together become a single
			// "fused" 256-bit PRNG value, having a summary PRNG period of
			// 2^66.

			Seed5 += r1h;
			Seed6 += r2h;
			Seed7 += r3h;
			Seed8 += r4h;
			Seed2 = Seed5 ^ r2l;
			Seed3 = Seed6 ^ r3l;
			Seed4 = Seed7 ^ r4l;
			Seed1 = Seed8 ^ r1l;

		} while( KOMIHASH_LIKELY( MsgLen > 63 ));

		Seed5 ^= Seed6 ^ Seed7 ^ Seed8;
		Seed1 ^= Seed2 ^ Seed3 ^ Seed4;
	}

	KOMIHASH_PREFETCH( Msg );

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

	const uint64_t fb = 1 << ( Msg[ MsgLen - 1 ] >> 7 );

	if( MsgLen > 7 )
	{
		r2h = Seed5 ^ kh_lpu64ec_l4( Msg + 8, MsgLen - 8, fb );
		r2l = Seed1 ^ kh_lu64ec( Msg );
	}
	else
	{
		r2l = Seed1 ^ kh_lpu64ec_l4( Msg, MsgLen, fb );
		r2h = Seed5;
	}

	KOMIHASH_HASHFIN();

	return( Seed1 );
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

static inline uint64_t komirand( uint64_t* const Seed1, uint64_t* const Seed2 )
{
	uint64_t r1l, r1h;

	kh_m128( *Seed1, *Seed2, &r1l, &r1h );
	*Seed2 += r1h + 0xAAAAAAAAAAAAAAAA;
	*Seed1 = *Seed2 ^ r1l;

	return( *Seed1 );
}

#endif // KOMIHASH_INCLUDED
