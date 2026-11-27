# Flat Hash Map 索引结构优化要点（可作为后续优化清单）

- 来源文章：<https://martin.ankerl.com/2026/09/09/hash-map-index-structures/>
  （2026-10-02 重写版，含勘误；作者 Martin Ankerl，`ankerl::unordered_dense` 5.x 作者，存在自评偏差）
- 覆盖设计：abseil `flat_hash_map`、emilib、ihtab、boost `unordered_flat_map`、folly F14、indivi `flat_umap`/`flat_wmap`、
  `emhash8`、Verstable、`unordered_dense` 4.11 / 5.0
- 数据环境：Ryzen 9 7950X / Fedora，clang 22.1.8，`-O3 -DNDEBUG -std=c++20`，`-march` 默认（plain x86-64，只有 SSE2）。
  **绝对值只对该机器有效**，比例关系才是可迁移的部分。
  注意：`-march=native` 会把 SSE2 intrinsic 变成 AVX-512（`vpcmpeqb` 直接出 mask，不再有 `pmovmskb`），
  测出来的已经不是多数程序实际运行的代码。
- 本文用途：记录"哪些手段真的有效、为什么有效、代价是什么"，并映射到本仓库 `emhash` / `emilib` 的落点。

---

## 1. 一句话结论

> 决定一个 flat hash map 快慢的，不是探测序列、指纹宽度这些微调，而是三件事：
> **① 一次 SIMD 比较 16 个槽位；② 给"不存在"一个显式答案（overflow bit / counter），而不是找空槽；
> ③ erase 不要留下只有 rehash 才能清掉的东西。**

作者的量化结论（同一 hash、同一组 key，只换索引设计）：

| 对比 | 结果 |
|---|---|
| `absl::flat_hash_map` vs `boost::unordered_flat_map`（miss） | absl 需要 **1.7x** 时间 |
| "空槽即止" vs "显式 missing 测试"（miss，两个其他方面相同的 SwissTable） | **1.4x – 1.7x** |
| 固定规模 churn 下，abseil 的 miss 从 fresh 到最差点 | **2.34x** 变慢 |
| boost（overflow bit + anti-drift）同条件 | **1.46x** 变慢，且仍是最快之一（起步领先太多） |
| dense vs flat 的 dTLB miss / lookup（1M 条） | **1.78–2.19** vs **1.15–1.60** |
| iterate：dense vs flat | dense 1.0–1.48x，flat **5–14x** |

---

## 2. 查找代价模型（做优化前先判断瓶颈在哪）

一次 lookup 只有三类成本：

1. **依赖载入（dependent loads）**：hash → 地址 → 元数据 → 地址 → key/value。
   每一跳都是全缓存丢失，且**不能提前开始**。表大于 L2 后，这是主成本；
   **"一次 lookup 触碰几个内存区域"比"多读几个字节"更重要**。
   - flat：2 跳（元数据、槽位）；dense：3 跳（元数据、index、value）。
   - 作者把"index 与指纹放同一 88B block"后：指令 -7%，L1 miss -12~14%，4M 条时 **dTLB miss -28%**
     （每次 lookup 从 3 个区域降到 2 个）。
2. **分支预测失败**：约 16 cycles/次。"这个槽位被占用吗"逐槽询问 ≈ 抛硬币；
   "这 16 个指纹里有没有匹配"每组问一次，miss 时几乎总是 no，预测极好。
   **这是逐槽设计 vs SIMD 组设计差距的主要来源**。
3. **指令数**：最便宜的一项。文中所有实现都只跑到 0.8–3.3 IPC（在等内存/分支）。
   已经在等的地方省指令 = 不省时间，多个"优化"因此失败。

两个必须知道的方法论陷阱：

- **锯齿（sawtooth）**：表每次翻倍，负载从 LFmax/2 爬到 LFmax。同一实现不同规模的 hit 成本，
  最坏可相差 **1.84x**；ud 4.11 在 32k octave 的 57 个规模里 hit 波动 **2.91x**（文中最宽）。
  单点测量等于随机采样。
  作者用 **octave**：`n, n·2^(1/5) … n·2^(4/5)` 五个规模取几何平均。
  不同 map 之间，五点仍可能让 churn 比例偏差 **最多 25%**，lookup 偏差几个百分点。
