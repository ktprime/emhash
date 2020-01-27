#include <random>
#include <map>
#include <ctime>
#include <cassert>
#include <iostream>
#include <string>
#if __GNUC__ > 5 || _MSC_VER > 1600
    #include <string_view>
#endif
#include <algorithm>
#include <array>


//#define HOOD_HASH         1
#define ET         1
//#define EMHASH_LRU_SET    1
//#define EMHASH_IDENTITY_HASH 1
//#define EMHASH_REHASH_LOG   1
//#define EMHASH_SAFE_HASH      1
//#define EMHASH_STATIS    1
//#define EMHASH_HIGH_LOAD 1

#ifndef TKey
    #define TKey  1
#endif

#include "hash_set.hpp" //
#include "hash_set2.hpp" //
#include "hash_set3.hpp" //
#include "hash_set4.hpp" //
constexpr int max_loop = 1000000;

////some others
//https://github.com/ilyapopov/car-race
//https://hpjansson.org/blag/2018/07/24/a-hash-table-re-hash/
//https://attractivechaos.wordpress.com/2018/01/13/revisiting-hash-table-performance/
//https://www.reddit.com/r/cpp/comments/auwbmg/hashmap_benchmarks_what_should_i_add/
//https://www.youtube.com/watch?v=M2fKMP47slQ
//https://yq.aliyun.com/articles/563053

//https://attractivechaos.wordpress.com/2008/08/28/comparison-of-hash-table-libraries/
//https://en.wikipedia.org/wiki/Hash_table
//https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html
//https://probablydance.com/2017/02/26/i-wrote-the-fastest-hashtable/
//https://martinus.ankerl.com/2016/09/21/very-fast-hashmap-in-c-part-3/
//https://andre.arko.net/2017/08/24/robin-hood-hashing/
//http://www.ilikebigbits.com/2016_08_28_hash_table.html
//http://www.idryman.org/blog/2017/05/03/writing-a-damn-fast-hash-table-with-tiny-memory-footprints/

#if __cplusplus >= 201103L && HOOD_HASH
    #include "./martin/robin_hood.h"       //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#endif

#if ET
    #include "./hrd/hash_set7.h"
    #include "./phmap/phmap.h"
    #include "./tsl/robin_set.h"        //https://github.com/tessil/robin-map
    #include "./tsl/hopscotch_set.h"    //https://github.com/tessil/hopscotch-map
    #include "./martin/robin_hood.h"    //https://github.com/martinus/robin-hood-hashing/blob/master/src/include/robin_hood.h
    #include "./ska/flat_hash_map.hpp"  //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
    #include "./ska/bytell_hash_map.hpp"//https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#endif

#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
    # define CONSOLE "CON"
    # include <windows.h>
#else
    # define CONSOLE "/dev/tty"
    # include <unistd.h>
    # include <sys/resource.h>
#endif

struct RankItem;

#if TKey == 0
    typedef unsigned int         keyType;
    #define TO_KEY(i)   (keyType)i
    #define sKeyType    "int"
    #define KEY_INT
    #define TO_SUM(i)   i
#elif TKey == 1
    typedef int64_t     keyType;
    #define TO_KEY(i)   (keyType)i
    #define sKeyType    "int64"
    #define KEY_INT
    #define TO_SUM(i)   i
#elif TKey == 2
    typedef std::string keyType;
    #define TO_KEY(i)   std::to_string(i)
    #define sKeyType    "string"
    #define TO_SUM(i)   i.size()
#else
    typedef std::string_view keyType;
    #define TO_KEY(i)   std::to_string(i)
    #define sKeyType    "string_view"
    #define TO_SUM(i)   i.size()
#endif

