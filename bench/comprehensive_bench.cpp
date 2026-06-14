/**
 * @file comprehensive_bench.cpp
 * @brief Comprehensive hash map benchmark with multiple test scenarios
 *
 * Test scenarios:
 * 1. Insert performance (reserve vs no-reserve)
 * 2. Find performance (hit rate: 100%/50%/0%)
 * 3. Erase performance
 * 4. Iterate performance
 * 5. Copy/Move performance
 * 6. High load factor performance
 * 7. Small map performance
 * 8. Mixed operations (insert+find+erase)
 *
 * Output formats: Markdown table or plain text
 */

#include "util.h"

#include <fstream>
#include <numeric>
#include <algorithm>
#include <iomanip>

// Hash map implementations - only emhash and emilib series
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"

#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"

#include "martin/robin_hood.h"

#if CXX17
#include "martin/unordered_dense.h"
#endif

#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#endif

#if ABSL_HMAP
#include "absl/container/flat_hash_map.h"
#endif

// ============================================================================
// Configuration
// ============================================================================

enum class OutputFormat {
    Markdown,   // Markdown table format
    PlainText,  // Plain text format
    CSV         // CSV format
};

struct BenchConfig {
    size_t num_elements = 1000000;      // Number of elements for main tests
    size_t warmup_iterations = 1000;    // Warmup iterations
    int benchmark_iterations = 3;       // Number of runs for averaging
    OutputFormat output_format = OutputFormat::Markdown;
    std::string output_file = "";       // Output file path
    bool enable_all_tests = true;
    std::vector<std::string> enabled_tests;
};

static BenchConfig g_config;
static std::string g_current_test;

// ============================================================================
// Result structures
// ============================================================================

struct TestResult {
    std::string test_name;
    std::string hash_map_name;
    double time_ms;
    double stddev_ms;
    size_t operations;
    double throughput;      // ops/sec
    float load_factor;
    size_t memory_bytes;
};

struct BenchmarkResults {
    std::string test_name;
    std::vector<TestResult> results;

    void sort_by_time() {
        std::sort(results.begin(), results.end(),
            [](const TestResult& a, const TestResult& b) {
                return a.time_ms < b.time_ms;
            });
    }

    void calculate_relative() {
        if (results.empty()) return;
        double best_time = results[0].time_ms;
        for (auto& r : results) {
            r.throughput = r.operations / (r.time_ms / 1000.0);
        }
    }
};

static std::vector<BenchmarkResults> g_all_results;

// ============================================================================
// Timer utilities
// ============================================================================

static inline double now_ms() {
    auto tp = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(tp).count();
}

class ScopedTimer {
public:
    ScopedTimer() : start_(now_ms()) {}

    double elapsed_ms() const {
        return now_ms() - start_;
    }

    void reset() {
        start_ = now_ms();
    }

private:
    double start_;
};

// ============================================================================
// Statistics
// ============================================================================

struct Stats {
    double mean;
    double stddev;
    double min;
    double max;
};

static Stats calculate_stats(const std::vector<double>& values) {
    if (values.empty()) return {0, 0, 0, 0};

    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();

    double variance = 0;
    for (double v : values) {
        variance += (v - mean) * (v - mean);
    }
    double stddev = std::sqrt(variance / values.size());

    double min_val = *std::min_element(values.begin(), values.end());
    double max_val = *std::max_element(values.begin(), values.end());

    return {mean, stddev, min_val, max_val};
}

// ============================================================================
// Random number generator
// ============================================================================

class FastRNG {
public:
    explicit FastRNG(uint64_t seed = 12345) : state_(seed) {}

    uint64_t next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1DULL;
    }

    int next_int() {
        return static_cast<int>(next() >> 33);
    }

    void reset(uint64_t seed) {
        state_ = seed;
    }

private:
    uint64_t state_;
};

// ============================================================================
// Hash map registry
// ============================================================================