- **吞吐 ≠ 延迟**：文中 lookup 是随机取 key 的吞吐测试。依赖链式 lookup 在 1M/4M 条时
  boost 慢 3.0x/4.0x、ud 5.0 慢 1.4x/1.5x，**排序会变**（吞吐 boost 快 1.69x，延迟 ud 更快）。
- **churn 必须用随机 key**：用连续数字或"很快复用被删 key"的 harness，会把 drift 低估约 **4x / 2x**。

---

## 3. 各设计的核心手段与代价（只摘"能拿走"的部分）

| 设计 | 元数据/槽 | 关键手段 | 代价 / 结论 |
|---|---|---|---|
| abseil flat | 1B | 16B window 从 home 槽开始（不对齐），尾部落一份拷贝；三角步进；**SOO：capacity 1 不分配** | tombstone 家族最弱；miss 要找空字节 |
| emilib | 1B | home 对齐到组；`%253 + EFILLED` 指纹；tombstone | 只把 live 计入负载上限，churn 时**不增长**（换来更长的 probe）；文章 32k octave churn 0.93、500k 0.70，其实不差 |
| boost flat | 16B/15 槽 | 第 16 字节 = **8 个 hash-class 的 overflow bit**；`h % 8` 不变的重映射表；erase 命中 overflow 时**降低 max_load（anti-drift）** | 每次 120k–150k 次 erase-insert 触发同尺寸 rehash；bit 不能清 → probe 变长 |
| folly F14 | 16B/14 槽 | **每 chunk 一个 overflow counter**，erase 可递减；**double hashing**（奇数步长 `2*tag+1`）；一个表三种形态 | 计数器饱和到 254 后不可恢复；计数器不分 class |
| indivi flat_umap | 2B | 16 指纹 + **8 个 per-class overflow counter** + 16 个 4bit distance | 2B/槽；distance 让 `erase(iterator)` 免 hash |
| indivi flat_wmap | 1B | 窗口 + 1B 指纹，空/删标记用有符号比较区分；8bit 指纹（254 个值） | 同作者同代码，比 `flat_umap` 各 lookup 快 1.07–1.38x：**差异主要来自 1B 元数据而非窗口** |
| emhash8 | 8B（dense） | 合并链；**index 字里空余高位当免费指纹**（`EMH_EQHASH`） | miss 要走到链尾，链是否存在是难预测分支 → miss 2.12x，是它最弱列 |
| Verstable | 2B | 16bit = 4bit 指纹 + **in-home bit（精确的"不存在"）** + 11bit 链位移 | 指令最少的 miss（46.5），但 0.81 次分支失败 → 周期是 ud 5.0 的 2 倍 |
| ud 4.11 | 8B（dense） | 距离与指纹同一 `uint32_t`，**一次整数比较同时判"相等/过大/继续"**；**backward shift 删除** | 每槽一次抛硬币；probe 长度随负载变化最大 |
| ud 5.0 | 5.5B（dense） | 88B block = 16 指纹 + 8 class counter + 16 个 uint32 index；**counter erase 递减**；**home 组内联 + probe_past 尾调用**；**管线化 rehash**；**move_home** | 多一跳依赖载入（dense 家族固有）；erase 需重 hash 被搬动的 key（int ~7ns，string ~50ns） |

---

## 4. 核心优化手段清单（按投入产出排序）

### P0 — 结构性、收益最大，且与本仓库直接相关

1. **每 16 槽一次 SIMD 比较**（已有：`emilib1-4`、`emhash9`）。
   文章：这比任何探测序列/指纹宽度调整都值钱；NEON 上更是"有没有"的差别（1.48x/1.62x）。
2. **用 per-hash-class overflow counter 回答"不存在"**，并**在 erase 时递减**。
   - 8 个 1B counter/组是拐点：1 个/组慢 4%（churn 慢 20%）；4bit×16 慢 1.4%；2bit×32 在 churn 下最差。
   - 关键顺序：`counter[h & 7] == 0` → 立即返回 miss；erase 时沿 home→所在组逐组递减。
   - boost 的 bit 版本会让 churn 退化到需要周期性同尺寸 rehash（~12–15 万对一次）。
3. **探针必须有与元数据无关的全局 bound**（`delta == group_mask` 即停）。
   否则"每个组的 counter 都非 0"的构造 key 会让 miss 死循环。boost 一直有；indivi 与 ud 5.0 都是后来补的。
   代价：几乎为 0（hit 83.6→82.7 指令）。
