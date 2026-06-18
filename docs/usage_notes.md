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
for (const auto& it : myhash) {
    if (condition) myhash.erase(it.first);
}

// ✅ Correct: use iterator erase and update
for (auto it = myhash.begin(); it != myhash.end(); ) {
    if (condition) {
        it = myhash.erase(it);  // Returns next valid iterator
    } else {
        ++it;
    }
}

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

emhash does not throw exceptions from hash table operations. However, if the key or value type's constructor/copy/move throws during insert, the table remains in a valid state (basic guarantee), but the specific element may not be inserted.

```cpp
// If ValueT's constructor throws, the map is still valid
// but the element was not inserted
try {
    map.emplace(key, value_that_may_throw);
} catch (...) {
    // map is still valid, element was not added
}
```
