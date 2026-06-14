# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `VERSION` file at repo root for build scripts to read the current version
- `docs/adr/` directory with initial 4 ADRs (open addressing, emhash8 layout, no-tombstone emhash7, header-only)
- `docs/PERFORMANCE_TRACKING.md` for tracking benchmark results across versions
- `.github/workflows/format-check.yml` — clang-format LLVM style check on PRs
- `.github/workflows/warnings-check.yml` — strict warnings build matrix (gcc/clang × c++17/20)
- `.github/workflows/release.yml` — automatic GitHub Release creation on tag push
- `.github/workflows/ci.yml` — added `compat` job with multi-version compiler matrix (GCC 11-13, Clang 16-18, C++17/20)
- `.github/dependabot.yml` — monthly GitHub Actions dependency updates
- `.github/CODEOWNERS` — default code owner assignment
- `.github/stale.yml` — auto-close inactive issues after 90 days
- `.editorconfig` — consistent editor settings across IDEs
- CMake: version from `VERSION` file, `emhashConfigVersion.cmake`, complete header list
- `conanfile.py` — Conan package manager support
- `port/vcpkg/` — vcpkg port definition (ready for submission)
- `scripts/amalgamate.sh` — generate single-header distributions
- `docs/mkdocs.yml` — MkDocs Material configuration for documentation site
- Doxygen class-level documentation for `hash_table7.hpp` and `hash_table8.hpp`
- Cross-links from README to new docs/adr/ and PERFORMANCE_TRACKING.md
- `.github/workflows/deploy-docs.yml` — GitHub Pages auto-deploy for MkDocs documentation site
- `docs/faq.md` — Frequently asked questions
- `docs/migration_guide.md` — Migration guide from std::unordered_map
- `scripts/pre-commit.sh` — Git pre-commit hook for clang-format checking
- README: added CI status and version badges, FAQ and migration guide links
- `quick_bench.cpp` — zero-dependency quick benchmark (emhash7/8 vs std::unordered_map)
- `docs/performance_tips.md` — performance tuning guide (compile flags, pre-allocation, hash selection, anti-patterns)
- `docs/faq.md` — frequently asked questions
- `docs/migration_guide.md` — migration guide from std::unordered_map
- `examples/CMakeLists.txt` — CMake build for all examples + quick_bench
- Method-level Doxygen for hash_table8.hpp core API (contains, try_get, try_set, insert_unique, set_get, erase)

### Changed
- `dist/` added to `.gitignore` for amalgamated outputs

### Notes
- All additions are non-breaking; no source code or public API changes

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

[1.1.0]: https://github.com/ktprime/emhash/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/ktprime/emhash/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/ktprime/emhash/releases/tag/v1.0.0
