# emhash Benchmark Framework

A small, zero-dependency benchmark harness and a paired A/B comparison tool for
the emhash/emilib hash tables.

Everything lives here and does **not** touch `../` (the historical `bench/*.cpp`
collection, `Makefile`, and the currently non-buildable `bench/emilib_bench/`).

## Why this exists

The project's optimization notes (`../../include/2026-07-08_1030-map-performance-optimization.md`)
document that on typical hardware a benchmark is **noisy**: same-binary repeats drift by
several percent and a host drifts by >10% over minutes. A naïve "before then after"
comparison therefore produces false positives/negatives every time.

This framework fixes two of those problems:

1. **A reproducible harness** (`emhash_bench`) that prints a bare number for a single
   workload, so results are comparable across machines and sessions.
2. **A paired A/B runner** (`ab.sh` / `ab.ps1`) that interleaves the two binaries and
   counts per-pair wins instead of trusting the means.

## Build

```bash
cmake -B build -S bench/framework -DCMAKE_BUILD_TYPE=Release
cmake --build build
# binaries: build/emhash_bench (matrix), build/emhash_hash_latency (hash chains)
```

`bench_single.cpp` is not a CMake target: it needs a map and a workload to be
chosen at compile time, which `build_single.sh` / `build_single.ps1` do in a
loop (see below).

Optional reference baselines (boost / absl) are enabled automatically when the headers
exist under `thirdparty/`, and skipped silently otherwise. No `-march=native` is forced:
an A/B comparison is only meaningful when both sides are built with identical flags.

## Usage

### Single workload (bare number — what the A/B runner calls)

```bash
./emhash_bench --single --op find_hit --map emhash7 --key int64 --size 100000
```

prints e.g. `549200.000` (total nanoseconds for the workload) and nothing else.

- `--op` : `insert find_hit find_miss iterate erase_all insert_erase churn churn_find_miss`
- `--map`: `emhash5 emhash6 emhash7 emhash8 emhash9 emilib1 emilib2 emilib3 emilib4` (+ `boost`/`absl` when built)
- `--key`: `int32 int64 string`
- `--key-len`: string key bytes, 8..64 (default 32; shorter keys exercise SSO)
- `--metric`: `min` (robust on noisy hosts, default for A/B) or `median`

### Full matrix

```bash
./emhash_bench --suite --maps emhash7,emilib2 --keys int64 --sizes 10000,100000 \
               --ops insert,find_hit --json out.json

# five decimal octaves: 1k, 10k, 100k, 1M, 4M
./emhash_bench --suite --maps emhash9 --keys int64 --ops find_miss --octave
```

`--suite` also prints a derived `erase = erase_all - insert` row: the `erase_all`
workload includes its construction cost (`insert` measures that under identical
conditions), so the difference is a valid pure-erase estimate.

## Workloads are self-contained units

`Bench::run(name, op)` re-invokes `op()` for every epoch and measures the whole call
(there is no pause/resume). Each workload is therefore a complete, idempotent unit:

| workload       | measures                                                     |
|----------------|--------------------------------------------------------------|
| `insert`       | build a map of n keys, no `reserve()` (rehash cost included) |
| `find_hit`     | n successful lookups against a pre-built map                |
| `find_miss`    | n unsuccessful lookups (disjoint key set, by construction)  |
| `iterate`      | sum all values                                               |
| `erase_all`    | build n + delete n                                           |
| `insert_erase` | reach n live, then n×(erase+insert) keeping n live           |
| `churn`        | build n, then n×(erase an old key + insert a fresh disjoint key) |
| `churn_find_miss` | build n, run one full churn round outside timing, then n miss probes |

Hit, miss and churn key sets are **disjoint by construction** (three non-overlapping
integer ranges; string indices offset), not by a luckier seed. The churn pair models
a fixed-size working set being replaced: `churn` measures the writes, while
`churn_find_miss` measures what churn did to the *lookup* path, which is what
move_home/counter designs are supposed to improve.

## A/B comparison

