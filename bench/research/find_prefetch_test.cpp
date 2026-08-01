/**
 * Targeted test: find with/without prefetch on clang++-20
 * Hypothesis: __builtin_prefetch in find hot path hurts clang performance
 */
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO626_REMOVED_WARNING
#define NDEBUG

// Test 1: With prefetch (default)
#include "emilib/emihmap4_opt.hpp"

#include <cstdio>
#include <cstdint>
#include <chrono>
#include <vector>

static int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static uint64_t wyrand(uint64_t& s) {
    s += 0xa0761d6478bd642full;
    uint64_t t = s ^ 0xe7037ed1a0b428dbull;
    uint64_t r = t ^ (t >> 33);
    r *= (t >> 31 | t << 33) ^ 0x74743c1bu;
    return r ^ (r >> 33);
}

template<typename Map>
int64_t measure_find_hit(Map& m, const std::vector<uint64_t>& keys, int iters) {
    volatile size_t sink = 0;
    int64_t total = 0;
    for (int i = 0; i < iters; i++) {
        auto t0 = now_ns();
        size_t sum = 0;
        for (auto& k : keys) {
            auto it = m.find(k);
            if (it != m.end()) sum += it->second;
        }
        total += now_ns() - t0;
        sink += sum;
    }
    (void)sink;
    return total / iters;
}

int main() {
#ifdef __clang__
    printf("Compiler: clang++ %d.%d\n", __clang_major__, __clang_minor__);
#elif defined(__GNUC__)
    printf("Compiler: g++ %d.%d\n", __GNUC__, __GNUC_MINOR__);
#endif

    // Check if EMH_NO_READ_PREFETCH is defined
#ifdef EMH_NO_READ_PREFETCH
    printf("Prefetch: DISABLED (EMH_NO_READ_PREFETCH)\n");
#else
    printf("Prefetch: ENABLED (default)\n");
#endif

    for (auto N : {100000, 1000000}) {
        uint64_t seed = 42;
        std::vector<uint64_t> keys(N);
        for (int i = 0; i < N; i++) keys[i] = wyrand(seed);

        emilib4_opt::HashMap<uint64_t, uint64_t> em;
        em.reserve(N * 2);
        for (auto& k : keys) em[k] = 1;

        // Warmup
        volatile size_t sink = 0;
        for (int i = 0; i < 1000; i++) sink += (em.find(keys[i]) != em.end()) ? 1 : 0;
        (void)sink;

        auto t = measure_find_hit(em, keys, 5);
        printf("  N=%d: find_hit = %lld ns (%.1f ns/key)\n", N, (long long)t, (double)t / N);
    }
    return 0;
}
