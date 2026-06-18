# emhash Examples

This directory contains example programs demonstrating how to use the emhash hash map and hash set libraries.

## Files

| File | Description |
|------|-------------|
| `basic_map.cpp` | Basic HashMap usage: insert, find, erase, iterate |
| `basic_set.cpp` | Basic HashSet usage: insert, contains, erase |
| `custom_allocator.cpp` | Using a custom allocator with emhash containers |
| `custom_hash.cpp` | Using a custom hash function for user-defined key types |

## Build

```bash
# From project root
cmake -B build -DWITH_EXAMPLES=ON
cmake --build build

# Or compile manually
g++ -std=c++17 -I../include basic_map.cpp -o basic_map
```

## Available Containers

| Header | Container | Description |
|--------|-----------|-------------|
| `emhash/hash_table5.hpp` | `emhash5::HashMap<K,V>` | Linear probing |
| `emhash/hash_table6.hpp` | `emhash6::HashMap<K,V>` | Quadratic probing |
| `emhash/hash_table7.hpp` | `emhash7::HashMap<K,V>` | No-tombstone design |
| `emhash/hash_table8.hpp` | `emhash8::HashMap<K,V>` | Latest version, recommended |
| `emhash/hash_set8.hpp` | `emhash8::HashSet<K>` | HashSet (latest) |
| `emilib/emihmap1.hpp` | `emilib::HashMap<K,V>` | SIMD-accelerated, inline probe depth |
| `emilib/emihmap2.hpp` | `emilib2::HashMap<K,V>` | SIMD-accelerated, high load factor |
| `emilib/emihmap3.hpp` | `emilib3::HashMap<K,V>` | SIMD-accelerated, balanced default |
| `emhash/lru_size.hpp` | `emhash::lru_size_cache<K,V>` | LRU cache (size-based) |
| `emhash/lru_time.hpp` | `emhash::lru_time_cache<K,V>` | LRU cache (time-based) |
