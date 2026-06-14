# ADR 003: emhash7 — No-Tombstone Linked-Bucket Design

## Status
Accepted (2020)

## Context
Open addressing has a long-standing problem: deletions leave "tombstones" that
degrade lookup performance over time. The classic solutions are:
- Periodic rehash on insert
- Backward-shift deletion (Robin Hood variants)
- Swap-with-last + pop

We needed stable performance in insert/erase-heavy workloads without periodic
rehashes.

## Decision
emhash7 uses a **linked-bucket** layout with **no tombstones**:
- Each bucket points to a chain of displaced entries
- On erase, the chain is repaired in-place
- A separate `_bitmask` enables fast empty-bucket scanning

## Consequences

### Positive
- **No performance degradation** during sustained insert/erase
- Achieves load factors up to 0.999 natively (no `EMH_HIGH_LOAD` required)
- Fast empty-bucket scan via bitmask

### Negative
- More complex erase logic
- Slightly slower erase than emhash5/6 (chain repair)
- Slightly higher memory usage (one extra word per entry for chain)

### When to use
- **Insert-intensive workloads** that need sustained performance
- Workloads with frequent insert/erase cycles
- Applications targeting LF > 0.80
