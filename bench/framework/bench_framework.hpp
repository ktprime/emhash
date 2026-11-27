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

// Instruction counting (instr_count.sh): with `--once` the workload runs exactly
// one iteration, and instrumentation is switched on after the setup phase and
// off right after the measured call. Zeroing alone is not enough: the map's
// destructor (and for string keys, freeing every key) runs after the workload and
// can dominate the total -- destroying 100k std::string keys costs more
// instructions than the 100k finds being measured.
//
// Build with -DEMH_BENCH_CALLGRIND=1 under `valgrind --tool=callgrind`.
// Note: valgrind 3.18 writes a bogus `summary:` field when instrumentation is
// stopped this way, so instr_count.sh reads the trailing `totals:` line instead.
#if defined(EMH_BENCH_CALLGRIND)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#define EMH_BENCH_COUNT_BEGIN()                                                                                        \
    do {                                                                                                               \
        CALLGRIND_ZERO_STATS;                                                                                          \
        CALLGRIND_START_INSTRUMENTATION;                                                                               \
    } while (0)
#define EMH_BENCH_COUNT_END() CALLGRIND_STOP_INSTRUMENTATION
#else
#error "EMH_BENCH_CALLGRIND requires the valgrind headers (e.g. apt install valgrind)"
#endif
#else
#define EMH_BENCH_COUNT_BEGIN() ((void)0)
#define EMH_BENCH_COUNT_END() ((void)0)
#endif

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
    // Fixed-size churn: erase one key, insert a disjoint fresh key, repeat n times.
    // Measures the write path only (the build is part of the workload).
    Churn,
    // Build n keys, run one full churn round *outside* the measurement, then look up
    // a third disjoint key set. This is the post-churn miss cost that move_home /
    // counter designs are supposed to improve.
    ChurnFindMiss,
};

inline constexpr std::string_view op_name(Op o) noexcept {
    switch (o) {
        case Op::Insert:        return "insert";
        case Op::FindHit:       return "find_hit";
        case Op::FindMiss:      return "find_miss";
        case Op::Iterate:       return "iterate";
        case Op::EraseAll:      return "erase_all";
        case Op::InsertErase:   return "insert_erase";
        case Op::Churn:         return "churn";
        case Op::ChurnFindMiss: return "churn_find_miss";
    }
    return "unknown";
}

inline bool parse_op(std::string_view s, Op& out) noexcept {
    for (auto o : {Op::Insert, Op::FindHit, Op::FindMiss, Op::Iterate, Op::EraseAll, Op::InsertErase,
                   Op::Churn, Op::ChurnFindMiss}) {
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
// Hit, miss and churn key sets are disjoint *by construction* rather than by
// relying on different seeds (which for a 32-bit key space and n=1e6 would still
// collide). Integer keys are split into three non-overlapping ranges; string keys
// encode the index with different offsets.
//
// String keys are 32 bytes so that they exceed typical SSO limits and therefore
// exercise heap allocation on insert, matching real-world long-key workloads.
// ---------------------------------------------------------------------------

inline constexpr std::size_t kStringKeyLen = 32;

template <typename KeyT>
std::vector<KeyT> make_keys(std::size_t n, std::uint64_t seed, std::size_t len = kStringKeyLen);

// Hit keys: live in the lowest third of the value range.
template <>
inline std::vector<std::int32_t> make_keys<std::int32_t>(std::size_t n, std::uint64_t seed, std::size_t len) {
    (void)len;
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int32_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys.push_back(static_cast<std::int32_t>(rng() & 0x1FFFFFFFull)); // [0, 2^29)
    }
    return keys;
}

template <>
inline std::vector<std::int64_t> make_keys<std::int64_t>(std::size_t n, std::uint64_t seed, std::size_t len) {
    (void)len;
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int64_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys.push_back(static_cast<std::int64_t>(rng() & 0x1FFFFFFFFFFFFFFFull)); // [0, 2^61)
    }
    return keys;
}

// Deterministic 64-bit mixer for string key material. Encoding a counter directly
// (`seed + i`) makes short string keys sequential integers, so the low bits that
// a hash's bucket index is built from are highly structured; at 8 bytes that turns
// a hash-latency comparison into a hash-quality comparison and lets one map's
// probe scheme look better than another's for reasons unrelated to hash cost.
inline std::uint64_t mix_key_material(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

template <>
inline std::vector<std::string> make_keys<std::string>(std::size_t n, std::uint64_t seed, std::size_t len) {
    if (len < sizeof(std::uint64_t)) {
        len = sizeof(std::uint64_t); // the encoded index must fit
    }
    std::vector<std::string> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        // Vary only the leading 8 bytes; the remaining bytes are filler so the key
        // reaches the requested length (32 by default: heap-allocated). The 8-byte
        // prefix is mixed so the key material is pseudorandom, not a counter.
        std::uint64_t x = mix_key_material(seed + i);
        std::string s(len, '\0');
        std::memcpy(s.data(), &x, sizeof(x));
        std::memset(s.data() + sizeof(x), static_cast<int>('a' + (x % 26)), len - sizeof(x));
        keys.push_back(std::move(s));
    }
    return keys;
}

