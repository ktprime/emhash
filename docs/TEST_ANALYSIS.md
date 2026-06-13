# emhash Test Code Analysis Document

This document organizes all test code in `fuzz/`, `test/`, and `bench/` directories, explaining the purpose and test type of each file.

---

## 1. fuzz/ Directory — Fuzzing and Debug Tests

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

## 2. test/ Directory — Unit Tests and Functional Verification

### 2.1 Comprehensive Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_emhash58.cpp` | Unit | emhash5/6/7/8 | Comprehensive test: CRUD, copy/move, iterator, rehash, boundary cases |
| `main.cpp` | Unit | All versions | Main test entry, integrated test suite |
| `hash_map_tests.cpp` | Unit | All versions | Historical test code |

### 2.2 emhash5 Specific Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_emhash5_verify.cpp` | Unit | emhash5 | Verification test: basic operations, random deletion, rehash loop |
| `test_emhash5_stress.cpp` | Stress | emhash5 | Stress test: insert/delete loop, INACTIVE key test |
| `test_emhash5_hifi.cpp` | Stress | emhash5 | High-intensity test: large-scale random operations, hash attack, iteration deletion |

### 2.3 emilib2 Test Series

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_emilib2o.cpp` | Unit | emilib2o | Basic insertion test |
| `test_emilib2o_basic.cpp` | Unit | emilib2o | Basic insert/find/delete test |
| `test_emilib2o_hang.cpp` | Stress | emilib2o | Loop insert/delete, verifies no infinite loop |
| `test_emilib2o_interleaved.cpp` | Stress | emilib2o | Interleaved insert/delete test |
| `test_emilib2o_stress.cpp` | Stress | emilib2o | Stress test |
| `test_emilib2s.cpp` | Unit | emilib2s | emilib2s version test |
| `test_emilib_comprehensive.cpp` | Unit | emilib | Comprehensive test |

### 2.4 Boundary and Bug Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_extreme.cpp` | Edge | emhash5/8 | Extreme test: various integer keys, boundary values, clear/redistribute |
| `test_interface_combo.cpp` | Unit | emilib series | Generic interface test, string key/value edge cases |
| `test_probe_bug.cpp` | Bug | Probe mode | Test probe mode bugs |
| `test_probe_coverage.cpp` | Coverage | Probe strategy | Verify coverage of different probe strategies |
| `test_repro_collision.cpp` | Bug | Hash collision | Reproduce hash collision issues |
| `test_find_hit_bug*.cpp` | Bug | EMH_FIND_HIT | Test find_hit related bugs |
| `test_find_hit_ok.cpp` | Verify | EMH_FIND_HIT | Verify correctness after fix |

### 2.5 Other Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_size_sweep.cpp` | Unit | Size adjustment | Test table behavior at different sizes |
| `test_steps.cpp` | Unit | Step | Test impact of insert/delete order |
| `test_struct_layout.cpp` | Unit | Struct layout | Verify struct layout compatibility |
| `test_bad_hash_stress.cpp` | Stress | Bad hash | Test behavior under bad hash functions |
| `test_allocator.cpp` | Unit | Custom allocator | Test custom allocator support |
| `test_coverage_detail.cpp` | Coverage | Coverage | Detailed coverage test |

---

## 3. bench/ Directory — Performance Benchmarks and Verification Tests

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
./tests_new/scripts/build_tests.sh quick

# Build stress tests (with ASan)
./tests_new/scripts/build_tests.sh stress

# Build hash attack tests
./tests_new/scripts/build_tests.sh attack

# Build all tests
./tests_new/scripts/build_tests.sh all

# Build and run quick tests
./tests_new/scripts/build_tests.sh run

# Build with clang (fuzzer requires)
./tests_new/scripts/build_tests.sh fuzz clang

# Clean
./tests_new/scripts/build_tests.sh clean
```

### Windows PowerShell Environment

```powershell
# Quick build
.\tests_new\scripts\build_tests.ps1 quick

# Stress tests
.\tests_new\scripts\build_tests.ps1 stress

# Build and run
.\tests_new\scripts\build_tests.ps1 run

# Clean
.\tests_new\scripts\build_tests.ps1 clean
```

### Using Makefile

```bash
# Build and run all quick tests
cd tests_new && make quick

# Build specific test
cd tests_new && make test_verify
```

### Using CMake

```bash
cd tests_new
mkdir build && cd build
cmake ..
cmake --build . --target quick_test
```

---

## 5. Test Classification Summary

| Category | File Count | Main Directory | Key Tests |
|----------|------------|----------------|-----------|
| **Fuzzing** | 10+ | fuzz/ | fuzz_emhash_all, fuzz_extreme |
| **Unit Tests** | 20+ | test/ | test_emhash58, test_emhash5_verify |
| **Stress Tests** | 8 | test/, bench/ | test_emhash5_hifi, stress_fix |
| **Bug Reproduction** | 15+ | fuzz/, bench/ | debug_chain, reproduce_crash |
| **Performance Benchmarks** | 12 | bench/ | ebench, martin_bench |
| **Hash Attack** | 2 | bench/ | hash_attack, hash_attack7 |

---

## 6. Test Coverage Matrix

| Test Type | emhash5 | emhash6 | emhash7 | emhash8 | emilib |
|-----------|---------|---------|---------|---------|--------|
| Fuzzing | ✓ | ✓ | ✓ | ✓ | ✓ |
| Unit Tests | ✓ | ✓ | ✓ | ✓ | ✓ |
| Stress Tests | ✓ | - | - | ✓ | ✓ |
| High Load | ✓ | ✓ | ✓ | ✓ | - |
| Hash Attack | ✓ | - | ✓ | - | - |
| Bug Reproduction | ✓ | - | - | ✓ | ✓ |

---

## 7. Recommended Test Order

1. **Quick Validation**: `quick_test8` → `test_hidden_bugs8` → `highload_test`
2. **Unit Tests**: `test_emhash58` → `test_emhash5_verify` → `test_extreme`
3. **Stress Tests**: `stress_fix_asan` → `test_emhash5_hifi_asan`
4. **Fuzzing**: `fuzz_extreme_asan` (60s) → `fuzz_emhash8` (requires clang)
5. **Performance Benchmarks**: `ebench` → `martin_bench` → `highload_bench`

---

## 8. Unified Test Directory

A unified test directory `tests_new/` has been created for easier maintenance:

```
tests_new/
├── fuzz/      # Fuzzing tests
├── stress/    # Stress tests
├── debug/     # Debug tools
├── verify/    # Quick validation tests
├── attack/    # Hash attack tests
├── bench/     # Performance benchmarks
├── scripts/   # Build scripts
└── README.md  # Documentation
```

See `tests_new/README.md` for detailed usage instructions.