#pragma once
//#define _HAS_STD_BYTE 0

#include <random>
#include <cstdint>
#include <map>
#include <set>
#include <ctime>
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <array>
#include <bitset>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>

#if STR_SIZE < 5
#define STR_SIZE 15
#endif

#define NDEBUG                1
#ifdef _WIN32
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    # include <windows.h>
    # pragma warning(disable : 4996)
#else
    # include <unistd.h>
    # include <sys/resource.h>
    # include <sys/time.h>
#endif

#if __cplusplus > 201402L
//   #define STR_VIEW  1
   #include <string_view>
#endif

#ifdef __has_include
    #if __has_include("wyhash.h")
    #include "wyhash.h"
    #endif
    #if __has_include("komihash.h")
    #include "komihash.h"
    #define KOMI_HESH 1
    #endif
#endif

#if __x86_64__ || __amd64__ || _M_X64
    #define X86 1
    #define X86_64 1
    #define PHMAP_HAVE_SSSE3                      1
    #define PHMAP_HAVE_SSE2                       1
//    #define PHMAP_NON_DETERMINISTIC               1
#endif
#if  _M_IX86 || __i386__
    #define X86 1
    #define X86_32 1
    #define PHMAP_HAVE_SSSE3                      1
    #define PHMAP_HAVE_SSE2                       1
//    #define PHMAP_NON_DETERMINISTIC               1
#endif

#if A_HASH
#include "ahash/ahash.c"
#include "ahash/random_state.c"
#include "ahash-cxx/hasher.h"
#include "ahash-cxx/ahash-cxx.h"
#endif

#if _WIN32 && _WIN64 == 0
uint64_t _umul128(uint64_t multiplier, uint64_t multiplicand, uint64_t *product_hi)
{
    // multiplier   = ab = a * 2^32 + b
    // multiplicand = cd = c * 2^32 + d
    // ab * cd = a * c * 2^64 + (a * d + b * c) * 2^32 + b * d
    uint64_t a = multiplier >> 32;
    uint64_t b = multiplier & 0xFFFFFFFF;
    uint64_t c = multiplicand >> 32;
    uint64_t d = multiplicand & 0xFFFFFFFF;

    //uint64_t ac = a * c;
    uint64_t ad = a * d;
    //uint64_t bc = b * c;
    uint64_t bd = b * d;

    uint64_t adbc = ad + (b * c);
    uint64_t adbc_carry = adbc < ad ? 1 : 0;

    // multiplier * multiplicand = product_hi * 2^64 + product_lo
    uint64_t product_lo = bd + (adbc << 32);
    uint64_t product_lo_carry = product_lo < bd ? 1 : 0;
    *product_hi = (a * c) + (adbc >> 32) + (adbc_carry << 32) + product_lo_carry;

    return product_lo;
}
#endif

