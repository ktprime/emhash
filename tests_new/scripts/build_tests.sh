#!/bin/bash
# emhash 测试编译脚本 (tests_new 目录)
# 用法: ./build_tests.sh [target] [compiler]
# target: all | verify | stress | attack | debug | fuzz | bench | quick | clean | run
# compiler: gcc | clang (默认 gcc)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"          # tests_new/
PROJECT_DIR="$(dirname "$ROOT_DIR")"        # emhash root
FUZZ_DIR="$ROOT_DIR/fuzz"
VERIFY_DIR="$ROOT_DIR/verify"
STRESS_DIR="$ROOT_DIR/stress"
ATTACK_DIR="$ROOT_DIR/attack"
DEBUG_DIR="$ROOT_DIR/debug"
BENCH_DIR="$ROOT_DIR/bench"

COMPILER="${2:-gcc}"
CXX="g++"
CXXFLAGS="-std=c++17 -O2 -g -I$ROOT_DIR/.. -I$ROOT_DIR/../thirdparty"
ASAN_FLAGS="-fsanitize=address,undefined"
FUZZ_CXXFLAGS="-std=c++17 -g -O1 -fno-omit-frame-pointer"

if [ "$COMPILER" == "clang" ]; then
    CXX="clang++"
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

build_fuzz() {
    log_info "Building fuzz tests..."
    mkdir -p "$FUZZ_DIR/bin"

    if [ "$COMPILER" == "clang" ]; then
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS \
            "$FUZZ_DIR/fuzz_emhash_all.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash_all"
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS \
            "$FUZZ_DIR/fuzz_emhash8.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash8"
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS \
            "$FUZZ_DIR/fuzz_emhash8_advanced.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash8_advanced"
        $CXX -fsanitize=fuzzer,address $FUZZ_CXXFLAGS \
            "$FUZZ_DIR/fuzz_emilib_all.cpp" -o "$FUZZ_DIR/bin/fuzz_emilib_all"
        log_info "Fuzzer binaries built (clang + libfuzzer)"
    else
        log_warn "Fuzzer requires clang. Building non-fuzzer versions with gcc..."
        $CXX $CXXFLAGS $ASAN_FLAGS "$FUZZ_DIR/fuzz_extreme.cpp" "$FUZZ_DIR/fuzz_main.cpp" -o "$FUZZ_DIR/bin/fuzz_extreme_asan"
        $CXX $CXXFLAGS $ASAN_FLAGS "$FUZZ_DIR/fuzz_nocoll.cpp" "$FUZZ_DIR/fuzz_main.cpp" -o "$FUZZ_DIR/bin/fuzz_nocoll_asan"
        log_info "Non-fuzzer debug binaries built (gcc + ASan)"
    fi
}

build_verify() {
    log_info "Building verification tests..."
    mkdir -p "$VERIFY_DIR/bin"

    $CXX $CXXFLAGS "$VERIFY_DIR/test_all_maps.cpp" -o "$VERIFY_DIR/bin/test_all_maps"
    $CXX $CXXFLAGS "$VERIFY_DIR/quick_test8.cpp" -o "$VERIFY_DIR/bin/quick_test8"
    $CXX $CXXFLAGS "$VERIFY_DIR/test_emhash58.cpp" -o "$VERIFY_DIR/bin/test_emhash58"
    $CXX $CXXFLAGS "$VERIFY_DIR/test_hidden_bugs8.cpp" -o "$VERIFY_DIR/bin/test_hidden_bugs8"
    $CXX $CXXFLAGS "$VERIFY_DIR/test_extreme.cpp" -o "$VERIFY_DIR/bin/test_extreme"
    $CXX $CXXFLAGS "$VERIFY_DIR/test_interface_combo.cpp" -o "$VERIFY_DIR/bin/test_interface_combo"
    $CXX $CXXFLAGS "$VERIFY_DIR/test_emhash5_verify.cpp" -o "$VERIFY_DIR/bin/test_emhash5_verify"
    $CXX $CXXFLAGS "$VERIFY_DIR/test_emhash_allocator.cpp" -o "$VERIFY_DIR/bin/test_emhash_allocator"
    $CXX $CXXFLAGS "$VERIFY_DIR/test_functional_edge.cpp" -o "$VERIFY_DIR/bin/test_functional_edge"

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

build_quick() {
    log_info "Building quick validation tests..."
    build_verify
    build_debug
    log_info "Quick tests built"
}

build_all() {
    build_fuzz
    build_verify
    build_stress
    build_attack
    build_debug
    build_bench
}

run_tests() {
    log_info "Running all tests..."

    log_info "=== Verification tests ==="
    "$VERIFY_DIR/bin/test_all_maps" || log_error "test_all_maps FAILED"

    log_info "=== Debug tests ==="
    "$DEBUG_DIR/bin/debug_all_maps" || log_error "debug_all_maps FAILED"

    log_info "All tests completed"
}

clean() {
    log_info "Cleaning build artifacts..."
    rm -rf "$FUZZ_DIR/bin" "$VERIFY_DIR/bin" "$STRESS_DIR/bin"
    rm -rf "$ATTACK_DIR/bin" "$DEBUG_DIR/bin" "$BENCH_DIR/bin"
    log_info "Clean completed"
}

case "$1" in
    all)
        build_all
        ;;
    verify)
        build_verify
        ;;
    stress)
        build_stress
        ;;
    attack)
        build_attack
        ;;
    debug)
        build_debug
        ;;
    fuzz)
        build_fuzz
        ;;
    bench)
        build_bench
        ;;
    quick)
        build_quick
        ;;
    run)
        build_quick
        run_tests
        ;;
    clean)
        clean
        ;;
    *)
        echo "用法: $0 [target] [compiler]"
        echo "target: all | verify | stress | attack | debug | fuzz | bench | quick | run | clean"
        echo "compiler: gcc | clang (默认 gcc)"
        echo ""
        echo "示例:"
        echo "  $0 quick          # 快速构建验证+调试测试"
        echo "  $0 stress         # 构建压力测试"
        echo "  $0 attack         # 构建哈希攻击测试(需要 -DEMH_SAFE_PSL)"
        echo "  $0 all clang      # 构建所有测试(使用clang)"
        echo "  $0 run            # 构建并运行快速测试"
        exit 1
        ;;
esac
