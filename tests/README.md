# emhash Test Suite

Test suite for the emhash project, built on the [doctest](https://github.com/doctest/doctest) framework (v2.5.3, vendored as a single header).

All tests use `TEST_CASE_TEMPLATE` to run each scenario against all 8 map implementations (emhash5/6/7/8 + emilib1/2/3/4) and all 5 set implementations (emhash2/4/8 + emihset2/3) with zero duplication.

---

## Table of Contents

- [Directory Structure](#directory-structure)
- [Quick Start](#quick-start)
- [Sanitizer Builds](#sanitizer-builds)
- [Google Benchmark (Optional)](#google-benchmark-optional)
- [Fuzz Testing (Optional)](#fuzz-testing-optional)
- [Coverage Matrix](#coverage-matrix)
- [Implementation Namespaces](#implementation-namespaces)
- [Custom Targets](#custom-targets)

---

## Directory Structure

```
tests/
├── common/          # Shared infrastructure (doctest header + type lists + utilities)
│   ├── doctest.h    # doctest v2.5.3 single-header (vendored)
│   ├── maps.hpp     # Unified type aliases + doctest::Types lists
│   ├── hashers.hpp  # ConstHasher / Range4Hasher / LinearHasher / CollidingHash
│   ├── trackers.hpp # LeakTracker (constructor/destructor balance checker)
│   ├── utilities.hpp # make_kv / now_ms / oracle_equal
│   ├── msan_unpoison.hpp # MSan std::cout/std::cerr unpoison helpers
│   └── msan-ignorelist.txt # MSan ignore list
├── unit/            # Functional correctness (doctest)
├── memory/          # Sanitizer + leak + lifecycle (3 files, doctest)
├── stress/          # Stress tests (doctest)
├── attack/          # Hash collision attack tests (2 files, doctest, EMH_SAFE_PSL)
├── fuzz/            # libfuzzer targets (optional) + bug reproduction
├── bench/           # Google Benchmark harness (optional, EMHASH_ENABLE_BENCH)
├── debug/           # Debugging tools (not built by CMake, see debug/README.md)
├── CMakeLists.txt
└── README.md
```

### File Inventory

| Directory | Files | Purpose |
|-----------|-------|---------|
| `unit/` | test_crud, test_iterators, test_copy_move, test_reserve_clear, test_edge_cases, test_special_keys, test_string_keys, test_full_api, test_allocator, test_hashset, test_api_coverage, test_small_data, test_iterator_invalidation, test_stress_correctness, test_emilib_lifecycle, test_ht8_optimizations, test_ht8_hetero | Core API correctness across all implementations |
| `memory/` | test_sanitizer, test_string_key_leak, test_lifecycle_audit | ASan/MSan/UBSan scenarios, LeakTracker balance, lifecycle audit |
| `stress/` | test_stress_all, test_highload, test_bad_hash, test_reserve_fix, test_million_stress, test_complex_conditions | Randomized stress with oracle comparison |
| `attack/` | test_hash_attack, test_collision_hardening | Collision attack correctness + performance |
| `fuzz/` | fuzz_emhash_all, fuzz_emilib_all, fuzz_extreme, fuzz_main, reproduce_emhash8_bug | libfuzzer targets (optional) + always-built regression reproducer |
| `bench/` | emhash_bench | Google Benchmark harness for the CI regression gate |
| `debug/` | debug_all_maps (+ local scratch files) | Cross-implementation debug harness (see [debug/README.md](debug/README.md)) |

---

## Quick Start

### Build and Run (CMake)

```bash
cd tests

# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all tests
cmake --build build --config Release

# Run quick tests (unit + memory)
cmake --build build --target quick_test

# Run stress tests
cmake --build build --target stress_test

# Run attack tests
cmake --build build --target attack_test

# Run all tests
cmake --build build --target all_tests
```

### Manual Build (single file)

```bash
g++ -std=c++17 -O2 -I../include -I. -Icommon unit/test_crud.cpp -o test_crud
./test_crud
```

---

## Sanitizer Builds

### AddressSanitizer + UBSan (Linux/macOS)

```bash
cmake -B build_asan -DEMHASH_SANITIZER=address
cmake --build build_asan
cd build_asan && ctest -L emhash --output-on-failure
```

### MemorySanitizer (Linux, clang only)

```bash
cmake -B build_msan -DEMHASH_SANITIZER=memory
cmake --build build_msan
cd build_msan && ctest -L emhash --output-on-failure
```

### MSVC AddressSanitizer (Windows x64)

```bash
cmake -B build_asan -DEMHASH_SANITIZER=address
cmake --build build_asan --config Release
```
Requires `clang_rt.asan_dynamic.dll` on PATH.

---

## Google Benchmark (Optional)

`bench/emhash_bench.cpp` is a [Google Benchmark](https://github.com/google/benchmark) (v1.9.4, fetched via FetchContent) harness measuring Insert/FindHit/FindMiss/Erase/Iterate at 100K elements for all map and set types, plus string-key and SetFindMiss scenarios. CI uses it as the 20% performance regression gate (see [bench/README.md](../bench/README.md)).

Disabled by default:

```bash
cmake -B build_bench -DEMHASH_ENABLE_BENCH=ON
cmake --build build_bench --target emhash_bench
./build_bench/emhash_bench --benchmark_format=console
```

---

## Fuzz Testing (Optional)

Fuzz targets use libfuzzer (clang only). Disabled by default.

`fuzz/reproduce_emhash8_bug.cpp` is the exception: it is a regression reproducer that is always built and registered with CTest.

```bash
# Enable fuzz targets
cmake -B build_fuzz -DEMHASH_ENABLE_FUZZ=ON
cmake --build build_fuzz

# Run fuzzer
./build_fuzz/fuzz_emhash_all -max_total_time=60 -print_final_stats=1
```

For non-libfuzzer builds (gcc + ASan), link with `fuzz_main.cpp`:

```bash
g++ -std=c++17 -fsanitize=address -I../include -Icommon \
    fuzz/fuzz_extreme.cpp fuzz/fuzz_main.cpp -o fuzz_extreme
./fuzz_extreme 1000
```

---

## Coverage Matrix

| Scenario | emhash5 | emhash6 | emhash7 | emhash8 | emilib1 | emilib2 | emilib3 | emilib4 | sets |
|----------|---------|---------|---------|---------|---------|---------|---------|---------|------|
| CRUD | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Iterators | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Copy/Move | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Reserve/Clear | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Edge cases | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| String keys | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Full API | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Custom allocator | ✅ | ✅ | ✅ | ✅ | — | — | — | — | ✅ |
| Sanitizer (8 scenarios) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| LeakTracker balance | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Lifecycle audit | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — | — |
| Stress | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| Hash attack | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅* | — |
| Collision hardening | ✅ | ✅ | ✅ | ✅ | — | — | — | — | — |
| Fuzz | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — | — |

\* emilib4 runs in the `AllIntMaps` attack baseline scenarios; the dedicated collision-attack type lists (`ConstAttackMaps`/`Range4AttackMaps`) exclude it.

---

## Implementation Namespaces

| Implementation | Namespace | Header |
|---------------|-----------|--------|
| emhash5/6/7/8 HashMap | `emhash5`/`emhash6`/`emhash7`/`emhash8` | `emhash/hash_table5.hpp` … `hash_table8.hpp` |
| emilib HashMap | `emilib` / `emilib2` / `emilib3` / `emilib4` | `emilib/emihmap1.hpp` / `emihmap2.hpp` / `emihmap3.hpp` / `emihmap4.hpp` |
| emhash HashSet | `emhash2` / `emhash4` / `emhash8` | `emhash/hash_set2.hpp` / `hash_set4.hpp` / `hash_set8.hpp` |
| emilib HashSet | `emilib2` / `emilib3` | `emilib/emihset2.hpp` / `emihset3.hpp` |

---

## Custom Targets

| Target | Description |
|--------|-------------|
| `quick_test` | unit + memory tests (fast feedback) |
| `stress_test` | stress tests |
| `attack_test` | hash attack tests |
| `all_tests` | everything registered in CTest |
| `bench` | Google Benchmark harness (requires `-DEMHASH_ENABLE_BENCH=ON`) |
