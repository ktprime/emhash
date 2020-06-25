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
#include "sfc64.h"

//#define TP                1
#define HOOD_HASH         1
//#define EMH__LRU_SET    1
//#define EMH__IDENTITY_HASH 1
//#define EMH__REHASH_LOG   1
//#define EMH__SAFE_HASH      1
//#define EMH__STATIS    1
#define EMH__HIGH_LOAD 1

#ifndef TKey
    #define TKey  0
#endif

#include "hash_set.hpp" //
#include "hash_set2.hpp" //
#include "hash_set3.hpp" //

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
//https://martin.ankerl.com/2016/09/21/very-fast-hashmap-in-c-part-3/
//https://andre.arko.net/2017/08/24/robin-hood-hashing/
//http://www.ilikebigbits.com/2016_08_28_hash_table.html
//http://www.idryman.org/blog/2017/05/03/writing-a-damn-fast-hash-table-with-tiny-memory-footprints/

#if __cplusplus >= 199711LL
    #define _C11_HASH   1
    #include "./phmap/phmap.h"
    #include "./tsl/robin_set.h"        //https://github.com/tessil/robin-map
    #include "./tsl/hopscotch_set.h"    //https://github.com/tessil/hopscotch-map
    #include "./martin/robin_hood.h"       //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
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
    #include <unistd.h>
    #include <sys/resource.h>
#endif

struct RankItem;

#if TKey == 0
    typedef unsigned int         keyType;
    #define TO_KEY(i)   (keyType)i
    #define sKeyType    "int"
    #define KEY_INT
    #define TO_SUM(i)   i
#elif TKey == 1
    typedef int64_t       keyType;
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

std::unordered_map<std::string, std::string> show_name = {
//    {"stl_hash", "unordered_map"},

    {"emhash7", "emhash7"},
    {"emhash9", "emhash9"},
    {"emhash8", "emhash8"},
    {"phmap", "phmap flat"},


    {"flat", "skarupk flat"},
    {"robin", "tessil robin"},
    {"hopsco", "tessil hopsco"},

    {"byte", "skarupk byte"},
};

static int64_t getTime()
{
#if _WIN32
    FILETIME ptime[4] = {0};
    GetThreadTimes(GetCurrentThread(), &ptime[0], &ptime[1], &ptime[2], &ptime[3]);
    return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10;
#elif __linux__ || __unix__
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000000 + usec;
#elif _WIN32
    return clock() * 1000ll;
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

static std::map<std::string, int64_t>  check_result;
static std::multimap <int64_t, std::string> func_time;
static std::map<std::string, int64_t> map_time;
static std::map<std::string, std::map<std::string, int64_t>> func_map_time;

#define AVE_TIME(ts, n)             1000 * (getTime() - ts) / (n)

static void check_mapfunc_result(const std::string& map_name, const std::string& func, long sum, long ts1)
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
long hash_iter(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
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
    return sum;
}

template<class MAP>
long hash_reinsert(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.insert(v).second;

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
//        printf("    %12s    reinsert  %5lld ns, factor = %.2f\n", map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
    return sum;
}

template<class MAP>
long hash_insert(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
#if 0
        amap.insert(vList.begin(), vList.end());
        sum = amap.size();
#else
        for (const auto v : vList)
            sum += amap.emplace(v).second;
#endif

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);

        //printf("    %12s    %s %5lld ns, factor/size = %.2f %zd\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor(), amap.size());
    }
    return sum;
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
        printf("    %12s  %s  %5zd ns, factor = %.2f\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class MAP>
long hash_emplace(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.emplace(v).second;

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
//      printf("    %12s    %s  %5lld ns, factor = %.2f\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
    return sum;
}

template<class MAP>
long hash_miss(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto n = vList.size();
        auto ts1 = getTime();
        for (size_t v = 1; v < 2*n; v++)
            sum += amap.count(TO_KEY(v));

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
    return sum;
}

template<class MAP>
void find_half(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())\
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

template<class MAP>
long hash_erase(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.erase(v);
//        for (auto it = amap.begin(); it != amap.end(); ) {
//            it = amap.erase(it);
//            sum++;
//        }

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
        //printf("    %12s    %s  %5lld ns, size = %zd\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), sum);
    }
    return sum;
}

