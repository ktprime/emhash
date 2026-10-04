// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2026 Huang Yuanbing bailuzhou@163.com
//
// Implementation of the HTML report generator for emilib benchmarks.

#include "bench_report.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <cstdlib>

namespace emilib_bench {

// ── Utility: format a double with fixed precision ──────────────────
static std::string fmt(double v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

static std::string fmt_int(int v) {
    return std::to_string(v);
}

static std::string json_str(const std::string& s) {
    std::string out;
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    out += '"';
    return out;
}

static std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
#ifdef _WIN32
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    char mb[64];
    std::strftime(mb, sizeof(mb), "%Y-%m-%d %H:%M:%S", &buf);
    return mb;
}

static std::string format_ns(double ns) {
    if (ns < 1e3)   return fmt(ns, 1) + " ns";
    if (ns < 1e6)   return fmt(ns / 1e3, 2) + " \xC2\xB5s"; // µs
    if (ns < 1e9)   return fmt(ns / 1e6, 2) + " ms";
    return fmt(ns / 1e9, 3) + " s";
}

static std::string format_bytes(double bytes) {
    if (bytes < 1024.0)       return fmt(bytes, 0) + " B";
    if (bytes < 1048576.0)    return fmt(bytes / 1024.0, 2) + " KB";
    if (bytes < 1073741824.0) return fmt(bytes / 1048576.0, 2) + " MB";
    return fmt(bytes / 1073741824.0, 2) + " GB";
}

static std::string format_throughput(double ops) {
    if (ops >= 1e9) return fmt(ops / 1e9, 2) + "G";
    if (ops >= 1e6) return fmt(ops / 1e6, 2) + "M";
    if (ops >= 1e3) return fmt(ops / 1e3, 2) + "K";
    return fmt(ops, 0);
}

// ── JSON data helpers ──────────────────────────────────────────────

std::string ReportGenerator::metric_to_json(const MetricResult& m) {
    std::ostringstream o;
    o << "{\"mean_ns\":" << m.mean_ns
      << ",\"min_ns\":" << m.min_ns
      << ",\"max_ns\":" << m.max_ns
      << ",\"stddev_ns\":" << m.stddev_ns
      << ",\"median_ns\":" << m.median_ns
      << ",\"throughput_ops_per_sec\":" << m.throughput_ops_per_sec
      << ",\"memory_bytes\":" << m.memory_bytes
      << ",\"p50_ns\":" << m.p50_ns
      << ",\"p90_ns\":" << m.p90_ns
      << ",\"p99_ns\":" << m.p99_ns
      << "}";
    return o.str();
}

std::string ReportGenerator::result_to_json(const BenchResult& r) {
    std::ostringstream o;
    o << "{\"map_name\":" << json_str(r.map_name)
      << ",\"operation_name\":" << json_str(r.operation_name)
      << ",\"key_type_name\":" << json_str(r.key_type_name)
      << ",\"hash_name\":" << json_str(r.hash_name)
      << ",\"data_size\":" << r.data_size
      << ",\"compiler_name\":" << json_str(r.compiler_name)
      << ",\"compiler_version\":" << json_str(r.compiler_version)
      << ",\"optimization_flag\":" << json_str(r.optimization_flag)
      << ",\"timing\":" << metric_to_json(r.timing)
      << ",\"memory_delta\":" << r.memory_delta
      << "}";
    return o.str();
}

std::string ReportGenerator::results_array_json(const std::vector<BenchResult>& results) {
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < results.size(); i++) {
        if (i) o << ",";
        o << result_to_json(results[i]);
    }
    o << "]";
    return o.str();
}

// ── Grouping helpers ───────────────────────────────────────────────

std::vector<ReportGenerator::OpGroup>
ReportGenerator::group_by_op_and_size(const std::vector<BenchResult>& results) {
    std::vector<OpGroup> groups;
    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        auto it = std::find_if(groups.begin(), groups.end(), [&](const OpGroup& g) {
            return g.operation == r.operation_name && g.data_size == r.data_size;
        });
        if (it != groups.end()) {
            it->indices.push_back(i);
        } else {
            OpGroup g;
            g.operation = r.operation_name;
            g.data_size = r.data_size;
            g.indices.push_back(i);
            groups.push_back(std::move(g));
        }
    }
    return groups;
}

// ── Unique-value helpers ───────────────────────────────────────────

std::vector<std::string>
ReportGenerator::unique_operations(const std::vector<BenchResult>& results) {
    std::vector<std::string> ops;
    for (const auto& r : results) {
        if (std::find(ops.begin(), ops.end(), r.operation_name) == ops.end())
            ops.push_back(r.operation_name);
    }
    return ops;
}

std::vector<std::string>
ReportGenerator::unique_maps(const std::vector<BenchResult>& results) {
    std::vector<std::string> maps;
    for (const auto& r : results) {
        if (std::find(maps.begin(), maps.end(), r.map_name) == maps.end())
            maps.push_back(r.map_name);
    }
    return maps;
}

std::vector<size_t>
ReportGenerator::unique_data_sizes(const std::vector<BenchResult>& results) {
    std::vector<size_t> sizes;
    for (const auto& r : results) {
        if (std::find(sizes.begin(), sizes.end(), r.data_size) == sizes.end())
            sizes.push_back(r.data_size);
    }
    std::sort(sizes.begin(), sizes.end());
    return sizes;
}

// ── Statistical helpers ────────────────────────────────────────────

double ReportGenerator::compute_speedup(double best_ns, double current_ns) {
    if (best_ns <= 0.0 || current_ns <= 0.0) return 0.0;
    return current_ns / best_ns;
}

// ── CSS ────────────────────────────────────────────────────────────

std::string ReportGenerator::generate_css() {
    return R"css(
:root {
    --bg-primary: #0d1117;
    --bg-secondary: #161b22;
    --bg-tertiary: #21262d;
    --border: #30363d;
    --text-primary: #e6edf3;
    --text-secondary: #8b949e;
    --text-muted: #6e7681;
    --accent-blue: #58a6ff;
    --accent-green: #3fb950;
    --accent-red: #f85149;
    --accent-orange: #d29922;
    --accent-purple: #bc8cff;
    --accent-cyan: #39d2c0;
    --card-shadow: 0 1px 3px rgba(0,0,0,0.3), 0 1px 2px rgba(0,0,0,0.2);
}

*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;
    background: var(--bg-primary);
    color: var(--text-primary);
    line-height: 1.6;
    padding: 0;
}

