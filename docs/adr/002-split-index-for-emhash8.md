# ADR 002: emhash8 — Separate Index + Dense Pairs Layout

## Status
Accepted (2020)

## Context
Earlier implementations (emhash5/6/7) embed bucket linkage and state into the entry
itself, which works well for integer keys but causes overhead for complex keys/values:

- Wasted space when `sizeof(value) % 8 != sizeof(key) % 8`
- Slower iteration when entry metadata is interleaved
- Harder to provide stable iteration order

## Decision
emhash8 uses a **two-array layout**:
- `_index[ bucket ]` — per-bucket chain linkage (`{ next, slot }`)
- `_pairs[ slot ]` — dense, packed `value_type` array

The dense `_pairs` array is always packed: `_pairs[0].._pairs[_num_filled-1]`.

## Consequences

### Positive
- **Extremely fast iteration**: just a sequential scan over `_pairs` (0.00 ms for 10M elements)
- No metadata overhead in the value array
- Clean separation: index/lookup uses `_index`, iteration uses `_pairs`

### Negative
- Slightly higher constant factor for insert/erase (two arrays to update)
- Slightly higher memory usage for small maps (two allocations instead of one)

### When to use
- Complex/large keys/values (e.g., `std::string`, custom structs)
- Iteration-heavy workloads (e.g., serialization, dumping)
- Code that needs to copy/move the dense pair array directly
