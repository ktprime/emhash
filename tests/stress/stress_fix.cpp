// Stress test for the crash path: reserve(1) + kickout
// Compile: g++ -fsanitize=address,undefined -std=c++17 -g stress_fix.cpp -o stress_fix
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include "../../hash_table8.hpp"

int main() {
    printf("=== Stress test: reserve(1) + random operations ===\n");
    
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> key_dist(-100000000, 100000000);
    
    for (int trial = 0; trial < 1000; trial++) {
        if (trial % 100 == 0) { printf("  trial %d...\n", trial); fflush(stdout); }
        
        emhash8::HashMap<int, int> m;
        m.reserve(1);  // Force tiny table
        
        for (int i = 0; i < 20; i++) {
            int key = key_dist(rng);
            int val = key_dist(rng);
            m.insert({key, val});
        }
        
        // Verify all inserted keys are findable via iteration
        for (auto it = m.begin(); it != m.end(); ++it) {
            auto found = m.find(it->first);
            if (found == m.end()) {
                printf("BUG trial %d: key %d not found via iteration\n", trial, it->first);
                return 1;
            }
        }
    }
    
    printf("PASS: 1000 trials with reserve(1) + 20 inserts each\n");
    
    // Test 2: stress the exact full crash sequence
    printf("\n=== Stress test: crash sequence with random variations ===\n");
    for (int trial = 0; trial < 10000; trial++) {
        emhash8::HashMap<int, int> m;
        m.reserve(1);
        
        // Insert 2 random keys
        int k1 = key_dist(rng), v1 = key_dist(rng);
        int k2 = key_dist(rng), v2 = key_dist(rng);
        m.insert({k1, v1});
        m.insert({k2, v2});
        
        // Erase random key
        m.erase(key_dist(rng));
        
        // Iterate
        for (auto it = m.begin(); it != m.end(); ++it) (void)it->first;
        
        // Count random key
        m.count(key_dist(rng));
        
        // Insert 3rd key (the crash trigger in fuzzer)
        int k3 = key_dist(rng), v3 = key_dist(rng);
        m.insert({k3, v3});
        
        // Verify
        for (auto it = m.begin(); it != m.end(); ++it) {
            if (m.find(it->first) == m.end()) {
                printf("BUG trial %d: key %d not found\n", trial, it->first);
                return 1;
            }
        }
    }
    printf("PASS: 10000 trials matching crash pattern\n");
    
    return 0;
}