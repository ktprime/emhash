#include "util.h"
#include <sstream>

#ifndef _WIN32
  #include <sys/resource.h>
#endif

//#define EMH_ITER_SAFE 1
//#include "wyhash.h"

//#define EMH_STATIS 1
//#define ET            1
//#define EMH_WYHASH64   1
//#define HOOD_HASH     1
//
//#define EMH_PACK_TAIL 16
//#define EMH_HIGH_LOAD 123456

//#define EMH_STATIS 1234567

#if CK_HMAP
  #include "ck/Common/HashTable/HashMap.h"
#endif

#include "../hash_table8.hpp"
#include "../hash_table5.hpp"
#include "../hash_table7.hpp"
#include "../hash_table6.hpp"
//#include "../hash_set8.hpp"

//#include "../thirdparty/emhash/hash_table8v.hpp"
//#include "../thirdparty/emhash/hash_table8v2.hpp"

#ifdef HAVE_BOOST
  #include <boost/unordered/unordered_flat_map.hpp>
#endif

//https://github.com/gaujay/indivi_collection
#ifdef HAVE_INDIVI
  #include "indivi/flat_umap.h"
  #include "indivi/flat_wmap.h"
#endif

//    #define EMH_QUADRATIC 1
//    #define EMH_STATIS 123456
//    #define AVX2_EHASH 1
//    #define EMH_PSL_LINEAR 1
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2s.hpp"

#include "martin/robin_hood.h"

#if CXX17
  #include "martin/unordered_dense.h"
#endif

//#include "FHashTable/fhash_table.h"

#if ET
    #include "phmap/phmap.h"
    #include "tsl/robin_map.h"
#if X86_64
    #include "ska/flat_hash_map.hpp"
    #include "hrd/hash_set_m.h"
#endif
#endif

#if FOLLY_F14
  #include "folly/container/F14Map.h"
#endif

static const uint64_t RND =(uint64_t)getus();
static float max_lf = 0.875f;

static std::map<std::string_view, std::string_view> show_name =
{
   {"emhash7", "emhash7"},
//   {"emhash8", "emhash8"},
//   {"emhash5", "emhash5"},
//     {"emhash6", "emhash6"},

   {"emilib",  "emilib1"},
   {"emilib2", "emilib2"},
   {"emilib3", "emilib3"},

#if HAVE_BOOST
    {"boost",  "boost flat"},
#endif
#ifdef HAVE_INDIVI
    {"flat_u", "indivi_umap" },
    {"flat_w", "indivi_wmap" },
#endif
#if CK_HMAP
    {"HashMapCell",  "ck_hashmap"},
    {"HashMapTable", "ck_hashmap"},
#endif

//    {"fhash_table", "fht"},
//    {"ankerl", "martin dense"},

#if QC_HASH
    {"qc", "qchash"},
    {"fph", "fph"},
#endif

#if ABSL_HMAP
    {"absl", "absl flat"},
#endif
#if CXX20
    {"rigtorp", "rigtorp"},
    {"jg", "jg_dense"},
#endif
#if ET
    {"hrd_m", "hrdm"},
    {"phmap", "phmap flat"},
    {"robin_hood", "martin flat"},
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
    if (map_name.find("HashMapCell") < 30)
        return show_name.count("HashMapCell") ? show_name["HashMapCell"].data() : nullptr;
    if (map_name.find("HashMapTable") < 30)
        return show_name.count("HashMapTable") ? show_name["HashMapTable"].data() : nullptr;
    if (map_name.find("flat_u") < 30)
        return show_name.count("flat_u") ? show_name["flat_u"].data() : nullptr;
    if (map_name.find("flat_w") < 30)
        return show_name.count("flat_w") ? show_name["flat_w"].data() : nullptr;

    for (const auto& kv : show_name)
    {
        if (map_name.find(kv.first) < 10)
            return kv.second.data();
    }
    return nullptr;
}

#ifndef RT
    #define RT 2 //2 wyrand 1 sfc64 3 RomuDuoJr 4 Lehmer64 5 mt19937_64
#endif

#if RT == 1
    #define MRNG sfc64
#elif RT == 2
    #define MRNG WyRand
#elif RT == 3
    #define MRNG RomuDuoJr
#else
    #define MRNG Lehmer64
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
      m_c = rotl(m_c, (int)rotation) + tmp;
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
      T rotl(T const x, uint32_t k) {
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

static inline double now2sec()
{
#if _WIN32
    FILETIME ft;
#if _WIN32_WINNT >= 0x0602
    GetSystemTimePreciseAsFileTime(&ft);
#else
    GetSystemTimeAsFileTime(&ft);
#endif  /* Windows 8  */

    /* `t := (low + high * 0x1p32) / 10000`  */
    double t = (double) ft.dwLowDateTime + (double) ft.dwHighDateTime * 0x1p32;

    /* 11644473600000 is number of milliseconds from 1601-01-01T00:00:00Z
     * (the NT epoch) to 1970-01-01T00:00:00Z (the Unix Epoch).  */
    return (t / 10000'000 - 11644473600);
#elif __linux__
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return (double)sec + (double)usec / 1000000.0;
#elif __unix__
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec + start.tv_usec / 1000000.0;
#else
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count() / 1000000.0;
#endif
}

template<typename HMAP>
static void bench_insert(HMAP& hmap)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %s\n", map_name);

#if X86_64 || __MAC__
    uint32_t maxn = 1000000;
#else
    uint32_t maxn = 1000000 / 5;
#endif

    hmap.max_load_factor(max_lf);
    for (int  i = 0; i < 3; i++) {
        auto nows = now2sec();
        {
            auto RNDI = (uint64_t)RND + 15ull + (uint64_t)i;
            {
                if (RND % 2 == 0)
                hmap.reserve(maxn / 2);
                auto ts = now2sec();
                MRNG rng(RNDI);
                for (size_t n = 0; n < maxn; ++n)
                    hmap[static_cast<int>(rng())];
                printf("        (lf=%.2f) insert %.2f", hmap.load_factor(), now2sec() - ts);
                fflush(stdout);
            }
            {
                auto ts = now2sec();
                MRNG rng(RNDI);
                for (size_t n = 0; n < maxn * 9 / 10; ++n)
                    hmap.erase(static_cast<int>(rng()));
                printf(", remove 90%% %.2f", now2sec() - ts);
                fflush(stdout);
                assert(hmap.size() == 0);
            }
            {
                auto ts = now2sec();
                MRNG rng(RNDI + 1);
                for (size_t n = 0; n < maxn; ++n)
                    hmap.emplace(static_cast<int>(rng()), 0);
                printf(", reinsert %.2f", now2sec() - ts);
            }
            {
                auto ts = now2sec();
                hmap.clear();
                printf(", clear %.3f", now2sec() - ts);
            }
        }
        printf(" total %dM int time = %.2f s\n", int(maxn / 1000000), now2sec() - nows);
        maxn *= 10;
    }
}

template<typename HMAP, bool unique = false>
static void bench_AccidentallyQuadratic()
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    auto nows = now2sec();
    sfc64 rng(12345);

    HMAP hmap;
    for (size_t n = 0; n < 10'000'000; ++n) {
        hmap[static_cast<int>(rng())];
    }
    assert(9988513 == hmap.size());

    //bench.beginMeasure("iterate");
    uint64_t sum = 0;
    for (auto const& kv : hmap) {
        sum += (uint64_t)kv.first + (uint64_t)kv.second;
    }
    if (sum != UINT64_C(18446739465311920326))
        puts("error\n");

#if CXX17
//    bench.beginMeasure("iterate & copy");
    HMAP map2;
    for (auto const& kv : hmap) {
        if constexpr (unique)
            map2.insert_unique(kv.first, kv.second);
        else
            map2.emplace(kv.first, kv.second);
    }
    assert(hmap.size() == map2.size());
#endif
    printf(" time %.2f s\n", now2sec() - nows);
}

template<typename HMAP>
static void bench_InsertEraseBegin()
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %s", map_name);

    size_t max_n = 100000;
    auto nows = now2sec();

    for (uint64_t i = 0; i < 3; ++i) {
        auto starts = now2sec();
        HMAP hmap;
        MRNG rng(987654321ull + i * i * i);

        // benchmark randomly inserting & erasing begin
        for (size_t j = 0; j < max_n / 5; ++j)
            hmap.emplace((int64_t)rng(), 0);

        for (size_t j = 0; j < max_n; ++j) {
            hmap.erase(hmap.begin());
            hmap.emplace((int64_t)rng(), 0);
        }

        printf("\n        %.2lf cycles lf = %.2f mapsize = %d time %.2lf", ((double)max_n / 1000000.0), hmap.load_factor(), (int)hmap.size(), now2sec() - starts);
        max_n *= 5;
    }

    printf(" total (%.2f s)\n", now2sec() - nows);
}

