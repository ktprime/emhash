#include <bitset>
#include <numeric>
#include <sstream>

#include <random>
#include <map>
#include <ctime>
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <array>

#define EMILIB_STATIS 1
#define HOOD_HASH     1
//#define EMILIB_REHASH_COPY 0
//#define EMILIB_HIGH_LOAD 10000000

using namespace std;

//#include "hash_table2.hpp"
#include "hash_table5.hpp"
#include "hash_table6.hpp"
#include "hash_table7.hpp"
#include "hash_set.hpp"

#if __cplusplus >= 199711LL
#define CPP14   1
#include "./tsl/robin_set.h"        //https://github.com/tessil/robin-map
#include "./tsl/robin_map.h"        //https://github.com/tessil/robin-map
#include "./tsl/hopscotch_map.h"    //https://github.com/tessil/hopscotch-map
#include "./martin/robin_hood.h"       //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h

#include "./ska/flat_hash_map.hpp"  //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
#include "./ska/bytell_hash_map.hpp"//https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#include "./phmap/phmap.h"        //https://github.com/tessil/robin-map
#endif

#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
# include <windows.h>
#else
# include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#endif

/*

   bench_IterateIntegers map = N7 emilib2 7HashMapImmN10robin_hood4hashImEESt8equal_toImEEE
   total iterate/removing time = 9.760, 11.096
   bench_IterateIntegers map = N7 emilib5 7HashMapImmN10robin_hood4hashImEESt8equal_toImEEE
   total iterate/removing time = 9.884, 11.124
   bench_IterateIntegers map = N7 emilib3 7HashMapImmN10robin_hood4hashImEESt8equal_toImEEE
   total iterate/removing time = 10.592, 11.092
   bench_IterateIntegers map = N3 tsl9 robin_map ImmN10robin_hood4hashImEESt8equal_toImESaISt4pairImmEELb0ENS_2rh26power_of_two_growth_policyILm2EEEEE total iterate/removing time = 11.676, 14.152
   bench_IterateIntegers map = N10 robin_hood 6detail13unordered_mapILb1ELm88EmmNS_4hashImEESt8equal_toImEEE
   total iterate/removing time = 6.372, 12.624
   bench_IterateIntegers map = N3 ska 13flat_hash_map ImmN10robin_hood4hashImEESt8equal_toImESaISt4pairImmEEEE
   total iterate/removing time = 10.524, 10.572
   bench_IterateIntegers map = N5 phmap 13 flat_hash_map ImmN10robin_hood4hashImEENS_7EqualToImEESaISt4pairIKmmEEEE

*/
emilib3::HashMap<std::string, std::string> show_name =
{
    {"emilib2", "emilib2"},
    {"emilib3", "emilib3"},
    {"emilib4", "emilib4"},
    {"emilib5", "emilib5"},
    {"emilib6", "emilib6"},

    {"robin_hood", "martin flat"},
    {"robin_map", "tessil robin"},

    {"phmap", "phmap flat"},
    {"ska", "skarupk flat"},
    {"byte", "skarupk byte"},
};

static const char* find(const std::string& map_name)
{
    for (const auto& kv : show_name)
    {
        if (map_name.find(kv.first) != std::string::npos)
        {
            return kv.second.c_str();
        }
    }

    return map_name.c_str();
}


//#pragma once
/**
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
 ***/

// this is probably the fastest high quality 64bit random number generator that exists.
// Implements Small Fast Counting v4 RNG from PractRand.
class sfc64 {
    public:
        using result_type = uint64_t;

        // no copy ctors so we don't accidentally get the same random again
        sfc64(sfc64 const&) = delete;
        sfc64& operator=(sfc64 const&) = delete;

        sfc64(sfc64&&) = default;
        sfc64& operator=(sfc64&&) = default;

        sfc64(std::array<uint64_t, 4> const& state)
            : m_a(state[0])
              , m_b(state[1])
              , m_c(state[2])
              , m_counter(state[3]) {}

        static constexpr uint64_t(min)() {
            return (std::numeric_limits<uint64_t>::min)();
        }
        static constexpr uint64_t(max)() {
            return (std::numeric_limits<uint64_t>::max)();
        }

        sfc64()
            : sfc64(UINT64_C(0x853c49e6748fea9b)) {}

