// Performance comparison: emilib2::HashMap vs emilib2_v2::HashMap
// Compile: clang++ -std=c++17 -O3 -march=native -I../include bench_emihmap2_compare.cpp -o bench_compare
// Run: ./bench_compare

#include "emilib/emihmap2.hpp"
#include "emilib/emihmap2_v2.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace std;

// Timing helper
template <typename Func>
double measure_ns(Func f, int iterations = 1) {
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) f();
    auto end = chrono::high_resolution_clock::now();
    return chrono::duration<double, chrono::nanoseconds::period>(end - start).count() / iterations;
}

// Benchmark configuration
struct BenchConfig {
    int N;              // Number of elements
    int lookup_ratio;   // Lookups per insert
    int erase_ratio;    // Erases per insert
    string key_type;    // "int" or "string"
};

void bench_int_keys(const BenchConfig& cfg) {
    cout << "\n=== Integer Keys (N=" << cfg.N << ") ===\n";
    
    // Generate random keys
    mt19937_64 rng(42);
    vector<int> keys(cfg.N);
    for (int i = 0; i < cfg.N; i++) keys[i] = rng() % (cfg.N * 10);
    
    // Generate lookup keys (50% hit, 50% miss)
    vector<int> lookup_keys(cfg.N * cfg.lookup_ratio);
    for (int i = 0; i < lookup_keys.size(); i++) {
        if (i % 2 == 0) lookup_keys[i] = keys[rng() % cfg.N];  // hit
        else            lookup_keys[i] = rng() % (cfg.N * 10) + cfg.N * 100;  // miss
    }
    
    // === emilib2 (original) ===
    emilib2::HashMap<int, int> map1;
    map1.reserve(cfg.N);
    
    double insert1 = measure_ns([&]() {
        for (int k : keys) map1[k] = k;
    });
    
    double lookup1 = measure_ns([&]() {
        volatile int sum = 0;
        for (int k : lookup_keys) {
            auto it = map1.find(k);
            if (it != map1.end()) sum += it->second;
        }
    });
    
    double erase1 = measure_ns([&]() {
        for (int i = 0; i < cfg.N * cfg.erase_ratio; i++) {
            map1.erase(keys[i]);
        }
    });
    
    // === emilib2_v2 (optimized) ===
    emilib2_v2::HashMap<int, int> map2;
    map2.reserve(cfg.N);
    
    double insert2 = measure_ns([&]() {
        for (int k : keys) map2[k] = k;
    });
    
    double lookup2 = measure_ns([&]() {
        volatile int sum = 0;
        for (int k : lookup_keys) {
            auto it = map2.find(k);
            if (it != map2.end()) sum += it->second;
        }
    });
    
    double erase2 = measure_ns([&]() {
        for (int i = 0; i < cfg.N * cfg.erase_ratio; i++) {
            map2.erase(keys[i]);
        }
    });
    
    // Report
    cout << fixed << setprecision(2);
    cout << "| Operation | emilib2 (ns/op) | emilib2_v2 (ns/op) | Delta |\n";
    cout << "|-----------|----------------|--------------------|-------|\n";
    cout << "| Insert    | " << insert1/cfg.N << " | " << insert2/cfg.N << " | " 
         << ((insert2 - insert1) / insert1 * 100) << "% |\n";
    cout << "| Lookup    | " << lookup1/(cfg.N*cfg.lookup_ratio) << " | " << lookup2/(cfg.N*cfg.lookup_ratio) << " | "
         << ((lookup2 - lookup1) / lookup1 * 100) << "% |\n";
    cout << "| Erase     | " << erase1/(cfg.N*cfg.erase_ratio) << " | " << erase2/(cfg.N*cfg.erase_ratio) << " | "
         << ((erase2 - erase1) / erase1 * 100) << "% |\n";
}

