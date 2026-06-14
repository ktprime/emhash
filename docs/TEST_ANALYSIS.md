# emhash Test Code Analysis Document

This document organizes all test code in `tests/` directory, explaining the purpose and test type of each file.

---

## 1. tests/fuzz/ — Fuzzing and Debug Tests

### 1.1 Core Fuzzers

| File | Type | Target | Description |
|------|------|--------|-------------|
| `fuzz_emhash_all.cpp` | Fuzzer | emhash5/6/7/8 + HashSet | Comprehensive test for all versions, supports multiple operations, custom hash, high load factor |
| `fuzz_emhash8.cpp` | Fuzzer | emhash8 | Basic fuzzer for emhash8, compares with std::unordered_map |
| `fuzz_emhash8_advanced.cpp` | Fuzzer | emhash8 | Advanced fuzzer with variable hasher seed, tests hash collision scenarios |
| `fuzz_extreme.cpp` | Fuzzer | All versions | Extreme tests: 100% collision, high load factor, intense insert/delete/query |
| `fuzz_nocoll.cpp` | Fuzzer | All versions | No-collision boundary tests, verifies consistency |
| `fuzz_emilib.cpp` | Fuzzer | emilib third-party library | Tests third-party emilib HashMap implementation correctness |

### 1.2 Debug Tools

| File | Type | Target | Description |
|------|------|--------|-------------|
| `debug_chain.cpp` | Debug | emhash8 chain | Debug `_index` chain corruption issues |
| `debug_set_erase.cpp` | Debug | emhash8 HashSet | Debug erase_slot chain corruption |
| `debug_rehash.cpp` | Debug | emhash8 rehash | Debug rehash path issues |
| `debug_emilib2o*.cpp` | Debug | emilib2o | Multiple debug files for specific emilib2o issues |
| `min_repro.cpp` | Debug | Crash reproduction | Minimal reproduction of specific crash scenarios |
| `calc_hash.py` | Script | Helper tool | Python script for hash value calculation |

### 1.3 Fuzzer Output

| Directory/File | Description |
|----------------|-------------|
| `corpus/` | Fuzzer seed input library (thousands of test cases) |
| `crashes/` | Discovered crash input files |
| `crash-*` | Specific crash reproduction files |

---

## 2. tests/ — Unit Tests and Functional Verification

### 2.1 verify/ — Comprehensive Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_emhash58.cpp` | Unit | emhash5/6/7/8 | Comprehensive test: CRUD, copy/move, iterator, rehash, boundary cases |
| `main.cpp` | Unit | All versions | Main test entry, integrated test suite |
| `hash_map_tests.cpp` | Unit | All versions | Historical test code |

### 2.2 stress/ — Stress Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `stress_all_maps.cpp` | Stress | All 7 maps | Insert/delete loop across all implementations |
| `test_emhash5_hifi.cpp` | Stress | emhash5 | High-intensity test: large-scale random operations, hash attack, iteration deletion |
| `highload_test.cpp` | Stress | emhash5/8 | High load test: LF=0.999, oscillation test |
| `test_bad_hash_stress.cpp` | Stress | Bad hash | Test behavior under bad hash functions |
| `stress_fix.cpp` | Stress | emhash8 | Stress test: reserve(1) + random operations, verifies crash fix |

### 2.3 debug/ — Boundary and Bug Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `debug_all_maps.cpp` | Debug | All 7 maps | Comprehensive debug test suite |
| `debug_chain.cpp` | Debug | emhash8 chain | Debug `_index` chain corruption issues |
| `debug_set_erase.cpp` | Debug | emhash8 HashSet | Debug erase_slot chain corruption |
| `min_repro.cpp` | Debug | Crash reproduction | Minimal reproduction of specific crash scenarios |
| `reproduce_crash.cpp` | Debug | emhash8 | Reproduce specific crash: reserve(1) + 0x68686868 key |
| `fuzz_reproduce.cpp` | Debug | emhash8 | Reproduce fuzzer-discovered crash sequence |
| `test_debug_find.cpp` | Debug | EMH_FIND_HIT | Test find_hit related bugs |
| `test_probe_bug.cpp` | Bug | Probe mode | Test probe mode bugs |
| `test_probe_coverage.cpp` | Coverage | Probe strategy | Verify coverage of different probe strategies |
| `test_repro_collision.cpp` | Bug | Hash collision | Reproduce hash collision issues |

