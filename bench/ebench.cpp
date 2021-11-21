#include <random>
#include <map>
#include <ctime>
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
    # define NOMINMAX
#endif
    # define _CRT_SECURE_NO_WARNINGS 1
    # include <windows.h>
#else
    # include <unistd.h>
    # include <sys/resource.h>
    # include <sys/time.h>
#endif

#if __cplusplus > 201402L || _MSVC_LANG >= 201402L
   #define STR_VIEW  1
   #include <string_view>
#endif

#ifdef __has_include
    #if __has_include("wyhash.h")
    #include "wyhash.h"
    #endif
#endif

#ifndef TKey
    #define TKey              1
#endif
#ifndef TVal
    #define TVal              1
#endif
#if __GNUC__ > 4 && __linux__
#include <ext/pb_ds/assoc_container.hpp>
#endif

#define ET                 1
#if STR_SIZE < 5
#define STR_SIZE 15
#endif

static void printInfo(char* out);
std::map<std::string, std::string> maps =
{
//    {"stl_hash", "unordered_map"},
    {"stl_map", "stl_map"},
    {"btree", "btree_map"},

    {"emhash2", "emhash2"},
    {"emhash3", "emhash3"},
    {"emhash4", "emhash4"},

    {"emhash5", "emhash5"},
//    {"emhash6", "emhash6"},
    {"emhash7", "emhash7"},
//    {"emhash8", "emhash8"},

//    {"lru_time", "lru_time"},
    {"lru_size", "lru_size"},

    {"emilib2", "emilib2"},
//    {"emilib4", "emilib4"},
//    {"emilib3", "emilib3"},
    //    {"ktprime", "ktprime"},
    {"fht", "fht"},
    {"absl", "absl_flat"},
//    {"f14_value", "f14_value"},
    {"f14_vector", "f14_vector"},
    {"cuckoo", "cuckoo hash"},

#if ET
    {"zhashmap", "zhashmap"},
    {"martin", "martin_flat"},
    {"phmap", "phmap_flat"},
//    {"hrdset", "hrdset"},

    {"robin", "tsl_robin"},
    {"flat", "ska_flat"},

    {"hopsco", "tsl_hopsco"},
    {"byte", "ska_byte"},
#endif
};

#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
#define PHMAP_HAVE_SSSE3      1
#define ABSL_INTERNAL_RAW_HASH_SET_HAVE_SSSE3 1
#endif

//rand data type
#ifndef RT
    #define RT 2  //1 wyrand 2 sfc64 3 RomuDuoJr 4 Lehmer64 5 mt19937_64
#endif

//#define NDEBUG                1
#ifndef _MSC_VER
//    #define ABSL            1
//    #define ABSL_HASH       1
#endif

//#define CUCKOO_HASHMAP       1
//#define EM3               1
//#define PHMAP_HASH        1
#if WYHASH_LITTLE_ENDIAN
//    #define WY_HASH         1
#endif

//#define MIX_HASH            1
//#define HOOD_HASH           1
//#define FL1                 1

//feature of emhash
//#define EMH_FIBONACCI_HASH  1
//#define EMH_BUCKET_INDEX    2
//#define EMH_REHASH_LOG      1234567
//#define EMH_AVX_MEMCPY      1

//#define EMH_STATIS          1
//#define EMH_SAFE_ITER       1
//#define EMH_SAFE_HASH       1
//#define EMH_IDENTITY_HASH   1

//#define EMH_LRU_SET         1
//#define EMH_STD_STRING      1
//#define EMH_ERASE_SMALL     1
//#define EMH_BDKR_HASH       1
//#define EMH_HIGH_LOAD         2345


#ifdef EM3
#include "old/hash_table2.hpp"
#include "old/hash_table3.hpp"
#include "old/hash_table4.hpp"
#endif
#include "hash_table5.hpp"
//#include "old/hash_table557.hpp"
#include "hash_table6.hpp"
#include "hash_table7.hpp"
#include "hash_table8.hpp"

//https://zhuanlan.zhihu.com/p/363213858
//https://www.zhihu.com/question/46156495
//http://www.brendangregg.com/index.html

//https://eourcs.github.io/LockFreeCuckooHash/
//https://lemire.me/blog/2018/08/15/fast-strongly-universal-64-bit-hashing-everywhere/
////some others
//https://sites.google.com/view/patchmap/overview
//https://github.com/ilyapopov/car-race
//https://hpjansson.org/blag/2018/07/24/a-hash-table-re-hash/
//https://www.reddit.com/r/cpp/comments/auwbmg/hashmap_benchmarks_what_should_i_add/
//https://www.youtube.com/watch?v=M2fKMP47slQ
//https://yq.aliyun.com/articles/563053

//https://engineering.fb.com/developer-tools/f14/
//https://github.com/facebook/folly/blob/master/folly/container/F14.md

//https://martin.ankerl.com/2019/04/01/hashmap-benchmarks-01-overview/
//https://martin.ankerl.com/2016/09/21/very-fast-hashmap-in-c-part-3/

//https://attractivechaos.wordpress.com/2018/01/13/revisiting-hash-table-performance/
//https://attractivechaos.wordpress.com/2019/12/28/deletion-from-hash-tables-without-tombstones/#comment-9548
//https://attractivechaos.wordpress.com/2008/08/28/comparison-of-hash-table-libraries/
//https://en.wikipedia.org/wiki/Hash_table
//https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html
//https://probablydance.com/2017/02/26/i-wrote-the-fastest-hashtable/
//https://andre.arko.net/2017/08/24/robin-hood-hashing/
//http://www.ilikebigbits.com/2016_08_28_hash_table.html
//http://www.idryman.org/blog/2017/05/03/writing-a-damn-fast-hash-table-with-tiny-memory-footprints/
//https://jasonlue.github.io/algo/2019/08/27/clustered-hashing-basic-operations.html
//https://bigdata.uni-saarland.de/publications/p249-richter.pdf

#if __linux__ && AVX2
#include <sys/mman.h>
#include "fht/fht_ht.hpp" //https://github.com/goldsteinn/hashtable_test/blob/master/my_table/fht_ht.hpp
#define FHT_HMAP          1
#endif

#if ABSL
//#define _HAS_DEPRECATED_RESULT_OF 1
#include "absl/container/flat_hash_map.h"
#include "absl/container/internal/raw_hash_set.cc"
#endif

#if ABSL_HASH
#include "absl/hash/internal/low_level_hash.cc"
#include "absl/hash/internal/hash.cc"
#include "absl/hash/internal/city.cc"
#endif

#if FOLLY
#include "folly/container/F14Map.h"
#endif

#if CUCKOO_HASHMAP
#include "libcuckoo/cuckoohash_map.hh"
#endif

#if HOOD_HASH
    #include "martin/robin_hood.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#endif
#if PHMAP_HASH
    #include "phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap
#endif

#if 1
#include "ahash/ahash.c"
#include "ahash/random_state.c"
#endif

#if ET
    #include "emilib/emilib32.hpp"
    #include "zhashmap/hashmap.h"

#if __x86_64__ || _WIN64
    #include "hrd/hash_set7.h"        //https://github.com/hordi/hash/blob/master/include/hash_set7.h
    #include "emilib/emilib12.hpp"
    #include "emilib/emilib33.hpp"
    #include "ska/flat_hash_map.hpp"  //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
#endif

    #include "tsl/robin_map.h"        //https://github.com/tessil/robin-map

    #include "lru_size.h"
    #include "lru_time.h"
    #include "phmap/btree.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap
