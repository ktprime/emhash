// LICENSE:
// version 1.2.0
// https://github.com/ktprime/emhash/blob/master/include/emilib/emihmap4.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2026 Huang Yuanbing & bailuzhou AT 163.com
//
// Swiss-table style hash map based on Boost.Unordered FOA design.
// Key design choices ported from Boost:
//   - group15: 15 slots + 1 overflow byte per group
//   - High bits of hash for group positioning, low byte for in-group matching
//   - mulx_mix for non-avalanching hash functions
//   - Quadratic probing (triangular numbers)
//   - No tombstone: erase resets to available(0), overflow byte for early termination
//   - Single-block allocation: [elements][align][groups] for cache locality
//   - Compact iterator: (pc, p) with inline byte scanning
//   - Prefetch before key comparison (ARM: half-group, x86: first cache line)
//   - EMH_ASSUME hints for compiler optimization
//   - unchecked_emplace_with_rehash: insert before rehash to avoid double hash
//   - move_if_noexcept during rehash for exception safety
//   - erase_if using direct group traversal instead of iterator

#pragma once

#include "emhash/config.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <iterator>
#include <utility>
#include <cassert>
#include <type_traits>

#ifdef _WIN32
#include <intrin.h>
#elif defined(__x86_64__) || defined(__amd64__) || defined(__i386__) || defined(__i686__) || defined(_M_IX86) ||       \
    defined(_M_X64)
#include <x86intrin.h>
#elif defined(__ARM_ARCH__) || defined(__aarch64__) || defined(__arm__)
#include <sse2neon.h>
#endif

#undef EMH_LIKELY
#undef EMH_UNLIKELY

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

namespace emilib4 {

// ─── EBO helper for stateless Hash/Pred ────────────────────────────

namespace detail {
template <typename T, int Tag, bool = std::is_empty<T>::value && !std::is_final<T>::value>
struct ebo : private T {
    ebo() = default;
    template <typename U> explicit ebo(U&& v) : T(std::forward<U>(v)) {}
    T& get() noexcept { return *this; }
    const T& get() const noexcept { return *this; }
};

template <typename T, int Tag>
struct ebo<T, Tag, false> {
    ebo() = default;
    template <typename U> explicit ebo(U&& v) : val(std::forward<U>(v)) {}
    T& get() noexcept { return val; }
    const T& get() const noexcept { return val; }
    T val;
};
} // namespace detail

// ─── mulx hash mixing (from Boost) ───────────────────────────────────

#if defined(__SIZEOF_INT128__)
inline uint64_t mulx64(uint64_t x, uint64_t y) {
    __uint128_t r = (__uint128_t)x * y;
    return (uint64_t)r ^ (uint64_t)(r >> 64);
}
#elif defined(_MSC_VER) && defined(_M_X64)
inline uint64_t mulx64(uint64_t x, uint64_t y) {
    uint64_t hi;
    uint64_t lo = _umul128(x, y, &hi);
    return lo ^ hi;
}
#elif defined(_MSC_VER) && defined(_M_ARM64)
inline uint64_t mulx64(uint64_t x, uint64_t y) {
    return (x * y) ^ __umulh(x, y);
}
#else
inline uint64_t mulx64(uint64_t x, uint64_t y) {
    uint64_t x1 = (uint32_t)x, x2 = x >> 32;
    uint64_t y1 = (uint32_t)y, y2 = y >> 32;
    uint64_t r3 = x2 * y2;
    uint64_t r2a = x1 * y2;
    r3 += r2a >> 32;
    uint64_t r2b = x2 * y1;
    r3 += r2b >> 32;
    uint64_t r1 = x1 * y1;
    uint64_t r2 = (r1 >> 32) + (uint32_t)r2a + (uint32_t)r2b;
    r1 = (r2 << 32) + (uint32_t)r1;
    r3 += r2 >> 32;
    return r1 ^ r3;
}
#endif

// Detect 64-bit size_t
#if defined(SIZE_MAX)
#if ((((SIZE_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define EMILIB4_64BIT_ARCH
#endif
#elif defined(UINTPTR_MAX)
#if ((((UINTPTR_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define EMILIB4_64BIT_ARCH
#endif
#endif

inline size_t mulx_hash(size_t x) noexcept {
#ifdef EMILIB4_64BIT_ARCH
    return (size_t)mulx64((uint64_t)x, 0x9E3779B97F4A7C15ull);
#else
    uint64_t r = (uint64_t)x * 0xE817FB2Du;
    return (uint32_t)r ^ (uint32_t)(r >> 32);
#endif
}

// ─── hash_is_avalanching detection ───────────────────────────────────

template <typename Hash, typename = void> struct hash_is_avalanching : std::false_type {};

template <typename Hash>
struct hash_is_avalanching<Hash, typename std::enable_if<((void)sizeof(typename Hash::is_avalanching), true)>::type>
    : std::integral_constant<bool, !std::is_same<typename Hash::is_avalanching, void>::value> {};

// ─── group15 metadata ────────────────────────────────────────────────
//
// 16-byte metadata per group of N=15 slots:
//   [h0][h1]...[h14][overflow]
// hi = 0          → slot available
// hi = 1          → sentinel
// hi = [2..255]   → reduced hash value (7.99 bits of info)
// overflow byte: bit k set if any element with (hash%8)==k overflowed from this group

struct group15 {
    static constexpr int N = 15;
    static constexpr uint8_t available_ = 0;
    static constexpr uint8_t sentinel_ = 1;

    struct dummy_group_type {
        alignas(16) unsigned char storage[N + 1] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
    };

    alignas(16) uint8_t m[16];

    void initialize() {
#if defined(BOOST_UNORDERED_SSE2) || (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
        _mm_store_si128(reinterpret_cast<__m128i*>(m), _mm_setzero_si128());
#else
        memset(m, 0, 16);
#endif
    }

    void set(int pos, size_t hash) {
        assert(pos < N);
        m[pos] = reduced_hash(hash);
    }

    void set_sentinel() {
        m[N - 1] = sentinel_;
    }

    bool is_sentinel(int pos) const {
        assert(pos < N);
        return m[pos] == sentinel_;
    }

    void reset(int pos) {
        assert(pos < N);
        m[pos] = available_;
    }

    static void reset(uint8_t* pc) {
        *pc = available_;
    }

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    int match(size_t hash) const {
        return _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_load_si128(reinterpret_cast<const __m128i*>(m)),
                                                _mm_set1_epi32(match_word(hash)))) &
               0x7FFF;
    }

    int match_available() const {
        return _mm_movemask_epi8(
                   _mm_cmpeq_epi8(_mm_load_si128(reinterpret_cast<const __m128i*>(m)), _mm_setzero_si128())) &
               0x7FFF;
    }

    int match_occupied() const {
        return (~match_available()) & 0x7FFF;
    }