template<typename HMAP>
static void bench_InsertEraseContinue()
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %s", map_name);

    size_t max_n = 400000;
    auto nows = now2sec();

    for (uint64_t i = 0; i < 3; ++i) {
        auto starts = now2sec();
        HMAP hmap;
        //hmap.reserve((std::size_t)(max_n * .));
        MRNG rng(2345ull + i * i * i);

        // benchmark randomly inserting & erasing begin
        for (size_t j = 0; j < max_n / 3; ++j)
            hmap.emplace((int)rng(), 0);

        auto key = hmap.begin()->first;
        for (size_t j = max_n; j > 0; j--) {
            auto it = hmap.find(key);
            if (it == hmap.end()) {
                it = hmap.begin();
                key = it->first;
            }

            if constexpr(std::is_void_v<decltype(hmap.erase(it))>) {
                hmap.erase(it);
                if (++it != hmap.end()) key = it->first;
            } else {
                it = hmap.erase(it);
                if (it != hmap.end()) key = it->first;
            }

            hmap.emplace((int)rng(), 0);
        }

        printf("\n        %.2lf cycles lf = %.2f mapsize = %d time %.2lf", ((double)max_n / 1000000.0), hmap.load_factor(), (int)hmap.size(), now2sec() - starts);
        max_n *= 7;
    }

    printf(" total (%.2f s)\n", now2sec() - nows);
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
static void rshuffle(RandomIt first, RandomIt last, URBG&& g)
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
static void iotas(ForwardIt first, ForwardIt last, T value)
{
    while(first != last) {
        *first++ = value;
        ++value;
    }
}

template<typename HMAP>
static void bench_randomInsertErase(HMAP& hmap)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    auto nows = now2sec();
	double erase1 = 0, erase2 = 0;

    if (1)
    {
        uint32_t min_n    = 1 << 20;
        uint32_t max_loop = min_n << 5;
        hmap.max_load_factor(max_lf);
        for (uint64_t j = 0; j < 5; ++j) {
            const uint64_t rndj = RND + 6 + j;
            MRNG rng(rndj);
            MRNG rng2(rndj);
            // each iteration, set 4 new random bits.
            // std::cout << (i + 1) << ". " << as_bits(bitMask) << std::endl;
            auto maxn = min_n * (50 + j * 9) / 100;
            for (size_t i = 0; i < maxn / 8; ++i) {
                hmap.emplace(rng(), 0);
            }

            //auto ts = now2sec();
            maxn = max_loop * 10 / (10 + 4*j);
            // benchmark randomly inserting & erasing
            for (size_t i = 0; i < maxn; ++i) {
                hmap.emplace(rng(), 0);
                hmap.erase(rng2());
            }
//            printf("    %8u %2d M cycles time %.3f s hmap size %8d loadf = %.2f\n",
//                    maxn, int(min_n / 1000000), now2sec() - ts, (int)hmap.size(), hmap.load_factor());
            min_n *= 2;
            hmap.clear();
        }

        erase1 = now2sec() - nows;
    }

    {
        HMAP map2;
        map2.max_load_factor(max_lf);
        std::vector<int> bits(64, 0);
        iotas(bits.begin(), bits.end(), 0);
        sfc64 rng(999);

#if 0
        for (auto &v : bits) v = rng();
#else
        rshuffle(bits.begin(), bits.end(), rng);
#endif

		nows = now2sec();
        uint64_t bitMask = 0;
        auto bitsIt = bits.begin();
        //size_t const expectedFinalSizes[] = {7, 127, 2084, 32722, 524149, 8367491};
        size_t const max_n = 20000000;

        for (int i = 0; i < 6; ++i) {
            for (int b = 0; b < 4; ++b) {
                bitMask |= UINT64_C(1) << *bitsIt++;
            }

//            auto ts = now2sec();
            for (size_t j = 0; j < max_n; ++j) {
                map2.emplace(rng() & bitMask, 0);
                map2.erase(rng() & bitMask);
            }
//            printf("    %02d bits  %2d M cycles time %.3f s hmap size %d loadf = %.2f\n",
//                    int(std::bitset<64>(bitMask).count()), int(max_n / 1000000), now2sec() - ts, (int)map2.size(), map2.load_factor());
        }
        erase2 = now2sec() - nows;
    }

    printf(" erase time = %.2lf:%.2lf, total = %.2lf s\n", erase1, erase2, erase1 + erase2);
}

template<typename HMAP>
static void bench_CreateInsert()
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    const std::array<size_t, 7> counts = {
        200, 2000, 2000, 20000, 200000, 2000000, 20000000
    };

    MRNG rng(213 + RND);
    auto nows = now2sec(), erase1 = 0.;
    for (size_t i = 0; i < counts.size(); ++i) {
        size_t count = counts[i];
        size_t repeats = counts.back() / count;
        size_t res = 0;
        HMAP hmap;

        for (size_t j = 0; j < repeats; ++j) {
            for (size_t n = 0; n < count; ++n)
                hmap[static_cast<int>(rng())];
            res += (size_t)hmap.size();
        }
    }
    erase1 = now2sec() - nows;
    nows = now2sec();

    MRNG rng2(213 + RND);
    for (size_t i = 0; i < counts.size(); ++i) {
        size_t count = counts[i];
        size_t repeats = counts.back() / count;
        size_t res = 0;
        HMAP hmap;

        for (size_t j = 0; j < repeats; ++j) {
            for (size_t n = 0; n < count; ++n)
                hmap[static_cast<int>(rng2())];
            res += (size_t)hmap.size();
            hmap.clear();
        }
    }

    auto erase2 = now2sec() - nows;
    printf(" CreateInsert/InsertCreate total time = %2.2f + %2.2f (%2.2f) s\n", erase1, erase2, erase1 + erase2);
}

static inline uint32_t udb_hash32(uint32_t key)
{
 #if 0
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
#else
    uint64_t x = key;
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return (uint32_t)x;
#endif
}

static inline uint32_t udb_get_key(const uint32_t n, const uint32_t x)
{
#if 0
    return udb_hash32(x % (n>>2));
#else
    return (uint32_t)((x % (n>>2)) * 0x45D9F3B);
#endif
}

static uint64_t splitmix64(uint64_t& x)
{
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

struct Hash32 {
    //using is_avalanching = void;
    inline size_t operator()(const uint32_t x) const {
        return x;
    }
};

const static uint32_t x0 = (uint32_t)getus();
const static bool is_del = (x0 % 2 == 0);

template<typename HMAP>
static void bench_udb3()
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    const auto nows = now2sec();
    constexpr uint32_t n_cp = 11, N = 80000000, n0 = 10000000;
    constexpr uint32_t step = (N - n0) / (n_cp - 1);

    HMAP h;
    uint64_t z = 0, x = x0;
    for (uint32_t j = 0, i = 0, n = n0; j < n_cp; ++j, n += step) {
        for (; i < n; ++i) {
            const uint64_t y = splitmix64(x);
            const uint32_t key = udb_get_key(n, (uint32_t)y);
            if (is_del) {
                auto p = h.emplace(key, i);
                if (!p.second) h.erase(p.first);
                z += p.second;
            } else {
                z += ++h[key];
            }
        }
    }

    printf(" z[%d] = %d lf = %.2f total time = %.2lf\n", is_del, (int)z, h.load_factor(), now2sec() - nows);
}

template<typename HMAP>
static void bench_randomDistinct2(HMAP& hmap)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

#if X86_64 || __MAC__
    constexpr size_t const n = 50000000;
#else
    constexpr size_t const n = 50000000 / 2;
#endif
    auto nows = now2sec();
    MRNG rng(RND + 786512);

    hmap.max_load_factor(max_lf);
    int checksum;
    {
        //auto ts = now2sec();
        checksum = 0;
        size_t const max_rng = n / 20;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng(max_rng))];
        }
//        printf("     05%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(), (int)hmap.size());
        assert(RND != 123 || 549985352 == checksum);
    }

    {
        hmap.clear();
        //auto ts = now2sec();
        checksum = 0;
        size_t const max_rng = n / 4;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng(max_rng))];
        }
//        printf("     25%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(), (int)hmap.size());
        assert(RND != 123 || 149979034 == checksum);
    }

    {
        hmap.clear();
        //auto ts = now2sec();
        size_t const max_rng = n / 2;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng(max_rng))];
        }
//        printf("     50%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(), (int)hmap.size());
        assert(RND != 123 || 249981806 == checksum);
    }

    {
        hmap.clear();
        //auto ts = now2sec();
        checksum = 0;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng())];
        }
//        printf("    100%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(), (int)hmap.size());
        assert(RND != 123 || 50291811 == checksum);
    }
    //#endif

    printf(" total time = %.2f s\n", now2sec() - nows);
}

#define CODE_FOR_NUCLEOTIDE(nucleotide) (" \0 \1\3  \2"[nucleotide & 0x7])

template<typename HMAP>
static size_t kcount(const std::vector<char> &poly, const std::string &oligo) {

    HMAP hmap;
    //hmap.max_load_factor(0.5);

    uint64_t key = 0;
    const uint64_t mask = ((uint64_t)1 << 2 * oligo.size()) - 1;

    // For the first several nucleotides we only need to append them to key in
    // preparation for the insertion of complete oligonucleotides to hmap.
    for (size_t i = 0; i < oligo.size() - 1; ++i)
        key = (key << 2 & mask) | (uint64_t)poly[i];

    // Add all the complete oligonucleotides of oligo.size() to
    // hmap and update the count for each oligonucleotide.
    for (size_t i = oligo.size() - 1; i < poly.size(); ++i){
        key= (key << 2 & mask) | (uint64_t)poly[i];
        ++hmap[key];
    }

    // Generate the key for oligonucleotide.
    key = 0;
    for (size_t i = 0; i < oligo.size(); ++i) {
        key = (key << 2) | (uint64_t)CODE_FOR_NUCLEOTIDE(oligo[i]);
    }

    if (oligo == "GGT")
        printf(" (lf=%.2f) ", hmap.load_factor());

    return hmap[key];
}

