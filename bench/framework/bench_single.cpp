// bench_single.cpp — one binary = one map x one workload, fixed at compile time.
//
// Why this exists: bench_main.cpp registers every map and every workload in a
// single translation unit. That is convenient for a matrix run, but it changes
// inlining, register allocation and code layout relative to a real program that
// uses exactly one map, which is enough to fake or hide sub-10% differences.
// Here the preprocessor includes exactly one map header and this TU instantiates
// exactly one workload (see `run_workload_fixed<O>`), so the resulting binary is
// as close as a benchmark can get to a real user of that map.
//
// Required defines (build_single.sh / build_single.ps1 set all four):
//   BENCH_MAP_INC   header to include, between angle brackets: <emhash/hash_table9.hpp>
//   BENCH_MAP_TMPL  the template, e.g. emhash9::HashMap
//   BENCH_MAP_NAME  the name users pass to --map, e.g. emhash9 (stringified below)
//   BENCH_OP        Insert | FindHit | FindMiss | Iterate | EraseAll | InsertErase
//
// Optional:
//   BENCH_WYHASH=1  hash string keys with the bundled wyhash (emh_wyhash) instead
//                   of std::hash<std::string>. The functor declares
//                   `is_avalanching`, so emhash9 skips its extra mix64 and the
//                   map consumes the raw wyhash output, as it does on emhash8's
//                   -DEMH_WYHASH_HASH=1 path.
//
// CLI (a superset of bench_main.cpp's --single contract, so ab.sh/ab.ps1 work
// unchanged; --map/--op are verified against the compiled-in values):
//
//   bench_single_emhash9_insert --single --map emhash9 --op insert
//       --key string --size 100000 --metric min [--seed N] [--once]
//
// --once runs the workload exactly once with no warmup. Combined with
// -DEMH_BENCH_CALLGRIND=1 it makes callgrind's Ir an exact per-workload count
// (see instr_count.sh).

#ifndef BENCH_MAP_INC
#error "BENCH_MAP_INC is required, e.g. -DBENCH_MAP_INC=<emhash/hash_table9.hpp>"
#endif
#ifndef BENCH_MAP_TMPL
#error "BENCH_MAP_TMPL is required, e.g. -DBENCH_MAP_TMPL=emhash9::HashMap"
#endif
#ifndef BENCH_MAP_NAME
#error "BENCH_MAP_NAME is required, e.g. -DBENCH_MAP_NAME=emhash9"
#endif
#ifndef BENCH_OP
#error "BENCH_OP is required, one of: Insert FindHit FindMiss Iterate EraseAll InsertErase"
#endif

#define ANKERL_NANOBENCH_IMPLEMENT

#include "bench_framework.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include BENCH_MAP_INC

namespace eb = emhash_bench;

