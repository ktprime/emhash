// bench_framework.hpp — shared infrastructure for reproducible emhash micro-benchmarks.
//
// Zero-dependency harness built on the vendored `../nanobench.h` single header (v4.3.7).
// Exactly one TU must `#define ANKERL_NANOBENCH_IMPLEMENT` before including nanobench.h;
// `bench_main.cpp` is that TU.
//
// Design constraint (verified against nanobench.h): `Bench::run(name, op)` has no
// PauseTiming/ResumeTiming and re-invokes `op()` for every epoch. Therefore every workload
// below is a complete, idempotent, self-contained unit of work. Any expensive data setup
// (key generation, pre-populating the read-only maps) happens outside the measured lambda.
//
// All reported values are the *total* nanoseconds for one invocation of the workload, so the
// derived estimate `erase = erase_all - insert` is a valid subtraction.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "../nanobench.h"

namespace emhash_bench {

// ---------------------------------------------------------------------------
// Workload and key-type tags (runtime-selectable via CLI strings)
// ---------------------------------------------------------------------------

enum class Op : std::uint8_t {
    Insert,
    FindHit,
    FindMiss,
    Iterate,
    EraseAll,
    InsertErase,
};

inline constexpr std::string_view op_name(Op o) noexcept {
    switch (o) {
        case Op::Insert:      return "insert";
        case Op::FindHit:     return "find_hit";
        case Op::FindMiss:    return "find_miss";
        case Op::Iterate:     return "iterate";
        case Op::EraseAll:    return "erase_all";
        case Op::InsertErase: return "insert_erase";
    }
    return "unknown";
}

inline bool parse_op(std::string_view s, Op& out) noexcept {
    for (auto o : {Op::Insert, Op::FindHit, Op::FindMiss, Op::Iterate, Op::EraseAll, Op::InsertErase}) {
        if (op_name(o) == s) {
            out = o;
            return true;
        }
    }
    return false;
}

enum class KeyKind : std::uint8_t {
    Int32,
    Int64,
    String,
};

inline constexpr std::string_view key_kind_name(KeyKind k) noexcept {
    switch (k) {
        case KeyKind::Int32:  return "int32";
        case KeyKind::Int64:  return "int64";
        case KeyKind::String: return "string";
    }
    return "unknown";
}

inline bool parse_key_kind(std::string_view s, KeyKind& out) noexcept {
    for (auto k : {KeyKind::Int32, KeyKind::Int64, KeyKind::String}) {
        if (key_kind_name(k) == s) {
            out = k;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Reproducible key generation
//
// Hit and miss key sets are disjoint *by construction* rather than by relying on
// a different seed (which for a 32-bit key space and n=1e6 would still collide
// with probability ~1). Integer keys are split into two non-overlapping ranges;
// string keys encode the index with a different offset.
//
// String keys are 32 bytes so that they exceed typical SSO limits and therefore
// exercise heap allocation on insert, matching real-world long-key workloads.
// ---------------------------------------------------------------------------

inline constexpr std::size_t kStringKeyLen = 32;

template <typename KeyT>
std::vector<KeyT> make_keys(std::size_t n, std::uint64_t seed);

// Hit keys: live in the lower half of the value range.
template <>
inline std::vector<std::int32_t> make_keys<std::int32_t>(std::size_t n, std::uint64_t seed) {
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int32_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys.push_back(static_cast<std::int32_t>(rng() & 0x3FFFFFFFull)); // [0, 2^30)
    }
    return keys;
}

template <>
inline std::vector<std::int64_t> make_keys<std::int64_t>(std::size_t n, std::uint64_t seed) {
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int64_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys.push_back(static_cast<std::int64_t>(rng() & 0x3FFFFFFFFFFFFFFFull)); // [0, 2^62)
    }
    return keys;
}

template <>
inline std::vector<std::string> make_keys<std::string>(std::size_t n, std::uint64_t seed) {
    std::vector<std::string> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        // Vary only the leading 8 bytes; the remaining bytes are filler so the key
        // is long enough to be heap-allocated. Bytes are seed-derived so that two
        // different seeds produce different key material.
        std::uint64_t x = seed + i;
        std::string s(kStringKeyLen, '\0');
        std::memcpy(s.data(), &x, sizeof(x));
        std::memset(s.data() + sizeof(x), static_cast<int>('a' + (x % 26)), kStringKeyLen - sizeof(x));
        keys.push_back(std::move(s));
    }
    return keys;
}

// Miss keys: guaranteed absent from the corresponding hit-key set.
template <typename KeyT>
std::vector<KeyT> make_miss_keys(std::size_t n, std::uint64_t seed);

template <>
inline std::vector<std::int32_t> make_miss_keys<std::int32_t>(std::size_t n, std::uint64_t seed) {
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int32_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        // [2^30, 2^31) — disjoint from the hit range and still a valid positive int32.
        keys.push_back(static_cast<std::int32_t>((rng() & 0x3FFFFFFFull) + (1ull << 30)));
    }
    return keys;
}

