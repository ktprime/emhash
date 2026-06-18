// Test cases to trigger hidden bugs in emhash8
// Compile: g++ -std=c++17 -O2 -I.. test_hidden_bugs8.cpp -o test_hidden_bugs8
#include <cstdio>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <chrono>
#include "emhash/hash_table8.hpp"

// =============================================================================
// Test 1: Hash high-bit collision causing false positive match (EMH_EQHASH bug)
// =============================================================================
// EMH_EQHASH only compares hash high bits: ((key_hash) & ~_mask) == (_index[n].slot & ~_mask)
// If two different keys have same bucket (low bits) and same high bits,
// EMH_EQHASH returns true and triggers _eq comparison.
// We construct keys where hash & ~_mask collides but keys are different.

struct CollisionHasher {
    size_t collision_high_bits;
    size_t operator()(int /*x*/) const {
        // Force all keys to have the same high bits after masking
        // bucket = hash & _mask, high = hash & ~_mask
        // We want different keys to map to same bucket AND same high bits
        // Return: collision_high_bits | (x & _mask)  -- but _mask is runtime known
        // Instead, we use a simpler approach: return same value for all keys
        // This makes all keys collide to bucket 0, and high bits are also same
        return collision_high_bits;
    }
};

int test_hash_collision() {
    printf("\n=== Test 1: Hash high-bit collision ===\n");

    // Use a hasher that returns constant hash for all keys
    // This forces maximum collision and tests EMH_EQHASH correctness
    struct ConstHash { size_t operator()(int) const { return 0xABCD1234; } };

    emhash8::HashMap<int, int, ConstHash> m;

    // Insert many keys with same hash
    for (int i = 0; i < 100; i++) {
        m.insert_unique(i, i * 10);
    }

    // Verify all keys are correctly retrievable
    bool pass = true;
    for (int i = 0; i < 100; i++) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i * 10) {
            printf("FAIL: key %d not found or wrong value\n", i);
            pass = false;
        }
    }

    // Verify no false positives - keys not inserted should not be found
    for (int i = 1000; i < 1100; i++) {
        if (m.find(i) != m.end()) {
            printf("FAIL: key %d falsely found (false positive)\n", i);
            pass = false;
        }
    }

    // Test kickout with collision: insert key that causes kickout
    // then verify chain integrity
    m.clear();
    for (int i = 0; i < 50; i++) {
        m[i] = i;  // operator[] with kickout
    }
    for (int i = 0; i < 50; i++) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i) {
            printf("FAIL after kickout: key %d not found\n", i);
            pass = false;
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

// =============================================================================
// Test 2: kickout_bucket chain break / cycle detection
// =============================================================================
// When kickout_bucket is called, it moves a node from main bucket to empty bucket.
// If there's a bug in relinking, the chain may break or form a cycle.
// We detect cycles by limiting traversal steps.

int test_kickout_chain_integrity() {
    printf("\n=== Test 2: kickout_bucket chain integrity ===\n");

    // Custom hasher to force controlled collisions
    struct ControlledHash {
        size_t operator()(int x) const {
            // Keys 0, 1, 2, ... will have hashes that collide on low bits
            // but we want them to map to same main bucket
            return (size_t)(x * 7);  // Linear, many will collide for small table
        }
    };

    emhash8::HashMap<int, int, ControlledHash> m;

    // Pre-reserve to control table size
    m.reserve(16);

    // Insert keys that will definitely collide and trigger kickout
    std::vector<int> keys;
    for (int i = 0; i < 64; i++) {
        keys.push_back(i * 16);  // Spread values but hash may collide
    }

    for (int k : keys) {
        m.insert_unique(k, k * 2);
    }

    // Verify all keys accessible
    bool pass = true;
    for (int k : keys) {
        auto it = m.find(k);
        if (it == m.end()) {
            printf("FAIL: key %d lost after insertions\n", k);
            pass = false;
        } else if (it->second != k * 2) {
            printf("FAIL: key %d has wrong value %d (expected %d)\n", k, it->second, k * 2);
            pass = false;
        }
    }

    // Now erase some keys from middle of chains and verify integrity
    for (size_t i = keys.size() / 4; i < keys.size() / 2; i++) {
        m.erase(keys[i]);
    }

    // Verify remaining keys still accessible
    for (size_t i = 0; i < keys.size(); i++) {
        if (i >= keys.size() / 4 && i < keys.size() / 2) {
            // These were erased
            if (m.find(keys[i]) != m.end()) {
                printf("FAIL: erased key %d still found\n", keys[i]);
                pass = false;
            }
        } else {
            auto it = m.find(keys[i]);
            if (it == m.end()) {
                printf("FAIL: key %d lost after erase\n", keys[i]);
                pass = false;
            } else if (it->second != keys[i] * 2) {
                printf("FAIL: key %d wrong value after erase\n", keys[i]);
                pass = false;
            }
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

// =============================================================================
// Test 3: erase_slot slot_to_bucket inconsistency
// =============================================================================
// erase_slot uses _etail to optimize finding the bucket for last_slot.
// If _etail is stale (points to already-erased bucket), slot_to_bucket
// is called which recomputes hash and traverses chain.
// We try to create a scenario where _etail becomes stale.

int test_etail_staleness() {
    printf("\n=== Test 3: _etail staleness ===\n");

    struct ConstHash { size_t operator()(int) const { return 0; } };
    emhash8::HashMap<int, int, ConstHash> m;

    // Insert multiple keys with same hash (all collide in bucket 0)
    for (int i = 0; i < 20; i++) {
        m.insert_unique(i, i * 100);
    }

    // Erase from the main bucket repeatedly
    // This should trigger _etail updates and potentially staleness
    bool pass = true;

    // Erase first element - this sets _etail = INACTIVE
    auto it = m.begin();
    int first_key = it->first;
    (void)first_key;
    m.erase(it);

    // Now the last element's bucket should be tracked
    // Insert more to trigger _etail usage
    for (int i = 100; i < 110; i++) {
        m.insert_unique(i, i);
    }

    // Erase the last inserted element using iterator
    // This is where _etail optimization kicks in
    auto last_it = m.last();
    int last_key = last_it->first;
    m.erase(last_it);

    // Verify the erased key is gone
    if (m.find(last_key) != m.end()) {
        printf("FAIL: last erased key %d still found\n", last_key);
        pass = false;
    }

    // Verify all remaining keys accessible
    for (auto it2 = m.begin(); it2 != m.end(); ++it2) {
        auto found = m.find(it2->first);
        if (found == m.end()) {
            printf("FAIL: remaining key %d lost\n", it2->first);
            pass = false;
        }
    }

    // Stress test: alternating erase and insert
    for (int round = 0; round < 10; round++) {
        // Erase half
        std::vector<int> to_erase;
        for (auto it3 = m.begin(); it3 != m.end(); ++it3) {
            if (it3->first % 2 == 0) to_erase.push_back(it3->first);
        }
        for (int k : to_erase) m.erase(k);

        // Insert new
        for (int i = 200 + round * 10; i < 200 + round * 10 + 5; i++) {
            m.insert_unique(i, i);
        }

        // Verify
        for (auto it3 = m.begin(); it3 != m.end(); ++it3) {
            auto found = m.find(it3->first);
            if (found == m.end()) {
                printf("FAIL: round %d key %d lost\n", round, it3->first);
                pass = false;
            }
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

// =============================================================================
// Test 4: value_type non-const KeyT allows key mutation
// =============================================================================
// emhash8 uses std::pair<KeyT, ValueT> instead of std::pair<const KeyT, ValueT>
// This allows modifying the key through iterator, breaking hash invariant.

int test_key_mutation() {
    printf("\n=== Test 4: Key mutation via non-const iterator ===\n");

    emhash8::HashMap<int, int> m;
    for (int i = 0; i < 10; i++) {
        m.insert_unique(i, i * 10);
    }

    // Modify key through iterator (this should NOT be allowed)
    auto it = m.find(5);
    if (it != m.end()) {
        // This compiles because KeyT is not const!
        int old_key = it->first;
        (void)old_key;
        const_cast<int&>(it->first) = 999;

        // Now the table is corrupted: key 999 is in bucket for key 5
        // Try to find original key 5 - should fail
        bool pass = true;

        if (m.find(5) != m.end()) {
            printf("UNEXPECTED: key 5 still found after mutation\n");
        }

        // Try to find mutated key 999 - may or may not work depending on hash
        auto it2 = m.find(999);
        if (it2 == m.end()) {
            printf("FAIL: mutated key 999 not found (table corrupted)\n");
            pass = false;
        } else if (it2->second != 50) {
            printf("FAIL: mutated key has wrong value %d\n", it2->second);
            pass = false;
        }

        // The real issue: iterating may show inconsistent state
        int count = 0;
        for (auto it3 = m.begin(); it3 != m.end(); ++it3) {
            count++;
        }
        if (count != 10) {
            printf("FAIL: iteration count %d != 10 after mutation\n", count);
            pass = false;
        }

        printf("%s (key mutation was possible - this is the bug)\n", pass ? "PASS" : "FAIL");
        return pass ? 0 : 1;
    }

    printf("SKIP: could not find key 5\n");
    return 0;
}

// =============================================================================
// Test 5: find_empty_bucket near-full deadlock detection
// =============================================================================
// When table is nearly full, find_empty_bucket may loop forever.
// We detect this with a timeout.

int test_near_full_deadlock() {
    printf("\n=== Test 5: Near-full table deadlock detection ===\n");

    struct ConstHash { size_t operator()(int) const { return 0; } };
    emhash8::HashMap<int, int, ConstHash> m;

    // Reserve small space and fill to near capacity
    m.reserve(32);

    bool pass = true;
    auto start = std::chrono::steady_clock::now();

    try {
        // Try to insert more than capacity to force extreme probing
        for (int i = 0; i < 100; i++) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            if (elapsed > 5) {
                printf("TIMEOUT: Deadlock detected after %d insertions\n", i);
                pass = false;
                break;
            }
            m.insert_unique(i * 7, i);  // Different values, same hash
        }
    } catch (...) {
        printf("EXCEPTION caught during near-full insertion\n");
        pass = false;
    }

    if (pass) {
        printf("PASS (no deadlock detected within time limit)\n");
    }
    return pass ? 0 : 1;
}

// =============================================================================
// Test 6: EMH_PACK_TAIL memory safety
// =============================================================================
// When EMH_PACK_TAIL is defined, num_buckets is expanded beyond _mask.
// Verify no out-of-bounds access.

#ifdef EMH_PACK_TAIL
int test_pack_tail_safety() {
    printf("\n=== Test 6: EMH_PACK_TAIL memory safety ===\n");

    emhash8::HashMap<int, int> m;
    m.reserve(64);

    for (int i = 0; i < 50; i++) {
        m.insert_unique(i, i);
    }

    bool pass = true;
    for (int i = 0; i < 50; i++) {
        if (m.find(i) == m.end()) {
            printf("FAIL: key %d lost with EMH_PACK_TAIL\n", i);
            pass = false;
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
#else
int test_pack_tail_safety() {
    printf("\n=== Test 6: EMH_PACK_TAIL memory safety ===\n");
    printf("SKIP: EMH_PACK_TAIL not defined\n");
    return 0;
}
#endif

// =============================================================================
// Test 7: Prefetch nullptr / out-of-bounds
// =============================================================================
// find_or_allocate prefetches _pairs[bucket] before checking if bucket is valid.
// With empty map or after clear, this could prefetch invalid memory.

int test_prefetch_safety() {
    printf("\n=== Test 7: Prefetch safety on empty/edge cases ===\n");

    bool pass = true;

    // Test 1: Insert into freshly constructed small map
    {
        emhash8::HashMap<int, int> m;
        m.insert_unique(42, 1);  // This triggers find_or_allocate
        if (m.find(42) == m.end()) {
            printf("FAIL: first insert lost\n");
            pass = false;
        }
    }

    // Test 2: Insert after clear
    {
        emhash8::HashMap<int, int> m;
        m.insert_unique(1, 1);
        m.clear();
        m.insert_unique(2, 2);  // _pairs may be null or reallocated
        if (m.find(2) == m.end()) {
            printf("FAIL: insert after clear lost\n");
            pass = false;
        }
    }

    // Test 3: operator[] on empty map
    {
        emhash8::HashMap<int, int> m;
        m[100] = 100;  // Should trigger rehash + insert
        if (m.find(100) == m.end() || m[100] != 100) {
            printf("FAIL: operator[] on empty map failed\n");
            pass = false;
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

// =============================================================================
// Main
// =============================================================================
int main() {
    int rc = 0;

    printf("Starting hidden bug tests for emhash8...\n");
    printf("=======================================\n");

    rc |= test_hash_collision();
    rc |= test_kickout_chain_integrity();
    rc |= test_etail_staleness();
    rc |= test_key_mutation();
    rc |= test_near_full_deadlock();
    rc |= test_pack_tail_safety();
    rc |= test_prefetch_safety();

    printf("\n=======================================\n");
    printf("Final result: %s\n", rc == 0 ? "ALL PASSED" : "SOME FAILED");
    return rc;
}
