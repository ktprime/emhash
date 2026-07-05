// Benchmark: Compare emhash built-in prefetch vs disabled prefetch
// emhash already has prefetch_heap_block in find_filled_slot
// This test measures its effectiveness
//
// Compile:
//   g++ -O3 -std=c++17 -I./include prefetch_internal_bench.cpp -o bench_prefetch_on.exe
//   g++ -O3 -std=c++17 -DEMH_NO_PREFETCH=1 -I./include prefetch_internal_bench.cpp -o bench_prefetch_off.exe

#include <iostream>
#include <cstdint>
#include <random>
#include <ctime>
#include <vector>

volatile size_t g_result = 0;

#include "emhash/hash_table8.hpp"

class Timer {
    clock_t start_;

public:
    Timer() : start_(clock()) {}
    double elapsed_ms() const { return (double)(clock() - start_) * 1000.0 / CLOCKS_PER_SEC; }
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

std::vector<int> generate_sparse_keys(size_t n, int step = 100000) {
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = static_cast<int>(i * step);
    }
    return keys;
}

std::vector<int> generate_sequential_keys(size_t n) {
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = static_cast<int>(i);
    }
    return keys;
}

double bench_insert(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    Timer t;
    for (int k : keys) {
        map.insert({k, k * 2});
    }
    return t.elapsed_ms();
}