.container {
    max-width: 1400px;
    margin: 0 auto;
    padding: 20px;
}

header {
    background: var(--bg-secondary);
    border-bottom: 1px solid var(--border);
    padding: 20px 0;
    margin-bottom: 24px;
}

header .container {
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 12px;
}

header h1 {
    font-size: 1.5em;
    font-weight: 600;
    color: var(--text-primary);
}

header .meta {
    font-size: 0.85em;
    color: var(--text-secondary);
    text-align: right;
}

h2 {
    font-size: 1.3em;
    font-weight: 600;
    margin: 32px 0 16px;
    padding-bottom: 8px;
    border-bottom: 1px solid var(--border);
    color: var(--text-primary);
}

h3 {
    font-size: 1.1em;
    font-weight: 600;
    margin: 20px 0 10px;
    color: var(--accent-cyan);
}

/* Dashboard cards */
.dashboard {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
    gap: 16px;
    margin-bottom: 24px;
}

.card {
    background: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 20px;
    box-shadow: var(--card-shadow);
}

.card .label {
    font-size: 0.8em;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-secondary);
    margin-bottom: 4px;
}

.card .value {
    font-size: 1.8em;
    font-weight: 700;
    color: var(--accent-blue);
}

.card .detail {
    font-size: 0.85em;
    color: var(--text-muted);
    margin-top: 4px;
}

.card.winner .value { color: var(--accent-green); }
.card.warning .value { color: var(--accent-orange); }
.card.danger .value  { color: var(--accent-red); }

/* Tables */
.table-wrap {
    overflow-x: auto;
    margin-bottom: 24px;
    background: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 8px;
}

table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.85em;
}

thead th {
    background: var(--bg-tertiary);
    padding: 10px 12px;
    text-align: left;
    font-weight: 600;
    color: var(--text-secondary);
    cursor: pointer;
    user-select: none;
    white-space: nowrap;
    border-bottom: 2px solid var(--border);
    position: sticky;
    top: 0;
    z-index: 1;
}

thead th:hover { color: var(--accent-blue); }
thead th .sort-arrow { margin-left: 4px; opacity: 0.4; }
thead th.sorted-asc .sort-arrow,
thead th.sorted-desc .sort-arrow { opacity: 1; color: var(--accent-blue); }

tbody td {
    padding: 8px 12px;
    border-bottom: 1px solid var(--border);
    white-space: nowrap;
}

tbody tr:hover { background: rgba(88,166,255,0.05); }

td.best  { color: var(--accent-green); font-weight: 700; }
td.worst { color: var(--accent-red);   font-weight: 700; }

tr.section-header td {
    background: var(--bg-tertiary);
    font-weight: 600;
    color: var(--accent-cyan);
    padding: 12px;
    font-size: 0.95em;
}

/* Charts */
.chart-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(600px, 1fr));
    gap: 20px;
    margin-bottom: 24px;
}

.chart-box {
    background: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 16px;
    box-shadow: var(--card-shadow);
}

.chart-box h3 {
    margin-top: 0;
    margin-bottom: 12px;
    font-size: 1em;
    color: var(--text-primary);
}

.chart-box canvas {
    width: 100% !important;
    max-height: 400px;
}

/* Analysis & Recommendations */
.analysis-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
    gap: 16px;
    margin-bottom: 24px;
}

.analysis-card {
    background: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 16px;
}

.analysis-card .title {
    font-weight: 600;
    color: var(--accent-cyan);
    margin-bottom: 8px;
}

.regression { color: var(--accent-red); border-left: 3px solid var(--accent-red); padding-left: 12px; margin: 6px 0; }
.improvement { color: var(--accent-green); border-left: 3px solid var(--accent-green); padding-left: 12px; margin: 6px 0; }
.info-item { color: var(--text-secondary); margin: 4px 0; }

.recommendation {
    background: var(--bg-tertiary);
    border-radius: 6px;
    padding: 12px 16px;
    margin: 8px 0;
    border-left: 3px solid var(--accent-blue);
}

.recommendation .rec-title {
    font-weight: 600;
    color: var(--accent-blue);
    margin-bottom: 4px;
}

.recommendation .rec-body {
    color: var(--text-secondary);
    font-size: 0.9em;
}

/* Config summary */
.config-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
    gap: 12px;
    margin-bottom: 24px;
}

.config-item {
    background: var(--bg-secondary);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 12px;
}

.config-item .cfg-label {
    font-size: 0.75em;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
}

.config-item .cfg-value {
    font-weight: 600;
    color: var(--text-primary);
    margin-top: 2px;
}

/* Print styles */
@media print {
    body { background: #fff; color: #000; }
    .chart-grid, .chart-box { break-inside: avoid; }
    .card, .analysis-card, .config-item { box-shadow: none; border: 1px solid #ccc; }
    :root { --bg-primary: #fff; --bg-secondary: #f8f9fa; --bg-tertiary: #eee; --border: #ccc; --text-primary: #000; --text-secondary: #555; }
}

@media (max-width: 768px) {
    .container { padding: 12px; }
    .chart-grid { grid-template-columns: 1fr; }
    .dashboard { grid-template-columns: repeat(2, 1fr); }
}
)css";
}

// ── HTML <head> section ────────────────────────────────────────────

std::string ReportGenerator::generate_head(const std::string& timestamp) {
    std::ostringstream o;
    o << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>emilib Benchmark Report &mdash; )" << timestamp << R"(</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
)" << generate_css() << R"(
</style>
</head>
<body>
<header>
<div class="container">
<h1>emilib Benchmark Report</h1>
<div class="meta">
<div>emhash v1.1.0</div>
<div>)" << timestamp << R"(</div>
</div>
</div>
</header>
<div class="container">
)";
    return o.str();
}

// ── Summary Dashboard ──────────────────────────────────────────────

