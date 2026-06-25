// Benchmark: Prefetch effect on all emhash/emilib implementations
// Tests emhash8, emilib1, emilib2, emilib3
//
// Compile:
//   g++ -O3 -std=c++17 -I./include prefetch_all_bench.cpp -o bench_prefetch_on.exe
//   g++ -O3 -std=c++17 -DEMH_NO_PREFETCH=1 -I./include prefetch_all_bench.cpp -o bench_prefetch_off.exe

#include <iostream>
#include <cstdint>
#include <random>
#include <ctime>
#include <vector>

volatile size_t g_result = 0;

#include "emhash/hash_table8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

class Timer {
    clock_t start_;
public:
    Timer() : start_(clock()) {}
    double elapsed_ms() const {
        return (double)(clock() - start_) * 1000.0 / CLOCKS_PER_SEC;
    }
};

std::vector<int> generate_keys(size_t n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, n * 10);
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = dist(rng);
    }
    return keys;
}

template<typename Map>
double bench_insert(Map& map, const std::vector<int>& keys) {
    Timer t;
    for (int k : keys) {
        map.insert({k, k * 2});
    }
    return t.elapsed_ms();
}

template<typename Map>
double bench_find(Map& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    for (int k : keys) {
        auto it = map.find(k);
        if (it != map.end()) sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

template<typename Map>
double bench_find_miss(Map& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    for (int k : keys) {
        auto it = map.find(k + 100000000);
        if (it != map.end()) sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

template<typename Map>
double bench_erase(Map& map, const std::vector<int>& keys) {
    Timer t;
    for (int k : keys) {
        map.erase(k);
    }
    return t.elapsed_ms();
}

template<typename Map>
void run_bench(const std::string& name, const std::vector<int>& keys) {
    Map map;
    map.reserve(keys.size());
    
    double t_insert = bench_insert(map, keys);
    
    std::mt19937 rng(123);
    auto shuffled = keys;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    
    double t_find = bench_find(map, shuffled);
    double t_miss = bench_find_miss(map, shuffled);
    double t_erase = bench_erase(map, shuffled);
    
    printf("%-12s | insert: %5.0f ms | find(hit): %5.0f ms | find(miss): %5.0f ms | erase: %5.0f ms\n",
           name.c_str(), t_insert, t_find, t_miss, t_erase);
}

int main() {
#ifdef EMH_NO_PREFETCH
    std::cout << "=== Prefetch Benchmark (PREFETCH DISABLED) ===\n\n";
#else
    std::cout << "=== Prefetch Benchmark (PREFETCH ENABLED) ===\n\n";
#endif

    const size_t N = 2000000;
    
    // Test 1: Random keys
    {
        std::cout << "=== Test 1: Random Keys (2M) ===\n";
        auto keys = generate_keys(N);
        
        run_bench<emhash8::HashMap<int, int>>("emhash8", keys);
        run_bench<emilib::HashMap<int, int>>("emilib1", keys);
        run_bench<emilib2::HashMap<int, int>>("emilib2", keys);
        run_bench<emilib3::HashMap<int, int>>("emilib3", keys);
    }
    
    // Test 2: High load factor
    {
        std::cout << "\n=== Test 2: High Load Factor (1M, LF=0.90) ===\n";
        auto keys = generate_keys(1000000);
        
        std::mt19937 rng(456);
        std::shuffle(keys.begin(), keys.end(), rng);
        
        // emhash8
        {
            emhash8::HashMap<int, int> map;
            map.reserve(static_cast<size_t>(keys.size() * 1.05));
            map.max_load_factor(0.90f);
            for (int k : keys) map.insert({k, k * 2});
            
            std::cout << "emhash8 LF: " << map.load_factor() << "\n";
            double t_find = bench_find(map, keys);
            double t_miss = bench_find_miss(map, keys);
            printf("emhash8     | find(hit): %5.0f ms | find(miss): %5.0f ms\n", t_find, t_miss);
        }
        
        // emilib1
        {
            emilib::HashMap<int, int> map;
            map.reserve(static_cast<size_t>(keys.size() * 1.05));
            map.max_load_factor(0.90f);
            for (int k : keys) map.insert({k, k * 2});
            
            std::cout << "emilib1 LF: " << map.load_factor() << "\n";
            double t_find = bench_find(map, keys);
            double t_miss = bench_find_miss(map, keys);
            printf("emilib1     | find(hit): %5.0f ms | find(miss): %5.0f ms\n", t_find, t_miss);
        }
        
        // emilib2
        {
            emilib2::HashMap<int, int> map;
            map.reserve(static_cast<size_t>(keys.size() * 1.05));
            map.max_load_factor(0.90f);
            for (int k : keys) map.insert({k, k * 2});
            
            std::cout << "emilib2 LF: " << map.load_factor() << "\n";
            double t_find = bench_find(map, keys);
            double t_miss = bench_find_miss(map, keys);
            printf("emilib2     | find(hit): %5.0f ms | find(miss): %5.0f ms\n", t_find, t_miss);
        }
        
        // emilib3
        {
            emilib3::HashMap<int, int> map;
            map.reserve(static_cast<size_t>(keys.size() * 1.05));
            map.max_load_factor(0.90f);
            for (int k : keys) map.insert({k, k * 2});
            
            std::cout << "emilib3 LF: " << map.load_factor() << "\n";
            double t_find = bench_find(map, keys);
            double t_miss = bench_find_miss(map, keys);
            printf("emilib3     | find(hit): %5.0f ms | find(miss): %5.0f ms\n", t_find, t_miss);
        }
    }
    
    // Test 3: Sequential keys
    {
        std::cout << "\n=== Test 3: Sequential Keys (1M, 0,1,2,...) ===\n";
        std::vector<int> keys(1000000);
        for (size_t i = 0; i < keys.size(); i++) keys[i] = static_cast<int>(i);
        
        run_bench<emhash8::HashMap<int, int>>("emhash8", keys);
        run_bench<emilib::HashMap<int, int>>("emilib1", keys);
        run_bench<emilib2::HashMap<int, int>>("emilib2", keys);
        run_bench<emilib3::HashMap<int, int>>("emilib3", keys);
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "Compare prefetch ON vs OFF:\n";
    std::cout << "  g++ -O3 -std=c++17 -I./include prefetch_all_bench.cpp -o bench_prefetch_on.exe\n";
    std::cout << "  g++ -O3 -std=c++17 -DEMH_NO_PREFETCH=1 -I./include prefetch_all_bench.cpp -o bench_prefetch_off.exe\n";
    
    return 0;
}