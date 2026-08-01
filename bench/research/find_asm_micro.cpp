/**
 * Minimal micro-benchmark: isolate find-hit path for emihmap4_opt vs boost
 * Generate assembly with: clang++-20 -std=c++17 -O3 -march=native -msse2 -S -o find_asm.s find_asm_micro.cpp
 */
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO646_REMOVED_WARNING

#include "emilib/emihmap4_opt.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdint>
#include <vector>

// Volatile sink to prevent optimization
volatile size_t g_sink = 0;

// ─── emihmap4_opt find benchmark ─────────────────────────────────────

__attribute__((noinline))
size_t bench_emihmap4_find(emilib4_opt::HashMap<uint64_t, uint64_t>& m, const std::vector<uint64_t>& keys) {
    size_t sum = 0;
    for (auto& k : keys) {
        auto it = m.find(k);
        if (it != m.end()) sum += it->second;
    }
    return sum;
}

// ─── boost find benchmark ────────────────────────────────────────────

__attribute__((noinline))
size_t bench_boost_find(boost::unordered_flat_map<uint64_t, uint64_t>& m, const std::vector<uint64_t>& keys) {
    size_t sum = 0;
    for (auto& k : keys) {
        auto it = m.find(k);
        if (it != m.end()) sum += it->second;
    }
    return sum;
}

int main() {
    // Setup
    const int N = 100000;
    std::vector<uint64_t> keys(N);
    for (int i = 0; i < N; i++) keys[i] = i + 1;

    emilib4_opt::HashMap<uint64_t, uint64_t> em;
    boost::unordered_flat_map<uint64_t, uint64_t> bm;
    em.reserve(N);
    bm.reserve(N);
    for (auto& k : keys) { em[k] = k; bm[k] = k; }

    // Run
    g_sink = bench_emihmap4_find(em, keys);
    g_sink = bench_boost_find(bm, keys);
    return 0;
}
