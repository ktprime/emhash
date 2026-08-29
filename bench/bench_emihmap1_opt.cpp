// =============================================================================
// Benchmark: emihmap1 (original) vs emihmap1_opt (optimized)
//
// Build (run from d:\emhash\bench\):
//   g++-16 -std=c++17 -O3 -march=native -msse2 -DNOMINMAX \
//     -I../include -I../thirdparty \
//     bench_emihmap1_opt.cpp -o bench_emihmap1_opt_g16 -lpthread
//
//   clang++-20 -std=c++17 -O3 -march=native -msse2 -DNOMINMAX \
//     -I../include -I../thirdparty \
//     bench_emihmap1_opt.cpp -o bench_emihmap1_opt_c20 -lpthread
//
//   MSVC (developer prompt):
//     cl /std:c++17 /O2 /arch:AVX2 /EHsc /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS \
//       /I..\include /I..\thirdparty bench_emihmap1_opt.cpp
//
// Usage:
//   ./bench_emihmap1_opt_g16            # full suite
//   ./bench_emihmap1_opt_g16 --quick    # quick run (smaller sizes)
// =============================================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "emilib/emihmap1.hpp"     // original (namespace emilib)
#include "emilib/emihmap1_opt.hpp" // optimized  (namespace emilib1_opt)

// -----------------------------------------------------------------------------
// Timing helpers
// -----------------------------------------------------------------------------
using Clock = std::chrono::steady_clock;

static inline long long micros_since(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
}

// -----------------------------------------------------------------------------
// PRNG — fast, deterministic across compilers (WyRand variant)
// -----------------------------------------------------------------------------
static inline uint64_t wyrand(uint64_t& s) {
    s ^= 0xa0761d6478bd642full;
    uint64_t hi, lo;
#if defined(__SIZEOF_INT128__)
    __uint128_t r = (__uint128_t)s * (s ^ 0xe7037ed1a0b428dbull);
    lo = (uint64_t)r;
    hi = (uint64_t)(r >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
    lo = _umul128(s, s ^ 0xe7037ed1a0b428dbull, &hi);
#else
    uint64_t a = s, b = s ^ 0xe7037ed1a0b428dbull;
    uint64_t ha = a >> 32, hb = b >> 32, la = (uint32_t)a, lb = (uint32_t)b;
    lo = la * lb;
    hi = ha * hb + (ha * lb + hb * la) / 0x100000000ull;
#endif
    s = hi ^ lo;
    return s;
}

// -----------------------------------------------------------------------------
// Memory measurement (Linux: /proc/self/status; Windows: GetProcessMemoryInfo)
// -----------------------------------------------------------------------------
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
static size_t peak_rss_kb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.PeakWorkingSetSize / 1024;
    return 0;
}
#else
#include <sys/resource.h>
static size_t peak_rss_kb() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (size_t)ru.ru_maxrss; // kB on Linux
}
#endif

// -----------------------------------------------------------------------------
// Compiler identification string
// -----------------------------------------------------------------------------
static const char* compiler_name() {
#if defined(__clang__)
    return "clang++";
#elif defined(__GNUC__)
    return "g++";
#elif defined(_MSC_VER)
    return "MSVC";
#else
    return "unknown";
#endif
}

