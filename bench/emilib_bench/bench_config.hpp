#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <type_traits>

namespace emilib_bench {

// ============================================================================
// Data size category classification
// ============================================================================
enum class DataSizeCategory : uint8_t {
    SMALL,   // 10 - 100
    MEDIUM,  // 1,000 - 10,000
    LARGE,   // 100,000 - 1,000,000
    XLARGE   // 1,000,000+
};

inline DataSizeCategory classify_data_size(size_t n) noexcept {
    if (n <= 100)       return DataSizeCategory::SMALL;
    if (n <= 10000)     return DataSizeCategory::MEDIUM;
    if (n <= 1000000)   return DataSizeCategory::LARGE;
    return DataSizeCategory::XLARGE;
}

inline const char* data_size_category_name(DataSizeCategory cat) noexcept {
    switch (cat) {
        case DataSizeCategory::SMALL:  return "SMALL";
        case DataSizeCategory::MEDIUM: return "MEDIUM";
        case DataSizeCategory::LARGE:  return "LARGE";
        case DataSizeCategory::XLARGE: return "XLARGE";
    }
    return "UNKNOWN";
}

// ============================================================================
// Metric result: timing statistics for a benchmark run
// ============================================================================
struct MetricResult {
    double mean_ns               = 0.0;
    double min_ns                = 0.0;
    double max_ns                = 0.0;
    double stddev_ns             = 0.0;
    double median_ns             = 0.0;
    double throughput_ops_per_sec = 0.0;
    size_t memory_bytes          = 0;

    // Latency percentiles
    double p50_ns = 0.0;
    double p90_ns = 0.0;
    double p99_ns = 0.0;
};

// ============================================================================
// Compiler info: captured via preprocessor macros at compile time
// ============================================================================
struct CompilerInfo {
    std::string name;
    std::string version;
    std::string optimization_flags;

    static CompilerInfo detect() {
        CompilerInfo info;

#if defined(__clang__)
        info.name = "clang";
        info.version = std::to_string(__clang_major__) + "." +
                       std::to_string(__clang_minor__) + "." +
                       std::to_string(__clang_patchlevel__);
#elif defined(_MSC_VER)
        info.name = "msvc";
        info.version = std::to_string(_MSC_VER);
#elif defined(__GNUC__)
        info.name = "gcc";
        info.version = std::to_string(__GNUC__) + "." +
                       std::to_string(__GNUC_MINOR__) + "." +
                       std::to_string(__GNUC_PATCHLEVEL__);
#else
        info.name = "unknown";
        info.version = "0";
#endif

#if defined(__OPTIMIZE__)
        info.optimization_flags = "-O2";
#elif defined(__OPTIMIZE_SIZE__)
        info.optimization_flags = "-Os";
#elif defined(_DEBUG)
        info.optimization_flags = "-O0 (debug)";
#else
        info.optimization_flags = "-O0";
#endif

        return info;
    }

    std::string to_string() const {
        return name + " " + version + " " + optimization_flags;
    }
};

// ============================================================================
// Hash function configuration
// ============================================================================
struct HashFuncConfig {
    std::string name;
    // We store a type-erased tag to identify which hash to use at runtime.
    // 0 = std::hash, 1 = emh_wyhash (strings), 2 = FNV-1a, 3 = mul-xor (integers)
    int hash_tag = 0;
};

// Predefined hash configurations
inline const std::vector<HashFuncConfig>& default_hash_configs() {
    static const std::vector<HashFuncConfig> configs = {
        {"std::hash",    0},
        {"emh_wyhash",   1},
        {"FNV-1a",       2},
        {"mul-xor",      3}
    };
    return configs;
}

// ============================================================================
// Benchmark configuration
// ============================================================================
struct BenchConfig {
    std::vector<size_t> data_sizes = {
        10, 50, 100, 1000, 5000, 10000, 50000, 100000, 500000, 1000000, 5000000
    };

    int warmup_iterations  = 3;
    int measure_iterations = 10;

    // Key types are handled at compile time via templates; this vector stores
    // their names for reporting purposes.
    std::vector<std::string> key_type_names = {"int32_t", "int64_t", "std::string"};

    // CLI-configurable options
    std::vector<std::string> key_types = {"int32", "int64", "string"};
    std::vector<std::string> map_types = {"emilib1", "emilib2", "emilib2_v2", "emilib3", "emilib4", "boost"};
    std::vector<std::string> ops = {"insert", "find_hit", "find_miss", "erase", "iterate"};
    std::string output_path = "emilib_bench_report.html";
    bool hash_compare = false;
};

} // namespace emilib_bench
