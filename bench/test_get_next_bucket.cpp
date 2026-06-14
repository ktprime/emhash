// Test: Verify get_next_bucket coverage and find_empty_slot infinite loop risk
// This test simulates the probe sequence to check if all buckets can be covered

#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>

// Simulate emilib2ss/emilib2s get_next_bucket (potentially problematic)
uint32_t get_next_bucket_ss(uint32_t next_bucket, uint32_t offset, uint32_t num_buckets, uint32_t simd_bytes) {
    if (offset < 7)
        next_bucket += simd_bytes * offset;
    else {
        // This is the problematic formula
        next_bucket += (((num_buckets / 8 + simd_bytes) / simd_bytes) | 1) * simd_bytes;
    }
    return next_bucket & (num_buckets - 1);
}

// Simulate emilib2o get_next_bucket (fixed version)
uint32_t get_next_bucket_o(uint32_t next_bucket, uint32_t offset, uint32_t num_buckets, uint32_t simd_bytes) {
    if (offset < 5)
        next_bucket += simd_bytes * offset;
    else {
        // Odd step: coprime with power-of-2 num_buckets
        next_bucket += (num_buckets / 11 + 1) | 1;
    }
    return next_bucket & (num_buckets - 1);
}

// Test coverage: how many unique buckets are visited in N iterations?
void test_coverage(const char* name, uint32_t num_buckets, uint32_t simd_bytes,
                   uint32_t (*get_next)(uint32_t, uint32_t, uint32_t, uint32_t)) {
    printf("\n=== Testing %s (num_buckets=%u, simd_bytes=%u) ===\n", name, num_buckets, simd_bytes);

    // Test from different starting positions
    for (uint32_t start = 0; start < num_buckets; start += simd_bytes) {
        std::vector<bool> visited(num_buckets, false);
        uint32_t next_bucket = start;
        uint32_t offset = 0;

        // Simulate 1000 iterations (should be enough to cover all if algorithm is correct)
        for (int i = 0; i < 1000 && offset < 1000; i++) {
            visited[next_bucket] = true;
            next_bucket = get_next(next_bucket, offset++, num_buckets, simd_bytes);
        }

        // Count coverage
        int covered = 0;
        for (bool v : visited) if (v) covered++;

        double coverage_pct = 100.0 * covered / num_buckets;
        printf("  Start=%u: covered %d/%u buckets (%.1f%%)\n", start, covered, num_buckets, coverage_pct);

        if (coverage_pct < 100.0) {
            printf("  WARNING: Not all buckets covered! Infinite loop risk!\n");

            // Find unvisited buckets
            printf("  Unvisited buckets: ");
            int count = 0;
            for (uint32_t i = 0; i < num_buckets && count < 10; i++) {
                if (!visited[i]) {
                    printf("%u ", i);
                    count++;
                }
            }
            if (count == 10) printf("...");
            printf("\n");
        }
    }
}

// Test step calculation
void test_step_calculation() {
    printf("\n=== Step Calculation Analysis ===\n");

    uint32_t simd_bytes = 16;

    // Test different num_buckets values
    uint32_t test_sizes[] = {16, 32, 64, 128, 256, 1024, 4096, 65536};

    for (uint32_t num_buckets : test_sizes) {
        printf("\nnum_buckets = %u:\n", num_buckets);

        // emilib2ss/emilib2s formula (offset >= 7)
        uint32_t step_ss = (((num_buckets / 8 + simd_bytes) / simd_bytes) | 1) * simd_bytes;
        printf("  emilib2ss step (offset>=7): %u\n", step_ss);
        printf("    GCD(step, num_buckets) = %u\n", std::gcd(step_ss, num_buckets));
        if (step_ss % num_buckets == 0) {
            printf("    ERROR: step is multiple of num_buckets! Will cycle in same position!\n");
        } else if (std::gcd(step_ss, num_buckets) != 1) {
            printf("    WARNING: GCD > 1, not all buckets covered!\n");
        }

        // emilib2o formula (offset >= 5)
        uint32_t step_o = (num_buckets / 11 + 1) | 1;
        printf("  emilib2o step (offset>=5): %u\n", step_o);
        printf("    GCD(step, num_buckets) = %u\n", std::gcd(step_o, num_buckets));
        if (std::gcd(step_o, num_buckets) == 1) {
            printf("    OK: GCD=1, all buckets covered\n");
        }
    }
}

// Simulate find_empty_slot infinite loop scenario
void simulate_infinite_loop() {
    printf("\n=== Simulating find_empty_slot Infinite Loop ===\n");

    uint32_t num_buckets = 64;
    uint32_t simd_bytes = 16;

    // Scenario: table is full (no empty/delete slots)
    printf("Scenario: Table is full, no empty/delete slots\n");

    // Using emilib2ss formula
    uint32_t next_bucket = 0;
    uint32_t offset = 7;  // Start from offset 7 where problematic formula kicks in

    std::vector<bool> visited(num_buckets, false);
    int iterations = 0;
    int max_iterations = 100;

    printf("  Simulating find_empty_slot with emilib2ss formula...\n");
    while (iterations < max_iterations) {
        if (visited[next_bucket]) {
            printf("  Iteration %d: Revisited bucket %u (CYCLE DETECTED!)\n", iterations, next_bucket);
            break;
        }
        visited[next_bucket] = true;
        next_bucket = get_next_bucket_ss(next_bucket, offset++, num_buckets, simd_bytes);
        iterations++;
    }

    if (iterations >= max_iterations) {
        printf("  No cycle detected in %d iterations\n", max_iterations);
    }

    // Count unique buckets visited
    int unique = 0;
    for (bool v : visited) if (v) unique++;
    printf("  Visited %d unique buckets in %d iterations\n", unique, iterations);
}

int main() {
    printf("========================================\n");
    printf("get_next_bucket Coverage Analysis\n");
    printf("========================================\n");

    // Test step calculation first
    test_step_calculation();

    // Test coverage for different configurations
    test_coverage("emilib2ss", 64, 16, get_next_bucket_ss);
    test_coverage("emilib2ss", 128, 16, get_next_bucket_ss);
    test_coverage("emilib2ss", 256, 16, get_next_bucket_ss);
    test_coverage("emilib2ss", 1024, 16, get_next_bucket_ss);

    test_coverage("emilib2o", 64, 16, get_next_bucket_o);
    test_coverage("emilib2o", 128, 16, get_next_bucket_o);
    test_coverage("emilib2o", 256, 16, get_next_bucket_o);
    test_coverage("emilib2o", 1024, 16, get_next_bucket_o);

    // Simulate infinite loop
    simulate_infinite_loop();

    printf("\n========================================\n");
    printf("Summary:\n");
    printf("1. emilib2ss/emilib2s: Potentially problematic step formula\n");
    printf("2. emilib2o: Fixed with odd step (GCD=1)\n");
    printf("3. All versions: find_empty_slot has no termination condition\n");
    printf("========================================\n");

    return 0;
}
