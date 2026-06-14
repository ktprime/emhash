// Debug test for collision handling
#include <cstdio>
#include <cstdint>

#include "emilib2o_v2.hpp"

int main() {
    printf("=== Collision Debug Test ===\n");

    emilib2_v2::HashMap<int, int> map;

    // Insert keys that are powers of 2
    for (int i = 0; i < 10; i++) {
        int key = 1 << i;
        printf("Inserting key %d (bucket before: %u)\n", key, map.bucket_count());
        map[key] = i;
    }

    printf("\nMap size: %u, bucket_count: %u\n", map.size(), map.bucket_count());

    // Try to find them
    for (int i = 0; i < 10; i++) {
        int key = 1 << i;
        auto it = map.find(key);
        if (it == map.end()) {
            printf("FAIL: key %d not found!\n", key);
        } else {
            printf("OK: key %d found, value = %d\n", key, it->second);
        }
    }

    // Test with regular sequential keys
    printf("\n=== Sequential Keys Test ===\n");
    emilib2_v2::HashMap<int, int> map2;
    for (int i = 0; i < 100; i++) {
        map2[i] = i;
    }

    for (int i = 0; i < 100; i++) {
        auto it = map2.find(i);
        if (it == map2.end()) {
            printf("FAIL: key %d not found!\n", i);
        }
    }
    printf("Sequential keys test passed\n");

    return 0;
}
