# emhash

A high-performance, header-only C++ hash table library with multiple implementations optimized for different workloads.

## Features

- **Header-only**: Single `#include`, no compilation needed
- **Multiple implementations**: 4 emhash versions + 3 emilib SIMD-accelerated versions
- **Extremely fast**: 2-3x faster than `std::unordered_map` for most operations
- **High load factor**: emhash7 supports load factors up to 0.999
- **SIMD-accelerated**: emilib versions use SSE2/AVX2/AVX-512 for vectorized lookups
- **Custom allocators**: All versions support custom allocators
- **C++17 compatible**: Requires C++17 or later

## Quick Start

```cpp
#include <emhash/hash_table7.hpp>

emhash7::HashMap<std::string, int> scores;
scores["alice"] = 95;
scores["bob"] = 87;

if (auto it = scores.find("alice"); it != scores.end()) {
    std::cout << it->first << ": " << it->second << "\n";
}
```

## Available Implementations

| Header | Container | Best For |
|--------|-----------|----------|
| `emhash/hash_table5.hpp` | `emhash5::HashMap` | Fast lookup/erase with integer keys |
| `emhash/hash_table6.hpp` | `emhash6::HashMap` | Integer keys, fast empty scan |
| `emhash/hash_table7.hpp` | `emhash7::HashMap` | High load factor, insert-intensive |
| `emhash/hash_table8.hpp` | `emhash8::HashMap` | Complex keys/values, fast iteration |
| `emhash/hash_set2.hpp` | `emhash2::HashSet` | HashSet (based on emhash2) |
| `emhash/hash_set8.hpp` | `emhash8::HashSet` | HashSet (based on emhash8) |
| `emilib/emihmap1.hpp` | `emilib::HashMap` | SIMD-accelerated, inline probe depth |
| `emilib/emihmap2.hpp` | `emilib2::HashMap` | SIMD-accelerated, high load factor |
| `emilib/emihmap3.hpp` | `emilib3::HashMap` | SIMD-accelerated, balanced default |

## Documentation

- [API Reference](api.md)
- [Design Principles](design.md)
- [Performance](performance.md)
- [Performance Tips](performance_tips.md)
- [Usage Notes](usage_notes.md)
- [FAQ](faq.md)
- [Migration Guide](migration_guide.md)
- [Examples](examples/README.md)
