#!/bin/bash
# clang-format check for the given files; prints NEED-FORMAT per file that differs.
cd /mnt/d/emhash || exit 1
for f in "$@"; do
    if clang-format-20 --dry-run -Werror "$f" >/dev/null 2>&1; then
        echo "OK $f"
    else
        echo "NEED-FORMAT $f"
        clang-format-20 -i "$f"
    fi
done
