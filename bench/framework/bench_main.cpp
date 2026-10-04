// bench_main.cpp — CLI driver for the emhash benchmark framework.
//
// This is the only TU that defines ANKERL_NANOBENCH_IMPLEMENT.
//
// Modes:
//   --suite    run a matrix of {container x key x size x workload}, print a table (+ optional JSON)
//   --single   run exactly one workload and print a bare number on stdout (for the A/B runner)
//
// Container registry covers emhash5/6/7/8 and emilib1/2/3/4 maps. boost/absl reference
// baselines are compiled in only when the corresponding headers are available.

#define ANKERL_NANOBENCH_IMPLEMENT

#include "bench_framework.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ---- containers under test -------------------------------------------------
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihmap4.hpp"

// ---- optional reference baselines -----------------------------------------
// Guarded so a missing third-party tree degrades to "skipped" instead of a build error.
#if defined(EMH_BENCH_HAVE_BOOST)
#include <boost/unordered/unordered_flat_map.hpp>
#endif
#if defined(EMH_BENCH_HAVE_ABSL)
#include <absl/container/flat_hash_map.h>
#endif

namespace eb = emhash_bench;

namespace {

using RunFn = double (*)(eb::Op, std::size_t, std::uint64_t, eb::BenchOpts const&);

// One registry entry: a runtime name -> a compile-time (Map, KeyT) instantiation.
struct Entry {
    const char* name;
    eb::KeyKind key_kind;
    RunFn       run;
};

template <template <typename, typename> class MapTmpl, typename KeyT>
double run_t(eb::Op op, std::size_t n, std::uint64_t seed, eb::BenchOpts const& opts) {
    return eb::run_workload<MapTmpl<KeyT, int>, KeyT>(op, n, seed, opts);
}

// Alias templates give every container a uniform <K, V> shape.
template <typename K, typename V> using map5_t  = emhash5::HashMap<K, V>;
template <typename K, typename V> using map6_t  = emhash6::HashMap<K, V>;
template <typename K, typename V> using map7_t  = emhash7::HashMap<K, V>;
template <typename K, typename V> using map8_t  = emhash8::HashMap<K, V>;
template <typename K, typename V> using imap1_t = emilib::HashMap<K, V>;
template <typename K, typename V> using imap2_t = emilib2::HashMap<K, V>;
template <typename K, typename V> using imap3_t = emilib3::HashMap<K, V>;
template <typename K, typename V> using imap4_t = emilib4::HashMap<K, V>;
#if defined(EMH_BENCH_HAVE_BOOST)
template <typename K, typename V> using bmap_t = boost::unordered_flat_map<K, V>;
#endif
#if defined(EMH_BENCH_HAVE_ABSL)
template <typename K, typename V> using amap_t = absl::flat_hash_map<K, V>;
#endif

// Expand one container template into one entry per key type.
template <template <typename, typename> class MapTmpl>
void add_container(std::vector<Entry>& out, const char* name) {
    out.push_back({name, eb::KeyKind::Int32, &run_t<MapTmpl, std::int32_t>});
    out.push_back({name, eb::KeyKind::Int64, &run_t<MapTmpl, std::int64_t>});
    out.push_back({name, eb::KeyKind::String, &run_t<MapTmpl, std::string>});
}

std::vector<Entry> const& registry() {
    static std::vector<Entry> const entries = [] {
        std::vector<Entry> v;
        add_container<map5_t>(v, "emhash5");
        add_container<map6_t>(v, "emhash6");
        add_container<map7_t>(v, "emhash7");
        add_container<map8_t>(v, "emhash8");
        add_container<imap1_t>(v, "emilib1");
        add_container<imap2_t>(v, "emilib2");
        add_container<imap3_t>(v, "emilib3");
        add_container<imap4_t>(v, "emilib4");
#if defined(EMH_BENCH_HAVE_BOOST)
        add_container<bmap_t>(v, "boost");
#endif
#if defined(EMH_BENCH_HAVE_ABSL)
        add_container<amap_t>(v, "absl");
#endif
        return v;
    }();
    return entries;
}

RunFn find_runner(std::string const& map_name, eb::KeyKind kk) {
    for (auto const& e : registry()) {
        if (map_name == e.name && e.key_kind == kk) {
            return e.run;
        }
    }
    return nullptr;
}

std::vector<std::string> split_csv(std::string const& s) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos <= s.size()) {
        auto comma = s.find(',', pos);
        if (comma == std::string::npos) {
            comma = s.size();
        }
        if (comma > pos) {
            out.push_back(s.substr(pos, comma - pos));
        }
        pos = comma + 1;
    }
    return out;
}

