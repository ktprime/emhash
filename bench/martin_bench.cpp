#include <sstream>

#ifndef _WIN32
#include <sched.h>
#include <pthread.h>
//#include <cpuid.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#endif

#if 0
#include "ahash/ahash.c"
#include "ahash/random_state.c"
#endif

#include "wyhash.h"
using namespace std;

//#define EMH_STATIS 1
//#define ET            1
//#define EMH_WYHASH64   1
//#define HOOD_HASH     1
//#define ABSL 1
//#define ABSL_HASH 1

//#define EMH_HIGH_LOAD 123456

#include "util.h"
#include "old/hash_table2.hpp"
#include "old/hash_table3.hpp"
#include "old/hash_table4.hpp"
#include "hash_table5.hpp"
#include "hash_table6.hpp"
#include "hash_table7.hpp"
#include "hash_table8.hpp"

#include "emilib/emilib.hpp"
#include "emilib/emilib2.hpp"

#if __cplusplus >= 201103L || _MSC_VER > 1600
#include "martin/robin_hood.h"     //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h

#if ET
#include "phmap/phmap.h"           //https://github.com/tessil/robin-map
#include "tsl/robin_map.h"         //https://github.com/tessil/robin-map
#include "tsl/hopscotch_map.h"     //https://github.com/tessil/hopscotch-map
//#include "hrd/hash_set7.h"         //https://github.com/tessil/robin-map

#include "ska/flat_hash_map.hpp"   //https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_map.hpp
#include "ska/bytell_hash_map.hpp" //https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp
#endif

#if FOLLY
#include "folly/container/F14Map.h"
#endif
#endif

static auto RND = time(0);

static std::map<std::string, std::string> show_name =
{
#if EM3
    {"emhash2", "emhash2"},
    //{"emhash3", "emhash3"},
    {"emhash4", "emhash4"},
#endif
    {"emhash7", "emhash7"},
    //    {"emhash8", "emhash8"},

    {"emhash5", "emhash5"},
    {"emilib", "emilib"},
    {"emilib2", "emilib2"},
    //    {"emhash6", "emhash6"},
#if ABSL
    {"absl", "absl flat"},
#endif
//    {"folly", "f14_vector"},
#if ET
    {"phmap", "phmap flat"},
    {"robin_hood", "martin flat"},
    //    {"hrd7", "hrd7map"},

#if ET > 1
    {"robin_map", "tessil robin"},
    {"ska", "skarupk flat"},
#endif
#endif
};

static const char* find(const std::string& map_name)
{
    for (const auto& kv : show_name)
    {
        if (map_name.find(kv.first) < 10)
        {
            return kv.second.c_str();
        }
    }

    return nullptr;
}

#ifndef RT
    #define RT 1  //1 wyrand 2 sfc64 3 RomuDuoJr 4 Lehmer64 5 mt19937_64
#endif

#if RT == 1
    #define MRNG sfc64
#elif RT == 2
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
#elif _MSC_VER
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

static inline int64_t now2ms()
{
    return getus() / 1000;
}

