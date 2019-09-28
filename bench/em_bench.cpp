
#include <random>
#include <map>
#include <ctime>
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <array>

//#include "wyhash.h"

#ifndef TP
#define TP                   100
#endif
//#define HOOD_HASH            1
//#define EMHASH_BUCKET_INDEX  1

//#define EMHASH_REHASH_LOG  1
//#define EMHASH_AVX_MEMCPY  1
//#define EMHASH_STATIS      1

//#define EMHASH_FIBONACCI_HASAH 1
//#define EMHASH_SAFE_HASH     1
//#define EMHASH_IDENTITY_HASH 1

//#define EMHASH_LRU_SET       1
//#define EMHASH_ERASE_SMALL     1
//#define EMHASH_STD_STRING    1
//#define EMHASH_HIGH_LOAD 201000


#ifndef TKey
    #define TKey  1
#endif
#ifndef TVal
    #define TVal  1
#endif

#include "hash_table2.hpp"
#include "hash_table3.hpp"
#include "hash_table4.hpp"
#include "hash_table5.hpp"
#include "hash_table6.hpp"
//#include "hash_table64.hpp"

////some others
//https://github.com/ilyapopov/car-race
//https://hpjansson.org/blag/2018/07/24/a-hash-table-re-hash/
//https://attractivechaos.wordpress.com/2018/01/13/revisiting-hash-table-performance/
//https://www.reddit.com/r/cpp/comments/auwbmg/hashmap_benchmarks_what_should_i_add/
//https://www.youtube.com/watch?v=M2fKMP47slQ
//https://yq.aliyun.com/articles/563053

//https://engineering.fb.com/developer-tools/f14/
//https://github.com/facebook/folly/blob/master/folly/container/F14.md

//https://martin.ankerl.com/2019/04/01/hashmap-benchmarks-01-overview/

//https://attractivechaos.wordpress.com/2008/08/28/comparison-of-hash-table-libraries/
//https://en.wikipedia.org/wiki/Hash_table
//https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html
//https://probablydance.com/2017/02/26/i-wrote-the-fastest-hashtable/
//https://martin.ankerl.com/2016/09/21/very-fast-hashmap-in-c-part-3/
//https://andre.arko.net/2017/08/24/robin-hood-hashing/
//http://www.ilikebigbits.com/2016_08_28_hash_table.html
//http://www.idryman.org/blog/2017/05/03/writing-a-damn-fast-hash-table-with-tiny-memory-footprints/

#if __cplusplus >= 199711L
//    #include "./martin/robin_hood.h"       //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#endif

#if __cplusplus >= 201103L || _MSC_VER >= 1601
    #define _CPP11_HASH    1
    #include "./tsl/robin_map.h"        //https://github.com/tessil/robin-map
    #include "./tsl/hopscotch_map.h"    //https://github.com/tessil/hopscotch-map
    #include "./martin/robin_hood.h"       //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
    #include "./ska/flat_hash_map.hpp"  //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
    #include "./phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap

#if __cplusplus > 201402L || _MSC_VER >= 1600
    #define _CPP14_HASH   1
    #include "./ska/bytell_hash_map.hpp"//https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#endif
#endif

#include <unordered_map>

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
    #define KEY_INT     1
#elif TKey == 1
    typedef int64_t       keyType;
    #define TO_KEY(i)   (keyType)i
    #define sKeyType    "int64_t"
    #define KEY_INT     1
#else
    typedef std::string keyType;
    #define TO_KEY(i)   std::to_string(i)
    #define sKeyType    "string"
#endif

#if TVal == 0
    typedef int         valueType;
    #define TO_VAL(i)   i
    #define TO_SUM(i)   i
    #define sValueType  "int"
#elif TVal == 1
    typedef int64_t       valueType;
    #define TO_VAL(i)   i
    #define TO_SUM(i)   i
    #define sValueType  "int64_t"
#elif TVal == 2
    typedef std::string valueType;
    #define TO_VAL(i)   std::to_string(i)
    #define TO_SUM(i)   i.size()
    #define sValueType  "string"
#else
    typedef RankItem    valueType;
    #define TO_VAL(i)   i
    #define TO_SUM(i)   i.lScore
    #define sValueType  "RankItem"
#endif