std::string ReportGenerator::generate_summary_dashboard(const std::vector<BenchResult>& results) {
    if (results.empty()) {
        return "<div class=\"dashboard\"><div class=\"card\"><div class=\"label\">No Data</div>"
               "<div class=\"value\">0</div></div></div>";
    }

    std::ostringstream o;
    o << "<h2>Summary Dashboard</h2>\n<div class=\"dashboard\">\n";

    // Total benchmarks
    o << "<div class=\"card\"><div class=\"label\">Total Benchmarks</div>"
      << "<div class=\"value\">" << results.size() << "</div></div>\n";

    // Best map per operation (find the overall winner per operation)
    auto ops = unique_operations(results);
    auto maps = unique_maps(results);

    // For each operation, find the best map (lowest mean across all data sizes)
    std::string best_find_map;
    double best_find_ns = 1e18;
    std::string best_insert_map;
    double best_insert_ns = 1e18;

    for (const auto& op : ops) {
        double op_best_ns = 1e18;
        std::string op_best_map;
        for (const auto& map : maps) {
            double sum = 0;
            int cnt = 0;
            for (const auto& r : results) {
                if (r.operation_name == op && r.map_name == map) {
                    sum += r.timing.mean_ns;
                    cnt++;
                }
            }
            if (cnt > 0) {
                double avg = sum / cnt;
                if (avg < op_best_ns) {
                    op_best_ns = avg;
                    op_best_map = map;
                }
            }
        }
        if (op.find("Find") != std::string::npos || op.find("find") != std::string::npos) {
            if (op_best_ns < best_find_ns) {
                best_find_ns = op_best_ns;
                best_find_map = op_best_map;
            }
        }
        if (op.find("Insert") != std::string::npos || op.find("insert") != std::string::npos) {
            if (op_best_ns < best_insert_ns) {
                best_insert_ns = op_best_ns;
                best_insert_map = op_best_map;
            }
        }
    }

    if (!best_find_map.empty()) {
        o << "<div class=\"card winner\"><div class=\"label\">Best Find Map</div>"
          << "<div class=\"value\">" << best_find_map << "</div>"
          << "<div class=\"detail\">avg " << format_ns(best_find_ns) << "</div></div>\n";
    }
    if (!best_insert_map.empty()) {
        o << "<div class=\"card winner\"><div class=\"label\">Best Insert Map</div>"
          << "<div class=\"value\">" << best_insert_map << "</div>"
          << "<div class=\"detail\">avg " << format_ns(best_insert_ns) << "</div></div>\n";
    }

    // Performance range for FindHit
    double fastest = 1e18, slowest = 0;
    std::string fastest_map, slowest_map;
    for (const auto& r : results) {
        if (r.operation_name.find("Find") != std::string::npos ||
            r.operation_name.find("find") != std::string::npos) {
            if (r.timing.mean_ns < fastest) {
                fastest = r.timing.mean_ns;
                fastest_map = r.map_name;
            }
            if (r.timing.mean_ns > slowest) {
                slowest = r.timing.mean_ns;
                slowest_map = r.map_name;
            }
        }
    }
    if (fastest < 1e18) {
        double ratio = (slowest > 0 && fastest > 0) ? slowest / fastest : 0;
        o << "<div class=\"card\"><div class=\"label\">Find Performance Range</div>"
          << "<div class=\"value\">" << fmt(ratio, 1) << "x</div>"
          << "<div class=\"detail\">" << fastest_map << " (" << format_ns(fastest)
          << ") vs " << slowest_map << " (" << format_ns(slowest) << ")</div></div>\n";
    }

    // Maps tested
    o << "<div class=\"card\"><div class=\"label\">Maps Tested</div>"
      << "<div class=\"value\">" << maps.size() << "</div></div>\n";

    // Operations tested
    o << "<div class=\"card\"><div class=\"label\">Operations</div>"
      << "<div class=\"value\">" << ops.size() << "</div></div>\n";

    // Data sizes
    auto sizes = unique_data_sizes(results);
    if (!sizes.empty()) {
        o << "<div class=\"card\"><div class=\"label\">Data Size Range</div>"
          << "<div class=\"value\">" << sizes.front() << " &ndash; " << sizes.back() << "</div></div>\n";
    }

    o << "</div>\n";
    return o.str();
}

// ── Comparison Tables ──────────────────────────────────────────────

std::string ReportGenerator::generate_comparison_tables(const std::vector<BenchResult>& results) {
    if (results.empty()) return "";

    auto ops = unique_operations(results);
    std::ostringstream o;
    o << "<h2>Comparison Tables</h2>\n";

    for (const auto& op : ops) {
        o << "<h3>" << op << "</h3>\n";
        o << "<div class=\"table-wrap\"><table data-operation=\"" << op << "\">\n";
        o << "<thead><tr>"
           << "<th data-col=\"map\">Map <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"size\">DataSize <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"mean\">Mean(ns) <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"min\">Min(ns) <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"max\">Max(ns) <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"stddev\">StdDev <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"p50\">P50 <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"p90\">P90 <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"p99\">P99 <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"tput\">Throughput(op/s) <span class=\"sort-arrow\">&#9650;</span></th>"
           << "<th data-col=\"mem\">Memory(MB) <span class=\"sort-arrow\">&#9650;</span></th>"
           << "</tr></thead><tbody>\n";

        // Collect results for this operation, grouped by data_size
        auto sizes = unique_data_sizes(results);
        for (size_t ds : sizes) {
            // Find all results for this (op, ds)
            std::vector<size_t> idx;
            for (size_t i = 0; i < results.size(); i++) {
                if (results[i].operation_name == op && results[i].data_size == ds)
                    idx.push_back(i);
            }
            if (idx.empty()) continue;

            // Find best/worst mean in this group
            double best_mean = 1e18, worst_mean = 0;
            for (auto i : idx) {
                double m = results[i].timing.mean_ns;
                if (m < best_mean) best_mean = m;
                if (m > worst_mean) worst_mean = m;
            }

            o << "<tr class=\"section-header\"><td colspan=\"11\">Data Size: " << ds << "</td></tr>\n";

            for (auto i : idx) {
                const auto& r = results[i];
                const auto& t = r.timing;
                std::string mean_cls;
                if (idx.size() > 1) {
                    if (t.mean_ns == best_mean) mean_cls = " class=\"best\"";
                    else if (t.mean_ns == worst_mean) mean_cls = " class=\"worst\"";
                }
                o << "<tr>"
                  << "<td>" << r.map_name << "</td>"
                  << "<td>" << r.data_size << "</td>"
                  << "<td" << mean_cls << " data-numeric=\"" << t.mean_ns << "\">" << fmt(t.mean_ns, 1) << "</td>"
                  << "<td data-numeric=\"" << t.min_ns << "\">" << fmt(t.min_ns, 1) << "</td>"
                  << "<td data-numeric=\"" << t.max_ns << "\">" << fmt(t.max_ns, 1) << "</td>"
                  << "<td data-numeric=\"" << t.stddev_ns << "\">" << fmt(t.stddev_ns, 1) << "</td>"
                  << "<td data-numeric=\"" << t.p50_ns << "\">" << fmt(t.p50_ns, 1) << "</td>"
                  << "<td data-numeric=\"" << t.p90_ns << "\">" << fmt(t.p90_ns, 1) << "</td>"
                  << "<td data-numeric=\"" << t.p99_ns << "\">" << fmt(t.p99_ns, 1) << "</td>"
                  << "<td data-numeric=\"" << t.throughput_ops_per_sec << "\">" << format_throughput(t.throughput_ops_per_sec) << "</td>"
                  << "<td data-numeric=\"" << r.memory_delta << "\">" << format_bytes(r.memory_delta) << "</td>"
                  << "</tr>\n";
            }
        }
        o << "</tbody></table></div>\n";
    }
    return o.str();
}

