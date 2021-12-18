#include "util.h"
#include <algorithm>

#ifndef TKey
    #define TKey              1
#endif
#ifndef TVal
    #define TVal              1
#endif
#if __GNUC__ > 4 && __linux__
#include <ext/pb_ds/assoc_container.hpp>
#endif

#ifndef ET
#define ET                 1
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
//    {"lru_size", "lru_size"},

    {"emilib2", "emilib2"},
    {"emilib", "emilib"},
    {"emilib1", "emilib1"},
//    {"simd_hash", "simd_hash"},
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

//    {"robin", "tsl_robin"},
//    {"flat", "ska_flat"},

    {"hopsco", "tsl_hopsco"},
    {"byte", "ska_byte"},
#endif
};

#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
#define PHMAP_HAVE_SSSE3      1
#endif

//rand data type
#ifndef RT
    #define RT 2  //1 wyrand 2 Sfc4 3 RomuDuoJr 4 Lehmer64 5 mt19937_64
#endif

//#define CUCKOO_HASHMAP       1
//#define EM3               1
//#define PHMAP_HASH        1
//#define WY_HASH           1

//#define FL1                 1

//feature of emhash
//#define EMH_FIBONACCI_HASH  1
//#define EMH_BUCKET_INDEX    2
//#define EMH_REHASH_LOG      1234567

//#define EMH_STATIS          1
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
//https://gankra.github.io/blah/hashbrown-tldr/ swiss
//https://leventov.medium.com/hash-table-tradeoffs-cpu-memory-and-variability-22dc944e6b9a

#if __linux__ && AVX2
#include <sys/mman.h>
#include "fht/fht_ht.hpp" //https://github.com/goldsteinn/hashtable_test/blob/master/my_table/fht_ht.hpp
#define FHT_HMAP          1
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

#include "emilib/emilib2.hpp"
#include "emilib/emilib.hpp"
//#include "emilib/emilib21.hpp"

#if ET
//    #include "emilib/emilib32.hpp"
//    #include "zhashmap/hashmap.h"

#if __x86_64__ || _WIN64

#if __cplusplus > 201402L || _MSVC_LANG >= 201402L
    #include "hrd/hash_set7.h"        //https://github.com/hordi/hash/blob/master/include/hash_set7.h
#endif
    #include "emilib/emilib33.hpp"
//    #include "simd_hash_map/simd_hash_map.hpp"
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
#elif TKey == 4
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

static int test_case = 0;
static int loop_vector_time = 0;
static int func_index = 0, func_size = 10;
static int func_first = 0, func_last = 0;
static float hlf = 0.0;

static std::map<std::string, int64_t> func_result;
//func:hash -> time
static std::map<std::string, std::map<std::string, int64_t>> once_func_hash_time;

static void check_func_result(const std::string& hash_name, const std::string& func, size_t sum, int64_t ts1, int weigh = 1)
{
    if (func_result.find(func) == func_result.end()) {
        func_result[func] = sum;
    } else if (sum != func_result[func]) {
        printf("%s %s %zd != %zd (o)\n", hash_name.data(), func.data(), (size_t)sum, (size_t)func_result[func]);
    }

    auto& showname = maps[hash_name];
    once_func_hash_time[func][showname] += (getus() - ts1) / weigh;
    func_index ++;

    long ts = (getus() - ts1) / 1000;

    if (func_index == func_first)
        printf("%8s  (%.3f): %8s %4ld, ", hash_name.data(), hlf, func.data(), ts);

    for (int i = 0; i <= 3; i++) {
        if (func_index == (func_first + i) % func_size + 1) {
            printf("%8s %4ld, ", func.data(), ts);
            break;
        }
    }
    if (func_index == func_last)
        printf("\n");
}

static void inline hash_convert(const std::map<std::string, int64_t>& hash_score, std::multimap <int64_t, std::string>& score_hash)
{
    for (const auto& v : hash_score)
        score_hash.emplace(v.second, v.first);
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
        printf("%5d   %13s   (%4.2lf %6.1lf%%)\n", int(v.first / (func_index - 1)), v.second.data(), last * 1.0 / v.first, first * 100.0 / v.first);
    }
}

