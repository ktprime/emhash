// Debug emilib2o - add debug output directly to find_filled_bucket
// We temporarily modify the header to add printf

#include <cstdio>
#include <cstdint>
#include <cstring>

// Include with debug modifications
#define EMH_DEBUG_FIND 1

#include "emilib/emilib2o.hpp"

int main() {
    emilib2::HashMap<int, int> em(4, 0.8f);

    printf("=== Step 1: emplace(-37009, -36865) ===\n");
    auto r1 = em.emplace(-37009, -36865);
    printf("  inserted=%d, size=%u, buckets=%u\n", r1.second, em.size(), em.bucket_count());

    printf("\n=== Step 2: emplace(1862336511, -37009) ===\n");
    auto r2 = em.emplace(1862336511, -37009);
    printf("  inserted=%d, size=%u, buckets=%u\n", r2.second, em.size(), em.bucket_count());

    printf("\n=== Find -37009 ===\n");
    auto f1 = em.find(-37009);
    printf("  result: %s\n", f1 != em.end() ? "FOUND" : "NOT FOUND");

    printf("\n=== Find 1862336511 ===\n");
    auto f2 = em.find(1862336511);
    printf("  result: %s\n", f2 != em.end() ? "FOUND" : "NOT FOUND");

    return 0;
}
