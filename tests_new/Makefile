# emhash Test Suite Makefile
# Build and run tests with make

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic
RELEASE_FLAGS := -O2
DEBUG_FLAGS := -g -O0
BENCH_FLAGS := -O3 -march=native

INCLUDES := -I.. -I../thirdparty -I../bench

# ============================================================================
# Targets
# ============================================================================

.PHONY: all quick stress attack debug bench clean help

all: quick stress

help:
	@echo "Available targets:"
	@echo "  make quick       - Quick validation tests (~20s)"
	@echo "  make stress      - Stress tests (~1min)"
	@echo "  make attack      - Hash attack tests (~2min)"
	@echo "  make debug       - Debug tests"
	@echo "  make bench       - Performance benchmarks"
	@echo "  make all         - Run all tests (quick + stress)"
	@echo "  make clean       - Remove all binaries"
	@echo ""
	@echo "Individual tests:"
	@echo "  make test_verify"
	@echo "  make test_stress"
	@echo "  make test_attack"
	@echo "  make test_debug"
	@echo ""
	@echo "Compile options:"
	@echo "  CXX=clang++      - Use clang compiler"
	@echo "  CXXFLAGS=...     - Custom compiler flags"

# ============================================================================
# Quick Validation Tests
# ============================================================================

quick: test_verify test_debug
	@echo "=== Running Quick Tests ==="
	@./test_verify
	@./test_debug
	@echo "=== All Quick Tests Passed ==="
test_verify: verify/test_all_maps.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_all_maps.cpp -o test_verify

quick_test8: verify/quick_test8.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/quick_test8.cpp -o quick_test8

test_emhash58: verify/test_emhash58.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_emhash58.cpp -o test_emhash58

test_hidden_bugs8: verify/test_hidden_bugs8.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_hidden_bugs8.cpp -o test_hidden_bugs8

test_extreme: verify/test_extreme.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_extreme.cpp -o test_extreme

test_interface_combo: verify/test_interface_combo.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_interface_combo.cpp -o test_interface_combo

test_emhash5_verify: verify/test_emhash5_verify.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_emhash5_verify.cpp -o test_emhash5_verify

test_emhash_allocator: verify/test_emhash_allocator.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_emhash_allocator.cpp -o test_emhash_allocator

test_functional_edge: verify/test_functional_edge.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) verify/test_functional_edge.cpp -o test_functional_edge

# ============================================================================
# Stress Tests
# ============================================================================

stress: test_stress highload_test
	@echo "=== Running Stress Tests ==="
	@./test_stress
	@./highload_test
	@echo "=== All Stress Tests Passed ==="

test_stress: stress/stress_all_maps.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) stress/stress_all_maps.cpp -o test_stress

stress_fix: stress/stress_fix.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) stress/stress_fix.cpp -o stress_fix

highload_test: stress/highload_test.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(INCLUDES) stress/highload_test.cpp -o highload_test

test_emhash5_stress: stress/test_emhash5_stress.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) stress/test_emhash5_stress.cpp -o test_emhash5_stress

test_emhash5_hifi: stress/test_emhash5_hifi.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) stress/test_emhash5_hifi.cpp -o test_emhash5_hifi

# ============================================================================
# Hash Attack Tests (requires EMH_SAFE_PSL)
# ============================================================================

attack: test_attack
	@echo "=== Running Hash Attack Tests ==="
	@./test_attack
	@echo "=== All Attack Tests Passed ==="

test_attack: attack/hash_attack_all.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -DEMH_SAFE_PSL $(INCLUDES) attack/hash_attack_all.cpp -o test_attack

hash_attack5: attack/hash_attack.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -DEMH_SAFE_PSL $(INCLUDES) attack/hash_attack.cpp -o hash_attack5

hash_attack7: attack/hash_attack7.cpp
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -DEMH_SAFE_PSL $(INCLUDES) attack/hash_attack7.cpp -o hash_attack7

# ============================================================================
# Debug Tests
# ============================================================================

debug: test_debug
	@echo "=== Running Debug Tests ==="
	@./test_debug
	@echo "=== All Debug Tests Passed ==="

test_debug: debug/debug_all_maps.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) debug/debug_all_maps.cpp -o test_debug

debug_chain: debug/debug_chain.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) debug/debug_chain.cpp -o debug_chain

debug_set_erase: debug/debug_set_erase.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) debug/debug_set_erase.cpp -o debug_set_erase

min_repro: debug/min_repro.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) debug/min_repro.cpp -o min_repro

reproduce_crash: debug/reproduce_crash.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) debug/reproduce_crash.cpp -o reproduce_crash