static std::unordered_map<std::string, std::string> show_name = {
//    {"stl_hash", "unordered_map"},
//    {"emhash7", "emhash7"},
//    {"emhash6", "emhash6"},

    {"emhash8", "emhash8"},
    {"emhash9", "emhash9"},
    {"hrdhash", "hrd7 hash"},

#if ET > 0
    {"martin", "martin flat"},
    {"phmap", "phmap flat"},
#if ET > 1
    {"robin", "tessil robin"},
    {"flat", "skarupk flat"},
#endif
#if ET > 2
    {"hopsco", "tessil hopsco"},
    {"byte", "skarupk byte"},
#endif
#endif
};

static int64_t getTime()
{
#if _WIN32 && 0
    FILETIME ptime[2] = {0, 0, 0, 0};
    GetThreadTimes(GetCurrentThread(), NULL, NULL, &ptime[0], &ptime[1]);
    return (ptime[0].dwLowDateTime + ptime[1].dwLowDateTime) / 10;
#elif __linux__ || __unix__
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000000 + usec;
#elif _WIN32
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        SetThreadAffinityMask(GetCurrentThread(), 0x3);
    }

    LARGE_INTEGER nowus;
    QueryPerformanceCounter(&nowus);
    return (nowus.QuadPart * 1000000) / (freq.QuadPart);
#else
    return clock();
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

static std::map<std::string, int64_t>  check_result;
static std::multimap <int64_t, std::string> func_time;
static std::map<std::string, int64_t> map_time;
static std::map<std::string, std::map<std::string, int64_t>> func_map_time;

#define AVE_TIME(ts, n)             1000 * (getTime() - ts) / (n)

static void check_mapfunc_result(const std::string& map_name, const std::string& func, int64_t sum, int64_t ts1)
{
    if (check_result.find(func) == check_result.end()) {
        check_result[func] = sum;
    }
    else if (sum != check_result[func]) {
        printf("%s %s %ld != %ld\n", map_name.c_str(), func.c_str(), sum, check_result[func]);
    }

    auto& showname = show_name[map_name];
    auto timeuse = (getTime() - ts1);

    func_time.insert(std::pair<int64_t, std::string>(timeuse / 1000, showname));
    map_time[showname] += timeuse;
    func_map_time[func][showname] += timeuse;
}

static void set_func_time(std::map<std::string, std::map<std::string, int64_t>>& func_rank_time)
{
    for (auto& v : func_map_time) {
        for (auto& f : v.second) {
            func_rank_time[v.first][f.first] += f.second;
        }
    }
    func_map_time.clear();
}

static void dump_func(const std::string& func, const std::map<std::string, int64_t >& map_rtime)
{
    std::multimap <int64_t, std::string> functime;
    for (const auto& v : map_rtime)
        functime.insert(std::pair<int64_t, std::string>(v.second, v.first));

    puts(func.c_str());
    auto min = functime.begin()->first + 1;
    for (auto& v : functime)
        printf("   %-8ld     %-21s   %02d\n", v.first / 10000, v.second.c_str(), (int)((min * 100) / (v.first + 1)));
    putchar('\n');
}

static void dump_all(std::map<std::string, std::map<std::string, int64_t>>& func_rtime)
{
    for (const auto& v : func_rtime)
        dump_func(v.first, v.second);
}