### 2.4 verify/ — Boundary and Bug Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_extreme.cpp` | Edge | emhash5/8 | Extreme test: various integer keys, boundary values, clear/redistribute |
| `test_interface_combo.cpp` | Unit | emilib series | Generic interface test, string key/value edge cases |
| `test_probe_bug.cpp` | Bug | Probe mode | Test probe mode bugs |
| `test_probe_coverage.cpp` | Coverage | Probe strategy | Verify coverage of different probe strategies |
| `test_repro_collision.cpp` | Bug | Hash collision | Reproduce hash collision issues |
| `test_find_hit_bug*.cpp` | Bug | EMH_FIND_HIT | Test find_hit related bugs |
| `test_find_hit_ok.cpp` | Verify | EMH_FIND_HIT | Verify correctness after fix |

### 2.5 attack/ — Hash Attack Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `hash_attack_all.cpp` | Attack | All 7 maps | Comprehensive hash attack: constant hash, small range hash, linear hash |
| `hash_attack.cpp` | Attack | emhash5 | Hash attack benchmark: constant hash, small range hash, linear hash |
| `hash_attack7.cpp` | Attack | emhash7 | emhash7 hash attack test |

### 2.6 tests/bench/ — Benchmark Test Files

| File | Type | Target | Description |
|------|------|--------|-------------|
| `ebench.cpp` | Perf | All versions | Basic benchmark: insert/find/erase/iterate |
| `martin_bench.cpp` | Perf | All versions | Martin Ankerl format benchmark |
| `highload_bench.cpp` | Perf | All versions | High load benchmark |

---

## 3. bench/ — Legacy Performance Benchmarks

Files in the root `bench/` directory are legacy benchmarks and analysis tools.
The main benchmark runners are `bench/ebench.cpp` and `tests/bench/ebench.cpp`
(these share most code; the `tests/bench/` copy is preferred).

### 3.1 Quick Validation Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `quick_test8.cpp` | Verify | emhash8 | Quick correctness validation, compares with std::unordered_map |
| `test_hidden_bugs8.cpp` | Bug | emhash8 | Test hidden bugs: hash high-bit collision, kickout chain integrity, etc. |
| `highload_test.cpp` | Stress | emhash5/8 | High load test: LF=0.999, oscillation test |
| `smallsize_test.cpp` | Unit | emhash5 | Small size optimization test: _small buffer, swap, shrink_to_fit |

### 3.2 Crash Reproduction and Stress Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `stress_fix.cpp` | Stress | emhash8 | Stress test: reserve(1) + random operations, verifies crash fix |
| `fuzz_reproduce.cpp` | Debug | emhash8 | Reproduce fuzzer-discovered crash sequence |
| `reproduce_crash.cpp` | Debug | emhash8 | Reproduce specific crash: reserve(1) + 0x68686868 key |
| `analyze_crash.cpp`, `analyze_crash_deep.cpp` | Debug | Crash analysis | Helper tools for crash root cause analysis |

### 3.3 Hash Attack Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `hash_attack.cpp` | Attack | emhash5 | Hash attack benchmark: constant hash, small range hash, linear hash |
| `hash_attack7.cpp` | Attack | emhash7 | emhash7 hash attack test |

### 3.4 Performance Benchmarks

