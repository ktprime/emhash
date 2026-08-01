# ADR-007: emihmap4 Swiss table experiment — findings and status

Date: 2026-07-19

## Status

Experimental (not recommended for production)

## Context

`emihmap4` (in `include/emilib/emihmap4.hpp`) is an experimental SIMD-accelerated
hash map implementation inspired by Google's Swiss table design (used in
abseil's `flat_hash_map`). It uses 1-byte metadata per slot (8 states) and
SSE2/AVX2 group scanning.

The goal was to provide a drop-in replacement for emhash7/8 with faster
metadata scanning on modern CPUs.

## Findings

### Performance Results

| Scenario              | emihmap4 vs Boost (Clang-20) | emihmap4 vs Boost (GCC-16) |
|----------------------|------------------------------|----------------------------|
| Int key Insert (10K)  | -66% to -84%                 | -78% to -80%               |
| Int key Insert (100K) | -66% to -84%                 | -78% to -80%               |
| String key operations  | Comparable to emhash7/8      | Comparable to emhash7/8    |

### Root Cause Analysis

The performance gap in small-to-medium int key Insert scenarios is caused by:

1. **High rehash frequency**: The growth strategy triggers frequent rehash
   in the 10K-100K range, causing O(n) overhead per rehash.

2. **Metadata overhead**: 1 byte per slot (vs 1 bit in emhash6/7) means
   8x more metadata cache pressure.

3. **Inline impact**: Forcing `EMH_INLINE` leads to code bloat and icache
   pressure; removing it does not fully resolve the gap.

The performance difference is primarily caused by **underlying
implementation differences** (rehash strategy, metadata density) rather
than inline details.

## Decision

1. **emihmap4 remains experimental**: Not recommended for production use
   due to Insert performance regression in small-to-medium int key scenarios.

2. **emihmap2/3 are the recommended SIMD-accelerated alternatives**: They
   use the bitmask front layout (see [ADR-006](006-bitmask-front-layout-emhash6-7.md))
   and do not have the rehash frequency issue.

3. **LUT optimization rejected**: Replacing `hash % 253` with a lookup table
   in emihmap2/3 caused +3-45% degradation due to memory load latency vs
   compiler-optimized multiplication inverse.

## Consequences

- emihmap4 is kept for research purposes but excluded from the
  "recommended" set in documentation.
- Future SIMD optimization efforts should focus on emihmap2/3
  (bitmask-based) rather than emihmap4 (full Swiss table).
- The experiment validated that **1-bit metadata** (emhash6/7) is
  preferable to **1-byte metadata** (emihmap4) for this workload profile.

## Related

- [ADR-006: Bitmask front layout](006-bitmask-front-layout-emhash6-7.md)
- Project memory: "Swiss table (emihmap4) has structural issues with high
  rehash frequency in small-to-medium int key Insert scenarios"
