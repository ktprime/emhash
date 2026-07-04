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
| `try_set` | Set value if key exists, do nothing if it doesn't (emhash5/8) |
| `set_get` | Updates value and returns old value (emhash5/8) |
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

Use this decision tree to pick the right version:

```
你的 key 是整数吗？
  ├─ 是 → 追求极致性能？
  │     ├─ 是 → emhash7 (高负载因子，无墓碑)
  │     └─ 否 → emhash5 (内存高效，低探测)
  │
  └─ 否 → 编译器支持 SIMD 指令集？
         ├─ 是 → emilib2 (SIMD 加速查找，最快)
         └─ 否 → emhash8 (通用，快速迭代)
```

### Detailed Comparison

| Version | Best For | Key Strengths | Weaknesses |
|---------|----------|---------------|------------|
| **emhash5** | Integer keys, fast lookup | Small-size optimization (`EMH_SMALL_SIZE`), lowest probe count | Slightly slower than emhash6 for high LF |
| **emhash6** | Fast lookup/erase, integer keys | Fastest find/erase, linked-bucket with bitmask | More memory for metadata |
| **emhash7** | Insert-heavy, mixed workloads | No tombstones, stable insert/erase, high load factor | Slightly slower erase than emhash5/6 |
| **emhash8** | Iteration-heavy, large KV types | Dense pairs array, near-zero iteration time | Higher memory for separate index array |
| **emilib1/2/3** | Swiss-table style SIMD-accelerated lookup | Group-level SIMD probing, very fast find | Higher memory overhead per bucket; **emilib2ss may hang under extreme hash collision attack** (all keys hashing to same bucket) — use emilib2o or emilib2s in such scenarios |

### Feature Matrix

| Feature | emhash5 | emhash6 | emhash7 | emhash8 | emilib1/2/3 |
|---------|---------|---------|---------|---------|-------------|
| Integer keys | ✅ | ✅ | ✅ | ✅ | ✅ |
| String keys | ✅ | ✅ | ✅ | ✅ | ✅ |
| Custom types | ✅ | ✅ | ✅ | ✅ | ✅ |
| High load factor (0.9+) | ✅ | ✅ | ✅ | ✅ | ✅ |
| `try_get` | ✅ | ✅ | ✅ | ✅ | ❌ |
| `try_set` | ✅ | ❌ | ❌ | ✅ | ❌ |
| `set_get` | ✅ | ❌ | ❌ | ✅ | ❌ |
| `shrink_to_fit` | ✅ | ✅ | ✅ | ✅ | ❌ |
| Custom allocator | ✅ | ✅ | ✅ | ✅ | ❌ |
| SIMD acceleration | ❌ | ❌ | ❌ | ❌ | ✅ |
| Fastest iteration | ❌ | ❌ | ❌ | ✅ | ❌ |

See [Performance Overview](docs/performance.md) for detailed benchmark numbers.

---

## Testing

The test suite is in [`tests/`](tests/) and requires **no third-party dependencies** — only emhash/emilib headers.

### Quick Start

```bash
cd tests

# Build and run quick validation tests (247,268 assertions, ~5 seconds)
cmake -B build && cmake --build build --config Release
./build/Release/test_verify.exe        # Windows
./build/test_verify                    # Linux/WSL

# Or use the custom targets
cmake --build build --target quick_test    # Quick validation
cmake --build build --target stress_test   # Stress tests
cmake --build build --target attack_test   # Hash attack tests
cmake --build build --target all_tests     # All tests
```

### Test Categories

| Category | Directory | Description |
|----------|-----------|-------------|
| Validation | `tests/verify/` | 247K+ assertions, full API coverage, special key types |
| Stress | `tests/stress/` | 35K trials, high load factor, bad hash scenarios |
| Attack | `tests/attack/` | Hash collision attacks (constant/small-range/linear) |
| Debug | `tests/debug/` | Debugging tools for chain corruption, probe bugs |
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
