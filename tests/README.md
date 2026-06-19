# emhash Test Suite

This directory contains the test suite for the emhash project, organized by functionality.
All tests depend only on emhash/emilib headers — **no third-party dependencies required**.

---

## Quick Start

### Using CMake (Recommended)

```bash
cd tests

# Configure
cmake -B build

# Build all tests
cmake --build build --config Release

# Run quick tests
cmake --build build --target quick_test

# Run stress tests
cmake --build build --target stress_test

# Run attack tests
cmake --build build --target attack_test

# Run all tests
cmake --build build --target all_tests

# Show available targets
cmake --build build --target emhash_test_help

# Enable fuzz tests (requires clang)
cmake -B build -DENABLE_FUZZ_TESTS=ON
cmake --build build
```

### Manual Compilation (Linux/WSL)

```bash
cd tests

# Quick validation tests (~10 seconds)
g++ -std=c++17 -O2 -I../include verify/test_all_maps.cpp -o test_verify && ./test_verify

# Stress tests (~30 seconds)
g++ -std=c++17 -O2 -I../include stress/stress_all_maps.cpp -o test_stress && ./test_stress

# Hash attack tests (~2 minutes, requires EMH_SAFE_PSL)
g++ -std=c++17 -O2 -DEMH_SAFE_PSL -I../include attack/hash_attack_all.cpp -o test_attack && ./test_attack

# Debug tests (~10 seconds)
g++ -std=c++17 -g -O0 -I../include debug/debug_all_maps.cpp -o test_debug && ./test_debug

# Fuzz tests (requires clang+libfuzzer; source files are generated locally)
clang++ -fsanitize=fuzzer,address -std=c++17 -I../include fuzz/fuzz_emhash8.cpp -o test_fuzz
./test_fuzz -max_total_time=60
```

### Windows (MSVC)

```powershell
cd tests

# Quick validation tests
cl /std:c++17 /O2 /I../include verify\test_all_maps.cpp /Fe:test_verify.exe && .\test_verify.exe

# Stress tests
cl /std:c++17 /O2 /I../include stress\stress_all_maps.cpp /Fe:test_stress.exe && .\test_stress.exe

# Hash attack tests (requires EMH_SAFE_PSL)
cl /std:c++17 /O2 /DEMH_SAFE_PSL /I../include attack\hash_attack_all.cpp /Fe:test_attack.exe && .\test_attack.exe
```

---

## Test Coverage

| Test File | Coverage | Assertions | Time |
|-----------|----------|------------|------|
| `verify/test_all_maps.cpp` | emhash5-8 + emihmap1/2/3 | 247,268 | ~5s |
| `stress/stress_all_maps.cpp` | 7 maps x 5 items x 1000 trials | 35,000 trials | ~30s |
| `attack/hash_attack_all.cpp` | 7 maps x 3 hash attacks | correctness+performance | ~2min |
| `debug/debug_all_maps.cpp` | 7 maps x 10 debug tests | 70 tests | ~10s |
| `fuzz/fuzz_emhash8.cpp` | emhash8 fuzzing (requires clang, generated locally) | - | - |

---

## Directory Structure

```
tests/
├── verify/            # Quick validation tests (no third-party deps)
├── stress/            # Stress tests (with ASan)
├── debug/             # Debug tools
├── attack/            # Hash attack tests
├── fuzz/              # Fuzzing tests (requires clang + libfuzzer)
├── scripts/           # Build scripts (build_tests.sh, build_tests.ps1, run_all.sh)
├── CMakeLists.txt     # CMake build configuration
└── README.md          # This document
```

> **Note**: Performance benchmarks have been moved to the project-level [`bench/`](../bench/) directory, which requires third-party dependencies. The `tests/` directory is fully self-contained.

---

## 1. verify/ — Quick Validation Tests