static int state = 42;
static inline int fasta_next() {
    static constexpr int IM = 139968, IA = 3877, IC = 29573;

    state = (state * IA + IC) % IM;
    float p = (float)state * (1.0f / IM);
    return (p >= 0.3029549426680f) + (p >= 0.5009432431601f) + (p >= 0.6984905497992f);
}

template<typename HMAP>
static void bench_knucleotide() {
    static constexpr size_t n = 25000000;

    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    HMAP hmap;
    state = 42;
    for (size_t i = 0; i < n * 3; ++i)
        (void)fasta_next();

    std::vector<char> poly(n * 5);
    for (size_t i = 0; i < poly.size(); ++i) {
        poly[i] = (char)fasta_next();
    }

    auto nows = now2sec();
    size_t ans = 0;
    ans += kcount<HMAP>(poly, "GGTATTTTAATTTATAGT");
    ans += kcount<HMAP>(poly, "GGTATTTTAATT");
    ans += kcount<HMAP>(poly, "GGTATT");
    ans += kcount<HMAP>(poly, "GGTA");
    ans += kcount<HMAP>(poly, "GGT");
    printf(" ans = %d time = %.2f s\n", (int)ans, now2sec() - nows);
}

class vec2 {
    uint32_t m_xy;

public:
    constexpr vec2(uint16_t x, uint16_t y)
        : m_xy{static_cast<uint32_t>(x) << 16U | y} {}

    constexpr explicit vec2(uint32_t xy)
        : m_xy(xy) {}

    [[nodiscard]] constexpr auto pack() const -> uint32_t {
        return m_xy;
    };

    [[nodiscard]] constexpr auto add_x(uint16_t x) const -> vec2 {
        return vec2{m_xy + (static_cast<uint32_t>(x) << 16U)};
    }

    [[nodiscard]] constexpr auto add_y(uint16_t y) const -> vec2 {
        return vec2{(m_xy & 0xffff0000) | ((m_xy + y) & 0xffff)};
    }

    template <typename Op>
    constexpr void for_each_surrounding(Op&& op) const {
        uint32_t v = m_xy;

        uint32_t upper = (v & 0xffff0000U);
        uint32_t l1 = (v - 1) & 0xffffU;
        uint32_t l2 = v & 0xffffU;
        uint32_t l3 = (v + 1) & 0xffffU;

        op((upper - 0x10000) | l1);
        op((upper - 0x10000) | l2);
        op((upper - 0x10000) | l3);

        op(upper | l1);
        // op(upper | l2);
        op(upper | l3);

        op((upper + 0x10000) | l1);
        op((upper + 0x10000) | l2);
        op((upper + 0x10000) | l3);
    }
};

#include "has_member.hpp"
template <typename HMAP>
static void game_of_life(const char* name, size_t nsteps, size_t finalPopulation, HMAP& map1, std::vector<vec2> state) {

    (void)name;
    (void)finalPopulation;
    map1.clear();
    auto map2 = map1;

    for (auto& v : state) {
        v = v.add_x(UINT16_MAX / 2).add_y(UINT16_MAX / 2);
        map1[v.pack()] = true;
        v.for_each_surrounding([&](uint32_t xy) { map1.emplace(xy, false); });
    }

    auto* m1 = &map1;
    auto* m2 = &map2;
    for (size_t i = 0; i < nsteps; ++i) {
        for (auto const& kv : *m1) {
            auto const& pos = kv.first;
            const auto alive = kv.second;
            int neighbors = 0;
            vec2{pos}.for_each_surrounding([&](uint32_t xy) {
                auto x = m1->find(xy);
                if (x != m1->end()) {
                    neighbors += x->second;
                }
            });
            if ((alive && neighbors == 2) || neighbors == 3) {
                (*m2)[pos] = true;
                vec2{pos}.for_each_surrounding([&](uint32_t xy) { m2->emplace(xy, false); });
            }
        }
        m1->clear();
        std::swap(m1, m2);
    }

    size_t count = 0;
    for (auto const& kv : *m1) {
        count += kv.second;
    }
    assert(finalPopulation ==count);
}

template<typename HMAP>
static void bench_GameOfLife()
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    HMAP hmap;
    auto stastabilizing = now2sec();
    {
        // https://conwaylife.com/wiki/R-pentomino
        game_of_life( "R-pentomino", 1103, 116, hmap, {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2}});

        // https://conwaylife.com/wiki/Acorn
        game_of_life( "Acorn", 5206, 633, hmap, {{1, 0}, {3, 1}, {0, 2}, {1, 2}, {4, 2}, {5, 2}, {6, 2}});

        // https://conwaylife.com/wiki/Jaydot
        game_of_life( "Jaydot", 6929, 1124, hmap, {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {1, 3}, {1, 4}, {2, 4}, {0, 5}});

        // https://conwaylife.com/wiki/Bunnies
        game_of_life( "Bunnies", 17332, 1744, hmap, {{0, 0}, {6, 0}, {2, 1}, {6, 1}, {2, 2}, {5, 2}, {7, 2}, {1, 3}, {3, 3}});

        printf(" stastabilizing = %.2f", now2sec() - stastabilizing);
    }

    auto grow = now2sec();
    {
        auto map1 = hmap;
        // https://conwaylife.com/wiki/Gotts_dots
        game_of_life( "Gotts dots", 2000, 4599, map1,
        {
                {0, 0},    {0, 1},    {0, 2},                                                                                 // 1
                {4, 11},   {5, 12},   {6, 13},   {7, 12},   {8, 11},                                                          // 2
                {9, 13},   {9, 14},   {9, 15},                                                                                // 3
                {185, 24}, {186, 25}, {186, 26}, {186, 27}, {185, 27}, {184, 27}, {183, 27}, {182, 26},                       // 4
                {179, 28}, {180, 29}, {181, 29}, {179, 30},                                                                   // 5
                {182, 32}, {183, 31}, {184, 31}, {185, 31}, {186, 31}, {186, 32}, {186, 33}, {185, 34},                       // 6
                {175, 35}, {176, 36}, {170, 37}, {176, 37}, {171, 38}, {172, 38}, {173, 38}, {174, 38}, {175, 38}, {176, 38}, // 7
        });

        // https://conwaylife.com/wiki/Puffer_2
        game_of_life( "Puffer 2", 2000, 7400, map1,
        {
                {1, 0}, {2, 0}, {3, 0},  {15, 0}, {16, 0}, {17, 0}, // line 0
                {0, 1}, {3, 1}, {14, 1}, {17, 1},                   // line 1
                {3, 2}, {8, 2}, {9, 2},  {10, 2}, {17, 2},          // line 2
                {3, 3}, {8, 3}, {11, 3}, {17, 3},                   // line 3
                {2, 4}, {7, 4}, {16, 4},                            // line 4
        });
    }

    printf(", grow = %.2f (total %.2f) s\n", now2sec() - grow, now2sec() - stastabilizing);
}

template<typename HMAP>
static void bench_copy(HMAP&)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    size_t result = 0;
    sfc64 rng(987);

    constexpr int KN = 1000;
    constexpr int KL = 1000;
    HMAP mapSource(1'000);
    mapSource.max_load_factor(max_lf);
    uint64_t rememberKey = 0;
    for (size_t i = 0; i < 200'000; ++i) {
        auto key = rng();
        if (i == 100'000) {
            rememberKey = key;
        }
        mapSource[key] = (int)i;
    }

    auto nows = now2sec();
    HMAP mapForCopy = mapSource;
    for (int n = 0; n < KL; ++n) {
        HMAP m = mapForCopy;
        result += (size_t)m.size() + (size_t)m[rememberKey];
        for (int i = 0; i < KN; i++) //with different load factor
            mapForCopy[rng()] = (int)rng();
    }
//    assert(result == 300019900);
    auto copyt = now2sec();
    printf(" copy = %.2f", copyt - nows);

    mapForCopy = mapSource;
    HMAP m;
    for (int n = 0; n < KL; ++n) {
        m = mapForCopy;
        result += (size_t)m.size() + (size_t)m[rememberKey];
        for (int i = 0; i < KN; i++)
            mapForCopy[rng()] = (int)rng();
    }
//    assert(result == 600039800);
    printf(", assign time = %.2f s, result = %zu\n", now2sec() - copyt, result);
}

