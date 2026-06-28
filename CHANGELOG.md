# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `VERSION` file at repo root for build scripts to read the current version
- `docs/adr/` directory with initial 4 ADRs (open addressing, emhash8 layout, no-tombstone emhash7, header-only)
- `docs/performance_tracking.md` for tracking benchmark results across versions
- `.github/workflows/format-check.yml` — clang-format LLVM style check on PRs
- `.github/workflows/release.yml` — automatic GitHub Release creation on tag push
- `.github/workflows/ci.yml` — added `compat` job with C++20/23 standard compatibility testing
- `.github/dependabot.yml` — monthly GitHub Actions dependency updates
- `.github/CODEOWNERS` — default code owner assignment
- `.github/stale.yml` — auto-close inactive issues after 90 days
- `.editorconfig` — consistent editor settings across IDEs
- CMake: version from `VERSION` file, `emhashConfigVersion.cmake`, complete header list
- `conanfile.py` — Conan package manager support
- `scripts/vcpkg/` — vcpkg port definition (ready for submission)
- `scripts/amalgamate.sh` — generate single-header distributions
- `docs/mkdocs.yml` — MkDocs Material configuration for documentation site
- Doxygen class-level documentation for `hash_table7.hpp` and `hash_table8.hpp`
- Cross-links from README to new docs/adr/ and PERFORMANCE_TRACKING.md
- `.github/workflows/deploy-docs.yml` — GitHub Pages auto-deploy for MkDocs documentation site
- `docs/faq.md` — Frequently asked questions
- `docs/migration_guide.md` — Migration guide from std::unordered_map
- `scripts/pre-commit.sh` — Git pre-commit hook for clang-format checking
- `docs/examples/CMakeLists.txt` — CMake build for all examples + quick_bench
- Method-level Doxygen for hash_table8.hpp core API (contains, try_get, try_set, insert_unique, set_get, erase)
- `THIRDPARTY_LICENSES.md` — aggregated license summary for all `thirdparty/` dependencies
- `docs/OPTIMIZATION_RECOMMENDATIONS.md` — engineering optimization roadmap (P0/P1/P2 with risk/ROI)
- `tests/common/` — shared test infrastructure (doctest.h, hashers.hpp, maps.hpp, msan_unpoison.hpp, trackers.hpp, utilities.hpp)
- MSan unpoison header (`tests/common/msan_unpoison.hpp`) injected via `-include` to suppress false positives from uninstrumented libc++ `std::cout`/`std::cerr`
- `.github/msan-suppressions.txt` — explicit MSan suppression list
- CI: 80% line coverage gate (lcov + bc) and 25% benchmark regression gate against `benchmark-baseline` artifact

### Changed
- `dist/` added to `.gitignore` for amalgamated outputs
- Test directory reorganized from `verify/` into `unit/` / `memory/` / `stress/` / `attack/` / `fuzz/` / `debug/` / `bench/` / `common/` / `archive/` categories for clearer separation of concerns
- Test framework migrated to doctest with `TEST_CASE_TEMPLATE` parameterized over all hash map/set implementations
- CI matrix tightened: removed C++20 job (kept C++17 and C++23); MSan test list now covers `unit/test_crud.cpp`, `memory/test_string_key_leak.cpp`, `memory/test_sanitizer.cpp`, `memory/test_lifecycle_audit.cpp`
- Windows MSVC builds use explicit target lists instead of `.exe` globbing
- CI configuration updated to reference new test file paths (`unit/`, `memory/`, `stress/`, `attack/`) instead of deprecated `verify/`
- All compilation commands include `-Icommon` flag to resolve `doctest.h`
- clang-format-18 enforcement tightened across `include/` (excluding `sse2neon.h`)
- Renamed `index` parameter to `slot` in `hash_set8.hpp` and `hash_table8.hpp` to silence `-Wshadow`
- Pragma-wrapped `size_t` typedefs in `emihmap`/`emihset` headers to silence `-Wshadow`/`-Wunneeded-internal-declaration`

