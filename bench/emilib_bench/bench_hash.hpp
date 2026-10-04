#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <type_traits>

#include "bench_timer.hpp"
#include "../../include/emhash/config.hpp"

namespace emilib_bench {

// ============================================================================
// FNV-1a hash implementation (inline)
// ============================================================================
struct Fnv1aHash {
    static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    static constexpr uint64_t FNV_PRIME        = 1099511628211ULL;

    // String specialization
    std::size_t operator()(const std::string& key) const noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        for (unsigned char c : key) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        return static_cast<std::size_t>(hash);
    }

    // Generic byte-range hash
    static uint64_t hash_bytes(const void* data, size_t len) noexcept {
        uint64_t hash = FNV_OFFSET_BASIS;
        const auto* p = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(p[i]);
            hash *= FNV_PRIME;
        }
        return hash;
    }
};

// FNV-1a for integer keys (treats the integer as raw bytes)
template <typename IntT>
struct Fnv1aIntHash {
    std::size_t operator()(IntT key) const noexcept {
        return static_cast<std::size_t>(Fnv1aHash::hash_bytes(&key, sizeof(IntT)));
    }
};

// ============================================================================
// Mul-xor hash for integers (fast non-avalanching hash)
// ============================================================================
template <typename IntT>
struct MulXorHash {
    static constexpr uint64_t MUL_CONSTANT = 0x9E3779B97F4A7C15ULL; // golden ratio

    std::size_t operator()(IntT key) const noexcept {
        uint64_t k = static_cast<uint64_t>(key);
        k *= MUL_CONSTANT;
        return static_cast<std::size_t>(k ^ (k >> 32));
    }
};

// ============================================================================
// Wyhash wrapper for std::string (uses emhash_detail::emh_wyhash from config.hpp)
// ============================================================================
struct WyHashString {
    std::size_t operator()(const std::string& key) const noexcept {
        return static_cast<std::size_t>(
            ::emh_wyhash(key.data(), key.size(), 0x9E3779B97F4A7C15ULL));
    }
};

// Wyhash wrapper for integer keys (uses emh_wyhash64 from config.hpp)
template <typename IntT>
struct WyHashInt {
    std::size_t operator()(IntT key) const noexcept {
        return static_cast<std::size_t>(
            emhash_detail::emh_wyhash64(static_cast<uint64_t>(key), 0x9E3779B97F4A7C15ULL));
    }
};

// ============================================================================
// Hash benchmark result
// ============================================================================
struct HashBenchResult {
    std::string hash_name;
    uint64_t    compute_time_ns = 0;  // Total time to hash all keys
    double      collision_rate  = 0.0; // 1 - (unique_hashes / total_keys)
    double      avalanche_score = 0.0; // Bit-flip avalanche quality (0.0 - 1.0, ideal=0.5)
};

// ============================================================================
// Hash function speed benchmark
// ============================================================================
template <typename HashT, typename KeyT>
uint64_t measure_hash_speed(const std::vector<KeyT>& keys, const HashT& hasher) {
    volatile std::size_t sink = 0; // Prevent optimization
    uint64_t elapsed_ns = 0;

    {
        ScopedTimer timer(elapsed_ns);
        for (const auto& k : keys) {
            sink = hasher(k);
        }
    }
    (void)sink;
    return elapsed_ns;
}

// ============================================================================
// Collision rate measurement
// ============================================================================
template <typename HashT, typename KeyT>
double measure_collision_rate(const std::vector<KeyT>& keys, const HashT& hasher) {
    if (keys.empty()) return 0.0;

    std::unordered_set<std::size_t> unique_hashes;
    unique_hashes.reserve(keys.size());
    for (const auto& k : keys) {
        unique_hashes.insert(hasher(k));
    }

    double total = static_cast<double>(keys.size());
    double unique = static_cast<double>(unique_hashes.size());
    return 1.0 - (unique / total);
}

