#!/usr/bin/env bash
# ab.sh — paired A/B comparison runner for the emhash benchmark framework.
#
# Implements the methodology validated in the project's optimization handoff notes
# (include/2026-07-08_1030-map-performance-optimization.md, "Benchmark methodology"):
#
#   1. Interleave the runs (A B A B ...) instead of running all A then all B.
#      Same-binary repeats drift by several percent and hosts drift over minutes,
#      so a naive "before then after" comparison produces false positives.
#   2. Count per-pair wins, not just the means.
#   3. Only trust a change that wins nearly every pair. A sub-2% mean difference
#      with no consistent pairing advantage is noise.
#
# Requires the harness to print a bare number on stdout (its --single mode).
#
# usage:
#   ab.sh <binA> <binB> <op> <map> <key> <size> [pairs] [reps]
#   ab.sh <binA> <binB> --all [pairs] [reps]
#
#   op    : insert find_hit find_miss iterate erase_all insert_erase
#   key   : int32 int64 string
#   pairs : number of interleaved A/B pairs (default 6)
#   reps  : harness invocations per side per pair, min is kept (default 3)
#
# examples:
#   ./ab.sh ./bench_base ./bench_cand find_hit emhash7 int64 100000
#   ./ab.sh ./bench_base ./bench_cand --all 6 2

set -u

METRIC="${AB_METRIC:-min}"
PAIRS_DEFAULT=6
# One harness invocation costs ~40ms, so taking the min of several is cheap and
# measurably reduces run-to-run spread on a noisy host.
REPS_DEFAULT=3

usage() {
    sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
}

[ $# -lt 2 ] && usage

BIN_A=$1
BIN_B=$2
shift 2

for b in "$BIN_A" "$BIN_B"; do
    if [ ! -x "$b" ]; then
        echo "error: '$b' is not an executable" >&2
        exit 2
    fi
done

# --- workload matrix for --all ------------------------------------------------
# "op map key size" tuples, swept in order.
ALL_WORKLOADS=(
    "insert      emhash7 int64  100000"
    "find_hit    emhash7 int64  100000"
    "find_miss   emhash7 int64  100000"
    "erase_all   emhash7 int64  100000"
    "insert_erase emhash7 int64 100000"
    "insert      emhash7 string 100000"
    "find_hit    emhash7 string 100000"
)

if [ "${1:-}" = "--all" ]; then
    PAIRS=${2:-$PAIRS_DEFAULT}
    REPS=${3:-$REPS_DEFAULT}
    MODE=all
else
    [ $# -lt 4 ] && usage
    W_OP=$1; W_MAP=$2; W_KEY=$3; W_SIZE=$4
    PAIRS=${5:-$PAIRS_DEFAULT}
    REPS=${6:-$REPS_DEFAULT}
    MODE=single
fi

# --- helpers ------------------------------------------------------------------

# Run one side once; print the harness's bare number.
run_side() {
    "$1" --single --op "$2" --map "$3" --key "$4" --size "$5" --metric "$METRIC"
}

# min of $REPS invocations
run_side_min() {
    local bin=$1 op=$2 map=$3 key=$4 size=$5 best="" v
    local i=0
    while [ "$i" -lt "$REPS" ]; do
        v=$(run_side "$bin" "$op" "$map" "$key" "$size")
        if [ -z "$best" ]; then
            best=$v
        else
            best=$(awk -v a="$best" -v b="$v" 'BEGIN{print (b<a)?b:a}')
        fi
        i=$((i + 1))
    done
    printf '%s' "$best"
}

# Compare one workload; prints a one-line verdict and appends a TSV row to $2.
compare_workload() {
    local op=$1 map=$2 key=$3 size=$4 outfile=$5
    local a_wins=0 b_wins=0 ties=0
    local a_sum=0 b_sum=0
    local i=1

    printf '  %-13s %-8s %-7s %-8s ' "$op" "$map" "$key" "$size"

    while [ "$i" -le "$PAIRS" ]; do
        local ra rb
        # Alternate which side runs first. Running A first in every pair leaves a
        # systematic first-run advantage with A (cache/turbo/thermal effects), which
        # shows up as a fake win rate even when both binaries are identical.
        if [ $((i % 2)) -eq 1 ]; then
            ra=$(run_side_min "$BIN_A" "$op" "$map" "$key" "$size")
            rb=$(run_side_min "$BIN_B" "$op" "$map" "$key" "$size")
        else
            rb=$(run_side_min "$BIN_B" "$op" "$map" "$key" "$size")
            ra=$(run_side_min "$BIN_A" "$op" "$map" "$key" "$size")
        fi

        if [ -z "$ra" ] || [ -z "$rb" ]; then
            echo "ERROR: harness produced no output" >&2
            return 1
        fi

        read -r win asum bsum <<EOF
$(awk -v a="$ra" -v b="$rb" 'BEGIN{
    if (a < b) w="A"; else if (b < a) w="B"; else w="=";
    printf "%s %s %s", w, a, b
}')
EOF
        case "$win" in
            A) a_wins=$((a_wins + 1)) ;;
            B) b_wins=$((b_wins + 1)) ;;
            *) ties=$((ties + 1)) ;;
        esac
        a_sum=$(awk -v s="$a_sum" -v x="$asum" 'BEGIN{print s+x}')
        b_sum=$(awk -v s="$b_sum" -v x="$bsum" 'BEGIN{print s+x}')
        i=$((i + 1))
    done

    read -r a_mean b_mean delta <<EOF