int64_t getus()
{
#if STD_HRC
    auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count();
#elif WIN32_RUS
    FILETIME ptime[4] = {0, 0, 0, 0, 0, 0, 0, 0};
    GetThreadTimes(GetCurrentThread(), &ptime[0], &ptime[2], &ptime[2], &ptime[3]);
    return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10;
#elif WIN32_TICK
    return GetTickCount() * 1000;
#elif WIN32_HTIME || _WIN320
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER nowus;
    QueryPerformanceCounter(&nowus);
    return (nowus.QuadPart * 1000000) / (freq.QuadPart);
#elif _WIN32
    FILETIME ft;
#if _WIN32_WINNT >= 0x0602
    GetSystemTimePreciseAsFileTime(&ft);
#else
    GetSystemTimeAsFileTime(&ft);
#endif  /* Windows 8  */

    int64_t t1 = (int64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
    /* Convert to UNIX epoch, 1970-01-01. Still in 100 ns increments. */
    t1 -= 116444736000000000ull;
    t1 = t1 / 10;
    return t1;
#elif LINUX_RUS
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000000ull + usec;
//#elif LINUX_TICK || __APPLE__
//    return clock();
#elif __linux__
    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
#elif __unix__
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec * 1000000ull + start.tv_usec;
#else
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count();
#endif
}

static inline uint32_t ilog(uint32_t x, uint32_t n = 2)
{
    uint32_t logn = 0;
    while (x / n) {
        logn ++;
        x /= n;
    }

    return logn;
}

static inline uint64_t randomseed() {
    std::random_device rd;
    std::mt19937_64 g(rd());
    return g();
}

#if __SIZEOF_INT128__
class Lehmer64 {
public:
    __uint128_t g_lehmer64_state = 1;
    uint64_t splitmix64_x; /* The state can be seeded wit// original documentation by Vigna:
    This is a fixed-increment version of Java 8's SplittableRandom generator
    See http://dx.doi.org/10.1145/2714064.2660195 and
http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

It is a very fast generator passing BigCrush, and it can be useful if
for some reason you absolutely want 64 bits of state; otherwise, we
rather suggest to use a xoroshiro128+ (for moderately parallel
computations) or xorshift1024* (for massively parallel computations)
generator. */

    Lehmer64(uint64_t seed) {
        splitmix64_seed(seed);
        g_lehmer64_state = (((__uint128_t)splitmix64_stateless(seed, 0)) << 64) + splitmix64_stateless(seed, 1);
    }

private:
    // call this one before calling splitmix64
    inline void splitmix64_seed(uint64_t seed) { splitmix64_x = seed; }

    // floor( ( (1+sqrt(5))/2 ) * 2**64 MOD 2**64)
#define GOLDEN_GAMMA UINT64_C(0x9E3779B97F4A7C15)

    // returns random number, modifies seed[0]
    // compared with D. Lemire against
    // http://grepcode.com/file/repository.grepcode.com/java/root/jdk/openjdk/8-b132/java/util/SplittableRandom.java#SplittableRandom.0gamma
    inline uint64_t splitmix64_r(uint64_t *seed) {
        uint64_t z = (*seed += GOLDEN_GAMMA);
        // David Stafford's Mix13 for MurmurHash3's 64-bit finalizer
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31);
    }

    // returns random number, modifies splitmix64_x
   inline uint64_t splitmix64(void) {
        return splitmix64_r(&splitmix64_x);
    }

    // returns the 32 least significant bits of a call to splitmix64
    // this is a simple (inlined) function call followed by a cast
    inline uint32_t splitmix64_cast32(void) {
        return (uint32_t)splitmix64();
    }

    // returns the value of splitmix64 "offset" steps from seed
    inline uint64_t splitmix64_stateless(uint64_t seed, uint64_t offset) {
        seed += offset*GOLDEN_GAMMA;
        return splitmix64_r(&seed);
    }/*h any value. */

public:
    inline uint64_t operator()() {
        g_lehmer64_state *= UINT64_C(0xda942042e4dd58b5);
        return g_lehmer64_state >> 64;
    }

    // this is a bit biased, but for our use case that's not important.
    uint64_t operator()(uint64_t boundExcluded) noexcept {
	(void) boundExcluded;
        g_lehmer64_state *= UINT64_C(0xda942042e4dd58b5);
        return g_lehmer64_state >> 64;
    }
};
#endif

class Orbit {
public:
    using result_type = uint64_t;

    static constexpr uint64_t(min)() {
        return 0;
    }
    static constexpr uint64_t(max)() {
        return UINT64_C(0xffffffffffffffff);
    }

    Orbit(uint64_t seed) noexcept
        : stateA(seed)
        , stateB(UINT64_C(0x9E6C63D0676A9A99)) {
        for (size_t i = 0; i < 10; ++i) {
            operator()();
        }
    }

    uint64_t operator()() noexcept {
        uint64_t s = (stateA += 0xC6BC279692B5C323u);
        uint64_t t = ((s == 0u) ? stateB : (stateB += 0x9E3779B97F4A7C15u));
        uint64_t z = (s ^ s >> 31) * ((t ^ t >> 22) | 1u);
        return z ^ z >> 26;
    }

    // this is a bit biased, but for our use case that's not important.
    uint64_t operator()(uint64_t boundExcluded) noexcept {
#ifdef __SIZEOF_INT128__
        return static_cast<uint64_t>((static_cast<unsigned __int128>(operator()()) * static_cast<unsigned __int128>(boundExcluded)) >> 64u);
#elif _WIN32
        uint64_t high;
        uint64_t a = operator()();
        _umul128(a, boundExcluded, &high);
        return high;
#endif
    }


private:
    static constexpr uint64_t rotl(uint64_t x, unsigned k) noexcept {
        return (x << k) | (x >> (64U - k));
    }

    uint64_t stateA;
    uint64_t stateB;
};

class RomuDuoJr {
public:
    using result_type = uint64_t;

    static constexpr uint64_t(min)() {
        return 0;
    }
    static constexpr uint64_t(max)() {
        return UINT64_C(0xffffffffffffffff);
    }

    RomuDuoJr(uint64_t seed) noexcept
        : mX(seed)
        , mY(UINT64_C(0x9E6C63D0676A9A99)) {
        for (size_t i = 0; i < 10; ++i) {
            operator()();
        }
    }

