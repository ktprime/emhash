# Performance Tips

This guide covers techniques to get the best performance from emhash.

## Compile-Time Optimization

### 1. Enable Compiler Optimizations

```bash
# Release build (critical for performance)
g++ -O3 -march=native -std=c++17 your_app.cpp

# With LTO for cross-module optimization
g++ -O3 -march=native -flto -std=c++17 your_app.cpp
```

> **Never benchmark with `-O0` or `-O1`** — emhash relies heavily on inlining.

### 2. Choose the Right Standard

```bash
# C++17: best balance of features and compiler support
g++ -std=c++17 ...

# C++20: enables structured bindings with lambda hash/eq (minor benefit)
g++ -std=c++20 ...
```

## Pre-Allocation

### 3. Always `reserve()` When Size Is Known

```cpp
// Bad: multiple rehashes as elements are added
emhash7::HashMap<int, int> map;
for (int i = 0; i < 1000000; i++)
    map[i] = i;  // triggers ~20 rehashes

// Good: single allocation
emhash7::HashMap<int, int> map;
map.reserve(1000000);
for (int i = 0; i < 1000000; i++)
    map[i] = i;  // no rehash
```

### 4. Reserve Slightly More for Mixed Workloads

```cpp
// If you'll insert and erase frequently, reserve 20-30% extra
map.reserve(expected_size * 1.25);
```

## Insert Optimization

### 5. Use `insert_unique()` When Keys Are Unique

```cpp
// Slow: checks if key exists first
map.insert({key, val});

// Fast: skips existence check (key MUST be unique)
map.insert_unique(key, val);  // 20-40% faster for new keys
```

### 6. Use `operator[]` for Overwrite Semantics

```cpp
// operator[] is faster than insert() when overwriting
map[key] = new_val;  // Fast path: key exists, just overwrite
```

## Lookup Optimization

### 7. Use `try_get()` Instead of `find()` + Check

```cpp
// Verbose
auto it = map.find(key);
if (it != map.end()) {
    use(it->second);
}

// Faster and cleaner
if (auto* pval = map.try_get(key)) {
    use(*pval);
}
```

### 8. Use `contains()` for Existence Checks

```cpp
// Slower: constructs a value_type
map.count(key) > 0;

// Faster: no value construction
map.contains(key);
```

## Hash Function Selection

### 9. Integer Keys: Consider Bit-Mixing Hash

```cpp
// Default std::hash<int> is identity — can cause clustering with power-of-2 buckets
// for arithmetic sequence keys (0, 1024, 2048, ...). Consider enabling EMH_INT_HASH:
emhash7::HashMap<int, int> map;  // OK for random keys

// For sequential/arithmetic keys, compile with -DEMH_INT_HASH=1 for golden-ratio mixing
```

### 10. String Keys: Consider wyhash

```bash
# Compile with wyhash for faster string hashing
g++ -DEMH_WY_HASH=1 -O3 -std=c++17 your_app.cpp
```

### 11. Custom Hash: Avoid Poor Hash Functions

```cpp
// Bad: high collision rate for sequential keys
struct BadHash {
    size_t operator()(int x) const { return x % 1000; }
};

// Good: use bit mixing
struct GoodHash {
    size_t operator()(int x) const {
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        return (x >> 16) ^ x;
    }
};
```

## Version Selection for Performance

### 12. Choose Based on Your Bottleneck

| Bottleneck | Best Version | Why |
|------------|-------------|-----|
| Insert-heavy | emhash7 | No tombstones, stable insert performance |
| Lookup-heavy (integer keys) | emhash5/6 | Lowest probe count |
| Iteration-heavy | emhash8 | Dense pairs array, sequential scan |
| Mixed insert/erase | emhash7 | No tombstone accumulation |
| Large key/value types | emhash8 | Dense pairs, no metadata interleaving |

## Memory Optimization

### 13. Compact Layout Saves Memory

```cpp
// emhash packs key+value tightly when sizes differ
emhash7::HashMap<uint64_t, uint32_t> map;  // Saves ~1/3 vs <uint64_t, uint64_t>
```

### 14. Use `shrink_to_fit()` After Bulk Erase

```cpp
for (auto& k : keys_to_remove)
    map.erase(k);
map.shrink_to_fit();  // Release unused memory
```

### 15. High Load Factor Saves Memory

```bash
# Trade: slightly slower lookup for less memory
g++ -DEMH_HIGH_LOAD=123456 -O3 -std=c++17 your_app.cpp
```

```cpp
emhash7::HashMap<int, int> map(1024, 0.95f);  // 95% load factor
```

## Anti-Patterns to Avoid

### 16. Don't Rehash in Hot Loops

```cpp
// Bad: repeated small inserts cause rehashing
for (auto& [k, v] : data)
    map[k] = v;

// Good: reserve first
map.reserve(data.size());
for (auto& [k, v] : data)
    map[k] = v;
```

### 17. Don't Use `at()` for Lookup

```cpp
// Slow: exception overhead
auto val = map.at(key);

// Fast: no exception
auto it = map.find(key);
if (it != map.end()) val = it->second;

// Fastest: pointer return
if (auto* p = map.try_get(key)) val = *p;
```

### 18. Don't Copy Maps Unnecessarily

```cpp
// Bad: deep copy
auto copy = original_map;

// Good: move
auto moved = std::move(original_map);

// Good: const reference
void process(const emhash7::HashMap<int, int>& map);
```
