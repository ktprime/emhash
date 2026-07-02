# Debug Tools

This directory contains debugging and reproduction tools for emhash. These are
**not** part of the CI test suite and are not built by CMakeLists.txt.

## Tracked files (committed to git)

Long-lived debugging tools worth keeping:

| File | Purpose |
|------|---------|
| `debug_all_maps.cpp` | Comprehensive cross-implementation debug harness |
| `debug_chain.cpp` | Bucket chain traversal debugger |
| `debug_set_erase.cpp` | HashSet erase behavior tracer |
| `fuzz_reproduce.cpp` | Fuzzer failure reproducer |
| `min_repro.cpp` | Minimal crash reproducer |
| `reproduce_crash.cpp` | Crash reproduction tool |
| `test_debug_find.cpp` | find() behavior debugger |
| `test_probe_bug.cpp` | Probe sequence bug tracer |
| `test_probe_coverage.cpp` | Probe coverage analyzer |
| `test_repro_collision.cpp` | Collision reproduction |

## Untracked files (local scratch)

One-off debugging files created during issue investigation. These are **not**
committed and should not be referenced by CMakeLists.txt:

- `debug_merge*.cpp` — merge bug investigation (resolved)
- `debug_erase*.cpp` — erase iterator investigation (resolved)

To build any debug file locally:

```bash
g++ -std=c++17 -I../include debug/debug_all_maps.cpp -o debug_all_maps
```

## Conventions

1. New debug files should be added to git **only** if they have long-term value.
2. One-off scratch files should be prefixed with `debug_` and left untracked.
3. Never add debug files to CMakeLists.txt — they will break CI.
