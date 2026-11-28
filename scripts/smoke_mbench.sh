#!/bin/bash
# Smoke-run a single mbench bench block and report exit code + output.
# usage: smoke_mbench.sh <binary> <sflag> <eflag> [timeout_sec] [extra args...]
BIN=${1:-/tmp/mbench_check}
SF=${2:-8}
EF=${3:-8}
TO=${4:-240}
shift 4 2>/dev/null || shift $#
LOG=/tmp/mb_smoke_$SF.log
cd /mnt/d/emhash/bench || exit 1
timeout "$TO" "$BIN" -b"$SF" -e"$EF" "$@" > "$LOG" 2>&1
rc=$?
echo "exitcode=$rc lines=$(wc -l < "$LOG")"
grep -n "CHECK FAILED\|checksum\|random seed" "$LOG" | head -20
echo "---- output tail ----"
tail -30 "$LOG"
