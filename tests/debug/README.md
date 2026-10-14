# Debug Tools

This directory contains debugging and reproduction tools for emhash. These are
**not** part of the CI test suite and are not built by CMakeLists.txt.

## Tracked files (committed to git)

| File | Purpose |
|------|---------|
| `debug_all_maps.cpp` | Comprehensive cross-implementation debug harness |

## Untracked files (local scratch)

One-off debugging files created during issue investigation. These are **not**
committed and should not be referenced by CMakeLists.txt. Historical examples
(resolved issues): `debug_merge*.cpp`, `debug_erase*.cpp`.

To build a debug file locally:

```bash
g++ -std=c++17 -O2 -I../include debug/debug_all_maps.cpp -o debug_all_maps
```

## Conventions

1. New debug files should be added to git **only** if they have long-term value.
2. One-off scratch files should be prefixed with `debug_` and left untracked.
3. Never add debug files to CMakeLists.txt — they will break CI.
