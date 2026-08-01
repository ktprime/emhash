#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <string>
#include <random>
#include <vector>

int main() {
    using Map = emilib4::HashMap<int64_t, int64_t>;

    printf("1. Build map with 500K random keys...\n");
    Map m6;
    std::mt19937_64 rng(42);
    std::vector<int64_t> keys;
    const int M = 500000;
    for (int i = 0; i < M; i++) {
        auto k = static_cast<int64_t>(rng());
        keys.push_back(k);
        m6[k] = k;
    }
    printf("   size=%zu cap=%zu\n", m6.size(), m6.bucket_count());

    printf("2. Find all keys...\n");
    for (auto k : keys) {
        auto it = m6.find(k);
        if (it == m6.end()) {
            printf("   FAIL: key %lld not found!\n", (long long)k);
            return 1;
        }
        if (it->second != k) {
            printf("   FAIL: key %lld has wrong value %lld!\n", (long long)k, (long long)it->second);
            return 1;
        }
    }
    printf("   All keys found correctly\n");

    printf("3. Erase all keys...\n");
    for (auto k : keys) {
        auto n = m6.erase(k);
        if (n != 1) {
            printf("   FAIL: erase key %lld returned %zu!\n", (long long)k, n);
            return 1;
        }
    }
    printf("   size=%zu empty=%d\n", m6.size(), m6.empty());

    printf("\nAll tests passed!\n");
    return 0;
}