// ── Chart Data JavaScript ──────────────────────────────────────────

std::string ReportGenerator::generate_chart_data_js(const std::vector<BenchResult>& results) {
    auto ops = unique_operations(results);
    auto maps = unique_maps(results);
    auto sizes = unique_data_sizes(results);

    // Color palette for maps
    std::vector<std::string> palette = {
        "#58a6ff", "#3fb950", "#f85149", "#d29922", "#bc8cff",
        "#39d2c0", "#f778ba", "#79c0ff", "#56d364", "#ff7b72",
        "#e3b341", "#d2a8ff", "#7ee787", "#ffa657", "#a5d6ff"
    };

    std::ostringstream o;

    // Raw data for charts
    o << "const BENCH_DATA = " << results_array_json(results) << ";\n\n";

    // Unique arrays
    o << "const OPERATIONS = [";
    for (size_t i = 0; i < ops.size(); i++) {
        if (i) o << ",";
        o << json_str(ops[i]);
    }
    o << "];\n";

    o << "const MAPS = [";
    for (size_t i = 0; i < maps.size(); i++) {
        if (i) o << ",";
        o << json_str(maps[i]);
    }
    o << "];\n";

    o << "const DATA_SIZES = [";
    for (size_t i = 0; i < sizes.size(); i++) {
        if (i) o << ",";
        o << sizes[i];
    }
    o << "];\n";

    o << "const PALETTE = [";
    for (size_t i = 0; i < palette.size(); i++) {
        if (i) o << ",";
        o << json_str(palette[i]);
    }
    o << "];\n\n";

    return o.str();
}

// ── Charts Section ─────────────────────────────────────────────────

std::string ReportGenerator::generate_charts_section(const std::vector<BenchResult>& results) {
    if (results.empty()) return "";

    std::ostringstream o;
    o << "<h2>Charts</h2>\n";
    o << "<div class=\"chart-grid\">\n";

    // 1. Bar chart: Mean time by map type
    o << "<div class=\"chart-box\"><h3>Mean Time by Map Type (Grouped by Operation)</h3>"
      << "<canvas id=\"chartMeanBar\"></canvas></div>\n";

    // 2. Line chart: Scalability
    o << "<div class=\"chart-box\"><h3>Scalability: Mean Time vs Data Size (log-log)</h3>"
      << "<canvas id=\"chartScalability\"></canvas></div>\n";

    // 3. Box plot approximation
    o << "<div class=\"chart-box\"><h3>Latency Distribution: Min/P50/P90/P99/Max</h3>"
      << "<canvas id=\"chartBoxPlot\"></canvas></div>\n";

    // 4. Throughput chart
    o << "<div class=\"chart-box\"><h3>Throughput vs Data Size</h3>"
      << "<canvas id=\"chartThroughput\"></canvas></div>\n";

    // 5. Memory chart
    o << "<div class=\"chart-box\"><h3>Memory Delta vs Data Size</h3>"
      << "<canvas id=\"chartMemory\"></canvas></div>\n";

    // 6. Latency P50/P90/P99 comparison
    o << "<div class=\"chart-box\"><h3>Tail Latency: P50 / P90 / P99 Comparison</h3>"
      << "<canvas id=\"chartTailLatency\"></canvas></div>\n";

    // 7. Hash function comparison
    o << "<div class=\"chart-box\"><h3>Hash Function Comparison</h3>"
      << "<canvas id=\"chartHashCompare\"></canvas></div>\n";

    o << "</div>\n";
    return o.str();
}

// ── Statistical Analysis ───────────────────────────────────────────