        sfc64(uint64_t seed)
            : m_a(seed)
              , m_b(seed)
              , m_c(seed)
              , m_counter(1) {
                  for (int i = 0; i < 12; ++i) {
                      operator()();
                  }
              }

        void seed() {
            *this = sfc64{std::random_device{}()};
        }

        uint64_t operator()() noexcept {
            auto const tmp = m_a + m_b + m_counter++;
            m_a = m_b ^ (m_b >> right_shift);
            m_b = m_c + (m_c << left_shift);
            m_c = rotl(m_c, rotation) + tmp;
            return tmp;
        }

        // this is a bit biased, but for our use case that's not important.
        uint64_t operator()(uint64_t boundExcluded) noexcept {
#ifdef __SIZEOF_INT128__
            return static_cast<uint64_t>((static_cast<unsigned __int128>(operator()()) * static_cast<unsigned __int128>(boundExcluded)) >> 64u);
#endif
        }

        std::array<uint64_t, 4> state() const {
            return {m_a, m_b, m_c, m_counter};
        }

        void state(std::array<uint64_t, 4> const& s) {
            m_a = s[0];
            m_b = s[1];
            m_c = s[2];
            m_counter = s[3];
        }

    private:
        template <typename T>
            T rotl(T const x, int k) {
                return (x << k) | (x >> (8 * sizeof(T) - k));
            }

        static constexpr int rotation = 24;
        static constexpr int right_shift = 11;
        static constexpr int left_shift = 3;
        uint64_t m_a;
        uint64_t m_b;
        uint64_t m_c;
        uint64_t m_counter;
};

template<class RandomIt>
void myrandom_shuffle(RandomIt first, RandomIt last)
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

static int64_t now2ms()
{
#if _WIN32
    FILETIME ptime[4] = { 0 };
    GetThreadTimes(GetCurrentThread(), &ptime[0], &ptime[1], &ptime[2], &ptime[3]);
    return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10000;
#elif __linux__ || __unix__
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec = rup.ru_utime.tv_sec + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000 + usec / 1000;
#elif _WIN32
    return clock();
#else
    return clock() * 1000ll;
#endif
}

template<class MAP> int bench_insert(MAP& map)
{
    printf("%s map = %s\n", __FUNCTION__, find(typeid(MAP).name()));
    auto nowms = now2ms();
    sfc64 rng(123);
    {
        {
            auto ts = now2ms();
            for (size_t n = 0; n < 100'000'000; ++n) {
                map[static_cast<int>(rng())];
            }
            printf("    insert 100M int %.4lf sec loadf = %.2f, size = %d\n", (now2ms() - ts) / 1000.0, map.load_factor(), (int)map.size());
        }

        {
            auto ts = now2ms();
            map.clear();
            printf("    clear 100M int %.4lf , size = %d\n", (now2ms() - ts) / 1000.0, (int)map.size());
        }

        {
            auto ts = now2ms();
            for (size_t n = 0; n < 100'000'000; ++n) {
                map[static_cast<int>(rng())];
            }
            printf("    reinsert 100M int %.4lf sec loadf = %.2f, size = %d\n", (now2ms() - ts) / 1000.0, map.load_factor(), (int)map.size());
        }

        {
            auto ts = now2ms();
            for (size_t n = 0; n < 100'000'000; ++n) {
                map.erase(static_cast<int>(rng()));
            }
            printf("    remove 100M int %.4lf sec, size = %d\n", (now2ms() - ts) / 1000.0, (int)map.size());
        }
    }

    printf("%s %.3lf s\n\n", __FUNCTION__,  (now2ms() - nowms)/1000.0);
    return 0;
}

template <typename T>
struct as_bits_t {
    T value;
};

template <typename T>
as_bits_t<T> as_bits(T value) {
    return as_bits_t<T>{value};
}

template <typename T>
std::ostream& operator<<(std::ostream& os, as_bits_t<T> const& t) {
    os << std::bitset<sizeof(t.value) * 8>(t.value);
    return os;
}