4. **指纹预广播表（256 × `uint32_t`）**：一次 L1 载入替代 and/compare/shift/or/multiply 五条指令，
   整型 miss **快 5–6%**（boost 首创，indivi 版本把 0 映射成 8；`h % 8` 不变，保证 class 一致）。

### P1 — 布局与热路径，通常 3–8%

5. **index 与指纹同块**（ud 5.0 的 88B block）：省一次地址计算，少触一个区域。
   suite +1.5–2.2%，find +4.4–5.3%，指令 -7%（62.0 vs 66.9/lookup），L1 miss -12–14%，4M 时 dTLB -28%（3.79 vs 5.30）。
   注意反例：**把 counter 单独放一个数组是免费的**（20M 条 57.1 vs 57.0 ns），因为它的地址只依赖组号，
   可以和指纹载入同时发出；所以"必须同块"的只有 index，不是计数器。
6. **home 与 fingerprint 取 hash 的不同端**（home 高位，指纹低位/或 `%253`），避免"同一 home 的 key 指纹相同"。
   指纹 0 表示空时，把 0 重映射为 8（或 9），保持低 3 bit 不变。
7. **热/冷路径拆分**：home 组内联比较，其余组放进 `probe_past_home` 由尾调用进入。
   string miss 指令/时间 **-8~9%**，string hit <2%，整型代码完全不变。
   触发条件：key 比较是真函数调用（`memcmp`）。
8. **prefetch**：
   - 在**比较指纹之前**预取 index/槽位所在行（地址只依赖 hash），可让多余的那跳依赖载入提前。
   - 作者在 x86 上"删掉已有 prefetch 的任意一个"都在 3% 内 → 已有 prefetch 时 x86 不必细调；
     但 emilib1 之前默认把 find 的 prefetch 整个编译掉了（`EMH_NO_READ_PREFETCH`），属于"从无到有"。
     ARM 上 boost 的注释说预取组内前半槽位最快。
   - 本仓库实测（emilib1，1M，随机，6 对全胜）：`find` 命中时对 pairs 组首行做 prefetch，
     int `find_hit` **+15.9%**、string `find_hit` +5.5%、int `insert_erase` +8.4%，
     `find_miss` 在噪声内（+0.25%）。**保留**。
   - 反面：在 `find` 入口无条件预取（未命中指纹也算）会让 `find_miss` 退化约 11%。

### P2 — 抗 churn / 抗漂移，收益 1.07–1.30x，但有对价

9. **move_home（懒修复）**：在"写操作里的命中"（`try_emplace`/`operator[]`/`insert`/`assign`）
   把刚命中的元素搬回 home（此时 home 已知，且 `match_empty(home)` 是已读过的组）。
   - churn 表的 miss **1.26–1.30x** 更快，churn 轮次本身 -8%；去掉 35% 分支失败。
   - 只在 `find()`/const 路径**不能**做（会被当成读操作，产生数据竞争）。
   - 收益集中在：固定规模 churn + 按 key 写 + 还有负查询（去重集合、带负缓存、滑动窗口计数）。
10. **double hashing 替代三角步进**（奇数步长，遍历所有组）：
    churn miss 1.07–1.18x，fresh 整型 hit 最多 **-5%**，suite 在 0.1% 内。
    原因：三角步进下同 home 的兄弟 key 总是落在后续 miss 会走的同一组；double hashing 打散。
    代价是"步长"成为循环里第三个活跃值，整型 hit 先付钱。**是可交易项，不是纯赢**。
11. **精确 in-home counter**（再加 8 个 counter，记录"home 就是本组且溢出"的 key）：
    估计再省 churn miss 的 6–7%；只对 churn 有意义，fresh 无收益；每插入/删除多写 home 组。
    文章列为"未完成、值得做"的开放项。
12. **管线化 rehash**（dense 特有）：ring 里保持 16 个已算好的 hash，放置 i 之前先 hash i+16 并预取目标 block 的前两行。
    缓存内 1.26x；4M string rehash 30.6→12.4 ns/elem；4M int 无收益（瓶颈是 TLB）。
    rehash 循环还要求：**用迭代器遍历 values、组指针/mask/shift 放局部变量**，
    否则一次 `uint8_t` 指纹存储的别名语义会让每次放置都重载 data 指针（clang 10.43→2.74 ns/insert）。

### P3 — 特定场景，非默认开启

13. **SSO（小表不分配）**：capacity ≤ 1 时把唯一元素放进 map 对象本身（abseil）。
    省一次 malloc/free，比省 probe 值钱得多；只对 `sizeof(value_type) ≤ 指针` 生效。
