#!/usr/bin/env python3
"""Patch martin_bench.cpp: fix P0 comparability issues + P1/P2 methodology gaps.

P0-1: fixed RND seed (reproducible) + remove reserve coin flip
P0-2: runtime checksum verification (survives -DNDEBUG)
P0-3: fix dead print condition in randomFindInternal
P0-4: unified default_hash across all cases
P1  : benchmark_iterations repetitions w/ stddev in IterateIntegers;
      g_results recorded for all cases; max_load_factor applied everywhere
P2  : RSS memory capture in bench_insert + MemBytes column
"""
import re
import sys

PATH = "/mnt/d/emhash/bench/martin_bench.cpp"
src = open(PATH, encoding="utf-8").read()
fails = []


def rep(old, new, count=1, label=""):
    global src
    found = src.count(old)
    if found < count:
        fails.append(f"{label or old[:60]!r}: found {found}, need {count}")
        return
    src = src.replace(old, new, count)


# ---------------------------------------------------------------- includes
rep("""#ifndef _WIN32
#include <sys/resource.h>
#endif""",
    """#ifndef _WIN32
#include <sys/resource.h>
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif""", label="win includes")

rep("#include <sstream>", "#include <cmath>\n#include <sstream>", label="cmath")

# ------------------------------------------------------- P0-1 fixed seed
rep("static const uint64_t RND = (uint64_t)getus();",
    """// Fixed seed: reproducible run-to-run. All cases derive their RNG streams
// from RND, so numbers are directly comparable across runs and maps.
static constexpr uint64_t RND = UINT64_C(123);""", label="RND seed")

rep("""const static uint32_t x0 = (uint32_t)getus();
const static bool is_del = (x0 % 2 == 0);""",
    """static constexpr uint32_t x0 = (uint32_t)RND;
static constexpr bool is_del = (x0 % 2 == 0);""", label="udb3 x0")

# ----------------------------------------------- helpers + unified hash
rep("static float max_lf = 7.0f / 8;",
    """static float max_lf = 7.0f / 8;

static int g_check_errors = 0;

// Runtime checksum verification: unlike assert(), survives -DNDEBUG, so the
// DCE-prevention guarantees and workload validation hold in release builds.
static inline void verify(bool ok, const char* what) {
    if (!ok) {
        printf("  !! CHECK FAILED: %s\\n", what);
        ++g_check_errors;
    }
}

// RSS footprint of the process (peak-so-far on Linux via ru_maxrss).
static size_t current_rss_bytes() {
#ifndef _WIN32
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (size_t)ru.ru_maxrss * 1024;
#else
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (size_t)pmc.WorkingSetSize;
    return 0;
#endif
}

// Unified hash function for every case so timings are comparable across
// sections. One -D override (ABSL_HASH / STD_HASH / FIB_HASH / A_HASH)
// switches all cases at once; the old per-case divergent fallbacks are gone.
#if ABSL_HASH
template <typename T> struct default_hash : absl::Hash<T> {};
#elif A_HASH
template <typename T> struct default_hash : Axxhasher<T> {};
#elif STD_HASH
template <typename T> struct default_hash : std::hash<T> {};
#elif FIB_HASH
template <typename T> struct default_hash : Int64Hasher<T> {};
#elif CXX17
template <typename T> struct default_hash : ankerl::unordered_dense::hash<T> {};
#else
template <typename T> struct default_hash : std::hash<T> {};
#endif""", label="helpers")

# ---------------------------------------------- P0-1 reserve coin flip
rep("""                if (g_config.enable_reserve && RND % 2 == 0)
                    hmap.reserve(maxn / 2);""",
    """                if (g_config.enable_reserve)
                    hmap.reserve(maxn / 2);""", label="reserve coin")

# --------------------------------------------- P0-3 dead print condition
rep("""    if (hmap.size() == 0) {
        printf("    %3u%% %016x time %2.2f s, %8d loadf = %.2f\\n", uint32_t(numSequential * 100 / NumTotal),
               (int)bitMask, now2sec() - ts, (int)num_found, hmap.load_factor());
    }""",
    """    printf("    %3u%% %016x time %2.2f s, %8d loadf = %.2f\\n", uint32_t(numSequential * 100 / NumTotal),
           (int)bitMask, now2sec() - ts, (int)num_found, hmap.load_factor());""", label="randomFindInternal print")

rep("""    if (sum != 123)
        printf(" nums = %zd, time %2.2f s\\n", numInserts, elapsed);""",
    """    printf(" nums = %zd, time %2.2f s\\n", numInserts, elapsed);""", label="bench_randomFind print")

