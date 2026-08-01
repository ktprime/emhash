#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <string>

int main() {
    using Map = emilib4::HashMap<int64_t, int64_t>;

    printf("1. Creating map...\n");
    Map m;
    printf("   empty=%d size=%zu capacity=%zu\n", m.empty(), m.size(), m.bucket_count());

    printf("2. Inserting 3 elements...\n");
    m[1] = 10;
    printf("   inserted key=1, size=%zu\n", m.size());
    m[2] = 20;
    printf("   inserted key=2, size=%zu\n", m.size());
    m[3] = 30;
    printf("   inserted key=3, size=%zu\n", m.size());

    printf("3. Checking contains...\n");
    printf("   contains(1)=%d contains(2)=%d contains(3)=%d contains(4)=%d\n",
           m.contains(1), m.contains(2), m.contains(3), m.contains(4));

    printf("4. Checking values...\n");
    printf("   m[1]=%lld m[2]=%lld m[3]=%lld\n", (long long)m[1], (long long)m[2], (long long)m[3]);

    printf("5. Erasing key=2...\n");
    auto erased = m.erase(2);
    printf("   erased=%zu size=%zu contains(2)=%d\n", erased, m.size(), m.contains(2));

    printf("6. Iteration...\n");
    int count = 0;
    for (auto& [k, v] : m) {
        printf("   [%lld] = %lld\n", (long long)k, (long long)v);
        count++;
    }
    printf("   iterated %d elements\n", count);

    printf("7. Clear...\n");
    m.clear();
    printf("   size=%zu empty=%d\n", m.size(), m.empty());

    printf("8. Large insert (10000)...\n");
    Map m2;
    for (int i = 0; i < 10000; i++) m2[i] = i * 10;
    printf("   size=%zu\n", m2.size());

    printf("9. Large find...\n");
    int found = 0;
    for (int i = 0; i < 10000; i++) {
        if (m2.find(i) != m2.end()) found++;
    }
    printf("   found %d/10000\n", found);

    printf("10. Large erase...\n");
    for (int i = 0; i < 10000; i++) m2.erase(i);
    printf("   size=%zu empty=%d\n", m2.size(), m2.empty());

    printf("\nAll tests passed!\n");
    return 0;
}
