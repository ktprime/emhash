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
| `test_all_maps.cpp` | Unit | All versions | Main validation test, 247K+ assertions |
| `quick_test8.cpp` | Unit | emhash8 | Quick correctness validation, compares with std::unordered_map |

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

### 2.4 verify/ — Edge Case and Bug Verification Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `test_extreme.cpp` | Edge | emhash5/8 | Extreme test: various integer keys, boundary values, clear/redistribute |
| `test_interface_combo.cpp` | Unit | emilib series | Generic interface test, string key/value edge cases |

### 2.5 attack/ — Hash Attack Tests

| File | Type | Target | Description |
|------|------|--------|-------------|
| `hash_attack_all.cpp` | Attack | All 7 maps | Comprehensive hash attack: constant hash, small range hash, linear hash |
| `hash_attack.cpp` | Attack | emhash5 | Hash attack benchmark: constant hash, small range hash, linear hash |
| `hash_attack7.cpp` | Attack | emhash7 | emhash7 hash attack test |

## 3. bench/ — Performance Benchmarks

Files in the root `bench/` directory are performance benchmarks and analysis tools.
The main benchmark runners are `bench/ebench.cpp`, `bench/comprehensive_bench.cpp`
and `bench/martin_bench.cpp`. These require third-party dependencies in `thirdparty/`.

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
| **Performance Benchmarks** | 20+ | bench/ | ebench, martin_bench |
| **Hash Attack** | 3 | tests/attack/ | hash_attack_all, hash_attack7 |

---

## 6. Test Coverage Matrix

| Test Type | emhash5 | emhash6 | emhash7 | emhash8 | emihmap1 | emihmap2 | emihmap3 |
|-----------|---------|---------|---------|---------|-----------|----------|----------|
| Fuzzing | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Unit Tests | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Stress Tests | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| High Load | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Hash Attack | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Bug Reproduction | Yes | - | - | Yes | Yes | - | - |

---

## 7. Recommended Test Order

1. **Quick Validation**: `cd tests && cmake -B build && cmake --build build --target quick_test` (~20s)
2. **Stress Tests**: `cmake --build build --target stress_test` (~1min)
3. **Hash Attack**: `cmake --build build --target attack_test` (~2min)
4. **Fuzzing (ASan)**: `./tests/scripts/build_tests.sh fuzz` (non-fuzzer variant, requires clang)
5. **Performance Benchmarks**: `cd bench && make`

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
├── bench/     # Performance benchmarks (root directory)
├── scripts/   # Build scripts
└── README.md  # Documentation
```

---

## 9. Code Coverage Report

Coverage is generated by the `coverage` job in CI using **gcov + lcov** (GCC only).
The configuration lives in `.github/workflows/ci.yml` (job: `coverage`).

### 9.1 How to Reproduce Locally

```bash
# Build & run with coverage flags
cd tests
mkdir -p cov_build && cd cov_build
COV='-std=c++17 -O0 -g --coverage -fprofile-arcs -ftest-coverage -I.. -I../../include'
g++ $COV verify/test_all_maps.cpp            -o test_verify_cov
g++ $COV verify/test_extreme.cpp             -o test_extreme_cov
g++ $COV verify/test_string_key_leak.cpp     -o test_string_key_leak_cov
g++ $COV verify/test_hashset_full_api.cpp    -o test_hashset_cov
g++ $COV verify/test_emilib_comprehensive.cpp -o test_emilib_cov
g++ $COV stress/stress_fix.cpp               -o test_stress_cov
g++ $COV stress/stress_all_maps.cpp          -o test_stress_all_cov
g++ $COV -DEMH_SAFE_PSL attack/hash_attack_all.cpp -o hash_attack_all_cov

# Run all
for t in test_verify_cov test_extreme_cov test_string_key_leak_cov test_hashset_cov \
         test_emilib_cov test_stress_cov test_stress_all_cov hash_attack_all_cov; do
  ./$t
done

# Generate report
lcov --capture --directory . --output-file coverage.info \
     --include '*/include/emhash/hash_*' \
     --include '*/include/emilib/*' \
     --exclude '*/include/emilib/sse2neon.h'
