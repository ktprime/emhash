# Design Principles

## Collision Resolution (varies by version)

| Version | Strategy | High LF Support |
|---------|----------|----------------|
| **emhash5** | Three-way hybrid: linear probing → quadratic probing → bidirectional search | With `EMH_HIGH_LOAD` |
| **emhash6** | Linked-bucket with separate bitmask for fast empty-bucket search | Native (0.80-0.999 via `max_load_factor()`) |
| **emhash7** | Linked-bucket with separate bitmask, no tombstones | Native (0.80-0.999) |
| **emhash8** | Separate index + dense pairs array, linked-bucket chains | With `EMH_HIGH_LOAD` |

> In `emhash/hash_table5.hpp`, the optimized three-way linear probing strategy is still **2-3x faster** than traditional strategies even at load factors **> 0.9**.

## Memory Layout

**emhash5/6/7** — Single inline array with embedded bucket linkage:

```cpp
struct Entry {
    Key key;           // Key
    size_t bucket;     // Bucket chain linkage / state info
    Value value;       // Value
};
// _pairs[bucket] stores key, value, and chain pointer in one struct
// emhash6/7 additionally use a separate _bitmask for fast empty-bucket search
```

**emhash8** — Split index + dense pairs (like `std::vector`):

```cpp
struct Index { size_t next, slot; };  // Per-bucket chain linkage
Index*    _index;   // bucket → slot mapping
value_type* _pairs; // dense, compact key-value array (no metadata)
// _pairs is always packed: _pairs[0].._pairs[_num_filled-1]
// This enables extremely fast iteration (just scan _pairs sequentially)
```

## Primary Bucket Mapping

- Primary bucket is always assigned to `key_hash(key) % size` and **cannot be occupied**
- All operations start searching from the primary bucket
- Avoids circular displacement issues in cuckoo hashing

## Backup Hash Function

Enable backup hash function by defining `EMH_SAFE_HASH` macro to defend against hash attacks (performance cost ~ **10%**).

---

## Implementation Comparison

emhash provides 4 different implementations, each with different focus:

| Implementation | Layout | Collision Strategy | Default LF | Best Scenario |
|----------------|--------|--------------------|------------|---------------|
| **emhash5** | Inline `_pairs[]` | Three-way hybrid probing | 0.80 | Fast lookup/erase with integer keys, SBO support |
| **emhash6** | Inline `_pairs[]` + `_bitmask` | Linked-bucket | 0.80 | Lookup/erase with integer keys, fast empty scan |
| **emhash7** | Inline `_pairs[]` + `_bitmask` | Linked-bucket, no tombstones | 0.80 | High load factor (0.80-0.999), insert-intensive |
| **emhash8** | Separate `_index[]` + dense `_pairs[]` | Linked-bucket | 0.80 | Complex keys/values, extremely fast iteration |

### Selection Guide

- **Complex/large keys/values** (e.g., `std::string`, custom structs) → **emhash8**
- **Insert-intensive workloads** or need high load factor → **emhash7**
- **Fast search and erase with integer keys** → **emhash5/6**
- **Small maps that should avoid heap allocation** → **emhash5** with `EMH_SMALL_SIZE`

### HashSet

emhash also provides `HashSet` implementations:

```cpp
#include "emhash/hash_set2.hpp"   // namespace emhash2
#include "emhash/hash_set3.hpp"   // namespace emhash3
#include "emhash/hash_set4.hpp"   // namespace emhash4
#include "emhash/hash_set8.hpp"   // namespace emhash8

emhash2::HashSet<int> set1;
emhash3::HashSet<int> set2;
emhash4::HashSet<int> set3;
emhash8::HashSet<int> set4;
```

### Custom Allocator

All `HashMap` versions support custom allocators (emhash5/6/7/8):

```cpp
emhash7::HashMap<Key, Val, Hash, Eq, MyAllocator> mymap;
```

---

## emilib Library: SIMD-Accelerated Hash Maps

The emilib library provides three hash map implementations (`emihmap1`, `emihmap2`, `emihmap3`) that share a common design philosophy: **SIMD-accelerated open addressing with metadata-byte filtering**. Unlike emhash's linked-bucket approach, emilib uses Swiss-table-style byte-level metadata to enable vectorized lookups, achieving extremely fast search performance on modern CPUs.

