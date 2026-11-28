// #define FIB_HASH 7
#include "util.h"
#include <cmath>
#include <sstream>
#include <fstream>
#include <numeric>
#include <thread>
#include <algorithm>

#ifndef _WIN32
#include <sys/resource.h>
#include <sched.h>
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

// ============================================================================
// Benchmark Configuration and Output Support
// ============================================================================

// Output format options
enum class OutputFormat {
    Text, // Default printf output
    CSV,  // CSV format for analysis
    JSON  // JSON format for visualization
};

// Benchmark configuration structure
struct BenchConfig {
    size_t warmup_iterations = 1000;  // Warmup iterations before timing
    int benchmark_iterations = 1;     // Number of benchmark runs (for averaging)
    float max_load_factor = 7.0f / 8; // Default max load factor
    bool enable_reserve = true;       // Whether to reserve before insert
    OutputFormat output_format = OutputFormat::Text;
    std::string output_file = ""; // Output file path (empty = stdout)
};

// Benchmark result structure
struct BenchResult {
    std::string test_name;
    std::string hash_map_name;
    double time_sec;
    double time_stddev = 0.0; // Standard deviation (if multiple runs)
    double time_median = 0.0; // Median over repeated runs (global --iter)
    size_t operations = 0;
    double throughput = 0.0; // Operations per second
    float load_factor = 0.0f;
    size_t map_size = 0;
    size_t memory_bytes = 0;
};

// Global configuration
static BenchConfig g_config;
static std::vector<BenchResult> g_results;

// Output formatter
class ResultOutput {
public:
    static void output_text(const BenchResult& r) {
        if (r.time_stddev > 0) {
            printf("    %20s: %8.3f s (median=%8.3f stddev=%8.3f) ops=%zu throughput=%7.2f M/s\n",
                   r.hash_map_name.c_str(), r.time_sec, r.time_median, r.time_stddev, r.operations,
                   r.throughput / 1000000.0);
        } else {
            printf("    %20s: %8.3f s ops=%zu throughput=%7.2f M/s\n", r.hash_map_name.c_str(), r.time_sec,
                   r.operations, r.throughput / 1000000.0);
        }
    }

    static void output_csv_header() {
        printf("Test,HashMap,Time,StdDev,Operations,Throughput,LoadFactor,MapSize,MemBytes\n");
    }

    static void output_csv(const BenchResult& r) {
        printf("%s,%s,%.6f,%.6f,%zu,%.2f,%.2f,%zu,%zu\n", r.test_name.c_str(), r.hash_map_name.c_str(), r.time_sec,
               r.time_stddev, r.operations, r.throughput, r.load_factor, r.map_size, r.memory_bytes);
    }

    static void output_json_header() { printf("[\n"); }

    static void output_json(const BenchResult& r, bool is_last) {
        printf("  {\n");
        printf("    \"test\": \"%s\",\n", r.test_name.c_str());
        printf("    \"hashmap\": \"%s\",\n", r.hash_map_name.c_str());
        printf("    \"time\": %.6f,\n", r.time_sec);
        printf("    \"stddev\": %.6f,\n", r.time_stddev);
        printf("    \"median\": %.6f,\n", r.time_median);
        printf("    \"operations\": %zu,\n", r.operations);
        printf("    \"throughput\": %.2f,\n", r.throughput);
        printf("    \"load_factor\": %.2f,\n", r.load_factor);
        printf("    \"map_size\": %zu,\n", r.map_size);
        printf("    \"memory_bytes\": %zu\n", r.memory_bytes);
        printf("  }%s\n", is_last ? "" : ",");
    }

    static void output_json_footer() { printf("]\n"); }

    static void output_all(const std::vector<BenchResult>& results, OutputFormat format) {
        if (format == OutputFormat::CSV) {
            output_csv_header();
            for (const auto& r : results) {
                output_csv(r);
            }
        } else if (format == OutputFormat::JSON) {
            output_json_header();
            for (size_t i = 0; i < results.size(); ++i) {
                output_json(results[i], i == results.size() - 1);
            }
            output_json_footer();
        } else {
            for (const auto& r : results) {
                output_text(r);
            }
        }
    }
};

// Warmup helper function
template <typename HMAP, typename RNG> static void do_warmup(HMAP& hmap, RNG& rng, size_t iterations) {
    for (size_t i = 0; i < iterations; ++i) {
        hmap[static_cast<int>(rng())] = 0;
    }
    hmap.clear();
}

// #define EMH_ITER_SAFE 1
// #include "wyhash.h"

// #define EMH_STATIS 1
// #define ET            1
// #define EMH_WYHASH64   1
//
// #define EMH_PACK_TAIL 16
// #define EMH_HIGH_LOAD 123456
// #define EMH_STATIS 1234567

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emhash/hash_table9.hpp"

#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#endif

#if HAVE_INDIVI
#include "indivi/flat_umap.h"
#include "indivi/flat_wmap.h"
#endif

//    #define EMH_QUADRATIC 1
//    #define EMH_STATIS 123456
//    #define AVX2_EHASH 1
//    #define EMH_PSL_LINEAR 1
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap1m.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihmap4.hpp"
#include "martin/unordered_dense.h"

// #include "FHashTable/fhash_table.h"

// Fixed seed: reproducible run-to-run. All cases derive their RNG streams
// from RND, so numbers are directly comparable across runs and maps.
static constexpr uint64_t RND = UINT64_C(123);
static float max_lf = 7.0f / 8;

static int g_check_errors = 0;

// Runtime checksum verification: unlike assert(), survives -DNDEBUG, so the
// DCE-prevention guarantees and workload validation hold in release builds.
static inline void verify(bool ok, const char* what) {
    if (!ok) {
        printf("  !! CHECK FAILED: %s\n", what);
        ++g_check_errors;
    }
}

// Cross-map workload consistency. Deterministic workloads (fixed seeds) must
// produce identical results on every correct map: the first map run records
// the value, every later map verifies against it. This catches maps that
// silently drop, overwrite or duplicate entries without needing precomputed
// per-case constants.
static std::map<std::string, uint64_t> g_expected;
static inline void check_consistent(const std::string& what, uint64_t v) {
    auto it = g_expected.find(what);
    if (it == g_expected.end())
        g_expected.emplace(what, v);
    else if (it->second != v) {
        printf("  !! CHECK FAILED: %s (expected %llu, got %llu)\n", what.c_str(), (unsigned long long)it->second,
               (unsigned long long)v);
        ++g_check_errors;
    }
}

// Current RSS of the process (NOT peak-so-far): before/after deltas must
// reflect the live map. ru_maxrss on Linux is a monotonic peak and always
// reports the high-water mark, so deltas for later maps would read 0.
static size_t current_rss_bytes() {
#ifndef _WIN32
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f)
        return 0;
    long long total_pages = 0, resident_pages = 0;
    if (fscanf(f, "%lld %lld", &total_pages, &resident_pages) != 2)
        resident_pages = 0;
    fclose(f);
    return (size_t)resident_pages * (size_t)sysconf(_SC_PAGESIZE);
#else
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (size_t)pmc.WorkingSetSize; // current working set
    return 0;
#endif
}

// Unified hash function for every case so timings are comparable across
// sections. One -D override (ABSL_HASH / STD_HASH / FIB_HASH / A_HASH)
// switches all cases at once; the old per-case divergent fallbacks are gone.
#if ABSL_HASH
template <typename T> struct default_hash : absl::Hash<T> {};
#elif A_HASH
template <typename T> struct default_hash : Axxhasher<T> {};
#elif STD_HASH
template <typename T> struct default_hash : std::hash<T> {};
#elif FIB_HASH
template <typename T> struct default_hash : Int64Hasher<T> {};
#elif CXX17
template <typename T> struct default_hash : ankerl::unordered_dense::hash<T> {};
#else
template <typename T> struct default_hash : std::hash<T> {};
#endif

static std::map<std::string_view, std::string_view> show_name = {
    {"emhash7", "emhash7"},
    {"emhash8", "emhash8"},
    {"emhash9", "emhash9"},
    //{"emhash5", "emhash5"},
    //{"emhash6", "emhash6"},

    {"emilib", "emilib1"},
    {"emilib1m", "emilib1m"},
    {"emilib2", "emilib2"},
    {"emilib3", "emilib3"},
    {"emilib4", "emilib4"},

#if HAVE_BOOST
    {"boost", "boost flat"},
#endif

#if HAVE_INDIVI
    {"flat_u", "indivi_umap"},
//{"flat_w", "indivi_wmap"},
#endif

    //    {"fhash_table", "fht"},
    {"ankerl", "martin dense"},

#if ABSL_HMAP
    {"absl", "absl flat"},
#endif
};

