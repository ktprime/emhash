#include "emilib/emihmap4.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>

int main() {
    using KeyInt = uint64_t;
    using ValInt = uint64_t;

    printf("=== Capacity comparison at various sizes ===\n");
    printf("  %8s %12s %12s %12s %12s\n", "N", "emihmap4_cap", "boost_cap", "emihmap4_lf", "boost_lf");

    for (auto N : {1000, 10000, 100000, 1000000, 10000000}) {
        emilib4::HashMap<KeyInt, ValInt> m4;
        m4.reserve(N);
        for (int i = 0; i < N; i++) m4[i] = i;

        boost::unordered_flat_map<KeyInt, ValInt> bm;
        bm.reserve(N);
        for (int i = 0; i < N; i++) bm[i] = i;

        printf("  %8d %12zu %12zu %12.3f %12.3f\n",
               N, m4.bucket_count(), bm.bucket_count(),
               (double)m4.size() / m4.bucket_count(),
               (double)bm.size() / bm.bucket_count());
    }

    return 0;
}