14. **huge page 分配器**（2MB 对齐 + `madvise`）：lookup ~22%，build 1.53–1.73x（>200k）。
    会改变整个进程的分配器类型，只能作为可选 header，不能默认。
15. **PGO**：clang 的 insert 96.4→69.3 指令（clang 没内联 insert 的调用开销 15 条指令）。header 无法发布 PGO，但说明"不要为这个差距改源码"。
16. **per-table seed**：lookup 零成本，build **+3.5%**；换来抗已知 hash 的构造 key 与不可复现迭代序。默认不做。

### 已验证无效 / 不要重复尝试

| 想法 | 结果 |
|---|---|
| 第二个指纹塞进 index 空余位（来自 emhash8） | suite **-2.5%**；组内 16 路指纹已经把能过滤的过滤完了，只多一条依赖链 |
| 16 槽窗口（从 home 槽开始，来自 abseil/flat_wmap） | hit +1–5%，但 1M 条 churn **-24%**：window 的 tombstone 复用率 15.8% vs 组版 33.8% |
| 把 index 按 cache line 对齐（64B） | suite -0.7%（两者的 2 的幂偏移导致组元数据与 index 争同一组相联） |
| 16bit value index（CPython） | suite -1.4%；小表的 index 本来就在 L2 |
| distance nibble + value→slot 反向指针 | string `erase(iterator)` 1.10x，但 int 变慢 1.10x；suite -4%；内存 +7B/条 |
| 每 chunk 一个 counter（F14） | suite -4%，churn -20% |
| 每步换 class 读 counter / 每步换 3 bit hash | 前者恒等（兄弟 key 与 miss 同步）；后者 1.262→1.238 组，属噪声 |
| 加 `always_inline` 到放置函数 | 大 TU（benchmark）里 -1.7~-3.9%，单 map TU 里 +14–20%；取决于 TU 大小，不能盲加 |

---

## 5. 映射到本仓库（现状 → 建议动作）

### emilib1（`include/emilib/emihmap1.hpp`，namespace `emilib`）
- 现状：15/16 槽 + 组内联 probe depth（第 16 字节）；tombstone；miss 靠 probe depth 停止；
  指纹 `key_hash % 253 + EFILLED`；`_states` 与 `_pairs` 分开分配。
- 文章定位：最接近"plain SwissTable"，32k octave 各列中游；**churn 反而好**（0.93 / 500k 时 0.70），
  因为 tombstone 不占容量上限、不增长。
- 建议：
  1. 保留 tombstone + live-count 的 churn 优势，**不要**为了抗 churn 改成 counter（会丢掉它的强项）。
  2. 已完成并 A/B 通过：`find` 命中时对 `_pairs` 组首行做 prefetch（`EMILIB1_NO_FIND_PREFETCH` 可关）。
  3. 已完成：把 `%253` 换成 xor-fold 指纹 `((h>>56) ^ (h>>32)) & 0xFF`，并给 find 热路径加 `EMH_INLINE`
     （clang 20 会因内联悬崖慢 65~82%）。clang22 单 map：100k **+21.5%**、1M **+21.8%**、miss **+30.8%**、
     string 1M **+17.6%**。
  4. 若要再提升 find，可试"home 组内联 + 其余组拆函数"（string key 才有意义）。
  5. 原结构性问题（home 取低位、指纹取全 hash 的 `%253`，两者不独立）已由第 3 条解决：
     指纹改取高字节后与低位正交；home 仍取低位，布局/迭代序不变。

### emilib2（`include/emilib/emihmap2.hpp`，namespace `emilib2`）
- 现状：16/16 槽 + 独立 `_offset[]`；可配 LF 0.25–0.999；`EMH_PSL_LINEAR` / `EMH_PSL_ERASE` 支持 backward deletion。
- 文章定位：本机实测 find 最好（1M int 30.1 / string 147.3 ns）。指纹已改成 xor-fold：
  WSL clang20 为 +5~9%，Windows clang22 约 −2~4.5%（`%253` 的常量取模在 clang22 更便宜）。
- 建议：churn 定向优化只在"有 tombstone 且 backward deletion 关闭"的配置下考虑 counter 化；
  默认配置优先保证 find 不退化（任何改动都要 `find_hit`/`find_miss` A/B 双绿）。