static const char* find_hash(const std::string& map_name) {
    // Must run before the "emilib" prefix entry: emilib1m's mangled name also contains
    // "emilib" within the first ten characters.
    if (map_name.find("emilib1m") < 10)
        return show_name.count("emilib1m") ? show_name["emilib1m"].data() : nullptr;
    if (map_name.find("emilib2") < 10)
        return show_name.count("emilib2") ? show_name["emilib2"].data() : nullptr;
    if (map_name.find("emilib3") < 10)
        return show_name.count("emilib3") ? show_name["emilib3"].data() : nullptr;
    if (map_name.find("emilib4") < 10)
        return show_name.count("emilib4") ? show_name["emilib4"].data() : nullptr;
    if (map_name.find("flat_u") < 30)
        return show_name.count("flat_u") ? show_name["flat_u"].data() : nullptr;
    if (map_name.find("flat_w") < 30)
        return show_name.count("flat_w") ? show_name["flat_w"].data() : nullptr;

    for (const auto& kv : show_name) {
        if (map_name.find(kv.first) < 10)
            return kv.second.data();
    }
    return nullptr;
}

#ifndef RT
#define RT 2 // 2 wyrand 1 sfc64 3 RomuDuoJr 4 Lehmer64 5 mt19937_64
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
#if defined(__SIZEOF_INT128__)
// GCC/Clang extension; __extension__ keeps -Wpedantic quiet about __int128 in
// the 64x64->128 multiply below.
__extension__ typedef unsigned __int128 emh_uint128_t;
#endif

class sfc64 {
public:
    using result_type = uint64_t;

    // no copy ctors so we don't accidentally get the same random again
    sfc64(sfc64 const&) = delete;
    sfc64& operator=(sfc64 const&) = delete;

    sfc64(sfc64&&) = default;
    sfc64& operator=(sfc64&&) = default;

    sfc64(std::array<uint64_t, 4> const& state) : m_a(state[0]), m_b(state[1]), m_c(state[2]), m_counter(state[3]) {}

    static constexpr uint64_t(min)() { return (std::numeric_limits<uint64_t>::min)(); }
    static constexpr uint64_t(max)() { return (std::numeric_limits<uint64_t>::max)(); }

    sfc64() : sfc64(UINT64_C(0x853c49e6748fea9b)) {}

    sfc64(uint64_t seed) : m_a(seed), m_b(seed), m_c(seed), m_counter(1) {
        for (int i = 0; i < 12; ++i) {
            operator()();
        }
    }

    void seed() { *this = sfc64{std::random_device{}()}; }

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
        return static_cast<uint64_t>(
            (static_cast<emh_uint128_t>(operator()()) * static_cast<emh_uint128_t>(boundExcluded)) >> 64u);
#elif _WIN32
        uint64_t high;
        uint64_t a = operator()();
        _umul128(a, boundExcluded, &high);
        return high;
#endif
    }

    std::array<uint64_t, 4> state() const { return {m_a, m_b, m_c, m_counter}; }

    void state(std::array<uint64_t, 4> const& s) {
        m_a = s[0];
        m_b = s[1];
        m_c = s[2];
        m_counter = s[3];
    }

private:
    template <typename T> T rotl(T const x, uint32_t k) { return (x << k) | (x >> (8 * sizeof(T) - k)); }

    static constexpr int rotation = 24;
    static constexpr int right_shift = 11;
    static constexpr int left_shift = 3;
    uint64_t m_a;
    uint64_t m_b;
    uint64_t m_c;
    uint64_t m_counter;
};

static inline double now2sec() {
#if _WIN32
    FILETIME ft;
#if _WIN32_WINNT >= 0x0602
    GetSystemTimePreciseAsFileTime(&ft);
#else
    GetSystemTimeAsFileTime(&ft);
#endif /* Windows 8  */

    /* `t := (low + high * 0x1p32) / 10000`  */
    double t = (double)ft.dwLowDateTime + (double)ft.dwHighDateTime * 0x1p32;

    /* 11644473600000 is number of milliseconds from 1601-01-01T00:00:00Z
     * (the NT epoch) to 1970-01-01T00:00:00Z (the Unix Epoch).  */
    return (t / 10000'000 - 11644473600);
#elif __linux__
    // Wall clock (monotonic), not CPU time: Windows/Unix branches use wall
    // clock too, and only wall clock is comparable across platforms.
    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#elif __unix__
    struct timeval start;
    gettimeofday(&start, NULL);
    return start.tv_sec + start.tv_usec / 1000000.0;
#else
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(tp).count() / 1000000.0;
#endif
}

template <typename HMAP> static void bench_insert(HMAP& hmap) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:\n", map_name);

#if X86_64 || __MAC__
    uint32_t maxn = 1000000;
#else
    uint32_t maxn = 1000000 / 5;
#endif

    hmap.max_load_factor(g_config.max_load_factor);
    for (int i = 0; i < 3; i++) {
        auto nows = now2sec();
        {
            uint64_t RNDI = static_cast<uint64_t>(RND) + UINT64_C(15) + static_cast<uint64_t>(i);

            // Warmup phase - warm up cache before actual benchmark
            if (g_config.warmup_iterations > 0) {
                MRNG warmup_rng(RNDI + 1000);
                do_warmup(hmap, warmup_rng, g_config.warmup_iterations);
            }

            {
                if (g_config.enable_reserve)
                    hmap.reserve(maxn / 2);
                const size_t rss_before = current_rss_bytes();
                auto ts = now2sec();
                // Uniform workload for every map: operator[] dedupes, while
                // emilib's insert_unique skips the duplicate lookup entirely
                // (caller-asserted-unique fast path), so mixing the two made
                // maps run different workloads and diverge in final size.
                MRNG rng(RNDI);
                for (size_t n = 0; n < maxn; ++n)
                    hmap[static_cast<int>(rng())];

                printf("\t\t        (lf=%.2f) insert %7.2f", hmap.load_factor(), now2sec() - ts);
                fflush(stdout);

                // Record result for CSV/JSON output
                {
                    BenchResult r;
                    r.test_name = "insert";
                    r.hash_map_name = map_name;
                    r.time_sec = now2sec() - ts;
                    r.operations = maxn;
                    r.throughput = maxn / r.time_sec;
                    r.load_factor = hmap.load_factor();
                    r.map_size = static_cast<size_t>(hmap.size());
                    r.memory_bytes = current_rss_bytes() - rss_before;
                    g_results.push_back(r);
                }
            }
            {
                auto ts = now2sec();
                MRNG rng(RNDI);
                for (size_t n = 0; n < maxn * 9 / 10; ++n)
                    hmap.erase(static_cast<int>(rng()));
                printf(", remove 90%% %7.2f", now2sec() - ts);
                fflush(stdout);
                // erase attempts target the same key stream as inserts;
                // duplicates in the stream leave a small remainder.
                verify((size_t)hmap.size() < maxn, "insert/erase90 sanity");

                // Record result
                {
                    BenchResult r;
                    r.test_name = "erase_90pct";
                    r.hash_map_name = map_name;
                    r.time_sec = now2sec() - ts;
                    r.operations = maxn * 9 / 10;
                    r.throughput = static_cast<double>(r.operations) / r.time_sec;
                    // Survivor count distinguishes the 1M/10M/100M tiers in
                    // the aggregation group key (deterministic across maps).
                    r.map_size = static_cast<size_t>(hmap.size());
                    g_results.push_back(r);
                }
            }
            {
                auto ts = now2sec();
                MRNG rng(RNDI + 1);
                for (size_t n = 0; n < maxn; ++n)
                    (void)hmap.emplace(static_cast<int>(rng()), 0);
                printf(", reinsert %7.2f", now2sec() - ts);

                // Same RNG stream for every map, so the final size is
                // deterministic; a map that mis-inserts will diverge here.
                check_consistent("insert reinsert size " + std::to_string(maxn), (uint64_t)hmap.size());

                // Record result
                {
                    BenchResult r;
                    r.test_name = "reinsert";
                    r.hash_map_name = map_name;
                    r.time_sec = now2sec() - ts;
                    r.operations = maxn;
                    r.throughput = maxn / r.time_sec;
                    r.load_factor = hmap.load_factor();
                    r.map_size = static_cast<size_t>(hmap.size());
                    g_results.push_back(r);
                }
            }
            {
                auto ts = now2sec();
                hmap.clear();
                printf(", clear %8.3f", now2sec() - ts);
            }
        }
        printf(" %dM, total %7.2f s\n", int(maxn / 1000000), now2sec() - nows);
        maxn *= 10;
    }
}

template <typename HMAP, bool unique = false> static void bench_AccidentallyQuadratic() {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    auto nows = now2sec();
    sfc64 rng(12345);

    HMAP hmap;
    hmap.max_load_factor(max_lf);
    for (size_t n = 0; n < 10'000'000; ++n) {
        hmap[static_cast<int>(rng())];
    }
    verify(9988513 == hmap.size(), "AccidentallyQuadratic distinct count");

    // bench.beginMeasure("iterate");
    uint64_t sum = 0;
    for (auto const& kv : hmap) {
        sum += (uint64_t)kv.first + (uint64_t)kv.second;
    }
    verify(sum == UINT64_C(18446739465311920326), "AccidentallyQuadratic iteration sum");

#if CXX17
    //    bench.beginMeasure("iterate & copy");
    HMAP map2;
    for (auto const& kv : hmap) {
        if constexpr (unique)
            map2.insert_unique(kv.first, kv.second);
        else
            map2.emplace(kv.first, kv.second);
    }
    verify(hmap.size() == map2.size(), "AccidentallyQuadratic copy size");
#endif
    printf(" time %7.2f s\n", now2sec() - nows);
    {
        BenchResult r;
        r.test_name = "AccidentallyQuadratic";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 10000000;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = hmap.size();
        g_results.push_back(r);
    }
}