    uint64_t operator()() noexcept {
        uint64_t x = mX;

        mX = UINT64_C(15241094284759029579) * mY;
        mY = rotl(mY - x, 27);

        return x;
    }

    // this is a bit biased, but for our use case that's not important.
    uint64_t operator()(uint64_t boundExcluded) noexcept {
#ifdef __SIZEOF_INT128__
        return static_cast<uint64_t>((static_cast<unsigned __int128>(operator()()) * static_cast<unsigned __int128>(boundExcluded)) >> 64u);
#elif _WIN32
        uint64_t high;
        uint64_t a = operator()();
        _umul128(a, boundExcluded, &high);
        return high;
#endif
    }


private:
    static constexpr uint64_t rotl(uint64_t x, unsigned k) noexcept {
        return (x << k) | (x >> (64U - k));
    }

    uint64_t mX;
    uint64_t mY;
};

class Sfc4 {
public:
    using result_type = uint64_t;

    static constexpr uint64_t(min)() {
        return 0;
    }
    static constexpr uint64_t(max)() {
        return UINT64_C(0xffffffffffffffff);
    }

    Sfc4(uint64_t seed) noexcept
        : mA(seed)
        , mB(seed)
        , mC(seed)
        , mCounter(1) {
        for (size_t i = 0; i < 12; ++i) {
            operator()();
        }
    }

    uint64_t operator()() noexcept {
        uint64_t tmp = mA + mB + mCounter++;
        mA = mB ^ (mB >> 11U);
        mB = mC + (mC << 3U);
        mC = rotl(mC, 24U) + tmp;
        return tmp;
    }

        // this is a bit biased, but for our use case that's not important.
    uint64_t operator()(uint64_t boundExcluded) noexcept {
#ifdef __SIZEOF_INT128__
        return static_cast<uint64_t>((static_cast<unsigned __int128>(operator()()) * static_cast<unsigned __int128>(boundExcluded)) >> 64u);
#elif _WIN32
        uint64_t high;
        uint64_t a = operator()();
        _umul128(a, boundExcluded, &high);
        return high;
#endif
    }


private:
    static constexpr uint64_t rotl(uint64_t x, unsigned k) noexcept {
        return (x << k) | (x >> (64U - k));
    }

    uint64_t mA;
    uint64_t mB;
    uint64_t mC;
    uint64_t mCounter;
};

static inline uint64_t hashfib(uint64_t key)
{
#if __SIZEOF_INT128__
    __uint128_t r =  (__int128)key * UINT64_C(11400714819323198485);
    return (uint64_t)(r >> 64) ^ (uint64_t)r;
#elif _WIN64
    uint64_t high;
    return _umul128(key, UINT64_C(11400714819323198485), &high) ^ high;
#else
    uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
    return (r >> 32) + r;
#endif
}

static inline uint64_t hashmix(uint64_t key)
{
    auto ror  = (key >> 32) | (key << 32);
    auto low  = key * 0xA24BAED4963EE407ull;
    auto high = ror * 0x9FB21C651E98DF25ull;
    auto mix  = low + high;
    return (mix >> 32) | (mix << 32);
}

static inline uint64_t rrxmrrxmsx_0(uint64_t v)
{
    /* Pelle Evensen's mixer, https://bit.ly/2HOfynt */
    v ^= (v << 39 | v >> 25) ^ (v << 14 | v >> 50);
    v *= UINT64_C(0xA24BAED4963EE407);
    v ^= (v << 40 | v >> 24) ^ (v << 15 | v >> 49);
    v *= UINT64_C(0x9FB21C651E98DF25);
    return v ^ v >> 28;
}

