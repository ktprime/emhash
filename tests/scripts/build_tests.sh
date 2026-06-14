#!/bin/bash
# emhash test build script
# Usage: ./build_tests.sh [target] [compiler]
# target: all | verify | stress | attack | debug | fuzz | bench | quick | clean | run | info
# compiler: gcc | clang (default: auto-detect)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
FUZZ_DIR="$ROOT_DIR/fuzz"
VERIFY_DIR="$ROOT_DIR/verify"
STRESS_DIR="$ROOT_DIR/stress"
ATTACK_DIR="$ROOT_DIR/attack"
DEBUG_DIR="$ROOT_DIR/debug"
BENCH_DIR="$ROOT_DIR/bench"

# Compiler selection
COMPILER="${2:-auto}"
if [ "$COMPILER" = "auto" ]; then
    if command -v g++ &>/dev/null; then COMPILER="gcc"
    elif command -v clang++ &>/dev/null; then COMPILER="clang"
    else echo "ERROR: No C++ compiler found."; exit 1; fi
fi
case "$COMPILER" in
    gcc)   CXX="g++" ;;
    clang) CXX="clang++" ;;
    *)     echo "ERROR: Unknown compiler '$COMPILER'. Use 'gcc' or 'clang'."; exit 1 ;;
esac

if ! command -v "$CXX" &>/dev/null; then
    echo "ERROR: Compiler '$CXX' not found."
    exit 1
fi

CXXFLAGS="-std=c++17 -O2 -g -I$ROOT_DIR/.. -I$ROOT_DIR/../thirdparty -I$ROOT_DIR/../bench"
ASAN_FLAGS="-fsanitize=address,undefined"
FUZZ_CXXFLAGS="-std=c++17 -g -O1 -fno-omit-frame-pointer"

# macOS: no undefined sanitizer
if [ "$(uname -s)" = "Darwin" ]; then
    ASAN_FLAGS="-fsanitize=address"
fi

# Color output
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

print_env() {
    echo -e "${CYAN}=== Build Environment ===${NC}"
    echo "  OS:       $(uname -s)"
    echo "  Arch:     $(uname -m)"
    echo "  Compiler: $CXX ($($CXX --version 2>/dev/null | head -1))"
    echo -e "${CYAN}=========================${NC}"
}

# ============================================================================
# Build functions
# ============================================================================

build_fuzz() {
    log_info "Building fuzz tests..."
    mkdir -p "$FUZZ_DIR/bin"
    if [ "$COMPILER" = "clang" ]; then
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS "$FUZZ_DIR/fuzz_emhash_all.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash_all"
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS "$FUZZ_DIR/fuzz_emhash8.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash8"
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS "$FUZZ_DIR/fuzz_emhash8_advanced.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash8_advanced"
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS "$FUZZ_DIR/fuzz_emilib_all.cpp" -o "$FUZZ_DIR/bin/fuzz_emilib_all"
        log_info "Fuzzer binaries built (clang + libfuzzer)"
    else
        log_warn "Fuzzer requires clang. Building non-fuzzer versions..."
        $CXX $CXXFLAGS $ASAN_FLAGS "$FUZZ_DIR/fuzz_extreme.cpp" "$FUZZ_DIR/fuzz_main.cpp" -o "$FUZZ_DIR/bin/fuzz_extreme_asan"
        $CXX $CXXFLAGS $ASAN_FLAGS "$FUZZ_DIR/fuzz_nocoll.cpp" "$FUZZ_DIR/fuzz_main.cpp" -o "$FUZZ_DIR/bin/fuzz_nocoll_asan"
        log_info "Non-fuzzer ASan binaries built"
    fi
}

build_verify() {
    log_info "Building verification tests..."
    mkdir -p "$VERIFY_DIR/bin"
    for src in test_all_maps quick_test8 test_emhash58 test_hidden_bugs8 test_extreme \
               test_interface_combo test_emhash5_verify test_emhash_allocator \
               test_functional_edge test_hashset_allocator test_emilib_comprehensive test_size_sweep test_main; do
        $CXX $CXXFLAGS "$VERIFY_DIR/${src}.cpp" -o "$VERIFY_DIR/bin/${src}"
    done
    log_info "Verification tests built"
}

build_stress() {
    log_info "Building stress tests..."
    mkdir -p "$STRESS_DIR/bin"
    $CXX $CXXFLAGS "$STRESS_DIR/stress_all_maps.cpp" -o "$STRESS_DIR/bin/stress_all_maps"
    $CXX $CXXFLAGS $ASAN_FLAGS "$STRESS_DIR/stress_fix.cpp" -o "$STRESS_DIR/bin/stress_fix_asan"
    $CXX $CXXFLAGS "$STRESS_DIR/highload_test.cpp" -o "$STRESS_DIR/bin/highload_test"
    $CXX $CXXFLAGS $ASAN_FLAGS "$STRESS_DIR/test_emhash5_stress.cpp" -o "$STRESS_DIR/bin/test_emhash5_stress_asan"
    $CXX $CXXFLAGS $ASAN_FLAGS "$STRESS_DIR/test_emhash5_hifi.cpp" -o "$STRESS_DIR/bin/test_emhash5_hifi_asan"
    $CXX $CXXFLAGS "$STRESS_DIR/test_bad_hash_stress.cpp" -o "$STRESS_DIR/bin/test_bad_hash_stress"
    log_info "Stress tests built"
}