static void dump_func(const std::string& func, const std::map<std::string, int64_t >& hash_rtime, std::map<std::string, int64_t>& hash_score, std::map<std::string, std::map<std::string, int64_t>>& hash_func_score)
{
    std::multimap <int64_t, std::string> rscore_hash;
    hash_convert(hash_rtime, rscore_hash);

    puts(func.data());

    auto mins = rscore_hash.begin()->first;
    for (auto& v : rscore_hash) {
        hash_score[v.second] += (int)((mins * 100) / (v.first + 1e-3));

        //hash_func_score[v.second][func] = (int)((mins * 100) / (v.first + 1));
        hash_func_score[v.second][func] = v.first / test_case;
        printf("%4d        %-20s   %-2.1f%%\n", (int)(v.first / test_case), v.second.data(), ((mins * 100.0f) / v.first));
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

    //puts(pys.data());
    std::ofstream  outfile;
    auto full_file = file + ".py";
    outfile.open("./" + full_file, std::fstream::out | std::fstream::trunc | std::fstream::binary);
    if (outfile.is_open())
        outfile << pys;
    else
        printf("\n\n =============== can not open %s ==============\n\n", full_file.data());

    outfile.close();
}

template<class hash_type>
void hash_iter(const hash_type& ht_hash, const std::string& hash_name)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : ht_hash)
        sum += 1;

    for (auto v : ht_hash)
        sum += 1;

    for (auto it = ht_hash.begin(); it != ht_hash.end(); it++)
#if KEY_INT
        sum += it->first;
#elif KEY_CLA
    sum += it->first.lScore;
#else
    sum += it->first.size();
#endif

    for (auto it = ht_hash.cbegin(); it != ht_hash.cend(); ++it)
#if KEY_INT
        sum += it->first;
#elif KEY_CLA
    sum += it->first.lScore;
#else
    sum += it->first.size();
#endif

#ifndef SMAP
    hlf = ht_hash.load_factor();
