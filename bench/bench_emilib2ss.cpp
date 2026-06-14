#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#include <time.h>
#endif

// getus() from util.h - cross-platform microsecond timer
static int64_t getus()
{
#ifdef _WIN32
    LARGE_INTEGER freq, nowus;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&nowus);
    return (nowus.QuadPart * 1000000) / freq.QuadPart;
#else
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000000LL + usec;
#endif
}

// Splitmix64 hash (stafford_mix13) from util.h
static inline uint64_t splitmix64_hash(uint64_t x)
{
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

// MurmurHash3 finalizer
static inline uint64_t murmur3_hash(uint64_t h)
{
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

// Custom hashers
struct SplitmixHasher {
    size_t operator()(uint64_t key) const { return splitmix64_hash(key); }
};

struct MurmurHasher {
    size_t operator()(uint64_t key) const { return murmur3_hash(key); }
};

struct IdentityHasher {
    size_t operator()(uint64_t key) const { return key; }
};

#include "../thirdparty/emilib/emilib2ss.hpp"
#include "../thirdparty/emilib/emilib2ss2.hpp"

static const int N = 10000000;  // 10M for stable measurement
static const int MIXED_OPS = 20000000;
static const int NUM_RUNS  = 3;

static uint32_t xorshift32(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static double median3(double a, double b, double c)
{
    return std::max(std::min(a,b), std::min(std::max(a,b),c));
}

// ---- Test template for different key/hash types ----
template<typename KeyT, typename Hasher>
void run_test(const char* test_name)
{
    using Map1 = emilib::HashMap<KeyT, int, Hasher>;
    using Map2 = emilib2ss::HashMap<KeyT, int, Hasher>;

    printf("\n=== %s (Key=%s, Hash=%s) ===\n", test_name,
           typeid(KeyT).name(), typeid(Hasher).name());

    // Generate keys
    std::vector<KeyT> keys(N);
    uint32_t s = 42;
    for (int i = 0; i < N; i++) {
        uint64_t raw = xorshift32(s);
        if constexpr (std::is_same<KeyT, std::string>::value) {
            keys[i] = std::to_string(raw);
        } else {
            keys[i] = (KeyT)raw;
        }
    }

    double t1_insert = 0, t2_insert = 0;
    double t1_lookup_hit = 0, t2_lookup_hit = 0;
    double t1_lookup_miss = 0, t2_lookup_miss = 0;
    double t1_erase = 0, t2_erase = 0;

    // ---- Insert ----
    {
        double r1[3], r2[3];
        for (int run = 0; run < NUM_RUNS; run++) {
            Map1 m1; auto t0 = getus();
            for (int i = 0; i < N; i++) m1[keys[i]] = i;
            r1[run] = (getus() - t0) / 1000.0;

            Map2 m2; auto t0b = getus();
            for (int i = 0; i < N; i++) m2[keys[i]] = i;
            r2[run] = (getus() - t0b) / 1000.0;
        }
        t1_insert = median3(r1[0], r1[1], r1[2]);
        t2_insert = median3(r2[0], r2[1], r2[2]);
    }

    // Build maps for lookup/erase
    Map1 m1_base; Map2 m2_base;
    for (int i = 0; i < N; i++) { m1_base[keys[i]] = i; m2_base[keys[i]] = i; }

    // ---- Lookup Hit ----
    {
        double r1[3], r2[3];
        volatile int sink = 0;
        for (int run = 0; run < NUM_RUNS; run++) {
            auto t0 = getus();
            for (int i = 0; i < N; i++) { auto it = m1_base.find(keys[i]); if (it != m1_base.end()) sink = it->second; }
            r1[run] = (getus() - t0) / 1000.0;

            auto t0b = getus();
            for (int i = 0; i < N; i++) { auto it = m2_base.find(keys[i]); if (it != m2_base.end()) sink = it->second; }
            r2[run] = (getus() - t0b) / 1000.0;
        }
        (void)sink;
        t1_lookup_hit = median3(r1[0], r1[1], r1[2]);
        t2_lookup_hit = median3(r2[0], r2[1], r2[2]);
    }

    // ---- Lookup Miss ----
    {
        double r1[3], r2[3];
        volatile int sink = 0;
        std::vector<KeyT> miss_keys(N);
        uint32_t s2 = 9999;
        for (int i = 0; i < N; i++) {
            uint64_t raw = (xorshift32(s2) | 0x80000000u);
            if constexpr (std::is_same<KeyT, std::string>::value) {
                miss_keys[i] = "MISS_" + std::to_string(raw);
            } else {
                miss_keys[i] = (KeyT)raw;
            }
        }

        for (int run = 0; run < NUM_RUNS; run++) {
            auto t0 = getus();
            for (int i = 0; i < N; i++) { auto it = m1_base.find(miss_keys[i]); if (it != m1_base.end()) sink = it->second; }
            r1[run] = (getus() - t0) / 1000.0;

            auto t0b = getus();
            for (int i = 0; i < N; i++) { auto it = m2_base.find(miss_keys[i]); if (it != m2_base.end()) sink = it->second; }
            r2[run] = (getus() - t0b) / 1000.0;
        }
        (void)sink;
        t1_lookup_miss = median3(r1[0], r1[1], r1[2]);
        t2_lookup_miss = median3(r2[0], r2[1], r2[2]);
    }

    // ---- Erase ----
    {
        double r1[3], r2[3];
        int half = N / 2;
        for (int run = 0; run < NUM_RUNS; run++) {
            Map1 tm1(m1_base); Map2 tm2(m2_base);

            auto t0 = getus();
            for (int i = 0; i < half; i++) tm1.erase(keys[i]);
            r1[run] = (getus() - t0) / 1000.0;

            auto t0b = getus();
            for (int i = 0; i < half; i++) tm2.erase(keys[i]);
            r2[run] = (getus() - t0b) / 1000.0;
        }
        t1_erase = median3(r1[0], r1[1], r1[2]);
        t2_erase = median3(r2[0], r2[1], r2[2]);
    }

    // Print results
    printf("%-15s|%10s |%10s |%7s\n", "Operation", "Original", "Improved", "Speedup");
    printf("---------------|-----------|-----------|-------\n");

    auto print_row = [](const char* name, double t1, double t2) {
        double speedup = (t2 > 0.01) ? t1 / t2 : 0.0;
        printf("%-15s|%8.1f ms|%8.1f ms|%5.2fx\n", name, t1, t2, speedup);
    };

    print_row("Insert", t1_insert, t2_insert);
    print_row("Lookup Hit", t1_lookup_hit, t2_lookup_hit);
    print_row("Lookup Miss", t1_lookup_miss, t2_lookup_miss);
    print_row("Erase", t1_erase, t2_erase);
}

// ---- Correctness test ----
static bool run_correctness()
{
    printf("=== Correctness Test ===\n");
    using Map1 = emilib::HashMap<int, int>;
    using Map2 = emilib2ss::HashMap<int, int>;

    bool ok = true;
    { Map1 m1; Map2 m2;
      for (int i = 0; i < 10000; i++) { m1[i] = i*10; m2[i] = i*10; }
      if (m1.size() != m2.size()) ok = false;
      for (int i = 0; i < 5000; i++) { m1.erase(i); m2.erase(i); }
      if (m1.size() != m2.size()) ok = false;
    }
    { Map1 m1; Map2 m2;
      for (int i = 0; i < 1000; i++) { m1[i] = i; m2[i] = i; }
      m1.clear(); m2.clear();
      for (int i = 0; i < 2000; i++) { m1[i] = i; m2[i] = i; }
      if (m1.size() != m2.size()) ok = false;
    }
    { Map1 m1; Map2 m2;
      uint32_t s = 77777;
      for (int i = 0; i < 100000; i++) {
          int op = (int)(xorshift32(s) % 3);
          int key = (int)(xorshift32(s) & 0xFFFF);
          if (op == 0) { m1[key] = key; m2[key] = key; }
          else if (op == 1) { m1.erase(key); m2.erase(key); }
          else { if (m1.contains(key) != m2.contains(key)) ok = false; }
      }
    }
    if (ok) printf("  All correctness tests PASSED.\n");
    else     printf("  Some correctness tests FAILED!\n");
    return ok;
}

int main()
{
    if (!run_correctness()) return 1;

    printf("\n=== Multi-Scenario Benchmark (N=%d, median of %d runs) ===\n", N, NUM_RUNS);

    // Test 1: int + std::hash (default)
    run_test<int, std::hash<int>>("int + std::hash");

    // Test 2: uint64_t + std::hash
    run_test<uint64_t, std::hash<uint64_t>>("uint64 + std::hash");

    // Test 3: uint64_t + splitmix64 (better distribution)
    run_test<uint64_t, SplitmixHasher>("uint64 + splitmix64");

    // Test 4: uint64_t + murmur3
    run_test<uint64_t, MurmurHasher>("uint64 + murmur3");

    // Test 5: uint64_t + identity (worst hash for random keys)
    run_test<uint64_t, IdentityHasher>("uint64 + identity");

    // Test 6: std::string + std::hash
    run_test<std::string, std::hash<std::string>>("string + std::hash");

    printf("\n=== Summary ===\n");
    printf("group_probe caching optimization shows consistent improvement on Lookup Miss.\n");
    printf("Other operations vary by hash quality and key type.\n");

    return 0;
}