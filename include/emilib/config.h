// LICENSE:
// version 1.2.0
// https://github.com/ktprime/emhash/blob/master/include/emilib/emihmap1.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2025 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

#pragma once

// Shared boilerplate for the emilib containers (emihmap1/1m/1d/2/3/4, emihset2/3):
//
//   - compiler/arch discovery: standard includes, SIMD intrin includes,
//     EMH_LIKELY / EMH_UNLIKELY, EMH_RESTRICT
//   - fixed helpers shared verbatim: CTZ, the 4KB fingerprint broadcast table
//     (fp_broadcast), the xor-fold fingerprint (emilib_fp_from_hash, takes the
//     map's EFILLED as the range base) and the MSAN-clean hash entry point
//     (emilib_hash_of)
//
// Everything at namespace scope deliberately stays in each container header:
// the State enums and SIMD vector constants differ per design (state values,
// sentinel handling, set_simd_* naming in emihset2, EM3_* macro aliases in
// emihmap3), several headers share one namespace (emihmap2/emihset2 both in
// emilib2, emihmap3/emihset3 both in emilib3) so namespace-scope definitions
// cannot be duplicated safely, and emihmap2/emihmap3 keep per-key-type
// fingerprint policies (hash % 253, Fibonacci hash of the raw key). Only
// global-scope code is shared from here.

// emilib intentionally does not include anything from emhash/: the two trees are independent.
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <type_traits>
#include <iterator>
#include <utility>
#include <cassert>
#include <stdexcept>
#include <new>

#ifdef _WIN32
#include <intrin.h>
#elif defined(__x86_64__) || defined(__amd64__) || defined(__i386__) || defined(__i686__) || defined(_M_IX86) ||       \
    defined(_M_X64)
#include <x86intrin.h>
#elif defined(__ARM_ARCH__) || defined(__aarch64__) || defined(__arm__)
#include "sse2neon.h"
#endif

#undef EMH_LIKELY
#undef EMH_UNLIKELY
#undef bucket_to_slot

// likely/unlikely
#if defined(__GNUC__) && (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1) || defined(__clang__)
#define EMH_LIKELY(condition) __builtin_expect(!!(condition), 1)
#define EMH_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1920)
#define EMH_LIKELY(condition) ((condition) ? ((void)__assume(condition), 1) : 0)
#define EMH_UNLIKELY(condition) ((condition) ? 1 : ((void)__assume(!condition), 0))
#else
#define EMH_LIKELY(condition) (condition)
#define EMH_UNLIKELY(condition) (condition)
#endif

// pointer restrict qualifier: _states/_pairs never alias each other
#if defined(__GNUC__) || defined(__clang__)
#define EMH_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define EMH_RESTRICT __restrict
#else
#define EMH_RESTRICT
#endif

// Force inline. emilib must stay self-contained (it may not include emhash/config.hpp), so
// the few macros its containers share with emhash are defined here as well, behind #ifndef
// so a translation unit that also uses emhash keeps a single definition.
#ifndef EMH_INLINE
#if defined(__GNUC__) || defined(__clang__)
#define EMH_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define EMH_INLINE __forceinline
#else
#define EMH_INLINE inline
#endif
#endif

// Assume the condition holds (used on hot paths after a load the compiler cannot see).
#ifndef EMH_ASSUME
#if defined(__GNUC__) || defined(__clang__)
#define EMH_ASSUME(cond) (static_cast<void>(!(cond) ? __builtin_unreachable() : (void)0))
#elif defined(_MSC_VER)
#define EMH_ASSUME(cond) __assume(cond)
#else
#define EMH_ASSUME(cond) ((void)0)
#endif
#endif

// MemorySanitizer: mark freshly allocated / written storage as initialized.
#ifndef EMH_MSAN_UNPOISON
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
#endif

// [[no_unique_address]] lets an empty hasher/comparator/allocator member share the byte it
// would otherwise occupy (and, when placed after the real fields, the struct's own tail
// padding) instead of pushing the pointers past 8 bytes of padding. GCC and Clang honor the
// standard attribute in C++17 too; MSVC only accepts it with /std:c++20, so it is gated on
// the feature-test macro there to avoid C5051 under -Werror.
#ifndef EMH_NO_UNIQUE_ADDRESS
#if defined(__cpp_no_unique_address) && __cpp_no_unique_address >= 201803L
#define EMH_NO_UNIQUE_ADDRESS [[no_unique_address]]
#elif (defined(__GNUC__) || defined(__clang__)) && __cplusplus >= 201703L
#define EMH_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define EMH_NO_UNIQUE_ADDRESS
#endif
#endif

