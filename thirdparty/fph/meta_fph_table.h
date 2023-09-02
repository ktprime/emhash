// Meta Flash Perfect Hash Table

/**
 * This file provides meta perfect hash set and map, which are ultra-fast in query but slow in
 * insert. The flash perfect hash table has no collisions in its hash, so there will be one shot in
 * fetching from the main slots memory. The difference between meta fph table and dynamic fph table
 * is that meta fph table is better at finding keys not existing in the table when the
 * number of elements is large.
 *
 *
 * The api of MetaFphMap is almost the same with the std::unordered_map, but there are
 * some differences (so is MetaFphSet):
 * 1. The template parameter SeedHash is different from the Hash in STL, it has to be a functor
 * accept two arguments: both the key and the seed
 * 2. The keys have to be CopyConstructible
 * 3. The values have to be MoveConstructible
 * 4. May invalidates any references and pointers to elements within the table after rehash
 *
 * We use an exponential multiple of 2 size for slots. Saying that the number of slots is m and the
 * element number is n. n <= m and the size of slots will be sizeof(value_type) * m bytes
 *
 * The speed of insertion is very sensitive to the max_load_factor parameter, if you will use
 * insert() function to construct a table and you do care a little about the insert time. We suggest
 * that you use the default max_load_factor, which is around 0.6. But if you don't care about the
 * insert time, or you will use the Build() to construct the table, and most importantly, you want
 * to save the memory size and cache size (which would probably accelerate the querying), you can
 * set a max_load_factor no larger than max_load_factor_upper_limit(), which should be 0.98.
 *
 * If the range of your keys are limited and they won't change at some time of your program,
 * you can set a large max_load_factor and then call rehash(element_size) to rehash the elements to
 * smaller slots if the load_factor can be larger in that case. (Make sure almost no new keys will
 * be added to the table after this because the insert operation will be very slow when the
 * load_factor is very large.)
 *
 * The extra hot memory space except slots during querying is the space for buckets and metadata.
 * The size of buckets is about c * n / (log2(n) + 1) * sizeof(BucketParamType) bytes. The size of
 * meta data is n bytes. c is a parameter that must be larger than 1.5. The larger c is, the quicker
 * it will be for the insert operations. BucketParamType is a unsigned type and it must meet the
 * condition that 2^(number of bits of BucketParamType) is bigger than the element number. So you
 * should choose the BucketParamType that is just large enough but not too large if you don't want
 * to waste the memory and cache size. The memory size for this extra hot memory space will be
 * slightly larger than c * n bits.
 *
 *
 * We provide three kinds of SeedHash function for basic types: fph::meta::SimpleSeedHash<T>,
 * fph::meta::MixSeedHash<T> and fph::meta::StrongSeedHash<T>;
 * The SimpleSeedHash has the fastest calculation speed and the weakest hash distribution, while the
 * StrongSeedHash is the slowest of them to calculate but has the best distribution of hash value.
 * The MixSeedHash is in the mid of them.
 * Take integer for an example, if the keys you want to insert is not uniformly distributed in
 * the integer interval of that type, then the hash value may probably not be uniformly distributed
 * in the hash type interval as well for a weak hash function. But with a strong hash function,
 * you can easily produce uniformly distributed hash values regardless of your input distribution.
 * If the hash values of the input keys are not uniformly distributed, there may be a failure in the
 * building of the hash table.
 * Tips: know your input keys patterns before choosing the seed hash function. If your keys may
 * cause a failure in the building of the table, use a stronger seed hash function.
 *
 * If you want to write your a custom seed hash function for your own class, refer to the
 * fph::SimpleSeedHash<T>; the functor needs to take both a key and a seed (size_t) as input and
 * return a size_t type hash value;
 *
 */

#pragma once

#include <stdexcept>
#include <cmath>
#include <limits>
#include <tuple>
#include <cassert>
#include <iterator>
#include <vector>
#include <string>
#include <random>
#include <cstring>
#include <deque>
#include <chrono>
#include <utility>
#include <algorithm>


// flash perfect map
namespace fph {

#ifndef FPH_ENABLE_ITERATOR
#define FPH_ENABLE_ITERATOR 1
#endif

#ifndef FPH_DY_DUAL_BUCKET_SET
#define FPH_DY_DUAL_BUCKET_SET 0
#endif

#ifndef FPH_DEBUG_FLAG
#define FPH_DEBUG_FLAG 0
#endif

#ifndef FPH_DEBUG_ERROR
#define FPH_DEBUG_ERROR 0
#endif

#ifndef FPH_HAVE_BUILTIN
#ifdef __has_builtin
#define FPH_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define FPH_HAVE_BUILTIN(x) 0
#endif
#endif

#ifndef FPH_HAS_BUILTIN_OR_GCC_CLANG
#if defined(__GNUC__) || defined(__clang__)
#   define FPH_HAS_BUILTIN_OR_GCC_CLANG(x) 1
#else
#   define FPH_HAS_BUILTIN_OR_GCC_CLANG(x) FPH_HAVE_BUILTIN(x)
#endif
#endif

#ifndef FPH_PREFETCH
#   if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_prefetch)
#       define FPH_PREFETCH(addr, rw, level) __builtin_prefetch((addr), rw, level)
#   else
#       define FPH_PREFETCH(addr, rw, level) {}
#   endif
#endif


#ifndef FPH_LIKELY
#   if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_expect)
#       define FPH_LIKELY(x) (__builtin_expect((x), 1))
#   else
#       define FPH_LIKELY(x) (x)
#   endif
#endif

#ifndef FPH_UNLIKELY
#   if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_expect)
#       define FPH_UNLIKELY(x) (__builtin_expect((x), 0))
#   else
#       define FPH_UNLIKELY(x) (x)
#   endif
#endif

#ifndef FPH_ALWAYS_INLINE
#   ifdef _MSC_VER
#       define FPH_ALWAYS_INLINE __forceinline
#   else
#       define FPH_ALWAYS_INLINE inline __attribute__((__always_inline__))
#   endif
#endif

#ifndef FPH_RESTRICT
#ifdef _MSC_VER
#   define FPH_RESTRICT
#else
#   define FPH_RESTRICT __restrict
#endif
#endif

#ifndef FPH_FUNC_RESTRICT
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#   define FPH_FUNC_RESTRICT __restrict
#else
#   define FPH_FUNC_RESTRICT
#endif
#endif

#ifndef FPH_HAVE_EXCEPTIONS

#if !(defined(__GNUC__) && !defined(__cpp_exceptions)) && \
    !(defined(__GNUC__) && defined(__cpp_exceptions) && __cpp_exceptions == 0 ) && \
    !(defined(_MSC_VER) && !defined(_CPPUNWIND))
#define FPH_HAVE_EXCEPTIONS 1
#endif

#endif

// From absl library
// A function-like feature checking macro that accepts C++11 style attributes.
// It's a wrapper around `__has_cpp_attribute`, defined by ISO C++ SD-6
// (https://en.cppreference.com/w/cpp/experimental/feature_test). If we don't
// find `__has_cpp_attribute`, will evaluate to 0.
#ifndef FPH_HAVE_CPP_ATTRIBUTE
#if defined(__cplusplus) && defined(__has_cpp_attribute)
// NOTE: requiring __cplusplus above should not be necessary, but
// works around https://bugs.llvm.org/show_bug.cgi?id=23435.
#   define FPH_HAVE_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#   define FPH_HAVE_CPP_ATTRIBUTE(x) 0
#endif
#endif

#ifndef FPH_FALLTHROUGH_INTENDED
#   if FPH_HAVE_CPP_ATTRIBUTE(fallthrough)
#       define FPH_FALLTHROUGH_INTENDED [[fallthrough]]
#   elif FPH_HAVE_CPP_ATTRIBUTE(clang::fallthrough)
#       define FPH_FALLTHROUGH_INTENDED [[clang::fallthrough]]
#   elif FPH_HAVE_CPP_ATTRIBUTE(gnu::fallthrough)
#       define FPH_FALLTHROUGH_INTENDED [[gnu::fallthrough]]
#   else
#define FPH_FALLTHROUGH_INTENDED \
          do {                            \
          } while (0)
#   endif
#endif

    namespace meta::detail {
        template <typename> inline constexpr bool always_false_v = false;

#if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_clz) && FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_clzll)
#define FPH_INTERNAL_CONSTEXPR_CLZ constexpr
#define FPH_INTERNAL_HAS_CONSTEXPR_CLZ 1
#else
#define FPH_INTERNAL_CONSTEXPR_CLZ
#define FPH_INTERNAL_HAS_CONSTEXPR_CLZ 0
#endif

        FPH_INTERNAL_CONSTEXPR_CLZ inline int CountLeadingZero64(uint64_t x) {
#if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_clzll)
            return __builtin_clzll(x);
#elif defined(_MSC_VER)
            unsigned long result = 0;  // NOLINT(runtime/int)
            if ((x >> 32) &&
                _BitScanReverse(&result, static_cast<unsigned long>(x >> 32))) {
                return 31 - result;
            }
            if (_BitScanReverse(&result, static_cast<unsigned long>(x))) {
                return 63 - result;
            }
            return 64;
#endif
        }

        FPH_INTERNAL_CONSTEXPR_CLZ inline int CountLeadingZero32(uint32_t x) {
#if FPH_HAS_BUILTIN_OR_GCC_CLANG(__builtin_clz)
            return __builtin_clz(x);
#elif defined(_MSC_VER)
            unsigned long result = 0;  // NOLINT(runtime/int)
            if (_BitScanReverse(&result, x)) {
                return 31 - result;
            }
            return 32;
#endif
        }

        FPH_INTERNAL_CONSTEXPR_CLZ inline uint64_t RoundUp64Log2(uint64_t x) {
            if (x <= 1ULL) {
                return x;
            }
            return 64ULL - CountLeadingZero64(x - 1ULL);
        }

        FPH_INTERNAL_CONSTEXPR_CLZ inline uint32_t RoundUp32Log2(uint32_t x) {
            if (x <= 1U) {
                return x;
            }
            return 32U - CountLeadingZero32(x - 1U);
        }

        template<typename T>
        FPH_INTERNAL_CONSTEXPR_CLZ T RoundUpLog2(T x) {
            static_assert(std::is_unsigned<T>::value, "RoundUpLog2 type must be unsigned");
            if constexpr (sizeof(T) == 8) {
                return RoundUp64Log2(x);
            }
            else if constexpr (sizeof(T) == 4) {
                return RoundUp32Log2(x);
            }
            else {
                static_assert(always_false_v<T>, "Not supported size for RoundUpLog2");
            }
        }

#if FPH_INTERNAL_HAS_CONSTEXPR_CLZ
        static_assert(RoundUpLog2(0U) == 0);
        static_assert(RoundUpLog2(1U) == 1U);
        static_assert(RoundUpLog2(15U) == 4U);
        static_assert(RoundUpLog2(16U) == 4U);
        static_assert(RoundUpLog2(31ULL) == 5ULL);
        static_assert(RoundUpLog2(32ULL) == 5ULL);
#endif

        template<typename T>
        constexpr T GenBitMask(unsigned mask_len) {
            static_assert(std::is_unsigned<T>::value, "Type T is not unsigned for mask");
            constexpr unsigned num_bits = std::numeric_limits<T>::digits;
            static_assert(static_cast<uint32_t>(-1) == 0xffffffffU);
            T ret = mask_len >= num_bits ? static_cast<T>(-1) : (static_cast<T>(1U) << mask_len) - static_cast<T>(1U);
            return ret;
        }

        static_assert(GenBitMask<uint32_t>(0) == 0x0U);
        static_assert(GenBitMask<uint32_t>(4) == 0xfU);
        static_assert(GenBitMask<uint32_t>(32) == 0xffffffffU);
        static_assert(GenBitMask<uint64_t>(64) == 0xffffffffffffffffULL);

        /**
         * Calculates the smallest integral power of two that is not smaller than x.
         * @tparam T
         * @param x
         * @return
         */
        template<typename T>
        FPH_INTERNAL_CONSTEXPR_CLZ T Ceil2(T x) {
            static_assert(std::is_unsigned<T>::value, "Ceil2 type must be unsigned");
            auto roundup_log_2 = RoundUpLog2(x);
            return (T(1U)) << roundup_log_2;
        }

#if FPH_INTERNAL_HAS_CONSTEXPR_CLZ
        static_assert(Ceil2(1U) == 2U);
        static_assert(Ceil2(0U) == 1U);
        static_assert(Ceil2(32U) == 32U);
        static_assert(Ceil2(33U) == 64U);
#endif

        /**
         * Get the mask value in the format of (1ULL << num_bits) - 1 and no less than the x
         * @tparam T
         * @param x
         * @return
         */
        template<typename T>
        FPH_INTERNAL_CONSTEXPR_CLZ T CeilToMask(T x) {
            static_assert(std::is_unsigned<T>::value, "x is not unsigned");
            auto roundup_log_2 = RoundUpLog2(x);
            return x == std::numeric_limits<T>::max() ? x :
                   ( x == (T(1U) << roundup_log_2) ? GenBitMask<T>(roundup_log_2 + 1) :
                     GenBitMask<T>(roundup_log_2));
        }

#if FPH_INTERNAL_HAS_CONSTEXPR_CLZ
        static_assert(CeilToMask(0U) == 0U);
        static_assert(CeilToMask(5U) == 7U);
        static_assert(CeilToMask(8U) == 15U);
        static_assert(CeilToMask(15U) == 15U);
        static_assert(CeilToMask(0xffffffffU) == 0xffffffffU);
#endif

        template <typename T, typename U>
        constexpr T RotateR (T v, U b)
        {
            static_assert(std::is_integral<T>::value, "rotate of non-integral type");
            static_assert(! std::is_signed<T>::value, "rotate of signed type");
            static_assert(std::is_integral<U>::value, "rotate of non-integral type");
            constexpr unsigned int num_bits {std::numeric_limits<T>::digits};
            static_assert(0 == (num_bits & (num_bits - 1)), "rotate value bit length not power of two");
            constexpr unsigned int count_mask {num_bits - 1};
            // to make sure mb < num_bits
            const U mb {b & count_mask};
            using promoted_type = typename std::common_type<int, T>::type;
            using unsigned_promoted_type = typename std::make_unsigned<promoted_type>::type;
            return ((unsigned_promoted_type{v} >> mb)
                    | (unsigned_promoted_type{v} << (-mb & count_mask)));
        }

        inline void ThrowRuntimeError(const char* what) {
#ifdef FPH_HAVE_EXCEPTIONS
            throw std::runtime_error(what);
#else
            (void)what;
            std::abort();
#endif
        }

        inline void ThrowInvalidArgument(const char* what) {
#ifdef FPH_HAVE_EXCEPTIONS
            throw std::invalid_argument(what);
#else
            (void)what;
            std::abort();
#endif
        }

        inline void ThrowOutOfRange(const char* what) {
#ifdef FPH_HAVE_EXCEPTIONS
            throw std::out_of_range(what);
#else
            (void)what;
            std::abort();
#endif
        }

        // Used to fetch bits from array
        /**
         * The bit number of T need to be divisible by the bit number of ITEM
         * @tparam T , the underlying array entry type, should be no smaller than the ITEM
         * @tparam ITEM_BIT_SIZE , the bits number of the ITEM
         */
        template <typename T, size_t ITEM_BIT_SIZE>
        class BitArrayView {
        private:

            static constexpr size_t ARRAY_ENTRY_BITS = sizeof(T) * 8UL;
            static constexpr T ITEM_MASK = GenBitMask<T>(ITEM_BIT_SIZE);
            T* arr_;
        public:
            BitArrayView(T* arr) : arr_(arr) {}
            FPH_ALWAYS_INLINE T get(size_t index) const FPH_FUNC_RESTRICT {
                size_t arrIndex = index / (ARRAY_ENTRY_BITS / ITEM_BIT_SIZE);
                size_t shiftCount = (index * ITEM_BIT_SIZE) % ARRAY_ENTRY_BITS;
                return (arr_[arrIndex] >> shiftCount) & ITEM_MASK;
            }
            FPH_ALWAYS_INLINE void set(size_t index, T value) FPH_FUNC_RESTRICT {
                size_t arrIndex = index / (ARRAY_ENTRY_BITS / ITEM_BIT_SIZE);
                size_t shiftCount = (index * ITEM_BIT_SIZE) % ARRAY_ENTRY_BITS;
                // value &= ITEM_MASK; // trim
                arr_[arrIndex] &= ~(ITEM_MASK << shiftCount); // clear target bits
                arr_[arrIndex] |= value << shiftCount; // insert new bits
            }

            T* data() const {
                return arr_;
            }

            void SetUnderlyingArray(T* arr) {
                arr_ = arr;
            }

            // Get the needed underlying array entry
            static size_t GetUnderlyingEntryNum(size_t item_num) {
                return CeilDiv(item_num, ARRAY_ENTRY_BITS / ITEM_BIT_SIZE);
            }

        private:
            static size_t CeilDiv(size_t a, size_t b) {
                return (a + b - size_t(1U)) / b;
            }
        }; // class BitArrayView


    } // namespace meta::detail



    namespace meta::detail {

        // from robin_hood hash
        template <typename T>
        inline T UnalignedLoad(void const* ptr) noexcept {
            // using memcpy so we don't get into unaligned load problems.
            // compiler should optimize this very well anyways.
            T t;
            std::memcpy(&t, ptr, sizeof(T));
            return t;
        }

