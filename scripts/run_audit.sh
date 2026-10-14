#!/usr/bin/env bash
# Run the lifecycle audit test outside CMake (quick manual check).
# Usage: ./scripts/run_audit.sh        (from repo root or anywhere)

set -e
cd "$(dirname "$0")/.."

g++ -std=c++17 -O2 -Iinclude -Itests -Itests/common \
    tests/memory/test_lifecycle_audit.cpp -o /tmp/test_audit 2>&1
echo "BUILD_EXIT=$?"
/tmp/test_audit 2>&1
echo "RUN_EXIT=$?"