### emilib3（`include/emilib/emihmap3.hpp`，namespace `emilib3`）
- 现状：sandwich layout（states 在前、pairs 在后，单次分配）；全局 `_max_probe_length` + group mask；
  已有 `EMH_MAP3_SPLIT_ALLOC`、`EMILIB3_NO_FIND_PREFETCH` 开关。
- 建议：
  1. ~~先评估 `EMH_MAP3_FAST_FIND`~~ 已完成：指纹默认改为 xor-fold（`((h>>56)^(h>>32))&0xFF`，
     与 home 低位正交），1M int `find_hit` +5.4%（clang20）/ +0.6%（clang22），
     100k int +2.9% / +5.8%，string 100k +9.1% / +5.5%。
  2. 试"per-class counter 替代全局 probe length"：这是文章里对 miss 收益最大的单项，且能让 erase 不再依赖 tombstone。
  3. 保持现有 find prefetch（与文章结论一致：地址只依赖 hash，提前发出）。

### emilib4（`include/emilib/emihmap4.hpp`，namespace `emilib4`）
- 现状：boost 风格 15 槽 + overflow byte；`maybe_caused_overflow` 就是 boost 的 anti-drift；无 backward shift。
- 文章定位：**这正是文章第 7 章的 boost 设计**，其 churn 短板有明确机制：
  overflow bit 不可清除 → 定期同尺寸 rehash（约 12–15 万对/次）。
- 建议（优先级最高）：
  1. 把 overflow bit 换成 **8 个 1B per-class counter**，erase 命中时递减；删掉 anti-drift 的 `_max_load` 扣减/周期性 rehash。
     预期：churn 不再退化，fresh miss 再快 5–6%（若同时上预广播指纹表）。
  2. 保留 `is_not_overflowed` 语义的快捷判断（`counter[h&7] == 0`），并确认已有"遍历完所有组即停"的 bound。
  3. iterate 慢是 flat 家族固有（文章 5–14x），不要为它改索引；如需要，另开 dense 变体。

### emhash5 / 6 / 7
- 现状：链式/位移，无组比较。
- 文章结论对它们的含义：**逐槽/逐链分支是结构上限**，"compare 16 slots at once"这一项就值 1.4–1.7x（miss）。
  继续在 5/6/7 上做微调收益有限；优化预算应优先给 `emhash9` 这种组索引设计。

### emhash8（`include/emhash/hash_table8.hpp`）
- 现状：dense + 合并链；`EMH_EQHASH` 用 index 字空余高位当免费指纹（文章点名的好设计）。
- 建议：
  1. 保留免费指纹；**不要**再往 index 里塞第二指纹。
  2. miss 是它的最弱列（链是否存在的分支不可预测）→ 可试：链尾判断改成一次 load + 无分支比较，
     或在 index 中缓存"链长"以去掉一步分支；任何改动用 miss 专项 A/B。
  3. rehash 可套用管线化 ring + prefetch（dense 家族通用收益）。

### emhash9（`include/emhash/hash_table9.hpp`）
- 现状：`group_block { uint8_t fingerprints[16]; uint8_t reach[8]; uint32_t index[16]; }`，
  已具备"指纹 + 每 class 的 reach 字节 + 同块 index"，与 ud 5.0 的 88B block 同构。
- 建议：直接按 P0/P1 清单核对：
  1. `reach` 是否在 erase 时递减（counter 语义）而不是只置位；miss 是否 `reach[class] == 0` 即停。
  2. 是否有独立于 `reach` 的全局 probe bound。
  3. 指纹是否用 256 项预广播表；是否 0→8 重映射并保持 `h % 8`。
  4. 分块分配阈值（1 MiB）之外，是否保证指纹与 index 永远同块（文章：同块值 1.5–2.2% suite）。
  5. 可加 move_home（写操作命中时）验证 churn 收益。

---

## 6. 后续实验/验证规范（避免自欺）

1. **A/B 必须成对交错**：本仓库用 `bench/framework/ab.ps1`（`-BinA/-BinB`，6 对，看 unanimous winner）。
   单独两段均值差 <2% 视为噪声。
