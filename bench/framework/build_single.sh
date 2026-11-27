#!/usr/bin/env bash
# build_single.sh — build the "one binary = one map x one workload" benchmarks.
#
# bench_main.cpp compiles every map into one TU, which is fine for a matrix run
# but changes inlining and code layout. Any verdict below ~10% has to come from
# a binary that contains exactly one map and one workload; this script builds
# that matrix (docs §6 row 7).
#
# usage:
#   ./build_single.sh [options]
#
# options:
#   --maps "emhash8 emhash9"   maps to build (default: all nine)
#   --ops  "insert find_hit"   workloads to build (default: all six)
#   --out DIR                  output directory (default: ./build_single)
#   --compiler PATH            C++ compiler (default: $CXX, else c++)
#   --std 17|20|23             language standard (default: 17)
#   --hash wyhash              hash string keys with wyhash (-DBENCH_WYHASH=1)
#   --callgrind                -DEMH_BENCH_CALLGRIND=1 plus -gdwarf-4, for
#                              instr_count.sh (needs the valgrind headers).
#                              DWARF5 trips valgrind 3.18's parser and destroys
#                              per-function attribution; DWARF4 is fine.
#                              Linux/WSL only.
#   --extra "FLAGS"            extra compiler flags, e.g. "-mavx2 -flto"
#   --list                     print the resolved matrix and exit
#
# Output names: <map>_<op>[_wyhash][_cg]. Every binary accepts bench_main.cpp's
# --single arguments (--map/--op are verified against the compiled-in values),
# so ab.sh and ab.ps1 can drive paired A/B runs unchanged.
#
# Value types are fixed at int; key type is a runtime option (--key), because it
# does not change which map/workload code is instantiated.
#
# examples:
#   ./build_single.sh --maps "emhash9" --ops "insert find_hit"
#   ./build_single.sh --maps emhash9 --ops find_hit --hash wyhash
#   ./build_single.sh --maps emhash9 --ops insert --callgrind --out build_single_cg

set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/../.." && pwd)

maps="emhash5 emhash6 emhash7 emhash8 emhash9 emilib1 emilib2 emilib3 emilib4"
ops="insert find_hit find_miss iterate erase_all insert_erase churn churn_find_miss"
out="$here/build_single"
compiler="${CXX:-c++}"
std=17
hash_flavour=""
callgrind=0
extra=""
list_only=0

die() {
    echo "error: $*" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        --maps) maps=${2:-}; shift 2 ;;
        --ops) ops=${2:-}; shift 2 ;;
        --out) out=${2:-}; shift 2 ;;
        --compiler) compiler=${2:-}; shift 2 ;;
        --std) std=${2:-}; shift 2 ;;
        --hash) hash_flavour=${2:-}; shift 2 ;;
        --callgrind) callgrind=1; shift ;;
        --extra) extra=${2:-}; shift 2 ;;
        --list) list_only=1; shift ;;
        -h|--help) sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option '$1' (try --help)" ;;
    esac
done

[ -n "$maps" ] || die "--maps must not be empty"
[ -n "$ops" ] || die "--ops must not be empty"
# Accept comma-separated lists as well as space-separated ones, so the scripts can
# be called from shells that mangle embedded quotes (e.g. Windows PowerShell -> WSL).
maps=${maps//,/ }
ops=${ops//,/ }
case "$hash_flavour" in
    ""|wyhash) ;;
    *) die "--hash must be 'wyhash' (got '$hash_flavour')" ;;
esac

map_include() {
    case "$1" in
        emhash5) echo "emhash/hash_table5.hpp" ;;
        emhash6) echo "emhash/hash_table6.hpp" ;;
        emhash7) echo "emhash/hash_table7.hpp" ;;
        emhash8) echo "emhash/hash_table8.hpp" ;;
        emhash9) echo "emhash/hash_table9.hpp" ;;
        emilib1) echo "emilib/emihmap1.hpp" ;;
        emilib2) echo "emilib/emihmap2.hpp" ;;
        emilib3) echo "emilib/emihmap3.hpp" ;;
        emilib4) echo "emilib/emihmap4.hpp" ;;
        *) return 1 ;;
    esac
}

map_template() {
    case "$1" in
        emhash5) echo "emhash5::HashMap" ;;
        emhash6) echo "emhash6::HashMap" ;;
        emhash7) echo "emhash7::HashMap" ;;
        emhash8) echo "emhash8::HashMap" ;;
        emhash9) echo "emhash9::HashMap" ;;
        emilib1) echo "emilib::HashMap" ;;
        emilib2) echo "emilib2::HashMap" ;;
        emilib3) echo "emilib3::HashMap" ;;
        emilib4) echo "emilib4::HashMap" ;;
        *) return 1 ;;
    esac
}

op_token() {
    case "$1" in
        insert) echo "Insert" ;;
        find_hit) echo "FindHit" ;;
        find_miss) echo "FindMiss" ;;
        iterate) echo "Iterate" ;;
        erase_all) echo "EraseAll" ;;
        insert_erase) echo "InsertErase" ;;
        churn) echo "Churn" ;;
        churn_find_miss) echo "ChurnFindMiss" ;;
        *) return 1 ;;
    esac
}

# Resolve the matrix up front so a typo fails before compiling anything.
resolved=""
for m in $maps; do
    inc=$(map_include "$m") || die "unknown map '$m'"
    tmpl=$(map_template "$m") || die "unknown map '$m'"
    for o in $ops; do
        tok=$(op_token "$o") || die "unknown op '$o'"
        name="${m}_${o}"
        [ -n "$hash_flavour" ] && name="${name}_${hash_flavour}"
        [ "$callgrind" -eq 1 ] && name="${name}_cg"
        resolved="${resolved}${name}|${m}|${o}|${inc}|${tmpl}|${tok}"$'\n'
    done
done

if [ "$list_only" -eq 1 ]; then
    printf '%s' "$resolved" | while IFS='|' read -r name _m _o inc tmpl tok; do
        [ -n "$name" ] && printf '%-28s %-40s %s\n' "$name" "$inc" "$tok"
    done
    exit 0
fi

command -v "$compiler" >/dev/null || die "compiler '$compiler' not found (use --compiler)"
mkdir -p "$out"

flags=(-O3 -g -DNDEBUG "-std=c++${std}" -Wall -Wextra -Wno-unused-parameter
       -I "$root/include" -I "$here" -I "$here/..")
[ "$callgrind" -eq 1 ] && flags+=(-DEMH_BENCH_CALLGRIND=1 -gdwarf-4)
[ "$hash_flavour" = "wyhash" ] && flags+=(-DBENCH_WYHASH=1)

extra_flags=()
if [ -n "$extra" ]; then
    # Deliberate word splitting: --extra is a flag list, not a single argument.
    read -r -a extra_flags <<< "$extra"
    flags+=("${extra_flags[@]}")
fi

printf '%s' "$resolved" | while IFS='|' read -r name m op inc tmpl tok; do
    [ -n "$name" ] || continue
    bin="$out/$name"
    printf 'compile %-24s <- %s / %s\n' "$name" "$inc" "$op"
    "$compiler" "${flags[@]}" \
        -DBENCH_MAP_INC="<$inc>" \
        -DBENCH_MAP_TMPL="$tmpl" \
        -DBENCH_MAP_NAME="$m" \
        -DBENCH_OP="$tok" \
        "$here/bench_single.cpp" -o "$bin"
done

echo
echo "built into $out"
ls -1 "$out"