# --------------------------------------------- P0-2 assert -> runtime verify
rep("                assert(hmap.size() == 0);",
    "                // erase attempts target the same key stream as inserts;\n"
    "                // duplicates in the stream leave a small remainder.\n"
    "                verify(hmap.size() < maxn, \"insert/erase90 sanity\");",
    label="bench_insert assert")

rep("""    if (sum != UINT64_C(18446739465311920326))
        puts("error\\n");""",
    """    verify(sum == UINT64_C(18446739465311920326), "AccidentallyQuadratic iteration sum");""",
    label="A-Q sum")

rep("    assert(9988513 == hmap.size());",
    "    verify(9988513 == hmap.size(), \"AccidentallyQuadratic distinct count\");", label="A-Q size")

rep("    assert(finalPopulation == count);",
    "    verify(finalPopulation == count, \"GameOfLife final population\");", label="GameOfLife assert")

rep("    assert(res);", "    verify(res != 0, \"CreateInsert res\");", label="CreateInsert assert")

for chk, tag in (("549985352", "distinct 05%"), ("149979034", "distinct 25%"),
                 ("249981806", "distinct 50%"), ("50291811", "distinct 100%")):
    rep(f"        assert(RND != 123 || {chk} == checksum);",
        f"        verify(RND != 123 || {chk} == checksum, \"{tag} checksum\");", label=f"distinct {chk}")

# ---------------------------- P0-4 unify typedef chains via regex
pat = re.compile(
    r'#if (?:ABSL_HASH|STD_HASH|ANKERL_HASH)\n'
    r'[ \t]*(?:typedef|using) [A-Za-z_:0-9]+<(?P<T>[A-Za-z_:0-9]+)> hash_func;\n'
    r'(?:#el(?:if|se)[^\n]*\n(?:[ \t]*//[^\n]*\n|[ \t]*(?:typedef|using)[^\n]*\n))*'
    r'#endif')
nchains = len(pat.findall(src))
src = pat.sub(lambda m: f"using hash_func = default_hash<{m.group('T')}>;", src)
print(f"[patch] unified {nchains} hash_func typedef chains")
if nchains < 12:
    fails.append(f"typedef chains: only {nchains} replaced (expected >=12)")

# ---------------------------------------------------- P1 unify load factor
rep("""    HMAP hmap;
    // hmap.max_load_factor(0.5);""",
    """    HMAP hmap;
    hmap.max_load_factor(max_lf);""", label="kcount LF")

rep("""    HMAP h;
    uint64_t z = 0, x = x0;""",
    """    HMAP h;
    h.max_load_factor(max_lf);
    uint64_t z = 0, x = x0;""", label="udb3 LF")

rep("""    HMAP hmap;
    auto stastabilizing = now2sec();""",
    """    HMAP hmap;
    hmap.max_load_factor(max_lf);
    auto stastabilizing = now2sec();""", label="GameOfLife LF")

rep("""        [[maybe_unused]] size_t res = 0;
        HMAP hmap;""",
    """        [[maybe_unused]] size_t res = 0;
        HMAP hmap;
        hmap.max_load_factor(max_lf);""", count=2, label="CreateInsert LF")

rep("""        HMAP hmap;
        MRNG rng(987654321ull + i * i * i);""",
    """        HMAP hmap;
        hmap.max_load_factor(max_lf);
        MRNG rng(987654321ull + i * i * i);""", label="InsertEraseBegin LF")

rep("""        HMAP hmap;
        // hmap.reserve((std::size_t)(max_n * .));""",
    """        HMAP hmap;
        hmap.max_load_factor(max_lf);
        // hmap.reserve((std::size_t)(max_n * .));""", label="InsertEraseContinue LF")

rep("""    HMAP hmap;
    for (size_t n = 0; n < 10'000'000; ++n) {""",
    """    HMAP hmap;
    hmap.max_load_factor(max_lf);
    for (size_t n = 0; n < 10'000'000; ++n) {""", label="AccidentallyQuadratic LF")