template<typename HMAP>
static size_t runInsertEraseString(size_t max_n, size_t string_length, uint32_t bitMask)
{
    //printf("%s hmap = %s\n", __FUNCTION__, typeid(HMAP).name());
    MRNG rng(RND + 4);

    // time measured part
    size_t verifier = 0;
    std::stringstream ss;
    ss << string_length << " bytes" << std::dec;

    std::string str(string_length, 'x');
    // point to the back of the string (32bit aligned), so comparison takes a while
    auto const idx32 = (string_length / 4) - 1;
    auto const strData32 = reinterpret_cast<uint32_t*>(&str[0]) + idx32;

    HMAP hmap;
    hmap.max_load_factor(max_lf);

//    auto ts = now2sec();
    for (size_t i = 0; i < max_n; ++i) {
        *strData32 = uint32_t(rng() & bitMask);
#if 0
        // create an entry.
        hmap[str] = 0;
        *strData32 = rng() & bitMask;
        auto it = hmap.find(str);
        if (it != hmap.end()) {
            ++verifier;
            hmap.erase(it);
        }
#else
        hmap.emplace(str, 0);
        *strData32 = uint32_t(rng() & bitMask);
        verifier += (size_t)hmap.erase(str);
#endif
    }

//    printf("%4zd bytes time = %.2f, loadf = %.2f %d\n", string_length, now2sec() - ts, hmap.load_factor(), (int)hmap.size());
    return verifier;
}

template<typename HMAP>
static uint64_t randomFindInternalString(size_t numRandom, size_t const length, size_t numInserts, size_t numFindsPerInsert)
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
    HMAP hmap;
    hmap.max_load_factor(max_lf);
    {
        size_t i = 0;
        size_t findCount = 0;

        do {
            // insert NumTotal entries: some random, some sequential.
            std::shuffle(insertRandom.begin(), insertRandom.end(), rng);
            for (bool isRandomToInsert : insertRandom) {
                auto val = anotherUnrelatedRng();
                if (isRandomToInsert) {
                    *strData32 = (uint32_t)rng();
                } else {
                    *strData32 = uint32_t(val);
                }
                hmap[str] = 1;
                ++i;
            }

            // the actual benchmark code which sohould be as fast as possible
            for (size_t j = 0; j < numFindsPerIter; ++j) {
                if (++findCount > i) {
                    findCount = 0;
                    findRng.state(anotherUnrelatedRngInitialState);
                }
                *strData32 = (uint32_t)findRng();
                auto it = hmap.find(str);
                if (it != hmap.end()) {
#ifndef CK_HMAP
                    num_found += it->second;
#else
                    num_found += 1;
#endif
                }
            }
        } while (i < numInserts);
    }

    if (hmap.size() > 12)
      printf("        %s time = %.2f s %8d loadf = %.2f\n",
            title.c_str(), now2sec() - ts, (int)num_found, hmap.load_factor());
    return num_found;
}

