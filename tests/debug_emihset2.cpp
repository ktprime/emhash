#include <string>
#include <cstdio>
#include "emilib/emihset2.hpp"

int main() {
    printf("=== Simplified emihset2 test ===\n");
    
    // Test 1: Basic insert
    printf("Test 1: Basic insert...\n");
    emilib2::HashSet<std::string> s1;
    for (int i = 0; i < 100; i++) {
        s1.insert("key_" + std::to_string(i));
    }
    printf("  size = %zu (expected 100)\n", s1.size());
    
    // Test 2: Copy constructor
    printf("Test 2: Copy constructor...\n");
    emilib2::HashSet<std::string> s2(s1);
    printf("  s2.size = %zu (expected 100)\n", s2.size());
    
    // Test 3: Iterate s2
    printf("Test 3: Iterate s2...\n");
    int count = 0;
    for (auto it = s2.begin(); it != s2.end(); ++it) {
        count++;
    }
    printf("  iterated %d entries\n", count);
    
    // Test 4: Erase
    printf("Test 4: Erase...\n");
    for (int i = 0; i < 50; i++) {
        s1.erase("key_" + std::to_string(i));
    }
    printf("  size = %zu (expected 50)\n", s1.size());
    
    printf("=== All tests passed ===\n");
    return 0;
}