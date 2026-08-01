#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>
#include <chrono>

static int64_t now_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main() {
    using KeyInt = uint64_t;
    using ValInt = uint64_t;
    const int N = 1000000;

    std::mt19937_64 rng(42);
    std::vector<KeyInt> keys(N), miss_keys(N);
    for (int i = 0; i < N; i++) { keys[i] = rng(); miss_keys[i] = rng(); }

    {
        emilib4::HashMap<KeyInt, ValInt> m;
        for (auto k : keys) m[k] = k;
        printf("emihmap4: cap=%zu size=%zu lf=%.3f\n", m.bucket_count(), m.size(), (double)m.size()/m.bucket_count());

        volatile size_t sink = 0;
        for (int i = 0; i < 1000; i++) m.find(keys[i]);

        auto t0 = now_us();
        for (auto k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
        printf("  findHit=%lld us\n", (long long)(now_us() - t0));

        t0 = now_us();
        for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        printf("  findMiss=%lld us\n", (long long)(now_us() - t0));
        (void)sink;
    }

    {
        boost::unordered_flat_map<KeyInt, ValInt> m;
        for (auto k : keys) m[k] = k;
        printf("boost:   cap=%zu size=%zu lf=%.3f\n", m.bucket_count(), m.size(), (double)m.size()/m.bucket_count());

        volatile size_t sink = 0;
        for (int i = 0; i < 1000; i++) m.find(keys[i]);

        auto t0 = now_us();
        for (auto k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
        printf("  findHit=%lld us\n", (long long)(now_us() - t0));

        t0 = now_us();
        for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        printf("  findMiss=%lld us\n", (long long)(now_us() - t0));
        (void)sink;
    }

    return 0;
}
