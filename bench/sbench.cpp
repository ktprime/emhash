#include "util.h"
#include <algorithm>

#ifndef TKey
    #define TKey           1
#endif

#if __GNUC__ > 4 && __linux__
#include <ext/pb_ds/assoc_container.hpp>
#endif

#if CXX17
#include "martin/unordered_dense.h"
#endif

#ifdef ET
//#define ET                 1
#endif
static void printInfo(char* out);
std::map<std::string, std::string> maps =
{
//    {"stl_hset", "unordered_set"},
//    {"stl_set", "stl_set"},
    {"btree", "btree_set"},
    {"qchash", "qc-hash"},

//    {"emhash7", "emhash7"},
//    {"emhash2", "emhash2"},
    {"emhash9", "emhash9"},
#if HAVE_BOOST
    {"boostf",  "boost_flat"},
#endif
    {"emhash8", "emhash8"},
    {"martind", "martin_dense"},
    {"ck_hash", "sk_hset"},

    {"gp_hash", "gp_hash"},

//    {"emiset", "emiset"},
    {"emiset2", "emiset2"},
    {"emiset2s","emiset2s"},
    {"absl", "absl_flat"},

#if ET
    {"martin", "martin_flat"},
    {"phmap", "phmap_flat"},
    {"hrdset",   "hrdset"},

    {"tslr", "tsl_robin"},
    {"skaf", "ska_flat"},

    {"hopsco", "tsl_hopsco"},
    {"byte", "ska_byte"},
#endif
};


//rand data type
#ifndef RT
    #define RT 3  //1 wyrand 2 sfc64 3 RomuDuoJr 4 Lehmer64 5 mt19937_64
#endif

//#define PHMAP_HASH          1
#if WYHASH_LITTLE_ENDIAN
//#define WY_HASH             1
#endif
//#define FL1                 1
//#define EMH_INT_HASH  1
//#define EMH_REHASH_LOG      1234567

//#define EMH_STATIS          1
//#define EMH_SAFE_HASH       1
//#define EMH_IDENTITY_HASH   1

//#define EMH_LRU_SET         1
//#define EMH_STD_STRING      1
//#define EMH_ERASE_SMALL     1
//#define EMH_BDKR_HASH       1
//#define EMH_HIGH_LOAD       201000
//#define EMH_SIZE_TYPE_64BIT  1

//
#include "hash_set2.hpp"
#include "hash_set3.hpp"
#include "hash_set4.hpp"
#include "hash_set8.hpp"


#ifdef HAVE_BOOST
#include <boost/unordered/unordered_flat_set.hpp>
#endif
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
//https://gankra.github.io/blah/hashbrown-tldr/ swiss
//https://leventov.medium.com/hash-table-tradeoffs-cpu-memory-and-variability-22dc944e6b9a


#if HOOD_HASH
    #include "martin/robin_hood.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#endif
#if PHMAP_HASH
    #include "phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap
#endif

#include "emilib/emiset.hpp"
#include "emilib/emiset2.hpp"
#include "emilib/emiset2s.hpp"

#if ET
#if X86_64
    #include "hrd/hash_set7.h"        //https://github.com/hordi/hash/blob/master/include/hash_set7.h
    #include "ska/flat_hash_map.hpp"  //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
    #include "ska/bytell_hash_map.hpp"//https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#endif
    #include "tsl/robin_set.h"        //https://github.com/tessil/robin-map
    #include "phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap
    #include "martin/robin_hood.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#if ET > 1
    #include "tsl/hopscotch_set.h"    //https://github.com/tessil/hopscotch-map
#endif
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

    int64_t operator + (int64_t r) const { return lScore + r; }

    int64_t lUid;
    int64_t lScore;
    int  iUpdateTime;
    int  iRank;

#if PACK >= 24
    char data[(PACK - 24) / 8 * 8];
#endif

#if VCOMP
    std::string sdata = {"test data input"};
    std::vector<int> vint = {1, 2,3, 4, 5, 6, 7, 8};
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
    #define TO_VAL(i)   ""
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

