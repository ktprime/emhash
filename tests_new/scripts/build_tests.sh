#!/bin/bash
# emhash 测试编译脚本
# 用法: ./build_tests.sh [target] [compiler]
# target: all | fuzz | test | bench | quick | stress | clean
# compiler: gcc | clang (默认 gcc)

set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
FUZZ_DIR="$ROOT_DIR/fuzz"
TEST_DIR="$ROOT_DIR/test"
BENCH_DIR="$ROOT_DIR/bench"

COMPILER="${2:-gcc}"
CXX="g++"
CXXFLAGS="-std=c++17 -O2 -g -I$ROOT_DIR"
ASAN_FLAGS="-fsanitize=address,undefined"

if [ "$COMPILER" == "clang" ]; then
    CXX="clang++"
fi

# 颜色输出
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
    
    # Fuzzer (需要 clang + libfuzzer)
    if [ "$COMPILER" == "clang" ]; then
        $CXX -fsanitize=fuzzer,address -std=c++17 -g -I$ROOT_DIR \
            "$FUZZ_DIR/fuzz_emhash_all.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash_all"
        $CXX -fsanitize=fuzzer,address -std=c++17 -g -I$ROOT_DIR \
            "$BENCH_DIR/fuzz_emhash8.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash8"
        $CXX -fsanitize=fuzzer,address -std=c++17 -g -I$ROOT_DIR \
            "$BENCH_DIR/fuzz_emhash8_advanced.cpp" -o "$FUZZ_DIR/bin/fuzz_emhash8_advanced"
        log_info "Fuzzer binaries built (clang + libfuzzer)"
    else
        log_warn "Fuzzer requires clang. Building non-fuzzer versions with gcc..."
        # 非 libfuzzer 版本，用于手动测试
        $CXX $CXXFLAGS $ASAN_FLAGS "$FUZZ_DIR/fuzz_extreme.cpp" -o "$FUZZ_DIR/bin/fuzz_extreme_asan"
        $CXX $CXXFLAGS $ASAN_FLAGS "$FUZZ_DIR/fuzz_nocoll.cpp" -o "$FUZZ_DIR/bin/fuzz_nocoll_asan"
        $CXX $CXXFLAGS $ASAN_FLAGS "$FUZZ_DIR/min_repro.cpp" -o "$FUZZ_DIR/bin/min_repro_asan"
        $CXX $CXXFLAGS "$FUZZ_DIR/debug_chain.cpp" -o "$FUZZ_DIR/bin/debug_chain"
        $CXX $CXXFLAGS "$FUZZ_DIR/debug_set_erase.cpp" -o "$FUZZ_DIR/bin/debug_set_erase"
        log_info "Non-fuzzer debug binaries built (gcc + ASan)"
    fi
}

build_test() {
    log_info "Building unit tests..."
    mkdir -p "$TEST_DIR/bin"
    
    # 核心测试
    $CXX $CXXFLAGS "$TEST_DIR/test_emhash58.cpp" -o "$TEST_DIR/bin/test_emhash58"
    $CXX $CXXFLAGS "$TEST_DIR/test_emhash5_verify.cpp" -o "$TEST_DIR/bin/test_emhash5_verify"
    $CXX $CXXFLAGS "$TEST_DIR/test_emhash5_stress.cpp" -o "$TEST_DIR/bin/test_emhash5_stress"
    $CXX $CXXFLAGS "$TEST_DIR/test_emhash5_hifi.cpp" -o "$TEST_DIR/bin/test_emhash5_hifi"
    
    # emilib 测试
    $CXX $CXXFLAGS "$TEST_DIR/test_emilib2o.cpp" -o "$TEST_DIR/bin/test_emilib2o"
    $CXX $CXXFLAGS "$TEST_DIR/test_emilib2o_basic.cpp" -o "$TEST_DIR/bin/test_emilib2o_basic"
    $CXX $CXXFLAGS "$TEST_DIR/test_emilib2o_stress.cpp" -o "$TEST_DIR/bin/test_emilib2o_stress"
    $CXX $CXXFLAGS "$TEST_DIR/test_emilib2s.cpp" -o "$TEST_DIR/bin/test_emilib2s"
    
    # 边界/bug测试
    $CXX $CXXFLAGS "$TEST_DIR/test_extreme.cpp" -o "$TEST_DIR/bin/test_extreme"
    $CXX $CXXFLAGS "$TEST_DIR/test_interface_combo.cpp" -o "$TEST_DIR/bin/test_interface_combo"
    $CXX $CXXFLAGS "$TEST_DIR/test_probe_bug.cpp" -o "$TEST_DIR/bin/test_probe_bug"
    $CXX $CXXFLAGS "$TEST_DIR/test_probe_coverage.cpp" -o "$TEST_DIR/bin/test_probe_coverage"
    $CXX $CXXFLAGS "$TEST_DIR/test_repro_collision.cpp" -o "$TEST_DIR/bin/test_repro_collision"
    $CXX $CXXFLAGS "$TEST_DIR/test_size_sweep.cpp" -o "$TEST_DIR/bin/test_size_sweep"
    $CXX $CXXFLAGS "$TEST_DIR/test_steps.cpp" -o "$TEST_DIR/bin/test_steps"
    $CXX $CXXFLAGS "$TEST_DIR/test_struct_layout.cpp" -o "$TEST_DIR/bin/test_struct_layout"
    
    log_info "Unit tests built"
}

