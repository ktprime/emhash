# Benchmark Directory

## Structure

```
bench/
├── *.cpp          # Official benchmarks (built by CMake WITH_BENCHMARKS=ON)
├── research/      # One-off investigation scripts (not built by CMake)
└── README.md      # This file
```

## Official Benchmarks (CMake targets)

Built via `cmake -DWITH_BENCHMARKS=ON ..`:

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

## Research Scripts (bench/research/)

One-off investigation scripts not included in CMake build:
- `bench_swiss_debug[2-4].cpp` — Swiss table debugging
- `bench_diag*.cpp`, `bench_clang_diag.cpp` — Diagnostic benchmarks
- `find_asm_micro.cpp`, `find_clang_investigate.cpp` — Assembly-level find analysis
- `find_prefetch_test.cpp` — Prefetch impact measurement
- `microbench_emihmap4.cpp` — Micro-benchmark for emihmap4

To compile a research script manually:
```bash
g++ -O3 -std=c++17 -I../include -I../thirdparty research/<script>.cpp -o /tmp/bench
```

## CI Benchmark Gate

The CI `benchmark` job uses `tests/bench/emhash_bench.cpp` (Google Benchmark),
which covers Insert/FindHit/FindMiss/Erase/Iterate for all emhash map/set types
at 100K elements, plus string key and SetFindMiss scenarios.
Regression threshold: **20%** (see `.github/workflows/ci.yml`).
