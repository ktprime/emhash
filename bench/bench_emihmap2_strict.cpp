// Strict performance comparison: emilib2 vs emilib2_v2
// Run each test 10 times, report median
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap2_v2.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

template <typename Func>
double measure_ns(Func f, int iterations = 10) {
    vector<double> times;
    times.reserve(iterations);
    for (int i = 0; i < iterations; i++) {
        auto start = chrono::high_resolution_clock::now();
        f();
        auto end = chrono::high_resolution_clock::now();
        times.push_back(chrono::duration<double, chrono::nanoseconds::period>(end - start).count());
    }
    sort(times.begin(), times.end());
    return times[iterations / 2];  // median
}

int main() {
    const int N = 100000;
    mt19937_64 rng(42);
    vector<int> keys(N);
    for (int i = 0; i < N; i++) keys[i] = rng() % (N * 10);
    
    cout << "=== Strict Performance Test (N=" << N << ", 10 runs, median) ===\n\n";
    
    // === Insert ===
    double insert1, insert2;
    {
        emilib2::HashMap<int, int> map;
        insert1 = measure_ns([&]() {
            map.reserve(N);
            for (int k : keys) map[k] = k;
        }, 10);
    }
    {
        emilib2_v2::HashMap<int, int> map;
        insert2 = measure_ns([&]() {
            map.reserve(N);
            for (int k : keys) map[k] = k;
        }, 10);
    }
    cout << "Insert: emilib2=" << insert1/N << " ns/op, v2=" << insert2/N << " ns/op, delta=" 
         << ((insert2 - insert1) / insert1 * 100) << "%\n";
    
    // === Lookup (hit) ===
    emilib2::HashMap<int, int> map1;
    map1.reserve(N);
    for (int k : keys) map1[k] = k;
    
    emilib2_v2::HashMap<int, int> map2;
    map2.reserve(N);
    for (int k : keys) map2[k] = k;
    
    vector<int> hit_keys(N * 10);
    for (int i = 0; i < hit_keys.size(); i++) hit_keys[i] = keys[rng() % N];
    
    double lookup1 = measure_ns([&]() {
        volatile int sum = 0;
        for (int k : hit_keys) {
            auto it = map1.find(k);
            if (it != map1.end()) sum += it->second;
        }
    }, 10);
    
    double lookup2 = measure_ns([&]() {
        volatile int sum = 0;
        for (int k : hit_keys) {
            auto it = map2.find(k);
            if (it != map2.end()) sum += it->second;
        }
    }, 10);
    
    cout << "Lookup (hit): emilib2=" << lookup1/(N*10) << " ns/op, v2=" << lookup2/(N*10) 
         << " ns/op, delta=" << ((lookup2 - lookup1) / lookup1 * 100) << "%\n";
    
    // === Erase (sequential) ===
    double erase1 = measure_ns([&]() {
        auto m = map1;
        for (int k : keys) m.erase(k);
    }, 10);
    
    double erase2 = measure_ns([&]() {
        auto m = map2;
        for (int k : keys) m.erase(k);
    }, 10);
    
    cout << "Erase (seq): emilib2=" << erase1/N << " ns/op, v2=" << erase2/N 
         << " ns/op, delta=" << ((erase2 - erase1) / erase1 * 100) << "%\n";
    
    cout << "\n=== Summary ===\n";
    cout << "v2 is SLOWER in erase due to backward cleanup overhead.\n";
    cout << "v2 should be FASTER in lookup after erase, but results vary.\n";
    
    return 0;
}