emhash6::HashMap<std::string, std::string> show_name = {
//    {"stl_hash", "unordered_map"},

    {"emhash2", "emhash2"},
    {"emhash6", "emhash6"},
    {"emhash4", "emhash4"},
//    {"emhash3", "emhash3"},
    {"emhash5", "emhash5"},
//    {"emhash7", "emhash7"},

    {"martin", "martin flat"},
    {"phmap", "phmap flat"},
//#if ET
#if HOOD_HASH || KEY_INT == 0
    {"robin", "tessil robin"},
//    {"hopsco", "tessil hopsco"},
    {"flat", "skarupk flat"},
#endif
//#endif

//    {"byte", "skarupk byte"},
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

class RandomBool {
public:
	template <typename Rng>
	bool operator()(Rng& rng) {
		if (1 == m_rand) {
			m_rand = std::uniform_int_distribution<size_t>{}(rng) | s_mask_left1;
		}
		bool const ret = m_rand & 1;
		m_rand >>= 1;
		return ret;
	}

private:
	static constexpr const size_t s_mask_left1 = size_t(1) << (sizeof(size_t) * 8 - 1);
	size_t m_rand = 1;
};

static std::map<std::string, size_t>  check_result;
static std::multimap <int64_t, std::string> func_time;
static std::map<std::string, int64_t> map_time;
static std::map<std::string, std::map<std::string, int64_t>> func_map_time;

#define AVE_TIME(ts, n)             int(1000 * (getTime() - ts) / (n))

static void check_mapfunc_result(const std::string& map_name, const std::string& func, size_t sum, int64_t ts1)
{
    if (check_result.find(func) == check_result.end()) {
        check_result[func] = sum;
    }
    else if (sum != check_result[func]) {
        printf("%s %s %zd != %zd\n", map_name.c_str(), func.c_str(), sum, check_result[func]);
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
        printf("   %-8d     %-21s   %02d\n", (int)(v.first / 10000), v.second.c_str(), (int)((min * 100) / (v.first + 1)));
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
    if (show_name.find(map_name) != show_name.end())\
    {
        auto ts1 = getTime(); size_t sum = 0;
        for (const auto& v : amap)
            sum += TO_SUM(v.second);

#if KEY_INT
        for (auto it = amap.cbegin(); it != amap.cend(); ++it)
            sum += it->first;
#endif

        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void erase_reinsert(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        size_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList) {
            amap[v] = TO_VAL(1);
#if TVal < 2
            sum += amap[v];
#else
            sum += TO_SUM(amap[v]);
#endif

        }
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
        printf("    %82s    reinsert  %5d ns, factor = %.2f\n", map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class MAP>
void hash_insert2(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        size_t sum = 0;
        auto ts1 = getTime();
        for (const auto v : vList) {
            amap.insert_unique(v, TO_VAL(0));
            sum ++;
        }
        check_mapfunc_result(map_name, "hash_insert", sum, ts1);
//        printf("    %12s    %s  %5lu ns, factor = %.2f\n", "hash_insert2", map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
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
            sum += amap.emplace(v, TO_VAL(0)).second;
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
//        printf("    %12s  %s  %5zd ns, factor = %.2f\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), amap.load_factor());
    }
}

template<class MAP>
void insert_reserve(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        size_t sum = 0;
        MAP nmap(1);
        nmap.max_load_factor(80.0f/100);
        nmap.reserve(vList.size());
        auto ts1 = getTime();
        for (const auto v : vList)
            sum += nmap.emplace(v, TO_VAL(0)).second;
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
        printf("    %12s    %s  %5d ns, factor = %.2f\n", __FUNCTION__, map_name.c_str(), AVE_TIME(ts1, vList.size()), nmap.load_factor());
    }
}

template<class MAP>
void find_miss(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())\
    {
        auto n = vList.size();
        size_t pow2 = 2 << ilog(n, 2);
        auto ts1 = getTime(); size_t sum = 0;
        for (size_t v = 1; v < pow2; v++)
            sum += amap.count(TO_KEY(v));
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
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
#if KEY_INT
            sum += amap.count(v + pow2);
#endif
        }
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void erase_half(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())\
    {
        auto ts1 = getTime(); size_t sum = 0;
        for (const auto v : vList)
            sum += amap.erase(v);
        //func_time.insert(std::pair<int64_t, std::string>(AVE_TIME(ts1, vList.size() + sum % 2), show_name[map_name]));
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void find_hit(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        auto ts1 = getTime();
        size_t sum = 0;
        for (const auto v : vList) {
#ifdef KEY_INT
            sum += amap.count(v) + v;
#else
            sum += amap.count(v) + v.size();
#endif
        }
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
void hash_clear(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())\
    {
        auto ts1 = getTime();
        size_t sum = amap.size();
        amap.clear(); amap.clear();
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

template<class MAP>
void hash_copy(MAP& amap, const std::string& map_name, std::vector<keyType>& vList)
{
    if (show_name.find(map_name) != show_name.end())
    {
        size_t sum = 0;
        auto ts1 = getTime();
        MAP tmap = amap;
        amap = tmap;
        sum  = tmap.size();
        check_mapfunc_result(map_name, __FUNCTION__, sum, ts1);
    }
}

//#define FUNC_RANK() for (auto& v : func_time) \ printf("   %-4ld     %-49s\n",  v.first, v.second.c_str()); putchar('\n'); func_time.clear();
#define FUNC_RANK()  func_time.clear();

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
    const auto iRation = 20;
#endif

    if (rand() % 100 > iRation)
    {
        emhash6 ::HashMap<keyType, int> eset(size);
        for (int i = 0; ; i++) {
            auto key = TO_KEY(srng());
            if (eset.emplace(key, 0).second) {
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

    return flag;
#endif
}

static int HashMapTest(int n, int max_loops = 1234567)
{
    emhash6::HashMap <keyType,int> emap5;
    emhash2::HashMap <keyType,int> emap2;

#if _CPP11_HASH
    //robin_hood::unordered_map <keyType,int> umap;
    emhash5::HashMap <keyType,int> umap;
#else
    std::unordered_map<keyType,int> umap;
#endif

    const auto step = n % 2 + 1;
  //  emap2.reserve(8); emap5.reserve(n / 8);
    umap.reserve(n);

    for (int i = 1; i < n * step; i += step) {
        auto ki = TO_KEY(i);
        emap5[ki] = umap[ki] = emap2[ki] = rand();
    }

    int loops = max_loops;
    while (loops -- > 0) {
        assert(emap2.size() == umap.size()); assert(emap5.size() == umap.size());

        const uint32_t type = rand() % 100;
        auto rid  = n ++;
        auto id   = TO_KEY(rid);
        if (type <= 40 || emap2.size() < 1000) {
            emap2[id] += type; emap5[id] += type; umap[id]  += type;

            assert(emap2[id] == umap[id]); assert(emap5[id] == umap[id]);
        }
        else if (type < 60) {
            if (rand() % 3 == 0)
                id = umap.begin()->first;
            else if (rand() % 2 == 0)
                id = emap2.begin()->first;
            else
                id = emap5.begin()->first;

            emap5.erase(emap5.find(id));
            umap.erase(id);
            emap2.erase(id);

            assert(emap5.count(id) == umap.count(id));
            assert(emap2.count(id) == umap.count(id));
        }
        else if (type < 80) {
            auto it = emap5.begin();
            for (int i = n % 64; i > 0; i--)
                it ++;
            id = it->first;
            umap.erase(id);
            emap2.erase(emap2.find(id));
            emap5.erase(it);
            assert(emap2.count(id) == 0);
            assert(emap5.count(id) == umap.count(id));
        }
        else if (type < 100) {
            if (umap.count(id) == 0) {
                const auto vid = (int)rid;
                emap5.emplace(id, vid);
                assert(emap5.count(id) == 1);
                assert(emap2.count(id) == 0);

                //if (id == 1043)
                emap2.insert(id, vid);
                assert(emap2.count(id) == 1);

                umap[id] = emap2[id];
                assert(umap[id] == emap2[id]);
                assert(umap[id] == emap5[id]);
            } else {
                umap[id] = emap2[id] = emap5[id] = 1;
                umap.erase(id);
                emap2.erase(id);
                emap5.erase(id);
            }
        }
        if (loops % 100000 == 0) {
            printf("%d %d\r", loops, (int)emap2.size());
#ifdef KEY_INT
            emap2.shrink_to_fit();
            emap5.shrink_to_fit();
            uint64_t sum1 = 0, sum2 = 0, sum3 = 0;
            for (auto v : umap)  sum1 += v.first * v.second;
            for (auto v : emap2) sum2 += v.first * v.second;
            for (auto v : emap5) sum3 += v.first * v.second;
            assert(sum1 == sum2);
            assert(sum1 == sum3);
#endif
        }
    }

    printf("\n");
    return 0;
}

template<class MAP>
int benOneMap(MAP& hmap, const std::string& map_name, std::vector<keyType>& oList)
{
    if (show_name.find(map_name) == show_name.end())
        return 80;

    auto vList(oList);
    func_time.clear();

    hmap.reserve(vList.size() / 8);
    insert_noreserve(hmap, map_name, vList);
    insert_reserve(hmap, map_name, vList);

//    shuffle(vList.begin(), vList.end());
    find_hit (hmap, map_name, vList);
    find_half(hmap, map_name, vList);
    find_miss(hmap, map_name, vList);

#ifdef KEY_INT
    for (int v = 0; v < (int)vList.size(); v += 2)
        vList[v] += vList.size();
#else
    sfc64 rng(vList.size());
    for (int v = 0; v < (int)vList.size() / 2; v ++)
        vList[v][0] = v % 256 + '0';
#endif

    erase_half(hmap, map_name, vList);
//    shuffle(vList.begin(), vList.end());
    find_erase(hmap, map_name, vList);
    erase_reinsert(hmap, map_name, vList);

#if 0
    check_result.erase("find_half");
    find_half(hmap, map_name, vList);
    check_result.erase("find_half");
#endif

    auto lf = (int)(hmap.load_factor() * 100);

#if UF
    hash_iter(hmap, map_name, vList);
    hash_copy(hmap, map_name, vList);
    hash_clear(hmap, map_name, vList);
#endif

    return lf;
}

struct StrHasher
{
  std::size_t operator()(const std::string& str) const
  {
#if BKR
      return wyhash(str.c_str(), str.size(), 131);
#else
      size_t hash = 0;
      for (const auto c : str)
          hash = c + hash * 131;
      return hash;
#endif
  }
};

static int benchMarkHashMap2(int n)
{
    if (n < 10000)
        n = 123456;
    printf("%s n = %d, keyType = %s, valueType = %s\n", __FUNCTION__, n, sKeyType, sValueType);

    typedef std::hash<keyType>        std_func;

#ifdef HOOD_HASH
    typedef robin_hood::hash<keyType> hood_func;
    using hash_func = hood_func;
#else
    using hash_func = std_func;
#endif

    auto iload = 0;
    const float lf = 90.5f / 100;

    check_result.clear();
    map_time.clear();
    func_map_time.clear();

    std::vector<keyType> vList;
    auto step = buildTestData(n, vList);

#ifdef KEY_INT
    using emap_func = hash_func;
#else
    using emap_func = StrHasher;
#endif

    {
        { emhash4::HashMap <keyType, valueType, emap_func> emap; emap.max_load_factor(lf); benOneMap(emap, "emhash4", vList); }
        { emhash3::HashMap <keyType, valueType, emap_func> emap; emap.max_load_factor(lf); iload = benOneMap(emap, "emhash3", vList); }
        { emhash6::HashMap <keyType, valueType, emap_func> emap; emap.max_load_factor(lf); iload = benOneMap(emap, "emhash6", vList); }
        { emhash2::HashMap <keyType, valueType, emap_func> emap; emap.max_load_factor(lf); benOneMap(emap, "emhash2", vList); }
        { emhash5::HashMap <keyType, valueType, emap_func> emap; emap.max_load_factor(lf); benOneMap(emap, "emhash5", vList); }
    }

#if _CPP14_HASH
    { std::unordered_map<keyType, valueType, hash_func> umap; umap.max_load_factor(lf); benOneMap(umap, "stl_hash", vList); }
    { ska::bytell_hash_map <keyType, valueType, hash_func > bmap; bmap.max_load_factor(lf); benOneMap(bmap, "byte", vList); }
#endif

#if _CPP11_HASH
    { phmap::flat_hash_map <keyType, valueType, hash_func> emap; emap.max_load_factor(lf); benOneMap(emap, "phmap", vList); }
    { robin_hood::unordered_map <keyType, valueType, hash_func> mmap; benOneMap(mmap, "martin", vList); }
    { ska::flat_hash_map <keyType, valueType, hash_func> fmap; fmap.max_load_factor(lf); benOneMap(fmap, "flat", vList); }
    { tsl::hopscotch_map <keyType, valueType, hash_func> hmap; hmap.max_load_factor(lf); benOneMap(hmap, "hopsco", vList); }
    { tsl::robin_map     <keyType, valueType, hash_func> rmap; rmap.max_load_factor(lf); benOneMap(rmap, "robin", vList); }
#endif

    static int tcase = 1;
    printf("\n %d ======== n = %d, flag = %d load_factor = %.2lf ========\n", tcase, n, step, iload / 100.0);
    std::multimap <int64_t, std::string> time_map;
    for (auto& v : map_time)
        time_map.insert(std::pair<int64_t, std::string>(v.second, v.first));

    const auto last  = double(time_map.rbegin()->first);
    const auto first = double(time_map.begin()->first);
    if (first < 10 || last < 9)
        return -1;

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
        printf("%5d   %13s   (%4.2lf %6.1lf%%)\n", int(v.first * 1000l / n), v.second.c_str(), last * 1.0 / v.first, first * 100.0 / v.first);
    }

    if (tcase++ % 5 == 0) {
        printf("--------------------------------%s lf = %d--------------------------------\n", __FUNCTION__, iload);
        dump_all(func_rank_time);

        puts("======== map  top1   top2  top3 =======================");
        for (auto& v : rank)
            printf("%13s %4.1lf  %4.1lf %4ld\n", v.first.c_str(), v.second / (double)(base1), (v.second / (base2 / 2) % 1000) / 2.0, (v.second % (base2 / 2)));

        puts("======== map    score ================================");
        for (auto& v : rank_time)
            printf("%13s %4ld\n", v.first.c_str(), v.second / (tcase - 1));
#if _WIN32
        //        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
#else
        usleep(1000*4000);
#endif
        printf("--------------------------------------------------------------------\n\n");
        return tcase;
    }

    printf("=======================================================================\n\n");
    return tcase;
}

#ifdef TREAD
static int readFile(std::string fileName, int size)
{
    if (!freopen(fileName.c_str(), "rb", stdin)) {
        std::cerr << "Cannot open the File : " << fileName << std::endl;
        freopen(CONSOLE, "r", stdin);
        return -1;
    }

    auto ts1 = getTime();
    std::vector<int64_t> vList;
    vList.reserve(1038860);

    int64_t                uid, score, pid;
    if (size == 1) {
        while (scanf("%ld", &uid) == size) vList.emplace_back(uid);
    } else if (size == 2) {
        while (scanf("%ld %ld", &uid, &score) == size) vList.emplace_back(uid);
    } else if (size == 6) {
        int items = 0;
        while (scanf("%ld %ld %d %ld %d %d", &uid, &pid, &items, &score, &items, &items) == size)
            vList.emplace_back(pid + uid);
    }
    freopen(CONSOLE, "r", stdin);

    //    std::sort(vList.begin(), vList.end());
    int n = (int)vList.size();
    printf("\nread file %s  %ld ms, size = %zd\n", fileName.c_str(), getTime() - ts1, vList.size());

    emhash2::HashMap<int64_t, int> emap2(n);
    emhash6::HashMap<int64_t, int> emap6(n);
    emhash5::HashMap<int64_t, int> emap5(n);

#if _CPP11_HASH
    ska::flat_hash_map  <int64_t, int> fmap(n);
    tsl::robin_map      <int64_t, int> rmap(n);
    phmap::flat_hash_map<int64_t, int> pmap(n);
    robin_hood::unordered_map<int64_t, int> mmap(n);
#endif

    ts1 = getTime();    for (auto v : vList)        emap2[v] = 1; printf("emhash2   insert  %4ld ms, size = %zd\n", getTime() - ts1, emap2.size());
    ts1 = getTime();    for (auto v : vList)        emap6[v] = 1; printf("emhash6   insert  %4ld ms, size = %zd\n", getTime() - ts1, emap6.size());
    ts1 = getTime();    for (auto v : vList)        emap5[v] = 1; printf("emhash5   insert  %4ld ms, size = %zd\n", getTime() - ts1, emap5.size());

#if _CPP11_HASH
    ts1 = getTime();    for (auto v : vList)        fmap[v] = 1;  printf("skamap    insert  %4ld ms, size = %zd\n", getTime() - ts1, fmap.size());
    ts1 = getTime();    for (auto v : vList)        rmap[v] = 1;  printf("tslmap    insert  %4ld ms, size = %zd\n", getTime() - ts1, rmap.size());
    ts1 = getTime();    for (auto v : vList)        pmap[v] = 1;  printf("phflat    insert  %4ld ms, size = %zd\n", getTime() - ts1, pmap.size());
    ts1 = getTime();    for (auto v : vList)        mmap[v] = 1;  printf("martin    insert  %4ld ms, size = %zd\n", getTime() - ts1, mmap.size());
#endif

    printf("\n");
    return 0;
}

void testSynax()
{
    emhash3::HashMap <std::string, std::string> mymap =
    {
        {"house","maison"},
        {"apple","pomme"},
        {"tree","arbre"},
        {"book","livre"},
        {"door","porte"},
        {"grapefruit","pamplemousse"}
    };

    std::vector<std::pair<std::string, std::string>> kv =
    {
        {"house2","maison"},
        {"apple2","pomme"},
        {"tree2","arbre"},
        {"book2","livre"},
        {"door2","porte"},
        {"grapefruit2","pamplemousse"}
    };

    for (auto& item : kv) {
        //mymap.insert_unique(item);
        mymap.emplace(item.first, item.second);
        mymap.insert(item);
        //mymap.emplace(std::move(item));
    }
    decltype(mymap) mymap2;
    mymap2.insert2(kv.begin(), kv.end());
    mymap2.insert2(kv.begin(), kv.end());
    mymap2 = mymap;
    //mymap2.reserve(mymap.bucket_count() * mymap.max_load_factor());

    auto nbuckets = mymap2.bucket_count();
    std::cout << "mymap has " << nbuckets << " buckets. and size = " << mymap2.size() << std::endl;
    for (unsigned i = 0; i < nbuckets; ++i) {
        std::cout << "bucket #" << i << " has " << mymap2.bucket_size(i) << " elements.\n";
    }

    for (auto& x : mymap2) {
        std::cout << "Element [" << x.first << ":" << x.second << "]";
        std::cout << " is in bucket #" << mymap2.bucket(x.first) << std::endl;
    }
}
#endif

int main(int argc, char* argv[])
{
    srand((unsigned)time(0));

    auto n = rand() % 1234567 + 100000;
    auto maxn = 3123456;

//    testSynax();
    double load_factor = 0.0;

    sfc64 srng(1);

    if (argc == 1)
        printf("./test maxn load_factor(0-100) n (key=%s,value=%s)\n", sKeyType, sValueType);

    if (argc > 1 && argv[1][0] > '0' && argv[1][0] <= '9')
        maxn = atoi(argv[1]) + 1000;
    if (argc > 2 && argv[1][0] > '0' && argv[1][0] <= '9')
        load_factor = atoi(argv[2]) / 100.0;
    if (argc > 3 && argv[3][0] > '0' && argv[3][0] <= '9')
        n = atoi(argv[2]);

    if (argc > 2 && argv[2][0] == 'd') {
        for (char c = argv[2][0], i = 0; c != '\0'; c = argv[2][i ++ ]) {
            if (c >= '2' && c < '8') {
                std:: string map_name("emhash");
                map_name += c;
                show_name.erase(map_name);
            } else if (c == 'm')
                show_name.erase("martin");
            else if (c == 'p')
                show_name.erase("phmap");
            else if (c == 't')
                show_name.erase("robin");
            else if (c == 's')
                show_name.erase("flat");
        }
    }

    HashMapTest(n, 234567);

#ifdef TREAD
    //readFile("./uid_income.txt", 1);
    //readFile("./pid_income.txt", 1);
    //readFile("./uids.csv", 2);
    readFile("./item.log", 6);
#endif

    auto nows = time(0);

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
            benchMarkHashMap2(n);
        }
#else
        n = (srng() % maxn) + srng() % 1234567 + 10000;
        if (load_factor > 0.4 && load_factor < 0.95) {
            auto pow2 = 1 << ilog(n, 2);
            n = int(pow2 * load_factor) + (1 << 12) - (rand() * rand()) % (1 << 13);
        }

        int tp = benchMarkHashMap2(n);
#endif

#if TP
        if (tp > TP)
            break;
#elif GCOV
        break;
#endif

        if (time(0) % 101 == 0)
            HashMapTest(n, (rand() * rand()) % 123457 + 10000);
    }

    return 0;
}

