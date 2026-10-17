// bench/research/ht8_cache_probe.cpp
// Cache-miss attribution probe for emhash8's split-index + dense-pairs layout.
//
// Companion to bench/ht8_instr_probe.cpp (retired-instruction count under
// callgrind). This probe runs under cachegrind to answer a different question:
// for a given workload, do the last-level misses come from the `_index[]` array
// or the `_pairs[]` array? The two arrays are touched on distinct source lines
// inside find_filled_slot()/erase_slot(), so cg_annotate can attribute D1mr/DLmr
// per array through line attribution.
//
// Usage:
//   ht8_cache_probe <key> <op> [N]
//     <key> = i | s          int->int, std::string->int
//     <op>  = f | m | e | i  find-hit, find-miss, erase, insert
//
// Run:
//   valgrind --tool=cachegrind --cache-sim=yes --branch-sim=no --I1=32768,8,64 \
//            ./ht8_cache_probe i f 300000
//   cg_annotate --auto=yes cachegrind.out.<pid> | grep -A40 hash_table8
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "emhash/hash_table8.hpp"

template <typename K> struct KeyGen;

// Distinct, deterministic keys. Distribution quality is irrelevant here:
// emhash8 mixes integer keys internally (int_hash), so we only need the raw
// keys to be unique; the hash that drives bucket placement mixes them.
template <> struct KeyGen<int> {
    static int at(size_t i) { return static_cast<int>(i + 1); }
};

template <> struct KeyGen<std::string> {
    static std::string at(size_t i) {
        char buf[24];
        int n = snprintf(buf, sizeof(buf), "key_%zu", i);
        return std::string(buf, static_cast<size_t>(n));
    }
};

template <typename K> static void run(char op, size_t N) {
    std::vector<K> keys;
    keys.reserve(N);
    for (size_t i = 0; i < N; ++i)
        keys.push_back(KeyGen<K>::at(i));

    // Absent keys come from a disjoint index range -> guaranteed distinct strings
    // and no overlap with present keys.
    std::vector<K> absent;
    absent.reserve(N);
    for (size_t i = 0; i < N; ++i)
        absent.push_back(KeyGen<K>::at(i + N + 1000));

    emhash8::HashMap<K, int> m;
    m.reserve(N);
    for (size_t i = 0; i < N; ++i)
        m.emplace(keys[i], static_cast<int>(i));

    uint64_t s = 0;
    switch (op) {
    case 'f':
        for (size_t i = 0; i < N; ++i)
            s += m.find(keys[i]) != m.end();
        break;
    case 'm':
        for (size_t i = 0; i < N; ++i)
            s += m.find(absent[i]) != m.end();
        break;
    case 'e':
        for (size_t i = 0; i < N; ++i)
            s += m.erase(keys[i]);
        break;
    default:
        // fresh map so every emplace is a real insertion path
        {
            emhash8::HashMap<K, int> m2;
            m2.reserve(N);
            for (size_t i = 0; i < N; ++i)
                s += m2.emplace(keys[i], static_cast<int>(i)).second;
        }
        break;
    }
    printf("%llu\n", static_cast<unsigned long long>(s));
}

int main(int argc, char** argv) {
    const size_t N = (argc > 3) ? static_cast<size_t>(strtoul(argv[3], nullptr, 10)) : 300000;
    const char key = (argc > 1) ? argv[1][0] : 'i';
    const char op = (argc > 2) ? argv[2][0] : 'f';
    if (key == 's')
        run<std::string>(op, N);
    else
        run<int>(op, N);
    return 0;
}
