#include <string>
#include <cstdio>
#include "emilib/emihset2.hpp"

int main() {
    printf("=== Key 55 Debug Test ===\n");

    emilib2::HashSet<std::string> s1;
    for (int i = 0; i < 56; i++) {
        auto key = "key_" + std::to_string(i);
        auto result = s1.insert(key);
        if (i == 55) {
            printf("Insert key_55: success=%d, size=%zu\n", result.second, s1.size());
            // Check if bucket is valid
            if (!result.second) {
                printf("  bucket=%zu, bucket_count=%zu\n",
                       result.first.bucket(), s1.bucket_count());
            }
        }
    }

    // Check if key_55 exists before and after
    printf("Before insert: count(key_55)=%zu\n", s1.count("key_55"));

    // Try insert key_55 again
    auto result2 = s1.insert("key_55");
    printf("Insert key_55 again: success=%d\n", result2.second);

    printf("size=%zu\n", s1.size());

    return 0;
}