double bench_find(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    for (int k : keys) {
        auto it = map.find(k);
        if (it != map.end())
            sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

double bench_find_miss(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    for (int k : keys) {
        auto it = map.find(k + 100000000); // keys that don't exist
        if (it != map.end())
            sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

double bench_erase(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    Timer t;
    for (int k : keys) {
        map.erase(k);
    }
    return t.elapsed_ms();
}

double bench_iteration(emhash8::HashMap<int, int>& map) {
    size_t sum = 0;
    Timer t;
    for (auto& p : map) {
        sum += p.second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

int main() {
#ifdef EMH_NO_PREFETCH
    std::cout << "=== emhash Prefetch Benchmark (PREFETCH DISABLED) ===\n\n";
#else
    std::cout << "=== emhash Prefetch Benchmark (PREFETCH ENABLED) ===\n\n";
#endif

    const size_t N = 5000000;

    // Test 1: Random keys
    {
        auto keys = generate_keys(N);
        std::cout << "=== Test 1: Random Keys (5M) ===\n";

        emhash8::HashMap<int, int> map;
        map.reserve(N);

        double t_insert = bench_insert(map, keys);
        std::cout << "insert:      " << t_insert << " ms (" << N / t_insert / 1000 << " M/s)\n";

        // Shuffle for random access
        std::mt19937 rng(123);
        std::shuffle(keys.begin(), keys.end(), rng);

        double t_find = bench_find(map, keys);
        std::cout << "find (hit):  " << t_find << " ms (" << N / t_find / 1000 << " M/s)\n";

        double t_miss = bench_find_miss(map, keys);
        std::cout << "find (miss): " << t_miss << " ms (" << N / t_miss / 1000 << " M/s)\n";

        double t_erase = bench_erase(map, keys);
        std::cout << "erase:       " << t_erase << " ms (" << N / t_erase / 1000 << " M/s)\n";

        std::cout << "Load factor: " << map.load_factor() << "\n";
    }

    // Test 2: Sequential keys (worst for prefetch?)
    {
        auto keys = generate_sequential_keys(N);
        std::cout << "\n=== Test 2: Sequential Keys (0,1,2,...) ===\n";

        emhash8::HashMap<int, int> map;
        map.reserve(N);

        double t_insert = bench_insert(map, keys);
        std::cout << "insert:      " << t_insert << " ms (" << N / t_insert / 1000 << " M/s)\n";

        double t_find = bench_find(map, keys);
        std::cout << "find (hit):  " << t_find << " ms (" << N / t_find / 1000 << " M/s)\n";

        double t_miss = bench_find_miss(map, keys);
        std::cout << "find (miss): " << t_miss << " ms (" << N / t_miss / 1000 << " M/s)\n";

        double t_erase = bench_erase(map, keys);
        std::cout << "erase:       " << t_erase << " ms (" << N / t_erase / 1000 << " M/s)\n";
    }

    // Test 3: Sparse keys (more cache misses)
    {
        auto keys = generate_sparse_keys(N / 10, 10000);
        std::cout << "\n=== Test 3: Sparse Keys (500K, step=10000) ===\n";

        emhash8::HashMap<int, int> map;
        map.reserve(keys.size());

        double t_insert = bench_insert(map, keys);
        std::cout << "insert:      " << t_insert << " ms (" << keys.size() / t_insert / 1000 << " M/s)\n";

        std::mt19937 rng(456);
        std::shuffle(keys.begin(), keys.end(), rng);

        double t_find = bench_find(map, keys);
        std::cout << "find (hit):  " << t_find << " ms (" << keys.size() / t_find / 1000 << " M/s)\n";

        double t_erase = bench_erase(map, keys);
        std::cout << "erase:       " << t_erase << " ms (" << keys.size() / t_erase / 1000 << " M/s)\n";
    }

    // Test 4: High load factor
    {
        auto keys = generate_keys(1000000);
        std::cout << "\n=== Test 4: High Load Factor (1M, LF=0.90) ===\n";

        emhash8::HashMap<int, int> map;
        map.reserve(static_cast<size_t>(keys.size() * 1.05));
        map.max_load_factor(0.90f);

        for (int k : keys) {
            map.insert({k, k * 2});
        }

        std::cout << "Load factor: " << map.load_factor() << "\n";

        std::mt19937 rng(789);
        std::shuffle(keys.begin(), keys.end(), rng);

        double t_find = bench_find(map, keys);
        std::cout << "find (hit):  " << t_find << " ms (" << keys.size() / t_find / 1000 << " M/s)\n";

        double t_miss = bench_find_miss(map, keys);
        std::cout << "find (miss): " << t_miss << " ms (" << keys.size() / t_miss / 1000 << " M/s)\n";
    }

    // Test 5: Iteration
    {
        auto keys = generate_keys(N);
        std::cout << "\n=== Test 5: Iteration (5M entries) ===\n";

        emhash8::HashMap<int, int> map;
        map.reserve(N);
        for (int k : keys) {
            map.insert({k, k * 2});
        }

        double t_iter = bench_iteration(map);
        std::cout << "iteration:   " << t_iter << " ms (" << N / t_iter / 1000 << " M/s)\n";
    }

    // Test 6: Large map (cache pressure)
    {
        auto keys = generate_sparse_keys(10000000, 100);
        std::cout << "\n=== Test 6: Large Sparse Map (10M, step=100) ===\n";

        size_t est_mem = keys.size() * (sizeof(int) * 2 + 16);
        std::cout << "Estimated memory: " << est_mem / (1024 * 1024) << " MB\n";

        emhash8::HashMap<int, int> map;
        map.reserve(keys.size());

        double t_insert = bench_insert(map, keys);
        std::cout << "insert:      " << t_insert << " ms (" << keys.size() / t_insert / 1000 << " M/s)\n";

        std::mt19937 rng(999);
        std::shuffle(keys.begin(), keys.end(), rng);

        double t_find = bench_find(map, keys);
        std::cout << "find (hit):  " << t_find << " ms (" << keys.size() / t_find / 1000 << " M/s)\n";
    }

    std::cout << "\n=== How to compare ===\n";
    std::cout << "Run twice:\n";
    std::cout << "  1. g++ -O3 -std=c++17 -I./include prefetch_internal_bench.cpp -o bench_prefetch_on.exe\n";
    std::cout << "  2. g++ -O3 -std=c++17 -DEMH_NO_PREFETCH=1 -I./include prefetch_internal_bench.cpp -o "
                 "bench_prefetch_off.exe\n";
    std::cout << "Then compare the results.\n";

    return 0;
}