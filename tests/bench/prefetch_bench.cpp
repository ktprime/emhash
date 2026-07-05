// Benchmark: Prefetch optimization for hash map operations
// Prefetch hints CPU to load data into cache before it's needed
//
// Compile: g++ -O3 -std=c++17 -I./include prefetch_bench.cpp -o prefetch_bench.exe
//
// Expected result: Prefetch may help for:
// - Large hash maps (cache misses common)
// - Sequential iteration
// - High load factor (long probe chains)

#include <iostream>
#include <cstdint>
#include <random>
#include <ctime>
#include <vector>

#ifdef __GNUC__
#include <x86intrin.h>
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch(addr, 1, 3)
#else
#ifdef _MSC_VER
#include <intrin.h>
#define PREFETCH(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#define PREFETCH_WRITE(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#endif
#endif

volatile size_t g_result = 0;

#include "emhash/hash_table8.hpp"

class Timer {
    clock_t start_;

public:
    Timer() : start_(clock()) {}
    double elapsed_ms() const { return (double)(clock() - start_) * 1000.0 / CLOCKS_PER_SEC; }
};

// Generate random keys
std::vector<int> generate_keys(size_t n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, n * 10);
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = dist(rng);
    }
    return keys;
}

// Generate sequential keys
std::vector<int> generate_sequential_keys(size_t n) {
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = static_cast<int>(i);
    }
    return keys;
}

// Generate sparse keys (large gaps)
std::vector<int> generate_sparse_keys(size_t n, int step = 100000) {
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = static_cast<int>(i * step);
    }
    return keys;
}

