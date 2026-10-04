// emilib benchmark — HTML report generation
// https://github.com/ktprime/emhash
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT

#pragma once

#include "bench_runner.hpp"
#include "bench_hash.hpp"
#include <string>
#include <vector>

namespace emilib_bench {

// ============================================================================
// ReportGenerator: produces HTML benchmark reports
// ============================================================================
class ReportGenerator {
public:
    struct OpGroup {
        std::string operation;
        size_t      data_size = 0;
        std::vector<size_t> indices;
    };

    // Generate the <head> section with CSS and opening tags
    static std::string generate_head(const std::string& timestamp);

    // Generate the summary dashboard cards
    static std::string generate_summary_dashboard(const std::vector<BenchResult>& results);

    // Generate comparison tables grouped by operation
    static std::string generate_comparison_tables(const std::vector<BenchResult>& results);

    // Generate chart data as JavaScript for Chart.js
    static std::string generate_chart_data_js(const std::vector<BenchResult>& results);

    // Generate charts section HTML (canvas elements for Chart.js)
    static std::string generate_charts_section(const std::vector<BenchResult>& results);

    // Generate statistical analysis section
    static std::string generate_statistical_analysis(const std::vector<BenchResult>& results);

    // Generate configuration summary section
    static std::string generate_config_summary(const std::vector<BenchResult>& results);

    // Generate recommendations section
    static std::string generate_recommendations(const std::vector<BenchResult>& results);

    // JSON serialization helpers
    static std::string metric_to_json(const MetricResult& m);
    static std::string result_to_json(const BenchResult& r);
    static std::string results_array_json(const std::vector<BenchResult>& results);

    // CSS generation (private helper)
    static std::string generate_css();

    // Grouping helpers
    static std::vector<OpGroup> group_by_op_and_size(const std::vector<BenchResult>& results);

    // Unique value extraction
    static std::vector<std::string> unique_operations(const std::vector<BenchResult>& results);
    static std::vector<std::string> unique_maps(const std::vector<BenchResult>& results);
    static std::vector<size_t> unique_data_sizes(const std::vector<BenchResult>& results);

    // Statistical helpers
    static double compute_speedup(double best_ns, double current_ns);

    // Convenience: generate complete HTML report to file
    static bool generate_html(const std::vector<BenchResult>& results, const std::string& output_path);
};

} // namespace emilib_bench