        inline size_t HashBytes(void const* ptr, size_t len, uint64_t seed) noexcept {
            static constexpr uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
//            static constexpr uint64_t seed = UINT64_C(0xe17a1465);
            static constexpr unsigned int r = 47;

            auto const* const data64 = static_cast<uint64_t const*>(ptr);
            uint64_t h = seed ^ (len * m);

            size_t const n_blocks = len / 8;
            for (size_t i = 0; i < n_blocks; ++i) {
                auto k = detail::UnalignedLoad<uint64_t>(data64 + i);

                k *= m;
                k ^= k >> r;
                k *= m;

                h ^= k;
                h *= m;
            }

            auto const* const data8 = reinterpret_cast<uint8_t const*>(data64 + n_blocks);
            switch (len & 7U) {
                case 7:
                    h ^= static_cast<uint64_t>(data8[6]) << 48U;
                    FPH_FALLTHROUGH_INTENDED;
                case 6:
                    h ^= static_cast<uint64_t>(data8[5]) << 40U;
                    FPH_FALLTHROUGH_INTENDED;
                case 5:
                    h ^= static_cast<uint64_t>(data8[4]) << 32U;
                    FPH_FALLTHROUGH_INTENDED;
                case 4:
                    h ^= static_cast<uint64_t>(data8[3]) << 24U;
                    FPH_FALLTHROUGH_INTENDED;
                case 3:
                    h ^= static_cast<uint64_t>(data8[2]) << 16U;
                    FPH_FALLTHROUGH_INTENDED;
                case 2:
                    h ^= static_cast<uint64_t>(data8[1]) << 8U;
                    FPH_FALLTHROUGH_INTENDED;
                case 1:
                    h ^= static_cast<uint64_t>(data8[0]);
                    h *= m;
                    FPH_FALLTHROUGH_INTENDED;
                default:
                    break;
            }

            h ^= h >> r;

            // not doing the final step here, because this will be done by keyToIdx anyways
            // h *= m;
            // h ^= h >> r;
            return static_cast<size_t>(h);
        }

        template<class T>
        class SimpleSeedHash64 {
        public:

            FPH_ALWAYS_INLINE uint64_t operator() (T key, uint64_t seed) const {
                static_assert(sizeof(T) == 8, "sizeof(T) should be 64-bit");
                static_assert(std::is_unsigned<T>::value, "T should be unsigned type");
                key ^= key >> 33U;
                key *= (seed + 0xff51afd7ed558ccdULL);
                key ^= key >> 33U;
                return key;
            }
        };

        template<class T>
        class SimpleSeed2Hash64 {
        public:

            FPH_ALWAYS_INLINE uint64_t operator() (T key, uint64_t seed) const {
                static_assert(sizeof(T) == 8, "sizeof(T) should be 64-bit");
                static_assert(std::is_unsigned<T>::value, "T should be unsigned type");
                key ^= key >> 33U;
                key *= seed;
                key ^= key >> 33U;
                return key;
            }
        };

        FPH_ALWAYS_INLINE uint64_t AnoSeedHash64(uint64_t key, uint64_t seed) {
            key ^= key >> 33U;
            key *= (seed + 0xff51afd7ed558ccdULL);
            key ^= key >> 33U;
            return key;
        }

        FPH_ALWAYS_INLINE uint64_t Ano2SeedHash64(uint64_t key, uint64_t seed) {
            key ^= key >> 33U;
            key *= seed;
            key ^= key >> 33U;
            return key;
        }

        FPH_ALWAYS_INLINE uint64_t Ano5SeedHash64(uint64_t key, uint64_t seed) {
            key ^= key >> 33U;
            key *= seed;
            key ^= key << 33U;
            return key;
        }

        FPH_ALWAYS_INLINE uint64_t Ano4SeedHash64(uint64_t key, uint64_t seed) {
            key ^= key >> 37U;
            key *= seed;
            key ^= key >> 32U;
            return key;
        }

        FPH_ALWAYS_INLINE uint64_t Ano2SeedHash32(uint32_t key, uint64_t seed) {
            uint64_t m = uint64_t(key) * seed;
            auto ret = m ^ (m >> 33U);
            return ret;
        }

        FPH_ALWAYS_INLINE uint64_t Ano2SeedHash16(uint16_t key, uint64_t seed) {
//            key ^= key >> 33U;
            uint64_t m = uint64_t(key) * seed;
            auto ret = m ^ (m >> 33U);
            return ret;
        }



        // from absl
        FPH_ALWAYS_INLINE constexpr size_t Ano3SeedHash16(uint16_t key, size_t seed) {
#if defined(__aarch64__)
            // On AArch64, calculating a 128-bit product is inefficient, because it
            // requires a sequence of two instructions to calculate the upper and lower
            // halves of the result.
            using MultType = uint64_t;
#else
            using MultType = uint64_t;
#endif
            MultType m = uint64_t(key) + 0xff51afd7ed558ccdULL;
            m *= seed;
            return static_cast<size_t>(m ^ (m >> (sizeof(m) * 8 / 2)));
        }

        FPH_ALWAYS_INLINE uint64_t MulSeedHash64(uint64_t key, uint64_t seed) {
            key = RotateR((key +0xff51afd7ed558ccdULL) * seed, 33U);
            return key;
        }

        constexpr FPH_ALWAYS_INLINE size_t RotMulSeedHash64(uint64_t key, uint64_t seed) {
            key = RotateR(key * seed, 33U);
            return key;
        }

        constexpr FPH_ALWAYS_INLINE uint64_t MulOnlyHash64(uint64_t key, uint64_t seed) {
            uint64_t ret = key * seed;
            return ret;
        }

        constexpr FPH_ALWAYS_INLINE uint64_t IdentitySeedHash64(uint64_t key, uint64_t /* seed */) {
            return key;
        }

        FPH_ALWAYS_INLINE size_t RotMul3SeedHash64(uint64_t key, uint64_t seed) {
            uint64_t mul = key * seed;
            key = mul ^ RotateR(mul, 48U);
            return key;
        }

        FPH_ALWAYS_INLINE size_t RotMulSeedHash16(uint16_t key, size_t seed) {
            size_t mul = size_t(key) * seed;
            auto ret = (mul & size_t(0xffffffff00000000ULL)) ^ (RotateR(uint32_t(mul), 7U));
            return ret;
        }

        template<class T>
        FPH_ALWAYS_INLINE T ReverseOrder64(T x) noexcept {
#ifdef _MSC_VER
            return _byteswap_uint64(x);
#else
            return __builtin_bswap64(x);
#endif
        }

        FPH_ALWAYS_INLINE size_t RevMulSeedHash64(uint64_t key, uint64_t seed) {
            uint64_t mul = key * seed;
            auto ret = mul ^ ReverseOrder64(mul);
            return ret;
        }

        FPH_ALWAYS_INLINE size_t RevMul2SeedHash64(uint64_t key, uint64_t seed) {
            key = key ^ ReverseOrder64(key);
            auto ret = key * seed;
            return ret;
        }

        FPH_ALWAYS_INLINE size_t MulMovSeedHash64(uint64_t key, uint64_t seed) {
            key ^= (key >> 33U);
            auto ret = (seed * key) >> 33U;
            return ret;
        }

        FPH_ALWAYS_INLINE size_t ShiftMulSeedHash64(uint64_t key, uint64_t seed) {
            key ^= (key >> 33U);
            auto ret = (seed * key);
            return ret;
        }

        FPH_ALWAYS_INLINE size_t ShiftXorSeedHash64(uint64_t key, uint64_t /* seed */) {
            key ^= (key >> 32U);
            return key;
        }

        FPH_ALWAYS_INLINE uint64_t NaiveMulSeedHash64(uint64_t key, uint64_t seed) {
            auto ret = key * (seed & 1ULL) ;
            return ret;
        }

        FPH_ALWAYS_INLINE uint64_t XorMulSeedHash64(uint64_t key, uint64_t seed) {
            auto ret = key ^ RotateR(key * seed, 33U);
            return ret;
        }

//        FPH_ALWAYS_INLINE uint64_t Mul2SeedHash64(uint64_t key, uint64_t seed) {
//            __uint128_t mul = key;
//            mul *= seed;
//            return uint64_t(mul) ^ uint64_t(mul >> 64U);
//        }

        FPH_ALWAYS_INLINE uint64_t DualRotMulSeedHash64(uint64_t key, uint64_t seed) {
            uint64_t mul = key * seed;
            auto ret = mul ^ RotateR(mul, 48U);
            return ret;
        }

        FPH_ALWAYS_INLINE uint32_t MulSeedHash32(uint32_t key, uint32_t seed) {
            key = RotateR(key * seed, 17U);
            return key;
        }


        template<class T>
        class SimpleMulSeedHash64 {
        public:

            FPH_ALWAYS_INLINE uint64_t operator() (T key, uint64_t seed) const {
                static_assert(sizeof(T) == 8, "sizeof(T) should be 64-bit");
                static_assert(std::is_unsigned<T>::value, "T should be unsigned type");
                return MulSeedHash64(key, seed);
            }
        };

        constexpr FPH_ALWAYS_INLINE size_t ChosenSimpleSeedHash64(uint64_t key, size_t seed) {
//            return RotMulSeedHash64(key, seed);
//            return MulOnlyHash64(key, seed);
            return IdentitySeedHash64(key, seed);
        }

        FPH_ALWAYS_INLINE size_t ChosenMixSeedHash64(uint64_t key, size_t seed) {
//            return RevMulSeedHash64(key, seed);
//            return MulMovSeedHash64(key, seed);
//            return ShiftMulSeedHash64(key, seed);
            return ShiftXorSeedHash64(key, seed);
//            return Ano4SeedHash64(key, seed);
        }

        FPH_ALWAYS_INLINE size_t ChosenStrongSeedHash64(uint64_t key, size_t seed) {
            return Ano2SeedHash64(key, seed);
        }

        FPH_ALWAYS_INLINE size_t ChosenSimpleSeedHash16(uint16_t key, size_t seed) {
            return RotMulSeedHash64(key, seed);
        }



        FPH_ALWAYS_INLINE size_t ChosenSeedHash16(uint16_t key, size_t seed) {
//            return RotMulSeedHash16(key, seed);
            return AnoSeedHash64(key, seed);
//            return Ano3SeedHash16(key, seed);
        }

        FPH_ALWAYS_INLINE size_t ChosenSeedHash32(uint32_t key, size_t seed) {
            return Ano2SeedHash32(key, seed);
        }

        template<class T, typename = void>
        struct SimpleSeedHash {
            constexpr FPH_ALWAYS_INLINE size_t operator()(const T& x, size_t seed) const noexcept {
                return meta::detail::ChosenSimpleSeedHash64(x, seed);
            }
        };

        template<>
        struct SimpleSeedHash<uint32_t> {
            size_t operator()(uint32_t x, size_t seed) const noexcept {
                return meta::detail::ChosenSimpleSeedHash64(x, seed);
            }
        };

        template<>
        struct SimpleSeedHash<int32_t> {
            size_t operator()(int32_t x, size_t seed) const noexcept {
                return meta::detail::ChosenSimpleSeedHash64(x, seed);
            }
        };

        template<>
        struct SimpleSeedHash<uint16_t> {
            size_t operator()(uint16_t x, size_t seed) const noexcept {
                return meta::detail::ChosenSimpleSeedHash64(x, seed);
            }
        };

        template<>
        struct SimpleSeedHash<int16_t> {
            size_t operator()(int16_t x, size_t seed) const noexcept {
                return meta::detail::ChosenSimpleSeedHash64(x, seed);
            }
        };

        template<class T>
        struct SimpleSeedHash<T*> {
            size_t operator()(T* x, size_t seed) const noexcept {
                return meta::detail::ChosenSimpleSeedHash64(reinterpret_cast<size_t>(x), seed);
            }
        };

        template<class CharT>
        struct SimpleSeedHash<std::basic_string<CharT>> {
            size_t operator()(const std::basic_string<CharT> &str, size_t seed) const noexcept {
                return meta::detail::HashBytes(str.data(), sizeof(CharT) * str.length(), seed);
            }
        };

        template<class CharT>
        struct SimpleSeedHash<std::basic_string_view<CharT>> {
            size_t operator()(std::basic_string_view<CharT> str, size_t seed) const noexcept {
                return meta::detail::HashBytes(str.data(), sizeof(CharT) * str.length(), seed);
            }
        };

        template<class T, typename = void>
        struct MixSeedHash {
            size_t operator()(const T& x, size_t seed) const noexcept {
                return meta::detail::ChosenMixSeedHash64(x, seed);
            }
        };

        template<>
        struct MixSeedHash<uint32_t> {
            size_t operator()(uint32_t x, size_t seed) const noexcept {
                return meta::detail::ChosenMixSeedHash64(x, seed);
            }
        };

        template<>
        struct MixSeedHash<int32_t> {
            size_t operator()(int32_t x, size_t seed) const noexcept {
                return meta::detail::ChosenMixSeedHash64(x, seed);
            }
        };

        template<>
        struct MixSeedHash<uint16_t> {
            size_t operator()(uint16_t x, size_t seed) const noexcept {
                return meta::detail::ChosenMixSeedHash64(x, seed);
            }
        };

        template<>
        struct MixSeedHash<int16_t> {
            size_t operator()(int16_t x, size_t seed) const noexcept {
                return meta::detail::ChosenMixSeedHash64(x, seed);
            }
        };

        template<class T>
        struct MixSeedHash<T*> {
            size_t operator()(T* x, size_t seed) const noexcept {
                return meta::detail::ChosenMixSeedHash64(reinterpret_cast<size_t>(x), seed);
            }
        };

        template<class CharT>
        struct MixSeedHash<std::basic_string<CharT>> {
            size_t operator()(const std::basic_string<CharT> &str, size_t seed) const noexcept {
                return meta::detail::HashBytes(str.data(), sizeof(CharT) * str.length(), seed);
            }
        };

        template<class CharT>
        struct MixSeedHash<std::basic_string_view<CharT>> {
            size_t operator()(std::basic_string_view<CharT> str, size_t seed) const {
                return meta::detail::HashBytes(str.data(), sizeof(CharT) * str.length(), seed);
            }
        };

        template<class T, typename = void>
        struct StrongSeedHash {
            size_t operator()(const T& x, size_t seed) const noexcept {
                return meta::detail::ChosenStrongSeedHash64(x, seed);
            }
        };

        template<>
        struct StrongSeedHash<uint32_t> {
            size_t operator()(uint32_t x, size_t seed) const noexcept {
                return meta::detail::ChosenStrongSeedHash64(x, seed);
            }
        };

        template<>
        struct StrongSeedHash<int32_t> {
            size_t operator()(int32_t x, size_t seed) const noexcept {
                return meta::detail::ChosenStrongSeedHash64(x, seed);
            }
        };

        template<>
        struct StrongSeedHash<uint16_t> {
            size_t operator()(uint16_t x, size_t seed) const noexcept {
                return meta::detail::ChosenStrongSeedHash64(x, seed);
            }
        };

        template<>
        struct StrongSeedHash<int16_t> {
            size_t operator()(int16_t x, size_t seed) const noexcept {
                return meta::detail::ChosenStrongSeedHash64(x, seed);
            }
        };

        template<class T>
        struct StrongSeedHash<T*> {
            size_t operator()(T* x, size_t seed) const noexcept {
                return meta::detail::ChosenStrongSeedHash64(reinterpret_cast<size_t>(x), seed);
            }
        };

        template<class CharT>
        struct StrongSeedHash<std::basic_string<CharT>> {
            size_t operator()(const std::basic_string<CharT> &str, size_t seed) const noexcept {
                return meta::detail::HashBytes(str.data(), sizeof(CharT) * str.length(), seed);
            }
        };

        template<class CharT>
        struct StrongSeedHash<std::basic_string_view<CharT>> {
            size_t operator()(std::basic_string_view<CharT> str, size_t seed) const noexcept {
                return meta::detail::HashBytes(str.data(), sizeof(CharT) * str.length(), seed);
            }
        };

        template<class T, typename std::enable_if_t<!std::is_pointer<T>::value>* = nullptr>
        std::string ToString(const T& t) {
            return std::to_string(t);
        }

        template<class T, typename std::enable_if_t<std::is_pointer<T>::value>* = nullptr>
        std::string ToString(T p) {
            auto x = reinterpret_cast<uintptr_t>(p);
            return std::to_string(x);
        }


        inline std::string ToString(const std::string& t) {
            return t;
        }

        inline std::string ToString(const char* src) {
            return src;
        }

    } // namespace meta::detail

    namespace meta{

        template<class T>
        using SimpleSeedHash = meta::detail::SimpleSeedHash<T>;

        template<class T>
        using MixSeedHash = meta::detail::MixSeedHash<T>;

        template<class T>
        using StrongSeedHash = meta::detail::StrongSeedHash<T>;

    }; // namespace meta



    namespace meta::detail {

        template<class T>
        class BaseNode {
        public:
            constexpr BaseNode() noexcept: value_ptr_(nullptr) {};
            explicit constexpr BaseNode(T *value_ptr) noexcept: value_ptr_(value_ptr) {}

            BaseNode(const BaseNode<T> &other) noexcept: value_ptr_(other.value_ptr_) {}
            BaseNode<T>& operator=(const BaseNode<T> &other) noexcept {
                this->value_ptr_ = other.value_ptr_;
                return *this;
            }

            BaseNode(BaseNode<T> &&other) noexcept: value_ptr_(other.value_ptr_) {
                other.value_ptr_ = nullptr;
            }
            BaseNode<T>& operator=(BaseNode<T> &&other) noexcept {
                this->value_ptr_ = other.value_ptr_;
                other.value_ptr_ = nullptr;
                return *this;
            }

            T *value_ptr() const {
                return this->value_ptr_;
            }

        protected:
            T *value_ptr_;
        };

        template<class T>
        class IteratorBase: public BaseNode<T> {
        public:

            using difference_type   = std::ptrdiff_t;
            using value_type        = typename std::remove_cv<T>::type;
            using pointer           = T*;
            using reference         = T&;

            constexpr IteratorBase() noexcept: BaseNode<T>() {}
            constexpr explicit IteratorBase(T *value_ptr) noexcept: BaseNode<T>(value_ptr) {}

            reference operator*() const {return *this->value_ptr_;};
            pointer operator->() const {return this->value_ptr_;}
            friend bool operator== (const IteratorBase<T>& a, const IteratorBase<T>& b) {
                return a.value_ptr_ == b.value_ptr_;
            }

            friend bool operator!= (const IteratorBase<T>& a, const IteratorBase<T>& b) {
                return a.value_ptr_ != b.value_ptr_;
            }
        };


    } // namespace meta::detail