// Test 1: Find with prefetch (random keys)
double bench_find_no_prefetch(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
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

double bench_find_with_prefetch(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;

    // Prefetch next key's potential bucket while processing current
    for (size_t i = 0; i < keys.size(); i++) {
        int k = keys[i];

        // Prefetch next key (if exists)
        if (i + 1 < keys.size()) {
            // This is a simplified prefetch - in real implementation,
            // you'd prefetch the bucket based on hash of next key
            PREFETCH(&keys[i + 1]);
        }

        auto it = map.find(k);
        if (it != map.end())
            sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

// Test 2: Iteration with prefetch
double bench_iter_no_prefetch(emhash8::HashMap<int, int>& map) {
    size_t sum = 0;
    Timer t;
    for (auto& p : map) {
        sum += p.second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

double bench_iter_with_prefetch(emhash8::HashMap<int, int>& map) {
    size_t sum = 0;
    Timer t;

    auto it = map.begin();
    auto end = map.end();

    while (it != end) {
        // Prefetch next element's data
        auto next = it;
        ++next;
        if (next != end) {
            PREFETCH(&(*next));
        }

        sum += it->second;
        ++it;
    }
    g_result = sum;
    return t.elapsed_ms();
}

// Test 3: Insert with prefetch (sparse keys - more cache misses)
double bench_insert_no_prefetch(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    Timer t;
    for (int k : keys) {
        map.insert({k, k * 2});
    }
    return t.elapsed_ms();
}

double bench_insert_with_prefetch(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    Timer t;
    for (size_t i = 0; i < keys.size(); i++) {
        int k = keys[i];

        // Prefetch next key
        if (i + 4 < keys.size()) { // Prefetch 4 keys ahead
            PREFETCH(&keys[i + 4]);
        }

        map.insert({k, k * 2});
    }
    return t.elapsed_ms();
}

// Test 4: High load factor scenario
void test_high_load_factor() {
    std::cout << "\n=== Test 4: High Load Factor (0.95) ===\n";

    const size_t N = 1000000;
    auto keys = generate_keys(N);

    emhash8::HashMap<int, int> map;
    map.reserve(N);
    map.max_load_factor(0.95f);

    // Fill to high load factor
    for (int k : keys) {
        map.insert({k, k * 2});
    }

    std::cout << "Load factor: " << map.load_factor() << "\n";
    std::cout << "Bucket count: " << map.bucket_count() << "\n";

    // Shuffle keys for random access test
    std::mt19937 rng(123);
    std::shuffle(keys.begin(), keys.end(), rng);

    double t1 = bench_find_no_prefetch(map, keys);
    double t2 = bench_find_with_prefetch(map, keys);

    std::cout << "find (no prefetch):  " << t1 << " ms\n";
    std::cout << "find (with prefetch): " << t2 << " ms\n";
    std::cout << "Speedup: " << (t1 / t2) << "x\n";
}

// Test 5: Large hash map (cache pressure)
void test_large_map() {
    std::cout << "\n=== Test 5: Large Hash Map (10M entries) ===\n";

    const size_t N = 10000000;
    auto keys = generate_sparse_keys(N, 100); // Sparse keys = more cache misses

    emhash8::HashMap<int, int> map;
    map.reserve(N);

    // Measure memory usage
    size_t est_memory = N * (sizeof(int) * 2 + 8); // pairs + index overhead
    std::cout << "Estimated memory: " << est_memory / (1024 * 1024) << " MB\n";

    // Insert phase
    double t_insert1 = bench_insert_no_prefetch(map, keys);
    map.clear();
    map.reserve(N);
    double t_insert2 = bench_insert_with_prefetch(map, keys);

    std::cout << "insert (no prefetch):  " << t_insert1 << " ms\n";
    std::cout << "insert (with prefetch): " << t_insert2 << " ms\n";
    std::cout << "Speedup: " << (t_insert1 / t_insert2) << "x\n";

    // Refill for find test
    for (int k : keys) {
        map.insert({k, k * 2});
    }

    // Shuffle for random access
    std::mt19937 rng(456);
    std::shuffle(keys.begin(), keys.end(), rng);

    double t_find1 = bench_find_no_prefetch(map, keys);
    double t_find2 = bench_find_with_prefetch(map, keys);

    std::cout << "\nfind (no prefetch):  " << t_find1 << " ms\n";
    std::cout << "find (with prefetch): " << t_find2 << " ms\n";
    std::cout << "Speedup: " << (t_find1 / t_find2) << "x\n";
}

void print_header(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
    std::cout << "Operation       | No Prefetch | With Prefetch | Speedup\n";
    std::cout << "-----------------|-------------|---------------|--------\n";
}

int main() {
    std::cout << "=== Prefetch Optimization Benchmark ===\n";
    std::cout << "Testing whether prefetch hints improve hash map performance\n\n";

    const size_t N = 5000000;

    // Test 1: Random keys (typical case)
    {
        auto keys = generate_keys(N);
        print_header("Test 1: Random Keys (5M)");

        emhash8::HashMap<int, int> map;
        map.reserve(N);

        for (int k : keys) {
            map.insert({k, k * 2});
        }

        // Shuffle for random access
        std::mt19937 rng(789);
        std::shuffle(keys.begin(), keys.end(), rng);

        double t1 = bench_find_no_prefetch(map, keys);
        double t2 = bench_find_with_prefetch(map, keys);

        printf("find (random)    | %11.1f | %13.1f | %6.2fx\n", t1, t2, t1 / t2);
    }

    // Test 2: Iteration
    {
        print_header("Test 2: Iteration (5M entries)");

        auto keys = generate_keys(N);
        emhash8::HashMap<int, int> map;
        map.reserve(N);

        for (int k : keys) {
            map.insert({k, k * 2});
        }

        double t1 = bench_iter_no_prefetch(map);
        double t2 = bench_iter_with_prefetch(map);

        printf("iteration        | %11.1f | %13.1f | %6.2fx\n", t1, t2, t1 / t2);
    }

    // Test 3: Sparse keys (more cache misses)
    {
        auto keys = generate_sparse_keys(N / 10, 10000); // 500K sparse keys
        print_header("Test 3: Sparse Keys (500K, step=10000)");

        emhash8::HashMap<int, int> map;
        map.reserve(keys.size());

        double t1 = bench_insert_no_prefetch(map, keys);
        map.clear();
        map.reserve(keys.size());
        double t2 = bench_insert_with_prefetch(map, keys);

        printf("insert (sparse)  | %11.1f | %13.1f | %6.2fx\n", t1, t2, t1 / t2);

        // Refill for find test
        for (int k : keys) {
            map.insert({k, k * 2});
        }

        double t3 = bench_find_no_prefetch(map, keys);
        double t4 = bench_find_with_prefetch(map, keys);

        printf("find (sparse)    | %11.1f | %13.1f | %6.2fx\n", t3, t4, t3 / t4);
    }

    // Test 4: High load factor
    test_high_load_factor();

    // Test 5: Large map
    test_large_map();

    std::cout << "\n=== Analysis ===\n";
    std::cout << "Prefetch effectiveness depends on:\n";
    std::cout << "1. Cache miss rate - prefetch helps when data is not in cache\n";
    std::cout << "2. Memory latency - prefetch amortizes latency cost\n";
    std::cout << "3. Predictability - prefetch needs predictable access patterns\n";
    std::cout << "\n";
    std::cout << "Expected results:\n";
    std::cout << "- Random keys: minimal benefit (unpredictable access)\n";
    std::cout << "- Sequential iteration: moderate benefit (predictable)\n";
    std::cout << "- Sparse keys: potential benefit (more cache misses)\n";
    std::cout << "- High load factor: potential benefit (longer probe chains)\n";
    std::cout << "- Large maps: significant benefit (cache pressure)\n";
    std::cout << "\n";
    std::cout << "Note: This benchmark uses simplified prefetch. Real implementation\n";
    std::cout << "would prefetch bucket addresses based on hash values, not just keys.\n";

    return 0;
}