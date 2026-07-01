# Migrating from std::unordered_map

This guide helps you migrate from `std::unordered_map` to emhash with minimal code changes.

## Quick Migration (Drop-In Replacement)

For most cases, just change the type and add the include:

```cpp
// Before
#include <unordered_map>
std::unordered_map<int, std::string> map;

// After
#include "emhash/hash_table7.hpp"
emhash7::HashMap<int, std::string> map;
```

The API is largely compatible with `std::unordered_map`.

## API Compatibility

### Fully Compatible (Same Signature)

| Method | Notes |
|--------|-------|
| `operator[]` | Identical behavior |
| `at()` | Identical behavior (throws `std::out_of_range` on missing key, same as std::unordered_map) |
| `insert()` | Identical behavior |
| `emplace()` | Identical behavior |
| `erase(key)` | Identical behavior |
| `erase(it)` | Identical behavior |
| `find()` | Identical behavior |
| `contains()` | Standard C++20 method, emhash also provides it under C++17 |
| `count()` | Identical behavior |
| `size()` / `empty()` | Identical behavior |
| `clear()` | Identical behavior |
| `reserve()` | Identical behavior |
| `bucket_count()` | Identical behavior |
| `load_factor()` | Identical behavior |
| `begin()` / `end()` | Identical behavior |
| `key_eq()` | Identical behavior |
| `hash_function()` | Identical behavior |

### Behavioral Differences

| Feature | `std::unordered_map` | emhash |
|---------|---------------------|--------|
| **Reference stability** | Guaranteed (node-based) | **Not guaranteed** (open addressing) |
| **Iterator invalidation** | Only on erase of that element | On any insert/erase/rehash |
| **`at()` on missing key** | Throws `std::out_of_range` | Throws `std::out_of_range` (same as std) |
| **`max_load_factor()`** | Can be set freely | Settable at runtime (0.80 default, up to 0.999 with `EMH_HIGH_LOAD`) |
| **`bucket()` / `bucket_size()`** | Available | Available only with `EMH_STATIS` compile flag |
| **`equal_range()`** | Available | Available (all versions) |
| **`merge()`** | Available (C++17) | Available (all versions) |
| **Node handle** | Available (C++17) | Not available |

### emhash-Only Methods (Not in std::unordered_map)

| Method | Description |
|--------|-------------|
| `insert_unique(key, val)` | Direct insert without lookup — faster when key is guaranteed unique |
| `try_get(key)` | Returns `ValueT*` (`nullptr` if not found) — avoids exception overhead |
| `try_set(key, val)` | Set value if key exists, do nothing if it doesn't. Returns `bool`. (emhash5/8 only) |
| `set_get(key, val)` | Sets new value, returns old value (emhash5/8). Different signature in emilib: `set_get(key, val, oldv)` returns `bool` |
| `_erase(it)` | Erase by iterator returning void — slightly faster than `erase()` (emhash5/6/7 and emilib1/2/3) |
| `shrink_to_fit()` | Releases unused memory |

## Common Migration Patterns

### Pattern 1: Safe Access

```cpp
// std::unordered_map: at() throws on missing key
try {
    auto val = map.at(key);
} catch (const std::out_of_range&) {
    // handle missing key
}

// emhash: use find() or try_get()
if (auto it = map.find(key); it != map.end()) {
    auto val = it->second;
}
// Or:
if (auto* pval = map.try_get(key)) {
    auto val = *pval;
}
```

### Pattern 2: Reference Stability

```cpp
// std::unordered_map: reference stays valid
auto& ref = map[1];
map[2] = "new";  // ref is still valid
ref = "updated";

// emhash: copy first, or pre-reserve
auto val = map[1];       // copy, don't reference
map.reserve(10000);      // pre-allocate to avoid rehash
map[2] = "new";
map[1] = val;            // safe
```

### Pattern 3: Erase During Iteration

```cpp
// Both: same pattern works
for (auto it = map.begin(); it != map.end(); ) {
    if (should_erase(it->first)) {
        it = map.erase(it);
    } else {
        ++it;
    }
}
```

### Pattern 4: Custom Types

```cpp
// Both: same custom hash/equality pattern
struct Point {
    int x, y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

struct PointHash {
    size_t operator()(const Point& p) const {
        return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
    }
};

// std::unordered_map
std::unordered_map<Point, int, PointHash> map1;

// emhash
emhash7::HashMap<Point, int, PointHash> map2;
```

## Performance Expectations

Typical speedup when migrating from `std::unordered_map`:

| Operation | Expected Speedup |
|-----------|-----------------|
| Insert | 3-5x faster |
| Find (hit) | 1.5-2x faster |
| Find (miss) | 2-3x faster |
| Erase | 5-10x faster |
| Iterate | 10-80x faster |
| Memory usage | 30-50% less |

## What NOT to Do

1. **Don't store references/pointers to elements** — they may be invalidated
2. **Don't use `at()` for hot-path lookups** — emhash's `at()` throws `std::out_of_range` (same as std) which incurs exception overhead; prefer `try_get()` for performance-critical code
3. **Don't use `bucket()` / `bucket_size()`** — not available in open addressing
4. **Don't assume iterator stability** — any modification may invalidate all iterators

---

## Upgrading from emhash < 1.1.0

If you are upgrading from an older version where headers were in the repository root:

### Include path changes

```cpp
// Old (before v1.1.0)
#include "hash_table7.hpp"

// New (v1.1.0+)
#include "emhash/hash_table7.hpp"
```

### Compiler flags

```bash
# Old
g++ -I/path/to/emhash ...

# New
g++ -I/path/to/emhash/include ...
```

### CMake

No changes needed if using `find_package(emhash)`. The CMake config automatically provides the correct include paths.

### Header files moved

| Old location | New location |
|---|---|
| `hash_table5.hpp` | `include/emhash/hash_table5.hpp` |
| `hash_table6.hpp` | `include/emhash/hash_table6.hpp` |
| `hash_table7.hpp` | `include/emhash/hash_table7.hpp` |
| `hash_table8.hpp` | `include/emhash/hash_table8.hpp` |
| `hash_set2.hpp` | `include/emhash/hash_set2.hpp` |
| `hash_set3.hpp` | `include/emhash/hash_set3.hpp` |
| `hash_set4.hpp` | `include/emhash/hash_set4.hpp` |
| `hash_set8.hpp` | `include/emhash/hash_set8.hpp` |
