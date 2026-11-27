#!/usr/bin/env bash
set +e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../tests" || exit 1
CXX="${1:-g++-16}"
echo "=== Building with $CXX ==="
FLAGS="-std=c++17 -O2 -I../include -I. -Icommon"
failed=0
total=0
for f in unit/test_crud unit/test_full_api unit/test_copy_move unit/test_iterators unit/test_edge_cases \
         unit/test_string_keys unit/test_special_keys unit/test_reserve_clear unit/test_allocator \
         unit/test_hashset unit/test_stress_correctness unit/test_small_data unit/test_iterator_invalidation \
         unit/test_api_coverage unit/test_api_contract unit/test_emilib_lifecycle unit/test_ht8_optimizations \
         unit/test_ht8_hetero unit/test_emhash9; do
    total=$((total+1))
    bn=$(basename "$f")
    if $CXX $FLAGS "$f.cpp" -o "/tmp/${CXX}_${bn}" 2>/tmp/build_err_${CXX}_${bn}.txt; then
        echo "  OK  $bn"
    else
        echo "  FAIL $bn"
        cat /tmp/build_err_${CXX}_${bn}.txt | tail -3
        failed=$((failed+1))
    fi
done
echo "=== Build: $((total-failed))/$total succeeded, $failed failed ==="

echo ""
echo "=== Running unit tests ==="
runned=0
for f in test_crud test_full_api test_copy_move test_iterators test_edge_cases \
         test_string_keys test_special_keys test_reserve_clear test_allocator \
         test_hashset test_stress_correctness test_small_data test_iterator_invalidation \
         test_api_coverage test_api_contract test_emilib_lifecycle test_ht8_optimizations \
         test_ht8_hetero test_emhash9; do
    runned=$((runned+1))
    exe="/tmp/${CXX}_${f}"
    if [ -x "$exe" ]; then
        if timeout 120 "$exe" >/tmp/run_${CXX}_${f}.log 2>&1; then
            echo "  PASS $f"
        else
            echo "  FAIL $f (exit=$?)"
            tail -5 /tmp/run_${CXX}_${f}.log
        fi
    fi
done
echo "=== Ran $runned tests ==="
