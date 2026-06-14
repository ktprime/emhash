// Debug emilib2o - verify offset mismatch between find and insert
#include <cstdio>
#include <cstdint>
#include <emilib/emilib2o.hpp>

int main() {
    emilib2::HashMap<int, int> em(4, 0.8f);

    // Insert two keys that hash to the same bucket
    auto r1 = em.emplace(-37009, -36865);
    printf("Step 1: emplace(-37009, -36865) -> inserted=%d\n", r1.second);

    auto r2 = em.emplace(1862336511, -37009);
    printf("Step 2: emplace(1862336511, -37009) -> inserted=%d\n", r2.second);

    // Check if the two keys hash to the same bucket
    // emilib2o uses hash_key2 which returns h2 and sets main_bucket
    printf("\nHash analysis:\n");
    printf("  hash(-37009) = %zu\n", std::hash<int>()(-37009));
    printf("  hash(1862336511) = %zu\n", std::hash<int>()(1862336511));
    printf("  bucket_count = %u\n", em.bucket_count());

    // The issue: find_filled_bucket uses offset < max_offset
    // but find_or_allocate uses offset <= get_offset(main_bucket)
    // This means find checks one fewer position than insert

    printf("\nFind results:\n");
    printf("  find(-37009): %s\n", em.find(-37009) != em.end() ? "FOUND" : "NOT FOUND");
    printf("  find(1862336511): %s\n", em.find(1862336511) != em.end() ? "FOUND" : "NOT FOUND");

    // Try with operator[] which should also find
    printf("  operator[](1862336511): %d\n", em[1862336511]);

    printf("\nIterate all:\n");
    for (auto it = em.begin(); it != em.end(); ++it) {
        printf("  key=%d value=%d\n", it->first, it->second);
    }

    return 0;
}
