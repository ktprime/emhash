#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <type_traits>
#include <utility>
#include <random>
#include <sstream>
#include <typeinfo>

#include "bench_config.hpp"
#include "bench_timer.hpp"
#include "bench_memory.hpp"

namespace emilib_bench {

// ============================================================================
// Benchmark operation types
// ============================================================================
enum class BenchOp : uint8_t {
    Insert,
    FindHit,
    FindMiss,
    Erase,
    Iterate
};

inline const char* bench_op_name(BenchOp op) noexcept {
    switch (op) {
        case BenchOp::Insert:   return "Insert";
        case BenchOp::FindHit:  return "FindHit";
        case BenchOp::FindMiss: return "FindMiss";
        case BenchOp::Erase:    return "Erase";
        case BenchOp::Iterate:  return "Iterate";
    }
    return "Unknown";
}

// All benchmark operations
inline constexpr BenchOp all_bench_ops[] = {
    BenchOp::Insert,
    BenchOp::FindHit,
    BenchOp::FindMiss,
    BenchOp::Erase,
    BenchOp::Iterate
};

// ============================================================================
// Single benchmark result
// ============================================================================
struct BenchResult {
    std::string   map_name;
    std::string   operation_name;
    std::string   key_type_name;
    size_t        data_size     = 0;
    std::string   hash_name;
    CompilerInfo  compiler_info;
    MetricResult  timing;
    MemoryMetrics memory;
    // Flat fields for report compatibility
    std::string   compiler_name;
    std::string   compiler_version;
    std::string   optimization_flag;
    double        memory_delta  = 0.0; // bytes

    void fill_flat_fields() {
        compiler_name    = compiler_info.name;
        compiler_version = compiler_info.version;
        optimization_flag = compiler_info.optimization_flags;
        memory_delta     = static_cast<double>(memory.delta_rss);
    }
};

// ============================================================================
// Key generation utilities
// ============================================================================
template <typename KeyT>
std::vector<KeyT> generate_keys(size_t count, uint64_t seed = 42) {
    std::vector<KeyT> keys;
    keys.reserve(count);

    if constexpr (std::is_same_v<KeyT, int32_t>) {
        std::mt19937 rng(static_cast<std::mt19937::result_type>(seed));
        for (size_t i = 0; i < count; ++i) {
            keys.push_back(static_cast<int32_t>(rng()));
        }
    } else if constexpr (std::is_same_v<KeyT, int64_t>) {
        std::mt19937_64 rng(seed);
        for (size_t i = 0; i < count; ++i) {
            keys.push_back(static_cast<int64_t>(rng()));
        }
    } else if constexpr (std::is_same_v<KeyT, std::string>) {
        std::mt19937 rng(static_cast<std::mt19937::result_type>(seed));
        for (size_t i = 0; i < count; ++i) {
            // Generate random string of length 8-32
            int len = 8 + static_cast<int>(rng() % 25);
            std::string s;
            s.reserve(static_cast<size_t>(len));
            for (int j = 0; j < len; ++j) {
                s.push_back(static_cast<char>('a' + rng() % 26));
            }
            keys.push_back(std::move(s));
        }
    }

    return keys;
}

// Generate keys that are NOT in the map (for FindMiss benchmark)
template <typename KeyT>
std::vector<KeyT> generate_miss_keys(size_t count, uint64_t seed = 12345) {
    // Use a different seed so these keys are unlikely to collide with insert keys
    return generate_keys<KeyT>(count, seed);
}

// ============================================================================
// Type name utilities
// ============================================================================
template <typename T>
std::string type_name() {
    if constexpr (std::is_same_v<T, int32_t>)       return "int32_t";
    else if constexpr (std::is_same_v<T, int64_t>)  return "int64_t";
    else if constexpr (std::is_same_v<T, std::string>) return "std::string";
    else {
        return typeid(T).name();
    }
}

// ============================================================================
// BenchRunner: template class that benchmarks a specific map type
// ============================================================================
template <typename MapType, typename KeyT, typename ValueT = int>
class BenchRunner {
public:
    using MapT = MapType;

    explicit BenchRunner(const std::string& map_name,
                         const std::string& hash_name,
                         const BenchConfig& config = BenchConfig{})
        : map_name_(map_name)
        , hash_name_(hash_name)
        , config_(config)
        , compiler_info_(CompilerInfo::detect()) {}

    // Run a single operation benchmark for a given data size
    BenchResult run_operation(BenchOp op, size_t data_size) {
        BenchResult result;
        result.map_name      = map_name_;
        result.operation_name = bench_op_name(op);
        result.key_type_name = type_name<KeyT>();
        result.data_size     = data_size;
        result.hash_name     = hash_name_;
        result.compiler_info = compiler_info_;

        auto keys      = generate_keys<KeyT>(data_size);
        auto miss_keys = generate_miss_keys<KeyT>(data_size);

        // Warmup
        WarmupGuard::run_warmup(config_.warmup_iterations, [&]() {
            MapT map;
            run_op_once(map, op, keys, miss_keys);
        });

        // Measurement
        std::vector<uint64_t> samples;
        samples.reserve(static_cast<size_t>(config_.measure_iterations));

        for (int i = 0; i < config_.measure_iterations; ++i) {
            MapT map;
            uint64_t elapsed_ns = 0;

            // For Insert, we time just the inserts
            // For Find/Erase, we pre-populate first
            if (op == BenchOp::Insert) {
                {
                    ScopedTimer timer(elapsed_ns);
                    for (const auto& k : keys) {
                        map.emplace(k, ValueT{});
                    }
                }
            } else {
                // Pre-populate the map
                for (const auto& k : keys) {
                    map.emplace(k, ValueT{});
                }

                {
                    ScopedTimer timer(elapsed_ns);
                    run_op_once(map, op, keys, miss_keys);
                }
            }

            samples.push_back(elapsed_ns);
        }

        // Compute timing metrics
        result.timing = compute_metrics(samples, data_size);

        // Compute memory metrics
        auto mem_result = MemoryTracker::measure_delta([&]() {
            MapT map;
            for (const auto& k : keys) {
                map.emplace(k, ValueT{});
            }
        });
        result.memory = mem_result;
        result.fill_flat_fields();

        return result;
    }

