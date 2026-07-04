# Third-Party Dependencies License Summary

> **Scope**: This file summarizes the licenses of third-party dependencies referenced by the
> emhash project (primarily for benchmark comparison). The full source and license texts
> live in the [`thirdparty/`](thirdparty/) git submodule, which points to
> [`ktprime/emhash-bench-deps`](https://github.com/ktprime/emhash-bench-deps).
>
> **No third-party code is compiled into the emhash library itself.**
> The headers under [`include/`](include/) are the only files users link against.
> Third-party code is used solely by `bench/` and `tests/` for performance comparison
> and correctness cross-checks.
>
> **For full license text, run** `git submodule update --init thirdparty` **and inspect
> `thirdparty/<project>/LICENSE*` directly.**

## License Summary Table

| Project | Directory | License | Upstream | Purpose |
|---|---|---|---|---|
| abseil-cpp | `thirdparty/absl/` | Apache-2.0 | https://github.com/abseil/abseil-cpp | `flat_hash_map` benchmark baseline |
| ahash (C) | `thirdparty/ahash/` | MIT OR Apache-2.0 | https://github.com/tkaitchuck/ahash | Hash function benchmark |
| ahash-cxx | `thirdparty/ahash-cxx/` | MIT OR Apache-2.0 | https://github.com/ahknight/ahash-cxx | C++ aHash wrapper |
| Boost (subset) | `thirdparty/boost/` | BSL-1.0 | https://www.boost.org/ | `boost::unordered_map` benchmark |
| ClickHouse hash | `thirdparty/ck/` | Apache-2.0 | https://github.com/ClickHouse/ClickHouse | City hash comparison |
| ExcaliburHash | `thirdparty/ExcaliburHash/` | MIT | https://github.com/Excalibur/ExcaliburHash | Hash map benchmark |
| FHashTable | `thirdparty/FHashTable/` | MIT | https://github.com/factual/FHashTable | Hash table comparison |
| fph (frozen-hash-position) | `thirdparty/fph/` | MIT | https://github.com/kaifeidai/fph | Dynamic FPH table benchmark |
| google-sparsehash | `thirdparty/google/` | BSD-3-Clause | https://github.com/sparsehash/sparsehash | `dense_hash_map` baseline |
| hrd hash_set | `thirdparty/hrd/` | MIT | https://github.com/hrd/hash_set | Hash set comparison |
| indivi containers | `thirdparty/indivi/` | MIT | https://github.com/indivi/containers | `flat_umap` / `sparque` comparison |
| jg dense_hash_map | `thirdparty/jg/` | MIT | https://github.com/jgaa/dense_hash_map | Hash map benchmark |
| komihash | `thirdparty/komihash.h` | MIT | https://github.com/avaneevt/komihash | Hash function benchmark |
| libcuckoo | `thirdparty/libcuckoo/` | Apache-2.0 | https://github.com/efficient/libcuckoo | Cuckoo hash benchmark |
| martin/robin_hood | `thirdparty/martin/` | MIT | https://github.com/martinus/robin_hood | `robin_hood::unordered_map` baseline |
| martin/unordered_dense | `thirdparty/martin/` | MIT | https://github.com/martinus/unordered_dense | `unordered_dense` benchmark |
| nark hash maps | `thirdparty/nark/` | LGPL-2.1+ | https://github.com/rocky/nark | Gold-hash comparison |
| patchmap | `thirdparty/patchmap/` | MIT | https://github.com/skarupke/patchmap | Skarupke hash map benchmark |
| phmap | `thirdparty/phmap/` | MIT | https://github.com/greg7mdp/parallel-hashmap | `phmap::flat_hash_map` baseline |
| qchash | `thirdparty/qchash/` | MIT | https://github.com/0x10240/qchash | Hash map benchmark |
| rapidhash | `thirdparty/rapidhash/` | BSD-2-Clause | https://github.com/Nicoshev/rapidhash | Hash function benchmark |
| rigtorp hashmap | `thirdparty/rigtorp/` | MIT | https://github.com/rigtorp/Hashmap | Hash map benchmark |
| simd_hash_map | `thirdparty/simd_hash_map/` | MIT | https://github.com/Nicoshev/simd_hash_map | SIMD hash map benchmark |
| ska bytell_hash_map | `thirdparty/ska/` | MIT | https://github.com/skarupke/flat_hash_map | Bytell / flat hash map baseline |
| tsl (tessil) | `thirdparty/tsl/` | MIT | https://github.com/Tessil/* | robin / hopscotch / sparse / array / htrie baselines |
| wyhash | `thirdparty/wyhash.h` | Unlicense | https://github.com/wangyi-fudan/wyhash | Hash function (used internally by emhash as fallback) |
| wyhash2 | `thirdparty/wyhash2.h` | Unlicense | https://github.com/wangyi-fudan/wyhash | Hash function |
| sse2neon | `thirdparty/sse2neon.h` | MIT | https://github.com/DLTcollab/sse2neon | ARM NEON emulation of SSE2 intrinsics |
| zhashmap | `thirdparty/zhashmap/` | MIT | https://github.com/zhou2011/ztinyhash | Hash map benchmark |
| a5hash | `thirdparty/a5hash.h` | MIT | — | Hash function |
| w1hash | `thirdparty/w1hash.h` | MIT | — | Hash function |
| lru_map | `thirdparty/lru_map.h` | MIT | — | LRU cache comparison |
| pdqsort | `thirdparty/pdqsort.h` | MIT | https://github.com/orlp/pdqsort | Pattern-defeating quicksort |
| sfc64 | `thirdparty/sfc64.h` | MIT | https://github.com/wunk的一个sfc64 | PRNG for benchmarks |
| ska_sort | `thirdparty/ska_sort.hpp` | MIT | https://github.com/skarupke/ska_sort | Radix sort |

## License Categories

- **Permissive (MIT / BSD / BSL / Unlicense / Apache-2.0)**: All benchmark-comparison dependencies.
  These licenses allow unrestricted use, modification, and distribution with attribution.
- **Copyleft (LGPL-2.1+)**: `nark` is the only LGPL dependency. It is **benchmark-only** and is
  **not linked into the emhash library**. Users who do not run the benchmark suite are unaffected.
  If you build and redistribute the benchmark binaries, you must comply with LGPL terms
  (dynamic linking recommended, or include source).
- **Public Domain / Unlicense**: `wyhash` / `wyhash2` are public-domain-equivalent.

## emhash License

The emhash library itself is licensed under the **Apache License 2.0** — see
[LICENSE](LICENSE) for the full text.

## Updating This File

When adding a new dependency to `thirdparty/`:
1. Add the upstream URL and license to the table above.
2. Verify the project's `LICENSE` / `COPYING` / `README` is present in the submodule.
3. If the license is copyleft (GPL/LGPL/AGPL), flag it for review — emhash should not
   transitively burden users who only use the library headers.

When in doubt, prefer MIT- or Apache-2.0-licensed dependencies for benchmarks.
