#include "util.h"
#include <sstream>

#ifndef _WIN32
#include <sys/resource.h>
#endif

//#include "wyhash.h"

//#define EMH_STATIS 1
//#define ET            1
//#define EMH_WYHASH64   1
//#define HOOD_HASH     1
//#define ABSL 1
//#define ABSL_HASH 1
//
//#define EMH_PACK_TAIL 16
//#define EMH_HIGH_LOAD 123456
#if EM3
#include "old/hash_table2.hpp"
#include "old/hash_table3.hpp"
#include "old/hash_table4.hpp"
#endif

#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table7.hpp"
#include "../hash_table8.hpp"

#if X86
#include "emilib/emilib.hpp"
#include "emilib/emilib2.hpp"
#include "emilib/emilib2s.hpp"
#endif
//#include "old/ktprime_hash.hpp"

#if __cplusplus >= 201103L || _MSC_VER > 1600
#include "martinus/robin_hood.h"     //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h

#if CXX17
#include "martinus/unordered_dense.h"
#endif

#if ET
#include "phmap/phmap.h"           //https://github.com/tessil/robin-map
#include "tsl/robin_map.h"         //https://github.com/tessil/robin-map
//#include "tsl/hopscotch_map.h"     //https://github.com/tessil/hopscotch-map
#if X86_64
#include "ska/flat_hash_map.hpp"   //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
//#include "hrd/hash_set7.h"         //https://github.com/tessil/robin-map
#endif
//#include "ska/bytell_hash_map.hpp" //https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#endif

#if FOLLY
#include "folly/container/F14Map.h"
#endif
#endif

static const auto RND = getus() + time(0);

static std::map<std::string, std::string> show_name =
{
#if EM3
    {"emhash2", "emhash2"},
    //{"emhash3", "emhash3"},
    {"emhash4", "emhash4"},
#endif
   {"emhash7", "emhash7"},
   {"emhash8", "emhash8"},
   {"emhash5", "emhash5"},
#if X86
//    {"emilib", "emilib"},
    {"emilib2", "emilib2"},
    {"emilib3", "emilib3"},
#endif

    {"ankerl", "martinus dense"},

#if QC_HASH
    {"qc", "qchash"},
    {"fph", "fph"},
#endif

//    {"jg", "jg_dense"},
     {"emhash6", "emhash6"},
#if ABSL
    {"absl", "absl flat"},
#endif
#if ET
    {"rigtorp", "rigtorp"},
    {"phmap", "phmap flat"},
    {"robin_hood", "martinus flat"},
//    {"hrd7", "hrd7map"},
//    {"folly", "f14_vector"},

#if ET > 1
    {"robin_map", "tessil robin"},
    {"ska", "skarupk flat"},
#endif
#endif
};

static const char* find_hash(const std::string& map_name)
{
    if (map_name.find("emilib2") < 10)
        return show_name.count("emilib2") ? show_name["emilib2"].data() : nullptr;
    if (map_name.find("emilib3") < 10)
        return show_name.count("emilib3") ? show_name["emilib3"].data() : nullptr;

    for (const auto& kv : show_name)
    {
        if (map_name.find(kv.first) < 10)
            return kv.second.c_str();
    }

    return nullptr;
}

#ifndef RT
    #define RT 1  //2 wyrand 1 sfc64 3 RomuDuoJr 4 Lehmer64 5 mt19937_64
#endif

#if RT == 1
    #define MRNG sfc64
#elif RT == 2 && WYHASH_LITTLE_ENDIAN
    #define MRNG WyRand
#elif RT == 3
    #define MRNG RomuDuoJr
#else
    #define MRNG std::mt19937_64