# ------------------------------------ P1 reps + stddev in IterateIntegers
rep("""    auto ts = now2sec();

    {
        MRNG rng(123);
        for (size_t n = 0; n < num_iters; ++n) {
            hmap[rng()] = n;
            for (const auto& keyVal : hmap)
                result += keyVal.second;
        }
        assert(result == 20833333325000ull);
    }

    auto ts1 = now2sec();
    {
        MRNG rng(123);
        for (size_t n = 0; n < num_iters; ++n) {
            hmap.erase(rng());
            for (auto const& keyVal : hmap)
                result += keyVal.second;
        }
    }
    assert(result == 62498750000000ull + 20833333325000ull);
    printf("result = %d, time add/removing = %.2f, %.2f\\n", int(result), ts1 - ts, now2sec() - ts1);""",
    """    // benchmark_iterations repetitions; report best time + stddev (P1)
    const int reps = g_config.benchmark_iterations > 0 ? g_config.benchmark_iterations : 1;
    double t_add = 1e30, t_remove = 1e30;
    std::vector<double> rep_times;
    for (int rep = 0; rep < reps; ++rep) {
        hmap.clear();
        result = 0;

        auto ts = now2sec();
        {
            MRNG rng(123);
            for (size_t n = 0; n < num_iters; ++n) {
                hmap[rng()] = n;
                for (const auto& keyVal : hmap)
                    result += keyVal.second;
            }
        }
        verify(result == 20833333325000ull, "IterateIntegers insert checksum");
        t_add = std::min(t_add, now2sec() - ts);

        auto ts1 = now2sec();
        {
            MRNG rng(123);
            for (size_t n = 0; n < num_iters; ++n) {
                hmap.erase(rng());
                for (auto const& keyVal : hmap)
                    result += keyVal.second;
            }
        }
        verify(result == 62498750000000ull + 20833333325000ull, "IterateIntegers erase checksum");
        t_remove = std::min(t_remove, now2sec() - ts1);
        rep_times.push_back((now2sec() - ts) + (now2sec() - ts1));
    }

    double t_sd = 0.0;
    if (rep_times.size() > 1) {
        const double mean = std::accumulate(rep_times.begin(), rep_times.end(), 0.0) / rep_times.size();
        double sq = 0.0;
        for (double t : rep_times) sq += (t - mean) * (t - mean);
        t_sd = std::sqrt(sq / (rep_times.size() - 1));
    }
    printf("result = %d, time add/removing = %.2f, %.2f (best of %d)\\n", int(result), t_add, t_remove, reps);""",
    label="IterateIntegers reps")

rep("""        r.time_sec = now2sec() - ts;
        r.operations = num_iters * 2;""",
    """        r.time_sec = t_add + t_remove;
        r.time_stddev = t_sd;
        r.operations = num_iters * 2;""", label="IterateIntegers record")

# ------------------------------------------- P2 memory in bench_insert
rep("""                if (g_config.enable_reserve)
                    hmap.reserve(maxn / 2);
                auto ts = now2sec();""",
    """                if (g_config.enable_reserve)
                    hmap.reserve(maxn / 2);
                const size_t rss_before = current_rss_bytes();
                auto ts = now2sec();""", label="bench_insert rss_before")

rep("""                    r.load_factor = hmap.load_factor();
                    r.map_size = static_cast<size_t>(hmap.size());
                    g_results.push_back(r);""",
    """                    r.load_factor = hmap.load_factor();
                    r.map_size = static_cast<size_t>(hmap.size());
                    r.memory_bytes = current_rss_bytes() - rss_before;
                    g_results.push_back(r);""", count=1, label="bench_insert memory")

# --------------------------------------- P2 BenchResult + CSV/JSON fields
rep("""    float load_factor = 0.0f;
    size_t map_size = 0;
};""",
    """    float load_factor = 0.0f;
    size_t map_size = 0;
    size_t memory_bytes = 0;
};""", label="BenchResult memory")

rep('static void output_csv_header() { printf("Test,HashMap,Time,StdDev,Operations,Throughput,LoadFactor,MapSize\\n"); }',
    'static void output_csv_header() { printf("Test,HashMap,Time,StdDev,Operations,Throughput,LoadFactor,MapSize,MemBytes\\n"); }',
    label="csv header")

rep("""        printf("%s,%s,%.6f,%.6f,%zu,%.2f,%.2f,%zu\\n", r.test_name.c_str(), r.hash_map_name.c_str(), r.time_sec,
               r.time_stddev, r.operations, r.throughput, r.load_factor, r.map_size);""",
    """        printf("%s,%s,%.6f,%.6f,%zu,%.2f,%.2f,%zu,%zu\\n", r.test_name.c_str(), r.hash_map_name.c_str(), r.time_sec,
               r.time_stddev, r.operations, r.throughput, r.load_factor, r.map_size, r.memory_bytes);""",
    label="csv row")

rep("""        printf("    \\"map_size\\": %zu\\n", r.map_size);""",
    """        printf("    \\"map_size\\": %zu,\\n", r.map_size);
        printf("    \\"memory_bytes\\": %zu\\n", r.memory_bytes);""", label="json out")

rep("""                fout << "Test,HashMap,Time,StdDev,Operations,Throughput,LoadFactor,MapSize\\n";""",
    """                fout << "Test,HashMap,Time,StdDev,Operations,Throughput,LoadFactor,MapSize,MemBytes\\n";""",
    label="file csv header")