```bash
# one workload
./ab.sh ./bench_base ./bench_cand find_hit emhash7 int64 100000        # 6 pairs by default
./ab.sh ./bench_base ./bench_cand find_hit emhash7 int64 100000 8 2    # 8 pairs, min of 2 runs

# string-key length (default 32 bytes)
AB_KEY_LEN=8 ./ab.sh ./bench_base ./bench_cand find_hit emhash9 string 100000

# a canned slice of workloads
./ab.sh ./bench_base ./bench_cand --all 6 1
```

PowerShell mirror:

```powershell
.\ab.ps1 -BinA .\bench_base.exe -BinB .\bench_cand.exe -Op find_hit -Map emhash7 -Key int64 -Size 100000
.\ab.ps1 -BinA .\bench_base.exe -BinB .\bench_cand.exe -Op find_hit -Map emhash9 -Key string -Size 100000 -KeyLen 8
```

`AB_METRIC=median ./ab.sh ...` switches to the median estimate.

### Methodology (follow it or the results lie)

1. **Interleave** A/B, never "all A then all B".
2. **Alternate** which side runs first within each pair (`ab.sh`/`ab.ps1` do this
   automatically) — otherwise the first-run side picks up a systematic cache/turbo
   advantage.
3. **≥5-6 pairs and ≥3 reps** (both are the defaults). One harness invocation costs
   ~40 ms, so the default `reps=3` min-of-three is nearly free and reduces spread.
   With only 2 pairs a lucky run produced a fake +10%+ verdict even when both binaries
   were identical.
4. **A change is real only when it wins nearly every pair.** A sub-2% mean difference
   with an even win split is noise.
5. **Self-check the tool before trusting any result**: run the *same* binary as both
   A and B. You should see an even win split and a verdict of `NOISE`. Any systematic
   bias means the runner (or the machine) is flawed.

```bash
cp ./emhash_bench ./emhash_bench_same
./ab.sh ./emhash_bench ./emhash_bench_same find_hit emhash7 int64 100000 6 3
```

Measured on the development host (2.8 GHz-class virtual Xeon):

| observation | value |
|---|---|
| spread of one workload across 8 independent invocations, same binary | 558700 … 604350 ns (~±4%) |
| self-check delta, pairs=6 reps=1 | −0.33 % → `NOISE` |
| self-check delta, pairs=6 reps=3 | −0.37 % → `NOISE` |
| self-check delta, pairs=10 reps=3 | −0.81 % → `NOISE` |

The host also has occasional drift episodes: during one run every measurement jumped
to ~690k ns (vs the usual ~570k) and the self-check produced a spurious 1:5 split. That
is exactly why the rule is "wins **nearly every** pair", not "wins on the mean" — and
why you should re-run a surprising result before acting on it.

## One binary per (map, workload): `bench_single`

`bench_main.cpp` registers every map and every workload in one translation unit.
That is what you want for a matrix run, but a real program includes one map, and
the difference in inlining and code layout is larger than most of the effects
this project measures. `bench_single.cpp` includes exactly one map header and
instantiates exactly one workload, so each binary is as small as a real user of
that map.

```bash
# the full matrix: 9 maps x 8 workloads
./build_single.sh

# only what a single experiment needs
CXX=clang++ ./build_single.sh --maps emhash9,emhash8 --ops insert,find_hit

# string keys hashed with the bundled wyhash instead of std::hash<string>
./build_single.sh --maps emhash9 --ops find_hit,insert --hash wyhash --out build_single_wyhash
```

PowerShell mirror (no valgrind there, so no `-Callgrind`):

```powershell
.\build_single.ps1 -Maps "emhash9 emhash8" -Ops "find_hit insert" -Out .\build_single
.\build_single.ps1 -Maps emhash9 -Ops find_hit -StringHash wyhash -Out .\build_single_wyhash
```

Output names are `<map>_<op>[_wyhash][_cg]` (`.exe` on Windows). Every binary
accepts the same `--single` arguments as `bench_main`, and rejects a `--map` or
`--op` that does not match what it was compiled with, so a typo cannot compare
two different workloads:

```bash
./ab.sh ./build_single/emhash9_find_hit ./build_single_wyhash/emhash9_find_hit_wyhash \
        find_hit emhash9 string 100000
```

`--hash wyhash` only affects `std::string` keys. The functor declares
`is_avalanching`, which makes emhash9 skip its extra `mix64`; emhash8 ignores the
marker but also has no final mix enabled by default, so both maps end up hashing
the string with wyhash alone. The same hasher ships as
`emhash9::string_hash` (pair it with `std::equal_to<>`), so what this flag
measures is the hasher users can actually opt into. Key type stays a runtime
option because it does not change which map or workload code is instantiated.

## Instruction counts: `instr_count.sh`

Wall-clock time cannot resolve a 2% difference on this project's hosts, and code
layout can fake one. Executed instructions do not depend on layout, so use
callgrind when an A/B delta is small.

```bash
# needs valgrind (Linux/WSL); --callgrind also adds -gdwarf-4
./build_single.sh --maps emhash9 --ops find_hit --callgrind
./instr_count.sh build_single/emhash9_find_hit_cg --key string --size 100000
```

It prints `Ir` (instructions), `Bc` (branches) and `Bcm` (mispredicts) for
**one** workload invocation, plus the functions covering 95% of Ir:

```
Ir   18883697   instructions
Bc   1727399    branches
Bcm  6690       mispredicts
```

Two details matter:

- Key generation, the map prefill and the map's destructor are outside the counted
  region (`CALLGRIND_START/STOP_INSTRUMENTATION` in `bench_framework.hpp`).
  Destroying 100k `std::string` keys costs more instructions than the 100k lookups
  being measured, so counting "everything after setup" is useless for string keys.
- `-gdwarf-4` is required: valgrind 3.18 does not understand some DWARF5 forms and
  then attributes the whole main executable to `__libc_start_main`. It also writes
  a bogus `summary:` field when instrumentation is stopped; the script reads the
  trailing `totals:` line instead.

## Hash latency: `hash_latency`

Throughput benchmarks hide hash latency because independent hashes overlap. This
target measures the dependency chain (`x = hash(key); write x into key; repeat`),
which is what a random lookup actually pays. It deliberately does not index an
array with the hash, because that would put the array address on the chain.

```bash
cmake --build build --target emhash_hash_latency
./build/emhash_hash_latency
```

```
dependency-chain hash latency, ns per hash (chain depth 2048, key mutated in place)
hasher                          len=8   len=16   len=24   len=64
----------------------------------------------------------------
std::hash<string>               12.18    20.21    30.25    73.51
emhash9 (hash + mix64)          14.08    23.32    32.71    70.66
wyhash (emh_wyhash)              2.73     2.74     4.38     3.73
```

(One run; the host drifts ~10% between runs, so read these as ratios, not absolute
values. Windows / MSVC STL: `std::hash<std::string>` is byte-at-a-time FNV. On
libstdc++ the same rows are ~5/7/8.5/14 ns and wyhash is still 2.7-5 ns.)

A faster hash does not automatically make a map faster: whether it does is a
whole-table question, decided by an A/B of `bench_single --key string` with and
without `--hash wyhash`. The store->load handoff between chain iterations costs a
few cycles and is identical for every row, so differences between rows are hash
latency.

## Notes

- Values are total nanoseconds per workload invocation (not per-op); divide by `--size`
  to get per-element cost.
- String keys are 32 bytes by default (heap-allocated) so insert is measured with real
  allocation cost; `--key-len 8..15` switches to SSO. Only the leading 8 bytes vary per
  key, and they are splitmix64-mixed so the material is pseudorandom: encoding a counter
  makes short keys sequential integers and turns a hash-latency comparison into a
  hash-quality comparison.
- Doubling as a regression smoke test: if you break `find_hit` after a header edit, the
  bare-number output jumps and the A/B runner flags it.