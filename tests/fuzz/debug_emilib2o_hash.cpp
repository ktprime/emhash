// Debug emilib2o - add printf to find_filled_bucket
#include <cstdio>
#include <cstdint>
#include <emilib/emilib2o.hpp>

// We'll add debug output by wrapping find
int main() {
    emilib2::HashMap<int, int> em(4, 0.8f);

    auto r1 = em.emplace(-37009, -36865);
    printf("Step 1: emplace(-37009, -36865) -> inserted=%d, size=%u, buckets=%u\n",
           r1.second, em.size(), em.bucket_count());

    auto r2 = em.emplace(1862336511, -37009);
    printf("Step 2: emplace(1862336511, -37009) -> inserted=%d, size=%u, buckets=%u\n",
           r2.second, em.size(), em.bucket_count());

    // Compute hash info
    printf("\nHash analysis:\n");
    auto hash1 = std::hash<int>()(-37009);
    auto hash2 = std::hash<int>()(1862336511);
    printf("  hash(-37009) = %zu\n", hash1);
    printf("  hash(1862336511) = %zu\n", hash2);
    printf("  bucket_count = %u\n", em.bucket_count());
    printf("  -37009 main_bucket = %zu\n", hash1 & (em.bucket_count() - 1));
    printf("  1862336511 main_bucket = %zu\n", hash2 & (em.bucket_count() - 1));

    // h2 tags
    int8_t h2_1 = (int8_t)((size_t)hash1 % 253) - 126;
    int8_t h2_2 = (int8_t)((size_t)hash2 % 253) - 126;
    printf("  -37009 h2 = %d\n", h2_1);
    printf("  1862336511 h2 = %d\n", h2_2);

    // Are h2 tags the same? If so, find_filled_bucket should find both in same group
    printf("  h2 match: %s\n", h2_1 == h2_2 ? "YES" : "NO");

    // Find results
    printf("\nFind results:\n");
    printf("  find(-37009): %s\n", em.find(-37009) != em.end() ? "FOUND" : "NOT FOUND");
    printf("  find(1862336511): %s\n", em.find(1862336511) != em.end() ? "FOUND" : "NOT FOUND");

    // Try contains
    printf("  contains(-37009): %d\n", em.contains(-37009));
    printf("  contains(1862336511): %d\n", em.contains(1862336511));

    // Try count
    printf("  count(-37009): %u\n", em.count(-37009));
    printf("  count(1862336511): %u\n", em.count(1862336511));

    printf("\nIterate all:\n");
    for (auto it = em.begin(); it != em.end(); ++it) {
        printf("  key=%d value=%d\n", it->first, it->second);
    }

    return 0;
}
