// emilib benchmark framework — main driver
// https://github.com/ktprime/emhash
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT

#include "bench_config.hpp"
#include "bench_runner.hpp"
#include "bench_registry.hpp"
#include "bench_hash.hpp"
#include "bench_report.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <numeric>

// emilib headers
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap2_v2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihmap4.hpp"

// boost
#define EMILIB_BENCH_ENABLE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>

using namespace emilib_bench;

// ============================================================================
// Command-line parsing helpers
// ============================================================================

static std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(s);
    while (std::getline(iss, token, ',')) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

static std::vector<size_t> parse_sizes(const std::string& s) {
    auto tokens = split_csv(s);
    std::vector<size_t> result;
    for (const auto& t : tokens) {
        result.push_back(static_cast<size_t>(std::stoull(t)));
    }
    return result;
}

// ============================================================================
// Map registration helper — maps CLI name to template instantiation
// ============================================================================

template <typename KeyT>
static void register_map_for_key(BenchSuite& suite, const std::string& map_name,
                                  const std::string& hash_name, const BenchConfig& config) {
    if (map_name == "emilib1") {
        suite.register_map<emilib::HashMap<KeyT, int>, KeyT>(map_name, hash_name);
    } else if (map_name == "emilib2") {
        suite.register_map<emilib2::HashMap<KeyT, int>, KeyT>(map_name, hash_name);
    } else if (map_name == "emilib2_v2") {
        suite.register_map<emilib2_v2::HashMap<KeyT, int>, KeyT>(map_name, hash_name);
    } else if (map_name == "emilib3") {
        suite.register_map<emilib3::HashMap<KeyT, int>, KeyT>(map_name, hash_name);
    } else if (map_name == "emilib4") {
        suite.register_map<emilib4::HashMap<KeyT, int>, KeyT>(map_name, hash_name);
    } else if (map_name == "boost") {
        suite.register_map<boost::unordered_flat_map<KeyT, int>, KeyT>(map_name, hash_name);
    }
}

static void register_all_maps(BenchSuite& suite, const BenchConfig& config) {
    for (const auto& map_name : config.map_types) {
        for (const auto& kt : config.key_types) {
            if (kt == "int32") {
                register_map_for_key<int32_t>(suite, map_name, "std::hash", config);
            } else if (kt == "int64") {
                register_map_for_key<int64_t>(suite, map_name, "std::hash", config);
            } else if (kt == "string") {
                register_map_for_key<std::string>(suite, map_name, "std::hash", config);
            }
        }
    }
}

// ============================================================================
// Hash comparison runner
// ============================================================================

static void run_hash_comparison(const BenchConfig& config) {
    std::cout << "\n--- Hash Function Comparison ---\n";
    for (auto n : config.data_sizes) {
        for (const auto& kt : config.key_types) {
            if (kt == "int32") {
                auto keys = generate_keys<int32_t>(n);
                auto results = run_hash_comparison<int32_t>(keys);
                for (const auto& r : results) {
                    std::cout << "  " << r.hash_name << " key=int32 N=" << n
                              << ": " << r.compute_time_ns << " ns total, "
                              << std::fixed << std::setprecision(4)
                              << "collision=" << r.collision_rate
                              << " avalanche=" << r.avalanche_score << "\n";
                }
            } else if (kt == "int64") {
                auto keys = generate_keys<int64_t>(n);
                auto results = run_hash_comparison<int64_t>(keys);
                for (const auto& r : results) {
                    std::cout << "  " << r.hash_name << " key=int64 N=" << n
                              << ": " << r.compute_time_ns << " ns total, "
                              << std::fixed << std::setprecision(4)
                              << "collision=" << r.collision_rate
                              << " avalanche=" << r.avalanche_score << "\n";
                }
            } else if (kt == "string") {
                auto keys = generate_keys<std::string>(n);
                auto results = run_hash_comparison<std::string>(keys);
                for (const auto& r : results) {
                    std::cout << "  " << r.hash_name << " key=string N=" << n
                              << ": " << r.compute_time_ns << " ns total, "
                              << std::fixed << std::setprecision(4)
                              << "collision=" << r.collision_rate
                              << " avalanche=" << r.avalanche_score << "\n";
                }
            }
        }
    }
}