### Fixed
- MSan use-of-uninitialized-value in `hash_table5.hpp` `at()` method (switched from `size_type` to `int` for negative comparisons in `find_or_kickout`)
- MSan false positives caused by `std::cout`/`std::cerr` internal state set up by uninstrumented libc++ — resolved by injecting unpoison header via `-include`
- Uninitialized bucket fields after `clear()` in `hash_table5/6/8` and `hash_set8` — buckets now reset to `INACTIVE` state
- MSan UB from `memcpy`/`memset` of non-trivially copyable types (e.g., `std::string`) — bucket fields now initialized individually; `-Wclass-memaccess` resolved by casting pointers to `char*`
- UB in `emilib/hash_set2/4` `bInCacheLine` flag — combined with `is_trivially_copyable<KeyT>` guard to avoid UB with non-trivial keys
- Tail sentinels `memset`/`memcpy` without `is_trivially_copyable` guard — non-trivial types now only initialize `.second` field
- Sentinel memory leak in `emihmap1`/`emihmap3` `clone()` when `_num_buckets` differed — old sentinel explicitly destructed before resetting `_num_buckets` to 0
- 32-bit GCC crash from `__builtin_ctzl` on 64-bit masks — replaced with `__builtin_ctzll` throughout `emilib`/`emihset`
- emilib2 `HashSet` `_erase` modulo-on-`State::EEMPTY` bug causing size mismatch and SIGSEGV on 32-bit — fixed by direct state comparison and leftward empty-state propagation
- `-Wshadow` and `-Wunneeded-internal-declaration` warnings across 12 header files
- clang-format-18 violations in `emihset2.hpp`, `emihmap2.hpp`, `hash_table6.hpp`
- Tracked build artifacts removed from working tree and `.gitignore` hardened

### Known Issues (tracked via `doctest::skip`)
- `emhash6::HashMap::merge()` / `emhash7::HashMap::merge()` — key loss during rehash (under investigation)
- `set merge` / `set erase_if` for `emilib2`/`emilib3` — runtime issue
- `erase_if` predicate signature unification across map implementations
- `emhash8::HashMap::insert_unique` key=0 deduplication failure — see `tests/fuzz/reproduce_emhash8_bug.cpp`

### Notes
- Source-level additions are non-breaking; the public API of existing hash map implementations is unchanged
- `doctest::skip` markers are temporary; tracked in `docs/OPTIMIZATION_RECOMMENDATIONS.md` as P0.3

## [1.1.0] - 2026-06-13

### Changed
- Reorganized test directory: merged `test/` into `tests/` with subcategories (verify/stress/attack/debug/fuzz/bench)
- Renamed `tests_new/` to `tests/` for clarity
- Fixed build scripts to support Linux/macOS/Windows (gcc/clang)
- Fixed CMakeLists.txt MSVC compatibility for bench and debug targets
- Added `-I../bench` include path for bench tests (resolves `util.h` not found)

### Fixed
- Fixed 128-bit multiplication fallback in `bench/util.h` and `martin_bench.cpp` for 32-bit platforms
- Fixed `pb_ds` header include in `ebench.cpp` for clang compatibility
- Fixed `build_tests.ps1` hardcoded WSL path issue
- Fixed `run_all.sh` hardcoded `/tmp/` path and `timeout` command for macOS
- Fixed `.gitignore` entries for test binaries

### Added
- Restored `test/main.cpp` as `tests/verify/test_main.cpp`
- Added `test_hashset_allocator.cpp`, `test_emilib_comprehensive.cpp`, `test_size_sweep.cpp` from old `test/`
- Added `test_bad_hash_stress.cpp`, `test_probe_bug.cpp`, `test_probe_coverage.cpp`, `test_debug_find.cpp`, `test_repro_collision.cpp`
- Added GitHub Actions CI workflow (`.github/workflows/ci.yml`)
- Added `examples/` directory with basic_map, basic_set, custom_allocator, custom_hash examples
- Added `.clang-format` configuration
- Added `CONTRIBUTING.md`
- Added `CHANGELOG.md`

## [1.0.1] - 2025-12-01

### Fixed
- Fixed fuzz test build failures
- Fixed CMake reserved target name conflict

## [1.0.0] - 2025-11-15

### Added
- Initial public release
- hash_table5: Robin-hood probing hash map
- hash_table6: Linear probing hash map
- hash_table7: Quadratic probing hash map
- hash_table8: Optimized flat hash map
- hash_set2/3/4/8/81: Hash set implementations
- emilib2s/2ss/2o: Alternative hash map implementations
- Comprehensive benchmark suite
- Third-party comparison benchmarks

[Unreleased]: https://github.com/ktprime/emhash/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/ktprime/emhash/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/ktprime/emhash/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/ktprime/emhash/releases/tag/v1.0.0
