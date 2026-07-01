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
#include "emhash/hash_table7.hpp"

emhash7::HashMap<std::string, int> scores;
scores["alice"] = 95;
scores["bob"] = 87;

if (auto it = scores.find("alice"); it != scores.end()) {
    std::cout << it->first << ": " << it->second << "\n";
}
```

## Available Implementations

### HashMap

| Header | Container | Best For |
|--------|-----------|----------|
| `emhash/hash_table5.hpp` | `emhash5::HashMap` | Fast lookup/erase with integer keys |
| `emhash/hash_table6.hpp` | `emhash6::HashMap` | Integer keys, fast empty scan |
| `emhash/hash_table7.hpp` | `emhash7::HashMap` | High load factor, insert-intensive |
| `emhash/hash_table8.hpp` | `emhash8::HashMap` | Complex keys/values, fast iteration |
| `emilib/emihmap1.hpp` | `emilib::HashMap` | SIMD-accelerated, inline probe depth |
| `emilib/emihmap2.hpp` | `emilib2::HashMap` | SIMD-accelerated, high load factor |
| `emilib/emihmap3.hpp` | `emilib3::HashMap` | SIMD-accelerated, balanced default |

### HashSet

> **Note**: HashSet uses independent version numbering from HashMap. `hash_set2/3/4` are not derived from `hash_table2/3/4`.

| Header | Container | Best For |
|--------|-----------|----------|
| `emhash/hash_set2.hpp` | `emhash2::HashSet` | Basic hash set |
| `emhash/hash_set3.hpp` | `emhash3::HashSet` | Alternative probing strategy |
| `emhash/hash_set4.hpp` | `emhash4::HashSet` | Enhanced version |
| `emhash/hash_set8.hpp` | `emhash8::HashSet` | Fast iteration, complex key support |

## Documentation

- [API Reference](api.md)
- [Design Principles](design.md)
- [Performance](performance.md)
- [Performance Tips](performance_tips.md)
- [Usage Notes](usage_notes.md)
- [FAQ](faq.md)
- [Migration Guide](migration_guide.md)
- [Test Analysis & Coverage Report](test_analysis.md)
- [Examples](examples/README.md)
