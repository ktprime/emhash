// Simple SIMD bitmask performance test - minimal version
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <ctime>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// Prevent optimization by using result
volatile size_t g_result = 0;

// Current emhash: uint8_t bitmask
static size_t find_empty_uint8(const uint8_t* bitmask, size_t bucket_from, size_t num_buckets) {
    const auto boset = bucket_from % 8;
    const auto align = bitmask + bucket_from / 8;
    size_t bmask;
    memcpy(&bmask, align, sizeof(bmask));
    bmask >>= boset;

    if (bmask != 0)
        return bucket_from + __builtin_ctzll(bmask);

    const size_t qmask = (num_buckets - 1) / 64;
    size_t last = (bucket_from / 64 + 1) & qmask;
    for (int i = 0; i < 100; i++) {
        const auto bmask2 = *((size_t*)bitmask + last);
        if (bmask2 != 0)
            return last * 64 + __builtin_ctzll(bmask2);
        last = (last + 1) & qmask;
    }
    return 0;
}

// Optimized: uint64_t bitmask
static size_t find_empty_uint64(const uint64_t* bitmask, size_t bucket_from, size_t num_buckets) {
    const auto idx = bucket_from / 64;
    const auto boset = bucket_from % 64;
    auto bmask = bitmask[idx] >> boset;

    if (bmask != 0)
        return bucket_from + __builtin_ctzll(bmask);

    const size_t num_blocks = num_buckets / 64;
    size_t last = (idx + 1) % num_blocks;
    for (int i = 0; i < 100; i++) {
        if (bitmask[last] != 0)
            return last * 64 + __builtin_ctzll(bitmask[last]);
        last = (last + 1) % num_blocks;
    }
    return 0;
}

#ifdef __AVX2__
static size_t find_empty_avx2(const uint8_t* bitmask, size_t bucket_from, size_t num_buckets) {
    const size_t block_idx = bucket_from / 256;
    __m256i vec = _mm256_loadu_si256((__m256i*)(bitmask + block_idx * 32));

    if (_mm256_testz_si256(vec, vec) == 0) {
        const uint64_t* blocks = (const uint64_t*)(bitmask + block_idx * 32);
        for (int i = 0; i < 4; i++) {
            if (blocks[i] != 0)
                return block_idx * 256 + i * 64 + __builtin_ctzll(blocks[i]);
        }
    }

    const size_t num_blocks = num_buckets / 256;
    size_t last = (block_idx + 1) % num_blocks;
    for (int i = 0; i < 50; i++) {
        __m256i v = _mm256_loadu_si256((__m256i*)(bitmask + last * 32));
        if (_mm256_testz_si256(v, v) == 0) {
            const uint64_t* blocks = (const uint64_t*)(bitmask + last * 32);
            for (int j = 0; j < 4; j++) {
                if (blocks[j] != 0)
                    return last * 256 + j * 64 + __builtin_ctzll(blocks[j]);
            }
        }
        last = (last + 1) % num_blocks;
    }
    return 0;
}
#endif

// Generate bitmask (bit=1 means empty)
static std::vector<uint8_t> generate_bitmask(size_t num_buckets, double load_factor) {
    std::vector<uint8_t> bitmask(num_buckets / 8 + 32, 0xFF);
    std::mt19937_64 rng(42);
    size_t occupied = 0;
    size_t target = static_cast<size_t>(num_buckets * load_factor);

    while (occupied < target) {
        size_t bucket = rng() % num_buckets;
        if ((bitmask[bucket / 8] & (1 << (bucket % 8))) != 0) {
            bitmask[bucket / 8] &= ~(1 << (bucket % 8));
            occupied++;
        }
    }
    return bitmask;
}

int main() {
    std::cout << "=== SIMD Bitmask Performance Test ===\n\n";

    const size_t num_buckets = 1 << 20; // 1M buckets
    const size_t iterations = 50000000; // 50M iterations

    double load_factors[] = {0.80, 0.90, 0.95, 0.99};

    std::cout << "LF   | uint8_t (ms) | uint64_t (ms) | AVX2 (ms) | uint64 speedup | AVX2 speedup\n";
    std::cout << "-----|-------------|--------------|----------|---------------|------------\n";

    for (double lf : load_factors) {
        auto bitmask8 = generate_bitmask(num_buckets, lf);
        std::vector<uint64_t> bitmask64(num_buckets / 64 + 4);
        memcpy(bitmask64.data(), bitmask8.data(), bitmask8.size());

        // Benchmark uint8_t
        clock_t start8 = clock();
        size_t bucket_from = 0;
        for (size_t i = 0; i < iterations; i++) {
            g_result = find_empty_uint8(bitmask8.data(), bucket_from, num_buckets);
            bucket_from = (g_result + 1) % num_buckets;
        }
        clock_t end8 = clock();
        double time8 = (double)(end8 - start8) * 1000 / CLOCKS_PER_SEC;

        // Benchmark uint64_t
        clock_t start64 = clock();
        bucket_from = 0;
        for (size_t i = 0; i < iterations; i++) {
            g_result = find_empty_uint64(bitmask64.data(), bucket_from, num_buckets);
            bucket_from = (g_result + 1) % num_buckets;
        }
        clock_t end64 = clock();
        double time64 = (double)(end64 - start64) * 1000 / CLOCKS_PER_SEC;

#ifdef __AVX2__
        // Benchmark AVX2
        clock_t startavx = clock();
        bucket_from = 0;
        for (size_t i = 0; i < iterations; i++) {
            g_result = find_empty_avx2(bitmask8.data(), bucket_from, num_buckets);
            bucket_from = (g_result + 1) % num_buckets;
        }
        clock_t endavx = clock();
        double timeavx = (double)(endavx - startavx) * 1000 / CLOCKS_PER_SEC;
#else
        double timeavx = time8; // fallback
#endif

        double speedup64 = time8 / time64;
        double speedup_avx2 = time8 / timeavx;

        std::cout << lf * 100 << "% | " << time8 << " | " << time64 << " | " << timeavx << " | " << speedup64 << "x | "
                  << speedup_avx2 << "x\n";
    }

    std::cout << "\n=== Conclusion ===\n";
    std::cout << "uint64_t: Direct 64-bit load eliminates memcpy, expected 1.1-1.3x faster\n";
    std::cout << "AVX2:     Checks 256 bits at once, expected 1.2-1.5x faster at LF>=0.95\n";
    std::cout << "\nNote: Actual speedup depends on CPU cache hierarchy and memory bandwidth.\n";

    return 0;
}