#else
    // Non-SIMD fallback: branchless byte comparisons for fast matching
    int match(size_t hash) const {
        auto rh = reduced_hash(hash);
        int mask = 0;
        for (int i = 0; i < N; i++)
            mask |= (m[i] == rh) << i;
        return mask;
    }

    int match_available() const {
        int mask = 0;
        for (int i = 0; i < N; i++)
            mask |= (m[i] == available_) << i;
        return mask;
    }

    int match_occupied() const {
        return (~match_available()) & 0x7FFF;
    }
#endif

    bool is_not_overflowed(size_t hash) const {
        static constexpr uint8_t shift[] = {1, 2, 4, 8, 16, 32, 64, 128};
        return !(m[N] & shift[hash % 8]);
    }

    void mark_overflow(size_t hash) {
        m[N] |= static_cast<uint8_t>(1 << (hash % 8));
    }

    static bool maybe_caused_overflow(uint8_t* pc) {
        auto pos = reinterpret_cast<uintptr_t>(pc) % sizeof(group15);
        auto* pg = reinterpret_cast<group15*>(reinterpret_cast<uintptr_t>(pc) - pos);
        return !pg->is_not_overflowed(*pc);
    }

    bool is_occupied(int pos) const {
        assert(pos < N);
        return m[pos] != available_;
    }

    static bool is_occupied(uint8_t* pc) noexcept {
        return *pc != available_;
    }

    static bool is_sentinel(uint8_t* pc) noexcept {
        return *pc == sentinel_;
    }

    // reduced_hash: map hash byte to [2..255], avoiding 0 (available) and 1 (sentinel)
    static uint8_t reduced_hash(size_t hash) {
        static constexpr uint8_t table[] = {
            8,   9,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
            20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,
            40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
            60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
            80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,
            100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
            120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
            140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
            160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
            180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,
            200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
            220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
            240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
        };
        return table[(unsigned char)hash];
    }

    // match_word: pre-multiplied 32-bit value for SIMD comparison
    static uint32_t match_word(size_t hash) {
        static constexpr uint32_t word[] = {
            0x08080808u, 0x09090909u, 0x02020202u, 0x03030303u, 0x04040404u, 0x05050505u, 0x06060606u, 0x07070707u,
            0x08080808u, 0x09090909u, 0x0A0A0A0Au, 0x0B0B0B0Bu, 0x0C0C0C0Cu, 0x0D0D0D0Du, 0x0E0E0E0Eu, 0x0F0F0F0Fu,
            0x10101010u, 0x11111111u, 0x12121212u, 0x13131313u, 0x14141414u, 0x15151515u, 0x16161616u, 0x17171717u,
            0x18181818u, 0x19191919u, 0x1A1A1A1Au, 0x1B1B1B1Bu, 0x1C1C1C1Cu, 0x1D1D1D1Du, 0x1E1E1E1Eu, 0x1F1F1F1Fu,
            0x20202020u, 0x21212121u, 0x22222222u, 0x23232323u, 0x24242424u, 0x25252525u, 0x26262626u, 0x27272727u,
            0x28282828u, 0x29292929u, 0x2A2A2A2Au, 0x2B2B2B2Bu, 0x2C2C2C2Cu, 0x2D2D2D2Du, 0x2E2E2E2Eu, 0x2F2F2F2Fu,
            0x30303030u, 0x31313131u, 0x32323232u, 0x33333333u, 0x34343434u, 0x35353535u, 0x36363636u, 0x37373737u,
            0x38383838u, 0x39393939u, 0x3A3A3A3Au, 0x3B3B3B3Bu, 0x3C3C3C3Cu, 0x3D3D3D3Du, 0x3E3E3E3Eu, 0x3F3F3F3Fu,
            0x40404040u, 0x41414141u, 0x42424242u, 0x43434343u, 0x44444444u, 0x45454545u, 0x46464646u, 0x47474747u,
            0x48484848u, 0x49494949u, 0x4A4A4A4Au, 0x4B4B4B4Bu, 0x4C4C4C4Cu, 0x4D4D4D4Du, 0x4E4E4E4Eu, 0x4F4F4F4Fu,
            0x50505050u, 0x51515151u, 0x52525252u, 0x53535353u, 0x54545454u, 0x55555555u, 0x56565656u, 0x57575757u,
            0x58585858u, 0x59595959u, 0x5A5A5A5Au, 0x5B5B5B5Bu, 0x5C5C5C5Cu, 0x5D5D5D5Du, 0x5E5E5E5Eu, 0x5F5F5F5Fu,
            0x60606060u, 0x61616161u, 0x62626262u, 0x63636363u, 0x64646464u, 0x65656565u, 0x66666666u, 0x67676767u,
            0x68686868u, 0x69696969u, 0x6A6A6A6Au, 0x6B6B6B6Bu, 0x6C6C6C6Cu, 0x6D6D6D6Du, 0x6E6E6E6Eu, 0x6F6F6F6Fu,
            0x70707070u, 0x71717171u, 0x72727272u, 0x73737373u, 0x74747474u, 0x75757575u, 0x76767676u, 0x77777777u,
            0x78787878u, 0x79797979u, 0x7A7A7A7Au, 0x7B7B7B7Bu, 0x7C7C7C7Cu, 0x7D7D7D7Du, 0x7E7E7E7Eu, 0x7F7F7F7Fu,
            0x80808080u, 0x81818181u, 0x82828282u, 0x83838383u, 0x84848484u, 0x85858585u, 0x86868686u, 0x87878787u,
            0x88888888u, 0x89898989u, 0x8A8A8A8Au, 0x8B8B8B8Bu, 0x8C8C8C8Cu, 0x8D8D8D8Du, 0x8E8E8E8Eu, 0x8F8F8F8Fu,
            0x90909090u, 0x91919191u, 0x92929292u, 0x93939393u, 0x94949494u, 0x95959595u, 0x96969696u, 0x97979797u,
            0x98989898u, 0x99999999u, 0x9A9A9A9Au, 0x9B9B9B9Bu, 0x9C9C9C9Cu, 0x9D9D9D9Du, 0x9E9E9E9Eu, 0x9F9F9F9Fu,
            0xA0A0A0A0u, 0xA1A1A1A1u, 0xA2A2A2A2u, 0xA3A3A3A3u, 0xA4A4A4A4u, 0xA5A5A5A5u, 0xA6A6A6A6u, 0xA7A7A7A7u,
            0xA8A8A8A8u, 0xA9A9A9A9u, 0xAAAAAAAAu, 0xABABABABu, 0xACACACACu, 0xADADADADu, 0xAEAEAEAEu, 0xAFAFAFAFu,
            0xB0B0B0B0u, 0xB1B1B1B1u, 0xB2B2B2B2u, 0xB3B3B3B3u, 0xB4B4B4B4u, 0xB5B5B5B5u, 0xB6B6B6B6u, 0xB7B7B7B7u,
            0xB8B8B8B8u, 0xB9B9B9B9u, 0xBABABABAu, 0xBBBBBBBBu, 0xBCBCBCBCu, 0xBDBDBDBDu, 0xBEBEBEBEu, 0xBFBFBFBFu,
            0xC0C0C0C0u, 0xC1C1C1C1u, 0xC2C2C2C2u, 0xC3C3C3C3u, 0xC4C4C4C4u, 0xC5C5C5C5u, 0xC6C6C6C6u, 0xC7C7C7C7u,
            0xC8C8C8C8u, 0xC9C9C9C9u, 0xCACACACAu, 0xCBCBCBCBu, 0xCCCCCCCCu, 0xCDCDCDCDu, 0xCECECECEu, 0xCFCFCFCFu,
            0xD0D0D0D0u, 0xD1D1D1D1u, 0xD2D2D2D2u, 0xD3D3D3D3u, 0xD4D4D4D4u, 0xD5D5D5D5u, 0xD6D6D6D6u, 0xD7D7D7D7u,
            0xD8D8D8D8u, 0xD9D9D9D9u, 0xDADADADAu, 0xDBDBDBDBu, 0xDCDCDCDCu, 0xDDDDDDDDu, 0xDEDEDEDEu, 0xDFDFDFDFu,
            0xE0E0E0E0u, 0xE1E1E1E1u, 0xE2E2E2E2u, 0xE3E3E3E3u, 0xE4E4E4E4u, 0xE5E5E5E5u, 0xE6E6E6E6u, 0xE7E7E7E7u,
            0xE8E8E8E8u, 0xE9E9E9E9u, 0xEAEAEAEAu, 0xEBEBEBEBu, 0xECECECECu, 0xEDEDEDEDu, 0xEEEEEEEEu, 0xEFEFEFEFu,
            0xF0F0F0F0u, 0xF1F1F1F1u, 0xF2F2F2F2u, 0xF3F3F3F3u, 0xF4F4F4F4u, 0xF5F5F5F5u, 0xF6F6F6F6u, 0xF7F7F7F7u,
            0xF8F8F8F8u, 0xF9F9F9F9u, 0xFAFAFAFAu, 0xFBFBFBFBu, 0xFCFCFCFCu, 0xFDFDFDFDu, 0xFEFEFEFEu, 0xFFFFFFFFu,
        };
        return word[(unsigned char)hash];
    }
};

