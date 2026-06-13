// Stress test for emhash5
#include "../../hash_table5.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <unordered_map>
#include <vector>

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { failures++; printf("  FAIL: %s\n", msg); } \
} while(0)

int main()
{
    printf("=== emhash5 stress test ===\n\n");

    // Random insert/erase cycles
    for (int seed = 0; seed < 5; seed++) {
        emhash5::HashMap<int64_t, int> m;
        std::unordered_map<int64_t, int> ref;
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> op(0, 9);
        std::uniform_int_distribution<int64_t> keyd(0, 9999);
        int64_t next_key = 0;

        printf("--- seed=%d ---\n", seed);
        for (int round = 0; round < 5000; round++) {
            int o = op(rng);
            if (o < 7) {  // 70% insert
                int64_t k = next_key++;
                int v = round;
                auto r1 = m.emplace(k, v);
                auto [it2, b2] = ref.emplace(k, v);
                CHECK(r1.second == b2, "insert return mismatch");
                (void)it2;
            } else {  // 30% erase
                int64_t k = keyd(rng);
                auto r1 = m.erase(k);
                auto r2 = ref.erase(k);
                CHECK((size_t)r1 == r2, "erase return mismatch");
            }

            // Verify size every 100 rounds
            if (round % 100 == 99) {
                if ((size_t)m.size() != ref.size()) {
                    printf("  FAIL at round %d: size=%zu ref=%zu\n",
                        round, (size_t)m.size(), ref.size());
                    return 1;
                }
                // Verify all keys
                for (auto& [k, v] : ref) {
                    auto it = m.find(k);
                    if (it == m.end() || it->second != v) {
                        printf("  FAIL at round %d: missing key=%lld\n",
                            round, (long long)k);
                        return 1;
                    }
                }
            }
        }
        CHECK((size_t)m.size() == ref.size(), "final size");
        printf("  size=%zu OK\n", (size_t)m.size());
    }

    // Test with -1 keys (INACTIVE)
    printf("\n--- INACTIVE key stress ---\n");
    emhash5::HashMap<int, int> m;
    for (int i = -1000; i < 1000; i++) m[i] = i * 2;
    for (int i = -1000; i < 1000; i++) {
        auto it = m.find(i);
        CHECK(it != m.end() && it->second == i * 2, "negative key find");
    }
    m.erase(-1);
    CHECK(m.find(-1) == m.end(), "after erase -1");
    CHECK(m.find(-2) != m.end(), "find -2 not affected");
    printf("  INACTIVE key: OK\n");

    printf("\n=== Summary: %d failures ===\n", failures);
    return failures > 0 ? 1 : 0;
}