std::string ReportGenerator::generate_statistical_analysis(const std::vector<BenchResult>& results) {
    if (results.empty()) return "";

    auto groups = group_by_op_and_size(results);

    std::ostringstream o;
    o << "<h2>Statistical Analysis</h2>\n";
    o << "<div class=\"analysis-grid\">\n";

    for (const auto& g : groups) {
        if (g.indices.size() < 2) continue;

        // Find best (lowest mean)
        size_t best_idx = g.indices[0];
        double best_mean = results[best_idx].timing.mean_ns;
        for (auto i : g.indices) {
            if (results[i].timing.mean_ns < best_mean) {
                best_mean = results[i].timing.mean_ns;
                best_idx = i;
            }
        }

        o << "<div class=\"analysis-card\">\n";
        o << "<div class=\"title\">" << g.operation << " @ size=" << g.data_size << "</div>\n";

        // Rankings
        std::vector<size_t> ranked = g.indices;
        std::sort(ranked.begin(), ranked.end(), [&](size_t a, size_t b) {
            return results[a].timing.mean_ns < results[b].timing.mean_ns;
        });

        o << "<div class=\"info-item\">Rankings (best to worst):</div>\n";
        for (size_t rank = 0; rank < ranked.size(); rank++) {
            const auto& r = results[ranked[rank]];
            double speedup = compute_speedup(best_mean, r.timing.mean_ns);
            std::string cls = (speedup > 1.2) ? "regression" : "info-item";
            o << "<div class=\"" << cls << "\">"
              << (rank + 1) << ". " << r.map_name
              << " &mdash; " << format_ns(r.timing.mean_ns)
              << " (" << fmt(speedup, 2) << "x vs best)</div>\n";
        }

        // Regressions (>20% slower than best)
        for (auto i : g.indices) {
            double speedup = compute_speedup(best_mean, results[i].timing.mean_ns);
            if (speedup > 1.2) {
                o << "<div class=\"regression\">&#9888; " << results[i].map_name
                  << " is " << fmt((speedup - 1.0) * 100, 0) << "% slower than "
                  << results[best_idx].map_name << "</div>\n";
            }
        }

        // Trend indicator: compare small vs large data sizes is not per-group,
        // but we can note stddev as stability indicator
        double avg_stddev = 0;
        for (auto i : g.indices) avg_stddev += results[i].timing.stddev_ns;
        avg_stddev /= g.indices.size();
        double avg_mean = 0;
        for (auto i : g.indices) avg_mean += results[i].timing.mean_ns;
        avg_mean /= g.indices.size();
        double cv = (avg_mean > 0) ? avg_stddev / avg_mean : 0;
        o << "<div class=\"info-item\">Coefficient of Variation: " << fmt(cv * 100, 1) << "% (";
        if (cv < 0.05) o << "very stable";
        else if (cv < 0.15) o << "stable";
        else if (cv < 0.30) o << "moderate variance";
        else o << "high variance";
        o << ")</div>\n";

        o << "</div>\n";
    }

    o << "</div>\n";

    // Trend analysis across data sizes
    auto ops = unique_operations(results);
    auto sizes = unique_data_sizes(results);
    auto maps_list = unique_maps(results);

    if (sizes.size() >= 2) {
        o << "<h3>Scaling Trend Analysis</h3>\n<div class=\"analysis-grid\">\n";
        for (const auto& op : ops) {
            for (const auto& map : maps_list) {
                // Collect mean times across data sizes
                std::vector<std::pair<size_t, double>> pts;
                for (const auto& r : results) {
                    if (r.operation_name == op && r.map_name == map)
                        pts.emplace_back(r.data_size, r.timing.mean_ns);
                }
                if (pts.size() < 2) continue;

                std::sort(pts.begin(), pts.end());

                // Simple linear fit on log-log scale to estimate exponent
                double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
                int n = 0;
                for (const auto& p : pts) {
                    if (p.first <= 0 || p.second <= 0) continue;
                    double lx = std::log10(static_cast<double>(p.first));
                    double ly = std::log10(p.second);
                    sum_x += lx; sum_y += ly;
                    sum_xy += lx * ly; sum_x2 += lx * lx;
                    n++;
                }
                if (n < 2) continue;
                double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);

                std::string scaling;
                if (slope < 0.3)       scaling = "sub-linear (fast)";
                else if (slope < 0.8)  scaling = "sub-linear";
                else if (slope < 1.2)  scaling = "linear";
                else if (slope < 1.8)  scaling = "super-linear";
                else                   scaling = "quadratic+ (slow)";

                o << "<div class=\"analysis-card\">"
                  << "<div class=\"title\">" << op << " / " << map << "</div>"
                  << "<div class=\"info-item\">Scaling exponent: " << fmt(slope, 2)
                  << " (" << scaling << ")</div>"
                  << "</div>\n";
            }
        }
        o << "</div>\n";
    }

    return o.str();
}

// ── Configuration Summary ──────────────────────────────────────────

std::string ReportGenerator::generate_config_summary(const std::vector<BenchResult>& results) {
    if (results.empty()) return "";

    std::ostringstream o;
    o << "<h2>Configuration Summary</h2>\n<div class=\"config-grid\">\n";

    // Compiler info from first result
    const auto& r0 = results[0];
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Compiler</div>"
      << "<div class=\"cfg-value\">" << r0.compiler_name << "</div></div>\n";
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Version</div>"
      << "<div class=\"cfg-value\">" << r0.compiler_version << "</div></div>\n";
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Optimization</div>"
      << "<div class=\"cfg-value\">" << r0.optimization_flag << "</div></div>\n";

    // Key type
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Key Type</div>"
      << "<div class=\"cfg-value\">" << r0.key_type_name << "</div></div>\n";

    // Hash
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Hash Function</div>"
      << "<div class=\"cfg-value\">" << r0.hash_name << "</div></div>\n";

    // Data sizes
    auto sizes = unique_data_sizes(results);
    std::ostringstream sz_str;
    for (size_t i = 0; i < sizes.size(); i++) {
        if (i) sz_str << ", ";
        sz_str << sizes[i];
    }
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Data Sizes</div>"
      << "<div class=\"cfg-value\">" << sz_str.str() << "</div></div>\n";

    // Maps
    auto maps = unique_maps(results);
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Maps Tested</div>"
      << "<div class=\"cfg-value\">" << maps.size() << "</div></div>\n";

    // Operations
    auto ops = unique_operations(results);
    o << "<div class=\"config-item\"><div class=\"cfg-label\">Operations</div>"
      << "<div class=\"cfg-value\">" << ops.size() << "</div></div>\n";

    o << "</div>\n";
    return o.str();
}

// ── Recommendations ────────────────────────────────────────────────