#if ET > 1
    #include "tsl/hopscotch_map.h"    //https://github.com/tessil/hopscotch-map
    #include "ska/bytell_hash_map.hpp"//https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#endif
    #include "phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap
    #include "martin/robin_hood.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#endif

#include <unordered_map>


#ifndef PACK
#define PACK 128
#endif
struct StructValue
{
    StructValue(int64_t i = 0)
    {
        lScore = i;
        lUid = 0;
        iRank = iUpdateTime = 0;
    }

    bool operator == (const StructValue& v) const
    {
        return v.lScore == lScore;
    }

    int64_t operator ()() const
    {
        return lScore;
    }

    bool operator < (const StructValue& r) const
    {
        return lScore < r.lScore;
    }

    int64_t lUid;
    int64_t lScore;
    int  iUpdateTime;
    int  iRank;

#if PACK >= 24
    char data[(PACK - 24) / 8 * 8];
#endif

#if VCOMP
    std::string sdata = {"test data input"};
    std::vector<int> vint = {1,2,3,4,5,6,7,8};
    std::map<std::string, int> msi = {{"111", 1}, {"1222", 2}};
#endif
};

struct StuHasher
{
    std::size_t operator()(const StructValue& v) const
    {
        return v.lScore * 11400714819323198485ull;
    }
};

#if BAD_HASH > 100
template <typename KeyT>
struct BadHasher
{
    std::size_t operator()(const KeyT& v) const
    {
#if 1
        return v % BAD_HASH;
#else
        return v.size();
#endif
    }
};
#endif

#if TKey == 0
#ifndef SMK
    typedef unsigned int keyType;
    #define sKeyType    "int"
#else
    typedef short        keyType;
    #define sKeyType    "short"
#endif
    #define TO_KEY(i)   (keyType)i
    #define KEY_INT     1
#elif TKey == 1
    typedef int64_t      keyType;
    #define TO_KEY(i)   (keyType)i
    #define sKeyType    "int64_t"
    #define KEY_INT     1
#elif TKey == 2
    #define KEY_STR     1
    typedef std::string keyType;
    #define TO_KEY(i)   std::to_string(i)
    #define sKeyType    "string"
#elif TKey == 3
    #define KEY_CLA    1
    typedef StructValue keyType;
    #define TO_KEY(i)   StructValue((int64_t)i)
    #define sKeyType    "Struct"
#else
    #define KEY_STR     1
    typedef std::string_view keyType;
    #define TO_KEY(i)   std::to_string(i)
    #define sKeyType    "string_view"
#endif

#if TVal == 0
    typedef int         valueType;
    #define TO_VAL(i)   i
    #define TO_SUM(i)   i
    #define sValueType  "int"
#elif TVal == 1
    typedef int64_t     valueType;
    #define TO_VAL(i)   i
    #define TO_SUM(i)   i
    #define sValueType  "int64_t"
#elif TVal == 2
    typedef std::string valueType;
    #define TO_VAL(i)   #i
    #define TO_SUM(i)   i.size()
    #define sValueType  "string"
#elif TValue == 3
    typedef std::string_view valueType;
    #define TO_VAL(i)   #i
    #define TO_SUM(i)   i.size()
    #define sValueType  "string_view"
#else
    typedef StructValue valueType;
    #define TO_VAL(i)   i
    #define TO_SUM(i)   i.lScore
    #define sValueType  "Struct"
#endif

static int64_t getus()
{
#if 0
    auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count();

#elif WIN32_RSU
    FILETIME ptime[4] = {0, 0, 0, 0, 0, 0, 0, 0};
    GetThreadTimes(GetCurrentThread(), NULL, NULL, &ptime[2], &ptime[3]);
    return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10;
#elif WIN32_TICK
    return GetTickCount() * 1000;
#elif WIN32_HTIME || _WIN32
    static LARGE_INTEGER freq = {0, 0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }

    LARGE_INTEGER nowus;
    QueryPerformanceCounter(&nowus);
    return (nowus.QuadPart * 1000000) / (freq.QuadPart);
#elif WIN32_STIME
    FILETIME    file_time;
    SYSTEMTIME  system_time;
    ULARGE_INTEGER ularge;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    ularge.LowPart  = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;
    return ularge.QuadPart / 10 + system_time.wMilliseconds / 1000;
#elif LINUX_RUS
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000000 + usec;
#elif LINUX_TICK || __APPLE__
    return clock();
#elif __linux__
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000'000 + ts.tv_nsec / 1000;
#elif __unix__
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec * 1'000'000ull + start.tv_usec;
#else
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count();
#endif
}

static int ilog(int x, const int n = 2)
{
    int logn = 0;
    while (x / n) {
        logn ++;
        x /= n;
    }

    return logn;
}

static uint64_t randomseed() {
    std::random_device rd;
    return std::uniform_int_distribution<uint64_t>{}(rd);
}

#if __SIZEOF_INT128__
class Lehmer64 {
    __uint128_t g_lehmer64_state;

    uint64_t splitmix64_x; /* The state can be seeded with any value. */

    public:
    // call this one before calling splitmix64
    Lehmer64(uint64_t seed) { splitmix64_x = seed; }

    // returns random number, modifies splitmix64_x
    // compared with D. Lemire against
    // http://grepcode.com/file/repository.grepcode.com/java/root/jdk/openjdk/8-b132/java/util/SplittableRandom.java#SplittableRandom.0gamma
    inline uint64_t splitmix64(void) {
        uint64_t z = (splitmix64_x += UINT64_C(0x9E3779B97F4A7C15));
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31);
    }

    // returns the 32 least significant bits of a call to splitmix64
    // this is a simple function call followed by a cast
    inline uint32_t splitmix64_cast32(void) {
        return (uint32_t)splitmix64();
    }

    // same as splitmix64, but does not change the state, designed by D. Lemire
    inline uint64_t splitmix64_stateless(uint64_t index) {
        uint64_t z = (index + UINT64_C(0x9E3779B97F4A7C15));
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31);
    }

    inline void lehmer64_seed(uint64_t seed) {
        g_lehmer64_state = (((__uint128_t)splitmix64_stateless(seed)) << 64) +
            splitmix64_stateless(seed + 1);
    }

    inline uint64_t operator()() {
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

private:
    static constexpr uint64_t rotl(uint64_t x, unsigned k) noexcept {
        return (x << k) | (x >> (64U - k));
    }

    uint64_t mA;
    uint64_t mB;
    uint64_t mC;
    uint64_t mCounter;
};

// this is probably the fastest high quality 64bit random number generator that exists.
// Implements Small Fast Counting v4 RNG from PractRand.
#include <chrono>
static const std::array<char, 62> ALPHANUMERIC_CHARS = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

std::uniform_int_distribution<std::size_t> rd_uniform(0, ALPHANUMERIC_CHARS.size() - 1);


static std::mt19937_64 generator(time(0));
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

static int test_case = 0;
static int loop_vector_time = 0;
static int func_index = 1, func_print = 0;

static std::map<std::string, int64_t> func_result;
//func:hash -> time
static std::map<std::string, std::map<std::string, int64_t>> once_func_hash_time;

static void check_func_result(const std::string& hash_name, const std::string& func, size_t sum, int64_t ts1, int weigh = 1)
{
    if (func_result.find(func) == func_result.end()) {
        func_result[func] = sum;
    } else if (sum != func_result[func]) {
        printf("%s %s %zd != %zd (o)\n", hash_name.c_str(), func.c_str(), (size_t)sum, (size_t)func_result[func]);
    }

    auto& showname = maps[hash_name];
    once_func_hash_time[func][showname] += (getus() - ts1) / weigh;
    func_index ++;

    long ts = (getus() - ts1) / 1000;
    if (func_index == func_print)
        printf("%8s: %8s %4ld, ",hash_name.data(), func.c_str(), ts);
    else if (func_index == func_print + 1)
        printf("%8s %4ld, ", func.c_str(), ts);
    else if (func_index == func_print + 2)
        printf("%8s %4ld, ", func.c_str(), ts);
    else if (func_index == func_print + 3)
        printf("%8s %4ld\n", func.c_str(), ts);
}