template<class MAP>
void hash_iter(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        for (auto it = amap.begin(); it != amap.end(); ++it)
#ifdef KEY_INT
            sum += *it;
#else
            sum += 1;
#endif

        for (const auto& v : amap)
            sum += TO_SUM(v);

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void hash_reinsert(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.insert(v).second;

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
//        printf("    %12s    reinsert  %5ld ns, factor = %.2f\n", map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class MAP>
void insert_reserve(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
#if 0
        amap.insert(vList.begin(), vList.end());
        sum = amap.size();
#else
        for (const auto v : vList)
            sum += amap.emplace(v).second;
#endif

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void insert_noreserve(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        size_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.emplace(v).second;

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
//      printf("    %82s  %s  %5zd ns, factor = %.2f\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class MAP>
void hash_emplace(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.emplace(v).second;

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
//      printf("    %12s    %s  %5ld ns, factor = %.2f\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class hash_type>
void insert_small_size(const std::string& hash_name, const std::vector<keyType>& vList)
{
    if (show_name.count(hash_name) != 0)
    {
        size_t sum = 0;
        const auto smalls = 100 + vList.size() % (256 * 1024);
        hash_type tmp, empty;

        auto ts1 = getTime();
        for (const auto& v : vList)
        {
            sum += tmp.emplace(v).second;
            sum += tmp.count(v);
            if (tmp.size() > smalls) {
                if (smalls % 2 == 0)
                    tmp.clear();
                else
                    tmp = empty;
                tmp.max_load_factor(0.8);
            }
        }
        check_mapfunc_result(hash_name, __FUNCTION__, sum, ts1);
        printf("             %62s    %s  %5lu ns, factor = %.2f\n", __FUNCTION__, hash_name.c_str(), AVE_TIME(ts1, vList.size()), tmp.load_factor());
    }
}

template<class hash_type>
void insert_high_load(const std::string& hash_name, const std::vector<keyType>& vList)
{
    if (show_name.count(hash_name) != 0)
    {
        size_t sum = 0;
        size_t pow2 = 2 << ilog(vList.size(), 2);
        hash_type tmp;
        auto max_loadf = 0.990;
        tmp.max_load_factor(max_loadf);
        tmp.reserve(pow2 / 2);
        int minn = (max_loadf - 0.2) * pow2, maxn = max_loadf * pow2;
        int i = 0;

        for (; i < minn; i++) {
            if (i < vList.size())
                tmp.emplace(vList[i]);
            else {
                auto v = vList[i - vList.size()];
#ifdef KEY_INT
                auto v2 = v + (v / 11) + i;
#else
                auto v2 = v; v2[0] += '2';
#endif
                tmp.emplace(v2);
            }
        }

        auto ts1 = getTime();
        for (; i  < maxn; i++) {
            auto v = vList[i - minn];
#ifdef KEY_INT
            auto v2 = (v / 7) + 7 * v;
#else
            auto v2 = v; v2[0] += '1';
#endif
            tmp.insert(v2);
            sum += tmp.count(v2);
        }

        check_mapfunc_result(hash_name, __FUNCTION__, sum, ts1);
        printf("             %122s    %s  %5ld ns, factor = %.2f\n", __FUNCTION__, hash_name.c_str(), AVE_TIME(ts1, maxn - minn), tmp.load_factor());
    }
}

template<class MAP>
void find_miss(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto n = vList.size();
        auto ts1 = getTime();
        for (size_t v = 1; v < 2*n; v++)
            sum += amap.count(TO_KEY(v));

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
        printf("    %12s    %s %5ld ns, factor = %.2f\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class MAP>
void find_half(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        auto n = vList.size();
        size_t pow2 = 2 << ilog(n, 2);
        auto ts1 = getTime(); size_t sum = 0;
        for (size_t v = 1; v < vList.size(); v += 2) {
            sum += amap.count(TO_KEY(v));
#ifdef KEY_INT
            sum += amap.count(v + pow2);
#endif
        }
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class hash_type>
void insert_find_erase(hash_type& ahash, const std::string& hash_name, std::vector<keyType>& vList)
{
    if (show_name.find(hash_name) != show_name.end())
    {
        size_t sum = 0;
        hash_type tmp(ahash);
        auto ts1 = getTime();
        for (const auto v : vList) {
#ifdef KEY_INT
            auto v2 = v + 1;
#else
            auto v2 = v + "1";
#endif
            sum += tmp.emplace(v2).second;
            sum += tmp.count(v2);
            sum += tmp.erase(v2);
        }
        check_mapfunc_result(hash_name, __FUNCTION__, sum, ts1);
        //printf("             %82s    %s  %5ld ns, factor = %.2f\n", __FUNCTION__, hash_name.c_str(), AVE_TIME(ts1, vList.size()), tmp.load_factor());
    }
}

template<class MAP>
void hash_erase(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.erase(v);
//        for (auto it = amap.begin(); it != amap.end(); ) {
//            it = amap.erase(it);
//            sum++;
//        }

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
        //printf("    %12s    %s  %5ld ns, size = %zd\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), sum);
    }
}

template<class MAP>
void find_hit(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.count(v);

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void find_erase(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        size_t sum = 0;
        for (const auto v : vList)
            sum += amap.count(v);
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void hash_find2(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.count(v);

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void hash_clear(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        auto n = amap.size();
        amap.clear();
        amap.clear();
        sum = amap.size();

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
        //printf("    %12s    erase    %5ld ns, factor = %.2f\n", map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class MAP>
void hash_copy(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        int64_t sum = 0;
        auto ts1 = getTime();
        MAP tmap = amap;
        amap = tmap;
        sum = amap.size();

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

#define FUNC_RANK() func_time.clear();//for (auto& v : func_time) printf("   %-4ld     %-49s\n",  v.first, v.second.c_str()); putchar('\n');

#ifndef PACK
#define PACK 128
#endif
struct RankItem
{
    RankItem()
    {
        lScore = 0;
        lUid = iRank = 0;
        iUpdateTime = 0;
    }

    RankItem(int64_t lUid1, int64_t lScore1 = 0, int iTime = 0)
    {
        lUid   = lUid1;
        lScore = lScore1;
        iUpdateTime = iTime;
    }

    int64_t operator ()()
    {
        return lScore;
    }

    int64_t lUid;
    int64_t lScore;
    int  iUpdateTime;
    int  iRank;
#if PACK >= 24
    char data[(PACK - 24) / 8 * 8];
#else
    std::string data;
#endif
};

#if PACK >= 24
static_assert(sizeof(RankItem) == PACK, "PACK >=24");
#endif

#include <chrono>

static uint32_t get32rand()
{
    return (rand() ^ (rand() << 15u) ^ (rand() << 30u));
}

static int64_t get64rand()
{
    return (((uint64_t)get32rand()) << 32) | get32rand();
}

static const std::array<char, 62> ALPHANUMERIC_CHARS = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

static const std::int64_t SEED = 0;
static std::mt19937_64 generator(SEED);
std::uniform_int_distribution<std::size_t> rd_uniform(0, ALPHANUMERIC_CHARS.size() - 1);

#ifndef KEY_INT
static std::string get_random_alphanum_string(std::size_t size) {
    std::string str(size, '\0');
    for(std::size_t i = 0; i < size; i++) {
        str[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    return str;
}
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
static int buildTestData(int size, std::vector<keyType>& rankdata)
{
    rankdata.reserve(size);
#ifndef KEY_INT
    for (int i = 0; i < size; i++)
        rankdata.emplace_back(get_random_alphanum_string(rand() % 10 + 6));
    return 0;
#else

    sfc64 srng(size);
#if AR > 0
    const auto iRation = AR;
#else
    const auto iRation = 10;
#endif

    auto flag = (int)srng() % 5 + 1;
    if (srng() % 100 > iRation)
    {
        emhash9::HashSet<keyType> eset(size);
        for (int i = 0; ; i++) {
            auto key = TO_KEY(srng());
            if (eset.insert(key).second) {
                rankdata.emplace_back(key);
                if (rankdata.size() >= size)
                    break;
            }
        }
        flag = 0;
    }
    else
    {
        const int pow2 = 2 << ilog(size, 2);
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
                    k += 64;
            } else if (flag == 5) {
                k = i * (uint64_t)pow2 + srng() % (pow2 / 8);
            }
            rankdata.emplace_back(k);
        }
    }

    return flag;
#endif
}

static int HashSetTest(int n, int max_loops = 1234567)
{
    emhash8::HashSet <keyType> eset;
    emhash9::HashSet <keyType> eset2;
    std::unordered_set <keyType> uset;

    const auto step = 1;// rand() % 64 + 1;
    eset.reserve(n);
    eset2.reserve(n);
    uset.reserve(n);

    sfc64 sfc;

    for (int i = 1; i < n * (step / 1); i += step) {
        auto ki = TO_KEY(i);
        eset.insert(ki);
        eset2.insert(ki);
        uset.insert(ki);
        assert(eset.size() == uset.size());
        assert(eset2.size() == uset.size());
        assert(eset.contains(ki));
        assert(eset2.contains(ki));
    }

    int loops = 0;
    while (loops++ < max_loops) {
        const int type = sfc() % 100;
        auto id = TO_KEY((sfc()) % (n * 2) );
        assert(eset.size() == uset.size());
        assert(eset2.size() == uset.size());
        assert(eset.count(id) == uset.count(id));
        assert(eset2.count(id) == uset.count(id));

        if (type <= 50 || uset.size() < 10000) {
            int id0 = uset.insert(id).second;
            int id1 = eset.insert(id).second;
            int id2 = eset2.insert(id).second;
            assert(id1 == id2);
            assert(id1 == id0);
            assert(eset2.size() == uset.size());
            assert(eset.size() == uset.size());
            assert(eset.count(id) == uset.count(id));
        }
        else if (type < 70) {
            if (sfc() % 8 == 0)
                id = *uset.begin();
            else if (sfc() % 8 == 0)
                id = *eset.begin();
            assert(eset.count(id) == uset.count(id));
            assert(eset2.count(id) == uset.count(id));
            uset.erase(id);
            eset.erase(id);
            eset2.erase(id);
            assert(eset.count(id) == uset.count(id));
        }
        else if (type < 80) {
            id = *uset.begin();
            assert(eset.count(id) == 1);
            assert(eset2.count(id) == 1);
            uset.erase(id);
            eset.erase(id);
            eset2.erase(id);
            assert(eset.count(id) == uset.count(id));
            assert(eset2.count(id) == uset.count(id));
            assert(eset.size() == uset.size());
        }
        else if (type < 90) {
            auto it = uset.begin();
            for (int i = sfc() % 32; i > 0; i--)
                it ++;
            id = *it;

            uset.erase(id);
            eset.erase(id);
            eset2.erase(id);
            assert(eset.count(id) == uset.count(id));
            assert(eset2.count(id) == uset.count(id));
            if (eset.count(id) == 1) {
                eset.erase(id);
            }
            assert (eset.size() == uset.size());
            assert (eset2.size() == uset.size());
        }
        else if (type < 98) {
            if (eset.count(id) == 0) {
                //                eset.insert_unique(id);
                //                uset.insert(id);
            }
        }
        if (loops % 1024 == 0) {
            printf("%zd %d\r", eset.size(), loops);
        }
    }

    printf("\n");
    return 0;
}

template<class MAP>
void benOneSet(MAP& hmap, const std::string& map_name, std::vector<keyType> vList)
{
    if (show_name.find(map_name) == show_name.end())
        return;

    check_result.clear();
    func_time.clear();

    insert_noreserve(hmap, map_name, vList);
    //insert_high_load <MAP>(map_name, vList);
    insert_small_size<MAP>(map_name, vList);
    insert_reserve(hmap, map_name, vList);

    //  shuffle(vList.begin(), vList.end());
    find_hit(hmap, map_name, vList);
    find_half(hmap, map_name, vList);
    //shuffle(vList.begin(), vList.end());
    find_miss(hmap, map_name, vList);

#ifdef KEY_INT
    for (int v = 0; v < (int)vList.size() / 2; v ++)
        vList[v] ++;
#else
    for (int v = 0; v < (int)vList.size() / 2; v ++)
        vList[v] = get_random_alphanum_string(rand() % 24 + 2);
#endif

    insert_find_erase(hmap, map_name, vList);

    //    shuffle(vList.begin(), vList.end());
    hash_erase(hmap, map_name, vList);
    find_erase(hmap, map_name, vList);

    //    shuffle(vList.begin(), vList.end());
    hash_reinsert(hmap, map_name, vList);

#if UF
    hash_iter(hmap, map_name, vList);
    hash_copy(hmap, map_name, vList);
    hash_clear(hmap, map_name, vList);
#endif
}

static int tcase = 0;
static void benchHashSet(int n)
{
    if (n < 10000)
        n = 123456;

    printf("%s n = %d, keyType = %s\n", __FUNCTION__, n, sKeyType);
    typedef std::hash<keyType>        std_func;
#ifdef HOOD_HASH
    typedef robin_hood::hash<keyType> hood_func;
    using hash_func = hood_func;
#else
     typedef std_func hash_func;
#endif

    float lf = 0.87f;
    map_time.clear();
    check_result.clear();
    std::vector<keyType> vList;
    auto flag = buildTestData(n, vList);

    if (n % 2 == 0)
    {
        { emhash8::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash8", vList); }
        { emhash6::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash6", vList); }
        { emhash9::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash9", vList); }
        { emhash7::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash7", vList); }
    }
    else
    {
        { emhash7::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash7", vList); }
        { emhash9::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash9", vList); }
        { emhash6::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash6", vList); }
        { emhash8::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash8", vList); }
    }

#if 0
    { std::unordered_set<keyType, hash_func> umap; umap.max_load_factor(lf);     benOneSet(umap, "stl_hash", vList); }
#endif

#ifdef ET
    { robin_hood::unordered_flat_set <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "martin", vList); }
    { phmap::flat_hash_set <keyType, hash_func> eset; eset.max_load_factor(lf);   benOneSet(eset, "phmap", vList); }
    { hrd7::hash_set <keyType, hash_func> eset; eset.max_load_factor(lf);         benOneSet(eset, "hrdhash", vList); }

#if ET > 1
    { ska::bytell_hash_set <keyType, hash_func > bmap; bmap.max_load_factor(lf);  benOneSet(bmap, "byte", vList); }
    { ska::flat_hash_set <keyType, hash_func> fmap; fmap.max_load_factor(lf);     benOneSet(fmap, "flat", vList); }
    { tsl::hopscotch_set <keyType, hash_func> hmap; hmap.max_load_factor(lf);     benOneSet(hmap, "hopsco", vList); }
    { tsl::robin_set  <keyType, hash_func> rmap; rmap.max_load_factor(lf);        benOneSet(rmap, "robin", vList); }
#endif
#endif

    std::multimap <int64_t, std::string> time_map;
    for (auto& v : map_time)
        time_map.insert(std::pair<int64_t, std::string>(v.second, v.first));

    printf("\n %d ======== n = %d, --------  flag = %d  ========\n", 1 + tcase, n, flag);
    const auto last  = time_map.rbegin()->first;
    const auto first = time_map.begin()->first;
    if (first + last < 20) {
        return;
    }

    static std::map<std::string,int64_t> rank;
    static std::map<std::string,int64_t> rank_time;
    static std::map<std::string, std::map<std::string, int64_t>> func_rank_time;

    auto it0 = time_map.begin();
    auto it1 = *(it0++);
    auto it2 = *(it0++);
    auto it3 = *(it0++);

    constexpr auto base1 = 300000000;
    constexpr auto base2 =      20000;

    //the top 3 func map
    if (it1.first == it3.first) {
        rank[it1.second] += base1 / 3;
        rank[it2.second] += base1 / 3;
        rank[it3.second] += base1 / 3;
    } else if (it1.first == it2.first) {
        rank[it1.second] += base1 / 2;
        rank[it2.second] += base1 / 2;
        rank[it3.second] += 1;
    } else {
        rank[it1.second] += base1;
        if (it2.first == it3.first) {
            rank[it2.second] += base2 / 2;
            rank[it3.second] += base2 / 2;
        } else {
            rank[it2.second] += base2;
            rank[it3.second] += 1;
        }
    }

    set_func_time(func_rank_time);
    for (auto& v : time_map) {
        rank_time[v.second] += (int)(first * 100 / v.first);
        printf("%5ld   %13s   (%4.2lf %6.1lf%%)\n", v.first * 1000l / n, v.second.c_str(), last * 1.0 / v.first, first * 100.0 / v.first);
    }

    if ((++tcase) % 5 == 0) {
        printf("\n------------------------- %d one ----------------------------------\n", tcase);
        dump_all(func_rank_time);
        puts("======== map  top1  top2  top3 =======================");
        for (auto& v : rank)
            printf("%13s %4.1lf %4.1lf %4ld\n", v.first.c_str(), v.second / (double)(base1), (v.second / (base2 / 2) % 1000) / 2.0, (v.second % (base2 / 2)));
        puts("======== map    score ================================");
        for (auto& v : rank_time)
            printf("%13s %4ld\n", v.first.c_str(), v.second / tcase);

#if _WIN32
        //        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
#else
        usleep(1000*4000);
#endif
        printf("--------------------------------------------------------------------\n\n");
        printf("------------------------- %d --------------------------------------\n\n", tcase);
        return;
    }

    printf("=======================================================================\n\n");
}

void testHashSet8(int n)
{
    n = 1 << n;
    emhash9::HashSet <int, std::hash<int>> eset(n);

    for (int i = 0; i < n; i++) {
        eset.insert(i);
    }
    for (int i = 0; i < n / 2; i++)
        eset.erase(i);

    for (auto it = eset.begin(); it != eset.end(); )
        it = eset.erase(it);
}

int main(int argc, char* argv[])
{
    srand((unsigned)time(0));
    for (int i = 10; i < 0; i++) {
        printf("%d\n", i);
        testHashSet8(i);
    }

    auto tn = 0;
    auto maxn = 4123456;
    double load_factor = 0.00945;
    printf("./sbench maxn f(0-100) d[2-6]mpsf t(n)\n");

    for (int i = 1; i < argc; i++) {
        const auto cmd = argv[i][0];
        if (isdigit(cmd))
            maxn = atoi(argv[i]) + 1000;
        else if (cmd == 'f' && isdigit(argv[i][1]))
            load_factor = atoi(&argv[i][0] + 1) / 100.0;
        else if (cmd == 't' && isdigit(argv[i][1]))
            tn = atoi(&argv[i][0] + 1);
        else if (cmd == 'd') {
            for (char c = argv[i][0], j = 0; c != '\0'; c = argv[i][j++]) {
                if (c >= '2' && c <= '9') {
                    std::string hash_name("emhash");
                    hash_name += c;
                    if (show_name.find(hash_name) != show_name.end())
                        show_name.erase(hash_name);
                    else
                        show_name[hash_name] = hash_name;
                }
                else if (c == 'm')
                    show_name.erase("martin");
                else if (c == 'p')
                    show_name.erase("phmap");
                else if (c == 't')
                    show_name.erase("robin");
                else if (c == 's')
                    show_name.erase("flat");
                else if (c == 'u')
                    show_name.emplace("stl_hash", "unordered_map");
            }
        }
    }

    if (tn > 100000)
        HashSetTest(tn, 434567);

    sfc64 srng;
    auto nows = time(0);

    while (true) {
        auto n = (srng() % maxn) + max_loop / 2;
        if (load_factor > 0.4 && load_factor < 1) {
            auto pow2 = 1 << ilog(n, 2);
            n = int(pow2 * load_factor) - (1 << 10) + (rand()) % (1 << 8);
        }
        benchHashSet(n);
    }

    return 0;
}