#endif

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
            : m_a(seed), m_b(seed), m_c(seed), m_counter(1) {
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
#elif _WIN32
            uint64_t high;
            uint64_t a = operator()();
            _umul128(a, boundExcluded, &high);
            return high;
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

static inline float now2sec()
{
#if _WIN320
    FILETIME ptime[4];
    GetThreadTimes(GetCurrentThread(), &ptime[0], &ptime[2], &ptime[2], &ptime[3]);
    return (ptime[3].dwLowDateTime + ptime[2].dwLowDateTime) / 10000000.0f;
#elif __linux__
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec + usec / 1000000.0;
#elif __unix__
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec + start.tv_usec / 1000000.0;
#else
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count() / 1000000.0;
#endif
}

template<class MAP> void bench_insert(MAP& map)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

#if X86_64
    size_t maxn = 1000000;
#else
    size_t maxn = 1000000 / 5;
#endif

    for (int  i = 0; i < 2; i++) {
        auto nows = now2sec();
        MRNG rng(RND + 5 + i);
        {
            {
                auto ts = now2sec();
                for (size_t n = 0; n < maxn; ++n) {
                    map[static_cast<int>(rng())];
                }
                printf("insert %.2f", now2sec() - ts);
            }

            {
                auto ts = now2sec();
                map.clear();
                printf(", clear %.3f", now2sec() - ts); fflush(stdout);
            }

            {
                auto ts = now2sec();
                for (size_t n = 0; n < maxn; ++n) {
                    map.emplace(static_cast<int>(rng()), 0);
                }
                printf(", reinsert %.2f", now2sec() - ts); fflush(stdout);
            }

            {
                auto ts = now2sec();
                for (size_t n = 0; n < maxn; ++n) {
                    map.erase(static_cast<int>(rng()));
                }
                printf(", remove %.2f", now2sec() - ts); fflush(stdout);
            }
        }
        printf(", loadf = %.2f size = %d, total %dM int time = %.2f s\n",
                map.load_factor(), (int)map.size(), int(maxn / 1000000), now2sec() - nows);
        maxn *= 100;
    }
    printf("\n");
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

template<class RandomIt, class URBG>
void rshuffle(RandomIt first, RandomIt last, URBG&& g)
{
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


template<class ForwardIt, class T>
void iotas(ForwardIt first, ForwardIt last, T value)
{
    while(first != last) {
        *first++ = value;
        ++value;
    }
}
template<class MAP> void bench_randomInsertErase(MAP& map)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);
    auto nows = now2sec();

    {
        uint32_t min_n    = 1 << 20;
        uint32_t max_loop = min_n << 5;
//        map.max_load_factor(0.87);
        for (int j = 0; j < 5; ++j) {
            MRNG rng(RND + 6 + j);
            MRNG rng2(RND + 6 + j);
            // each iteration, set 4 new random bits.
            // std::cout << (i + 1) << ". " << as_bits(bitMask) << std::endl;
            auto maxn = min_n * (50 + j * 9) / 100;
            for (size_t i = 0; i < maxn; ++i) {
                map.emplace(rng(), 0);
            }

            auto ts = now2sec();
            maxn = max_loop * 10 / (10 + 4*j);
            // benchmark randomly inserting & erasing
            for (size_t i = 0; i < maxn; ++i) {
                map.emplace(rng(), 0);
                map.erase(rng2());
            }
            printf("    %8u %2d M cycles time %.3f s map size %8d loadf = %.2f\n",
                    maxn, int(min_n / 1000000), now2sec() - ts, (int)map.size(), map.load_factor());
            min_n *= 2;
            map.clear();
        }
    }

    {
        MAP map2;
        std::vector<int> bits(64, 0);
        iotas(bits.begin(), bits.end(), 0);
        sfc64 rng(999);

#if 0
        for (auto &v : bits) v = rng();
#else
        rshuffle(bits.begin(), bits.end(), rng);
#endif

        uint64_t bitMask = 0;
        auto bitsIt = bits.begin();
        //size_t const expectedFinalSizes[] = {7, 127, 2084, 32722, 524149, 8367491};
        size_t const max_n = 50000000;

        for (int i = 0; i < 6; ++i) {
            for (int b = 0; b < 4; ++b) {
                bitMask |= UINT64_C(1) << *bitsIt++;
            }

            auto ts = now2sec();
            for (size_t i = 0; i < max_n; ++i) {
                map2.emplace(rng() & bitMask, 0);
                map2.erase(rng() & bitMask);
            }
            printf("    %02d bits  %2d M cycles time %.3f s map size %d loadf = %.2f\n",
                    int(std::bitset<64>(bitMask).count()), int(max_n / 1000000), now2sec() - ts, (int)map2.size(), map2.load_factor());
        }
    }

    printf("total time = %.2f s\n\n", now2sec() - nows);
}

template<class MAP> void bench_randomDistinct2(MAP& map)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

