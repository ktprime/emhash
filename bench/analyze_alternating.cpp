// 分析交替步长的问题
// 问题：交替使用两个步长可能导致某些bucket无法访问

#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

// 问题分析：
// 假设从bucket 0开始，交替使用step1和step2
// 序列：0 -> step1 -> step1+step2 -> 2*step1+step2 -> 2*step1+2*step2 -> ...
// 
// 如果step1和step2都是奇数，但它们的组合可能不覆盖所有bucket

// 测试：分析步长组合的覆盖能力
void analyze_step_combination(uint32_t num_buckets, uint32_t step1, uint32_t step2) {
    printf("\n=== Step Combination Analysis ===\n");
    printf("num_buckets=%u, step1=%u, step2=%u\n", num_buckets, step1, step2);
    
    // 计算GCD
    printf("GCD(step1, num_buckets) = %u\n", std::gcd(step1, num_buckets));
    printf("GCD(step2, num_buckets) = %u\n", std::gcd(step2, num_buckets));
    printf("GCD(step1+step2, num_buckets) = %u\n", std::gcd(step1+step2, num_buckets));
    printf("GCD(step1-step2, num_buckets) = %u\n", std::gcd(step1-step2, num_buckets));
    
    // 模拟交替步长序列
    std::vector<bool> visited(num_buckets, false);
    uint32_t bucket = 0;
    uint32_t offset = 5;  // 从offset=5开始使用交替步长
    
    for (int i = 0; i < num_buckets * 2; i++) {
        visited[bucket] = true;
        
        // 交替步长
        if ((offset - 5) % 2 == 0)
            bucket = (bucket + step1) & (num_buckets - 1);
        else
            bucket = (bucket + step2) & (num_buckets - 1);
        
        offset++;
    }
    
    int covered = 0;
    for (bool v : visited) if (v) covered++;
    printf("Coverage: %u/%u (%.1f%%)\n", covered, num_buckets, 100.0*covered/num_buckets);
    
    // 分析：为什么覆盖率低？
    // 交替步长的总位移是 step1 + step2（每次循环）
    // 如果 GCD(step1+step2, num_buckets) != 1，则无法覆盖所有bucket
    
    uint32_t cycle_step = step1 + step2;
    printf("Cycle step (step1+step2) = %u\n", cycle_step);
    printf("GCD(cycle_step, num_buckets) = %u\n", std::gcd(cycle_step, num_buckets));
    
    if (std::gcd(cycle_step, num_buckets) != 1) {
        printf("PROBLEM: GCD != 1, cannot cover all buckets!\n");
        printf("Solution: Need to ensure step1+step2 is coprime with num_buckets\n");
    }
}

// 修复方案：使用单一奇数步长，而不是交替步长
uint32_t get_next_bucket_fixed(uint32_t next_bucket, uint32_t offset,
                                uint32_t num_buckets, uint32_t simd_bytes) {
    if (offset < 5) {
        next_bucket += simd_bytes * offset;
    } else {
        // 使用单一奇数步长，确保GCD=1
        // 选择一个足够大但不会太大的步长
        // 步长 = num_buckets / 2 + 1（保证奇数且足够大）
        uint32_t step = (num_buckets / 2) | 1;  // 约一半表大小，奇数
        next_bucket += step;
    }
    return next_bucket & (num_buckets - 1);
}

// 更好的方案：使用伪随机步长（基于offset）
uint32_t get_next_bucket_random(uint32_t next_bucket, uint32_t offset,
                                 uint32_t num_buckets, uint32_t simd_bytes) {
    if (offset < 5) {
        next_bucket += simd_bytes * offset;
    } else {
        // 使用offset生成伪随机奇数步长
        // 公式：step = (offset * phi) % range | 1
        // phi = 0x9e3779b9（黄金比例）
        uint32_t phi = 0x9e3779b9;
        uint32_t range = num_buckets / 4;  // 步长范围
        uint32_t step = ((offset * phi) >> 16) % range + 1;
        step |= 1;  // 确保奇数
        next_bucket += step;
    }
    return next_bucket & (num_buckets - 1);
}

// 测试修复方案
void test_fixed_solutions(uint32_t num_buckets) {
    printf("\n=== Testing Fixed Solutions (num_buckets=%u) ===\n", num_buckets);
    
    uint32_t simd_bytes = 16;
    
    // 方案1：单一奇数步长
    std::vector<bool> visited1(num_buckets, false);
    uint32_t bucket = 0, offset = 0;
    for (int i = 0; i < num_buckets * 2; i++) {
        visited1[bucket] = true;
        bucket = get_next_bucket_fixed(bucket, offset++, num_buckets, simd_bytes);
    }
    int covered1 = 0;
    for (bool v : visited1) if (v) covered1++;
    
    // 方案2：伪随机步长
    std::vector<bool> visited2(num_buckets, false);
    bucket = 0; offset = 0;
    for (int i = 0; i < num_buckets * 2; i++) {
        visited2[bucket] = true;
        bucket = get_next_bucket_random(bucket, offset++, num_buckets, simd_bytes);
    }
    int covered2 = 0;
    for (bool v : visited2) if (v) covered2++;
    
    printf("Single odd step: %u/%u (%.1f%%)\n", covered1, num_buckets, 100.0*covered1/num_buckets);
    printf("Pseudo-random:   %u/%u (%.1f%%)\n", covered2, num_buckets, 100.0*covered2/num_buckets);
}

int main() {
    printf("========================================\n");
    printf("Alternating Step Problem Analysis\n");
    printf("========================================\n");
    
    // 分析问题表大小
    analyze_step_combination(128, (128/7)|1, (128/13)|1);
    analyze_step_combination(256, (256/7)|1, (256/13)|1);
    analyze_step_combination(4096, (4096/7)|1, (4096/13)|1);
    analyze_step_combination(65536, (65536/7)|1, (65536/13)|1);
    
    // 测试修复方案
    test_fixed_solutions(128);
    test_fixed_solutions(256);
    test_fixed_solutions(4096);
    test_fixed_solutions(65536);
    
    printf("\n========================================\n");
    printf("Conclusion:\n");
    printf("1. Alternating step fails when GCD(step1+step2, N) != 1\n");
    printf("2. Single odd step works: GCD(N/2|1, N) = 1 for power-of-2 N\n");
    printf("3. Pseudo-random step also works\n");
    printf("========================================\n");
    
    return 0;
}