template<typename HMAP>
const char* get_map_name() {
    return typeid(HMAP).name();
}

// ============================================================================
// Test functions
// ============================================================================

// Test 1: Insert with reserve
template<typename HMAP>
static TestResult test_insert_reserve(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();

    std::vector<double> times;
    const size_t n = keys.size();

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        HMAP hmap;
        hmap.reserve(n);

        ScopedTimer timer;
        for (size_t i = 0; i < n; ++i) {
            hmap[keys[i]] = static_cast<int>(i);
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "insert_reserve";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = n;
    result.load_factor = 0.875f;

    return result;
}

// Test 2: Insert without reserve
template<typename HMAP>
static TestResult test_insert_no_reserve(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();

    std::vector<double> times;
    const size_t n = keys.size();

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        HMAP hmap;

        ScopedTimer timer;
        for (size_t i = 0; i < n; ++i) {
            hmap[keys[i]] = static_cast<int>(i);
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "insert_no_reserve";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = n;

    return result;
}

// Test 3: Find with 100% hit rate
template<typename HMAP>
static TestResult test_find_hit_100(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();

    // Pre-populate map
    HMAP hmap;
    hmap.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        hmap[keys[i]] = static_cast<int>(i);
    }

    std::vector<double> times;
    const size_t num_finds = n * 10;  // 10x finds

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        ScopedTimer timer;
        volatile size_t count = 0;
        for (size_t i = 0; i < num_finds; ++i) {
            if (hmap.find(keys[i % n]) != hmap.end()) {
                count++;
            }
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "find_hit_100";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = num_finds;

    return result;
}

// Test 4: Find with 50% hit rate
template<typename HMAP>
static TestResult test_find_hit_50(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();

    // Pre-populate map with half the keys
    HMAP hmap;
    hmap.reserve(n);
    for (size_t i = 0; i < n / 2; ++i) {
        hmap[keys[i]] = static_cast<int>(i);
    }

    std::vector<double> times;
    const size_t num_finds = n * 10;

    // Create mixed keys (50% existing, 50% new)
    std::vector<int> find_keys;
    FastRNG rng(12345);
    for (size_t i = 0; i < num_finds; ++i) {
        if (i % 2 == 0) {
            find_keys.push_back(keys[i % (n / 2)]);  // Hit
        } else {
            find_keys.push_back(keys[n / 2 + i % (n / 2)]);  // Miss
        }
    }

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        ScopedTimer timer;
        volatile size_t count = 0;
        for (size_t i = 0; i < num_finds; ++i) {
            if (hmap.find(find_keys[i]) != hmap.end()) {
                count++;
            }
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "find_hit_50";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = num_finds;

    return result;
}

// Test 5: Find with 0% hit rate (all miss)
template<typename HMAP>
static TestResult test_find_hit_0(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();

    // Pre-populate map
    HMAP hmap;
    hmap.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        hmap[keys[i]] = static_cast<int>(i);
    }

    // Create keys that don't exist
    std::vector<int> miss_keys;
    FastRNG rng(99999);
    for (size_t i = 0; i < n * 10; ++i) {
        miss_keys.push_back(static_cast<int>(rng.next_int() + n * 2));
    }

    std::vector<double> times;
    const size_t num_finds = n * 10;

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        ScopedTimer timer;
        volatile size_t count = 0;
        for (size_t i = 0; i < num_finds; ++i) {
            if (hmap.find(miss_keys[i]) != hmap.end()) {
                count++;
            }
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "find_hit_0";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = num_finds;

    return result;
}

// Test 6: Erase
template<typename HMAP>
static TestResult test_erase(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();

    std::vector<double> times;

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        // Pre-populate map
        HMAP hmap;
        hmap.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            hmap[keys[i]] = static_cast<int>(i);
        }

        ScopedTimer timer;
        for (size_t i = 0; i < n; ++i) {
            hmap.erase(keys[i]);
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "erase";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = n;

    return result;
}

// Test 7: Iterate
template<typename HMAP>
static TestResult test_iterate(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();

    // Pre-populate map
    HMAP hmap;
    hmap.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        hmap[keys[i]] = static_cast<int>(i);
    }

    std::vector<double> times;
    const size_t num_iters = 100;

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        ScopedTimer timer;
        volatile size_t sum = 0;
        for (size_t i = 0; i < num_iters; ++i) {
            for (const auto& kv : hmap) {
                sum += kv.second;
            }
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "iterate";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = n * num_iters;

    return result;
}

// Test 8: Copy
template<typename HMAP>
static TestResult test_copy(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();

    // Pre-populate map
    HMAP hmap;
    hmap.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        hmap[keys[i]] = static_cast<int>(i);
    }

    std::vector<double> times;

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        ScopedTimer timer;
        HMAP copy = hmap;
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "copy";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = n;

    return result;
}

// Test 9: High load factor
template<typename HMAP>
static TestResult test_high_load_factor(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();
    const float target_lf = 0.95f;

    std::vector<double> times;

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        HMAP hmap;
        hmap.max_load_factor(target_lf);
        hmap.reserve(static_cast<size_t>(n / target_lf));

        ScopedTimer timer;
        for (size_t i = 0; i < n; ++i) {
            hmap[keys[i]] = static_cast<int>(i);
        }
        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "high_load_factor";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = n;
    result.load_factor = target_lf;

    return result;
}

// Test 10: Mixed operations (insert + find + erase)
template<typename HMAP>
static TestResult test_mixed_operations(const std::vector<int>& keys) {
    const char* name = get_map_name<HMAP>();
    const size_t n = keys.size();

    std::vector<double> times;

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        HMAP hmap;
        hmap.reserve(n);

        ScopedTimer timer;

        // Insert phase
        for (size_t i = 0; i < n; ++i) {
            hmap[keys[i]] = static_cast<int>(i);
        }

        // Find phase
        volatile size_t count = 0;
        for (size_t i = 0; i < n * 2; ++i) {
            if (hmap.find(keys[i % n]) != hmap.end()) {
                count++;
            }
        }

        // Erase phase
        for (size_t i = 0; i < n / 2; ++i) {
            hmap.erase(keys[i]);
        }

        // Re-insert phase
        for (size_t i = 0; i < n / 2; ++i) {
            hmap[keys[i]] = static_cast<int>(i);
        }

        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "mixed_ops";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = n * 4;  // insert + 2*find + erase + reinsert

    return result;
}

// Test 11: Small map (few elements, many operations)
template<typename HMAP>
static TestResult test_small_map(const std::vector<int>& /* unused */) {
    const char* name = get_map_name<HMAP>();
    const size_t small_size = 100;
    const size_t num_ops = 1000000;

    std::vector<int> small_keys;
    FastRNG rng(12345);
    for (size_t i = 0; i < small_size; ++i) {
        small_keys.push_back(rng.next_int() % 10000);
    }

    std::vector<double> times;

    for (int iter = 0; iter < g_config.benchmark_iterations; ++iter) {
        HMAP hmap;
        hmap.reserve(small_size);

        ScopedTimer timer;
        FastRNG op_rng(99999);

        for (size_t i = 0; i < num_ops; ++i) {
            int key = small_keys[op_rng.next_int() % small_size];
            int op = op_rng.next_int() % 3;

            if (op == 0) {
                hmap[key] = static_cast<int>(i);
            } else if (op == 1) {
                hmap.find(key);
            } else {
                hmap.erase(key);
            }
        }

        times.push_back(timer.elapsed_ms());
    }

    Stats stats = calculate_stats(times);

    TestResult result;
    result.test_name = "small_map";
    result.hash_map_name = name;
    result.time_ms = stats.mean;
    result.stddev_ms = stats.stddev;
    result.operations = num_ops;

    return result;
}

// ============================================================================
// Test runner macros
// ============================================================================

// Run all hash maps for a given test function - directly expanded
#define RUN_ALL_MAPS_FOR_TEST(TEST_FUNC, KEYS) \
    do { \
        BenchmarkResults bench_result; \
        bench_result.test_name = g_current_test; \
        do { TestResult r = TEST_FUNC<emhash5::HashMap<int, int>>(KEYS); r.hash_map_name = "emhash5"; bench_result.results.push_back(r); } while(0); \
        do { TestResult r = TEST_FUNC<emhash6::HashMap<int, int>>(KEYS); r.hash_map_name = "emhash6"; bench_result.results.push_back(r); } while(0); \
        do { TestResult r = TEST_FUNC<emhash7::HashMap<int, int>>(KEYS); r.hash_map_name = "emhash7"; bench_result.results.push_back(r); } while(0); \
        do { TestResult r = TEST_FUNC<emhash8::HashMap<int, int>>(KEYS); r.hash_map_name = "emhash8"; bench_result.results.push_back(r); } while(0); \
        do { TestResult r = TEST_FUNC<emilib::HashMap<int, int>>(KEYS); r.hash_map_name = "emilib1"; bench_result.results.push_back(r); } while(0); \
        do { TestResult r = TEST_FUNC<emilib2::HashMap<int, int>>(KEYS); r.hash_map_name = "emilib2"; bench_result.results.push_back(r); } while(0); \
        do { TestResult r = TEST_FUNC<emilib3::HashMap<int, int>>(KEYS); r.hash_map_name = "emilib3"; bench_result.results.push_back(r); } while(0); \
        do { TestResult r = TEST_FUNC<robin_hood::unordered_map<int, int>>(KEYS); r.hash_map_name = "robin_hood"; bench_result.results.push_back(r); } while(0); \
        RUN_OPTIONAL_CXX17(TEST_FUNC, KEYS, bench_result); \
        RUN_OPTIONAL_BOOST(TEST_FUNC, KEYS, bench_result); \
        RUN_OPTIONAL_ABSL(TEST_FUNC, KEYS, bench_result); \
        bench_result.sort_by_time(); \
        bench_result.calculate_relative(); \
        g_all_results.push_back(bench_result); \
    } while(0)

// Optional hash maps - CXX17
#if CXX17
#define RUN_OPTIONAL_CXX17(TEST_FUNC, KEYS, bench_result) \
    do { TestResult r = TEST_FUNC<ankerl::unordered_dense::map<int, int>>(KEYS); r.hash_map_name = "ankerl"; bench_result.results.push_back(r); } while(0)
#else
#define RUN_OPTIONAL_CXX17(TEST_FUNC, KEYS, bench_result)
#endif

// Optional hash maps - Boost
#if HAVE_BOOST
#define RUN_OPTIONAL_BOOST(TEST_FUNC, KEYS, bench_result) \
    do { TestResult r = TEST_FUNC<boost::unordered_flat_map<int, int>>(KEYS); r.hash_map_name = "boost"; bench_result.results.push_back(r); } while(0)
#else
#define RUN_OPTIONAL_BOOST(TEST_FUNC, KEYS, bench_result)
#endif

// Optional hash maps - Abseil
#if ABSL_HMAP
#define RUN_OPTIONAL_ABSL(TEST_FUNC, KEYS, bench_result) \
    do { TestResult r = TEST_FUNC<absl::flat_hash_map<int, int>>(KEYS); r.hash_map_name = "absl"; bench_result.results.push_back(r); } while(0)
#else
#define RUN_OPTIONAL_ABSL(TEST_FUNC, KEYS, bench_result)
#endif

// ============================================================================
// Output functions
// ============================================================================

static void output_markdown_header() {
    printf("# Hash Map Benchmark Results\n\n");
    printf("**Configuration:**\n");
    printf("- Elements: %zu\n", g_config.num_elements);
    printf("- Iterations: %d\n", g_config.benchmark_iterations);
    printf("- Warmup: %zu\n", g_config.warmup_iterations);
    printf("\n");
}

static void output_markdown_table(const BenchmarkResults& results) {
    printf("\n## %s\n\n", results.test_name.c_str());
    printf("| HashMap | Time (ms) | StdDev | Ops | Throughput (M/s) | Relative |\n");
    printf("|---------|-----------|--------|-----|------------------|----------|\n");

    double best_time = results.results[0].time_ms;

    for (const auto& r : results.results) {
        double relative = r.time_ms / best_time;
        double throughput_m = r.throughput / 1000000.0;

        printf("| %s | %.2f | %.2f | %zu | %.2f | %.2fx |\n",
               r.hash_map_name.c_str(),
               r.time_ms,
               r.stddev_ms,
               r.operations,
               throughput_m,
               relative);
    }
}

static void output_plain_text(const BenchmarkResults& results) {
    printf("\n=== %s ===\n", results.test_name.c_str());

    double best_time = results.results[0].time_ms;

    for (const auto& r : results.results) {
        double relative = r.time_ms / best_time;
        printf("  %-15s: %.2f ms (stddev=%.2f) ops=%zu throughput=%.2f M/s relative=%.2fx\n",
               r.hash_map_name.c_str(),
               r.time_ms,
               r.stddev_ms,
               r.operations,
               r.throughput / 1000000.0,
               relative);
    }
}

static void output_csv_header() {
    printf("Test,HashMap,Time,StdDev,Ops,Throughput,Relative\n");
}

static void output_csv(const BenchmarkResults& results) {
    double best_time = results.results.empty() ? 1.0 : results.results[0].time_ms;

    for (const auto& r : results.results) {
        double relative = r.time_ms / best_time;
        printf("%s,%s,%.2f,%.2f,%zu,%.2f,%.2f\n",
               r.test_name.c_str(),
               r.hash_map_name.c_str(),
               r.time_ms,
               r.stddev_ms,
               r.operations,
               r.throughput,
               relative);
    }
}

static void output_summary() {
    printf("\n## Summary\n\n");

    // Calculate overall scores
    std::map<std::string, double> total_scores;
    std::map<std::string, int> wins;

    for (const auto& results : g_all_results) {
        if (results.results.empty()) continue;

        const std::string& winner = results.results[0].hash_map_name;
        wins[winner]++;

        for (size_t i = 0; i < results.results.size(); ++i) {
            const auto& r = results.results[i];
            // Score: inverse of relative performance
            double score = 1.0 / (r.time_ms / results.results[0].time_ms);
            total_scores[r.hash_map_name] += score;
        }
    }

    printf("| HashMap | Wins | Total Score |\n");
    printf("|---------|------|-------------|\n");

    // Sort by wins
    std::vector<std::pair<std::string, int>> sorted_wins;
    for (const auto& p : wins) {
        sorted_wins.push_back(p);
    }
    std::sort(sorted_wins.begin(), sorted_wins.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& p : sorted_wins) {
        printf("| %s | %d | %.2f |\n",
               p.first.c_str(),
               p.second,
               total_scores[p.first]);
    }
}

static void write_to_file(const std::string& filename) {
    std::ofstream fout(filename);
    if (!fout.is_open()) {
        printf("Error: Cannot open file %s\n", filename.c_str());
        return;
    }

    fout << "# Hash Map Benchmark Results\n\n";
    fout << "**Configuration:**\n";
    fout << "- Elements: " << g_config.num_elements << "\n";
    fout << "- Iterations: " << g_config.benchmark_iterations << "\n";
    fout << "\n";

    for (const auto& results : g_all_results) {
        fout << "\n## " << results.test_name << "\n\n";
        fout << "| HashMap | Time (ms) | StdDev | Ops | Throughput (M/s) | Relative |\n";
        fout << "|---------|-----------|--------|-----|------------------|----------|\n";

        double best_time = results.results.empty() ? 1.0 : results.results[0].time_ms;

        for (const auto& r : results.results) {
            double relative = r.time_ms / best_time;
            fout << "| " << r.hash_map_name << " | " << r.time_ms
                 << " | " << r.stddev_ms << " | " << r.operations
                 << " | " << (r.throughput / 1000000.0) << " | " << relative << "x |\n";
        }
    }

    fout.close();
    printf("Results written to: %s\n", filename.c_str());
}

// ============================================================================
// Main benchmark runner
// ============================================================================

static void run_all_benchmarks() {
    // Generate test data
    std::vector<int> keys;
    keys.reserve(g_config.num_elements);
    FastRNG rng(12345);
    for (size_t i = 0; i < g_config.num_elements; ++i) {
        keys.push_back(rng.next_int());
    }

    printf("Running benchmarks with %zu elements...\n", g_config.num_elements);

    // Run all tests
    g_current_test = "Insert (reserve)";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_insert_reserve, keys);

    g_current_test = "Insert (no reserve)";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_insert_no_reserve, keys);

    g_current_test = "Find (100% hit)";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_find_hit_100, keys);

    g_current_test = "Find (50% hit)";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_find_hit_50, keys);

    g_current_test = "Find (0% hit)";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_find_hit_0, keys);

    g_current_test = "Erase";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_erase, keys);

    g_current_test = "Iterate";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_iterate, keys);

    g_current_test = "Copy";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_copy, keys);

    g_current_test = "High Load Factor";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_high_load_factor, keys);

    g_current_test = "Mixed Operations";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_mixed_operations, keys);

    g_current_test = "Small Map";
    printf("  Running: %s...\n", g_current_test.c_str());
    RUN_ALL_MAPS_FOR_TEST(test_small_map, std::vector<int>());

    printf("\nAll benchmarks completed!\n");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.find("--n=") == 0) {
            g_config.num_elements = std::stoul(arg.substr(4));
        } else if (arg.find("--iter=") == 0) {
            g_config.benchmark_iterations = std::stoi(arg.substr(7));
        } else if (arg.find("--format=") == 0) {
            std::string fmt = arg.substr(9);
            if (fmt == "md" || fmt == "markdown") {
                g_config.output_format = OutputFormat::Markdown;
            } else if (fmt == "text" || fmt == "plain") {
                g_config.output_format = OutputFormat::PlainText;
            } else if (fmt == "csv") {
                g_config.output_format = OutputFormat::CSV;
            }
        } else if (arg.find("--output=") == 0) {
            g_config.output_file = arg.substr(9);
        } else if (arg == "--help" || arg == "-h") {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --n=N          Number of elements (default: 1000000)\n");
            printf("  --iter=N       Benchmark iterations (default: 3)\n");
            printf("  --format=F     Output format: md, text, csv (default: md)\n");
            printf("  --output=FILE  Write results to file\n");
            printf("  --help         Show this help\n");
            return 0;
        }
    }

    printf("Hash Map Comprehensive Benchmark\n");
    printf("================================\n\n");

    // Run benchmarks
    run_all_benchmarks();

    // Output results
    switch (g_config.output_format) {
        case OutputFormat::Markdown:
            output_markdown_header();
            for (const auto& results : g_all_results) {
                output_markdown_table(results);
            }
            output_summary();
            break;

        case OutputFormat::PlainText:
            for (const auto& results : g_all_results) {
                output_plain_text(results);
            }
            break;

        case OutputFormat::CSV:
            output_csv_header();
            for (const auto& results : g_all_results) {
                output_csv(results);
            }
            break;
    }

    // Write to file if specified
    if (!g_config.output_file.empty()) {
        write_to_file(g_config.output_file);
    }

    return 0;
}
