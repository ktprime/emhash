// Minimal repro for emhash8 HashSet rehash losing keys
#include "emhash/hash_set8.hpp"
#include <cstdio>
#include <unordered_set>

int main() {
    emhash8::HashSet<int> em(2);
    std::unordered_set<int> ref;

    // Reproduce the exact sequence
    em.insert(188908031); ref.insert(188908031);
    fprintf(stderr, "After insert 188908031: size=%u\n", (unsigned)em.size());

    em.emplace(-1280); ref.insert(-1280);
    fprintf(stderr, "After emplace -1280: size=%u\n", (unsigned)em.size());

    em.insert(2114715647); ref.insert(2114715647);
    fprintf(stderr, "After insert 2114715647: size=%u\n", (unsigned)em.size());

    em.emplace(263680); ref.insert(263680);
    fprintf(stderr, "After emplace 263680: size=%u\n", (unsigned)em.size());

    // Verify before reserve
    fprintf(stderr, "Before reserve: contains 263680 = %d\n", em.contains(263680));
    for (const auto& k : ref) {
        if (!em.contains(k))
            fprintf(stderr, "LOST before reserve: %d\n", k);
    }

    // This reserve triggers the bug
    em.reserve(4);
    fprintf(stderr, "After reserve(4): size=%u\n", (unsigned)em.size());

    // Verify after reserve
    for (const auto& k : ref) {
        if (!em.contains(k))
            fprintf(stderr, "LOST after reserve: %d\n", k);
    }

    return 0;
}