build_attack() {
    log_info "Building hash attack tests..."
    mkdir -p "$ATTACK_DIR/bin"
    $CXX $CXXFLAGS -DEMH_SAFE_PSL "$ATTACK_DIR/hash_attack_all.cpp" -o "$ATTACK_DIR/bin/hash_attack_all"
    $CXX $CXXFLAGS -DEMH_SAFE_PSL "$ATTACK_DIR/hash_attack.cpp" -o "$ATTACK_DIR/bin/hash_attack5"
    $CXX $CXXFLAGS -DEMH_SAFE_PSL "$ATTACK_DIR/hash_attack7.cpp" -o "$ATTACK_DIR/bin/hash_attack7"
    log_info "Hash attack tests built"
}

build_debug() {
    log_info "Building debug tests..."
    mkdir -p "$DEBUG_DIR/bin"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/debug_all_maps.cpp" -o "$DEBUG_DIR/bin/debug_all_maps"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/debug_chain.cpp" -o "$DEBUG_DIR/bin/debug_chain"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/debug_set_erase.cpp" -o "$DEBUG_DIR/bin/debug_set_erase"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/min_repro.cpp" -o "$DEBUG_DIR/bin/min_repro"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/reproduce_crash.cpp" -o "$DEBUG_DIR/bin/reproduce_crash"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/fuzz_reproduce.cpp" -o "$DEBUG_DIR/bin/fuzz_reproduce"
    $CXX $CXXFLAGS "$DEBUG_DIR/test_probe_bug.cpp" -o "$DEBUG_DIR/bin/test_probe_bug"
    $CXX $CXXFLAGS "$DEBUG_DIR/test_probe_coverage.cpp" -o "$DEBUG_DIR/bin/test_probe_coverage"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/test_debug_find.cpp" -o "$DEBUG_DIR/bin/test_debug_find"
    $CXX $CXXFLAGS -g -O0 "$DEBUG_DIR/test_repro_collision.cpp" -o "$DEBUG_DIR/bin/test_repro_collision"
    log_info "Debug tests built"
}

build_bench() {
    log_info "Building benchmarks..."
    mkdir -p "$BENCH_DIR/bin"
    $CXX $CXXFLAGS -O3 -march=native "$BENCH_DIR/ebench.cpp" -o "$BENCH_DIR/bin/ebench"
    $CXX $CXXFLAGS -O3 -march=native "$BENCH_DIR/martin_bench.cpp" -o "$BENCH_DIR/bin/martin_bench"
    $CXX $CXXFLAGS -O3 -march=native -DEMH_HIGH_LOAD=123456 "$BENCH_DIR/highload_bench.cpp" -o "$BENCH_DIR/bin/highload_bench"
    log_info "Benchmarks built"
}

build_quick() { build_verify; build_debug; }
build_all() { build_fuzz; build_verify; build_stress; build_attack; build_debug; build_bench; }

run_tests() {
    log_info "Running tests..."
    "$VERIFY_DIR/bin/test_all_maps" || log_error "test_all_maps FAILED"
    "$DEBUG_DIR/bin/debug_all_maps" || log_error "debug_all_maps FAILED"
    log_info "All tests completed"
}

clean() {
    rm -rf "$FUZZ_DIR/bin" "$VERIFY_DIR/bin" "$STRESS_DIR/bin" "$ATTACK_DIR/bin" "$DEBUG_DIR/bin" "$BENCH_DIR/bin"
    log_info "Clean completed"
}

# ============================================================================
# Main entry
# ============================================================================

print_env

case "$1" in
    all)     build_all ;;
    verify)  build_verify ;;
    stress)  build_stress ;;
    attack)  build_attack ;;
    debug)   build_debug ;;
    fuzz)    build_fuzz ;;
    bench)   build_bench ;;
    quick)   build_quick ;;
    run)     build_quick; run_tests ;;
    clean)   clean ;;
    info)    print_env ;;
    *)
        echo "Usage: $0 [target] [compiler]"
        echo "Targets: all | verify | stress | attack | debug | fuzz | bench | quick | run | clean | info"
        echo "Compilers: gcc | clang | auto (default: auto)"
        echo ""
        echo "Examples:"
        echo "  $0 quick          # Quick build (auto-detect compiler)"
        echo "  $0 all clang      # Build all with clang"
        exit 1
        ;;
esac