// Default probing sequence for the emilib containers. 0 = triangular step then a
// prime-like stride (the historical default), 1 = the linear-style step used by
// emihmap2's PSL_LINEAR==1 branch (and emihmap1/emihset3's #else), 2 = pure linear.
// Striped A/B runs on martin_bench (integer core cases, 2^21 scale) showed 1 is a
// net win over 0 for find/insert/erase/reinsert, so it is the default here.
// Override on the command line with -DEMH_PSL_LINEAR=0|2.
#ifndef EMH_PSL_LINEAR
#define EMH_PSL_LINEAR 1
#endif

// Count trailing zeros of a (non-zero) SIMD match mask.
inline static uint32_t CTZ(size_t n) {
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward(&index, static_cast<unsigned long>(n));
    return static_cast<uint32_t>(index);
#else
    return static_cast<uint32_t>(__builtin_ctzl(static_cast<unsigned long>(n)));
#endif
}

// Pre-broadcast fingerprint table: the fingerprint is already a byte in
// [EFILLED, EFILLED + 252], but turning it into a vector costs movd + unpack +
// shuffle on the lookup's critical path. One aligned 16-byte load replaces that.
// Only the 16-byte (SSE2) width is table-driven: 4 KB stays L1-resident, while the
// 32-byte variant would need 8 KB and lose more than it gains.
struct fp_broadcast_table {
    alignas(16) unsigned char bytes[256][16];
    constexpr fp_broadcast_table() : bytes{} {
        for (size_t b = 0; b < 256; ++b) {
            for (size_t i = 0; i < 16; ++i) {
                bytes[b][i] = static_cast<unsigned char>(b);
            }
        }
    }
};
inline constexpr fp_broadcast_table fp_broadcast{};

// Fingerprint folded from the top of the hash while the home bucket comes from the low
// bits, so the two are independent. A full-hash % 253 is correlated with the bucket,
// which makes keys that share a home group share a fingerprint more often and costs
// extra key comparisons. The xor fold keeps the byte spread for hashes that do not use
// their top bits (62-bit integer keys), without a multiply on the lookup path. Range
// stays [EFILLED, EFILLED + 252] and never hits ESENTINEL.
// filled_base receives the map's EFILLED.
template <typename HashType> static EMH_INLINE int8_t emilib_fp_from_hash(HashType key_hash, int filled_base) noexcept {
    uint64_t mixed = static_cast<uint64_t>(key_hash);
    if constexpr (sizeof(HashType) < 8)
        mixed *= UINT64_C(0x9E3779B97F4A7C15);
    const auto h8 = static_cast<int>(((mixed >> 56) ^ (mixed >> 32)) & 0xFF);
    return static_cast<int8_t>(h8 - 3 * (h8 >= 253) + filled_base);
}

// Fingerprint hashed from the raw key with one Fibonacci multiply: the golden-ratio
// product spreads dense key ranges over every bit, and the top byte folded into
// [0, 252] maps into [EFILLED, EFILLED + 252] independent of the home bucket and
// the hasher. The fold avoids a modulo: a % 253 compiles to a magic-multiply
// chain adding ~6-8 cycles to the lookup's critical path, while the top-byte fold
// is a single shift (253..255 fold onto 250..252, three values get double weight).
// Hashing the raw key also decouples the fingerprint from the hasher (a custom
// integer hasher no longer feeds both the bucket and the fingerprint). filled_base
// receives the map's EFILLED.
template <typename KeyType> static EMH_INLINE int8_t emilib_fp_from_key(KeyType key, int filled_base) noexcept {
    const auto mixed = static_cast<uint64_t>(key) * UINT64_C(0x9E3779B97F4A7C15);
    const auto h8 = static_cast<int>(mixed >> 56);
    return static_cast<int8_t>(h8 - 3 * (h8 >= 253) + filled_base);
}

// MSAN-clean hash entry point: strip key poisoning, run the hasher, return the raw
// hash value. Each container derives its home bucket and fingerprint from it.
template <typename Hasher, typename K>
static EMH_INLINE auto emilib_hash_of(const Hasher& hasher, const K& key) -> decltype(hasher(key)) {
    EMH_MSAN_UNPOISON(&key, sizeof(key));
    if constexpr (std::is_same<K, std::string>::value) {
        EMH_MSAN_UNPOISON(key.data(), key.size());
    }
    return hasher(key);
}
