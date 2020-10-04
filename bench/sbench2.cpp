#include <random>
#include <map>
#include <ctime>
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_set>


#if __cplusplus > 201402L || _MSC_VER >= 1600
   // #define STR_VIEW  1
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

#if __GNUC__
#include <ext/pb_ds/assoc_container.hpp>
#endif

static void printInfo(char* out);
std::map<std::string, std::string> hash_tables =
{
    {"stl_hset", "unordered_set"},
    {"stl_set", "stl_set"},
    {"btree", "btree_set"},

    {"emhash7", "emhash7"},
    {"emhash2", "emhash2"},
    {"emhash9", "emhash9"},

    {"gp_hash", "gp_hash"},

    {"emiset", "emiset"},

#if ET
    {"martin", "martin_flat"},
    {"phmap", "phmap_flat"},
    {"hrdset",   "hrdset"},

    {"robin", "tsl_robin"},
    {"flat", "ska_flat"},

    {"hopsco", "tsl_hopsco"},
    {"byte", "ska_byte"},
#endif
};

#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
#define PHMAP_HAVE_SSSE3      1
#endif

//#define HOOD_HASH           1
//#define PHMAP_HASH          1
//#define EMH_WY_HASH         1
//#define FL1                 1
//#define EMH_FIBONACCI_HASH  1
//#define EMH_REHASH_LOG      1234567

//#define EMH_STATIS          1
//#define EMH_SAFE_ITER       1
//#define EMH_SAFE_HASH       1
//#define EMH_IDENTITY_HASH   1

//#define EMH_LRU_SET         1
//#define EMH_STD_STRING      1
//#define EMH_ERASE_SMALL     1
//#define EMH_BDKR_HASH       1
//#define EMH_HIGH_LOAD       201000

//
#include "hash_set2.hpp"
#include "hash_set3.hpp"
#include "hash_set4.hpp"


//https://www.zhihu.com/question/46156495
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

#if HOOD_HASH
    #include "martin/robin_hood.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#endif
#if PHMAP_HASH
    #include "phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap
#endif

#include "emilib/emiset.hpp"

#if ET
    #define  _CPP11_HASH    1

#if __x86_64__ || _WIN64
    #include "hrd/hash_set7.h"        //https://github.com/hordi/hash/blob/master/include/hash_set7.h
    #include "ska/flat_hash_map.hpp"  //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
    #include "tsl/robin_set.h"        //https://github.com/tessil/robin-map
#endif

#if ET > 1
    #include "tsl/hopscotch_set.h"    //https://github.com/tessil/hopscotch-map
    #include "ska/bytell_hash_map.hpp"//https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#endif
    #include "phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap
    #include "martin/robin_hood.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#endif

#include <unordered_map>

#ifdef _WIN32
    # define CONSOLE "CON"
    # include <windows.h>
#else
    # define CONSOLE "/dev/tty"
    # include <unistd.h>
    # include <sys/resource.h>
    # include <sys/time.h>
#endif

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

    int64_t lUid;
    int64_t lScore;
    int  iUpdateTime;
    int  iRank;

#if PACK >= 24
    char data[(PACK - 24) / 8 * 8];
#endif

#if COMP
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

struct WysHasher
{
    std::size_t operator()(const std::string& str) const
    {
#ifdef WYHASH_LITTLE_ENDIAN
        return wyhash(str.c_str(), str.size(), 0x123456789 + str.size());
#else
        size_t hash = 0;
        for (const auto c : str)
            hash = c + hash * 131;
        return hash;
#endif
    }
};

#if TKey == 0
    typedef unsigned int keyType;
    #define TO_KEY(i)   (keyType)i
    #define sKeyType    "int"
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
    #define KEY_SUC    1
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

static int64_t getTime()
{
#if WIN32_RSU
    FILETIME ptime[4] = {0, 0, 0, 0, 0, 0, 0, 0};
    GetThreadTimes(GetCurrentThread(), NULL, NULL, &ptime[2], &ptime[3]);
    return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10;
#elif WIN32_TICK
    return GetTickCount() * 1000;
#elif _WIN32
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }

    LARGE_INTEGER nowus;
    QueryPerformanceCounter(&nowus);
    return (nowus.QuadPart * 1000000) / (freq.QuadPart);
