# Performance Overview

## Performance Comparison with Mainstream Implementations (lower is better)

Test Environment: AMD 5800H / Windows 10 / GCC 11.3

| Operation | emhash7 | emhash8 | emhash6 | emhash5 | phmap | absl | std::unordered_map |
|-----------|---------|---------|---------|---------|-------|------|-------------------|
| Insert (10M) | 66.0 | 95.6 | 63.6 | 64.7 | 59.5 | 53.2 | 295 |
| Find Hit | 16.9 | 18.3 | 15.1 | 16.6 | 36.4 | 22.6 | 33.0 |
| Find Miss | 18.1 | 18.1 | 17.0 | 18.8 | 19.7 | 11.7 | 52.0 |
| Erase | 20.4 | 46.3 | 24.0 | 22.3 | 56.2 | 41.5 | 293 |
| Iterate | 0.74 | 0.00 | 0.75 | 4.75 | 3.46 | 3.49 | 62.6 |

> **Note**: emhash8's iteration time of 0.00 ms indicates extremely fast iteration due to its contiguous memory layout (actual time is < 0.005 ms).

## Excellent Performance at High Load Factors

Even at an extreme load factor of **0.999**, emhash maintains outstanding performance:

```cpp
emhash7::HashMap<int64_t, int> myhash(1 << 20, 0.999f);
// Insert 1M+ elements without rehash
// Stable insert/erase performance without degradation
```

### How it works

```cpp
// Compile: g++ -O3 -march=native -I.. -I../thirdparty -std=c++17 -DEMH_HIGH_LOAD=123456 highload_bench.cpp

#include "hash_table7.hpp"

static void RunHighLoadFactor()
{
    std::random_device rd;
    const auto rand_key = rd();
    WyRand srngi(rand_key), srnge(rand_key);

    const auto max_lf   = 0.999f; //<= 0.999f
    const auto vsize    = 1u << (20 + rand_key % 6); //must be power of 2
    emhash7::HashMap<int64_t, int> myhash(vsize, max_lf);

    auto nowus = getus();
    for (size_t i = 0; i < size_t(vsize * max_lf); i++)
        myhash.emplace(srngi(), i);
    assert(myhash.bucket_count() == vsize); //no rehash

    const auto insert_time = getus() - nowus; nowus = getus();
    //erase & insert at a fixed load factor
    for (size_t i = 0; i < vsize; i++) {
        myhash.erase(srnge()); //erase an old key
        myhash[srngi()] = 1;   //insert a new key
    }
    const auto erase_time = getus() - nowus;
    printf("vsize = %d, load factor = %.4f, insert/erase = %ld/%ld ms\n",
        vsize, myhash.load_factor(), insert_time / 1000, erase_time / 1000);
    assert(myhash.load_factor() >= max_lf - 0.001);
}
```

Full benchmark code with multi-version comparison: [bench/highload_bench.cpp](https://github.com/ktprime/emhash/blob/master/bench/highload_bench.cpp)

### Real benchmark results (1M buckets, LF=0.999, compiled with `-DEMH_HIGH_LOAD=123456`)

Test Environment: AMD 5800H / Windows 10 / GCC 11.3

| hashmap          | Insert(ms) | Erase+Insert(ms) | LF    |
|------------------|------------|------------------|------|
| emhash7::HashMap |         44 |              118 | 99.9 |
| emhash6::HashMap |         38 |              202 | 99.9 |
| emhash5::HashMap |         42 |               90 | 99.9 |
| emhash8::HashMap |         49 |              110 | 99.9 |

| hashmap          | Fhit(ms) | Fmiss(ms) | Iter(ms) | LF    |
|------------------|----------|-----------|----------|------|
| emhash7::HashMap |       18 |        21 |        2 | 99.9 |
| emhash6::HashMap |       15 |        18 |        2 | 99.9 |
| emhash5::HashMap |       17 |        17 |        1 | 99.9 |

> Other hash maps (absl, phmap, ska, tsl, robin_hood) cannot run at 0.999 LF — they either cap max_load_factor at ~0.875 or suffer catastrophic clustering.