$(awk -v as="$a_sum" -v bs="$b_sum" -v p="$PAIRS" -v aw="$a_wins" -v bw="$b_wins" 'BEGIN{
    am = as/p; bm = bs/p;
    d = (bm != 0) ? (am - bm) / bm * 100.0 : 0.0;
    # negative delta => A is faster (lower ns is better)
    if (aw == p)      v = "A faster (all pairs)";
    else if (bw == p) v = "B faster (all pairs)";
    else if (d < -2.0) v = "A faster (majority)";
    else if (d >  2.0) v = "B faster (majority)";
    else               v = "NOISE (<2%)";
    printf "%.1f %.1f %+.2f%%|%s", am, bm, d, v
}')
EOF
    # awk emitted "<delta>|<verdict>"; split the two apart.
    local delta_part verdict_part
    delta_part="${delta%%|*}"
    verdict_part="${delta##*|}"

    printf 'A=%s B=%s wins %d:%d  delta=%s  %s\n' \
        "$a_mean" "$b_mean" "$a_wins" "$b_wins" "$delta_part" "$verdict_part"

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%s\t%s\n' \
        "$op" "$map" "$key" "$size" "$a_mean" "$b_mean" "$a_wins" "$b_wins" \
        "$delta_part" "$verdict_part" >>"$outfile"
}

# --- main ---------------------------------------------------------------------

echo "=== paired A/B comparison ==="
echo "A     : $BIN_A"
echo "B     : $BIN_B"
echo "metric: $METRIC   pairs: $PAIRS   reps: $REPS"
if [ "$PAIRS" -lt 5 ]; then
    echo
    echo "WARNING: only $PAIRS pairs requested. On a noisy host a single lucky run can"
    echo "         produce a fake 'unanimous' verdict. Use at least 5-6 pairs (the default"
    echo "         is $PAIRS_DEFAULT) before trusting any result."
fi
echo

TSV=$(mktemp)
trap 'rm -f "$TSV"' EXIT

if [ "$MODE" = all ]; then
    echo "  workload      map      key     size     result"
    echo "  ----------------------------------------------------------------"
    for w in "${ALL_WORKLOADS[@]}"; do
        # shellcheck disable=SC2086
        compare_workload $w "$TSV"
    done
else
    compare_workload "$W_OP" "$W_MAP" "$W_KEY" "$W_SIZE" "$TSV"
fi

echo
echo "=== summary (negative delta => A faster; lower ns is better) ==="
printf '%-13s %-8s %-7s %-8s %10s %10s %8s %8s %9s  %s\n' \
    op map key size A_ns B_ns A_wins B_wins delta verdict
printf '%s\n' "-----------------------------------------------------------------------------------------------"
awk -F'\t' '{
    printf "%-13s %-8s %-7s %-8s %10.1f %10.1f %8d %8d %9s  %s\n", \
        $1, $2, $3, $4, $5, $6, $7, $8, $9, $10
}' "$TSV"

REAL=$(awk -F'\t' '$10 ~ /faster \(all pairs\)/ {c++} END{print c+0}' "$TSV")
NOISE=$(awk -F'\t' '$10 ~ /NOISE/ {c++} END{print c+0}' "$TSV")
TOTAL=$(wc -l <"$TSV" | tr -d ' ')

echo
echo "verdict: ${REAL}/${TOTAL} workloads show a unanimous winner, ${NOISE}/${TOTAL} are within noise."
echo "Reminder: a change is only real if it wins nearly every pair, not just on the mean."