    namespace meta::detail {

        template<class Key,
                class KeyPointerAllocator=std::allocator<const Key *>,
                class BucketParamType = uint32_t>
        struct FphBucket {
        public:
            BucketParamType entry_cnt;
            BucketParamType index;
            std::vector<const Key *, KeyPointerAllocator> key_array;

            explicit FphBucket(size_t index) noexcept: entry_cnt(0), index(index) {}

            FphBucket() noexcept: entry_cnt(0), index(0) {}

            void AddKey(const Key *key_ptr) {
                key_array.push_back(key_ptr);
            }

        protected:

        };
    } // namespace meta::detail

    namespace meta::detail {

        template<class T>
        struct SimpleGetKey {
            T operator()(const T& x) const {
                return x;
            }
        };

        template<class Bucket>
        struct BucketGetKey {
            size_t operator()(const Bucket& x) const {
                return x.entry_cnt;
            }
        };

        /**
         * Counting sort,
         * @tparam GetKey
         * @tparam is_descend
         * @tparam SizeTAllocator
         * @tparam T
         * @tparam RandomIt
         * @tparam OutputIt
         * @param first
         * @param last
         * @param d_first the output array consisting of the index of sorted array, first[d_first[0]]
         * will be the first element in the sorted array
         * @param max_key
         */
        template<class GetKey, bool is_descend=true, class SizeTAllocator = std::allocator<size_t>, class T, class RandomIt, class OutputIt>
        void CountSortOutIndex(RandomIt first, RandomIt last, OutputIt d_first, T max_key) {
            if (first >= last) {
                return;
            }
            size_t array_num = std::distance(first, last);
            GetKey get_key{};

            std::vector<size_t, SizeTAllocator> count_array(max_key + 1, 0);

            for (auto it = first; it != last; ++it) {
                auto temp_key = get_key(*it);
                ++count_array[temp_key];
            }
            for (size_t i = 1; i <= max_key; ++i) {
                count_array[i] += count_array[i - 1];
            }
            if constexpr (!is_descend) {
                for (size_t i = array_num; i-- > 0;) {
                    auto temp_key = get_key(*(first + i));
                    d_first[--count_array[temp_key]] =  i;
                }
            }
            else {
                for (size_t i = array_num; i-- > 0;) {
                    auto temp_key = get_key(*(first + i));
                    d_first[array_num - (--count_array[temp_key]) - 1] = i;
                }
            }
        }

        template<class GetKey, bool is_descend=true, class SizeTAllocator = std::allocator<size_t>, class RandomIt, class OutputIt>
        void CountSortOutIndex(RandomIt first, RandomIt last, OutputIt d_first) {
            if (first >= last) {
                return;
            }
            GetKey get_key{};
            using ValueType = decltype(get_key(*first));
            static_assert(std::is_integral<ValueType>::value, "Key type should be integer");
            ValueType max_key = std::numeric_limits<ValueType>::min();
            for (auto it = first; it < last; ++it) {
                auto temp_key = get_key(*it);
                if (max_key < temp_key) {
                    max_key = temp_key;
                }
            }

            CountSortOutIndex<GetKey, is_descend, SizeTAllocator>(first, last, d_first, max_key);

        }

        template<class GetKey, bool is_descend=true, class SizeTAllocator = std::allocator<size_t>, class T, class RandomIt, class OutputIt>
        void CountSort(RandomIt first, RandomIt last, OutputIt d_first, T max_key) {
            if (first >= last) {
                return;
            }
            size_t array_num = std::distance(first, last);
            GetKey get_key{};

            std::vector<size_t, SizeTAllocator> count_array(max_key + 1, 0);

            for (auto it = first; it != last; ++it) {
                auto temp_key = get_key(*it);
                ++count_array[temp_key];
            }
            for (size_t i = 1; i <= max_key; ++i) {
                count_array[i] += count_array[i - 1];
            }
            if constexpr (!is_descend) {
                for (auto it = last - 1; it >= first; --it) {
                    auto temp_key = get_key(*it);
                    d_first[--count_array[temp_key]] = std::move(*it);
                }
            }
            else {
                for (auto it = last - 1; it >= first; --it) {
                    auto temp_key = get_key(*it);
                    d_first[array_num - (--count_array[temp_key]) - 1] = std::move(*it);
                }
            }
        }

        template<class GetKey, bool is_descend=true, class SizeTAllocator = std::allocator<size_t>, class RandomIt, class OutputIt>
        void CountSort(RandomIt first, RandomIt last, OutputIt d_first) {
            if (first >= last) {
                return;
            }
            GetKey get_key{};
            using ValueType = decltype(get_key(*first));
            static_assert(std::is_integral<ValueType>::value, "Key type should be integer");
            ValueType max_key = std::numeric_limits<ValueType>::min();
            for (auto it = first; it < last; ++it) {
                auto temp_key = get_key(*it);
                if (max_key < temp_key) {
                    max_key = temp_key;
                }
            }

            CountSort<GetKey, is_descend, SizeTAllocator>(first, last, d_first, max_key);

        }


    } // namespace meta::detail



    namespace meta::detail {

        template<class Table, class slot_type>
        class ForwardIterator : public meta::detail::IteratorBase<slot_type> {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = typename std::remove_cv<typename slot_type::value_type>::type;
            using pointer    = value_type*;
            using reference  = value_type&;

            constexpr ForwardIterator() noexcept
                    : meta::detail::IteratorBase<slot_type>(), map_ptr_(
                    nullptr), iterate_cnt(0) {}


            constexpr explicit ForwardIterator(slot_type *slot_ptr,
                                               const Table *map_ptr) noexcept:
                    meta::detail::IteratorBase<slot_type>(slot_ptr), map_ptr_(map_ptr),
                    iterate_cnt(0) {}

            reference operator*() const {return this->value_ptr_->value;};

            pointer operator->() const {return std::addressof(this->value_ptr_->value);}

            ForwardIterator<Table, slot_type> &operator++() {
                if FPH_UNLIKELY(++iterate_cnt >= map_ptr_->size()) {
                    this->value_ptr_ = nullptr;
                } else {
                    this->value_ptr_ = map_ptr_->GetNextSlotAddress(this->value_ptr_);
                }
                return *this;
            }

            ForwardIterator<Table, slot_type> operator++(int) {
                auto ret = *this;
                ++(*this);
                return ret;
            }

            void SetTable(const Table *new_table_ptr) {
                map_ptr_ = new_table_ptr;
            }


        protected:
            const Table *map_ptr_;
            size_t iterate_cnt;


        };

        template<class Table, class slot_type>
        class ConstForwardIterator : public meta::detail::IteratorBase<slot_type> {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = typename std::remove_cv<typename slot_type::value_type>::type;
            using pointer    = const value_type*;
            using reference  = const value_type&;

            constexpr ConstForwardIterator() noexcept
                    : meta::detail::IteratorBase<slot_type>(), map_ptr_(
                    nullptr), iterate_cnt(0) {}


            constexpr explicit ConstForwardIterator(slot_type *slot_ptr,
                                               const Table *map_ptr) noexcept:
                    meta::detail::IteratorBase<slot_type>(slot_ptr), map_ptr_(map_ptr),
                    iterate_cnt(0) {}

            reference operator*() const {return this->value_ptr_->value;};

            pointer operator->() const {return std::addressof(this->value_ptr_->value);}

            ConstForwardIterator<Table, slot_type> &operator++() {
                if FPH_UNLIKELY(++iterate_cnt >= map_ptr_->size()) {
                    this->value_ptr_ = nullptr;
                } else {
                    this->value_ptr_ = map_ptr_->GetNextSlotAddress(this->value_ptr_);
                }
                return *this;
            }

            ConstForwardIterator<Table, slot_type> operator++(int) {
                auto ret = *this;
                ++(*this);
                return ret;
            }

            void SetTable(const Table *new_table_ptr) {
                map_ptr_ = new_table_ptr;
            }


        protected:
            const Table *map_ptr_;
            size_t iterate_cnt;


        };

    } // namespace meta::detail

    namespace meta::detail {

        // For Heterogeneous lookup
        // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1690r0.html

        template<typename... Ts> struct make_void { typedef void type;};
        template<typename... Ts> using void_t = typename make_void<Ts...>::type;

        template <class, class = void>
        struct IsTransparent : std::false_type {};
        template <class T>
        struct IsTransparent<T, void_t<typename T::is_transparent>>
                : std::true_type {};

        template <bool is_transparent>
        struct KeyArg {
            // Transparent. Forward `K`.
            template <typename K, typename key_type>
            using type = K;
        };

        template <>
        struct KeyArg<false> {
            // Not transparent. Always use `key_type`.
            template <typename K, typename key_type>
            using type = key_type;
        };

        template<class Policy, class SeedHash, class KeyEqual, class Allocator, class BucketParamType>
        class MetaRawSet {
        public:

            using key_type = typename Policy::key_type;
            using value_type = typename Policy::value_type;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using hasher = SeedHash;
            using key_equal = KeyEqual;
            using allocator_type = Allocator;
            using reference = value_type &;
            using const_reference = const value_type &;
            using pointer = typename std::allocator_traits<Allocator>::pointer;
            using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

#if FPH_ENABLE_ITERATOR

            using iterator = ForwardIterator<MetaRawSet, typename Policy::slot_type>;
            friend iterator;
            using const_iterator = ConstForwardIterator<MetaRawSet, typename Policy::slot_type>;
            friend const_iterator;
#endif

        private:
            using KeyArgImpl = KeyArg<IsTransparent<key_equal>::value
                                      && IsTransparent<hasher>::value>;
        public:
            template <class K>
            using key_arg = typename KeyArgImpl::template type<K, key_type>;

            explicit MetaRawSet(size_type bucket_count, const SeedHash& hash = SeedHash(),
                                   const key_equal& equal = key_equal(),
                                   const Allocator& alloc = Allocator()) :
#if FPH_DY_DUAL_BUCKET_SET
                    p1_(0), p2_(0), p2_plus_1_(0), p2_remain_(0),
                                keys_first_part_ratio_(DEFAULT_KEYS_FIRST_PART_RATIO),
                                buckets_first_part_ratio_(DEFAULT_BUCKETS_FIRST_PART_RATIO),
#else
//                    bucket_mask_(0),
                    bucket_index_policy_{0},
#endif
//                    item_num_mask_(bucket_count - 1U),
                    slot_index_policy_{bucket_count},
                    seed0_(0),
                    seed1_(0), seed2_(0),
                    bucket_p_array_{nullptr},
                    meta_data_(nullptr),
                    slot_(nullptr),
                    param_(nullptr) {

                TableParamAllocator  param_alloc{};
                param_ = param_alloc.allocate(1);
                std::allocator_traits<TableParamAllocator>::construct(param_alloc, param_,
#if FPH_DY_DUAL_BUCKET_SET
                        ,DEFAULT_KEYS_FIRST_PART_RATIO, DEFAULT_BUCKETS_FIRST_PART_RATIO,
#endif
                                                                      DEFAULT_MAX_LOAD_FACTOR, DEFAULT_BITS_PER_KEY, alloc
                );
                param_->item_num_ceil_ = meta::detail::Ceil2(std::max(bucket_count, size_type(4U)));
//                item_num_mask_ = param_->item_num_ceil_ - 1U;
                slot_index_policy_ = IndexMapPolicy(param_->item_num_ceil_);

                Build<false, false, true>(end(), end(), 0, false, DEFAULT_BITS_PER_KEY,
                                   DEFAULT_KEYS_FIRST_PART_RATIO,
                                   DEFAULT_BUCKETS_FIRST_PART_RATIO);
                (void)hash;
                (void)equal;
            }

            MetaRawSet(): MetaRawSet(DEFAULT_INIT_ITEM_NUM_CEIL) {}

            MetaRawSet(size_type bucket_count, const Allocator& alloc):
                    MetaRawSet(bucket_count, SeedHash(), key_equal(), alloc) {}

            MetaRawSet(size_type bucket_count, const SeedHash& seed_hash, const Allocator& alloc):
                    MetaRawSet(bucket_count, seed_hash, key_equal(), alloc) {}

            explicit MetaRawSet(const Allocator& alloc):
                    MetaRawSet(DEFAULT_INIT_ITEM_NUM_CEIL, SeedHash(), key_equal(), alloc) {}

            template< class InputIt >
            MetaRawSet(InputIt first, InputIt last,
                          size_type bucket_count = DEFAULT_INIT_ITEM_NUM_CEIL,
                          const SeedHash& hash = SeedHash(),
                          const key_equal& equal = key_equal(),
                          const Allocator& alloc = Allocator()):
                    MetaRawSet(std::max(size_type(std::distance(first, last)), bucket_count), hash, equal, alloc) {
                insert(first, last);
            }

            template< class InputIt >
            MetaRawSet(InputIt first, InputIt last,
                          size_type bucket_count, const Allocator& alloc ):
                    MetaRawSet(first, last, bucket_count, SeedHash(), key_equal(), alloc) {}

            template< class InputIt >
            MetaRawSet(InputIt first, InputIt last, size_type bucket_count,
                          const SeedHash& hash, const Allocator& alloc ):
                    MetaRawSet(first, last, bucket_count, hash, key_equal(), alloc) {}

            MetaRawSet(const MetaRawSet &other, const Allocator& alloc):
#if FPH_DY_DUAL_BUCKET_SET
                    p1_(other.p1_), p2(other.p2_), p2_plus_1_(other.p2_plus_1_), p2_remain_(other.p2_remain_),
                keys_first_part_ratio_(other.keys_first_part_ratio_),
                buckets_first_part_ratio_(other.buckets_first_part_ratio_),
#else
//                    bucket_mask_(other.bucket_mask_),
                    bucket_index_policy_(other.bucket_index_policy_),
#endif
//                    item_num_mask_(other.item_num_mask_),
                    slot_index_policy_(other.slot_index_policy_),
                    seed0_(other.seed0_),
                    seed1_(other.seed1_),
                    seed2_(other.seed2_),
                    bucket_p_array_(nullptr),
                    meta_data_(nullptr),
                    slot_(nullptr),
                    param_(nullptr)
            {
                if (other.param_ != nullptr) {
                    TableParamAllocator param_alloc{};
                    param_ = param_alloc.allocate(1);
                    std::allocator_traits<TableParamAllocator>::construct(param_alloc, param_,
                                                                          *other.param_, alloc);


                    slot_ = SlotAllocator{}.allocate(param_->slot_capacity_);
                    meta_data_.SetUnderlyingArray(
                            MetaUnderAllocator{}.allocate(param_->meta_under_entry_capacity_));
                    memcpy(meta_data_.data(), other.meta_data_.data(), param_->meta_under_entry_capacity_);


                    bucket_p_array_ = BucketParamAllocator{}.allocate(param_->bucket_capacity_);
                    memcpy(bucket_p_array_, other.bucket_p_array_,
                           sizeof(BucketParamType) * param_->bucket_num_);

//                    KeyAllocator key_alloc;
                    for (size_t i = 0; i < param_->item_num_ceil_; ++i) {
                        if (!other.IsSlotEmpty(i)) {
                            std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                        std::addressof(
                                                                                slot_[i].mutable_value),
                                                                        other.slot_[i].mutable_value);
                        } else {
//                            std::allocator_traits<KeyAllocator>::construct(key_alloc,
//                                                                           std::addressof(
//                                                                                   slot_[i].key),
//                                                                           other.slot_[i].key);
                        }
                    }
                    if (other.param_->begin_it_.value_ptr() != nullptr) {
                        param_->begin_it_ = iterator(
                                slot_ + (other.param_->begin_it_.value_ptr() - other.slot_), this);
                    }
                    for (size_t k = 0; k < param_->bucket_num_; ++k) {
                        auto &temp_bucket = param_->bucket_array_[k];
                        for (size_t i = 0; i < temp_bucket.key_array.size(); ++i) {
                            temp_bucket.key_array[i] = std::addressof(slot_[GetSlotPos(
                                    *(other.param_->bucket_array_[k].key_array[i]))].key);
                        }
                    }
                }
            }

            MetaRawSet(const MetaRawSet &other):
                    MetaRawSet(other,
                                  other.param_ != nullptr ?
                                  std::allocator_traits<Allocator>::select_on_container_copy_construction(other.param_->alloc_)
                                                          : Allocator() ) {}

            MetaRawSet(MetaRawSet&& other) noexcept:
#if FPH_DY_DUAL_BUCKET_SET
                    p1_(std::exchange(other.p1_, 0)),
                p2_(std::exchange(other.p2_, 0)),
                p2_plus_1_(std::exchange(other.p2_plus_1_, 1)),
                p2_remain_(std::exchange(other.p2_remain_, 0)),
                keys_first_part_ratio_(std::exchange(other.keys_first_part_ratio_, DEFAULT_KEYS_FIRST_PART_RATIO)),
                buckets_first_part_ratio_(std::exchange(other.buckets_first_part_ratio_,DEFAULT_BUCKETS_FIRST_PART_RATIO)),

#else
//                    bucket_mask_(std::exchange(other.bucket_mask_, 0)),
                    bucket_index_policy_(std::move(other.bucket_index_policy_)),
#endif
//                    item_num_mask_(std::exchange(other.item_num_mask_, 0)),
                    slot_index_policy_(std::move(other.slot_index_policy_)),
                    seed0_(std::exchange(other.seed0_, 0x3284723912901723ULL)),
                    seed1_(std::exchange(other.seed1_, 0x123456797291071ULL)),
                    seed2_(std::exchange(other.seed2_, 0x832748923732847ULL)),
                    bucket_p_array_(std::exchange(other.bucket_p_array_, nullptr)),
                    meta_data_(std::exchange(other.meta_data_, MetaDataView(nullptr))),
                    slot_(std::exchange(other.slot_, nullptr)),
                    param_(std::exchange(other.param_, nullptr))

            {
                if (param_ != nullptr) {
                    param_->begin_it_.SetTable(this);
                }


            }