#if X86_64
    constexpr size_t const n = 50000000;
#else
    constexpr size_t const n = 50000000 / 2;
#endif
    auto nows = now2sec();
    MRNG rng(RND + 7);

    //    map.max_load_factor(7.0 / 8);
    int checksum;
    {
        auto ts = now2sec();
        checksum = 0;
        size_t const max_rng = n / 20;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     05%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, map.load_factor(), (int)map.size());
        assert(RND != 123 || 549985352 == checksum);
    }

    {
        map.clear();
        auto ts = now2sec();
        checksum = 0;
        size_t const max_rng = n / 4;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     25%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, map.load_factor(), (int)map.size());
        assert(RND != 123 || 149979034 == checksum);
    }

    {
        map.clear();
        auto ts = now2sec();
        size_t const max_rng = n / 2;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     50%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, map.load_factor(), (int)map.size());
        assert(RND != 123 || 249981806 == checksum);
    }

    {
        map.clear();
        auto ts = now2sec();
        checksum = 0;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng())];
        }
        printf("    100%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, map.load_factor(), (int)map.size());
        assert(RND != 123 || 50291811 == checksum);
    }
    //#endif

    printf("total time = %.2f s\n\n", now2sec() - nows);
}

template<class MAP> void bench_copy(MAP& map)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    size_t result = 0;
    sfc64 rng(987);

    MAP mapSource;
    uint64_t rememberKey = 0;
    for (size_t i = 0; i < 1'000'000; ++i) {
        auto key = rng();
        if (i == 500'000) {
            rememberKey = key;
        }
        mapSource[key] = i;
    }

    auto nows = now2sec();
    MAP mapForCopy = mapSource;
    for (size_t n = 0; n < 200; ++n) {
        MAP m = mapForCopy;
        result += m.size() + m[rememberKey];
        mapForCopy[rng()] = rng();
    }
    assert(result == 300019900);
    auto copyt = now2sec();
    printf("copy time = %.2f s,", copyt - nows);
    mapForCopy = mapSource;

    MAP m;
    for (size_t n = 0; n < 200; ++n) {
        m = mapForCopy;
        result += m.size() + m[rememberKey];
        mapForCopy[rng()] = rng();
    }
    assert(result == 600039800);
    printf(" assign time = %.2f s\n\n", now2sec() - copyt);
}

template<class MAP>
size_t runInsertEraseString(size_t max_n, size_t string_length, uint32_t bitMask )
{
    //printf("%s map = %s\n", __FUNCTION__, typeid(MAP).name());
    MRNG rng(RND + 4);

    // time measured part
    size_t verifier = 0;
    std::stringstream ss;
    ss << string_length << " bytes" << std::dec;

    std::string str(string_length, 'x');
    // point to the back of the string (32bit aligned), so comparison takes a while
    auto const idx32 = (string_length / 4) - 1;
    auto const strData32 = reinterpret_cast<uint32_t*>(&str[0]) + idx32;

    MAP map;
//    map.reserve(bitMask / 8);
    auto ts = now2sec();
    for (size_t i = 0; i < max_n; ++i) {
        *strData32 = rng() & bitMask;
#if 0
        // create an entry.
        map[str] = 0;
        *strData32 = rng() & bitMask;
        auto it = map.find(str);
        if (it != map.end()) {
            ++verifier;
            map.erase(it);
        }
#else
        map.emplace(str, 0);
        *strData32 = rng() & bitMask;
        verifier += map.erase(str);
#endif
    }

    printf("%4zd bytes time = %.2f, loadf = %.2f %d\n", string_length, now2sec() - ts, map.load_factor(), (int)map.size());
    return verifier;
}

