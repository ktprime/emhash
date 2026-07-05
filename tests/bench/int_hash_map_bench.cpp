// Benchmark: EMH_INT_HASH=1 performance impact on hash map operations
// Compile with: g++ -O3 -std=c++17 [-DEMH_INT_HASH=1] int_hash_map_bench.cpp -o bench
//
// Expected result: EMH_INT_HASH=1 provides 20-50% speedup for integer keys
// by using wyhash64 instead of std::hash<int>

#include <iostream>
#include <cstdint>
#include <random>
#include <ctime>
#include <vector>
#include <string>

// Prevent compiler optimization
volatile size_t g_result = 0;

// Include emhash
#include "emhash/hash_table8.hpp"

// Timer helper
class Timer {
    clock_t start_;

public:
    Timer() : start_(clock()) {}
    double elapsed_ms() const { return (double)(clock() - start_) * 1000.0 / CLOCKS_PER_SEC; }
};

// Generate test data
std::vector<int> generate_random_keys(size_t n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, n * 10);
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = dist(rng);
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

std::vector<int> generate_sparse_keys(size_t n, int step = 1000) {
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = static_cast<int>(i * step);
    }
    return keys;
}

// Benchmark: insert
template <typename Map> double bench_insert(Map& map, const std::vector<int>& keys) {
    Timer t;
    for (int k : keys) {
        map.insert({k, k * 2});
    }
    return t.elapsed_ms();
}

// Benchmark: find (hit)
template <typename Map> double bench_find_hit(Map& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    for (int k : keys) {
        auto it = map.find(k);
        if (it != map.end()) {
            sum += it->second;
        }
    }
    g_result = sum;
    return t.elapsed_ms();
}

// Benchmark: find (miss)
template <typename Map> double bench_find_miss(Map& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    for (int k : keys) {
        auto it = map.find(k + 10000000); // keys that don't exist
        if (it != map.end()) {
            sum += it->second;
        }
    }
    g_result = sum;
    return t.elapsed_ms();
}

// Benchmark: erase
template <typename Map> double bench_erase(Map& map, const std::vector<int>& keys) {
    Timer t;
    for (int k : keys) {
        map.erase(k);
    }
    return t.elapsed_ms();
}

// Benchmark: mixed operations
template <typename Map> double bench_mixed(Map& map, const std::vector<int>& keys) {
    Timer t;
    size_t sum = 0;
    for (size_t i = 0; i < keys.size(); i++) {
        int k = keys[i];
        if (i % 4 == 0) {
            map.insert({k, k});
        } else if (i % 4 == 1) {
            auto it = map.find(k);
            if (it != map.end())
                sum += it->second;
        } else if (i % 4 == 2) {
            map.erase(k);
        } else {
            map[k] = k * 2;
        }
    }
    g_result = sum;
    return t.elapsed_ms();
}

void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
    std::cout << "Operation     | Time (ms) | Ops/sec (M)\n";
    std::cout << "---------------|-----------|------------\n";
}

void print_result(const std::string& op, double time_ms, size_t ops) {
    double mops = ops / time_ms / 1000.0;
    printf("%-14s | %9.1f | %10.2f\n", op.c_str(), time_ms, mops);
}

int main() {
#ifdef EMH_INT_HASH
    std::cout << "=== EMH_INT_HASH Benchmark (EMH_INT_HASH=" << EMH_INT_HASH << ") ===\n\n";
#else
    std::cout << "=== EMH_INT_HASH Benchmark (EMH_INT_HASH=0, using std::hash) ===\n\n";
#endif

    const size_t N = 5000000; // 5M operations

    // Test 1: Random keys
    {
        auto keys = generate_random_keys(N);
        print_header("Test 1: Random Keys (5M)");

        emhash8::HashMap<int, int> map;
        map.reserve(N);

        double t_insert = bench_insert(map, keys);
        print_result("insert", t_insert, N);

        double t_find_hit = bench_find_hit(map, keys);
        print_result("find(hit)", t_find_hit, N);

        double t_find_miss = bench_find_miss(map, keys);
        print_result("find(miss)", t_find_miss, N);

        double t_erase = bench_erase(map, keys);
        print_result("erase", t_erase, N);

        std::cout << "\nLoad factor: " << map.load_factor() << "\n";
    }

    // Test 2: Sequential keys (worst case for identity hash)
    {
        auto keys = generate_sequential_keys(N);
        print_header("Test 2: Sequential Keys (0,1,2,...)");

        emhash8::HashMap<int, int> map;
        map.reserve(N);

        double t_insert = bench_insert(map, keys);
        print_result("insert", t_insert, N);

        double t_find_hit = bench_find_hit(map, keys);
        print_result("find(hit)", t_find_hit, N);

        double t_find_miss = bench_find_miss(map, keys);
        print_result("find(miss)", t_find_miss, N);

        double t_erase = bench_erase(map, keys);
        print_result("erase", t_erase, N);

        std::cout << "\nLoad factor: " << map.load_factor() << "\n";
    }

    // Test 3: Sparse keys (step=1000)
    {
        auto keys = generate_sparse_keys(N, 1000);
        print_header("Test 3: Sparse Keys (0,1000,2000,...)");

        emhash8::HashMap<int, int> map;
        map.reserve(N);

        double t_insert = bench_insert(map, keys);
        print_result("insert", t_insert, N);

        double t_find_hit = bench_find_hit(map, keys);
        print_result("find(hit)", t_find_hit, N);

        double t_find_miss = bench_find_miss(map, keys);
        print_result("find(miss)", t_find_miss, N);

        double t_erase = bench_erase(map, keys);
        print_result("erase", t_erase, N);

        std::cout << "\nLoad factor: " << map.load_factor() << "\n";
    }

    // Test 4: Mixed operations
    {
        auto keys = generate_random_keys(N);
        print_header("Test 4: Mixed Operations (insert/find/erase/operator[])");

        emhash8::HashMap<int, int> map;
        map.reserve(N);

        double t_mixed = bench_mixed(map, keys);
        print_result("mixed", t_mixed, N);
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Run this benchmark twice:\n";
    std::cout << "  1. g++ -O3 -std=c++17 int_hash_map_bench.cpp -o bench_default\n";
    std::cout << "  2. g++ -O3 -std=c++17 -DEMH_INT_HASH=1 int_hash_map_bench.cpp -o bench_wyhash\n";
    std::cout << "Then compare the results.\n";
    std::cout << "\nActual findings:\n";
    std::cout << "- std::hash<int> is usually identity hash on MSVC/GCC\n";
    std::cout << "- For sequential keys (0,1,2,...): identity hash is MUCH faster\n";
    std::cout << "- For random keys: wyhash64 may be slightly faster for insert\n";
    std::cout << "- EMH_INT_HASH=1 provides SECURITY (hash flooding protection), NOT speed\n";
    std::cout << "\nRecommendation:\n";
    std::cout << "- Trusted input + sequential keys: use default std::hash\n";
    std::cout << "- Untrusted input (web/API): use EMH_INT_HASH=1 for security\n";
    std::cout << "- The '20-50% speedup' claim is NOT accurate for most cases\n";

    return 0;
}