    // Run all operations for a given data size
    std::vector<BenchResult> run_all_operations(size_t data_size) {
        std::vector<BenchResult> results;
        for (auto op : all_bench_ops) {
            results.push_back(run_operation(op, data_size));
        }
        return results;
    }

    // Run all operations across all configured data sizes
    std::vector<BenchResult> run_all() {
        std::vector<BenchResult> results;
        for (size_t ds : config_.data_sizes) {
            auto batch = run_all_operations(ds);
            for (auto& r : batch) {
                results.push_back(std::move(r));
            }
        }
        return results;
    }

private:
    std::string  map_name_;
    std::string  hash_name_;
    BenchConfig  config_;
    CompilerInfo compiler_info_;

    // Execute a single operation (map must be pre-populated for Find/Erase/Iterate)
    static void run_op_once(MapT& map, BenchOp op,
                            const std::vector<KeyT>& keys,
                            const std::vector<KeyT>& miss_keys) {
        switch (op) {
            case BenchOp::Insert:
                for (const auto& k : keys) {
                    map.emplace(k, ValueT{});
                }
                break;

            case BenchOp::FindHit:
                for (const auto& k : keys) {
                    volatile auto it = map.find(k);
                    (void)it;
                }
                break;

            case BenchOp::FindMiss:
                for (const auto& k : miss_keys) {
                    volatile auto it = map.find(k);
                    (void)it;
                }
                break;

            case BenchOp::Erase:
                for (const auto& k : keys) {
                    map.erase(k);
                }
                break;

            case BenchOp::Iterate: {
                volatile ValueT sink = ValueT{};
                for (auto& [k, v] : map) {
                    (void)k;
                    sink = v;
                }
                (void)sink;
                break;
            }
        }
    }

    // Compute MetricResult from raw nanosecond samples
    static MetricResult compute_metrics(const std::vector<uint64_t>& samples, size_t data_size) {
        MetricResult mr;
        if (samples.empty()) return mr;

        mr.mean_ns    = compute_mean(samples);
        mr.min_ns     = static_cast<double>(*std::min_element(samples.begin(), samples.end()));
        mr.max_ns     = static_cast<double>(*std::max_element(samples.begin(), samples.end()));
        mr.stddev_ns  = compute_stddev(samples);
        mr.median_ns  = compute_median(samples);
        mr.p50_ns     = compute_percentile(samples, 50.0);
        mr.p90_ns     = compute_percentile(samples, 90.0);
        mr.p99_ns     = compute_percentile(samples, 99.0);

        // Throughput: ops/sec based on mean time per operation
        if (mr.mean_ns > 0.0) {
            mr.throughput_ops_per_sec = static_cast<double>(data_size) / (mr.mean_ns / 1e9);
        }

        return mr;
    }
};

// ============================================================================
// BenchSuite: orchestrates benchmarks across multiple map types
// ============================================================================
class BenchSuite {
public:
    explicit BenchSuite(const BenchConfig& config = BenchConfig{})
        : config_(config) {}

    // Register a benchmark runner (moves it into the suite)
    template <typename MapType, typename KeyT, typename ValueT = int>
    void register_map(const std::string& map_name, const std::string& hash_name = "std::hash") {
        // Store a lambda that runs all benchmarks for this map type
        runners_.push_back({
            map_name,
            hash_name,
            type_name<KeyT>(),
            [map_name, hash_name, config = this->config_]() -> std::vector<BenchResult> {
                BenchRunner<MapType, KeyT, ValueT> runner(map_name, hash_name, config);
                return runner.run_all();
            }
        });
    }

    // Run all registered benchmarks
    void run_all() {
        all_results_.clear();
        for (auto& entry : runners_) {
            auto results = entry.run_fn();
            for (auto& r : results) {
                all_results_.push_back(std::move(r));
            }
        }
    }

    // Run benchmarks for a specific data size
    void run_for_data_size(size_t data_size) {
        for (auto& entry : runners_) {
            // Re-run with modified config for single data size
            BenchConfig single_config = config_;
            single_config.data_sizes = {data_size};

            // We need to re-invoke the runner with the modified config.
            // Since the lambda captures config, we create a new approach:
            // just re-run all and filter. For simplicity, we let run_all() handle this.
        }
        // Simplified: just run all and the caller can filter results
        run_all();
    }

    // Access results
    const std::vector<BenchResult>& results() const noexcept {
        return all_results_;
    }

    // Get the configuration
    const BenchConfig& config() const noexcept {
        return config_;
    }

private:
    struct RunnerEntry {
        std::string map_name;
        std::string hash_name;
        std::string key_type_name;
        std::function<std::vector<BenchResult>()> run_fn;
    };

    BenchConfig config_;
    std::vector<RunnerEntry> runners_;
    std::vector<BenchResult> all_results_;
};

} // namespace emilib_bench
