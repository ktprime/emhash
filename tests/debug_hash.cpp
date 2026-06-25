#include <string>
#include <cstdio>
#include <functional>
#include "emilib/emihset2.hpp"

int main() {
    printf("=== Hash Collision Analysis ===\n");
    
    std::hash<std::string> hasher;
    
    for (int i = 50; i < 60; i++) {
        auto key = "key_" + std::to_string(i);
        auto hash = hasher(key);
        auto keymask = (uint8_t)(hash >> 24) << 1;  // KEYHASH_MASK
        
        printf("key_%d: hash=%016llx, keymask=%02x\n", i, hash, keymask);
    }
    
    return 0;
}