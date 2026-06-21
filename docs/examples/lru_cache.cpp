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

    // Time-based cache with 200ms TTL and 64 bucket initial capacity
    emlru_time::lru_cache<int, std::string> cache(200, 64);

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

    // Access triggers eviction of expired entries
    cache[4] = "four";
    std::cout << "After TTL + re-insert: size = " << cache.size() << "\n"; // 1 (only 4)

    assert(cache.size() == 1);
    assert(cache.contains(4));
    assert(!cache.contains(1)); // evicted

    cache.clear();
    std::cout << "After clear: size = " << cache.size() << "\n"; // 0
}

static void example_size_based() {
    std::cout << "\n=== Size-based LRU Cache ===\n";

    // Size-based cache with max 3 entries
    emlru_size::lru_cache<int, std::string> cache(3);

    cache[1] = "one";
    cache[2] = "two";
    cache[3] = "three";
    std::cout << "After insert 3 items: size = " << cache.size() << "\n"; // 3

    assert(cache.size() == 3);

    // Insert 4th item → evicts least recently used (key 1)
    cache[4] = "four";
    std::cout << "After insert 4th item: size = " << cache.size() << "\n"; // 3
    assert(cache.size() == 3);
    assert(!cache.contains(1)); // evicted
    assert(cache.contains(4));

    // Access key 2 (makes it recently used)
    (void)cache[2];
    cache[5] = "five"; // evicts key 3, not key 2
    assert(cache.contains(2));
    assert(!cache.contains(3));

    // Remove_if by predicate
    cache.erase_if([](const auto& kv) { return kv.second == "two"; });
    std::cout << "After erase_if: size = " << cache.size() << "\n"; // 2

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