static void inline hash_convert(const std::map<std::string, int64_t>& hash_score, std::multimap <int64_t, std::string>& score_hash)
{
    for (const auto& v : hash_score)
        score_hash.insert(std::pair<int64_t, std::string>(v.second, v.first));
}

static void add_hash_func_time(std::map<std::string, std::map<std::string, int64_t>>& func_hash_score, std::multimap <int64_t, std::string>& once_score_hash, int vsize)
{
    std::map<std::string, int64_t> once_hash_score;
    for (auto& v : once_func_hash_time) {
        int64_t maxv = v.second.begin()->second;
        for (auto& h : v.second) {
            if (h.second > maxv)
                maxv = h.second;
        }

        for (auto& f : v.second) {
            const auto score = (100ull * f.second) / maxv; //f.second;
            func_hash_score[v.first][f.first] += score;
            once_hash_score[f.first]          += score;
        }
    }
    hash_convert(once_hash_score, once_score_hash);

    const auto last  = double(once_score_hash.rbegin()->first);
    const auto first = double(once_score_hash.begin()->first);
    //print once score
    for (auto& v : once_score_hash) {
        printf("%5d   %13s   (%4.2lf %6.1lf%%)\n", int(v.first / (func_index - 1)), v.second.c_str(), last * 1.0 / v.first, first * 100.0 / v.first);
    }
}

static void dump_func(const std::string& func, const std::map<std::string, int64_t >& hash_rtime, std::map<std::string, int64_t>& hash_score, std::map<std::string, std::map<std::string, int64_t>>& hash_func_score)
{
    std::multimap <int64_t, std::string> rscore_hash;
    hash_convert(hash_rtime, rscore_hash);

    puts(func.c_str());

    auto mins = rscore_hash.begin()->first;
    for (auto& v : rscore_hash) {
        hash_score[v.second] += (int)((mins * 100) / (v.first + 0));

        //hash_func_score[v.second][func] = (int)((mins * 100) / (v.first + 1));
        hash_func_score[v.second][func] = v.first / test_case;
        printf("   %-8d     %-21s   %02d\n", (int)(v.first / test_case), v.second.c_str(), (int)((mins * 100) / v.first));
    }
    putchar('\n');
}

static void dump_all(std::map<std::string, std::map<std::string, int64_t>>& func_rtime, std::multimap<int64_t, std::string>& score_hash)
{
    std::map<std::string, int64_t> hash_score;
    std::map<std::string, std::map<std::string, int64_t>> hash_func_score;
    for (const auto& v : func_rtime) {
        dump_func(v.first, v.second, hash_score, hash_func_score);
    }
    hash_convert(hash_score, score_hash);

    if (test_case % 100 != 0)
        return;

    std::string pys =
    "import numpy as np\n"
    "import matplotlib.pyplot as plt\n\n"
    "def autolabel(rects):\n"
        "\tfor rect in rects:\n"
        "\t\twidth = rect.get_width()\n"
        "\t\tplt.text(width + 1.0, rect.get_y(), '%s' % int(width))\n\n"
    "divisions = [";

    pys.reserve(2000);
    for (const auto& v : func_rtime) {
        pys += "\"" + v.first + "\",";
    }

    pys.back() = ']';
    pys += "\n\n";

    auto hash_size = hash_func_score.size();
    auto func_size = func_rtime.size();

    pys += "plt.figure(figsize=(14," + std::to_string(func_size) + "))\n";
    pys += "index = np.arange(" + std::to_string(func_size) + ")\n";
    if (hash_size > 4)
        pys += "width = " + std::to_string(0.8 / hash_size) + "\n\n";
    else
        pys += "width = 0.20\n\n";

    std::string plt;
    int id = 0;
    static std::vector<std::string> colors = {
        "cyan", "magenta",
        "green", "red", "blue", "yellow", "black", "orange", "brown", "grey", "pink",
        "#eeefff", "burlywood",
    };

    for (auto& kv : hash_func_score) {
        pys += kv.first + "= [";
        for (auto& vk : kv.second) {
            pys += std::to_string(vk.second) + ",";
        }
        pys.back() = ']';
        pys += "\n";

        plt += "a" + std::to_string(id + 1) + " = plt.barh(index + width * " + std::to_string(id) + "," + kv.first +
            ",width, label = \"" + kv.first + "\")\n";
        plt += "autolabel(a" + std::to_string(id + 1) + ")\n\n";
        id ++;
    }

    //os
    char os_info[2048]; printInfo(os_info);

    pys += "\n" + plt + "\n";
    auto file = std::string(sKeyType) + "_" + sValueType;
    pys += std::string("file = \"") + file + ".png\"\n\n";
    pys += std::string("plt.title(\"") + file + "-" + std::to_string(test_case) + "\")\n";
    pys +=
        "plt.xlabel(\"performance\")\n"
        "plt.xlabel(\"" + std::string(os_info) + "\")\n"
        "plt.yticks(index + width / 2, divisions)\n"
        "plt.legend()\n"
        "plt.show()\n"
        "plt.savefig(file)\n";

    pys += std::string("\n\n# ") + os_info;

    //puts(pys.c_str());
    std::ofstream  outfile;
    auto full_file = file + ".py";
    outfile.open("./" + full_file, std::fstream::out | std::fstream::trunc | std::fstream::binary);
    if (outfile.is_open())
        outfile << pys;
    else
        printf("\n\n =============== can not open %s ==============\n\n", full_file.c_str());

    outfile.close();
}

