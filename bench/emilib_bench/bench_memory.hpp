#pragma once

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <cstdio>
#include <cstring>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task_info.h>
#endif

namespace emilib_bench {

// ============================================================================
// Memory metrics structure
// ============================================================================
struct MemoryMetrics {
    size_t baseline_rss          = 0; // RSS before operation (bytes)
    size_t peak_rss              = 0; // Peak RSS observed (bytes)
    size_t delta_rss             = 0; // RSS after - RSS before (bytes)
    size_t malloc_count_estimate = 0; // Estimated allocation count (platform-dependent)
};

// ============================================================================
// MemoryTracker: cross-platform RSS tracking
// ============================================================================
class MemoryTracker {
public:
    // Take a snapshot of the current process RSS. Returns current RSS in bytes.
    static size_t snapshot() noexcept {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS pmc;
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
        return 0;
#elif defined(__linux__)
        // Parse /proc/self/status for VmRSS
        FILE* f = std::fopen("/proc/self/status", "r");
        if (!f) return 0;
        size_t rss = 0;
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "VmRSS:", 6) == 0) {
                // VmRSS is in kB
                unsigned long val = 0;
                if (std::sscanf(line + 6, "%lu", &val) == 1) {
                    rss = val * 1024; // Convert kB to bytes
                }
                break;
            }
        }
        std::fclose(f);
        return rss;
#elif defined(__APPLE__)
        task_t task = mach_task_self();
        struct task_basic_info info;
        mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
        if (task_info(task, TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
            return static_cast<size_t>(info.resident_size);
        }
        return 0;
#else
        return 0;
#endif
    }

    // Get the peak RSS of the process so far (bytes)
    static size_t peak_rss() noexcept {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS pmc;
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.PeakWorkingSetSize;
        }
        return 0;
#elif defined(__linux__)
        // Parse /proc/self/status for VmPeak
        FILE* f = std::fopen("/proc/self/status", "r");
        if (!f) return 0;
        size_t peak = 0;
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "VmPeak:", 7) == 0) {
                unsigned long val = 0;
                if (std::sscanf(line + 7, "%lu", &val) == 1) {
                    peak = val * 1024;
                }
                break;
            }
        }
        std::fclose(f);
        return peak;
#elif defined(__APPLE__)
        // macOS doesn't have a direct peak RSS accessor in task_info.
        // We return the current resident_size as the best available.
        return snapshot();
#else
        return 0;
#endif
    }

    // Measure memory delta around a callable operation.
    // Captures baseline RSS before calling fn, then RSS after, and returns
    // a MemoryMetrics struct with delta.
    template <typename Func>
    static MemoryMetrics measure_delta(Func&& fn) {
        MemoryMetrics metrics;
        metrics.baseline_rss = snapshot();
        size_t before_peak = peak_rss();

        fn();

        size_t after_rss  = snapshot();
        size_t after_peak = peak_rss();

        metrics.peak_rss  = (after_peak > before_peak) ? after_peak : before_peak;
        metrics.delta_rss = (after_rss > metrics.baseline_rss) ? (after_rss - metrics.baseline_rss) : 0;
        metrics.malloc_count_estimate = 0; // Not directly measurable without interposition

        return metrics;
    }
};

} // namespace emilib_bench
