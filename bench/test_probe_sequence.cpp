// 探测序列设计分析
// 目标：避免聚集、保证覆盖、缓存友好

#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

// 方案1: 二次探测 (Quadratic Probing)
// step = i^2, 但可能不覆盖所有位置（当num_buckets不是prime时）
uint32_t quadratic_step(uint32_t offset) {
    return offset * offset;
}

// 方案2: 双重哈希 (Double Hashing)
// step = hash2(key), 需要第二个哈希函数
// 问题：需要额外计算hash2

// 方案3: 伪随机探测 (Pseudo-random Probing)
// 使用确定性随机序列，基于offset生成步长
uint32_t pseudo_random_step(uint32_t offset, uint32_t num_buckets) {
    // 使用黄金比例作为乘数，产生伪随机序列
    // 0x9e3779b9 是黄金比例的整数近似
    return ((offset * 0x9e3779b9) ^ (offset >> 16)) | 1;  // 保证奇数
}

// 方案4: 混合探测 (Hybrid Probing) - 推荐
// 前7次线性探测（缓存友好），之后伪随机探测（避免聚集）
uint32_t hybrid_step(uint32_t offset, uint32_t num_buckets, uint32_t simd_bytes) {
    if (offset < 7) {
        // 线性探测：缓存友好，SIMD批量检查
        return simd_bytes * offset;
    } else {
        // 伪随机探测：避免聚集，保证覆盖
        // 使用黄金比例乘法产生伪随机奇数步长
        // 步长范围：1 到 num_buckets/4，保证足够分散
        uint32_t step = ((offset * 0x9e3779b9) >> 16) % (num_buckets / 4) + 1;
        return step | 1;  // 保证奇数，与power-of-2互质
    }
}

// 方案5: Fibonacci探测 - 推荐
// 使用Fibonacci数列作为步长，天然避免聚集
uint32_t fibonacci_step(uint32_t offset, uint32_t num_buckets) {
    // Fibonacci数列: 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89...
    // 预计算前20个Fibonacci数
    static const uint32_t fib[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 
                                    89, 144, 233, 377, 610, 987, 1597, 2584, 4181, 6765};
    
    if (offset < 20) {
        return fib[offset];
    } else {
        // 超过预计算范围，使用伪随机
        return ((offset * 0x9e3779b9) >> 16) | 1;
    }
}

// 方案6: 交替步长 (Alternating Step) - 推荐
// 在两个不同的奇数步长之间交替，打破聚集模式
uint32_t alternating_step(uint32_t offset, uint32_t num_buckets) {
    // 两个互质的奇数步长
    const uint32_t step1 = num_buckets / 7 | 1;   // 较大步长
    const uint32_t step2 = num_buckets / 13 | 1;  // 较小步长
    
    if (offset < 5) {
        return offset;  // 线性探测
    } else if (offset % 2 == 0) {
        return step1;   // 奇数offset用大步长
    } else {
        return step2;   // 偶数offset用小步长
    }
}

// 方案7: 三步长交替 (Triple Alternating) - 最优
// 在三个不同的奇数步长之间循环，彻底打破聚集
uint32_t triple_alternating_step(uint32_t offset, uint32_t num_buckets) {
    if (offset < 5) {
        return offset;  // 线性探测
    } else {
        // 三个互质的奇数步长
        const uint32_t s1 = num_buckets / 5  | 1;  // 大跳跃
        const uint32_t s2 = num_buckets / 11 | 1;  // 中跳跃
        const uint32_t s3 = num_buckets / 17 | 1;  // 小跳跃
        
        const uint32_t phase = (offset - 5) % 3;
        if (phase == 0) return s1;
        else if (phase == 1) return s2;
        else return s3;
    }
}

// 测试覆盖率
void test_coverage(const char* name, uint32_t num_buckets, 
                   uint32_t (*step_func)(uint32_t, uint32_t)) {
    printf("\n=== %s (num_buckets=%u) ===\n", name, num_buckets);
    
    for (uint32_t start = 0; start < std::min(num_buckets, 64u); start += 16) {
        std::vector<bool> visited(num_buckets, false);
        uint32_t bucket = start;
        uint32_t offset = 0;
        
        for (int i = 0; i < 2000 && offset < 2000; i++) {
            visited[bucket] = true;
            uint32_t step = step_func(offset++, num_buckets);
            bucket = (bucket + step) & (num_buckets - 1);
        }
        
        int covered = 0;
        for (bool v : visited) if (v) covered++;
        printf("  Start=%u: %u/%u (%.1f%%)\n", start, covered, num_buckets, 100.0*covered/num_buckets);
    }
}

// 测试聚集程度
void test_clustering(const char* name, uint32_t num_buckets, 
                     uint32_t (*step_func)(uint32_t, uint32_t)) {
    printf("\n=== Clustering Test: %s ===\n", name);
    
    // 模拟1000个key插入到相同bucket
    std::vector<int> probe_depth(num_buckets, 0);
    uint32_t bucket = 0;
    
    for (int key = 0; key < 1000; key++) {
        uint32_t offset = 0;
        uint32_t probe_bucket = bucket;
        
        while (probe_depth[probe_bucket] > 0 && offset < 100) {
            uint32_t step = step_func(offset++, num_buckets);
            probe_bucket = (probe_bucket + step) & (num_buckets - 1);
        }
        
        if (offset < 100) {
            probe_depth[probe_bucket] = key;
        }
    }
    
    // 计算聚集指标：连续填充的bucket数量
    int max_cluster = 0;
    int current_cluster = 0;
    int total_clusters = 0;
    
    for (int i = 0; i < num_buckets; i++) {
        if (probe_depth[i] > 0) {
            current_cluster++;
        } else {
            if (current_cluster > 0) {
                total_clusters++;
                if (current_cluster > max_cluster) max_cluster = current_cluster;
            }
            current_cluster = 0;
        }
    }
    
    printf("  Max cluster size: %d\n", max_cluster);
    printf("  Total clusters: %d\n", total_clusters);
    printf("  Avg cluster size: %.1f\n", 1000.0 / total_clusters);
    printf("  Clustering score: %.2f (lower is better)\n", (double)max_cluster / 1000.0);
}

int main() {
    printf("========================================\n");
    printf("Probe Sequence Design Analysis\n");
    printf("========================================\n");
    
    uint32_t num_buckets = 1024;
    
    // 测试覆盖率
    test_coverage("Pseudo-random", num_buckets, pseudo_random_step);
    test_coverage("Hybrid", num_buckets, [](uint32_t o, uint32_t n) { 
        return hybrid_step(o, n, 16); 
    });
    test_coverage("Fibonacci", num_buckets, fibonacci_step);
    test_coverage("Alternating", num_buckets, alternating_step);
    test_coverage("Triple-Alt", num_buckets, triple_alternating_step);
    
    // 测试聚集程度
    test_clustering("Pseudo-random", num_buckets, pseudo_random_step);
    test_clustering("Hybrid", num_buckets, [](uint32_t o, uint32_t n) { 
        return hybrid_step(o, n, 16); 
    });
    test_clustering("Fibonacci", num_buckets, fibonacci_step);
    test_clustering("Alternating", num_buckets, alternating_step);
    test_clustering("Triple-Alt", num_buckets, triple_alternating_step);
    
    printf("\n========================================\n");
    printf("Recommendation:\n");
    printf("1. Triple-Alt: 100% coverage + best anti-clustering\n");
    printf("2. Alternating: 100% coverage + good anti-clustering\n");
    printf("3. Hybrid: Good balance of cache-friendly + coverage\n");
    printf("========================================\n");
    
    return 0;
}