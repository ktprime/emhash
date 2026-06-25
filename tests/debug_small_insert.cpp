#include <string>
#include <cstdio>
#include "emilib/emihset2.hpp"

int main() {
    printf("=== Small insert test ===\n");
    
    emilib2::HashSet<std::string> s1;
    printf("Initial: bucket_count=%zu (should be 0)\n", s1.bucket_count());
    
    for (int i = 0; i < 10; i++) {
        auto result = s1.insert("key_" + std::to_string(i));
        printf("Insert key_%d: success=%d, size=%zu, buckets=%zu\n", 
               i, result.second, s1.size(), s1.bucket_count());
    }
    
    return 0;
}