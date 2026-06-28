# emhash Test Suite

Test suite for the emhash project, built on the [doctest](https://github.com/doctest/doctest) framework (v2.4.11, vendored as a single header).

All tests use `TEST_CASE_TEMPLATE` to run each scenario against all 7 map implementations (emhash5/6/7/8 + emilib1/2/3) and all 5 set implementations (emhash2/4/8 + emihset2/3) with zero duplication.

---

## Directory Structure

```
tests/
├── common/          # Shared infrastructure (doctest header + type lists + utilities)
│   ├── doctest.h    # doctest v2.4.11 single-header (vendored)
│   ├── maps.hpp     # Unified type aliases + doctest::Types lists
│   ├── hashers.hpp  # ConstHasher / Range4Hasher / LinearHasher / CollidingHash
│   ├── trackers.hpp # LeakTracker (constructor/destructor balance checker)
│   └── utilities.hpp # make_kv / now_ms / oracle_equal
├── unit/            # Functional correctness (10 files, doctest)
├── memory/          # Sanitizer + leak + lifecycle (3 files, doctest)
├── stress/          # Stress tests (4 files, doctest)
├── attack/          # Hash collision attack tests (2 files, doctest, EMH_SAFE_PSL)
├── fuzz/            # libfuzzer targets (5 files, optional)
├── debug/           # Debug tools (not part of doctest suite, unchanged)
├── bench/           # Google Benchmark (optional, unchanged)
├── archive/         # Old scratch files (not built)
├── CMakeLists.txt
└── README.md
```

### File Inventory

| Directory | Files | Purpose |
|-----------|-------|---------|
| `unit/` | test_crud, test_iterators, test_copy_move, test_reserve_clear, test_edge_cases, test_special_keys, test_string_keys, test_full_api, test_allocator, test_hashset | Core API correctness across all implementations |
| `memory/` | test_sanitizer, test_string_key_leak, test_lifecycle_audit | ASan/MSan/UBSan scenarios, LeakTracker balance, lifecycle audit |
| `stress/` | test_stress_all, test_highload, test_bad_hash, test_reserve_fix | Randomized stress with oracle comparison |
| `attack/` | test_hash_attack, test_collision_hardening | Collision attack correctness + performance |
| `fuzz/` | fuzz_emhash_all, fuzz_emilib_all, fuzz_extreme, fuzz_main, reproduce_emhash8_bug | libfuzzer targets (optional) |

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
g++ -std=c++17 -O2 -I../include -Icommon unit/test_crud.cpp -o test_crud
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

## Fuzz Testing (Optional)

Fuzz targets use libfuzzer (clang only). Disabled by default.

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

| Scenario | emhash5 | emhash6 | emhash7 | emhash8 | emilib1 | emilib2 | emilib3 | sets |
|----------|---------|---------|---------|---------|---------|---------|---------|------|
| CRUD | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Iterators | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Copy/Move | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Reserve/Clear | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Edge cases | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| String keys | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Full API | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Custom allocator | ✓ | ✓ | ✓ | ✓ | — | — | — | ✓ |
| Sanitizer (8 scenarios) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| LeakTracker balance | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Lifecycle audit | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| Stress (5 categories) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| Hash attack | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| Collision hardening | ✓ | ✓ | ✓ | ✓ | — | — | — | — |
| Fuzz | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

---

## Implementation Namespaces

| Implementation | Namespace | Header |
|---------------|-----------|--------|
| emhash5/6/7/8 HashMap | `emhash5`/`emhash6`/`emhash7`/`emhash8` | `emhash/hash_table5.hpp` … `hash_table8.hpp` |
| emilib HashMap | `emilib` / `emilib2` / `emilib3` | `emilib/emihmap1.hpp` / `emihmap2.hpp` / `emihmap3.hpp` |
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
