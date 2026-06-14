# Frequently Asked Questions

## General

### Which emhash version should I use?

| Scenario | Recommended Version |
|----------|-------------------|
| Complex/large keys or values (e.g., `std::string`) | **emhash8** |
| Insert-intensive workloads, high load factor | **emhash7** |
| Fast lookup/erase with integer keys | **emhash5** or **emhash6** |
| Small maps that should avoid heap allocation | **emhash5** with `EMH_SMALL_SIZE` |

### Is emhash thread-safe?

No. emhash is **not thread-safe** for concurrent writes. However, concurrent **read-only** access (e.g., `find()`, `contains()`, `count()`) from multiple threads is safe as long as no thread is modifying the map.

For concurrent writes, use external synchronization:
```cpp
std::mutex mtx;
{
    std::lock_guard<std::mutex> lock(mtx);
    map[key] = val;
}
```

### Can I use a higher load factor?

Yes. Define `EMH_HIGH_LOAD=<value>` at compile time (e.g., `-DEMH_HIGH_LOAD=123456`) to enable load factors up to **0.999**. emhash7 supports high load factors natively without this macro.

```cpp
emhash7::HashMap<int, int> map(1024, 0.999f);  // Works natively
emhash8::HashMap<int, int> map(1024, 0.999f);  // Requires -DEMH_HIGH_LOAD=123456
```

### Does emhash guarantee reference stability?

No. emhash uses open addressing with a single contiguous array. References, pointers, and iterators to elements may be invalidated by `insert()`, `erase()`, or `rehash()`.

**Workaround**: Copy values before modifying the map, or call `reserve()` to pre-allocate and avoid rehashing.

```cpp
// Safe: copy first
auto val = map[key1];
map[key2] = val;  // OK even if rehash occurs

// Unsafe: reference may dangle
auto& ref = map[key1];
map[key2] = ref;  // DANGER: rehash may invalidate ref
```

### How do I defend against hash attacks?

Define `EMH_SAFE_HASH=1` at compile time. This enables a backup hash function at approximately 10% performance cost.

For emilib implementations, use `EMH_SAFE_PSL=1` to limit probe sequence length.

---

## Performance

### Why is emhash faster than `std::unordered_map`?

1. **Open addressing** — single contiguous array, better cache locality
2. **No per-element heap allocation** — `std::unordered_map` allocates a node per element
3. **No tombstones** (emhash7) — no performance degradation from frequent erase
4. **Smart collision resolution** — hybrid probing strategies

### Why is emhash8 iteration so fast?

emhash8 uses a split layout: a separate dense `_pairs[]` array stores all key-value pairs contiguously. Iteration is just a sequential memory scan — no metadata to skip, no empty buckets to check.

### How much memory does emhash save?

When `sizeof(key) % 8 != sizeof(value) % 8`, emhash saves significant memory:

```cpp
// emhash7: compact layout
emhash7::HashMap<uint64_t, uint32_t>  // saves ~1/3 vs HashMap<uint64_t, uint64_t>

// std::unordered_map: always allocates full node
std::unordered_map<uint64_t, uint32_t>  // same node size as <uint64_t, uint64_t>
```

---

## Compatibility

### Which C++ standards are supported?

| Version | C++11 | C++14 | C++17 | C++20 |
|---------|-------|-------|-------|-------|
| emhash5 | Yes | Yes | Yes | Yes |
| emhash6 | Yes | Yes | Yes | Yes |
| emhash7 | Yes | Yes | Yes | Yes |
| emhash8 | Partial | Yes | Yes | Yes |

### Which compilers are supported?

- **GCC** 7+ (tested: 11, 12, 13)
- **Clang** 6+ (tested: 16, 17, 18)
- **MSVC** 2019+ (Visual Studio 16+)
- **MinGW GCC** 10+

### Can I use a custom allocator?

Yes. All `HashMap` versions support custom allocators as the 5th template parameter:

```cpp
emhash7::HashMap<Key, Val, Hash, Eq, MyAllocator> mymap;
```

The allocator must satisfy the C++ `Allocator` requirements and support `rebind`.