template<class MAP> void bench_randomInsertErase(MAP& map)
{
    // random bits to set for the mask
    std::vector<int> bits(64);
    std::iota(bits.begin(), bits.end(), 0);
    sfc64 rng(999);

#if _MSC_VER
    for (auto &v : bits) v = rng();
#else
    std::shuffle(bits.begin(), bits.end(), rng);
#endif

    uint64_t bitMask = 0;
    auto bitsIt = bits.begin();

    size_t const expectedFinalSizes[] = {7, 127, 2084, 32722, 524149, 8367491};
    size_t const max_n = 50'000'000;

    printf("%s map = %s\n", __FUNCTION__, find(typeid(MAP).name()));
    auto nowms = now2ms();
    for (int i = 0; i < 6; ++i) {
        // each iteration, set 4 new random bits.
        for (int b = 0; b < 4; ++b) {
            bitMask |= UINT64_C(1) << *bitsIt++;
        }

        // std::cout << (i + 1) << ". " << as_bits(bitMask) << std::endl;

        auto ts = now2ms();
        // benchmark randomly inserting & erasing
        for (size_t i = 0; i < max_n; ++i) {
            map.emplace(rng() & bitMask, i);
            map.erase(rng() & bitMask);
        }

        printf("    %02ld bits %2zd M cycles time use %.4lf sec map %zd size\n", std::bitset<64>(bitMask).count(), (max_n / 1000'000), (now2ms() - ts) / 1000.0, map.size());
        assert(map.size() == expectedFinalSizes[i]);
    }
    printf("%s %.3lf s\n\n", __FUNCTION__,  (now2ms() - nowms)/1000.0);
}

template<class MAP> void bench_randomDistinct2(MAP& map)
{
    printf("%s map = %s\n", __FUNCTION__,find( typeid(MAP).name()));
    constexpr size_t const n = 50'000'000;

    auto nowms = now2ms();
    sfc64 rng(123);

//#ifndef _MSC_VER

    int checksum;
    {
        auto ts = now2ms();
        checksum = 0;
        size_t const max_rng = n / 20;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     05%% distinct %.4lf sec loadf = %.2f, size = %d\n", (now2ms() - ts) / 1000.0, map.load_factor(), (int)map.size());
        assert(549985352 == checksum);
    }

    {
        map.clear();
        auto ts = now2ms();
        checksum = 0;
        size_t const max_rng = n / 4;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     25%% distinct %.4lf sec loadf = %.2f, size = %d\n", (now2ms() - ts) / 1000.0, map.load_factor(), (int)map.size());
        assert(149979034 == checksum);
    }

    {
        map.clear();
        auto ts = now2ms();
        size_t const max_rng = n / 2;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     50%% distinct %.4lf sec loadf = %.2f, size = %d\n", (now2ms() - ts) / 1000.0, map.load_factor(), (int)map.size());
        assert(249981806 == checksum);
    }

    {
        map.clear();
        auto ts = now2ms();
        checksum = 0;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng())];
        }
        printf("    100%% distinct %.4lf sec loadf = %.2f, size = %d\n", (now2ms() - ts) / 1000.0, map.load_factor(), (int)map.size());
        assert(50291811 == checksum);
    }
//#endif

    printf("%s %.3lf s\n\n", __FUNCTION__, (now2ms() - nowms)/1000.0);
}

template<class MAP>
size_t runRandomString(size_t max_n, size_t string_length, uint32_t bitMask, MAP& map)
{
    //printf("%s map = %s\n", __FUNCTION__, typeid(MAP).name());
    sfc64 rng(123);

    // time measured part
    size_t verifier = 0;
    std::stringstream ss;
    ss << string_length << " bytes" << std::dec;

    std::string str(string_length, 'x');
    // point to the back of the string (32bit aligned), so comparison takes a while
    auto const idx32 = (string_length / 4) - 1;
    auto const strData32 = reinterpret_cast<uint32_t*>(&str[0]) + idx32;

    auto ts = now2ms();
    for (size_t i = 0; i < max_n; ++i) {
        *strData32 = rng() & bitMask;

        // create an entry.
        map[str] = 0;

        *strData32 = rng() & bitMask;
        auto it = map.find(str);
        if (it != map.end()) {
            ++verifier;
            map.erase(it);
        }
    }

    printf("    %016x time = %.3lf, loadf = %.3lf %zd\n", bitMask, (now2ms() - ts) / 1000.0, map.load_factor(), map.size());
    return verifier;
}

