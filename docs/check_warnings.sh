#!/usr/bin/env bash
# Check compiler warnings across all test files
# Usage: ./docs/check_warnings.sh [gcc|clang] [c++-standard]
#   compiler: gcc (default) or clang
#   standard: c++17 (default) or c++20

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

COMPILER="${1:-gcc}"
STANDARD="${2:-c++17}"

case "$COMPILER" in
    gcc)   CXX="g++" ;;
    clang) CXX="clang++" ;;
    *)     echo "ERROR: Unknown compiler '$COMPILER'. Use 'gcc' or 'clang'." >&2; exit 1 ;;
esac

if ! command -v "$CXX" &>/dev/null; then
    echo "ERROR: Compiler '$CXX' not found." >&2
    exit 1
fi

echo "Checking warnings with $CXX -std=$STANDARD ..."
echo ""

WARNING_COUNT=0
for f in "$ROOT_DIR"/tests/verify/*.cpp "$ROOT_DIR"/tests/stress/*.cpp \
         "$ROOT_DIR"/tests/attack/*.cpp "$ROOT_DIR"/tests/debug/*.cpp; do
    rel="${f#$ROOT_DIR/}"
    echo "=== $rel ==="
    warnings=$($CXX -std="$STANDARD" -O2 -Wall -Wextra -I"$ROOT_DIR/include" -c "$f" -o /dev/null 2>&1 | grep "warning:" || true)
    if [ -n "$warnings" ]; then
        echo "$warnings"
        WARNING_COUNT=$((WARNING_COUNT + $(echo "$warnings" | wc -l)))
    fi
done

echo ""
if [ "$WARNING_COUNT" -gt 0 ]; then
    echo "Total warnings: $WARNING_COUNT"
else
    echo "No warnings found."
fi
