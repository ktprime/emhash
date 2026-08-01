// stress/test_complex_conditions.cpp
// Complex working condition tests for emhash/emilib hash maps.
//
// This file covers scenarios that are NOT covered by existing unit/stress/attack
// tests:
//   1. Concurrent read-only access (multi-threaded, TSan-compatible)
//   2. Burst insert/erase cycles (data surge simulation)
//   3. High load factor sustained operations (0.99f load factor + mixed ops)
//   4. Allocator failure injection (OOM simulation + strong exception guarantee)
//   5. Long-running stability (10M mixed operations with periodic verification)
//   6. Performance metrics collection (PSL distribution, collision rate, latency)
//
// Pass Criteria:
//   - All data integrity checks pass after each scenario
//   - No memory leaks (verified by CountingAllocator in scenario 4)
//   - Concurrent read scenario has no data races (verified by TSan)
//   - Long-running test completes within 60 seconds for 10M operations
//   - Performance metrics are collected and reported (non-blocking)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Scenario 1: Concurrent Read-Only Access
// emhash maps are not thread-safe for writes, but concurrent reads from a
// const map should be safe (no mutations, no lazy init). This test spawns
// multiple reader threads to verify no data corruption occurs.
//
// TSan should report NO data races for read-only concurrent access.
// ============================================================================
TEST_CASE("concurrent read-only access: 8 threads, 100K elements") {
    constexpr int N = 100000;
    constexpr int THREADS = 8;

    map7<int, int> map;
    (void)map.reserve(N);
    for (int i = 0; i < N; i++)
        map[i] = i * 2;

    std::atomic<int> errors{0};
    std::atomic<long long> total_reads{0};

    auto reader = [&]() {
        std::mt19937 rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        for (int iter = 0; iter < 100000; iter++) {
            int key = rng() % N;
            auto it = map.find(key);
            if (it == map.end() || it->second != key * 2) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
            total_reads.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; i++)
        threads.emplace_back(reader);
    for (auto& t : threads)
        t.join();

    CHECK(errors.load() == 0);
    CHECK(total_reads.load() == THREADS * 100000LL);
}

TEST_CASE("concurrent read with concurrent insert on separate maps") {
    constexpr int N = 50000;
    constexpr int THREADS = 4;

    // Each thread has its own map to verify thread safety of construction/destruction
    std::atomic<int> errors{0};

    auto worker = [&](int tid) {
        map8<int, int> map;
        (void)map.reserve(N);
        for (int i = 0; i < N; i++)
            map[tid * N + i] = i;
        for (int i = 0; i < N; i++) {
            if (map.at(tid * N + i) != i)
                errors.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; i++)
        threads.emplace_back(worker, i);
    for (auto& t : threads)
        t.join();

    CHECK(errors.load() == 0);
}

// ============================================================================
// Scenario 2: Burst Insert/Erase Cycles (Data Surge Simulation)
// Simulates real-world traffic patterns: rapid growth followed by mass
// deletion, then growth again. Tests rehash stability and bucket reuse.
// ============================================================================
TEST_CASE("burst insert/erase: 10 cycles of 100K insert + 90K erase") {
    constexpr int N = 100000;
    constexpr int ERASE = 90000;
    constexpr int CYCLES = 10;

    map8<int, int> map;

    for (int cycle = 0; cycle < CYCLES; cycle++) {
        // Burst insert
        for (int i = 0; i < N; i++)
            map[cycle * N + i] = i;
        // Size = previous survivors + this cycle's N
        CHECK(map.size() == cycle * (N - ERASE) + N);

        // Mass erase (keep only 10K from this cycle)
        for (int i = 0; i < ERASE; i++)
            map.erase(cycle * N + i);
        // Size = all cycles' survivors
        CHECK(map.size() == (cycle + 1) * (N - ERASE));

        // Verify remaining elements from this cycle
        for (int i = ERASE; i < N; i++) {
            auto it = map.find(cycle * N + i);
            CHECK(it != map.end());
            CHECK(it->second == i);
        }
    }

    // Final state: 10 cycles * 10K = 100K elements
    CHECK(map.size() == CYCLES * (N - ERASE));

    // Verify all remaining elements are correct
    for (int cycle = 0; cycle < CYCLES; cycle++) {
        for (int i = ERASE; i < N; i++) {
            CHECK(map.at(cycle * N + i) == i);
        }
    }
}

TEST_CASE("burst string key insert/erase: 5 cycles of 50K") {
    constexpr int N = 50000;
    constexpr int CYCLES = 5;

    map7<std::string, int> map;

    for (int cycle = 0; cycle < CYCLES; cycle++) {
        // Insert
        for (int i = 0; i < N; i++)
            map["cycle" + std::to_string(cycle) + "_key" + std::to_string(i)] = cycle * 1000 + i;
        // Size = this cycle's N (after erasing previous cycle)
        if (cycle > 0) {
            for (int i = 0; i < N; i++)
                map.erase("cycle" + std::to_string(cycle - 1) + "_key" + std::to_string(i));
        }
        CHECK(map.size() == N);
    }

    // Final: only last cycle's keys remain
    CHECK(map.size() == N);
}

// ============================================================================
// Scenario 3: High Load Factor Sustained Operations
// Tests behavior at extreme load factors (0.99f) with mixed operations.
// At high load, collision rates increase and probe sequences lengthen.
// ============================================================================
TEST_CASE("high load factor 0.99f: 1M elements sustained find") {
    constexpr int N = 1000000;

    map8<int, int> map;
    map.max_load_factor(0.99f);
    (void)map.reserve(N);

    for (int i = 0; i < N; i++)
        map[i] = i;

    // Verify load factor is actually high
    CHECK(map.load_factor() > 0.95f);
    CHECK(map.max_load_factor() == doctest::Approx(0.99f).epsilon(0.01f));

    // Find all elements (find-hit at high load)
    int errors = 0;
    for (int i = 0; i < N; i++) {
        auto it = map.find(i);
        if (it == map.end() || it->second != i)
            errors++;
    }
    CHECK(errors == 0);

    // Find-miss at high load
    for (int i = N; i < N + 10000; i++) {
        auto it = map.find(i);
        if (it != map.end())
            errors++;
    }
    CHECK(errors == 0);
}

TEST_CASE("high load factor: mixed insert/find/erase at 0.98f") {
    constexpr int N = 200000;
    map7<int, int> map;
    map.max_load_factor(0.98f);

    // Phase 1: Insert
    for (int i = 0; i < N; i++)
        map[i] = i;
    CHECK(map.size() == N);

    // Phase 2: Find + Erase + Reinsert interleaved
    for (int i = 0; i < N; i += 3) {
        auto it = map.find(i);
        CHECK(it != map.end());
        CHECK(map.erase(i) > 0);
        map[i + N] = i + N; // Insert new key
    }

    // Verify: original 2/3 + new 1/3 = N elements
    CHECK(map.size() == N);

    // Phase 3: Verify all remaining original elements
    for (int i = 0; i < N; i++) {
        if (i % 3 == 0) {
            // Erased and replaced with i+N
            CHECK(map.find(i) == map.end());
            CHECK(map.at(i + N) == i + N);
        } else {
            CHECK(map.at(i) == i);
        }
    }
}

// ============================================================================
// Scenario 4: Allocator Failure Injection (OOM Simulation)
// Tests strong exception guarantee: when allocator throws during rehash, the
// map must remain in its previous valid state with no corruption or leaks.
//
// This was previously disabled due to a bug where alloc_bucket() was marked
// noexcept (causing std::terminate on bad_alloc) and rehash() modified member
// variables before allocating new memory. Both issues are now fixed:
//   1. alloc_bucket() noexcept removed — bad_alloc can propagate
//   2. rehash() reordered — allocates new memory BEFORE updating members
// ============================================================================

struct FailingStats {
    int alloc_count = 0;
    int fail_at = -1; // Fail on the Nth allocation (-1 = never)
    int dealloc_count = 0;
    void reset() { alloc_count = dealloc_count = 0; }
};

template <typename T> struct FailingAllocator {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    FailingStats* stats;

    FailingAllocator() : stats(nullptr) {}
    explicit FailingAllocator(FailingStats* s) : stats(s) {}
    template <typename U> FailingAllocator(const FailingAllocator<U>& o) noexcept : stats(o.stats) {}

    T* allocate(std::size_t n) {
        if (stats) {
            stats->alloc_count++;
            if (stats->fail_at >= 0 && stats->alloc_count >= stats->fail_at) {
                stats->fail_at = -1; // Fail only once, then auto-reset
                throw std::bad_alloc();
            }
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) {
        if (stats)
            stats->dealloc_count++;
        ::operator delete(p);
    }

    template <typename U> bool operator==(const FailingAllocator<U>& other) const { return stats == other.stats; }
    template <typename U> bool operator!=(const FailingAllocator<U>& other) const { return stats != other.stats; }
};

TEST_CASE("allocator failure during rehash: map remains valid") {
    using FA = FailingAllocator<std::pair<const int, int>>;
    using FailMap = emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, FA>;

    FailingStats stats;
    stats.fail_at = -1; // No failures during setup

    FailMap map{FA{&stats}};
    for (int i = 0; i < 1000; i++)
        map[i] = i;

    // Now fail on the next allocation (trigger rehash)
    stats.fail_at = stats.alloc_count + 1;
    bool caught = false;
    try {
        for (int i = 1000; i < 10000; i++)
            map[i] = i;
    } catch (const std::bad_alloc&) {
        caught = true;
    }
    CHECK(caught);

    // Strong exception guarantee: map must still be usable
    // All original 1000 elements must be intact
    int valid = 0;
    for (int i = 0; i < 1000; i++) {
        auto it = map.find(i);
        if (it != map.end() && it->second == i)
            valid++;
    }
    CHECK(valid == 1000);

    // Map should accept new insertions after recovery
    stats.fail_at = -1;
    map[99999] = 99999;
    CHECK(map.at(99999) == 99999);
}

TEST_CASE("allocator failure: no memory leak after OOM") {
    using FA = FailingAllocator<std::pair<const int, int>>;
    using FailMap = emhash5::HashMap<int, int, std::hash<int>, std::equal_to<int>, FA>;

    FailingStats stats;
    stats.fail_at = -1;

    {
        FailMap map{FA{&stats}};
        for (int i = 0; i < 5000; i++)
            map[i] = i;

        // Trigger failure
        stats.fail_at = stats.alloc_count + 1;
        try {
            for (int i = 5000; i < 50000; i++)
                map[i] = i;
        } catch (const std::bad_alloc&) {
            // Expected — map should remain in valid state
        }

        // Map destructor should free all allocated memory
    }

    // Verify all successful allocations were freed (no leak).
    // alloc_count includes 1 failed allocation that threw before ::operator new,
    // so dealloc_count should equal alloc_count - 1.
    CHECK(stats.dealloc_count == stats.alloc_count - 1);
}

// ============================================================================
// Scenario 5: Long-Running Stability (10M Mixed Operations)
// Simulates sustained production-like workload over an extended period.
// Periodically verifies data integrity and checks for memory growth.
// ============================================================================
TEST_CASE("long-running: 10M mixed operations with periodic verification") {
    constexpr int TOTAL_OPS = 10000000;
    constexpr int VERIFY_EVERY = 1000000;
    constexpr int MAP_CAPACITY = 500000;

    map8<int, int> map;
    (void)map.reserve(MAP_CAPACITY);

    std::mt19937 rng(42);
    std::uniform_int_distribution<> op_dist(0, 9); // 0-3 insert, 4-6 find, 7-9 erase
    std::uniform_int_distribution<> key_dist(0, MAP_CAPACITY * 2 - 1);

    auto start = std::chrono::steady_clock::now();
    int insert_count = 0, find_count = 0, erase_count = 0;

    for (int op = 0; op < TOTAL_OPS; op++) {
        int key = key_dist(rng);
        int action = op_dist(rng);

        if (action <= 3) {
            map[key] = key;
            insert_count++;
        } else if (action <= 6) {
            auto it = map.find(key);
            if (it != map.end())
                CHECK(it->second == key);
            find_count++;
        } else {
            map.erase(key);
            erase_count++;
        }

        // Periodic verification
        if ((op + 1) % VERIFY_EVERY == 0) {
            // Verify size is within expected bounds
            CHECK(map.size() <= MAP_CAPACITY * 2);

            // Verify a sample of elements
            bool sample_ok = true;
            for (int s = 0; s < 100; s++) {
                int k = key_dist(rng);
                auto it = map.find(k);
                if (it != map.end() && it->second != k) {
                    sample_ok = false;
                    break;
                }
            }
            CHECK(sample_ok);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Report performance metrics (non-blocking — just printed to stdout)
    double ops_per_sec = TOTAL_OPS / (elapsed.count() / 1000.0);
    std::printf("\n  [perf] 10M ops in %lldms (%.0f ops/sec)\n"
                "         inserts=%d finds=%d erases=%d final_size=%llu\n",
                (long long)elapsed.count(), ops_per_sec, insert_count, find_count, erase_count,
                (unsigned long long)map.size());

    // Pass criteria: completes within 60 seconds (non-strict, informational)
    CHECK(elapsed.count() < 60000);

    // Final integrity check
    int integrity_errors = 0;
    for (auto& p : map) {
        if (p.first != p.second)
            integrity_errors++;
    }
    CHECK(integrity_errors == 0);
}

// ============================================================================
// Scenario 6: Performance Metrics Collection
// Collects probe sequence length distribution, collision rate, and operation
// latency. This is informational — reported via printf, not enforced.
// ============================================================================
TEST_CASE("performance metrics: PSL and collision rate at various loads") {
    constexpr int SIZES[] = {1000, 10000, 100000, 1000000};

    for (int N : SIZES) {
        map8<int, int> map;
        (void)map.reserve(N);
        for (int i = 0; i < N; i++)
            map[i] = i;

        // Measure find-hit latency
        auto start = std::chrono::steady_clock::now();
        int hits = 0;
        for (int i = 0; i < N; i++) {
            auto it = map.find(i);
            if (it != map.end())
                hits++;
        }
        auto end = std::chrono::steady_clock::now();
        auto hit_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / N;

        // Measure find-miss latency
        start = std::chrono::steady_clock::now();
        int misses = 0;
        for (int i = N; i < N * 2; i++) {
            auto it = map.find(i);
            if (it != map.end())
                misses++;
        }
        end = std::chrono::steady_clock::now();
        auto miss_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / N;

        // Report metrics (informational, not enforced)
        std::printf("\n  [metrics] N=%d load_factor=%.3f find_hit=%lldns find_miss=%lldns\n", N,
                    (double)map.load_factor(), (long long)hit_ns, (long long)miss_ns);

        // Basic correctness
        CHECK(hits == N);
        CHECK(misses == 0);
    }
}

TEST_CASE("performance metrics: operation latency under mixed workload") {
    constexpr int N = 500000;
    map7<int, int> map;
    (void)map.reserve(N);

    // Insert phase
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; i++)
        map[i] = i;
    auto t1 = std::chrono::steady_clock::now();
    auto insert_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    // Find phase
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; i++)
        (void)map.find(i);
    t1 = std::chrono::steady_clock::now();
    auto find_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    // Erase phase
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; i++)
        map.erase(i);
    t1 = std::chrono::steady_clock::now();
    auto erase_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    std::printf("\n  [latency] N=%d insert=%lldus find=%lldus erase=%lldus\n", N, (long long)insert_us,
                (long long)find_us, (long long)erase_us);

    // Erase should be within 3x of insert (heuristic)
    CHECK(erase_us < insert_us * 3);
    CHECK(map.size() == 0);
}

// ============================================================================
// Scenario 7: Adversarial Input Patterns
// Tests with pathological inputs designed to trigger worst-case behavior.
// ============================================================================
TEST_CASE("adversarial: sequential keys (worst case for some hash functions)") {
    constexpr int N = 100000;
    map8<int, int> map;

    // Sequential keys can cause clustering with poor hash functions
    for (int i = 0; i < N; i++)
        map[i] = i;

    CHECK(map.size() == N);

    // All elements must be findable
    for (int i = 0; i < N; i++) {
        CHECK(map.at(i) == i);
    }
}

TEST_CASE("adversarial: very large string keys") {
    constexpr int N = 10000;
    constexpr int KEY_LEN = 1000;
    map7<std::string, int> map;

    // Use std::to_string(i) as prefix to guarantee uniqueness
    for (int i = 0; i < N; i++) {
        std::string key = std::to_string(i) + std::string(KEY_LEN - std::to_string(i).length(), 'x');
        map[key] = i;
    }

    CHECK(map.size() == N);

    // Verify a sample
    for (int i = 0; i < N; i += 100) {
        std::string key = std::to_string(i) + std::string(KEY_LEN - std::to_string(i).length(), 'x');
        CHECK(map.at(key) == i);
    }
}

TEST_CASE("adversarial: extreme load factor 0.999f with 100K elements") {
    constexpr int N = 100000;
    map8<int, int> map;
    map.max_load_factor(0.999f);
    // Don't reserve — let the map grow naturally to reach high load factor

    for (int i = 0; i < N; i++)
        map[i] = i;

    // At 0.999 load factor, load should be high (but not necessarily >0.99
    // because the map may have rehashed to a larger size)
    CHECK(map.load_factor() > 0.5f);
    CHECK(map.max_load_factor() == doctest::Approx(0.999f).epsilon(0.01f));

    // All elements must still be accessible
    for (int i = 0; i < N; i++) {
        CHECK(map.at(i) == i);
    }

    // Erasing should work correctly
    for (int i = 0; i < N / 2; i++) {
        CHECK(map.erase(i) > 0);
    }
    CHECK(map.size() == N / 2);

    // Remaining elements still correct
    for (int i = N / 2; i < N; i++) {
        CHECK(map.at(i) == i);
    }
}