Comprehensive validation tests for all emhash versions. No third-party dependencies.

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `test_all_maps.cpp` | All 7 maps validation (247K assertions) | `g++ -std=c++17 -O2 -I../include test_all_maps.cpp -o test_verify` |
| `test_hashmap_full_api.cpp` | Full API coverage for all HashMap versions | `g++ -std=c++17 -O2 -I../include test_hashmap_full_api.cpp -o test_hashmap_api` |
| `test_hashset_full_api.cpp` | Full API coverage for all HashSet versions | `g++ -std=c++17 -O2 -I../include test_hashset_full_api.cpp -o test_hashset_api` |
| `test_special_key_types.cpp` | Special key types: float, string, move-only, large values | `g++ -std=c++17 -O2 -I../include test_special_key_types.cpp -o test_special_keys` |
| `test_merge_correctness.cpp` | Merge operation correctness | `g++ -std=c++17 -O2 -I../include test_merge_correctness.cpp -o test_merge` |
| `test_interface_combo.cpp` | Interface combination tests | `g++ -std=c++17 -O2 -I../include test_interface_combo.cpp -o test_interface` |
| `test_emhash_allocator.cpp` | Custom allocator support | `g++ -std=c++17 -O2 -I../include test_emhash_allocator.cpp -o test_alloc` |
| `test_hashset_allocator.cpp` | HashSet custom allocator | `g++ -std=c++17 -O2 -I../include test_hashset_allocator.cpp -o test_set_alloc` |
| `test_emilib_comprehensive.cpp` | emilib1/2/3 comprehensive tests | `g++ -std=c++17 -O2 -I../include test_emilib_comprehensive.cpp -o test_emilib` |
| `test_size_sweep.cpp` | Size sweep tests | `g++ -std=c++17 -O2 -I../include test_size_sweep.cpp -o test_sweep` |
| `test_functional_edge.cpp` | Functional edge cases | `g++ -std=c++17 -O2 -I../include test_functional_edge.cpp -o test_edge` |
| `test_extreme.cpp` | Extreme boundary tests | `g++ -std=c++17 -O2 -I../include test_extreme.cpp -o test_extreme` |
| `test_hidden_bugs8.cpp` | Hidden bug tests (7 items) | `g++ -std=c++17 -O2 -I../include test_hidden_bugs8.cpp -o test_hidden` |
| `test_emhash58.cpp` | Comprehensive tests (5/6/7/8) | `g++ -std=c++17 -O2 -I../include test_emhash58.cpp -o test_58` |
| `test_emhash5_verify.cpp` | emhash5 verification | `g++ -std=c++17 -O2 -I../include test_emhash5_verify.cpp -o test_verify5` |
| `quick_test8.cpp` | emhash8 quick validation | `g++ -std=c++17 -O2 -I../include quick_test8.cpp -o quick_test8` |

**Run Examples**:
```bash
./test_verify          # 247,268 assertions for all maps
./test_hashmap_api     # Full HashMap API coverage
./test_special_keys    # Special key type tests
```

---

## 2. stress/ — Stress Tests

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `stress_all_maps.cpp` | All 7 maps stress test | `g++ -std=c++17 -O2 -I../include stress_all_maps.cpp -o test_stress` |
| `stress_fix.cpp` | Verify reserve(1) crash fix | `g++ -fsanitize=address,undefined -std=c++17 -I../include stress_fix.cpp -o stress_fix_asan` |
| `highload_test.cpp` | High load tests (LF=0.999) | `g++ -std=c++17 -O2 -I../include highload_test.cpp -o highload_test` |
| `test_emhash5_hifi.cpp` | High-intensity random operations | `g++ -fsanitize=address,undefined -std=c++17 -I../include test_emhash5_hifi.cpp -o hifi_asan` |
| `test_emhash5_stress.cpp` | emhash5 stress test | `g++ -fsanitize=address,undefined -std=c++17 -I../include test_emhash5_stress.cpp -o stress5_asan` |
| `test_bad_hash_stress.cpp` | Bad hash stress tests | `g++ -std=c++17 -O2 -I../include test_bad_hash_stress.cpp -o test_bad_hash` |

**Run Examples**:
```bash
./test_stress           # 35,000 trials for all maps
./stress_fix_asan       # 11000 random stress tests
./highload_test         # LF oscillation tests
```

---

## 3. attack/ — Hash Attack Tests

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `hash_attack_all.cpp` | All 7 maps hash attack | `g++ -std=c++17 -O2 -DEMH_SAFE_PSL -I../include hash_attack_all.cpp -o test_attack` |
| `hash_attack.cpp` | emhash5 hash attack | `g++ -std=c++17 -O2 -DEMH_SAFE_PSL -I../include hash_attack.cpp -o hash_attack5` |
| `hash_attack7.cpp` | emhash7 hash attack | `g++ -std=c++17 -O2 -DEMH_SAFE_PSL -I../include hash_attack7.cpp -o hash_attack7` |

**Test Scenarios**:
- Constant hash (all keys map to same bucket)
- Small range hash (4 buckets)
- Linear hash (key % 4)

**Run Examples**:
```bash
./test_attack          # All 7 maps attack test (requires EMH_SAFE_PSL)
./hash_attack5         # emhash5 attack test
```

---

## 4. debug/ — Debug Tools

| File | Purpose | Use Case |
|------|---------|----------|
| `debug_all_maps.cpp` | Debug tests for all 7 maps | `g++ -std=c++17 -g -O0 -I../include debug_all_maps.cpp -o test_debug` |
| `debug_chain.cpp` | Debug `_index` chain corruption | Analyze kickout_bucket issues |
| `debug_set_erase.cpp` | Debug erase_slot issues | HashSet deletion chain breakage |
| `min_repro.cpp` | Minimal crash reproduction | Identify specific crash root cause |
| `reproduce_crash.cpp` | Reproduce fuzzer-discovered crash | Verify fix effectiveness |
| `fuzz_reproduce.cpp` | Replay crash input file | Parse fuzzer crash files |
| `test_probe_bug.cpp` | Probe sequence debugging | `g++ -std=c++17 -g -O0 -I../include test_probe_bug.cpp -o test_probe` |
| `test_probe_coverage.cpp` | Probe coverage analysis | `g++ -std=c++17 -g -O0 -I../include test_probe_coverage.cpp -o test_probe_cov` |
| `test_debug_find.cpp` | Find operation debugging | `g++ -std=c++17 -g -O0 -I../include test_debug_find.cpp -o test_find` |
| `test_repro_collision.cpp` | Collision reproduction | `g++ -std=c++17 -g -O0 -I../include test_repro_collision.cpp -o test_coll` |

