# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- Removed the `EMH_EXT` compile gate: `try_get`/`try_set` that were hidden inside `#ifdef EMH_EXT` blocks in `hash_table5/6/7.hpp` are now unconditionally compiled (zero runtime cost — plain inline non-virtual functions). `try_get` becomes uniform across all 8 map implementations (emhash5-7 expose both the `bool try_get(key, out&)` and `ValueT* try_get(key)` shapes); `try_set` becomes available on emhash5 in addition to emhash8 and emilib1-3
- Removed `get_or_return_default()` from `hash_table5/6/7/8.hpp` (legacy `EMH_EXT`-era convenience API, superseded by `try_get`/`at`); the dedicated LRU containers keep their own copies. README heterogeneous-lookup row corrected: `find`/`contains`/`count`/`at`/pointer-form `try_get` are open templates on every implementation and work heterogeneously with a transparent hash & equality (verified for emilib4); only heterogeneous `erase`/value-form `try_get` remain emhash8-exclusive
- API audit cleanups (2026-10): (1) removed emhash8's ambiguous pointer-form `try_get(key, uint64_t key_hash)` — for integral value types it made `try_get(key, val)` an ambiguous call (compile error in multi-header builds, silently wrong overload elsewhere); the 3-arg `try_get(key, out, hash(key))` and the plain forms remain. (2) emilib1-3 maps and emilib2/3 sets: the raw-bucket `_erase(size_t)` is now private (a bare int argument silently converted to a bucket index and erased the wrong element); the public fast-erase surface is `_erase(const_iterator)`/`_erase(iterator)`. (3) emilib range `insert(first, last)` and emilib3's range constructor are SFINAE-constrained to real iterators, so `insert(3, 4)` is now a compile error instead of int-as-iterator UB. (4) Verified `merge` (emhash5-8, emilib1-3) and set `erase_if`/`merge` against `std::unordered_map`/`std::unordered_set` ground truth (300 randomized rounds + 5000-element rehash-heavy runs) — all correct; the previously documented key-loss/runtime-issue footnotes were removed from the README matrices

### Added
- Range constructor `(first, last)` for `emilib2::HashSet` (emilib3 already had one); emilib sets' range constructors are constrained to real iterators
- API-parity backfill across implementations (no behavioral cost): `hash_function()`/`key_eq()` accessors on emilib1-4 maps and emilib2/3 sets; `emplace_hint` (hint accepted and ignored) on emilib1-4 maps and emilib2/3 sets; `insert_unique(value_type)` overload on emilib1-4 maps; void `_erase` fast-erase on `emhash8::HashMap`/`emhash8::HashSet`/emilib4; single-element `erase(it)` now returns the iterator to the next element on emilib1-4 maps and emilib3 set (advancing before erasing, the same code path as a normal `++` walk)
- `iterator` -> `const_iterator` conversion on emilib1/2/3 maps and emilib3 set is now implicit (required by the standard container convention; it was `explicit`)
- Custom allocator support for `emilib1-4` maps (`emihmap1-4.hpp`): trailing `AllocT` template parameter (default `std::allocator<std::pair<KeyT, ValueT>>`), allocator-extended constructors (`(alloc)`, `(n, alloc)`, `(n, lf, alloc)` where applicable, `(map, alloc)` copy/move forms), `get_allocator()`, `select_on_container_copy_construction` and `propagate_on_container_copy_assignment` handling. Raw blocks are routed through the rebound allocator: emihmap1 splits pairs/states, emihmap2/4 keep the single pairs-first block as `PairT` elements, and emihmap3 allocates via a `max_align_t` rebound so its SIMD-aligned state bytes keep the 16-byte alignment malloc previously guaranteed

### Removed
- Obsolete `include/emilib/emihmap1_opt.hpp` (namespace `emilib1_opt`, 1094 lines) and its manual benchmark `bench/bench_emihmap1_opt.cpp`: an undocumented experimental emihmap1 variant, registered in no CMake target and superseded by the allocator-aware `emihmap1.hpp`

### Fixed
- `emihmap4.hpp`: `unchecked_emplace_with_rehash` and `clone` could `free(nullptr)`-style deallocate through the new allocator path without a null guard (harmless with `free`, UB with `allocator_traits::deallocate`)

## [1.3.0] - 2026-10-15

### Added
- Compile-time API contract test (`tests/unit/test_api_contract.cpp`) asserting the shared surface of all 8 map implementations and 5 sets, including the `try_emplace` zero/one/multi-argument forms (issue #104 guard) and pinned documented deviations
- `scripts/check_ci_registration.py` plus a fast-forward CI `registration` job: every unit/memory test must be compiled and run in the build, asan, coverage and windows-msvc jobs
- Heterogeneous lookup for `emhash8::HashMap`: transparent-hash overloads (`find`/`contains`/`count`/`at`/`erase`/`try_get`) accepting `string_view`/`char*` keys (find-hit ~43% faster than temporary-string construction)
- `bench/framework` and `bench/emilib_bench` benchmark suites, A/B scenarios and a callgrind instruction probe for layout-noise-free regression evidence
- README "Development Conventions": performance evidence grading, optimization acceptance rule, cross-implementation sync checklist

### Changed
- `hash_table8.hpp` optimizations (borrowed from martinus/unordered_dense): shared empty-index sentinel with lazy allocation (default construction 63ns → 0.4ns, zero allocations), ADL `swap`, opt-in `EMH_POD_HASH` byte-hashing for 8/12/16-byte trivially-copyable keys, precomputed-hash lookup path
- CI: fixed 10 registration gaps — `test_ht8_optimizations`/`test_ht8_hetero` now run in asan and coverage jobs, `test_api_coverage` and both ht8 tests added to windows-msvc, three compiled-but-never-run tests now executed

### Fixed
- [#104](https://github.com/ktprime/emhash/issues/104): `HashMap::try_emplace` failed to compile with zero or multiple mapped-value constructor arguments on `hash_table5-8`; the value is now constructed in place from the argument pack and only when the key is absent
- `hash_table7.hpp` hardening: CTZ byte-swap width matched to `sizeof(size_t)` (32-bit safe), power-of-two metadata invariants documented and asserted, dead code removed

## [1.2.0] - 2026-07-15

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
- `docs/faq.md` — Frequently asked questions
- `docs/migration_guide.md` — Migration guide from std::unordered_map
- `scripts/pre-commit.sh` — Git pre-commit hook for clang-format checking
- `docs/examples/CMakeLists.txt` — CMake build for all examples + quick_bench
- Method-level Doxygen for `hash_table8.hpp` core API (contains, try_get, try_set, insert_unique, set_get, erase)
- `THIRDPARTY_LICENSES.md` — aggregated license summary for all `thirdparty/` dependencies
- `tests/common/` — shared test infrastructure (doctest.h, hashers.hpp, maps.hpp, msan_unpoison.hpp, trackers.hpp, utilities.hpp)
- MSan unpoison header (`tests/common/msan_unpoison.hpp`) injected via `-include` to suppress false positives from uninstrumented libc++ `std::cout`/`std::cerr`
- `.github/msan-suppressions.txt` — explicit MSan suppression list
- CI: 80% line coverage gate (lcov + bc) and 20% benchmark regression gate against `benchmark-baseline` artifact

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
- (none — `emhash6/7 merge` and `emilib2/3 set merge/erase_if` were verified correct in 2026-10 against `std::unordered_map`/`std::unordered_set` ground truth, 300 randomized rounds plus 5000-element rehash-heavy runs; the previously documented key-loss/runtime issues no longer reproduce)
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
