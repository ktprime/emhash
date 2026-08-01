# ADR-006: Bitmask front layout for emhash6/7

Date: 2026-07-19

## Status

Accepted

## Context

emhash6 and emhash7 use a bitmask (1 bit per slot) to track occupied/empty
bucket states, enabling SIMD-accelerated group scanning (similar to
abseil flat_hash_map's "Swiss table" design).

The original layout placed the bitmask **after** the pairs array:

```
[pairs[0..N-1]] [sentinel] [bitmask (16B aligned)]
```

This required two separate memory allocations and an extra pointer dereference
to access the bitmask during find operations.

## Decision

Use a **single block allocation** with the bitmask placed at the front
(16-byte aligned):

```
[bitmask (16B aligned)] [pairs[0..N-1]] [sentinel]
```

Implementation details:
- `bitmask_aligned_size()` ensures 16-byte alignment for the bitmask
- `AllocSize()` computes the total allocation size in a single call
- `_bitmask` points to the allocation base; `_pairs` is derived as
  `reinterpret_cast<PairT*>(_bitmask + bitmask_size)`
- `clone()` uses single `memcpy` for trivially-copyable types
- `rehash()` saves old bitmask (`obmask`) for deallocation
- Destructor uses `reinterpret_cast<PairT*>(_bitmask)` as base address for
  deallocation

## Consequences

- **Positive**: Single allocation reduces `malloc` overhead and improves
  cache locality. Bitmask access is faster due to pointer proximity.
- **Negative (GCC-16 only)**: ~2-12% regression for int key Insert at 10M+
  data size due to additional offset calculation instructions.
- **Positive (Clang-20)**: Consistent 3-8% improvement for string key
  operations across data sizes.
- **Trade-off**: The Clang improvement and memory efficiency outweigh the
  GCC regression, which is expected to improve with future GCC releases.

## Related

- [emihmap4 swiss table experiment](007-emihmap4-swiss-table-experiment.md)
- Project memory: "emhash6/7 memory layout must be
  [bitmask (16B aligned)] [pairs[0..N-1]] [sentinel] with single block
  allocation"