std::vector<std::size_t> parse_sizes(std::string const& s) {
    std::vector<std::size_t> out;
    for (auto const& tok : split_csv(s)) {
        out.push_back(static_cast<std::size_t>(std::strtoull(tok.c_str(), nullptr, 10)));
    }
    return out;
}

void print_usage(char const* prog) {
    std::fprintf(stderr,
                 "usage:\n"
                 "  %s --suite [--maps a,b] [--keys k1,k2] [--sizes n1,n2] [--ops o1,o2]\n"
                 "            [--json FILE] [--metric min|median] [--seed N]\n"
                 "  %s --single --op OP --map NAME --key KIND --size N\n"
                 "            [--metric min|median] [--seed N]\n"
                 "\n"
                 "  ops   : insert find_hit find_miss iterate erase_all insert_erase\n"
                 "  keys  : int32 int64 string\n"
                 "  maps  : emhash5 emhash6 emhash7 emhash8 emilib1 emilib2 emilib3 emilib4\n",
                 prog, prog);
}

// ---- argument state --------------------------------------------------------
struct Args {
    bool suite  = false;
    bool single = false;

    std::string op;
    std::string map;
    std::string key;
    std::size_t size = 0;

    std::vector<std::string> maps;
    std::vector<std::string> keys;
    std::vector<std::size_t> sizes = {1000, 10000, 100000};
    std::vector<std::string> ops;

    std::string json;
    bool        use_min = false;
    std::uint64_t seed  = 12345;
};

bool parse_args(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--suite") {
            a.suite = true;
        } else if (arg == "--single") {
            a.single = true;
        } else if (arg == "--op") {
            a.op = next("--op");
        } else if (arg == "--map") {
            a.map = next("--map");
        } else if (arg == "--key") {
            a.key = next("--key");
        } else if (arg == "--size") {
            a.size = static_cast<std::size_t>(std::strtoull(next("--size"), nullptr, 10));
        } else if (arg == "--maps") {
            a.maps = split_csv(next("--maps"));
        } else if (arg == "--keys") {
            a.keys = split_csv(next("--keys"));
        } else if (arg == "--sizes") {
            a.sizes = parse_sizes(next("--sizes"));
        } else if (arg == "--ops") {
            a.ops = split_csv(next("--ops"));
        } else if (arg == "--json") {
            a.json = next("--json");
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
        } else if (arg == "--seed") {
            a.seed = std::strtoull(next("--seed"), nullptr, 10);
        } else if (arg == "--help" || arg == "-h") {
            return false;
        } else {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg.c_str());
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// --single: run one workload, print a bare number (the A/B runner's contract)
// ---------------------------------------------------------------------------
int run_single(Args const& a) {
    eb::Op op{};
    if (!eb::parse_op(a.op, op)) {
        std::fprintf(stderr, "error: bad --op '%s'\n", a.op.c_str());
        return 2;
    }
    eb::KeyKind kk{};
    if (!eb::parse_key_kind(a.key, kk)) {
        std::fprintf(stderr, "error: bad --key '%s'\n", a.key.c_str());
        return 2;
    }
    RunFn fn = find_runner(a.map, kk);
    if (fn == nullptr) {
        std::fprintf(stderr, "error: unknown --map '%s' for key '%s'\n", a.map.c_str(), a.key.c_str());
        return 2;
    }

    eb::BenchOpts opts;
    opts.use_min = a.use_min;

    double ns = fn(op, a.size, a.seed, opts);
    std::printf("%.3f\n", ns);
    return 0;
}

// ---------------------------------------------------------------------------
// --suite: full matrix
// ---------------------------------------------------------------------------
struct Row {
    std::string map;
    eb::KeyKind key_kind;
    eb::Op      op;
    std::size_t size;
    double      ns;
};

