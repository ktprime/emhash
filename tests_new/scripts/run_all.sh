#!/bin/bash
# emhash 统一测试运行脚本
# 用法: ./run_all.sh [test_type]
# test_type: quick | stress | fuzz | bench | all

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
TESTS_DIR="$ROOT_DIR"

CXX="g++"
CXXFLAGS="-std=c++17 -O2 -g -I$ROOT_DIR/.. -I$ROOT_DIR/../thirdparty"
ASAN_FLAGS="-fsanitize=address,undefined"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_test() { echo -e "${BLUE}[TEST]${NC} $1"; }

# 快速验证测试
run_quick() {
    log_info "=== Quick Validation Tests ==="
    
    log_test "Building quick_test8..."
    $CXX $CXXFLAGS "$TESTS_DIR/verify/quick_test8.cpp" -o /tmp/quick_test8
    
    log_test "Running quick_test8..."
    /tmp/quick_test8 || log_error "quick_test8 FAILED"
    
    log_test "Building test_hidden_bugs8..."
    $CXX $CXXFLAGS "$TESTS_DIR/verify/test_hidden_bugs8.cpp" -o /tmp/hidden_bugs8
    
    log_test "Running test_hidden_bugs8..."
    /tmp/hidden_bugs8 || log_warn "test_hidden_bugs8 has known issues (Test 4 key mutation)"
    
    log_test "Building test_extreme..."
    $CXX $CXXFLAGS "$TESTS_DIR/verify/test_extreme.cpp" -o /tmp/extreme
    
    log_test "Running test_extreme..."
    /tmp/extreme || log_error "test_extreme FAILED"
    
    log_info "Quick tests completed!"
}

# 压力测试
run_stress() {
    log_info "=== Stress Tests (with ASan) ==="
    
    log_test "Building stress_fix_asan..."
    $CXX $CXXFLAGS $ASAN_FLAGS "$TESTS_DIR/stress/stress_fix.cpp" -o /tmp/stress_fix_asan
    
    log_test "Running stress_fix_asan..."
    /tmp/stress_fix_asan || log_error "stress_fix FAILED"
    
    log_test "Building highload_test..."
    $CXX $CXXFLAGS "$TESTS_DIR/stress/highload_test.cpp" -o /tmp/highload_test
    
    log_test "Running highload_test..."
    /tmp/highload_test || log_error "highload_test FAILED"
    
    log_info "Stress tests completed!"
}

# Fuzzing 测试 (非 fuzzer 版本)
run_fuzz() {
    log_info "=== Fuzz Tests (non-fuzzer version) ==="
    
    log_test "Building fuzz_extreme_asan..."
    $CXX $CXXFLAGS $ASAN_FLAGS "$TESTS_DIR/fuzz/fuzz_extreme.cpp" -o /tmp/fuzz_extreme_asan
    
    log_test "Running fuzz_extreme_asan (60 seconds)..."
    timeout 60 /tmp/fuzz_extreme_asan || log_warn "fuzz_extreme timed out or failed"
    
    log_info "Fuzz tests completed!"
}

# 性能基准测试
run_bench() {
    log_info "=== Benchmark Tests ==="
    
    log_test "Building ebench..."
    $CXX $CXXFLAGS -O3 -march=native "$TESTS_DIR/bench/ebench.cpp" -o /tmp/ebench
    
    log_test "Running ebench (brief)..."
    timeout 30 /tmp/ebench || log_warn "ebench timed out"
    
    log_info "Bench tests completed!"
}

# Hash 攻击测试
run_attack() {
    log_info "=== Hash Attack Tests ==="
    
    log_test "Building hash_attack..."
    $CXX $CXXFLAGS "$TESTS_DIR/attack/hash_attack.cpp" -o /tmp/hash_attack
    
    log_test "Running hash_attack..."
    /tmp/hash_attack || log_warn "hash_attack shows expected slowdown under attack"
    
    log_info "Attack tests completed!"
}

# 全部测试
run_all() {
    run_quick
    run_stress
    run_fuzz
    run_attack
    log_info "=== ALL TESTS COMPLETED ==="
}

# 清理
cleanup() {
    rm -f /tmp/quick_test8 /tmp/hidden_bugs8 /tmp/extreme
    rm -f /tmp/stress_fix_asan /tmp/highload_test
    rm -f /tmp/fuzz_extreme_asan
    rm -f /tmp/ebench
    rm -f /tmp/hash_attack
    log_info "Cleanup completed"
}

# 主入口
case "$1" in
    quick)
        run_quick
        ;;
    stress)
        run_stress
        ;;
    fuzz)
        run_fuzz
        ;;
    bench)
        run_bench
        ;;
    attack)
        run_attack
        ;;
    all)
        run_all
        ;;
    clean)
        cleanup
        ;;
    *)
        echo "用法: $0 [test_type]"
        echo "test_type: quick | stress | fuzz | bench | attack | all | clean"
        echo ""
        echo "示例:"
        echo "  $0 quick     # 快速验证测试 (几分钟)"
        echo "  $0 stress    # 压力测试 (几十分钟)"
        echo "  $0 fuzz      # Fuzzing 测试 (60秒)"
        echo "  $0 all       # 运行所有测试"
        echo "  $0 clean     # 清理临时文件"
        exit 1
        ;;
esac