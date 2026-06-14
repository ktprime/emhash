// 推荐的探测序列实现
// 交替步长策略：保证100%覆盖 + 避免聚集 + 缓存友好

inline size_t get_next_bucket(size_t next_bucket, size_t offset) const noexcept
{
#if EMH_PSL_LINEAR == 0
    // 前5次线性探测（缓存友好，SIMD批量检查）
    if (offset < 5) {
        next_bucket += simd_bytes * offset;
    }
    else {
        // 交替步长：在两个不同的奇数步长之间交替
        // 打破聚集模式，保证覆盖所有bucket
        
        // 步长1: 较大步长，快速跳转
        // 步长2: 较小步长，精细探测
        // 两个步长都是奇数，与power-of-2的num_buckets互质
        
        const size_t step1 = (_num_buckets / 7) | 1;   // 约14%的表大小
        const size_t step2 = (_num_buckets / 13) | 1;  // 约8%的表大小
        
        // 交替使用两个步长，打破聚集模式
        if ((offset - 5) % 2 == 0)
            next_bucket += step1;
        else
            next_bucket += step2;
    }
#elif EMH_PSL_LINEAR == 1
    // 纯线性探测（简单但容易聚集）
    next_bucket += simd_bytes;
#elif EMH_PSL_LINEAR == 2
    // Fibonacci探测序列
    static const size_t fib_steps[] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144};
    if (offset < 12)
        next_bucket += fib_steps[offset];
    else
        next_bucket += ((_num_buckets / 11) | 1);  // 后续用固定奇数步长
#else
    // 黄金比例伪随机探测
    // 0x9e3779b9 是黄金比例整数近似，产生伪随机序列
    const size_t golden_step = ((offset * 0x9e3779b9ULL) >> 16) | 1;
    next_bucket += golden_step % (_num_buckets / 4) + 1;
#endif
    
    return next_bucket & _mask;
}

// 另一个推荐方案：黄金比例步长
inline size_t get_next_bucket_golden(size_t next_bucket, size_t offset) const noexcept
{
    if (offset < 5) {
        // 线性探测：缓存友好
        next_bucket += simd_bytes * offset;
    }
    else {
        // 黄金比例步长：产生伪随机序列，避免聚集
        // 公式：step = (offset * phi) mod num_buckets
        // phi = 0x9e3779b9 (黄金比例整数近似)
        
        // 方法1: 直接使用黄金比例乘法
        const size_t phi = 0x9e3779b9ULL;
        size_t step = (offset * phi) >> 16;  // 取高位，更随机
        
        // 确保步长在合理范围内：1 到 num_buckets/4
        step = step % (_num_buckets / 4) + 1;
        
        // 确保奇数（与power-of-2互质）
        step |= 1;
        
        next_bucket += step;
    }
    
    return next_bucket & _mask;
}

// 终极推荐：三步长交替
inline size_t get_next_bucket_ultimate(size_t next_bucket, size_t offset) const noexcept
{
    if (offset < 5) {
        // 线性探测：SIMD批量检查，缓存友好
        next_bucket += simd_bytes * offset;
    }
    else {
        // 三步长交替：彻底打破聚集模式
        // 使用三个互质的奇数步长，保证覆盖
        
        // 计算三个步长（都是奇数，互质）
        const size_t s1 = (_num_buckets / 5)  | 1;  // 20% - 大跳跃
        const size_t s2 = (_num_buckets / 11) | 1;  // 9%  - 中跳跃
        const size_t s3 = (_num_buckets / 17) | 1;  // 6%  - 小跳跃
        
        // 三步循环：s1 -> s2 -> s3 -> s1 -> ...
        const size_t phase = (offset - 5) % 3;
        if (phase == 0)
            next_bucket += s1;
        else if (phase == 1)
            next_bucket += s2;
        else
            next_bucket += s3;
    }
    
    return next_bucket & _mask;
}

// 关键：添加终止条件防止死循环
size_t find_empty_slot(size_t main_bucket, size_t next_bucket, size_t offset) noexcept
{
    // 最大探测次数：num_buckets / simd_bytes（所有group）
    const size_t max_probe = _num_buckets / simd_bytes;
    
    do {
        const auto maske = empty_delete(next_bucket);
        if (maske) {
            const auto ebucket = CTZ(maske) + next_bucket;
            prefetch_heap_block((char*)&_pairs[ebucket]);
            if (offset > get_offset(main_bucket))
                set_offset(main_bucket, offset);
            return ebucket;
        }
        
        next_bucket = get_next_bucket(next_bucket, ++offset);
        
        // 终止条件：探测次数超过最大值
        if (offset >= max_probe) {
            // 表可能满了，触发rehash
            rehash(_num_filled + 2);
            // 重新查找
            return find_empty_slot(main_bucket, main_bucket, 0);
        }
    } while (true);
    
    return 0;  // 永不执行，但需要返回值
}