// Quick benchmark for emhash — zero dependencies
// Compile: g++ -O3 -march=native -std=c++17 -I. quick_bench.cpp -o quick_bench
// Usage:   ./quick_bench [num_elements]

#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <unordered_map>

template<typename Map>
void bench(const char* name, size_t N) {
    std::mt19937_64 rng(42);
    std::vector<uint64_t> keys(N);
    for (auto& k : keys) k = rng();

    Map map;
    map.reserve(N);

    // Insert
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto k : keys)
        map[k] = k;
    auto t1 = std::chrono::high_resolution_clock::now();

    // Find hit
    volatile uint64_t sink = 0;
    auto t2 = std::chrono::high_resolution_clock::now();
    for (auto k : keys) {
        auto it = map.find(k);
        if (it != map.end()) sink = it->second;
    }
    auto t3 = std::chrono::high_resolution_clock::now();

    // Find miss
    auto t4 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; i++) {
        auto it = map.find(rng() | 1);  // unlikely to hit
        if (it != map.end()) sink = it->second;
    }
    auto t5 = std::chrono::high_resolution_clock::now();

    // Iterate
    auto t6 = std::chrono::high_resolution_clock::now();
    uint64_t sum = 0;
    for (auto& kv : map) sum += kv.first;
    auto t7 = std::chrono::high_resolution_clock::now();

    // Erase
    auto t8 = std::chrono::high_resolution_clock::now();
    for (auto k : keys)
        map.erase(k);
    auto t9 = std::chrono::high_resolution_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    printf("%-25s  Insert %6.1f  FindHit %6.1f  FindMiss %6.1f  Iterate %6.1f  Erase %6.1f  (ms)\n",
           name, ms(t0,t1), ms(t2,t3), ms(t4,t5), ms(t6,t7), ms(t8,t9));
    (void)sink; (void)sum;
}

int main(int argc, char** argv) {
    size_t N = argc > 1 ? atol(argv[1]) : 10000000;

    printf("=== emhash Quick Benchmark (N=%zu) ===\n\n", N);

    bench<emhash7::HashMap<uint64_t, uint64_t>>("emhash7::HashMap", N);
    bench<emhash8::HashMap<uint64_t, uint64_t>>("emhash8::HashMap", N);
    bench<std::unordered_map<uint64_t, uint64_t>>("std::unordered_map", N);

    printf("\nCompile: g++ -O3 -march=native -std=c++17 -I. quick_bench.cpp -o quick_bench\n");
    return 0;
}
