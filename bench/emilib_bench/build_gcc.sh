#!/usr/bin/env bash
set -euo pipefail

# === emilib_bench GCC build script ===

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_gcc"

mkdir -p "${BUILD_DIR}"

echo "=== Building emilib_bench with g++ ==="

g++ -std=c++17 -O2 -march=native \
    -I"${SCRIPT_DIR}/../../include" \
    -I"${SCRIPT_DIR}/../../thirdparty" \
    -I"${SCRIPT_DIR}/../.." \
    -I"${SCRIPT_DIR}" \
    -D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING \
    -DNOMINMAX \
    "${SCRIPT_DIR}"/*.cpp \
    -o "${BUILD_DIR}/emilib_bench" \
    -lpthread

echo "=== Running emilib_bench ==="
echo ""

"${BUILD_DIR}/emilib_bench" "$@"

echo ""
echo "Done."