// ─── power-of-2 size policy ──────────────────────────────────────────
// Uses HIGH bits of hash for group positioning, LOW byte for h2 matching.

struct pow2_size_policy {
    static inline size_t size_index(size_t n) {
        if (n <= 2)
            return sizeof(size_t) * 8 - 1;
        int bits = 0;
#if defined(_MSC_VER)
        unsigned long msb;
        _BitScanReverse64(&msb, (unsigned __int64)(n - 1));
        bits = (int)msb + 1;
#elif defined(__GNUC__) || defined(__clang__)
        bits = 64 - __builtin_clzll(n - 1);
#else
        auto x = n - 1;
        while (x >>= 1)
            ++bits;
#endif
        return sizeof(size_t) * 8 - bits;
    }

    static inline size_t size(size_t size_index) {
        return size_t(1) << (sizeof(size_t) * 8 - size_index);
    }

    static constexpr size_t min_size() {
        return 2;
    }

    static inline size_t position(size_t hash, size_t size_index) {
        return size_index < sizeof(size_t) * 8 ? (hash >> size_index) : 0;
    }
};

// ─── quadratic prober ─────────────────────────────────────────────────
// Triangular number probing: step 1, 3, 6, 10, 15, 21...

struct quadratic_prober {
    quadratic_prober(size_t pos) : pos_(pos) {}

    size_t get() const { return pos_; }
    size_t length() const { return step_ + 1; }

    bool next(size_t mask) {
        step_ += 1;
        pos_ = (pos_ + step_) & mask;
        return step_ <= mask;
    }

private:
    size_t pos_;
    size_t step_ = 0;
};

// ─── unchecked CTZ ────────────────────────────────────────────────────

inline unsigned int unchecked_ctz(int x) {
#if defined(_MSC_VER)
    unsigned long r;
    _BitScanForward(&r, (unsigned long)x);
    return (unsigned int)r;
#elif defined(__GNUC__) || defined(__clang__)
    return (unsigned int)__builtin_ctz((unsigned int)x);
#else
    unsigned int r = 0;
    while (!((x >> r) & 1))
        ++r;
    return r;
#endif
}

// ─── prefetch ─────────────────────────────────────────────────────────

static inline void prefetch_element(const void* p) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(p);
#endif
}

// ARM architectures benefit from prefetching the first half of element slots
// in a group, whereas Intel prefers just the first cache line.
#if defined(__ARM_ARCH__) || defined(__aarch64__) || defined(__arm__)
#define EMH_PREFETCH_ELEMENTS(p, group_n)                                                                              \
    do {                                                                                                               \
        auto _ep = (p);                                                                                                \
        constexpr int _cl = 64;                                                                                        \
        const char* _p0 = reinterpret_cast<const char*>(_ep);                                                          \
        const char* _p1 = _p0 + sizeof(*_ep) * (group_n) / 2;                                                          \
        for (; _p0 < _p1; _p0 += _cl)                                                                                  \
            prefetch_element(_p0);                                                                                     \
    } while (0)
#else
#define EMH_PREFETCH_ELEMENTS(p, group_n) prefetch_element(p)
#endif

// ─── HashMap ──────────────────────────────────────────────────────────

#ifndef EMH_DEFAULT_LOAD_FACTOR
constexpr static float EMH_DEFAULT_LOAD_FACTOR = 0.875f;
#endif
constexpr static float EMH_MIN_LOAD_FACTOR = 0.25f;
constexpr static float EMH_MAX_LOAD_FACTOR = 0.999f;