### Common Design Foundation

All three implementations share these core principles:

**1. Dual-Hash Metadata Encoding**

Each key produces two hash values from a single hash computation:

```
hash = hasher(key)
H1 = hash & _mask          // primary bucket index (aligned to SIMD group)
H2 = (hash % 253) + 3      // 1-byte metadata tag (range [3..255])
```

- `H1` determines the starting SIMD group for probing
- `H2` is stored as a 1-byte tag in the `_states[]` array, enabling SIMD-filtered lookup before key comparison

**2. SIMD Group Probing**

Buckets are organized in groups of `simd_bytes` (16 for SSE2, 32 for AVX2, 64 for AVX-512). A single SIMD instruction compares 16/32/64 metadata bytes simultaneously:

```cpp
// Pseudocode: find key in hash map
vec = SIMD_LOAD(&_states[group_start])       // load 16 metadata bytes
mask = SIMD_MOVEMASK(SIMD_CMPEQ(vec, H2))    // find matching H2 tags
for each set_bit in mask:
    if key == _pairs[bucket].first: return bucket  // verify full key
```

This reduces key comparisons by ~93.75% (15/16 buckets filtered by H2 mismatch on SSE2).

**3. State Encoding**

Each bucket has a 1-byte state tag in `_states[]`:

| State | Value | Meaning |
|-------|-------|---------|
| `EEMPTY` | -128 (0x80) | Never occupied |
| `EDELETE` | -127 (0x81) | Was occupied, now deleted (tombstone) |
| `EFILLED`+ | 3..255 | Occupied; value = H2 tag |

The sentinel value `ESENTINEL` (127) marks the end of the `_states` array.

**4. Data Layout**

```
_states[]:  [H2][H2][EMP][DEL][H2]...[SENTINEL]  ← metadata (1 byte/bucket)
_pairs[]:   [KV][KV][   ][   ][KV]...            ← key-value pairs
```

`_pairs[i]` and `_states[i]` share the same bucket index, enabling direct indexing.

---

### emihmap1: Group-Probing with Inline Probe Depth

**Namespace:** `emilib` | **File:** `emilib/emihmap1.hpp`

#### Core Design

emihmap1 uses a **group-based probing** strategy where the probe depth is stored inline within the `_states` array itself. Each SIMD group's last byte encodes the maximum probe offset for that group, eliminating the need for a separate offset array.

#### Internal Structure

```cpp
class HashMap {
    HashT   _hasher;
    EqT     _eq;
    int8_t* _states;          // metadata + inline probe depth
    PairT*  _pairs;           // key-value storage
    size_t  _num_buckets;     // total bucket count (power of 2)
    size_t  _mask;            // _num_buckets - 1
    size_t  _num_filled;      // number of occupied entries
};
// Max load factor: 5/6 ≈ 0.833 (hardcoded MXLOAD_FACTOR = 5)
```

#### Key Mechanism: Inline Group Probe Depth

In each SIMD group of 16 buckets, 15 slots store H2 tags and 1 slot (the `group_index` position) stores the probe depth for that group:

```
Group layout (16 bytes):
[H2][H2][H2]...[H2][PROBE_DEPTH]  ← 15 data slots + 1 probe depth slot
 ^--- slot_size = 15 ---^  ^-- group_index = 15
```

```cpp
// Read probe depth for a group
int group_probe(size_t gbucket) const {
    return _states[gbucket + group_index];  // last byte of group
}

// Set probe depth when inserting
void set_group_probe(size_t gbucket, int offset) {
    if (offset > _states[gbucket + group_index])
        _states[gbucket + group_index] = offset;
}
```

#### Lookup Algorithm

```cpp
find_filled_bucket(key):
    H2 = hash_key2(main_bucket, key)   // get H1 (aligned) + H2
    next_bucket = main_bucket
    offset = 0
    while true:
        vec = SIMD_LOAD(&_states[next_bucket])
        mask = MOVEMASK(CMPEQ(vec, H2)) & group_bmask  // filter H2 matches
        for each match in mask:
            if _pairs[slot].first == key: return slot    // full key compare
        if ++offset > group_probe(main_bucket):          // check inline depth
            return NOT_FOUND
        next_bucket = get_next_bucket(next_bucket, offset)
```