            MetaRawSet(MetaRawSet&& other, const Allocator& alloc):
#if FPH_DY_DUAL_BUCKET_SET
                    p1_(std::exchange(other.p1_, 0)),
                    p2_(std::exchange(other.p2_, 0)),
                    p2_plus_1_(std::exchange(other.p2_plus_1_, 1)),
                    p2_remain_(std::exchange(other.p2_remain_, 0)),
                    keys_first_part_ratio_(std::exchange(other.keys_first_part_ratio_, DEFAULT_KEYS_FIRST_PART_RATIO)),
                    buckets_first_part_ratio_(std::exchange(other.buckets_first_part_ratio_,DEFAULT_BUCKETS_FIRST_PART_RATIO)),

#else
//                    bucket_mask_(std::exchange(other.bucket_mask_, 0)),
                    bucket_index_policy_(std::move(other.bucket_index_policy_)),
#endif
                    slot_index_policy_(std::move(other.slot_index_policy_)),

                    seed0_(std::exchange(other.seed0_, 0x3284723912901723ULL)),
                    seed1_(std::exchange(other.seed1_, 0x123456797291071ULL)),
                    seed2_(std::exchange(other.seed2_, 0x832748923732847ULL)),
                    bucket_p_array_(std::exchange(other.bucket_p_array_, nullptr)),

                    meta_data_(std::exchange(other.meta_data_, MetaDataView(nullptr))),
                    slot_(std::exchange(other.slot_, nullptr)),
                    param_(std::exchange(other.param_, nullptr))


            {
                if (param_ != nullptr) {
                    param_->begin_it_.SetTable(this);
                    param_->alloc_ = alloc;
                }
            }

            MetaRawSet(std::initializer_list<value_type> init,
                          size_type bucket_count = DEFAULT_INIT_ITEM_NUM_CEIL,
                          const SeedHash& hash = SeedHash(), const key_equal& equal = key_equal(),
                          const Allocator& alloc = Allocator())
                    : MetaRawSet(init.begin(), init.end(), bucket_count, hash, equal, alloc){}

            MetaRawSet(std::initializer_list<value_type> init,
                          size_type bucket_count, const Allocator& alloc)
                    : MetaRawSet(init.begin(), init.end(), bucket_count, SeedHash(), key_equal(), alloc){}

            MetaRawSet(std::initializer_list<value_type> init, size_type bucket_count,
                          const SeedHash& hash, const Allocator& alloc)
                    : MetaRawSet(init.begin(), init.end(), bucket_count, hash, key_equal(), alloc){}

            MetaRawSet& operator=(const MetaRawSet& other) {
                auto tmp(other);
                this->swap(tmp);
                return *this;
            }

            MetaRawSet& operator=(MetaRawSet&& other) noexcept {
                this->swap(other);
                return *this;
            }

            MetaRawSet& operator=(std::initializer_list<value_type> ilist) {
                clear();
                insert(ilist);
                return *this;
            }

            allocator_type get_allocator() const noexcept {
                return param_->alloc_;
            }

            void swap(MetaRawSet& other) noexcept {
                SwapImp(other);
            }


            template<bool is_rehash, bool called_by_rehash,
                    bool use_move=false, bool last_element_only_has_key=false, class InputIt>
            void Build(InputIt pair_begin, InputIt pair_end, uint64_t seed = 0,
                       bool verbose = false, double c = DEFAULT_BITS_PER_KEY,
                       double keys_first_part_ratio = DEFAULT_KEYS_FIRST_PART_RATIO, double buckets_first_part_ratio = DEFAULT_BUCKETS_FIRST_PART_RATIO,
                       size_t max_try_seed2_time = 1000, size_t max_reseed2_time = 1000) {
                constexpr size_t max_try_seed0_time = 10;
                constexpr size_t max_try_seed1_time = 100;
                BuildImp<is_rehash, called_by_rehash, use_move,
                        last_element_only_has_key>(pair_begin, pair_end, seed,
                                                   verbose, c, keys_first_part_ratio,
                                                   buckets_first_part_ratio,
                                                   max_try_seed0_time,
                                                   max_try_seed1_time, max_try_seed2_time, max_reseed2_time);
            }

            void rehash(size_type count) {

                size_type new_item_ceil_num = meta::detail::Ceil2(
                        size_t(std::ceil(param_->item_num_ / param_->max_load_factor_)));
                if (count > new_item_ceil_num) {
                    new_item_ceil_num = meta::detail::Ceil2(count);
                }
                new_item_ceil_num = std::min(new_item_ceil_num, MAX_ITEM_NUM_CEIL_LIMIT);
                new_item_ceil_num = std::max(new_item_ceil_num, DEFAULT_INIT_ITEM_NUM_CEIL);
                if (new_item_ceil_num != param_->item_num_ceil_) {
                    slot_index_policy_.UpdateBySlotNum(new_item_ceil_num);
//                    item_num_mask_ = new_item_ceil_num - 1;
                    param_->temp_pair_buf_.resize(param_->item_num_ * sizeof(value_type));
                    value_type *temp_value_buf_start = reinterpret_cast<value_type*>(param_->temp_pair_buf_.data());
                    value_type *temp_value_buf = temp_value_buf_start;
                    for (auto it = begin(); it != end(); ++it) {
                        std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                    temp_value_buf++, std::move(*it));
                    }

                    Build<true, true, true>(temp_value_buf_start, temp_value_buf_start + param_->item_num_, seed1_,
#if FPH_DEBUG_FLAG
                            true,
#else
                                      false,
#endif
                                      param_->bits_per_key_,
#if FPH_DY_DUAL_BUCKET_SET
                            keys_first_part_ratio_, buckets_first_part_ratio_
#else
                                      DEFAULT_KEYS_FIRST_PART_RATIO, DEFAULT_BUCKETS_FIRST_PART_RATIO
#endif
                    );
                    for (auto *temp_buf_ptr = temp_value_buf_start; temp_buf_ptr != temp_value_buf_start + param_->item_num_; temp_buf_ptr++) {
                        std::allocator_traits<Allocator>::destroy(param_->alloc_, temp_buf_ptr);
                    }
                    param_->temp_pair_buf_.clear();
                    param_->temp_pair_buf_.shrink_to_fit();
                }


            }

            void reserve(size_type count) {
                rehash(std::ceil(count / param_->max_load_factor_));
            }

            void max_load_factor(float ml) {
                if (ml > 0.0 && ml < 1.0) {
                    param_->max_load_factor_ = ml;
                    param_->should_expand_item_num_ = std::ceil(param_->item_num_ceil_ * ml);
                }
            }

            float max_load_factor() const {
                return param_->max_load_factor_;
            }

            static float max_load_factor_upper_limit() {
                return MAX_LOAD_FACTOR_UPPER_LIMIT;
            }

            float load_factor() const {
                return static_cast<double>(param_->item_num_) / param_->item_num_ceil_;
            }

            hasher hash_function() const {
                return hash_;
            }

            key_equal key_eq() const {
                return key_equal_;
            }


#if FPH_ENABLE_ITERATOR

            std::pair<iterator, bool> insert(const value_type &value) {
                return InsertImp(value);

            } // insert

            std::pair<iterator, bool> insert(value_type &&value) {
                return InsertImp(std::move(value));
            } // insert

            iterator insert(const_iterator, const value_type &value) {
                return insert(value).first;
            }

            iterator insert(const_iterator, value_type &&value) {
                return insert(std::move(value)).first;
            }

            template <class InputIt>
            void insert(InputIt first, InputIt last) {
                for (; first != last; ++first) emplace(*first);
            }

            template <class InputIt>
            void InsertNoDuplicated(InputIt first, InputIt last) {
                if (param_->item_num_ != 0) {
                    for (; first != last; ++first) emplace(*first);
                }
                else {
                    Build<false, false, false>(first, last, seed1_, false, param_->bits_per_key_);
                }
            }

            void insert(std::initializer_list<value_type> ilist) {
                insert(ilist.begin(), ilist.end());
            }

            iterator erase(iterator iter) {
                return EraseImp(iter);
            }

            iterator erase(const_iterator iter) {
                return EraseImp(ConstIteratorToIterator(iter));
            }



            iterator erase(const_iterator first, const_iterator last) {
                return EraseImp(first, last);
            }

            size_type erase(const key_type& key) {
                return EraseImp(key);
            }




            template<class... Args>
            std::pair<iterator, bool> emplace(Args&&... args) {
                return EmplaceImp(std::forward<Args>(args)...);
            }

            template <class... Args>
            iterator emplace_hint(const_iterator, Args&&... args ) {
                return emplace(std::forward<Args>(args)...).first;
            }

            iterator begin() noexcept {
                if FPH_UNLIKELY(param_ == nullptr) {
                    return iterator(nullptr, nullptr);
                }
                return param_->begin_it_;
            }

            const_iterator begin() const noexcept {
                if FPH_UNLIKELY(param_ == nullptr) {
                    return const_iterator(nullptr, nullptr);
                }
                return const_iterator( param_->begin_it_.value_ptr() , this);
            }

            const_iterator cbegin() const noexcept {
                return begin();
            }

            constexpr iterator end() noexcept {
                return iterator(nullptr, nullptr);
            }

            constexpr const_iterator end() const noexcept {
                return const_iterator(nullptr, nullptr);
            }

            constexpr const_iterator cend() const noexcept {
                return end();
            }

            template<class K = key_type>
            FPH_ALWAYS_INLINE iterator find(const key_arg<K>&
                    FPH_RESTRICT key) FPH_FUNC_RESTRICT noexcept {
                auto seed0_hash = hash_(key, seed0_);
                auto seed1_hash = MidHash(seed0_hash, seed1_);
                auto slot_pos = GetSlotPosBySeed0And1Hash(seed0_hash, seed1_hash);
                slot_type *pair_address = slot_ + slot_pos;
#if defined(__APPLE__) && defined(__aarch64__)
                // according to benchmark, Apple Silicon chips can probably benefit from prefetch
                FPH_PREFETCH(pair_address, 0, 1);
#endif
                if (MayEqual(slot_pos, seed1_hash)) {
                    if FPH_LIKELY(key_equal_(pair_address->key, key)) {
                        return iterator(pair_address, this);
                    }
                }
                return end();
            }

            template<class K = key_type>
            FPH_ALWAYS_INLINE const_iterator find(const key_arg<K>&
                    FPH_RESTRICT key) const FPH_FUNC_RESTRICT noexcept {
                auto seed0_hash = hash_(key, seed0_);
                auto seed1_hash = MidHash(seed0_hash, seed1_);
                auto slot_pos = GetSlotPosBySeed0And1Hash(seed0_hash, seed1_hash);
                slot_type *pair_address = slot_ + slot_pos;
#if defined(__APPLE__) && defined(__aarch64__)
                // according to benchmark, Apple Silicon chips can probably benefit from prefetch
                FPH_PREFETCH(pair_address, 0, 1);
#endif
                if (MayEqual(slot_pos, seed1_hash)) {
                    if FPH_LIKELY(key_equal_(pair_address->key, key)) {
                        return const_iterator(pair_address, this);
                    }
                }
                return end();
            }


#endif

            size_type size() const noexcept {
                if FPH_UNLIKELY(param_ == nullptr) {
                    return 0;
                }
                return param_->item_num_;
            }

            bool empty() const noexcept {
                if FPH_UNLIKELY(param_ == nullptr) {
                    return true;
                }
                return param_->item_num_ == 0U;
            }

            constexpr size_type max_size() const noexcept {
                return MAX_ITEM_NUM_CEIL_LIMIT;
            }



            void clear() noexcept {
                if (param_ == nullptr) {
                    TableParamAllocator param_alloc{};
                    param_ = param_alloc.allocate(1);
                    std::allocator_traits<TableParamAllocator>::construct(param_alloc, param_,
#if FPH_DY_DUAL_BUCKET_SET
                            ,DEFAULT_KEYS_FIRST_PART_RATIO, DEFAULT_BUCKETS_FIRST_PART_RATIO,
#endif
                                                                          DEFAULT_MAX_LOAD_FACTOR, DEFAULT_BITS_PER_KEY, Allocator()
                    );
                }
                param_->item_num_ = 0;
                if (param_->item_num_ceil_ > 0) {
                    slot_index_policy_.UpdateBySlotNum(param_->item_num_ceil_);
//                    item_num_mask_ = param_->item_num_ceil_ - 1U;
                }
                else {
                    slot_index_policy_.UpdateBySlotNum(0);
//                    item_num_mask_ = 0;
                }
#if FPH_ENABLE_ITERATOR
                param_->begin_it_ = iterator(nullptr, nullptr);
#endif

                if (slot_ != nullptr) {
                    DestroySlots();
                }
                if (meta_data_.data() != nullptr) {
                    auto meta_under_num = MetaDataView::GetUnderlyingEntryNum(param_->item_num_ceil_);
                    memset(meta_data_.data(), 0, sizeof(MetaUnderEntry) * meta_under_num);
                }

                param_->filled_count_ = 0;
                for (size_t i = 0; i < param_->bucket_num_; ++i ) {
                    auto &temp_bucket = param_->bucket_array_[i];
                    temp_bucket.key_array.clear();
                    temp_bucket.entry_cnt = 0;
                }
            }

            template<class K = key_type>
            size_t count(const key_arg<K> &key) const {
                auto seed0_hash = hash_(key, seed0_);
                auto seed1_hash = MidHash(seed0_hash, seed1_);
                auto slot_pos = GetSlotPosBySeed0And1Hash(seed0_hash, seed1_hash);
                if (MayEqual(slot_pos, seed1_hash) && key_equal_(slot_[slot_pos].key, key)) {
                    return 1U;
                }
                return 0;
            }

            template<class K = key_type>
            bool contains(const key_arg<K>& key) const {
                return count(key) == 1U;
            }

            bool HasElement(const value_type& ele) const{
                return contains(slot_type::GetSlotAddressByValueAddress(std::addressof(ele))->key);
            }

            template<class K = key_type>
            std::pair<iterator,iterator> equal_range( const key_arg<K>& key ) {
                auto it = find(key);
                if (it != end()) {
                    return {it, std::next(it)};
                }
                return {it, it};
            }

            template<class K = key_type>
            std::pair<const_iterator,const_iterator> equal_range( const key_arg<K>& key ) const {
                auto it = find(key);
                if (it != end()) {return {it, std::next(it)};}
                return {it, it};
            }

            size_type bucket_count() const {
                return param_->item_num_ceil_;
            }

            size_type max_bucket_count() const {
                return MAX_ITEM_NUM_CEIL_LIMIT;
            }

            friend bool operator==(const MetaRawSet&a, const MetaRawSet&b) {
                if (a.size() != b.size()) return false;
                const auto* a_ptr = &a;
                const auto* b_ptr = &b;
                // small capacity is faster in iterating
                if (a_ptr->capacity() > b_ptr->capacity()) {
                    std::swap(a_ptr, b_ptr);
                }
                for (const auto& ele: *a_ptr) {
                    if (!b_ptr->HasElement(ele)) {
                        return false;
                    }
                }
                return true;
            }

            friend bool operator!=(const MetaRawSet&a, const MetaRawSet&b) {
                return !(a == b);
            }

            friend void swap(MetaRawSet &a, MetaRawSet &b) noexcept {
                a.swap(b);
            }

            /**
             * Get the pointer of the value without checking whether the two keys are equal.
             * Only use this function when you are sure that the key is in the table and you don't
             * want to waste cpu cycles in comparing the keys.
             * @param key
             * @return
             */
            template<class K = key_type>
            FPH_ALWAYS_INLINE pointer GetPointerNoCheck(const key_arg<K>&
                    FPH_RESTRICT key)
            FPH_FUNC_RESTRICT noexcept {
                size_t pos = GetSlotPos(key);
                return std::addressof(slot_[pos].value);
            }

            template<class K = key_type>
            FPH_ALWAYS_INLINE const_pointer GetPointerNoCheck(const key_arg<K>&
                    FPH_RESTRICT key)
            const FPH_FUNC_RESTRICT noexcept {
                size_t pos = GetSlotPos(key);
                return std::addressof(slot_[pos].value);
            }


            /**
             * Get the position in the underlying slot of one key
             * @param key
             * @return
             */
            template<class K = key_type>
            FPH_ALWAYS_INLINE size_t GetSlotPos(const key_arg<K> &key)
                    const FPH_FUNC_RESTRICT noexcept {
                auto k_seed0_hash = hash_(key, seed0_);
                size_t bucket_index = GetBucketIndex(k_seed0_hash);
                auto bucket_param = bucket_p_array_[bucket_index];
                auto temp_offset = bucket_param >> 1U;
                auto optional_bit = bucket_param & 0x1U;


                auto temp_hash_value = MidHash(k_seed0_hash, MixSeedAndBit(seed2_, optional_bit));
//                auto temp_hash_value = hash_(key, seed2_ + optional_bit);

                auto reverse_offset = slot_index_policy_.ReverseMap(temp_offset);
                auto slot_pos = slot_index_policy_.MapToIndex(temp_hash_value + reverse_offset);
//                auto slot_pos = (temp_hash_value + temp_offset) & item_num_mask_;
                return slot_pos;
            }

            FPH_ALWAYS_INLINE size_t GetSlotPosBySeed0Hash(size_t k_seed0_hash) const FPH_FUNC_RESTRICT noexcept {
//                auto k_seed0_hash = hash_(key, seed0_);
                size_t bucket_index = GetBucketIndex(k_seed0_hash);
                auto bucket_param = bucket_p_array_[bucket_index];
                auto temp_offset = bucket_param >> 1U;
                auto optional_bit = bucket_param & 0x1U;


                auto temp_hash_value = MidHash(k_seed0_hash, MixSeedAndBit(seed2_, optional_bit));
//                auto temp_hash_value = hash_(key, seed2_ + optional_bit);

                auto reverse_offset = slot_index_policy_.ReverseMap(temp_offset);
                auto slot_pos = slot_index_policy_.MapToIndex(temp_hash_value + reverse_offset);
//                auto slot_pos = (temp_hash_value + temp_offset) & item_num_mask_;
                return slot_pos;
            }