// Miss keys: guaranteed absent from the corresponding hit-key set.
template <typename KeyT>
std::vector<KeyT> make_miss_keys(std::size_t n, std::uint64_t seed, std::size_t len = kStringKeyLen);

template <>
inline std::vector<std::int32_t> make_miss_keys<std::int32_t>(std::size_t n, std::uint64_t seed, std::size_t len) {
    (void)len;
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
inline std::vector<std::int64_t> make_miss_keys<std::int64_t>(std::size_t n, std::uint64_t seed, std::size_t len) {
    (void)len;
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
inline std::vector<std::string> make_miss_keys<std::string>(std::size_t n, std::uint64_t seed, std::size_t len) {
    // Offset the encoded index beyond the hit-key range so the sets cannot overlap.
    return make_keys<std::string>(n, seed + 0x100000000ull, len);
}

// Churn keys: the fresh keys inserted while the original hit keys are erased.
// They occupy the middle integer range, so hit/churn/miss are pairwise disjoint.
template <typename KeyT>
std::vector<KeyT> make_churn_keys(std::size_t n, std::uint64_t seed, std::size_t len = kStringKeyLen);

template <>
inline std::vector<std::int32_t> make_churn_keys<std::int32_t>(std::size_t n, std::uint64_t seed, std::size_t len) {
    (void)len;
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int32_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        // [2^29, 2^30) — between the hit range and the miss range.
        keys.push_back(static_cast<std::int32_t>((rng() & 0x1FFFFFFFull) + (1ull << 29)));
    }
    return keys;
}

template <>
inline std::vector<std::int64_t> make_churn_keys<std::int64_t>(std::size_t n, std::uint64_t seed, std::size_t len) {
    (void)len;
    ankerl::nanobench::Rng rng(seed);
    std::vector<std::int64_t> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        // [2^61, 2^62) — between the hit range and the miss range.
        keys.push_back(static_cast<std::int64_t>((rng() & 0x1FFFFFFFFFFFFFFFull) + (1ull << 61)));
    }
    return keys;
}

template <>
inline std::vector<std::string> make_churn_keys<std::string>(std::size_t n, std::uint64_t seed, std::size_t len) {
    return make_keys<std::string>(n, seed + 0x200000000ull, len);
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
    // Run the workload exactly once, with no warmup. Used for instruction counting:
    // a single invocation makes callgrind's Ir an exact per-workload count instead
    // of an average over an unknown number of epochs.
    bool          once            = false;
    // Byte length of generated string keys. 32 by default (heap-allocated); shorter
    // values exercise SSO and change how much of the hash cost is per-byte.
    std::size_t   string_len      = kStringKeyLen;
};

// ---------------------------------------------------------------------------
// Workload runner
//
// `run_workload_fixed<O>` takes the workload as a template argument, so a
// translation unit that only calls it with one O instantiates exactly one
// workload body. `bench_single.cpp` relies on this to build "one binary = one
// map x one workload"; the runtime-`op` wrapper below is what bench_main.cpp's
// matrix driver uses.
//
// Returns the total elapsed nanoseconds for one invocation of the workload
// (median across epochs, or minimum when `opts.use_min` is set).
// ---------------------------------------------------------------------------

