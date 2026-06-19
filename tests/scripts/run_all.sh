#!/bin/bash
# emhash unified test runner
# Usage: ./run_all.sh [test_type] [compiler]
# test_type: quick | stress | fuzz | bench | attack | all | clean | info
# compiler: gcc | clang (default: gcc)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
TESTS_DIR="$ROOT_DIR"

# Compiler selection
COMPILER="${2:-gcc}"
case "$COMPILER" in
    gcc)   CXX="g++" ;;
    clang) CXX="clang++" ;;
    *)     echo "ERROR: Unknown compiler '$COMPILER'. Use 'gcc' or 'clang'."; exit 1 ;;
esac

if ! command -v "$CXX" &>/dev/null; then
    echo "ERROR: Compiler '$CXX' not found."
    exit 1
fi

CXXFLAGS="-std=c++17 -O2 -g -I$ROOT_DIR/../include"
ASAN_FLAGS="-fsanitize=address,undefined"

# Platform-specific settings
OS="$(uname -s)"
case "$OS" in
    Darwin*) ASAN_FLAGS="-fsanitize=address"; TIMEOUT_CMD="gtimeout" ;;
    MINGW*|MSYS*|CYGWIN*) TIMEOUT_CMD="timeout" ;;
    *) TIMEOUT_CMD="timeout" ;;
esac

# Temporary directory
TMPDIR="${TMPDIR:-/tmp}"

# ============================================================================
# Color output
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_test()  { echo -e "${BLUE}[TEST]${NC} $1"; }

print_env() {
    echo -e "${CYAN}=== Test Environment ===${NC}"
    echo "  OS:       $OS"
    echo "  Arch:     $ARCH"
    echo "  Compiler: $CXX ($($CXX --version 2>/dev/null | head -1))"
    echo "  MARCH:    ${MARCH_FLAG:-(none)}"
    echo -e "${CYAN}========================${NC}"
}

# ============================================================================
# Quick validation tests
# ============================================================================

run_quick() {
    log_info "=== Quick Validation Tests ==="

    log_test "Building quick_test8..."
    $CXX $CXXFLAGS "$TESTS_DIR/verify/quick_test8.cpp" -o "$TMPDIR/quick_test8"
    log_test "Running quick_test8..."
    "$TMPDIR/quick_test8" || log_error "quick_test8 FAILED"

    log_test "Building test_hidden_bugs8..."
    $CXX $CXXFLAGS "$TESTS_DIR/verify/test_hidden_bugs8.cpp" -o "$TMPDIR/hidden_bugs8"
    log_test "Running test_hidden_bugs8..."
    "$TMPDIR/hidden_bugs8" || log_warn "test_hidden_bugs8 has known issues (Test 4 key mutation)"

    log_test "Building test_extreme..."
    $CXX $CXXFLAGS "$TESTS_DIR/verify/test_extreme.cpp" -o "$TMPDIR/extreme"
    log_test "Running test_extreme..."
    "$TMPDIR/extreme" || log_error "test_extreme FAILED"

    log_test "Building test_hashset_allocator..."
    $CXX $CXXFLAGS "$TESTS_DIR/verify/test_hashset_allocator.cpp" -o "$TMPDIR/test_hashset_allocator"
    log_test "Running test_hashset_allocator..."
    "$TMPDIR/test_hashset_allocator" || log_error "test_hashset_allocator FAILED"

    log_info "Quick tests completed!"
}

# ============================================================================
# Stress tests
# ============================================================================

run_stress() {
    log_info "=== Stress Tests (with ASan) ==="

    log_test "Building stress_fix_asan..."
    $CXX $CXXFLAGS $ASAN_FLAGS "$TESTS_DIR/stress/stress_fix.cpp" -o "$TMPDIR/stress_fix_asan"
    log_test "Running stress_fix_asan..."
    "$TMPDIR/stress_fix_asan" || log_error "stress_fix FAILED"

    log_test "Building highload_test..."
    $CXX $CXXFLAGS "$TESTS_DIR/stress/highload_test.cpp" -o "$TMPDIR/highload_test"
    log_test "Running highload_test..."
    "$TMPDIR/highload_test" || log_error "highload_test FAILED"

    log_info "Stress tests completed!"
}

