// One-off generator: recomputes the 4 distinct2 checksums under the current
// canonical configuration (RND=123, RT=2 WyRand, seed RND+786512, n=50M).
// The checksum in bench_randomDistinct2 depends only on the RNG stream (sum of
// running multiplicities), so plain counters replicate it exactly.
// Replicates the exact block semantics, including the missing `checksum = 0`
// reset in the 50% block (cumulative there by design/legacy).
#include <cstdint>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include "wyhash.h"

struct WyRandGen {
    uint64_t seed;
    explicit WyRandGen(uint64_t s) : seed(s) {}
    uint64_t operator()() { return wyrand(&seed); }
    uint64_t operator()(uint64_t boundExcluded) noexcept {
        return static_cast<uint64_t>(
            (static_cast<unsigned __int128>(operator()()) *
             static_cast<unsigned __int128>(boundExcluded)) >>
            64u);
    }
};

int main() {
    const uint64_t RND = 123;
    const size_t n = 50000000; // X86_64 build
    WyRandGen rng(RND + 786512);

    int checksum = 0;

    { // 05%: fresh map, checksum = 0
        checksum = 0;
        size_t const max_rng = n / 20;
        std::vector<int> counter(max_rng, 0);
        for (size_t i = 0; i < n; ++i) {
            checksum += ++counter[static_cast<int>(rng(max_rng))];
        }
        printf("05%%:  %d\n", checksum);
    }

    { // 25%: map cleared, checksum = 0
        checksum = 0;
        size_t const max_rng = n / 4;
        std::vector<int> counter(max_rng, 0);
        for (size_t i = 0; i < n; ++i) {
            checksum += ++counter[static_cast<int>(rng(max_rng))];
        }
        printf("25%%:  %d\n", checksum);
    }

    { // 50%: map cleared, checksum NOT reset (continues from 25% block)
        size_t const max_rng = n / 2;
        std::vector<int> counter(max_rng, 0);
        for (size_t i = 0; i < n; ++i) {
            checksum += ++counter[static_cast<int>(rng(max_rng))];
        }
        printf("50%%(cumulative):  %d\n", checksum);
    }

    { // 100%: map cleared, checksum = 0, full-range int keys
        checksum = 0;
        std::unordered_map<int, int> counter;
        counter.reserve(n / 2);
        for (size_t i = 0; i < n; ++i) {
            checksum += ++counter[static_cast<int>(rng())];
        }
        printf("100%%:  %d\n", checksum);
    }
    return 0;
}