std::string ReportGenerator::generate_recommendations(const std::vector<BenchResult>& results) {
    if (results.empty()) return "";

    auto ops = unique_operations(results);
    auto maps = unique_maps(results);
    auto sizes = unique_data_sizes(results);

    std::ostringstream o;
    o << "<h2>Recommendations</h2>\n";

    // 1. Best map for small datasets
    if (!sizes.empty()) {
        size_t small_threshold = 100;
        // Find the smallest data size
        size_t smallest = sizes.front();
        if (smallest <= small_threshold) {
            // Find best map for smallest size across all operations
            std::string best_small_map;
            double best_small_ns = 1e18;
            for (const auto& map : maps) {
                double sum = 0;
                int cnt = 0;
                for (const auto& r : results) {
                    if (r.map_name == map && r.data_size == smallest) {
                        sum += r.timing.mean_ns;
                        cnt++;
                    }
                }
                if (cnt > 0 && sum / cnt < best_small_ns) {
                    best_small_ns = sum / cnt;
                    best_small_map = map;
                }
            }
            if (!best_small_map.empty()) {
                o << "<div class=\"recommendation\">"
                  << "<div class=\"rec-title\">Small Dataset Optimization</div>"
                  << "<div class=\"rec-body\">For small datasets (&le;" << smallest
                  << "), <strong>" << best_small_map
                  << "</strong> is optimal with average latency of "
                  << format_ns(best_small_ns) << ".</div></div>\n";
            }
        }
    }

    // 2. Best P99 for find-heavy workloads
    {
        std::string best_p99_map;
        double best_p99 = 1e18;
        for (const auto& map : maps) {
            double sum_p99 = 0;
            int cnt = 0;
            for (const auto& r : results) {
                if ((r.operation_name.find("Find") != std::string::npos ||
                     r.operation_name.find("find") != std::string::npos) &&
                    r.map_name == map) {
                    sum_p99 += r.timing.p99_ns;
                    cnt++;
                }
            }
            if (cnt > 0 && sum_p99 / cnt < best_p99) {
                best_p99 = sum_p99 / cnt;
                best_p99_map = map;
            }
        }
        if (!best_p99_map.empty()) {
            o << "<div class=\"recommendation\">"
              << "<div class=\"rec-title\">Find-Heavy Workload</div>"
              << "<div class=\"rec-body\">For find-heavy workloads, <strong>" << best_p99_map
              << "</strong> provides the best P99 latency at "
              << format_ns(best_p99) << ".</div></div>\n";
        }
    }

    // 3. Memory overhead at large scales
    if (sizes.size() >= 2) {
        size_t largest = sizes.back();
        std::string high_mem_map;
        double max_mem = 0;
        for (const auto& r : results) {
            if (r.data_size == largest && r.memory_delta > max_mem) {
                max_mem = r.memory_delta;
                high_mem_map = r.map_name;
            }
        }
        if (!high_mem_map.empty() && max_mem > 0) {
            // Find the lowest memory at same size
            double min_mem = 1e18;
            for (const auto& r : results) {
                if (r.data_size == largest && r.memory_delta > 0 && r.memory_delta < min_mem)
                    min_mem = r.memory_delta;
            }
            double ratio = (min_mem > 0) ? max_mem / min_mem : 0;
            if (ratio > 1.3) {
                o << "<div class=\"recommendation\">"
                  << "<div class=\"rec-title\">Memory Overhead Warning</div>"
                  << "<div class=\"rec-body\"><strong>" << high_mem_map
                  << "</strong> shows high memory overhead at data size "
                  << largest << " (" << format_bytes(max_mem)
                  << ", " << fmt(ratio, 1) << "x the most efficient). "
                  << "Consider using a more memory-efficient map for large-scale deployments.</div></div>\n";
            }
        }
    }

    // 4. Overall best map across all operations
    {
        std::string overall_best;
        double overall_best_ns = 1e18;
        for (const auto& map : maps) {
            double sum = 0;
            int cnt = 0;
            for (const auto& r : results) {
                if (r.map_name == map) {
                    sum += r.timing.mean_ns;
                    cnt++;
                }
            }
            if (cnt > 0 && sum / cnt < overall_best_ns) {
                overall_best_ns = sum / cnt;
                overall_best = map;
            }
        }
        if (!overall_best.empty()) {
            o << "<div class=\"recommendation\">"
              << "<div class=\"rec-title\">Overall Best Performer</div>"
              << "<div class=\"rec-body\">Across all operations and data sizes, <strong>"
              << overall_best << "</strong> has the lowest average latency of "
              << format_ns(overall_best_ns) << ".</div></div>\n";
        }
    }

    // 5. Erase operation recommendation
    {
        std::string best_erase_map;
        double best_erase_ns = 1e18;
        for (const auto& map : maps) {
            double sum = 0;
            int cnt = 0;
            for (const auto& r : results) {
                if ((r.operation_name.find("Erase") != std::string::npos ||
                     r.operation_name.find("erase") != std::string::npos) &&
                    r.map_name == map) {
                    sum += r.timing.mean_ns;
                    cnt++;
                }
            }
            if (cnt > 0 && sum / cnt < best_erase_ns) {
                best_erase_ns = sum / cnt;
                best_erase_map = map;
            }
        }
        if (!best_erase_map.empty()) {
            o << "<div class=\"recommendation\">"
              << "<div class=\"rec-title\">Erase-Heavy Workload</div>"
              << "<div class=\"rec-body\">For erase-heavy workloads, <strong>" << best_erase_map
              << "</strong> is the best choice with average erase latency of "
              << format_ns(best_erase_ns) << ".</div></div>\n";
        }
    }

    return o.str();
}

// ── Main generate_html ─────────────────────────────────────────────