2. **每个结论至少两个规模**，最好按 octave 五点；churn 结论需要 ≥3 个规模。
3. **churn 用随机 key、保持规模恒定**；不要复用刚删掉的 key，不要用连续 key。
4. **lookup 用不重复的随机序列**，否则分支预测器会"背下"命中模式（可把排序反转 2.7x）。
5. **小于 10% 的差异**：单 map 一个二进制 + 指令计数（`perf stat` 不可用时用 callgrind），不要相信多 map 同 TU 的耗时。
6. **指令数是唯一不会被代码布局移动的指标**；TU 大小会改变内联预算，benchmark 大 TU 与真实单 map TU 的结论可能相反。
7. 我们的 `bench/framework` 目前是单点规模 + 派生 `erase`，建议后续加 `--octave` 与"随机 key churn"模式，
   以便直接复用文章的方法。

---

## 7. 建议的执行顺序（面向"churn + find"）

| 顺序 | 动作 | 目标 map | 预期 | 风险 |
|---|---|---|---|---|
| 1 | per-class counter 化（替换 overflow bit） | emilib4 | churn 不再周期性 rehash；miss +5–6% | 中（erase 路径重写） |
| 2 | 预广播指纹表 + `0→8` 重映射 | emilib3/4、emhash9 | 整型 miss +5–6% | 低 |
| 3 | 校验并补齐 probe bound | emilib1-4、emhash9 | 正确性；性能中性 | 低 |
| 4 | move_home（写路径命中） | emhash9、emilib3 | churn miss 1.26–1.30x | 中（语义：不能放进 const find） |
| 5 | home 内联 + probe_past 拆分 | emilib2/3（string） | string miss -8–9% | 中（代码结构） |
| 6 | `EMH_MAP3_FAST_FIND` 默认化评估 | emilib3 | 待测 | 低 |
| 7 | 管线化 rehash | emhash8/9（dense） | build 1.2–1.5x（大表/长 key） | 中 |
| 8 | double hashing / 精确 in-home counter | 只在 churn 专项确认后再做 | 1.07–1.18x / 6–7% | 高（fresh hit 有对价） |

> 第 8 行两项都已评估（见 [CPU 层文档 §5-Y/§5-Z](./2026-11-23-cpu-level-hash-map-optimizations.md)）：
> - **精确 in-home counter**：在 `emihmap4` 的三角探测布局上只减少 2.6% 的 miss 探测组（1M 为 0），
>   只增不减的近似会漂移得比现状更差 → 否决。
> - **double hashing**：churn 后负查询的探测组 1.65 → 1.37，`churn_find_miss` +10~14%、
>   `insert_erase` +6.5%（均 6:0），但 `find_hit` −4~−9%（Ir +10%，stride 被 LICM 提到主路径）
>   → **默认否决**；已落地为 opt-in `EMILIB4_DOUBLE_HASH`，只给"固定规模 churn + 大量负查询"的负载打开。

---

## 8. 文章中的关键数字备查

- **miss 终止方式**（32k octave，相对 ud 5.0，越低越快）：
  boost 0.83、indivi flat_wmap 0.83、flat_umap 0.92、ihtab 1.10、emilib 1.12、abseil 1.42；
  emhash8 2.12、Verstable 2.05（链式最慢）。
- **500k octave**（索引离开 L2）：hit/miss 排序反转，flat_wmap 0.63/0.64、boost 0.78/0.69、abseil 0.73/0.77、
  emilib 1.04/**0.80**、indivi flat_umap 0.82/0.88。
- **每 lookup 指令/周期**（50k，全 hit）：flat_wmap 50.1/19.5、abseil 56.2/21.1、boost 57.0/25.1、
  ud 5.0 60.5/29.8、emilib 73.1/32.2。
- **每 lookup 指令/周期**（全 miss）：flat_umap 54.0/18.0、F14Vector 60.5/18.3、boost 54.3/20.8、
  ud 5.0 57.2/21.0、emilib 75.6/32.2、emhash8 46.4/38.1、Verstable 46.5/42.1。
- **counter 宽度**（churn 后每 miss 组数 / 继续比例 / suite）：1 个 1.21/60%/1.043；
  8×1B 1.06/17.5%/1.000；16×4bit 1.03/9.7%/1.014；32×2bit 1.02/rising/1.012。
- **merge 88B block**：suite +1.5–2.2%，find +4.4–5.3%，指令 -7%，L1 miss -12–14%，dTLB -28%（4M）。
- **move_home**：churn miss 1.26–1.30x；churn 轮次 -8%；分支失败 -35%；指令仅 -5.5%。
- **hash 选择**：AES-NI hash 在 hash 循环里快 25%，进 map 后每项慢 1.10–1.59x。
  string key 下"选哪个索引"差 15%，"选哪个 hash"可差 1.7x（boost 自带 string hash）。
