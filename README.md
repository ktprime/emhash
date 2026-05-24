# emhash

> High-performance, memory-efficient C++ open addressing flat hash table

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-11%2F14%2F17%2F20-blue.svg)](https://en.cppreference.com/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()

emhash is a family of high-performance, **header-only** hash table implementations designed for maximum performance and memory efficiency. Through innovative collision resolution strategies and cache optimization techniques, emhash demonstrates exceptional performance across various benchmarks.

---

## Table of Contents

- [Core Features](#core-features)
- [Performance Overview](#performance-overview)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Design Principles](#design-principles)
- [Implementation Comparison](#implementation-comparison)
- [Build and Test](#build-and-test)
- [Usage Notes](#usage-notes)
- [Third-Party Benchmarks](#third-party-benchmarks)
- [License](#license)

---

## Core Features

### Extreme Performance
- **High load factor support**: Set load factor up to **0.999** via `EMH_HIGH_LOAD` macro (available in `hash_table[5-8].hpp`)
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
| `try_set` | Sets value only if key does not exist (emhash5/8) |
| `set_get` | Updates value and returns old value (emhash5/8) |
| `_erase` | Delete operation returning void (faster) |
| **LRU Mode** | Enable LRU cache optimization with `EMH_LRU_SET` |

### Platform Support
- **Operating Systems**: Windows, Linux, macOS
- **Processors**: x86_64, ARM64 (Apple M1/M2, AMD, Intel)
- **Compilers**: GCC, Clang, MSVC (C++11/14/17/20)

---

## Performance Overview

### Performance Comparison with Mainstream Implementations (lower is better)

Test Environment: AMD 5800H / Windows 10 / GCC 11.3

| Operation | emhash7 | emhash8 | emhash6 | emhash5 | phmap | absl | std::unordered_map |
|-----------|---------|---------|---------|---------|-------|------|-------------------|
| Insert (10M) | 66.0 | 95.6 | 63.6 | 64.7 | 59.5 | 53.2 | 295 |
| Find Hit | 16.9 | 18.3 | 15.1 | 16.6 | 36.4 | 22.6 | 33.0 |
| Find Miss | 18.1 | 18.1 | 17.0 | 18.8 | 19.7 | 11.7 | 52.0 |
| Erase | 20.4 | 46.3 | 24.0 | 22.3 | 56.2 | 41.5 | 293 |
| Iterate | 0.74 | 0.00 | 0.75 | 4.75 | 3.46 | 3.49 | 62.6 |

> **Note**: emhash8's iteration time of 0.00 ms indicates extremely fast iteration due to its contiguous memory layout (actual time is < 0.005 ms).

### Excellent Performance at High Load Factors

Even at an extreme load factor of **0.999**, emhash maintains outstanding performance:

```cpp
emhash7::HashMap<int64_t, int> myhash(1 << 20, 0.999f);
// Insert 1M+ elements without rehash
// Stable insert/erase performance without degradation
```

---

## Quick Start

### 1. Include Header

```cpp
#include "hash_table7.hpp"  // Or other versions hash_table[5-8].hpp
```

### 2. Basic Usage

```cpp
#include "hash_table7.hpp"
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

---

## API Reference

### Constructors

```cpp
HashMap();                                    // Default constructor
HashMap(size_t bucket_count);                 // Specify initial bucket count
HashMap(size_t bucket_count, float max_load_factor);  // Specify load factor
HashMap(const HashMap& other);                // Copy constructor
HashMap(HashMap&& other) noexcept;            // Move constructor
HashMap(std::initializer_list<value_type> il);// Initializer list
```

### Capacity Operations

| Method | Description |
|--------|-------------|
| `size()` / `empty()` | Element count / is empty |
| `bucket_count()` | Current bucket count |
| `load_factor()` / `max_load_factor()` | Current/max load factor |
| `reserve(n)` | Reserve space for at least n elements |
| `shrink_to_fit()` | Shrink to fit current size |

### Element Access

| Method | Description |
|--------|-------------|
| `operator[key]` | Insert or access element |
| `at(key)` | Access element, undefined behavior if not found |
| `find(key)` | Find element, returns iterator |
| `try_get(key)` | Find element, returns pointer to value (`nullptr` on failure) |
| `contains(key)` | Check if key exists |
| `count(key)` | Key occurrence count (0 or 1) |

### Modification Operations

| Method | Description |
|--------|-------------|
| `emplace(key, val)` | In-place construct insert |
| `insert({key, val})` | Insert key-value pair |
| `insert_unique(key, val)` | Direct insert (no existence check, best performance) |
| `try_set(key, val)` | Set only if key does not exist (emhash5/8) |
| `set_get(key, val)` | Set new value, return old value (emhash5/8) |
| `erase(key)` / `erase(it)` | Delete element |
| `_erase(key)` | Delete element, return void (faster) |
| `clear()` | Clear all elements |

### Iterators

```cpp
for (auto it = map.begin(); it != map.end(); ++it) {
    // it->first: key, it->second: value
}

// C++17 structured binding
for (const auto& [key, value] : map) { }
```

---

## Design Principles

### Memory Layout

emhash adopts a **single array inline storage** design with compact node structure:

```cpp
struct Entry {
    Key key;           // Key
    size_t bucket;     // Bucket index/state info
    Value value;       // Value
};
```

This design minimizes memory allocation frequency and ensures data is stored contiguously in memory, maximizing CPU cache utilization.

### Primary Bucket Mapping

- Primary bucket is always assigned to `key_hash(key) % size` and **cannot be occupied**
- All operations start searching from the primary bucket
- Avoids circular displacement issues in cuckoo hashing

### Three-Way Hybrid Probing Strategy

1. **Linear Probing**: Scans 2-3 CPU cache lines (fastest path)
2. **Quadratic Probing**: Kicks in after linear probing, balancing performance and cache
3. **Bidirectional Linear Search**: Searches from both ends simultaneously, utilizing last found empty slot

> In `hash_table5.hpp`, the optimized three-way linear probing strategy is still **2-3x faster** than traditional strategies even at load factors **> 0.9**.

### Backup Hash Function

Enable backup hash function by defining `EMH_SAFE_HASH` macro to defend against hash attacks (performance cost ~ **10%**).

---

## Implementation Comparison

emhash provides 4 different implementations, each with different focus:

| Implementation | Best Scenario | Characteristics |
|----------------|---------------|-----------------|
| **emhash5** | Fast lookup and erase with integer keys | Optimized for find-hit and erase, supports small stack hash table (`EMH_SMALL_SIZE`) |
| **emhash6** | Lookup and erase with integer keys | Similar to emhash5, different memory layout optimization |
| **emhash7** | High load factor, insert-intensive | Supports extremely high load factor (0.90-0.99), efficient memory usage |
| **emhash8** | Complex keys/values (strings, structs) | Contiguous memory layout (like `std::vector`), extremely fast iteration, fast search and insert |

### Selection Guide

- **Complex/large keys/values** (e.g., `std::string`, custom structs) → **emhash8**
- **Insert-intensive workloads** → **emhash7**
- **Fast search and erase with integer keys** → **emhash5/6**

---

## Build and Test

### Using CMake

```bash
# Clone repository
git clone https://github.com/ktprime/emhash.git
cd emhash

# Create build directory
mkdir build && cd build

# Configure (optional: disable benchmarks)
cmake .. -DWITH_BENCHMARKS=ON

# Build
cmake --build . --config Release
```

### Running Tests

```bash
# Run unit tests
./test/emhash_test

# Run benchmarks
./bench/ebench
./bench/mbench
```

### Compile Options

| Macro | Description |
|-------|-------------|
| `EMH_HIGH_LOAD=<value>` | Enable high load factor support |
| `EMH_WY_HASH=1` | Use wyhash algorithm (faster for string keys) |
| `EMH_SAFE_HASH=1` | Enable backup hash function (hash attack protection) |
| `EMH_LRU_SET=1` | Enable LRU cache mode |
| `EMH_STATIS=1` | Enable collision statistics output |
| `EMH_FIBONACCI_HASH=1` | Use Fibonacci hashing |
| `EMH_IDENTITY_HASH=1` | Use identity hashing (integer key optimization) |

---

## Usage Notes

### 1. Non-Node-Based Hash Table

emhash **does not guarantee** reference stability during insert/erase/rehash operations.

```cpp
// ❌ Wrong: reference may become invalid after rehash
auto& ref = myhash[1];
myhash[2] = ref;  // Danger! May trigger rehash

// ✅ Correct: copy value first
auto val = myhash[1];
myhash.reserve(10000);  // Pre-allocate
myhash[2] = val;
```

### 2. Erasing During Iteration

Deleting elements during iteration may cause some keys to be visited twice or skipped.

```cpp
// ❌ Wrong: direct deletion without updating iterator
for (const auto& it : myhash) {
    if (condition) myhash.erase(it.first);
}

// ✅ Correct: use iterator erase and update
for (auto it = myhash.begin(); it != myhash.end(); ) {
    if (condition) {
        it = myhash.erase(it);  // Returns next valid iterator
    } else {
        ++it;
    }
}
```

### 3. Large Value Type Optimization

For very large value types (e.g., `> 100 bytes`), consider using pointer storage:

```cpp
// High memory usage
emhash7::HashMap<Key, LargeValue> map1;

// Optimized memory usage
emhash7::HashMap<Key, std::unique_ptr<LargeValue>> map2;
```

---

## Third-Party Benchmarks

emhash has been validated by multiple well-known third-party benchmarks:

- **[Martin Ankerl's Hashmap Benchmark](https://martin.ankerl.com/2022/08/27/hashmap-bench-01/)** - Comprehensive performance testing
- **[Jackson Allan's C/C++ Hash Tables Benchmark](https://jacksonallan.github.io/c_cpp_hash_tables_benchmark/)** - Multi-dimensional comparison

### Benchmark Code

- [Comprehensive Benchmark](https://github.com/ktprime/emhash/blob/master/bench/ebench.cpp)
- [High Load Test](https://github.com/ktprime/emhash/blob/master/bench/martin_bench.cpp)

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
- [wyhash](https://github.com/wangyi-fudan/wyhash)