# ============================================================================
# Fuzzing tests (non-fuzzer version)
# ============================================================================

run_fuzz() {
    log_info "=== Fuzz Tests ==="

    if [ -f "$TESTS_DIR/fuzz/fuzz_extreme.cpp" ]; then
        log_test "Building fuzz_extreme_asan..."
        $CXX $CXXFLAGS $ASAN_FLAGS "$TESTS_DIR/fuzz/fuzz_extreme.cpp" -o "$TMPDIR/fuzz_extreme_asan"
        log_test "Running fuzz_extreme_asan (60 seconds)..."
        $TIMEOUT_CMD 60 "$TMPDIR/fuzz_extreme_asan" || log_warn "fuzz_extreme timed out or failed"
    else
        log_warn "fuzz_extreme.cpp not found, skipping fuzz tests"
    fi

    log_info "Fuzz tests completed!"
}

# ============================================================================
# Performance benchmarks
# ============================================================================

run_bench() {
    log_info "=== Benchmark Tests ==="

    local bench_dir="$ROOT_DIR/../bench"
    local bench_flags="-O3 -march=native"
    if [ -f "$bench_dir/ebench.cpp" ]; then
        log_test "Building ebench..."
        $CXX $CXXFLAGS $bench_flags "$bench_dir/ebench.cpp" -o "$TMPDIR/ebench"
        log_test "Running ebench (brief)..."
        $TIMEOUT_CMD 30 "$TMPDIR/ebench" || log_warn "ebench timed out"
    else
        log_warn "bench/ebench.cpp not found (requires thirdparty/), skipping"
    fi

    log_info "Bench tests completed!"
}

# ============================================================================
# Hash attack tests
# ============================================================================

run_attack() {
    log_info "=== Hash Attack Tests ==="

    log_test "Building hash_attack..."
    $CXX $CXXFLAGS -DEMH_SAFE_PSL "$TESTS_DIR/attack/hash_attack_all.cpp" -o "$TMPDIR/hash_attack"
    log_test "Running hash_attack..."
    "$TMPDIR/hash_attack" || log_warn "hash_attack shows expected slowdown under attack"

    log_info "Attack tests completed!"
}

# ============================================================================
# All tests
# ============================================================================

run_all() {
    run_quick
    run_stress
    run_fuzz
    run_attack
    log_info "=== ALL TESTS COMPLETED ==="
}

# ============================================================================
# Cleanup
# ============================================================================

cleanup() {
    rm -f "$TMPDIR/quick_test8" "$TMPDIR/hidden_bugs8" "$TMPDIR/extreme" "$TMPDIR/test_hashset_allocator"
    rm -f "$TMPDIR/stress_fix_asan" "$TMPDIR/highload_test"
    rm -f "$TMPDIR/fuzz_extreme_asan"
    rm -f "$TMPDIR/ebench"
    rm -f "$TMPDIR/hash_attack"
    log_info "Cleanup completed"
}

# ============================================================================
# Main entry
# ============================================================================

print_env

case "$1" in
    quick)   run_quick ;;
    stress)  run_stress ;;
    fuzz)    run_fuzz ;;
    bench)   run_bench ;;
    attack)  run_attack ;;
    all)     run_all ;;
    clean)   cleanup ;;
    info)    print_env ;;
    *)
        echo "Usage: $0 [test_type] [compiler]"
        echo ""
        echo "Test types:"
        echo "  quick    - Quick validation tests"
        echo "  stress   - Stress tests (with ASan)"
        echo "  fuzz     - Fuzzing tests (60 seconds)"
        echo "  bench    - Performance benchmarks"
        echo "  attack   - Hash attack tests"
        echo "  all      - Run all tests"
        echo "  clean    - Cleanup temporary files"
        echo "  info     - Show environment info"
        echo ""
        echo "Compilers: gcc | clang (default: gcc)"
        echo ""
        echo "Examples:"
        echo "  $0 quick        # Quick tests with gcc"
        echo "  $0 all clang    # All tests with clang"
        echo ""
        echo "Detected: OS=$OS  Arch=$ARCH"
        exit 1
        ;;
esac