int run_suite(Args const& a) {
    // Resolve selections, defaulting to everything available.
    std::vector<std::string> maps = a.maps;
    if (maps.empty()) {
        for (auto const& e : registry()) {
            if (std::find(maps.begin(), maps.end(), e.name) == maps.end()) {
                maps.emplace_back(e.name);
            }
        }
    }

    std::vector<eb::KeyKind> keys;
    if (a.keys.empty()) {
        keys = {eb::KeyKind::Int32, eb::KeyKind::Int64, eb::KeyKind::String};
    } else {
        for (auto const& k : a.keys) {
            eb::KeyKind kk{};
            if (!eb::parse_key_kind(k, kk)) {
                std::fprintf(stderr, "error: bad --keys entry '%s'\n", k.c_str());
                return 2;
            }
            keys.push_back(kk);
        }
    }

    std::vector<eb::Op> ops;
    if (a.ops.empty()) {
        ops = {eb::Op::Insert,  eb::Op::FindHit, eb::Op::FindMiss,
               eb::Op::Iterate, eb::Op::EraseAll, eb::Op::InsertErase};
    } else {
        for (auto const& o : a.ops) {
            eb::Op op{};
            if (!eb::parse_op(o, op)) {
                std::fprintf(stderr, "error: bad --ops entry '%s'\n", o.c_str());
                return 2;
            }
            ops.push_back(op);
        }
    }

    eb::BenchOpts opts;
    opts.use_min = a.use_min;

    std::vector<Row> rows;
    std::printf("%-10s %-7s %-13s %9s %14s\n", "map", "key", "op", "size", "total_ns");
    std::printf("%s\n", std::string(58, '-').c_str());

    for (auto const& map_name : maps) {
        for (auto kk : keys) {
            RunFn fn = find_runner(map_name, kk);
            if (fn == nullptr) {
                continue;
            }
            for (auto n : a.sizes) {
                for (auto op : ops) {
                    double ns = fn(op, n, a.seed, opts);
                    rows.push_back({map_name, kk, op, n, ns});
                    std::printf("%-10s %-7s %-13s %9zu %14.0f\n", map_name.c_str(),
                                std::string(eb::key_kind_name(kk)).c_str(),
                                std::string(eb::op_name(op)).c_str(), n, ns);
                    std::fflush(stdout);
                }
            }
        }
    }

    // Derived pure-erase estimate: erase_all includes the build cost, which is what
    // the `insert` workload measures under identical conditions.
    std::printf("\n%s\n", std::string(58, '-').c_str());
    std::printf("%-10s %-7s %-13s %9s %14s\n", "map", "key", "derived", "size", "total_ns");
    for (auto const& map_name : maps) {
        for (auto kk : keys) {
            for (auto n : a.sizes) {
                auto find_row = [&](eb::Op op) -> Row const* {
                    for (auto const& r : rows) {
                        if (r.map == map_name && r.key_kind == kk && r.op == op && r.size == n) {
                            return &r;
                        }
                    }
                    return nullptr;
                };
                auto const* ea = find_row(eb::Op::EraseAll);
                auto const* ins = find_row(eb::Op::Insert);
                if (ea != nullptr && ins != nullptr) {
                    std::printf("%-10s %-7s %-13s %9zu %14.0f\n", map_name.c_str(),
                                std::string(eb::key_kind_name(kk)).c_str(), "erase(=ea-ins)", n,
                                ea->ns - ins->ns);
                }
            }
        }
    }

    if (!a.json.empty()) {
        FILE* f = std::fopen(a.json.c_str(), "w");
        if (f == nullptr) {
            std::fprintf(stderr, "error: cannot write '%s'\n", a.json.c_str());
            return 1;
        }
        std::fprintf(f, "{\n  \"metric\": \"%s\",\n  \"seed\": %llu,\n  \"results\": [\n",
                     a.use_min ? "min" : "median", static_cast<unsigned long long>(a.seed));
        for (std::size_t i = 0; i < rows.size(); ++i) {
            auto const& r = rows[i];
            std::fprintf(f,
                         "    {\"map\": \"%s\", \"key\": \"%s\", \"op\": \"%s\", \"size\": %zu, "
                         "\"total_ns\": %.3f}%s\n",
                         r.map.c_str(), std::string(eb::key_kind_name(r.key_kind)).c_str(),
                         std::string(eb::op_name(r.op)).c_str(), r.size, r.ns,
                         i + 1 == rows.size() ? "" : ",");
        }
        std::fprintf(f, "  ]\n}\n");
        std::fclose(f);
        std::fprintf(stderr, "JSON written to %s\n", a.json.c_str());
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Args a;
    if (!parse_args(argc, argv, a)) {
        print_usage(argv[0]);
        return 2;
    }
    if (a.single == a.suite) {
        print_usage(argv[0]);
        return 2;
    }
    return a.single ? run_single(a) : run_suite(a);
}
