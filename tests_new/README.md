# emhash Test Code Organization

This directory contains the newly added test code for the emhash project, organized by functionality.

---

## Directory Structure

```
tests_new/
├── fuzz/              # Fuzzing tests (requires clang + libfuzzer)
├── stress/            # Stress tests (with ASan)
├── debug/             # Debug tools
├── verify/            # Quick validation tests
├── attack/            # Hash attack tests
├── bench/             # Performance benchmarks
├── scripts/           # Build scripts
└── README.md          # This document
```

---

## 1. fuzz/ — Fuzzing Tests

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `fuzz_emhash_all.cpp` | Test all emhash versions (5/6/7/8) | `clang++ -fsanitize=fuzzer,address -std=c++17 -I.. fuzz_emhash_all.cpp -o fuzz_all` |
| `fuzz_emhash8.cpp` | emhash8 basic fuzzer | `clang++ -fsanitize=fuzzer,address -std=c++17 -I.. fuzz_emhash8.cpp -o fuzz8` |
| `fuzz_emhash8_advanced.cpp` | Advanced fuzzer with variable hasher | `clang++ -fsanitize=fuzzer,address -std=c++17 -I.. fuzz_emhash8_advanced.cpp -o fuzz8_adv` |
| `fuzz_extreme.cpp` | Extreme tests: 100% collision, high LF | `g++ -fsanitize=address,undefined -std=c++17 -I.. fuzz_extreme.cpp -o fuzz_extreme_asan` |
| `fuzz_nocoll.cpp` | No-collision boundary tests | `g++ -fsanitize=address,undefined -std=c++17 -I.. fuzz_nocoll.cpp -o fuzz_nocoll_asan` |

**Run Examples**:
```bash
# Fuzzer (requires clang)
./fuzz8 -max_total_time=60 corpus/

# Non-fuzzer version
./fuzz_extreme_asan
```

---

## 2. stress/ — Stress Tests

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `stress_fix.cpp` | Verify reserve(1) crash fix | `g++ -fsanitize=address,undefined -std=c++17 -I.. stress_fix.cpp -o stress_fix_asan` |
| `highload_test.cpp` | High load tests (LF=0.999) | `g++ -std=c++17 -O2 -I.. highload_test.cpp -o highload_test` |
| `test_emhash5_hifi.cpp` | High-intensity random operations | `g++ -fsanitize=address,undefined -std=c++17 -I.. test_emhash5_hifi.cpp -o hifi_asan` |
| `test_emhash5_stress.cpp` | emhash5 stress test | `g++ -fsanitize=address,undefined -std=c++17 -I.. test_emhash5_stress.cpp -o stress5_asan` |

**Run Examples**:
```bash
./stress_fix_asan       # 11000 random stress tests
./highload_test         # LF oscillation tests
```

---

## 3. debug/ — Debug Tools

| File | Purpose | Use Case |
|------|---------|----------|
| `debug_chain.cpp` | Debug `_index` chain corruption | Analyze kickout_bucket issues |
| `debug_set_erase.cpp` | Debug erase_slot issues | HashSet deletion chain breakage |
| `min_repro.cpp` | Minimal crash reproduction | Identify specific crash root cause |
| `reproduce_crash.cpp` | Reproduce fuzzer-discovered crash | Verify fix effectiveness |
| `fuzz_reproduce.cpp` | Replay crash input file | Parse fuzzer crash files |

---

## 4. verify/ — Quick Validation Tests

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `quick_test8.cpp` | emhash8 quick validation | `g++ -std=c++17 -O2 -I.. quick_test8.cpp -o quick_test8` |
| `test_hidden_bugs8.cpp` | Hidden bug tests (7 items) | `g++ -std=c++17 -O2 -I.. test_hidden_bugs8.cpp -o hidden_bugs8` |
| `test_extreme.cpp` | Extreme boundary tests | `g++ -std=c++17 -O2 -I.. test_extreme.cpp -o extreme` |
| `test_interface_combo.cpp` | Interface combination tests | `g++ -std=c++17 -O2 -I.. test_interface_combo.cpp -o interface_combo` |
| `test_emhash58.cpp` | Comprehensive tests (5/6/7/8) | `g++ -std=c++17 -O2 -I.. test_emhash58.cpp -o emhash58` |
| `test_emhash5_verify.cpp` | emhash5 verification | `g++ -std=c++17 -O2 -I.. test_emhash5_verify.cpp -o verify5` |

**Run Examples**:
```bash
./quick_test8          # Quick validation (seconds)
./hidden_bugs8         # Hidden bug tests
./extreme              # Extreme boundary tests
```

---

## 5. attack/ — Hash Attack Tests

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `hash_attack.cpp` | emhash5 hash attack | `g++ -std=c++17 -O2 -I.. hash_attack.cpp -o hash_attack5` |
| `hash_attack7.cpp` | emhash7 hash attack | `g++ -std=c++17 -O2 -I.. hash_attack7.cpp -o hash_attack7` |

**Test Scenarios**:
- Constant hash (all keys map to same bucket)
- Small range hash (4 buckets)
- Linear hash (key % 4)

---

## 6. bench/ — Performance Benchmarks

| File | Purpose | Compile Command |
|------|---------|-----------------|
| `ebench.cpp` | Basic performance benchmark | `g++ -std=c++17 -O3 -march=native -I.. ebench.cpp -o ebench` |
| `martin_bench.cpp` | Martin Ankerl format benchmark | `g++ -std=c++17 -O3 -march=native -I.. martin_bench.cpp -o martin_bench` |
| `highload_bench.cpp` | High load performance benchmark | `g++ -std=c++17 -O3 -DEMH_HIGH_LOAD=123456 -I.. highload_bench.cpp -o highload_bench` |

---

## 7. scripts/ — Build Scripts

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

## 8. Test Coverage Matrix

| Test Type | emhash5 | emhash6 | emhash7 | emhash8 |
|-----------|---------|---------|---------|---------|
| Fuzzing | ✓ | ✓ | ✓ | ✓ |
| Stress | ✓ | - | - | ✓ |
| High Load | ✓ | ✓ | ✓ | ✓ |
| Hash Attack | ✓ | - | ✓ | - |
| Validation | ✓ | ✓ | ✓ | ✓ |

---

## 9. Recommended Test Order

1. **Quick Validation** (minutes):
   ```bash
   ./quick_test8
   ./hidden_bugs8
   ./extreme
   ```

2. **Stress Tests** (tens of minutes):
   ```bash
   ./stress_fix_asan
   ./highload_test
   ```

3. **Fuzzing** (requires clang):
   ```bash
   ./fuzz8 -max_total_time=300 corpus/
   ```

4. **Performance Benchmarks**:
   ```bash
   ./ebench
   ./martin_bench
   ```

---

## 10. Notes

1. **Fuzzer requires clang + libfuzzer**, gcc compiles non-fuzzer versions
2. **ASan tests should not use -O3**, use -O2 or -g
3. **High load tests require `-DEMH_HIGH_LOAD=123456`** compile flag
4. **Windows users should use WSL** for compilation and execution

---

## 11. Original Test File Locations

Original test files remain in their original locations:
- `fuzz/` — Original fuzzer directory
- `test/` — Original unit test directory
- `bench/` — Original benchmark directory

This directory `tests_new/` is the unified entry point after organization, for easier maintenance and usage.