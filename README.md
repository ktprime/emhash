# emhash

> High-performance, memory-efficient C++ open addressing flat hash table

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17%2F20%2F23-blue.svg)](https://en.cppreference.com/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
[![CI](https://github.com/ktprime/emhash/actions/workflows/ci.yml/badge.svg)](https://github.com/ktprime/emhash/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-1.3.0-green.svg)](https://github.com/ktprime/emhash/releases)

emhash is a family of high-performance, **header-only** hash table implementations designed for maximum performance and memory efficiency. Through innovative collision resolution strategies and cache optimization techniques, emhash demonstrates exceptional performance across various benchmarks.

---

## Table of Contents

- [Core Features](#core-features)
- [Quick Start](#quick-start)
- [Version Selection Guide](#version-selection-guide)
- [Testing](#testing)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Versioning](#versioning)
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
| `_erase` | Faster void-returning delete — iterator form in emhash5-7, key form in emilib1-3 (absent from emhash8/emilib4) |
| Heterogeneous lookup | `find`/`contains`/`count`/`at`/`erase`/`try_get` accept `std::string_view` / `const char*` without building a temporary `std::string` (emhash8; requires transparent hash & equality) |
| **LRU Mode** | Enable LRU cache optimization with `EMH_LRU_SET`, or use the dedicated [lru_size.hpp](include/emhash/lru_size.hpp) / [lru_time.hpp](include/emhash/lru_time.hpp) caches |

### Recent emhash8 Optimizations (v1.1.0)
- **Zero-cost default constructor**: empty maps share a static sentinel and allocate nothing until the first insert
- **Heterogeneous string lookup**: see table above — enabled when both the hasher and the equality carry `is_transparent` (e.g. `emhash8::string_hash` + `std::equal_to<>`)
- **`EMH_POD_HASH`**: opt-in byte-hash for fixed-size (8/12/16-byte) trivially-copyable keys via `-DEMH_POD_HASH=1`
- **ADL `swap`**: free-function `swap(a, b)` for emhash8::HashMap (other containers use the member `swap()`)

### Platform Support
- **Operating Systems**: Windows, Linux, macOS
- **Processors**: x86_64, ARM64 (Apple M1/M2, AMD, Intel)
- **Compilers**: GCC, Clang, MSVC (C++17/20/23)

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
#   FetchContent_Declare(emhash GIT_REPOSITORY https://github.com/ktprime/emhash.git GIT_TAG v1.3.0)
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
| **emhash7** | General purpose, mixed workloads | Chain repair on erase (no tombstones), stable at 0.9+ LF | Erase slightly slower than emhash5/6 |
| **emhash8** | Iteration-heavy, large KV types | Split-index + dense pairs, sequential iteration, fast copy/move | Extra memory for separate index array |
| **emilib2/3** | Read-heavy at scale (GCC) | SIMD group probing (16 buckets/cycle), excellent iteration | Tombstone accumulation under mixed workloads; insert slower on Clang; **emilib2ss may hang under extreme hash collision attack** — use emilib2o or emilib2s |
| **emilib4** | Experimental Swiss-table variant | Fast insert on Clang, dense iteration | Tombstone accumulation (no backward shift); no `try_set`/`set_get`/`_erase`; fixed LF 0.875 |

### Feature Matrix

| Feature | emhash5 | emhash6 | emhash7 | emhash8 | emilib1/2/3 | emilib4 |
|---------|---------|---------|---------|---------|-------------|---------|
| High load factor (0.9+) | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| `try_set` | ⚠️² | ❌ | ❌ | ✅ | ✅ | ❌ |
| `set_get` | ✅³ | ❌ | ❌ | ✅³ | ✅⁴ | ❌ |
| `try_get` | ⚠️² | ⚠️² | ⚠️² | ✅⁵ | ✅⁶ | ✅⁶ |
| `_erase` | ✅⁸ | ✅⁸ | ✅⁸ | ❌ | ✅⁸ | ❌ |
| `erase(it)` returns iterator | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| `insert_unique(value_type)` overload | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| `hash_function()`/`key_eq()` | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| `emplace_hint` | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| `merge` | ✅ | ⚠️¹ | ⚠️¹ | ✅ | ✅ | ✅ |
| Precomputed `hash(key)` lookup | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ |
| Heterogeneous erase/try_get (transparent hash) | ❌ | ❌ | ❌ | ✅⁷ | ❌ | ❌ |
| SIMD acceleration | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ |
| No tombstones | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Bidirectional iterator | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ |
| Dense pairs iteration | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |

> API presence is enforced by [`tests/unit/test_api_contract.cpp`](tests/unit/test_api_contract.cpp) (compile-time contract) and audited against the headers at release time.
>
> Uniformly available on all map implementations (hence not listed above): `try_emplace` including the zero/multi value-arg forms (issue #104), `insert_or_assign`, `erase_if`, `contains`, `at`, `count`, `insert_unique`, `reserve`/`rehash`, `shrink_to_fit`, member `swap`, `operator==`/`operator!=`, custom allocator (`AllocT` as the trailing template parameter, allocator-extended constructors, `get_allocator`).
>
> ¹ `merge` exists on emhash6/7 but has a known key-loss bug during rehash (see [CHANGELOG](CHANGELOG.md) Known Issues).
>
> ² Available only when `EMH_EXT` is defined (off by default) — the `#ifdef EMH_EXT` block in emhash5-7 holds `try_get`/`try_set`.
>
> ³ Shape: `ValueT set_get(key, val)` (get-or-insert), plus a `bool set_get(key, val, old_out)` overload.
>
> ⁴ Shape: `bool set_get(key, val, old_out)` only (no 2-arg form).
>
> ⁵ Shape: `bool try_get(key, out&)`. Caution for integer keys: the precomputed-hash overload `try_get(key, uint64_t)` can make plain `try_get(1, out)` ambiguous in multi-header builds — prefer `find()`/`operator[]`.
>
> ⁶ Shape: `ValueT* try_get(key)` (returns nullptr when absent).
>
> ⁷ Requires a transparent hasher + equality (e.g. `emhash8::string_hash` + `std::equal_to<>`). emhash5-7 `find()` is an open template that only works heterogeneously with a transparent equality; they provide no heterogeneous `erase`/`try_get`.
>
> ⁸ Shape: emhash5-7 take `_erase(iterator)`, emilib1-3 take `_erase(key)` (emilib2 also has the iterator form). emhash8/emilib4 provide neither — use `erase()`.
>
> Hash sets (`emhash2/4/8::HashSet`, `emilib2/3::HashSet`) mirror the corresponding map structures; set API surface is covered by the same contract test.

### Set Feature Matrix

| Feature | emhash2 | emhash4 | emhash8 | emilib2 | emilib3 |
|---------|---------|---------|---------|---------|---------|
| High load factor (0.9+) | ✅¹⁰ | ✅ | ✅ | ❌¹⁰ | ✅ |
| `emplace_hint` | ✅ | ✅ | ✅ | ❌ | ❌ |
| `merge` | ✅ | ✅ | ✅ | ⚠️⁹ | ⚠️⁹ |
| `erase_if` | ✅ | ✅ | ✅ | ⚠️⁹ | ⚠️⁹ |
| `_erase` | ✅¹¹ | ✅¹¹ | ❌¹¹ | ✅¹¹ | ✅¹¹ |
| `erase(it)` returns iterator | ✅ | ✅ | ✅ | ✅ | ❌ |
| `hash_function()`/`key_eq()` | ✅ | ✅ | ✅ | ❌ | ❌ |
| Custom allocator | ✅ | ✅ | ✅ | ❌¹² | ❌¹² |
| SIMD acceleration | ❌ | ❌ | ❌ | ✅ | ✅ |
| No tombstones | ✅ | ✅ | ✅ | ❌ | ❌ |
| Bidirectional iterator | ❌ | ❌ | ✅ | ❌ | ❌ |
| Dense storage iteration | ❌ | ❌ | ✅ | ✅ | ✅ |

> Uniformly available on all sets (hence not listed above): `insert`/`insert_unique`, `contains`, `count`, `try_emplace` (insert-if-absent), `reserve`/`rehash`, `shrink_to_fit`, member `swap`, `operator==`/`operator!=`.
>
> ⁹ Present but has a known runtime issue (see [CHANGELOG](CHANGELOG.md) Known Issues) — prefer per-element `erase()` until fixed.
>
> ¹⁰ emhash2 defaults to load factor 0.95; emilib2's `max_load_factor(float)` is a no-op (fixed at 7/8 = 0.875); emilib3 accepts up to 0.999. Note the emilib2 *map* does support a settable load factor — this divergence is set-only.
>
> ¹¹ hash_set8 provides no `_erase()`; use `erase()` instead. Shapes differ elsewhere: emhash2/4 take `_erase(iterator)`, emilib2/3 take `_erase(key)` (emilib2 has both).
>
> ¹² emilib *maps* support custom allocators; the emilib *sets* do not take an `AllocT` template parameter.

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
| [Documentation Index](docs/index.md) | Quick links to all documentation pages |
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

## Contributing

Contributions are welcome. Before opening a pull request:

1. **Add or update tests** for any behavior change — see [tests/README.md](tests/README.md) for the suite layout and build commands.
2. **Format your code** with `clang-format` (LLVM style, see [.clang-format](.clang-format)); CI enforces this via `format-check.yml`. CMake files are excluded from formatting.
3. **Run the test suite locally** — at minimum the `quick_test` target.
4. CI gates your PR against: unit/memory/stress/attack tests (GCC, Clang, MSVC), sanitizer builds (ASan/UBSan/MSan), C++17/20/23 compatibility, an 80% line-coverage floor, clang-tidy, and a benchmark regression gate (20% threshold).

### Development Conventions

- **API contract**: all map implementations (emhash5-8 and emilib1-4) expose the same core surface; this is enforced at compile time by [tests/unit/test_api_contract.cpp](tests/unit/test_api_contract.cpp). Documented deviations (e.g. only emhash5 has `try_emplace(hint, ...)`) are asserted there too — flipping an assertion requires updating the documentation in the same PR.
- **Performance evidence grading**: trust conclusions in this order — allocation counts > instruction counts (e.g. valgrind callgrind) > consistent multi-compiler / multi-scale trends > single-run wall-clock numbers. Single-point ns/op comparisons are not valid regression evidence; measurement noise on typical dev machines can exceed ±10-50%.
- **Optimization acceptance**: an optimization lands only with interleaved A/B benchmark data showing it is faster; otherwise keep the simpler code.
- **Cross-implementation sync**: API and conformance changes must be applied to all four hash_table implementations (5/6/7/8) in the same PR and covered by the contract test above.

---

## Versioning

- The library version lives in the [VERSION](VERSION) file (currently **1.3.0**); CMake reads it via `project(emhash VERSION ...)` and the vcpkg port reuses it.
- Releases are tagged `vX.Y.Z`; the installed CMake package config uses `SameMajorVersion` compatibility.

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
