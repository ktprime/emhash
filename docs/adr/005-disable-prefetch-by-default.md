# ADR-005: Disable hardware prefetch by default in emhash5/7/8

Date: 2026-07-19

## Status

Accepted

## Context

`__builtin_prefetch` was used in the `find()` hot path of emhash5/7/8 to
prefetch the next bucket during linear probing, aiming to hide memory access
latency.

Benchmarking on modern x86-64 CPUs (2024-2026 era) revealed that:

1. **Hardware speculation is sufficient**: Modern CPUs use out-of-order
   execution and hardware prefetchers that already hide the latency of the
   next bucket access in the common case (low collision rate).

2. **`__builtin_prefetch` adds instructions**: The explicit prefetch
   instruction consumes a slot in the instruction stream, increasing icache
   pressure without measurable benefit.

3. **Regression in key scenarios**: `find_hit` and `find_miss` benchmarks
   showed 0-5% **regression** when prefetch was enabled, especially for
   small-to-medium maps that fit in L1/L2 cache.

## Decision

Disable prefetch by default via `EMH_NO_READ_PREFETCH` and
`EMH_NO_WRITE_PREFETCH` macros in all emhash5/7/8 headers.

Users who want to re-enable prefetch can define `EMH_ENABLE_PREFETCH`:

```cpp
#define EMH_ENABLE_PREFETCH
#include <emhash/hash_table8.hpp>
```

## Consequences

- Default performance: 0-5% improvement in find paths (removed regression).
- Users on older CPUs (pre-Haswell) can re-enable prefetch if needed.
- Prefetch code remains in the source but is `#ifdef`-guarded.
