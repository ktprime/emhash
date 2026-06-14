# Design Principles

## Collision Resolution (varies by version)

| Version | Strategy | High LF Support |
|---------|----------|----------------|
| **emhash5** | Three-way hybrid: linear probing → quadratic probing → bidirectional search | With `EMH_HIGH_LOAD` |
| **emhash6** | Linked-bucket with separate bitmask for fast empty-bucket search | With `EMH_HIGH_LOAD` |
| **emhash7** | Linked-bucket with separate bitmask, no tombstones | Native (0.80-0.999) |
| **emhash8** | Separate index + dense pairs array, linked-bucket chains | With `EMH_HIGH_LOAD` |

> In `hash_table5.hpp`, the optimized three-way linear probing strategy is still **2-3x faster** than traditional strategies even at load factors **> 0.9**.

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
#include "hash_set3.hpp"   // Based on emhash7, namespace emhash7
#include "hash_set81.hpp"  // Based on emhash8, namespace emhash8

emhash7::HashSet<int> set1;
emhash8::HashSet<int> set2;
```

### Custom Allocator

All `HashMap` versions support custom allocators (emhash5/6/7/8):

```cpp
emhash7::HashMap<Key, Val, Hash, Eq, MyAllocator> mymap;
```