template<class MAP>
uint64_t randomFindInternalString(size_t numRandom, size_t const length, size_t numInserts, size_t numFindsPerInsert)
{
    size_t constexpr NumTotal = 4;
    size_t const numSequential = NumTotal - numRandom;

    size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

    std::stringstream ss;
    ss << (numSequential * 100 / NumTotal) << "% " << length << " byte";
    auto title = ss.str();

    sfc64 rng(RND + 3);

    size_t num_found = 0;

    std::array<bool, NumTotal> insertRandom = {false};
    for (size_t i = 0; i < numRandom; ++i) {
        insertRandom[i] = true;
    }

    sfc64 anotherUnrelatedRng(987654321);
    auto const anotherUnrelatedRngInitialState = anotherUnrelatedRng.state();
    sfc64 findRng(anotherUnrelatedRngInitialState);

    std::string str(length, 'y');
    // point to the back of the string (32bit aligned), so comparison takes a while
    auto const idx32 = (length / 4) - 1;
    auto const strData32 = reinterpret_cast<uint32_t*>(&str[0]) + idx32;

    auto ts = now2sec();
    MAP map;
    {
        size_t i = 0;
        size_t findCount = 0;

        do {
            // insert NumTotal entries: some random, some sequential.
            std::shuffle(insertRandom.begin(), insertRandom.end(), rng);
            for (bool isRandomToInsert : insertRandom) {
                auto val = anotherUnrelatedRng();
                if (isRandomToInsert) {
                    *strData32 = rng();
                } else {
                    *strData32 = val;
                }
                map[str] = 1;
                ++i;
            }

            // the actual benchmark code which sohould be as fast as possible
            for (size_t j = 0; j < numFindsPerIter; ++j) {
                if (++findCount > i) {
                    findCount = 0;
                    findRng.state(anotherUnrelatedRngInitialState);
                }
                *strData32 = findRng();
                auto it = map.find(str);
                if (it != map.end()) {
                    num_found += it->second;
                }
            }
        } while (i < numInserts);
    }

    printf("    %s success time = %.2f s %8d loadf = %.2f\n", title.c_str(), now2sec() - ts, (int)num_found, map.load_factor());
    return num_found;
}