            FPH_ALWAYS_INLINE size_t GetSlotPosBySeed0And1Hash(size_t k_seed0_hash, size_t k_seed1_hash)
                const FPH_FUNC_RESTRICT noexcept {
                size_t bucket_index = GetBucketIndexBySeed1Hash(k_seed1_hash);
                auto bucket_param = bucket_p_array_[bucket_index];
                auto temp_offset = bucket_param >> 1U;
                auto optional_bit = bucket_param & 0x1U;
                auto temp_hash_value = MidHash(k_seed0_hash, MixSeedAndBit(seed2_, optional_bit));
                auto reverse_offset = slot_index_policy_.ReverseMap(temp_offset);
                auto slot_pos = slot_index_policy_.MapToIndex(temp_hash_value + reverse_offset);
                return slot_pos;
            }

            template<class K = key_type>
            FPH_ALWAYS_INLINE size_t GetSlotPos(const key_arg<K> &key,
                                            size_t offset, size_t optional_bit)
            const FPH_FUNC_RESTRICT noexcept {
                auto k_seed0_hash = hash_(key, seed0_);
                auto temp_hash_value = MidHash(k_seed0_hash, MixSeedAndBit(seed2_, optional_bit));
                //  auto temp_hash_value = (hash_(key, seed2_ + optional_bit));
                size_t reverse_offset = slot_index_policy_.ReverseMap(offset);
                auto slot_pos = slot_index_policy_.MapToIndex(temp_hash_value + reverse_offset);
//                auto slot_pos = (temp_hash_value + offset) & item_num_mask_;
                return slot_pos;
            }

            ~MetaRawSet() {
                if (slot_ != nullptr) {
                    DestroySlots();
                    SlotAllocator{}.deallocate(slot_, param_->slot_capacity_);
                    slot_ = nullptr;
                }

                if (meta_data_.data() != nullptr) {
                    MetaUnderAllocator{}.deallocate(meta_data_.data(), param_->meta_under_entry_capacity_);
                    meta_data_.SetUnderlyingArray(nullptr);
                }

                if (bucket_p_array_ != nullptr) {
                    BucketParamAllocator{}.deallocate(bucket_p_array_, param_->bucket_capacity_);
                    bucket_p_array_ = nullptr;
                }

                if (param_ != nullptr) {

                    TableParamAllocator param_alloc{};
                    std::allocator_traits<TableParamAllocator>::destroy(param_alloc, param_);
                    param_alloc.deallocate(param_, 1);
                    param_ = nullptr;
                }
            }

#if FPH_DEBUG_ERROR



            void PrintTableParams() {
                fprintf(stderr, "Fph table item_num: %lu, item_num_ceil: %lu, seed1: %lu, seed2: %lu,"
                                "bucket_num: %lu, "
                                //                                "default_fill_key: %s, second_default_key: %s, "
                                "random init seed: %lu\n",
                        param_->item_num_, param_->item_num_ceil_, seed1_, seed2_,
                        param_->bucket_num_,
//                                detail::ToString(*param_->default_fill_key_).c_str(),
//                                detail::ToString(*param_->second_default_key_).c_str(),
                        param_->key_gen_->init_seed);
            }
#endif



        protected:

            static_assert(std::is_unsigned<BucketParamType>::value,
                          "BucketParamType should be unsigned type");




#if FPH_DY_DUAL_BUCKET_SET
            // p2_remain_ + p2 == bucket_num_
            size_t p1_, p2_, p2_plus_1_, p2_remain_; // direct
#else
            using IndexMapPolicy = typename Policy::index_map_policy;
            IndexMapPolicy bucket_index_policy_;
//            size_t bucket_mask_; // direct
#endif

            // item_num_mask_ is no less than item_num_ and is in the format of (1UL << n) - 1UL
//            size_t item_num_mask_; // direct
            IndexMapPolicy slot_index_policy_;

            size_t seed0_;
            size_t seed1_, seed2_; // direct

            using BucketParamAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<BucketParamType>;
            using BucketParamVector = std::vector<BucketParamType, BucketParamAllocator>;
            BucketParamType *bucket_p_array_; // direct

            using MetaUnderEntry = uint8_t;
            constexpr static size_t META_ITEM_BIT_SIZE = 8U;
            using MetaDataView = BitArrayView<MetaUnderEntry, META_ITEM_BIT_SIZE>;
            using MetaUnderAllocator =
                typename std::allocator_traits<Allocator>::template rebind_alloc<MetaUnderEntry>;
            MetaDataView meta_data_;


            using slot_type = typename Policy::slot_type;
            slot_type *slot_ = nullptr; // direct

            using SlotAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<slot_type>;
            using KeyPointerAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<const key_type *>;

            using BoolAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<bool>;
            using SizeTAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<size_t>;
            using KeyAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<key_type>;
            using CharAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<char>;

            using BucketType = detail::FphBucket<key_type, KeyPointerAllocator,
                    BucketParamType>;
            using BucketAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<BucketType>;

            using SizeTVector = std::vector<size_t, SizeTAllocator>;
            using CharVector = std::vector<char, CharAllocator>;

            // all the member variables that are used in non-look-up operations will be put in the
            // FphTableParam, so all the lookup operations will not use dereference of pointer
            struct FphTableParam {

                FphTableParam(
#if FPH_DY_DUAL_BUCKET_SET
                        double keys_first_part_ratio, double buckets_first_part_ratio,
#endif
                        float max_load_factor,
                        float bits_per_key,
                        const Allocator& alloc

                ): item_num_(0), item_num_ceil_(0),
                   bucket_num_(0), slot_capacity_(0), bucket_capacity_(0), meta_under_entry_capacity_(0),
#if FPH_DY_DUAL_BUCKET_SET
                        keys_first_part_ratio_(keys_first_part_ratio),
                                buckets_first_part_ratio_(buckets_first_part_ratio),
#endif
                   should_expand_item_num_(0),
                   filled_count_(0),
                   alloc_{alloc},
                   max_load_factor_(max_load_factor),
                   bits_per_key_(bits_per_key),
                   begin_it_{nullptr, nullptr},
                   seed2_test_table_{},
                   tested_hash_vec_{},
                   random_table_{},
                   map_table_{},
                   bucket_array_{},
                   temp_byte_buf_vec_{},
                   temp_pair_buf_{}
                {}

                FphTableParam(const FphTableParam& o, const Allocator& alloc) : item_num_(o.item_num_),
                                                                                item_num_ceil_(o.item_num_ceil_),
                                                                                bucket_num_(o.bucket_num_),
                                                                                slot_capacity_(o.slot_capacity_),
                                                                                bucket_capacity_(o.bucket_capacity_),
                                                                                meta_under_entry_capacity_(o.meta_under_entry_capacity_),
#if FPH_DY_DUAL_BUCKET_SET
                        keys_first_part_ratio_(o.keys_first_part_ratio_),
                        buckets_first_part_ratio_(o.buckets_first_part_ratio_),
#endif
                                                                                should_expand_item_num_(o.should_expand_item_num_),
                                                                                filled_count_(o.filled_count_),
                                                                                alloc_(alloc),
                                                                                max_load_factor_(o.max_load_factor_),
                                                                                bits_per_key_(o.bits_per_key_),
                                                                                begin_it_(nullptr, nullptr),
                                                                                seed2_test_table_(o.seed2_test_table_),
                                                                                tested_hash_vec_(o.tested_hash_vec_),
                                                                                random_table_(o.random_table_),
                                                                                map_table_(o.map_table_),
                                                                                bucket_array_(o.bucket_array_),
                                                                                temp_byte_buf_vec_(o.temp_byte_buf_vec_),
                                                                                temp_pair_buf_(o.temp_pair_buf_) {
                }

                FphTableParam(const FphTableParam& o):
                        FphTableParam(o, std::allocator_traits<Allocator>::select_on_container_copy_construction(o.alloc_)) {}


                ~FphTableParam() {
//                    KeyAllocator key_alloc{};
                }


                // the number of items
                size_t item_num_;
                // max item num for current capacity, power of 2, equal to item_num_mask_ + 1
                size_t item_num_ceil_;

                size_t bucket_num_;
                size_t slot_capacity_;
                size_t bucket_capacity_;

                // number of the meta data underlying entry
                size_t meta_under_entry_capacity_;

#if FPH_DY_DUAL_BUCKET_SET
                double keys_first_part_ratio_;
                double buckets_first_part_ratio_;
#endif

                // equals to item_num_ceil * max_load_factor
                size_t should_expand_item_num_;

                size_t filled_count_;

                Allocator alloc_;


                float max_load_factor_;
                float bits_per_key_;

#if FPH_ENABLE_ITERATOR
                iterator begin_it_;
#endif

                std::vector<bool, BoolAllocator> seed2_test_table_;
                SizeTVector tested_hash_vec_;

                BucketParamVector random_table_;
                BucketParamVector map_table_;
                std::vector<BucketType, BucketAllocator> bucket_array_;
//                BucketType *bucket_array_;
                // TODO: may use pointer to replace vector to save space

                CharVector temp_byte_buf_vec_;
                // buffer for rehash
                CharVector temp_pair_buf_;

            }; // struct FphTableParam

            FphTableParam *param_;

            using TableParamAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<FphTableParam>;


            constexpr static double DEFAULT_KEYS_FIRST_PART_RATIO = 0.5;
            constexpr static double DEFAULT_BUCKETS_FIRST_PART_RATIO = 0.3;

            static constexpr SeedHash hash_{}; // direct

            static constexpr KeyEqual key_equal_{}; // direct

            constexpr static double DEFAULT_MAX_LOAD_FACTOR = 0.6;
            constexpr static double MAX_LOAD_FACTOR_UPPER_LIMIT = 0.98;
            constexpr static double DEFAULT_BITS_PER_KEY = 2.0;
            constexpr static size_t DEFAULT_INIT_ITEM_NUM_CEIL = 8;
            constexpr static size_t DEFAULT_INIT_ITEM_NUM_MASK = 7;
            static_assert(DEFAULT_INIT_ITEM_NUM_CEIL == DEFAULT_INIT_ITEM_NUM_MASK + 1U);


            static constexpr size_t bucket_param_type_num_bits_ =
                    std::numeric_limits<BucketParamType>::digits - 1;
            static constexpr BucketParamType BUCKET_PARAM_MASK = meta::detail::GenBitMask<BucketParamType>(
                    bucket_param_type_num_bits_);
            static constexpr size_t MAX_ITEM_NUM_CEIL_LIMIT =
                    size_t(1U) << bucket_param_type_num_bits_;
            static_assert(BUCKET_PARAM_MASK + 1U == MAX_ITEM_NUM_CEIL_LIMIT);
            static_assert(DEFAULT_INIT_ITEM_NUM_CEIL <= MAX_ITEM_NUM_CEIL_LIMIT);

            iterator ConstIteratorToIterator(const_iterator const_it) {
                return iterator(const_it.value_ptr(), this);
            }


            void SwapImp(MetaRawSet &o) noexcept {
                using std::swap;
                swap(slot_index_policy_, o.slot_index_policy_);
                swap(meta_data_, o.meta_data_);
                swap(slot_, o.slot_);
                swap(param_, o.param_);
#if FPH_DY_DUAL_BUCKET_SET
                swap(p1_, o.p1_); swap(p2_, o.p2_);
                swap(p2_plus_1_, o.p2_plus_1_);
                swap(p2_remain_, o.p2_remain_);
#else
                swap(bucket_index_policy_, o.bucket_index_policy_);
#endif
                if (param_ != nullptr) {
                    param_->begin_it_.SetTable(this);
                }
                if (o.param_ != nullptr) {
                    o.param_->begin_it_.SetTable(std::addressof(o));
                }
                swap(seed0_, o.seed0_);
                swap(seed1_, o.seed1_);
                swap(seed2_, o.seed2_);
                swap(bucket_p_array_, o.bucket_p_array_);

            }

            FPH_ALWAYS_INLINE size_t GetBucketIndex(size_t k_seed0_hash) const FPH_FUNC_RESTRICT noexcept {
                size_t temp_hash1 = MidHash(k_seed0_hash, seed1_);
#if FPH_DY_DUAL_BUCKET_SET
                size_t temp_value = temp_hash1 & item_num_mask_;
                size_t ret = temp_value < p1_ ? (temp_hash1 & p2_) : p2_plus_1_ + (temp_hash1 & p2_remain_);
#else
//                size_t ret = temp_hash1 & bucket_mask_;
                size_t ret = bucket_index_policy_.MapToIndex(temp_hash1);
#endif
                return ret;
            }

            FPH_ALWAYS_INLINE size_t GetBucketIndexBySeed1Hash(size_t k_seed1_hash) const FPH_FUNC_RESTRICT noexcept {
                size_t ret = bucket_index_policy_.MapToIndex(k_seed1_hash);
                return ret;
            }

            FPH_ALWAYS_INLINE size_t CompleteGetBucketIndex(const key_type& FPH_RESTRICT key) const FPH_FUNC_RESTRICT noexcept {
                auto k_seed0_hash = hash_(key, seed0_);
                size_t temp_hash1 = MidHash(k_seed0_hash, seed1_);
#if FPH_DY_DUAL_BUCKET_SET
                size_t temp_value = temp_hash1 & item_num_mask_;
                size_t ret = temp_value < p1_ ? (temp_hash1 & p2_) : p2_plus_1_ + (temp_hash1 & p2_remain_);
#else
                size_t ret = bucket_index_policy_.MapToIndex(temp_hash1);
#endif
                return ret;
            }

            FPH_ALWAYS_INLINE bool MayEqual(size_t slot_pos, size_t seed1_hash) const FPH_FUNC_RESTRICT noexcept {
                auto hash_v = seed1_hash;
                auto meta_v = meta_data_.get(slot_pos);
                constexpr uint32_t keep_bit_offset = META_ITEM_BIT_SIZE - 1U;
                constexpr MetaUnderEntry meta_hash_mask = GenBitMask<MetaUnderEntry>(keep_bit_offset);
                // not empty and part of the hash is equal
                bool ret = ((meta_v >> keep_bit_offset) != 0U)
                        & (PartHash(hash_v) == (meta_v & meta_hash_mask));
                return ret;
            }

            bool IsSlotEmpty(size_t pos) const FPH_FUNC_RESTRICT {
                auto meta_v = meta_data_.get(pos);
                constexpr uint32_t offset = META_ITEM_BIT_SIZE - 1U;
                bool ret = !(meta_v >> offset);
                return ret;
            }

            bool IsSlotEmpty(const slot_type* FPH_RESTRICT slot_ptr) const FPH_FUNC_RESTRICT {
                auto slot_pos = slot_ptr - slot_;
                return IsSlotEmpty(slot_pos);
            }

            void MarkSlotEmpty(size_t pos) FPH_FUNC_RESTRICT {
                meta_data_.set(pos, 0U);
            }


            slot_type *GetNextSlotAddress(const slot_type* FPH_RESTRICT pair_ptr) const FPH_FUNC_RESTRICT {
                const auto *slot_end = slot_ + param_->item_num_ceil_;
                for (size_t slot_dis = 1; slot_dis < param_->item_num_ceil_; ++slot_dis) {
                    // TODO: test whether using branch-less will be faster
//                    pair_ptr = slot_ + (++pair_ptr - slot_) & item_num_ceil_;
                    if FPH_UNLIKELY(++pair_ptr >= slot_end) {
                        pair_ptr = slot_;
                    }
                    if (!IsSlotEmpty(pair_ptr)) {
                        return const_cast<slot_type*>(pair_ptr);
                    }
                }
                return nullptr;
            }

            size_t GetNextSlotPos(size_t now_pos) const FPH_FUNC_RESTRICT {

                size_t new_pos = now_pos;
                for (size_t slot_dis = 1; slot_dis < param_->item_num_ceil_; ++slot_dis) {
//                    new_pos = (++new_pos) & item_num_mask_;
                    ++new_pos;
                    if FPH_UNLIKELY(new_pos >= param_->item_num_ceil_) {
                        new_pos = 0;
                    }
                    if (!IsSlotEmpty(new_pos)) {
                        return new_pos;
                    }
                }
                return std::numeric_limits<size_t>::max();

            }

            FPH_ALWAYS_INLINE static size_t MixSeedAndBit(size_t seed, uint32_t optional_bit) {
                return seed + optional_bit;
            }

            static FPH_ALWAYS_INLINE size_t MixValue(size_t hash_value, size_t seed) {
                return hash_value * seed;
            }

            static FPH_ALWAYS_INLINE size_t MidHash(size_t hash_k_seed0, size_t seed) {
                return MixValue(hash_k_seed0, seed);
            }

            FPH_ALWAYS_INLINE size_t CompleteHash(const key_type& FPH_RESTRICT key, size_t seed) const FPH_FUNC_RESTRICT noexcept {
                auto hash_k_seed0 = hash_(key, seed0_);
                return MixValue(hash_k_seed0, seed);
            }

#if FPH_ENABLE_ITERATOR

            // change begin()
            void AddNewIterator(slot_type *address) {
                param_->begin_it_ = iterator(address, this);
            }

#endif


            /**
             * Test whether there is collision in this bucket
             * @tparam Bucket
             * @tparam SeedTestTable make sure this is all zero, this will still be all zero after call
             * @tparam TestedHashVec make sure this is empty, this will still be empty after call
             * @param testing_bucket
             * @param seed2_test_table
             * @param tested_hash_vec
             * @param seed
             * @return true if pass the test
             */
            template<class Bucket, class SeedTestTable, class TestedHashVec>
            bool
            TestBucketSelfCollision(const Bucket &testing_bucket, SeedTestTable &seed2_test_table,
                                    TestedHashVec &tested_hash_vec, size_t seed) {
                bool test_pass_flag = true;
                assert(tested_hash_vec.empty());
                for (const key_type *key_ptr: testing_bucket.key_array) {
                    // TODO: test whether test optional bit will accelerate the construction
//                    auto temp_hash_value = hash_(*key_ptr, seed) & item_num_mask_;
                    auto temp_hash_value = slot_index_policy_.MapToIndex(CompleteHash(*key_ptr, seed));
//                    auto temp_hash_value = slot_index_policy_.MapToIndex(hash_(*key_ptr, seed));
                    if FPH_UNLIKELY(seed2_test_table[temp_hash_value]) {
                        test_pass_flag = false;
                        break;
                    }
                    seed2_test_table[temp_hash_value] = true;
                    tested_hash_vec.push_back(temp_hash_value);
                }
                while (!tested_hash_vec.empty()) {
                    seed2_test_table[tested_hash_vec.back()] = false;
                    tested_hash_vec.pop_back();
                }
                return test_pass_flag;
            }