template <typename HMAP> static void bench_InsertEraseBegin() {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    size_t max_n = 100000;
    auto nows = now2sec();

    for (uint64_t i = 0; i < 3; ++i) {
        auto starts = now2sec();
        HMAP hmap;
        hmap.max_load_factor(max_lf);
        MRNG rng(987654321ull + i * i * i);

        // benchmark randomly inserting & erasing begin
        for (size_t j = 0; j < max_n / 5; ++j)
            (void)hmap.emplace((int64_t)rng(), 0);

        for (size_t j = 0; j < max_n; ++j) {
            hmap.erase(hmap.begin());
            (void)hmap.emplace((int64_t)rng(), 0);
        }

        printf("\n\t\t        %.2f loadf = %.2f mapsize = %d time %7.2f s", ((double)max_n / 1000000.0),
               hmap.load_factor(), (int)hmap.size(), now2sec() - starts);
        max_n *= 5;
    }

    printf(", total %7.2f s\n", now2sec() - nows);
    {
        BenchResult r;
        r.test_name = "InsertEraseBegin";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 0;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

template <typename HMAP> static void bench_InsertEraseContinue() {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    size_t max_n = 400000;
    auto nows = now2sec();

    for (uint64_t i = 0; i < 3; ++i) {
        auto starts = now2sec();
        HMAP hmap;
        hmap.max_load_factor(max_lf);
        // hmap.reserve((std::size_t)(max_n * .));
        MRNG rng(2345ull + i * i * i);

        // benchmark randomly inserting & erasing begin
        for (size_t j = 0; j < max_n / 3; ++j)
            (void)hmap.emplace((int)rng(), 0);

        auto key = hmap.begin()->first;
        for (size_t j = max_n; j > 0; j--) {
            auto it = hmap.find(key);
            if (it == hmap.end()) {
                it = hmap.begin();
                key = it->first;
            }

            if constexpr (std::is_void_v<decltype(hmap.erase(it))>) {
                hmap.erase(it);
                if (++it != hmap.end())
                    key = it->first;
            } else {
                it = hmap.erase(it);
                if (it != hmap.end())
                    key = it->first;
            }

            (void)hmap.emplace((int)rng(), 0);
        }

        printf("\n\t\t        %.2f loadf = %.2f mapsize = %d time %7.2f", ((double)max_n / 1000000.0),
               hmap.load_factor(), (int)hmap.size(), now2sec() - starts);
        max_n *= 7;
    }

    printf(", total %7.2f s\n", now2sec() - nows);
    {
        BenchResult r;
        r.test_name = "InsertEraseContinue";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 0;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

template <typename T> struct as_bits_t {
    T value;
};

template <typename T> as_bits_t<T> as_bits(T value) {
    return as_bits_t<T>{value};
}

template <typename T> std::ostream& operator<<(std::ostream& os, as_bits_t<T> const& t) {
    os << std::bitset<sizeof(t.value) * 8>(t.value);
    return os;
}

template <class RandomIt, class URBG> static void rshuffle(RandomIt first, RandomIt last, URBG&& g) {
    typedef typename std::iterator_traits<RandomIt>::difference_type diff_t;
    typedef std::uniform_int_distribution<diff_t> distr_t;
    typedef typename distr_t::param_type param_t;

    distr_t D;
    diff_t n = last - first;
    for (diff_t i = n - 1; i > 0; --i) {
        using std::swap;
        swap(first[i], first[D(g, param_t(0, i))]);
    }
}

template <class ForwardIt, class T> static void iotas(ForwardIt first, ForwardIt last, T value) {
    while (first != last) {
        *first++ = value;
        ++value;
    }
}

template <typename HMAP> static void bench_randomInsertErase(HMAP& hmap) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    auto nows = now2sec();
    double erase1 = 0, erase2 = 0;

    if (1) {
        uint32_t min_n = 1 << 20;
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
                (void)hmap.emplace(rng(), 0);
            }

            // auto ts = now2sec();
            maxn = max_loop * 10 / (10 + 4 * j);
            // benchmark randomly inserting & erasing
            for (size_t i = 0; i < maxn; ++i) {
                (void)hmap.emplace(rng(), 0);
                hmap.erase(rng2());
            }
            //            printf("    %8u %2d M time %.3f s hmap size %8d loadf = %.2f\n",
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
        // size_t const expectedFinalSizes[] = {7, 127, 2084, 32722, 524149, 8367491};
        size_t const max_n = 20000000;

        for (int i = 0; i < 6; ++i) {
            for (int b = 0; b < 4; ++b) {
                bitMask |= UINT64_C(1) << *bitsIt++;
            }

            //            auto ts = now2sec();
            for (size_t j = 0; j < max_n; ++j) {
                (void)map2.emplace(rng() & bitMask, 0);
                map2.erase(rng() & bitMask);
            }
            //            printf("    %02d bits  %2d M time %.3f s hmap size %d loadf = %.2f\n",
            //                    int(std::bitset<64>(bitMask).count()), int(max_n / 1000000), now2sec() - ts,
            //                    (int)map2.size(), map2.load_factor());
        }
        // Key stream is fixed, so the churn-steady-state size is deterministic.
        check_consistent("randomInsertErase final size", map2.size());
        erase2 = now2sec() - nows;
    }

    printf(" erase time %7.2f + %7.2f, total %7.2f s\n", erase1, erase2, erase1 + erase2);
    {
        BenchResult r;
        r.test_name = "randomInsertErase";
        r.hash_map_name = map_name;
        r.time_sec = erase1 + erase2;
        r.operations = 0;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

template <typename HMAP> static void bench_CreateInsert() {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    const std::array<size_t, 7> counts = {200, 2000, 2000, 20000, 200000, 2000000, 20000000};

    MRNG rng(213 + RND);
    auto nows = now2sec(), erase1 = 0.;
    size_t res = 0;
    for (size_t i = 0; i < counts.size(); ++i) {
        size_t count = counts[i];
        size_t repeats = counts.back() / count;
        HMAP hmap;
        hmap.max_load_factor(max_lf);

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
        HMAP hmap;
        hmap.max_load_factor(max_lf);

        for (size_t j = 0; j < repeats; ++j) {
            for (size_t n = 0; n < count; ++n)
                hmap[static_cast<int>(rng2())];
            res += (size_t)hmap.size();
            hmap.clear();
        }
    }

    verify(res != 0, "CreateInsert res");
    check_consistent("CreateInsert size sum", (uint64_t)res);
    auto erase2 = now2sec() - nows;
    printf(" CreateInsert/InsertCreate %7.2f + %7.2f, total %7.2f s\n", erase1, erase2, erase1 + erase2);
    {
        BenchResult r;
        r.test_name = "CreateInsert";
        r.hash_map_name = map_name;
        r.time_sec = erase1 + erase2;
        r.operations = 0;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

static inline uint32_t udb_hash32(uint32_t key) {
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

static inline uint32_t udb_get_key(const uint32_t n, const uint32_t x) {
#if 0
    return udb_hash32(x % (n>>2));
#else
    return (uint32_t)((x % (n >> 2)) * 0x45D9F3B);
#endif
}

static uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

struct Hash32 {
    // using is_avalanching = void;
    inline size_t operator()(const uint32_t x) const { return x; }
};

static constexpr uint32_t x0 = (uint32_t)RND;
static constexpr bool is_del = (x0 % 2 == 0);

template <typename HMAP> static void bench_udb3() {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    const auto nows = now2sec();
    constexpr uint32_t n_cp = 11, N = 80000000, n0 = 10000000;
    constexpr uint32_t step = (N - n0) / (n_cp - 1);

    HMAP h;
    h.max_load_factor(max_lf);
    uint64_t z = 0, x = x0;
    for (uint32_t j = 0, i = 0, n = n0; j < n_cp; ++j, n += step) {
        for (; i < n; ++i) {
            const uint64_t y = splitmix64(x);
            const uint32_t key = udb_get_key(n, (uint32_t)y);
            if (is_del) {
                auto p = h.emplace(key, i);
                if (!p.second)
                    h.erase(p.first);
                z += p.second;
            } else {
                z += ++h[key];
            }
        }
    }

    printf(" z[%d] = %d loadf = %.2f, total %7.2f s\n", is_del, (int)z, h.load_factor(), now2sec() - nows);
    verify(z > 0, "udb3 z > 0");
    check_consistent("udb3 z", z);
    {
        BenchResult r;
        r.test_name = "udb3";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 80000000;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = h.size();
        g_results.push_back(r);
    }
}

template <typename HMAP> static void bench_randomDistinct2(HMAP& hmap) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

#if X86_64 || __MAC__
    constexpr size_t const n = 50000000;
#else
    constexpr size_t const n = 50000000 / 2;
#endif
    auto nows = now2sec();
    MRNG rng(RND + 786512);

    hmap.max_load_factor(max_lf);
    [[maybe_unused]] int checksum;
    {
        // auto ts = now2sec();
        checksum = 0;
        size_t const max_rng = n / 20;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng(max_rng))];
        }
        //        printf("     05%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(),
        //        (int)hmap.size());
        verify(RND != 123 || n != 50000000 || 549953097 == checksum, "distinct 05% checksum");
    }

    {
        hmap.clear();
        // auto ts = now2sec();
        checksum = 0;
        size_t const max_rng = n / 4;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng(max_rng))];
        }
        //        printf("     25%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(),
        //        (int)hmap.size());
        verify(RND != 123 || n != 50000000 || 149998262 == checksum, "distinct 25% checksum");
    }

    {
        hmap.clear();
        // auto ts = now2sec();
        checksum = 0;
        size_t const max_rng = n / 2;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng(max_rng))];
        }
        //        printf("     50%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(),
        //        (int)hmap.size());
        verify(RND != 123 || n != 50000000 || 99994827 == checksum, "distinct 50% checksum");
    }

    {
        hmap.clear();
        // auto ts = now2sec();
        checksum = 0;
        for (size_t i = 0; i < n; ++i) {
            checksum += ++hmap[static_cast<int>(rng())];
        }
        //        printf("    100%% distinct %.3f s loadf = %.2f, size = %d\n", now2sec() - ts, hmap.load_factor(),
        //        (int)hmap.size());
        verify(RND != 123 || n != 50000000 || 50290416 == checksum, "distinct 100% checksum");
    }
    // #endif

    printf(" time %7.2f s\n", now2sec() - nows);
    {
        BenchResult r;
        r.test_name = "randomDistinct2";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 50000000 * 4;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

#define CODE_FOR_NUCLEOTIDE(nucleotide) (" \0 \1\3  \2"[nucleotide & 0x7])

template <typename HMAP> static size_t kcount(const std::vector<char>& poly, const std::string& oligo) {

    HMAP hmap;
    hmap.max_load_factor(max_lf);

    uint64_t key = 0;
    const uint64_t mask = ((uint64_t)1 << 2 * oligo.size()) - 1;

    // For the first several nucleotides we only need to append them to key in
    // preparation for the insertion of complete oligonucleotides to hmap.
    for (size_t i = 0; i < oligo.size() - 1; ++i)
        key = (key << 2 & mask) | (uint64_t)poly[i];

    // Add all the complete oligonucleotides of oligo.size() to
    // hmap and update the count for each oligonucleotide.
    for (size_t i = oligo.size() - 1; i < poly.size(); ++i) {
        key = (key << 2 & mask) | (uint64_t)poly[i];
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

template <typename HMAP> static void bench_knucleotide() {
    static constexpr size_t n = 25000000;

    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    HMAP hmap;
    state = static_cast<int>(RND);
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
    printf(" ans = %d time %7.2f s\n", (int)ans, now2sec() - nows);
    verify(ans > 0, "knucleotide ans > 0");
    check_consistent("knucleotide ans", (uint64_t)ans);
    {
        BenchResult r;
        r.test_name = "knucleotide";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 25000000 * 3;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

class vec2 {
    uint32_t m_xy;

public:
    constexpr vec2(uint16_t x, uint16_t y) : m_xy{static_cast<uint32_t>(x) << 16U | y} {}

    constexpr explicit vec2(uint32_t xy) : m_xy(xy) {}

    [[nodiscard]] constexpr auto pack() const -> uint32_t { return m_xy; };

    [[nodiscard]] constexpr auto add_x(uint16_t x) const -> vec2 {
        return vec2{m_xy + (static_cast<uint32_t>(x) << 16U)};
    }

    [[nodiscard]] constexpr auto add_y(uint16_t y) const -> vec2 {
        return vec2{(m_xy & 0xffff0000) | ((m_xy + y) & 0xffff)};
    }

    template <typename Op> constexpr void for_each_surrounding(Op&& op) const {
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
static void game_of_life(const char* name, size_t nsteps, size_t finalPopulation, HMAP& map1, std::vector<vec2> cells) {

    (void)name;
    (void)finalPopulation;
    map1.clear();
    auto map2 = map1;

    for (auto& v : cells) {
        v = v.add_x(UINT16_MAX / 2).add_y(UINT16_MAX / 2);
        map1[v.pack()] = true;
        v.for_each_surrounding([&](uint32_t xy) { (void)map1.emplace(xy, false); });
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
                vec2{pos}.for_each_surrounding([&](uint32_t xy) { (void)m2->emplace(xy, false); });
            }
        }
        m1->clear();
        std::swap(m1, m2);
    }

    [[maybe_unused]] size_t count = 0;
    for (auto const& kv : *m1) {
        count += kv.second;
    }
    verify(finalPopulation == count, "GameOfLife final population");
}

template <typename HMAP> static void bench_GameOfLife() {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    HMAP hmap;
    hmap.max_load_factor(max_lf);
    auto stastabilizing = now2sec();
    {
        // https://conwaylife.com/wiki/R-pentomino
        game_of_life("R-pentomino", 1103, 116, hmap, {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2}});

        // https://conwaylife.com/wiki/Acorn
        game_of_life("Acorn", 5206, 633, hmap, {{1, 0}, {3, 1}, {0, 2}, {1, 2}, {4, 2}, {5, 2}, {6, 2}});

        // https://conwaylife.com/wiki/Jaydot
        game_of_life("Jaydot", 6929, 1124, hmap,
                     {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {1, 3}, {1, 4}, {2, 4}, {0, 5}});

        // https://conwaylife.com/wiki/Bunnies
        game_of_life("Bunnies", 17332, 1744, hmap,
                     {{0, 0}, {6, 0}, {2, 1}, {6, 1}, {2, 2}, {5, 2}, {7, 2}, {1, 3}, {3, 3}});

        printf(" stastabilizing = %7.2f", now2sec() - stastabilizing);
    }

    auto grow = now2sec();
    {
        auto map1 = hmap;
        // https://conwaylife.com/wiki/Gotts_dots
        game_of_life("Gotts dots", 2000, 4599, map1,
                     {
                         {0, 0},    {0, 1},    {0, 2},                                                           // 1
                         {4, 11},   {5, 12},   {6, 13},   {7, 12},   {8, 11},                                    // 2
                         {9, 13},   {9, 14},   {9, 15},                                                          // 3
                         {185, 24}, {186, 25}, {186, 26}, {186, 27}, {185, 27}, {184, 27}, {183, 27}, {182, 26}, // 4
                         {179, 28}, {180, 29}, {181, 29}, {179, 30},                                             // 5
                         {182, 32}, {183, 31}, {184, 31}, {185, 31}, {186, 31}, {186, 32}, {186, 33}, {185, 34}, // 6
                         {175, 35}, {176, 36}, {170, 37}, {176, 37}, {171, 38}, {172, 38}, {173, 38}, {174, 38},
                         {175, 38}, {176, 38}, // 7
                     });

        // https://conwaylife.com/wiki/Puffer_2
        game_of_life("Puffer 2", 2000, 7400, map1,
                     {
                         {1, 0}, {2, 0}, {3, 0},  {15, 0}, {16, 0}, {17, 0}, // line 0
                         {0, 1}, {3, 1}, {14, 1}, {17, 1},                   // line 1
                         {3, 2}, {8, 2}, {9, 2},  {10, 2}, {17, 2},          // line 2
                         {3, 3}, {8, 3}, {11, 3}, {17, 3},                   // line 3
                         {2, 4}, {7, 4}, {16, 4},                            // line 4
                     });
    }

    printf(", grow = %7.2f, total %7.2f s\n", now2sec() - grow, now2sec() - stastabilizing);
    {
        BenchResult r;
        r.test_name = "GameOfLife";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - stastabilizing;
        r.operations = 0;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

template <typename HMAP> static void bench_copy(HMAP&) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

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
        for (int i = 0; i < KN; i++) // with different load factor
            mapForCopy[rng()] = (int)rng();
    }
    //    assert(result == 300019900);
    auto copyt = now2sec();

    mapForCopy = mapSource;
    HMAP m;
    for (int n = 0; n < KL; ++n) {
        m = mapForCopy;
        result += (size_t)m.size() + (size_t)m[rememberKey];
        for (int i = 0; i < KN; i++)
            mapForCopy[rng()] = (int)rng();
    }
    //    assert(result == 600039800);
    check_consistent("copy result", result);
    printf("result = %d copy = %7.2f, assign = %7.2f s\n", int(result), copyt - nows, now2sec() - copyt);
    {
        BenchResult r;
        r.test_name = "copy";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 1000;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = mapSource.size();
        g_results.push_back(r);
    }
}

template <typename HMAP> static size_t runInsertEraseString(size_t max_n, size_t string_length, uint32_t bitMask) {
    MRNG rng(RND + 4);

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
        (void)hmap.emplace(str, 0);
        *strData32 = uint32_t(rng() & bitMask);
        verifier += (size_t)hmap.erase(str);
#endif
    }

    //    printf("%4zd bytes time %.2f, loadf = %.2f %d\n", string_length, now2sec() - ts, hmap.load_factor(),
    //    (int)hmap.size());
    return verifier;
}

template <typename HMAP>
static uint64_t randomFindInternalString(size_t numRandom, size_t const length, size_t numInserts,
                                         size_t numFindsPerInsert) {
    size_t constexpr NumTotal = 4;

    size_t const numFindsPerIter = numFindsPerInsert * NumTotal;

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
                    num_found += it->second;
                }
            }
        } while (i < numInserts);
    }

    return num_found;
}

