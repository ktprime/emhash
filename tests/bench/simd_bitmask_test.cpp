// Simple SIMD bitmask performance test (no external dependencies)
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <chrono>
#include <atomic>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// Current emhash implementation: uint8_t bitmask with CTZ
static size_t find_empty_uint8(const uint8_t* bitmask, size_t bucket_from, size_t num_buckets) {
    const auto boset = bucket_from % 8;
    const auto align = bitmask + bucket_from / 8;
    size_t bmask;
    memcpy(&bmask, align, sizeof(bmask));
    bmask >>= boset;

    if (bmask != 0) {
        return bucket_from + __builtin_ctzll(bmask);
    }

    // Fallback: scan other regions
    const size_t qmask = (num_buckets - 1) / 64;
    size_t last = (bucket_from / 64 + 1) & qmask;
    for (int i = 0; i < 100; i++) { // limit iterations
        const auto bmask2 = *((size_t*)bitmask + last);
        if (bmask2 != 0) {
            return last * 64 + __builtin_ctzll(bmask2);
        }
        last = (last + 1) & qmask;
    }
    return 0;
}

// Optimized: uint64_t bitmask (native 64-bit operations)
static size_t find_empty_uint64(const uint64_t* bitmask, size_t bucket_from, size_t num_buckets) {
    const auto idx = bucket_from / 64;
    const auto boset = bucket_from % 64;
    auto bmask = bitmask[idx] >> boset;

    if (bmask != 0) {
        return bucket_from + __builtin_ctzll(bmask);
    }

    // Fallback: scan other 64-bit blocks
    const size_t num_blocks = num_buckets / 64;
    size_t last = (idx + 1) % num_blocks;
    for (int i = 0; i < 100; i++) {
        if (bitmask[last] != 0) {
            return last * 64 + __builtin_ctzll(bitmask[last]);
        }
        last = (last + 1) % num_blocks;
    }
    return 0;
}

#ifdef __AVX2__
// AVX2: check 256 bits at once
static size_t find_empty_avx2(const uint8_t* bitmask, size_t bucket_from, size_t num_buckets) {
    const size_t block_idx = bucket_from / 256;
    const size_t offset = bucket_from % 256;

    // Load 256 bits
    __m256i vec = _mm256_loadu_si256((__m256i*)(bitmask + block_idx * 32));

    // Check if any bit is set (empty bucket exists)
    if (_mm256_testz_si256(vec, vec) == 0) {
        // Find which 64-bit chunk has the bit
        const uint64_t* blocks = (const uint64_t*)(bitmask + block_idx * 32);
        for (int i = offset / 64; i < 4; i++) {
            uint64_t shifted = blocks[i] >> (offset % 64);
            if (i > offset / 64) shifted = blocks[i];
            if (shifted != 0) {
                return block_idx * 256 + i * 64 + __builtin_ctzll(shifted);
            }
        }
    }

    // Fallback: scan other 256-bit blocks
    const size_t num_blocks = num_buckets / 256;
    size_t last = (block_idx + 1) % num_blocks;
    for (int i = 0; i < 50; i++) {
        __m256i v = _mm256_loadu_si256((__m256i*)(bitmask + last * 32));
        if (_mm256_testz_si256(v, v) == 0) {
            const uint64_t* blocks = (const uint64_t*)(bitmask + last * 32);
            for (int j = 0; j < 4; j++) {
                if (blocks[j] != 0) {
                    return last * 256 + j * 64 + __builtin_ctzll(blocks[j]);
                }
            }
        }
        last = (last + 1) % num_blocks;
    }
    return 0;
}
#endif

// Generate bitmask with given load factor (bit=1 means occupied)
static std::vector<uint8_t> generate_bitmask(size_t num_buckets, double load_factor) {
    std::vector<uint8_t> bitmask(num_buckets / 8 + 32, 0xFF); // Start all empty (1)

    std::mt19937_64 rng(42);
    size_t occupied = 0;
    size_t target = static_cast<size_t>(num_buckets * load_factor);

    // Mark buckets as occupied (set bit to 0)
    while (occupied < target) {
        size_t bucket = rng() % num_buckets;
        if ((bitmask[bucket / 8] & (1 << (bucket % 8))) != 0) {
            bitmask[bucket / 8] &= ~(1 << (bucket % 8));
            occupied++;
        }
    }
    return bitmask;
}

