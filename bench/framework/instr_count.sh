#!/usr/bin/env bash
# instr_count.sh — instruction/branch counts for one single-map workload.
#
# Time on a noisy host cannot resolve a 2% difference; executed instructions can,
# because Ir does not depend on code layout. This runs one workload exactly once
# under callgrind and reports Ir (instructions), Bc (branches) and Bcm
# (mispredicts) for the workload body (docs §6 row 7, §7 item 3).
#
# usage:
#   ./instr_count.sh <single-binary> [--key string] [--size 100000] [--seed N]
#
# The binary must come from build_single.sh --callgrind: instrumentation is off
# outside the measured call, so key generation, the map prefill and -- crucially
# for string keys -- freeing every key afterwards are not counted. `--single
# --once` is added automatically; a bench_main binary rejects `--once` and fails
# loudly instead of reporting a meaningless count.
#
# environment:
#   VALGRIND      valgrind executable (default: valgrind from PATH)
#   ANNOTATE      callgrind_annotate executable (default: from PATH)
#
# example (WSL):
#   ./build_single.sh --maps emhash9 --ops find_hit --callgrind
#   ./instr_count.sh build_single/emhash9_find_hit_cg --key string --size 100000

set -euo pipefail

die() {
    echo "error: $*" >&2
    exit 2
}

if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    [ $# -ge 1 ] || exit 2
    exit 0
fi

bin=$1
shift

[ -x "$bin" ] || die "'$bin' is not an executable"

valgrind=${VALGRIND:-valgrind}
annotate=${ANNOTATE:-callgrind_annotate}
command -v "$valgrind" >/dev/null || die "valgrind not found (apt install valgrind)"
command -v "$annotate" >/dev/null || die "callgrind_annotate not found (apt install valgrind)"

out="${TMPDIR:-/tmp}/emhash-callgrind.$$"
rm -f "$out"*
trap 'rm -f "$out"*' EXIT

echo "== $bin $* (one invocation) =="
# --quiet keeps valgrind's startup banner out of the way; the harness's stdout is
# a bare number we do not need here.
"$valgrind" --tool=callgrind --branch-sim=yes --callgrind-out-file="$out" --quiet \
    "$bin" --single --once "$@" >/dev/null

if [ ! -f "$out" ]; then
    # Valgrind appends .<pid> when the configured name has no %p.
    out=$(ls -1 "$out".* 2>/dev/null | head -n 1)
    [ -n "$out" ] || die "callgrind produced no output file"
fi

# valgrind 3.18 writes a bogus `summary:` when instrumentation is stopped via a
# client request, but the trailing `totals:` line is correct.
totals=$(grep '^totals:' "$out" | tail -n 1)
[ -n "$totals" ] || die "no totals line in $out"

# shellcheck disable=SC2086
set -- $totals # totals:  <Ir>  <Bc>  <Bcm>
echo
echo "=== workload totals (setup and teardown excluded) ==="
printf 'Ir   %s   instructions\n' "${2:-?}"
printf 'Bc   %s   branches\n' "${3:-?}"
printf 'Bcm  %s   mispredicts\n' "${4:-?}"

echo
echo "=== functions by self cost, covering 95% of Ir ==="
"$annotate" --auto=no --inclusive=no --show-percs=no --threshold=95 "$out" | grep -v 'PROGRAM TOTALS'
