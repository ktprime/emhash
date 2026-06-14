// Test probe coverage: verify get_next_bucket visits ALL groups for all table sizes
// The bug: fixed step with GCD(step, num_buckets) > 1 causes probe sequence to cycle
// without visiting all groups, making find_empty_slot loop forever.

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <set>

// Simulate emilib2ss/emilib2s get_next_bucket (aligned loads, step must be multiple of simd_bytes)
size_t get_next_bucket_aligned(size_t next_bucket, size_t offset, size_t num_buckets, size_t simd_bytes)
{
    if (offset < 7)
        next_bucket += simd_bytes * offset;
    else {
        next_bucket += (((num_buckets / 8 + simd_bytes) / simd_bytes) | 1) * simd_bytes;
    }
    return next_bucket & (num_buckets - 1);
}

// Simulate emilib2o get_next_bucket (unaligned loads, step can be any value)
size_t get_next_bucket_unaligned(size_t next_bucket, size_t offset, size_t num_buckets, size_t simd_bytes)
{
    if (offset < 5)
        next_bucket += simd_bytes * offset;
    else {
        next_bucket += (num_buckets / 11 + 1) | 1;
    }
    return next_bucket & (num_buckets - 1);
}

// Test probe coverage for aligned implementation (emilib2ss/emilib2s)
bool test_aligned_coverage(size_t num_buckets, size_t simd_bytes = 16)
{
    size_t num_groups = num_buckets / simd_bytes;
    size_t max_steps = num_groups * 2; // should need at most num_groups steps

    // Test from multiple starting positions
    bool all_ok = true;
    for (size_t start = 0; start < num_buckets; start += simd_bytes) {
        std::set<size_t> visited_groups;
        size_t next_bucket = start;
        size_t offset = 0;

        visited_groups.insert(next_bucket / simd_bytes);

        for (size_t step = 0; step < max_steps; step++) {
            offset++;
            next_bucket = get_next_bucket_aligned(next_bucket, offset, num_buckets, simd_bytes);
            visited_groups.insert(next_bucket / simd_bytes);
        }

        if (visited_groups.size() != num_groups) {
            printf("  FAIL: nb=%zu start=%zu covered %zu/%zu groups (%.1f%%)\n",
                   num_buckets, start, visited_groups.size(), num_groups,
                   100.0 * visited_groups.size() / num_groups);
            all_ok = false;
            break; // one failure is enough
        }
    }
    return all_ok;
}

// Test probe coverage for unaligned implementation (emilib2o)
bool test_unaligned_coverage(size_t num_buckets, size_t simd_bytes = 16)
{
    size_t max_steps = num_buckets * 2;

    bool all_ok = true;
    for (size_t start = 0; start < num_buckets; start += simd_bytes) {
        std::set<size_t> visited;
        size_t next_bucket = start;
        size_t offset = 0;

        visited.insert(next_bucket);

        for (size_t step = 0; step < max_steps; step++) {
            offset++;
            next_bucket = get_next_bucket_unaligned(next_bucket, offset, num_buckets, simd_bytes);
            visited.insert(next_bucket);
        }

        if (visited.size() != num_buckets) {
            printf("  FAIL: nb=%zu start=%zu covered %zu/%zu positions (%.1f%%)\n",
                   num_buckets, start, visited.size(), num_buckets,
                   100.0 * visited.size() / num_buckets);
            all_ok = false;
            break;
        }
    }
    return all_ok;
}

// Test the OLD buggy implementation for comparison
size_t get_next_bucket_old_aligned(size_t next_bucket, size_t offset, size_t num_buckets, size_t simd_bytes)
{
    if (offset < 7)
        next_bucket += simd_bytes * offset;
    else if (offset < 14)
        next_bucket += (num_buckets / 7 + simd_bytes * 3) & ~(simd_bytes - 1);
    else
        next_bucket += num_buckets / 8 + simd_bytes;
    return next_bucket & (num_buckets - 1);
}

size_t get_next_bucket_old_unaligned(size_t next_bucket, size_t offset, size_t num_buckets, size_t simd_bytes)
{
    if (offset < 5)
        next_bucket += simd_bytes * offset;
    else if (offset < 11)
        next_bucket += num_buckets / 11 + 1;
    else
        next_bucket += num_buckets / 7 + simd_bytes;
    return next_bucket & (num_buckets - 1);
}