template<typename Func>
double benchmark_find(Func func, const std::vector<uint8_t>& bitmask8,
                      const std::vector<uint64_t>& bitmask64,
                      size_t num_buckets, size_t iterations) {
    auto start = std::chrono::high_resolution_clock::now();

    size_t bucket_from = 0;
    size_t found = 0;
    for (size_t i = 0; i < iterations; i++) {
        found = func(bitmask8.data(), bitmask64.data(), bucket_from, num_buckets);
        bucket_from = (found + 1) % num_buckets;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    return elapsed.count() / iterations; // microseconds per operation
}

int main() {
    std::cout << "=== SIMD Bitmask Optimization Benchmark ===\n\n";

    // Test different sizes and load factors
    struct TestCase {
        size_t num_buckets;
        double load_factor;
    };

    TestCase cases[] = {
        {1 << 16, 0.80},  // 64K, LF=0.80 (easy)
        {1 << 16, 0.90},  // 64K, LF=0.90 (medium)
        {1 << 16, 0.95},  // 64K, LF=0.95 (hard)
        {1 << 16, 0.99},  // 64K, LF=0.99 (extreme)
        {1 << 20, 0.80},  // 1M, LF=0.80
        {1 << 20, 0.95},  // 1M, LF=0.95
        {1 << 20, 0.99},  // 1M, LF=0.99
    };

    std::cout << "Load Factor | Size | uint8_t (ns) | uint64_t (ns) | AVX2 (ns) | uint64 vs uint8 | AVX2 vs uint8\n";
    std::cout << "------------|------|-------------|--------------|----------|----------------|-------------\n";

    for (const auto& tc : cases) {
        auto bitmask8 = generate_bitmask(tc.num_buckets, tc.load_factor);
        std::vector<uint64_t> bitmask64(tc.num_buckets / 64 + 4);
        memcpy(bitmask64.data(), bitmask8.data(), bitmask8.size());

        const size_t iterations = 10000000; // 10M iterations

        // Benchmark uint8_t
        auto t8 = benchmark_find([](const uint8_t* b8, const uint64_t* b64, size_t from, size_t n) {
            return find_empty_uint8(b8, from, n);
        }, bitmask8, bitmask64, tc.num_buckets, iterations);

        // Benchmark uint64_t
        auto t64 = benchmark_find([](const uint8_t* b8, const uint64_t* b64, size_t from, size_t n) {
            return find_empty_uint64(b64, from, n);
        }, bitmask8, bitmask64, tc.num_buckets, iterations);

#ifdef __AVX2__
        auto tavx2 = benchmark_find([](const uint8_t* b8, const uint64_t* b64, size_t from, size_t n) {
            return find_empty_avx2(b8, from, n);
        }, bitmask8, bitmask64, tc.num_buckets, iterations);
#else
        double tavx2 = 0;
#endif

        double speedup64 = t8 / t64;
        double speedup_avx2 = t8 / tavx2;

        std::cout << tc.load_factor * 100 << "% | "
                  << (tc.num_buckets >= 1<<20 ? "1M" : "64K") << " | "
                  << t8 * 1000 << " | "
                  << t64 * 1000 << " | "
                  << tavx2 * 1000 << " | "
                  << speedup64 << "x | "
                  << speedup_avx2 << "x\n";
    }

    std::cout << "\n=== Analysis ===\n";
    std::cout << "uint64_t optimization:\n";
    std::cout << "  - Eliminates memcpy overhead (direct 64-bit load)\n";
    std::cout << "  - Better cache utilization (8x fewer memory accesses per check)\n";
    std::cout << "  - Expected: 1.2-1.5x faster at LF>=0.95\n\n";

#ifdef __AVX2__
    std::cout << "AVX2 optimization:\n";
    std::cout << "  - Checks 256 bits (4x uint64_t) per iteration\n";
    std::cout << "  - _mm256_testz_si256 is fast (single instruction)\n";
    std::cout << "  - BUT: fallback still uses uint64_t extraction\n";
    std::cout << "  - Expected: 1.3-2x faster at LF>=0.99 (sparse empty buckets)\n";
#else
    std::cout << "AVX2 not available on this platform\n";
#endif

    return 0;
}