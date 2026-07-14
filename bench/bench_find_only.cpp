// Microbenchmark: isolate find performance
// Pre-build map, then measure only find() calls
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap2_v2.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

template <typename Body>
double bench_median(int runs, Body body) {
    vector<double> times;
    times.reserve(runs);
    for (int i = 0; i < runs; i++) {
        auto s = chrono::high_resolution_clock::now();
        body();
        auto e = chrono::high_resolution_clock::now();
        times.push_back(chrono::duration<double, chrono::nanoseconds::period>(e - s).count());
    }
    sort(times.begin(), times.end());
    return times[runs / 2];
}

int main() {
    const int N = 500000;
    const int RUNS = 30;
    mt19937_64 rng(42);

    cout << "========================================================\n";
    cout << "  Find-Only Microbenchmark (N=" << N << ", " << RUNS << " runs, median)\n";
    cout << "========================================================\n";

    // Generate keys
    vector<int> keys(N);
    for (int i = 0; i < N; i++) keys[i] = rng() % (N * 4);

    // Build maps once
    emilib2::HashMap<int, int> m1;
    m1.reserve(N);
    for (int k : keys) m1[k] = k;

    emilib2_v2::HashMap<int, int> m2;
    m2.reserve(N);
    for (int k : keys) m2[k] = k;

    // Hit lookup keys (all exist)
    vector<int> hit_keys(N * 3);
    for (int i = 0; i < hit_keys.size(); i++) hit_keys[i] = keys[rng() % N];

    // Miss lookup keys (none exist)
    vector<int> miss_keys(N * 3);
    for (int i = 0; i < miss_keys.size(); i++) miss_keys[i] = rng() + N * 10000;

    cout << fixed << setprecision(3);
    cout << left << setw(22) << "Test" << right
         << setw(12) << "emilib2" << setw(12) << "v2"
         << setw(10) << "delta" << "\n";
    cout << string(56, '-') << "\n";

    // Find hit
    double t1 = bench_median(RUNS, [&]() {
        volatile int sum = 0;
        for (int k : hit_keys) { auto it = m1.find(k); if (it != m1.end()) sum += it->second; }
    });
    double t2 = bench_median(RUNS, [&]() {
        volatile int sum = 0;
        for (int k : hit_keys) { auto it = m2.find(k); if (it != m2.end()) sum += it->second; }
    });
    cout << left << setw(22) << "find_hit" << right
         << setw(9) << t1 / (N*3) << "ns" << setw(9) << t2 / (N*3) << "ns"
         << setw(8) << showpos << (t2 - t1) / t1 * 100 << "%" << noshowpos << "\n";

    // Find miss
    t1 = bench_median(RUNS, [&]() {
        volatile int sum = 0;
        for (int k : miss_keys) { auto it = m1.find(k); if (it != m1.end()) sum += it->second; }
    });
    t2 = bench_median(RUNS, [&]() {
        volatile int sum = 0;
        for (int k : miss_keys) { auto it = m2.find(k); if (it != m2.end()) sum += it->second; }
    });
    cout << left << setw(22) << "find_miss" << right
         << setw(9) << t1 / (N*3) << "ns" << setw(9) << t2 / (N*3) << "ns"
         << setw(8) << showpos << (t2 - t1) / t1 * 100 << "%" << noshowpos << "\n";

    // contains hit
    t1 = bench_median(RUNS, [&]() {
        volatile bool sum = false;
        for (int k : hit_keys) sum ^= m1.contains(k);
    });
    t2 = bench_median(RUNS, [&]() {
        volatile bool sum = false;
        for (int k : hit_keys) sum ^= m2.contains(k);
    });
    cout << left << setw(22) << "contains_hit" << right
         << setw(9) << t1 / (N*3) << "ns" << setw(9) << t2 / (N*3) << "ns"
         << setw(8) << showpos << (t2 - t1) / t1 * 100 << "%" << noshowpos << "\n";

    // contains miss
    t1 = bench_median(RUNS, [&]() {
        volatile bool sum = false;
        for (int k : miss_keys) sum ^= m1.contains(k);
    });
    t2 = bench_median(RUNS, [&]() {
        volatile bool sum = false;
        for (int k : miss_keys) sum ^= m2.contains(k);
    });
    cout << left << setw(22) << "contains_miss" << right
         << setw(9) << t1 / (N*3) << "ns" << setw(9) << t2 / (N*3) << "ns"
         << setw(8) << showpos << (t2 - t1) / t1 * 100 << "%" << noshowpos << "\n";

    // Find after 30% erase (mixed tombstones)
    vector<int> erase_keys(keys.begin(), keys.begin() + N * 3 / 10);
    for (int k : erase_keys) { m1.erase(k); m2.erase(k); }

    // Lookup remaining (hit) — should benefit from backward cleanup
    vector<int> remain_hit(N * 3);
    for (int i = 0; i < remain_hit.size(); i++) remain_hit[i] = keys[rng() % N];

    t1 = bench_median(RUNS, [&]() {
        volatile int sum = 0;
        for (int k : remain_hit) { auto it = m1.find(k); if (it != m1.end()) sum += it->second; }
    });
    t2 = bench_median(RUNS, [&]() {
        volatile int sum = 0;
        for (int k : remain_hit) { auto it = m2.find(k); if (it != m2.end()) sum += it->second; }
    });
    cout << left << setw(22) << "find_after_erase30" << right
         << setw(9) << t1 / (N*3) << "ns" << setw(9) << t2 / (N*3) << "ns"
         << setw(8) << showpos << (t2 - t1) / t1 * 100 << "%" << noshowpos << "\n";

    return 0;
}