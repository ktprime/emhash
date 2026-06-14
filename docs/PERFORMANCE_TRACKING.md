# Performance Tracking

This document records benchmark results across emhash versions to track
performance evolution and detect regressions.

## Test Environment

| Component | Specification |
|-----------|---------------|
| CPU | AMD 5800H |
| OS | Windows 10 |
| Compiler | GCC 11.3 |
| Build | `-O3 -march=native -std=c++17` |

## Baseline: v1.1.0 (2026-06-13)

Source: [README.md](../../README.md) performance section.

### Throughput (10M elements, integer keys)

| Operation | emhash5 | emhash6 | emhash7 | emhash8 | phmap | absl | std::unordered_map |
|-----------|---------|---------|---------|---------|-------|------|-------------------|
| Insert    | 64.7    | 63.6    | 66.0    | 95.6    | 59.5  | 53.2 | 295               |
| Find Hit  | 16.6    | 15.1    | 16.9    | 18.3    | 36.4  | 22.6 | 33.0              |
| Find Miss | 18.8    | 17.0    | 18.1    | 18.1    | 19.7  | 11.7 | 52.0              |
| Erase     | 22.3    | 24.0    | 20.4    | 46.3    | 56.2  | 41.5 | 293               |
| Iterate   | 4.75    | 0.75    | 0.74    | 0.00    | 3.46  | 3.49 | 62.6              |

### High Load Factor (LF=0.999, `-DEMH_HIGH_LOAD=123456`)

| Implementation | Insert (ms) | Erase+Insert (ms) | LF |
|----------------|-------------|-------------------|-----|
| emhash7        | 44          | 118               | 99.9 |
| emhash6        | 38          | 202               | 99.9 |
| emhash5        | 42          | 90                | 99.9 |
| emhash8        | 49          | 110               | 99.9 |

## How to Run Benchmarks Locally

```bash
# Standard benchmarks
cd tests
make bench
./bench/ebench

# High load factor
cd bench
g++ -O3 -march=native -I../include -I../thirdparty -std=c++17 -DEMH_HIGH_LOAD=123456 highload_bench.cpp -o highload_bench
./highload_bench
```

## Recording New Results

When adding a benchmark result row, please:

1. Note the commit SHA at the top
2. Include the test environment (CPU, OS, compiler version, build flags)
3. Run each benchmark at least 3 times and report the median
4. If a result regresses by more than 10%, investigate and document the cause
5. If a result improves by more than 20%, document the optimization in the commit message

## Regression Policy

| Change Range | Action |
|--------------|--------|
| ±5%          | No action — within noise |
| 5-10%        | Investigate, document in PR |
| > 10%        | Block PR, require optimization or justification |
