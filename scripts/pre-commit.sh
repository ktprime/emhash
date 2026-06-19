#!/usr/bin/env bash
# Git pre-commit hook for emhash
# Checks clang-format on staged C++ files
# Install: cp scripts/pre-commit.sh .git/hooks/pre-commit
# Or:       ln -sf ../../scripts/pre-commit.sh .git/hooks/pre-commit

set -euo pipefail

STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM -- '*.hpp' '*.cpp' '*.h' '*.cc')

if [ -z "$STAGED_FILES" ]; then
    exit 0
fi

if ! command -v clang-format &>/dev/null; then
    echo "WARNING: clang-format not found, skipping format check"
    exit 0
fi

INVALID=""

for file in $STAGED_FILES; do
    # Skip third-party files
    case "$file" in
        thirdparty/*|bench/tsl_bench/*|bench/qc-core/*|bench/boost_bench/*|include/sse2neon.h)
            continue ;;
    esac

    DIFF=$(clang-format "$file" | diff -u "$file" - 2>/dev/null || true)
    if [ -n "$DIFF" ]; then
        echo "$file needs formatting:"
        echo "$DIFF"
        INVALID=1
    fi
done

if [ -n "$INVALID" ]; then
    echo ""
    echo "Run 'clang-format -i <file>' to fix formatting, then re-commit."
    exit 1
fi
