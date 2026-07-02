## Summary

<!-- Brief description of what this PR changes and why. -->

## Pre-merge Checklist

Please confirm the following before requesting review:

### Build
- [ ] `cmake -B build -S tests && cmake --build build` succeeds locally
- [ ] No new compiler warnings on `-Wall -Wextra -pedantic` (GCC/Clang) or `/W4` (MSVC)

### Files & Dependencies
- [ ] All new `.cpp`/`.hpp` files referenced by CMakeLists.txt are `git add`-ed
- [ ] All scripts referenced by CI workflows (`.github/workflows/*.yml`) are committed
- [ ] No debug scratch files (`tests/debug/debug_*.cpp`) added to CMakeLists.txt

### Tests
- [ ] New code is covered by tests (unit/stress/attack as appropriate)
- [ ] `ctest -L emhash` passes locally
- [ ] ASan+UBSan: `g++ -fsanitize=address,undefined` build and run passes (if applicable)
- [ ] MSan: `clang++ -fsanitize=memory` build and run passes (if memory-sensitive changes)

### Code Quality
- [ ] `scripts/run_clang_tidy.sh --ci` introduces no new errors
- [ ] No C-style casts (use `static_cast`/`reinterpret_cast`)
- [ ] No `memset`/`memcpy` on non-trivially-copyable types without `is_trivially_copyable` guard

### Performance (if applicable)
- [ ] No regression beyond 25% threshold on benchmark
- [ ] `insert_unique` path does not perform redundant `find`/`contains` checks

## Test Plan

<!-- How did you verify this change? List specific test commands run. -->