bool test_old_aligned_coverage(size_t num_buckets, size_t simd_bytes = 16)
{
    size_t num_groups = num_buckets / simd_bytes;
    size_t max_steps = num_groups * 2;

    for (size_t start = 0; start < num_buckets; start += simd_bytes) {
        std::set<size_t> visited_groups;
        size_t next_bucket = start;
        size_t offset = 0;

        visited_groups.insert(next_bucket / simd_bytes);

        for (size_t step = 0; step < max_steps; step++) {
            offset++;
            next_bucket = get_next_bucket_old_aligned(next_bucket, offset, num_buckets, simd_bytes);
            visited_groups.insert(next_bucket / simd_bytes);
        }

        if (visited_groups.size() != num_groups) {
            return false; // bug found
        }
    }
    return true;
}

bool test_old_unaligned_coverage(size_t num_buckets, size_t simd_bytes = 16)
{
    size_t max_steps = num_buckets * 2;

    for (size_t start = 0; start < num_buckets; start += simd_bytes) {
        std::set<size_t> visited;
        size_t next_bucket = start;
        size_t offset = 0;

        visited.insert(next_bucket);

        for (size_t step = 0; step < max_steps; step++) {
            offset++;
            next_bucket = get_next_bucket_old_unaligned(next_bucket, offset, num_buckets, simd_bytes);
            visited.insert(next_bucket);
        }

        if (visited.size() != num_buckets) {
            return false;
        }
    }
    return true;
}

int main()
{
    int pass = 0, fail = 0;

    printf("=== Testing NEW (fixed) aligned probe coverage (emilib2ss/emilib2s) ===\n");
    for (size_t nb = 64; nb <= 65536; nb *= 2) {
        if (test_aligned_coverage(nb))
            { printf("  nb=%-6zu PASS (all %zu groups reachable)\n", nb, nb/16); pass++; }
        else
            { fail++; }
    }

    printf("\n=== Testing NEW (fixed) unaligned probe coverage (emilib2o) ===\n");
    for (size_t nb = 64; nb <= 65536; nb *= 2) {
        if (test_unaligned_coverage(nb))
            { printf("  nb=%-6zu PASS (all %zu positions reachable)\n", nb, nb); pass++; }
        else
            { fail++; }
    }

    printf("\n=== Testing OLD (buggy) aligned probe coverage for comparison ===\n");
    for (size_t nb = 64; nb <= 65536; nb *= 2) {
        bool ok = test_old_aligned_coverage(nb);
        printf("  nb=%-6zu %s\n", nb, ok ? "PASS" : "FAIL (bug: not all groups reachable)");
    }

    printf("\n=== Testing OLD (buggy) unaligned probe coverage for comparison ===\n");
    for (size_t nb = 64; nb <= 65536; nb *= 2) {
        bool ok = test_old_unaligned_coverage(nb);
        printf("  nb=%-6zu %s\n", nb, ok ? "PASS" : "FAIL (bug: not all positions reachable)");
    }

    printf("\n=== GCD analysis for fixed steps ===\n");
    printf("  Aligned: step = ((nb/8+16)/16|1)*16\n");
    for (size_t nb = 64; nb <= 65536; nb *= 2) {
        size_t step = (((nb / 8 + 16) / 16) | 1) * 16;
        size_t ng = nb / 16;
        size_t step_g = step / 16;
        // compute GCD
        size_t a = step_g, b = ng;
        while (b) { size_t t = b; b = a % b; a = t; }
        printf("  nb=%-6zu ng=%-5zu step=%-5zu step_g=%-3zu GCD(step_g,ng)=%zu %s\n",
               nb, ng, step, step_g, a, a == 1 ? "OK" : "BUG");
    }

    printf("\n  Unaligned: step = (nb/11+1)|1\n");
    for (size_t nb = 64; nb <= 65536; nb *= 2) {
        size_t step = (nb / 11 + 1) | 1;
        // compute GCD
        size_t a = step, b = nb;
        while (b) { size_t t = b; b = a % b; a = t; }
        printf("  nb=%-6zu step=%-5zu GCD(step,nb)=%zu %s\n",
               nb, step, a, a == 1 ? "OK" : "BUG");
    }

    printf("\n=== Summary: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
