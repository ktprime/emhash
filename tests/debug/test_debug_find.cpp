// Test find for various buckets
#define EMH_FIND_HIT 1
#include "emhash/hash_table5.hpp"
#include <cstdio>
#include <cstdint>

int main()
{
    emhash5::HashMap<int32_t, int32_t> m(16);
    printf("--- find(-1) and find(-1, b) tests ---\n");
    fflush(stdout);

    auto it1 = m.find(-1);
    printf("find(-1) = %s\n", it1 == m.end() ? "end" : "FOUND");
    fflush(stdout);

    for (size_t b = 0; b < 16; b++) {
        auto it = m.find(-1, b);
        printf("find(-1, %2zu) = %s\n", b, it == m.end() ? "end" : "FOUND");
    }
    fflush(stdout);
    return 0;
}