// ============================================================================
// Avalanche score measurement
// Flip each bit of a sample key and measure how many output bits change.
// An ideal hash flips ~50% of output bits for each input bit change.
// Returns a score between 0.0 and 1.0 (closer to 0.5 is ideal).
// ============================================================================
template <typename HashT, typename KeyT>
double measure_avalanche(const HashT& hasher, int sample_count = 100) {
    // Only applicable to integer key types
    if constexpr (!std::is_integral_v<KeyT>) {
        (void)sample_count;
        return -1.0; // Not applicable for string keys
    } else {
        double total_deviation = 0.0;
        int tested = 0;

        for (int s = 0; s < sample_count; ++s) {
            KeyT base = static_cast<KeyT>(s * 7919 + 1); // Pseudo-random base key
            std::size_t base_hash = hasher(base);

            int bit_width = sizeof(KeyT) * 8;
            double bit_deviation_sum = 0.0;

            for (int bit = 0; bit < bit_width; ++bit) {
                KeyT flipped = base ^ static_cast<KeyT>(1ULL << bit);
                std::size_t flipped_hash = hasher(flipped);

                // Count output bits that differ
                std::size_t diff = base_hash ^ flipped_hash;
                int flipped_bits = 0;
                while (diff) {
                    flipped_bits += diff & 1;
                    diff >>= 1;
                }

                double ratio = static_cast<double>(flipped_bits) / (sizeof(std::size_t) * 8.0);
                // Deviation from ideal 0.5
                bit_deviation_sum += std::abs(ratio - 0.5);
            }

            total_deviation += bit_deviation_sum / bit_width;
            ++tested;
        }

        if (tested == 0) return -1.0;
        // Average deviation from 0.5; we return 0.5 - avg_dev so that
        // a score close to 0.5 means good avalanche behavior.
        return 0.5 - (total_deviation / tested);
    }
}

// ============================================================================
// Bench a single hash function over a set of keys
// ============================================================================
template <typename HashT, typename KeyT>
HashBenchResult bench_hash_func(const std::vector<KeyT>& keys, const std::string& name, const HashT& hasher) {
    HashBenchResult result;
    result.hash_name      = name;
    result.compute_time_ns = measure_hash_speed(keys, hasher);
    result.collision_rate  = measure_collision_rate(keys, hasher);
    result.avalanche_score = measure_avalanche<HashT, KeyT>(hasher);
    return result;
}

// ============================================================================
// Run comparison across all hash functions for the given keys
// ============================================================================
template <typename KeyT>
std::vector<HashBenchResult> run_hash_comparison(const std::vector<KeyT>& keys) {
    std::vector<HashBenchResult> results;

    // 1. std::hash<KeyT>
    {
        std::hash<KeyT> hasher;
        results.push_back(bench_hash_func(keys, "std::hash", hasher));
    }

    // 2. emh_wyhash (dispatches to WyHashInt for integers, WyHashString for strings)
    if constexpr (std::is_integral_v<KeyT>) {
        WyHashInt<KeyT> hasher;
        results.push_back(bench_hash_func(keys, "emh_wyhash", hasher));
    } else if constexpr (std::is_same_v<KeyT, std::string>) {
        WyHashString hasher;
        results.push_back(bench_hash_func(keys, "emh_wyhash", hasher));
    }

    // 3. FNV-1a
    if constexpr (std::is_integral_v<KeyT>) {
        Fnv1aIntHash<KeyT> hasher;
        results.push_back(bench_hash_func(keys, "FNV-1a", hasher));
    } else if constexpr (std::is_same_v<KeyT, std::string>) {
        Fnv1aHash hasher;
        results.push_back(bench_hash_func(keys, "FNV-1a", hasher));
    }

    // 4. Mul-xor (integers only)
    if constexpr (std::is_integral_v<KeyT>) {
        MulXorHash<KeyT> hasher;
        results.push_back(bench_hash_func(keys, "mul-xor", hasher));
    }

    return results;
}

} // namespace emilib_bench