template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap {
private:
    using htype = HashMap<KeyT, ValueT, HashT, EqT>;
    using PairT = std::pair<const KeyT, ValueT>;
    using group_type = group15;

public:
    using value_type = PairT;
    using reference = PairT&;
    using const_reference = const PairT&;
    using mapped_type = ValueT;
    using val_type = ValueT;
    using key_type = KeyT;
    using hasher = HashT;
    using key_equal = EqT;
    using size_type = size_t;

    // ─── iterator (Boost-style compact: pc + p) ──────────────────────

    class const_iterator;
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = PairT;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : _pc(nullptr), _p(nullptr) {}

        reference operator*() const { return *_p; }
        pointer operator->() const { return _p; }

        iterator& operator++() {
            increment();
            return *this;
        }
        iterator operator++(int) {
            iterator old = *this;
            increment();
            return old;
        }

        bool operator==(const iterator& rhs) const { return _p == rhs._p; }
        bool operator!=(const iterator& rhs) const { return _p != rhs._p; }
        bool operator==(const const_iterator& rhs) const { return _p == rhs._p; }
        bool operator!=(const const_iterator& rhs) const { return _p != rhs._p; }

    private:
        friend class HashMap;
        friend class const_iterator;

        iterator(uint8_t* pc, PairT* p) : _pc(pc), _p(p) {}

        void increment() noexcept {
            // Phase 1: byte-by-byte scan within current group
            for (;;) {
                ++_p;
                // Last slot in group? (position N-1 = 14)
                if (reinterpret_cast<uintptr_t>(_pc) % sizeof(group15) == (size_t)(N - 1)) {
                    // Skip overflow byte, advance to next group's first slot
                    _pc += sizeof(group15) - (N - 1);
                    break;
                }
                ++_pc;
                if (*_pc == group15::available_)
                    continue;
                if (EMH_UNLIKELY(*_pc == group15::sentinel_)) {
                    _p = nullptr;
                    return;
                }
                return; // found occupied slot
            }

            // Phase 2: cross-group scan using match_occupied
            for (;;) {
                auto* pg = reinterpret_cast<group15*>(reinterpret_cast<uintptr_t>(_pc) & ~(sizeof(group15) - 1));
                int mask = pg->match_occupied();
                if (mask != 0) {
                    auto n = unchecked_ctz(mask);
                    if (EMH_UNLIKELY(pg->is_sentinel(n))) {
                        _p = nullptr;
                    } else {
                        _pc += n;
                        _p += n;
                    }
                    return;
                }
                _pc += sizeof(group15);
                _p += N;
            }
        }

        uint8_t* _pc;
        PairT* _p;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const PairT;
        using pointer = value_type*;
        using reference = value_type&;

        const_iterator() : _pc(nullptr), _p(nullptr) {}
        const_iterator(const iterator& it) : _pc(it._pc), _p(it._p) {}

        reference operator*() const { return *_p; }
        pointer operator->() const { return _p; }

        const_iterator& operator++() {
            increment();
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator old = *this;
            increment();
            return old;
        }

        bool operator==(const const_iterator& rhs) const { return _p == rhs._p; }
        bool operator!=(const const_iterator& rhs) const { return _p != rhs._p; }
        bool operator==(const iterator& rhs) const { return _p == rhs._p; }
        bool operator!=(const iterator& rhs) const { return _p != rhs._p; }

    private:
        friend class HashMap;
        friend class iterator;

        const_iterator(uint8_t* pc, const PairT* p) : _pc(pc), _p(p) {}

        void increment() noexcept {
            for (;;) {
                ++_p;
                if (reinterpret_cast<uintptr_t>(_pc) % sizeof(group15) == (size_t)(N - 1)) {
                    _pc += sizeof(group15) - (N - 1);
                    break;
                }
                ++_pc;
                if (*_pc == group15::available_)
                    continue;
                if (EMH_UNLIKELY(*_pc == group15::sentinel_)) {
                    _p = nullptr;
                    return;
                }
                return;
            }
            for (;;) {
                auto* pg = reinterpret_cast<group15*>(reinterpret_cast<uintptr_t>(_pc) & ~(sizeof(group15) - 1));
                int mask = pg->match_occupied();
                if (mask != 0) {
                    auto n = unchecked_ctz(mask);
                    if (EMH_UNLIKELY(pg->is_sentinel(n))) {
                        _p = nullptr;
                    } else {
                        _pc += n;
                        _p += n;
                    }
                    return;
                }
                _pc += sizeof(group15);
                _p += N;
            }
        }

        uint8_t* _pc;
        const PairT* _p;
    };

    // ─── constructors ─────────────────────────────────────────────────

    explicit HashMap(size_t n = 0, float lf = EMH_DEFAULT_LOAD_FACTOR) {
        (void)lf;
        rehash_impl(n);
    }

    HashMap(const HashMap& other) : HashMap() { clone(other); }

    HashMap(HashMap&& other) noexcept { swap(other); }

    HashMap(std::initializer_list<value_type> il) : HashMap() {
        rehash_impl(il.size());
        for (auto& v : il)
            insert(v);
    }

    template <class InputIt> HashMap(InputIt first, InputIt last) : HashMap() {
        rehash_impl(static_cast<size_t>(std::distance(first, last)));
        for (; first != last; ++first)
            insert(*first);
    }

    HashMap& operator=(const HashMap& other) {
        if (this != &other)
            clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap() noexcept {
        clear_data();
        if (_pairs)
            free(_pairs); // single free; _groups is derived from _pairs
    }

    // ─── capacity ─────────────────────────────────────────────────────

    size_t size() const noexcept { return _num_filled; }
    bool empty() const noexcept { return _num_filled == 0; }
    size_t bucket_count() const noexcept { return _capacity(); }
    float load_factor() const noexcept { return _capacity() ? (float)_num_filled / (float)_capacity() : 0.0f; }
    constexpr float max_load_factor() const { return mlf; }
    void max_load_factor(float) {} // swiss table has fixed load factor; no-op for API compatibility

    // ─── iterators ────────────────────────────────────────────────────

    EMH_INLINE iterator begin() noexcept {
        // Boost pattern: construct iterator at group[0] slot 0, increment if not occupied
        auto it = iterator(&_groups[0].m[0], _pairs);
        if (_groups[0].m[0] == group15::available_)
            ++it;
        return it;
    }
    EMH_INLINE const_iterator cbegin() const noexcept {
        auto it = const_iterator(const_cast<uint8_t*>(&_groups[0].m[0]), _pairs);
        if (_groups[0].m[0] == group15::available_)
            ++it;
        return it;
    }
    const_iterator begin() const noexcept { return cbegin(); }
    iterator end() noexcept { return iterator(nullptr, nullptr); }
    const_iterator cend() const noexcept { return const_iterator(nullptr, nullptr); }
    const_iterator end() const noexcept { return cend(); }

    // ─── locator (avoid div/mod on find return) ──────────────────────

    struct locator {
        group15* pg = nullptr;
        unsigned n = 0;
        PairT* p = nullptr;
        explicit operator bool() const noexcept { return p != nullptr; }
    };

    // ─── lookup ───────────────────────────────────────────────────────

    template <typename K> iterator find(const K& key) noexcept {
        auto hash = hash_for(key);
        auto pos0 = position_for(hash);
        // Inline find loop to avoid template function call overhead
        auto* pairs = _pairs;
        auto* groups = _groups;
        quadratic_prober pb(pos0);
        do {
            auto pos = pb.get();
            auto* pg = groups + pos;
            auto mask = pg->match(hash);
            if (mask) {
                auto p = pairs + pos * N;
                EMH_ASSUME(p != nullptr);
                EMH_PREFETCH_ELEMENTS(p, N);
                do {
                    auto n = unchecked_ctz(mask);
                    if (EMH_LIKELY(_eq()(p[n].first, key)))
                        return iterator(&pg->m[n], p + n);
                    mask &= mask - 1;
                } while (mask);
            }
            if (EMH_LIKELY(pg->is_not_overflowed(hash)))
                return end();
        } while (EMH_LIKELY(pb.next(_groups_size_mask)));
        return end();
    }
    template <typename K> const_iterator find(const K& key) const noexcept {
        auto hash = hash_for(key);
        auto pos0 = position_for(hash);
        auto* pairs = _pairs;
        auto* groups = _groups;
        quadratic_prober pb(pos0);
        do {
            auto pos = pb.get();
            auto* pg = groups + pos;
            auto mask = pg->match(hash);
            if (mask) {
                auto p = pairs + pos * N;
                EMH_ASSUME(p != nullptr);
                EMH_PREFETCH_ELEMENTS(p, N);
                do {
                    auto n = unchecked_ctz(mask);
                    if (EMH_LIKELY(_eq()(p[n].first, key)))
                        return const_iterator(&pg->m[n], p + n);
                    mask &= mask - 1;
                } while (mask);
            }
            if (EMH_LIKELY(pg->is_not_overflowed(hash)))
                return cend();
        } while (EMH_LIKELY(pb.next(_groups_size_mask)));
        return cend();
    }
    template <typename K = KeyT> bool contains(const K& key) const noexcept {
        auto hash = hash_for(key);
        auto pos0 = position_for(hash);
        auto* groups = _groups;
        quadratic_prober pb(pos0);
        do {
            auto pos = pb.get();
            auto* pg = groups + pos;
            auto mask = pg->match(hash);
            if (mask) {
                auto p = _pairs + pos * N;
                do {
                    auto n = unchecked_ctz(mask);
                    if (_eq()(p[n].first, key))
                        return true;
                    mask &= mask - 1;
                } while (mask);
            }
            if (EMH_LIKELY(pg->is_not_overflowed(hash)))
                return false;
        } while (EMH_LIKELY(pb.next(_groups_size_mask)));
        return false;
    }
    template <typename K = KeyT> size_t count(const K& key) const noexcept {
        return contains(key);
    }

    template <typename K = KeyT> ValueT& at(const K& key) {
        auto loc = find_locator(key);
        if (!loc)
            throw std::out_of_range("emilib4::HashMap::at");
        return loc.p->second;
    }
    template <typename K = KeyT> const ValueT& at(const K& key) const {
        auto loc = find_locator(key);
        if (!loc)
            throw std::out_of_range("emilib4::HashMap::at");
        return loc.p->second;
    }

    template <typename K = KeyT> ValueT* try_get(const K& key) noexcept {
        auto loc = find_locator(key);
        return loc ? &loc.p->second : nullptr;
    }

    // ─── insert ───────────────────────────────────────────────────────

    EMH_INLINE std::pair<iterator, bool> insert(const value_type& value) { return do_insert(value.first, value.second); }
    EMH_INLINE std::pair<iterator, bool> insert(value_type&& value) { return do_insert(std::move(value)); }

    template <typename K, typename V> std::pair<iterator, bool> do_insert(K&& key, V&& val) {
        auto hash = hash_for(key);
        auto pos0 = position_for(hash);

        auto loc = find_locator_at(key, pos0, hash);
        if (loc)
            return {iterator(&loc.pg->m[loc.n], loc.p), false};

        if (EMH_UNLIKELY(_num_filled >= _max_load)) {
            loc = unchecked_emplace_with_rehash(hash, std::forward<K>(key), std::forward<V>(val));
            return {iterator(&loc.pg->m[loc.n], loc.p), true};
        } else {
            loc = find_empty_slot_and_insert(pos0, hash);
            new (loc.p) PairT(std::forward<K>(key), std::forward<V>(val));
            _num_filled++;
            return {iterator(&loc.pg->m[loc.n], loc.p), true};
        }
    }

    EMH_INLINE std::pair<iterator, bool> do_insert(value_type&& value) {
        return do_insert(std::move(const_cast<KeyT&>(value.first)), std::move(value.second));
    }

    EMH_INLINE std::pair<iterator, bool> do_insert(const value_type& value) { return do_insert(value.first, value.second); }

    template <class... Args> std::pair<iterator, bool> emplace(Args&&... args) {
        return do_insert(std::forward<Args>(args)...);
    }

    template <class... Args> std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) {
        return do_insert(key, ValueT(std::forward<Args>(args)...));
    }
    template <class... Args> std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) {
        return do_insert(std::move(key), ValueT(std::forward<Args>(args)...));
    }

    template <typename M> std::pair<iterator, bool> insert_or_assign(const KeyT& key, M&& m) {
        auto loc = find_locator(key);
        if (loc) {
            loc.p->second = std::forward<M>(m);
            return {iterator(&loc.pg->m[loc.n], loc.p), false};
        }
        return do_insert(key, ValueT(std::forward<M>(m)));
    }
    template <typename M> std::pair<iterator, bool> insert_or_assign(KeyT&& key, M&& m) {
        auto loc = find_locator(key);
        if (loc) {
            loc.p->second = std::forward<M>(m);
            return {iterator(&loc.pg->m[loc.n], loc.p), false};
        }
        return do_insert(std::move(key), ValueT(std::forward<M>(m)));
    }

    void insert(std::initializer_list<value_type> ilist) {
        rehash_impl(_num_filled + ilist.size());
        for (auto& v : ilist)
            do_insert(v.first, v.second);
    }

    template <typename Iter> void insert(Iter beginc, Iter endc) {
        rehash_impl(_num_filled + static_cast<size_t>(endc - beginc));
        for (; beginc != endc; ++beginc)
            do_insert(beginc->first, beginc->second);
    }

    EMH_INLINE ValueT& operator[](const KeyT& key) {
        auto hash = hash_for(key);
        auto pos0 = position_for(hash);
        // Inline find loop
        {
            auto* pairs = _pairs;
            auto* groups = _groups;
            quadratic_prober pb(pos0);
            do {
                auto pos = pb.get();
                auto* pg = groups + pos;
                auto mask = pg->match(hash);
                if (mask) {
                    auto p = pairs + pos * N;
                    EMH_ASSUME(p != nullptr);
                    EMH_PREFETCH_ELEMENTS(p, N);
                    do {
                        auto n = unchecked_ctz(mask);
                        if (EMH_LIKELY(_eq()(p[n].first, key)))
                            return p[n].second;
                        mask &= mask - 1;
                    } while (mask);
                }
                if (EMH_LIKELY(pg->is_not_overflowed(hash)))
                    break;
            } while (EMH_LIKELY(pb.next(_groups_size_mask)));
        }
        // Key not found - insert
        if (EMH_UNLIKELY(_num_filled >= _max_load)) {
            auto loc = unchecked_emplace_with_rehash(hash, key, ValueT());
            return loc.p->second;
        } else {
            auto loc = find_empty_slot_and_insert(pos0, hash);
            new (loc.p) PairT(key, ValueT());
            _num_filled++;
            return loc.p->second;
        }
    }

    EMH_INLINE ValueT& operator[](KeyT&& key) {
        auto hash = hash_for(key);
        auto pos0 = position_for(hash);
        {
            auto* pairs = _pairs;
            auto* groups = _groups;
            quadratic_prober pb(pos0);
            do {
                auto pos = pb.get();
                auto* pg = groups + pos;
                auto mask = pg->match(hash);
                if (mask) {
                    auto p = pairs + pos * N;
                    EMH_ASSUME(p != nullptr);
                    EMH_PREFETCH_ELEMENTS(p, N);
                    do {
                        auto n = unchecked_ctz(mask);
                        if (EMH_LIKELY(_eq()(p[n].first, key)))
                            return p[n].second;
                        mask &= mask - 1;
                    } while (mask);
                }
                if (EMH_LIKELY(pg->is_not_overflowed(hash)))
                    break;
            } while (EMH_LIKELY(pb.next(_groups_size_mask)));
        }
        if (EMH_UNLIKELY(_num_filled >= _max_load)) {
            auto loc = unchecked_emplace_with_rehash(hash, std::move(key), ValueT());
            return loc.p->second;
        } else {
            auto loc = find_empty_slot_and_insert(pos0, hash);
            new (loc.p) PairT(std::move(key), ValueT());
            _num_filled++;
            return loc.p->second;
        }
    }

    // ─── erase ────────────────────────────────────────────────────────

    EMH_INLINE size_t erase(const KeyT& key) noexcept {
        auto it = find(key);
        if (it != end()) {
            erase_at(it._pc, it._p);
            return 1;
        }
        return 0;
    }

    void erase(const iterator& it) noexcept { erase_at(it._pc, it._p); }
    void erase(const const_iterator& it) noexcept { erase_at(const_cast<uint8_t*>(it._pc), const_cast<PairT*>(it._p)); }

    template <typename Pred> size_t erase_if(Pred pred) noexcept {
        auto old = _num_filled;
        auto ng = _num_groups();
        for (size_t gn = 0; gn < ng; gn++) {
            auto mask = _groups[gn].match_occupied();
            if (gn == ng - 1)
                mask &= ~(1 << (N - 1));
            while (mask) {
                auto sn = unchecked_ctz(mask);
                auto slot = gn * N + sn;
                if (pred(_pairs[slot])) {
                    erase_bucket(slot);
                }
                mask &= mask - 1;
            }
        }
        return old - _num_filled;
    }

    // ─── misc ─────────────────────────────────────────────────────────

    void swap(HashMap& other) noexcept {
        std::swap(_hasher_base, other._hasher_base);
        std::swap(_eq_base, other._eq_base);
        std::swap(_groups, other._groups);
        std::swap(_pairs, other._pairs);
        std::swap(_num_filled, other._num_filled);
        std::swap(_max_load, other._max_load);
        std::swap(_groups_size_index, other._groups_size_index);
        std::swap(_groups_size_mask, other._groups_size_mask);
    }

    void clear() noexcept {
        if (_num_filled == 0)
            return;
        auto ng = _num_groups();
        clear_data();
        for (size_t i = 0; i < ng; i++)
            _groups[i].initialize();
        _groups[ng - 1].set_sentinel();
        _num_filled = 0;
        _max_load = initial_max_load();
    }

    void reserve(size_t n) { rehash_impl(n); }
    void shrink_to_fit() { rehash_impl(_num_filled); }
    void rehash(size_t n) { rehash_impl(n); }

    template <typename Con> bool operator==(const Con& rhs) const noexcept {
        if (size() != rhs.size())
            return false;
        for (auto it = begin(); it != end(); ++it) {
            auto oi = rhs.find(it->first);
            if (oi == rhs.end() || it->second != oi->second)
                return false;
        }
        return true;
    }
    template <typename Con> bool operator!=(const Con& rhs) const noexcept { return !(*this == rhs); }

    void merge(HashMap& rhs) noexcept {
        if (this == &rhs)
            return;
        auto rit = rhs.begin();
        while (rit != rhs.end()) {
            auto next = std::next(rit);
            auto loc = find_locator(rit->first);
            if (!loc) {
                do_insert(rit->first, std::move(rit->second));
                rhs.erase(rit);
            }
            rit = next;
        }
    }

    template <typename K, typename V> size_t insert_unique(K&& key, V&& val) {
        do_insert(std::forward<K>(key), std::forward<V>(val));
        return _num_filled; // approximate return
    }