bool ReportGenerator::generate_html(const std::vector<BenchResult>& results,
                                    const std::string& output_path) {
    std::string timestamp = get_timestamp();

    std::ostringstream html;

    // Head + CSS
    html << generate_head(timestamp);

    // Summary Dashboard
    html << generate_summary_dashboard(results);

    // Comparison Tables
    html << generate_comparison_tables(results);

    // Charts
    html << generate_charts_section(results);

    // Statistical Analysis
    html << generate_statistical_analysis(results);

    // Configuration Summary
    html << generate_config_summary(results);

    // Recommendations
    html << generate_recommendations(results);

    // Chart data JS + Chart rendering JS
    html << "<script>\n";
    html << generate_chart_data_js(results);
    html << R"js(
// ── Chart defaults ─────────────────────────────────────────────────
Chart.defaults.color = '#8b949e';
Chart.defaults.borderColor = '#30363d';
Chart.defaults.font.family = "-apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif";
Chart.defaults.font.size = 11;
Chart.defaults.plugins.legend.labels.boxWidth = 12;
Chart.defaults.plugins.legend.labels.padding = 16;
Chart.defaults.animation.duration = 600;

const tooltipStyle = {
    backgroundColor: '#161b22',
    titleColor: '#e6edf3',
    bodyColor: '#8b949e',
    borderColor: '#30363d',
    borderWidth: 1,
    padding: 10,
    cornerRadius: 6,
};

function getColor(i) { return PALETTE[i % PALETTE.length]; }
function getColorAlpha(i, a) {
    const hex = PALETTE[i % PALETTE.length];
    const r = parseInt(hex.slice(1,3),16), g = parseInt(hex.slice(3,5),16), b = parseInt(hex.slice(5,7),16);
    return `rgba(${r},${g},${b},${a})`;
}

// Helper: filter data
function filterData(op, size, map) {
    return BENCH_DATA.filter(d => {
        if (op && d.operation_name !== op) return false;
        if (size && d.data_size !== size) return false;
        if (map && d.map_name !== map) return false;
        return true;
    });
}

// ── 1. Mean Time Bar Chart (grouped by operation) ─────────────────
(function() {
    const labels = OPERATIONS;
    const datasets = MAPS.map((map, mi) => {
        const data = OPERATIONS.map(op => {
            const recs = filterData(op, null, map);
            if (!recs.length) return 0;
            const sum = recs.reduce((s, r) => s + r.timing.mean_ns, 0);
            return sum / recs.length;
        });
        return {
            label: map,
            data: data,
            backgroundColor: getColorAlpha(mi, 0.7),
            borderColor: getColor(mi),
            borderWidth: 1,
        };
    });
    new Chart(document.getElementById('chartMeanBar'), {
        type: 'bar',
        data: { labels, datasets },
        options: {
            responsive: true,
            interaction: { mode: 'index', intersect: false },
            plugins: {
                tooltip: { ...tooltipStyle, callbacks: {
                    label: ctx => ctx.dataset.label + ': ' + ctx.parsed.y.toFixed(1) + ' ns'
                }},
            },
            scales: {
                y: { type: 'logarithmic', title: { display: true, text: 'Mean Time (ns, log scale)' }},
                x: { title: { display: true, text: 'Operation' }}
            }
        }
    });
})();

// ── 2. Scalability Line Chart (log-log) ────────────────────────────
(function() {
    if (DATA_SIZES.length < 2) return;
    const op = OPERATIONS[0]; // primary operation
    const datasets = MAPS.map((map, mi) => {
        const pts = DATA_SIZES.map(sz => {
            const recs = filterData(op, sz, map);
            if (!recs.length) return null;
            return { x: sz, y: recs[0].timing.mean_ns };
        }).filter(p => p !== null);
        return {
            label: map,
            data: pts,
            borderColor: getColor(mi),
            backgroundColor: getColorAlpha(mi, 0.1),
            fill: false,
            tension: 0.3,
            pointRadius: 4,
        };
    });
    new Chart(document.getElementById('chartScalability'), {
        type: 'line',
        data: { datasets },
        options: {
            responsive: true,
            interaction: { mode: 'nearest', intersect: false },
            plugins: {
                title: { display: true, text: 'Operation: ' + op, color: '#8b949e' },
                tooltip: { ...tooltipStyle, callbacks: {
                    label: ctx => ctx.dataset.label + ': ' + ctx.parsed.y.toFixed(1) + ' ns @ size=' + ctx.parsed.x
                }},
            },
            scales: {
                x: { type: 'logarithmic', title: { display: true, text: 'Data Size (log)' }},
                y: { type: 'logarithmic', title: { display: true, text: 'Mean Time ns (log)' }},
            }
        }
    });
})();

// ── 3. Box Plot Approximation (error bars via floating bar) ────────
(function() {
    const op = OPERATIONS[0];
    const sz = DATA_SIZES[0];
    const recs = filterData(op, sz, null);
    if (!recs.length) return;

    const labels = recs.map(r => r.map_name);
    // Show [min, max] as the bar, with p50/p90/p99 as custom annotation
    const minData = recs.map(r => r.timing.min_ns);
    const p50Data = recs.map(r => r.timing.p50_ns);
    const p90Data = recs.map(r => r.timing.p90_ns);
    const p99Data = recs.map(r => r.timing.p99_ns);
    const maxData = recs.map(r => r.timing.max_ns);

    // Use floating bars for ranges
    const datasets = [
        { label: 'Min-Max', data: recs.map((r,i) => [r.timing.min_ns, r.timing.max_ns]),
          backgroundColor: recs.map((_,i) => getColorAlpha(i, 0.2)),
          borderColor: recs.map((_,i) => getColor(i)), borderWidth: 1 },
        { label: 'P50-P99', data: recs.map((r,i) => [r.timing.p50_ns, r.timing.p99_ns]),
          backgroundColor: recs.map((_,i) => getColorAlpha(i, 0.5)),
          borderColor: recs.map((_,i) => getColor(i)), borderWidth: 1 },
    ];
    new Chart(document.getElementById('chartBoxPlot'), {
        type: 'bar',
        data: { labels, datasets },
        options: {
            responsive: true,
            indexAxis: 'y',
            interaction: { mode: 'nearest', intersect: true },
            plugins: {
                title: { display: true, text: op + ' @ size=' + sz, color: '#8b949e' },
                tooltip: { ...tooltipStyle, callbacks: {
                    label: ctx => {
                        const v = ctx.raw;
                        if (Array.isArray(v)) return ctx.dataset.label + ': ' + v[0].toFixed(1) + ' - ' + v[1].toFixed(1) + ' ns';
                        return ctx.dataset.label + ': ' + v.toFixed(1) + ' ns';
                    }
                }},
            },
            scales: {
                x: { type: 'logarithmic', title: { display: true, text: 'Time (ns, log)' }},
            }
        }
    });
})();

// ── 4. Throughput Chart ────────────────────────────────────────────
(function() {
    const datasets = MAPS.map((map, mi) => {
        const pts = DATA_SIZES.map(sz => {
            const recs = filterData(null, sz, map);
            if (!recs.length) return null;
            const avg = recs.reduce((s,r) => s + r.timing.throughput_ops_per_sec, 0) / recs.length;
            return { x: sz, y: avg };
        }).filter(p => p !== null);
        return {
            label: map,
            data: pts,
            borderColor: getColor(mi),
            backgroundColor: getColorAlpha(mi, 0.1),
            fill: false,
            tension: 0.3,
            pointRadius: 4,
        };
    });
    new Chart(document.getElementById('chartThroughput'), {
        type: 'line',
        data: { datasets },
        options: {
            responsive: true,
            interaction: { mode: 'nearest', intersect: false },
            plugins: {
                tooltip: { ...tooltipStyle, callbacks: {
                    label: ctx => {
                        const v = ctx.parsed.y;
                        if (v >= 1e9) return ctx.dataset.label + ': ' + (v/1e9).toFixed(2) + ' Gop/s';
                        if (v >= 1e6) return ctx.dataset.label + ': ' + (v/1e6).toFixed(2) + ' Mop/s';
                        return ctx.dataset.label + ': ' + (v/1e3).toFixed(2) + ' Kop/s';
                    }
                }},
            },
            scales: {
                x: { type: 'logarithmic', title: { display: true, text: 'Data Size' }},
                y: { type: 'logarithmic', title: { display: true, text: 'Throughput (op/s, log)' }},
            }
        }
    });
})();

// ── 5. Memory Chart ────────────────────────────────────────────────
(function() {
    const datasets = MAPS.map((map, mi) => {
        const pts = DATA_SIZES.map(sz => {
            const recs = filterData(null, sz, map);
            if (!recs.length) return null;
            const avg = recs.reduce((s,r) => s + r.memory_delta, 0) / recs.length;
            return { x: sz, y: avg / (1024*1024) }; // MB
        }).filter(p => p !== null);
        return {
            label: map,
            data: pts,
            borderColor: getColor(mi),
            backgroundColor: getColorAlpha(mi, 0.1),
            fill: true,
            tension: 0.3,
            pointRadius: 4,
        };
    });
    new Chart(document.getElementById('chartMemory'), {
        type: 'line',
        data: { datasets },
        options: {
            responsive: true,
            interaction: { mode: 'nearest', intersect: false },
            plugins: {
                tooltip: { ...tooltipStyle, callbacks: {
                    label: ctx => ctx.dataset.label + ': ' + ctx.parsed.y.toFixed(2) + ' MB'
                }},
            },
            scales: {
                x: { type: 'logarithmic', title: { display: true, text: 'Data Size' }},
                y: { title: { display: true, text: 'Memory Delta (MB)' }},
            }
        }
    });
})();

// ── 6. Tail Latency P50/P90/P99 ────────────────────────────────────
(function() {
    const op = OPERATIONS[0];
    const sz = DATA_SIZES[0];
    const recs = filterData(op, sz, null);
    if (!recs.length) return;

    const labels = recs.map(r => r.map_name);
    const datasets = [
        { label: 'P50', data: recs.map(r => r.timing.p50_ns), backgroundColor: getColorAlpha(0, 0.7), borderColor: getColor(0), borderWidth: 1 },
        { label: 'P90', data: recs.map(r => r.timing.p90_ns), backgroundColor: getColorAlpha(1, 0.7), borderColor: getColor(1), borderWidth: 1 },
        { label: 'P99', data: recs.map(r => r.timing.p99_ns), backgroundColor: getColorAlpha(3, 0.7), borderColor: getColor(3), borderWidth: 1 },
    ];
    new Chart(document.getElementById('chartTailLatency'), {
        type: 'bar',
        data: { labels, datasets },
        options: {
            responsive: true,
            interaction: { mode: 'index', intersect: false },
            plugins: {
                title: { display: true, text: op + ' @ size=' + sz, color: '#8b949e' },
                tooltip: { ...tooltipStyle, callbacks: {
                    label: ctx => ctx.dataset.label + ': ' + ctx.parsed.y.toFixed(1) + ' ns'
                }},
            },
            scales: {
                y: { type: 'logarithmic', title: { display: true, text: 'Latency (ns, log)' }},
                x: { title: { display: true, text: 'Map' }}
            }
        }
    });
})();

// ── 7. Hash Function Comparison ────────────────────────────────────
(function() {
    // Group by hash_name and compute average mean for find operations
    const hashNames = [...new Set(BENCH_DATA.map(r => r.hash_name))];
    if (hashNames.length <= 1) {
        // If only one hash, show bar chart by map instead
        const recs = BENCH_DATA.filter(r => r.operation_name.indexOf('Find') >= 0 || r.operation_name.indexOf('find') >= 0);
        if (!recs.length) return;
        const labels = [...new Set(recs.map(r => r.map_name))];
        const data = labels.map(m => {
            const subset = recs.filter(r => r.map_name === m);
            return subset.reduce((s,r) => s + r.timing.mean_ns, 0) / subset.length;
        });
        new Chart(document.getElementById('chartHashCompare'), {
            type: 'bar',
            data: {
                labels,
                datasets: [{
                    label: 'Avg Find Latency (ns)',
                    data,
                    backgroundColor: labels.map((_,i) => getColorAlpha(i, 0.7)),
                    borderColor: labels.map((_,i) => getColor(i)),
                    borderWidth: 1,
                }]
            },
            options: {
                responsive: true,
                plugins: {
                    tooltip: { ...tooltipStyle, callbacks: {
                        label: ctx => ctx.parsed.y.toFixed(1) + ' ns'
                    }},
                },
                scales: {
                    y: { type: 'logarithmic', title: { display: true, text: 'Mean Find Time (ns, log)' }},
                    x: { title: { display: true, text: 'Map' }}
                }
            }
        });
        return;
    }
    // Multiple hash functions
    const labels = hashNames;
    const findRecs = BENCH_DATA.filter(r => r.operation_name.indexOf('Find') >= 0);
    const data = hashNames.map(h => {
        const subset = findRecs.filter(r => r.hash_name === h);
        if (!subset.length) return 0;
        return subset.reduce((s,r) => s + r.timing.mean_ns, 0) / subset.length;
    });
    new Chart(document.getElementById('chartHashCompare'), {
        type: 'bar',
        data: {
            labels,
            datasets: [{
                label: 'Avg Find Latency (ns)',
                data,
                backgroundColor: labels.map((_,i) => getColorAlpha(i, 0.7)),
                borderColor: labels.map((_,i) => getColor(i)),
                borderWidth: 1,
            }]
        },
        options: {
            responsive: true,
            plugins: {
                tooltip: { ...tooltipStyle, callbacks: {
                    label: ctx => ctx.parsed.y.toFixed(1) + ' ns'
                }},
            },
            scales: {
                y: { type: 'logarithmic', title: { display: true, text: 'Mean Find Time (ns, log)' }},
                x: { title: { display: true, text: 'Hash Function' }}
            }
        }
    });
})();

// ── Sortable Tables ────────────────────────────────────────────────
document.querySelectorAll('table[data-operation]').forEach(table => {
    const tbody = table.querySelector('tbody');
    const headers = table.querySelectorAll('thead th');
    headers.forEach((th, colIdx) => {
        let asc = true;
        th.addEventListener('click', () => {
            asc = !asc;
            headers.forEach(h => h.classList.remove('sorted-asc', 'sorted-desc'));
            th.classList.add(asc ? 'sorted-asc' : 'sorted-desc');

            const rows = Array.from(tbody.querySelectorAll('tr:not(.section-header)'));
            rows.sort((a, b) => {
                const aCell = a.children[colIdx];
                const bCell = b.children[colIdx];
                const aVal = aCell.getAttribute('data-numeric') || aCell.textContent;
                const bVal = bCell.getAttribute('data-numeric') || bCell.textContent;
                const aNum = parseFloat(aVal), bNum = parseFloat(bVal);
                if (!isNaN(aNum) && !isNaN(bNum)) return asc ? aNum - bNum : bNum - aNum;
                return asc ? aVal.localeCompare(bVal) : bVal.localeCompare(aVal);
            });
            rows.forEach(r => tbody.appendChild(r));
        });
    });
});

</script>
)js";

    // Close HTML
    html << "</div>\n</body>\n</html>\n";

    // Write file
    std::ofstream ofs(output_path);
    if (!ofs.is_open()) return false;
    ofs << html.str();
    ofs.close();
    return ofs.good();
}

} // namespace emilib_bench
