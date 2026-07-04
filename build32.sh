#!/bin/bash
cd /mnt/d/emhash/tests
FLAGS="-m32 -std=c++17 -O2 -msse2 -Wall -Wextra -Wno-unused-parameter -I../include -I. -Icommon"
for f in unit/test_crud unit/test_edge_cases memory/test_string_key_leak memory/test_lifecycle_audit; do
    echo "=== $f ==="
    g++ $FLAGS $f.cpp -o /tmp/$(basename $f)_32 2>&1 | grep -iE "warning|error" | head -20
done
echo DONE
