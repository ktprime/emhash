#!/usr/bin/env bash
# Git pre-commit hook: check C++ formatting with clang-format
# Install: cp scripts/pre-commit.sh .git/hooks/pre-commit
# Or:       ln -sf ../../scripts/pre-commit.sh .git/hooks/pre-commit

set -euo pipefail

STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(hpp|cpp|h|cc)$' || true)

if [ -z "$STAGED_FILES" ]; then
    exit 0
fi

echo "Checking formatting for ${#STAGED_FILES[@]} file(s)..."

if ! command -v clang-format &>/dev/null; then
    echo "WARNING: clang-format not found, skipping format check"
    exit 0
fi

PASS=true
for FILE in $STAGED_FILES; do
    DIFF=$(clang-format "$FILE" | diff -u "$FILE" - 2>/dev/null || true)
    if [ -n "$DIFF" ]; then
        echo "FAILED: $FILE is not formatted correctly"
        echo "$DIFF"
        PASS=false
    fi
done

if [ "$PASS" = false ]; then
    echo ""
    echo "Run 'clang-format -i <file>' to fix formatting, then re-commit."
    echo "Or run: git diff --cached --name-only | grep -E '\.(hpp|cpp|h|cc)$' | xargs clang-format -i"
    exit 1
fi

echo "All files formatted correctly."
exit 0