#### Erase Strategy

When erasing, the bucket state is set to `EEMPTY` or `EDELETE` based on whether the group still contains other filled entries:

```cpp
_erase(bucket):
    _pairs[slot].~PairT()
    _states[bucket] = group_has_empty(bucket) ? EEMPTY : EDELETE
```

This optimization allows the search to terminate early when encountering `EEMPTY` (no need to probe further), while `EDELETE` indicates potential collisions beyond.

#### Performance Characteristics

| Operation | Average | Worst Case |
|-----------|---------|------------|
| Lookup | O(1) ~1-2 group probes | O(n) pathological hash |
| Insert | O(1) amortized | O(n) + rehash |
| Erase | O(1) | O(n) |
| Iteration | O(n/filled_ratio) with SIMD | O(n) |
| Space overhead | 1 byte/bucket (states) | ~6.25% for pairs |

#### Key Optimizations

- **Zero extra offset storage**: Probe depth embedded in `_states[]` group trailer byte
- **Early termination**: `EEMPTY` state allows search to stop without probing all groups
- **SIMD iterator**: `filled_mask()` uses SIMD to skip empty groups during iteration
- **Prefetch**: `_mm_prefetch` on the pair data when H2 matches are found

---

### emihmap2: PSL-Based Probing with Separate Offset Array

**Namespace:** `emilib2` | **File:** `emilib/emihmap2.hpp`

#### Core Design

emihmap2 uses a **Probe Sequence Length (PSL)** approach with a separate `_offset[]` array that stores the maximum probe distance for each primary bucket. This decouples metadata from probe tracking, allowing all 16 bytes per SIMD group to store H2 tags (no group trailer byte wasted).

#### Internal Structure

```cpp
class HashMap {
    HashT    _hasher;
    EqT      _eq;
    int8_t*  _states;          // metadata only (all 16 bytes = H2 tags)
    uint8_t* _offset;          // per-group probe depth (1 byte per OFFSET_STEP buckets)
    PairT*   _pairs;           // key-value storage
    size_t   _num_buckets;
    size_t   _mask;
    size_t   _num_filled;
    uint32_t _mlf;             // load factor as fixed-point (1<<28 / lf)
};
// Default load factor: 0.84, configurable 0.25..0.999
```

#### Key Mechanism: Separate Offset Array

The `_offset[]` array stores the maximum probe distance per group of `OFFSET_STEP` (default 8) buckets:

```cpp
// Read max probe distance for a primary bucket
uint32_t get_offset(main_bucket) const {
    return _offset[main_bucket / OFFSET_STEP];
}

// Update probe distance when inserting further than before
void set_offset(main_bucket, off) {
    _offset[main_bucket / OFFSET_STEP] = off;
}
```

With `EMH_SAFE_PSL`, offsets > 128 are encoded as `128 + off/128`, supporting probe distances up to 16,384.

#### Probing Strategy

emihmap2 supports multiple probing strategies via compile-time macros:

```cpp
get_next_bucket(next_bucket, offset):
    #if EMH_PSL_LINEAR
        return (next_bucket + 3 * simd_bytes) & _mask     // fixed-step linear
    #elif offset < 5
        return (next_bucket + simd_bytes * offset) & _mask // quadratic-like
    #else
        return (next_bucket + _num_buckets/11 + 1) & _mask // jump probing
```

#### Lookup Algorithm

```cpp
find_filled_bucket(key):
    H2 = hash_key2(main_bucket, key)
    next_bucket = main_bucket

    // First group: check H2 matches AND early-empty termination
    vec = SIMD_LOAD(&_states[next_bucket])
    mask = MOVEMASK(CMPEQ(vec, H2))
    for each match: if key match: return bucket
    if MOVEMASK(CMPEQ(vec, simd_empty)): return NOT_FOUND  // early exit!
    if get_offset(main_bucket) == 0: return NOT_FOUND       // no probes yet

    // Subsequent groups
    do:
        next_bucket = get_next_bucket(next_bucket, ++offset)
        vec = SIMD_LOAD(&_states[next_bucket])
        mask = MOVEMASK(CMPEQ(vec, H2))
        for each match: if key match: return bucket
    while offset <= get_offset(main_bucket)
    return NOT_FOUND
```

