// bench/ht8_instr_probe.cpp
// Deterministic instruction-count probe for the S7 hot paths (1M int keys).
// Run under callgrind: total retired instructions are layout- and frequency-
// independent, which wall-clock microbenchmarks in WSL are not.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "emhash/hash_table8.hpp"

int main(int argc, char** argv) {
    const size_t N = 1000000;
    std::vector<int> keys(N);
    unsigned st = 12345u;
    for (auto& k : keys) {
        st = st * 1664525u + 1013904223u;
        k = (int)(st & 0x3fffffffu);
    }
    emhash8::HashMap<int, int> m;
    m.reserve(N);
    for (size_t i = 0; i < N; ++i)
        m.emplace(keys[i], (int)i);

    uint64_t s = 0;
    if (argc > 1 && argv[1][0] == 'f') { // find-hit passes
        for (int rep = 0; rep < 3; ++rep)
            for (size_t i = 0; i < N; ++i)
                s += m.find(keys[i]) != m.end();
    } else if (argc > 1 && argv[1][0] == 'e') { // erase passes
        for (int rep = 0; rep < 3; ++rep) {
            for (size_t i = 0; i < N; ++i)
                s += m.erase(keys[i]);
            for (size_t i = 0; i < N; ++i)
                m.emplace(keys[i], (int)i);
        }
    } else { // insert passes (fresh map each pass is not needed: duplicates
             // still traverse the full find_or_allocate path)
        for (int rep = 0; rep < 3; ++rep)
            for (size_t i = 0; i < N; ++i)
                s += m.emplace(keys[i], (int)i).second;
    }
    printf("%llu\n", (unsigned long long)s);
    return 0;
}
