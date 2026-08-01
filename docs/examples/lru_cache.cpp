/// @file lru_cache.cpp
/// @brief Example: LRU Cache usage — time-based and size-based eviction
///
/// This example demonstrates two LRU cache implementations:
/// - emlru_time::lru_cache: evicts entries by TTL (time-to-live)
/// - emlru_size::lru_cache: evicts least recently used entries when size limit reached

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "emhash/lru_time.hpp"
#include "emhash/lru_size.hpp"

static void example_time_based() {
    std::cout << "=== Time-based LRU Cache ===\n";

    // Constructor: lru_cache(bucket, max_bucket, timeout_ms)
    //   - bucket:      initial bucket count
    //   - max_bucket:  maximum number of slots (bounded by ~2x before half-eviction kicks in)
    //   - timeout_ms:  TTL in milliseconds for each entry
    // Here we create a cache with 64 initial buckets, 1024 max slots, 200ms TTL.
    emlru_time::lru_cache<int, std::string> cache(64, 1024, 200);

    cache[1] = "one";
    cache[2] = "two";
    cache[3] = "three";

    std::cout << "After insert: size = " << cache.size() << "\n"; // 3

    // All entries should be present
    assert(cache.contains(1));
    assert(cache.contains(2));
    assert(cache.contains(3));
    assert(cache.size() == 3);

    // Wait for TTL to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Note: Expired entries are evicted lazily on the next rehash / insert cycle.
    // Force a rehash by reserving more slots so the expired ones are dropped.
    cache.reserve(128);

    cache[4] = "four";
    std::cout << "After TTL + reserve + re-insert: size = " << cache.size() << "\n"; // 1 (only 4)

    assert(cache.contains(4));
    assert(!cache.contains(1)); // evicted during rehash

    cache.clear();
    std::cout << "After clear: size = " << cache.size() << "\n"; // 0
}

static void example_size_based() {
    std::cout << "\n=== Size-based LRU Cache ===\n";

    // Constructor: lru_cache(bucket, max_bucket)
    //   - bucket:     initial bucket count (reserve)
    //   - max_bucket: internal slot limit; eviction (remove_half) triggers
    //                 when _num_filled >= _max_buckets * 2. Use small values
    //                 here so the example can exercise eviction.
    // Here: 8 initial buckets, max_bucket = 2 → half-eviction triggers
    //       once _num_filled reaches ~4, leaving roughly the more-recently-used half.
    emlru_size::lru_cache<int, std::string> cache(8, 2);

    cache[1] = "one";
    cache[2] = "two";
    cache[3] = "three";
    std::cout << "After insert 3 items: size = " << cache.size() << "\n"; // 3

    assert(cache.size() == 3);

    // Insert 4th item → eviction threshold crossed; some older entries get pruned.
    cache[4] = "four";
    std::cout << "After insert 4th item (triggered half-eviction): size = " << cache.size() << "\n"; // ≤ 3
    assert(cache.size() <= 3);
    assert(cache.contains(4));

    // Access key 2 (refreshes its orderid, making it more likely to survive)
    (void)cache[2];
    cache[5] = "five"; // another cycle → more half-eviction
    assert(cache.contains(2));

    // Remove a specific entry using key-based erase (lru_size has no erase_if)
    cache.erase(2);
    std::cout << "After erase(key=2): size = " << cache.size() << "\n";
    assert(!cache.contains(2));

    cache.clear();
    assert(cache.empty());
    std::cout << "After clear: size = " << cache.size() << "\n";
}

int main() {
    example_time_based();
    example_size_based();

    std::cout << "\nAll LRU cache examples passed!\n";
    return 0;
}