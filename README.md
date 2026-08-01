# emhash

> High-performance, memory-efficient C++ open addressing flat hash table

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
[![CI](https://github.com/ktprime/emhash/actions/workflows/ci.yml/badge.svg)](https://github.com/ktprime/emhash/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-1.1.0-green.svg)](https://github.com/ktprime/emhash/releases)

emhash is a family of high-performance, **header-only** hash table implementations designed for maximum performance and memory efficiency. Through innovative collision resolution strategies and cache optimization techniques, emhash demonstrates exceptional performance across various benchmarks.

---

## Table of Contents

- [Core Features](#core-features)
- [Quick Start](#quick-start)
- [Version Selection Guide](#version-selection-guide)
- [Testing](#testing)
- [Documentation](#documentation)
- [Third-Party Benchmarks](#third-party-benchmarks)
- [License](#license)

---

## Core Features

### Extreme Performance
- **High load factor support**: Set load factor up to **0.999** via `EMH_HIGH_LOAD` macro (available in `hash_table5/7/8`) or `max_load_factor()` (emhash6/7)
- **No tombstones**: Performance does not degrade even with frequent insert/erase operations
- **Smart collision resolution**: Three-way hybrid strategy combining linear probing, quadratic probing, and bidirectional search
- **Cache-friendly design**: Single array inline storage minimizes memory footprint and maximizes cache hit rate

### Memory Optimization
- **Compact layout**: Significant memory savings when `sizeof(key) % 8 != sizeof(value) % 8`
  - Example: `hash_map<uint64_t, uint32_t>` saves **1/3** memory compared to `hash_map<uint64_t, uint64_t>`
- **Dynamic shrinking**: `shrink_to_fit()` releases unused memory

### Extended Features
| Feature | Description |
|---------|-------------|
| `insert_unique` | Direct insertion without lookup (performance boost) |
| `try_get` | Returns pointer to value, `nullptr` if key not found |
| `try_set` | Set value if key exists, do nothing if it doesn't (emhash5/8, emilib1/2/3) |
| `set_get` | Updates value and returns old value (emhash5/8, emilib1/2/3) |
| `_erase` | Delete operation returning void (faster) |
| **LRU Mode** | Enable LRU cache optimization with `EMH_LRU_SET` |

### Platform Support
- **Operating Systems**: Windows, Linux, macOS
- **Processors**: x86_64, ARM64 (Apple M1/M2, AMD, Intel)
- **Compilers**: GCC, Clang, MSVC (C++17/20)

---

## Quick Start

### 1. Include Header

emhash is **header-only** — just copy one `.hpp` file to your project:

```bash
# Option A: Copy directly (simplest, no build system needed)
cp include/emhash/hash_table7.hpp /your/project/emhash/

# Option B: Clone and include
git clone https://github.com/ktprime/emhash.git
# Then add -I/path/to/emhash/include to your compiler flags

# Option C: CMake FetchContent (no install needed)
# Add to your CMakeLists.txt:
#   include(FetchContent)
#   FetchContent_Declare(emhash GIT_REPOSITORY https://github.com/ktprime/emhash.git GIT_TAG v1.1.0)
#   FetchContent_MakeAvailable(emhash)
#   target_link_libraries(your_target PRIVATE emhash::emhash)

# Option D: CMake find_package (after install)
# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/emhash-install
# In CMakeLists.txt: find_package(emhash REQUIRED CONFIG)
#   target_link_libraries(your_target PRIVATE emhash::emhash)
```

```cpp
#include "emhash/hash_table7.hpp"  // Or emhash/hash_table[5-8].hpp
```

### 2. Basic Usage

```cpp
#include "emhash/hash_table7.hpp"
#include <iostream>

int main() {
    // Create and reserve space
    emhash7::HashMap<int, std::string> map;
    map.reserve(100);

    // Insert elements (multiple ways)
    map[1] = "one";
    map.emplace(2, "two");
    map.insert_unique(3, "three");  // Best performance, key must be unique

    // Find element
    auto it = map.find(2);
    if (it != map.end()) {
        std::cout << "Found: " << it->second << "\n";
    }

    // try_get: returns pointer, more concise
    if (auto* pval = map.try_get(3)) {
        std::cout << "Value: " << *pval << "\n";
    }

    // Iterate
    for (const auto& [key, value] : map) {
        std::cout << key << " => " << value << "\n";
    }

    return 0;
}
```

### 3. Advanced Usage

```cpp
// Custom key type
struct Point {
    int x, y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct PointHash {
    size_t operator()(const Point& p) const {
        return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
    }
};

emhash7::HashMap<Point, std::string, PointHash> point_map;

// C++20 using Lambda
#if __cplusplus >= 202002L
auto hash = [](const Point& p) { return std::hash<int>()(p.x + p.y); };
auto eq = [](const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; };
emhash7::HashMap<Point, int, decltype(hash), decltype(eq)> map(0, hash, eq);
#endif
```

More examples: [docs/examples/](docs/examples/)

---

## Version Selection Guide

### 30-Second Quick Guide

> **If you're not sure which version to use, start with `emhash7`** — no tombstones means stable performance under mixed insert/erase workloads, with native high load factor support (0.9+).

Choose by your **primary workload pattern**, not key type:

```
What is your primary workload?
  ├─ Mixed (insert + find + erase) → emhash7 (no tombstones, stable at high LF)
  ├─ Find/erase-heavy →
  │     ├─ Integer keys → emhash6 (bitmask-accelerated empty-slot search)
  │     └─ String/large KV types → emhash8 (dense pairs, cache-friendly)
  ├─ Iteration-heavy → emhash8 (dense pairs array, sequential scan)
  └─ Small tables (< 1K elements) → emhash5 (small-size optimization)

Consider emilib2 (Swiss Table) if:
  - You need maximum find throughput at scale (1M+ elements) on GCC
  - Your workload is read-heavy with minimal erase (tombstone accumulation degrades mixed workloads)
  - Note: insert performance is significantly slower on Clang vs GCC
```

### Detailed Comparison

| Version | Best For | Key Strengths | Weaknesses |
|---------|----------|---------------|------------|
| **emhash5** | Small tables, memory-constrained | Small-size optimization (`EMH_SMALL_SIZE`), 3-way hybrid probing | Slower than emhash6 at high load factor |
| **emhash6** | Find/erase-heavy, integer keys | Bitmask-accelerated empty-slot search, fastest find/erase | Extra memory for bitmask array |
| **emhash7** | General purpose, mixed workloads | No tombstones (chain repair on erase), stable at 0.9+ LF | Erase slightly slower than emhash5/6 |
| **emhash8** | Iteration-heavy, large KV types | Split-index + dense pairs, sequential iteration, fast copy/move | Extra memory for separate index array |
| **emilib2/3** | Read-heavy at scale (GCC) | SIMD group probing (16 buckets/cycle), excellent iteration | Tombstone accumulation under mixed workloads; insert slower on Clang; **emilib2ss may hang under extreme hash collision attack** — use emilib2o or emilib2s |
| **emilib4** | Experimental Swiss-table variant | Fast insert on Clang, dense iteration | Tombstone accumulation (no backward shift); no `try_set`/`set_get`/`_erase`; fixed LF 0.875 |

### Feature Matrix

| Feature | emhash5 | emhash6 | emhash7 | emhash8 | emilib1/2/3 | emilib4 |
|---------|---------|---------|---------|---------|-------------|---------|
| Integer keys | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| String keys | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Custom types | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| High load factor (0.9+) | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| `try_get` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `try_set` | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ |
| `set_get` | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ |
| `shrink_to_fit` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Custom allocator | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| SIMD acceleration | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ |
| No tombstones | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ |
| Dense pairs iteration | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |

See [Performance Overview](docs/performance.md) for detailed benchmark numbers.

---

## Testing

The test suite is in [`tests/`](tests/) and requires **no third-party dependencies** — only emhash/emilib headers.

### Quick Start

```bash
cd tests

# Build all tests
cmake -B build && cmake --build build --config Release

# Run via custom targets
cmake --build build --target quick_test    # Unit + memory tests (fast feedback)
cmake --build build --target stress_test   # Stress tests
cmake --build build --target attack_test   # Hash attack tests
cmake --build build --target all_tests     # All tests
```

### Test Categories

| Category | Directory | Description |
|----------|-----------|-------------|
| Unit | `tests/unit/` | CRUD, iterators, copy/move, edge cases, full API coverage |
| Memory | `tests/memory/` | ASan/MSan/UBSan, leak detection, lifecycle audit |
| Stress | `tests/stress/` | High load factor, bad hash, randomized stress |
| Attack | `tests/attack/` | Hash collision attacks (constant/small-range/linear) |
| Fuzz | `tests/fuzz/` | LibFuzzer + ASan fuzzing (requires clang) |

See [tests/README.md](tests/README.md) for detailed instructions.

---

## Documentation

| Document | Description |
|----------|-------------|
| [Test Suite](tests/README.md) | Test organization, build instructions, coverage matrix |
| [Test Analysis & Coverage Report](docs/test_analysis.md) | Per-file coverage data, test classification, CI integration |
| [Performance Overview](docs/performance.md) | Benchmark results, high load factor performance |
| [API Reference](docs/api.md) | Constructors, methods, iterators |
| [Design Principles](docs/design.md) | Collision resolution, memory layout, implementation comparison |
| [Usage Notes](docs/usage_notes.md) | Thread safety, reference stability, iteration, large values |
| [Performance Tips](docs/performance_tips.md) | Compile flags, pre-allocation, hash selection, anti-patterns |
| [FAQ](docs/faq.md) | Frequently asked questions |
| [Migration Guide](docs/migration_guide.md) | Migrating from std::unordered_map |
| [Performance Tracking](docs/performance_tracking.md) | Version-by-version benchmark history, regression policy |
| [Architecture Decisions (ADR)](docs/adr/README.md) | Why we chose open addressing, header-only, emhash8 layout, etc. |

---

## Third-Party Benchmarks

emhash has been validated by multiple well-known third-party benchmarks:

- **[Martin Ankerl's Hashmap Benchmark](https://martin.ankerl.com/2022/08/27/hashmap-bench-01/)** - Comprehensive performance testing
- **[Jackson Allan's C/C++ Hash Tables Benchmark](https://jacksonallan.github.io/c_cpp_hash_tables_benchmark/)** - Multi-dimensional comparison

### Benchmark Code

- [Comprehensive Benchmark](bench/ebench.cpp)
- [High Load Test](bench/martin_bench.cpp)

### Performance Charts

Historical performance charts are available in `docs/images/`:
- `int64_t*.png` - Integer key benchmarks
- `string*.png` - String key benchmarks
- `int_string.png` - Mixed key/value type benchmarks

### Interactive Performance Charts

Download all files in the `bench/tsl_bench/` directory and open `chartsAll.html` in a browser to view interactive performance curves.

---

## License

This project is open-sourced under the [MIT License](LICENSE).

```
Copyright (c) 2019-2026 Huang Yuanbing & bailuzhou AT 163.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

---

## Acknowledgments

Thanks to the following projects and authors for inspiration and comparison:

- [martinus/robin-hood-hashing](https://github.com/martinus/robin-hood-hashing)
- [greg7mdp/parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap)
- [Tessil/hopscotch-map](https://github.com/Tessil/hopscotch-map)
- [ska::flat_hash_map](https://github.com/skarupke/flat_hash_map)
- [wyhash](https://github.com/wangyi-fudan/wyhash) — integrated directly into [config.hpp](include/emhash/config.hpp) (no external dependency required); pass `-DEMH_NO_BUILTIN_WYHASH` to use your own wyhash.h