private:
    // ─── internal constants ───────────────────────────────────────────

    static constexpr int N = group15::N;
    static constexpr float mlf = 0.875f;

    // ─── derived accessors (eliminate redundant _num_groups, _capacity) ──

    HashT& _hasher() noexcept { return _hasher_base.get(); }
    const HashT& _hasher() const noexcept { return _hasher_base.get(); }
    EqT& _eq() noexcept { return _eq_base.get(); }
    const EqT& _eq() const noexcept { return _eq_base.get(); }

    size_t _num_groups() const noexcept { return _pairs ? _groups_size_mask + 1 : 0; }
    size_t _capacity() const noexcept { return _pairs ? (_groups_size_mask + 1) * N - 1 : 0; }

    // ─── hash computation ─────────────────────────────────────────────

    struct no_mix {
        static inline size_t mix(size_t h) noexcept { return h; }
    };

    struct mulx_mix {
        static inline size_t mix(size_t h) noexcept { return mulx_hash(h); }
    };

    using mix_policy = typename std::conditional<hash_is_avalanching<HashT>::value, no_mix, mulx_mix>::type;

    template <typename K> size_t hash_for(const K& key) const {
        EMH_MSAN_UNPOISON(&key, sizeof(key));
        if constexpr (std::is_same<K, std::string>::value) {
            EMH_MSAN_UNPOISON(key.data(), key.size());
        }
        return mix_policy::mix(_hasher()(key));
    }

    EMH_INLINE size_t position_for(size_t hash) const { return pow2_size_policy::position(hash, _groups_size_index); }

    size_t initial_max_load() const {
        auto cap = _capacity();
        if (cap <= 2 * N - 1)
            return cap;
        return (size_t)(mlf * (float)cap);
    }

    // ─── iterator helpers ─────────────────────────────────────────────

    iterator iterator_from(size_t bucket) noexcept {
        auto gn = bucket / N, sn = bucket % N;
        return iterator(&_groups[gn].m[sn], _pairs + bucket);
    }

    const_iterator const_iterator_from(size_t bucket) const noexcept {
        auto gn = bucket / N, sn = bucket % N;
        return const_iterator(&_groups[gn].m[sn], _pairs + bucket);
    }

    // ─── find ─────────────────────────────────────────────────────────

    template <typename K> size_t find_filled_bucket(const K& key) const noexcept {
        auto loc = find_locator(key);
        return loc ? static_cast<size_t>(loc.p - _pairs) : _capacity();
    }

    template <typename K> locator find_locator(const K& key) const noexcept {
        auto hash = hash_for(key);
        return find_locator_at(key, position_for(hash), hash);
    }

    template <typename K>
    locator find_locator_at(const K& key, size_t pos0, size_t hash) const noexcept {
        auto* pairs = _pairs;
        auto* groups = _groups;
        quadratic_prober pb(pos0);
        do {
            auto pos = pb.get();
            auto* pg = groups + pos;
            auto mask = pg->match(hash);
            if (mask) {
                auto p = pairs + pos * N;
                EMH_ASSUME(p != nullptr);
                EMH_PREFETCH_ELEMENTS(p, N);
                do {
                    auto n = unchecked_ctz(mask);
                    if (EMH_LIKELY(_eq()(p[n].first, key)))
                        return {pg, static_cast<unsigned>(n), p + n};
                    mask &= mask - 1;
                } while (mask);
            }
            if (EMH_LIKELY(pg->is_not_overflowed(hash)))
                return {};
        } while (EMH_LIKELY(pb.next(_groups_size_mask)));
        return {};
    }

    // ─── insert (find empty slot) ─────────────────────────────────────

    EMH_INLINE locator find_empty_slot_and_insert(size_t pos0, size_t hash) {
        assert(_pairs != nullptr);
        for (quadratic_prober pb(pos0);; pb.next(_groups_size_mask)) {
            auto pos = pb.get();
            auto* pg = _groups + pos;
            auto mask = pg->match_available();
            if (EMH_LIKELY(mask != 0)) {
                auto n = unchecked_ctz(mask);
                pg->set(n, hash);
                auto p = _pairs + pos * N + n;
                return {pg, static_cast<unsigned>(n), p};
            }
            pg->mark_overflow(hash);
        }
    }

    // ─── erase ────────────────────────────────────────────────────────

    void erase_bucket(size_t bucket) noexcept {
        erase_at(&_groups[bucket / N].m[bucket % N], _pairs + bucket);
    }

    // Fast erase with both pc and p known (no redundant pointer arithmetic)
    EMH_INLINE void erase_at(uint8_t* pc, PairT* p) noexcept {
        if (need_explicit_dtor())
            p->~PairT();

        // recover_slot: anti-drift (Boost idiom)
        _max_load -= group15::maybe_caused_overflow(pc);
        group15::reset(pc);
        _num_filled--;
    }

    // ─── iteration helper ─────────────────────────────────────────────

    size_t find_next_filled(size_t start) const noexcept {
        auto ng = _num_groups();
        auto gn = start / N;
        auto sn = start % N;
        for (; gn < ng; gn++) {
            auto mask = _groups[gn].match_occupied();
            if (sn > 0) {
                mask &= ~((1 << sn) - 1);
                sn = 0;
            }
            if (gn == ng - 1)
                mask &= ~(1 << (N - 1));
            if (mask)
                return gn * N + unchecked_ctz(mask);
        }
        return _capacity();
    }

    // ─── memory management ────────────────────────────────────────────

    static constexpr bool need_explicit_dtor() {
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
    }

    static constexpr bool is_trivially_copyable() {
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
    }

    // Dummy groups for empty containers: avoids _groups==nullptr checks in hot paths
    static group15* dummy_groups() {
        static constexpr typename group_type::dummy_group_type
            storage[2] = {typename group_type::dummy_group_type(), typename group_type::dummy_group_type()};
        return reinterpret_cast<group15*>(const_cast<typename group_type::dummy_group_type*>(storage));
    }

    // Single-block buffer size: [elements][alignment padding][groups]
    static size_t buffer_size(size_t num_groups) {
        auto elements_bytes = sizeof(PairT) * (num_groups * N - 1);
        auto groups_bytes = sizeof(group15) * num_groups;
        auto total = elements_bytes + (sizeof(group15) - 1) + groups_bytes;
        // Round up to alignment of PairT for safe array access
        return ((total + alignof(PairT) - 1) / alignof(PairT)) * alignof(PairT);
    }

    void clear_data() noexcept {
        if (!need_explicit_dtor() || _num_filled == 0)
            return;
        auto ng = _num_groups();
        for (size_t gn = 0; gn < ng; gn++) {
            auto mask = _groups[gn].match_occupied();
            if (gn == ng - 1)
                mask &= ~(1 << (N - 1));
            while (mask) {
                auto n = unchecked_ctz(mask);
                _pairs[gn * N + n].~PairT();
                mask &= mask - 1;
            }
        }
    }

    void clone(const HashMap& other) {
        if (other.size() == 0) {
            clear();
            return;
        }
        clear_data();
        auto other_ng = other._num_groups();
        if (other_ng != _num_groups()) {
            if (_pairs)
                free(_pairs); // single free
            _pairs = nullptr;
            _groups = dummy_groups();
            alloc_arrays(other_ng, other._groups_size_index);
        }
        auto ng = _num_groups();
        if (is_trivially_copyable()) {
            memcpy(reinterpret_cast<char*>(_pairs), reinterpret_cast<const char*>(other._pairs),
                   _capacity() * sizeof(PairT));
        } else {
            for (size_t gn = 0; gn < ng; gn++) {
                auto mask = other._groups[gn].match_occupied();
                if (gn == ng - 1)
                    mask &= ~(1 << (N - 1));
                while (mask) {
                    auto n = unchecked_ctz(mask);
                    auto slot = gn * N + n;
                    new (_pairs + slot) PairT(other._pairs[slot]);
                    mask &= mask - 1;
                }
            }
        }
        memcpy(_groups, other._groups, ng * sizeof(group15));
        _num_filled = other._num_filled;
        _max_load = other._max_load;
    }

    void alloc_arrays(size_t num_groups, size_t size_index) {
        _groups_size_mask = num_groups - 1;
        _groups_size_index = size_index;

        // Single allocation: [elements...][align padding][groups...]
        _pairs = static_cast<PairT*>(malloc(buffer_size(num_groups)));

        // Derive _groups from _pairs: advance past all element slots, align to sizeof(group15)=16
        auto p = reinterpret_cast<uintptr_t>(_pairs + num_groups * N - 1);
        _groups = reinterpret_cast<group15*>((p + sizeof(group15) - 1) & ~(sizeof(group15) - 1));

        // Initialize groups
        for (size_t i = 0; i < num_groups; i++)
            _groups[i].initialize();
        _groups[num_groups - 1].set_sentinel();

        _max_load = initial_max_load();
    }

    size_t capacity_for(size_t n) const {
        auto si = pow2_size_policy::size_index(n / N + 1);
        auto gs = pow2_size_policy::size(si);
        return gs * N - 1;
    }

    void rehash_impl(size_t n) {
        if (n < _num_filled)
            n = _num_filled;
        auto m = (size_t)(std::ceil((float)_num_filled / mlf));
        if (m > n)
            n = m;
        if (n)
            n = capacity_for(n);

        if (n == _capacity())
            return;

        auto old_pairs = _pairs;
        auto old_groups = _groups;
        auto old_num_groups = _num_groups();
        auto old_num_filled = _num_filled;
        auto old_owns_data = old_pairs != nullptr;

        _groups = nullptr;
        _pairs = nullptr;
        _num_filled = 0;

        if (n > 0) {
            auto si = pow2_size_policy::size_index(n / N + 1);
            _groups_size_index = si;
            alloc_arrays(pow2_size_policy::size(si), si);
        } else {
            _groups_size_index = sizeof(size_t) * 8;
            _groups_size_mask = 0;
            _max_load = 0;
            _groups = dummy_groups();
            _pairs = nullptr;
        }

        // Reinsert old elements
        if (old_num_filled > 0 && old_pairs && old_num_groups > 0) {
            for (size_t gn = 0; gn < old_num_groups; gn++) {
                auto mask = old_groups[gn].match_occupied();
                if (gn == old_num_groups - 1)
                    mask &= ~(1 << (N - 1));
                while (mask) {
                    auto sn = unchecked_ctz(mask);
                    auto slot = gn * N + sn;
                    auto& src = old_pairs[slot];
                    auto hash = hash_for(src.first);
                    auto pos0 = position_for(hash);
                    auto loc = find_empty_slot_and_insert(pos0, hash);
                    transfer_element(loc, src);
                    if (need_explicit_dtor())
                        src.~PairT();
                    _num_filled++;
                    mask &= mask - 1;
                }
            }
        }

        if (old_owns_data)
            free(old_pairs); // single free
    }

    // Insert new element into new arrays first, then transfer old elements.
    // Avoids double hash computation for the new element.
    template <typename... Args> locator unchecked_emplace_with_rehash(size_t hash, Args&&... args) {
        auto cap = _capacity();
        auto new_cap = capacity_for((size_t)std::ceil((_num_filled + _num_filled / 61 + 1) / mlf));
        if (new_cap <= cap)
            new_cap = capacity_for(cap + N * 2);

        auto old_pairs = _pairs;
        auto old_groups = _groups;
        auto old_num_groups = _num_groups();
        auto old_num_filled = _num_filled;

        // Allocate new arrays
        auto si = pow2_size_policy::size_index(new_cap / N + 1);
        _groups_size_index = si;
        alloc_arrays(pow2_size_policy::size(si), si);

        // Insert new element first (only one hash computation)
        auto pos0 = position_for(hash);
        auto loc = find_empty_slot_and_insert(pos0, hash);
        new (loc.p) PairT(std::forward<Args>(args)...);
        _num_filled = 1;

        // Transfer old elements
        if (old_num_filled > 0 && old_pairs && old_num_groups > 0) {
            for (size_t gn = 0; gn < old_num_groups; gn++) {
                auto mask = old_groups[gn].match_occupied();
                if (gn == old_num_groups - 1)
                    mask &= ~(1 << (N - 1));
                while (mask) {
                    auto sn = unchecked_ctz(mask);
                    auto old_slot = gn * N + sn;
                    auto& src = old_pairs[old_slot];
                    auto h = hash_for(src.first);
                    auto p0 = position_for(h);
                    auto loc = find_empty_slot_and_insert(p0, h);
                    transfer_element(loc, src);
                    if (need_explicit_dtor())
                        src.~PairT();
                    _num_filled++;
                    mask &= mask - 1;
                }
            }
        }

        free(old_pairs);
        return loc;
    }

    // Move or copy element during rehash (move_if_noexcept semantics)
    void transfer_element(const locator& dst, PairT& src) {
        if constexpr (std::is_nothrow_move_constructible<PairT>::value || !std::is_copy_constructible<PairT>::value) {
            new (dst.p) PairT(std::move(src));
        } else {
            new (dst.p) PairT(src);
        }
    }

    void rehash_grow() {
        auto cap = _capacity();
        auto new_cap = capacity_for((size_t)std::ceil((_num_filled + _num_filled / 61 + 1) / mlf));
        if (new_cap <= cap)
            new_cap = capacity_for(cap + N * 2);
        rehash_impl(new_cap);
    }

    // ─── data members ─────────────────────────────────────────────────

    detail::ebo<HashT, 0> _hasher_base;
    detail::ebo<EqT, 1> _eq_base;

    group15* _groups = dummy_groups(); // derived pointer into _pairs allocation
    PairT* _pairs = nullptr;    // base allocation pointer
    size_t _num_filled = 0;
    size_t _max_load = 0;
    size_t _groups_size_index = sizeof(size_t) * 8;
    size_t _groups_size_mask = 0;
};

} // namespace emilib4

#ifdef EMILIB4_64BIT_ARCH
#undef EMILIB4_64BIT_ARCH
#endif