static const char* compiler_version() {
    static char buf[64];
#if defined(__clang__)
    std::snprintf(buf, sizeof(buf), "%d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    std::snprintf(buf, sizeof(buf), "%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    std::snprintf(buf, sizeof(buf), "%d", _MSC_VER);
#else
    std::snprintf(buf, sizeof(buf), "?");
#endif
    return buf;
}

// -----------------------------------------------------------------------------
// Key generators
// -----------------------------------------------------------------------------
struct SeqInt {
    using key_t = int64_t;
    static const char* name() { return "Sequential int64"; }
    static std::vector<key_t> gen(size_t n) {
        std::vector<key_t> v(n);
        for (size_t i = 0; i < n; i++) v[i] = (key_t)i + 1; // 1..n
        return v;
    }
};

struct RandInt {
    using key_t = int64_t;
    static const char* name() { return "Random int64"; }
    static std::vector<key_t> gen(size_t n) {
        std::vector<key_t> v(n);
        uint64_t s = 0x123456789abcdefULL;
        for (size_t i = 0; i < n; i++) v[i] = (key_t)(wyrand(s) & (((uint64_t)1 << 40) - 1));
        return v;
    }
};

struct RandStr {
    using key_t = std::string;
    static const char* name() { return "Random string(8-32)"; }
    static std::vector<key_t> gen(size_t n) {
        std::vector<key_t> v(n);
        uint64_t s = 0xdeadbeefcafe1234ULL;
        static const char* alpha = "abcdefghijklmnopqrstuvwxyz0123456789";
        for (size_t i = 0; i < n; i++) {
            size_t len = 8 + (wyrand(s) % 25);
            std::string str(len, ' ');
            for (size_t j = 0; j < len; j++) str[j] = alpha[wyrand(s) % 36];
            v[i] = std::move(str);
        }
        return v;
    }
};

// -----------------------------------------------------------------------------
// Benchmark harness for a single map type
// -----------------------------------------------------------------------------
template <typename MapT> struct MapBench {
    const char* name;
    MapBench(const char* n) : name(n) {}
};

// Run a single op on a map and return microseconds.
template <typename MapT, typename KeyT>
static long long run_insert(MapT& m, const std::vector<KeyT>& keys) {
    auto t0 = Clock::now();
    for (const auto& k : keys) m[k] = KeyT{};
    return micros_since(t0);
}

template <typename MapT, typename KeyT>
static long long run_find_hit(const MapT& m, const std::vector<KeyT>& keys) {
    volatile long long sink = 0; // prevent DCE
    auto t0 = Clock::now();
    for (const auto& k : keys) {
        auto it = m.find(k);
        sink += (it != m.end()) ? 1 : 0;
    }
    auto us = micros_since(t0);
    (void)sink;
    return us;
}

template <typename MapT, typename KeyT>
static long long run_find_miss(const MapT& m, const std::vector<KeyT>& keys) {
    volatile long long sink = 0;
    auto t0 = Clock::now();
    for (const auto& k : keys) {
        // search for a key that is NOT in the map (k + large offset)
        auto miss = k;
        if constexpr (std::is_integral<KeyT>::value) miss = k + 0x7FFFFFFFFFFFFFFFULL;
        else miss = std::string("__miss_") + miss;
        auto it = m.find(miss);
        sink += (it != m.end()) ? 1 : 0;
    }
    auto us = micros_since(t0);
    (void)sink;
    return us;
}

template <typename MapT, typename KeyT>
static long long run_erase(MapT& m, const std::vector<KeyT>& keys) {
    auto t0 = Clock::now();
    for (const auto& k : keys) m.erase(k);
    return micros_since(t0);
}

template <typename MapT> static long long run_iterate(const MapT& m) {
    volatile size_t sink = 0;
    auto t0 = Clock::now();
    for (auto it = m.begin(); it != m.end(); ++it) sink += (size_t)&(*it);
    auto us = micros_since(t0);
    (void)sink;
    return us;
}

// -----------------------------------------------------------------------------
// Print helpers
// -----------------------------------------------------------------------------
struct Result {
    long long insert, find_hit, find_miss, erase, iterate;
    size_t bucket_count;
    size_t peak_rss;
};

template <typename KeyGen>
static Result bench_original(const std::vector<typename KeyGen::key_t>& keys) {
    using KeyT = typename KeyGen::key_t;
    emilib::HashMap<KeyT, KeyT> m;
    Result r{};
    r.insert = run_insert(m, keys);
    r.find_hit = run_find_hit(m, keys);
    r.find_miss = run_find_miss(m, keys);
    r.iterate = run_iterate(m);
    r.bucket_count = m.bucket_count();
    r.peak_rss = peak_rss_kb();
    r.erase = run_erase(m, keys);
    return r;
}

template <typename KeyGen>
static Result bench_optimized(const std::vector<typename KeyGen::key_t>& keys) {
    using KeyT = typename KeyGen::key_t;
    emilib1_opt::HashMap<KeyT, KeyT> m;
    Result r{};
    r.insert = run_insert(m, keys);
    r.find_hit = run_find_hit(m, keys);
    r.find_miss = run_find_miss(m, keys);
    r.iterate = run_iterate(m);
    r.bucket_count = m.bucket_count();
    r.peak_rss = peak_rss_kb();
    r.erase = run_erase(m, keys);
    return r;
}

// Pretty-print a comparison row
static void print_header(const char* title) {
    std::printf("\n=== %s (us) ===\n", title);
    std::printf("  %-22s %10s %10s %10s %10s %10s\n", "Map", "Insert", "FindHit", "FindMiss", "Erase", "Iterate");
}

static void print_row(const char* name, const Result& r) {
    std::printf("  %-22s %10lld %10lld %10lld %10lld %10lld\n", name, r.insert, r.find_hit, r.find_miss, r.erase,
                r.iterate);
}

static void print_speedup(const Result& orig, const Result& opt) {
    auto spd = [](long long o, long long n) {
        if (n == 0) return 1.0;
        return (double)o / (double)n;
    };
    std::printf("  %-22s %9.2fx %9.2fx %9.2fx %9.2fx %9.2fx\n", "speedup(opt/orig)", spd(orig.insert, opt.insert),
                spd(orig.find_hit, opt.find_hit), spd(orig.find_miss, opt.find_miss), spd(orig.erase, opt.erase),
                spd(orig.iterate, opt.iterate));
}

static void print_memory(const Result& orig, const Result& opt, size_t n) {
    std::printf("\n=== Memory (peak RSS KB, N=%zu) ===\n", n);
    std::printf("  original : %zu KB  (buckets=%zu)\n", orig.peak_rss, orig.bucket_count);
    std::printf("  optimized: %zu KB  (buckets=%zu)\n", opt.peak_rss, opt.bucket_count);
    long long diff = (long long)opt.peak_rss - (long long)orig.peak_rss;
    std::printf("  delta    : %lld KB\n", diff);
}

// -----------------------------------------------------------------------------
// Correctness check
// -----------------------------------------------------------------------------
static bool verify_correctness() {
    std::printf("=== Correctness verification ===\n");
    // int keys
    {
        emilib::HashMap<int, int> a;
        emilib1_opt::HashMap<int, int> b;
        for (int i = 0; i < 1000; i++) {
            a[i] = i * 2;
            b[i] = i * 2;
        }
        for (int i = 0; i < 1000; i++) {
            auto fa = a.find(i);
            auto fb = b.find(i);
            if ((fa == a.end()) != (fb == b.end())) {
                std::printf("  FAIL[int]: key %d presence mismatch\n", i);
                return false;
            }
            if (fa != a.end() && fa->second != fb->second) {
                std::printf("  FAIL[int]: key %d value mismatch %d vs %d\n", i, fa->second, fb->second);
                return false;
            }
        }
        // erase half
        for (int i = 0; i < 500; i++) {
            a.erase(i);
            b.erase(i);
        }
        if (a.size() != b.size()) {
            std::printf("  FAIL[int]: size mismatch after erase %zu vs %zu\n", (size_t)a.size(), (size_t)b.size());
            return false;
        }
    }
    // string keys
    {
        emilib::HashMap<std::string, int> a;
        emilib1_opt::HashMap<std::string, int> b;
        for (int i = 0; i < 500; i++) {
            auto k = std::to_string(i * 7 + 3);
            a[k] = i;
            b[k] = i;
        }
        for (int i = 0; i < 500; i++) {
            auto k = std::to_string(i * 7 + 3);
            auto fa = a.find(k);
            auto fb = b.find(k);
            if ((fa == a.end()) != (fb == b.end())) {
                std::printf("  FAIL[str]: key %s presence mismatch\n", k.c_str());
                return false;
            }
            if (fa != a.end() && fa->second != fb->second) {
                std::printf("  FAIL[str]: key %s value mismatch\n", k.c_str());
                return false;
            }
        }
    }
    // sequential int keys (the clustering stress test for OPT-1)
    {
        emilib::HashMap<int64_t, int64_t> a;
        emilib1_opt::HashMap<int64_t, int64_t> b;
        for (int64_t i = 1; i <= 10000; i++) {
            a[i] = i;
            b[i] = i;
        }
        if (a.size() != b.size()) {
            std::printf("  FAIL[seq]: size mismatch %zu vs %zu\n", (size_t)a.size(), (size_t)b.size());
            return false;
        }
        // every key must be findable
        for (int64_t i = 1; i <= 10000; i++) {
            if (b.find(i) == b.end()) {
                std::printf("  FAIL[seq]: key %lld missing in optimized\n", (long long)i);
                return false;
            }
        }
    }
    std::printf("  PASS: int, string, and sequential-key correctness verified.\n");
    return true;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
    bool quick = argc > 1 && std::strcmp(argv[1], "--quick") == 0;

    std::printf("=== Compiler: %s ===\n", compiler_name());
    std::printf(" Version: %s\n", compiler_version());
#if defined(__x86_64__) || defined(_M_X64)
    std::printf(" Arch: x86_64\n");
#elif defined(__aarch64__) || defined(_M_ARM64)
    std::printf(" Arch: aarch64\n");
#endif
#ifdef AVX2_EHASH
    std::printf(" SIMD: AVX2\n");
#else
    std::printf(" SIMD: SSE2\n");
#endif

    if (!verify_correctness()) {
        std::printf("Correctness check FAILED — aborting benchmark.\n");
        return 1;
    }

    // Benchmark sizes
    std::vector<size_t> sizes;
    if (quick) {
        sizes = {1000, 10000, 100000};
    } else {
        sizes = {1000, 10000, 100000, 1000000, 5000000};
    }

    // Run each key type × size
    auto run_key_type = [](auto kg, size_t n) {
        using KeyGen = decltype(kg);
        auto keys = KeyGen::gen(n);
        print_header(KeyGen::name());
        std::printf("  [N = %zu]\n", n);
        // baseline RSS before
        peak_rss_kb();
        auto ro = bench_original<KeyGen>(keys);
        print_row("emihmap1(orig)", ro);
        auto rn = bench_optimized<KeyGen>(keys);
        print_row("emihmap1(opt)", rn);
        print_speedup(ro, rn);
        print_memory(ro, rn, n);
    };

    for (size_t n : sizes) {
        run_key_type(SeqInt{}, n);
        run_key_type(RandInt{}, n);
        run_key_type(RandStr{}, n);
    }

    // Iterated insert/erase stress test
    {
        std::printf("\n=== Iterated Insert/Erase stress (us) ===\n");
        const size_t n = quick ? 100000 : 1000000;
        auto keys = RandInt::gen(n);
        auto run_stress = [&](auto& m, const char* name) {
            auto t0 = Clock::now();
            for (int round = 0; round < 5; round++) {
                for (auto k : keys) m[k] = k;
                for (auto k : keys) m.erase(k);
            }
            auto us = micros_since(t0);
            // final find
            auto t1 = Clock::now();
            long long hit = 0;
            for (auto k : keys) hit += (m.find(k) != m.end()) ? 1 : 0;
            auto fus = micros_since(t1);
            std::printf("  %-22s Ins+Era x5=%lldus  FindHit=%lldus  FinalSize=%zu\n", name, us, fus, (size_t)m.size());
        };
        {
            emilib::HashMap<int64_t, int64_t> m;
            run_stress(m, "emihmap1(orig)");
        }
        {
            emilib1_opt::HashMap<int64_t, int64_t> m;
            run_stress(m, "emihmap1(opt)");
        }
    }

    std::printf("\n=== Done ===\n");
    return 0;
}
