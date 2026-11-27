#!/usr/bin/env bash
# Quick CI: rerun core tests with existing binaries
fail=0
for t in test_crud test_full_api test_edge_cases test_string_keys test_emilib_lifecycle test_ht8_optimizations; do
    exe="/tmp/g++-16_$t"
    if [ -x "$exe" ]; then
        if timeout 60 "$exe" >/dev/null 2>&1; then
            echo "PASS $t"
        else
            echo "FAIL $t"; fail=1
        fi
    else
        echo "MISSING $t"; fail=1
    fi
done
echo "QUICK_CI_EXIT=$fail"