static int test_case = 0, test_extra = 0;
static int loop_vector_time = 0, loop_rand = 0;
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
    } else if ((int64_t)sum != func_result[func]) {
        printf("%s %s %zd != %zd (o)\n", hash_name.data(), func.data(), (size_t)sum, (size_t)func_result[func]);
    }

    auto& showname = maps[hash_name];
    once_func_hash_time[func][showname] += (getus() - ts1) / weigh;
    func_index ++;

    long ts = (getus() - ts1) / 1000;

    if (func_first < func_last)  {
        if (func_index == func_first)
            printf("%8s  (%.3f): ", hash_name.data(), hlf);
        if (func_index >= func_first && func_index <= func_last)
            printf("%8s %4ld, ", func.data(), ts);
        if (func_index == func_last)
            printf("\n");
    } else {
        if (func_index == 1)
            printf("%8s  (%.3f): ", hash_name.data(), hlf);
        if (func_index >= func_first || func_index <= func_last)
            printf("%8s %4ld, ", func.data(), ts);
        if (func_index == func_size)
            printf("\n");
    }
}

static void hash_convert(const std::map<std::string, int64_t>& hash_score, std::multimap <int64_t, std::string>& score_hash)
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

//    const auto last  = double(once_score_hash.rbegin()->first);
    const auto first = double(once_score_hash.begin()->first);
    //print once score
    for (auto& v : once_score_hash) {
        printf("%5d   %13s   (%6.1lf %%)\n", int(v.first / (func_index - 1)), v.second.data(), 100 * v.first / first);
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

    //puts(pys.data());
    std::ofstream outfile;
    auto full_file = file + ".py";
    outfile.open("./" + full_file, std::fstream::out | std::fstream::trunc | std::fstream::binary);
    if (outfile.is_open())
        outfile << pys;
    else
        printf("\n\n =============== can not open %s ==============\n\n", full_file.data());

    outfile.close();
}

template<class hash_type>
static void hash_iter(const hash_type& ht_hash, const std::string& hash_name)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : ht_hash)
        sum += sum;

    for (auto& v : ht_hash)
        sum += 2;

    for (auto it = ht_hash.begin(); it != ht_hash.end(); it++)
#if KEY_INT
        sum += *it;
#elif KEY_CLA
    sum += *it.lScore;
#else
    sum += (*it).size();
#endif

    for (auto it = ht_hash.cbegin(); it != ht_hash.cend(); ++it)
#if KEY_INT
        sum += *it;
#elif KEY_CLA
    sum += *it.lScore;
#else
    sum += (*it).size();
#endif

#ifndef SMAP
    hlf = ht_hash.load_factor();
