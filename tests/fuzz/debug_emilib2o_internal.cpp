// Debug emilib2o - inspect internal state
#include <cstdio>
#include <cstdint>
#include <emilib/emilib2o.hpp>

// Hack to access private members
#define private public
#define class struct

// Re-include after #define hack
#undef __EMILIB2O_HPP__
#include "emilib/emilib2o.hpp"

int main() {
    emilib2::HashMap<int, int> em(4, 0.8f);

    auto r1 = em.emplace(-37009, -36865);
    printf("Step 1: emplace(-37009, -36865) -> inserted=%d, size=%u, buckets=%u\n",
           r1.second, em.size(), em.bucket_count());

    // Print states after step 1
    printf("  States after step 1:\n  ");
    for (uint32_t i = 0; i < em.bucket_count() + 16; i++) {
        printf("%3d ", (int)em._states[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\n  Offsets: ");
    for (uint32_t i = 0; i < em.bucket_count() / 4 + 1; i++) {
        printf("%u ", em._offset[i]);
    }
    printf("\n");

    auto r2 = em.emplace(1862336511, -37009);
    printf("\nStep 2: emplace(1862336511, -37009) -> inserted=%d, size=%u, buckets=%u\n",
           r2.second, em.size(), em.bucket_count());

    // Print states after step 2
    printf("  States after step 2:\n  ");
    for (uint32_t i = 0; i < em.bucket_count() + 16; i++) {
        printf("%3d ", (int)em._states[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\n  Offsets: ");
    for (uint32_t i = 0; i < em.bucket_count() / 4 + 1; i++) {
        printf("%u ", em._offset[i]);
    }
    printf("\n");

    // Compute hash info for both keys
    printf("\nHash analysis:\n");
    auto hash1 = em._hasher(-37009);
    auto hash2 = em._hasher(1862336511);
    printf("  hash(-37009) = %zu, main_bucket = %zu, h2 = %d\n",
           (size_t)hash1, (size_t)hash1 & em._mask,
           (int)((size_t)hash1 % 253) - 126);
    printf("  hash(1862336511) = %zu, main_bucket = %zu, h2 = %d\n",
           (size_t)hash2, (size_t)hash2 & em._mask,
           (int)((size_t)hash2 % 253) - 126);

    // Find results
    printf("\nFind results:\n");
    printf("  find(-37009): %s\n", em.find(-37009) != em.end() ? "FOUND" : "NOT FOUND");
    printf("  find(1862336511): %s\n", em.find(1862336511) != em.end() ? "FOUND" : "NOT FOUND");

    printf("\nIterate all:\n");
    for (auto it = em.begin(); it != em.end(); ++it) {
        printf("  bucket=%d key=%d value=%d\n", 0, it->first, it->second);
    }

    return 0;
}