static inline uint64_t hash_mur3(uint64_t key)
{
    //MurmurHash3Mixer
    uint64_t h = key;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

static inline uint64_t squirrel3(uint64_t at)
{
    constexpr uint64_t BIT_NOISE1 = 0x9E3779B185EBCA87ULL;
    constexpr uint64_t BIT_NOISE2 = 0xC2B2AE3D27D4EB4FULL;
    constexpr uint64_t BIT_NOISE3 = 0x27D4EB2F165667C5ULL;
    at *= BIT_NOISE1; at ^= (at >> 8);
    at += BIT_NOISE2; at ^= (at << 8);
    at *= BIT_NOISE3; at ^= (at >> 8);
    return at;
}

static inline uint64_t udb_splitmix64(uint64_t x)
{
    uint64_t z = x += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

#if __SSE4_2__ || _WIN32
#include <nmmintrin.h>
#elif defined(__aarch64__)
#include <arm_acle.h>
#include <arm_neon.h>
#endif

static inline uint64_t intHashCRC32(uint64_t x)
{
#if __SSE4_2__ || _WIN32
    return _mm_crc32_u64(-1ULL, x);
#elif defined(__aarch64__)
    return __crc32cd(-1U, x);
#else
    return x;
#endif
}

template<typename T>
struct Int64Hasher
{
    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    inline std::size_t operator()(T key) const
    {
#if FIB_HASH == 1
        return key;
#elif FIB_HASH == 2
        return hashfib(key);
#elif FIB_HASH == 3
        return hash_mur3(key);
#elif FIB_HASH == 4
        return hashmix(key);
#elif FIB_HASH == 5
        return rrxmrrxmsx_0(key);
#elif FIB_HASH == 6
        return squirrel3(key);
#elif FIB_HASH == 7
        return intHashCRC32(key);
#elif FIB_HASH == 9
        return udb_splitmix64(key);
#elif FIB_HASH > 10000
        return key % FIB_HASH; //bad hash
#elif FIB_HASH > 100
        return key * FIB_HASH; //bad hash
#elif FIB_HASH == 8
        return wyhash64(key, KC);
#else //staffort_mix13
        auto x = key;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
#endif
    }
};

template<class RandomIt>
void shuffle(RandomIt first, RandomIt last)
{
    std::random_device rd;
    std::mt19937 g(rd());
    typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;
    typedef std::uniform_int_distribution<diff_t> distr_t;
    typedef typename distr_t::param_type param_t;

    distr_t D;
    diff_t n = last - first;
    for (diff_t i = n-1; i > 0; --i) {
        using std::swap;
        swap(first[i], first[D(g, param_t(0, i))]);
    }
}

#if A_HASH
struct Ahash64er
{
    std::size_t operator()(const std::string& str) const
    {
        return ahash64(str.data(), str.size(), str.size());
    }
};

template<typename T>
struct Axxhasher
{
    std::size_t operator()(const T& str) const
    {
        ahash::Hasher hasher{1};
        hasher.consume(str.data(), str.size());
        return hasher.finalize();
    }
};
#endif

#if KOMI_HESH
struct KomiHasher
{
    std::size_t operator()(const std::string& str) const
    {
        return komihash(str.data(), str.size(), str.size());
    }
};
#endif

#if WYHASH_LITTLE_ENDIAN
struct WysHasher
{
    std::size_t operator()(const std::string& str) const
    {
        return wyhash(str.data(), str.size(), 0);
    }
};

struct WyIntHasher
{
    uint64_t seed;
    WyIntHasher(uint64_t seed1 = randomseed())
    {
        seed = seed1;
    }

    std::size_t operator()(size_t v) const
    {
        return wyhash64(v, seed);
    }
};

struct WyRand
{
    using result_type = uint64_t;
    uint64_t seed;
    WyRand(uint64_t seed1 = randomseed())
    {
        seed = seed1;
    }
    static constexpr uint64_t(min)() {
        return 0;
    }
    static constexpr uint64_t(max)() {
        return UINT64_C(0xffffffffffffffff);
    }

    uint64_t operator()()
    {
        return wyrand(&seed);
    }

    // this is a bit biased, but for our use case that's not important.
    uint64_t operator()(uint64_t boundExcluded) noexcept {
#ifdef __SIZEOF_INT128__
        return static_cast<uint64_t>((static_cast<unsigned __int128>(operator()()) * static_cast<unsigned __int128>(boundExcluded)) >> 64u);
#elif _WIN32
        uint64_t high;
        uint64_t a = operator()();
        _umul128(a, boundExcluded, &high);
        return high;
#endif
    }
};
#endif

static void cpuidInfo(int regs[4], int id, int ext)
{
#if X86
#if _MSC_VER >= 1600 //2010
    __cpuidex(regs, id, ext);
#elif __GNUC__
    __asm__ (
        "cpuid\n"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(id), "c"(ext)
    );
#elif ASM_X86
    __asm
    {
        mov eax, id
        mov ecx, ext
        cpuid
        mov edi, regs
        mov dword ptr [edi + 0], eax
        mov dword ptr [edi + 4], ebx
        mov dword ptr [edi + 8], ecx
        mov dword ptr [edi +12], edx
    }
#endif
#endif
}

static void printInfo(char* out)
{
    const char* sepator =
        "-----------------------------------------------------------------------------------------------------------------";

    puts(sepator);
    //    puts("Copyright (C) by 2019-2022 Huang Yuanbing bailuzhou at 163.com\n");

    char cbuff[1512] = {0};
    char* info = cbuff;
#ifdef __clang__
    info += snprintf(info, 20,  "clang %s", __clang_version__); //vc/gcc/llvm
#if __llvm__
    info += snprintf(info, 20,  " on llvm/");
#endif
#endif

#if _MSC_VER
    info += snprintf(info, 20,  "ms vc++ %d", _MSC_VER);
#elif __GNUC__
    info += snprintf(info, 20,  "gcc %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif __INTEL_COMPILER
    info += snprintf(info, 20,  "intel c++ %d", __INTEL_COMPILER);
#endif

#if __cplusplus > 199711L
    info += snprintf(info, 40,  " __cplusplus = %d", static_cast<int>(__cplusplus));
#endif

#if X86_64
    info += snprintf(info, 20,  " x86-64");
#elif X86_32
    info += snprintf(info, 20,  " x86");
#elif __arm64__ || __aarch64__
    info += snprintf(info, 20,  " arm64");
#elif __arm__
    info += snprintf(info, 20,  " arm");
#else
    info += snprintf(info, 20,  " unknow");
#endif

#if _WIN32
    info += snprintf(info, 20,  " OS = Win");
//    SetThreadAffinityMask(GetCurrentThread(), 0x1);
#elif __linux__
    info += snprintf(info, 20,  " OS = linux");
#elif __MAC__ || __APPLE__
    info += snprintf(info, 20,  " OS = mac");
#elif __unix__
    info += snprintf(info, 20,  " OS = unix");
#else
    info += snprintf(info, 20,  " OS = unknow");
#endif

#if X86
    info += snprintf(info, 40,  ", cpu = ");
    char vendor[0x40] = {0};
    int (*pTmp)[4] = (int(*)[4])vendor;
    cpuidInfo(*pTmp ++, 0x80000002, 0);
    cpuidInfo(*pTmp ++, 0x80000003, 0);
    cpuidInfo(*pTmp ++, 0x80000004, 0);

    info += snprintf(info, 40,  "%s", vendor);
#endif

    puts(cbuff);
    if (out)
#if _WIN32
        strncpy_s(out, sizeof(cbuff), cbuff, sizeof(cbuff) - 1);
#else
        strncpy(out, cbuff, sizeof(cbuff) - 1);
#endif

    puts(sepator);
}

static const std::array<char, 62> ALPHANUMERIC_CHARS = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

static std::uniform_int_distribution<std::size_t> rd_uniform(0, ALPHANUMERIC_CHARS.size() - 1);

static std::mt19937_64 generator(time(0));

#if TKey > 1
static std::string get_random_alphanum_string(std::size_t size) {
    std::string str(size, '\0');

    const auto comm_head = size % 1 + 0;
    //test common head
    for(std::size_t i = 0; i < comm_head; i++) {
        str[i] = ALPHANUMERIC_CHARS[i];
    }
    for(std::size_t i = comm_head; i < size; i++) {
        str[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    return str;
}
#endif

#if STR_VIEW
static char string_view_buf[1024 * 32] = {0};
static std::string_view get_random_alphanum_string_view(std::size_t size) {

    if (string_view_buf[0] == 0) {
        for(std::size_t i = 0; i < sizeof(string_view_buf); i++)
            string_view_buf[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    auto start = generator() % (sizeof(string_view_buf) - size - 1);
    return {string_view_buf + start, size};
}
#endif

#if X86
#define  ABSL_INTERNAL_RAW_HASH_SET_HAVE_SSSE3 1
#define  ABSL_INTERNAL_RAW_HASH_SET_HAVE_SSSE2 1
#endif

#if ABSL_HMAP
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/internal/raw_hash_set.cc"
#include "absl/hash/internal/low_level_hash.cc"
#include "absl/hash/internal/hash.cc"
#include "absl/hash/internal/city.cc"

#if ABSL_HASH
#include "absl/crc/internal/crc_x86_arm_combined.cc"
#endif
#endif

#if __cplusplus > 201402L || CXX17 || _MSC_VER > 1730
#define CXX17 1
#include "fph/dynamic_fph_table.h" //https://github.com/renzibei/fph-table
#include "fph/meta_fph_table.h"
#include "jg/dense_hash_map.hpp" //https://github.com/Jiwan/dense_hash_map
#endif

#if __cplusplus > 201704L || CXX20 || _MSC_VER >= 1929
#if QC_HASH
#include "qchash/qc-hash.hpp" //https://github.com/daskie/qc-hash
#endif
#define CXX20 1
#include "rigtorp/rigtorp.hpp"   //https://github.com/rigtorp/HashMap/blob/master/include/rigtorp/HashMap.h

//#include "ck/Common/HashTable/HashMap.h"
//#include "ck/Common/HashTable/HashSet.h"
#endif