template<class MAP>
void bench_randomEraseString(MAP& map)
{
    printf("%s map = %s\n", __FUNCTION__, find(typeid(MAP).name()));
    auto ts = now2ms();
    {
        assert(9734165 == runRandomString(20'000'000, 7, 0xfffff, map));
    }
    {
        assert(2966452 == runRandomString(6'000'000, 1000, 0x1ffff, map));
    }
    {
        assert(9734165 == runRandomString(20'000'000, 8, 0xfffff, map));
    }
    {
        assert(9734165 == runRandomString(20'000'000, 13, 0xfffff, map));
    }
    {
        assert(5867228 == runRandomString(12'000'000, 100, 0x7ffff, map));
    }
    printf("total time = %.3lf\n\n", (now2ms() - ts)/1000.0);
}

template<class MAP>
uint64_t randomFindInternal(size_t numRandom, uint64_t bitMask, size_t numInserts, size_t numFindsPerInsert) {
    size_t constexpr NumTotal = 4;
    size_t const numSequential = NumTotal - numRandom;

    size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

    sfc64 rng(123);

    size_t num_found = 0;
    MAP map;
    std::array<bool, NumTotal> insertRandom = {false};
    for (size_t i = 0; i < numRandom; ++i) {
        insertRandom[i] = true;
    }

    sfc64 anotherUnrelatedRng(987654321);
    auto const anotherUnrelatedRngInitialState = anotherUnrelatedRng.state();
    sfc64 findRng(anotherUnrelatedRngInitialState);
    auto ts = now2ms();

    {
        size_t i = 0;
        size_t findCount = 0;

        //bench.beginMeasure(title.c_str());
        do {
            // insert NumTotal entries: some random, some sequential.
            std::shuffle(insertRandom.begin(), insertRandom.end(), rng);
            for (bool isRandomToInsert : insertRandom) {
                auto val = anotherUnrelatedRng();
                if (isRandomToInsert) {
                    map[rng() & bitMask] = static_cast<size_t>(1);
                } else {
                    map[val & bitMask] = static_cast<size_t>(1);
                }
                ++i;
            }

            // the actual benchmark code which sohould be as fast as possible
            for (size_t j = 0; j < numFindsPerIter; ++j) {
                if (++findCount > i) {
                    findCount = 0;
                    findRng.state(anotherUnrelatedRngInitialState);
                }
                auto it = map.find(findRng() & bitMask);
                if (it != map.end()) {
                    num_found += it->second;
                }
            }
        } while (i < numInserts);
    }

    printf("    %3lu%% %016lx success time = %.3lf s loadf = %.3lf\n", numSequential * 100 / NumTotal, bitMask, (now2ms() - ts) / 1000.0, map.load_factor());
    return num_found;
}

template<class MAP>
void bench_IterateIntegers(MAP& map) {
    printf("%s map = %s\n", __FUNCTION__, find(typeid(MAP).name()));
    sfc64 rng(123);

    size_t const num_iters = 50000;
    uint64_t result = 0;
    //Map<uint64_t, uint64_t> map;

    auto ts = now2ms();
    for (size_t n = 0; n < num_iters; ++n) {
        map[rng()] = n;
        for (auto const& keyVal : map) {
            result += keyVal.second;
        }
    }

    auto ts1 = now2ms();
    for (size_t n = 0; n < num_iters; ++n) {
        map.erase(rng());
        for (auto const& keyVal : map) {
            result += keyVal.second;
        }
    }
    printf("    total iterate/removing time = %.3lf, %.3lf\n", (ts1 - ts)/1000.0, (now2ms() - ts1)/1000.0);
}

