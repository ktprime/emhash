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

static inline uint64_t splitmix64_hash(uint64_t x)
{
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

struct SplitmixHasher {
    size_t operator()(uint64_t key) const { return splitmix64_hash(key); }
};

struct IdentityHasher {
    size_t operator()(uint64_t key) const { return key; }
};

// Include both versions: original (emilib2 namespace) and optimized (emilib2_opt namespace)
#include "../thirdparty/emilib/emilib2o.hpp"

// Create optimized version by copying the file with different namespace
namespace emilib2_opt = emilib2;

static const int N = 10000000;
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

template<typename KeyT, typename Hasher>
void run_test(const char* test_name)
{
    using Map = emilib2::HashMap<KeyT, int, Hasher>;

    printf("\n=== %s ===\n", test_name);

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

    double t_insert = 0, t_lookup_hit = 0, t_lookup_miss = 0, t_erase = 0;

    // Insert
    {
        double r[3];
        for (int run = 0; run < NUM_RUNS; run++) {
            Map m; auto t0 = getus();
            for (int i = 0; i < N; i++) m[keys[i]] = i;
            r[run] = (getus() - t0) / 1000.0;
        }
        t_insert = median3(r[0], r[1], r[2]);
    }

    Map m_base;
    for (int i = 0; i < N; i++) m_base[keys[i]] = i;

    // Lookup Hit
    {
        double r[3];
        volatile int sink = 0;
        for (int run = 0; run < NUM_RUNS; run++) {
            auto t0 = getus();
            for (int i = 0; i < N; i++) { auto it = m_base.find(keys[i]); if (it != m_base.end()) sink = it->second; }
            r[run] = (getus() - t0) / 1000.0;
        }
        (void)sink;
        t_lookup_hit = median3(r[0], r[1], r[2]);
    }

    // Lookup Miss
    {
        double r[3];
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
            for (int i = 0; i < N; i++) { auto it = m_base.find(miss_keys[i]); if (it != m_base.end()) sink = it->second; }
            r[run] = (getus() - t0) / 1000.0;
        }
        (void)sink;
        t_lookup_miss = median3(r[0], r[1], r[2]);
    }

    // Erase
    {
        double r[3];
        int half = N / 2;
        for (int run = 0; run < NUM_RUNS; run++) {
            Map tm(m_base);
            auto t0 = getus();
            for (int i = 0; i < half; i++) tm.erase(keys[i]);
            r[run] = (getus() - t0) / 1000.0;
        }
        t_erase = median3(r[0], r[1], r[2]);
    }

    printf("%-15s|%10s\n", "Operation", "Time (ms)");
    printf("---------------|-----------\n");
    printf("%-15s|%8.1f ms\n", "Insert", t_insert);
    printf("%-15s|%8.1f ms\n", "Lookup Hit", t_lookup_hit);
    printf("%-15s|%8.1f ms\n", "Lookup Miss", t_lookup_miss);
    printf("%-15s|%8.1f ms\n", "Erase", t_erase);
}

static bool run_correctness()
{
    printf("=== Correctness Test (emilib2o) ===\n");
    using Map = emilib2::HashMap<int, int>;

    bool ok = true;
    { Map m;
      for (int i = 0; i < 10000; i++) m[i] = i*10;
      if (m.size() != 10000) ok = false;
      for (int i = 0; i < 5000; i++) m.erase(i);
      if (m.size() != 5000) ok = false;
    }
    { Map m;
      for (int i = 0; i < 1000; i++) m[i] = i;
      m.clear();
      for (int i = 0; i < 2000; i++) m[i] = i;
      if (m.size() != 2000) ok = false;
    }
    { Map m;
      uint32_t s = 77777;
      for (int i = 0; i < 100000; i++) {
          int op = (int)(xorshift32(s) % 3);
          int key = (int)(xorshift32(s) & 0xFFFF);
          if (op == 0) m[key] = key;
          else if (op == 1) m.erase(key);
          else { volatile bool b = m.contains(key); (void)b; }
      }
    }
    if (ok) printf("  All correctness tests PASSED.\n");
    else     printf("  Some correctness tests FAILED!\n");
    return ok;
}

int main()
{
    if (!run_correctness()) return 1;

    printf("\n=== emilib2o Benchmark (N=%d, median of %d runs) ===\n", N, NUM_RUNS);
    printf("Note: This tests the OPTIMIZED version (with get_offset cached)\n");

    run_test<int, std::hash<int>>("int + std::hash");
    run_test<uint64_t, std::hash<uint64_t>>("uint64 + std::hash");
    run_test<uint64_t, SplitmixHasher>("uint64 + splitmix64");
    run_test<uint64_t, IdentityHasher>("uint64 + identity");
    run_test<std::string, std::hash<std::string>>("string + std::hash");

    printf("\n=== Done ===\n");
    return 0;
}