namespace {

// Stringify the bare map name so the build scripts never have to pass a quoted
// argument (Windows PowerShell mangles embedded double quotes when invoking a
// native compiler).
#define EMH_BENCH_STR2(x) #x
#define EMH_BENCH_STR(x) EMH_BENCH_STR2(x)
constexpr char const* kMapName = EMH_BENCH_STR(BENCH_MAP_NAME);
#undef EMH_BENCH_STR
#undef EMH_BENCH_STR2

// The single map under test. Exactly one header was included, so this is the
// only container the compiler can see.
template <typename K, typename V> using MapT = BENCH_MAP_TMPL<K, V>;

constexpr eb::Op kOp = eb::Op::BENCH_OP; // NOLINT: BENCH_OP is a token, e.g. FindHit

#if defined(BENCH_WYHASH) && BENCH_WYHASH
// wyhash for string keys. `is_avalanching` is the marker emhash8/emhash9 use to
// mean "this hash already avalanches, skip the extra finalizer".
struct wyhash_hasher {
    using is_avalanching = void;
    std::size_t operator()(std::string const& s) const noexcept { return emh_wyhash(s.data(), s.size(), 0); }
    std::size_t operator()(std::string_view s) const noexcept { return emh_wyhash(s.data(), s.size(), 0); }
    std::size_t operator()(const char* s) const noexcept { return emh_wyhash(s, std::strlen(s), 0); }
};

template <typename K, typename V> struct map_for_key {
    using type = MapT<K, V>;
};
template <typename V> struct map_for_key<std::string, V> {
    // Spell the underlying template out: the MapT alias above only forwards two
    // parameters, and every map in this repo takes HashT as its third.
    using type = BENCH_MAP_TMPL<std::string, V, wyhash_hasher>;
};
template <typename K, typename V> using MapSel = typename map_for_key<K, V>::type;
#else
template <typename K, typename V> using MapSel = MapT<K, V>;
#endif

double run_for_key(eb::KeyKind kk, std::size_t n, std::uint64_t seed, eb::BenchOpts const& opts) {
    switch (kk) {
    case eb::KeyKind::Int32:
        return eb::run_workload_fixed<kOp, MapSel<std::int32_t, int>, std::int32_t>(n, seed, opts);
    case eb::KeyKind::Int64:
        return eb::run_workload_fixed<kOp, MapSel<std::int64_t, int>, std::int64_t>(n, seed, opts);
    case eb::KeyKind::String:
        return eb::run_workload_fixed<kOp, MapSel<std::string, int>, std::string>(n, seed, opts);
    }
    return 0.0;
}

struct Args {
    bool once = false;
    bool use_min = true;
    bool has_map = false;
    bool has_op = false;
    std::string map;
    std::string op;
    std::string key = "int64";
    std::size_t size = 100000;
    std::uint64_t seed = 12345;
    std::size_t key_len = eb::kStringKeyLen;
};

void usage(char const* prog) {
    std::fprintf(stderr,
                 "usage: %s [--single] [--map %s] [--op %s] [--key int32|int64|string]\n"
                 "          [--size N] [--metric min|median] [--seed N] [--key-len N] [--once]\n"
                 "  compiled-in: map=%s op=%s   (--key-len: string bytes, default 32)\n",
                 prog, kMapName, std::string(eb::op_name(kOp)).c_str(), kMapName,
                 std::string(eb::op_name(kOp)).c_str());
}

bool parse_args(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](char const* what) -> char const* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--single") {
            // Accepted for ab.sh/ab.ps1 compatibility; a single-workload binary
            // has nothing to select.
        } else if (arg == "--once") {
            a.once = true;
        } else if (arg == "--map") {
            a.map = next("--map");
            a.has_map = true;
        } else if (arg == "--op") {
            a.op = next("--op");
            a.has_op = true;
        } else if (arg == "--key") {
            a.key = next("--key");
        } else if (arg == "--size") {
            a.size = static_cast<std::size_t>(std::strtoull(next("--size"), nullptr, 10));
        } else if (arg == "--seed") {
            a.seed = std::strtoull(next("--seed"), nullptr, 10);
        } else if (arg == "--key-len") {
            a.key_len = static_cast<std::size_t>(std::strtoull(next("--key-len"), nullptr, 10));
            if (a.key_len < 8) {
                std::fprintf(stderr, "error: --key-len must be >= 8\n");
                return false;
            }
        } else if (arg == "--metric") {
            std::string m = next("--metric");
            if (m == "min") {
                a.use_min = true;
            } else if (m == "median") {
                a.use_min = false;
            } else {
                std::fprintf(stderr, "error: --metric must be min|median\n");
                return false;
            }
        } else if (arg == "--help" || arg == "-h") {
            return false;
        } else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg.c_str());
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Args a;
    if (!parse_args(argc, argv, a)) {
        usage(argv[0]);
        return 2;
    }

    // A binary built for one map/workload must not silently run another: a typo
    // in the A/B script would otherwise compare two different workloads.
    if (a.has_map && a.map != kMapName) {
        std::fprintf(stderr, "error: this binary was built for map '%s', not '%s'\n", kMapName, a.map.c_str());
        return 2;
    }
    if (a.has_op) {
        eb::Op requested{};
        if (!eb::parse_op(a.op, requested)) {
            std::fprintf(stderr, "error: bad --op '%s'\n", a.op.c_str());
            return 2;
        }
        if (requested != kOp) {
            std::fprintf(stderr, "error: this binary was built for op '%s', not '%s'\n",
                         std::string(eb::op_name(kOp)).c_str(), a.op.c_str());
            return 2;
        }
    }

    eb::KeyKind kk{};
    if (!eb::parse_key_kind(a.key, kk)) {
        std::fprintf(stderr, "error: bad --key '%s'\n", a.key.c_str());
        return 2;
    }

    eb::BenchOpts opts;
    opts.use_min = a.use_min;
    opts.once = a.once;
    opts.string_len = a.key_len;

    std::printf("%.3f\n", run_for_key(kk, a.size, a.seed, opts));
    return 0;
}
