#!/usr/bin/env bash
# ============================================================================
# emhash Comprehensive Sanitizer Test Suite
# ----------------------------------------------------------------------------
# Runs all critical test executables under AddressSanitizer, UndefinedBehavior
# Sanitizer, and (on Clang) MemorySanitizer to detect memory safety issues,
# undefined behavior, and use of uninitialized values.
#
# Usage:
#   ./tests/scripts/run_sanitizers.sh           # run all 3 sanitizers
#   ./tests/scripts/run_sanitizers.sh asan      # address only
#   ./tests/scripts/run_sanitizers.sh ubsan     # undefined behavior only
#   ./tests/scripts/run_sanitizers.sh msan      # memory only (Clang)
#
# Environment variables (optional):
#   CXX                Compiler to use (default: clang++ for msan, g++ else)
#   CXXFLAGS_EXTRA     Extra flags (e.g. "-fsanitize=address,undefined")
#   TEST_FILTER        Substring filter; only tests containing it will run
# ============================================================================
set -euo pipefail

MODE="${1:-all}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TESTS_DIR="$ROOT/tests"
BUILD_DIR="$ROOT/build_san"
INCLUDE_DIR="$ROOT/include"

CXX_DEFAULT="g++"
if [[ "$MODE" == "msan" ]] || command -v clang++ >/dev/null 2>&1; then
    CXX_DEFAULT="clang++"
fi
CXX="${CXX:-$CXX_DEFAULT}"

COMMON_FLAGS="-std=c++17 -O1 -g -fno-omit-frame-pointer -I${INCLUDE_DIR}"
CXXFLAGS_EXTRA="${CXXFLAGS_EXTRA:-}"

# Source files to test (whitelist of critical correctness & memory tests).
TEST_SOURCES=(
    "verify/test_all_maps.cpp:test_verify"
    "verify/test_string_key_leak.cpp:test_string_key_leak"
    "verify/test_extreme.cpp:test_extreme"
    "verify/test_emilib_comprehensive.cpp:test_emilib"
    "verify/test_functional_edge.cpp:test_functional_edge"
    "verify/test_interface_combo.cpp:test_interface_combo"
    "verify/test_emhash_allocator.cpp:test_emhash_allocator"
    "verify/test_hidden_bugs8.cpp:test_hidden_bugs8"
    "verify/test_emhash5_verify.cpp:test_emhash5_verify"
    "verify/test_reserve_rehash_clear.cpp:test_reserve_rehash_clear"
    "stress/stress_all_maps.cpp:stress_all_maps"
)

should_run() {
    local mode="$1" want="$2"
    [[ "$want" == "all" ]] && return 0
    [[ "$mode" == "$want" ]] && return 0
    return 1
}

run_sanitizer() {
    local san="$1"
    local san_flags="$2"
    local out_dir="$BUILD_DIR/$san"
    mkdir -p "$out_dir"

    echo "============================================================"
    echo "  $san build  (CXX=$CXX)"
    echo "============================================================"

    local failed=0 passed=0 skipped=0
    for entry in "${TEST_SOURCES[@]}"; do
        local src="${entry%%:*}"
        local name="${entry##*:}"
        if [[ -n "${TEST_FILTER:-}" && "$name" != *"$TEST_FILTER"* ]]; then
            skipped=$((skipped+1)); continue
        fi

        local full_src="$TESTS_DIR/$src"
        if [[ ! -f "$full_src" ]]; then
            echo "  [SKIP] $name  (missing: $src)"
            skipped=$((skipped+1)); continue
        fi

        local bin="$out_dir/${name}_${san}"
        echo "  [BUILD] $name  ->  $bin"

        if ! $CXX $COMMON_FLAGS $san_flags $CXXFLAGS_EXTRA "$full_src" -o "$bin" 2>"$out_dir/${name}.build.log"; then
            echo "    [BUILD FAIL] see $out_dir/${name}.build.log"
            failed=$((failed+1)); continue
        fi

        echo "  [RUN]   $name"
        if "$bin" >"$out_dir/${name}.out" 2>"$out_dir/${name}.run.log"; then
            echo "    [PASS] $name"
            passed=$((passed+1))
        else
            echo "    [FAIL] $name  (exit $?)"
            echo "---- last 30 lines of run log ----"
            tail -n 30 "$out_dir/${name}.run.log" || true
            failed=$((failed+1))
        fi
    done

    echo "------------------------------------------------------------"
    echo "  $san summary:  PASS=$passed  FAIL=$failed  SKIP=$skipped"
    echo "============================================================"
    return $failed
}

if should_run asan "$MODE" || should_run all "$MODE"; then
    run_sanitizer "asan" "-fsanitize=address" || FAILURES=1
fi
if should_run ubsan "$MODE" || should_run all "$MODE"; then
    run_sanitizer "ubsan" "-fsanitize=undefined -fno-sanitize-recover=all" || FAILURES=1
fi
if should_run msan "$MODE" || should_run all "$MODE"; then
    if [[ "$CXX" == "clang++" ]]; then
        run_sanitizer "msan" "-fsanitize=memory -fsanitize-memory-track-origins" || FAILURES=1
    else
        echo "WARNING: MemorySanitizer requires clang++; skipping msan (CXX=$CXX)"
    fi
fi

exit "${FAILURES:-0}"
