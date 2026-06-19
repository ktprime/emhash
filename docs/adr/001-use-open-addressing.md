# ADR 001: Use Open Addressing Instead of Chaining

## Status
Accepted (2019)

## Context
The standard library's `std::unordered_map` uses separate chaining (linked list per
bucket). For high-performance scenarios we needed:

- Better cache locality
- Lower per-element memory overhead
- Faster iteration
- Higher load factors

## Decision
All emhash implementations (emhash5/6/7/8) use **open addressing** with a single
inline array, rather than chaining with node allocations.

## Consequences

### Positive
- Cache-friendly: a single contiguous array improves scan and iteration performance
- Lower per-element memory overhead (no next-pointer in each entry)
- Higher achievable load factors (up to 0.999 with `EMH_HIGH_LOAD`)
- Faster iteration (emhash8 achieves near-zero iteration time on integer keys)

### Negative
- **No reference stability** during insert/erase/rehash (see [usage_notes.md](../usage_notes.md))
- Deletion requires care (handled differently per version: emhash7 uses no-tombstone linked-bucket; emhash5/6/8 use bucket chain or backshift deletion)
- Cluster management is critical to performance at high load factors

### Mitigations
- Users can call `reserve()` to pre-allocate and avoid rehash-related reference invalidation
- emhash7's no-tombstone design solves the classic "tombstone accumulation" problem
