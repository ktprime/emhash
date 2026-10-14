#!/bin/bash
# Run the string-key leak test outside CMake (quick manual check).
# Usage: ./scripts/run_test_leak.sh    (from repo root or anywhere)

set -e
cd "$(dirname "$0")/.."

g++ -std=c++17 -O2 -Iinclude -Itests -Itests/common \
    tests/memory/test_string_key_leak.cpp -o /tmp/test_leak 2>&1
echo "--- compile done ---"
/tmp/test_leak 2>&1
echo "RUN_EXIT=$?"
