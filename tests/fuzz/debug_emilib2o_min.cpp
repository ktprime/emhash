// Debug emilib2o crash - minimal reproduction
#include <cstdio>
#include <cstdint>
#include <emilib/emilib2o.hpp>

int main() {
    emilib2::HashMap<int, int> em(4, 0.8f);

    // From crash: emplace(-37009, -36865) then emplace(1862336511, -37009)
    printf("Step 1: emplace(-37009, -36865)\n");
    auto r1 = em.emplace(-37009, -36865);
    printf("  result: inserted=%d, size=%u, buckets=%u\n", r1.second, em.size(), em.bucket_count());

    printf("Step 2: emplace(1862336511, -37009)\n");
    auto r2 = em.emplace(1862336511, -37009);
    printf("  result: inserted=%d, size=%u, buckets=%u\n", r2.second, em.size(), em.bucket_count());

    printf("\nVerification:\n");
    printf("  find(-37009): %s\n", em.find(-37009) != em.end() ? "FOUND" : "NOT FOUND");
    printf("  find(1862336511): %s\n", em.find(1862336511) != em.end() ? "FOUND" : "NOT FOUND");
    printf("  count(-37009): %u\n", em.count(-37009));
    printf("  count(1862336511): %u\n", em.count(1862336511));
    printf("  contains(-37009): %d\n", em.contains(-37009));
    printf("  contains(1862336511): %d\n", em.contains(1862336511));

    printf("\nIterate:\n");
    for (auto it = em.begin(); it != em.end(); ++it) {
        printf("  key=%d value=%d\n", it->first, it->second);
    }

    return 0;
}
