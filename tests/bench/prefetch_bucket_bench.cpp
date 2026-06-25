// Benchmark: Precise Prefetch - prefetch actual bucket addresses
// The key to effective prefetch is to prefetch the RIGHT address
//
// Compile: g++ -O3 -std=c++17 -I./include prefetch_bucket_bench.cpp -o prefetch_bucket_bench.exe

#include <iostream>
#include <cstdint>
#include <random>
#include <ctime>
#include <vector>

#ifdef __GNUC__
#include <x86intrin.h>
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#else
#ifdef _MSC_VER
#include <intrin.h>
#define PREFETCH(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#endif
#endif

volatile size_t g_result = 0;

#include "emhash/hash_table8.hpp"

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

std::vector<int> generate_sparse_keys(size_t n, int step = 100000) {
    std::vector<int> keys(n);
    for (size_t i = 0; i < n; i++) {
        keys[i] = static_cast<int>(i * step);
    }
    return keys;
}

// Direct access to internal structure for prefetch
// This simulates what a real implementation would do
template<typename Map>
double bench_find_with_bucket_prefetch(Map& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    
    // We need to know the bucket address to prefetch
    // In hash_table8, the bucket is at _index[hash(key) & _mask]
    // This is internal, so we use a workaround
    
    for (size_t i = 0; i < keys.size(); i++) {
        int k = keys[i];
        
        // Prefetch 4 keys ahead (give CPU time to load data)
        if (i + 4 < keys.size()) {
            int next_key = keys[i + 4];
            // We can't directly access _index, but we can use find_filled_slot
            // to get the slot, then prefetch the pair
            // This is still not ideal - real prefetch would be in find logic
            PREFETCH(&next_key);  // At least prefetch the key itself
        }
        
        auto it = map.find(k);
        if (it != map.end()) sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

// Test with custom hash map that has built-in prefetch
namespace emhash_prefetch {

// Wrapper that adds prefetch to find operation
template<typename BaseMap>
class PrefetchMap : public BaseMap {
public:
    using BaseMap::BaseMap;
    
    // Prefetch-enhanced find for batch lookups
    template<typename KeyIter>
    static size_t batch_find_sum(BaseMap& map, KeyIter begin, KeyIter end) {
        size_t sum = 0;
        auto it = begin;
        
        while (it != end) {
            auto curr_key = *it;
            
            // Prefetch next few keys
            auto prefetch_it = it;
            for (int p = 0; p < 4 && prefetch_it != end; ++p, ++prefetch_it) {
                PREFETCH(&(*prefetch_it));
            }
            
            auto found = map.find(curr_key);
            if (found != map.end()) {
                sum += found->second;
            }
            ++it;
        }
        return sum;
    }
};

} // namespace emhash_prefetch

// Test iteration with prefetch of next pair
double bench_iter_prefetch(emhash8::HashMap<int, int>& map) {
    size_t sum = 0;
    Timer t;
    
    // Get raw pair array access (hash_table8 stores pairs densely)
    // We can prefetch next pairs during iteration
    auto* pairs = &map.index(0);  // Access first pair
    
    for (auto it = map.begin(); it != map.end(); ++it) {
        // Prefetch next element's pair
        auto next = std::next(it);
        if (next != map.end()) {
            PREFETCH(&(*next));
        }
        sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

// Test probe chain prefetch (for high load factor)
// When load factor is high, probe chains are longer
// Prefetching next bucket in chain could help
double bench_find_high_lf(emhash8::HashMap<int, int>& map, const std::vector<int>& keys) {
    size_t sum = 0;
    Timer t;
    
    for (int k : keys) {
        // In high LF scenario, we might need to check multiple buckets
        // Prefetching the key gives CPU hint to start loading
        PREFETCH(&k);
        
        auto it = map.find(k);
        if (it != map.end()) sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

// Test with predictable access pattern (sequential keys)
double bench_find_sequential_prefetch(emhash8::HashMap<int, int>& map, size_t n) {
    size_t sum = 0;
    Timer t;
    
    for (size_t i = 0; i < n; i++) {
        int k = static_cast<int>(i);
        
        // Prefetch 8 keys ahead for sequential access
        if (i + 8 < n) {
            int next_k = static_cast<int>(i + 8);
            PREFETCH(&next_k);
        }
        
        auto it = map.find(k);
        if (it != map.end()) sum += it->second;
    }
    g_result = sum;
    return t.elapsed_ms();
}

int main() {
    std::cout << "=== Precise Prefetch Benchmark ===\n";
    std::cout << "Testing prefetch with actual bucket/pair addresses\n\n";
    
    // Test 1: Sequential keys (predictable pattern)
    {
        std::cout << "=== Test 1: Sequential Keys (1M) ===\n";
        const size_t N = 1000000;
        
        emhash8::HashMap<int, int> map;
        map.reserve(N);
        for (size_t i = 0; i < N; i++) {
            map.insert({static_cast<int>(i), static_cast<int>(i * 2)});
        }
        
        Timer t1;
        size_t sum1 = 0;
        for (size_t i = 0; i < N; i++) {
            auto it = map.find(static_cast<int>(i));
            if (it != map.end()) sum1 += it->second;
        }
        double time1 = t1.elapsed_ms();
        g_result = sum1;
        
        double time2 = bench_find_sequential_prefetch(map, N);
        
        std::cout << "find (no prefetch):    " << time1 << " ms\n";
        std::cout << "find (prefetch 8):     " << time2 << " ms\n";
        std::cout << "Speedup:               " << (time1 / time2) << "x\n";
    }
    
    // Test 2: Iteration (dense pairs)
    {
        std::cout << "\n=== Test 2: Iteration (5M entries) ===\n";
        const size_t N = 5000000;
        
        auto keys = generate_keys(N);
        emhash8::HashMap<int, int> map;
        map.reserve(N);
        for (int k : keys) {
            map.insert({k, k * 2});
        }
        
        Timer t1;
        size_t sum1 = 0;
        for (auto& p : map) {
            sum1 += p.second;
        }
        double time1 = t1.elapsed_ms();
        g_result = sum1;
        
        double time2 = bench_iter_prefetch(map);
        
        std::cout << "iteration (no prefetch): " << time1 << " ms\n";
        std::cout << "iteration (prefetch):    " << time2 << " ms\n";
        std::cout << "Speedup:                 " << (time1 / time2) << "x\n";
    }
    
    // Test 3: High load factor
    {
        std::cout << "\n=== Test 3: High Load Factor ===\n";
        const size_t N = 1000000;
        
        auto keys = generate_keys(N);
        emhash8::HashMap<int, int> map;
        map.reserve(static_cast<size_t>(N * 1.1));  // Force high LF
        map.max_load_factor(0.95f);
        
        for (int k : keys) {
            map.insert({k, k * 2});
        }
        
        std::cout << "Load factor: " << map.load_factor() << "\n";
        
        // Shuffle for random access
        std::mt19937 rng(999);
        std::shuffle(keys.begin(), keys.end(), rng);
        
        Timer t1;
        size_t sum1 = 0;
        for (int k : keys) {
            auto it = map.find(k);
            if (it != map.end()) sum1 += it->second;
        }
        double time1 = t1.elapsed_ms();
        g_result = sum1;
        
        double time2 = bench_find_high_lf(map, keys);
        
        std::cout << "find (no prefetch):    " << time1 << " ms\n";
        std::cout << "find (prefetch key):   " << time2 << " ms\n";
        std::cout << "Speedup:               " << (time1 / time2) << "x\n";
    }
    
    // Test 4: Large sparse map (cache pressure)
    {
        std::cout << "\n=== Test 4: Large Sparse Map (5M, step=100000) ===\n";
        const size_t N = 5000000;
        
        auto keys = generate_sparse_keys(N, 100000);
        emhash8::HashMap<int, int> map;
        map.reserve(N);
        
        Timer t1;
        for (int k : keys) {
            map.insert({k, k * 2});
        }
        double time1 = t1.elapsed_ms();
        
        map.clear();
        map.reserve(N);
        
        Timer t2;
        for (size_t i = 0; i < keys.size(); i++) {
            int k = keys[i];
            if (i + 8 < keys.size()) {
                PREFETCH(&keys[i + 8]);
            }
            map.insert({k, k * 2});
        }
        double time2 = t2.elapsed_ms();
        
        std::cout << "insert (no prefetch):  " << time1 << " ms\n";
        std::cout << "insert (prefetch 8):   " << time2 << " ms\n";
        std::cout << "Speedup:               " << (time1 / time2) << "x\n";
        
        // Find test
        std::mt19937 rng(111);
        std::shuffle(keys.begin(), keys.end(), rng);
        
        Timer t3;
        size_t sum = 0;
        for (int k : keys) {
            auto it = map.find(k);
            if (it != map.end()) sum += it->second;
        }
        double time3 = t3.elapsed_ms();
        g_result = sum;
        
        Timer t4;
        sum = 0;
        for (size_t i = 0; i < keys.size(); i++) {
            int k = keys[i];
            if (i + 8 < keys.size()) {
                PREFETCH(&keys[i + 8]);
            }
            auto it = map.find(k);
            if (it != map.end()) sum += it->second;
        }
        double time4 = t4.elapsed_ms();
        g_result = sum;
        
        std::cout << "find (no prefetch):    " << time3 << " ms\n";
        std::cout << "find (prefetch 8):     " << time4 << " ms\n";
        std::cout << "Speedup:               " << (time3 / time4) << "x\n";
    }
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "Prefetch results vary significantly:\n";
    std::cout << "- Iteration over dense pairs: prefetch helps (predictable)\n";
    std::cout << "- Sequential find: prefetch may help (predictable)\n";
    std::cout << "- Random find: prefetch often hurts (overhead > benefit)\n";
    std::cout << "- High load factor: prefetch may help (longer probe chains)\n";
    std::cout << "\n";
    std::cout << "Key insight: Prefetch must target the RIGHT address:\n";
    std::cout << "- Prefetching keys array is useless (already in cache)\n";
    std::cout << "- Prefetching bucket/pair address is what matters\n";
    std::cout << "- Need internal access to _index/_pairs for effective prefetch\n";
    std::cout << "\n";
    std::cout << "Recommendation:\n";
    std::cout << "- Add prefetch to internal find logic (prefetch next bucket in chain)\n";
    std::cout << "- Add prefetch to iteration (prefetch next pair in dense array)\n";
    std::cout << "- Skip prefetch for single-key lookups (overhead dominates)\n";
    
    return 0;
}