#### Erase Strategy

With `EMH_PSL_LINEAR` and `EMH_PSL_ERASE`, emihmap2 can perform **backward deletion** — when a bucket is cleared and the next bucket is empty, it backtracks to convert `EDELETE` markers to `EEMPTY`, improving future lookup performance:

```cpp
_erase(bucket):
    _pairs[bucket].~PairT()
    #if EMH_PSL_LINEAR
        _states[bucket] = (_states[bucket+1] == EEMPTY) ? EEMPTY : EDELETE
    #else
        _states[bucket] = EDELETE
    #endif

    #if EMH_PSL_ERASE && EMH_PSL_LINEAR
        if _states[bucket] == EEMPTY:
            bucket = (bucket - 1) & _mask
            while _states[bucket] == EDELETE:
                _states[bucket] = EEMPTY
                _offset[bucket/OFFSET_STEP] = 0
                bucket = (bucket - 1) & _mask
    #endif
```

#### Performance Characteristics

| Operation | Average | Worst Case |
|-----------|---------|------------|
| Lookup | O(1) ~1-2 group probes | O(n) pathological hash |
| Insert | O(1) amortized | O(n) + rehash |
| Erase | O(1), backward deletion O(k) | O(n) |
| Iteration | O(n/filled_ratio) with SIMD | O(n) |
| Space overhead | 1 byte/bucket (states) + 1 byte/OFFSET_STEP buckets (offset) | ~6.3% |

#### Key Optimizations

- **Full SIMD utilization**: All 16 bytes per group store H2 tags (no probe depth byte)
- **Early empty detection**: First group checks for `EEMPTY` to terminate search immediately
- **Configurable probing**: Linear, quadratic, or jump probing via compile-time macros
- **Backward deletion**: Converts tombstones back to empty, reducing probe lengths
- **AVX2/AVX-512 support**: Runtime-selectable SIMD width (SSE2/AVX2/AVX-512)

---

### emihmap3: Global PSL with Adaptive Probing

**Namespace:** `emilib3` | **File:** `emilib/emihmap3.hpp`

#### Core Design

emihmap3 uses a **single global `_max_probe_length`** variable instead of per-group offset tracking. This simplifies the data structure at the cost of less precise search termination. It also uses a **group-level empty check** (`group_mask`) that stores a summary byte per SIMD group.

#### Internal Structure

```cpp
class HashMap {
    HashT    _hasher;
    EqT      _eq;
    int8_t*  _states;           // metadata + group summary bytes
    PairT*   _pairs;            // key-value storage
    size_t   _num_buckets;
    size_t   _mask;
    size_t   _num_filled;
    size_t   _max_probe_length; // global maximum probe distance
    uint32_t _mlf;              // load factor as fixed-point
};
// Default load factor: 0.84, configurable 0.25..0.999
```

#### Key Mechanism: Group Mask Summary

Each SIMD group's last byte (`_states[group_start + 15]`) serves as a **group summary**: it is set to `0` when the group contains only `EEMPTY` entries, allowing the search to terminate early at the group level:

```cpp
// Check if an entire group is empty
int8_t group_mask(gbucket) const {
    return _states[gbucket + simd_bytes - 1];  // 0 = all empty
}
```

#### Lookup Algorithm

```cpp
find_filled_bucket(key):
    H2 = hash_key2(main_bucket, key)
    next_bucket = main_bucket
    offset = 0
    do:
        vec = SIMD_LOAD(&_states[next_bucket])
        mask = MOVEMASK(CMPEQ(vec, H2))
        for each match: if key match: return bucket
        if group_mask(next_bucket) == EEMPTY: return NOT_FOUND  // group-level early exit
        if offset >= _max_probe_length: return NOT_FOUND         // global PSL limit
        next_bucket = get_next_bucket(next_bucket, ++offset)
    while true
```

#### Probing Strategy

