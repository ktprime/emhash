# Benchmark Directory

## Structure

```
bench/
├── *.cpp           # Official benchmarks (built by CMake WITH_BENCHMARKS=ON)
├── framework/      # Reproducible harness + paired A/B comparison tool (see framework/README.md)
├── research/       # One-off investigation scripts (not built by CMake)
└── README.md       # This file
```

## Official Benchmarks (CMake targets)

Built via `cmake -B build -DWITH_BENCHMARKS=ON`:

| Target        | Source                     | Description                          |
|---------------|----------------------------|--------------------------------------|
| `ebench`      | ebench.cpp                 | emhash vs third-party (int keys)    |
| `sbench`      | sbench.cpp                 | emhash hash_set benchmarks           |
| `mbench`      | martin_bench.cpp           | Martin's third-party comparison     |
| `cbench`      | comprehensive_bench.cpp    | Comprehensive multi-scenario        |
| `bs`          | bstring.cpp                | String key benchmarks                |
| `bi`          | buint64.cpp                | uint64 key benchmarks                |
| `fbench`      | fbench.cpp                 | Find-focused benchmarks              |
| `hbench`      | hbench.cpp                 | Hash function comparison             |
| `zbench`      | zhash_bench.cc             | zhashmap comparison                  |
| `jbench`      | hash_join2.cpp             | Hash join (OpenMP parallel)          |

The `bench/framework/` subdirectory additionally provides `emhash_bench` — a
zero-dependency, reproducible single-workload harness with a paired A/B runner
(`ab.sh` / `ab.ps1`). See [framework/README.md](framework/README.md) for build
and methodology details.

## Research Scripts (bench/research/)

One-off investigation scripts not included in the CMake build:
- `bench_swiss_debug*.cpp` — Swiss table debugging
- `bench_diag*.cpp`, `bench_hash_diag.cpp`, `bench_clang_diag.cpp` — Diagnostic benchmarks
- `find_asm_micro.cpp`, `find_clang_investigate.cpp` — Assembly-level find analysis
- `find_prefetch_test.cpp` — Prefetch impact measurement
- `microbench_emihmap4.cpp`, `micro_udm.cpp` — Micro-benchmarks for single implementations

To compile a research script manually:

```bash
g++ -O3 -std=c++17 -I../include -I../thirdparty research/<script>.cpp -o /tmp/bench
```

## CI Benchmark Gate

The CI `benchmark` job uses `tests/bench/emhash_bench.cpp` (Google Benchmark),
which covers Insert/FindHit/FindMiss/Erase/Iterate for all emhash map and set
types at 100K elements, plus string key and SetFindMiss scenarios.
Regression threshold: **20%** (see `.github/workflows/ci.yml`).
