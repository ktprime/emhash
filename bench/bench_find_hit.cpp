/**
 * EMH_FIND_HIT performance benchmark (rigorous)
 *
 * 编译:
 *   g++ -std=c++17 -O2 -DEMH_FIND_HIT=1 -o bench_hit_on  bench_find_hit.cpp
 *   g++ -std=c++17 -O2 -DEMH_FIND_HIT=0 -o bench_hit_off bench_find_hit.cpp
 */

#ifndef EMH_FIND_HIT
#define EMH_FIND_HIT 1
#endif

#include "../hash_table5.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <chrono>
#include <vector>
#include <algorithm>

static double now_ns()
{
    return std::chrono::duration<double, std::nano>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

using Map = emhash5::HashMap<int64_t, int64_t>;
using KVec = std::vector<int64_t>;
using HVec = std::vector<size_t>;

// 单次测量：返回 ns/op
static double bench_find_hit(const Map& m, const KVec& keys, int rounds)
{
    volatile size_t sink = 0;
    double t0 = now_ns();
    for (int r = 0; r < rounds; r++) {
        for (auto k : keys) {
            auto it = m.find(k);
            if (it != m.end()) sink += it->second;
        }
    }
    (void)sink;
    return (now_ns() - t0) / (keys.size() * rounds);
}

static double bench_find_miss(const Map& m, const KVec& keys, int rounds)
{
    volatile size_t sink = 0;
    double t0 = now_ns();
    for (int r = 0; r < rounds; r++) {
        for (auto k : keys) {
            if (m.find(k) == m.end()) sink++;
        }
    }
    (void)sink;
    return (now_ns() - t0) / (keys.size() * rounds);
}

static double bench_find_hash_hit(const Map& m, const KVec& keys, const HVec& hashes, int rounds)
{
    volatile size_t sink = 0;
    double t0 = now_ns();
    for (int r = 0; r < rounds; r++) {
        for (size_t i = 0; i < keys.size(); i++) {
            auto it = m.find(keys[i], hashes[i]);
            if (it != m.end()) sink += it->second;
        }
    }
    (void)sink;
    return (now_ns() - t0) / (keys.size() * rounds);
}

static double bench_find_hash_miss(const Map& m, const KVec& keys, const HVec& hashes, int rounds)
{
    volatile size_t sink = 0;
    double t0 = now_ns();
    for (int r = 0; r < rounds; r++) {
        for (size_t i = 0; i < keys.size(); i++) {
            if (m.find(keys[i], hashes[i]) == m.end()) sink++;
        }
    }
    (void)sink;
    return (now_ns() - t0) / (keys.size() * rounds);
}

// 多轮取中位数，每轮内部跑多次迭代
static double measure(double (*fn)(const Map&, const KVec&, int),
                      const Map& m, const KVec& v, int inner, int outer)
{
    std::vector<double> times;
    for (int i = 0; i < outer; i++) {
        double t = fn(m, v, inner);
        times.push_back(t);
    }
    // 去掉最高最低各 20%，取平均
    std::sort(times.begin(), times.end());
    int trim = outer / 5;
    double sum = 0;
    for (int i = trim; i < outer - trim; i++)
        sum += times[i];
    return sum / (outer - 2 * trim);
}

static double measure_h(double (*fn)(const Map&, const KVec&, const HVec&, int),
                        const Map& m, const KVec& v, const HVec& h, int inner, int outer)
{
    std::vector<double> times;
    for (int i = 0; i < outer; i++) {
        double t = fn(m, v, h, inner);
        times.push_back(t);
    }
    std::sort(times.begin(), times.end());
    int trim = outer / 5;
    double sum = 0;
    for (int i = trim; i < outer - trim; i++)
        sum += times[i];
    return sum / (outer - 2 * trim);
}

int main()
{
    const int N = 500000;
    const int SAMPLE = 100000;
    const int INNER = 5;   // 每次测量内部迭代轮数
    const int OUTER = 30;  // 外层测量次数

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int64_t> dist(1, 2000000000);

    KVec keys(N), missing(SAMPLE);
    for (int i = 0; i < N; i++) {
        int64_t k;
        do { k = dist(rng); } while (k == -1 || k == -2);
        keys[i] = k;
    }
    for (int i = 0; i < SAMPLE; i++) {
        int64_t k;
        do { k = -dist(rng) - 100; } while (k == -1 || k == -2);
        missing[i] = k;
    }

    Map map(N);
    for (int i = 0; i < N; i++)
        map[keys[i]] = i;

    KVec sample_keys(keys.begin(), keys.begin() + SAMPLE);

    HVec hit_hashes(SAMPLE), miss_hashes(SAMPLE);
    std::hash<int64_t> hasher;
    for (int i = 0; i < SAMPLE; i++) {
        hit_hashes[i]  = (size_t)(int32_t)hasher(keys[i]);
        miss_hashes[i] = (size_t)(int32_t)hasher(missing[i]);
    }

    double f_hit   = measure(bench_find_hit,      map, sample_keys, INNER, OUTER);
    double f_miss  = measure(bench_find_miss,     map, missing,     INNER, OUTER);
    double fh_hit  = measure_h(bench_find_hash_hit,  map, sample_keys, hit_hashes,  INNER, OUTER);
    double fh_miss = measure_h(bench_find_hash_miss, map, missing, miss_hashes, INNER, OUTER);

    printf("EMH_FIND_HIT=%d  map=%d/%d load=%.2f\n", EMH_FIND_HIT, (int)map.size(), (int)map.bucket_count(), map.load_factor());
    printf("============================================\n");
    printf("  find(key)      hit: %7.2f  miss: %7.2f  ns\n", f_hit, f_miss);
    printf("  find(key,hash) hit: %7.2f  miss: %7.2f  ns\n", fh_hit, fh_miss);

    return 0;
}