# Usage Notes

## 0. Thread Safety

emhash is **not thread-safe**. Concurrent access from multiple threads requires external synchronization:

```cpp
// ❌ Wrong: concurrent access without synchronization
// Thread 1: map[key] = val;
// Thread 2: map.find(key);

// ✅ Correct: use mutex for concurrent access
std::mutex mtx;
{
    std::lock_guard<std::mutex> lock(mtx);
    map[key] = val;
}

// ✅ Correct: concurrent read-only access is safe
// Multiple threads can call find()/contains()/count() simultaneously
// as long as no thread is modifying the map
```

## 1. Non-Node-Based Hash Table

emhash **does not guarantee** reference stability during insert/erase/rehash operations.

```cpp
// ❌ Wrong: reference may become invalid after rehash
auto& ref = myhash[1];
myhash[2] = ref;  // Danger! May trigger rehash

// ✅ Correct: copy value first
auto val = myhash[1];
myhash.reserve(10000);  // Pre-allocate
myhash[2] = val;
```

## 2. Erasing During Iteration

Deleting elements during iteration may cause some keys to be visited twice or skipped.

```cpp
// ❌ Wrong: direct deletion without updating iterator
for (const auto& [key, value] : myhash) {
    if (condition) myhash.erase(key);  // UB: modifying container during range-for
}
```

```cpp
// ✅ Correct: use iterator erase and update
for (auto it = myhash.begin(); it != myhash.end(); ) {
    if (condition) {
        it = myhash.erase(it);  // Returns next valid iterator
    } else {
        ++it;
    }
}
```

```cpp
// ✅ Correct: use erase_if for conditional removal (C++17)
auto removed = myhash.erase_if([](const auto& pair) {
    return pair.second < 0;
});
```

## 3. Large Value Type Optimization

For very large value types (e.g., `> 100 bytes`), consider using pointer storage:

```cpp
// High memory usage
emhash7::HashMap<Key, LargeValue> map1;

// Optimized memory usage
emhash7::HashMap<Key, std::unique_ptr<LargeValue>> map2;
```

## 4. Load Factor and Rehashing

The default max load factor is 0.80. When the load factor is exceeded, the table automatically rehashes to a larger size.

```cpp
emhash7::HashMap<int, int> map;
map.max_load_factor(0.95f);  // Allow higher density (fewer rehashes, slower lookups)

// Pre-allocate to avoid rehashing during critical paths
map.reserve(expected_size);
```

> emhash7 supports load factors up to 0.999 without performance collapse. Other versions support high load factors with the `EMH_HIGH_LOAD` macro.

## 5. Choosing the Right Version

| Scenario | Recommended Version | Reason |
|----------|-------------------|--------|
| General purpose, integer keys | `emhash7` | Best all-around performance |
| Complex/large keys or values | `emhash8` | Dense iteration, no per-bucket overhead |
| Read-heavy, SIMD-friendly keys | `emilib::HashMap` | H2 tag filtering reduces key comparisons |
| Need high load factor (>0.9) | `emhash7` | Native support up to 0.999 |
| Frequent insert/erase cycles | `emhash7` | No tombstones, no degradation |

## 6. Custom Hash Function

For user-defined key types, provide a hash function and equality comparator:

```cpp
struct Point {
    int x, y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

struct PointHash {
    size_t operator()(const Point& p) const {
        return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
    }
};

emhash7::HashMap<Point, int, PointHash> point_map;
```

## 7. Exception Safety

emhash operations themselves do not throw exceptions. The exception safety level depends on the operation and the underlying key/value types. Below is the guarantee per operation category:

### Insert / Emplace (Basic Guarantee)

If the key or value type's constructor, copy, or move operation throws during insertion, the table remains in a valid, consistent state. The element will simply not be inserted. No memory is leaked and all internal invariants are preserved.

```cpp
// If ValueT's constructor throws, the map is still valid
// but the element was not inserted
try {
    map.emplace(key, value_that_may_throw);
} catch (...) {
    // map is still valid, element was not added
    assert(map.size() == original_size);
}
```

### operator[] / insert_or_assign (Basic Guarantee)

Same as insert — if construction of the default value or assignment throws, the table state remains valid.

### Emplace (Basic Guarantee)

If the forwarded constructor arguments throw during in-place construction, the table is unchanged.

### Erase / clear (Noexcept / Strong Guarantee)

Erase and clear operations do not allocate memory and never throw (assuming key/value destructors do not throw, which is the C++ convention).

```cpp
// Erase is noexcept in practice
map.erase(key);  // never throws
map.clear();     // never throws
```

### reserve / rehash (Strong Guarantee or Basic Guarantee)

If the allocator's allocation or the element move/copy constructor throws during rehash:

- **If `std::is_nothrow_move_constructible_v<value_type>` is true** (e.g., keys and values are primitive types or smart pointers): rehashing is safe and provides a **strong guarantee** — the table is rolled back to its original state on failure.
- **Otherwise**: a **basic guarantee** is provided — the table remains valid but some elements may have been moved.

