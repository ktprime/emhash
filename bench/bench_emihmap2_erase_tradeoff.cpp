// Performance comparison: emilib2::HashMap vs emilib2_v2::HashMap
// Focus: erase impact on subsequent lookup (backward cleanup tradeoff)
// Compile: g++ -std=c++17 -O3 -march=native -I./include bench/bench_emihmap2_compare.cpp -o bench_compare

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
double measure_ns(Func f, int iterations = 1) {
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) f();
    auto end = chrono::high_resolution_clock::now();
    return chrono::duration<double, chrono::nanoseconds::period>(end - start).count() / iterations;
}

void bench_erase_then_lookup() {
    const int N = 100000;
    const int erase_count = N / 2;  // erase 50%
    
    cout << "\n=== Erase + Lookup Tradeoff (N=" << N << ", erase " << erase_count << ") ===\n";
    
    mt19937_64 rng(42);
    vector<int> keys(N);
    for (int i = 0; i < N; i++) keys[i] = rng() % (N * 10);
    
    // Shuffle erase order to avoid sequential pattern
    vector<int> erase_order(erase_count);
    for (int i = 0; i < erase_count; i++) erase_order[i] = i;
    shuffle(erase_order.begin(), erase_order.end(), rng);
    
    // === emilib2 (original) ===
    {
        emilib2::HashMap<int, int> map;
        map.reserve(N);
        for (int k : keys) map[k] = k;
        
        double erase_time = measure_ns([&]() {
            for (int i : erase_order) map.erase(keys[i]);
        });
        
        // Lookup on remaining keys
        vector<int> remaining_keys;
        for (int i = erase_count; i < N; i++) remaining_keys.push_back(keys[i]);
        remaining_keys.resize(N * 5);  // 5x lookup
        for (size_t i = remaining_keys.size() / 2; i < remaining_keys.size(); i++) {
            remaining_keys[i] = rng() % (N * 10) + N * 100;  // miss keys
        }
        
        double lookup_time = measure_ns([&]() {
            volatile int sum = 0;
            for (int k : remaining_keys) {
                auto it = map.find(k);
                if (it != map.end()) sum += it->second;
            }
        });
        
        cout << "emilib2:  erase=" << erase_time/erase_count << " ns/op, lookup=" 
             << lookup_time/remaining_keys.size() << " ns/op\n";
    }
    
    // === emilib2_v2 (optimized) ===
    {
        emilib2_v2::HashMap<int, int> map;
        map.reserve(N);
        for (int k : keys) map[k] = k;
        
        double erase_time = measure_ns([&]() {
            for (int i : erase_order) map.erase(keys[i]);
        });
        
        vector<int> remaining_keys;
        for (int i = erase_count; i < N; i++) remaining_keys.push_back(keys[i]);
        remaining_keys.resize(N * 5);
        for (size_t i = remaining_keys.size() / 2; i < remaining_keys.size(); i++) {
            remaining_keys[i] = rng() % (N * 10) + N * 100;
        }
        
        double lookup_time = measure_ns([&]() {
            volatile int sum = 0;
            for (int k : remaining_keys) {
                auto it = map.find(k);
                if (it != map.end()) sum += it->second;
            }
        });
        
        cout << "emilib2_v2: erase=" << erase_time/erase_count << " ns/op, lookup="
             << lookup_time/remaining_keys.size() << " ns/op\n";
    }
}

void bench_high_load_erase() {
    const int N = 50000;
    cout << "\n=== High Load Factor Erase (N=" << N << ", LF=0.95) ===\n";
    
    mt19937_64 rng(12345);
    vector<int> keys(N);
    for (int i = 0; i < N; i++) keys[i] = i * 7;  // sequential pattern
    
    // === emilib2 ===
    {
        emilib2::HashMap<int, int> map;
        map.reserve(N * 1.05);  // ~95% load factor
        for (int k : keys) map[k] = k;
        
        double erase_time = measure_ns([&]() {
            for (int k : keys) map.erase(k);
        });
        
        cout << "emilib2:  erase=" << erase_time/N << " ns/op (total=" << erase_time << ")\n";
    }
    
    // === emilib2_v2 ===
    {
        emilib2_v2::HashMap<int, int> map;
        map.reserve(N * 1.05);
        for (int k : keys) map[k] = k;
        
        double erase_time = measure_ns([&]() {
            for (int k : keys) map.erase(k);
        });
        
        cout << "emilib2_v2: erase=" << erase_time/N << " ns/op (total=" << erase_time << ")\n";
    }
}

int main() {
    cout << "========================================================\n";
    cout << "  emilib2 vs emilib2_v2: Erase/Lookup Tradeoff\n";
    cout << "========================================================\n";
    
    bench_erase_then_lookup();
    bench_high_load_erase();
    
    cout << "\n========================================================\n";
    cout << "  Analysis\n";
    cout << "========================================================\n";
    cout << "v2 backward cleanup trades erase speed for lookup speed.\n";
    cout << "If erase is rare and lookup is common, v2 may be better.\n";
    cout << "If erase is frequent, original emilib2 is better.\n";
    
    return 0;
}