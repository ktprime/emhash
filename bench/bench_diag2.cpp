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

    // Generate random keys once
    std::mt19937_64 rng(42);
    std::vector<KeyInt> keys(N), miss_keys(N);
    for (int i = 0; i < N; i++) {
        keys[i] = rng();
        miss_keys[i] = rng();
    }

    // emihmap4
    {
        emilib4::HashMap<KeyInt, ValInt> m;
        auto t0 = now_us();
        for (auto k : keys) m[k] = k;
        auto t_ins = now_us() - t0;
        printf("emihmap4: insert=%lldus cap=%zu lf=%.3f\n",
               (long long)t_ins, m.bucket_count(),
               (double)m.size() / m.bucket_count());

        volatile size_t sink = 0;
        t0 = now_us();
        for (auto k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
        auto t_hit = now_us() - t0;

        t0 = now_us();
        for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        auto t_miss = now_us() - t0;
        (void)sink;

        printf("         findHit=%lldus findMiss=%lldus\n", (long long)t_hit, (long long)t_miss);

        // Now erase half, then benchmark
        for (int i = 0; i < N/2; i++) m.erase(keys[i]);
        printf("         after erase half: size=%zu cap=%zu lf=%.3f\n",
               m.size(), m.bucket_count(), (double)m.size() / m.bucket_count());

        t0 = now_us();
        for (int i = N/2; i < N; i++) { auto it = m.find(keys[i]); if (it != m.end()) sink += it->second; }
        auto t_hit2 = now_us() - t0;

        t0 = now_us();
        for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        auto t_miss2 = now_us() - t0;
        printf("         findHit(half)=%lldus findMiss(half)=%lldus\n", (long long)t_hit2, (long long)t_miss2);
    }

    // Boost
    {
        boost::unordered_flat_map<KeyInt, ValInt> m;
        auto t0 = now_us();
        for (auto k : keys) m[k] = k;
        auto t_ins = now_us() - t0;
        printf("boost:   insert=%lldus cap=%zu lf=%.3f\n",
               (long long)t_ins, m.bucket_count(),
               (double)m.size() / m.bucket_count());

        volatile size_t sink = 0;
        t0 = now_us();
        for (auto k : keys) { auto it = m.find(k); if (it != m.end()) sink += it->second; }
        auto t_hit = now_us() - t0;

        t0 = now_us();
        for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        auto t_miss = now_us() - t0;
        (void)sink;

        printf("         findHit=%lldus findMiss=%lldus\n", (long long)t_hit, (long long)t_miss);

        for (int i = 0; i < N/2; i++) m.erase(keys[i]);
        printf("         after erase half: size=%zu cap=%zu lf=%.3f\n",
               m.size(), m.bucket_count(), (double)m.size() / m.bucket_count());

        t0 = now_us();
        for (int i = N/2; i < N; i++) { auto it = m.find(keys[i]); if (it != m.end()) sink += it->second; }
        auto t_hit2 = now_us() - t0;

        t0 = now_us();
        for (auto k : miss_keys) { if (m.find(k) == m.end()) sink++; }
        auto t_miss2 = now_us() - t0;
        printf("         findHit(half)=%lldus findMiss(half)=%lldus\n", (long long)t_hit2, (long long)t_miss2);
    }

    return 0;
}