            /**
             * Test whether there is collision in this hash value vector
             * @tparam HashVec
             * @tparam SeedTestTable make sure this is all zero, this will still be all zero after call
             * @tparam TestedHashVec make sure this is empty, this will still be empty after call
             * @param hash_vec
             * @param seed2_test_table
             * @param tested_hash_vec
             * @return true if pass the test
             */
            template<class HashVec, class SeedTestTable, class TestedHashVec>
            bool TestHashVecSelfCollision(const HashVec &hash_vec, SeedTestTable &seed2_test_table,
                                          TestedHashVec &tested_hash_vec) {
                tested_hash_vec.clear();
                bool test_pass_flag = true;
                for (auto temp_hash: hash_vec) {
                    auto temp_pos = slot_index_policy_.MapToIndex(temp_hash);
                    if FPH_UNLIKELY(seed2_test_table[temp_pos]) {
                        test_pass_flag = false;
                        break;
                    }
                    seed2_test_table[temp_pos] = true;
                    tested_hash_vec.push_back(temp_pos);
                }
                for (auto hash_value: tested_hash_vec) {
                    seed2_test_table[hash_value] = false;
                }
                tested_hash_vec.clear();
                return test_pass_flag;
            }

            template<class RandomTable, class MapTable>
            static bool
            IsRandomTableValid(const RandomTable &random_table, const MapTable &map_table) {
                for (size_t i = 0; i < map_table.size(); ++i) {
                    if (random_table[map_table[i]] != i) {
                        return false;
                    }
                }
                return true;
            }

            template<class BucketIt, class TestTable>
            bool TestBucketsCollision(BucketIt bucket_begin, BucketIt bucket_end,
                                      TestTable &test_table) {
                bool test_passed_flag = true;
                std::fill(test_table.begin(), test_table.end(), false);
                for (auto bucket_it = bucket_begin; bucket_it != bucket_end; ++bucket_it) {
                    auto &test_bucket = *bucket_it;
                    for (const auto *key_ptr: test_bucket.key_array) {
                        const auto &temp_key = *key_ptr;
                        auto slot_pos = GetSlotPos(temp_key);
                        if (test_table[slot_pos]) {
                            test_passed_flag = false;
                            break;
                        }
                        test_table[slot_pos] = true;
                    }
                    if (!test_passed_flag) {
                        break;
                    }
                }
                std::fill(test_table.begin(), test_table.end(), false);
                return test_passed_flag;
            }

