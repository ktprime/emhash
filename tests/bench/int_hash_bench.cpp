// Benchmark: std::hash<int> vs wyhash64 (EMH_INT_HASH)
// Tests whether wyhash64 is faster than std::hash for integer keys

#include <iostream>
#include <cstdint>
#include <random>
#include <ctime>
#include <functional>

#ifdef __GNUC__
#include <x86intrin.h>
#endif

// Prevent optimization
volatile size_t g_hash_result = 0;

// std::hash<int> implementation (typical libstdc++)
static size_t std_hash_int(int key) {
    return std::hash<int>{}(key);
}

// wyhash64 implementation (from emhash config.hpp)
static inline uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static inline uint64_t wyhash64(uint64_t A, uint64_t B) {
    A ^= 0xa0761d6478bd642fULL;
    B ^= 0xe7037ed1a0b428dbULL;
    A ^= B;
    A ^= rotl64(A, 41) ^ rotl64(B, 10);
    B ^= rotl64(B, 59) ^ rotl64(A, 27);
    A ^= rotl64(A, 7) ^ rotl64(B, 18);
    B ^= rotl64(B, 51) ^ rotl64(A, 46);
    return A ^ B;
}

// EMH_INT_HASH style: wyhash64 for integer
static size_t wyhash_int(int key) {
    return wyhash64(static_cast<uint64_t>(key), 0);
}

// EMH_INT_HASH alternative: multiply-shift (simpler)
static size_t mulshift_int(int key) {
    // Knuth's multiplicative hash
    const uint64_t M = 0x9e3779b97f4a7c15ULL; // golden ratio
    return static_cast<uint64_t>(key) * M;
}

// Identity hash (no transformation) - for testing only
static size_t identity_int(int key) {
    return static_cast<size_t>(key);
}

// Benchmark function
template<typename HashFunc>
double benchmark_hash(HashFunc func, size_t iterations) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 1000000);

    // Warmup
    for (size_t i = 0; i < 10000; i++) {
        g_hash_result = func(dist(rng));
    }

    // Actual benchmark
    clock_t start = clock();
    for (size_t i = 0; i < iterations; i++) {
        int key = dist(rng);
        g_hash_result = func(key);
    }
    clock_t end = clock();

    return (double)(end - start) * 1000 / CLOCKS_PER_SEC;
}

// Benchmark with sequential keys (better cache locality)
template<typename HashFunc>
double benchmark_hash_sequential(HashFunc func, size_t iterations) {
    // Warmup
    for (size_t i = 0; i < 10000; i++) {
        g_hash_result = func(static_cast<int>(i));
    }

    // Actual benchmark
    clock_t start = clock();
    for (size_t i = 0; i < iterations; i++) {
        g_hash_result = func(static_cast<int>(i));
    }
    clock_t end = clock();

    return (double)(end - start) * 1000 / CLOCKS_PER_SEC;
}

// Benchmark with repeated same key (worst case for identity hash)
template<typename HashFunc>
double benchmark_hash_samekey(HashFunc func, size_t iterations) {
    const int key = 12345;

    // Warmup
    for (size_t i = 0; i < 10000; i++) {
        g_hash_result = func(key);
    }

    // Actual benchmark
    clock_t start = clock();
    for (size_t i = 0; i < iterations; i++) {
        g_hash_result = func(key);
    }
    clock_t end = clock();

    return (double)(end - start) * 1000 / CLOCKS_PER_SEC;
}

int main() {
    std::cout << "=== Integer Hash Performance Benchmark ===\n\n";

    const size_t iterations = 100000000; // 100M iterations

    std::cout << "Test 1: Random keys (simulates real hash map usage)\n";
    std::cout << "Hash Function | Time (ms) | Throughput (M/s) | Speedup vs std::hash\n";
    std::cout << "--------------|-----------|------------------|--------------------\n";

    double time_std = benchmark_hash(std_hash_int, iterations);
    double time_wy = benchmark_hash(wyhash_int, iterations);
    double time_mul = benchmark_hash(mulshift_int, iterations);
    double time_id = benchmark_hash(identity_int, iterations);

    std::cout << "std::hash<int> | " << time_std << " | "
              << iterations / time_std / 1000 << " | 1.00x\n";
    std::cout << "wyhash64       | " << time_wy << " | "
              << iterations / time_wy / 1000 << " | "
              << time_std / time_wy << "x\n";
    std::cout << "mul-shift      | " << time_mul << " | "
              << iterations / time_mul / 1000 << " | "
              << time_std / time_mul << "x\n";
    std::cout << "identity       | " << time_id << " | "
              << iterations / time_id / 1000 << " | "
              << time_std / time_id << "x\n";

    std::cout << "\nTest 2: Sequential keys (i=0,1,2,...)\n";
    std::cout << "Hash Function | Time (ms) | Speedup vs std::hash\n";
    std::cout << "--------------|-----------|--------------------\n";

    time_std = benchmark_hash_sequential(std_hash_int, iterations);
    time_wy = benchmark_hash_sequential(wyhash_int, iterations);
    time_mul = benchmark_hash_sequential(mulshift_int, iterations);
    time_id = benchmark_hash_sequential(identity_int, iterations);

    std::cout << "std::hash<int> | " << time_std << " | 1.00x\n";
    std::cout << "wyhash64       | " << time_wy << " | " << time_std / time_wy << "x\n";
    std::cout << "mul-shift      | " << time_mul << " | " << time_std / time_mul << "x\n";
    std::cout << "identity       | " << time_id << " | " << time_std / time_id << "x\n";

    std::cout << "\nTest 3: Same key repeated (worst for identity)\n";
    std::cout << "Hash Function | Time (ms) | Speedup vs std::hash\n";
    std::cout << "--------------|-----------|--------------------\n";

    time_std = benchmark_hash_samekey(std_hash_int, iterations);
    time_wy = benchmark_hash_samekey(wyhash_int, iterations);
    time_mul = benchmark_hash_samekey(mulshift_int, iterations);
    time_id = benchmark_hash_samekey(identity_int, iterations);

    std::cout << "std::hash<int> | " << time_std << " | 1.00x\n";
    std::cout << "wyhash64       | " << time_wy << " | " << time_std / time_wy << "x\n";
    std::cout << "mul-shift      | " << time_mul << " | " << time_std / time_mul << "x\n";
    std::cout << "identity       | " << time_id << " | " << time_std / time_id << "x\n";

    std::cout << "\n=== Analysis ===\n";
    std::cout << "std::hash<int>: Usually implemented as identity hash (key % 2^64)\n";
    std::cout << "                or multiplicative hash depending on libstdc++ version\n";
    std::cout << "\n";
    std::cout << "wyhash64:       Strong hash with good distribution, prevents hash attacks\n";
    std::cout << "                Cost: ~8 arithmetic ops + 6 rotations\n";
    std::cout << "\n";
    std::cout << "mul-shift:      Simple multiplicative hash (1 multiplication)\n";
    std::cout << "                Good enough for most cases, faster than wyhash\n";
    std::cout << "\n";
    std::cout << "identity:       No transformation (key directly used as hash)\n";
    std::cout << "                Fastest but vulnerable to hash flooding attacks\n";
    std::cout << "\n";
    std::cout << "Recommendation:\n";
    std::cout << "- For security-sensitive code: use wyhash64 (EMH_INT_HASH=1)\n";
    std::cout << "- For trusted input only: use mul-shift or identity\n";
    std::cout << "- std::hash<int> is usually identity-like, so wyhash64 may be slower\n";
    std::cout << "  but provides better hash distribution for hash map performance\n";

    return 0;
}