template<class MAP> void bench_insert(MAP& map)
{
    auto map_name = find(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    constexpr int maxn = 100'000'000;
    auto nowms = now2ms();
    MRNG rng(RND);
    {
        {
            auto ts = now2ms();
            for (size_t n = 0; n < maxn; ++n) {
                map[static_cast<int>(rng())];
            }
            printf("    insert 100M int %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
        }

        {
            auto ts = now2ms();
            map.clear();
            printf("    clear 100M int %.4lf\n", ((int)(now2ms() - ts)) / 1000.0);
        }

        {
            auto ts = now2ms();
            for (size_t n = 0; n < maxn; ++n) {
                map[static_cast<int>(rng())];
            }
            printf("    reinsert 100M int %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
        }

        {
            auto ts = now2ms();
            for (size_t n = 0; n < maxn; ++n) {
                map.erase(static_cast<int>(rng()));
            }
            printf("    remove 100M int %.4lf sec, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, (int)map.size());
        }
    }

    printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
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
    std::vector<int> bits(64, 0);
    std::iota(bits.begin(), bits.end(), 0);
    MRNG rng(999);

#if 0
    for (auto &v : bits) v = rng();
#else
    std::shuffle(bits.begin(), bits.end(), rng);
#endif

    uint64_t bitMask = 0;
    auto bitsIt = bits.begin();

    size_t const expectedFinalSizes[] = {7, 127, 2084, 32722, 524149, 8367491};
    size_t const max_n = 50'000'000;

    //    map.max_load_factor(7.0 / 8);
    auto map_name = find(typeid(MAP).name());
    if (!map_name)
        return;
    printf("%s map = %s\n", __FUNCTION__, map_name);

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

        printf("    %02ld bits %2zd M cycles time use %.4lf sec map size %d loadf = %.2f\n",
                std::bitset<64>(bitMask).count(), (max_n / 1000'000), ((int)(now2ms() - ts)) / 1000.0, (int)map.size(), map.load_factor());

#ifndef _MSC_VER
        //assert(RND != 123 || map.size() == expectedFinalSizes[i]);
#endif
    }
    printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
}

template<class MAP> void bench_randomDistinct2(MAP& map)
{
    auto map_name = find(typeid(MAP).name());
    if (!map_name)
        return ;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    constexpr size_t const n = 50'000'000;

    auto nowms = now2ms();
    MRNG rng(RND);

    //    map.max_load_factor(7.0 / 8);
    int checksum;
    {
        auto ts = now2ms();
        checksum = 0;
        size_t const max_rng = n / 20;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     05%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
        assert(RND != 123 || 549985352 == checksum);
    }

    {
        map.clear();
        auto ts = now2ms();
        checksum = 0;
        size_t const max_rng = n / 4;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     25%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
        assert(RND != 123 || 149979034 == checksum);
    }

    {
        map.clear();
        auto ts = now2ms();
        size_t const max_rng = n / 2;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng(max_rng))];
        }
        printf("     50%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
        assert(RND != 123 || 249981806 == checksum);
    }

    {
        map.clear();
        auto ts = now2ms();
        checksum = 0;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++map[static_cast<int>(rng())];
        }
        printf("    100%% distinct %.4lf sec loadf = %.2f, size = %d\n", ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
        assert(RND != 123 || 50291811 == checksum);
    }
    //#endif

    printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
}

template<class MAP>
size_t runRandomString(size_t max_n, size_t string_length, uint32_t bitMask )
{
    //printf("%s map = %s\n", __FUNCTION__, typeid(MAP).name());
    MRNG rng(RND);

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
    auto ts = now2ms();
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

    printf("    %016x time = %.2lf, loadf = %.2lf %d\n", bitMask, ((int)(now2ms() - ts)) / 1000.0, map.load_factor(), (int)map.size());
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

    sfc64 rng(RND);

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

    auto ts = now2ms();
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

    printf("    %s success time = %.2lf s %d loadf = %.2lf\n", title.c_str(), ((int)(now2ms() - ts)) / 1000.0, (int)num_found, map.load_factor());
    return num_found;
}

template<class MAP>
void bench_randomFindString(MAP& map)
{
    auto map_name = find(typeid(MAP).name());
    if (!map_name)
        return ;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    static constexpr size_t numInserts = 100'000;
    static constexpr size_t numFindsPerInsert = 1000;
    auto nowms = now2ms();

    randomFindInternalString<MAP>(4, 100, numInserts, numFindsPerInsert);
    randomFindInternalString<MAP>(3, 100, numInserts, numFindsPerInsert);
    randomFindInternalString<MAP>(2, 100, numInserts, numFindsPerInsert);
    randomFindInternalString<MAP>(1, 100, numInserts, numFindsPerInsert);
    randomFindInternalString<MAP>(0, 100, numInserts, numFindsPerInsert);
    printf("total time = %.2lf\n\n", ((int)(now2ms() - nowms))/1000.0);
}

template<class MAP>
void bench_randomEraseString(MAP& map)
{
    auto map_name = find(typeid(MAP).name());
    if (!map_name)
        return ;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    auto nowms = now2ms();
    { runRandomString<MAP> (20'000'000, 7, 0xfffff); }
    { runRandomString<MAP>(6'000'000, 1000, 0x1ffff); }
    { runRandomString<MAP>(20'000'000, 8, 0xfffff); }
    { runRandomString<MAP>(20'000'000, 13, 0xfffff); }
    { runRandomString<MAP>(12'000'000, 100, 0x7ffff); }

    printf("total time = %.2lf s\n\n", (now2ms() - nowms) / 1000.0);
}

template<class MAP>
uint64_t randomFindInternal(size_t numRandom, uint64_t bitMask, const size_t numInserts, const size_t numFindsPerInsert) {
    size_t constexpr NumTotal = 4;
    size_t const numSequential = NumTotal - numRandom;

    size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

    sfc64 rng(RND);

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

    printf("    %3lu%% %016lx success time = %.2lf s, %8d loadf = %.2lf\n", numSequential * 100 / NumTotal, bitMask, ((int)(now2ms() - ts)) / 1000.0, (int)num_found, map.load_factor());
    return num_found;
}

template<class MAP>
void bench_IterateIntegers(MAP& map)
{
    auto map_name = find(typeid(MAP).name());
    if (!map_name)
        return ;
    printf("%s map = %s\n", __FUNCTION__, map_name);

    MRNG rng(RND);

    size_t const num_iters = 50000;
    uint64_t result = 0;
    //Map<uint64_t, uint64_t> map;

    auto ts = now2ms();
    for (size_t n = 0; n < num_iters; ++n) {
        map[rng()] = n;
        for (auto & keyVal : map) {
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
    printf("    total iterate/removing time = %.2lf, %.2lf|%lu\n\n", (ts1 - ts) / 1000.0, (now2ms() - ts) /1000.0, result);
}

template<class MAP>
void bench_randomFind(MAP& bench, size_t numInserts, size_t numFindsPerInsert)
{
    auto map_name = find(typeid(MAP).name());
    if (!map_name)
        return;
    printf("\n%s map = %s\n", __FUNCTION__, map_name);

    static constexpr auto lower32bit = UINT64_C(0x00000000FFFFFFFF);
    static constexpr auto upper32bit = UINT64_C(0xFFFFFFFF00000000);
    //    static constexpr size_t numInserts = 2000;
    //    static constexpr size_t numFindsPerInsert = 500'000;

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

    printf("nums = %zd, total time = %.2lf\n", numInserts, ((int)(now2ms() - ts))/1000.0);
}

void runTest(int sflags, int eflags)
{
    const auto start = now2ms();

    if (sflags <= 1 && eflags >= 1)
    {
#if ABSL_HASH
        typedef absl::Hash<uint64_t> hash_func;
#elif STD_HASH
        typedef std::hash<uint64_t> hash_func;
#else
        typedef robin_hood::hash<uint64_t> hash_func;
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
#if ET
        //        { hrd7::hash_map <uint64_t, uint64_t, hash_func> hmap;  bench_IterateIntegers(hmap); }
        { tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_IterateIntegers(rmap); }
        { robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_IterateIntegers(martin); }
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_IterateIntegers(fmap); }
        { phmap::flat_hash_map<uint64_t, uint64_t, hash_func> pmap; bench_IterateIntegers(pmap); }
#endif
        { emilib::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
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
#elif WYHASH_LITTLE_ENDIAN
        typedef WysHasher hash_func;
#else
        typedef std::hash<std::string> hash_func;
#endif

        {emhash8::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
#if EM3
        {emhash2::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        {emhash3::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        {emhash4::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
#endif
        {emhash6::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        {emhash5::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        {emhash7::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        {emilib2::HashMap<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        {emilib::HashMap<std::string,  int, hash_func> bench; bench_randomFindString(bench); }
#if ET
        //        {hrd7::hash_map <std::string, size_t, hash_func> hmap;   bench_randomFindString(hmap); }
        {tsl::robin_map  <std::string, size_t, hash_func> bench;     bench_randomFindString(bench); }
        {robin_hood::unordered_map <std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        {ska::flat_hash_map<std::string, size_t, hash_func> bench;   bench_randomFindString(bench); }
        {phmap::flat_hash_map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
#if FOLLY
        {folly::F14VectorMap<std::string, size_t, hash_func> bench;  bench_randomFindString(bench); }
#endif
#if ABSL
        {absl::flat_hash_map<std::string, size_t, hash_func> bench;  bench_randomFindString(bench); }
#endif
        putchar('\n');
    }

    if (sflags <= 3 && eflags >= 3)
    {
#ifdef HOOD_HASH
        typedef robin_hood::hash<std::string> hash_func;
#elif ABSL_HASH
        typedef absl::Hash<std::string> hash_func;
#elif WYHASH_LITTLE_ENDIAN
        typedef WysHasher hash_func;
#else
        typedef std::hash<std::string> hash_func;
#endif

#if EM3
        {emhash4::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {emhash2::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {emhash3::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
        {emilib::HashMap<std::string,  int, hash_func> bench; bench_randomEraseString(bench); }
        {emilib2::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {emhash8::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {emhash7::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {emhash6::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {emhash5::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }

#if ET
        //        {hrd7::hash_map <std::string, int, hash_func> hmap;   bench_randomEraseString(hmap); }
        {tsl::robin_map  <std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {robin_hood::unordered_map <std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {ska::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        {phmap::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if FOLLY
        {folly::F14VectorMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if ABSL
        {absl::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
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

        static constexpr size_t numInserts[] = { 200, 2000, 500'000 };
        static constexpr size_t numFindsPerInsert[] = { 5'000'000, 500'000, 1000 };
        for (int i = 0; i < 3; i++)
        {
#if ET
            { tsl::robin_map  <size_t, size_t, hash_func> rmap;  bench_randomFind(rmap, numInserts[i], numFindsPerInsert[i]); }
            { robin_hood::unordered_map <size_t, size_t, hash_func> martin; bench_randomFind(martin, numInserts[i], numFindsPerInsert[i]); }
            { ska::flat_hash_map <size_t, size_t, hash_func> fmap; bench_randomFind(fmap, numInserts[i], numFindsPerInsert[i]); }
            { phmap::flat_hash_map <size_t, size_t, hash_func> pmap; bench_randomFind(pmap, numInserts[i], numFindsPerInsert[i]); }
            //        { hrd7::hash_map <size_t, size_t, hash_func> hmap;  bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif
            { emilib2::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emilib::HashMap<size_t, size_t, hash_func> emap;  bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
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
#elif STD_HASH
        typedef std::hash<int> hash_func;
#else
        typedef robin_hood::hash<int> hash_func;
#endif

#if ABSL //failed on other hash
        { absl::flat_hash_map <int, int, hash_func> pmap; bench_insert(pmap); }
#endif
#if FOLLY
        { folly::F14VectorMap <int, int, hash_func> pmap; bench_insert(pmap); }
#endif
        { emhash7::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash6::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash8::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash5::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#if EM3
        { emhash2::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash4::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash3::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#endif
        { emilib::HashMap<int, int, hash_func>  emap; bench_insert(emap); }
        { emilib2::HashMap<int, int, hash_func> emap; bench_insert(emap); }
#if ET
        //        { hrd7::hash_map <int, int, hash_func> hmap;  bench_insert(hmap); }
        { tsl::robin_map  <int, int, hash_func> rmap; bench_insert(rmap); }
        { robin_hood::unordered_map <int, int, hash_func> martin; bench_insert(martin); }
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_insert(fmap); }
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

        { emhash5::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash7::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash6::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash8::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
#if EM3
        { emhash2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash4::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
#endif

        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_randomInsertErase(emap); }
        { emilib::HashMap<uint64_t, uint64_t, hash_func> emap;  bench_randomInsertErase(emap); }
#if ET
        //        { hrd7::hash_map <size_t, size_t, hash_func> hmap; bench_randomInsertErase(hmap); }
        { tsl::robin_map     <uint64_t, uint64_t, hash_func> rmap; bench_randomInsertErase(rmap); }
        { robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_randomInsertErase(martin); }
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_randomInsertErase(fmap); }
        { phmap::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
#endif
#if ABSL
        { absl::flat_hash_map <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
#endif
#if FOLLY
        { folly::F14VectorMap <uint64_t, uint64_t, hash_func> pmap; bench_randomInsertErase(pmap); }
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
        { emhash6::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash5::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash7::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash8::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#if EM3
        { emhash2::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash4::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash3::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#endif

        { emilib::HashMap<int, int, hash_func> emap;  bench_randomDistinct2(emap); }
        { emilib2::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#if ET
        //        { hrd7::hash_map <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
        { tsl::robin_map     <int, int, hash_func> rmap; bench_randomDistinct2(rmap); }
        { robin_hood::unordered_map <int, int, hash_func> martin; bench_randomDistinct2(martin); }
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_randomDistinct2(fmap); }
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

    printf("total time = %.3lf s", (now2ms() - start) / 1000.0);
}

int main(int argc, char* argv[])
{
    puts("./test [23456mptseb0d2] n");
    srand(time(0));

    for (auto& m : show_name)
        printf("%10s %20s\n", m.first.c_str(), m.second.c_str());

#if TCPU
#if _WIN32
    SetThreadAffinityMask(GetCurrentThread(), 0x2);
#elif __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0x2, &cpuset);
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
#endif
#endif

    int sflags = 1, eflags = 7;
    if (argc > 1) {
        printf("cmd agrs = %s\n", argv[1]);
        for (char c = argv[1][0], i = 0; c != '\0'; c = argv[1][++i]) {
            if (c >= '2' && c < '9') {
                string map_name("emhash");
                map_name += c;
                if (show_name.count(map_name) == 1)
                    show_name.erase(map_name);
                else
                    show_name.emplace(map_name, map_name);
            } else if (c == 'm')
                show_name.erase("robin_hood");
            else if (c == 'p')
                show_name.erase("phmap");
            else if (c == 'a')
                show_name.erase("absl");
            else if (c == 't')
                show_name.erase("robin_map");
            else if (c == 's')
                show_name.erase("ska");
            else if (c == 'h')
                show_name.erase("hrd7");
            else if (c == 'e') {
                show_name.erase("emilib");
//                show_name.erase("emilib");
            } else if (c == 'b')
                sflags = atoi(&argv[1][++i]);
            else if (c == 'd')
                eflags = atoi(&argv[1][++i]);
        }
    }

    puts("test hash:");
    for (auto& m : show_name)
        printf("%10s %20s\n", m.first.c_str(), m.second.c_str());

    runTest(sflags, eflags);
    return 0;
}