template<class MAP>
void bench_randomFindString(MAP& map)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    auto nows = now2sec();
    auto now1 = nows, now2 = nows;
    if (1)
    {
        static constexpr size_t numInserts = 1000000 / 2;
        static constexpr size_t numFindsPerInsert = 200 / 2;

        randomFindInternalString<MAP>(4, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(3, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(2, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(1, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(0, 13, numInserts, numFindsPerInsert);
        now1 = now2sec();
    }

    {
        static constexpr size_t numInserts = 100000;
        static constexpr size_t numFindsPerInsert = 1000;

        randomFindInternalString<MAP>(4, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(3, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(2, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(1, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<MAP>(0, 100, numInserts, numFindsPerInsert);
        now2 = now2sec();
    }
    printf("total time = %.2f + %.2f = %.2f\n\n", now1 - nows, now2 - now1, now2 - nows);
}

template<class MAP>
void bench_randomEraseString(MAP& map)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    auto nows = now2sec();
    { runInsertEraseString<MAP>(20000000, 7, 0xfffff); }
    { runInsertEraseString<MAP>(20000000, 8, 0xfffff); }
    { runInsertEraseString<MAP>(20000000, 13, 0xfffff); }
    { runInsertEraseString<MAP>(10000000, 24, 0xfffff); }
    { runInsertEraseString<MAP>(12000000, 100, 0x4ffff); }
    { runInsertEraseString<MAP>(8000000,  200, 0x3ffff); }
    { runInsertEraseString<MAP>(6000000,  1000,0x7ffff); }

    printf("total time = %.2f s\n\n", now2sec() - nows);
}

template<class MAP>
uint64_t randomFindInternal(size_t numRandom, uint64_t bitMask, const size_t numInserts, const size_t numFindsPerInsert) {
    size_t constexpr NumTotal = 4;
    size_t const numSequential = NumTotal - numRandom;

    size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

    sfc64 rng(RND + 2);

    size_t num_found = 0;
    MAP map;
    //map.max_load_factor(7.0 / 8);
    std::array<bool, NumTotal> insertRandom = {false};
    for (size_t i = 0; i < numRandom; ++i) {
        insertRandom[i] = true;
    }

    sfc64 anotherUnrelatedRng(987654321);
    auto const anotherUnrelatedRngInitialState = anotherUnrelatedRng.state();
    sfc64 findRng(anotherUnrelatedRngInitialState);
    auto ts = now2sec();

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
                num_found += map.count(findRng() & bitMask);
            }
        } while (i < numInserts);
    }

#if _WIN32
    printf("    %3u%% %016llx success time = %.2f s, %8d loadf = %.2f\n",
#else
    printf("    %3u%% %016lx success time = %.2f s, %8d loadf = %.2f\n",
#endif
            uint32_t(numSequential * 100 / NumTotal), bitMask, now2sec() - ts, (int)num_found, map.load_factor());

    return num_found;
}

template<class MAP>
void bench_IterateIntegers(MAP& map)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    MRNG rng(123);

    size_t const num_iters = 50000;
    uint64_t result = 0;
    //Map<uint64_t, uint64_t> map;

    auto ts = now2sec();
    for (size_t n = 0; n < num_iters; ++n) {
        map[rng()] = n;
        for (auto & keyVal : map) {
            result += keyVal.second;
        }
    }
    assert(result == 20833333325000ull);

    auto ts1 = now2sec();
    for (size_t n = 0; n < num_iters; ++n) {
        map.erase(rng());
        for (auto const& keyVal : map) {
            result += keyVal.second;
        }
    }
    assert(result == 62498750000000ull + 20833333325000ull);
    printf("    total iterate/removing time = %.2f, %.2f|%lu\n\n", (ts1 - ts), now2sec() - ts, result);
}

template<class MAP>
void bench_randomFind(MAP& bench, size_t numInserts, size_t numFindsPerInsert)
{
    auto map_name = find_hash(typeid(MAP).name());
    if (!map_name)
        return;
    printf("\n%s map = %s\n", __FUNCTION__, map_name);

    static constexpr auto lower32bit = UINT64_C(0x00000000FFFFFFFF);
    static constexpr auto upper32bit = UINT64_C(0xFFFFFFFF00000000);

    auto ts = now2sec();

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

    printf("nums = %zd, total time = %.2f\n", numInserts, now2sec() - ts);
}

void runTest(int sflags, int eflags)
{
    const auto start = now2sec();

    if (sflags <= 1 && eflags >= 1)
    {
#if ABSL_HASH
        typedef absl::Hash<uint64_t> hash_func;
#elif STD_HASH
        typedef std::hash<uint64_t> hash_func;
#else
        typedef robin_hood::hash<uint64_t> hash_func;
#endif

#if QC_HASH
        { qc::hash::RawMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { fph::DynamicFphMap<uint64_t, uint64_t, fph::MixSeedHash<uint64_t>> emap; bench_IterateIntegers(emap); }
#endif

#if EM3
        { emhash2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emhash3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emhash4::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
#endif
        { emhash5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emhash8::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emhash7::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emhash6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
#if CXX17
        { ankerl::unordered_dense::map <uint64_t, uint64_t, hash_func> martin; bench_IterateIntegers(martin); }
#endif
#if CXX20
        { jg::dense_hash_map<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { rigtorp::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
#endif
#if ET
        //        { hrd7::hash_map <uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
        { tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_IterateIntegers(rmap); }
        { robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_IterateIntegers(martin); }

#if X86_64
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_IterateIntegers(fmap); }
#endif
        { phmap::flat_hash_map<uint64_t, uint64_t, hash_func> pmap; bench_IterateIntegers(pmap); }
#endif
#if X86
        { emilib::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emilib3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
#endif
#if ABSL
        { absl::flat_hash_map<uint64_t, uint64_t, hash_func> pmap; bench_IterateIntegers(pmap); }
#endif
#if FOLLY
        { folly::F14VectorMap<uint64_t, uint64_t, hash_func> pmap; bench_IterateIntegers(pmap); }
#endif
        putchar('\n');
    }

    if (sflags <= 2 && eflags >= 2)
    {
#ifdef HOOD_HASH
        typedef robin_hood::hash<std::string> hash_func;
#elif ABSL_HASH
        typedef absl::Hash<std::string> hash_func;
//#elif AHASH_AHASH_H
//        typedef Ahash64er hash_func;
#elif STD_HASH
        typedef std::hash<std::string> hash_func;
#else
        typedef WysHasher hash_func;
#endif
#if CXX17
        { ankerl::unordered_dense::map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif

        { emhash8::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#if EM3
        { emhash2::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emhash3::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emhash4::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
        { emhash6::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emhash5::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#if QC_HASH
        { fph::DynamicFphMap<std::string, size_t, fph::MixSeedHash<std::string>> bench; bench_randomFindString(bench); }
#endif
        { emhash7::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#if X86
        { emilib3::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emilib2::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emilib::HashMap<std::string,  size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
#if ET
        //        { hrd7::hash_map <std::string, size_t, hash_func> hmap;   bench_randomFindString(hmap); }
        { tsl::robin_map  <std::string, size_t, hash_func> bench;     bench_randomFindString(bench); }
        { robin_hood::unordered_map <std::string, size_t, hash_func> bench; bench_randomFindString(bench); }

#if X86_64
        { ska::flat_hash_map<std::string, size_t, hash_func> bench;   bench_randomFindString(bench); }
#endif
        { phmap::flat_hash_map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
#if CXX20
        { jg::dense_hash_map<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        { rigtorp::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
#if FOLLY
        { folly::F14VectorMap<std::string, size_t, hash_func> bench;  bench_randomFindString(bench); }
#endif
#if ABSL
        { absl::flat_hash_map<std::string, size_t, hash_func> bench;  bench_randomFindString(bench); }
#endif
        putchar('\n');
    }

    if (sflags <= 3 && eflags >= 3)
    {
#ifdef HOOD_HASH
        typedef robin_hood::hash<std::string> hash_func;
#elif ABSL_HASH
        typedef absl::Hash<std::string> hash_func;
//#elif AHASH_AHASH_H
//        typedef Ahash64er hash_func;
#elif STD_HASH
        typedef std::hash<std::string> hash_func;
#else
        typedef WysHasher hash_func;
#endif

#if EM3
        { emhash4::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { emhash2::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { emhash3::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if X86
        { emilib2::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { emilib::HashMap<std::string,  int, hash_func> bench; bench_randomEraseString(bench); }
        { emilib3::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
        { emhash8::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { emhash7::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { emhash6::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { emhash5::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }

#if CXX17
        { ankerl::unordered_dense::map <std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if CXX20
        { rigtorp::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { jg::dense_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif

#if QC_HASH
        { fph::DynamicFphMap<std::string, int, fph::MixSeedHash<std::string>> bench; bench_randomEraseString(bench); }
#endif

#if ET
        //        { hrd7::hash_map <std::string, int, hash_func> hmap;   bench_randomEraseString(hmap); }
        { tsl::robin_map  <std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { robin_hood::unordered_map <std::string, int, hash_func> bench; bench_randomEraseString(bench); }

#if X86_64
        { ska::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
        { phmap::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if FOLLY
        { folly::F14VectorMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if ABSL
        { absl::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
    }

    if (sflags <= 4 && eflags >= 4)
    {
#if ABSL_HASH
        typedef absl::Hash<size_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<size_t> hash_func;
#elif STD_HASH
        typedef std::hash<size_t> hash_func;
#else
        typedef robin_hood::hash<size_t> hash_func;
#endif

        static constexpr size_t numInserts[] = { /*200,*/ 2000, 500000 };
        static constexpr size_t numFindsPerInsert[] = { /*5000000,*/ 500000, 1000 };
        for (size_t i = 0; i < sizeof(numInserts) / sizeof(numInserts[0]); i++)
        {
#if ET
            { tsl::robin_map <size_t, size_t, hash_func> rmap; bench_randomFind(rmap, numInserts[i], numFindsPerInsert[i]); }
            { robin_hood::unordered_map <size_t, size_t, hash_func> martin; bench_randomFind(martin, numInserts[i], numFindsPerInsert[i]); }

#if X86_64
            { ska::flat_hash_map <size_t, size_t, hash_func> fmap; bench_randomFind(fmap, numInserts[i], numFindsPerInsert[i]); }
#endif
            { phmap::flat_hash_map <size_t, size_t, hash_func> pmap; bench_randomFind(pmap, numInserts[i], numFindsPerInsert[i]); }
            //        { hrd7::hash_map <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if QC_HASH
            { fph::DynamicFphMap<size_t, size_t, fph::MixSeedHash<size_t>> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { qc::hash::RawMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if CXX17
            { ankerl::unordered_dense::map<size_t, size_t, hash_func> martin; bench_randomFind(martin, numInserts[i], numFindsPerInsert[i]); }
#endif
#if CXX20
            { jg::dense_hash_map<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { rigtorp::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if X86
            { emilib2::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emilib3::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emilib::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#endif
            { emhash5::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emhash6::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emhash7::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#if ABSL
            { absl::flat_hash_map <size_t, size_t, hash_func> pmap; bench_randomFind(pmap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if FOLLY
            { folly::F14VectorMap <size_t, size_t, hash_func> pmap; bench_randomFind(pmap, numInserts[i], numFindsPerInsert[i]); }
#endif
            { emhash8::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#if EM3
            { emhash4::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emhash2::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emhash3::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#endif
            putchar('\n');
        }
    }

    if (sflags <= 5 && eflags >= 5)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<int> hash_func;
#else
        typedef std::hash<int> hash_func;
#endif

#if ABSL //failed on other hash
        { absl::flat_hash_map <int, int, hash_func> amap; bench_insert(amap); }
#endif
#if FOLLY
        { folly::F14VectorMap <int, int, hash_func> pmap; bench_insert(pmap); }
#endif
        { emhash7::HashMap<int, int, hash_func> emap; bench_insert(emap); }

#if CXX20
        { jg::dense_hash_map<int, int, hash_func> qmap; bench_insert(qmap); }
        { rigtorp::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#endif
#if CXX17
        { ankerl::unordered_dense::map<int, int, hash_func> martin; bench_insert(martin); }
#endif

#if QC_HASH
        { qc::hash::RawMap<int, int, hash_func> qmap; bench_insert(qmap); }

#endif
        { emhash6::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash8::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash5::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#if EM3
        { emhash2::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash4::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash3::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#endif
#if X86
        { emilib::HashMap<int, int, hash_func>  emap; bench_insert(emap); }
        { emilib2::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emilib3::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#endif
#if ET
        //        { hrd7::hash_map <int, int, hash_func> hmap;  bench_insert(hmap); }
        { tsl::robin_map  <int, int, hash_func> rmap; bench_insert(rmap); }
        { robin_hood::unordered_map <int, int, hash_func> martin; bench_insert(martin); }



#if X86_64
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_insert(fmap); }
#endif
        { phmap::flat_hash_map <int, int, hash_func> pmap; bench_insert(pmap); }
#endif
        putchar('\n');
    }

    if (sflags <= 6 && eflags >= 6)
    {
#if ABSL_HASH
        typedef absl::Hash<uint64_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<uint64_t> hash_func;
#elif STD_HASH
        typedef std::hash<uint64_t> hash_func;
#else
        typedef robin_hood::hash<uint64_t> hash_func;
#endif

#if ABSL
        { absl::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
#endif
        { emhash5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash7::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash8::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
#if QC_HASH
        { fph::DynamicFphMap<uint64_t, uint64_t, fph::MixSeedHash<uint64_t>> emap; bench_randomInsertErase(emap); }
#if QC_HASH==2
        { qc::hash::RawMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); } //hang
#endif
#endif
#if EM3
        { emhash2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash4::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
#endif

#if X86
        { emilib3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emilib::HashMap<uint64_t, uint64_t, hash_func>  emap; bench_randomInsertErase(emap); }
#endif
#if ET
        //        { hrd7::hash_map <size_t, size_t, hash_func> hmap; bench_randomInsertErase(hmap); }
        { tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_randomInsertErase(rmap); }
        { robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_randomInsertErase(martin); }

#if X86_64
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_randomInsertErase(fmap); }
#endif
        { phmap::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
#endif
#if FOLLY
        { folly::F14VectorMap <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
#endif
#if CXX17
        { ankerl::unordered_dense::map<uint64_t, uint64_t, hash_func> martin; bench_randomInsertErase(martin); }
#endif
#if CXX20
        { jg::dense_hash_map<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        //{ rigtorp::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); } //hange
#endif
        putchar('\n');
    }

    if (sflags <= 7 && eflags >= 7)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif STD_HASH
        typedef std::hash<int> hash_func;
#else
        typedef robin_hood::hash<int> hash_func;
#endif

#if QC_HASH
        { qc::hash::RawMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
//        { fph::DynamicFphMap<int, int, fph::MixSeedHash<int>> emap; bench_randomDistinct2(emap); } //hang
#endif

#if CXX20
        { jg::dense_hash_map<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { rigtorp::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#endif
#if CXX17
        { ankerl::unordered_dense::map <int, int, hash_func> martin; bench_randomDistinct2(martin); }
#endif

        { emhash6::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash5::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash7::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash8::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#if EM3
        { emhash2::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash4::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash3::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#endif


#if X86
        { emilib::HashMap<int, int, hash_func> emap;  bench_randomDistinct2(emap); }
        { emilib2::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emilib3::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#endif
#if ET
        //        { hrd7::hash_map <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
        { tsl::robin_map     <int, int, hash_func> rmap; bench_randomDistinct2(rmap); }
        { robin_hood::unordered_map <int, int, hash_func> martin; bench_randomDistinct2(martin); }

#if X86_64
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_randomDistinct2(fmap); }
#endif
        { phmap::flat_hash_map <int, int, hash_func> pmap; bench_randomDistinct2(pmap); }
#endif
#if ABSL
        { absl::flat_hash_map <int, int, hash_func> pmap; bench_randomDistinct2(pmap); }
#endif

#if FOLLY
        { folly::F14VectorMap <int, int, hash_func> pmap; bench_randomDistinct2(pmap); }
#endif

        putchar('\n');
    }

    if (sflags <= 8 && eflags >= 8)
    {
#if STD_HASH
        typedef std::hash<uint64_t> hash_func;
#else
        typedef robin_hood::hash<uint64_t> hash_func;
#endif

        { emhash6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
        { emhash5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
        { emhash7::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
        { emhash8::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }

#if QC_HASH
        { qc::hash::RawMap<int, int, hash_func> emap; bench_copy(emap); }
#endif

#if CXX20
        { jg::dense_hash_map<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
        { rigtorp::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
#endif
#if CXX17
        { ankerl::unordered_dense::map <uint64_t, uint64_t, hash_func> martin; bench_copy(martin); }
#endif

#if EM3
        { emhash2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
        { emhash4::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
        { emhash3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
#endif

#if X86
        { emilib::HashMap<uint64_t, uint64_t, hash_func> emap;  bench_copy(emap); }
        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
        { emilib3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_copy(emap); }
#endif
#if ET
        { tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_copy(rmap); }
        { robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_copy(martin); }

#if X86_64
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_copy(fmap); }
#endif
        { phmap::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_copy(pmap); }
#endif
#if ABSL
        { absl::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_copy(pmap); }
#endif

#if FOLLY
        { folly::F14VectorMap <uint64_t, uint64_t, hash_func> pmap; bench_copy(pmap); }
#endif

    }

    printf("total time = %.3f s", now2sec() - start);
}

static void checkSet(const std::string& map_name)
{
    if (show_name.count(map_name) == 1)
        show_name.erase(map_name);
    else
        show_name.emplace(map_name, map_name);
}

int main(int argc, char* argv[])
{
    printInfo(nullptr);
    puts("./test [2-9mptseb0d2 rjqf] n");
    for (auto& m : show_name)
        printf("%10s %20s\n", m.first.c_str(), m.second.c_str());

    int sflags = 1, eflags = 8;
    if (argc > 1) {
        printf("cmd agrs = %s\n", argv[1]);
        for (char c = argv[1][0], i = 0; c != '\0'; c = argv[1][++i]) {
            if (c > '3' && c < '9') {
                std::string map_name("emhash");
                map_name += c;
                checkSet(map_name);
            } else if (c == 'm') {
                checkSet("robin_hood");
                checkSet("ankerl");
            }
            else if (c == 'p')
                checkSet("phmap");
            else if (c == 'a')
                checkSet("absl");
            else if (c == 't')
                checkSet("robin_map");
            else if (c == 's')
                checkSet("ska");
            else if (c == 'h')
                checkSet("hrd7");
            else if (c == 'e' || c == '1')
                checkSet("emilib");
            else if (c == '2')
                checkSet("emilib2");
            else if (c == '3')
                checkSet("emilib3");
            else if (c == 'j')
                checkSet("jg");
            else if (c == 'r')
                checkSet("rigtorp");
            else if (c == 'q')
                checkSet("qc");
            else if (c == 'f')
                checkSet("fph");
            else if (c == 'b')
                sflags = atoi(&argv[1][++i]);
            else if (c == 'e')
                eflags = atoi(&argv[1][++i]);
        }
    }

    puts("test hash:");
    for (auto& m : show_name)
        printf("%10s %20s\n", m.first.c_str(), m.second.c_str());

    runTest(sflags, eflags);
    return 0;
}

