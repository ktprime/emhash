# emhash benchmark framework

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
# binary: build/emhash_bench
```

Optional reference baselines (boost / absl) are enabled automatically when the headers
exist under `thirdparty/`, and skipped silently otherwise. No `-march=native` is forced:
an A/B comparison is only meaningful when both sides are built with identical flags.

## Usage

### Single workload (bare number — what the A/B runner calls)

```bash
./emhash_bench --single --op find_hit --map emhash7 --key int64 --size 100000
```

prints e.g. `549200.000` (total nanoseconds for the workload) and nothing else.

- `--op` : `insert find_hit find_miss iterate erase_all insert_erase`
- `--map`: `emhash5 emhash6 emhash7 emhash8 emilib1 emilib2 emilib3 emilib4` (+ `boost`/`absl` when built)
- `--key`: `int32 int64 string`
- `--metric`: `min` (robust on noisy hosts, default for A/B) or `median`

### Full matrix

```bash
./emhash_bench --suite --maps emhash7,emilib2 --keys int64 --sizes 10000,100000 \
               --ops insert,find_hit --json out.json
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

Hit and miss key sets are **disjoint by construction** (integer ranges that don't
overlap; string indices offset), not by a luckier seed.

## A/B comparison

```bash
# one workload
./ab.sh ./bench_base ./bench_cand find_hit emhash7 int64 100000        # 6 pairs by default
./ab.sh ./bench_base ./bench_cand find_hit emhash7 int64 100000 8 2    # 8 pairs, min of 2 runs

# a canned slice of workloads
./ab.sh ./bench_base ./bench_cand --all 6 1
```

PowerShell mirror:

```powershell
.\ab.ps1 -BinA .\bench_base.exe -BinB .\bench_cand.exe -Op find_hit -Map emhash7 -Key int64 -Size 100000
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

## Notes

- Values are total nanoseconds per workload invocation (not per-op); divide by `--size`
  to get per-element cost.
- String keys are 32 bytes (heap-allocated) so insert is measured with real allocation
  cost; only the leading 8 bytes vary per key.
- Doubling as a regression smoke test: if you break `find_hit` after a header edit, the
  bare-number output jumps and the A/B runner flags it.