#elif LINUX_RUS
    struct rusage rup;
    getrusage(RUSAGE_THREAD, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000000 + usec;
#elif LINUX_TICK || __APPLE__
    return clock();
#elif __linux__ || __unix__
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec * 1000000l + start.tv_usec;
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

uint64_t randomseed() {
    std::random_device rd;
    return std::uniform_int_distribution<uint64_t>{}(rd);
}
// this is probably the fastest high quality 64bit random number generator that exists.
// Implements Small Fast Counting v4 RNG from PractRand.
class sfc64 {
public:
    using result_type = uint64_t;

    sfc64()
        : sfc64(randomseed()) {}

    sfc64(uint64_t a, uint64_t b, uint64_t c, uint64_t counter)
        : m_a{ a }
        , m_b{ b }
        , m_c{ c }
        , m_counter{ counter } {}

    sfc64(std::array<uint64_t, 4> const& state)
        : m_a{ state[0] }
        , m_b{ state[1] }
        , m_c{ state[2] }
        , m_counter{ state[3] } {}

    explicit sfc64(uint64_t seed)
        : m_a(seed)
        , m_b(seed)
        , m_c(seed)
        , m_counter(1) {
        for (int i = 0; i < 12; ++i) {
            operator()();
        }
    }

    // no copy ctors so we don't accidentally get the same random again
    sfc64(sfc64 const&) = delete;
    sfc64& operator=(sfc64 const&) = delete;

    sfc64(sfc64&&) = default;
    sfc64& operator=(sfc64&&) = default;

    ~sfc64() = default;

    static constexpr uint64_t(min)() {
        return (std::numeric_limits<uint64_t>::min)();
    }
    static constexpr uint64_t(max)() {
        return (std::numeric_limits<uint64_t>::max)();
    }

    void seed() {
        seed(randomseed());
    }

    void seed(uint64_t seed) {
        state(sfc64{ seed }.state());
    }

    uint64_t operator()() noexcept {
        auto const tmp = m_a + m_b + m_counter++;
        m_a = m_b ^ (m_b >> right_shift);
        m_b = m_c + (m_c << left_shift);
        m_c = rotl(m_c, rotation) + tmp;
        return tmp;
    }

    template <typename T>
    T uniform(T input) {
        return static_cast<T>(operator()(static_cast<uint64_t>(input)));
    }

    template <typename T>
    T uniform() {
        return static_cast<T>(operator()());
    }

    // Uses the java method. A bit slower than 128bit magic from
    // https://arxiv.org/pdf/1805.10941.pdf, but produces the exact same results in both 32bit and
    // 64 bit.
    uint64_t operator()(uint64_t boundExcluded) noexcept {
        uint64_t x, r;
        do {
            x = operator()();
            r = x % boundExcluded;
        } while (x - r > (UINT64_C(0) - boundExcluded));
        return r;
    }

    std::array<uint64_t, 4> state() const {
        return { {m_a, m_b, m_c, m_counter} };
    }

    void state(std::array<uint64_t, 4> const& s) {
        m_a = s[0];
        m_b = s[1];
        m_c = s[2];
        m_counter = s[3];
    }

private:
    template <typename T>
    T rotl(T const x, size_t k) {
        return (x << k) | (x >> (8 * sizeof(T) - k));
    }

    static constexpr size_t rotation = 24;
    static constexpr size_t right_shift = 11;
    static constexpr size_t left_shift = 3;
    uint64_t m_a;
    uint64_t m_b;
    uint64_t m_c;
    uint64_t m_counter;
};

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

static void check_func_result(const std::string& hash_name, const std::string& func, int64_t sum, int64_t ts1, int weigh = 1)
{
    if (func_result.find(func) == func_result.end()) {
        func_result[func] = sum;
    } else if (sum != func_result[func]) {
        printf("%s %s %zd != %zd (o)\n", hash_name.c_str(), func.c_str(), sum, func_result[func]);
    }

    auto& showname = hash_tables[hash_name];
    once_func_hash_time[func][showname] += (getTime() - ts1 - loop_vector_time / 2) / weigh;
    func_index ++;

    long ts = (getTime() - ts1) / 1000;
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
        printf("%5d   %13s   (%4.2lf %6.1lf%%)\n", int(v.first / func_index), v.second.c_str(), last * 1.0 / v.first, first * 100.0 / v.first);
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
    auto file = std::string(sKeyType);
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
void hash_iter(const hash_type& ahash, const std::string& hash_name)
{
    auto ts1 = getTime(); size_t sum = 0;
    for (auto& v : ahash)
        sum += 1;

    for (auto it = ahash.cbegin(); it != ahash.cend(); ++it)
#if KEY_INT
        sum += *it;
#elif KEY_SUC
    sum += 1;
#else
    sum += it->size();
#endif
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_reinsert(hash_type& ahash, const std::string& hash_name, std::vector<keyType>& vList)
{
    auto ts1 = getTime(); size_t sum = 0;
    for (const auto& v : vList) {
        ahash.emplace(v);
        sum += ahash.count(v);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void insert_erase(const std::string& hash_name, const std::vector<keyType>& vList)
{
#if KEY_INT
    const int bucket = 1 << 14;
    hash_type ahash; //(bucket / 2);
    auto ts1 = getTime(); size_t sum = 0;
    for (const auto& v : vList) {
        auto v2 = v % (bucket);
        auto r = ahash.emplace(v2);
        if (!r.second) {
            ahash.erase(r.first);
            sum ++;
        }
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
#endif
}

template<class hash_type>
void insert_no_reserve( const std::string& hash_name, const std::vector<keyType>& vList)
{
    hash_type ahash;
    auto ts1 = getTime(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ahash.emplace(v).second;
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void insert_reserve(hash_type& ahash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getTime(); size_t sum = 0;
#ifndef SMAP
    ahash.reserve(vList.size());
    ahash.max_load_factor(0.99f);
#endif

    for (const auto& v : vList)
        sum += ahash.emplace(v).second;
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_insert_multi(const std::string& hash_name, const std::vector<keyType>& vList)
{
#if KEY_INT
    size_t sum = 0;
    constexpr auto hash_size = 65437;
    auto mh = new hash_type[hash_size];

    auto ts1 = getTime();
    for (const auto& v : vList) {
        auto hash_id = ((uint64_t)v) % hash_size;
        sum += mh[hash_id].emplace(v).second;
    }

    for (const auto& v : vList) {
        auto hash_id = ((uint64_t)v) % hash_size;
        sum += mh[hash_id].count(v + v % 2);
    }

    delete []mh;
    check_func_result(hash_name, __FUNCTION__, sum, ts1, 2);
#endif
}

template<class hash_type>
void insert_find_erase(const hash_type& ahash, const std::string& hash_name, std::vector<keyType>& vList)
{
    auto ts1 = getTime(); size_t sum = 0;
    hash_type tmp(ahash);

    for (auto & v : vList) {
#if KEY_INT
        auto v2 = v / 101 + v;
#elif KEY_SUC
        auto v2(v.lScore / 101 + v.lScore);
#elif TKey != 4
        v += char(128 + (int)v[0]);
        const auto &v2 = v;
#else
        const keyType v2(v.data(), v.size() - 1);
#endif
        sum += tmp.emplace(v2).second;
        sum += tmp.count(v2);
        sum += tmp.erase(v2);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1, 3);
}

template<class hash_type>
void insert_cache_size(const std::string& hash_name, const std::vector<keyType>& vList, const char* level, const uint32_t min_size, const uint32_t cache_size)
{
    auto ts1 = getTime(); size_t sum = 0;
    const auto smalls = min_size + vList.size() % cache_size;
    hash_type tmp, empty;
#ifndef SMAP
    empty.max_load_factor(0.875);
#endif
    tmp = empty;

    for (const auto& v : vList)
    {
        sum += tmp.emplace(v).second;
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

    const auto max_loadf = 0.999f;
#ifndef SMAP
    tmp.reserve(pow2 / 2);
    tmp.max_load_factor(max_loadf);
#endif
    int minn = (max_loadf - 0.2f) * pow2, maxn = max_loadf * pow2;
    int i = 0;

    for (; i < minn; i++) {
        if (i < (int)vList.size())
            tmp.emplace(vList[i]);
        else {
            auto& v = vList[i - vList.size()];
#if KEY_INT
            auto v2 = v + (v / 11) + i;
#elif KEY_SUC
            auto v2 = v.lScore + (v.lScore / 11) + i;
#elif TKey != 4
            auto v2 = v; v2[0] += '2';
#else
            const keyType v2(v.data(), v.size() - 1);
#endif
            tmp.emplace(v2);
        }
    }

    auto ts1 = getTime();
    for (; i  < maxn; i++) {
        auto& v = vList[i - minn];
#if KEY_INT
        auto v2 = (v / 7) + 4 * v;
#elif KEY_SUC
        auto v2 = (v.lScore / 7) + 4 * v.lScore;
#elif TKey != 4
        auto v2 = v; v2[0] += '1';
#else
        const keyType v2(v.data(), v.size() - 1);
#endif
//        tmp[v2];
        sum += tmp.count(v2);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

static uint8_t l1_cache[64 * 1024];
template<class hash_type>
void find_miss_all(hash_type& ahash, const std::string& hash_name)
{
    auto ts1 = getTime();
    auto n = ahash.size();
    size_t pow2 = 2u << ilog(n, 2), sum = 0;

#if KEY_STR
    std::string skey = get_random_alphanum_string(32);
#endif

    for (size_t v = 0; v < pow2; v++) {
#if FL1
        l1_cache[v % sizeof(l1_cache)] = 0;
#endif
#if KEY_STR
        skey[v % 32 + 1] ++;
        sum += ahash.count((const char*)skey.c_str());
#else
        sum += ahash.count(TO_KEY(v));
#endif
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_hit_half(hash_type& ahash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getTime(); size_t sum = 0;
    for (const auto& v : vList) {
#if FL1
        if (sum % (1024 * 256) == 0)
            memset(l1_cache, 0, sizeof(l1_cache));
#endif
        sum += ahash.count(v);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_hit_all(const hash_type& ahash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getTime(); size_t sum = 0;
    for (const auto& v : vList) {
#if KEY_INT
        sum += ahash.count(v) + v;
#elif KEY_SUC
        sum += ahash.count(v) + v.lScore;
#else
        sum += ahash.count(v) + v.size();
#endif
#if FL1
        if (sum % (1024 * 64) == 0)
            memset(l1_cache, 0, sizeof(l1_cache));
#endif

    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_find_half(const hash_type& ahash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getTime(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ahash.count(v);
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_half(hash_type& ahash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto tmp = ahash;
    auto ts1 = getTime(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ahash.erase(v);

    for (auto it = tmp.begin(); it != tmp.end(); ) {
        it = tmp.erase(it);
        sum += 1;
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void hash_clear(hash_type& ahash, const std::string& hash_name)
{
    auto ts1 = getTime();
    size_t sum = ahash.size();
    ahash.clear(); ahash.clear();
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void hash_copy(hash_type& ahash, const std::string& hash_name)
{
    size_t sum = 0;
    auto ts1 = getTime();
    hash_type thash = ahash;
    ahash = thash;
    ahash = std::move(thash);
    sum  = thash.size();
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

#if PACK >= 24
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

//https://en.wikipedia.org/wiki/Gamma_distribution#/media/File:Gamma_distribution_pdf.svg
//https://blog.csdn.net/luotuo44/article/details/33690179
static int buildTestData(int size, std::vector<keyType>& randdata)
{
    randdata.reserve(size);

#if 1
    sfc64 srng(size);
#else
    std::mt19937_64 srng; srng.seed(size);
#endif

#ifdef KEY_STR
    for (int i = 0; i < size; i++)
        randdata.emplace_back(get_random_alphanum_string(srng() % 64 + 4));
    return 0;
#else

#if AR > 0
    const auto iRation = AR;
#else
    const auto iRation = 0;
#endif

    auto flag = 0;
    if (srng() % 100 >= iRation)
    {
        for (int i = 0; ; i++) {
            auto key = TO_KEY(srng());
            randdata.emplace_back(key);
            if (randdata.size() >= size)
                break;
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

template<class hash_type>
void benOneHash(const std::string& hash_name, const std::vector<keyType>& oList)
{
    if (hash_tables.find(hash_name) == hash_tables.end())
        return;

    if (test_case == 0)
        printf("%s:size %zd\n", hash_name.data(), sizeof(hash_type));

    {
        hash_type hash;
        const uint32_t l1_size = (64 * 1024)   / (sizeof(keyType) + sizeof(int));
        const uint32_t l3_size = (8 * 1024 * 1024) / (sizeof(keyType) + sizeof(int));

        func_index  = 0;
#if 1
        insert_erase      <hash_type>(hash_name, oList);
        insert_high_load  <hash_type>(hash_name, oList);
        insert_cache_size <hash_type>(hash_name, oList, "insert_l1_cache", l1_size / 2, 2 * l1_size + 1000);
        insert_cache_size <hash_type>(hash_name, oList, "insert_l3_cache", l1_size * 4, l3_size * 4);
        insert_no_reserve <hash_type>(hash_name, oList);
        find_insert_multi <hash_type>(hash_name, oList);
#endif

        insert_reserve<hash_type>(hash, hash_name, oList);

        find_hit_all  <hash_type>(hash, hash_name,oList);
        find_miss_all <hash_type>(hash, hash_name);

        auto vList = oList;
        for (size_t v = 0; v < vList.size() / 2; v++) {
#if KEY_INT
            vList[v] += v * v + v;
#elif KEY_SUC
            vList[v].lScore += v * v;
#elif TKey != 4
            vList[v][0] += v;
            //vList[v] += vList[v].size();
#else
            auto& next2 = vList[v + vList.size() / 2];
            vList[v] = next2.substr(0, next2.size() - 1);
#endif
        }

        find_hit_half<hash_type>(hash, hash_name, vList);
        erase_half<hash_type>(hash, hash_name, vList);
        erase_find_half<hash_type>(hash, hash_name, vList);
        insert_find_erase<hash_type>(hash, hash_name, vList);
        erase_reinsert<hash_type>(hash, hash_name, vList);
        hash_iter<hash_type>(hash, hash_name);

#ifdef UF
        hash_copy <hash_type>(hash, hash_name);
        hash_clear<hash_type>(hash, hash_name);
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

static int benchHashSet(int n)
{
    if (n < 10000)
        n = 123456;

    func_result.clear(); once_func_hash_time.clear();

    std::vector<keyType> vList;
    auto flag = buildTestData(n, vList);

#if KEY_STR && EMH_WY_HASH
    using ehash_func = WysHasher;
#elif KEY_SUC
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
        int64_t ts = getTime(), sum = 0ul;
        for (auto& v : vList)
#if KEY_INT
            sum += v;
#elif KEY_SUC
        sum += v.lScore;
#else
        sum += v.size();
#endif
        loop_vector_time = getTime() - ts;
        printf("n = %d, keyType = %s, loop = %d ns:%d\n", n, sKeyType, (int)(loop_vector_time * 1000 / vList.size()), (int)sum);
    }

    {
        func_print = func_print % func_index + 1;
#if ET > 2
        {  benOneHash<tsl::hopscotch_set   <keyType,  ehash_func>>("hopsco", vList); }
#if __x86_64__
        {  benOneHash<ska::bytell_hash_set <keyType,  ehash_func>>("byte", vList); }
#endif
#endif

#if ET > 1
        {  benOneHash<std::unordered_set<keyType,  ehash_func>>("stl_hset", vList); }
        //{  benOneHash<emilib2::HashSet     <keyType,  ehash_func>>("emilib2", vList); }

        {  benOneHash<tsl::robin_set     <keyType,  ehash_func>>("robin", vList); }
#if __x86_64__
        {  benOneHash<ska::flat_hash_set <keyType,  ehash_func>>("flat", vList); }
        {  benOneHash<hrd7::hash_set     <keyType,  ehash_func>>("hrdset", vList); }
#endif
#endif

#ifdef SMAP
        {  benOneHash<std::map<keyType>>         ("stl_set", vList); }
#if __GNUC__
//        {  benOneHash<__gnu_pbds::gp_hash_table<keyType>>("gp_hash", vList) };
#endif

#if ET
        {  benOneHash<phmap::btree_set<keyType >("btree", vList); }
#endif
#endif

#if ET
        {  benOneHash<phmap::flat_hash_set <keyType,  ehash_func>>("phmap", vList); }
        {  benOneHash<robin_hood::unordered_flat_set <keyType,  ehash_func>>("martin", vList); }
#endif

        {  benOneHash<emilib::HashSet <keyType,  ehash_func>>("emiset", vList); }
        {  benOneHash<emhash7::HashSet <keyType,  ehash_func>>("emhash7", vList); }
        {  benOneHash<emhash2::HashSet <keyType,  ehash_func>>("emhash2", vList); }
        {  benOneHash<emhash9::HashSet <keyType,  ehash_func>>("emhash9", vList); }
    }

    auto pow2 = 1 << ilog(vList.size(), 2);
    auto iload = 50 * vList.size() / pow2;
    printf("\n %d ======== n = %d, load_factor = %.2lf, data_type = %d ========\n", test_case + 1, n, iload / 100.0, flag);
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
    //    puts("Copyright (C) by 2019-2020 Huang Yuanbing bailuzhou at 163.com\n");

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

    std::size_t operator()(const std::string& key)    const { return wyhash(key.c_str(), key.size(), 11400714819323198485ull); }
    std::size_t operator()(const std::string_view& key)    const { return wyhash(key.data(), key.size(), 11400714819323198485ull); }
    std::size_t operator()(const char* key)        const { return wyhash(key, std::strlen(key), 11400714819323198485ull); }
};

struct string_equal
{
    using is_transparent = int;

    bool operator()(const std::string_view& lhs, const std::string& rhs) const {
        //const std::string_view view = rhs;
        return lhs.size() == rhs.size() &&
            (lhs.data() == rhs.data() || std::strcmp(lhs.data(), rhs.data()) == 0);
    }

    bool operator()(const char* lhs, const std::string& rhs) const {
        return std::strcmp(lhs, rhs.c_str()) == 0;
    }
};

static int find_test()
{
    emhash6::HashSet<std::string, uint64_t, string_hash, string_equal> map;
    std::string_view key = "key";
    map.emplace(key, 100);
    const auto it = map.find(key); // fail
    assert(it == map.find("key"));
    assert(it == map.find(std::string("key")));

    assert(key == "key");
    assert(key == std::string("key"));
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

static void testHashInt(int loops = 100000009)
{
    long sum = 0;
    auto ts = getTime();

#ifdef PHMAP_VERSION_MAJOR
    for (int i = 0; i < loops; i++)
        sum += phmap::phmap_mix<8>()(i);
    printf("phmap hash = %4d ms [%ld]\n", (int)(getTime() - ts) / 1000, sum);
#endif

    ts = getTime();
    for (int i = 1; i < loops; i++)
        sum += i;
    printf("phmap mul = %4d ms [%ld]\n", (int)(getTime() - ts) / 1000, sum);

#ifdef ROBIN_HOOD_H_INCLUDED
    ts = getTime();
    for (int i = 0; i < loops; i++)
        sum += robin_hood::hash<uint64_t>()(i);
    printf("robin hash = %4d ms [%ld]\n", (int)(getTime() - ts) / 1000, sum);
#endif

    ts = getTime();
    for (int i = 0; i < loops; i++)
        sum += std::hash<uint64_t>()(i);
    printf("std hash = %4d ms [%ld]\n",  (int)(getTime() - ts) / 1000, sum);

    ts = getTime();
    for (int i = 0; i < loops; i++)
        sum += hash64(i);
    printf("hash64   = %4d ms [%ld]\n",  (int)(getTime() - ts) / 1000, sum);

    ts = getTime();
    for (int i = 0; i < loops; i++)
        sum += hash32(i);
    printf("hash32   = %4d ms [%ld]\n\n", (int)(getTime() - ts) / 1000, sum);
}

static void buildRandString(int size, std::vector<std::string>& rndstring, int str_min, int str_max)
{
    std::mt19937_64 srng; srng.seed(time(0));
    for (int i = 0; i < size; i++)
        rndstring.emplace_back(get_random_alphanum_string(srng() % (str_max - str_min + 1) + str_min));
}

static void testHashString(int size, int str_min, int str_max)
{
    std::vector<std::string> rndstring;
    rndstring.reserve(size * 4);

    char os_info[2048]; printInfo(os_info);
    long sum = 0;
    for (int i = 1; i <= 4; i++)
    {
        rndstring.clear();
        buildRandString(size * i, rndstring, str_min * i, str_max * i);
        int64_t start = 0;
        int t_find = 0;

        start = getTime();
        for (auto& v : rndstring)
            sum += std::hash<std::string>()(v);
        t_find = (getTime() - start) / 1000;
        printf("stdhash time use = %4d ms\n", t_find);

#ifdef WYHASH_LITTLE_ENDIAN
        start = getTime();
        for (auto& v : rndstring)
            sum += wyhash(v.data(), v.size(), 1);
        t_find = (getTime() - start) / 1000;
        printf("wyhash  time use = %4d ms\n", t_find);
#endif

#ifdef ROBIN_HOOD_H_INCLUDED
        start = getTime();
        for (auto& v : rndstring)
            sum += robin_hood::hash_bytes(v.data(), v.size());
        t_find = (getTime() - start) / 1000;
        printf("martin  time use = %4d ms\n", t_find);
#endif

#ifdef PHMAP_VERSION_MAJOR
        start = getTime();
        for (auto& v : rndstring)
            sum += phmap::Hash<std::string>()(v);
        t_find = (getTime() - start) / 1000;
        printf("phmap   time use = %4d ms\n", t_find);
#endif
        putchar('\n');
    }
    printf("sum = %ld\n", sum);

}

int main(int argc, char* argv[])
{
    //testHashInt();
    //testHashString(rand() % 1234567 + 1234567, 4, 64);

#if WYHASH_LITTLE_ENDIAN && STR_VIEW
    find_test();
#endif

    srand((unsigned)time(0));

    printInfo(nullptr);

    bool auto_set = false;
    int tn = 0, rnd = time(0) + rand() * rand();
    auto maxc = 500;
    auto maxn = (1024 * 1024 * 64) / (sizeof(keyType) + 8) + 100000;
    auto minn = (1024 * 1024 * 2) / (sizeof(keyType) + + 8) + 10000;

    float load_factor = 0.0945f;
    printf("./ebench maxn = %d i[0-1] c(0-1000) f(0-100) d[2-9 h m p s f u e] t(n)\n", (int)maxn);

    for (int i = 1; i < argc; i++) {
        const auto cmd = argv[i][0];
        if (isdigit(cmd))
            maxn = atoi(argv[i]) + 1000;
        else if (cmd == 'f' && isdigit(argv[i][1]))
            load_factor = atof(&argv[i][0] + 1) / 100.0f;
        else if (cmd == 't' && isdigit(argv[i][1]))
            tn = atoi(&argv[i][0] + 1);
        else if (cmd == 'c' && isdigit(argv[i][1]))
            maxc = atoi(&argv[i][0] + 1);
        else if (cmd == 'a')
            auto_set = true;
        else if (cmd == 'r' && isdigit(argv[i][1]))
            rnd = atoi(&argv[i][0] + 1);

        else if (cmd == 'd') {
            for (char c = argv[i][1], j = 1; c != '\0'; c = argv[i][++j]) {
                if (c >= '2' && c <= '9') {
                    std::string hash_name("emhash");
                    hash_name += c;
                    if (hash_tables.find(hash_name) != hash_tables.end())
                        hash_tables.erase(hash_name);
                    else
                        hash_tables[hash_name] = hash_name;
                }

                else if (c == 'h')
                    hash_tables.erase("hrdset");
                else if (c == 'm')
                    hash_tables.erase("martin");
                else if (c == 'p')
                    hash_tables.erase("phmap");
                else if (c == 't')
                    hash_tables.erase("robin");
                else if (c == 's')
                    hash_tables.erase("flat");
                else if (c == 'e') {
                    hash_tables.emplace("emiset", "emiset");
                }
                else if (c == 'b') {
                    hash_tables.emplace("btree", "btree_set");
                    hash_tables.emplace("stl_set", "stl_set");
                } else if (c == 'u')
                    hash_tables.emplace("stl_hset", "unordered_set");
            }
        }
    }

    sfc64 srng(rnd);
    for (auto& m : hash_tables)
        printf("  %s\n", m.second.data());
    putchar('\n');

    while (true) {
        int n = (srng() % maxn) + minn;
        if (auto_set) {
            printf(">> ");
            if (scanf("%u", &n) == 1 && n <= 0)
                auto_set = false;
        }
        if (load_factor > 0.2 && load_factor < 1) {
            auto pow2 = 1 << ilog(n, 2);
            n = int(pow2 * load_factor) - (1 << 10) + (srng()) % (1 << 8);
        }
        if (n < 1000 || n > 1234567890)
            n = 1234567 + rand() % 1234567;

        int tc = benchHashSet(n);
        if (tc >= maxc)
            break;
    }

    return 0;
}

