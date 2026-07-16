// emhash shared configuration macros
// https://github.com/ktprime/emhash
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2026 Huang Yuanbing & bailuzhou AT 163.com

#pragma once

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

// Allow users to provide their own config by defining EMH_CONFIG_INCLUDED
// before including any emhash header. User config should define all required
// macros (EMH_LIKELY, EMH_INLINE, etc.) or include this file after customizing.
#ifndef EMH_CONFIG_INCLUDED
#define EMH_CONFIG_INCLUDED

// Branch prediction hints
// Note: We intentionally avoid __assume on MSVC because it is an optimization
// assertion ("the condition is always true"), not a branch-prediction hint.
// Using __assume inside EMH_LIKELY/UNLIKELY could cause the compiler to delete
// the else branch entirely if the condition ever becomes false. MSVC has no
// direct equivalent to GCC's __builtin_expect, so we fall back to the plain
// condition on MSVC unless EMH_FORCE_MSC_ASSUME is defined.
#if defined(__GNUC__) || defined(__clang__)
#define EMH_LIKELY(condition) __builtin_expect(!!(condition), 1)
#define EMH_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#define EMH_ASSUME(cond)                                                                                               \
    do {                                                                                                               \
        if (!(cond))                                                                                                   \
            __builtin_unreachable();                                                                                   \
    } while (0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1920) && defined(EMH_FORCE_MSC_ASSUME)
#include <intrin.h>
#define EMH_LIKELY(condition) ((condition) ? ((void)__assume(condition), 1) : 0)
#define EMH_UNLIKELY(condition) ((condition) ? 1 : ((void)__assume(!(condition)), 0))
#define EMH_ASSUME(cond) __assume(cond)
#else
#define EMH_LIKELY(condition) (condition)
#define EMH_UNLIKELY(condition) (condition)
#if defined(_MSC_VER)
#define EMH_ASSUME(cond) __assume(cond)
#else
#define EMH_ASSUME(cond) ((void)0)
#endif
#endif

// Compiler detection
#if defined(_MSC_VER) && !defined(__clang__)
#define EMH_MSVC _MSC_VER
#else
#define EMH_MSVC 0
#endif

// Force inline
#if defined(__GNUC__) || defined(__clang__)
#define EMH_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define EMH_INLINE __forceinline
#else
#define EMH_INLINE inline
#endif

// Unreachable code hint
#if defined(__GNUC__) || defined(__clang__)
#define EMH_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define EMH_UNREACHABLE() __assume(0)
#else
#define EMH_UNREACHABLE() ((void)0)
#endif

// MSan false-positive workaround for uninstrumented std::string.
// When the C++ standard library is not MSan-instrumented (the common case),
// std::string internals (SSO size field, data pointer) appear uninitialized
// to MSan because the writes happen inside uninstrumented library code.
// This macro marks the string object (and its heap data, if any) as initialized
// before the hash map reads it, preventing false-positive use-of-uninitialized
// reports. No-op when MSan is not active.
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#define EMH_MSAN_UNPOISON(p, n) __msan_unpoison(p, n)
#else
#define EMH_MSAN_UNPOISON(p, n) ((void)0)
#endif
#elif defined(__SANITIZE_MEMORY__)
#include <sanitizer/msan_interface.h>
#define EMH_MSAN_UNPOISON(p, n) __msan_unpoison(p, n)
#else
#define EMH_MSAN_UNPOISON(p, n) ((void)0)
#endif

// Compatibility aliases (used by lru_time.hpp / lru_size.hpp)
#ifndef EMHASH_LIKELY
#define EMHASH_LIKELY(condition) EMH_LIKELY(condition)
#endif
#ifndef EMHASH_UNLIKELY
#define EMHASH_UNLIKELY(condition) EMH_UNLIKELY(condition)
#endif

// Default cache line size
#ifndef EMH_CACHE_LINE_SIZE
#define EMH_CACHE_LINE_SIZE 64
#endif
#ifndef EMHASH_CACHE_LINE_SIZE
#define EMHASH_CACHE_LINE_SIZE EMH_CACHE_LINE_SIZE
#endif

// ============================================================================
// Built-in wyhash implementation (unified for all hash table variants)
// Based on wyhash v4.2 by Wang Yi (https://github.com/wangyi-fudan/wyhash)
// Released under the Unlicense (public domain).
// Provides emh_wyhash(key, len, seed) -> uint64_t for fast string hashing.
//
// Users can define EMH_NO_BUILTIN_WYHASH before including config.hpp to disable
// this built-in and provide their own wyhash via external wyhash.h instead.
// ============================================================================
#ifndef EMH_NO_BUILTIN_WYHASH
#ifndef EMH_WYHASH_DEFINED
#define EMH_WYHASH_DEFINED

#include <cstdint>
#include <cstring>