genhtml coverage.info --output-directory coverage_html --title "emhash coverage"
lcov --list coverage.info
```

### 9.2 Overall Coverage (CI: ubuntu-latest, GCC)

| Metric    | Rate  | Covered / Total |
|-----------|-------|-----------------|
| **Lines**     | **85.8%** | 4,718 / 5,502 |
| **Functions** | **87.7%** | 3,124 / 3,561 |
| **Branches**  | n/a (gcov without `-b`) | — |

> Note: branch coverage requires recompiling with `--coverage -b`; the current
> CI configuration measures line + function coverage.

### 9.3 Per-File Coverage (Latest Run)

| File | Line Coverage | Lines Covered/Total | Function Coverage | Functions Covered/Total |
|------|---------------|---------------------|-------------------|-------------------------|
| `emilib/emihmap1.hpp` | **99.6%** | 443 / 445 | 95.0% | 361 / 380 |
| `emilib/emihmap3.hpp` | **99.1%** | 436 / 440 | 92.4% | 340 / 368 |
| `emilib/emihmap2.hpp` | **98.7%** | 459 / 465 | 92.4% | 354 / 383 |
| `emhash/hash_table5.hpp` | **96.7%** | 440 / 455 | 91.9% | 614 / 668 |
| `emhash/hash_table8.hpp` | **94.6%** | 452 / 478 | 92.2% | 390 / 423 |
| `emhash/hash_table7.hpp` | **93.1%** | 469 / 504 | 75.4% | 362 / 480 |
| `emhash/hash_table6.hpp` | **88.8%** | 477 / 537 | 74.2% | 284 / 383 |
| `emilib/emihset2.hpp`  | **81.7%** | 215 / 263 | 95.5% | 42 / 44 |
| `emilib/emihset3.hpp`  | **77.7%** | 233 / 300 | 88.1% | 52 / 59 |
| `emhash/hash_set4.hpp` | **74.4%** | 305 / 410 | 90.8% | 89 / 98 |
| `emhash/hash_set8.hpp` | **73.6%** | 329 / 447 | 82.6% | 114 / 138 |
| `emhash/hash_set2.hpp` | **68.7%** | 233 / 339 | 90.6% | 77 / 85 |
| `emhash/hash_set3.hpp` | **54.2%** | 227 / 419 | 86.5% | 45 / 52 |

### 9.4 Coverage Hot-Spots and Gaps

| Status | Component | Comment |
|--------|-----------|---------|
| Excellent (>95%) | emihmap1/2/3, hash_table5 | Mature core paths exercised by every test |
| Good (90–95%) | hash_table7/8 | High-traffic insert/find/erase paths covered |
| Acceptable (75–90%) | hash_table6, emihset2/3 | Some SIMD/specialized branches uncovered in -O0 build |
| Needs work (<75%) | hash_set2/3/4/8 | HashSet operations have less test coverage; targeted tests recommended |

### 9.5 CI Integration

- Workflow: `.github/workflows/ci.yml` → `coverage` job
- Trigger: push to master / pull_request
- Artifacts:
  - `coverage-html/` — full browsable HTML report (30 days retention)
  - `coverage.info` — raw LCOV file (30 days retention)
- Step summary: total line coverage percentage published to the run page

### 9.6 Files Not Measured

The following files are intentionally excluded from the coverage report
(typically non-portable / platform-specific code):

- `include/emilib/sse2neon.h` (ARM NEON → SSE2 shim)

LRU headers (`lru_size.hpp`, `lru_time.hpp`) and `config.hpp` are header-only
helpers and are not separately measured; they are exercised indirectly by the
`test_string_key_leak` and `test_all_maps` runs.

### 9.7 Improving Coverage

1. **HashSet gaps**: add explicit unit tests for `hash_set3.hpp` (`merge`,
   `extract`, `contains`) — currently below 60% line coverage.
2. **Branches**: enable `-b` in CI to capture branch coverage (requires
   `genhtml --branch-coverage`).
3. **EMH_HIGH_LOAD / EMH_SAVE_MEM / EMH_BITSET paths**: enable these macros
   in dedicated test variants to cover `#ifdef` branches.
4. **Exception paths**: add tests that throw from value-type constructors to
   exercise `try_emplace` and `try_insert` rollback branches.

---