#endif
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void erase_reinsert(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        ht_hash.insert(v);
        sum ++;
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void insert_erase(const std::string& hash_name, const std::vector<keyType>& vList)
{
    hash_type ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    const auto vsmall = 1024 + vList.size() % 1024;
    for (size_t i = 0; i < vList.size(); i++) {
        sum += ht_hash.insert(vList[i]).second;
        if (i > vsmall)
            ht_hash.erase(vList[i - vsmall]);
    }

    if (vList.size() % 3 == 0)
        ht_hash.clear();
    const auto vmedium = vList.size() / 100;
    for (size_t i = 0; i < vList.size(); i++) {
        auto it = ht_hash.insert(vList[i]);
        if (i > vmedium)
            ht_hash.erase(it.first);
    }

    if (vList.size() % 2 == 0)
        ht_hash.clear();
    const auto vsize = vList.size() / 8;
    for (size_t i = 0; i < vList.size(); i++) {
        sum += ht_hash.insert(vList[i]).second;
        if (i > vsize)
            sum += ht_hash.erase(vList[i - vsize]);
    }

    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void insert_no_reserve(const std::string& hash_name, const std::vector<keyType>& vList)
{
    hash_type ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ht_hash.insert(v).second;

    hlf = ht_hash.load_factor();
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void insert_reserve(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
#ifndef SMAP
    ht_hash.max_load_factor(0.80f);
    ht_hash.reserve(vList.size());
#endif

    for (const auto& v : vList)
        sum += ht_hash.insert(v).second;
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void insert_hit(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (auto& v : vList) {
        ht_hash.insert(v);
        sum ++;
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void multi_small_ife(const std::string& hash_name, const std::vector<keyType>& vList)
{
#if KEY_INT
    size_t sum = 0;
    const auto hash_size = vList.size() / 10003 + 200;
    const auto ts1 = getus();

    auto mh = new hash_type[hash_size];
    for (const auto& v : vList) {
        auto hash_id = ((uint64_t)v) % hash_size;
        sum += mh[hash_id].insert(v).second;
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
static void insert_find_erase(const hash_type& ht_hash, const std::string& hash_name, std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 1;
    hash_type tmp(ht_hash);

    for (auto & v : vList) {
#if KEY_INT
        auto v2 = keyType(v % 2 == 0 ? v + sum : v - sum);
#elif KEY_CLA
        int64_t v2(v.lScore + sum);
#elif TKey != 4
        v += char(128 + (int)v[0]);
        const auto &v2 = v;
#else
        const keyType v2(v.data(), v.size() - 1);
#endif
        auto it = tmp.insert(v2);
#if QC_HASH == 0
        sum += tmp.count(v2);
#endif
        tmp.erase(it.first);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1, 3);
}

template<class hash_type>
static void insert_cache_size(const std::string& hash_name, const std::vector<keyType>& vList, const char* level, const uint32_t cache_size, const uint32_t min_size)
{
    auto ts1 = getus(); size_t sum = 0;
    const auto lsize = cache_size + vList.size() % min_size;
    hash_type tmp, empty;
#ifndef SMAP
//    if (vList.size() % 4 == 0)
//        empty.max_load_factor(0.80);
//    if (vList.size() % 8 == 0)
//        empty.reserve(cache_size);
#endif

    for (const auto& v : vList)
    {
        sum += tmp.insert(v).second;
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
static void insert_high_load(const std::string& hash_name, const std::vector<keyType>& vList)
{
    size_t sum = 0;
    size_t pow2 = 2u << ilog(vList.size(), 2);
    hash_type tmp;

    const auto max_loadf = 0.99f;
#ifndef SMAP
    tmp.max_load_factor(max_loadf);
    tmp.reserve(pow2 / 2);
#endif
    int minn = int((max_loadf - 0.2f) * pow2), maxn = int(max_loadf * pow2);
    int i = 0;

    //fill data to min factor
    for (; i < minn; i++) {
        if (i < (int)vList.size())
            tmp.insert(vList[i]);
        else {
            auto& v = vList[i - vList.size()];
#if KEY_INT
            auto v2 = v - i;
#elif KEY_CLA
            auto v2 = v.lScore + (v.lScore / 11) + i;
#elif TKey != 4
            auto v2 = v; v2[0] += '2';
#else
            keyType v2(v.data(), v.size() - 1);
#endif
            tmp.insert(v2);
        }
    }

    auto ts1 = getus();
    for (; i < maxn; i++) {
        auto& v = vList[i - minn];
#if KEY_INT
        auto v2 = v + i;
#elif KEY_CLA
        auto v2 = (v.lScore / 7) + 4 * v.lScore;
#elif TKey != 4
        auto v2 = v; v2[0] += '1';
#else
        keyType v2(v.data(), v.size() - 1);
#endif
        tmp.insert(v2);
        sum += tmp.count(v2);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

#if FL1
static uint8_t l1_cache[64 * 1024];
#endif
template<class hash_type>
static void find_hit_0(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto n = ht_hash.size();

#if KEY_STR
#if TKey != 4
    auto skey = get_random_alphanum_string(STR_SIZE);
#endif
#endif
    size_t sum = 0;
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
        keyType v2(v + 1);
        sum += ht_hash.find(v2) != ht_hash.end();
#endif
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void find_hit_50(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
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
static void find_hit_50_erase(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto tmp = ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        auto it = tmp.find(v);
        if (it == tmp.end())
            sum ++;
        else
            tmp.erase(it);
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void find_hit_100(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
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
static void find_erase_50(const hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList) {
        sum += ht_hash.count(v);
        sum += ht_hash.find(v) != ht_hash.end();
    }
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void erase_50(hash_type& ht_hash, const std::string& hash_name, const std::vector<keyType>& vList)
{
    auto tmp = ht_hash;
    auto ts1 = getus(); size_t sum = 0;
    for (const auto& v : vList)
        sum += ht_hash.erase(v);

    for (auto it = tmp.begin(); it != tmp.end(); ) {
#if CXX17
        if constexpr( std::is_void_v< decltype( tmp.erase( it ) ) > )
            tmp.erase( it++ );
        else
#endif
            it = tmp.erase(it);
       sum += 1;
    }
    sum += tmp.size();
    check_func_result(hash_name, __FUNCTION__, sum, ts1);
}

template<class hash_type>
static void hash_clear(hash_type& ht_hash, const std::string& hash_name)
{
    if (ht_hash.size() > 1000000) {
        auto ts1 = getus();
        size_t sum = ht_hash.size();
        ht_hash.clear(); ht_hash.clear();
        check_func_result(hash_name, __FUNCTION__, sum, ts1);
    }
}

template<class hash_type>
static void copy_clear(hash_type& ht_hash, const std::string& hash_name)
{
    size_t sum = 0;
    auto ts1 = getus();
    hash_type thash = ht_hash;
    sum += thash.size();

    for (int i = 0; i < 10; i++) {
        ht_hash = thash;
        sum  += ht_hash.size();

        ht_hash = std::move(ht_hash);
        ht_hash = std::move(thash);
        sum  += ht_hash.size();
        assert(ht_hash.size() > 0 && thash.size() == 0);

        std::swap(ht_hash, thash);
        assert(ht_hash.size() == 0 && thash.size() > 0);
    }

    ht_hash.clear(); thash.clear();
    ht_hash.clear(); thash.clear();
    sum  += ht_hash.size();

    assert(ht_hash.size() == thash.size());
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
#elif RT == 4
    std::mt19937_64 srng(size);
#else
    Lehmer64 srng(size);
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

#if RA > 0
    const auto iRation = RA;
#else
    const auto iRation = 5;
#endif

    auto flag = srng();
    auto dataset = srng() % 100;
    if (srng() % 100 >= iRation)
    {
        const auto case_pointer = 5;
        const auto case_bitmix  = 3;
        for (int i = 0; i < size ; i++) {
            auto key = TO_KEY(srng());
            if (dataset < case_pointer)
                key *= 8; //pointer
            else if (dataset < case_pointer + case_bitmix * 1)
                key = flag++;
#if KEY_INT == 1 && STD_HASH == 0
            else if (dataset < case_pointer + case_bitmix * 2)
                key &= 0xFFFFFFFF00000000; //high 32 bit
            else if (dataset < case_pointer + case_bitmix * 3)
                key = ((uint32_t)key); //low 32 bit
            else if (dataset < case_pointer + case_bitmix * 4)
                key &= 0xFFFFFFFF0000; //medium 32 bit
#endif
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

template<class hash_type>
static void benOneHash(const std::string& hash_name, const std::vector<keyType>& oList)
{
    if (maps.find(hash_name) == maps.end())
        return;

    if (test_case == 0)
        printf("%s:size %zd\n", hash_name.data(), sizeof(hash_type));

    {
        hash_type hash;
        const uint32_t l1_size = (32 * 1024)   / (sizeof(keyType) + sizeof(valueType));
//        const uint32_t l2_size = (256 * 1024)   / (sizeof(keyType) + sizeof(valueType));
        const uint32_t l3_size = (8 * 1024 * 1024) / (sizeof(keyType) + sizeof(valueType));

        func_index = 0;
        multi_small_ife <hash_type>(hash_name, oList);
#if QC_HASH == 0 || QC_HASH == 2
        insert_erase     <hash_type>(hash_name, oList);
#endif
        insert_high_load <hash_type>(hash_name, oList);

        insert_cache_size <hash_type>(hash_name, oList, "insert_l1_cache", l1_size, l1_size + 1000);
//        insert_cache_size <hash_type>(hash_name, oList, "insert_l2_cache", l2_size, l2_size + 1000);
        insert_cache_size <hash_type>(hash_name, oList, "insert_l3_cache", l3_size, l3_size + 1000);

        insert_no_reserve <hash_type>(hash_name, oList);

        insert_hit<hash_type>(hash,hash_name, oList);
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

        shuffle(nList.begin(), nList.end());
        find_hit_50<hash_type>(hash, hash_name, nList);
        find_hit_50_erase<hash_type>(hash, hash_name, nList);
        erase_50<hash_type>(hash, hash_name, nList);
        find_erase_50<hash_type>(hash, hash_name, oList);
        insert_find_erase<hash_type>(hash, hash_name, nList);

        erase_reinsert<hash_type>(hash, hash_name, oList);
        hash_iter<hash_type>(hash, hash_name);

#ifndef UF
        //copy_clear <hash_type>(hash, hash_name);
        //hash_clear<hash_type>(hash, hash_name);
#endif

        func_size = func_index;
    }
}

constexpr static auto base1 = 300000000;
constexpr static auto base2 =      20000;
static void reset_top3(std::map<std::string, int64_t>& top3, const std::multimap <int64_t, std::string>& once_score_hash)
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

static int benchHashSet(int n)
{
    if (n < 10000)
        n = 123456;

    func_result.clear(); once_func_hash_time.clear();

    std::vector<keyType> vList;
    auto flag = buildTestData(n, vList);

    //more than 10 hash
#if KEY_CLA
    using ehash_func = StuHasher;
#elif ABSL_HASH
    using ehash_func = absl::Hash<keyType>;
#elif WY_HASH && KEY_STR
    using ehash_func = WysHasher;
#elif A_HASH && KEY_STR
    using ehash_func = Ahash64er;
#elif KEY_INT && FIB_HASH > 0
    using ehash_func = Int64Hasher<keyType>; //9 difference hashers
#elif PHMAP_HASH
    using ehash_func = phmap::Hash<keyType>;
#elif QCH
    using ehash_func = qc::hash::RawMap<keyType, valueType>::hasher;
#elif STD_HASH
    using ehash_func = std::hash<keyType>;
#elif HOOD_HASH
    using ehash_func = robin_hood::hash<keyType>;
#else
    using ehash_func = ankerl::unordered_dense::hash<keyType>;
#endif

    {
        int64_t ts = getus(), sum = 0ul;
        for (auto& v : vList)
#if KEY_INT
            sum += (int)v;
#elif KEY_CLA
        sum += v.lScore;
#else
        sum += v.size();
#endif
        loop_vector_time = getus() - ts;
        printf("n = %d, keyType = %s, loop_sum = %d ns:%d\n", n, sKeyType, (int)(loop_vector_time * 1000 / vList.size()), (int)sum);
    }

    {
        func_first = func_first % func_size + 1;
        func_last  = (func_first + 3) % func_size + 1;

#if ET > 2
        {  benOneHash<tsl::hopscotch_set   <keyType,  ehash_func>>("hopsco", vList); }
#if X86_64
        {  benOneHash<ska::bytell_hash_set <keyType,  ehash_func>>("byte", vList); }
#endif
#endif

#if ET > 1
        {  benOneHash<std::unordered_set<keyType,  ehash_func>>("stl_hset", vList); }
        //{  benOneHash<emilib2::HashSet     <keyType,  ehash_func>>("emilib2", vList); }

        {  benOneHash<tsl::robin_set     <keyType,  ehash_func>>("tslr", vList); }
#if X86_64
        {  benOneHash<ska::flat_hash_set <keyType,  ehash_func>>("skaf", vList); }
        //{  benOneHash<hrd7::hash_set     <keyType,  ehash_func>>("hrdset", vList); }
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

#if CXX17
        {  benOneHash<ankerl::unordered_dense::set <keyType, ehash_func>>("martind", vList); }
#endif

#if ET
        {  benOneHash<phmap::flat_hash_set <keyType,  ehash_func>>("phmap", vList); }
        {  benOneHash<robin_hood::unordered_flat_set <keyType,  ehash_func>>("martin", vList); }
#endif
#if HAVE_BOOST
        {  benOneHash<boost::unordered_flat_set<keyType, ehash_func>>("boostf", vList); }
#endif
#if ABSL_HMAP
        {  benOneHash<absl::flat_hash_set <keyType, ehash_func>>("absl", vList); }
#endif

        {  benOneHash<emhash8::HashSet <keyType,  ehash_func>>("emhash8", vList); }
        {  benOneHash<emilib::HashSet  <keyType,  ehash_func>>("emiset", vList); }
        {  benOneHash<emilib2::HashSet <keyType,  ehash_func>>("emiset2", vList); }
        {  benOneHash<emilib3::HashSet <keyType,  ehash_func>>("emiset2s", vList); }
        {  benOneHash<emhash7::HashSet <keyType,  ehash_func>>("emhash7", vList); }
        {  benOneHash<emhash2::HashSet <keyType,  ehash_func>>("emhash2", vList); }
        {  benOneHash<emhash9::HashSet <keyType,  ehash_func>>("emhash9", vList); }

//        {  benOneHash<CK::HashSet <keyType,  ehash_func>>("ck_hash", vList); }

#if QC_HASH && KEY_INT
        {  benOneHash<qc::hash::RawSet<keyType,  ehash_func>>("qchash", vList); }
#endif
    }

    auto pow2 = 1 << ilog(vList.size(), 2);
    auto iload = 50 * vList.size() / pow2;
    printf("\n %d ======== n = %d, load_factor = %.3lf, data_type = %d ========\n", test_case + 1, n, iload / 100.0, flag);
    printResult(n);
    return test_case;
}

static void high_load()
{
    size_t maxSize = 1U << 28;
    size_t numReps = 100;

    std::random_device rd;
    auto dis = std::uniform_int_distribution<uint32_t>{0, (1U << 31) - 1};

    for (size_t rep = 0; rep < numReps; ++rep) {
        {
            auto rng = std::mt19937(rep);
            emhash9::HashSet<uint32_t> set;
            set.max_load_factor(0.999);

            auto t1 = getus();
            while (set.size() < maxSize) {
                auto key = dis(rng);
                size_t prevCap = set.bucket_count();
                set.insert(key);
                if (set.bucket_count() > prevCap) {
                    size_t prevSize = set.size() - 1;
                    auto lf = static_cast<double>(prevSize) / prevCap;
                    std::cout << prevCap << " " << prevSize << " " << lf << "\n";
                }
            }
            std::cout << "emhash loop " << rep << " time use " << (getus() - t1) / 1000000.000 << " sec\n";
        }

#if X86_64 && ET
        {
            auto rng = std::mt19937(rep);
            ska::bytell_hash_set<uint32_t> set;
            set.max_load_factor(0.999);

            auto t1 = getus();
            while (set.size() < maxSize) {
                auto key = dis(rng);
                size_t prevCap = set.bucket_count();
                set.insert(key);
                if (set.bucket_count() > prevCap) {
                    size_t prevSize = set.size();
                    auto lf = static_cast<double>(prevSize) / prevCap;
                    std::cout << prevCap << " " << prevSize << " " << lf << "\n";
                }
            }
            std::cout << "skaset loop " << rep << " time use " << (getus() - t1) / 1000000.000 << "sec\n\n";
        }
#endif
    }
}

int main(int argc, char* argv[])
{
    auto start = getus();
    srand((unsigned)time(0));
    printInfo(nullptr);

    int run_type = 0;
    auto rnd = randomseed();
    auto maxc = 500;
    int minn = (1000 * 100 * 8) / sizeof(keyType) + 12345;
    int maxn = 100*minn;
    if (TKey < 3)
        minn *= 2;

    const int type_size = (sizeof(keyType) + 4);
    if (maxn > (1 << 30) / type_size)
        maxn = (1 << 30) / type_size;

    float load_factor = 0.0945f;
    printf("./sbench maxn = %d c(0-1000) f(0-100) d[2-9 mpatsebu] a(0-3) b t(n %dkB - %dMB)\n",
            (int)maxn, minn*type_size >> 10, maxn*type_size >> 20);

    for (int i = 1; i < argc; i++) {
        const auto cmd = argv[i][0];
        const auto value = atoi(&argv[i][0] + 1);

        if (isdigit(cmd))
            maxn = atoi(argv[i]) + 1000;
        else if (cmd == 'f' && value > 0)
            load_factor = float(atof(&argv[i][0] + 1) / 100.0);
        else if (cmd == 'c' && value > 0)
            maxc = value;
        else if (cmd == 'a')
            run_type = value;
        else if (cmd == 'r' && value > 0)
            rnd = value;
        else if (cmd == 'n' && value > 0)
            minn = value;
        else if (cmd == 'b')
            high_load();

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

                else if (c == 'h')
                    maps.erase("hrdset");
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
                else if (c == 'e') {
                    maps.erase("emiset");
                }
                else if (c == 'b') {
                    maps.emplace("btree", "btree_set");
                    maps.emplace("stl_set", "stl_set");
                } else if (c == 'u')
                    maps.emplace("stl_hset", "unordered_set");
            }
        }
    }

    Sfc4 srng(rnd);
    for (auto& m : maps)
        printf("  %s\n", m.second.data());
    putchar('\n');

    int n = (srng() % (2*minn)) + minn;
    while (true) {
        if (run_type == 2) {
            printf(">>");
#if _WIN32
            if (scanf_s("%d", &n) == 0)
#else
            if (scanf("%d", &n) == 0)
#endif
                break;
            if (n <= 1)
                run_type = 0;
            else if (n < -minn) {
                run_type = 1;
                n = -n;
            }
        } else if (run_type == 1) {
            n = (srng() % (maxn - minn)) + minn;
        } else {
            n += n / 20;
            if (n > maxn)
                n = (srng() % (maxn - minn)) + minn;
        }

        auto pow2 = 2 << ilog(n, 2);
        hlf = 1.0f * n / pow2;
        if (load_factor > 0.2 && load_factor < 1) {
            n = int(pow2 * load_factor) - (1 << 10) + (srng()) % (1 << 8);
            hlf = 1.0f * n / pow2;
        }
        if (n < 1e5 || n > 2e9)
            n = minn + srng() % minn;

        int tc = benchHashSet(n);
        if (tc >= maxc)
            break;
    }

    printf("total time = %.3lf s", (getus() - start) / 1000000.0);
    return 0;
}