template <typename HMAP> static void bench_randomFindString(HMAP&) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    auto nows = now2sec();
    auto now1 = nows, now2 = nows;
    uint64_t total_hits = 0;
    if (1) {
        static constexpr size_t numInserts = 1000000 / 2;
        static constexpr size_t numFindsPerInsert = 200 / 2;

        total_hits += randomFindInternalString<HMAP>(4, 13, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(3, 13, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(2, 13, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(1, 13, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(0, 13, numInserts, numFindsPerInsert);
        now1 = now2sec();
    }

    {
        static constexpr size_t numInserts = 100000;
        static constexpr size_t numFindsPerInsert = 1000;

        total_hits += randomFindInternalString<HMAP>(4, 100, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(3, 100, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(2, 100, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(1, 100, numInserts, numFindsPerInsert);
        total_hits += randomFindInternalString<HMAP>(0, 100, numInserts, numFindsPerInsert);
        now2 = now2sec();
    }
    // Fixed find stream: every correct map must observe the same hit count.
    verify(total_hits > 0, "randomFindString hits > 0");
    check_consistent("randomFindString hits", total_hits);
    printf(" %7.2f + %7.2f, total %7.2f s\n", now1 - nows, now2 - now1, now2 - nows);
    {
        BenchResult r;
        r.test_name = "randomFindString";
        r.hash_map_name = map_name;
        r.time_sec = now2 - nows;
        // 5 x (500k inserts x 100 finds) + 5 x (100k inserts x 1000 finds)
        r.operations = (500000ull * 100 + 100000ull * 1000) * 5;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

template <typename HMAP> static void bench_randomEraseString(HMAP&) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    auto nows = now2sec();
    uint64_t total_erased = 0;
    total_erased += runInsertEraseString<HMAP>(20000000, 7, 0xfffff);
    total_erased += runInsertEraseString<HMAP>(20000000, 8, 0xfffff);
    total_erased += runInsertEraseString<HMAP>(20000000, 13, 0xfffff);
    { runInsertEraseString<HMAP>(10000000, 24, 0xfffff); }
    total_erased += runInsertEraseString<HMAP>(12000000, 100, 0x4ffff);
    total_erased += runInsertEraseString<HMAP>(8000000, 200, 0x3ffff);
    total_erased += runInsertEraseString<HMAP>(6000000, 1000, 0x7ffff);

    // Insert/erase streams are fixed: the successful-erase count must agree
    // across maps (a map that drops or duplicates entries diverges here).
    verify(total_erased > 0, "randomEraseString erased > 0");
    check_consistent("randomEraseString erased", total_erased);

    printf("time %7.2f s\n", now2sec() - nows);
    {
        BenchResult r;
        r.test_name = "randomEraseString";
        r.hash_map_name = map_name;
        r.time_sec = now2sec() - nows;
        r.operations = 20000000 * 3 + 10000000 + 12000000 + 8000000 + 6000000;
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = 0;
        g_results.push_back(r);
    }
}

template <typename HMAP>
static uint64_t randomFindInternal(size_t numRandom, uint64_t bitMask, const size_t numInserts,
                                   const size_t numFindsPerInsert) {
    size_t constexpr NumTotal = 4;

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

    {
        size_t i = 0;
        size_t findCount = 0;

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

    (void)num_found;
    return (uint64_t)hmap.size();
}

template <typename HMAP> static void bench_IterateIntegers(HMAP& hmap) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    size_t const num_iters = 50000;
    uint64_t result = 0;

    // Warmup phase
    if (g_config.warmup_iterations > 0) {
        MRNG warmup_rng(999);
        for (size_t n = 0; n < g_config.warmup_iterations / 10; ++n) {
            hmap[warmup_rng()] = n;
            for (const auto& keyVal : hmap) {
                (void)keyVal;
                result += 1;
            }
        }
        hmap.clear();
        result = 0;
    }

    // Single pass: repetition/statistics are handled globally in main
    // (--iter=N repeats the whole suite, results aggregated to best/median).
    const int reps = 1;
    double t_add = 1e30, t_remove = 1e30;
    std::vector<double> rep_times;
    for (int rep = 0; rep < reps; ++rep) {
        hmap.clear();
        result = 0;

        auto ts = now2sec();
        {
            MRNG rng(123);
            for (size_t n = 0; n < num_iters; ++n) {
                hmap[rng()] = n;
                for (const auto& keyVal : hmap)
                    result += keyVal.second;
            }
        }
        verify(result == 20833333325000ull, "IterateIntegers insert checksum");
        t_add = std::min(t_add, now2sec() - ts);

        auto ts1 = now2sec();
        {
            MRNG rng(123);
            for (size_t n = 0; n < num_iters; ++n) {
                hmap.erase(rng());
                for (auto const& keyVal : hmap)
                    result += keyVal.second;
            }
        }
        // final result = insert sum 20833333325000 + erase-phase sum 41665416675000
        verify(result == 62498750000000ull, "IterateIntegers erase checksum");
        t_remove = std::min(t_remove, now2sec() - ts1);
        rep_times.push_back((now2sec() - ts) + (now2sec() - ts1));
    }

    double t_sd = 0.0;
    if (rep_times.size() > 1) {
        const double mean = std::accumulate(rep_times.begin(), rep_times.end(), 0.0) / rep_times.size();
        double sq = 0.0;
        for (double t : rep_times)
            sq += (t - mean) * (t - mean);
        t_sd = std::sqrt(sq / (rep_times.size() - 1));
    }
    printf("result = %d, time add/removing = %7.2f, %7.2f (best of %d)\n", int(result), t_add, t_remove, reps);

    // Record result for CSV/JSON output
    {
        BenchResult r;
        r.test_name = "IterateIntegers";
        r.hash_map_name = map_name;
        r.time_sec = t_add + t_remove;
        r.time_stddev = t_sd;
        r.operations = num_iters * 2;
        r.throughput = static_cast<double>(r.operations) / r.time_sec;
        g_results.push_back(r);
    }
}

template <typename HMAP> static void bench_randomFind(HMAP&, size_t numInserts, size_t numFindsPerInsert) {
    auto map_name = find_hash(typeid(HMAP).name());
    if (!map_name)
        return;
    printf("    %20s:", map_name);

    static constexpr auto lower32bit = UINT64_C(0x00000000FFFFFFFF);
    static constexpr auto upper32bit = UINT64_C(0xFFFFFFFF00000000);
    static constexpr auto mediu32bit = UINT64_C(0x0000FFFFFFFF0000);

    // Warmup phase - warm up cache before actual benchmark
    if (g_config.warmup_iterations > 0) {
        HMAP warmup_map;
        MRNG warmup_rng(RND + 1000);
        for (size_t i = 0; i < g_config.warmup_iterations; ++i) {
            warmup_map[warmup_rng()] = i;
            (void)warmup_map.find(warmup_rng());
        }
    }

    auto ts = now2sec();
    uint64_t sum = 0;

    sum += randomFindInternal<HMAP>(4, lower32bit, numInserts, numFindsPerInsert);
    sum += randomFindInternal<HMAP>(3, upper32bit, numInserts, numFindsPerInsert);
    sum += randomFindInternal<HMAP>(2, mediu32bit, numInserts, numFindsPerInsert);

    sum += randomFindInternal<HMAP>(1, upper32bit, numInserts, numFindsPerInsert);
    sum += randomFindInternal<HMAP>(0, lower32bit, numInserts, numFindsPerInsert);

    // Distinct counts are stream-determined; every correct map must agree.
    check_consistent("randomFind distinct n=" + std::to_string(numInserts), sum);

    auto elapsed = now2sec() - ts;
    printf(" nums = %zd, time %7.2f s\n", numInserts, elapsed);

    // Record result for CSV/JSON output
    {
        BenchResult r;
        r.test_name = "randomFind";
        r.hash_map_name = map_name;
        r.time_sec = elapsed;
        r.operations = numInserts * numFindsPerInsert * 5;
        r.throughput = static_cast<double>(r.operations) / r.time_sec;
        g_results.push_back(r);
    }
}

static void runTest(int sflags, int eflags) {
    const auto start = now2sec();

    if (sflags <= 1 && eflags >= 1) {
        using hash_func = default_hash<uint64_t>;

        puts("\nbench_IterateIntegers:");

        {
            emhash5::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emhash8::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emhash9::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emhash7::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emhash6::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            ankerl::unordered_dense::map<uint64_t, uint64_t, hash_func> martin;
            bench_IterateIntegers(martin);
        }
        {
            emilib::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emilib1m::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emilib3::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emilib2::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }
        {
            emilib4::HashMap<uint64_t, uint64_t, hash_func> emap;
            bench_IterateIntegers(emap);
        }

#if HAVE_BOOST
        {
            boost::unordered_flat_map<uint64_t, uint64_t, hash_func> hmap;
            bench_IterateIntegers(hmap);
        }
#endif

#if HAVE_INDIVI
        {
            indivi::flat_umap<uint64_t, uint64_t, hash_func> hmap;
            bench_IterateIntegers(hmap);
        }
        {
            indivi::flat_wmap<uint64_t, uint64_t, hash_func> hmap;
            bench_IterateIntegers(hmap);
        }
#endif

#if ABSL_HMAP
        {
            absl::flat_hash_map<uint64_t, uint64_t, hash_func> hmap;
            bench_IterateIntegers(hmap);
        }
#endif
    }

    if (sflags <= 2 && eflags >= 2) {
        using hash_func = default_hash<std::string>;
        puts("\nbench_randomFindString:");

        {
            ankerl::unordered_dense::map<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emhash8::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emhash9::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emhash6::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emhash5::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emhash7::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }

        {
            emilib3::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emilib2::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emilib4::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emilib::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            emilib1m::HashMap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }

#if HAVE_BOOST
        {
            boost::unordered_flat_map<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
#endif

#if HAVE_INDIVI
        {
            indivi::flat_umap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
        {
            indivi::flat_wmap<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
#endif

#if ABSL_HMAP
        {
            absl::flat_hash_map<std::string, size_t, hash_func> bench;
            bench_randomFindString(bench);
        }
#endif
    }

    if (sflags <= 3 && eflags >= 3) {
        using hash_func = default_hash<std::string>;
        puts("\nbench_randomEraseString:");

        {
            emilib2::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emilib::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emilib1m::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emilib3::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emilib4::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }

        {
            emhash8::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emhash9::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emhash7::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emhash6::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            emhash5::HashMap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }

        {
            ankerl::unordered_dense::map<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }

#if ABSL_HMAP
        {
            absl::flat_hash_map<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
#endif
#if HAVE_BOOST
        {
            boost::unordered_flat_map<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
#endif

#if HAVE_INDIVI
        {
            indivi::flat_umap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
        {
            indivi::flat_wmap<std::string, int, hash_func> bench;
            bench_randomEraseString(bench);
        }
#endif
    }

    if (sflags <= 4 && eflags >= 4) {
        using hash_func = default_hash<uint64_t>;
        puts("\nbench_copy:");

        {
            emhash6::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emhash5::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emhash7::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emhash8::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emhash9::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }

        {
            ankerl::unordered_dense::map<uint64_t, int, hash_func> martin;
            bench_copy(martin);
        }

        {
            emilib::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emilib1m::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emilib2::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emilib3::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }
        {
            emilib4::HashMap<uint64_t, int, hash_func> emap;
            bench_copy(emap);
        }

#if ABSL_HMAP
        {
            absl::flat_hash_map<uint64_t, int, hash_func> hmap;
            bench_copy(hmap);
        }
#endif
#if HAVE_BOOST
        {
            boost::unordered_flat_map<uint64_t, int, hash_func> hmap;
            bench_copy(hmap);
        }
#endif

#if HAVE_INDIVI
        {
            indivi::flat_umap<uint64_t, int, hash_func> hmap;
            bench_copy(hmap);
        }
        {
            indivi::flat_wmap<uint64_t, int, hash_func> hmap;
            bench_copy(hmap);
        }
#endif
    }

    if (sflags <= 5 && eflags >= 5) {
        // Same unified hash as every other case; the old per-case #if chain
        // (with a dead STD_HASH branch) made this section incomparable.
        using hash_func = default_hash<size_t>;

        puts("\nbench_randomFind:");

        // Fixed seed: the 6 workload sizes must not vary run to run, otherwise
        // numbers are not reproducible or comparable.
        MRNG rng(RND);
        const size_t numInserts[] = {(size_t)rng() % 123 + 123,         (size_t)rng() % 1234 + 1234,
                                     (size_t)rng() % 12345 + 12345,     (size_t)rng() % 123456 + 123456,
                                     (size_t)rng() % 1234567 + 1234567, (size_t)rng() % 12345678 + 12345678};
        constexpr size_t numFindsPerInsert[] = {800000, 50000, 5000, 200, 10, 2};

        for (size_t i = 0; i < size_t(sizeof(numInserts) / sizeof(numInserts[0])); i++) {
            {
                emilib3::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emilib::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emilib1m::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emilib2::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emilib4::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }

#if ABSL_HMAP
            {
                absl::flat_hash_map<size_t, size_t, hash_func> hmap;
                bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]);
            }
#endif
#if HAVE_BOOST
            {
                boost::unordered_flat_map<size_t, size_t, hash_func> hmap;
                bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]);
            }
#endif

#if HAVE_INDIVI
            {
                indivi::flat_umap<size_t, size_t, hash_func> hmap;
                bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                indivi::flat_wmap<size_t, size_t, hash_func> hmap;
                bench_randomFind(hmap, numInserts[i], numFindsPerInsert[i]);
            }
#endif

            {
                ankerl::unordered_dense::map<size_t, size_t, hash_func> martin;
                bench_randomFind(martin, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emhash8::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emhash9::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emhash5::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emhash6::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            {
                emhash7::HashMap<size_t, size_t, hash_func> emap;
                bench_randomFind(emap, numInserts[i], numFindsPerInsert[i]);
            }
            putchar('\n');
        }
    }

    if (sflags <= 6 && eflags >= 6) {
        using hash_func = default_hash<int>;

        puts("\nbench_insert:");

#if ABSL_HMAP
        {
            absl::flat_hash_map<int, int, hash_func> amap;
            bench_insert(amap);
        }
#endif
        {
            emhash7::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }

        {
            ankerl::unordered_dense::map<int, int, hash_func> martin;
            bench_insert(martin);
        }

        {
            emhash8::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
        {
            emhash9::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
        {
            emhash6::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
        {
            emhash5::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }

        {
            emilib::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
        {
            emilib1m::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
#if HAVE_BOOST
        {
            boost::unordered_flat_map<int, int, hash_func> amap;
            bench_insert(amap);
        }
#endif

#if HAVE_INDIVI
        {
            indivi::flat_umap<int, int, hash_func> amap;
            bench_insert(amap);
        }
        {
            indivi::flat_wmap<int, int, hash_func> amap;
            bench_insert(amap);
        }
#endif

        {
            emilib2::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
        {
            emilib3::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
        {
            emilib4::HashMap<int, int, hash_func> emap;
            bench_insert(emap);
        }
    }

    if (sflags <= 7 && eflags >= 7) {
        using hash_func = default_hash<uint64_t>;

        puts("\nbench_randomInsertErase:");

        {
            emhash6::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }

        {
            emilib3::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            emilib2::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            emilib::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            emilib1m::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            emilib4::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }

#if HAVE_BOOST
        {
            boost::unordered_flat_map<uint64_t, int, hash_func> hmap;
            bench_randomInsertErase(hmap);
        }
#endif

#if HAVE_INDIVI
        {
            indivi::flat_umap<uint64_t, int, hash_func> hmap;
            bench_randomInsertErase(hmap);
        }
        {
            indivi::flat_wmap<uint64_t, int, hash_func> hmap;
            bench_randomInsertErase(hmap);
        }
#endif

#if ABSL_HMAP
        {
            absl::flat_hash_map<uint64_t, int, hash_func> hmap;
            bench_randomInsertErase(hmap);
        }
#endif
        {
            emhash8::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            emhash9::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            emhash5::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            emhash7::HashMap<uint64_t, int, hash_func> emap;
            bench_randomInsertErase(emap);
        }
        {
            ankerl::unordered_dense::map<uint64_t, int, hash_func> martin;
            bench_randomInsertErase(martin);
        }
    }

    if (sflags <= 8 && eflags >= 8) {
        using hash_func = default_hash<int>;

        puts("\nbench_randomDistinct2:");

        {
            ankerl::unordered_dense::map<int, int, hash_func> martin;
            bench_randomDistinct2(martin);
        }

        {
            emhash8::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emhash9::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emhash6::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emhash5::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emhash7::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }

        {
            emilib::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emilib1m::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emilib2::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emilib3::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }
        {
            emilib4::HashMap<int, int, hash_func> emap;
            bench_randomDistinct2(emap);
        }

#if HAVE_BOOST
        {
            boost::unordered_flat_map<int, int, hash_func> hmap;
            bench_randomDistinct2(hmap);
        }
#endif

#if HAVE_INDIVI
        {
            indivi::flat_umap<int, int, hash_func> hmap;
            bench_randomDistinct2(hmap);
        }
        {
            indivi::flat_wmap<int, int, hash_func> hmap;
            bench_randomDistinct2(hmap);
        }
#endif

#if ABSL_HMAP
        {
            absl::flat_hash_map<int, int, hash_func> hmap;
            bench_randomDistinct2(hmap);
        }
#endif
    }

    if (sflags <= 9 && eflags >= 9) {
        using hash_func = default_hash<uint64_t>;
        puts("\nbench_knucleotide:");

        { bench_knucleotide<ankerl::unordered_dense::map<uint64_t, uint32_t, hash_func>>(); }

#if HAVE_BOOST
        { bench_knucleotide<boost::unordered_flat_map<uint64_t, uint32_t, hash_func>>(); }
#endif

#if HAVE_INDIVI
        { bench_knucleotide<indivi::flat_umap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<indivi::flat_wmap<uint64_t, uint32_t, hash_func>>(); }
#endif

#if ABSL_HMAP
        { bench_knucleotide<absl::flat_hash_map<uint64_t, uint32_t, hash_func>>(); }
#endif

        { bench_knucleotide<emhash6::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emhash5::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emhash7::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emhash8::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emhash9::HashMap<uint64_t, uint32_t, hash_func>>(); }

        { bench_knucleotide<emilib::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emilib1m::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emilib2::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emilib3::HashMap<uint64_t, uint32_t, hash_func>>(); }
        { bench_knucleotide<emilib4::HashMap<uint64_t, uint32_t, hash_func>>(); }
    }

    if (sflags <= 10 && eflags >= 10) {
        using hash_func = default_hash<uint32_t>;
        puts("\nbench_GameOfLife:");

        { bench_GameOfLife<emhash6::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emhash5::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emhash7::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emhash8::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emhash9::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<ankerl::unordered_dense::map<uint32_t, bool, hash_func>>(); }

#if HAVE_BOOST
        { bench_GameOfLife<boost::unordered_flat_map<uint32_t, bool, hash_func>>(); }
#endif

#if HAVE_INDIVI
        { bench_GameOfLife<indivi::flat_umap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<indivi::flat_wmap<uint32_t, bool, hash_func>>(); }
#endif
#if ABSL_HMAP
        { bench_GameOfLife<absl::flat_hash_map<uint32_t, bool, hash_func>>(); }
#endif

        { bench_GameOfLife<emilib::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emilib1m::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emilib2::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emilib3::HashMap<uint32_t, bool, hash_func>>(); }
        { bench_GameOfLife<emilib4::HashMap<uint32_t, bool, hash_func>>(); }
    }

    if (sflags <= 11 && eflags >= 11) {
        using hash_func = default_hash<int>;

        puts("\nbench_AccidentallyQuadratic (10M int insert.copy.iterator):");

        { bench_AccidentallyQuadratic<emhash6::HashMap<int, int, hash_func>, true>(); }
        { bench_AccidentallyQuadratic<emhash7::HashMap<int, int, hash_func>, true>(); }
        { bench_AccidentallyQuadratic<emhash5::HashMap<int, int, hash_func>, true>(); }
        { bench_AccidentallyQuadratic<emhash8::HashMap<int, int, hash_func>, true>(); }
        { bench_AccidentallyQuadratic<emhash9::HashMap<int, int, hash_func>, true>(); }
        { bench_AccidentallyQuadratic<ankerl::unordered_dense::map<int, int, hash_func>>(); }

#if HAVE_BOOST
        { bench_AccidentallyQuadratic<boost::unordered_flat_map<int, int, hash_func>>(); }
#endif

#if HAVE_INDIVI
        { bench_AccidentallyQuadratic<indivi::flat_umap<int, int, hash_func>>(); }
        { bench_AccidentallyQuadratic<indivi::flat_wmap<int, int, hash_func>>(); }
#endif

#if ABSL_HMAP
        { bench_AccidentallyQuadratic<absl::flat_hash_map<int, int, hash_func>>(); }
#endif

        { bench_AccidentallyQuadratic<emilib::HashMap<int, int, hash_func>>(); }
        { bench_AccidentallyQuadratic<emilib1m::HashMap<int, int, hash_func>>(); }
        { bench_AccidentallyQuadratic<emilib2::HashMap<int, int, hash_func>>(); }
        { bench_AccidentallyQuadratic<emilib3::HashMap<int, int, hash_func>>(); }
        { bench_AccidentallyQuadratic<emilib4::HashMap<int, int, hash_func>>(); }
    }

    if (sflags <= 12 && eflags >= 12) {
        using hash_func = default_hash<int>;

        puts("\nbench_InsertEraseContinue:");

        { bench_InsertEraseContinue<emhash6::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<emhash7::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<emhash5::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<emhash8::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<emhash9::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<ankerl::unordered_dense::map<int, int, hash_func>>(); }

#if ABSL_HMAP
        { bench_InsertEraseContinue<absl::flat_hash_map<int, int, hash_func>>(); }
#endif

        { bench_InsertEraseContinue<emilib3::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<emilib::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<emilib1m::HashMap<int, int, hash_func>>(); }
        { bench_InsertEraseContinue<emilib2::HashMap<int, int, hash_func>>(); }
    }

    if (sflags <= 13 && eflags >= 13) {
        using hash_func = default_hash<int64_t>;

        puts("\nbench_InsertEraseBegin:");

        { bench_InsertEraseBegin<emhash6::HashMap<int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<emhash7::HashMap<int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<emhash8::HashMap<int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<emhash9::HashMap<int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<ankerl::unordered_dense::map<int64_t, int, hash_func>>(); }

#if HAVE_INDIVI
        //        {  bench_InsertEraseBegin<indivi::flat_umap <int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<indivi::flat_wmap<int64_t, int, hash_func>>(); }
#endif

        { bench_InsertEraseBegin<emilib2::HashMap<int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<emilib3::HashMap<int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<emilib::HashMap<int64_t, int, hash_func>>(); }
        { bench_InsertEraseBegin<emilib1m::HashMap<int64_t, int, hash_func>>(); }
    }

    if (sflags <= 14 && eflags >= 14) {
        using hash_func = default_hash<int>;

        puts("\nbench_CreateInsert:");

        { bench_CreateInsert<emhash6::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<emhash7::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<emhash8::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<emhash9::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<ankerl::unordered_dense::map<int, int, hash_func>>(); }

#if HAVE_BOOST
        { bench_CreateInsert<boost::unordered_flat_map<int, int, hash_func>>(); }
#endif
#if HAVE_INDIVI
        { bench_CreateInsert<indivi::flat_umap<int, int, hash_func>>(); }
        { bench_CreateInsert<indivi::flat_wmap<int, int, hash_func>>(); }
#endif
#if ABSL_HMAP
        { bench_CreateInsert<absl::flat_hash_map<int, int, hash_func>>(); }
#endif

        { bench_CreateInsert<emilib2::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<emilib::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<emilib1m::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<emilib3::HashMap<int, int, hash_func>>(); }
        { bench_CreateInsert<emilib4::HashMap<int, int, hash_func>>(); }

        { bench_CreateInsert<emhash5::HashMap<int, int, hash_func>>(); }
    }

    if (sflags <= 15 && eflags >= 15) {
        using hash_func = default_hash<int>;

        puts("\nbench_udb3:");

        { bench_udb3<emhash6::HashMap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<emhash7::HashMap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<emhash8::HashMap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<emhash9::HashMap<uint32_t, uint32_t, hash_func>>(); }

        { bench_udb3<ankerl::unordered_dense::map<uint32_t, uint32_t, hash_func>>(); }

#if HAVE_BOOST
        { bench_udb3<boost::unordered_flat_map<uint32_t, uint32_t, hash_func>>(); }
#endif

#if HAVE_INDIVI
        { bench_udb3<indivi::flat_umap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<indivi::flat_wmap<uint32_t, uint32_t, hash_func>>(); }
#endif

#if ABSL_HMAP
        { bench_udb3<absl::flat_hash_map<uint32_t, uint32_t, hash_func>>(); }
#endif

        { bench_udb3<emilib2::HashMap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<emilib::HashMap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<emilib1m::HashMap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<emilib3::HashMap<uint32_t, uint32_t, hash_func>>(); }
        { bench_udb3<emilib4::HashMap<uint32_t, uint32_t, hash_func>>(); }

        { bench_udb3<emhash5::HashMap<uint32_t, uint32_t, hash_func>>(); }
    }

    printf("\ntime %7.2f s\n", now2sec() - start);
}

static void checkSet(const std::string_view& map_name) {
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

// Collapse repeated suite runs (--iter=N): one record per (test, map, size)
// group. time_sec = best (least-noise estimator for comparisons), plus median
// and stddev over all reps. group key includes map_size so size-tiered cases
// (insert 1M/10M/100M) stay separate.
static void aggregate_results() {
    std::map<std::string, std::pair<BenchResult, std::vector<double>>> groups;
    std::vector<std::string> order;
    for (const auto& r : g_results) {
        std::string key = r.test_name + "|" + r.hash_map_name + "|" + std::to_string(r.map_size);
        auto it = groups.find(key);
        if (it == groups.end()) {
            groups.emplace(key, std::make_pair(r, std::vector<double>{r.time_sec}));
            order.push_back(key);
        } else {
            it->second.second.push_back(r.time_sec);
        }
    }

    std::vector<BenchResult> aggregated;
    aggregated.reserve(order.size());
    for (const auto& key : order) {
        BenchResult rec = groups[key].first;
        std::vector<double>& times = groups[key].second;
        std::sort(times.begin(), times.end());
        rec.time_sec = times.front();
        rec.time_median = (times.size() % 2 == 1) ? times[times.size() / 2]
                                                  : 0.5 * (times[times.size() / 2 - 1] + times[times.size() / 2]);
        double mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        double sq = 0.0;
        for (double t : times)
            sq += (t - mean) * (t - mean);
        rec.time_stddev = times.size() > 1 ? std::sqrt(sq / (times.size() - 1)) : 0.0;
        if (rec.time_sec > 0 && rec.operations > 0)
            rec.throughput = rec.operations / rec.time_sec;
        aggregated.push_back(rec);
    }
    g_results.swap(aggregated);
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(0));
    printInfo(nullptr);

    // Pin to one core (skip core 0, which also services IRQs): removes
    // migration and cache-invalidation noise from per-map comparisons.
    {
        const unsigned ncpu = std::thread::hardware_concurrency();
        const unsigned core = ncpu > 2 ? 2 : 0;
#if defined(_WIN32)
        if (SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)1ull << core))
            printf("pinned to CPU core %u of %u\n", core, ncpu);
#else
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(core, &set);
        if (sched_setaffinity(0, sizeof(set), &set) == 0)
            printf("pinned to CPU core %u of %u\n", core, ncpu);
#endif
    }

    puts("usage: ./mbench [2-9mptseabrjqfx]b[d]e[d] [--warmup=N] [--iter=N] [--format=csv|json|text] [--output=FILE] "
         "[--lf=N] [--no-reserve]");
    puts("       --iter=N repeats the WHOLE suite N times and reports best/median/stddev per case");
    puts("all test case:");
    for (int i = 0; i < int(sizeof(cases) / sizeof(cases[0])); i++)
        printf("    %2d %s\n", i + 1, cases[i]);
    puts("-------------------------------------------------------------------------");

    int sflags = 1, eflags = 20;
    if (argc > 1) {
        // Parse command line arguments
        for (int arg_idx = 1; arg_idx < argc; arg_idx++) {
            const char* arg = argv[arg_idx];

            // Parse long options (--warmup, --iter, --format, etc.)
            if (arg[0] == '-' && arg[1] == '-') {
                std::string opt(arg);
                if (opt.find("--warmup=") == 0) {
                    g_config.warmup_iterations = std::stoul(opt.substr(9));
                } else if (opt.find("--iter=") == 0) {
                    g_config.benchmark_iterations = std::stoi(opt.substr(7));
                } else if (opt.find("--format=") == 0) {
                    std::string fmt = opt.substr(9);
                    if (fmt == "csv")
                        g_config.output_format = OutputFormat::CSV;
                    else if (fmt == "json")
                        g_config.output_format = OutputFormat::JSON;
                    else
                        g_config.output_format = OutputFormat::Text;
                } else if (opt.find("--output=") == 0) {
                    g_config.output_file = opt.substr(9);
                } else if (opt.find("--lf=") == 0) {
                    g_config.max_load_factor = std::stof(opt.substr(5)) / 10.0f;
                } else if (opt == "--no-reserve") {
                    g_config.enable_reserve = false;
                } else if (opt == "--no-warmup") {
                    g_config.warmup_iterations = 0;
                }
                continue;
            }

            // Parse short options (legacy format)
            for (int c = arg[0], i = 0; c != '\0'; c = arg[++i]) {
                if (c > '4' && c <= '9') {
                    std::string map_name("emhash");
                    map_name += (char)c;
                    checkSet(map_name);
                } else if (c == 'm') {
                    checkSet("ankerl");
                } else if (c == 'a')
                    checkSet("absl");
                else if (c == '1')
                    checkSet("emilib");
                else if (c == '2')
                    checkSet("emilib2");
                else if (c == '3')
                    checkSet("emilib3");
                else if (c == '4')
                    checkSet("emilib4");
                else if (c == 'x')
                    checkSet("emilib1m");
                else if (c == 'i') {
                    checkSet("flat_u");
                    checkSet("flat_w");
                } else if (c == 'b') {
                    if (isdigit(arg[i + 1]))
                        sflags = atoi(&arg[++i]);
#if HAVE_BOOST
                    else
                        checkSet("boost");
#endif
                    if (isdigit(arg[i + 1]))
                        i++;
                } else if (c == 'e') {
                    eflags = atoi(&arg[++i]);
                    if (isdigit(arg[i + 1]))
                        i++;
                } else if (c == 'l')
                    g_config.max_load_factor = float(atoi(&arg[++i]) / 10.0);
            }
        }
    }

    // Sync legacy max_lf with g_config
    max_lf = g_config.max_load_factor;

    printf("test with config: max_load_factor=%.3f, warmup=%zu, iterations=%d, format=%s, reserve=%s\n",
           g_config.max_load_factor, g_config.warmup_iterations, g_config.benchmark_iterations,
           g_config.output_format == OutputFormat::CSV    ? "csv"
           : g_config.output_format == OutputFormat::JSON ? "json"
                                                          : "text",
           g_config.enable_reserve ? "yes" : "no");
    printf("random seed: %d\n", (int)RND);
    puts("all test hashmap:");
    for (const auto& m : show_name)
        printf("%10s %20s\n", m.first.data(), m.second.data());
    puts("-------------------------------------------------------------------------");

    const int suite_reps = g_config.benchmark_iterations > 0 ? g_config.benchmark_iterations : 1;
    for (int rep = 0; rep < suite_reps; ++rep) {
        if (suite_reps > 1)
            printf("\n=== repetition %d/%d ===\n", rep + 1, suite_reps);
        runTest(sflags, eflags);
    }
    if (suite_reps > 1)
        aggregate_results();

    // Output results in specified format; in text mode still show the
    // aggregated best/median/stddev table when the suite ran multiple times.
    if (!g_results.empty() && (g_config.output_format != OutputFormat::Text || suite_reps > 1)) {
        puts("\n=== Benchmark Results (best/median/stddev) ===");
        ResultOutput::output_all(g_results, g_config.output_format);
    }

    // Write to file if specified
    if (!g_config.output_file.empty() && !g_results.empty()) {
        std::ofstream fout(g_config.output_file);
        if (fout.is_open()) {
            if (g_config.output_format == OutputFormat::CSV) {
                fout << "Test,HashMap,Time,StdDev,Operations,Throughput,LoadFactor,MapSize,MemBytes\n";
                for (const auto& r : g_results) {
                    fout << r.test_name << "," << r.hash_map_name << "," << r.time_sec << "," << r.time_stddev << ","
                         << r.operations << "," << r.throughput << "," << r.load_factor << "," << r.map_size << ","
                         << r.memory_bytes << "\n";
                }
            } else if (g_config.output_format == OutputFormat::JSON) {
                fout << "[\n";
                for (size_t i = 0; i < g_results.size(); ++i) {
                    const auto& r = g_results[i];
                    fout << "  {\n";
                    fout << "    \"test\": \"" << r.test_name << "\",\n";
                    fout << "    \"hashmap\": \"" << r.hash_map_name << "\",\n";
                    fout << "    \"time\": " << r.time_sec << ",\n";
                    fout << "    \"stddev\": " << r.time_stddev << ",\n";
                    fout << "    \"operations\": " << r.operations << ",\n";
                    fout << "    \"throughput\": " << r.throughput << ",\n";
                    fout << "    \"load_factor\": " << r.load_factor << ",\n";
                    fout << "    \"map_size\": " << r.map_size << ",\n";
                    fout << "    \"memory_bytes\": " << r.memory_bytes << "\n";
                    fout << "  }" << (i == g_results.size() - 1 ? "" : ",") << "\n";
                }
                fout << "]\n";
            }
            fout.close();
            printf("Results written to: %s\n", g_config.output_file.c_str());
        }
    }

    if (g_check_errors)
        printf("\n*** %d checksum check(s) FAILED -- results are suspect ***\n", g_check_errors);
    puts("---------------------------- all pass -----------------------------------");
    return 0;
}
