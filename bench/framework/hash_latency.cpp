// hash_latency.cpp — dependency-chain latency of the hashes an emhash map can use.
//
// Throughput benchmarks hide hash latency: independent hashes overlap, so a hash
// with a long dependency chain looks fine until it sits on the critical path of
// a random lookup. This measures the chain instead (see the §H3 rule in
// docs/2026-11-23-cpu-level-hash-map-optimizations.md):
//
//   x = hash(key); write x back into key; repeat
//
// so every hash depends on the previous one. The result is deliberately *not*
// used as an array index: `x = hash(keys[x & mask])` would put the array address
// and the key length on the chain and inflate the apparent win of a "faster"
// hash without any effect inside a real map.
//
// The 8-byte store -> load handoff between iterations costs a handful of cycles
// and is identical for every hasher, so the ns/hash differences between rows are
// pure hash latency.
//
// Hashers compared:
//   std::hash<std::string>          the default of every map in this repo
//   emhash9 (std::hash + mix64)     exactly what HashMap<string,int> computes:
//                                   the default hasher does not declare
//                                   `is_avalanching`, so hash_key adds mix64
//   wyhash (emh_wyhash)             the bundled wyhash, i.e. what emhash8's
//                                   -DEMH_WYHASH_HASH=1 string path uses
//
// This tool only measures hash cost. Whether a cheaper hash actually speeds up a
// map is a whole-table question: A/B `bench_single --key string` with and without
// -DBENCH_WYHASH=1 (see README.md).

#define ANKERL_NANOBENCH_IMPLEMENT

#include "bench_framework.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "emhash/hash_table9.hpp" // emhash9::mix64, and config.hpp's emh_wyhash

namespace {

using namespace ankerl::nanobench;

struct std_string_hash {
    std::size_t operator()(std::string const& s) const noexcept { return std::hash<std::string>{}(s); }
};

// What emhash9 computes for a std::string key by default: hash then mix64.
struct emhash9_default_hash {
    std::size_t operator()(std::string const& s) const noexcept {
        return static_cast<std::size_t>(
            emhash9::detail::mix64(static_cast<std::uint64_t>(std::hash<std::string>{}(s))));
    }
};

struct wyhash_string {
    std::size_t operator()(std::string const& s) const noexcept {
        return static_cast<std::size_t>(emh_wyhash(s.data(), s.size(), 0));
    }
};

// One chain iteration per hash; 2048 keeps a single measured sample around 30-100
// microseconds, far above timer resolution even for the slowest hasher/len.
constexpr std::size_t kChain = 2048;

template <typename Hasher> double latency_ns_per_hash(Hasher const& h, std::size_t len) {
    std::string key(len, 'a');

    Bench bench;
    bench.title("hash");
    bench.unit("chain");
    bench.output(nullptr);
    bench.epochs(11);
    bench.warmup(2);
    bench.minEpochIterations(1);
    bench.maxEpochTime(std::chrono::milliseconds(100));
    bench.run("hash_chain", [&] {
        std::uint64_t x = 0;
        for (std::size_t i = 0; i < kChain; ++i) {
            x = static_cast<std::uint64_t>(h(key));
            std::memcpy(key.data(), &x, sizeof(x));
        }
        doNotOptimizeAway(x);
    });

    const auto& r = bench.results().front();
    return r.median(Result::Measure::elapsed) * 1e9 / static_cast<double>(kChain);
}

std::vector<std::size_t> parse_lengths(char const* s) {
    std::vector<std::size_t> out;
    char const* p = s;
    while (*p != '\0') {
        char* end = nullptr;
        unsigned long v = std::strtoul(p, &end, 10);
        if (end == p || v < 8) {
            std::fprintf(stderr, "error: bad length list '%s' (lengths must be >= 8)\n", s);
            std::exit(2);
        }
        out.push_back(static_cast<std::size_t>(v));
        p = (*end == ',') ? end + 1 : end;
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::size_t> lengths = {8, 16, 24, 64};

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--lengths" && i + 1 < argc) {
            lengths = parse_lengths(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::printf("usage: %s [--lengths 8,16,24,64]\n", argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg.c_str());
            return 2;
        }
    }

    std::printf("dependency-chain hash latency, ns per hash (chain depth %zu, key mutated in place)\n", kChain);
    std::printf("%-28s", "hasher");
    for (auto len : lengths) {
        std::printf(" %8s", ("len=" + std::to_string(len)).c_str());
    }
    std::printf("\n%s\n", std::string(28 + 9 * lengths.size(), '-').c_str());

    struct Entry {
        char const* name;
        double (*fn)(std::size_t);
    };
    // Monomorphized once per hasher; a static table keeps the loop below readable.
    Entry const entries[] = {
        {"std::hash<string>", [](std::size_t len) { return latency_ns_per_hash(std_string_hash{}, len); }},
        {"emhash9 (hash + mix64)", [](std::size_t len) { return latency_ns_per_hash(emhash9_default_hash{}, len); }},
        {"wyhash (emh_wyhash)", [](std::size_t len) { return latency_ns_per_hash(wyhash_string{}, len); }},
    };

    for (auto const& e : entries) {
        std::printf("%-28s", e.name);
        for (auto len : lengths) {
            std::printf(" %8.2f", e.fn(len));
        }
        std::printf("\n");
        std::fflush(stdout);
    }
    return 0;
}