```cpp
// Prefer nothrow-movable types for best rehash safety
static_assert(std::is_nothrow_move_constructible_v<std::pair<KeyT, ValueT>>,
              "rehash may be slower and less safe");
```

### Iterator Operations (Noexcept)

All iterator operations (`++`, `--`, `*`, `->`, comparisons) are noexcept.

### Summary

| Operation | Guarantee | Notes |
|-----------|-----------|-------|
| `insert` / `emplace` / `operator[]` | Basic | Unsuccessful insert leaves table unchanged |
| `insert_unique` / `emplace_unique` | Basic | Same as insert |
| `erase` / `clear` | Noexcept | Destructors assumed not to throw |
| `find` / `contains` / `count` | Noexcept | Read-only, no allocation |
| `reserve` / `rehash` | Strong* | *If value_type is nothrow-move-constructible; otherwise Basic |
| `swap` | Noexcept | Swaps internal pointers |
| Iterator operations | Noexcept | `++`, `--`, `*`, `->`, comparisons |

## 8. Iterator and Reference Invalidation

emhash is an **open-addressing flat hash table**. Because key-value pairs are stored inline in a contiguous array, many operations can move elements and invalidate iterators and references.

### Invalidation Rules by Operation

| Operation | Iterators Invalidated | References Invalidated |
|-----------|----------------------|----------------------|
| `insert` / `emplace` / `operator[]` | **All** if rehash triggered; none otherwise | **All** if rehash triggered; none otherwise |
| `erase(it)` | Only `it` and any iterator pointing to the same element | Only the erased element's reference |
| `erase(key)` | Only the erased element's iterator | Only the erased element's reference |
| `erase_if` | Only iterators to erased elements | Only references to erased elements |
| `clear` | **All** | **All** |
| `reserve(n)` | **All** if rehash triggered | **All** if rehash triggered |
| `rehash(n)` | **All** | **All** |
| `shrink_to_fit` | **All** (always rehashes) | **All** (always rehashes) |
| `swap` | Swapped (iterators refer to the other container after swap) | Swapped (references refer to the other container after swap) |
| `find` / `contains` / `count` | None (read-only) | None (read-only) |
| `begin` / `end` | N/A | N/A |

### Key Consequences

1. **Do not hold references across rehash operations.** If an insert might trigger a rehash (e.g., you haven't reserved enough space), copy values rather than holding references.

```cpp
// ❌ Wrong: reference may become invalid after insert triggers rehash
auto& ref = myhash[1];
myhash[2] = ref;  // Danger: myhash may rehash on insert

// ✅ Correct: copy value first, ensure capacity
auto val = myhash[1];
myhash.reserve(myhash.size() + 1);
myhash[2] = val;
```

2. **Erase during iteration — always update the iterator.**

```cpp
// ✅ Correct: use erase returning next iterator
for (auto it = myhash.begin(); it != myhash.end(); ) {
    if (condition(it)) {
        it = myhash.erase(it);
    } else {
        ++it;
    }
}
```

3. **Pre-reserve to avoid unexpected invalidation in hot paths.**

```cpp
// Pre-allocate enough buckets before a critical section
map.reserve(expected_max_size);

// Now inserts will not invalidate iterators/references
// (unless size exceeds expected_max_size)
```

### Comparison with std::unordered_map

| Property | emhash (open addressing) | std::unordered_map (node-based) |
|----------|-------------------------|--------------------------------|
| Reference stability after insert | ❌ (if rehash) | ✅ (always) |
| Iterator stability after insert | ❌ (if rehash) | ✅ (always) |
| Reference stability after erase | ❌ | ✅ |
| Iterator stability after erase | ✅ (erase returns next) | ✅ (only erased invalidated) |
| Memory locality | ✅ Excellent | ❌ Poor (linked list) |

> **Rule of thumb**: If you need stable references/iterators across insertions, use `std::unordered_map`. If you need performance and can manage invalidation, use emhash.

## 9. Known Issues

### Some emilib2 variants may hang under extreme hash collisions

Certain `emilib2` variants in `thirdparty/emilib/` (e.g., `emilib2ss`) do not limit probe-sequence length when **all keys hash to the same bucket**. Under this crafted hash-collision attack, insertion/lookup can degenerate into a full table scan and appear to hang.

> **Note**: The main `include/emilib/emihmap2.hpp` is `emilib2::HashMap` (namespace `emilib2`). Variants like `emilib2ss`, `emilib2o`, `emilib2s` are experimental forks located in `thirdparty/emilib/` and are not part of the standard include path.

**Affected scenario**:
- An adversary supplies keys that all produce the same hash value, or
- A pathological custom hash function maps many distinct keys to the same bucket.

**Mitigation**:
- Prefer `emilib2o` or `emilib2s` (from `thirdparty/emilib/`) for untrusted input.
- For adversarial workloads, use `emhash7` or enable `EMH_SAFE_PSL` on the emilib variants that support it.
- Do not expose `emilib2ss` to unvalidated user-provided keys in security-sensitive contexts.