template <>
inline std::vector<std::int64_t> make_miss_keys<std::int64_t>(std::size_t n, std::uint64_t seed) {
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int64_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        // [2^62, 2^63) — disjoint from the hit range and still a valid positive int64.
        keys.push_back(static_cast<std::int64_t>((rng() & 0x3FFFFFFFFFFFFFFFull) + (1ull << 62)));
    }
    return keys;
}

template <>
inline std::vector<std::string> make_miss_keys<std::string>(std::size_t n, std::uint64_t seed) {
    // Offset the encoded index beyond the hit-key range so the sets cannot overlap.
    return make_keys<std::string>(n, seed + 0x100000000ull);
}

// ---------------------------------------------------------------------------
// Measurement options
// ---------------------------------------------------------------------------

struct BenchOpts {
    std::uint64_t epochs          = 11;
    std::uint64_t min_epoch_iters = 1;
    double        max_epoch_ms    = 100.0;
    std::uint64_t warmup          = 1;
    bool          use_min         = false; // noisy hosts: `min` is the more robust estimator
};

// ---------------------------------------------------------------------------
// Workload runner
//
// Returns the total elapsed nanoseconds for one invocation of the workload
// (median across epochs, or minimum when `opts.use_min` is set).
// ---------------------------------------------------------------------------

template <typename Map, typename KeyT>
double run_workload(Op op, std::size_t n, std::uint64_t seed, BenchOpts const& opts) {
    using namespace ankerl::nanobench;

    // ---- setup: never measured -------------------------------------------------
    auto keys      = make_keys<KeyT>(n, seed);
    auto miss_keys = make_miss_keys<KeyT>(n, seed);

    // Read-only workloads need a populated map to operate on.
    Map readonly;
    if (op == Op::FindHit || op == Op::FindMiss || op == Op::Iterate) {
        readonly.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            (void)readonly.emplace(keys[i], static_cast<int>(i));
        }
    }

    Bench bench;
    bench.title("emhash");
    bench.unit("workload");
    bench.output(nullptr); // keep stdout clean: single mode prints a bare number
    bench.epochs(opts.epochs);
    bench.minEpochIterations(opts.min_epoch_iters);
    bench.maxEpochTime(std::chrono::nanoseconds(static_cast<std::uint64_t>(opts.max_epoch_ms * 1e6)));
    bench.warmup(opts.warmup);

    switch (op) {
        case Op::Insert:
            // Natural growth: no reserve(), so rehash cost is included (realistic).
            bench.run("insert", [&] {
                Map m;
                for (std::size_t i = 0; i < n; ++i) {
                    (void)m.emplace(keys[i], static_cast<int>(i));
                }
                doNotOptimizeAway(m.size());
            });
            break;

        case Op::FindHit:
            bench.run("find_hit", [&] {
                std::size_t hits = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    hits += static_cast<std::size_t>(readonly.find(keys[i]) != readonly.end());
                }
                doNotOptimizeAway(hits);
            });
            break;

        case Op::FindMiss:
            bench.run("find_miss", [&] {
                std::size_t hits = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    hits += static_cast<std::size_t>(readonly.find(miss_keys[i]) != readonly.end());
                }
                doNotOptimizeAway(hits);
            });
            break;

        case Op::Iterate:
            bench.run("iterate", [&] {
                std::size_t sum = 0;
                for (auto const& kv : readonly) {
                    sum += static_cast<std::size_t>(kv.second);
                }
                doNotOptimizeAway(sum);
            });
            break;

        case Op::EraseAll:
            // Includes the build cost; subtract the `insert` workload to derive pure erase.
            bench.run("erase_all", [&] {
                Map m;
                for (std::size_t i = 0; i < n; ++i) {
                    (void)m.emplace(keys[i], static_cast<int>(i));
                }
                for (std::size_t i = 0; i < n; ++i) {
                    m.erase(keys[i]);
                }
                doNotOptimizeAway(m.size());
            });
            break;

        case Op::InsertErase:
            // Steady state: reach n live entries, then n x (erase + insert) keeping n live.
            bench.run("insert_erase", [&] {
                Map m;
                m.reserve(n);
                for (std::size_t i = 0; i < n; ++i) {
                    (void)m.emplace(keys[i], static_cast<int>(i));
                }
                for (std::size_t i = 0; i < n; ++i) {
                    m.erase(keys[i]);
                    (void)m.emplace(miss_keys[i], static_cast<int>(i));
                }
                doNotOptimizeAway(m.size());
            });
            break;
    }

    auto const& r = bench.results().front();
    // nanobench reports Measure::elapsed in *seconds* per op() invocation
    // (see detail::d(Clock::duration) -> std::chrono::duration<double>). Scale to ns.
    double seconds = opts.use_min ? r.minimum(Result::Measure::elapsed) : r.median(Result::Measure::elapsed);
    return seconds * 1e9;
}

} // namespace emhash_bench
