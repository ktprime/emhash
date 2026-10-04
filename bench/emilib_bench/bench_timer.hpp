#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <chrono>
#include <functional>

#ifdef _MSC_VER
#include <intrin.h>
#elif defined(__x86_64__) || defined(__amd64__) || defined(__i386__) || defined(_M_X64)
#include <x86intrin.h>
#endif

namespace emilib_bench {

// ============================================================================
// Statistics functions (input: std::vector<uint64_t> nanosecond samples)
// ============================================================================

inline double compute_mean(const std::vector<uint64_t>& samples) {
    if (samples.empty()) return 0.0;
    double sum = 0.0;
    for (auto s : samples) sum += static_cast<double>(s);
    return sum / static_cast<double>(samples.size());
}

inline double compute_stddev(const std::vector<uint64_t>& samples) {
    if (samples.size() < 2) return 0.0;
    double mean = compute_mean(samples);
    double sum_sq = 0.0;
    for (auto s : samples) {
        double diff = static_cast<double>(s) - mean;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / static_cast<double>(samples.size() - 1));
}

inline double compute_median(std::vector<uint64_t> samples) {
    if (samples.empty()) return 0.0;
    std::sort(samples.begin(), samples.end());
    size_t n = samples.size();
    if (n % 2 == 1) {
        return static_cast<double>(samples[n / 2]);
    }
    return (static_cast<double>(samples[n / 2 - 1]) + static_cast<double>(samples[n / 2])) / 2.0;
}

inline double compute_percentile(std::vector<uint64_t> samples, double percentile) {
    if (samples.empty()) return 0.0;
    std::sort(samples.begin(), samples.end());
    if (percentile <= 0.0) return static_cast<double>(samples.front());
    if (percentile >= 100.0) return static_cast<double>(samples.back());

    double rank = (percentile / 100.0) * static_cast<double>(samples.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(rank));
    size_t upper = lower + 1;
    if (upper >= samples.size()) return static_cast<double>(samples.back());

    double frac = rank - static_cast<double>(lower);
    return static_cast<double>(samples[lower]) * (1.0 - frac) + static_cast<double>(samples[upper]) * frac;
}

// ============================================================================
// ScopedTimer: records elapsed nanoseconds on destruction
// ============================================================================
class ScopedTimer {
public:
    explicit ScopedTimer(uint64_t& result) noexcept
        : result_(result)
        , start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        result_ = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count());
    }

    // Disallow copying/moving
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    uint64_t& result_;
    std::chrono::steady_clock::time_point start_;
};

// ============================================================================
// CpuClock: cycle-accurate timing using platform-specific RDTSC
// ============================================================================
class CpuClock {
public:
    static uint64_t now() noexcept {
#if defined(_MSC_VER) && defined(_M_X64)
        return __rdtsc();
#elif defined(__i386__) || defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
        unsigned int lo, hi;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<uint64_t>(hi) << 32) | lo;
#else
        // Fallback: use steady_clock in nanoseconds (not true cycles)
        auto tp = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(tp).count());
#endif
    }

    // Estimate CPU frequency in Hz by measuring against wall clock over a short spin.
    static double estimate_frequency_hz() {
        auto wall_start = std::chrono::steady_clock::now();
        uint64_t tsc_start = now();

        // Spin for ~50ms to get a stable measurement
        volatile int sink = 0;
        auto spin_end = wall_start + std::chrono::milliseconds(50);
        while (std::chrono::steady_clock::now() < spin_end) {
            ++sink;
        }

        auto wall_end = std::chrono::steady_clock::now();
        uint64_t tsc_end = now();

        double wall_ns = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start).count());
        double cycles = static_cast<double>(tsc_end - tsc_start);

        return cycles / (wall_ns / 1e9);
    }
};

// ============================================================================
// WarmupGuard: runs N warmup iterations before measurement begins
// ============================================================================
class WarmupGuard {
public:
    explicit WarmupGuard(int warmup_count) noexcept
        : warmup_count_(warmup_count)
        , current_(0) {}

    // Returns true while still in warmup phase
    bool is_warming_up() const noexcept {
        return current_ < warmup_count_;
    }

    // Advance to next iteration. Returns true if warmup is now complete.
    bool advance() noexcept {
        ++current_;
        return current_ >= warmup_count_;
    }

    // Execute a callable N times as warmup
    template <typename Func>
    static void run_warmup(int count, Func&& fn) {
        for (int i = 0; i < count; ++i) {
            fn();
        }
    }

    // Reset for reuse
    void reset() noexcept {
        current_ = 0;
    }

private:
    int warmup_count_;
    int current_;
};

} // namespace emilib_bench
