// Short-key hash benchmark — validates rapidhash-style wyr8 optimization
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <random>
#include <vector>

template<typename Map>
void bench_short_string(const char* name, size_t N) {
    std::mt19937 rng(42);
    Map map;
    map.reserve(N);

    // 8-16 byte string keys (benefit from wyr8 optimization)
    std::vector<std::string> keys;
    keys.reserve(N);
    for (size_t i = 0; i < N; i++) {
        auto v = rng();
        keys.push_back("key_" + std::to_string(v));
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& k : keys) map[k] = 1;
    auto t1 = std::chrono::high_resolution_clock::now();

    volatile size_t sink = 0;
    auto t2 = std::chrono::high_resolution_clock::now();
    for (auto& k : keys) { auto it = map.find(k); if (it != map.end()) sink = it->second; }
    auto t3 = std::chrono::high_resolution_clock::now();

    auto t4 = std::chrono::high_resolution_clock::now();
    for (auto& k : keys) map.erase(k);
    auto t5 = std::chrono::high_resolution_clock::now();

    auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
    printf("%-25s  Insert %6.1f  FindHit %6.1f  Erase %6.1f  (N=%zu)\n",
           name, ms(t0,t1), ms(t2,t3), ms(t4,t5), N);
    (void)sink;
}

int main() {
    const size_t N = 2000000;
    printf("=== Short-String Key Benchmark (N=%zu) ===\n\n", N);
    bench_short_string<emhash5::HashMap<std::string, int>>("emhash5", N);
    bench_short_string<emhash7::HashMap<std::string, int>>("emhash7", N);
    bench_short_string<emhash8::HashMap<std::string, int>>("emhash8", N);
    return 0;
}
