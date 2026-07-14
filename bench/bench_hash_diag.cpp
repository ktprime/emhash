#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>

int main() {
    // Check hash distribution for sequential keys
    printf("=== Hash distribution for sequential keys ===\n");
    for (int i = 1; i <= 5; i++) {
        auto raw = std::hash<uint64_t>()(i);
        auto mixed = emilib4::mulx_hash(raw);
        printf("  key=%d raw_hash=%llu mixed_hash=%llu\n",
               i, (unsigned long long)raw, (unsigned long long)mixed);
    }

    // Check position distribution for 10M elements
    printf("\n=== Position distribution (10M elements) ===\n");
    const int N = 10000000;
    auto si = emilib4::pow2_size_policy::size_index(N / 15 + 1);
    auto num_groups = emilib4::pow2_size_policy::size(si);
    printf("  num_groups=%zu si=%zu\n", num_groups, si);

    // Count how many keys land in each group (sample first 1000 groups)
    std::vector<int> counts(num_groups, 0);
    for (int i = 1; i <= N; i++) {
        auto raw = std::hash<uint64_t>()(i);
        auto mixed = emilib4::mulx_hash(raw);
        auto pos = emilib4::pow2_size_policy::position(mixed, si);
        if (pos < num_groups) counts[pos]++;
    }

    // Stats
    int max_count = *std::max_element(counts.begin(), counts.end());
    int min_count = *std::min_element(counts.begin(), counts.end());
    double avg = (double)N / num_groups;
    int overflows = 0;
    for (auto c : counts) if (c > 15) overflows++;

    printf("  min=%d max=%d avg=%.2f groups_with_overflow=%d/%zu\n",
           min_count, max_count, avg, overflows, num_groups);

    return 0;
}