fuzz_reproduce: debug/fuzz_reproduce.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) debug/fuzz_reproduce.cpp -o fuzz_reproduce

# ============================================================================
# Benchmarks
# ============================================================================

bench: ebench martin_bench highload_bench
	@echo "=== Benchmarks built ==="
	@echo "Run: ./ebench, ./martin_bench, ./highload_bench"

ebench: bench/ebench.cpp
	$(CXX) $(CXXFLAGS) $(BENCH_FLAGS) $(INCLUDES) bench/ebench.cpp -o ebench

martin_bench: bench/martin_bench.cpp
	$(CXX) $(CXXFLAGS) $(BENCH_FLAGS) $(INCLUDES) bench/martin_bench.cpp -o martin_bench

highload_bench: bench/highload_bench.cpp
	$(CXX) $(CXXFLAGS) $(BENCH_FLAGS) -DEMH_HIGH_LOAD=123456 $(INCLUDES) bench/highload_bench.cpp -o highload_bench

# ============================================================================
# Fuzz Tests (non-fuzzer versions, can be built with gcc)
# ============================================================================

# Non-fuzzer ASan variants (gcc + ASan with main wrapper)
fuzz_extreme: fuzz/fuzz_extreme.cpp fuzz/fuzz_main.cpp
	$(CXX) $(CXXFLAGS) $(ASAN_FLAGS) $(INCLUDES) fuzz/fuzz_extreme.cpp fuzz/fuzz_main.cpp -o fuzz_extreme

fuzz_nocoll: fuzz/fuzz_nocoll.cpp fuzz/fuzz_main.cpp
	$(CXX) $(CXXFLAGS) $(ASAN_FLAGS) $(INCLUDES) fuzz/fuzz_nocoll.cpp fuzz/fuzz_main.cpp -o fuzz_nocoll

reproduce_emhash8_bug: fuzz/reproduce_emhash8_bug.cpp
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDES) fuzz/reproduce_emhash8_bug.cpp -o reproduce_emhash8_bug

# Fuzzer targets (requires clang)
fuzz_emhash_all: fuzz/fuzz_emhash_all.cpp
	clang++ -std=c++17 -fsanitize=fuzzer,address $(INCLUDES) fuzz/fuzz_emhash_all.cpp -o fuzz_emhash_all

fuzz_emhash8: fuzz/fuzz_emhash8.cpp
	clang++ -std=c++17 -fsanitize=fuzzer,address $(INCLUDES) fuzz/fuzz_emhash8.cpp -o fuzz_emhash8

fuzz_emhash8_advanced: fuzz/fuzz_emhash8_advanced.cpp
	clang++ -std=c++17 -fsanitize=fuzzer,address $(INCLUDES) fuzz/fuzz_emhash8_advanced.cpp -o fuzz_emhash8_advanced

fuzz_emilib_all: fuzz/fuzz_emilib_all.cpp
	clang++ -std=c++17 -fsanitize=fuzzer,address $(INCLUDES) fuzz/fuzz_emilib_all.cpp -o fuzz_emilib_all

# Aggregate fuzz target (builds both ASan and LibFuzzer versions)
fuzz: fuzz_extreme fuzz_nocoll
	@echo "=== Fuzz Tests Built (ASan variants) ==="
	@echo "Run: ./fuzz_extreme, ./fuzz_nocoll"
	@echo "For LibFuzzer: CXX=clang++ make fuzz_emhash_all fuzz_emilib_all"

# ============================================================================
# ASan Tests (Address Sanitizer)
# ============================================================================

asan_stress: stress/stress_all_maps.cpp
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined $(INCLUDES) stress/stress_all_maps.cpp -o asan_stress

asan_verify: verify/test_all_maps.cpp
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined $(INCLUDES) verify/test_all_maps.cpp -o asan_verify

# ============================================================================
# Clean
# ============================================================================

clean:
	rm -f test_verify test_stress test_attack test_debug
	rm -f quick_test8 test_emhash58 test_hidden_bugs8 test_extreme test_interface_combo
	rm -f test_emhash5_verify test_emhash_allocator test_functional_edge
	rm -f stress_fix highload_test test_emhash5_stress test_emhash5_hifi
	rm -f hash_attack5 hash_attack7
	rm -f debug_chain debug_set_erase min_repro reproduce_crash fuzz_reproduce
	rm -f ebench martin_bench highload_bench
	rm -f fuzz_extreme fuzz_nocoll reproduce_emhash8_bug
	rm -f fuzz_emhash_all fuzz_emhash8 fuzz_emhash8_advanced fuzz_emilib_all
	rm -f asan_stress asan_verify
	rm -f t1 t2