template<class MAP>
void bench_randomFind(MAP& bench)
{
    printf("\n%s map = %s\n", __FUNCTION__, find(typeid(MAP).name()));
    static constexpr auto lower32bit = UINT64_C(0x00000000FFFFFFFF);
    static constexpr auto upper32bit = UINT64_C(0xFFFFFFFF00000000);
    static constexpr size_t numInserts = 2000;
    static constexpr size_t numFindsPerInsert = 500'000;

    auto ts = now2ms();

    randomFindInternal<MAP>(4, lower32bit, numInserts, numFindsPerInsert);
    randomFindInternal<MAP>(4, upper32bit, numInserts, numFindsPerInsert);

    randomFindInternal<MAP>(3, lower32bit, numInserts, numFindsPerInsert);
    randomFindInternal<MAP>(3, upper32bit, numInserts, numFindsPerInsert);

    randomFindInternal<MAP>(2, lower32bit, numInserts, numFindsPerInsert);
    randomFindInternal<MAP>(2, upper32bit, numInserts, numFindsPerInsert);

    randomFindInternal<MAP>(1, lower32bit, numInserts, numFindsPerInsert);
    randomFindInternal<MAP>(1, upper32bit, numInserts, numFindsPerInsert);

    randomFindInternal<MAP>(0, lower32bit, numInserts, numFindsPerInsert);
    randomFindInternal<MAP>(0, upper32bit, numInserts, numFindsPerInsert);

    printf("total time = %.3lf\n", (now2ms() - ts)/1000.0);
}

std::vector<size_t> integerKeys()
{
    std::mt19937_64 rd(0);

    std::vector<size_t> numbers;
    for (size_t i = 0; i < 1000000; ++i) {
        numbers.push_back(rd());
    }

    return numbers;
}

std::vector<std::string> generateKeys(
        const std::string& prefix, const std::vector<size_t>& integer_keys, const std::string& suffix)
{
    std::vector<std::string> results;
    results.reserve(integer_keys.size());
    for (const auto& int_key : integer_keys) {
        results.emplace_back(prefix + std::to_string(int_key) + suffix);
        // results.emplace_back(prefix + std::string((const char*)&int_key, sizeof(int_key)) + suffix);
    }
    return results;
}

template<typename HashSet, typename Keys>
double time_once(const Keys& keys)
{
    auto ns = now2ms();
    HashSet set;

    // Add keys individually to force rehashing every now and then:
    for (const auto& key : keys) {
        set.emplace(key);
    }

    return (now2ms() - ns) / 1000.0;
}

#if 0
template<typename HashSet, typename Keys>
double best_of_many(const Keys& keys)
{
    double best_time = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < 10; ++i) {
        best_time = std::min(best_time, time_once<HashSet>(keys));
    }
    return best_time;
}

int main2()
{

    const auto integer_keys  = integerKeys();
    const auto short_keys    = generateKeys("", integer_keys, "");
    const auto long_prefix   = generateKeys(std::string(81, 'x'), integer_keys, "");
    const auto long_suffix   = generateKeys("", integer_keys, std::string(81, 'x'));

    printf("\nInteger keys (e.g. %lu):\n", integer_keys[0]);
    printf("unordered_set<size_t>:            %5.0f ms\n", 1e3 * best_of_many<unordered_set<size_t>>(integer_keys));
    printf("HashSet<size_t>:                  %5.0f ms\n", 1e3 * best_of_many<emilib6::HashSet<size_t>>(integer_keys));

    printf("\nShort keys (e.g. \"%s\"):\n", short_keys[0].c_str());
    printf("unordered_set<string>:            %5.0f ms\n", 1e3 * best_of_many<unordered_set<string>>(short_keys));
    printf("HashSet<string>:                  %5.0f ms\n", 1e3 * best_of_many<emilib6::HashSet<string>>(short_keys));

    printf("\nLong suffixes (e.g. \"%s\"):\n", long_suffix[0].c_str());
    printf("unordered_set<string>:            %5.0f ms\n", 1e3 * best_of_many<unordered_set<string>>(long_suffix));
    printf("HashSet<string>:                  %5.0f ms\n", 1e3 * best_of_many<emilib6::HashSet<string>>(long_suffix));

    printf("\nLong prefixes (e.g. \"%s\"):\n", long_prefix[0].c_str());
    printf("unordered_set<string>:            %5.0f ms\n", 1e3 * best_of_many<unordered_set<string>>(long_prefix));
    printf("HashSet<string>:                  %5.0f ms\n", 1e3 * best_of_many<emilib6::HashSet<string>>(long_prefix));
    return 0;
}
#endif