template<class hash_type>
void hash_iter(hash_type& ht_hash, const std::string& hash_name)
{
    auto ts1 = getus(); size_t sum = 0;
    for (auto& v : ht_hash)
        sum += TO_SUM(v.second);

    for (auto it = ht_hash.cbegin(); it != ht_hash.cend(); ++it)
#if KEY_INT
        sum += it->first;
#elif KEY_CLA
    sum += it->first.lScore;
#else
    sum += it->first.size();
#endif
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_reinsert(hash_type& ht_hash, const std::string& hash_name, std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        ht_hash[v] = TO_VAL(0);
        //ht_hash.emplace(v, TO_VAL(0));
#if TVal < 2
        sum += ht_hash.count(v);
#else
        sum += TO_SUM(ht_hash[v]);
#endif
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void insert_erase(const std::string& hash_name, const std::vector<keyType>& vList)
{
#if TKey < 2
    hash_type ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        auto v2 = v % (1 << 12);
        auto r = ht_hash.emplace(v2, TO_VAL(0)).second;
        if (!r) {
            ht_hash.erase(v2);
            sum ++;
        }
    }

    ht_hash.clear();
    const auto vsize = vList.size() / 8;
    for (size_t i = 0; i < vList.size(); i++) {
        sum += ht_hash.emplace(vList[i], TO_VAL(0)).second;
        if (i > vsize)
            ht_hash.erase(vList[i - vsize]);
    }

    check_func_result(hash_name, __FUNCTION__, sum, ts1);
#endif
}

template<class hash_type>
void insert_no_reserve( const std::string& hash_name, const std::vector<keyType>& vList)
{
    hash_type ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ht_hash.emplace(v, TO_VAL(0)).second;
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void insert_reserve(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
#ifndef SMAP
    ht_hash.reserve(vList.size());
    ht_hash.max_load_factor(0.99f);
#endif

    for (const auto& v : vList)
        sum += ht_hash.emplace(v, TO_VAL(0)).second;
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void multi_small_bench(const std::string& hash_name, const std::vector<keyType>& vList)
{
#if KEY_INT
    size_t sum = 0;
    const auto hash_size = vList.size() / 10003 + 2021;
    const auto ts1 = getus();

    auto mh = new hash_type[hash_size];
    for (const auto& v : vList) {
        auto hash_id = ((uint64_t)v) % hash_size;
        sum += mh[hash_id].emplace(v, TO_VAL(0)).second;
    }

    for (const auto& v : vList) {
        auto hash_id = ((uint64_t)v) % hash_size;
        sum += mh[hash_id].count(v + v % 2);
    }

    for (const auto& v : vList) {
        auto hash_id = ((uint64_t)v) % hash_size;
        sum += mh[hash_id].erase(v + v % 2);
    }

    delete []mh;
    check_func_result(hash_name, __FUNCTION__, sum, ts1, 2);
#endif
}

template<class hash_type>
void insert_find_erase(const hash_type& ht_hash, const std::string& hash_name, std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    hash_type tmp(ht_hash);

    for (auto & v : vList) {
#if KEY_INT
        auto v2 = v / 101 + v;
#elif KEY_CLA
        auto v2(v.lScore / 101 + v.lScore);
#elif TKey != 4
        v += char(128 + (int)v[0]);
        const auto &v2 = v;
#else
        const keyType v2(v.data(), v.size() - 1);
#endif
        sum += tmp.emplace(v2, TO_VAL(0)).second;
        sum += tmp.count(v2);
        sum += tmp.erase(v2);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1, 3);
}

template<class hash_type>
void insert_cache_size(const std::string& hash_name, const std::vector<keyType>& vList, const char* level, const uint32_t min_size, const uint32_t cache_size)
{
    auto ts1 = getus(); size_t sum = 0;
    const auto smalls = min_size + vList.size() % cache_size;
    hash_type tmp, empty;
#ifndef SMAP
    //empty.max_load_factor(0.875);
#endif
    tmp = empty;

    for (const auto& v : vList)
    {
        sum += tmp.emplace(v, TO_VAL(0)).second;
        //sum += tmp.count(v);
        if (tmp.size() > smalls) {
            if (smalls % 2 == 0)
                tmp.clear();
            else
                tmp = empty;
        }
    }
    check_func_result(hash_name, level, sum, ts1);
}

template<class hash_type>
void insert_high_load(const std::string& hash_name, const std::vector<keyType>& vList)
{
    size_t sum = 0;
    size_t pow2 = 2u << ilog(vList.size(), 2);
    hash_type tmp;

    const auto max_loadf = 0.99f;
#ifndef SMAP
    tmp.reserve(pow2 / 2);
    tmp.max_load_factor(max_loadf);
#endif
    int minn = (max_loadf - 0.2f) * pow2, maxn = int(max_loadf * pow2);
    int i = 0;

    for (; i < minn; i++) {
        if (i < (int)vList.size())
            tmp.emplace(vList[i], TO_VAL(0));
        else {
            auto& v = vList[i - vList.size()];
#if KEY_INT
            auto v2 = v + (v / 11) + i;
#elif KEY_CLA
            auto v2 = v.lScore + (v.lScore / 11) + i;
#elif TKey != 4
            auto v2 = v; v2[0] += '2';
#else
            const keyType v2(v.data(), v.size() - 1);
#endif
            tmp.emplace(v2, TO_VAL(0));
        }
    }

    auto ts1 = getus();
    for (; i  < maxn; i++) {
        auto& v = vList[i - minn];
#if KEY_INT
        auto v2 = (v / 7) + 4 * v;
#elif KEY_CLA
        auto v2 = (v.lScore / 7) + 4 * v.lScore;
#elif TKey != 4
        auto v2 = v; v2[0] += '1';
#else
        const keyType v2(v.data(), v.size() - 1);
#endif
        tmp[v2] = TO_VAL(0);
        sum += tmp.count(v2);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

#if FL1
static uint8_t l1_cache[64 * 1024];
#endif
template<class hash_type>
void find_miss_all(hash_type& ht_hash, const std::string& hash_name)
{
    auto ts1 = getus();
    auto n = ht_hash.size();
    size_t pow2 = 2u << ilog(n, 2), sum = 0;

#if KEY_STR
    std::string skey = get_random_alphanum_string(STR_SIZE);
#endif

    for (size_t v = 0; v < pow2; v++) {
#if FL1
        l1_cache[v % sizeof(l1_cache)] = 0;
#endif
#if KEY_STR
        skey[v % STR_SIZE + 1] ++;
        sum += ht_hash.count((const char*)skey.c_str());
#else
        sum += ht_hash.count(TO_KEY(v));
#endif
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_hit_50(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
#if FL1
        if (sum % (1024 * 256) == 0)
            memset(l1_cache, 0, sizeof(l1_cache));
#endif
        sum += ht_hash.count(v);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_hit_all(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
#if KEY_INT
        sum += ht_hash.count(v) + (size_t)v;
#elif KEY_CLA
        sum += ht_hash.count(v) + (size_t)v.lScore;
#else
        sum += ht_hash.count(v) + v.size();
#endif
#if FL1
        if (sum % (1024 * 64) == 0)
            memset(l1_cache, 0, sizeof(l1_cache));
#endif

    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_find_50(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ht_hash.count(v);
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_50(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto tmp = ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ht_hash.erase(v);

#if !(AVX2 | ABSL | CUCKOO_HASHMAP)
    for (auto it = tmp.begin(); it != tmp.end(); ) {
        it = tmp.erase(it);
        sum += 1;
    }
#endif
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void hash_clear(hash_type& ht_hash, const std::string& hash_name)
{
    if (ht_hash.size() > 1000'000) {
        auto ts1 = getus();
        size_t sum = ht_hash.size();
        ht_hash.clear(); ht_hash.clear();
        check_func_result(hash_name, __FUNCTION__, sum, ts1);
    }
}

template<class hash_type>
void copy_clear(hash_type& ht_hash, const std::string& hash_name)
{
    size_t sum = 0;
    auto ts1 = getus();
    hash_type thash = ht_hash;
    ht_hash = thash;
    thash = thash;
    ht_hash = std::move(thash);
    sum  += ht_hash.size();
    ht_hash.clear(); ht_hash.clear();
    sum  += ht_hash.size();
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

#if PACK >= 24 && VCOMP == 0
static_assert(sizeof(StructValue) == PACK, "PACK >=24");
#endif

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

#if WYHASH_LITTLE_ENDIAN
struct WysHasher
{
    std::size_t operator()(const std::string& str) const
    {
        return wyhash(str.c_str(), str.size(), str.size());
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
    uint64_t seed;
    WyRand(uint64_t seed1 = randomseed())
    {
        seed = seed1;
    }

    uint64_t operator()()
    {
        return wyrand(&seed);
    }
};
#endif

struct IntMixHasher
{
    std::size_t operator()(uint64_t key) const
    {
        auto ror  = (key >> 32) | (key << 32);
        auto low  = key * 0xA24BAED4963EE407ull;
        auto high = ror * 0x9FB21C651E98DF25ull;
        auto mix  = low + high;
        return mix;
    }
};

//https://en.wikipedia.org/wiki/Gamma_distribution#/media/File:Gamma_distribution_pdf.svg
//https://blog.csdn.net/luotuo44/article/details/33690179
static int buildTestData(int size, std::vector<keyType>& randdata)
{
    randdata.reserve(size);

#if RT == 2
    Sfc4 srng(size);
#elif WYHASH_LITTLE_ENDIAN && RT == 1
    WyRand srng(size);
#elif RT == 3
    RomuDuoJr srng(size);
#elif (RT == 4 && __SIZEOF_INT128__)
    Lehmer64 srng(size);
#else
    std::mt19937_64 srng; srng.seed(size);
#endif

#ifdef KEY_STR
    for (int i = 0; i < size; i++)
        randdata.emplace_back(get_random_alphanum_string(srng() % STR_SIZE + 4));
    return 0;
#else

#if AR > 0
    const auto iRation = AR;
#else
    const auto iRation = 1;
#endif

    auto flag = 0;
    if (srng() % 100 >= iRation)
    {
        for (int i = 0; i < size ; i++) {
            auto key = TO_KEY(srng());
            randdata.emplace_back(key);
//            if (randdata.size() >= size)
//                break;
        }
    }
    else
    {
        flag = srng() % 5 + 1;
        const auto pow2 = 2u << ilog(size, 2);
        auto k = srng();
        for (int i = 1; i <= size; i ++) {
            k ++;
            if (flag == 2)
                k += (1 << 8) - 1;
            else if (flag == 3) {
                k += pow2 + 32 - srng() % 64;
                if (srng() % 64 == 0)
                    k += 80;
            }
            else if (flag == 4) {
                if (srng() % 32 == 0)
                    k += 32;
            } else if (flag == 5) {
                k = i * (uint64_t)pow2 + srng() % (pow2 / 8);
            }

            randdata.emplace_back(k);
        }
    }

    return flag;
#endif
}

static int TestHashMap(int n, int max_loops = 1234567)
{
#ifndef KEY_CLA
    emhash5::HashMap <keyType, int> ehash5;
    emhash6::HashMap <keyType, int> ehash2;

    Sfc4 srng(time(NULL));
#if 1
    emhash7::HashMap <keyType, int> unhash;
#else
    std::unordered_map<long,int> unhash;
#endif

    const auto step = n % 2 + 1;
    for (int i = 1; i < n * step; i += step) {
        auto ki = TO_KEY(i);
        ehash5[ki] = unhash[ki] = ehash2[ki] = (int)srng();
    }

    int loops = max_loops;
    while (loops -- > 0) {
        assert(ehash2.size() == unhash.size()); assert(ehash5.size() == unhash.size());

        const uint32_t type = srng() % 100;
        auto rid  = n ++;
        auto id   = TO_KEY(rid);
        if (type <= 40 || ehash2.size() < 1000) {
            ehash2[id] += type; ehash5[id] += type; unhash[id] += type;

            assert(ehash2[id] == unhash[id]); assert(ehash5[id] == unhash[id]);
        }
        else if (type < 60) {
            if (srng() % 3 == 0)
                id = unhash.begin()->first;
            else if (srng() % 2 == 0)
                id = ehash2.begin()->first;
            else
                id = ehash5.begin()->first;

            ehash5.erase(ehash5.find(id));
            unhash.erase(id);
            ehash2.erase(id);

            assert(ehash5.count(id) == unhash.count(id));
            assert(ehash2.count(id) == unhash.count(id));
        }
        else if (type < 80) {
            auto it = ehash5.begin();
            for (int i = n % 64; i > 0; i--)
                it ++;
            id = it->first;
            unhash.erase(id);
            ehash2.erase(ehash2.find(id));
            ehash5.erase(it);
            assert(ehash2.count(id) == 0);
            assert(ehash5.count(id) == unhash.count(id));
        }
        else if (type < 100) {
            if (unhash.count(id) == 0) {
                const auto vid = (int)rid;
                ehash5.emplace(id, vid);
                assert(ehash5.count(id) == 1);
                assert(ehash2.count(id) == 0);

                //if (id == 1043)
                ehash2.insert(id, vid);
                assert(ehash2.count(id) == 1);

                unhash[id] = ehash2[id];
                assert(unhash[id] == ehash2[id]);
                assert(unhash[id] == ehash5[id]);
            } else {
                unhash[id] = ehash2[id] = ehash5[id] = 1;
                unhash.erase(id);
                ehash2.erase(id);
                ehash5.erase(id);
            }
        }
        if (loops % 100000 == 0) {
            printf("%d %d\r", loops, (int)ehash2.size());
#ifdef KEY_INT
            ehash2.shrink_to_fit();
            //ehash5.shrink_to_fit();

            uint64_t sum1 = 0, sum2 = 0, sum3 = 0;
            for (auto v : unhash) sum1 += v.first * v.second;
            for (auto v : ehash2) sum2 += v.first * v.second;
            for (auto v : ehash5) sum3 += v.first * v.second;
            assert(sum1 == sum2);
            assert(sum1 == sum3);
#endif
        }
    }

    printf("\n");
#endif
    return 0;
}

template<class hash_type>
void benOneHash(const std::string& hash_name, const std::vector<keyType>& oList)
{
    if (maps.find(hash_name) == maps.end())
        return;

    if (test_case == 0)
        printf("%s:size %zd\n", hash_name.data(), sizeof(hash_type));

    {
        hash_type hash;
        const uint32_t l1_size = (32 * 1024)   / (sizeof(keyType) + sizeof(valueType));
        const uint32_t l3_size = (8 * 1024 * 1024) / (sizeof(keyType) + sizeof(valueType));

        func_index  = 0;
        multi_small_bench <hash_type>(hash_name, oList);

        insert_erase      <hash_type>(hash_name, oList);
        insert_high_load  <hash_type>(hash_name, oList);

        insert_cache_size <hash_type>(hash_name, oList, "insert_l1_cache", l1_size / 2, 2 * l1_size + 1000);
        insert_cache_size <hash_type>(hash_name, oList, "insert_l2_cache", 2 * l1_size + 1000, l3_size / 4);
        insert_cache_size <hash_type>(hash_name, oList, "insert_l3_cache", l3_size / 2, l3_size * 2);

        insert_no_reserve <hash_type>(hash_name, oList);

        insert_reserve<hash_type>(hash,hash_name, oList);
        find_hit_all  <hash_type>(hash,hash_name, oList);
        find_miss_all <hash_type>(hash,hash_name);

        auto vList = oList;
        for (size_t v = 0; v < vList.size(); v += 2) {
#if KEY_INT
            vList[v] += v;
#elif KEY_CLA
            vList[v].lScore += v;
#elif TKey != 4
            vList[v][0] += v;
            //vList[v] += vList[v].size();
#else
            auto& next2 = vList[v + vList.size() / 2];
            vList[v] = next2.substr(0, next2.size() - 1);
#endif
        }

        find_hit_50<hash_type>(hash, hash_name, vList);
        erase_50<hash_type>(hash, hash_name, vList);
        erase_find_50<hash_type>(hash, hash_name, vList);
        insert_find_erase<hash_type>(hash, hash_name, vList);
        erase_reinsert<hash_type>(hash, hash_name, vList);
        hash_iter<hash_type>(hash, hash_name);

#ifndef UF
        copy_clear <hash_type>(hash, hash_name);
        //hash_clear<hash_type>(hash, hash_name);
#endif
    }
}

constexpr auto base1 = 300000000;
constexpr auto base2 =      20000;
void reset_top3(std::map<std::string, int64_t>& top3, const std::multimap <int64_t, std::string>& once_score_hash)
{
    auto it0 = once_score_hash.begin();
    auto it1 = *(it0++);
    auto it2 = *(it0++);
    auto it3 = *(it0++);

    //the top 3 func map
    if (it1.first == it3.first) {
        top3[it1.second] += base1 / 3;
        top3[it2.second] += base1 / 3;
        top3[it3.second] += base1 / 3;
    } else if (it1.first == it2.first) {
        top3[it1.second] += base1 / 2;
        top3[it2.second] += base1 / 2;
        top3[it3.second] += 1;
    } else {
        top3[it1.second] += base1;
        if (it2.first == it3.first) {
            top3[it2.second] += base2 / 2;
            top3[it3.second] += base2 / 2;
        } else {
            top3[it2.second] += base2;
            top3[it3.second] += 1;
        }
    }
}

static void printResult(int n)
{
    //total func hash time
    static std::map<std::string, std::map<std::string, int64_t>> func_hash_score;
//    static std::map<std::string, std::map<std::string, int64_t>> func_hash_score;
    static std::map<std::string, int64_t> top3;

    std::multimap<int64_t, std::string> once_score_hash;
    add_hash_func_time(func_hash_score, once_score_hash, n);
    if (once_score_hash.size() >= 3) {
        reset_top3(top3, once_score_hash);
    }

    constexpr int dis_input = 10;
    if (++test_case % dis_input != 0 && test_case % 7 != 0) {
        printf("=======================================================================\n\n");
        return ;
    }

    //print function rank
    std::multimap<int64_t, std::string> score_hash;
    printf("-------------------------------- function benchmark -----------------------------------------------\n");
    dump_all(func_hash_score, score_hash);

    //print top 3 rank
    if (top3.size() >= 3)
        puts("======== hash  top1   top2  top3 =======================");
    for (auto& v : top3)
        printf("%13s %4.1lf  %4.1lf %4d\n", v.first.c_str(), v.second / (double)(base1), (v.second / (base2 / 2) % 1000) / 2.0, (int)(v.second % (base2 / 2)));

    auto maxs = score_hash.rbegin()->first;
    //print hash rank
    puts("======== hash    score  weigh ==========================");
    for (auto& v : score_hash)
        printf("%13s  %4d     %3.1lf%%\n", v.second.c_str(), (int)(v.first / func_hash_score.size()), (v.first * 100.0 / maxs));

#if _WIN32
    Sleep(100*1);
#else
    usleep(1000*2000);
#endif
    printf("--------------------------------------------------------------------\n\n");
}

static int benchHashMap(int n)
{
    if (n < 10000)
        n = 123456;

    func_result.clear(); once_func_hash_time.clear();

    std::vector<keyType> vList;
    auto flag = buildTestData(n, vList);

#if ABSL_HASH && ABSL
    using ehash_func = absl::Hash<keyType>;
#elif WY_HASH && KEY_STR
    using ehash_func = WysHasher;
#elif WY_HASH && KEY_INT
    using ehash_func = WyIntHasher;
#elif MIX_HASH && KEY_INT
    using ehash_func = IntMixHasher;
#elif KEY_CLA
    using ehash_func = StuHasher;
#elif KEY_INT && BAD_HASH > 100
    using ehash_func = BadHasher<keyType>;
#elif PHMAP_HASH
    using ehash_func = phmap::Hash<keyType>;
#elif HOOD_HASH
    using ehash_func = robin_hood::hash<keyType>;
#else
    using ehash_func = std::hash<keyType>;
#endif

    {
        int64_t ts = getus(), sum = 0ul;
        for (auto& v : vList)
#if KEY_INT
            sum += v;
#elif KEY_CLA
        sum += v.lScore;
#else
        sum += v.size();
#endif
        loop_vector_time = getus() - ts;
        printf("n = %d, keyType = %s, valueType = %s(%zd), loop_sum = %d us:%d\n", n, sKeyType, sValueType, sizeof(valueType), (int)(loop_vector_time), (int)sum);
    }

    {
        func_print = func_print % func_index + 1;
#if ET > 2
        {  benOneHash<tsl::hopscotch_map   <keyType, valueType, ehash_func>>("hopsco", vList); }
#if __x86_64__ || _M_X64
        {  benOneHash<ska::bytell_hash_map <keyType, valueType, ehash_func>>("byte", vList); }
#endif
#endif

        {  benOneHash<std::unordered_map<keyType, valueType, ehash_func>>   ("stl_hash", vList); }
#if ET > 1
        {  benOneHash<emlru_time::lru_cache<keyType, valueType, ehash_func>>("lru_time", vList); }
        {  benOneHash<emlru_size::lru_cache<keyType, valueType, ehash_func>>("lru_size", vList); }

        {  benOneHash<emilib2::HashMap      <keyType, valueType, ehash_func>>("emilib2", vList); }
        {  benOneHash<emilib4::HashMap      <keyType, valueType, ehash_func>>("emilib4", vList); }
        {  benOneHash<tsl::robin_map        <keyType, valueType, ehash_func>>("robin", vList); }

#if __x86_64__ || _M_X64
        {  benOneHash<ska::flat_hash_map <keyType, valueType, ehash_func>>("flat", vList); }
        {  benOneHash<hrd7::hash_map     <keyType, valueType, ehash_func>>("hrdset", vList); }
#endif
#endif

#ifdef SMAP
        {  benOneHash<std::map<keyType, valueType>>         ("stl_map", vList); }

#if __GNUC__ && __linux__
        {  benOneHash<__gnu_pbds::gp_hash_table<keyType, valueType>>("gp_hash", vList) };
#endif

#if ET
        {  benOneHash<phmap::btree_map<keyType, valueType> >("btree", vList); }
#endif
#endif

#ifdef EM3
//        {  benOneHash<emilib1::HashMap <keyType, valueType, ehash_func>>("ktprime", vList); }
        {  benOneHash<emhash2::HashMap <keyType, valueType, ehash_func>>("emhash2", vList); }
        {  benOneHash<emhash4::HashMap <keyType, valueType, ehash_func>>("emhash4", vList); }
        {  benOneHash<emhash3::HashMap <keyType, valueType, ehash_func>>("emhash3", vList); }
#endif
        {  benOneHash<emhash7::HashMap <keyType, valueType, ehash_func>>("emhash7", vList); }
#if ABSL
        {  benOneHash<absl::flat_hash_map <keyType, valueType, ehash_func>>("absl", vList); }
#endif

#if FOLLY
        {  benOneHash<folly::F14ValueMap<keyType, valueType, ehash_func>>("f14_value", vList); }
        {  benOneHash<folly::F14VectorMap<keyType, valueType, ehash_func>>("f14_vector", vList); }
#endif

#if CUCKOO_HASHMAP
        {  benOneHash<libcuckoo::cuckoohash_map <keyType, valueType, ehash_func>>("cuckoo", vList); }
#endif
        {  benOneHash<emhash6::HashMap <keyType, valueType, ehash_func>>("emhash6", vList); }
        {  benOneHash<emhash5::HashMap <keyType, valueType, ehash_func>>("emhash5", vList); }
        {  benOneHash<emhash8::HashMap <keyType, valueType, ehash_func>>("emhash8", vList); }
#if ET
        {  benOneHash<phmap::flat_hash_map <keyType, valueType, ehash_func>>("phmap", vList); }
        {  benOneHash<robin_hood::unordered_flat_map <keyType, valueType, ehash_func>>("martin", vList); }
        {  benOneHash<emilib3::HashMap <keyType, valueType, ehash_func>>("emilib3", vList); }
        //{  benOneHash<zedland::hashmap <keyType, valueType, ehash_func>>("zhashmap", vList); }

#if FHT_HMAP
        {  benOneHash<fht_table <keyType, valueType, ehash_func>>("fht", vList); }
#endif
#endif


    }

    auto pow2 = 1 << ilog(vList.size(), 2);
    auto iload = 50 * vList.size() / pow2;
    printf("\n %d ======== n = %d, load_factor = %.3lf, data_type = %d ========\n", test_case + 1, n, iload / 100.0, flag);
    printResult(n);
    return test_case;
}

static void cpuidInfo(int regs[4], int id, int ext)
{
#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
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
        "------------------------------------------------------------------------------------------------------------";

    puts(sepator);
    //    puts("Copyright (C) by 2019-2021 Huang Yuanbing bailuzhou at 163.com\n");

    char cbuff[512] = {0};
    char* info = cbuff;
#ifdef __clang__
    info += sprintf(info, "clang %s", __clang_version__); //vc/gcc/llvm
#if __llvm__
    info += sprintf(info, " on llvm/");
#endif
#endif

#if _MSC_VER
    info += sprintf(info, "ms vc++  %d", _MSC_VER);
#elif __GNUC__
    info += sprintf(info, "gcc %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif __INTEL_COMPILER
    info += sprintf(info, "intel c++ %d", __INTEL_COMPILER);
#endif

#if __cplusplus > 199711L
    info += sprintf(info, " __cplusplus = %d", static_cast<int>(__cplusplus));
#endif

#if __x86_64__ || __amd64__ || _M_X64
    info += sprintf(info, " x86-64");
#elif __i386__ || _M_IX86
    info += sprintf(info, " x86");
#elif __arm64__ || __aarch64__
    info += sprintf(info, " arm64");
#elif __arm__
    info += sprintf(info, " arm");
#else
    info += sprintf(info, " unknow");
#endif

#if _WIN32
    info += sprintf(info, " OS = Win");
//    SetThreadAffinityMask(GetCurrentThread(), 0x1);
#elif __linux__
    info += sprintf(info, " OS = linux");
#elif __MAC__ || __APPLE__
    info += sprintf(info, " OS = MAC");
#elif __unix__
    info += sprintf(info, " OS = unix");
#else
    info += sprintf(info, " OS = unknow");
#endif

    info += sprintf(info, ", cpu = ");
    char vendor[0x40] = {0};
    int (*pTmp)[4] = (int(*)[4])vendor;
    cpuidInfo(*pTmp ++, 0x80000002, 0);
    cpuidInfo(*pTmp ++, 0x80000003, 0);
    cpuidInfo(*pTmp ++, 0x80000004, 0);

    info += sprintf(info, "%s", vendor);

    puts(cbuff);
    if (out)
        strcpy(out, cbuff);
    puts(sepator);
}

#if WYHASH_LITTLE_ENDIAN && STR_VIEW
struct string_hash
{
    using is_transparent = void;
    std::size_t operator()(const char* key)             const { auto ksize = std::strlen(key); return wyhash(key, ksize, ksize); }
    std::size_t operator()(const std::string& key)      const { return wyhash(key.c_str(), key.size(), key.size()); }
    std::size_t operator()(const std::string_view& key) const { return wyhash(key.data(), key.size(), key.size()); }
};

struct string_equal
{
    using is_transparent = int;

#if 1
    bool operator()(const std::string_view& lhs, const std::string& rhs) const {
        return lhs.size() == rhs.size() && lhs == rhs;
    }
#endif

    bool operator()(const char* lhs, const std::string& rhs) const {
        return std::strcmp(lhs, rhs.c_str()) == 0;
    }

    bool operator()(const char* lhs, const std::string_view& rhs) const {
        return std::strcmp(lhs, rhs.data()) == 0;
    }

    bool operator()(const std::string_view& rhs, const char* lhs) const {
        return std::strcmp(lhs, rhs.data()) == 0;
    }
};

static int find_strview_test()
{
    emhash6::HashMap<const std::string, char, string_hash, string_equal> map;
    std::string_view key = "key";
    std::string     skey = "key";
    map.emplace(key, 0);
    const auto it = map.find(key); // fail
    assert(it == map.find("key"));
    assert(it == map.find(skey));

    assert(key == "key");
    assert(key == skey);
    return 0;
}
#endif

static inline uint64_t hash64(uint64_t key)
{
#if __SIZEOF_INT128__
    __uint128_t r =  (__int128)key * UINT64_C(11400714819323198485);
    return (uint64_t)(r >> 64) + (uint64_t)r;
#elif _WIN64
    uint64_t high;
    return _umul128(key, UINT64_C(11400714819323198485), &high) + high;
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
    return mix;// (mix >> 32) | (mix << 32);
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

static inline uint64_t hash32(uint64_t key)
{
#if 1
    uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
    return (r >> 32) + r;
#elif 1
    //MurmurHash3Mixer
    uint64_t h = key;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
#elif 1
    uint64_t x = key;
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
#endif
}

static void testHashRand(int loops = 100000009)
{
    printf("%s loops = %d\n",__FUNCTION__, loops);
    long sum = 0;
    auto ts = getus();

    {
        ts = getus();
        Sfc4 srng(randomseed());
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("Sfc4       = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }

    {
        ts = getus();
        RomuDuoJr srng(randomseed());
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("RomuDuoJr  = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }
    {
        ts = getus();
        Orbit srng(randomseed());
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("Orbit      = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }

#if __SIZEOF_INT128__
    {
        ts = getus();
        Lehmer64 srng(randomseed());
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("Lehmer64    = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }
#endif

    {
        ts = getus();
        std::mt19937_64 srng; srng.seed(randomseed());
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("mt19937_64 = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }

#if WYHASH_LITTLE_ENDIAN
    {
        ts = getus();
        WyRand srng;
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("wyrand     = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }
#endif
}

static void testHashInt(int loops = 100000009)
{
    printf("%s loops = %d\n", __FUNCTION__, loops);
    auto ts = getus();
    long sum = ts;
    auto r  = ts * ts;

#ifdef PHMAP_VERSION_MAJOR
    ts = getus(); sum = 0;
    for (int i = 0; i < loops; i++)
        sum += phmap::phmap_mix<8>()(i + r);
    printf("phmap hash = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
#endif

#if ABSL_HASH && ABSL
    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += absl::Hash<uint64_t>()(i + r);
    printf("absl hash = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
#endif

#ifdef WYHASH_LITTLE_ENDIAN
    ts = getus(); sum = r;
    auto seed = randomseed();
    for (int i = 0; i < loops; i++)
        sum += wyhash64(i + r, seed);
    printf("wyhash64   = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
#endif

    ts = getus(); sum = r;
    for (int i = 1; i < loops; i++)
        sum += sum + i;
    printf("sum  add   = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);

#ifdef ROBIN_HOOD_H_INCLUDED
    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += robin_hood::hash_int(i + r);
    printf("martin hash= %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
#endif

    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += std::hash<uint64_t>()(i + r);
    printf("std hash   = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);

    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += hash64(i + r);
    printf("hash64     = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);

    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += hash32(i + r);
    printf("hash32     = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);

    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += hashmix(i + r);
    printf("hashmix   = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);

    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += rrxmrrxmsx_0(i + r);
    printf("rrxmrrxmsx_0 = %4d ms [%ld]\n\n", (int)(getus() - ts) / 1000, sum);

#if 0
    const int buff_size = 1024*1024 * 16;
    const int pack_size = 64;
    auto buffer = new char[buff_size * pack_size];
    memset(buffer, 0, buff_size * pack_size);

    for (int i = 0; i < 4; i++) {
        ts = getus();
        memset(buffer, 0, buff_size * pack_size);
        printf("memset   = %4d ms\n", (int)(getus() - ts) / 1000);

        ts = getus();
        for (uint32_t bi = 0; bi < buff_size; bi++)
           *(int*)(buffer + (bi * pack_size)) = 0;
        printf("loops   = %4d ms\n\n", (int)(getus() - ts) / 1000);
    }
    delete [] buffer;
#endif
}

static void buildRandString(int size, std::vector<std::string>& rndstring, int str_min, int str_max)
{
    std::mt19937_64 srng; srng.seed(randomseed());
    for (int i = 0; i < size; i++)
        rndstring.emplace_back(get_random_alphanum_string(srng() % (str_max - str_min + 1) + str_min));
}

static void testHashString(int size, int str_min, int str_max)
{
    printf("\n%s loops = %d\n", __FUNCTION__, size);
    std::vector<std::string> rndstring;
    rndstring.reserve(size * 4);

    long sum = 0;
    for (int i = 1; i <= 4; i++)
    {
        rndstring.clear();
        buildRandString(size * i, rndstring, str_min * i, str_max * i);
        int64_t start = 0;
        int t_find = 0;

        start = getus();
        for (auto& v : rndstring)
            sum += std::hash<std::string>()(v);
        t_find = (getus() - start) / 1000;
        printf("std hash = %4d ms\n", t_find);

#ifdef WYHASH_LITTLE_ENDIAN
        start = getus();
        for (auto& v : rndstring)
            sum += wyhash(v.data(), v.size(), 1);
        t_find = (getus() - start) / 1000;
        printf("wyhash   = %4d ms\n", t_find);

#endif

#ifdef AHASH_AHASH_H
        start = getus();
        for (auto& v : rndstring)
            sum += ahash64(v.data(), v.size(), 1);
        t_find = (getus() - start) / 1000;
        printf("ahash   = %4d ms\n", t_find);
#endif

#if ABSL_HASH
        constexpr uint64_t kHashSalt[5] = {
            uint64_t{0x243F6A8885A308D3}, uint64_t{0x13198A2E03707344},
            uint64_t{0xA4093822299F31D0}, uint64_t{0x082EFA98EC4E6C89},
            uint64_t{0x452821E638D01377},
        };

        start = getus();
        for (auto& v : rndstring)
            sum += absl::hash_internal::LowLevelHash(v.data(), v.size(), 1, kHashSalt);
        t_find = (getus() - start) / 1000;
        printf("absl low = %4d ms\n", t_find);
#endif

#ifdef ROBIN_HOOD_H_INCLUDED
        start = getus();
        for (auto& v : rndstring)
            sum += robin_hood::hash_bytes(v.data(), v.size());
        t_find = (getus() - start) / 1000;
        printf("martin hash = %4d ms\n", t_find);
#endif

#if ABSL_HASH && ABSL
        start = getus();
        for (auto& v : rndstring)
            sum += absl::Hash<std::string>()(v);
        t_find = (getus() - start) / 1000;
        printf("absl hash = %4d ms\n", t_find);
#endif

#ifdef PHMAP_VERSION_MAJOR
        start = getus();
        for (auto& v : rndstring)
            sum += phmap::Hash<std::string>()(v);
        t_find = (getus() - start) / 1000;
        printf("phmap hash  = %4d ms\n", t_find);
#endif
        putchar('\n');
    }
    printf("sum = %ld\n", sum);
}

int main(int argc, char* argv[])
{
#if WYHASH_LITTLE_ENDIAN && STR_VIEW
    //find_test();
#endif
    auto start = getus();

#ifdef AHASH_AHASH_H
    printf("ahash_version = %s", ahash_version());
#endif

    srand((unsigned)time(0));
    printInfo(nullptr);

    testHashInt(1e8+8);
    testHashString(1e6+6, 2, 32);
    int run_type = 0;
    int tn = 0, rnd = randomseed();
    auto maxc = 500;
    auto maxn = (1024 * 1024 * 128) / (sizeof(keyType) + sizeof(valueType) + 4) + 100000;
    auto minn = (1024 * 1024 * 2) / (sizeof(keyType) + sizeof(valueType) + 4) + 10000;

    float load_factor = 0.0945f;
    printf("./ebench maxn = %d c(0-1000) f(0-100) d[2-9 mpatseblku] a(0-3) b t(n)\n", (int)maxn);

    for (int i = 1; i < argc; i++) {
        const auto cmd = argv[i][0];
        const auto value = atoi(&argv[i][0] + 1);

        if (isdigit(cmd))
            maxn = atoll(argv[i]) + 1000;
        else if (cmd == 'f' && value > 0)
            load_factor = atof(&argv[i][0] + 1) / 100.0f;
        else if (cmd == 't' && value > 0)
            tn = value;
        else if (cmd == 'c' && value > 0)
            maxc = value;
        else if (cmd == 'a')
            run_type = value;
        else if (cmd == 'r' && value > 0)
            rnd = value;
        else if (cmd == 'b') {
            testHashRand(1e8+8);
        }
        else if (cmd == 'd') {
            for (char c = argv[i][1], j = 1; c != '\0'; c = argv[i][++j]) {
                if (c >= '2' && c <= '9') {
                    std::string hash_name("emhash");
                    hash_name += c;
                    if (maps.find(hash_name) != maps.end())
                        maps.erase(hash_name);
                    else
                        maps[hash_name] = hash_name;
                }
                else if (c == 'm')
                    maps.erase("martin");
                else if (c == 'p')
                    maps.erase("phmap");
                else if (c == 't')
                    maps.erase("robin");
                else if (c == 's')
                    maps.erase("flat");
                else if (c == 'a')
                    maps.erase("absl");
                else if (c == 'f')
                    maps.erase("f14_vector");
                else if (c == 'h')
                    maps.erase("hrdset");
                else if (c == 'e') {
                    maps.emplace("emilib2", "emilib2");
                    maps.emplace("emilib3", "emilib3");
                    maps.emplace("emilib4", "emilib4");
                }
                else if (c == 'l') {
                    maps.emplace("lru_size", "lru_size");
                    maps.emplace("lru_time", "lru_time");
                }
                else if (c == 'k')
                    maps.emplace("ktprime", "ktprime");
                else if (c == 'b') {
                    maps.emplace("btree", "btree_map");
                    maps.emplace("smap", "stl_map");
                } else if (c == 'u')
                    maps.emplace("stl_hash", "unordered_map");
            }
        }
    }

#ifndef KEY_CLA
    if (tn > 100000)
        TestHashMap(tn);
#endif

#if WYHASH_LITTLE_ENDIAN
    WyRand srng(rnd);
#else
    Sfc4 srng(rnd);
#endif

    for (auto& m : maps)
        printf("  %s\n", m.second.data());
    putchar('\n');

    int n = (srng() % 2*minn) + minn;
    while (true) {
        if (run_type == 2) {
            printf(">>");
            if (scanf("%u", &n) == 1 && n <= 0)
                run_type = 0;
        } else if (run_type == 1) {
            n = (srng() % maxn) + minn;
        } else {
            n = n * 10 / 9;
            if (n > maxn)
                n = (srng() % maxn) + minn;
        }

        if (load_factor > 0.2 && load_factor < 1) {
            auto pow2 = 1 << ilog(n, 2);
            n = int(pow2 * load_factor) - (1 << 10) + (srng()) % (1 << 8);
        }
        if (n < 1000 || n > 1234567890)
            n = 1234567 + rand() % 1234567;

        int tc = benchHashMap(n);
        if (tc >= maxc)
            break;
    }

    printf("total time = %.3lf s", (getus() - start) / 1000000.0);
    return 0;
}

