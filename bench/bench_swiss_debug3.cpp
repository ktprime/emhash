#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <string>

int main() {
    using Map = emilib4::HashMap<int64_t, int64_t>;

    printf("1. Small insert...\n");
    Map m;
    m[1] = 10; m[2] = 20; m[3] = 30;
    printf("   size=%zu cap=%zu\n", m.size(), m.bucket_count());
    printf("   OK\n");

    printf("2. Growing insert...\n");
    Map m2;
    for (int i = 0; i < 1000; i++) {
        m2[i] = i * 10;
        if (i < 10 || i % 100 == 0)
            printf("   i=%d size=%zu cap=%zu\n", i, m2.size(), m2.bucket_count());
    }
    printf("   OK\n");

    printf("3. Random insert (100)...\n");
    Map m3;
    srand(42);
    for (int i = 0; i < 100; i++) {
        auto k = static_cast<int64_t>(rand());
        m3[k] = k;
    }
    printf("   size=%zu cap=%zu\n", m3.size(), m3.bucket_count());
    printf("   OK\n");

    printf("4. Random insert (1000)...\n");
    Map m4;
    srand(123);
    for (int i = 0; i < 1000; i++) {
        auto k = static_cast<int64_t>(rand()) * rand();
        m4[k] = k;
    }
    printf("   size=%zu cap=%zu\n", m4.size(), m4.bucket_count());
    printf("   OK\n");

    printf("5. Random insert (10000)...\n");
    Map m5;
    srand(456);
    for (int i = 0; i < 10000; i++) {
        auto k = static_cast<int64_t>(rand()) * rand();
        m5[k] = k;
    }
    printf("   size=%zu cap=%zu\n", m5.size(), m5.bucket_count());
    printf("   OK\n");

    printf("6. Random insert (50000)...\n");
    Map m6;
    srand(789);
    for (int i = 0; i < 50000; i++) {
        auto k = static_cast<int64_t>(rand()) * rand();
        m6[k] = k;
        if (i == 49999)
            printf("   last insert done\n");
    }
    printf("   size=%zu cap=%zu\n", m6.size(), m6.bucket_count());
    printf("   OK\n");

    printf("\nAll tests passed!\n");
    return 0;
}