#endif
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_reinsert(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        ht_hash.emplace(v, TO_VAL(0));
        sum ++;
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void insert_erase(const std::string& hash_name, const std::vector<keyType>& vList)
{
#if TVal < 2
    hash_type ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    const auto vsmall = 1024;
    for (size_t i = 0; i < vList.size(); i++) {
        sum += ht_hash.emplace(vList[i], TO_VAL(0)).second;
        if (i > vsmall)
            ht_hash.erase(vList[i - vsmall]);
    }

    if (vList.size() % 3 == 0)
        ht_hash.clear();
    const auto vmedium = vList.size() / 100;
    for (size_t i = 0; i < vList.size(); i++) {
        auto it = ht_hash.emplace(vList[i], TO_VAL(0));
        if (i > vmedium)
            ht_hash.erase(it.first);
    }

    if (vList.size() % 2 == 0)
        ht_hash.clear();
    const auto vsize = vList.size() / 8;
    for (size_t i = 0; i < vList.size(); i++) {
        ht_hash[vList[i]] = TO_VAL(0);
        if (i > vsize)
            sum += ht_hash.erase(vList[i - vsize]);
    }

    check_func_result(hash_name, __FUNCTION__, sum, ts1);
#endif
}

template<class hash_type>
void insert_no_reserve(const std::string& hash_name, const std::vector<keyType>& vList)
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
    //ht_hash.max_load_factor(0.99f);
    ht_hash.reserve(vList.size());
#endif

    for (const auto& v : vList)
        sum += ht_hash.emplace(v, TO_VAL(0)).second;
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void multi_small_ife(const std::string& hash_name, const std::vector<keyType>& vList)
{
#if KEY_INT
    size_t sum = 0;
    const auto hash_size = vList.size() / 10003 + 200;
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
    auto ts1 = getus(); size_t sum = 1;
    hash_type tmp(ht_hash);

    for (auto & v : vList) {
#if KEY_INT
        auto v2 = v % 2 == 0 ? v + sum : v - sum;
#elif KEY_CLA
        auto v2(v.lScore + sum);
#elif TKey != 4
        v += char(128 + (int)v[0]);
        const auto &v2 = v;
#else
        const keyType v2(v.data(), v.size() - 1);
#endif
        auto it = tmp.emplace(v2, TO_VAL(0));
        sum += tmp.count(v2);
        tmp.erase(it.first);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1, 3);
}

template<class hash_type>
void insert_cache_size(const std::string& hash_name, const std::vector<keyType>& vList, const char* level, const uint32_t cache_size, const uint32_t min_size)
{
    auto ts1 = getus(); size_t sum = 0;
    const auto lsize = cache_size + vList.size() % min_size;
    hash_type tmp, empty;
#ifndef SMAP
    if (vList.size() % 4 == 0)
        empty.max_load_factor(0.80);
//    if (vList.size() % 8 == 0)
//        empty.reserve(cache_size);
#endif

    tmp = empty;
    for (const auto& v : vList)
    {
        sum += tmp.emplace(v, TO_VAL(0)).second;
        //sum += tmp.count(v);
        if (tmp.size() > lsize) {
            if (lsize % 2 == 0)
                tmp.clear();
            else
                tmp = std::move(empty);
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
    tmp.max_load_factor(max_loadf);
    tmp.reserve(pow2 / 2);
#endif
    int minn = (max_loadf - 0.2f) * pow2, maxn = int(max_loadf * pow2);
    int i = 0;

    for (; i < minn; i++) {
        if (i < (int)vList.size())
            tmp.emplace(vList[i], TO_VAL(0));
        else {
            auto& v = vList[i - vList.size()];
#if KEY_INT
            auto v2 = v - i;
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
        auto v2 = v + i;
#elif KEY_CLA
        auto v2 = (v.lScore / 7) + 4 * v.lScore;
#elif TKey != 4
        auto v2 = v; v2[0] += '1';
#else
        const keyType v2(v.data(), v.size() - 1);
#endif
        //tmp[v2] = TO_VAL(0);
        tmp.emplace(v2, TO_VAL(0));
        sum += tmp.count(v2);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

#if FL1
static uint8_t l1_cache[64 * 1024];
#endif
template<class hash_type>
void find_hit_0(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto n = ht_hash.size();
    size_t pow2 = 2u << ilog(n, 2), sum = 0;

#if KEY_STR
#if TKey != 4
    auto skey = get_random_alphanum_string(STR_SIZE);
#endif
#endif

    auto ts1 = getus();
    for (const auto& v : vList) {
#if KEY_STR
#if TKey != 4
        skey[v.size() % STR_SIZE + 1] ++;
        sum += ht_hash.count(skey);
#else
        std::string_view skey = "notfind_view";
        sum += ht_hash.count(skey);
#endif
#else
        keyType v2 = TO_KEY(v + 2048 - (v % 4096));
        sum += ht_hash.find(v2) != ht_hash.end();
#endif
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_hit_50(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
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
void find_hit_50_erase(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto tmp = ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        auto it = tmp.find(v);
        if (it != tmp.end())
            tmp.erase(it);
        else
            sum ++;
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_hit_100(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
#if KEY_INT
        sum += ht_hash.count(v);
#elif KEY_CLA
        sum += ht_hash.count(v);
#else
        sum += ht_hash.count(v);
#endif
#if FL1
        if (sum % (1024 * 64) == 0)
            memset(l1_cache, 0, sizeof(l1_cache));
#endif
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void find_erase_50(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        sum += ht_hash.count(v);
        sum += ht_hash.find(v) != ht_hash.end();
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void erase_50(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto tmp = ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ht_hash.erase(v);

#ifndef ABSL
    int flag = 0;
    for (auto it = tmp.begin(); it != tmp.end(); ) {
       if (++flag & 1)
           it = tmp.erase(it);
       else
           ++it;
    }
#endif
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
void hash_clear(hash_type& ht_hash, const std::string& hash_name)
{
    if (ht_hash.size() > 1000000) {
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
    sum += thash.size();

    ht_hash = thash;
    sum  += ht_hash.size();

    ht_hash = std::move(thash);
    sum  += ht_hash.size();

    ht_hash.clear(); thash.clear();
    ht_hash.clear(); thash.clear();
    sum  += ht_hash.size();
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

#if PACK >= 24 && VCOMP == 0
static_assert(sizeof(StructValue) == PACK, "PACK >=24");
#endif

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
#if TKey != 4
        randdata.emplace_back(get_random_alphanum_string(srng() % STR_SIZE + 4));
#else
        randdata.emplace_back(get_random_alphanum_string_view(srng() % STR_SIZE + 4));
#endif

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
#if 0

#ifndef KEY_CLA
    emhash5::HashMap <keyType, int> ehash5;
    emilib::HashMap <keyType, int> ehash2;

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
#if 0
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
        const uint32_t l2_size = (256 * 1024)   / (sizeof(keyType) + sizeof(valueType));
        const uint32_t l3_size = (8 * 1024 * 1024) / (sizeof(keyType) + sizeof(valueType));

        func_index = 0;
#if 1
        multi_small_ife <hash_type>(hash_name, oList);

        insert_erase      <hash_type>(hash_name, oList);
        insert_high_load  <hash_type>(hash_name, oList);

        insert_cache_size <hash_type>(hash_name, oList, "insert_l1_cache", l1_size, l1_size + 1000);
        insert_cache_size <hash_type>(hash_name, oList, "insert_l2_cache", l2_size, l2_size + 1000);
        insert_cache_size <hash_type>(hash_name, oList, "insert_l3_cache", l3_size, l3_size + 1000);

        insert_no_reserve <hash_type>(hash_name, oList);

        insert_reserve<hash_type>(hash,hash_name, oList);
        find_hit_100  <hash_type>(hash,hash_name, oList);
        find_hit_0  <hash_type>(hash,hash_name, oList);

        auto nList = oList;
        for (size_t v = 0; v < nList.size(); v += 2) {
            auto& next = nList[v];
#if KEY_INT
            next += nList.size() / 2 - v;
#elif KEY_CLA
            next.lScore += nList.size() / 2 - v;
#elif TKey != 4
            next[v % next.size()] += 1;
#else
            next = next.substr(0, next.size() - 1);
#endif
        }

        find_hit_50<hash_type>(hash, hash_name, nList);
        find_hit_50_erase<hash_type>(hash, hash_name, nList);
        erase_50<hash_type>(hash, hash_name, nList);
        find_erase_50<hash_type>(hash, hash_name, oList);
        insert_find_erase<hash_type>(hash, hash_name, nList);

        erase_reinsert<hash_type>(hash, hash_name, oList);
        hash_iter<hash_type>(hash, hash_name);

#ifndef UF
        copy_clear <hash_type>(hash, hash_name);
        //hash_clear<hash_type>(hash, hash_name);
#endif
#endif

        func_size = func_index;
    }
}

constexpr static auto base1 = 300000000;
constexpr static auto base2 =      20000;
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
        printf("%13s %4.1lf  %4.1lf %4d\n", v.first.data(), v.second / (double)(base1), (v.second / (base2 / 2) % 1000) / 2.0, (int)(v.second % (base2 / 2)));

    auto maxs = score_hash.rbegin()->first;
    //print hash rank
    puts("======== hash    score  weigh ==========================");
    for (auto& v : score_hash)
        printf("%13s  %4d     %3.1lf%%\n", v.second.data(), (int)(v.first / func_hash_score.size()), (v.first * 100.0 / maxs));

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
#elif KEY_INT && FIB_HASH >= 0
    using ehash_func = Int64Hasher<keyType>;
#elif KEY_CLA
    using ehash_func = StuHasher;
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
        printf("n = %d, keyType = %s, valueType = %s(%zd), loop_sum = %d us, sum = %d\n",
                n, sKeyType, sValueType, sizeof(valueType), (int)(loop_vector_time), (int)sum);
    }

    {
        func_first = func_first % func_size + 1;
        func_last  = (func_first + 3) % func_size + 1;

#if ET > 2
        {  benOneHash<tsl::hopscotch_map   <keyType, valueType, ehash_func>>("hopsco", vList); }
#if __x86_64__ || _M_X64
        {  benOneHash<ska::bytell_hash_map <keyType, valueType, ehash_func>>("byte", vList); }
#endif
#endif

        {  benOneHash<std::unordered_map<keyType, valueType, ehash_func>>   ("stl_hash", vList); }
#if ET > 1
#if ET > 2
        {  benOneHash<emlru_time::lru_cache<keyType, valueType, ehash_func>>("lru_time", vList); }
        {  benOneHash<emlru_size::lru_cache<keyType, valueType, ehash_func>>("lru_size", vList); }
#endif

        //{  benOneHash<emilib3::HashMap <keyType, valueType, ehash_func>>("emilib3", vList); }
        //{  benOneHash<emilib4::HashMap      <keyType, valueType, ehash_func>>("emilib4", vList); }

#if __x86_64__ || _M_X64
        {  benOneHash<ska::flat_hash_map <keyType, valueType, ehash_func>>("flat", vList); }
#if __cplusplus > 201402L || _MSVC_LANG >= 201402L
        {  benOneHash<hrd7::hash_map     <keyType, valueType, ehash_func>>("hrdset", vList); }
#endif
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

        if (vList.size() & 1) {
            {  benOneHash<emilib::HashMap       <keyType, valueType, ehash_func>>("emilib", vList); }
//            {  benOneHash<emilib1::HashMap      <keyType, valueType, ehash_func>>("emilib1", vList); }
            {  benOneHash<emilib2::HashMap      <keyType, valueType, ehash_func>>("emilib2", vList); }
        } else {
            {  benOneHash<emilib2::HashMap      <keyType, valueType, ehash_func>>("emilib2", vList); }
//            {  benOneHash<emilib1::HashMap      <keyType, valueType, ehash_func>>("emilib1", vList); }
            {  benOneHash<emilib::HashMap       <keyType, valueType, ehash_func>>("emilib", vList); }
        }
#if ET
        {  benOneHash<phmap::flat_hash_map <keyType, valueType, ehash_func>>("phmap", vList); }
        {  benOneHash<robin_hood::unordered_flat_map <keyType, valueType, ehash_func>>("martin", vList); }
//        {  benOneHash<simd_hash_map<keyType, valueType, ehash_func>>("simd_hash", vList); }

        {  benOneHash<tsl::robin_map        <keyType, valueType, ehash_func>>("robin", vList); }
        //{  benOneHash<zedland::hashmap <keyType, valueType, ehash_func>>("zhashmap", vList); }

#if FHT_HMAP
        {  benOneHash<fht_table <keyType, valueType, ehash_func>>("fht", vList); }
#endif
#endif
    }

    assert(n == vList.size());
    auto pow2 = 2u << ilog(n, 2);

    constexpr uint64_t kv = sizeof(std::pair<keyType, valueType>);// sizeof(keyType) + sizeof(valueType) + (eq ? 0 : 4);
    auto memory1 = 8 * pow2 + kv * n; //emhash8 table
    auto memory2 = (1 + kv) * pow2;   //flat hash map
    auto memoryr = (8 * 4 + 8 + kv + 8) * n; //std::map
    auto memoryu = 8 * pow2 + (8 + 8 + 8 + kv) * n; //std::unordered_map

    printf("\n %d ======== n = %d, load_factor = %.3lf(emh8/flat = %.2lf/%.2lf, smap/umap = %.2lf/%.2lf MB), data_type = %d ========\n",
            test_case + 1, n, 1.0 * n / pow2, 1.0 * memory1 / (1 << 20) , 1.0 * memory2 / (1 << 20),
            1.0 * memoryr / (1 << 20), 1.0 * memoryu / (1 << 20), flag);

    printResult(n);
    return test_case;
}

#if WYHASH_LITTLE_ENDIAN && STR_VIEW
struct string_hash
{
    using is_transparent = void;
    std::size_t operator()(const char* key)             const { auto ksize = std::strlen(key); return wyhash(key, ksize, ksize); }
    std::size_t operator()(const std::string& key)      const { return wyhash(key.data(), key.size(), key.size()); }
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
        return std::strcmp(lhs, rhs.data()) == 0;
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
    auto r  =  ts;

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
        sum += hashfib(i + r);
    printf("hashfib     = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);

    ts = getus(); sum = r;
    for (int i = 0; i < loops; i++)
        sum += hash_mur3(i + r);
    printf("hash_mur3  = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);

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
    testHashInt(1e7+8);

#ifdef AHASH_AHASH_H
    printf("ahash_version = %s", ahash_version());
#endif

    //srand((unsigned)time(0));
    printInfo(nullptr);

    int run_type = 0;
    int tn = 0, rnd = randomseed();
    auto maxc = 500;
    auto minn = (1000 * 100 * 2) + 10000;
    auto maxn = 100*minn;
    if (TKey < 3)
        minn *= 2;

    const int type_size = (sizeof(keyType) + sizeof(valueType) + 4);
    if (maxn > 1024*1024*1024 / type_size)
        maxn = 1024*1024*1024 / type_size;

    float load_factor = 0.0945f;
    printf("./ebench maxn = %d c(0-1000) f(0-100) d[2-9 mpatseblku] a(0-3) b t(n %dkB - %dMB)\n",
            (int)maxn, minn*type_size >> 10, maxn*type_size >> 20);

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
        else if (cmd == 'n')
            minn = value;
        else if (cmd == 'b') {
            testHashRand(1e8+8);
            testHashString(1e6+6, 2, 32);
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
                    maps.erase("emilib");
//                    maps.erase("emilib1");
//                    maps.erase("emilib2");
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

    int n = (srng() % (2*minn)) + minn;
    while (true) {
        if (run_type == 2) {
            printf(">>");
            if (scanf("%u", &n) <= 0)
                break;
            if (n <= 1)
                run_type = 0;
            else if (n < -minn) {
                run_type = 1;
                n = -n;
            }
        } else if (run_type == 1) {
            n = (srng() % maxn) + minn;
        } else {
            n = n * 9 / 8;
            if (n > maxn)
                n = (srng() % maxn) + minn;
        }

        if (load_factor > 0.2 && load_factor < 1) {
            auto pow2 = 1 << ilog(n, 2);
            n = int(pow2 * load_factor) - (1 << 10) + (srng()) % (1 << 8);
        }
        if (n < 1000 || n > 1234567890)
            n = 1234567 + srng() % 1234567;

        int tc = benchHashMap(n);
        if (tc >= maxc)
            break;
    }

    printf("total time = %.3lf s", (getus() - start) / 1000000.0);
    return 0;
}