template<typename HMAP>
static void bench_randomFindString(HMAP&)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %8s\n", map_name);

    auto nows = now2sec();
    auto now1 = nows, now2 = nows;
    if (1)
    {
        static constexpr size_t numInserts = 1000000 / 2;
        static constexpr size_t numFindsPerInsert = 200 / 2;

        randomFindInternalString<HMAP>(4, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(3, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(2, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(1, 13, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(0, 13, numInserts, numFindsPerInsert);
        now1 = now2sec();
    }

    {
        static constexpr size_t numInserts = 100000;
        static constexpr size_t numFindsPerInsert = 1000;

        randomFindInternalString<HMAP>(4, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(3, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(2, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(1, 100, numInserts, numFindsPerInsert);
        randomFindInternalString<HMAP>(0, 100, numInserts, numFindsPerInsert);
        now2 = now2sec();
    }
    printf("total time = %.2f + %.2f = %.2f s\n", now1 - nows, now2 - now1, now2 - nows);
}

template<typename HMAP>
static void bench_randomEraseString(HMAP&)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    auto nows = now2sec();
    { runInsertEraseString<HMAP>(20000000, 7, 0xfffff); }
    { runInsertEraseString<HMAP>(20000000, 8, 0xfffff); }
    { runInsertEraseString<HMAP>(20000000, 13, 0xfffff); }
    { runInsertEraseString<HMAP>(10000000, 24, 0xfffff); }
    { runInsertEraseString<HMAP>(12000000, 100, 0x4ffff); }
    { runInsertEraseString<HMAP>(8000000,  200, 0x3ffff); }
    { runInsertEraseString<HMAP>(6000000,  1000,0x7ffff); }

    printf(" total time = %.2f s\n", now2sec() - nows);
}

template<typename HMAP>
static uint64_t randomFindInternal(size_t numRandom, uint64_t bitMask, const size_t numInserts, const size_t numFindsPerInsert) {
    size_t constexpr NumTotal = 4;
    size_t const numSequential = NumTotal - numRandom;

    size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

    sfc64 rng(RND + 2);

    size_t num_found = 0;
    HMAP hmap;
    hmap.max_load_factor(max_lf);
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
                const auto val = anotherUnrelatedRng();
                if (isRandomToInsert) {
                    hmap[rng() & bitMask] = static_cast<size_t>(1);
                } else {
                    hmap[val & bitMask] = static_cast<size_t>(1);
                }
            }
            i += insertRandom.size();

            // the actual benchmark code which sohould be as fast as possible
            for (size_t j = 0; j < numFindsPerIter; ++j) {
                if (++findCount > i) {
                    findCount = 0;
                    findRng.state(anotherUnrelatedRngInitialState);
                }
                num_found += (size_t)hmap.count(findRng() & bitMask);
            }
        } while (i < numInserts);
    }

    if (hmap.size() == 0) {
        printf("    %3u%% %016x time = %.2f s, %8d loadf = %.2f\n",
            uint32_t(numSequential * 100 / NumTotal), (int)bitMask, now2sec() - ts, (int)num_found, hmap.load_factor());
    }

    return (uint64_t)hmap.size();
}

template<typename HMAP>
static void bench_IterateIntegers(HMAP& hmap)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    size_t const num_iters = 50000;
    uint64_t result = 0;
    auto ts = now2sec();

    {
        MRNG rng(123);
        for (size_t n = 0; n < num_iters; ++n) {
            hmap[rng()] = n;
            for (const auto & keyVal : hmap)
#ifndef CK_HMAP
                result += keyVal.second;
#else
                result += 1;
#endif
        }
        assert(result == 20833333325000ull);
    }

    auto ts1 = now2sec();
    {
        MRNG rng(123);
        for (size_t n = 0; n < num_iters; ++n) {
            hmap.erase(rng());
            for (auto const& keyVal : hmap)
#ifndef CK_HMAP
                result += keyVal.second;
#else
                result += 1;
#endif
        }
    }
    assert(result == 62498750000000ull + 20833333325000ull);
    printf(", add/removing time = %.2f, %.2f|%d\n", (ts1 - ts), now2sec() - ts1, (int)result);
}

template<typename HMAP>
static void bench_randomFind(HMAP&, size_t numInserts, size_t numFindsPerInsert)
{
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s", map_name);

    static constexpr auto lower32bit = UINT64_C(0x00000000FFFFFFFF);
    static constexpr auto upper32bit = UINT64_C(0xFFFFFFFF00000000);
    static constexpr auto mediu32bit = UINT64_C(0x0000FFFFFFFF0000);

    auto ts = now2sec();
    uint64_t sum = 0;

    sum += randomFindInternal<HMAP>(4, lower32bit, numInserts, numFindsPerInsert);
    sum += randomFindInternal<HMAP>(3, upper32bit, numInserts, numFindsPerInsert);
    sum += randomFindInternal<HMAP>(2, mediu32bit, numInserts, numFindsPerInsert);

    sum += randomFindInternal<HMAP>(1, upper32bit, numInserts, numFindsPerInsert);
    sum += randomFindInternal<HMAP>(0, lower32bit, numInserts, numFindsPerInsert);

    if (sum != 123)
    printf(" nums = %zd total time = %.2f s\n", numInserts, now2sec() - ts);
}

static void runTest(int sflags, int eflags)
{
    const auto start = now2sec();

    if (sflags <= 1 && eflags >= 1)
    {
#if ABSL_HASH
        typedef absl::Hash<uint64_t> hash_func;
#elif STD_HASH
        typedef std::hash<uint64_t> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<uint64_t> hash_func;
#else
        using hash_func = ankerl::unordered_dense::hash<uint64_t>;
#endif

        puts("\nbench_IterateIntegers:");

#if QC_HASH
        { qc::hash::RawMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { fph::DynamicFphMap<uint64_t, uint64_t, fph::MixSeedHash<uint64_t>> emap; bench_IterateIntegers(emap); }
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
        { tsl::robin_map <uint64_t, uint64_t, hash_func> rmap; bench_IterateIntegers(rmap); }
        { robin_hood::unordered_map <uint64_t, uint64_t, hash_func> martin; bench_IterateIntegers(martin); }

#if X86_64
        { hrd_m::hash_map <uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
        { ska::flat_hash_map <uint64_t, uint64_t, hash_func> fmap; bench_IterateIntegers(fmap); }
#endif
        { phmap::flat_hash_map<uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
#endif

        { emilib::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emilib3::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }
        { emilib2::HashMap<uint64_t, uint64_t, hash_func> emap; bench_IterateIntegers(emap); }

#if HAVE_BOOST
        { boost::unordered_flat_map<uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
#endif

#if HAVE_INDIVI
        { indivi::flat_umap<uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
        { indivi::flat_wmap<uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
#endif

#if CK_HMAP
        { ck::HashMap<uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
#endif

#if ABSL_HMAP
        { absl::flat_hash_map<uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
#endif
#if FOLLY_F14
        { folly::F14VectorMap<uint64_t, uint64_t, hash_func> hmap; bench_IterateIntegers(hmap); }
#endif
    }

    if (sflags <= 2 && eflags >= 2)
    {
#ifdef HOOD_HASH
        typedef robin_hood::hash<std::string> hash_func;
#elif ABSL_HASH
        typedef absl::Hash<std::string> hash_func;
#elif A_HASH
        using hash_func = Axxhasher<std::string>;
#elif ANKERL_HASH
        using hash_func = ankerl::unordered_dense::hash<std::string>;
#elif STD_HASH
        typedef std::hash<std::string> hash_func;
#else
        typedef WysHasher hash_func;
#endif
        puts("\nbench_randomFindString:");

#if CXX17
        { ankerl::unordered_dense::map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif

        { emhash8::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emhash6::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emhash5::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#if QC_HASH > 1
        { fph::DynamicFphMap<std::string, size_t, fph::MixSeedHash<std::string>> bench; bench_randomFindString(bench); }
#endif
        { emhash7::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }

        { emilib3::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emilib2::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { emilib::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }

#if HAVE_BOOST
        { boost::unordered_flat_map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif

#if HAVE_INDIVI
        { indivi::flat_umap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { indivi::flat_wmap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif

#if CK_HMAP //crash TODO
//        { ck::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif

#if ET
        { tsl::robin_map <std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
        { robin_hood::unordered_map <std::string, size_t, hash_func> bench; bench_randomFindString(bench); }

#if X86_64
        { hrd_m::hash_map <std::string, size_t, hash_func> hmap; bench_randomFindString(hmap); }
        { ska::flat_hash_map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
        { phmap::flat_hash_map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
#if CXX20
        { jg::dense_hash_map<std::string, int, hash_func> bench; bench_randomFindString(bench); }
        { rigtorp::HashMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
#if FOLLY_F14
        { folly::F14VectorMap<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
#if ABSL_HMAP
        { absl::flat_hash_map<std::string, size_t, hash_func> bench; bench_randomFindString(bench); }
#endif
    }

    if (sflags <= 3 && eflags >= 3)
    {
#ifdef HOOD_HASH
        typedef robin_hood::hash<std::string> hash_func;
#elif ABSL_HASH
        typedef absl::Hash<std::string> hash_func;
#elif A_HASH
        using hash_func = Axxhasher<std::string>;
#elif STD_HASH
        typedef std::hash<std::string> hash_func;
#elif ANKERL_HASH
        using hash_func = ankerl::unordered_dense::hash<std::string>;
#else
        typedef WysHasher hash_func;
#endif
        puts("\nbench_randomEraseString:");

        { emilib2::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { emilib::HashMap<std::string,  int, hash_func> bench; bench_randomEraseString(bench); }
        { emilib3::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }

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

#if QC_HASH > 1
        { fph::DynamicFphMap<std::string, int, fph::MixSeedHash<std::string>> bench; bench_randomEraseString(bench); }
#endif

#if ET
        { tsl::robin_map <std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { robin_hood::unordered_map <std::string, int, hash_func> bench; bench_randomEraseString(bench); }

#if X86_64
        { hrd_m::hash_map <std::string, int, hash_func> hmap; bench_randomEraseString(hmap); }
        { ska::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
        { phmap::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if FOLLY_F14
        { folly::F14VectorMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if ABSL_HMAP
        { absl::flat_hash_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
#if HAVE_BOOST
        { boost::unordered_flat_map<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif

#if HAVE_INDIVI
        { indivi::flat_umap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
        { indivi::flat_wmap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif

#if CK_HMAP //crash TODO
//        { ck::HashMap<std::string, int, hash_func> bench; bench_randomEraseString(bench); }
#endif
    }

    if (sflags <= 4 && eflags >= 4)
    {
#if STD_HASH
        typedef std::hash<uint64_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<uint64_t> hash_func;
#elif ANKERL_HASH
        using hash_func = ankerl::unordered_dense::hash<uint64_t>;
#else
        typedef std::hash<uint64_t> hash_func;
#endif
        puts("\nbench_copy:");

        { emhash6::HashMap<uint64_t, int, hash_func> emap; bench_copy(emap); }
        { emhash5::HashMap<uint64_t, int, hash_func> emap; bench_copy(emap); }
        { emhash7::HashMap<uint64_t, int, hash_func> emap; bench_copy(emap); }
        { emhash8::HashMap<uint64_t, int, hash_func> emap; bench_copy(emap); }

#if QC_HASH
        { qc::hash::RawMap<uint64_t, int, hash_func> emap; bench_copy(emap); }
#endif

#if CXX20
        { jg::dense_hash_map<uint64_t, int, hash_func> emap; bench_copy(emap); }
        { rigtorp::HashMap<uint64_t, int, hash_func> emap; bench_copy(emap); }
#endif
#if CXX17
        { ankerl::unordered_dense::map <uint64_t, int, hash_func> martin; bench_copy(martin); }
#endif

        { emilib::HashMap<uint64_t, int, hash_func> emap;  bench_copy(emap); }
        { emilib2::HashMap<uint64_t, int, hash_func> emap; bench_copy(emap); }
        { emilib3::HashMap<uint64_t, int, hash_func> emap; bench_copy(emap); }

#if ET
        { tsl::robin_map     <uint64_t, int, hash_func> rmap; bench_copy(rmap); }
        { robin_hood::unordered_map <uint64_t, int, hash_func> martin; bench_copy(martin); }

#if X86_64
        { ska::flat_hash_map <uint64_t, int, hash_func> fmap; bench_copy(fmap); }
#endif
        { phmap::flat_hash_map <uint64_t, int, hash_func> hmap; bench_copy(hmap); }
#endif
#if ABSL_HMAP
        { absl::flat_hash_map <uint64_t, int, hash_func> hmap; bench_copy(hmap); }
#endif
#if HAVE_BOOST
        { boost::unordered_flat_map <uint64_t, int, hash_func> hmap; bench_copy(hmap); }
#endif

#if HAVE_INDIVI
        { indivi::flat_umap <uint64_t, int, hash_func> hmap; bench_copy(hmap); }
        { indivi::flat_wmap <uint64_t, int, hash_func> hmap; bench_copy(hmap); }
#endif

#if CK_HMAP
//        { ck::HashMap <uint64_t, int, hash_func> hmap; bench_copy(hmap); }
#endif

#if FOLLY_F14
        { folly::F14VectorMap <uint64_t, int, hash_func> hmap; bench_copy(hmap); }
#endif
    }

    if (sflags <= 5 && eflags >= 5)
    {
#if ABSL_HASH
        typedef absl::Hash<size_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<size_t> hash_func;
#elif STD_HASH
        typedef std::hash<size_t> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<size_t> hash_func;
#else
        using hash_func = ankerl::unordered_dense::hash<size_t>;
#endif
        puts("\nbench_randomFind:");

        MRNG rng((size_t)time(0));
        const size_t numInserts[] = {
            (size_t)rng() % 123 + 123,
            (size_t)rng() % 1234 + 1234,
            (size_t)rng() % 12345 + 12345,
            (size_t)rng() % 123456 + 123456,
            (size_t)rng() % 1234567 + 1234567,
            (size_t)rng() % 12345678 + 12345678};
        constexpr size_t numFindsPerInsert[] = {800000, 50000, 5000, 200, 10, 2};

        for (size_t i = 0; i < size_t(sizeof(numInserts) / sizeof(numInserts[0])); i++)
        {
#if ET
            { tsl::robin_map <size_t, size_t, hash_func> rmap; bench_randomFind(rmap, numInserts[i], numFindsPerInsert[i]); }
            { robin_hood::unordered_map <size_t, size_t, hash_func> martin; bench_randomFind(martin, numInserts[i], numFindsPerInsert[i]); }

#if X86_64
            { ska::flat_hash_map <size_t, size_t, hash_func> fmap; bench_randomFind(fmap, numInserts[i], numFindsPerInsert[i]); }
            { hrd_m::hash_map <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif

            { phmap::flat_hash_map <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if QC_HASH
#if QC_HASH > 1
            { fph::DynamicFphMap<size_t, size_t, fph::MixSeedHash<size_t>> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#endif
            { qc::hash::RawMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if CXX20
            { jg::dense_hash_map<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { rigtorp::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#endif

            { emilib3::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emilib::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emilib2::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }

#if ABSL_HMAP
            { absl::flat_hash_map <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if HAVE_BOOST
            { boost::unordered_flat_map <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif
#if HAVE_INDIVI
            { indivi::flat_umap <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
            { indivi::flat_wmap <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif

#if CK_HMAP
            { ck::HashMap <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif

#if CXX17
            { ankerl::unordered_dense::map<size_t, size_t, hash_func> martin; bench_randomFind(martin, numInserts[i], numFindsPerInsert[i]); }
#endif
            { emhash8::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emhash5::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emhash6::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
            { emhash7::HashMap<size_t, size_t, hash_func> emap; bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]); }
#if FOLLY_F14
            { folly::F14VectorMap <size_t, size_t, hash_func> hmap; bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]); }
#endif
            putchar('\n');
        }
    }

    if (sflags <= 6 && eflags >= 6)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif ANKERL_HASH
        using hash_func = ankerl::unordered_dense::hash<int>;
#elif HOOD_HASH
        typedef robin_hood::hash<int> hash_func;
#else
        typedef std::hash<int> hash_func;
#endif

        puts("\nbench_insert:");

#if ABSL_HMAP
        { absl::flat_hash_map <int, int, hash_func> amap; bench_insert(amap); }
#endif
#if CK_HMAP
        { ck::HashMap <int, int, hash_func> amap; bench_insert(amap); }
#endif
#if FOLLY_F14
        { folly::F14VectorMap <int, int, hash_func> hmap; bench_insert(hmap); }
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
        { emhash8::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash6::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emhash5::HashMap<int, int, hash_func> emap; bench_insert(emap); }

        { emilib::HashMap<int, int, hash_func>  emap; bench_insert(emap); }
#if HAVE_BOOST
        { boost::unordered_flat_map <int, int, hash_func> amap; bench_insert(amap); }
#endif

#if HAVE_INDIVI
        { indivi::flat_umap <int, int, hash_func> amap; bench_insert(amap); }
        { indivi::flat_wmap <int, int, hash_func> amap; bench_insert(amap); }
#endif

        { emilib2::HashMap<int, int, hash_func> emap; bench_insert(emap); }
        { emilib3::HashMap<int, int, hash_func> emap; bench_insert(emap); }

#if ET
        { tsl::robin_map <int, int, hash_func> rmap; bench_insert(rmap); }
        { robin_hood::unordered_map <int, int, hash_func> martin; bench_insert(martin); }

#if X86_64
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_insert(fmap); }
        { hrd_m::hash_map <int, int, hash_func> hmap; bench_insert(hmap); }
#endif
        { phmap::flat_hash_map <int, int, hash_func> hmap; bench_insert(hmap); }
#endif
    }

    if (sflags <= 7 && eflags >= 7)
    {
#if ABSL_HASH
        typedef absl::Hash<uint64_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<uint64_t> hash_func;
#elif STD_HASH
        typedef std::hash<uint64_t> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<uint64_t> hash_func;
#else
        using hash_func = ankerl::unordered_dense::hash<uint64_t>;
#endif

        puts("\nbench_randomInsertErase:");


        { emilib2::HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }
        { emilib3::HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }
        { emilib:: HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }

#if HAVE_BOOST
        { boost::unordered_flat_map <uint64_t, int, hash_func> hmap; bench_randomInsertErase(hmap); }
#endif

#if HAVE_INDIVI
        { indivi::flat_umap <uint64_t, int, hash_func> hmap; bench_randomInsertErase(hmap); }
        { indivi::flat_wmap <uint64_t, int, hash_func> hmap; bench_randomInsertErase(hmap); }
#endif

#if CK_HMAP
        { ck::HashMap <uint64_t, int, hash_func> hmap; bench_randomInsertErase(hmap); }
#endif

#if ABSL_HMAP
        { absl::flat_hash_map <uint64_t, int, hash_func> hmap; bench_randomInsertErase(hmap); }
#endif
        { emhash8::HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash5::HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash7::HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }
        { emhash6::HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }

#if QC_HASH
        { qc::hash::RawMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); } //hang
#if QC_HASH > 1
        { fph::DynamicFphMap<uint64_t, int, fph::MixSeedHash<uint64_t>> emap; bench_randomInsertErase(emap); }
#endif
#endif

#if ET
        { tsl::robin_map <uint64_t, int, hash_func> rmap; bench_randomInsertErase(rmap); }
        { robin_hood::unordered_map <uint64_t, int, hash_func> martin; bench_randomInsertErase(martin); }

#if X86_64
        { ska::flat_hash_map <uint64_t, int, hash_func> fmap; bench_randomInsertErase(fmap); }
        { hrd_m::hash_map <size_t, size_t, hash_func> hmap; bench_randomInsertErase(hmap); }
#endif
        { phmap::flat_hash_map <uint64_t, int, hash_func> hmap; bench_randomInsertErase(hmap); }
#endif
#if FOLLY_F14
        { folly::F14VectorMap <uint64_t, int, hash_func> hmap; bench_randomInsertErase(hmap); }
#endif
#if CXX17
        { ankerl::unordered_dense::map<uint64_t, int, hash_func> martin; bench_randomInsertErase(martin); }
#endif
#if CXX20
        { jg::dense_hash_map<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); }
        //{ rigtorp::HashMap<uint64_t, int, hash_func> emap; bench_randomInsertErase(emap); } //hange
#endif
    }

    if (sflags <= 8 && eflags >= 8)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif STD_HASH
        typedef std::hash<int> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<int> hash_func;
#else
        using hash_func = ankerl::unordered_dense::hash<int>;
#endif

        puts("\nbench_randomDistinct2:");
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

        { emhash8::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash6::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash5::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emhash7::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }

        { emilib::HashMap<int, int, hash_func> emap;  bench_randomDistinct2(emap); }
        { emilib2::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
        { emilib3::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }

#if ET
        { tsl::robin_map <int, int, hash_func> rmap; bench_randomDistinct2(rmap); }
        { robin_hood::unordered_map <int, int, hash_func> martin; bench_randomDistinct2(martin); }

#if X86_64
        { ska::flat_hash_map <int, int, hash_func> fmap; bench_randomDistinct2(fmap); }
        { hrd_m::hash_map <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
#endif
        { phmap::flat_hash_map <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
#endif
#if HAVE_BOOST
        { boost::unordered_flat_map <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
#endif

#if HAVE_INDIVI
        { indivi::flat_umap <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
        { indivi::flat_wmap <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
#endif

#if CK_HMAP
        { ck::HashMap<int, int, hash_func> emap; bench_randomDistinct2(emap); }
#endif
#if ABSL_HMAP
        { absl::flat_hash_map <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
#endif

#if FOLLY_F14
        { folly::F14VectorMap <int, int, hash_func> hmap; bench_randomDistinct2(hmap); }
#endif
    }

    if (sflags <= 9 && eflags >= 9)
    {
#if ABSL_HASH
        typedef absl::Hash<uint64_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<uint64_t> hash_func;
#elif ANKERL_HASH
        typedef ankerl::unordered_dense::hash<uint64_t> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<uint64_t> hash_func;
#else
        typedef std::hash<uint64_t> hash_func;
#endif
        puts("\nbench_knucleotide:");

#if QC_HASH
        { bench_knucleotide<qc::hash::RawMap<uint64_t, uint32_t, hash_func>>(); }
#endif

#if CXX20
        { bench_knucleotide<jg::dense_hash_map<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<rigtorp::HashMap<uint64_t, uint32_t, hash_func>>(); }
#endif
#if CXX17
        { bench_knucleotide<ankerl::unordered_dense::map <uint64_t, uint32_t, hash_func>>(); }
#endif
#if HAVE_BOOST
        { bench_knucleotide<boost::unordered_flat_map <uint64_t, uint32_t, hash_func>>(); }
#endif

#if HAVE_INDIVI
        { bench_knucleotide<indivi::flat_umap <uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<indivi::flat_wmap <uint64_t, uint32_t, hash_func>>(); }
#endif

#if ABSL_HMAP
        { bench_knucleotide<absl::flat_hash_map <uint64_t, uint32_t, hash_func>>(); }
#endif

        { bench_knucleotide<emhash6::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emhash5::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emhash7::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emhash8::HashMap<uint64_t, uint32_t, hash_func>>(); }


        { bench_knucleotide<emilib::HashMap <uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emilib2::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emilib3::HashMap<uint64_t, uint32_t, hash_func>>(); }

#if ET
        { bench_knucleotide<tsl::robin_map <uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<robin_hood::unordered_map <uint64_t, uint32_t, hash_func>>(); }

#if X86_64
        { bench_knucleotide<ska::flat_hash_map <uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<hrd_m::hash_map <uint64_t, uint32_t, hash_func>>(); }
#endif
        { bench_knucleotide< phmap::flat_hash_map <uint64_t, uint32_t, hash_func>>(); }
#endif

#if FOLLY
        { bench_knucleotide<folly::F14VectorMap <uint64_t, uint32_t, hash_func>>(); }
#endif
#if CK_HMAP
        { bench_knucleotide<ck::HashMap <uint64_t, uint32_t, hash_func>>(); }
#endif
    }

    if (sflags <= 10 && eflags >= 10)
    {
#if ABSL_HASH
        typedef absl::Hash<uint32_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<uint32_t> hash_func;
#elif STD_HASH
        typedef std::hash<uint32_t> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<uint32_t> hash_func;
#else
        typedef ankerl::unordered_dense::hash<uint32_t> hash_func;
#endif
        puts("\nbench_GameOfLife:");

        { bench_GameOfLife<emhash6::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emhash5::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emhash7::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emhash8::HashMap<uint32_t, bool, hash_func>>(); }

#if QC_HASH
        { bench_GameOfLife<qc::hash::RawMap<uint32_t, bool, hash_func>>(); }
#endif

#if CXX20
        { bench_GameOfLife<jg::dense_hash_map<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<rigtorp::HashMap<uint32_t, bool, hash_func>>(); }
#endif
#if CXX17
        { bench_GameOfLife<ankerl::unordered_dense::map <uint32_t, bool, hash_func>>(); }
#endif
#if HAVE_BOOST
        { bench_GameOfLife<boost::unordered_flat_map <uint32_t, bool, hash_func>>(); }
#endif
#if HAVE_INDIVI
        { bench_GameOfLife<indivi::flat_umap <uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<indivi::flat_wmap <uint32_t, bool, hash_func>>(); }
#endif
#if ABSL_HMAP
        { bench_GameOfLife<absl::flat_hash_map <uint32_t, bool, hash_func>>(); }
#endif


        { bench_GameOfLife<emilib::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emilib2::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emilib3::HashMap<uint32_t, bool, hash_func>>(); }

#if ET
        { bench_GameOfLife<tsl::robin_map <uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<robin_hood::unordered_map <uint32_t, bool, hash_func>>(); }

#if X86_64
        { bench_GameOfLife<ska::flat_hash_map <uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<hrd_m::hash_map <uint32_t, bool, hash_func>>(); }
#endif
        { bench_GameOfLife<phmap::flat_hash_map <uint32_t, bool, hash_func>>(); }
#endif

#if FOLLY
        { bench_GameOfLife<folly::F14VectorMap <uint32_t, bool, hash_func>>(); }
#endif
#if CK_HMAP
        { bench_GameOfLife<ck::HashMap <uint32_t, bool, hash_func>>(); }
#endif
    }

    if (sflags <= 11 && eflags >= 11)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif STD_HASH
        typedef std::hash<int> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<int> hash_func;
#else
        typedef ankerl::unordered_dense::hash<int> hash_func;
#endif

        puts("\nbench_AccidentallyQuadratic (10M int insert.copy.iterator):");

        {  bench_AccidentallyQuadratic<emhash6::HashMap<int, int, hash_func>, false>(); }
        {  bench_AccidentallyQuadratic<emhash7::HashMap<int, int, hash_func>, false>(); }
        {  bench_AccidentallyQuadratic<emhash5::HashMap<int, int, hash_func>, false>(); }
        {  bench_AccidentallyQuadratic<emhash8::HashMap<int, int, hash_func>, false>(); }

#if QC_HASH
        {  bench_AccidentallyQuadratic<qc::hash::RawMap<int, int, hash_func>>(); }
#endif
#if CXX20
        {  bench_AccidentallyQuadratic<jg::dense_hash_map<int, int, hash_func>>(); }
        {  bench_AccidentallyQuadratic<rigtorp::HashMap<int, int, hash_func>>(); }
#endif
#if CXX17
        {  bench_AccidentallyQuadratic<ankerl::unordered_dense::map <int, int, hash_func>>(); }
#endif
#if HAVE_BOOST
        {  bench_AccidentallyQuadratic<boost::unordered_flat_map <int, int, hash_func>>(); }
#endif

#if HAVE_INDIVI
        {  bench_AccidentallyQuadratic<indivi::flat_umap <int, int, hash_func>>(); }
        {  bench_AccidentallyQuadratic<indivi::flat_wmap <int, int, hash_func>>(); }
#endif


#if ABSL_HMAP
        {  bench_AccidentallyQuadratic<absl::flat_hash_map <int, int, hash_func>>(); }
#endif

        {  bench_AccidentallyQuadratic<emilib::HashMap <int, int, hash_func>>(); }
        {  bench_AccidentallyQuadratic<emilib2::HashMap <int, int, hash_func>>(); }
        {  bench_AccidentallyQuadratic<emilib3::HashMap <int, int, hash_func>>(); }

#if ET
        {  bench_AccidentallyQuadratic<tsl::robin_map <int, int, hash_func>>(); }
        {  bench_AccidentallyQuadratic<robin_hood::unordered_map <int, int, hash_func>>(); }

#if X86_64
        {  bench_AccidentallyQuadratic<hrd_m::hash_map <int, int, hash_func>>(); }
        {  bench_AccidentallyQuadratic<ska::flat_hash_map <int, int, hash_func>>(); }
#endif
        {  bench_AccidentallyQuadratic<phmap::flat_hash_map <int, int, hash_func>>(); }
#endif

#if CK_HMAP
//        { ck::HashMap <int, int, hash_func> bench_AccidentallyQuadratic(); }
#endif
    }

    if (sflags <= 12 && eflags >= 12)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif ANKERL_HASH
        typedef ankerl::unordered_dense::hash<int> hash_func;
#elif STD_HASH
        typedef std::hash<int> hash_func;
#else
        typedef robin_hood::hash<int> hash_func;
#endif

        puts("\nbench_InsertEraseContinue:");

        {  bench_InsertEraseContinue<emhash6::HashMap<int, int, hash_func>>(); }
        {  bench_InsertEraseContinue<emhash7::HashMap<int, int, hash_func>>(); }
        {  bench_InsertEraseContinue<emhash5::HashMap<int, int, hash_func>>(); }
        {  bench_InsertEraseContinue<emhash8::HashMap<int, int, hash_func>>(); }

#if QC_HASH
        {  bench_InsertEraseContinue<qc::hash::RawMap<int, int, hash_func>>(); }
#endif
#if CXX20
        {  bench_InsertEraseContinue<jg::dense_hash_map<int, int, hash_func>>(); }
//        {  bench_InsertEraseContinue<rigtorp::HashMap<int, int, hash_func>>(); }
#endif
#if CXX17
        {  bench_InsertEraseContinue<ankerl::unordered_dense::map <int, int, hash_func>>(); }
#endif
#if HAVE_BOOST
//        {  bench_InsertEraseContinue<boost::unordered_flat_map <int, int, hash_func>>(); }
#endif
#if HAVE_INDIVI
//        { bench_InsertEraseContinue<indivi::flat_umap <int, int, hash_func>>(); }
//        { bench_InsertEraseContinue<indivi::flat_umap <int, int, hash_func>>(); }
#endif
#if ABSL_HMAP
        {  bench_InsertEraseContinue<absl::flat_hash_map <int, int, hash_func>>(); }
#endif

        {  bench_InsertEraseContinue<emilib3::HashMap <int, int, hash_func>>(); }
        {  bench_InsertEraseContinue<emilib::HashMap <int, int, hash_func>>(); }
        {  bench_InsertEraseContinue<emilib2::HashMap <int, int, hash_func>>(); }

#if ET
//        {  bench_InsertEraseContinue<hrd_m::hash_map <int, int, hash_func>>(); }
        {  bench_InsertEraseContinue<tsl::robin_map  <int, int, hash_func>>(); }
        {  bench_InsertEraseContinue<robin_hood::unordered_map <int, int, hash_func>>(); }

#if X86_64
        {  bench_InsertEraseContinue<ska::flat_hash_map <int, int, hash_func>>(); }
#endif
        {  bench_InsertEraseContinue<phmap::flat_hash_map <int, int, hash_func>>(); }
#endif
    }

    if (sflags <= 13 && eflags >= 13)
    {
#if ABSL_HASH
        typedef absl::Hash<int64_t> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int64_t> hash_func;
#elif ANKERL_HASH
        typedef ankerl::unordered_dense::hash<int64_t> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<int64_t> hash_func;
#else
        typedef std::hash<int64_t> hash_func;
#endif

        puts("\nbench_InsertEraseBegin:");

        {  bench_InsertEraseBegin<emhash6::HashMap<int64_t, int, hash_func>>(); }
        {  bench_InsertEraseBegin<emhash7::HashMap<int64_t, int, hash_func>>(); }
        {  bench_InsertEraseBegin<emhash8::HashMap<int64_t, int, hash_func>>(); }

#if QC_HASH
        {  bench_InsertEraseBegin<qc::hash::RawMap<int64_t, int, hash_func>>(); }
#endif
#if CXX20
        {  bench_InsertEraseBegin<jg::dense_hash_map<int64_t, int, hash_func>>(); }
//        {  bench_InsertEraseBegin<rigtorp::HashMap<int64_t, int, hash_func>>(); }
#endif
#if CXX17
        {  bench_InsertEraseBegin<ankerl::unordered_dense::map <int64_t, int, hash_func>>(); }
#endif
#if HAVE_BOOST
        {  bench_InsertEraseBegin<boost::unordered_flat_map <int64_t, int, hash_func>>(); }
#endif
#if HAVE_INDIVI
//        {  bench_InsertEraseBegin<indivi::flat_umap <int64_t, int, hash_func>>(); }
        {  bench_InsertEraseBegin<indivi::flat_wmap <int64_t, int, hash_func>>(); }
#endif

#if ABSL_HMAP
//        {  bench_InsertEraseBegin<absl::flat_hash_map <int64_t, int, hash_func>>(); }
#endif

        {  bench_InsertEraseBegin<emilib2::HashMap <int64_t, int, hash_func>>(); }
        {  bench_InsertEraseBegin<emilib3::HashMap <int64_t, int, hash_func>>(); }
        {  bench_InsertEraseBegin<emilib::HashMap <int64_t, int, hash_func>>(); }

#if ET
//        {  bench_InsertEraseBegin<hrd_m::hash_map <int64_t, int, hash_func>>(); }
        {  bench_InsertEraseBegin<tsl::robin_map <int64_t, int, hash_func>>(); }
        {  bench_InsertEraseBegin<robin_hood::unordered_map <int64_t, int, hash_func>>(); }

#if X86_64
//        {  bench_InsertEraseBegin<ska::flat_hash_map <int64_t, int, hash_func>>(); }
#endif
        //{  bench_InsertEraseBegin<phmap::flat_hash_map <int64_t, int, hash_func>>(); }
#endif
//        {  bench_InsertEraseBegin<emhash5::HashMap<int64_t, int, hash_func>>(); }
    }

    if (sflags <= 14 && eflags >= 14)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif ANKERL_HASH
        typedef ankerl::unordered_dense::hash<int> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<int> hash_func;
#else
        typedef std::hash<int> hash_func;
#endif

        puts("\nbench_CreateInsert:");

        {  bench_CreateInsert<emhash6::HashMap<int, int, hash_func>>(); }
        {  bench_CreateInsert<emhash7::HashMap<int, int, hash_func>>(); }
        {  bench_CreateInsert<emhash8::HashMap<int, int, hash_func>>(); }

#if QC_HASH
        {  bench_CreateInsert<qc::hash::RawMap<int, int, hash_func>>(); }
#endif
#if CXX20
        {  bench_CreateInsert<jg::dense_hash_map<int, int, hash_func>>(); }
        {  bench_CreateInsert<rigtorp::HashMap<int, int, hash_func>>(); }
#endif
#if CXX17
        {  bench_CreateInsert<ankerl::unordered_dense::map <int, int, hash_func>>(); }
#endif
#if HAVE_BOOST
        {  bench_CreateInsert<boost::unordered_flat_map <int, int, hash_func>>(); }
#endif
#if HAVE_INDIVI
        {  bench_CreateInsert<indivi::flat_umap <int, int, hash_func>>(); }
        {  bench_CreateInsert<indivi::flat_wmap <int, int, hash_func>>(); }
#endif
#if ABSL_HMAP
        {  bench_CreateInsert<absl::flat_hash_map <int, int, hash_func>>(); }
#endif


        {  bench_CreateInsert<emilib2::HashMap <int, int, hash_func>>(); }
        {  bench_CreateInsert<emilib::HashMap <int, int, hash_func>>(); }
        {  bench_CreateInsert<emilib3::HashMap <int, int, hash_func>>(); }

#if ET
//        {  bench_CreateInsert<hrd_m::hash_map <int, int, hash_func>>(); }
        {  bench_CreateInsert<tsl::robin_map <int, int, hash_func>>(); }
        {  bench_CreateInsert<robin_hood::unordered_map <int, int, hash_func>>(); }

#if X86_64
//        {  bench_CreateInsert<ska::flat_hash_map <int, int, hash_func>>(); }
#endif
        //{  bench_CreateInsert<phmap::flat_hash_map <int, int, hash_func>>(); }
#endif
        {  bench_CreateInsert<emhash5::HashMap<int, int, hash_func>>(); }
    }

    if (sflags <= 15 && eflags >= 15)
    {
#if ABSL_HASH
        typedef absl::Hash<int> hash_func;
#elif FIB_HASH
        typedef Int64Hasher<int> hash_func;
#elif ANKERL_HASH
        typedef ankerl::unordered_dense::hash<int> hash_func;
#elif HOOD_HASH
        typedef robin_hood::hash<int> hash_func;
#else
        typedef Hash32 hash_func;
#endif

        puts("\nbench_udb3:");

        {  bench_udb3<emhash6::HashMap<uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<emhash7::HashMap<uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<emhash8::HashMap<uint32_t, uint32_t, hash_func>>(); }

#if QC_HASH
        {  bench_udb3<qc::hash::RawMap<uint32_t, uint32_t, hash_func>>(); }
#endif
#if CXX20
        {  bench_udb3<jg::dense_hash_map<uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<rigtorp::HashMap<uint32_t, uint32_t, hash_func>>(); }
#endif
#if CXX17
        {  bench_udb3<ankerl::unordered_dense::map <uint32_t, uint32_t, hash_func>>(); }
#endif
#if HAVE_BOOST
        {  bench_udb3<boost::unordered_flat_map <uint32_t, uint32_t, hash_func>>(); }
#endif
#if HAVE_INDIVI
        {  bench_udb3<indivi::flat_umap <uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<indivi::flat_wmap <uint32_t, uint32_t, hash_func>>(); }
#endif


#if ABSL_HMAP
        {  bench_udb3<absl::flat_hash_map <uint32_t, uint32_t, hash_func>>(); }
#endif

        {  bench_udb3<emilib2::HashMap <uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<emilib::HashMap <uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<emilib3::HashMap <uint32_t, uint32_t, hash_func>>(); }

#if ET
//        {  bench_udb3<hrd_m::hash_map <uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<tsl::robin_map <uint32_t, uint32_t, hash_func>>(); }
        {  bench_udb3<robin_hood::unordered_map <uint32_t, uint32_t, hash_func>>(); }

#if X86_64
//        {  bench_udb3<ska::flat_hash_map <uint32_t, uint32_t, hash_func>>(); }
#endif
        //{  bench_udb3<phmap::flat_hash_map <uint32_t, uint32_t, hash_func>>(); }
#endif
        {  bench_udb3<emhash5::HashMap<uint32_t, uint32_t, hash_func>>(); }
    }

    printf("\ntotal time = %.2f s\n", now2sec() - start);
}

static void checkSet(const std::string_view& map_name)
{
    if (show_name.count(map_name) == 1)
        show_name.erase(map_name);
    else
        show_name.emplace(map_name, map_name);
}

static const char* const cases[] = {
    "bench_IterateIntegers",
    "bench_randomFindString",
    "bench_randomEraseString",
    "bench_copy",
    "bench_randomFind",
    "bench_insert",
    "bench_randomInsertErase",
    "bench_randomDistinct2",
    "bench_knucleotide",
    "bench_GameOfLife",
    "bench_AccidentallyQuadratic",
    "bench_InsertEraseContinue",
    "bench_InsertEraseBegin",
    "bench_CreateInsert",
    "bench_udb3",
};

#if 0
#include <iostream>
#include <emmintrin.h>
#include <chrono>

#include <iostream>
#include <vector>
#include <immintrin.h>

#include <immintrin.h>
#include <algorithm>

void test_mm_set1_epi8() {
  int32_t sum1 = 1;

  {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000000; ++i) {
      __m128i vi = _mm_set1_epi8(char(i * i % 256));
      sum1 += _mm_extract_epi32(vi, 1);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << sum1 << " Time taken by _mm_set1_epi8: " << diff.count() << " s" << std::endl;
  }
  {
    sum1 = 1;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000000; ++i) {
      __m128i vi = _mm_set1_epi16(short(i * i % 65536));
      sum1 += _mm_extract_epi32(vi, 2);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << sum1 << " Time taken by _mm_set1_epi8: " << diff.count() << " s" << std::endl;
  }
  {
    auto start = std::chrono::high_resolution_clock::now();
    int32_t sum2 = 1;
    for (int i = 0; i < 1000000000; ++i) {
      __m128i vi = _mm_set1_epi32((i*i % 256) * 0x01010101);
      sum2 += _mm_extract_epi32(vi, 3);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << sum2 << " Time taken by _mm_set1_epi32: " << diff.count() << " s" << std::endl;
  }
}
#endif

int main(int argc, char* argv[])
{
    srand((unsigned int)time(0));
    printInfo(nullptr);

    puts("usage: ./mbench [2-9mptseabrjqf]b[d]e[d]");
    puts("all test case:");
    for (int i = 0; i < int(sizeof(cases) / sizeof(cases[0])); i++)
        printf("    %2d %s\n", i + 1, cases[i]);
    puts("-------------------------------------------------------------------------");

    int sflags = 1, eflags = 20;
    if (argc > 1) {
        //printf("cmd agrs = %s\n", argv[1]);
        for (int c = argv[1][0], i = 0; c != '\0'; c = argv[1][++i]) {
            if (c > '4' && c <= '8') {
                std::string map_name("emhash");
                map_name += (char)c;
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
                checkSet("hrd_m");
            else if (c == '1')
                checkSet("emilib");
            else if (c == '2')
                checkSet("emilib2");
            else if (c == '3')
                checkSet("emilib3");
            else if (c == 'j')
                checkSet("jg");
            else if (c == 'r')
                checkSet("rigtorp");
            else if (c == 'k') {
                checkSet("HashMapTable");
                checkSet("HashMapCell");
            }
            else if (c == 'i')
                checkSet("indivi");
#if QC_HASH
            else if (c == 'q')
                checkSet("qc");
            else if (c == 'f')
                checkSet("fph");
#endif
            else if (c == 'b') {
                 if (isdigit(argv[1][i + 1]))
                    sflags = atoi(&argv[1][++i]);
#if HAVE_BOOST
                else
                    checkSet("boost");
#endif
                if (isdigit(argv[1][i + 1])) i++;
            }
            else if (c == 'e') {
                eflags = atoi(&argv[1][++i]);
                if (isdigit(argv[1][i + 1])) i++;
            }
            else if (c == 'l')
              max_lf = float(atoi(&argv[1][++i]) / 10.0);
        }
    }

    printf("test with max_load_factor = %.3f:rnd=%d\n", max_lf, (int)RND);
    puts("all test hashmap:");
    for (const auto& m : show_name)
        printf("%10s %20s\n", m.first.data(), m.second.data());
    puts("-------------------------------------------------------------------------");

    runTest(sflags, eflags);
    puts("---------------------------- all pass -----------------------------------");
    return 0;
}