| File | Type | Target | Description |
|------|------|--------|-------------|
| `ebench.cpp` | Perf | All versions | Basic benchmark: insert/find/erase/iterate |
| `hbench.cpp` | Perf | All versions | Hash table benchmark |
| `tbench.cpp` | Perf | All versions | Threading benchmark |
| `martin_bench.cpp` | Perf | All versions | Martin Ankerl format benchmark |
| `highload_bench.cpp` | Perf | All versions | High load benchmark |
| `bench_find_hit.cpp` | Perf | Find hit rate | Find hit rate benchmark |

### 3.5 Other Benchmarks

| File | Type | Target | Description |
|------|------|--------|-------------|
| `bstring.cpp` | Perf | String key | String key performance test |
| `buint64.cpp` | Perf | uint64 key | uint64 key performance test |
| `hash_join.cpp` | Perf | Hash join | Hash join operation test |
| `word_count.cpp` | Perf | Word count | Word frequency count test |

---

## 4. Build Script Usage

### Linux/WSL Environment

```bash
# Quick build validation tests
./tests/scripts/build_tests.sh quick

# Build stress tests (with ASan)
./tests/scripts/build_tests.sh stress

# Build hash attack tests
./tests/scripts/build_tests.sh attack

# Build all tests
./tests/scripts/build_tests.sh all

# Build and run quick tests
./tests/scripts/build_tests.sh run

# Build with clang (fuzzer requires)
./tests/scripts/build_tests.sh fuzz clang

# Clean
./tests/scripts/build_tests.sh clean
```

### Windows PowerShell Environment

```powershell
# Quick build
.\tests\scripts\build_tests.ps1 quick

# Stress tests
.\tests\scripts\build_tests.ps1 stress

# Build and run
.\tests\scripts\build_tests.ps1 run

# Clean
.\tests\scripts\build_tests.ps1 clean
```

### Using Makefile

```bash
# Build and run all quick tests
cd tests && make quick

# Build specific test
cd tests && make test_verify
```

### Using CMake

```bash
cd tests
mkdir build && cd build
cmake ..
cmake --build . --target quick_test
```

---

## 5. Test Classification Summary

| Category | File Count | Location | Key Tests |
|----------|------------|----------|-----------|
| **Fuzzing** | 8 | tests/fuzz/ | fuzz_emhash_all, fuzz_extreme |
| **Unit Tests** | 15+ | tests/verify/ | test_all_maps, test_emhash58, test_extreme |
| **Stress Tests** | 6 | tests/stress/ | stress_all_maps, stress_fix |
| **Bug Reproduction** | 12+ | tests/debug/ | debug_chain, reproduce_crash |
| **Performance Benchmarks** | 20+ | bench/, tests/bench/ | ebench, martin_bench |
| **Hash Attack** | 3 | tests/attack/ | hash_attack_all, hash_attack7 |

---

## 6. Test Coverage Matrix

| Test Type | emhash5 | emhash6 | emhash7 | emhash8 | emilib2ss | emilib2o | emilib2s |
|-----------|---------|---------|---------|---------|-----------|----------|----------|
| Fuzzing | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Unit Tests | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Stress Tests | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| High Load | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Hash Attack | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Bug Reproduction | Yes | - | - | Yes | Yes | - | - |

---

## 7. Recommended Test Order

1. **Quick Validation**: `cd tests && make quick` (~20s)
2. **Stress Tests**: `cd tests && make stress` (~1min)
3. **Hash Attack**: `cd tests && make attack` (~2min)
4. **Fuzzing (ASan)**: `cd tests && make fuzz` (non-fuzzer variant)
5. **Performance Benchmarks**: `cd tests && make bench`

---

## 8. Unified Test Directory

A unified test directory `tests/` has been created for easier maintenance:

```
tests/
├── fuzz/      # Fuzzing tests
├── stress/    # Stress tests
├── debug/     # Debug tools
├── verify/    # Quick validation tests
├── attack/    # Hash attack tests
├── bench/     # Performance benchmarks
├── scripts/   # Build scripts
└── README.md  # Documentation
```

See `tests/README.md` for detailed usage instructions.