            template<class K, class... Args>
            std::pair<iterator, bool> TryEmplaceImp(K&& key, Args&&... args) {
                auto [slot_address, alloc_happen] = FindOrAlloc(key);
                if (alloc_happen) {
                    std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                std::addressof(slot_address->mutable_value),
                                                                std::piecewise_construct,
                                                                std::forward_as_tuple(std::forward<K>(key)),
                                                                std::forward_as_tuple(std::forward<Args>(args)...));
                }
                return {iterator(slot_address, this), alloc_happen};
            }

            template<int K, typename... Ts> using KthTypeOf =
            typename std::tuple_element<K, std::tuple<Ts...>>::type;

            template <int K, class... Ts>
            constexpr static auto GetTupleElement(Ts&&... ts) {
                return std::get<K>(std::forward_as_tuple(ts...));
            }


            template < std::size_t... Ns , typename... Ts >
            static auto TupleTailImp( std::index_sequence<Ns...> , std::tuple<Ts...> t )
            {
                return std::make_tuple( std::get<Ns + 1u>(t)... );
            }

            template < typename... Ts >
            static auto TupleTail( std::tuple<Ts...> t )
            {
                return  TupleTailImp( std::make_index_sequence<sizeof...(Ts) - 1u>() , t );
            }

            template<typename T>
            using base_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

            template<typename T1, typename T2, typename = void>
            struct is_base_same : std::false_type {};

            template<typename T1, typename T2>
            struct is_base_same<T1, T2, typename
            std::enable_if<std::is_same<base_type<T1>, base_type<T2>>::value>::type> : std::true_type {};

            template<class... Args, typename std::enable_if<is_base_same<KthTypeOf<0, Args...>, key_type>::value, int>::type = 0>
            std::pair<iterator, bool> EmplaceImp(Args&&... args) {
                auto [slot_address, alloc_happen] = FindOrAlloc(std::get<0>(std::forward_as_tuple(args...)));
                if (alloc_happen) {
                    std::allocator_traits<Allocator>::construct(param_->alloc_, std::addressof(slot_address->mutable_value), std::forward<Args>(args)...);
                }
                return {iterator{slot_address, this}, alloc_happen};
            }

            template<class... Args, typename std::enable_if<!is_base_same<KthTypeOf<0, Args...>, key_type>::value, int>::type = 0>
            std::pair<iterator, bool> EmplaceImp(Args&&... args) {
                alignas(slot_type) uint8_t temp_slot_buf[sizeof(slot_type)];
                slot_type* slot_ptr = reinterpret_cast<slot_type*>(temp_slot_buf);
                std::allocator_traits<Allocator>::construct(param_->alloc_, std::addressof(slot_ptr->mutable_value), std::forward<Args>(args)...);
                auto [slot_address, alloc_happen] = FindOrAlloc(slot_ptr->key);
                if (alloc_happen) {
                    std::allocator_traits<Allocator>::construct(param_->alloc_, std::addressof(slot_address->mutable_value), std::move(slot_ptr->mutable_value));
                }
                std::allocator_traits<Allocator>::destroy(param_->alloc_, std::addressof(slot_ptr->mutable_value));
                return {iterator(slot_address, this), alloc_happen};
            }

            std::pair<iterator, bool> InsertImp(const value_type &value) {
                const slot_type& src_slot = *slot_type::GetSlotAddressByValueAddress(&value);
                auto [slot_address, alloc_happen] = FindOrAlloc(src_slot.key);
                if (alloc_happen) {
                    std::allocator_traits<Allocator>::construct(param_->alloc_, std::addressof(slot_address->mutable_value), value);
                }
                return {iterator(slot_address, this), alloc_happen};
            }

            std::pair<iterator, bool> InsertImp(value_type &&value) {
                const slot_type& src_slot = *slot_type::GetSlotAddressByValueAddress(&value);
                auto [slot_address, alloc_happen] = FindOrAlloc(src_slot.key);
                if (alloc_happen) {
                    std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                std::addressof(slot_address->mutable_value), std::move(value));
                }
                return {iterator(slot_address, this), alloc_happen};
            }

            iterator EraseImp(iterator iter) {
                auto *slot_ptr = iter.value_ptr();
                size_t bucket_index = CompleteGetBucketIndex(slot_ptr->key);
                auto &temp_bucket = param_->bucket_array_[bucket_index];
#ifndef NDEBUG
                bool find_key_flag = false;
#endif
                for (size_t i = 0; i < temp_bucket.key_array.size(); ++i) {
                    if FPH_UNLIKELY(temp_bucket.key_array[i] == std::addressof(slot_ptr->key)) {
                        temp_bucket.key_array.erase(temp_bucket.key_array.begin() + i);
#ifndef NDEBUG
                        find_key_flag = true;
#endif
                        break;
                    }
                }
#ifndef NDEBUG
                assert(find_key_flag);
#endif
                --temp_bucket.entry_cnt;
                auto slot_pos = slot_ptr - slot_;
                assert(slot_pos >= 0 && size_t(slot_pos) < (param_->item_num_ceil_));
                auto y_pos = param_->map_table_[slot_pos];
                assert(y_pos < param_->filled_count_);
                std::swap(param_->random_table_[param_->filled_count_ - 1],
                          param_->random_table_[y_pos]);
                std::swap(param_->map_table_[param_->random_table_[param_->filled_count_ - 1]],
                          param_->map_table_[param_->random_table_[y_pos]]);
                --param_->filled_count_;

                MarkSlotEmpty(slot_pos);

                std::allocator_traits<Allocator>::destroy(param_->alloc_, std::addressof(slot_ptr->mutable_value));

                --param_->item_num_;
#if FPH_DEBUG_ERROR
                if FPH_UNLIKELY(!IsSlotEmpty(slot_ptr)) {
                    fprintf(stderr, "Error, slot not empty after erase\n");
                }
#endif
                auto *next_slot_ptr = GetNextSlotAddress(slot_ptr);

                param_->begin_it_ = iterator(next_slot_ptr, this);

                return iterator(next_slot_ptr, this);
            }

            iterator EraseImp(const_iterator first, const_iterator last) {
                const_iterator it = first;
                for (; it != last; ) {
                    it = this->EraseImp(it);
                }
                return iterator(ConstIteratorToIterator(it));
            }

            size_type EraseImp(const key_type& key) {
                size_t ret = 0U;
                auto pos = GetSlotPos(key);
                auto *slot_ptr = slot_ + pos;
                if (!IsSlotEmpty(pos) && key_equal_(slot_ptr->key, key)) {
                    ret = 1U;
                    this->EraseImp(iterator(slot_ptr, this));
#if FPH_DEBUG_ERROR
                    if FPH_UNLIKELY(!IsSlotEmpty(slot_ptr)) {
                        fprintf(stderr, "Error, slot not empty after erase const key&\n");
                    }
#endif
                }
                return ret;
            }



            std::pair<slot_type*, bool> FindOrAlloc(const key_type& key) {

                if FPH_UNLIKELY(param_->item_num_ + 1U > param_->should_expand_item_num_ &&
                                meta::detail::Ceil2(param_->item_num_ceil_ + 1U) <=
                                MAX_ITEM_NUM_CEIL_LIMIT) {
                    rehash(param_->item_num_ceil_ + 1U);
                }
                const auto k_seed0_hash = hash_(key, seed0_);
                const auto k_seed1_hash = MidHash(k_seed0_hash, seed1_);
                auto possible_pos = GetSlotPosBySeed0And1Hash(k_seed0_hash, k_seed1_hash);
                auto *insert_address = slot_ + possible_pos;
                bool insert_flag;
                if (MayEqual(possible_pos, k_seed1_hash) && key_equal_(insert_address->key, key)) {
                    insert_flag = false;
                }
                else {




                    if (IsSlotEmpty(possible_pos)) {
                        auto y_pos = param_->map_table_[possible_pos];
                        assert(y_pos >= param_->filled_count_);
                        std::swap(param_->random_table_[param_->filled_count_], param_->random_table_[y_pos]);
                        std::swap(param_->map_table_[param_->random_table_[param_->filled_count_]],
                                  param_->map_table_[param_->random_table_[y_pos]]);
                        ++param_->filled_count_;
                        auto bucket_index = GetBucketIndex(k_seed0_hash);
//                        auto bucket_index = GetBucketIndex(key);
                        auto &temp_bucket = param_->bucket_array_[bucket_index];
                        temp_bucket.key_array.push_back(&(insert_address->key));
                        ++temp_bucket.entry_cnt;

                        OccupyMetaDataSlot(possible_pos, k_seed1_hash);

                        insert_flag = true;
                        AddNewIterator(insert_address);
                        ++param_->item_num_;
                        return {insert_address, true};
                    }
                    else {
                        insert_flag = false;

                        auto bucket_index = GetBucketIndex(k_seed0_hash);
                        auto bucket_param = bucket_p_array_[bucket_index];
                        auto bucket_offset = bucket_param >> 1U;
                        auto optional_bit = bucket_param & 0x1U;

                        bool pattern_matched_flag = false;

                        auto &temp_bucket = param_->bucket_array_[bucket_index];

                        temp_bucket.key_array.push_back(&key);
                        bool is_first_try = true;

                        std::vector<size_t, SizeTAllocator> bucket_pattern;

                        for (auto bucket_try_bit = optional_bit; bucket_try_bit < 2U; ++bucket_try_bit) {

                            auto try_seed = MixSeedAndBit(seed2_, bucket_try_bit);

                            bucket_pattern.clear();

                            for (const auto *key_ptr: temp_bucket.key_array) {
                                size_t temp_hash = CompleteHash(*key_ptr, try_seed);
//                                size_t temp_hash = hash_(*key_ptr, try_seed);
//                                size_t temp_hash = hash_(*key_ptr, try_seed) & item_num_mask_;
                                bucket_pattern.push_back(temp_hash);
                            }

                            if (is_first_try) {
                                size_t total_pattern_num = bucket_pattern.size();
                                if (total_pattern_num < 1) {
                                    // error should at least be 1
                                    break;
                                }
                                for (size_t i = 0; i < total_pattern_num - 1U; ++i) {
                                    auto original_hash = bucket_pattern[i];
                                    auto temp_pos = slot_index_policy_.MapToIndex(original_hash + slot_index_policy_.ReverseMap(bucket_offset));
//                                    auto temp_pos =
//                                            (original_hash + bucket_offset) & item_num_mask_;
                                    auto y_pos = param_->map_table_[temp_pos];
                                    assert(y_pos < param_->filled_count_);
                                    std::swap(param_->random_table_[param_->filled_count_ - 1],
                                              param_->random_table_[y_pos]);
                                    std::swap(param_->map_table_[param_->random_table_[param_->filled_count_ - 1]],
                                              param_->map_table_[param_->random_table_[y_pos]]);
                                    --param_->filled_count_;
                                }
                            }
                            is_first_try = false;

                            // test collision in this bucket if try_bit == 1

                            bool self_collision_flag = !TestHashVecSelfCollision(bucket_pattern,
                                                                                 param_->seed2_test_table_,
                                                                                 param_->tested_hash_vec_);
                            if (self_collision_flag) {
                                continue;
                            }

                            size_t item_num_mask = param_->item_num_ceil_ - size_t(1ULL);

                            for (size_t search_pos_begin = param_->filled_count_;
                                 search_pos_begin < param_->item_num_ceil_; ++search_pos_begin) {
//                                size_t temp_offset =
//                                        (param_->item_num_ceil_ + param_->random_table_[search_pos_begin]
//                                         - bucket_pattern[0]) & item_num_mask_;
                                size_t temp_offset = (param_->item_num_ceil_ + param_->random_table_[search_pos_begin]
                                                      - slot_index_policy_.MapToIndex(bucket_pattern[0])) & item_num_mask;

                                bool this_offset_passed_flag = true;
                                for (auto temp_hash_value: bucket_pattern) {
                                    auto temp_pos = slot_index_policy_.MapToIndex(temp_hash_value +
                                                                                  slot_index_policy_.ReverseMap(temp_offset));
//                                    auto temp_pos =
//                                            (temp_hash_value + temp_offset) & item_num_mask_;
                                    if (param_->map_table_[temp_pos] < param_->filled_count_) {
                                        this_offset_passed_flag = false;
                                        break;
                                    }
                                }
                                if (this_offset_passed_flag) {
                                    pattern_matched_flag = true;
                                    for (auto temp_hash_value: bucket_pattern) {
                                        auto temp_pos = slot_index_policy_.MapToIndex(temp_hash_value +
                                                                                      slot_index_policy_.ReverseMap(temp_offset));
//                                        auto temp_pos =
//                                                (temp_hash_value + temp_offset) & item_num_mask_;
                                        auto y_pos = param_->map_table_[temp_pos];
                                        std::swap(param_->random_table_[param_->filled_count_],
                                                  param_->random_table_[y_pos]);
                                        std::swap(param_->map_table_[param_->random_table_[param_->filled_count_]],
                                                  param_->map_table_[param_->random_table_[y_pos]]);
                                        ++param_->filled_count_;
                                    }
                                    bucket_p_array_[temp_bucket.index] =
                                            (temp_offset << 1) | bucket_try_bit;

                                    break;
                                }
                            }

                            if (pattern_matched_flag) {
                                break;
                            }

                        } // for bucket_try_bit

                        if (!pattern_matched_flag) {
                            assert(param_->item_num_ < param_->item_num_ceil_);
                            param_->temp_pair_buf_.resize((param_->item_num_ + 1) * sizeof(value_type));
                            value_type *temp_value_buf_start =
                                    reinterpret_cast<value_type*>(param_->temp_pair_buf_.data());
                            value_type *temp_value_buf = temp_value_buf_start;
                            KeyAllocator key_alloc;
                            for (auto it = begin(); it != end(); ) {
                                std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                            temp_value_buf++, std::move(*it));
                                ++it;
                            }

                            slot_type* temp_slot_ptr =
                                    slot_type::GetSlotAddressByValueAddress(temp_value_buf);
                            std::allocator_traits<KeyAllocator>::construct(key_alloc,
                                                                           std::addressof(temp_slot_ptr->key), key);

                            ++param_->item_num_;
                            Build<true, false, true, true>(temp_value_buf_start,
                                                    temp_value_buf_start + param_->item_num_, seed1_,
#if FPH_DEBUG_FLAG
                                    true, // verbose
#else
                                                    false,
#endif
                                                    param_->bits_per_key_,
#if FPH_DY_DUAL_BUCKET_SET
                                    keys_first_part_ratio_, buckets_first_part_ratio_
#else
                                                    DEFAULT_KEYS_FIRST_PART_RATIO, DEFAULT_BUCKETS_FIRST_PART_RATIO
#endif
                            );
                            size_t temp_value_cnt = 0;
                            for(auto *temp_value_ptr = temp_value_buf_start;
                                temp_value_ptr != temp_value_buf_start + param_->item_num_; temp_value_ptr++) {
                                ++temp_value_cnt;
                                if (temp_value_cnt < param_->item_num_) {
                                    std::allocator_traits<Allocator>::destroy(param_->alloc_,
                                                                              temp_value_ptr);
                                }
                                else {
                                    std::allocator_traits<KeyAllocator>::destroy(key_alloc,
                                     std::addressof(slot_type::GetSlotAddressByValueAddress(temp_value_ptr)->key));
                                }
                            }
                            param_->temp_pair_buf_.clear();
                            param_->temp_pair_buf_.shrink_to_fit();

                            auto temp_pos = GetSlotPos(key);
                            insert_address = slot_ + temp_pos;
                        }
                        else {

                            param_->temp_byte_buf_vec_.resize(
                                    sizeof(slot_type) * temp_bucket.key_array.size());
                            auto *temp_pair_buf = reinterpret_cast<slot_type *>(param_->temp_byte_buf_vec_.data());


                            // prevent overlap from elements in the same bucket in slots
                            for (size_t i = 0; i + 1U < temp_bucket.key_array.size(); ++i) {
                                const key_type *key_ptr = temp_bucket.key_array[i];
                                auto original_slot_pos = GetSlotPos(*key_ptr, bucket_offset,
                                                                    optional_bit);
                                slot_type *temp_pair_ptr = temp_pair_buf + i;

                                auto *original_slot_address = slot_ + original_slot_pos;

                                std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                            std::addressof(temp_pair_ptr->mutable_value),
                                                                            std::move(original_slot_address->mutable_value));
                                // fill the free slot
                                std::allocator_traits<Allocator>::destroy(param_->alloc_,
                                                                          std::addressof(original_slot_address->mutable_value));
                                MarkSlotEmpty(original_slot_pos);

                            }
                            for (size_t i = 0; i + 1U < temp_bucket.key_array.size(); ++i) {
                                slot_type *src_pair_ptr = temp_pair_buf + i;
                                auto new_seed0_hash = hash_(src_pair_ptr->key, seed0_);
                                auto new_seed1_hash = MidHash(new_seed0_hash, seed1_);
                                auto new_slot_pos = GetSlotPosBySeed0And1Hash(new_seed0_hash, new_seed1_hash);
                                std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                            std::addressof(slot_[new_slot_pos].mutable_value),
                                                                            std::move(src_pair_ptr->mutable_value));
                                OccupyMetaDataSlot(new_slot_pos, new_seed1_hash);
                                std::allocator_traits<Allocator>::destroy(param_->alloc_, std::addressof(src_pair_ptr->mutable_value));

                                temp_bucket.key_array[i] = &slot_[new_slot_pos].key;

                            }

                            auto temp_pos = GetSlotPosBySeed0And1Hash(k_seed0_hash, k_seed1_hash);

                            insert_address = slot_ + temp_pos;
#ifndef NDEBUG
                            assert(IsSlotEmpty(temp_pos));
#endif
                            OccupyMetaDataSlot(temp_pos, k_seed1_hash);

                            temp_bucket.key_array.back() = std::addressof(insert_address->key);
                            ++temp_bucket.entry_cnt;
                            AddNewIterator(insert_address);
                            ++param_->item_num_;
                        } // else of if (!pattern_matched_flag)

                        insert_flag = true;

                    } // else of if IsSlotEmpty(possible_pos)

                } // else of if FPH_UNLIKELY(key_equal_(insert_address->key, key))


                return std::make_pair(insert_address, insert_flag);
            }

            void DestroySlots() {
                if (slot_ != nullptr) {
                    for (size_t i = 0; i < param_->item_num_ceil_; ++i) {
                        if (IsSlotEmpty(i)) {}
                        else {
                            std::allocator_traits<Allocator>::destroy(param_->alloc_, std::addressof(slot_[i].mutable_value));
                        }
                    }
                }
            }


            static FPH_ALWAYS_INLINE size_t PartHash(size_t hash_v) {
                constexpr uint32_t part_bits = META_ITEM_BIT_SIZE - 1U;
                constexpr uint32_t offset = sizeof(hash_v) * 8U - part_bits;
                return hash_v >> offset;
            }

            FPH_ALWAYS_INLINE void OccupyMetaDataSlot(size_t slot_pos, size_t seed1_hash_v) {
                constexpr uint32_t KeepBitOffset = META_ITEM_BIT_SIZE - 1U;
                auto meta_v = ((1U) << KeepBitOffset) | PartHash(seed1_hash_v);
                meta_data_.set(slot_pos, meta_v);
            }



            template<bool is_rehash, bool called_by_rehash, bool use_move,
                    bool last_element_only_has_key=false,
                    class InputIt>
            void BuildImp(InputIt pair_begin, InputIt pair_end, uint64_t seed = 0,
                          bool verbose = false, double c = 3.0,
                          double keys_first_part_ratio = 0.6, double buckets_first_part_ratio = 0.3,
                          size_t max_try_seed0_time = 10,
                          size_t max_try_seed1_time = 10, size_t max_try_seed2_time = 1000,
                          size_t max_reseed2_time = 1000) {


                auto build_start_time = std::chrono::high_resolution_clock::now();

                if FPH_UNLIKELY(c < 1.45) {
                    ThrowInvalidArgument("c must be no less than 1.45");
                }

                if FPH_UNLIKELY(keys_first_part_ratio < 0.0 || keys_first_part_ratio > 1.0) {
                    ThrowInvalidArgument("keys_first_part_ratio must be in [0.0, 1.0]");
                }

                if FPH_UNLIKELY(buckets_first_part_ratio < 0.0 || buckets_first_part_ratio > 1.0) {
                    ThrowInvalidArgument("buckets_first_part_ratio must be in [0.0, 1.0]");
                }


                param_->bits_per_key_ = c;

                auto temp_key_num = std::distance(pair_begin, pair_end);


                if FPH_UNLIKELY(temp_key_num < 0) {
                    ThrowInvalidArgument("Input pair_begin > pair_end");
                    return;
                }

                if (is_rehash) {
#ifndef NDEBUG
                    assert(size_t(temp_key_num) == param_->item_num_);
#endif
                }


                size_t key_num = temp_key_num;

                const size_t old_slot_capacity = param_->slot_capacity_;
                const size_t old_bucket_capacity = param_->bucket_capacity_;
                const size_t old_meta_under_capacity = param_->meta_under_entry_capacity_;

                // destroy the previous slot objects
                DestroySlots();

                param_->item_num_ = key_num;

                if (key_num != 0) {
//                    size_t temp_slot_num = size_t((double)key_num / MAX_LOAD_FACTOR_UPPER_LIMIT);
                    if (!is_rehash) {
                        slot_index_policy_.UpdateBySlotNum(size_t((double)key_num / max_load_factor()));
//                        item_num_mask_ = meta::detail::CeilToMask(size_t(key_num / MAX_LOAD_FACTOR_UPPER_LIMIT));
                    } else {
                        // item_num_mask_ should be set before call Build()
                    }
                    size_t temp_slot_num = slot_index_policy_.slot_num();
                    temp_slot_num = std::max(temp_slot_num, DEFAULT_INIT_ITEM_NUM_CEIL);
                    temp_slot_num = std::min(temp_slot_num, MAX_ITEM_NUM_CEIL_LIMIT);
                    slot_index_policy_.UpdateBySlotNum(temp_slot_num);
//                    item_num_mask_ = std::max(item_num_mask_, DEFAULT_INIT_ITEM_NUM_MASK);
//                    item_num_mask_ = std::min(item_num_mask_, size_t(BUCKET_PARAM_MASK));
                } else {
                    if (!is_rehash) {
                        slot_index_policy_.UpdateBySlotNum(DEFAULT_INIT_ITEM_NUM_CEIL);
//                        item_num_mask_ = DEFAULT_INIT_ITEM_NUM_MASK;
                    }
                }
                param_->item_num_ceil_ = slot_index_policy_.slot_num();
//                param_->item_num_ceil_ = item_num_mask_ + 1U;

                param_->should_expand_item_num_ = std::ceil(param_->item_num_ceil_ * param_->max_load_factor_);

                if (key_num > param_->item_num_ceil_) {
                    ThrowInvalidArgument(("BucketParamType num_bits: " +
                        std::to_string(bucket_param_type_num_bits_) +
                        " ,key number: " + std::to_string(key_num)).c_str());
                    return;

                }

                if (called_by_rehash) {
                    param_->slot_capacity_ = param_->item_num_ceil_;
                }
                else {
                    param_->slot_capacity_ = std::max(param_->item_num_ceil_, param_->slot_capacity_);
                }
                param_->meta_under_entry_capacity_ = MetaDataView::GetUnderlyingEntryNum(param_->slot_capacity_);


                // mapping


                size_t temp_bucket_num = (key_num > 0 && !is_rehash) ?
                                         std::ceil(param_->bits_per_key_ * key_num /
                                                   std::ceil(std::log2(key_num) + 1))
                                                                     :
                                         std::ceil(param_->bits_per_key_ * param_->item_num_ceil_ /
                                                   std::ceil(std::log2(param_->item_num_ceil_) + 1));

#if FPH_DY_DUAL_BUCKET_SET
                buckets_first_part_ratio_ = buckets_first_part_ratio;
                keys_first_part_ratio_ = keys_first_part_ratio;

                size_t temp_p2 = std::llround((double) temp_bucket_num * buckets_first_part_ratio_);
                p2_ = meta::detail::CeilToMask(temp_p2);
                p2_plus_1_ = p2_ + 1U;

                size_t temp_p2_remain = p2_ + 2U <= temp_bucket_num ? (temp_bucket_num - p2_ - 2U) : 0;
                p2_remain_ = meta::detail::CeilToMask(temp_p2_remain);

                param_->bucket_num_ = p2_ + p2_remain_ + 2U;

                double real_keys_first_part_ratio = keys_first_part_ratio *
                                (double(p2_) / param_->bucket_num_ / buckets_first_part_ratio_ );
                size_t temp_p1 = std::llround(param_->item_num_ceil_ * real_keys_first_part_ratio);
                p1_ = temp_p1;

#else
                param_->bucket_num_ = meta::detail::Ceil2(temp_bucket_num);
                if (param_->bucket_num_ <= 1UL) {
                    param_->bucket_num_ = 2U;
                }
//                bucket_mask_ = param_->bucket_num_ - 1U;
                bucket_index_policy_.UpdateBySlotNum(param_->bucket_num_);
#endif

                if (called_by_rehash) {
                    param_->bucket_capacity_ = param_->bucket_num_;
                }
                else {
                    param_->bucket_capacity_ = std::max(param_->bucket_num_, param_->bucket_capacity_);
                }

                if (old_bucket_capacity < param_->bucket_capacity_
                    || (old_bucket_capacity > param_->bucket_capacity_ && called_by_rehash)
                    || bucket_p_array_ == nullptr) {
                    BucketParamAllocator bucket_param_alloc;
                    if (bucket_p_array_ != nullptr) {
                        bucket_param_alloc.deallocate(bucket_p_array_, old_bucket_capacity);
                        bucket_p_array_ = nullptr;
                    }
                    bucket_p_array_ = bucket_param_alloc.allocate(param_->bucket_capacity_);
                    if (old_bucket_capacity > param_->bucket_capacity_) {
                        param_->bucket_array_.clear();
                        param_->bucket_array_.shrink_to_fit();
                    }
                    param_->bucket_array_.reserve(param_->bucket_num_);
                }


                if (verbose) {
                    size_t buckets_use_bytes = param_->bucket_num_ * sizeof(BucketParamType);
                    fprintf(stderr, "meta fph map, is_rehash: %d, c: %.3f,  use %zu bucket num, "
                                    "%zu ceil item num, %zu item num, %zu key num, "
                                    "buckets use memory: %zu bytes, %.3f bits per key up-bound, ",
                            is_rehash, c, param_->bucket_num_, param_->item_num_ceil_, param_->item_num_, key_num,
                            buckets_use_bytes, buckets_use_bytes * 8.0 / param_->item_num_ceil_);
#if FPH_DY_DUAL_BUCKET_SET
                    fprintf(stderr, "p1: %lu, p2: %lu, ", p1_, p2_);
#endif
                }



                std::mt19937_64 random_engine(seed);
                std::uniform_int_distribution<size_t> random_dis;

                bool build_succeed_flag = false;

                for (size_t try_seed0_time = 0; try_seed0_time < max_try_seed0_time; ++ try_seed0_time) {

                    seed0_ = random_dis(random_engine);
                    seed0_ |= size_t(1ULL);

                    for (size_t try_seed1_time = 0; try_seed1_time < max_try_seed1_time; ++try_seed1_time) {

#if !defined(NDEBUG) && FPH_DEBUG_FLAG
                        if (try_seed1_time >= max_try_seed1_time / 2) {
                            (void)0;
                        }
#endif

                        seed1_ = random_dis(random_engine);
                        seed1_ |= size_t(1ULL);

                        // ordering


                        size_t max_bucket_size = 0;


                        param_->bucket_array_.resize(0);
                        for (size_t i = 0; i < param_->bucket_num_; ++i) {
                            param_->bucket_array_.emplace_back(i);
                        }

                        auto update_bucket_func = [&](const auto* value_ptr) {
                            const auto *slot_ptr = reinterpret_cast<const slot_type *>(value_ptr);
//                            size_t hash_value = GetBucketIndex(slot_ptr->key);
                            size_t hash_value = CompleteGetBucketIndex(slot_ptr->key);
                            assert(hash_value < param_->bucket_num_);
                            auto &temp_bucket = param_->bucket_array_[hash_value];
                            ++temp_bucket.entry_cnt;
                            max_bucket_size =
                                    temp_bucket.entry_cnt > max_bucket_size ? temp_bucket.entry_cnt
                                                                            : max_bucket_size;
                            temp_bucket.AddKey(&(slot_ptr->key));
                        };

                        for (auto it = pair_begin; it != pair_end; ++it) {
                            update_bucket_func(std::addressof(*it));
                        }

                        std::vector<size_t, SizeTAllocator> sorted_index_array;
                        sorted_index_array.resize(param_->bucket_num_);

                        detail::CountSortOutIndex<BucketGetKey<BucketType>, true, SizeTAllocator>
                                                                                  (param_->bucket_array_.begin(), param_->bucket_array_.end(), sorted_index_array.begin(),
                                                                                          max_bucket_size);


                        // searching


                        // try to find seed2_ which makes no collision per bucket
                        param_->seed2_test_table_.resize(param_->item_num_ceil_, false);
                        param_->tested_hash_vec_.clear();

                        size_t old_random_table_size = param_->random_table_.size();
                        param_->random_table_.resize(param_->item_num_ceil_);
                        param_->map_table_.resize(param_->item_num_ceil_);
                        // only cause possible allocation when rehash
                        if (called_by_rehash) {
                            if (param_->item_num_ceil_ < old_random_table_size) {
                                param_->random_table_.shrink_to_fit();
                                param_->map_table_.shrink_to_fit();
                            }
                        }


                        for (size_t try_seed2_time = 0; try_seed2_time < max_try_seed2_time; ++try_seed2_time) {

                            bool found_useful_seed2 = false;
                            for (size_t seed_time = 0; seed_time < max_reseed2_time; ++seed_time) {
                                seed2_ = random_dis(random_engine);
                                seed2_ |= size_t(1ULL);

                                bool pass_test_flag = true;
                                for (size_t i = 0; i < param_->bucket_num_; ++i) {
                                    auto &testing_bucket = param_->bucket_array_[sorted_index_array[i]];
                                    pass_test_flag &= TestBucketSelfCollision(testing_bucket,
                                                                              param_->seed2_test_table_,
                                                                              param_->tested_hash_vec_, seed2_);

                                    if (!pass_test_flag) {
                                        break;
                                    }
                                }
                                if (pass_test_flag) {
                                    found_useful_seed2 = true;
                                    break;
                                }

                            }
                            if (!found_useful_seed2) {
                                continue;
                            }


                            for (size_t i = 0; i < param_->item_num_ceil_; ++i) {
                                param_->random_table_[i] = i;
                            }
                            std::shuffle(param_->random_table_.begin(), param_->random_table_.end(), random_engine);
                            for (size_t i = 0; i < param_->item_num_ceil_; ++i) {
                                param_->map_table_[param_->random_table_[i]] = i;
                            }

                            //                        assert(IsRandomTableValid(param_->random_table_, param_->map_table_));
                            param_->filled_count_ = 0;

                            std::vector<size_t, SizeTAllocator> bucket_pattern;
                            bucket_pattern.reserve(max_bucket_size);

                            bool this_try_seed2_succeed_flag = true;

                            for (size_t bucket_index = 0; bucket_index < param_->bucket_num_; ++bucket_index) {
                                auto &temp_bucket = param_->bucket_array_[sorted_index_array[bucket_index]];
                                if (temp_bucket.entry_cnt == 0) {
                                    continue;
                                }

                                bool pattern_matched_flag = false;

                                for (size_t bucket_try_bit = 0; bucket_try_bit < 2U; ++bucket_try_bit) {

                                    auto try_seed = MixSeedAndBit(seed2_, bucket_try_bit);

                                    bucket_pattern.clear();

                                    for (const auto *key_ptr: temp_bucket.key_array) {
                                        size_t temp_hash = CompleteHash(*key_ptr, try_seed);
                                        bucket_pattern.push_back(temp_hash);
                                    }

                                    // test collision in this bucket if try_bit == 1

                                    if (bucket_try_bit > 0) {
                                        bool self_collision_flag = !TestHashVecSelfCollision(
                                                bucket_pattern,
                                                param_->seed2_test_table_,
                                                param_->tested_hash_vec_);
                                        if (self_collision_flag) {
                                            break;
                                        }
                                    }

                                    size_t item_num_mask = slot_index_policy_.slot_num() - size_t(1ULL);

                                    for (size_t search_pos_begin = param_->filled_count_;
                                         search_pos_begin < param_->item_num_ceil_; ++search_pos_begin) {
                                        size_t temp_offset = (param_->item_num_ceil_ + param_->random_table_[search_pos_begin]
                                              - slot_index_policy_.MapToIndex(bucket_pattern[0]) ) & item_num_mask;
                                        bool this_offset_passed_flag = true;
                                        for (auto temp_hash_value: bucket_pattern) {
                                            auto temp_pos = slot_index_policy_.MapToIndex(temp_hash_value + slot_index_policy_.ReverseMap(temp_offset));
                                            if (param_->map_table_[temp_pos] < param_->filled_count_) {
                                                this_offset_passed_flag = false;
                                                break;
                                            }
                                        }
                                        if (this_offset_passed_flag) {
                                            pattern_matched_flag = true;
                                            for (auto temp_hash_value: bucket_pattern) {
                                                auto temp_pos = slot_index_policy_.MapToIndex(temp_hash_value + slot_index_policy_.ReverseMap(temp_offset));
                                                auto y_pos = param_->map_table_[temp_pos];
                                                std::swap(param_->random_table_[param_->filled_count_],
                                                          param_->random_table_[y_pos]);
                                                std::swap(param_->map_table_[param_->random_table_[param_->filled_count_]],
                                                          param_->map_table_[param_->random_table_[y_pos]]);
                                                ++param_->filled_count_;
                                            }
                                            bucket_p_array_[temp_bucket.index] =
                                                    (temp_offset << 1U) | bucket_try_bit;

                                            break;
                                        }
                                    }

                                    if (pattern_matched_flag) {
                                        break;
                                    }

                                } // for bucket_try_bit

                                if (!pattern_matched_flag) {
                                    this_try_seed2_succeed_flag = false;
                                    break;
                                }

                            } // for bucket_index

                            if (this_try_seed2_succeed_flag) {
                                build_succeed_flag = true;
                                break;
                            }


                        } // for try_seed2_time

                        if (build_succeed_flag) {
                            break;
                        }

                    } // for try_seed1_time

                    if (build_succeed_flag) {
                        break;
                    }


                } // for try_seed0_time

                if (!build_succeed_flag) {
                    ThrowInvalidArgument(("timeout when try to build fph map, consider using a stronger seed hash function, key_num: "
                        + std::to_string(key_num) + ", item_num_ceil: " + std::to_string(param_->item_num_ceil_)
                        + ", bucket num: " + std::to_string(param_->bucket_num_)).c_str());
                }



                // allocate
                if ((old_slot_capacity < param_->slot_capacity_)
                    || (old_slot_capacity > param_->slot_capacity_ && called_by_rehash)
                    || slot_ == nullptr) {

                    if (slot_ != nullptr) {
                        SlotAllocator{}.deallocate(slot_, old_slot_capacity);
                        slot_ = nullptr;
                    }
                    if (meta_data_.data() != nullptr) {
                        MetaUnderAllocator{}.deallocate(meta_data_.data(), old_meta_under_capacity);
                        meta_data_.SetUnderlyingArray(nullptr);
                    }

                    slot_ = SlotAllocator{}.allocate(param_->slot_capacity_);
                    meta_data_.SetUnderlyingArray(
                            MetaUnderAllocator{}.allocate(param_->meta_under_entry_capacity_));
                }

                // Set all the slot empty
                size_t meta_under_entry_num = MetaDataView::GetUnderlyingEntryNum(param_->item_num_ceil_);
                memset(meta_data_.data(), 0, sizeof(MetaUnderEntry) * meta_under_entry_num);

                if constexpr (use_move || (is_rehash && std::is_move_constructible<value_type>::value)) {
                    auto construct_pair_func_move = [&](value_type &&value, bool only_key) {
                        slot_type* slot_ptr = slot_type::GetSlotAddressByValueAddress(std::addressof(value));
                        auto temp_seed0_hash = hash_(slot_ptr->key, seed0_);
                        auto temp_seed1_hash = MidHash(temp_seed0_hash, seed1_);
                        auto slot_pos = GetSlotPosBySeed0And1Hash(temp_seed0_hash, temp_seed1_hash);
                        OccupyMetaDataSlot(slot_pos, temp_seed1_hash);
                        auto *insert_address = slot_ + slot_pos;

                        if (only_key) {
                            KeyAllocator key_alloc{};
                            std::allocator_traits<KeyAllocator>::construct(key_alloc,
                                   std::addressof(insert_address->key), std::move(slot_ptr->key));
                        }
                        else {
                            std::allocator_traits<Allocator>::construct(param_->alloc_,
                                    std::addressof(insert_address->mutable_value),
                                    std::move(value));
                        }
                    };
                    if constexpr (is_rehash) {
                        size_t temp_key_cnt = 0;
                        for (auto it = pair_begin; it != pair_end; ++it) {
                            ++temp_key_cnt;
                            bool only_key = last_element_only_has_key && (temp_key_cnt == key_num);
                            static_assert(std::is_move_constructible<value_type>::value);
                            construct_pair_func_move(std::move(*it), only_key);

                        }
                    }
                    else {
                        static_assert(use_move);
                        size_t temp_key_cnt = 0;
                        for (auto it = pair_begin; it != pair_end; ++it) {
                            ++temp_key_cnt;
                            bool only_key = last_element_only_has_key && (temp_key_cnt == key_num);
                            construct_pair_func_move(std::move(*it), only_key);
                        }
                    }
                }
                else {
                    auto construct_pair_func = [&](InputIt it, bool only_key) {
                        const slot_type* slot_ptr = slot_type::GetSlotAddressByValueAddress(std::addressof(*it));
                        auto temp_seed0_hash = hash_(slot_ptr->key, seed0_);
                        auto temp_seed1_hash = MidHash(temp_seed0_hash, seed1_);
                        auto slot_pos = GetSlotPosBySeed0And1Hash(temp_seed0_hash, temp_seed1_hash);
                        OccupyMetaDataSlot(slot_pos, temp_seed1_hash);
                        auto *insert_address = slot_ + slot_pos;
                        if (only_key) {
                            KeyAllocator key_alloc{};
                            std::allocator_traits<KeyAllocator>::construct(key_alloc, std::addressof(insert_address->key), slot_ptr->key);
                        }
                        else {
                            std::allocator_traits<Allocator>::construct(param_->alloc_,
                                                                        std::addressof(insert_address->mutable_value),
                                                                        *it);
                        }
                    };

                    if constexpr (!is_rehash) {
                        size_t temp_key_cnt = 0;
                        for (auto it = pair_begin; it != pair_end; ++it) {
                            ++temp_key_cnt;
                            bool only_key = last_element_only_has_key && (temp_key_cnt == key_num);
                            construct_pair_func(it, only_key);
                        }
                    }
                    else {
                        size_t temp_key_cnt = 0;
                        for (auto it = pair_begin; it != pair_end; ++it) {
                            ++temp_key_cnt;
                            bool only_key = last_element_only_has_key && (temp_key_cnt == key_num);
                            assert(!std::is_move_constructible<value_type>::value);
                            construct_pair_func(it, only_key);
                        }
                    }
                }



#if FPH_ENABLE_ITERATOR
                if (param_->item_num_ > 0) {
                    auto begin_it_pos = param_->random_table_[0];
#ifndef NDEBUG
                    assert(!IsSlotEmpty(begin_it_pos));
#endif
                    param_->begin_it_ = iterator(slot_ + begin_it_pos, this);
                } else {
                    param_->begin_it_ = iterator(nullptr, nullptr);
                }

                for (auto &bucket_param: param_->bucket_array_) {
                    bucket_param.key_array.clear();
                }
                for (auto it = begin(); it != end(); ++it) {
                    const auto* slot_address = slot_type::GetSlotAddressByValueAddress(std::addressof(*it));
                    auto bucket_index = CompleteGetBucketIndex(slot_address->key);
//                    auto bucket_index = this->GetBucketIndex(slot_address->key);
                    auto &key_array = param_->bucket_array_[bucket_index].key_array;
                    key_array.push_back(&(slot_address->key));
                }
#endif

                if (verbose) {
                    auto build_end_time = std::chrono::high_resolution_clock::now();
                    size_t build_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            build_end_time - build_start_time).count();
                    fprintf(stderr, "build use time: %.6f seconds\n", build_ns / (1e+9));
                }


            } // function Build

        }; // class raw set


    } // namespace meta::detail

    namespace meta::detail {

        template<class T>
        union MetaSetSlotType {

            ~MetaSetSlotType() = delete;

            using value_type = T;
            using mutable_value_type = T;

            value_type value;
            mutable_value_type mutable_value;
            value_type key;

            static MetaSetSlotType<T>* GetSlotAddressByValueAddress(value_type *value_ptr) {
                return reinterpret_cast<MetaSetSlotType<T>*>(value_ptr);
            }

            static const MetaSetSlotType<T>* GetSlotAddressByValueAddress(const value_type *value_ptr) {
                return reinterpret_cast<const MetaSetSlotType<T>*>(value_ptr);
            }

        };

        class HighBitsIndexMapPolicy {
        public:

            HighBitsIndexMapPolicy(size_t element_num) noexcept: shift_bits_(0) {
                UpdateBySlotNum(element_num);
            }

            // takes the high bits
            FPH_ALWAYS_INLINE size_t MapToIndex(size_t hash) const noexcept {
                return hash >> shift_bits_;

//                return (11400714819323198485llu * hash) >> shift_bits_;
            }

            FPH_ALWAYS_INLINE size_t ReverseMap(size_t index) const noexcept {
                return index << shift_bits_;
            }

            size_t slot_num() const noexcept {
                return size_t(1UL) << (std::numeric_limits<size_t>::digits - shift_bits_);
            }

            void UpdateBySlotNum(size_t element_num) {
                size_t round_up_log2_slot_num = meta::detail::RoundUpLog2(element_num);
                shift_bits_ = std::numeric_limits<size_t>::digits - round_up_log2_slot_num;
            }

        protected:
            uint32_t shift_bits_;
        };

        // using low bits of hash as slot index
        class LowBitsIndexMapPolicy {
        public:

            LowBitsIndexMapPolicy(size_t element_num) noexcept: mask_(0) {
                UpdateBySlotNum(element_num);
            }

            // takes the low bits
            FPH_ALWAYS_INLINE size_t MapToIndex(size_t hash) const noexcept {
                return hash & mask_;
            }

            FPH_ALWAYS_INLINE size_t ReverseMap(size_t index) const noexcept {
                return index;
            }

            size_t slot_num() const noexcept {
                return mask_ + size_t(1UL);
            }

            void UpdateBySlotNum(size_t element_num) {
                mask_ = meta::detail::GenBitMask<size_t>(meta::detail::RoundUpLog2(element_num));
            }

        protected:
            size_t mask_;
        };

        template<class T>
        class MetaFphSetPolicy {
        public:
            using key_type = T;
            using value_type = T;
            using slot_type = MetaSetSlotType<T>;
            using index_map_policy = HighBitsIndexMapPolicy;
//            using index_map_policy = LowBitsIndexMapPolicy;
        };

    } // namespace meta::detail

    /**
     * The meta perfect hash set container
     * @tparam Key
     * @tparam SeedHash the operator() takes two arguments: key and a size_t seed
     * @tparam KeyEqual
     * @tparam Allocator
     * @tparam BucketParamType
     */
    template<class Key,
            class SeedHash = meta::SimpleSeedHash<Key>,
            class KeyEqual = std::equal_to<Key>,
            class Allocator = std::allocator<Key>,
            class BucketParamType = uint32_t>
    class MetaFphSet: public meta::detail::MetaRawSet<meta::detail::MetaFphSetPolicy<Key>,
            SeedHash, KeyEqual, Allocator, BucketParamType> {
        using Base = typename MetaFphSet::MetaRawSet;
    public:
        using Base::Base;

        using Base::begin;


    protected:

    };

    template<class Key,
            class SeedHash = meta::SimpleSeedHash<Key>,
            class KeyEqual = std::equal_to<Key>,
            class Allocator = std::allocator<Key>,
            class BucketParamType = uint32_t>
    using meta_fph_set = MetaFphSet<Key, SeedHash, KeyEqual, Allocator,
                            BucketParamType>;

    namespace meta::detail {

        namespace memory {

            // If Pair is a standard-layout type, OffsetOf<Pair>::k_first and
            // OffsetOf<Pair>::k_second are equivalent to offsetof(Pair, first) and
            // offsetof(Pair, second) respectively. Otherwise they are -1.
            //
            // The purpose of OffsetOf is to avoid calling offsetof() on non-standard-layout
            // type, which is non-portable.
            template<class Pair, class = std::true_type>
            struct OffsetOf {
                static constexpr size_t k_first = static_cast<size_t>(-1);
                static constexpr size_t k_second = static_cast<size_t>(-1);
            };

            template<class Pair>
            struct OffsetOf<Pair, typename std::is_standard_layout<Pair>::type> {
                static constexpr size_t k_first = offsetof(Pair, first);
                static constexpr size_t k_second = offsetof(Pair, second);
            };

        } // namespace memory

        template <class K, class V>
        struct IsLayoutCompatible {
        private:
            struct Pair {
                K first;
                V second;
            };

            // Is P layout-compatible with Pair?
            template <class P>
            static constexpr bool LayoutCompatible() {
                return std::is_standard_layout<P>() && sizeof(P) == sizeof(Pair) &&
                       alignof(P) == alignof(Pair) &&
                       memory::OffsetOf<P>::k_first ==
                       memory::OffsetOf<Pair>::k_first &&
                       memory::OffsetOf<P>::k_second ==
                       memory::OffsetOf<Pair>::k_second;
            }

        public:
            // Whether pair<const K, V> and pair<K, V> are layout-compatible. If they are,
            // then it is safe to store them in a union and read from either.
            static constexpr bool value = std::is_standard_layout<K>() &&
                                          std::is_standard_layout<Pair>() &&
                                          memory::OffsetOf<Pair>::k_first == 0 &&
                                          LayoutCompatible<std::pair<K, V>>() &&
                                          LayoutCompatible<std::pair<const K, V>>();
        };


        template<class K, class V>
        union MetaMapSlotType {

            ~MetaMapSlotType() = delete;

            using value_type = std::pair<const K, V>;
            using mutable_value_type =
            std::pair<typename std::remove_const<K>::type, typename std::remove_const<V>::type>;

            value_type value;
            mutable_value_type mutable_value;
            typename std::remove_const<K>::type key;

            static MetaMapSlotType<K, V>* GetSlotAddressByValueAddress(value_type *value_ptr) {
                return reinterpret_cast<MetaMapSlotType<K, V>*>(value_ptr);
            }

            static MetaMapSlotType<K, V>* GetSlotAddressByValueAddress(mutable_value_type *value_ptr) {
                return reinterpret_cast<MetaMapSlotType<K, V>*>(value_ptr);
            }

            static const MetaMapSlotType<K, V>* GetSlotAddressByValueAddress(const value_type *value_ptr) {
                return reinterpret_cast<const MetaMapSlotType<K, V>*>(value_ptr);
            }

            static const MetaMapSlotType<K, V>* GetSlotAddressByValueAddress(const mutable_value_type *value_ptr) {
                return reinterpret_cast<const MetaMapSlotType<K, V>*>(value_ptr);
            }

            // TODO: handle the situation when Layout is not compatible
//            static_assert(IsLayoutCompatible<K, V>::value,
//                          "layout of Key and Value is not compatible");

        };

        template<class K, class V>
        class MetaFphMapPolicy {
        public:
            using key_type = K;
            using value_type = std::pair<const K, V>;
            using slot_type = MetaMapSlotType<K, V>;
//            using index_map_policy = LowBitsIndexMapPolicy;
            using index_map_policy = HighBitsIndexMapPolicy;
        };

    } // namespace meta::detail

    /**
     * The meta perfect hash map container
     * @tparam Key
     * @tparam T
     * @tparam SeedHash the operator() takes two arguments: key and a size_t seed
     * @tparam KeyEqual
     * @tparam Allocator
     * @tparam BucketParamType
     */
    template <class Key, class T,
            class SeedHash = meta::SimpleSeedHash<Key>,
            class KeyEqual = std::equal_to<Key>,
            class Allocator = std::allocator<std::pair<const Key, T>>,
            class BucketParamType = uint32_t
    >
    class MetaFphMap : public meta::detail::MetaRawSet<meta::detail::MetaFphMapPolicy<Key, T>,
            SeedHash, KeyEqual, Allocator, BucketParamType> {
        using Base = typename MetaFphMap::MetaRawSet;
        template<class K>
        using key_arg = typename Base::template key_arg<K>;
    public:

        using mapped_type = T;

        using Base::Base;

        using iterator = typename Base::iterator;
        using key_type = typename Base::key_type;


        using Base::find;

        using Base::emplace;

        using Base::insert;


        template<class... Args>
        std::pair<iterator, bool> try_emplace(const key_type& k, Args&&... args) {
            return this-> template TryEmplaceImp<>(k, std::forward<Args>(args)...);
        }

        template<class... Args>
        std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args) {
            return this-> template TryEmplaceImp<>(std::move(k),
                                                   std::forward<Args>(args)...);
        }

        T& operator[] (const key_type &key) {
            return this->try_emplace(key).first->second;
        }

        T& operator[] (key_type&& key) {
            return this->try_emplace(std::move(key)).first->second;
        }

        template<class K = key_type>
        const T& operator[] (const key_arg<K> &key) const noexcept {
            return this->GetPointerNoCheck(key)->second;
        }

        template<class K = key_type>
        T& at (const key_arg<K> &key) {
            auto *pair_ptr = this->GetPointerNoCheck(key);
            if FPH_UNLIKELY(!this->key_equal_(pair_ptr->first, key)) {
                meta::detail::ThrowOutOfRange("Can not find key in at");
            }
            return pair_ptr->second;
        }

        template<class K = key_type>
        const T& at (const key_arg<K>& key) const {
            const auto *pair_ptr = this->GetPointerNoCheck(key);
            if FPH_UNLIKELY(!this->key_equal_(pair_ptr->first, key)) {
                meta::detail::ThrowOutOfRange("Can not find key in at");
            }
            return pair_ptr->second;
        }

    protected:
    };

    template <class Key, class T,
            class SeedHash = meta::SimpleSeedHash<Key>,
            class KeyEqual = std::equal_to<Key>,
            class Allocator = std::allocator<std::pair<const Key, T>>,
            class BucketParamType = uint32_t
    >
    using meta_fph_map = MetaFphMap<Key, T, SeedHash, KeyEqual, Allocator,
                            BucketParamType>;


} // namespace fph

