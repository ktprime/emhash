// Collision test with EMH_FIND_HIT=1
#define EMH_FIND_HIT 1
#include "emhash/hash_table5.hpp"
#include <cstdio>
#include <cstdint>

struct const_hasher {
    size_t operator()(int k) const { return 0; }
};

int main()
{
    printf("EMH_FIND_HIT=1 + constant hash\n");

    for (int N = 1; N <= 5000; N += 1) {
        emhash5::HashMap<int, int, const_hasher> m(8);
        for (int i = 0; i < N; i++) m[i] = i;
        for (int i = 0; i < N; i++) {
            auto it = m.find(i);
            if (it == m.end() || it->second != i) {
                printf("  FAIL: N=%d i=%d not found correctly\n", N, i);
                return 1;
            }
        }
    }
    printf("OK up to 5000\n");
    return 0;
}