// ============================================================================
// Print usage
// ============================================================================

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n\n"
        << "Options:\n"
        << "  --sizes 100,1000,10000    Comma-separated data sizes (default: all)\n"
        << "  --iterations N            Measure iterations (default: 10)\n"
        << "  --warmup N                Warmup iterations (default: 3)\n"
        << "  --key-types int32,int64,string  Key types to test\n"
        << "  --maps emilib2,emilib3,boost    Map types to test\n"
        << "  --output report.html      Output HTML report path\n"
        << "  --hash-compare            Enable hash function comparison\n"
        << "  --ops insert,find_hit,find_miss,erase,iterate  Operations to benchmark\n"
        << "  --help                    Show this usage\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    // Default configuration
    BenchConfig config;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--sizes" && i + 1 < argc) {
            config.data_sizes = parse_sizes(argv[++i]);
        } else if (arg == "--iterations" && i + 1 < argc) {
            config.measure_iterations = std::stoi(argv[++i]);
        } else if (arg == "--warmup" && i + 1 < argc) {
            config.warmup_iterations = std::stoi(argv[++i]);
        } else if (arg == "--key-types" && i + 1 < argc) {
            config.key_types = split_csv(argv[++i]);
        } else if (arg == "--maps" && i + 1 < argc) {
            config.map_types = split_csv(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.output_path = argv[++i];
        } else if (arg == "--hash-compare") {
            config.hash_compare = true;
        } else if (arg == "--ops" && i + 1 < argc) {
            config.ops = split_csv(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Print config summary
    std::cout << "=== emilib Benchmark ===\n";
    std::cout << "Sizes: ";
    for (size_t i = 0; i < config.data_sizes.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << config.data_sizes[i];
    }
    std::cout << "\nIterations: " << config.measure_iterations << "\n";
    std::cout << "Warmup: " << config.warmup_iterations << "\n";
    std::cout << "Key types: ";
    for (size_t i = 0; i < config.key_types.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << config.key_types[i];
    }
    std::cout << "\nMaps: ";
    for (size_t i = 0; i < config.map_types.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << config.map_types[i];
    }
    std::cout << "\nOps: ";
    for (size_t i = 0; i < config.ops.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << config.ops[i];
    }
    std::cout << "\nOutput: " << config.output_path << "\n";
    std::cout << "Hash compare: " << (config.hash_compare ? "yes" : "no") << "\n\n";

    // Create BenchSuite, register requested map types
    BenchSuite suite(config);
    register_all_maps(suite, config);

    // Run benchmarks with progress output
    std::cout << "Running benchmarks...\n\n";
    int total_combos = static_cast<int>(config.map_types.size() * config.key_types.size() * config.data_sizes.size());
    int combo_idx = 0;

    // We iterate manually to show progress
    // The BenchSuite::run_all() does the heavy lifting, but we want progress output.
    // For progress, we iterate by data_size and map type manually.
    std::vector<BenchResult> all_results;

    for (auto n : config.data_sizes) {
        for (const auto& map_name : config.map_types) {
            for (const auto& kt : config.key_types) {
                ++combo_idx;
                std::cout << "[" << combo_idx << "/" << total_combos << "] "
                          << map_name << " N=" << n << " key=" << kt << " ... ";
                std::cout.flush();

                // Run this specific combination by creating a sub-config
                BenchConfig sub_config = config;
                sub_config.data_sizes = {n};

                BenchSuite sub_suite(sub_config);

                // Register only this map+key combo
                if (kt == "int32") {
                    register_map_for_key<int32_t>(sub_suite, map_name, "std::hash", sub_config);
                } else if (kt == "int64") {
                    register_map_for_key<int64_t>(sub_suite, map_name, "std::hash", sub_config);
                } else if (kt == "string") {
                    register_map_for_key<std::string>(sub_suite, map_name, "std::hash", sub_config);
                }

                sub_suite.run_all();

                const auto& sub_results = sub_suite.results();
                if (!sub_results.empty()) {
                    // Print summary of the first result as representative
                    const auto& r = sub_results[0];
                    std::cout << std::fixed << std::setprecision(1)
                              << r.timing.mean_ns << " ns/op ("
                              << std::setprecision(2)
                              << r.timing.throughput_ops_per_sec / 1e6 << " Mops/s)\n";
                } else {
                    std::cout << "SKIPPED\n";
                }

                for (const auto& r : sub_results) {
                    all_results.push_back(r);
                }
            }
        }
    }

    // Hash comparison
    if (config.hash_compare) {
        run_hash_comparison(config);
    }

    // Generate HTML report via ReportGenerator
    if (!ReportGenerator::generate_html(all_results, config.output_path)) {
        std::cerr << "Error: failed to write HTML report to " << config.output_path << "\n";
    }
    std::cout << "\nHTML report written to: " << config.output_path << "\n";

    // Print summary table to stdout
    std::cout << "\n=== Summary ===\n";
    std::cout << std::left << std::setw(22) << "Map"
              << std::setw(10) << "Key"
              << std::setw(10) << "Op"
              << std::setw(10) << "N"
              << std::right
              << std::setw(12) << "mean(ns)"
              << std::setw(12) << "Mops/s" << "\n";
    std::cout << std::string(76, '-') << "\n";
    for (const auto& r : all_results) {
        std::cout << std::left << std::setw(22) << r.map_name
                  << std::setw(10) << r.key_type_name
                  << std::setw(10) << r.operation_name
                  << std::setw(10) << r.data_size
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(12) << r.timing.mean_ns
                  << std::setw(12) << std::setprecision(2)
                  << r.timing.throughput_ops_per_sec / 1e6 << "\n";
    }

    return 0;
}