```cpp
get_next_bucket(next_bucket, offset):
    #if EMH_PSL_LINEAR
        next_bucket += 3 * simd_bytes
        if next_bucket >= _num_buckets:
            next_bucket += simd_bytes          // skip past wrap
    #else
        if offset < 7:
            next_bucket += simd_bytes * offset // quadratic-like
        else:
            next_bucket += _num_buckets/8 + simd_bytes  // jump
    #endif
    return next_bucket & _mask
```

#### Erase Strategy

Uses the same group-aware erase as emihmap1, checking if the group still has entries:

```cpp
_erase(bucket):
    _pairs[bucket].~PairT()
    gbucket = bucket / simd_bytes * simd_bytes
    _states[bucket] = (group_mask(gbucket) == EEMPTY) ? EEMPTY : EDELETE
```

#### Performance Characteristics

| Operation | Average | Worst Case |
|-----------|---------|------------|
| Lookup | O(1) ~1-2 group probes | O(n) pathological hash |
| Insert | O(1) amortized | O(n) + rehash |
| Erase | O(1) | O(n) |
| Iteration | O(n/filled_ratio) with SIMD | O(n) |
| Space overhead | 1 byte/bucket (states) | ~6.25% for pairs |

#### Key Optimizations

- **Minimal metadata**: Single `_max_probe_length` variable, no per-group offset array
- **Group-level early exit**: `group_mask` summary byte enables fast empty-group detection
- **Adaptive probing**: Hybrid quadratic + jump probing for better cache behavior
- **SIMD iterator**: Same `filled_mask()` optimization as emihmap1

---

### emilib Comparison Matrix

| Feature | emihmap1 | emihmap2 | emihmap3 |
|---------|----------|----------|----------|
| **Namespace** | `emilib` | `emilib2` | `emilib3` |
| **Probe tracking** | Inline (group trailer byte) | Separate `_offset[]` array | Global `_max_probe_length` |
| **SIMD group utilization** | 15/16 slots (93.75%) | 16/16 slots (100%) | 15/16 slots (93.75%) |
| **Early termination** | Per-group probe depth | Per-group offset + EEMPTY check | Group mask + global PSL |
| **Erase optimization** | Group-aware EEMPTY/EDELETE | Backward deletion (optional) | Group-aware EEMPTY/EDELETE |
| **Load factor** | Fixed 5/6 ≈ 0.833 | Configurable 0.25..0.999 | Configurable 0.25..0.999 |
| **Extra memory** | None | `_offset[]` (~0.125 bytes/bucket) | None |
| **Probing strategy** | Quadratic + jump | Linear / quadratic / jump | Quadratic + jump |
| **SIMD width** | SSE2 / AVX2 | SSE2 / AVX2 / AVX-512 | SSE2 / AVX2 / AVX-512 |
| **Metadata per bucket** | 1 byte | 1 byte | 1 byte |

### emilib vs emhash: Design Philosophy

| Aspect | emhash (5/6/7/8) | emilib (1/2/3) |
|--------|-------------------|-----------------|
| **Collision resolution** | Linked-bucket chains | Swiss-table-style byte probing |
| **Metadata** | Embedded in bucket struct | Separate `_states[]` byte array |
| **SIMD usage** | Limited (CTZ/bitmask) | Pervasive (H2 filtering, iteration) |
| **Key comparison** | Always required | Filtered by H2 tag (~94% skipped) |
| **Memory overhead** | 1 pointer/bucket (linkage) | 1 byte/bucket (metadata) |
| **Cache behavior** | Chasing bucket links | Linear scan of `_states[]` |
| **Best for** | General purpose, high LF | SIMD-friendly integer keys, read-heavy |

### Selection Guide for emilib

- **emihmap1** — Best for fixed workloads with stable load factors; simplest data structure with zero extra overhead
- **emihmap2** — Best for variable workloads needing high load factors (up to 0.999); most sophisticated erase optimization; full SIMD group utilization
- **emihmap3** — Best balanced option; minimal metadata with group-level early exit; good default choice for most SIMD-accelerated use cases

### Usage

```cpp
#include "emilib/emihmap1.hpp"  // namespace emilib
#include "emilib/emihmap2.hpp"  // namespace emilib2
#include "emilib/emihmap3.hpp"  // namespace emilib3

emilib::HashMap<int, std::string> map1;
emilib2::HashMap<int, std::string> map2;
emilib3::HashMap<int, std::string> map3;
```
