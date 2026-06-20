// emhash Performance Benchmark
// Uses Google Benchmark to measure core operations: insert, find, erase, iterate
// Covers all emhash versions (5/6/7/8) and emilib versions (1/2/3)

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emhash/hash_set2.hpp"
#include "emhash/hash_set3.hpp"
#include "emhash/hash_set4.hpp"
#include "emhash/hash_set8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

// ============================================================================
// Utility: generate random int keys
// ============================================================================
static std::vector<int> generate_keys(int n) {
    std::mt19937 rng(42);
    std::vector<int> keys(n);
    for (int i = 0; i < n; i++) keys[i] = rng();
    return keys;
}

// ============================================================================
// Benchmark template: insert
// ============================================================================
template <typename Map>
static void BM_Insert(benchmark::State& state) {
    const int n = state.range(0);
    auto keys = generate_keys(n);
    for (auto _ : state) {
        Map map;
        map.reserve(n);
        for (int i = 0; i < n; i++) map.emplace(keys[i], i);
        benchmark::DoNotOptimize(map.size());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// ============================================================================
// Benchmark template: find (hit)
// ============================================================================
template <typename Map>
static void BM_FindHit(benchmark::State& state) {
    const int n = state.range(0);
    auto keys = generate_keys(n);
    Map map;
    map.reserve(n);
    for (int i = 0; i < n; i++) map.emplace(keys[i], i);

    for (auto _ : state) {
        for (int i = 0; i < n; i++) {
            auto it = map.find(keys[i]);
            benchmark::DoNotOptimize(it);
        }
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// ============================================================================
// Benchmark template: find (miss)
// ============================================================================
template <typename Map>
static void BM_FindMiss(benchmark::State& state) {
    const int n = state.range(0);
    auto keys = generate_keys(n);
    Map map;
    map.reserve(n);
    for (int i = 0; i < n; i++) map.emplace(keys[i], i);

    for (auto _ : state) {
        for (int i = 0; i < n; i++) {
            auto it = map.find(~keys[i]);
            benchmark::DoNotOptimize(it);
        }
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// ============================================================================
// Benchmark template: erase
// ============================================================================
template <typename Map>
static void BM_Erase(benchmark::State& state) {
    const int n = state.range(0);
    auto keys = generate_keys(n);
    for (auto _ : state) {
        state.PauseTiming();
        Map map;
        map.reserve(n);
        for (int i = 0; i < n; i++) map.emplace(keys[i], i);
        state.ResumeTiming();
        for (int i = 0; i < n; i++) map.erase(keys[i]);
        benchmark::DoNotOptimize(map.size());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

// ============================================================================
// Benchmark template: iterate
// ============================================================================
template <typename Map>
static void BM_Iterate(benchmark::State& state) {
    const int n = state.range(0);
    auto keys = generate_keys(n);
    Map map;
    map.reserve(n);
    for (int i = 0; i < n; i++) map.emplace(keys[i], i);

    volatile size_t sum = 0;
    for (auto _ : state) {
        size_t s = 0;
        for (auto& kv : map) s += kv.second;
        sum = s;
    }
    benchmark::DoNotOptimize(sum);
    state.SetItemsProcessed(state.iterations() * n);
}

// ============================================================================
// Register benchmarks for all HashMap versions
// ============================================================================
#define BENCH_MAP(MapType, Name)                              \
    BENCHMARK_TEMPLATE(BM_Insert, MapType)->Arg(100000);      \
    BENCHMARK_TEMPLATE(BM_FindHit, MapType)->Arg(100000);    \
    BENCHMARK_TEMPLATE(BM_FindMiss, MapType)->Arg(100000);   \
    BENCHMARK_TEMPLATE(BM_Erase, MapType)->Arg(100000);      \
    BENCHMARK_TEMPLATE(BM_Iterate, MapType)->Arg(100000);

using Map5 = emhash5::HashMap<int, int>;
using Map6 = emhash6::HashMap<int, int>;
using Map7 = emhash7::HashMap<int, int>;
using Map8 = emhash8::HashMap<int, int>;
using HMap1 = emilib::HashMap<int, int>;
using HMap2 = emilib2::HashMap<int, int>;
using HMap3 = emilib3::HashMap<int, int>;

BENCH_MAP(Map5, emhash5)
BENCH_MAP(Map6, emhash6)
BENCH_MAP(Map7, emhash7)
BENCH_MAP(Map8, emhash8)
BENCH_MAP(HMap1, emilib1)
BENCH_MAP(HMap2, emilib2)
BENCH_MAP(HMap3, emilib3)

// ============================================================================
// HashSet benchmarks
// ============================================================================
template <typename Set>
static void BM_SetInsert(benchmark::State& state) {
    const int n = state.range(0);
    auto keys = generate_keys(n);
    for (auto _ : state) {
        Set set;
        set.reserve(n);
        for (int i = 0; i < n; i++) set.emplace(keys[i]);
        benchmark::DoNotOptimize(set.size());
    }
    state.SetItemsProcessed(state.iterations() * n);
}

template <typename Set>
static void BM_SetFindHit(benchmark::State& state) {
    const int n = state.range(0);
    auto keys = generate_keys(n);
    Set set;
    set.reserve(n);
    for (int i = 0; i < n; i++) set.emplace(keys[i]);

    for (auto _ : state) {
        for (int i = 0; i < n; i++) {
            auto it = set.find(keys[i]);
            benchmark::DoNotOptimize(it);
        }
    }
    state.SetItemsProcessed(state.iterations() * n);
}

using Set2 = emhash2::HashSet<int>;
using Set3 = emhash3::HashSet<int>;
using Set4 = emhash4::HashSet<int>;
using Set8 = emhash8::HashSet<int>;

BENCHMARK_TEMPLATE(BM_SetInsert, Set2)->Arg(100000);
BENCHMARK_TEMPLATE(BM_SetInsert, Set3)->Arg(100000);
BENCHMARK_TEMPLATE(BM_SetInsert, Set4)->Arg(100000);
BENCHMARK_TEMPLATE(BM_SetInsert, Set8)->Arg(100000);
BENCHMARK_TEMPLATE(BM_SetFindHit, Set2)->Arg(100000);
BENCHMARK_TEMPLATE(BM_SetFindHit, Set3)->Arg(100000);
BENCHMARK_TEMPLATE(BM_SetFindHit, Set4)->Arg(100000);
BENCHMARK_TEMPLATE(BM_SetFindHit, Set8)->Arg(100000);

BENCHMARK_MAIN();