namespace emhash_detail {

// 128-bit multiply-and-xor mix (MUM)
static EMH_INLINE void wymum(uint64_t* A, uint64_t* B) {
#if defined(__SIZEOF_INT128__)
    __uint128_t r = *A;
    r *= *B;
    *A = static_cast<uint64_t>(r);
    *B = static_cast<uint64_t>(r >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
    *A = _umul128(*A, *B, B);
#else
    // Portable 64x64->128 multiply (fallback for 32-bit or unusual platforms)
    uint64_t ha = *A >> 32, hb = *B >> 32, la = static_cast<uint32_t>(*A), lb = static_cast<uint32_t>(*B);
    uint64_t hi = ha * hb, lo = la * lb, rh = ha * lb, rl = hb * la;
    uint64_t t = lo + (rl << 32);
    hi += (t < lo) + (rh >> 32) + (rl >> 32);
    lo = t + (rh << 32);
    hi += (lo < t);
    *A = lo;
    *B = hi;
#endif
}

static EMH_INLINE uint64_t wymix(uint64_t A, uint64_t B) {
    wymum(&A, &B);
    return A ^ B;
}

static EMH_INLINE uint64_t wyr8(const uint8_t* p) {
    uint64_t v;
    std::memcpy(&v, p, 8);
    return v;
}

static EMH_INLINE uint64_t wyr4(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

static EMH_INLINE uint64_t wyr3(const uint8_t* p, size_t k) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return ((static_cast<uint64_t>(p[0]) << 16) | (static_cast<uint64_t>(p[k >> 1]) << 8)) | p[k - 1];
}

} // namespace emhash_detail

// Default secret parameters (same as wyhash v4.2)
static const uint64_t EMH_WYP[4] = {0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull,
                                    0x4d5a2da51de1aa47ull};

// Main wyhash function: hash arbitrary bytes with a seed.
// API compatible with wyhash(key, len, seed) from wyhash.h v4.2.
// Marked EMH_INLINE: always inlined into hash_key() hot path — no function call overhead.
static EMH_INLINE uint64_t emh_wyhash(const void* key, size_t len, uint64_t seed) {
    using namespace emhash_detail;
    const uint8_t* p = static_cast<const uint8_t*>(key);
    seed = wymix(seed ^ EMH_WYP[0], EMH_WYP[1]);
    uint64_t a = 0, b = 0;

    if (len <= 16) {
        if (len >= 4) {
            auto half = (len >> 3) << 2;
            a = (wyr4(p) << 32U) | wyr4(p + half);
            b = (wyr4(p + len - 4) << 32U) | wyr4(p + len - 4 - half);
        } else if (len > 0) {
            a = wyr3(p, len);
        }
    } else {
        size_t i = len;
        if (i >= 48) {
            uint64_t see1 = seed, see2 = seed;
            do {
                seed = wymix(wyr8(p) ^ EMH_WYP[1], wyr8(p + 8) ^ seed);
                see1 = wymix(wyr8(p + 16) ^ EMH_WYP[2], wyr8(p + 24) ^ see1);
                see2 = wymix(wyr8(p + 32) ^ EMH_WYP[3], wyr8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (i >= 48);
            seed ^= see1 ^ see2;
        }
        while (i > 16) {
            seed = wymix(wyr8(p) ^ EMH_WYP[1], wyr8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = wyr8(p + i - 16);
        b = wyr8(p + i - 8);
    }

    a ^= EMH_WYP[1];
    b ^= seed;
    emhash_detail::wymum(&a, &b);
    return wymix(a ^ EMH_WYP[0] ^ len, b ^ EMH_WYP[1]);
}

// Backward-compatible alias: wyhash() maps to emh_wyhash()
// This allows existing code that calls wyhash(key, len, seed) to work without external wyhash.h.
// If an external wyhash.h is already included (wyhash_final_version_4_2 is defined),
// skip the alias to avoid conflicts.
#if !defined(wyhash_defined) && !defined(wyhash_final_version_4_2)
#define wyhash_defined
namespace emhash_detail {
static EMH_INLINE uint64_t wyhash(const void* key, size_t len, uint64_t seed) {
    return emh_wyhash(key, len, seed);
}
} // namespace emhash_detail
using emhash_detail::wyhash;
#endif

// 64-bit -> 64-bit mix function (used by hash_table8 for integer hashing)
#ifndef emh_wyhash64_defined
#define emh_wyhash64_defined
namespace emhash_detail {
static EMH_INLINE uint64_t emh_wyhash64(uint64_t A, uint64_t B) {
    A ^= 0x2d358dccaa6c78a5ull;
    B ^= 0x8bb84b93962eacc9ull;
    emhash_detail::wymum(&A, &B);
    return emhash_detail::wymix(A ^ 0x2d358dccaa6c78a5ull, B ^ 0x8bb84b93962eacc9ull);
}
} // namespace emhash_detail
using emhash_detail::emh_wyhash64;
#endif

#endif // EMH_WYHASH_DEFINED
#endif // EMH_NO_BUILTIN_WYHASH

#endif // EMH_CONFIG_INCLUDED