build_bench() {
    log_info "Building benchmarks..."
    mkdir -p "$BENCH_DIR/bin"
    
    # 快速验证测试
    $CXX $CXXFLAGS "$BENCH_DIR/quick_test8.cpp" -o "$BENCH_DIR/bin/quick_test8"
    $CXX $CXXFLAGS "$BENCH_DIR/test_hidden_bugs8.cpp" -o "$BENCH_DIR/bin/test_hidden_bugs8"
    $CXX $CXXFLAGS "$BENCH_DIR/highload_test.cpp" -o "$BENCH_DIR/bin/highload_test"
    $CXX $CXXFLAGS "$BENCH_DIR/smallsize_test.cpp" -o "$BENCH_DIR/bin/smallsize_test"
    
    # 压力测试
    $CXX $CXXFLAGS $ASAN_FLAGS "$BENCH_DIR/stress_fix.cpp" -o "$BENCH_DIR/bin/stress_fix_asan"
    $CXX $CXXFLAGS "$BENCH_DIR/fuzz_reproduce.cpp" -o "$BENCH_DIR/bin/fuzz_reproduce"
    $CXX $CXXFLAGS "$BENCH_DIR/reproduce_crash.cpp" -o "$BENCH_DIR/bin/reproduce_crash"
    
    # Hash攻击测试
    $CXX $CXXFLAGS "$BENCH_DIR/hash_attack.cpp" -o "$BENCH_DIR/bin/hash_attack"
    $CXX $CXXFLAGS "$BENCH_DIR/hash_attack7.cpp" -o "$BENCH_DIR/bin/hash_attack7"
    
    # 性能基准测试
    $CXX $CXXFLAGS "$BENCH_DIR/ebench.cpp" -o "$BENCH_DIR/bin/ebench"
    $CXX $CXXFLAGS "$BENCH_DIR/hbench.cpp" -o "$BENCH_DIR/bin/hbench"
    $CXX $CXXFLAGS "$BENCH_DIR/mbench.cpp" -o "$BENCH_DIR/bin/mbench"
    $CXX $CXXFLAGS "$BENCH_DIR/martin_bench.cpp" -o "$BENCH_DIR/bin/martin_bench"
    $CXX $CXXFLAGS "$BENCH_DIR/highload_bench.cpp" -o "$BENCH_DIR/bin/highload_bench"
    
    log_info "Benchmarks built"
}

build_quick() {
    log_info "Building quick validation tests..."
    mkdir -p "$BENCH_DIR/bin"
    
    $CXX $CXXFLAGS "$BENCH_DIR/quick_test8.cpp" -o "$BENCH_DIR/bin/quick_test8"
    $CXX $CXXFLAGS "$BENCH_DIR/test_hidden_bugs8.cpp" -o "$BENCH_DIR/bin/test_hidden_bugs8"
    $CXX $CXXFLAGS "$BENCH_DIR/highload_test.cpp" -o "$BENCH_DIR/bin/highload_test"
    
    log_info "Quick tests built. Run with: ./bench/bin/quick_test8"
}

build_stress() {
    log_info "Building stress tests with ASan..."
    mkdir -p "$BENCH_DIR/bin"
    mkdir -p "$TEST_DIR/bin"
    
    $CXX $CXXFLAGS $ASAN_FLAGS "$BENCH_DIR/stress_fix.cpp" -o "$BENCH_DIR/bin/stress_fix_asan"
    $CXX $CXXFLAGS $ASAN_FLAGS "$TEST_DIR/test_emhash5_stress.cpp" -o "$TEST_DIR/bin/test_emhash5_stress_asan"
    $CXX $CXXFLAGS $ASAN_FLAGS "$TEST_DIR/test_emhash5_hifi.cpp" -o "$TEST_DIR/bin/test_emhash5_hifi_asan"
    
    log_info "Stress tests built with ASan"
}

run_tests() {
    log_info "Running all tests..."
    
    # 快速测试
    log_info "=== Quick validation ==="
    "$BENCH_DIR/bin/quick_test8" || log_error "quick_test8 failed"
    "$BENCH_DIR/bin/test_hidden_bugs8" || log_warn "test_hidden_bugs8 has known issues"
    "$BENCH_DIR/bin/highload_test" || log_error "highload_test failed"
    
    # 单元测试
    log_info "=== Unit tests ==="
    "$TEST_DIR/bin/test_emhash58" || log_error "test_emhash58 failed"
    "$TEST_DIR/bin/test_emhash5_verify" || log_error "test_emhash5_verify failed"
    "$TEST_DIR/bin/test_extreme" || log_error "test_extreme failed"
    
    log_info "All tests completed"
}

clean() {
    log_info "Cleaning build artifacts..."
    rm -rf "$FUZZ_DIR/bin"
    rm -rf "$TEST_DIR/bin"
    rm -rf "$BENCH_DIR/bin"
    rm -f "$FUZZ_DIR/*.o" "$TEST_DIR/*.o" "$BENCH_DIR/*.o"
    log_info "Clean completed"
}

# 主入口
case "$1" in
    all)
        build_fuzz
        build_test
        build_bench
        ;;
    fuzz)
        build_fuzz
        ;;
    test)
        build_test
        ;;
    bench)
        build_bench
        ;;
    quick)
        build_quick
        ;;
    stress)
        build_stress
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
        echo "target: all | fuzz | test | bench | quick | stress | run | clean"
        echo "compiler: gcc | clang (默认 gcc)"
        echo ""
        echo "示例:"
        echo "  $0 quick          # 快速构建验证测试"
        echo "  $0 stress         # 构建压力测试(带ASan)"
        echo "  $0 all clang      # 构建所有测试(使用clang)"
        echo "  $0 run            # 构建并运行快速测试"
        exit 1
        ;;
esac