static void basic_test(int n)
{
    printf("1. add 1 - %d one by one\n", n);
    {
        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (int i = 0; i < n; i++)
                emap.insert_unique(i, i);
            printf("emap unique time = %ld ms loadf = %.3lf\n", now2ms() - ts, emap.load_factor());
        }

        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (int i = 0; i < n; i++)
                emap.insert(i, i);
            printf("emap insert time = %ld ms loadf = %.3lf\n", now2ms() - ts, emap.load_factor());
        }

        {
            auto ts = now2ms();
            std::unordered_map<int, int> umap(n);
            for (int i = 0; i < n; i++)
                umap.emplace(i, i);
            printf("umap insert time = %ld ms loadf = %.3lf\n", now2ms() - ts, umap.load_factor());
        }

        if (0)
        {
            auto ts = now2ms();
            std::map<int, int> mmap;
            for (int i = 0; i < n; i++)
                mmap.emplace(i, i);
            printf("map time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            std::vector<int> v;
            v.reserve(n);
            for (int i = 0; i < n; i++)
                v.emplace_back(i);
            printf("vec reserve time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            std::vector<int> v;
            for (int i = 0; i < n; i++)
                v.emplace_back(i);
            printf("vec emplace time = %ld ms\n", now2ms() - ts);
        }
        {
            auto ts = now2ms();
            std::vector<int> v(n);
            for (int i = 0; i < n; i++)
                v[i] = i;
            printf("vec resize time = %ld ms\n", now2ms() - ts);
        }
        {
            auto ts = now2ms();
            std::vector<int> v;
            for (int i = 0; i < n; i++)
                v.push_back(i);
            printf("vec push time = %ld ms\n", now2ms() - ts);
        }
        {
            auto ts = now2ms();
            emilib9::HashSet<int> eset(n);
            for (int i = 0; i < n; i++)
                eset.insert_unique(i);
            printf("eset time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            std::unordered_set<int> uset(n);
            for (int i = 0; i < n; i++)
                uset.insert(i);
            printf("uset time = %ld ms\n", now2ms() - ts);
        }

        puts("\n");
    }

    printf("2. random_shuffle 1 - %d\n", n);
    {
        std::vector <int> data(n);
        for (int i = 0; i < n; i ++)
            data[i] = i;

        myrandom_shuffle(data.begin(), data.end());
        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert(v, 0);
            printf("emap insert time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert_unique(v, 0);
            printf("emap unique time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            std::unordered_map<int, int> umap(n);
            for (auto v: data)
                umap.emplace(v, 0);
            printf("umap insert time = %ld ms loadf = %.3lf\n", now2ms() - ts, umap.load_factor());
        }

        {
            auto ts = now2ms();
            std::vector<int> v(n);
            for (auto d: data)
                v[d] = 0;
            printf("vec    time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            emilib9::HashSet<int> eset(n);
            for (auto v: data)
                eset.insert_unique(v);
            printf("eset unique time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            std::unordered_set<int> eset(n);
            for (auto v: data)
                eset.insert(v);
            printf("uset insert time = %ld ms\n", now2ms() - ts);
        }
        puts("\n");
    }

    puts("3. random data");
    {
        std::vector <int> data(n);
        for (int i = 0; i < n; i ++)
            data[i] = (uint32_t)(rand() * rand()) % n + rand();
        myrandom_shuffle(data.begin(), data.end());

        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert(v, 0);
            printf("emap5 insert time = %ld ms loadf = %.3lf/size %zd\n", now2ms() - ts, emap.load_factor(), emap.size());
        }

        {
            auto ts = now2ms();
            std::unordered_map<int, int> umap(n);
            for (auto v: data)
                umap.emplace(v, 0);
            printf("umap insert time = %ld ms loadf = %.3lf\n", now2ms() - ts, umap.load_factor());
        }

        if (0)
        {
            auto ts = now2ms();
            std::vector<int> vec(n);
            for (auto d: data)
                vec[(unsigned int)d % n] = 0;
            printf("vec time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            emilib9::HashSet<int> eset(n);
            eset.insert(data.begin(), data.end());
            printf("eset insert range time = %ld ms loadf = %.3lf/size %zd\n", now2ms() - ts, eset.load_factor(), eset.size());
        }

        {
            auto ts = now2ms();
            std::unordered_set<int> uset(n);
            uset.insert(data.begin(), data.end());
            printf("uset insert time = %ld ms loadf = %.3lf/size %zd\n", now2ms() - ts, uset.load_factor(), uset.size());
        }

        puts("\n");
    }
}

int main(int argc, char* argv[])
{
    srand(time(0));
    //main2();
    int n = argc > 1 ? atoi(argv[1]) : 12345678;
    basic_test(n);

    if (1)
    {
#ifndef HOOD_HASH
        typedef robin_hood::hash<std::string> hash_func;
#else
        typedef std::hash<std::string> hash_func;
#endif

        {emilib2::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
        {emilib3::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
        {emilib5::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
        {emilib6::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench);}

        {tsl::robin_map  <std::string, int, hash_func> bench; bench_randomEraseString(bench);}
        {robin_hood::unordered_flat_map <std::string, int, hash_func> bench; bench_randomEraseString(bench);}
        {ska::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
        {phmap::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench);}
    }

    if (1)
    {
#if 1
        typedef robin_hood::hash<size_t> hash_func;
#else
        typedef std::hash<size_t> hash_func;
#endif

#if CPP14
        { tsl::robin_map  <size_t, size_t, hash_func> rmap;  bench_randomFind(rmap); }
        { robin_hood::unordered_flat_map <size_t, size_t, hash_func> martin; bench_randomFind(martin); }
        { ska::flat_hash_map <size_t, size_t, hash_func> fmap; bench_randomFind(fmap); }
        { phmap::flat_hash_map <size_t, size_t, hash_func> pmap; bench_randomFind(pmap); }
#endif
        { emilib2::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
        { emilib5::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
        { emilib3::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
        { emilib6::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap); }
    }

    if (1)
    {
#ifdef HOOD_HASH
        typedef robin_hood::hash<int> hash_func;
#else
        typedef std::hash<int> hash_func;
#endif
        { emilib5::HashMap<int, int, hash_func> emap; bench_insert(emap); /*            emap.dump_statis(); **/ }
        { emilib2::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emilib3::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emilib6::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#if CPP14
        { tsl::robin_map     <int, int, hash_func> rmap; bench_insert(rmap); }
        { robin_hood::unordered_flat_map <int, int, hash_func> martin; bench_insert(martin); }
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_insert(fmap); }
        { phmap::flat_hash_map <int, int, hash_func> pmap; bench_insert(pmap); }
        //        { ska::bytell_hash_map <int, int, hash_func > bmap; bench_insert(bmap); }
#endif
    }

    if (1)
    {
#if 1
        typedef robin_hood::hash<int> hash_func;
#else
        typedef std::hash<int> hash_func;
#endif
        { emilib5::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emilib3::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); /**           emap.dump_statis(); */ }
        { emilib2::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emilib6::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#if CPP14
        { tsl::robin_map     <int, int, hash_func> rmap; bench_randomDistinct2(rmap); }
        { robin_hood::unordered_flat_map <int, int, hash_func> martin; bench_randomDistinct2(martin); }
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_randomDistinct2(fmap); }
        { phmap::flat_hash_map <int, int, hash_func> pmap; bench_randomDistinct2(pmap); }
        //        { ska::bytell_hash_map <int, int, hash_func > bmap; bench_randomDistinct2(bmap); }
#endif
    }

    if (1)
    {
#if 1
        typedef robin_hood::hash<uint64_t> hash_func;
#else
        typedef std::hash<uint64_t> hash_func;
#endif
        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emilib5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emilib3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); /*            emap.dump_statis(); */ }
        { emilib6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
#if CPP14
        { tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_randomInsertErase(rmap); }
        { robin_hood::unordered_flat_map <uint64_t, uint64_t, hash_func> martin; bench_randomInsertErase(martin); }
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_randomInsertErase(fmap); }
        { phmap::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
        //        { ska::bytell_hash_map <uint64_t, uint64_t, hash_func > bmap; bench_randomInsertErase(bmap); }
#endif
    }

    if (1)
    {
#if 1
        typedef robin_hood::hash<uint64_t> hash_func;
#else
        typedef std::hash<uint64_t> hash_func;
#endif
        { emilib3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); /*            emap.dump_statis(); */ }
        { emilib5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emilib6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
#if CPP14
        { tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_IterateIntegers(rmap); }
        { robin_hood::unordered_flat_map <uint64_t, uint64_t, hash_func> martin; bench_IterateIntegers(martin); }
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_IterateIntegers(fmap); }
        { phmap::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_IterateIntegers(pmap); }
#endif
    }

    return 0;
}
