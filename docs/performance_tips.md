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

## Compile-Time Macros

### 19. High Load Factor (`EMH_HIGH_LOAD`)

Enables load factors up to **0.999** in emhash5/8 (emhash6/7 support it natively via `max_load_factor()`).

```bash
# Enable high load factor mode (value controls the rehash threshold)
g++ -DEMH_HIGH_LOAD=123456 -O3 -std=c++17 your_app.cpp
```

```cpp
emhash5::HashMap<int, int> map(1024, 0.999f);  // Requires -DEMH_HIGH_LOAD
emhash7::HashMap<int, int> map(1024, 0.999f);  // Works natively
```

> **Trade-off**: Higher load factor saves memory but slightly increases probe length on lookup.

### 20. Small-Size Optimization (`EMH_SMALL_SIZE`)

emhash5 can avoid heap allocation for very small maps by using a stack-allocated buffer.

```bash
# Use stack buffer for maps with ≤ N buckets (default: off)
g++ -DEMH_SMALL_SIZE=16 -O3 -std=c++17 your_app.cpp
```

Best for maps that are usually empty or contain only a few elements.

### 21. Integer Hash Mixing (`EMH_INT_HASH`)

By default, `std::hash<int>` is the identity function, which can cause clustering with sequential keys (0, 1, 2, ...) in power-of-2 bucket tables. `EMH_INT_HASH` enables golden-ratio bit mixing.

```bash
# 1 = golden-ratio mixing (recommended for sequential keys)
# 2 = murmur-style mixing
# 3 = splitmix64 mixing
g++ -DEMH_INT_HASH=1 -O3 -std=c++17 your_app.cpp
```

### 22. Bucket Index Layout (`EMH_BUCKET_INDEX`)

Controls the memory layout of the `entry` struct in emhash5/6 (affects cache behavior):

| Value | Layout | Best For |
|-------|--------|----------|
| `0` (default) | `{bucket, {key, value}}` | General use |
| `1` | `{value, bucket, key}` | Key-heavy workloads (default for emhash6) |
| `2` | `{{key, value}, bucket}` | Value-heavy workloads |

```bash
g++ -DEMH_BUCKET_INDEX=2 -O3 -std=c++17 your_app.cpp
```

### 23. Size Type Width (`EMH_SIZE_TYPE_16BIT` / `EMH_SIZE_TYPE_64BIT`)

Controls the `size_type` used for bucket indices:

| Macro | `size_type` | Max Buckets | Memory Savings |
|-------|-------------|-------------|----------------|
| (default) | `uint32_t` | ~4 billion | — |
| `EMH_SIZE_TYPE_16BIT` | `uint16_t` | ~65K | 2 bytes/entry |
| `EMH_SIZE_TYPE_64BIT` | `uint64_t` | ~18 quintillion | -4 bytes/entry |

```bash
# For small maps (< 65K elements), use 16-bit indices to save memory
g++ -DEMH_SIZE_TYPE_16BIT -O3 -std=c++17 your_app.cpp
```

### 24. LRU Mode (`EMH_LRU_SET`)

Enables LRU-style access ordering on emhash5/7/8 hash sets.

```bash
g++ -DEMH_LRU_SET=1 -O3 -std=c++17 your_app.cpp
```

See also the dedicated LRU implementations: `emhash/lru_size.hpp` and `emhash/lru_time.hpp`.

### 25. SIMD Level (`EMH_SIMD_LEVEL`)

Controls the SIMD instruction set used by emilib (CMake option):

```bash
# Via CMake
cmake -DEMH_SIMD_LEVEL=AVX2 ...

# Via compiler flag (non-CMake build)
g++ -mavx2 -O3 -std=c++17 your_app.cpp
```

| Level | emilib Support |
|-------|---------------|
| `SSE2` (default) | All emilib versions |
| `AVX2` | emilib2/3 wider SIMD |
| `NONE` | Disables SIMD intrinsics (portability fallback) |

## Profile-Guided Optimization (PGO)

PGO uses runtime profiling data to guide the compiler's inlining, branch prediction, and code layout decisions. For template-heavy libraries like emhash, PGO can yield **5–15%** additional speedup on top of `-O2`/`-O3`.

### GCC PGO Workflow

```bash
# Step 1: Instrumented build
g++ -O2 -fprofile-generate=./pgo_data -std=c++17 your_app.cpp

# Step 2: Run representative workload (the more realistic, the better)
./a.out  # feeds profile data into ./pgo_data/

# Step 3: Optimized build using profile data
g++ -O2 -fprofile-use=./pgo_data -std=c++17 your_app.cpp
```

### Clang PGO Workflow

```bash
# Step 1: Instrumented build
clang++ -O2 -fprofile-instr-generate=./pgo_data/default.profraw -std=c++17 your_app.cpp

# Step 2: Run representative workload
./a.out  # produces default.profraw

# Step 3: Merge raw profile data
llvm-profdata merge ./pgo_data/default.profraw -o ./pgo_data/default.profdata

# Step 4: Optimized build using profile data
clang++ -O2 -fprofile-instr-use=./pgo_data/default.profdata -std=c++17 your_app.cpp
```

### MSVC PGO Workflow

```bash
# Step 1: Instrumented build
cl /O2 /GL /c your_app.cpp /DPROFILE_INSTRUMENTATION
link /LTCG /GENPROFILE your_app.obj

# Step 2: Run representative workload
your_app.exe  # produces .pgc file

# Step 3: Optimized build using profile data
link /LTCG /USEPROFILE your_app.obj
```

> **Tips**: Use a realistic workload that covers your typical insert/find/erase mix. A workload that only does inserts won't help the optimizer for find/erase paths.

## Link-Time Optimization (LTO)

LTO enables cross-module inlining and dead code elimination. For header-only libraries, LTO ensures the compiler can see and optimize the full call chain.

```bash
# GCC (auto-detects parallelism)
g++ -O2 -flto=auto -std=c++17 your_app.cpp

# Clang
clang++ -O2 -flto=thin -std=c++17 your_app.cpp

# MSVC (whole-program optimization)
cl /O2 /GL your_app.cpp
link /LTCG your_app.obj
```

> **Combining PGO + LTO**: For maximum performance, use both together. PGO provides profile data while LTO enables whole-program visibility. Typical combined gain: 10–20% over plain `-O2`.

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