void bench_string_keys(const BenchConfig& cfg) {
    cout << "\n=== String Keys (N=" << cfg.N << ") ===\n";
    
    mt19937_64 rng(42);
    vector<string> keys(cfg.N);
    for (int i = 0; i < cfg.N; i++) {
        keys[i] = "key_" + to_string(rng() % (cfg.N * 10));
    }
    
    vector<string> lookup_keys(cfg.N * cfg.lookup_ratio);
    for (int i = 0; i < lookup_keys.size(); i++) {
        if (i % 2 == 0) lookup_keys[i] = keys[rng() % cfg.N];
        else            lookup_keys[i] = "key_miss_" + to_string(rng());
    }
    
    // === emilib2 (original) ===
    emilib2::HashMap<string, int> map1;
    map1.reserve(cfg.N);
    
    double insert1 = measure_ns([&]() {
        for (const auto& k : keys) map1[k] = stoi(k.substr(4));
    });
    
    double lookup1 = measure_ns([&]() {
        volatile int sum = 0;
        for (const auto& k : lookup_keys) {
            auto it = map1.find(k);
            if (it != map1.end()) sum += it->second;
        }
    });
    
    // === emilib2_v2 (optimized) ===
    emilib2_v2::HashMap<string, int> map2;
    map2.reserve(cfg.N);
    
    double insert2 = measure_ns([&]() {
        for (const auto& k : keys) map2[k] = stoi(k.substr(4));
    });
    
    double lookup2 = measure_ns([&]() {
        volatile int sum = 0;
        for (const auto& k : lookup_keys) {
            auto it = map2.find(k);
            if (it != map2.end()) sum += it->second;
        }
    });
    
    cout << fixed << setprecision(2);
    cout << "| Operation | emilib2 (ns/op) | emilib2_v2 (ns/op) | Delta |\n";
    cout << "|-----------|----------------|--------------------|-------|\n";
    cout << "| Insert    | " << insert1/cfg.N << " | " << insert2/cfg.N << " | "
         << ((insert2 - insert1) / insert1 * 100) << "% |\n";
    cout << "| Lookup    | " << lookup1/(cfg.N*cfg.lookup_ratio) << " | " << lookup2/(cfg.N*cfg.lookup_ratio) << " | "
         << ((lookup2 - lookup1) / lookup1 * 100) << "% |\n";
}

void bench_iteration(const BenchConfig& cfg) {
    cout << "\n=== Iteration (N=" << cfg.N << ") ===\n";
    
    mt19937_64 rng(42);
    vector<int> keys(cfg.N);
    for (int i = 0; i < cfg.N; i++) keys[i] = rng() % (cfg.N * 10);
    
    emilib2::HashMap<int, int> map1;
    map1.reserve(cfg.N);
    for (int k : keys) map1[k] = k;
    
    emilib2_v2::HashMap<int, int> map2;
    map2.reserve(cfg.N);
    for (int k : keys) map2[k] = k;
    
    double iter1 = measure_ns([&]() {
        volatile int sum = 0;
        for (auto& [k, v] : map1) sum += v;
    }, 100);
    
    double iter2 = measure_ns([&]() {
        volatile int sum = 0;
        for (auto& [k, v] : map2) sum += v;
    }, 100);
    
    cout << fixed << setprecision(2);
    cout << "| Version      | Total (ns) | Per-element (ns) |\n";
    cout << "|--------------|------------|------------------|\n";
    cout << "| emilib2      | " << iter1 << " | " << iter1/cfg.N << " |\n";
    cout << "| emilib2_v2   | " << iter2 << " | " << iter2/cfg.N << " |\n";
    cout << "| Delta        | " << ((iter2 - iter1) / iter1 * 100) << "% | - |\n";
}

int main() {
    cout << "========================================================\n";
    cout << "  emilib2 vs emilib2_v2 Performance Comparison\n";
    cout << "========================================================\n";
    
    BenchConfig cfg1{100000, 10, 1, "int"};
    BenchConfig cfg2{100000, 10, 0, "string"};
    
    bench_int_keys(cfg1);
    bench_string_keys(cfg2);
    bench_iteration(cfg1);
    
    cout << "\n========================================================\n";
    cout << "  Summary\n";
    cout << "========================================================\n";
    cout << "Negative delta = v2 faster, Positive delta = v2 slower\n";
    
    return 0;
}