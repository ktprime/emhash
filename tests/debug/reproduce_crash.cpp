// Reproduce the crash with sanitizers to find the root cause
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "emhash/hash_table8.hpp"

int main() {
    printf("=== Test 1: Insert 0x68686868 key (has high bit pattern issue) ===\n");
    {
        emhash8::HashMap<int, int> m;
        (void)m.insert({0x68686868, 1});
        auto it = m.find(0x68686868);
        printf("Find 0x68686868: %s\n", it != m.end() ? "FOUND" : "NOT FOUND");
    }

    printf("\n=== Test 2: reserve(1) + insert 0x68686868 ===\n");
    {
        emhash8::HashMap<int, int> m;
        (void)m.reserve(1);
        (void)m.insert({0x68686868, 1});
        auto it = m.find(0x68686868);
        printf("Find 0x68686868: %s\n", it != m.end() ? "FOUND" : "NOT FOUND");
    }

    printf("\n=== Test 3: Full crash sequence replay ===\n");
    {
        emhash8::HashMap<int, int> em;
        std::unordered_map<int, int> ref;

        // Op 0: ERASE 8447
        (void)em.erase(8447);
        ref.erase(8447);
        // Op 1: CLEAR
        em.clear();
        ref.clear();
        // Op 2: INSERT (131072, 0)
        (void)em.insert({131072, 0});
        ref.insert({131072, 0});
        printf("After op 2: em.size=%u ref.size=%zu\n", em.size(), ref.size());
        // Op 3: FIND 394
        em.find(394);
        ref.find(394);
        // Op 4: RESERVE(1)
        (void)em.reserve(1);
        ref.reserve(1);
        printf("After op 4 reserve(1): em.size=%u ref.size=%zu\n", em.size(), ref.size());
        // Op 5: INSERT (15663104, 13631725)
        (void)em.insert({15663104, 13631725});
        ref.insert({15663104, 13631725});
        printf("After op 5: em.size=%u ref.size=%zu\n", em.size(), ref.size());
        // Op 6: FIND 1744874668
        em.find(1744874668);
        ref.find(1744874668);
        // Op 7-9: ITERATE
        for (int i = 0; i < 3; i++) {
            std::unordered_map<int, int> em_collect;
            for (auto it = em.begin(); it != em.end(); ++it)
                em_collect[it->first] = it->second;
            if (em_collect != ref) {
                printf("BUG at op %d: iteration mismatch!\n", 7 + i);
            }
        }
        printf("After op 7-9: em.size=%u ref.size=%zu\n", em.size(), ref.size());
        // Op 10: COUNT (-805245696)
        em.count(-805245696);
        ref.count(-805245696);
        // Op 11: INSERT (1751672936, 1751672936) <-- This is where it crashes
        printf("About to do op 11 INSERT (1751672936, 1751672936)...\n");
        fflush(stdout);
        (void)em.insert({1751672936, 1751672936});
        printf("Op 11 succeeded!\n");
    }

    return 0;
}