template <Op O, typename Map, typename KeyT>
double run_workload_fixed(std::size_t n, std::uint64_t seed, BenchOpts const& opts) {
    using namespace ankerl::nanobench;

    // ---- setup: never measured -------------------------------------------------
    auto keys       = make_keys<KeyT>(n, seed, opts.string_len);
    auto miss_keys  = make_miss_keys<KeyT>(n, seed, opts.string_len);
    auto churn_keys = make_churn_keys<KeyT>(n, seed, opts.string_len);

    // Read-only workloads need a populated map to operate on.
    Map readonly;
    if constexpr (O == Op::FindHit || O == Op::FindMiss || O == Op::Iterate || O == Op::ChurnFindMiss) {
        readonly.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            (void)readonly.emplace(keys[i], static_cast<int>(i));
        }
        // Put the table into the post-churn state before measuring: one full
        // erase-old/insert-fresh round at constant size.
        if constexpr (O == Op::ChurnFindMiss) {
            for (std::size_t i = 0; i < n; ++i) {
                readonly.erase(keys[i]);
                (void)readonly.emplace(churn_keys[i], static_cast<int>(i));
            }
        }
    }

    Bench bench;
    bench.title("emhash");
    bench.unit("workload");
    bench.output(nullptr); // keep stdout clean: single mode prints a bare number
    if (opts.once) {
        // Exactly one invocation, no warmup: the instruction-count path.
        bench.epochs(1);
        bench.warmup(0);
        bench.epochIterations(1);
    } else {
        bench.epochs(opts.epochs);
        bench.minEpochIterations(opts.min_epoch_iters);
        bench.maxEpochTime(std::chrono::nanoseconds(static_cast<std::uint64_t>(opts.max_epoch_ms * 1e6)));
        bench.warmup(opts.warmup);
    }

    // Setup is done; from here on the counters measure the workload body only.
    EMH_BENCH_COUNT_BEGIN();

    if constexpr (O == Op::Insert) {
        // Natural growth: no reserve(), so rehash cost is included (realistic).
        bench.run("insert", [&] {
            Map m;
            for (std::size_t i = 0; i < n; ++i) {
                (void)m.emplace(keys[i], static_cast<int>(i));
            }
            doNotOptimizeAway(m.size());
        });
    } else if constexpr (O == Op::FindHit) {
        bench.run("find_hit", [&] {
            std::size_t hits = 0;
            for (std::size_t i = 0; i < n; ++i) {
                hits += static_cast<std::size_t>(readonly.find(keys[i]) != readonly.end());
            }
            doNotOptimizeAway(hits);
        });
    } else if constexpr (O == Op::FindMiss) {
        bench.run("find_miss", [&] {
            std::size_t hits = 0;
            for (std::size_t i = 0; i < n; ++i) {
                hits += static_cast<std::size_t>(readonly.find(miss_keys[i]) != readonly.end());
            }
            doNotOptimizeAway(hits);
        });
    } else if constexpr (O == Op::Iterate) {
        bench.run("iterate", [&] {
            std::size_t sum = 0;
            for (auto const& kv : readonly) {
                sum += static_cast<std::size_t>(kv.second);
            }
            doNotOptimizeAway(sum);
        });
    } else if constexpr (O == Op::EraseAll) {
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
    } else if constexpr (O == Op::InsertErase) {
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
    } else if constexpr (O == Op::Churn) {
        // Natural growth to n, then a full fixed-size churn round with fresh keys.
        bench.run("churn", [&] {
            Map m;
            for (std::size_t i = 0; i < n; ++i) {
                (void)m.emplace(keys[i], static_cast<int>(i));
            }
            for (std::size_t i = 0; i < n; ++i) {
                m.erase(keys[i]);
                (void)m.emplace(churn_keys[i], static_cast<int>(i));
            }
            doNotOptimizeAway(m.size());
        });
    } else if constexpr (O == Op::ChurnFindMiss) {
        // readonly was built and churned in setup; measure only the miss probes.
        bench.run("churn_find_miss", [&] {
            std::size_t hits = 0;
            for (std::size_t i = 0; i < n; ++i) {
                hits += static_cast<std::size_t>(readonly.find(miss_keys[i]) != readonly.end());
            }
            doNotOptimizeAway(hits);
        });
    }

    EMH_BENCH_COUNT_END();

    auto const& r = bench.results().front();
    // nanobench reports Measure::elapsed in *seconds* per op() invocation
    // (see detail::d(Clock::duration) -> std::chrono::duration<double>). Scale to ns.
    double seconds = opts.use_min ? r.minimum(Result::Measure::elapsed) : r.median(Result::Measure::elapsed);
    return seconds * 1e9;
}

template <typename Map, typename KeyT>
double run_workload(Op op, std::size_t n, std::uint64_t seed, BenchOpts const& opts) {
    switch (op) {
        case Op::Insert:      return run_workload_fixed<Op::Insert, Map, KeyT>(n, seed, opts);
        case Op::FindHit:     return run_workload_fixed<Op::FindHit, Map, KeyT>(n, seed, opts);
        case Op::FindMiss:    return run_workload_fixed<Op::FindMiss, Map, KeyT>(n, seed, opts);
        case Op::Iterate:     return run_workload_fixed<Op::Iterate, Map, KeyT>(n, seed, opts);
        case Op::EraseAll:    return run_workload_fixed<Op::EraseAll, Map, KeyT>(n, seed, opts);
        case Op::InsertErase: return run_workload_fixed<Op::InsertErase, Map, KeyT>(n, seed, opts);
        case Op::Churn:         return run_workload_fixed<Op::Churn, Map, KeyT>(n, seed, opts);
        case Op::ChurnFindMiss: return run_workload_fixed<Op::ChurnFindMiss, Map, KeyT>(n, seed, opts);
    }
    return 0.0;
}

} // namespace emhash_bench