---

## 5. fuzz/ — Fuzzing Tests

> **Note**: Most fuzz source files (`fuzz_*.cpp`) are not currently tracked in git.
> They are generated during fuzzing sessions and stored locally. Only `reproduce_emhash8_bug.cpp`
> is tracked. To add fuzz tests, create `fuzz_*.cpp` files and enable `ENABLE_FUZZ_TESTS` in CMake.

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `reproduce_emhash8_bug.cpp` | Reproduce emhash8 bug from fuzzer | `g++ -std=c++17 -I../include reproduce_emhash8_bug.cpp -o reproduce_bug` |

**Planned fuzz targets** (create these files to enable):

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `fuzz_emhash_all.cpp` | Test all emhash versions (5/6/7/8) | `clang++ -fsanitize=fuzzer,address -std=c++17 -I../include fuzz_emhash_all.cpp -o fuzz_all` |
| `fuzz_emilib_all.cpp` | emihmap1/2/3 fuzzing | `clang++ -fsanitize=fuzzer,address -std=c++17 -I../include fuzz_emilib_all.cpp -o test_fuzz` |
| `fuzz_extreme.cpp` | Extreme tests: 100% collision, high LF | `g++ -fsanitize=address,undefined -std=c++17 -I../include fuzz_extreme.cpp fuzz_main.cpp -o fuzz_extreme_asan` |

**Run Examples**:
```bash
# Reproduce known bug
./reproduce_bug

# Fuzzer (requires clang + fuzz source files)
./fuzz8 -max_total_time=60 corpus/
```

---

## 6. scripts/ — Build Scripts

### build_tests.sh (Linux/WSL)

```bash
# Build quick validation tests
./scripts/build_tests.sh quick

# Build stress tests (with ASan)
./scripts/build_tests.sh stress

# Build all tests
./scripts/build_tests.sh all

# Build and run
./scripts/build_tests.sh run
```

### build_tests.ps1 (Windows)

```powershell
# Quick build
.\scripts\build_tests.ps1 quick

# Stress tests
.\scripts\build_tests.ps1 stress

# Build and run
.\scripts\build_tests.ps1 run
```

### run_all.sh (Unified Runner)

```bash
# Quick validation tests
./scripts/run_all.sh quick

# Stress tests
./scripts/run_all.sh stress

# Fuzzing tests (60 seconds)
./scripts/run_all.sh fuzz

# All tests
./scripts/run_all.sh all

# Cleanup
./scripts/run_all.sh clean
```

---

## 7. Test Coverage Matrix

| Test Type | emhash5 | emhash6 | emhash7 | emhash8 | emilib1 | emilib2 | emilib3 |
|-----------|---------|---------|---------|---------|---------|---------|---------|
| Fuzzing | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Stress | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| High Load | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Hash Attack | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Validation | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Full API | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Special Keys | Yes | Yes | Yes | Yes | - | - | - |
| Allocator | Yes | Yes | Yes | Yes | - | - | - |

---

## 8. Recommended Test Order

1. **Quick Validation** (seconds):
   ```bash
   ./test_verify          # All 7 maps, 247,268 assertions
   ./test_debug           # Debug tests
   ```

2. **Stress Tests** (minutes):
   ```bash
   ./test_stress          # 35,000 trials
   ./stress_fix_asan      # reserve(1) fix verification
   ./highload_test        # LF oscillation tests
   ```

3. **Hash Attack** (minutes):
   ```bash
   ./test_attack          # Requires EMH_SAFE_PSL
   ```

4. **Performance Benchmarks** (see `bench/` directory):
   ```bash
   # Benchmarks are in the project-level bench/ directory
   # See bench/README.md for details
   ```

---

## 9. Notes

1. **No third-party dependencies**: All tests in `tests/` depend only on emhash/emilib headers
2. **Fuzzer requires clang + libfuzzer**, gcc cannot compile libfuzzer entry point
3. **ASan tests should not use -O3**, use -O2 or -g
4. **High load tests require `-DEMH_HIGH_LOAD=123456`** compile flag
5. **Hash attack tests require `-DEMH_SAFE_PSL`** for emilib to handle extreme collisions
6. **Windows users can use MSVC or WSL** for compilation and execution
7. **Performance benchmarks** are in the project-level `bench/` directory, which requires third-party dependencies
