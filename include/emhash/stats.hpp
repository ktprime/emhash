// emhash runtime statistics framework
// https://github.com/ktprime/emhash
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2026 Huang Yuanbing & bailuzhou AT 163.com

#pragma once

#include <cstdint>
#include <cstddef>

namespace emhash {

// Snapshot of hash table runtime health metrics.
// All fields are instantaneous values captured at the time of the call.
struct HashStatsSnapshot {
    // Total number of entries currently stored
    size_t num_filled = 0;
    // Total number of buckets (slots) allocated
    size_t num_buckets = 0;

    // Current load factor: num_filled / num_buckets
    double load_factor = 0.0;
    // Configured max load factor threshold
    double max_load_factor = 0.0;

    // Number of rehash operations since construction or last reset
    size_t rehash_count = 0;
    // Total number of insert operations
    size_t insert_count = 0;
    // Total number of erase operations
    size_t erase_count = 0;
    // Total number of find/lookup operations
    size_t find_count = 0;

    // Accumulated total probe distance across all insert operations.
    // Average probe per insert = total_insert_probe / insert_count.
    size_t total_insert_probe = 0;
    // Accumulated total probe distance across all find operations.
    // Average probe per find = total_find_probe / find_count.
    size_t total_find_probe = 0;

    // Worst-case (maximum) probe distance seen in any single operation
    size_t worst_probe = 0;
    // Worst probe distance during inserts specifically
    size_t worst_insert_probe = 0;
    // Worst probe distance during finds specifically
    size_t worst_find_probe = 0;

    // Number of kickout operations (emhash8/7: displaced guest keys)
    size_t kickout_count = 0;
    // Number of collision chain traversals that required >1 step
    size_t multi_step_collisions = 0;
};

// Accumulator for runtime statistics — used when EMH_ENABLE_STATS=1.
// Stores counters in a single cache line (64 bytes) for cache efficiency.
struct HashStatsAccumulator {
    size_t rehash_count = 0;
    size_t insert_count = 0;
    size_t erase_count = 0;
    size_t find_count = 0;
    size_t total_insert_probe = 0;
    size_t total_find_probe = 0;
    size_t worst_probe = 0;
    size_t worst_insert_probe = 0;
    size_t worst_find_probe = 0;
    size_t kickout_count = 0;
    size_t multi_step_collisions = 0;

    void on_rehash() noexcept { ++rehash_count; }

    void on_insert(size_t probe_distance) noexcept {
        ++insert_count;
        total_insert_probe += probe_distance;
        if (probe_distance > worst_insert_probe)
            worst_insert_probe = probe_distance;
        if (probe_distance > worst_probe)
            worst_probe = probe_distance;
        if (probe_distance > 1)
            ++multi_step_collisions;
    }

    void on_erase() noexcept { ++erase_count; }

    void on_find(size_t probe_distance) noexcept {
        ++find_count;
        total_find_probe += probe_distance;
        if (probe_distance > worst_find_probe)
            worst_find_probe = probe_distance;
        if (probe_distance > worst_probe)
            worst_probe = probe_distance;
    }

    void on_kickout() noexcept { ++kickout_count; }

    void reset() noexcept {
        rehash_count = 0;
        insert_count = 0;
        erase_count = 0;
        find_count = 0;
        total_insert_probe = 0;
        total_find_probe = 0;
        worst_probe = 0;
        worst_insert_probe = 0;
        worst_find_probe = 0;
        kickout_count = 0;
        multi_step_collisions = 0;
    }

    HashStatsSnapshot snapshot(size_t num_filled, size_t num_buckets, double lf, double max_lf) const noexcept {
        HashStatsSnapshot s;
        s.num_filled = num_filled;
        s.num_buckets = num_buckets;
        s.load_factor = lf;
        s.max_load_factor = max_lf;
        s.rehash_count = rehash_count;
        s.insert_count = insert_count;
        s.erase_count = erase_count;
        s.find_count = find_count;
        s.total_insert_probe = total_insert_probe;
        s.total_find_probe = total_find_probe;
        s.worst_probe = worst_probe;
        s.worst_insert_probe = worst_insert_probe;
        s.worst_find_probe = worst_find_probe;
        s.kickout_count = kickout_count;
        s.multi_step_collisions = multi_step_collisions;
        return s;
    }
};

// Zero-overhead no-op stats — used when EMH_ENABLE_STATS=0 (default).
// All methods are empty; the compiler optimizes them away entirely.
struct NoopStats {
    static void on_rehash() noexcept {}
    static void on_insert(size_t) noexcept {}
    static void on_erase() noexcept {}
    static void on_find(size_t) noexcept {}
    static void on_kickout() noexcept {}
    static void reset() noexcept {}
};

} // namespace emhash
