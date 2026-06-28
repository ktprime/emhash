#include <string>
#include <cstdio>
#include "emilib/emihset2.hpp"

int main() {
    printf("=== Insert-only test ===\n");

    emilib2::HashSet<std::string> s1;
    int success_count = 0;
    for (int i = 0; i < 100; i++) {
        auto result = s1.insert("key_" + std::to_string(i));
        if (result.second) {
            success_count++;
        } else {
            printf("  insert key_%d failed (already exists?)\n", i);
        }
    }
    printf("  size = %zu, success_count = %d\n", s1.size(), success_count);

    // Verify all keys exist
    printf("=== Verify keys ===\n");
    for (int i = 0; i < 100; i++) {
        if (s1.count("key_" + std::to_string(i)) == 0) {
            printf("  key_%d NOT FOUND\n", i);
        }
    }
    printf("  verification done\n");

    return 0;
}
