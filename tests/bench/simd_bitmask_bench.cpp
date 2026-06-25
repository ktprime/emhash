// SIMD bitmask optimization benchmark for emhash
// Compare: uint8_t bitmask vs uint64_t vs AVX2 vs AVX-512

#include <benchmark/benchmark.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <atomic>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __AVX512F__
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
    while (true) {
        const auto bmask2 = *((size_t*)bitmask + last);
        if (bmask2 != 0) {
            return last * 64 + __builtin_ctzll(bmask2);
        }
        last = (last + 1) & qmask;
    }
    return 0;
}

// Optimized: uint64_t bitmask (8x wider per check)
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
    while (true) {
        if (bitmask[last] != 0) {
            return last * 64 + __builtin_ctzll(bitmask[last]);
        }
        last = (last + 1) % num_blocks;
    }
    return 0;
}

#ifdef __AVX2__
// AVX2: check 256 bits (4x uint64_t) at once
static size_t find_empty_avx2(const uint8_t* bitmask, size_t bucket_from, size_t num_buckets) {
    // First check the local 256-bit block
    const size_t block_idx = bucket_from / 256;
    const size_t offset = bucket_from % 256;

    // Load 256 bits (32 bytes)
    __m256i vec = _mm256_loadu_si256((__m256i*)(bitmask + block_idx * 32));

    // Create mask for bits >= offset
    // This is complex - simpler approach: check if any bit is set
    if (_mm256_testz_si256(vec, vec) == 0) {
        // Some bits are set (empty buckets exist)
        // Extract and find first set bit >= offset
        // For simplicity, fall back to uint64 method
        const uint64_t* blocks = (const uint64_t*)(bitmask + block_idx * 32);
        for (int i = offset / 64; i < 4; i++) {
            if (blocks[i] != 0) {
                return block_idx * 256 + i * 64 + __builtin_ctzll(blocks[i]);
            }
        }
    }

    // Fallback: scan other 256-bit blocks
    const size_t num_blocks = num_buckets / 256;
    size_t last = (block_idx + 1) % num_blocks;
    while (true) {
        __m256i v = _mm256_loadu_si256((__m256i*)(bitmask + last * 32));
        if (_mm256_testz_si256(v, v) == 0) {
            const uint64_t* blocks = (const uint64_t*)(bitmask + last * 32);
            for (int i = 0; i < 4; i++) {
                if (blocks[i] != 0) {
                    return last * 256 + i * 64 + __builtin_ctzll(blocks[i]);
                }
            }
        }
        last = (last + 1) % num_blocks;
    }
    return 0;
}
#endif

// Generate bitmask with given load factor (0=empty, 1=occupied)
static std::vector<uint8_t> generate_bitmask(size_t num_buckets, double load_factor) {
    std::vector<uint8_t> bitmask(num_buckets / 8, 0);
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    size_t occupied = 0;
    size_t target = static_cast<size_t>(num_buckets * load_factor);

    for (size_t i = 0; i < num_buckets && occupied < target; i++) {
        if (dist(rng) < load_factor) {
            bitmask[i / 8] |= (1 << (i % 8));
            occupied++;
        }
    }
    return bitmask;
}

// Benchmark fixtures
class BitmaskBenchmark : public benchmark::Fixture {
protected:
    std::vector<uint8_t> bitmask8;
    std::vector<uint64_t> bitmask64;
    size_t num_buckets = 1 << 20; // 1M buckets

    void SetUp(const benchmark::State& state) override {
        double lf = state.range(0) / 100.0;
        bitmask8 = generate_bitmask(num_buckets, lf);

        // Convert to uint64_t format
        bitmask64.resize(num_buckets / 64);
        memcpy(bitmask64.data(), bitmask8.data(), bitmask8.size());
    }
};

// Benchmark: uint8_t (current emhash)
BENCHMARK_DEFINE_F(BitmaskBenchmark, Uint8)(benchmark::State& state) {
    size_t bucket_from = 0;
    size_t found = 0;
    for (auto _ : state) {
        found = find_empty_uint8(bitmask8.data(), bucket_from, num_buckets);
        bucket_from = (found + 1) % num_buckets;
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(BitmaskBenchmark, Uint8)->Arg(80)->Arg(90)->Arg(95)->Arg(99);

// Benchmark: uint64_t
BENCHMARK_DEFINE_F(BitmaskBenchmark, Uint64)(benchmark::State& state) {
    size_t bucket_from = 0;
    size_t found = 0;
    for (auto _ : state) {
        found = find_empty_uint64(bitmask64.data(), bucket_from, num_buckets);
        bucket_from = (found + 1) % num_buckets;
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(BitmaskBenchmark, Uint64)->Arg(80)->Arg(90)->Arg(95)->Arg(99);

#ifdef __AVX2__
// Benchmark: AVX2
BENCHMARK_DEFINE_F(BitmaskBenchmark, AVX2)(benchmark::State& state) {
    size_t bucket_from = 0;
    size_t found = 0;
    for (auto _ : state) {
        found = find_empty_avx2(bitmask8.data(), bucket_from, num_buckets);
        bucket_from = (found + 1) % num_buckets;
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(BitmaskBenchmark, AVX2)->Arg(80)->Arg(90)->Arg(95)->Arg(99);
#endif

// Single-threaded latency test
static void BM_SingleFind(benchmark::State& state) {
    const double lf = state.range(0) / 100.0;
    const size_t num_buckets = 1 << 16; // 64K buckets
    auto bitmask8 = generate_bitmask(num_buckets, lf);

    std::vector<uint64_t> bitmask64(num_buckets / 64);
    memcpy(bitmask64.data(), bitmask8.data(), bitmask8.size());

    size_t bucket_from = state.range(1);
    size_t found = 0;

    for (auto _ : state) {
        if (state.range(2) == 0) {
            found = find_empty_uint8(bitmask8.data(), bucket_from, num_buckets);
        } else if (state.range(2) == 1) {
            found = find_empty_uint64(bitmask64.data(), bucket_from, num_buckets);
        }
#ifdef __AVX2__
        else {
            found = find_empty_avx2(bitmask8.data(), bucket_from, num_buckets);
        }
#endif
        benchmark::DoNotOptimize(found);
    }
}
// Compare at different load factors and starting positions
BENCHMARK(BM_SingleFind)->Args({80, 0, 0})->Args({80, 0, 1})
                         ->Args({95, 0, 0})->Args({95, 0, 1})
                         ->Args({99, 0, 0})->Args({99, 0, 1})
                         ->Args({99, 32000, 0})->Args({99, 32000, 1});

BENCHMARK_MAIN();