template<class MAP>
long hash_find(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.count(v);

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
    return sum;
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
long hash_find2(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += amap.count(v);

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
    return sum;
}

template<class MAP>
long hash_clear(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        auto n = amap.size();
        amap.clear();
        amap.clear();
        sum = amap.size();

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
        //printf("    %12s    erase    %5lld ns, factor = %.2f\n", map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
    return 0;
}

template<class MAP>
long hash_copy(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    long sum = 0;
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        MAP tmap = amap;
        amap = tmap;
        sum = amap.size();

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
    return sum;
}

#define FUNC_RANK() func_time.clear();//for (auto& v : func_time) printf("   %-4lld     %-49s\n",  v.first, v.second.c_str()); putchar('\n');

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

/*:
    bool operator < (const RankItem& r) const
    {
        if (lScore != r.lScore)
            return lScore > r.lScore;
        else if (iUpdateTime != r.iUpdateTime)
            return iUpdateTime < r.iUpdateTime;
        return lUid < r.lUid;
    }

    bool operator == (const RankItem &r) const
    {
        if (lUid == r.lUid) {
            return true;
        }

        return false;
    }

    RankItem& operator += (const RankItem& r)
    {
        lScore += r.lScore;
        iRank   = r.iRank;
        //        iUpdateTime = time(0);
        return *this;
    }

    RankItem& operator += (int64_t r)
    {
        lScore += r;
        return *this;
    }
**/
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

static std::string get_random_alphanum_string(std::size_t size) {
    std::string str(size, '\0');
    for(std::size_t i = 0; i < size; i++) {
        str[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    return str;
}

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

    sfc64 srng;
    auto flag = (int)rand() % 5 + 1;
#if AR > 0
    const auto iRation = AR;
#else
    const auto iRation = 10;
#endif

    if (rand() % 100 > iRation)
    {
        emhash9::HashSet<keyType> eset(size);
        for (int i = 0; ; i++) {
#if 1
            auto key = TO_KEY(srng());
#else
            auto key = (keyType)get64rand();
#endif
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
                k += pow2 + 32 - rand() % 64;
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

    printf("flag = %d\n", flag);
    return flag;
#endif
}

static int HashSetTest(int n, int max_loops = 1234567)
{
    emhash8::HashSet <keyType> eset;
    emhash9::HashSet <keyType> eset2;
#if 1
    tsl::robin_set<keyType> uset;
#else
    std::unordered_set <keyType> uset;
#endif

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
    hash_insert(hmap, map_name, vList);

//  shuffle(vList.begin(), vList.end());
    hash_find(hmap, map_name, vList);
    find_half(hmap, map_name, vList);
    //shuffle(vList.begin(), vList.end());
    hash_miss(hmap, map_name, vList);

#ifdef KEY_INT
    for (int v = 0; v < (int)vList.size() / 2; v ++)
        vList[v] ++;
#else
    for (int v = 0; v < (int)vList.size() / 2; v ++)
        vList[v] = get_random_alphanum_string(rand() % 24 + 2);
#endif

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

static void benchMarkHashSet2(int n)
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

    float lf = 0.90f;
    map_time.clear();
    check_result.clear();
    std::vector<keyType> vList;
    auto step = buildTestData(n, vList);

    { emhash8::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "emhash8", vList); }
    { phmap::flat_hash_set <keyType, hash_func> eset; eset.max_load_factor(lf);  benOneSet(eset, "phmap", vList); }
    { emhash9::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);        benOneSet(eset, "emhash9", vList); }
    { emhash7::HashSet <keyType, hash_func> eset; eset.max_load_factor(lf);        benOneSet(eset, "emhash7", vList); }

    { std::unordered_set<keyType, hash_func> umap; umap.max_load_factor(lf);        benOneSet(umap, "stl_hash", vList); }

#ifdef _C11_HASH
    { ska::bytell_hash_set <keyType, hash_func > bmap; bmap.max_load_factor(lf);        benOneSet(bmap, "byte", vList); }
    { ska::flat_hash_set <keyType, hash_func> fmap; fmap.max_load_factor(lf);        benOneSet(fmap, "flat", vList); }
    { tsl::hopscotch_set <keyType, hash_func> hmap; hmap.max_load_factor(lf);        benOneSet(hmap, "hopsco", vList); }
    { tsl::robin_set  <keyType, hash_func> rmap; rmap.max_load_factor(lf);        benOneSet(rmap, "robin", vList); }
#endif


    static int tcase = 1;
    printf("\n %d ======== n = %d, flag = %d hash_map ========\n", tcase, n, step);
    std::multimap <int64_t, std::string> time_map;
    for (auto& v : map_time)
        time_map.insert(std::pair<int64_t, std::string>(v.second, v.first));

    const auto last  = double(time_map.rbegin()->first);
    const auto first = double(time_map.begin()->first);
    if (first < 10 || last < 9)
        return;

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

    if (tcase++ % 5 == 0) {
        printf("\n------------------------- %d one ----------------------------------\n", tcase - 1);
        dump_all(func_rank_time);
        puts("======== map  top1  top2  top3 =======================");
        for (auto& v : rank)
            printf("%13s %10ld  %4.1lf %4.1lf %4lld\n", v.first.c_str(), v.second, v.second / (double)(base1), (v.second / (base2 / 2) % 1000) / 2.0, (v.second % (base2 / 2)));
        puts("======== map    score ================================");
        for (auto& v : rank_time)
            printf("%13s %4ld\n", v.first.c_str(), v.second / (tcase - 1));

#if _WIN32
        //        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
#else
        usleep(1000*4000);
#endif
        printf("--------------------------------------------------------------------\n\n");
        printf("------------------------- %d --------------------------------------\n\n", tcase - 1);
        return;
    }

    printf("=======================================================================\n\n");
}

static void benchMarkHashSet(int n)
{
    if (n < 10000)
        n = 123456;
    printf("%s n = %d, keyType = %s\n", __FUNCTION__, n, sKeyType);

#ifdef HOOD_HASH
    typedef robin_hood::hash<keyType> hash_func;
#else
    typedef std::hash<keyType> hash_func;
#endif

    map_time.clear();
    check_result.clear();
    func_time.clear();
    func_map_time.clear();

    emhash9::HashSet <keyType, hash_func> emap9;
    emhash7::HashSet <keyType, hash_func> emap7;
    emhash8::HashSet <keyType, hash_func> emap8;

#if IR == 0
    const auto iRation = n / 1;
#else
    const auto iRation = n / IR;
#endif

    float lf = 0.85f;

#ifdef _C11_HASH
    ska::flat_hash_set   <keyType, hash_func> fmap;
    ska::bytell_hash_set <keyType, hash_func > bmap;
    tsl::hopscotch_set   <keyType, hash_func> hmap;
    tsl::robin_set       <keyType, hash_func> rmap;
    phmap::flat_hash_set <keyType, hash_func> pmap;

    fmap.max_load_factor(lf); bmap.max_load_factor(lf); hmap.max_load_factor(lf); rmap.max_load_factor(lf); pmap.max_load_factor(lf);
    fmap.reserve(iRation);    bmap.reserve(iRation);    hmap.reserve(iRation);    rmap.reserve(iRation);    pmap.reserve(iRation);
#endif

    std::unordered_set<keyType, hash_func> umap(iRation);
    emap9.max_load_factor(lf); emap7.max_load_factor(lf); emap8.max_load_factor(lf); umap.max_load_factor(lf);
    emap9.reserve(iRation);    emap7.reserve(iRation);    emap8.reserve(iRation);    umap.reserve(iRation);

    std::vector<keyType> vList;
    auto step = buildTestData(n, vList);
    //std::sort(vList.begin(), vList.end());

    {
        puts("1. rand insert");

#ifndef TP
        hash_insert(umap, "stl_hash", vList);
#endif
        hash_insert(emap9, "emhash9", vList);
        hash_insert(emap7, "emhash7", vList);
        hash_insert(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_insert(bmap, "byte", vList);
        hash_insert(pmap, "phmap", vList);

        try
        {
            hash_insert(fmap, "flat", vList);
            hash_insert(hmap, "hopsco", vList);
            hash_insert(rmap, "robin", vList);
        }
        catch(const std::exception& e)
        {
            printf("e = %s\n",e.what());
            return;
        }
#endif
        FUNC_RANK()
//        emap7.dump_statis();
    }

    {
        puts("2. find hit 100%");
        shuffle(vList.begin(), vList.end());

#ifndef TP
        hash_find(umap, "stl_hash", vList);
#endif
        hash_find(emap9, "emhash9", vList);
        hash_find(emap7, "emhash7", vList);
        hash_find(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_find(bmap, "byte", vList);
        hash_find(pmap, "phmap", vList);
        hash_find(hmap, "hopsco", vList);
        hash_find(fmap, "flat", vList);
        hash_find(rmap, "robin", vList);
#endif

        FUNC_RANK()
    }

    {
        puts("3. find miss");

#ifndef TP
        hash_miss(umap, "stl_hash", vList);
#endif
        hash_miss(emap9, "emhash9", vList);
        hash_miss(emap7, "emhash7", vList);
        hash_miss(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_miss(pmap, "phmap", vList);
        hash_miss(bmap, "byte", vList); hash_miss(hmap, "hopsco", vList);
        hash_miss(fmap, "flat", vList); hash_miss(rmap, "robin", vList);
#endif
        FUNC_RANK()
    }

    {
#ifdef KEY_INT
        for (int v = 0; v < (int)vList.size() / 2; v ++)
            vList[v] ++;
#else
        for (int v = 0; v < (int)vList.size() / 2; v ++)
            vList[v] = get_random_alphanum_string(rand() % 24 + 2);
#endif
        shuffle(vList.begin(), vList.end());
    }

    if (1)
    {
        puts("4. erase 50% key");
        shuffle(vList.begin(), vList.end());

#ifndef TP
        hash_erase(umap, "stl_hash", vList);
#endif
        hash_erase(emap9, "emhash9", vList);
        hash_erase(emap7, "emhash7", vList);
        hash_erase(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_erase(pmap, "phmap", vList);
        hash_erase(fmap, "flat", vList); hash_erase(rmap, "robin", vList);
        hash_erase(bmap, "byte", vList); hash_erase(hmap, "hopsco", vList);
#endif
        FUNC_RANK()
    }

    {
        puts("5. find ease 50% key");
#ifndef TP
        hash_find2(umap, "stl_hash", vList);
#endif
        hash_find2(emap9, "emhash9", vList);
        hash_find2(emap7, "emhash7", vList);
        hash_find2(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_find2(pmap, "phmap", vList);
        hash_find2(bmap, "byte", vList);
        hash_find2(hmap, "hopsco", vList);
        hash_find2(fmap, "flat", vList);
        hash_find2(rmap, "robin", vList);
#endif
        FUNC_RANK()
    }

    {
        puts("6. add 50% new key");
        shuffle(vList.begin(), vList.end());

#ifndef TP
        hash_reinsert(umap, "stl_hash", vList);
#endif
        hash_reinsert(emap9, "emhash9", vList);
        hash_reinsert(emap7, "emhash7", vList);
        hash_reinsert(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_reinsert(bmap, "byte", vList); hash_reinsert(hmap, "hopsco", vList);
        hash_reinsert(fmap, "flat", vList); hash_reinsert(rmap, "robin", vList);
        hash_reinsert(pmap, "phmap", vList);
#endif
        //        emap7.dump_statis();
        FUNC_RANK()
        lf = emap8.load_factor();
    }

    if (1)
    {
        puts("7. iterator all");

        hash_iter(emap7, "emhash7", vList);

#ifndef TP
        hash_iter(umap, "stl_hash", vList);
#endif
        hash_iter(emap9, "emhash9", vList);
        hash_iter(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_iter(fmap, "flat", vList); hash_iter(rmap, "robin", vList);
        hash_iter(bmap, "byte", vList); hash_iter(hmap, "hopsco", vList);
        hash_iter(pmap, "phmap", vList);
#endif

        FUNC_RANK()
    }

    if (1)
    {
        puts("8. copy all");

        hash_copy(emap7, "emhash7", vList);
        hash_copy(emap9, "emhash9", vList);
        hash_copy(emap8, "emhash8", vList);

#ifndef TP
        hash_copy(umap, "stl_hash", vList);
#endif

#ifdef _C11_HASH
        hash_copy(fmap, "flat", vList); hash_copy(rmap, "robin", vList);
        hash_copy(bmap, "byte", vList); hash_copy(hmap, "hopsco", vList);
        hash_copy(pmap, "phmap", vList);
#endif
        FUNC_RANK()
    }

    if (1)
    {
        puts("9. clear all");

#ifndef TP
        hash_clear(umap, "stl_hash", vList);
#endif
        hash_clear(emap9, "emhash9", vList);
        hash_clear(emap7, "emhash7", vList);
        hash_clear(emap8, "emhash8", vList);

#ifdef _C11_HASH
        hash_clear(bmap, "byte", vList);
        hash_clear(fmap, "flat", vList);
        hash_clear(rmap, "robin", vList);
        hash_clear(hmap, "hopsco", vList);
        hash_clear(pmap, "phmap", vList);
#endif
        FUNC_RANK()
    }

    static int tcase = 1;
    printf("\n %d ======== n = %d, load_facor = %.2f, flag = %d hash_map ========\n", tcase, n, lf, step);
    //    map_time.erase("unordered_map");
    std::multimap <int64_t, std::string> time_map;
    for (auto& v : map_time)
        time_map.insert(std::pair<int64_t, std::string>(v.second, v.first));

    const auto last  = double(time_map.rbegin()->first);
    const auto first = double(time_map.begin()->first);
    if (first < 10 || last < 9)
        return;

    static std::map<std::string,int64_t> rank;
    static std::map<std::string,int64_t> rank_time;
    static std::map<std::string, std::map<std::string, int64_t>> func_rank_time;

    auto it0 = time_map.begin();
    auto it1 = *(it0++);
    auto it2 = *(it0++);
    auto it3 = *(it0++);

    constexpr auto base1 = 300000000;
    constexpr auto base2 =      20000;

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
        printf("%5lld   %13s   (%4.2lf %6.1lf%%)\n", v.first * 1000ll / n, v.second.c_str(), last * 1.0 / v.first, first * 100.0 / v.first);
    }

    if (tcase++ % 6 == 0) {
        printf("\n------------------------- %d --------------------------------------\n", tcase - 1);
        dump_all(func_rank_time);
        for (auto& v : rank)
            printf("%13s %10ld  %4.1lf %4.1lf %4ld\n", v.first.c_str(), v.second, v.second / (double)(base1), (v.second / (base2 / 2) % 1000) / 2.0, (v.second % (base2 / 2)));
        for (auto& v : rank_time)
            printf("%13s %4ld\n", v.first.c_str(), v.second / (tcase - 1));

#if _WIN32
        Sleep(4000);
#else
        usleep(1000*4000);
#endif
        printf("------------------------- %d --------------------------------------\n\n", tcase - 1);
        return;
    }

    printf("=======================================================================\n\n");
}

void testHashSet8(int n)
{
    n = 1 << n;
    emhash8::HashSet <int, std::hash<int>> eset(n);

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
    auto n = 1234567;

    for (int i = 10; i < 20; i++) {
        printf("%d\n", i);
        testHashSet8(i);
    }

    printf("./test n load_factor (key=%s)\n", sKeyType);
    double load_factor = 0.1f;

#if LF
    load_factor = LF / 100.0;
#endif

    if (argc > 1 && argv[1][0] >= '0' && argv[1][0] <= '9')
        n = atoi(argv[1]);
    if (argc > 2)
        load_factor = atoi(argv[2]) / 100.0;

//    HashSetTest(n, 234567);

//    auto nows = time(0);

    while (true) {
#if INP
        char ccmd[257];
        printf(">> ");
        if (fgets(ccmd, 255, stdin)) {
            auto n = atol(ccmd);
            if (n == 0)
                n = (get32rand() >> 9) + 14567;
            else if (n < 0)
                break;
            if (load_factor > 0.4 && load_factor < 0.99) {
                int log2 = ilog(n, 2);
                n = int((1 << log2) * load_factor) + rand() % (1 << 10);
            }

            benchMarkHashSet(n);
        }
#else
        n = (get32rand() % 3123456) + 123456;
        if (n >= 22345678)
            break;
        auto pow2 = 1 << ilog(n, 2);

#if LF > 100
        n = pow2 * (60 + rand()% 35) / 1000;
#endif

        if (load_factor > 0.4 && load_factor < 0.95)
            n = int(pow2 * load_factor) + rand() % (1 << 10);

//        benchMarkHashSet(n);
        benchMarkHashSet2(n);

#endif

#if TP || GCOV
        if (time(0) > nows + 20)
            break;
#endif
    }

    return 0;
}