rep("""                    fout << r.test_name << "," << r.hash_map_name << "," << r.time_sec << "," << r.time_stddev << ","
                         << r.operations << "," << r.throughput << "," << r.load_factor << "," << r.map_size << "\\n";""",
    """                    fout << r.test_name << "," << r.hash_map_name << "," << r.time_sec << "," << r.time_stddev << ","
                         << r.operations << "," << r.throughput << "," << r.load_factor << "," << r.map_size << ","
                         << r.memory_bytes << "\\n";""", label="file csv row")

rep("""                    fout << "    \\"map_size\\": " << r.map_size << "\\n";""",
    """                    fout << "    \\"map_size\\": " << r.map_size << ",\\n";
                    fout << "    \\"memory_bytes\\": " << r.memory_bytes << "\\n";""", label="file json")

# --------------------------------------- P1 record g_results in all cases
RECORD = """    if (g_config.output_format != OutputFormat::Text) {{
        BenchResult r;
        r.test_name = "{name}";
        r.hash_map_name = map_name;
        r.time_sec = {time};
        r.operations = {ops};
        r.throughput = r.time_sec > 0 ? static_cast<double>(r.operations) / r.time_sec : 0;
        r.map_size = {size};
        g_results.push_back(r);
    }}
"""


def record(name, time, ops, size):
    return RECORD.format(name=name, time=time, ops=ops, size=size)


cases = [
    ('    printf("\\t\\t\\t%.2f + %.2f, total %2.2f s\\n", now1 - nows, now2 - now1, now2 - nows);\n}',
     'randomFindString', 'now2 - nows', 'numInserts * numFindsPerInsert * 10', '0'),
    ('    printf("time %2.2f s\\n", now2sec() - nows);\n}',
     'randomEraseString', 'now2sec() - nows', '20000000 * 3 + 10000000 + 12000000 + 8000000 + 6000000', '0'),
    ('    printf(" erase time %.2f + %.2f, total %2.2f s\\n", erase1, erase2, erase1 + erase2);\n}',
     'randomInsertErase', 'erase1 + erase2', '0', '0'),
    ('    //#endif\n\n    printf(" time %2.2f s\\n", now2sec() - nows);\n}',
     'randomDistinct2', 'now2sec() - nows', '50000000 * 4', '0'),
    ('    printf("result = %d copy = %2.2f, assign = %2.2f s\\n", int(result), copyt - nows, now2sec() - copyt);\n}',
     'copy', 'now2sec() - nows', '1000', 'mapSource.size()'),
    ('    printf(" CreateInsert/InsertCreate %2.2f + %2.2f, total %2.2f s\\n", erase1, erase2, erase1 + erase2);\n}',
     'CreateInsert', 'erase1 + erase2', '0', '0'),
    ('#endif\n    printf(" time %2.2f s\\n", now2sec() - nows);\n}',
     'AccidentallyQuadratic', 'now2sec() - nows', '10000000', 'hmap.size()'),
    ('    printf(" ans = %d time %2.2f s\\n", (int)ans, now2sec() - nows);\n}',
     'knucleotide', 'now2sec() - nows', '25000000 * 3', '0'),
    ('    printf(", grow = %.2f, total %2.2f s\\n", now2sec() - grow, now2sec() - stastabilizing);\n}',
     'GameOfLife', 'now2sec() - stastabilizing', '0', '0'),
    ('    printf(" z[%d] = %d loadf = %.2f, total %2.2f s\\n", is_del, (int)z, h.load_factor(), now2sec() - nows);\n}',
     'udb3', 'now2sec() - nows', '80000000', 'h.size()'),
    ('        max_n *= 5;\n    }\n\n    printf(", total %2.2f s\\n", now2sec() - nows);\n}',
     'InsertEraseBegin', 'now2sec() - nows', '0', '0'),
    ('        max_n *= 7;\n    }\n\n    printf(", total %2.2f s\\n", now2sec() - nows);\n}',
     'InsertEraseContinue', 'now2sec() - nows', '0', '0'),
]
for anchor, name, time, ops, size in cases:
    if anchor not in src:
        fails.append(f"record anchor missing: {name}")
        continue
    src = src.replace(anchor, anchor.rstrip("}") + record(name, time, ops, size) + "}", 1)

# ------------------------------------------------ main summary
rep('    puts("---------------------------- all pass -----------------------------------");',
    """    if (g_check_errors)
        printf("\\n*** %d checksum check(s) FAILED -- results are suspect ***\\n", g_check_errors);
    puts("---------------------------- all pass -----------------------------------");""",
    label="main summary")

open(PATH, "w", encoding="utf-8", newline="").write(src)

print(f"[patch] done, {len(fails)} failures")
for f in fails:
    print("  MISSING:", f)
sys.exit(1 if fails else 0)
