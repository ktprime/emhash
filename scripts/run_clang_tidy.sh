#!/usr/bin/env bash
# Run clang-tidy on emhash test sources using compile_commands.json
#
# Usage:
#   ./scripts/run_clang_tidy.sh [--all] [--ci] [--strict] [--threshold N] [--build DIR]
#
# Options:
#   --all         Run on all unit+memory test files (default: representative subset)
#   --ci          CI mode: emit GitHub annotations, write step summary
#   --strict      Fail on any warning (not just errors)
#   --threshold N Fail if warning count exceeds N (default: -1 = no limit)
#   --build DIR   Path to build directory containing compile_commands.json
#                 (default: tests/build)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults
ALL=false
CI_MODE=false
STRICT=false
THRESHOLD=-1
BUILD_DIR="$ROOT_DIR/tests/build"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)       ALL=true; shift ;;
        --ci)        CI_MODE=true; shift ;;
        --strict)    STRICT=true; shift ;;
        --threshold) THRESHOLD="$2"; shift 2 ;;
        --build)     BUILD_DIR="$2"; shift 2 ;;
        *)           echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# clang-tidy binary
TIDY="${CLANG_TIDY:-clang-tidy-18}"
if ! command -v "$TIDY" &>/dev/null; then
    echo "ERROR: $TIDY not found. Install clang-tidy-18 or set CLANG_TIDY." >&2
    exit 1
fi

# Verify compile_commands.json exists
if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
    echo "ERROR: $BUILD_DIR/compile_commands.json not found." >&2
    echo "Run: cmake -B $BUILD_DIR -S $ROOT_DIR/tests" >&2
    exit 1
fi

# Select source files
REPRESENTATIVE_FILES=(
    "$ROOT_DIR/tests/unit/test_crud.cpp"
    "$ROOT_DIR/tests/unit/test_full_api.cpp"
    "$ROOT_DIR/tests/unit/test_edge_cases.cpp"
    "$ROOT_DIR/tests/unit/test_string_keys.cpp"
    "$ROOT_DIR/tests/unit/test_hashset.cpp"
    "$ROOT_DIR/tests/memory/test_lifecycle_audit.cpp"
)

if [[ "$ALL" == "true" ]]; then
    FILES=()
    for f in "$ROOT_DIR"/tests/unit/*.cpp "$ROOT_DIR"/tests/memory/*.cpp; do
        FILES+=("$f")
    done
else
    FILES=("${REPRESENTATIVE_FILES[@]}")
fi

# Run clang-tidy
ERROR_COUNT=0
WARNING_COUNT=0
TOTAL_FILES=${#FILES[@]}
CURRENT=0

echo "clang-tidy: checking $TOTAL_FILES file(s) with $TIDY"
echo "  Build dir: $BUILD_DIR"
echo ""

for f in "${FILES[@]}"; do
    CURRENT=$((CURRENT + 1))
    REL="${f#$ROOT_DIR/}"
    echo "[$CURRENT/$TOTAL_FILES] $TIDY $REL"

    OUTPUT=$("$TIDY" -p "$BUILD_DIR" "$f" 2>&1) || true

    # Count errors and warnings in output
    ERRS=$(echo "$OUTPUT" | grep -c "error:" || true)
    WARNS=$(echo "$OUTPUT" | grep -c "warning:" || true)
    ERROR_COUNT=$((ERROR_COUNT + ERRS))
    WARNING_COUNT=$((WARNING_COUNT + WARNS))

    # Emit output
    if [[ -n "$OUTPUT" ]]; then
        echo "$OUTPUT"

        if [[ "$CI_MODE" == "true" ]]; then
            while IFS= read -r line; do
                if echo "$line" | grep -q "error:"; then
                    echo "::error::$line"
                elif echo "$line" | grep -q "warning:"; then
                    echo "::warning::$line"
                fi
            done <<< "$OUTPUT"
        fi
    fi
done

# Summary
echo ""
echo "========================================="
echo "clang-tidy summary"
echo "  Files checked: $TOTAL_FILES"
echo "  Errors:        $ERROR_COUNT"
echo "  Warnings:      $WARNING_COUNT"
echo "========================================="

if [[ "$CI_MODE" == "true" && -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    {
        echo "### clang-tidy Results"
        echo ""
        echo "| Metric | Count |"
        echo "|--------|-------|"
        echo "| Files checked | $TOTAL_FILES |"
        echo "| Errors | $ERROR_COUNT |"
        echo "| Warnings | $WARNING_COUNT |"
    } >> "$GITHUB_STEP_SUMMARY"
fi

# Exit logic
EXIT_CODE=0
if [[ "$ERROR_COUNT" -gt 0 ]]; then
    echo "FAIL: $ERROR_COUNT error(s) found by clang-tidy"
    EXIT_CODE=1
elif [[ "$STRICT" == "true" && "$WARNING_COUNT" -gt 0 ]]; then
    echo "FAIL: $WARNING_COUNT warning(s) found in strict mode"
    EXIT_CODE=1
elif [[ "$THRESHOLD" -ge 0 && "$WARNING_COUNT" -gt "$THRESHOLD" ]]; then
    echo "FAIL: $WARNING_COUNT warning(s) exceed threshold of $THRESHOLD"
    EXIT_CODE=1
else
    echo "clang-tidy check passed"
fi

exit $EXIT_CODE
