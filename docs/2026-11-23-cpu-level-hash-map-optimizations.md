# CPU 层分析：flat hash map 的指令、分支、缓存与编译器（细节优化清单）

- 配套文档：[索引结构优化要点](./2026-11-23-flat-hash-map-index-optimizations.md)（讲"设计该长什么样"）
- 本文只摘文章的**第 16–22 章**：硬件计数器、汇编、三种"快法"、TLB/大页、rehash 循环、
  编译器/PGO、hash 延迟，以及测量纪律。来源同前：<https://martin.ankerl.com/2026/09/09/hash-map-index-structures/>
- 目的：把"可以一个个试"的细节整理成可执行清单，每条都带**机理、预期幅度、判定方式、本仓库落点**。
- 平台：Ryzen 9 7950X / Fedora / clang 22.1.8 / gcc 16 / `-O3 -DNDEBUG -std=c++20` / **无 `-march`**（纯 SSE2）。
  `-march=native` 会把 `pcmpeqb+pmovmskb` 变成 AVX-512 的 `vpcmpeqb→mask`，profile 出来的不是多数程序跑的代码。

> 全文最重要的一句：**除了 IPC，所有列都是越低越好；而"指令数"是唯一不会被代码布局改变的量。**
> 小于 10% 的差异必须用"一个 map 一个二进制 + 硬件计数器"判定，不能用把 16 个 map 编进同一 TU 的耗时。

---

## 1. 计数器基线（50k 条，索引在 L1/L2，考察的是查找本身的"工作"）

`perf stat`，3000 万次查找，50,000 个 `uint64_t` key。

### 全命中（每次查找）

| map | ns | 指令 | 周期 | 分支失败 | L1 miss | IPC |
|---|---|---|---|---|---|---|
| indivi flat_wmap | 3.70 | 50.1 | 19.5 | 0.035 | 3.29 | 2.56 |
| absl flat | 4.01 | 56.2 | 21.1 | 0.044 | 3.51 | 2.66 |
| boost flat | 4.76 | 57.0 | 25.1 | 0.096 | 3.84 | 2.27 |
| indivi flat_umap | 4.75 | 55.4 | 25.3 | 0.066 | 3.74 | 2.19 |
| F14Value | 4.95 | 64.0 | 26.4 | 0.022 | 3.80 | 2.43 |
| ihtab | 5.42 | 53.8 | 29.0 | 0.022 | 3.23 | 1.86 |
| unordered_dense 5.0 | 5.61 | 60.5 | 29.8 | 0.065 | 4.22 | 2.03 |
| **emilib** | 6.05 | 73.1 | 32.2 | 0.099 | 2.86 | 2.27 |
| F14Vector | 6.05 | 68.1 | 32.3 | 0.024 | 3.95 | 2.11 |
| Verstable | 6.59 | 57.4 | 34.7 | 0.434 | 3.26 | 1.66 |
| emhash8 | 6.97 | 48.4 | 36.9 | 0.419 | 3.20 | 1.31 |
| unordered_dense 4.11 | 9.07 | 93.3 | 48.3 | 0.161 | 3.49 | 1.93 |
| std::unordered_map | 9.81 | 45.1 | 52.5 | 0.324 | 4.22 | 0.86 |

### 全 miss

| map | ns | 指令 | 周期 | 分支失败 | L1 miss | IPC |
|---|---|---|---|---|---|---|
| indivi flat_umap | 3.43 | 54.0 | 18.0 | 0.109 | 1.98 | 3.00 |
| F14Vector | 3.49 | 60.5 | 18.3 | 0.043 | 2.01 | 3.31 |
| boost flat | 3.98 | 54.3 | 20.8 | 0.164 | 1.89 | 2.61 |
| ihtab | 3.94 | 56.2 | 20.8 | 0.044 | 1.98 | 2.70 |
| unordered_dense 5.0 | 4.01 | 57.2 | 21.0 | 0.108 | 3.27 | 2.72 |
| indivi flat_wmap | 4.96 | 51.3 | 26.1 | 0.301 | 2.11 | 1.96 |
| absl flat | 6.01 | 61.1 | 31.7 | 0.360 | 3.44 | 1.93 |
| **emilib** | 6.12 | 75.6 | 32.2 | 0.419 | 1.92 | 2.35 |
| unordered_dense 4.11 | 6.62 | 90.3 | 34.8 | 0.162 | 2.48 | 2.59 |
| emhash8 | 7.22 | 46.4 | 38.1 | 0.592 | 2.08 | 1.22 |
| Verstable | 7.96 | 46.5 | 42.1 | 0.808 | 1.96 | 1.10 |
| std::unordered_map | 12.83 | 52.8 | 68.3 | 0.647 | 3.43 | 0.77 |

**读法（可直接指导优化）**

- 没有任何实现被"指令数"限制：全部只有 0.8–3.3 IPC，大家都在等分支或内存。
  所以**在已经等的地方省指令不省时间**，这是本文多个"优化"失败的原因。
- miss 差在**分支**：Verstable 46.5 指令 / 42.1 周期 / 0.808 分支失败，ud 5.0 57.2 指令 / 21.0 周期 / 0.108。
  "多 23% 的工作，用一半时间"：0.808 vs 0.108 的分支失败、约 16 周期/次，解释了一半差距。
- **emilib 的 miss 32.2 周期里有 0.419 次分支失败**（≈6.7 周期），absl 31.7 周期 / 0.360；
  boost 20.8 周期 / 0.164。这就是"用空字节停 miss"和"用一个 bit/字节停 miss"的差别——见 §3。
- 命中列 emilib 73.1 指令是**组比较家族里最高**（全场最高是逐槽 robin hood 的 ud 4.11，93.3）；
  同一家族里把指令压到 50 级别的空间来自"1 字节元数据/槽 + 无 counter 路径 + 按槽而非按组寻址"
  （文章自己也说不清最后 10 条指令去哪了）。

---

## 2. 表变大后：瓶颈从分支切换到 TLB（1M 条，仍在 32MB L3 内，吞吐）

| map | ns | 周期 | dTLB miss/查找 | L1 miss |
|---|---|---|---|---|
| boost flat | **15.26** | **82.8** | 1.366 | 4.226 |
| indivi flat_wmap | 15.66 | 85.2 | 1.348 | 3.681 |
| absl flat | 16.37 | 88.9 | **1.339** | 4.069 |
| Verstable | 18.32 | 99.4 | 1.600 | 3.619 |
| indivi flat_umap | 19.84 | 107.9 | 1.596 | 4.550 |
| unordered_dense 5.0 | 21.94 | 119.4 | 1.907 | 4.664 |
| emhash8 | 23.88 | 129.8 | 2.189 | 3.492 |
| boost node | 31.70 | 172.7 | 2.510 | 5.176 |

- **dTLB 是"布局家族"最清晰的单一指标**：触碰 1 个区域的 flat 是 1.15–1.60；触碰 2 个的 dense 是 1.78–2.19；
  还要跟指针的 node 是 2.51。4KB 页下，**prefetch 藏不住 page table walk**。
- 800k 全命中：ud 1.48 dTLB/查找 vs boost 0.89。
- 索引还能待在 L1/L2 时 → 分支预测是瓶颈，组设计赢；
  索引离开 L2 → 内存是瓶颈，**触碰区域最少的设计赢**。同一次改动在这两段可能结论相反。

---

## 3. 汇编级：三种"不存在"的终止方式（clang 22 `-O3`，一个 map 一个二进制）

三者前面完全相同：`broadcast` → `pcmpeqb` 16B → `pmovmskb` → 走匹配位。差别只在 **mask 为空之后**：

**abseil：再比一次向量，比的是 kEmpty**

```
pcmpeqb xmm3,xmm2 ; 16 个控制字节 vs H2
pmovmskb ebx,xmm3
test ebx,ebx
je .no_match
...            ; 走匹配 lane
.no_match:
pcmpeqb xmm2,xmm0 ; 同样 16 字节再和 kEmpty 比
pmovmskb r9d,xmm2
test r9d,r9d
jne .absent        ; 有空槽 → 不存在
add rdx,rdi        ; 否则下一个窗口
```

**boost：一次字节 test，bit 在循环外算好**

```
pcmpeqb xmm1,xmm0
pmovmskb ecx,xmm1
movzx r15d,BYTE PTR [rsp-0x9] ; 1 << (hash % 8)，循环外一次
and ecx,0x7fff                ; 屏蔽 overflow 字节
je .no_match
.no_match:
test BYTE PTR [rdx+0x4248a0],r15b ; 组的 overflow 字节 & 本 key 的 bit
je .absent
```

**unordered_dense 5.0：一条 `cmp byte,0`**

```
prefetcht0 BYTE PTR [r13+r10*1+0x40] ; 块的第 2、
prefetcht0 BYTE PTR [r13+r10*1+0x57] ; 最后一行，在指纹载入之前发出
movdqu xmm1,XMMWORD PTR [r13+r10*1+0x0]
pcmpeqb xmm1,xmm0
pmovmskb r10d,xmm1
test r10d,r10d
je .no_match
.match:
tzcnt r10d,ebp
mov r10d,DWORD PTR [r9+r10*4+0x18] ; index 与指纹同块，固定偏移
shl r10,0x4
cmp r11,QWORD PTR [r12+r10*1]      ; key 在 value 向量里
je .found
lea r10d,[rbp-0x1] ; 清最低匹配位
and r10d,ebp
mov ebp,r10d
jne .match
.no_match:
cmp BYTE PTR [r9+rax*1+0x10],0x0 ; 本 key 的 overflow counter
je .absent
cmp edi,r14d                      ; 或者所有组都走过了
je .absent
```

**三个结论**

1. miss 终止 = 2 条向量指令 + 分支（absl）/ 1 次内存测试（boost）/ 1 条 cmp（ud5）。
   absl 要看全部 16 字节，这就是它 miss 慢 1.42x 的来源。
2. ud5 的两条 prefetch 在 `movdqu` **之前**发出，所以"多一跳依赖载入"的实际代价比"触碰 3 个区域"的静态图小。
   但 x86 上删掉任一条都在 3% 内 → **已有 prefetch 时不值得细调**。
3. 走匹配位的三步全场一样：`tzcnt` → 用 lane → `lea/and` 清位。**设计差异都在它前后**。
   （x86 上可以试 `blsr` 代替 `lea/and`，语义相同，见 §5-B3。）

---

## 4. 三种"快法"，以及它们各自的死穴

| 策略 | 代表 | 优势场景 | 死穴 |
|---|---|---|---|
| **指令最少** | emhash8 / Verstable（miss 46.4 / 46.5） | 链只访问同 home 的 key，理论上不浪费 | 每步一个分支 → 0.592 / 0.808 分支失败，全输 |
| **区域最少** | flat SwissTable（absl/boost/emilib） | 新表 hit，整型，表越大越明显 | miss 要靠"空槽"停 → 分支多；churn 留 tombstone |
| **不可预测分支最少** | 组索引（boost/ud5/indivi） | 在缓存内、有 erase/churn、任何规模 | 表离 L2 后多一跳依赖载入 |

- 没有一种全赢：**小表比分支，大表比内存**；dense 家族在"迭代/增长/大 value"三列天然领先。
- 对我们的含义：`emhash5/6/7`（逐槽/逐链）已经撞到"分支上限"，继续微调收益有限；
  预算应给 `emhash9`（组索引）与 `emilib4`（counter 化）。

---

## 5. 可逐条尝试的细节清单

> 每条给出：机理 → 预期 → 判定 → 落点。默认判定方式：单 map 单 workload 二进制，
> 看**指令数**是否下降；再看 6 对交错 A/B 是否全胜。仅当两者一致才保留。

### A. miss 终止路径（当前 emilib 最大的单点）

- [ ] **A1. 把"空字节/内联 probe depth"换成"每 class 一个 1B counter"**
  - 机理：`cmp byte,0` 一条指令、分支几乎总可预测；空字节方案是"再比一次向量 + 分支"。
  - 预期：miss 分支失败从 0.419 → 0.16 量级；emilib 的 miss 32.2 周期有机会降到 25 以下。
  - 判定：`--single --op find_miss`，同时用 callgrind 看指令数不应上升。
  - 落点：`emihmap4.hpp`（overflow bit → counter，见配套文档）；`emihmap3.hpp`（全局 probe length → 每 class counter）。
- [ ] **A2. 保持"miss 测试"在指纹比较之后、且只用 1 次 load**
  - 机理：`no_match` 路径上多读的那一个字节与指纹同块/同页，几乎免费；放在前面会多一条依赖。
  - 落点：`emihmap1/2/3/4` 的 `find_filled_bucket`，确认 counter/状态字节的地址只依赖组号。
- [ ] **A3. 独立于元数据的全局 bound：`if (delta == group_mask) return npos;`**
  - 机理：否则"每个组都说继续"的被构造 key 会死循环；成本 1 条 cmp（hit 83.6→82.7 指令）。
  - 落点：`emhash9`（`reach` 是否可能全非 0 且无 bound）、`emihmap3/4`。
- [ ] **A4. 不要用"每个组一个 counter"（F14 式）**
  - 实测：suite -4%，churn -20%；共享 counter 不知道 class，一次溢出让该组后续所有 miss 继续（60% 继续 vs 17.5%）。
  - 同理不要 4bit×16（-1.4%）或 2bit×32（churn 下最差，饱和后不回落）。

### B. 指令流

- [ ] **B1. 指纹"预广播表"：`static const __m128i fp_words[256]`，用 `fp` 直接索引**
  - 机理：现在每次查找都在关键路径上算 `%253`、加常数、再 `_mm_set1_epi8`（≈5 条指令）；
    一次 L1 载入更便宜。boost 首创，indivi 版本把 0 映射成 8。
  - 预期：整型 miss 快 5–6%（文中 paired + 一个 map 一个二进制都测过）；64B value 的 find 在 gcc 下 14%。
  - 代价：4KB 常量表（SSE2 每项 16B），会占 L1；必须在缓存内规模上确认不退化。
  - 落点：`emihmap1/2/3/4` 的 `hash_key2`/`reduced_hash`；`emhash9` 的指纹广播。
- [ ] **B2. 广播常量确认在循环外（现状已满足），下一步是换成查表**
  - 现状：`emihmap1:1012`、`emihmap2:1031`、`emihmap3:1132` 都在进入 probe 循环前算一次 `SET1_EPI8/32`，
    所以"循环内重复 broadcast"不是问题——不要为此改代码。
  - 真正的优化是 B1 的查表：把"算 → 广播"整体换成一次 16B 载入。
- [ ] **B3. 匹配位遍历用 `blsr`（`x & (x-1)`）**
  - 机理：clang 在文章里生成 `lea`+`and` 两条；`blsr` 一条。只在一次命中多个指纹时省。
  - 判定：主要看 value 小的 key（碰撞多）与 `find_hit` 指令数；预期 <2%，属"顺手"。
- [ ] **B4. 不要加"第二指纹"**
  - 实测 suite -2.5%（find -9%、64B find -8%、随机 hit -7%、churn -8.5%）：16 路指纹已经过滤完了，
    第二检查只是给依赖链加 xor/shl/cmp。

### C. 预取

- [ ] **C1. 在指纹载入之前发出 value/index 的预取**
  - 机理：地址只依赖 hash，可以和数据载入并行；ud5 把它放在 `movdqu` 前。
  - 我们已验证：emilib1 在命中指纹时预取 pairs 组首行 → 1M int `find_hit` +15.9%、string +5.5%、
    int `insert_erase` +8.4%，`find_miss` 持平（见配套文档 §4-8）。
- [ ] **C2. 反面：不要在 find 入口无条件预取**
  - 实测：`find_miss` 退化约 11%（多一条无用内存流量）。只在指纹命中后预取。
- [ ] **C3. ARM 需要不同策略**
  - boost 注释：ARM 上"预取组内前一半槽位"收益更高，Intel 上只预取第一行最好。
  - 落点：`emilib` 侧可加 `__aarch64__` 分支；本机无法验证，标记为待真机测。
- [ ] **C4. 确认 MSVC 也有 prefetch**
  - 文中 ud5 曾因宏只对 gcc/clang 定义，在 MSVC 上完全没有 prefetch。我们已有 `_MSC_VER` 分支，保持即可。

### D. 布局与 dTLB

- [ ] **D1. 减少"一次查找触碰的区域数"**
  - 目标：`[指纹+index]` 同块（`emhash9` 的 `group_block` 已符合），`values` 独立一块 → 共 2 个区域。
  - 预期：1.5–2.2% suite，find 4.4–5.3%，4M 时 dTLB -28%。
- [ ] **D2. counter 可以单独数组，不必塞进块**
  - 机理：counter 的地址只依赖组号，其载入可与指纹载入同时发出；20M 条实测 57.1 vs 57.0 ns（打平）。
  - 结论：`emhash9` 的 `reach[8]` 放哪都行，**别为它挪 index**。
- [ ] **D3. 不要做 64B 对齐 index / 16bit index**
  - 实测：对齐 index suite -0.7%（64B 对齐后两组元数据与 index 争同一 cache set）；
    16bit index 在 <2^16 的表上 -1.4%（小表 index 本来就在 L2，窄载入还要零扩展）。
- [ ] **D4. 大页（可选 header）**
  - 实测：2MB 对齐 + `madvise(MADV_HUGEPAGE)` → 查找约 +22%（ud 17.10→13.32，boost 9.75→7.58）；
    build 1.53x@200k / 1.73x@800k / 1.56x@4M；整型 <200k 无收益，string build 50k 就有 1.17x。
  - 结论：收益真实但**是进程级决策**，只能作为可选分配器，不做默认。

### E. churn / erase 的 CPU 视角

- [ ] **E1. 抗 churn 的正确目标不是"常数不变"，而是"探针不变长"**
  - 文中：ud5 在 load 0.76、长 churn 后 miss 从 1.05 → 1.27 组；counter 精确不增长，但被搬离 home 的 key 仍在。
  - 判定：用**随机 key、固定规模**的 churn harness 测"每 miss 组数"，而不是只看 ns。
- [ ] **E2. move_home：只在写路径（`try_emplace`/`operator[]`/`insert`/`assign`）命中时做**
  - 预期：churn miss 1.26–1.30x；churn 轮次 -8%；分支失败 -35%；指令只 -5.5%。
  - 禁止：`find()`/const 路径（会成为读操作里的写，产生数据竞争）。
- [ ] **E3. distance nibble + value→slot 反向指针：只对"string key + erase(iterator)"成立**
  - 实测：string 该模式 1.10x 快（1006→888 指令/轮）；uint64 同模式 **1.10x 慢**（省下的 hash 只有 8 条指令）；
    suite -4%，内存 +7B/条。单独 back-pointer：string erase 每档快 9–11%（4M 省 40.8ns），但整数 build 慢 6.9–23.3%，
    churn 打平。**不建议默认做**，除非明确有"贵 key + erase(iterator)"负载。
- [ ] **E4. erase 不要留下只能 rehash 清掉的东西**
  - boost（bit + anti-drift）仍要每 12–15 万对 erase-insert 重建一次；counter 递减可以完全避免。

### F. 编译器（最容易"测反"的一块）

- [ ] **F1. `always_inline` 的结论取决于 TU 大小，必须两个 TU 都测**
  - 文中：去掉放置函数的 `always_inline`，**90 文件大 TU 的 suite 快 1.7%（clang）/3.9%（gcc）**；
    但**单 map 一个小 TU** 的 build 慢 14–20%、指令多 17–20%。
  - 我们的 `bench/framework/bench_main.cpp` 把 10+ 个 map 编进一个 TU → 属于"大 TU"，
    与用户实际"一个 map"的结论可能相反。**bench 里赢的 inline 改动不要直接下结论。**
  - 判定：指令数 + 单 map 小 TU 构建。
- [ ] **F2. clang 的 insert 慢不是寄存器分配，是 15 条函数调用开销**
  - 实测：clang 96.4 指令 / 33.8 周期，gcc 67.5 / 25.0；clang 每次 insert 付 15 条 prologue/epilogue
    （6 push + 6 pop + 两次栈调整 + ret，call 另计），boost 上也一样。
  - 结论：想赢回来靠 PGO 或把热路径（命中 + 常见放置）内联、只把"走过 home 组"做成调用（5.1/5.2 形状）。
- [ ] **F3. PGO 能消掉整个差距，header 库发不了但要知道"别往这找"**
  - 实测：clang insert 96.4 → 69.3 指令（34.0 → 17.5 周期），boost 82.9 → 57.8；gcc 无变化。
  - 剩下的 ~11 条是"先找命中、再找空位"的第二次遍历（boost 的探针直接返回插入位置）。
- [ ] **F4. 写路径的"字节 store 别名"会让每次放置重载 data 指针**
  - 机理：`uint8_t` 指纹存储可能别名任何对象（包括 value 向量自己的 data 指针）；
    每次放置后按索引读 key，编译器必须重载指针 → rehash 每元素等一次内存延迟。
  - 修法：**用迭代器遍历 values**，把 `groups`/`mask`/`shift` 放局部变量，不调用 `place_group` 成员函数。
  - 实测：clang rehash 10.43 → 2.74 ns/insert，200k build 16.72 → 8.96 ms；**gcc 无变化**（它自己已推出不别名）。
  - 落点：`emhash9` 的 `clone_from`/rehash、`emhash8` 的 rehash、`emihmap*` 的 `rehash_impl`。
- [ ] **F5. 不要相信多 map 同 TU 的耗时差异 <10%**
  - 文中同一份代码两次运行相差 140%；一个不碰 map 的 control 也会 8%/13% 漂移。
    多 map TU 的代码布局会随任一 map 改动而变，**比要测的效果还大**。

### G. 增长 / rehash（dense 家族）

- [ ] **G1. 管线化 rehash：16 个 hash 的环 + 预取**
  - 机理：hash 是依赖指令链（想跑前面），放置是随机写（可能不在缓存）；逐个做就互相等。
  - 实测：缓存内 1.26x（2.05 → 1.63 ns/elem）；4M string 30.6 → 12.4 ns/elem；
    4M int（176MB）12.6 → 12.5 **无收益**（瓶颈是 TLB，1.15 dTLB/放置）；gcc 在 40–500KB 区间最多慢 4.2%。
  - 落点：`emhash8/9` 的 rehash 循环。
- [ ] **G2. 不要为了 rehash 做"按目标组排序"**
  - 实测：dTLB 降到 0.24/放置、循环近乎减半（10.29 → 5.34 ns/elem），
    但在整个 build 中只值 0–7%（>32MB），且临时内存每次翻倍都要 fault（约 1µs/页）。不保留。
- [ ] **G3. counters 在 rehash 的放置遍历里同步重建，不要第二遍**
- [ ] **G4. flat 的增长贵在"搬 value"**：1M 条每元素 boost 97.5 指令/110.9 周期，dense 51.9/46.7（L1 miss 相同）。
  所以"增长快"是 dense 布局的红利，不是循环写得巧。

### H. hash（延迟，不是吞吐）

- [ ] **H1. map 里要选低延迟 hash**
  - AES-NI hash：hash 循环里快 25%，进 map 后每个 workload 慢 **1.10–1.59x**；
    指令更少（5.15G vs 5.35G）但周期多 59% —— 这是依赖链，不是工作量。
  - 最坏是随机命中（无法重叠），最轻是 build（rehash 提前 hash 16 个）。
- [ ] **H2. 缩短依赖链的四个具体手法（wyhash 改写）**
  - 8–16B：两次重叠的 8B 读，而不是四次 4B 读 + 拼装。
  - 17–144B：每 16B 块各自独立混合（各自 secrets），最后 xor 合并，而不是串 seed；
    48B key 从"3 次连续 multiply 才能开始 final mix"变成"1 次 multiply + final mix"。
  - >192B：6 条独立 lane（不是 3 条），长 key 的乘法链减半。
  - >48B 的 tail 独立混合（只用 secrets），与 lane 并行。
  - 注意：17–144B 的第二个 multiply 只用于修正 avalanche，**删不掉**（所有长度都会挂 avalanche 测试）。
- [ ] **H3. hash 延迟要"链式"测**
  - 把每个结果的一个字节写进下一个 key 再 hash（`x = hash(key)`），并让长度分派不可预测。
    **不要**用 `x = hash(keys[x & mask])`：那把 key 的长度和地址放上了依赖链，
    会测出 1.40x 的"收益"，进真实 map 后为 0。
- [ ] **H4. string key 的排序由 hash 决定，不由索引决定**
  - boost 的索引在 string hit 上比 ud5 快 13%，但它的 `hash<string>` 慢 1.7x，最终慢 15%；
    folly 的 SpookyHashV2 慢 2.9x（128B 时是 boost 的 2 倍）。
  - 我们的 `emhash`/`emilib` 默认 `std::hash<std::string>`（libstdc++/libc++ 的实现不差，MSVC 是 FNV 偏慢），
    换 hash 前后必须整表 A/B（索引差异只有 3–15%，hash 能翻盘）。

### I. 已落地验证：`thirdparty/emilib/emilib2o.hpp`（2026-11-23）

一个 map 一个二进制的 A/B（6 对，`--metric min`，1M 随机 key）：

| 改动 | 缺陷机理 | 实测 |
|---|---|---|
| **热路径强制内联**：`find`/`const find`/`contains`/`find_filled_bucket` 加 always-inline（`hash_key2` 保持普通 inline） | 原先只有 `inline`，clang -O2 把这个 TU 的 probe 留在栈外：一次 find 多付 call + 15 条 prologue/epilogue，且预留的 prefetch/提前退出无法与调用方合并 | int `find_hit` **+30~40%**（10k/100k/1M），miss +27~30%；string hit/miss +9% |
| **指纹改取 hash 高字节**：`key_hash % 253` → `h >> 56`（32 位 hash 先乘 0x9E3779B9…），home 仍取低位 | `%253` 与 `hash & mask` 相关，同一 home 组的 key 指纹碰撞更多，窗口内多做 key 比较 | 单独贡献 int 10k hit +7.7%、string 1M hit +6.7%、miss +5% |
| 对照：把 prefetch 地址改成候选槽（或再加一条） | 预取窗口首行时命中槽常在 4–10 行外，看起来"错"，但实测**反而慢 15%**：收益来自 TLB 提示，不是行命中 | 不采用 |
| 对照：256 项预广播指纹表（4KB） | 省掉 `%253`+broadcast 的 5 条指令，但多一次 L1 载入 | 与纯高字节方案打平或略慢，不采用 |
| 代价 | 内联使 insert 路径代码变大 | int insert 1M **-1.5~-2.8%**（all pairs）；churn 持平 |

同函数内还发现一个 **HEAD 已存在**的正确性缺陷（非本次引入）：`_offset` 每 8 桶一个 `uint8_t`，
全零 hash 这类退化输入下 probe depth 超过 255 会被截断，`find`/`erase` 找不到已存在的 key。
修法：`EMH_SAFE_PSL` 编码，或深度超限时强制 rehash/clamp（属于 §5-A3 的"必须有无条件 bound"）。

`include/emilib/emihmap2.hpp` **已经**对 `find`/`contains`/`hash_key2`/`find_filled_bucket` 用了
`EMH_INLINE`（always-inline），所以它只需要第二项改动（指纹取高字节）。实测（多 map 同一 TU 的 A/B，6 对）：

| 工作负载 | 提升 |
|---|---|
| int `find_hit` 10k / 100k / 1M | **+21.1% / +14.2% / +5.5%** |
| int `find_miss` 100k / 1M | **+8.5% / +6.1%** |
| string `find_hit` 100k / 1M | **+6.1% / +6.4%** |
| string `find_miss` 1M | **+3.7%** |
| insert / `insert_erase` / `erase_all` / iterate | 全部在噪声内（≤±1.4%，3/3 或 2/4 分裂） |

同样的零 hash 缺陷在 HEAD 版 emihmap2 上也能复现，且改动前后行为完全一致（`test_api_coverage` 284 例、
`test_emilib_lifecycle` 19 例全过）。

`emihmap1.hpp` / `emihmap3.hpp` 同步了同一项指纹改动，并在 WSL clang 20 上发现了两个坑：

1. **只取 `hash >> 56` 不够**：bench 的 int64 key 被限制在 `[0, 2^62)`，高 2 位恒为 0，
   于是"高字节"只有 **64 个取值**（实测 `distinct fp: old=253 new=64`，同组同指纹的额外 key 0.012→0.047）。
2. **clang 20 的内联悬崖**：emihmap1 的 `find_filled_bucket` 原本没有 `EMH_INLINE`，加了几行指纹代码后，
   WSL clang 20 在"三种 key 类型同 TU"的 harness 里把它变成真实调用，100k/1M 的 `find_hit` 慢 **65~82%**
   （指令数只差 0.8%，说明是调用/代码布局，不是算法）。

最终方案：**xor fold 指纹 + 强制内联**
`h8 = ((h >> 56) ^ (h >> 32)) & 0xFF`（<8 字节的 hash 先乘常数），再映射到 `[EFILLED, EFILLED+252]`；
`find`/`const find`/`contains`/`find_filled_bucket`/`hash_key2` 加 `EMH_INLINE`（与 emihmap2/3 一致）。
fold 对 62 位整数 key 有完整 8bit 熵，又不需要在查找路径上乘一次。

单 map 单 TU 的 A/B（clang 20 / WSL 与 clang 22 / Windows 两个编译器都跑）：

| 工作负载 | emilib1（clang20 / clang22） | emilib2（clang20 / clang22） | emilib3（clang20 / clang22） |
|---|---|---|---|
| int `find_hit` 100k | +9.2% / **+21.5%** | +9.2% / −4.5% | +2.9% / +5.8% |
| int `find_hit` 1M | +8.8% / **+21.8%** | +5.1% / −2.7% | +5.4% / +0.6% |
| int `find_miss` 100k/1M | — / +30.8% | +4.5% / −2.0% | — / −0.9% |
| string `find_hit` 1M | — / **+17.6%** | −2.5%(100k) / −1.9% | +9.1%(100k) / +5.5% |

结论：emilib1/emilib3 在两个编译器上都为正；emilib2 在 clang 20 为正、clang 22 约 −3%
（它本来 find 就最好，且 `%253` 的常量取模在 clang 22 上更便宜）。三个 map 的桶布局哈希在改动前后完全一致，
`test_api_coverage` 284 例、`test_emilib_lifecycle` 19 例均通过。

**方法论教训（写进下次的必查项）**：
- 只用一种编译器 + 一个 TU 形状得出的"±10%"结论会翻车；WSL clang 20 曾把"高字节指纹"测成 −73%。
- 指纹必须对"高位恒零"的 key 空间（62 位整数、32 位 hasher）仍有 8bit 熵，否则碰撞率直接 4x。
- 11 个 map 编进一个 TU 的 bench，与"一个 map 一个二进制"的结论可以相反（emilib1 曾 −4% vs +21.8%）。

### J. 已落地验证：`emihmap4`（boost 式 overflow bit → per-class counter）

- 结构：`group15` 仍是 16 字节（15 槽 + 1 字节），第 16 字节改为**粘性 "本组有溢出" 标志**
  （与指纹同 cache line，miss 的常见路径零额外载入）；每 class 的 1B 计数放在 groups 之后的独立数组
  （`_groups + num_groups`，每 8 字节/组），`mark_overflow` 自增、`erase_at` 沿 home→slot 递减，
  当某 class 计数减到 0 时用一次 64bit 载入检查整组，若全零就清掉标志。
- 删除了 anti-drift：erase 不再扣 `_max_load`，表不会因为 churn 触发同尺寸重建。
- 结果（clang 22 / Windows，6 对，多次重复）：

| 工作负载 | 结果 |
|---|---|
| `insert_erase` string 100k | **+44.6% / +49.9% / +50.2%**（三次，均 6/6） |
| `insert_erase` string 1M | +5.5% |
| `insert_erase` int 100k | +14.9% / +1.0% / +2.5%（基本是小幅波动） |
| `insert_erase` int 1M | **−5.9% / −9.0%**（6/6，稳定回退） |
| `find_miss` int 1M | +3.2% / +3.8% / +1.0% |
| `find_hit` int 1M | −0.8% / +1.5%（噪声） |
| `find_hit` string 1M | −1.2%（噪声） |
| `insert` int 1M | **−4.7% / −5.6% / −6.1%**（稳定回退） |
| `erase_all` int 1M | −3.3% |
| `iterate` 1M | +2.2% |

- 正确性：`test_api_coverage` 284 例、`test_emilib_lifecycle` 19 例、随机化等价测试
  （4 种 hasher + string，40 万次混合操作）全过；**全零 hash 的退化用例也通过**
  （counter 饱和到 255 后不再递减，永远保守地"继续探测"，避免把仍存在的 key 判成 absent）。
- 结论：这是**有对有价**的改动——string-key churn（旧版的 anti-drift 重建很贵）大幅受益，
  miss 小胜；但 int 大表的 insert/churn 要为 counter 的 RMW 和 erase 的路径递减付 ~5–9%。
- 后续可消掉这笔代价（原清单第 2 项）：把 8 个 counter 字节**紧挨着组**放（组 24B，
  用 `loadu`），counter 与指纹共享 cache line，insert/erase 不再多触一条线，`find` 也不需要
  粘性标志做快路径。文章实测 24B 组跨行"基本免费"（20M 条 57.1 vs 57.0 ns）。
- 注意：第一次实现把 counter 直接放在独立数组、miss 每次都读，`find_miss` **−10.7%**；
  加了"同 cache line 的粘性标志"快路径后恢复到 +3.2%。这正是文章"counter 地址只依赖组号所以免费"
  的一个反例：counter 单独成数组确实能并行发出，但**它仍然是每个组多一条 cache line**，
  fresh 表上必须用同线的标志先过滤。

#### 第 2 项（组内联 counter）实测：否决

按"把 counter 放进组自身的 cache line"做了 32 字节组（15 指纹 + 8 counter + 8 padding，保持
2 的幂 stride，所有 `& ~(sizeof-1)` / 迭代器技巧不变），结果**全面变差**（vs HEAD）：

| 工作负载 | 结果 |
|---|---|
| `find_hit` int 1M | **−8.9%** |
| `insert` int 1M | −6.1% |
| `insert_erase` int 1M | −7.2% |
| `iterate` 1M | −3.3% |
| `insert_erase` string 100k | +53.7% |

原因是**元数据密度**：第 1 项是 16B 组 + 8B 独立 counter = 24B/组；32B 组 = 32B/组（+33%）。
多出的 8B 让每次查找多扫 cache line/TLB，把所有 lookup 拖慢，counter 共享一行省下的 RMW 补不回来。
已回退到第 1 项。

#### 第 3 项（严格 24B 组）实测：否决

把 8 个 counter 直接放进组内（`struct { uint8_t m[16]; uint8_t ov[8]; }`，sizeof = 24，stride = 24，
SIMD 改用 `loadu`），总内存与第 1 项相同，目的是干掉"counter 独立数组"的第二次访问。
单 map A/B（clang 22 / int64 100k / 6 对 × 3 reps 取 min，A = HEAD，负 delta 表示 A 快）：

| 工作负载 | delta |
|---|---|
| `find_hit` | **−4.68%** (6:0) |
| `find_miss` | **+3.19%** (6:0) |
| `insert_erase` | +1.25% (6:0) |
| `iterate` | +30.12%（来自迭代器改造，见 §5-M） |

- miss 变快符合预期（sticky flag 与 counter 同一条 24B 记录，miss 不再触第二处）；
  但 hit 慢 4.7%：stride 不是 2 的幂，16B 指纹 load 有 2/3 概率跨 cache line，
  探测链上每组的有效载荷密度也下降。
- 净判定：**否决**（lookup 是主路径，hit 的损失把 miss 的收益吃回去），已回退，
  counter 继续留在组后的独立数组里（第 1 项）。
- 正确性（改动期间，已随回退撤销）：`test_emilib_lifecycle` 19 例、`test_api_contract` 7 例、
  `test_stress_correctness` 222 例 / 944.4 万断言、`test_api_coverage` 284 例、`test_crud` 90 例、
  `test_iterators` 54 例、全零 hash 与随机化等价测试全部通过；clang-format 与 gcc 16 严格警告干净。

#### 第 4 项（恢复 16/16 槽，粘性标志搬到位图）实测：否决

第 1 项把 byte 15 变成了粘性标志，组只剩 15 个槽。这次把标志移到"1 bit/组"的位图
（放在 counter 数组之后，容量 `(num_groups+7)/8`），让 16 个字节全部当槽用；其余结构不变。
单 map A/B（clang 22 / int64 / 6 对 × 3 reps，A = 当前版本（15 槽），负 delta 表示 A 快）：

| 工作负载 | 100k | 1M |
|---|---|---|
| `find_hit` | +1.67% | **−7.15%** |
| `find_miss` | **+27.58%** | **−17.76%** |
| `insert` | +3.11% | — |
| `insert_erase` | +8.48% | +4.52% |
| `iterate` | **−11.17%** | — |

- `reserve(n)` 对两个版本给出**相同的组数**（都是 2 的幂），所以 16 槽版本在这份容量里多了
  6.25% 的槽 → 同样放 100k 条时装载率从 81.4% 降到 76.3%，`find_miss` 的探测链变短，
  于是 100k 出现 +27.6% 的大胜（miss 的探测长度近似 1/(1-α)）。
- 但代价是同样容量下**多分配 6.25% 的槽和 element 空间**：1M 时索引+数据约 10MB，
  多出的 0.7MB 让 `find_hit` −7.2%、`find_miss` −17.8%（额外的位图行在 L2 里是纯开销，
  且 `_pairs` 变大 6.25% 影响 TLB）；`iterate` 也因为要跳过更多空槽而 −11.2%。
- 净判定：**否决**。这就是 §5-J 第 2 项那条结论的又一次印证：emihmap4 的关键是
  **每个 cache line 装下的元数据与数据量**，而不是每组的槽数；16 槽只有在"表还塞得进 L2"
  时占优，大表上净亏。已回退。

WSL gcc 16.0.1 复测（同样的 6 对 × 3 reps），趋势一致而且更极端——小表赢更多、大表输更多：

| 工作负载 | 100k | 1M |
|---|---|---|
| `find_hit` | +1.57%（噪声） | **−12.09%** |
| `find_miss` | **+35.37%** | **−22.07%** |
| `insert` | −4.94% | — |
| `insert_erase` | +13.97% | **−23.58%** |
| `iterate` | −6.30% | — |

- gcc 上 1M 的 `insert_erase` −23.6%、`find_miss` −22.1%，说明"多 6.25% 容量"在大表上是
  实打实的 TLB / L2 代价，而不是 clang 特有的布局噪声。
- 正确性（改动期间，已随回退撤销）：`test_emilib_lifecycle`/`api_contract`/`api_coverage`/`crud`/
  `iterators` 全部通过，`test_stress_correctness` 222 例 / 944.4 万断言通过，全零 hash 与
  随机化等价测试通过；clang-format 干净。

### K. 已落地验证：`emhash9` 的 `reach` 距离 → 可递减计数器

- 原来每组 8 个字节记录"该 class 的 key 最远探到第几组"，只增不减；churn 后所有 miss 都按历史最大值探测，erase 也无法回落。
- 改成**经过计数**：insert 对每个跳过的满组 `++`，erase 沿 home→slot `--`，lazy move-home 也 `--`；
  miss 在"本组该 class 计数为 0"处立即停（组数 bound 仍保证终止）；255 饱和且饱和后不再递减。
- 单 map 单 TU 的 A/B：`find_miss` int 1M **+10.6%**、string 1M **+3.7%**；`find_hit` 噪声（+1.5%/−0.9%）；
  churn int 100k −4.3%、string +2.1%。
- 又一次 TU 形状陷阱：多 map 同一 TU 里 `find_hit` 读到 −9.1%（5/1），单 map 二进制是 +1.5%——
  以单 map 结论为准。
- 正确性：`test_emhash9`（14 例 / 122.6 万断言）、`test_api_coverage` 284 例、随机化等价测试、
  全零 hash 退化用例全部通过。

### L. 实测否决：`emihmap3` 的全局 probe bound → per-home 深度（2026-11-24）

- 现状：`_max_probe_length` 是**全局**最大探测偏移，只增不减；miss 靠组内 `EEMPTY` 终止，
  全局 bound 只是"整组 16 槽全满"时的兜底。
- 试过：states 之后追加 **1B/组**（`num_buckets/simd_bytes`，6.25% 元数据）的 per-home 深度，
  `find` 用 `depth_of(home)` 取代全局 bound；插入时对 home 组 `max(depth, offset)`；
  255 饱和表示"整圈"，退化 hash 下仍能终止（否则 write 循环遇到 tombstone 后不退出，会挂死）。
- 单 map 单 TU 的 A/B（clang 22.1.4 / `-O3 -DNDEBUG -std=c++20`，int64 100k，6 对 × 3 reps 取 min，
  A = HEAD，负 delta 表示 A 快）：

| 变体 | `find_hit` | `find_miss` | `insert_erase` |
|---|---|---|---|
| 入口就读深度（eager） | **−10.6%** (6/0) | −2.9% (6/0) | −1.4% (6/0) |
| 只在 `maske==0` 后读（lazy） | **−4.6%** (6/0) | −4.6% (6/0) | — |

WSL 上再用 clang 20.1.8 与 gcc 16.0.1 各测一遍（同一台机器，`-O3 -DNDEBUG -std=c++20`，
`bench/framework/ab.sh` 6 对 × 3 reps 取 min）。结论**不是"全面变慢"，而是"写路径变快、读路径变慢"**，
两个编译器一致：

| 工作负载 | clang 20.1 | gcc 16.0 |
|---|---|---|
| `find_hit` int64 100k | −6.45% (6:0) | −9.05% (6:0) |
| `find_hit` int64 1M | −2.57% (6:0) | −2.00% (6:0) |
| `find_hit` string 100k | −8.67% (6:0) | −13.45% (5:1) |
| `find_miss` int64 100k | −3.08% (6:0) | −1.59% (6:0) |
| `find_miss` int64 1M | −7.40% (6:0) | −6.44% (5:1) |
| `insert_erase` int64 100k | **+7.10%** (6:0) | **+3.38%** (6:0) |
| `insert_erase` int64 1M | **+2.83%** (5:1) | +0.50%（噪声） |

（负 delta = HEAD 快；括号是 A:B 胜场。）

- 写路径变快符合机制：插入循环的终止条件从"全局最大偏移"换成"本 home 组自己的深度"，
  随机 key churn 时少扫很多组；收益幅度依赖编译器/布局（clang 22 上甚至是 −1.4%）。
- 读路径变慢也符合机制：`find` 多一次独立数组 load；string 上损失最大（−8.7% / −13.5%），
  因为 key 比较本身较重、多出来的 load 直接串在关键路径上。
- 结论：**否决**（lookup 是主路径）。根因正是 emhash9 那条经验的反面——emhash9 的 per-class counter
  放在组内第一条 cache line，"计数为 0 即 absent"能省掉整组扫描；而 emihmap3 的 16/16 槽没有空余字节，
  miss 本来就用 `pcmpeqb` 对 `EEMPTY` 精确终止（信息比 counter 更强），全局 bound 只在
  ~6%（0.84^16）"组全满"时才被读到。于是"更精确的 bound"省不下 miss 的工作，
  却给每次查找多了一次独立数组的 load，净亏。
- 这个"分叉"本身有价值：若将来真要一个纯写密集变体，可以把深度 bound 只用在写路径上
  （读路径保留全局 `_max_probe_length`），两边都不吃亏。
- 正确性（改动期间，已随回退撤销）：`test_emilib_lifecycle` 19 例 / 12857 断言、
  `test_api_coverage` 284 例 / 44995 断言、聚簇 hash 随机化等价测试（含 781 组的 1B 饱和路径）、
  全零 hash 退化用例；WSL clang 20 / gcc 16 与 Windows clang 22 的
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror` 干净，clang-format 无改动。
- 副产品结论：**emihmap3 的 miss 终止路径已经到头**，要再快只能减少每次 lookup 触摸的区域
  或指令数（§5-B/§5-D），而不是加元数据。

### M. 已落地验证：`emihmap4` 迭代器缓存组指针（2026-11-24）

- 旧迭代器只有 `(_pc, _p)`：每次 `++` 都用 `_pc % 16 == 14` 判断组边界，跨组时再用 `_pc & ~15`
  反推出组首地址。
- 改成 `(_pc, _p, _pg)`：`_pg` 就是当前组指针。组边界判断退化成一次地址比较，跨组只需 `++_pg`，
  phase 2 的 `match_occupied()` 不再做任何取整运算。
- 单 map A/B（A = HEAD，6 对 × 3 reps 取 min，int64 / string 100k）：

| 编译 / 负载 | delta |
|---|---|
| clang 22 Windows `iterate` int64 | **+39.2%** (6:0) |
| clang 22 Windows `iterate` string | **+30.4%** (6:0) |
| WSL clang 20.1 `iterate` int64 / string | +33.0% / +34.4% (6:0) |
| WSL gcc 16.0 `iterate` int64 / string | +14.9% / +15.6% (6:0) |
| `find_hit` / `find_miss` / `insert_erase` | −1.7%~+1.8%，均 ≤2% 噪声 |

- 代价：迭代器从 16B 变 24B（多一个组指针）。find/insert 不构造迭代器，所以热路径没有额外成本，
  只有 `find()` 返回迭代器时多写 8B。
- 正确性：`test_iterators` 54 例（含 `erase(it)` 返回下一元素、空 map 的 begin/end）、
  `test_stress_correctness` 222 例 / 944.4 万断言、lifecycle / api_contract / api_coverage / crud
  全过；clang-format 干净，clang 22 + clang 20 + gcc 16 严格警告干净。

### N. 实测否决：把 emihmap4 的"逐字节游走"迭代器推广到 emihmap1/2/3（2026-11-24）

- 前提不同：emihmap1/2/3 的迭代器本来就缓存了组起点 `_from` 和组内位图 `_bmask`，
  每步是 `blsr → tzcnt → add → cmp`，并没有 emihmap4 旧版那种"每个元素用地址取模判断组边界"的开销。
- 试了两个版本（都保持 `_states` 连续 + 末尾 SIMD 哨兵组）：
  1. **纯字节游走**：`++_bucket; ++_pc; while (*_pc < EFILLED)`，哨兵字节天然终止；
  2. **混合**：快路径同上，只在"遇到空槽且正好是组末"时才用 `filled_mask` 一次跳过整组。
- 单 map A/B（clang 22，A = 现版本）：

| 场景 | 纯字节游走 | 混合 |
|---|---|---|
| `iterate` 100k（81% 装载） | **+22.69%** | +2.45% |
| `iterate` 1M（51% 装载，harness） | 持平 | **−24.75%** |
| `reserve(1M)` 只放 1000 条 | **85µs → 540µs（−6.3x）** | 85µs → 79µs |

- 为什么 emihmap4 上成立、这里不成立：emihmap4 旧迭代器的每步链里有一次**地址取模**（`_pc % 16`），
  换成组指针后省掉的是 2 条串行 ALU；而这里的 `tzcnt` 结果不参与"下一个要测的状态字节"的地址计算
  （状态字节由位图决定，位图已经在寄存器里），省不出对应的时间。反过来，逐字节游走每步都要
  **从内存读状态字节**，稀疏表（大量空组）就会被 16 倍放大。
- 结论：**否决**，`emihmap1/2/3`（以及同样的 `emihset3`）迭代器保持现有的 `_from + _bmask` 位图式实现。
  这条"负结果"说明 §5-M 的收益来自 emihmap4 迭代器的特定形态，不是"逐字节一定更快"。
- 正确性（改动期间，已随回退撤销）：lifecycle / api_contract / api_coverage / crud / iterators 全过，
  `test_stress_correctness` 222 例 / 944.4 万断言、全零 hash、随机化等价测试通过。

### O. 实测否决：`emihmap3` 的 move_home（2026-11-24）

- 做法：`find_or_allocate`（写路径 `operator[]`/`insert_or_assign`/`emplace`）命中且不在 home 组时，
  若 home 组还有空位，把 `pair` 搬回 home：复制指纹、placement-new 搬值、源槽按 erase 规则释放。
  读路径 `find()` 不搬。emhash9 的同类优化默认开启，所以先在 emihmap3 上照做一次。
- 单 map A/B（clang 22；"churn" = 40 万步、50% 写命中 / 50% 删旧插新，固定 10 万条，
  splitmix hash + 打乱后的 key 池，之后 6 对 × 3 reps 取 min）：

| 场景 | 数据 |
|---|---|
| churn 后 `find_hit` int64 100k | **+5.63%**（6:0） |
| churn 后 `find_miss` int64 100k | **−2.38%**（6:0，HEAD 快） |
| churn 后 `find_hit` string 100k | +2.96%（6:0） |
| 官方 harness `insert_erase` int64 / string 100k | +0.48% / +0.39%（噪声） |
| 官方 harness 新表 `find_hit` / `find_miss` | +0.51% / 噪声 |

- 机理：emihmap3 的 miss 靠组内 `EEMPTY` 终止。把元素搬回 home 会让 home 组更满，
  **miss 命中 home 组时更常看到"无空位"而不得不继续下一组**；hit 则因为不必再跨组而变快。
  只复用 EDELETE（不消耗 EEMPTY）的变体也一样：`find_hit` +5.6%、`find_miss` −2.4% 不变，
  说明代价来自"home 组变密"本身，而不是"填掉最后一个空槽"。
- 判定：**否决**。计划里对它的预期是"churn miss 1.26–1.30x"（miss 变快），实测方向相反；
  用 5.6% 的 hit 换 2.4% 的 miss 对通用容器不划算（miss 常是负缓存/去重场景的主路径）。
- 记录一个反例：`std::hash` 指代 identity 时，用一个"单调递增的新键"做 churn 会让
  `_max_probe_length` 涨到上万组，WSL 上该场景 hit 会慢到 23µs/次（move_home +83% 也救不回来），
  说明这类微基准的 key 生成必须打乱 + 混合 hash，否则测的是病态表。
- 正确性（改动期间，已随回退撤销）：lifecycle / api_contract / api_coverage / crud / iterators 全过，
  `test_stress_correctness` 222 例 / 944.4 万断言、全零 hash、随机化等价测试通过；gcc 17/23 严格警告干净。

### P. 实测否决：`emihmap3` 的 find 拆分（home 内联 + `probe_past` 出线）（2026-11-24）

- 想法（§5-F 的"内联悬崖"反向利用）：home 组内联处理（绝大多数字命中在这里），
  跨组续探放进 `EMH3_NOINLINE probe_past()`，让 `find/contains/count/at/try_get/erase`
  各自内联的快路径变小，减少 icache 压力。
- 结果（clang 22，int64 100k，6 对 × 3 reps，A = 现版本）：

| 工作负载 | delta |
|---|---|
| `find_hit` | **−14.23%**（6:0，HEAD 快） |
| `find_miss` | **−12.99%**（6:0） |

- 原因：一旦 `find_filled_bucket` 里出现一个（哪怕是冷分支上的）**函数调用**，编译器就必须为
  每次内联展开准备栈帧并把值落到 callee-saved 寄存器；这个 prologue/epilogue 成本大于
  "少展开几十条指令"省下的 icache。注意 miss 路径本来在 home 组就因 `EEMPTY` 返回、
  根本不会走到那个调用，却同样 −13%，说明代价来自"函数里存在调用"这件事本身。
- 判定：**否决**，已回退。这条与 §5-F 的结论互为印证：在这类 header-only map 里，
  **热查找路径必须保持"无调用的叶子函数 + 强制内联"**，拆分只能在"一定不会被 *内联调用点* 触及"
  的位置做（例如 erase 的冷路径、rehash），不能切在 find 里。

### Q. 已落地（小胜）：`emhash9` rehash 消别名 + 平凡可拷贝值整块搬移（2026-11-24）

- `relocate()` 原本已经是"新表用局部指针 `nb`/`np` + 静态 `place_index`"的形态（早前 emhash9 工作
  已做过一部分），本轮补齐两处：
  1. 旧表指针 `op`、元素数 `n_filled`、hasher 引用提到局部，避免循环里每次从 `this` 重读；
  2. `value_type` 平凡可拷贝时，值搬移从逐元素 placement-new 改为一次 `memcpy`。
- 数据（`insert` 自然增长，含全部 rehash；6 对 × 3 reps 取 min）：

| 平台 / 负载 | 200k | 1M |
|---|---|---|
| clang 22 Windows int64 | **+3.06%** (6:0)、**+2.43%** (6:0) | +14.71% (6:0) / +1.63% / +0.17%（三次不稳定） |
| clang 22 Windows string 100k | — | −2.28% (5:1)，视为布局噪声 |
| WSL clang 20.1 int64 | −0.18% | −0.40%（均噪声） |
| WSL gcc 16.0 int64 | +0.80% | +1.58%（均噪声） |

- 解读：clang 本身已经能把"平凡可拷贝的 placement-new 循环"向量化成等价的块搬移，所以 memcpy 只
  在小表（200k）稳定赢 ~2.5–3%，大表上落在噪声带里。1M 那次 +14.71% 是**假读数**（随后两次复测
  分别 +1.63% / +0.17%），说明同机长跑时宿主干扰可以伪造 10% 以上的"全胜"。
- 判定：**保留**（同样的数据、更少的指令，200k 可重复小胜，其余平台噪声；无任何场景出现系统性回退）。
  这也把 §6 第 4 行的"gcc 无变化"预期补上了实测。

### R. 已落地验证：`emhash9` 管线化 rehash（16 深 hash 环 + 提前预取目标组）（2026-11-24）

- 做法（§P2 第 12 项落地）：`relocate()` 重建索引时用 16 深环形缓冲保存 hash——处理第 i 个元素时，
  先算第 i+16 个的 hash 写回环，并对它的 home 组发一次 `prefetch_group`，再放置第 i 个。
  这样"随机访问新 block 数组 + 页表 walk"的延迟与当前放置重叠；§5-Q 的局部化与
  memcpy 搬值继续保留。
- 数据（`insert` 自然增长，含全部 rehash；6 对 × 3 reps，4M 用 2 reps；负 delta = A 快）：

| 平台 / 负载 | 200k | 1M | 4M |
|---|---|---|---|
| clang 22 Windows int64 | +1.78%（噪声） | **+19.52%** (6:0)、**+18.22%** (6:0) | **+27.20%** (6:0) |
| clang 22 Windows string | — | **+11.73%** (6:0) | **+22.63%** (6:0) |
| WSL clang 20.1 int64 / string | — | +6.82% / **+12.97%** (均 6:0) | — |
| WSL gcc 16.0 int64 / string | — | **+12.57%** / **+17.32%** (均 6:0) | — |

- 解读：收益随表变大而增大（200k 噪声 → 1M ~+18% → 4M ~+27%），说明省下的正是**目标组随机访问的
  TLB/L2 延迟**；string 也显著（key 本身要访存，提前 16 个元素算 hash 刚好覆盖这段延迟）。
  这与文章"4M int 无收益（瓶颈是 TLB）"不同：把 prefetch 提前 16 个元素后，页表 walk 也被覆盖了。
- 三次重复测量（1M int64 +19.52% / +18.22%，及 §5-Q 里那次 +14.71% 假读数）说明这类收益必须复测；
  本项在两个编译器、两种 key 上都 6:0，可信。
- 正确性：`test_emhash9` 14 例、`test_api_coverage` 284 例、`test_stress_correctness` 222 例 /
  944.4 万断言、随机化等价、全零 hash 全过；gcc 17/23 严格警告、clang-format 干净。

### S. 实测否决：把管线化 rehash 推广到 `emhash8`（2026-11-24）

- 同样的 16 深 hash 环 + 提前预取 `&_index[hash & _mask]`，加在 `rehash()` 的重建循环里（其余不变）。
- 数据（`insert` 自然增长；6 对 × 3 reps，4M 用 2 reps）：

| 平台 / 负载 | 200k | 1M | 4M |
|---|---|---|---|
| clang 22 Windows int64 | ~0% | +5.45% (6:0) → 复测 **+0.77%（噪声）** | +2.77% (6:0) |
| clang 22 Windows string 1M | — | +1.13%（噪声） | — |
| WSL clang 20.1 int64 1M | — | **−2.17%**（6:0，A 快） | — |
| WSL gcc 16.0 int64 1M | — | +0.88%（噪声） | — |

- 原因：emhash9 的 `place_index` 是"一次随机访问一个扁平 block"的自包含函数，提前 16 个元素预取
  home 组就能覆盖 TLB/L2；emhash8 的 `find_unique_bucket` 要在 `_index` 的 `next` 链上做
  `find_last_bucket`/`find_empty_bucket` 多次**相依**访问，预取 home 桶覆盖不到真正的访问序列，
  而 hash 环本身是净开销。
- 判定：**否决**，已回退（emhash8 保持原 rehash）。这条说明 §5-R 的前提是"重建索引 = 每个元素
  一次随机访问"，链式索引不满足。

### T. 已落地：单 map 单 workload 基建 + hash 延迟评估（2026-11-24）

- 对应 §6 第 7、9 行。全部改动在 `bench/framework/`，没有改 `include/` 里的任何 map。
- 组成：
  - `bench_single.cpp`：一个 TU 只 `#include` 一个 map 头、只实例化一个 workload
    （`run_workload_fixed<O>` 把 op 变成模板参数），产出"一个二进制 = 一个 map × 一个 workload"。
    命令行兼容 `bench_main --single`，并会校验脚本传入的 `--map/--op` 与编译期值一致，防止比错对象。
  - `build_single.sh` / `build_single.ps1`：按 (map, op) 矩阵编译到 `build_single/`，
    额外支持 `--hash wyhash`（给 string key 换 hash）与 `--callgrind`。
  - `instr_count.sh`：callgrind 跑一次 workload，输出 Ir/Bc/Bcm 与 Top 函数。
  - `hash_latency.cpp`（CMake target `emhash_hash_latency`）：按 §H3 的"链式"方式测 hash 延迟。
- 基建本身踩到的三个坑（都会让结论反过来，已固化进脚本）：
  1. **只 `CALLGRIND_ZERO_STATS` 不够**：string map 析构时 free 10 万个 key 的指令数（约 6000 万）
     比被测量的 10 万次 find 还多，测出来的是析构不是查找。必须
     `START_INSTRUMENTATION`/`STOP_INSTRUMENTATION` 把 setup 与 teardown 都关在计数区外。
  2. **valgrind 3.18 不认 clang 的 DWARF5**（`unhandled dwarf2 abbrev form code 0x25`）：
     整个主程序会被归到 `__libc_start_main` 一行，函数级归因全丢。加 `-gdwarf-4`。
  3. **停止 instrumentation 后 3.18 的 `summary:` 会下溢**（2^64 − n），但文件末尾的
     `totals:` 是正确的；脚本读后者。
- 数据 1：hash 依赖链，ns/hash（2048 深，`x = hash(key)` 后把 x 写回 key；含一次 8B store→load
  交接，约 5 cycles，所有 hasher 相同）：

| hasher | Windows clang 22（MSVC STL）8/16/24/64B | WSL clang 20（libstdc++）8/16/24/64B |
|---|---|---|
| `std::hash<std::string>` | 12.2–13.2 / 20.2–23.3 / 30.3–33.3 / **73.5–84.4** | 4.7–5.9 / 5.7–7.2 / 6.7–8.5 / 11.9–14.9 |
| emhash9（hash + mix64） | 14.1–17.0 / 23.3–25.3 / 32.7–35.5 / 70.7–92.4 | 7.7–9.7 / 8.7–10.8 / 9.9–12.5 / 14.6–18.0 |
| wyhash（`emh_wyhash`） | **2.7–3.8 / 2.7–3.6 / 4.4–5.2 / 3.7–5.3** | **2.7–3.4 / 2.7–3.6 / 4.2–5.4 / 3.6–6.1** |

  （3–4 次独立测量的范围：同一台机器几分钟内就会漂 ~10%，单点不可信。）

  MSVC 的 `std::hash<string>` 是逐字节 FNV：64B 要 64 次串行乘加，延迟 73.5–84.4ns；wyhash 3.7–5.3ns，
  约 20 倍。libstdc++ 的 murmur 短 key 只要 4.7–5.9ns，差距缩小到 ~1.7 倍（64B 约 3 倍）。
  emhash9 的 `mix64` 在 8–24B 上再加 ~2–3ns。
- 数据 2：整表 A/B（`bench_single --key string`，6 对 × 3 reps，`--metric min`；
  A=默认 `std::hash`，B=wyhash；正 delta = B 快）：

| 平台 / 负载 | emhash9 | emhash8 |
|---|---|---|
| Windows clang 22 `find_hit` 100k / 1M | **+75.57% / +65.64%**（均 6:0） | **+56.83% / +43.55%**（均 6:0） |
| Windows clang 22 `find_miss` 100k | **+259.17%**（6:0） | **+125.24%**（6:0） |
| Windows clang 22 `insert` 100k | **+43.16%**（6:0） | **+38.04%**（6:0） |
| WSL clang 20 `find_hit` 100k / 1M | **+33.12% / +14.87%**（均 6:0） | **+27.64% / +34.55%**（均 6:0） |
| WSL clang 20 `find_miss` 100k | **+37.73%**（6:0） | **+30.34%**（6:0） |
| WSL clang 20 `insert` 100k | **+8.17%**（6:0） | **+15.38%**（6:0） |

  所有组合 6:0，远大于 §6 第 9 行预期的 3–15%。
- 数据 3：指令数（WSL clang 20，emhash9，100k string，单次调用，不含 setup/teardown）：

| workload | 默认 hash | wyhash | 每 op 变化 |
|---|---|---|---|
| `find_hit` | 18,883,691（189/op） | 14,573,633（146/op） | **−22.8%** |
| `find_miss` | 14,587,647（146/op） | 10,561,145（106/op） | **−27.6%** |
| `find_hit`（int32 参照） | 5,027,776（50/op） | — | — |

  `find_hit` 的默认 hash 里，libstdc++ `_Hash_bytes` 占 7.2M（72/op），`memcmp` 1.74M，
  循环与索引约 10M。换成 wyhash 后指令只少 23%，但墙钟时间少 33%（100k）：
  FNV/murmur 的乘法在依赖链上串行，wyhash 的 4 条 lane 可并行——这正是 §H1 说的
  "指令更少不等于更快，要看依赖链"。
- 判定：**评估完成，暂不改默认**。默认 `HashT = std::hash<KeyT>` 是 API 兼容的一部分
  （换 hash 会改变迭代顺序，且用户可能按默认 hash 做了持久化），但 MSVC/Windows 的
  string 负载差距太大（insert +38~43%、find +56~259%），值得作为可选 hasher 暴露并在
  文档里明确推荐；真要改默认，需在全部 map × 全部 op 上按本节的整表 A/B 复测。
- 复现：`bench/framework/README.md` 的 "One binary per (map, workload)"、
  "Instruction counts"、"Hash latency" 三节。

### U. 已落地/已否决：指纹表、wyhash 短 key 链、`string_hash`（2026-11-24）

- U0. bench 扩展：新增 `churn`（构建后固定规模地 erase 旧 key + insert 新 key）和
  `churn_find_miss`（**计时区外**做一整轮 churn，再测 miss），`--octave` 直接给
  1k/10k/100k/1M/4M 五档。三套 key 按构造不相交（int 三个区间、string 三个 seed 偏移）。
  这是 P2 类"抗 churn"实验的前置条件。
- U1. 指纹预广播表（§6 第 2 行）：emhash9（`fingerprint_words`）与 emihmap4
  （`match_word` 表）本来就有；本次只给 **emihmap1** 补上 `fp_broadcast[256][16]`
  + 一次 16B 载入，替换 `SET1_EPI8(fp)`。
  - 保留：`find_miss` int64 100k **+2.78%（6:0）**，复测 **+2.06%（6:0）**；
    指令数 5,371,127 → **5,071,127（−3 条/查找）**，string miss 同样 −3 条/查找。
  - 1M 的 +3.04% 是 3:3，按纪律不采信；string 的墙钟在噪声内（hash 占主导）。
  - 否决：同样改动在 **emihmap2**（`find_miss` int 100k −0.94% 6:0、1M −4.02%、
    churn string −1.71% 6:0）和 **emihmap3**（miss int −4.83%/−6.15%、
    churn int −3.02% 6:0）都是负收益，已回退。4 KB 表在它们的热路径上争 L1，
    而 `SET1_EPI32` 的乘法本来可以提前算好。结论：**只有 emihmap1 值得**。
- U2. wyhash 17–48B 链改写（§H2）：原假设是"每块串 seed，去掉后 24B 少一次 multiply"。
  编译后核对汇编发现：map 调用时 `seed=0`，`seed = wymix(0^W0,W1)` 被**常量折叠**，
  旧代码其实已经是 `lane → MUM → final` 三次；24B 比 16B 多出的那次 multiply 是
  必需的第三个 8B 块混合，删不掉。
  - 实测：`hash_latency` 24B 4.24–4.79ns（改前 4.21–5.43，噪声内）；整表 A/B
    （old wyhash vs new wyhash，string 32B）find_hit 100k +0.69%（噪声）、
    1M −4.13%（3:3）、find_miss/insert 全在 ±1% 噪声内。
  - 判定：**否决，已回退**（config.hpp 恢复原样）。§H2 的"1 次 multiply + final"
    对话的是上游 wyhash 的串行实现，本仓库的 17–48B 早已并行化，没有剩余空间。
- U3. `emhash9::string_hash`（§6 第 9 行的可选落地）：在 `hash_table9.hpp` 增加
  与 emhash8 同款的透明 wyhash hasher（`is_transparent` + `is_avalanching`），
  用法：
  ```cpp
  emhash9::HashMap<std::string, int, emhash9::string_hash, std::equal_to<>> m;
  ```
  它是 **opt-in，默认 `std::hash` 不变**（换 hash 会改迭代顺序）。收益见 §5-T：
  Windows/MSVC find +56~259%、insert +38~43%；libstdc++ find +15~38%、insert +8~15%。
  正确性：10 万条插入/查找/异构查找/删除/`rehash`/迭代/`clear` 在
  ASAN+UBSAN 与 gcc16 `-Wconversion -Wsign-conversion` 下通过。

### V. 已落地/已否决：`always_inline` 审计（2026-11-24）

- 方法：小 TU（`bench_single`，一个 map × 一个 workload）与大 TU（`bench_main`，全部 map）
  两套二进制；clang 20 与 gcc 16；6 对 × 3 reps；Ir 用 callgrind（setup/teardown 排除）。
- 现状盘点：emihmap1-3 的 `find`/`contains`/`find_filled_bucket`/`hash_key2` 已是 `EMH_INLINE`；
  emihmap4 的 `find`/`contains`/`insert`/`do_insert` 已是 `EMH_INLINE`；emhash9 的
  `find`/`contains`/`index_of`/`insert_core` 已是 `EMH9_FORCEINLINE`。
  **唯一缺口是 emihmap1/2 的插入链**：`emplace`/`insert`/`do_insert`/`find_or_allocate`
  只有普通 `inline`。
- V1 落地（clang-only）：给 emihmap1/2 的 7 个插入链函数加
  `EMILIB1_INSERT_INLINE`/`EMILIB2_INSERT_INLINE`，clang 下 = `EMH_INLINE`，gcc 下 = `inline`。
  - clang：`insert_erase` int64 100k **+11.02%（6:0，emilib1）/ +7.68%（6:0，emilib2）**，
    大 TU **+13.28%（6:0）**，emilib1 `erase_all` +5.84%（6:0）；自然增长 `insert` +1.1~2.0%。
  - 指令数：emilib1 29,811,458 → 24,818,956（**−16.8%**）；emilib2 29,801,213 → 24,577,586
    （**−17.5%**）——每对 erase+insert **−50 条指令、−2 个分支**。
  - gcc：强制内联反而让 emilib1 `insert_erase` −4.9~−8.0%（1M −3.5%），所以做编译器门控；
    门控后 gcc 四个负载都在 +0.1~1.0% 噪声内。
  - 判定：**保留**。收益来自"clang 在这个 TU 形状里把 3 层调用留在栈外"（§F2），
    不是 `always_inline` 本身——这正是 §F1 说的"结论取决于 TU / 编译器"。
- V2 否决 emihmap3：整套强制内联让 `insert_erase` +10.28%/+9.74%（6:0）、大 TU +13.13%、
  `erase_all` +8.69%，但自然增长 `insert` **−20.99%/−21.31%/−20.71%（三次 6:0）**。
  隔离后确认 `find_or_allocate` 同时是收益的必要条件和回退的来源，任何子集都无法兼得，
  整体回退。
- V3 否决 emihmap4：给 `hash_for`/`find_locator` 加 `EMH_INLINE` 后，
  `find_hit`/`find_miss`/`insert_erase`/`churn` 在小/大 TU 全部 ±1.5% 噪声内——`-O3`
  本来就会内联，无需标注。
- V4 否决"去掉 emhash9 `insert_core` 的 FORCEINLINE"：Ir 14,981,107 → 15,075,031
  （+0.94 条/insert），两套 TU 的墙钟都在 ±1.7% 噪声内；保留原样。
- 结论：§6 第 8 行的剩余空间只有 emihmap1/2 的插入链，且只在 clang 上成立；其余 map
  没有"漏掉的内联"。
- V5（方向 2，去掉强制内联）：emihmap4 的 `find_locator_at` 是一个体量较大的
  `EMH_INLINE` 函数，去掉后小/大 TU 的 `insert`/`insert_erase`/`churn_find_miss`/`find_hit`
  全部 ±1% 噪声（`find_hit` 本就不走它，作为对照也是 ±0.3%）。说明 clang 对这个函数
  本来就在自行取舍，标注是惰性的；不改。
- V6（方向 1，擦除链）：emihmap1 的 `erase(key) -> _erase(bucket)` 也是普通 `inline`，
  补上 clang-only 强制内联后：`erase_all` **+3.86%（0:6）**，但 `insert_erase` +1.39%（2:4）
  是噪声、自然增长 `insert` **−2.03%（6:0）**，且 Ir 从 24,818,956 升到 25,018,956
  （**+2 条/对**）。收益不成立、指令数反向，否决并回退。
- V7 总结：§6 第 8 行关闭——五个 map 里只有 emihmap1/2 的插入链存在真实的
  "clang 未内联"缺口；擦除链、emihmap4 的大函数、emhash9 的 `insert_core` 都无需改动。
- V8 跨平台复核（Windows clang 22，MSVC STL；与 WSL clang 20 对比）：
  - int64 `insert_erase`：emilib1 **+11.21%（6:0）**、emilib2 **+12.09%（6:0）**
    （WSL 为 +11.02%/+7.68%），结论一致。
  - int64 `insert`：+5.17%/+7.17%（均 6:0，Windows 上比 WSL 的 +1~2% 更明显）。
  - `erase_all`：emilib2 +5.74%（6:0），emilib1 +0.63%（噪声）。
  - string：`insert` 中性（+0.5%/+1.6% 噪声）；`insert_erase` emilib1
    −1.6~−2.4%（4:2）、emilib2 ±1.4%（3:3/5:1）——32B string 的 erase+insert 被
    堆分配主导，这点差异是布局噪声，不影响 int64 的结论。
  - 判定：clang 门控在 Windows 上同样成立，保留。

### W. 已评估：全部 map × key 长度的 hash 延迟（2026-11-24）

- 目的：把 §5-T 的 emhash8/9 两点扩成"哪些 map、哪些 key 长度会被 hash 翻盘"。
  方法不变：`bench_single` 单 map 二进制、6 对 × 3 reps、`--metric min`，
  A=`std::hash<string>`，B=wyhash（`--hash wyhash`），正 delta = B 快。
- W1. 全部 9 张 map，32B string，100k，Windows clang 22（MSVC STL）。**27/27 全 6:0**：

| 负载 | emhash5 | emhash6 | emhash7 | emhash8 | emhash9 | emilib1 | emilib2 | emilib3 | emilib4 |
|---|---|---|---|---|---|---|---|---|---|
| `find_hit` | +81.3% | +94.5% | +78.2% | +65.6% | +77.6% | +62.6% | +77.2% | +68.8% | +64.5% |
| `find_miss` | +92.7% | +106.4% | +91.5% | +153.4% | +209.4% | +85.3% | +166.7% | +79.3% | +124.1% |
| `insert` | +43.6% | +42.2% | +36.7% | +42.4% | +42.7% | +12.5% | +13.4% | +25.6% | +20.6% |

  emilib1/2 的 insert 是 5:1，其余 25 格全 6:0。
  WSL clang 20（libstdc++）：混合 key 抽查 emhash9/emilib2 的 `find` 为 +23~41%
  （6:0/5:1）；早先 9 张 map 用旧 key 的完整矩阵是 `find_hit` +9.5~34.1%、
  `find_miss` +11.2~45.1%、`insert` +6.3~15.4%（emilib1/2 +1.4% 噪声）。
  MSVC 的 FNV 才是大头，libstdc++ 的 murmur 已经把 gap 压小。
- **W1 的关键教训：key material 必须伪随机**。之前 string key 的前 8 字节直接编码
  `seed+i`（连续整数），8B 时等价于顺序整数 key，结果变成"hash 质量 × 各 map 的
  probe 方案"而不是延迟：emhash7 hit −22.3%、emilib2 miss −18.6%。把前 8 字节过
  splitmix64 后重测，**9 张 map 的 8B 全部转正**（见 W2 末行）。凡涉及短 key 的
  结论，都必须用伪随机 key material。
- W2. key 长度（Windows，100k，`--key-len`，混合 key）：

| map / op | 8B | 16B | 24B | 32B | 64B |
|---|---|---|---|---|---|
| emhash9 `find_hit` | +30.8% | +51.8% | +72.3% | +77.6% | +112.4% |
| emhash9 `find_miss` | +49.1% | +96.4% | +123.9% | +209.4% | +316.7% |
| emilib2 `find_hit` | +11.8% | +38.6% | +64.6% | +77.2% | +104.8% |
| emilib2 `find_miss` | +15.1% | +58.0% | +87.9% | +166.7% | +270.9% |
| emhash7 `find_hit` | +12.4% | +30.3% | +59.3% | +78.2% | +121.2% |
| emhash7 `find_miss` | +17.4% | +29.6% | +64.0% | +91.5% | +134.0% |

  8B 的全部 9 张 map（混合 key）：hit +4.3~30.8%、miss +6.0~49.1%，全部 B 快
  （只有 emhash5 是 5:1/4:2，其余 6:0）。
- 解读：
  - **所有长度、所有 map、所有负载，wyhash 都胜**；收益随 key 长度增长，
    8B 也有 +4~49%，64B 时 find 可达 +100~317%。
  - insert 收益（+12~44%）小于 find，因为分配/rehash 占了大头。
  - 与 §H1/H4 一致：hash 在查找里的占比决定它能翻盘多少。
- W3. 实现之间：emhash8 的 `string_hash` 与 emhash9 新增的**完全相同**
  （`emh_wyhash(...,0)`）；而 `-DEMH_WYHASH_HASH=1` 走的 `wyhashstr`（secret 起始）
  在混合 key 下：32B `find_miss` **−8.4%（6:0）**、8B `find_hit`/`find_miss`
  **−14.4%/−18.4%（6:0）**，32B `find_hit`/`insert` 在噪声内（−3.3% 3:3、−1.0%）。
  functor 从不更差（早先用连续 key 时它全 6:0 领先 4~22%）。所以推荐 functor，
  不要用 `EMH_WYHASH_HASH=1` 去追性能。
- 落地建议：string key 直接 opt-in `emhash9::string_hash` / `emhash8::string_hash`
  （8B 也赢）；默认 `std::hash` 仍不变。
- 工具：`--key-len`（bench_main/bench_single）、`AB_KEY_LEN`（ab.sh）、`-KeyLen`（ab.ps1）；
  默认 32B，key material 为 splitmix64 混合后的伪随机值。

### X. 实测否决：`emihmap3` 的全局 probe bound → 每 fingerprint class 的 bound（2026-11-25）

- 与 §5-L（per-home 深度）同族，但按**指纹低 3 bit 分 8 个 class**（`cls = uint8(key_h2) & 7`），
  用 `uint32_t _class_max_probe[8]` 记录"该 class 的元素曾被放到的最远组偏移"，只增不减（erase 不回落），
  因此始终是保守上界。`find_filled_bucket` 的终止条件从全局 `_max_probe_length` 换成
  `offset >= _class_max_probe[cls]`；写路径的 `find_or_allocate` 循环、hole 回落、`find_empty_slot`、
  `rehash`/`insert_unique` 都在落位时 `note_probe`。
- 单 map A/B（WSL clang 20.1.8，int64 100k，6 对 × 3 reps 取 min，A = HEAD，负 delta = HEAD 快）：

| 负载 | delta | 胜场 A:B |
|---|---|---|
| `find_hit` int64 | −1.83% | 3:3 |
| `find_miss` int64 | −3.08% | 6:0 |
| `churn_find_miss` int64 | −6.43% | 6:0 |
| `insert` int64 | **−25.11%** | 6:0 |
| `find_miss` string 32B | −0.24% | 2:4 |
| `churn_find_miss` string 32B | +0.70% | 1:5 |

  （`find_miss` string 的 +0.7% 属于噪声：同一二进制自比 A/B 在 insert 上是 −1.5%（3:3）、
  find_miss 上 +2.28%（4:2），所以只有 6:0 的格子可信。）
- 指令数（callgrind，`--op find_miss --once`，setup/teardown 排除）：

| key | Ir HEAD → 变体 | Bc | Bcm |
|---|---|---|---|
| int64 100k | 5,271,823 → 5,467,661（**+3.71%**） | 465,000 → 464,338 | 66,164 → 66,163 |
| string 32B 100k | 14,190,630 → 14,394,208（**+1.44%**） | 1,057,775 → 1,056,111 | 51,278 → 51,265 |

- 两个控制实验定位代价来源（均为 int64 100k，6 对 × 3 reps）：
  1. **读用 class bound、写循环保留全局 bound**：`insert` 仍 −28.27%（6:0），`find_miss` −0.80%（4:2）。
     说明 insert 的损失不是"bound 变严/变松"，而是每次插入的 class 记账本身。
  2. **只加 32B 成员数组、不改任何逻辑**：`insert` −0.93%（3:3）。
     说明对象变大（字段偏移/对齐变化）不解释损失。
- 结论：**否决**。读路径这边，miss 已经用 `pcmpeqb` 对 `EEMPTY` 精确终止（信息量比 counter 更强），
  class bound 只在"整组 16 槽全满"（0.84^16≈6%）时才被读到，省不下任何组扫描，却给每次查找
  多了一次依赖 hash 的数组 load；写路径这边，`note_probe` 落在每次插入的必经分支上，净亏。
  与 §5-L 一致：`emihmap3` 的 miss 终止已经到头，要再快只能减少每次 lookup 触摸的区域/指令数
  （§5-B/§5-D），不能加元数据。A1 那条建议里的"每 class counter"只对"计数为 0 即 absent、
  先读 counter 再载组"的设计（emhash9）成立；emilib 的 16/16 槽没有空余字节，
  counter 进不了终止路径。
- 正确性（改动期间，已随回退撤销）：`test_emilib_lifecycle`、`test_api_coverage`
  在 ASan+UBSan 下全部通过。

### Y. 复核：`emihmap4` 的 per-class counter（2026-11-25）

- 现状（§5-J 落地）：16B 组 = 15 指纹 + 1B 粘性"本组有溢出"标志；组数组之后每 8B/组存 8 个
  per-class 计数。插入经过"已满组"时 `mark_overflow`（自增，255 饱和，并置粘性标志）；
  erase 沿 home→slot 递减，某 class 减到 0 时用一次 64bit 载入检查，全零才清粘性标志。
- 审计方法：临时 TU 用 `#define private public` 暴露内部（不入库），对每个活元素按插入路径重算
  "(组,class) 经过计数"的期望值，与真实 counter 逐字节比较；同时检查标志位一致性、饱和、
  erase-all 归零、copy/assign、以及常量 hash 触发 255 饱和后的行为。
- 结果（clang 20.1.8，WSL，100k 与 1M，多轮 churn）：

| 检查 | 100k | 1M |
|---|---|---|
| 8 轮 churn 中计数与重算值不符 | 0 | 0 |
| 粘性标志 ≠（任一计数非 0） | 0 | 0 |
| 随机 key 出现饱和计数 | 0 | 0 |
| erase-all 后残留计数 / 标志 | 0 / 0 | 0 / 0 |
| copy / assign 后不一致 | 0 | 0 |
| 常量 hash 的饱和分支 | 183 个计数饱和，查找/删除仍全对 | 同 |

  结论：counter 是**精确**的（不是估计），固定规模 churn 下不漂移，erase-all 能回到全零；
  饱和后只会"多探"、不会"漏找"。§5-J 里"不需要 anti-drift"的前提成立。

- 对配套文档 P2 第 11 条（精确 in-home counter，预估再省 6–7%）的评估：用同一套内部访问直接数
  "每次 miss 探测了多少组"（这比 ns 更接近该优化的目标量，100k → 1M）：

| 状态 | 现状 | home 计数（为 0 停在 home，否则回退现状） | 理想下界（知道最远位移） | 只增不减的 home 距离上界 |
|---|---|---|---|---|
| fresh 100k | 1.0985 | 1.0951 | 1.0754 | 1.0754 |
| churn 100k 第 1 轮 | 1.6525 | 1.6100 | 1.3821 | 1.4419 |
| churn 100k 第 8 轮 | 1.6139 | 1.5777 | 1.3680 | **1.9614** |
| fresh 1M | 1.0010 | 1.0010 | 1.0010 | 1.0010 |
| churn 1M 第 2 轮 | 1.0064 | 1.0064 | 1.0063 | 1.0128 |

- 解读：
  - 有空间的是"理想下界"那 ~16%（churn 100k），但它要求 per-(home,class) 的**最远位移**且 erase
    时能精确回退；最便宜的近似"只增不减"第二轮就超过现状、第八轮 1.96 vs 1.61，反而更差
    ——正是 §5-J 删掉 anti-drift 要避免的漂移。
  - 按文章描述"再加 8 个 counter 记录 home 就是本组且溢出"只会把 1.6525 降到 1.6100（−2.6% 组），
    1M 上是 0；代价是每 15 槽多 8B 元数据（元数据 +33%）和每次 insert/erase 多一次 home 组 RMW。
    文章对 F14/ud5 估的 6–7% 在 emihmap4 的三角探测布局上不成立。
  - 结论：**否决，不改代码**。emihmap4 的 miss 终止（粘性标志 + 精确经过计数）已接近该布局上限；
    剩下那 16% 只能靠"精确最远位移"结构，维护成本高于收益。P2 里的 double hashing 本次未测。
- 未入库：审计 TU 是 throwaway（放在 /tmp），验证的是不变量与探测组数，不改头文件。

### Z. 实测否决（默认）：`emihmap4` 的 double hashing 替代三角步进（2026-11-25）

- 做法：`quadratic_prober` 的步长改成 hash 派生的奇数 `((hash >> 32) | 1)`——高位与选 home 的
  `hash >> size_index` 正交，奇数保证遍历所有组。先做"入口算 stride"，再做"只在 `next()` 里算"两版。
- 先用 §5-Y 的审计 harness 直接数每次 miss 的探测组数（100k，int64）：

| 状态 | 三角步进 | double hashing |
|---|---|---|
| fresh | 1.0985 | 1.0629 |
| churn 第 1 轮 | 1.6525 | **1.3665** |
| churn 第 2 轮 | 1.6733 | **1.3419** |
| 1M fresh / churn | 1.0010 / 1.0064 | 不变 |

- 单 map A/B（A = HEAD 三角，B = double hashing，clang 20.1.8 WSL，6 对 × 3 reps 取 min，
  正 delta = B 快）：

| 负载 | delta | 胜场 |
|---|---|---|
| `churn_find_miss` int64 100k | **+13.84%** | 6:0 |
| `churn_find_miss` string 100k | **+9.83%** | 6:0 |
| `insert_erase` int64 100k | **+6.48%** | 6:0 |
| `churn` int64 100k | +3.13% | 2:4（噪声） |
| `find_miss` int64 / string 100k | +3.47% / +3.73% | 3:3 / 1:5（噪声） |
| `insert` int64 / string 100k | −0.71% / −2.85% | 3:3 / 4:2 |
| `find_hit` int64 100k | **−10.80% / −6.18% / −3.81%** | 5:1 / 6:0 / 5:1（三次重测） |
| `find_hit` int64 1M | **−9.22%** | 6:0 |

  （同一二进制自比 `find_hit` 为 +0.02%（2:4），−4~−9% 不是噪声。）
- 指令数（callgrind，100k int64）：`find_hit` 3,786,202 → 4,157,210（**+9.80%**）；
  `churn_find_miss` 5,401,748 → 4,964,997（**−8.09%**），Bcm 113,990 → 97,215（−14.7%）。
- 为什么"只在 `next()` 里算 stride"救不回命中：`(hash >> 32) | 1` 对探测循环是循环不变量，
  clang 用 LICM 把它提到循环外，于是每次 find 都要付这 2~3 条指令（第二版 Ir 4,162,147，+9.94%，
  与入口版本相同）。也就是说，hash 派生的步长必然落在主路径上。
- 结论：**默认否决，落地为 opt-in `EMILIB4_DOUBLE_HASH`**（默认仍是三角步进）。这就是文章说的
  "可交易项，不是纯赢"：`insert_erase` 与 churn 后的负查询稳定 +6~14%，但主路径 `find_hit`
  −4~−9%（+10% Ir），违反"lookup 优先"。开关形态下两者都保留：
  - 默认（不定义宏）：探测循环与 HEAD 逐指令一致（find_hit 3,786,202 / churn_find_miss 5,401,748，Ir 完全相同）；
  - `-DEMILIB4_DOUBLE_HASH`：复现上表（`churn_find_miss` +12.67%、`find_miss` +7.13%、
    `insert_erase` +6.85% 全 6:0，`find_hit` −6.44% 6:0）。
  只建议给"固定规模 churn + 大量负查询"的负载打开。
- 正确性：两种形态都过审计 harness（计数精确、标志一致、erase-all 归零、255 饱和分支、
  与 `std::unordered_map` 的差分）；`test_emilib_lifecycle` 19/19、`test_api_coverage` 284/284
  （开关打开，ASan+UBSan）；clang 20 / gcc 16 的 `-Wall -Wextra -Wpedantic -Wshadow -Werror` 两种形态均干净。

### AA. 已落地：整数键的指纹改用斐波那契乘法（2026-11-25）

- 问题：`std::hash<int>` 在 libstdc++/MSVC 上就是恒等，而原 `fp_from_hash` 用
  `((h>>56) ^ (h>>32)) & 0xFF` 折叠——只有高 32..63 位参与。任何"稠密整数区间"（1..N、ID 段）
  的高位恒定，指纹退化成 **1 种**，SIMD 匹配对同组 16 个槽全部命中，等价于没有指纹过滤。
- 量化（10 万顺序 int64 键，num_buckets=131072）：不同指纹 **1 → 253**；同组内同指纹的其它键
  平均 **15.00 → 0.00**。微基准（同进程取最小，100k/轮）：`find_hit` 9.0~9.3 → 2.2~2.4 ns、
  `find_miss` 11.2~11.4 → 1.8 ns（3.8~6.3x）；混洗键查找持平。
- 落地：
  - **integral key** 的指纹直接对**输入 key** 做斐波那契乘法（`key * 0x9E3779B97F4A7C15` 后取最高字节），
    不复用 `_hasher` 的输出——相当于给指纹单独算一次 hash。`std::hash` 恒等时与"对 hash 乘"逐位相同
    （Ir 与 A/B 均验证完全一致）；自定义整数 hasher 时指纹与桶哈希解耦，乘法还能与 hasher 并行、
    不在它的关键路径上。乘法型/恒等两种自定义 hasher 的 40 万次随机 insert/erase/find 差分测试通过。
  - **非 integral（string / 自定义 key）直接 `EFILLED + hash % 253`**：一次取模、无二次混合。
    253 与 2 的幂互质，所以与 home 组独立；取值天然落在 `[EFILLED, EFILLED+252]`，
    不需要 253..255 的重映射，也顺带修好"自定义恒等 hasher 的非整数 key"（原 xor 折叠同样会退化）。
    string 的 Ir 与 HEAD 差 ≤0.11%：find_hit +395、find_miss −3,680、insert +102,637。
- 代价（callgrind 与 A/B，混洗键 100k，6 对 × 3 reps，clang 20.1.8）：

| 负载 | Ir | A/B delta | 胜场 |
|---|---|---|---|
| `find_hit` int64 | 5,000,675 → 4,802,069（−3.97%） | −0.40% | 4:2（噪声） |
| `find_miss` int64 | 5,271,823 → 5,073,335（−3.77%） | −0.53% | 4:2（噪声） |
| `insert` int64 | 10,944,174 → 10,835,892（−0.99%） | **−6.51%** | 6:0 |
| `find_hit`/`find_miss`/`insert` string | ±0.11% 以内 | −0.52% / −0.34% / −0.60%/+0.90% | 均噪声 |
| `churn_find_miss` int64 / string | — | −0.13% / −1.09% | 4:2 |

- 推广（2026-11-26）：key 侧斐波那契指纹提取为 config.h 的共享 `emilib_fp_from_key(key, EFILLED)`
  并接入 emihmap1/1m/3 的 integral 路径（§5-AD）。实现上试过"乘法 + `%253` 取余"：
  `%253` 编译成魔数乘法链，在查找关键路径上多 ~6-8 周期，emilib1 `find_hit`@100k
  **−13.7%(0:6)**；改回顶字节 `>>56` 折叠（253..255 归入 250..252，仅 3 个值双倍权重）
  后 find_hit@100k **+0.82%(0:6)**、@1M 噪声——所以共享实现取顶字节。顺序/随机键均
  253/253 个不同指纹，与 §5-AA 的量化一致。emihmap1/1m 非整型仍走 xor 折叠的
  `emilib_fp_from_hash`（string 的 std::hash 已充分混合）。

  同源双编对照为 insert +0.39% / find_hit +0.99%，所以 insert 的 6.5% 是真实代价：乘法落在
  "指纹 → `0x01010101` 广播 → `pcmpeqb`"这条链上，而 insert 的组载入命中 L1，链尾等待被暴露。
  字符串用取模后实测在噪声带内（find_hit 复测 −0.52% / −3.34%，insert 复测 −0.60% / +0.90%）。
- 为什么不是"全部 key 都用乘法"、也不是"连 bucket 一起改"：
  - 字符串 hash 已经混好，乘法只加约 3 周期延迟、无质量收益；取模已经能给出均匀指纹，
    所以按 key 类别分流（整数乘法、其余取模）。
  - 把 bucket 也换成 `(h*C) >> (64-k)`（Fibonacci hashing 原式）会把乘法放到**取组地址**的关键路径：
    混洗键 `find_hit` 微基准 4.6 → 6.3 ns（+35%），顺序键也不如只改指纹（4.3 vs 2.4 ns）。已否决。
- 结论：这是**质量修复**，不是纯 A/B 赢家——README 的验收规则在这里不适用，因为现有 bench 的
  int64 key 全部是 splitmix 预混洗的，结构上看不到退化。收益是 identity hash 用户 3.8~6.3x 的查找，
  代价是预混洗整数键 insert −6.5%。
- 正确性：`test_emilib_lifecycle` 19/19、`test_api_coverage` 284/284（ASan+UBSan）；
  clang 20 / gcc 16 的 `-Wall -Wextra -Wpedantic -Wshadow -Werror` 干净。

### AB. 已落地（opt-in）：`emihmap1` 的 indivi 式 32B meta（2026-11-25）

- 做法：`emihmap1m.hpp` 放在独立命名空间 `emilib1m`，可和默认的 `emilib::HashMap` 在同一
  TU 里共存；定义 `EMILIB1_META32` 时 `emihmap1.hpp` 包含它并把 `emilib::HashMap` 别名到
  `emilib1m::HashMap`（保持旧 A/B 构建方式可用）。布局从"15 槽 + 1B
  per-home probe bound"换成 indivi 的 32B meta：
  - 16 槽/组（全部 16 个状态字节都是指纹，不再预留第 16 字节）；
  - 8 个 per-class 溢出计数（class = 存储指纹的 bit0..2，饱和 255）；
  - 8B 距离半字节 = 16 槽 × 4bit（2 槽/字节，饱和 15），记录槽位离 home 组的探测步数。
- 关键机制：
  - find 的终止从"超过 home 组的 probe bound"换成 `oflws[group][cls] == 0`（该类没有任何元素
    经过本组），miss 少扫大量组；
  - 计数在探测过程中**单次**维护：insert 每离开一个满组 +1、erase 每经过一个组 −1；
    落位在已记录的 tombstone 洞时撤销多记的区间；命中已存在 key / erase 未命中时撤销已做的增量；
  - `erase(iterator)` 用"距离半字节倒推 home 组 + 指纹低 3 bit 得 class"，**不再对 key 做哈希**
    （距离饱和到 15 时回退到哈希）。
- 正确性：一次性审计 TU（`#define private public`，不入库）在**每个操作后**重算所有活元素的
  期望计数与距离并逐字节比对，5000 次随机 insert/erase/erase(it)/find 全过；差分压力测试
  （默认/乘法/常量/字符串 hasher，含常量 hash 的深链与饱和分支）全过；6 个单测套件
  （lifecycle/API/crud/copy_move/edge/full）在两种模式下均通过；clang 20 / gcc 16 的
  `-Wall -Wextra -Wpedantic -Wshadow -Werror` 两种模式均干净。
- 单 map A/B（A = 默认 15+1，B = `EMILIB1_META32`，100k，6 对 × 3 reps，clang 20.1.8）：

| 负载 | int64 | string |
|---|---|---|
| `find_hit` | +3.80%（5:1） | **+13.23%（6:0）** |
| `find_miss` | **+58.43%（6:0）** | **+32.69%（6:0）** |
| `churn_find_miss` | **+54.11%（6:0）** | **+52.56%（6:0）** |
| `insert` | −10.99%（6:0） | +5.12%（3:3） |
| `insert_erase` | **−33.83%（6:0）** | −1.38%（噪声） |
| `erase_all` | −14.86%（6:0） | −15.42%（6:0） |

- 为什么写路径这么贵：每个被经过的组都要做一次 counter 的读改写 + 一次距离半字节读改写；
  计数器和距离在 states 之后的独立数组里，等于每次探测多碰 1~2 条 cache line。这正是文章对
  indivi 的结论（"每槽位两字节 + 记账工作"），本仓库实测与之一致。
- 判定：**opt-in**。lookup 收益很大（miss +33~58%，churn 后 miss +53~54%），但写路径 −11~−34%
  不能让默认配置背。默认文件只多了 include 分支与别名模板，未定义宏时与 HEAD 完全一致。
  用法：`-DEMILIB1_META32`（`emilib::HashMap` 变为该变体）；或直接在 ebench / mbench 里按
  `emilib1m` 名字跑（两者都默认注册，ebench 用 `de` 开关）。
- 与第三方的独立性（不是照搬 indivi）：states、per-class 计数、per-slot reach 是同一块内存里的
  **三个平行字节数组**，而不是 indivi 的 32B `MetaGroup` 结构；class 取自**存储指纹的低 3 bit**
  （不是原始 hash 的低 3 bit），所以 emihmap1 自己的指纹推导保持不变、`erase(iterator)` 仍能免哈希；
  探测沿用 emihmap1 的三角/跳跃序列并用 `prev_bucket()` 倒推，而不是 indivi 基于 hfrag 的奇数步长；
  计数由探测遍历**单次**维护（复用墓碑洞、命中已存在 key、erase 未命中时都会撤销已做的增量），
  没有 indivi 的 inc/dec 调用点布局。内部命名（`_passed` / `_reach` / `mark_passed` / `unmark_passed` /
  `slot_reach` / `release_slot`）均为本项目命名，未沿用第三方标识符；仅"每 class 一个饱和计数"这一
  设计思想来自 indivi（MIT）。

### AC. 重设计（opt-in）：`emilib1m` 的 per-slot-pair offset/cnt 探测界（2026-11-26）

- 背景：§5-AB 的"路过计数器"方案走查过程要逐组 mark/unmark、miss/复命中还要撤销，
  写路径 RMW 太多。本次按 emihmap2 的单调 max-offset 思路重做，但下沉到组内、按
  slot 对细分：
  - 32B `GroupMeta { states[16] | offset[8] | cnt[8] }`，字节粒度（不再是 4bit）；
  - `offset[i]` = home slot 为 `2i, 2i+1` 的元素的最大溢出步数（跳跃序列步数，
    饱和 255 = "界未知，走完整循环"）；`cnt[i]` = 该 slot 对的存活溢出元素数；
  - 查找：读一次 `offset[home_slot/2]` 得上界，走查**纯只读**（miss/复命中不碰
    计数器、无撤销）；插入只在真正落位时 `cnt++` + 扩界；删除 `cnt--`，
    **归 0 时 offset 回 0**（探测链收缩，emihmap2 没有的）；
  - `erase(iterator)` 重新哈希 key 定位 home pair 后 O(1) 收尾（换掉 §5-AB 的
    免哈希回退链）；无 tombstone（删除一律恢复 EEMPTY，终止只由 offset 界 +
    ESENTINEL 决定）；cnt 饱和 255 粘滞（offset 只会偏大 = 只损性能不损正确性）；
  - find/erase 快路径：`offset == 0`（绝大多数查找）直接单组扫描，无循环机制。
- 正确性：smoke、ASan+UBSan 压力（默认/乘法/常量/字符串 hasher + churn/复制/
  clear/swap，两种模式）、常量 hash 饱和对抗（链 > 255 组，cnt 粘滞下增删查
  全量比对）全过；gcc/clang `-Werror` 全家干净。
- 单 map A/B（A = 默认 15+1，B = `EMILIB1_META32`，6 对 × 3 reps）：

  | 负载 | 100k | 1M/4M |
  |---|---|---|
  | `find_hit` | −10.4%（0:6） | −18.6% @4M（0:6） |
  | `find_miss` | **+72.3%（0:6）** | −43.5% @4M（0:6） |
  | `insert` | **+152.9%（0:6）** | **+30.2% @1M（0:6）** |
  | `insert_erase` | −12.2%（0:6） | −27.9% @1M（0:6） |

- 分析：miss@100k 快是纯分支预测收益（Ir 反而 +11%，Bcm 64K→16.7K）；4M 的
  find_miss/find_hit 大退是 meta 16MB 正好挤掉 L3（默认 8MB 能放进一个 CCX），
  1M（meta 4MB）时 miss 仍有 −19.6% 纯指令开销；insert 大胜来自走查只读化
  （不再逐组 RMW）；insert_erase 的 erase(key) 已强制内联（gcc 曾漏内联，
  82 Ir/次）。跳跃 vs 线性步进实测无差（差异来自指令数与缓存，不是序列）。
- 判定：**继续 opt-in，不进默认**。miss@100k 与 insert 的大胜换来大表 hit/miss
  与 churn 的回退；1M 上下 miss 还需 +12 Ir 的簿记优化（如 offset 按半字节）才
  有机会转正。

### AD. 已落地：`emilib` 公共 config.h（2026-11-26）

- 新建 `include/emilib/config.h`，收编各容器头文件重复的全局作用域代码：
  编译器/arch 头（x86intrin/sse2neon）、`EMH_LIKELY/EMH_UNLIKELY`、
  `EMH_RESTRICT`、`CTZ`、4KB 指纹广播表 `fp_broadcast`（原来 emihmap1/1m
  各一份，现整个 TU 一份）、xor-fold 指纹 `emilib_fp_from_hash(hash, EFILLED)`、
  MSAN 去毒哈希入口 `emilib_hash_of(hasher, key)`。
- emihmap1/1m/2/3 完整接入；emihmap1d/4/emihset2/3 暂不动：emihset2 的 arch 块
  不同（zmmintrin）、`set_simd_*` 命名不同；emihmap2/emihset2 同在 `emilib2`、
  emihmap3/emihset3 同在 `emilib3`，namespace 级实体（State、simd 常量、EM3_*
  宏）不能安全共享，只共享全局作用域代码（config.h 头注释已写明约定）。
- 顺带修复真 bug：emihmap1m/emihmap2 非整型 key 的 `key_hash % 253` 直接转
  int8_t 时 ≥128 的值回绕成 −128~−4，正好撞上 EEMPTY/EDELETE/ESENTINEL，
  string key 会破坏状态字节（smoke 段错误、UBSan 报错）；已改为
  `key_hash % 253 + EFILLED` 映射进 [EFILLED, EFILLED+252]。
- 验证：严格警告（gcc/clang，含同命名空间组合 map2+set2 / map1+set3 / META32
  别名）全过；emihmap1/2/3 int+string 功能检查（两种模式）、emilib1m 全套
  （smoke / ASan+UBSan 压力 / 饱和对抗）全过；find_hit@100k sanity 基准
  −10.6%（重构前 −10.4%，噪声内，纯重构零影响）。

### AE. `emilib1m` 收尾：rehash 内联快路径 + offsets 4bit→8B（2026-11-26）

- **rehash 内联快路径（201155c，已落地）**：callgrind 显示 insert 负载的
  `find_empty_slot` 64 Ir/次 × 10.9 万次调用里 94% 来自 rehash 重插
  （10.9 万 vs 插入路径 0.7 万）。rehash 重插循环内联 home 组扫描
  （LF 0.417 时 99.7% 命中 home 组，满组 <0.4%），只有满组才走通用
  `find_empty_slot`。insert 负载 Ir 23.98M → 19.03M（−20.7%）。
  交叉核实：emilib1/2/3 的 rehash 重插本来就是内联的（无独立函数入口），
  emilib4 的 `rehash_impl` 是刻意的 NOINLINE 冷路径设计，问题仅 emilib1m 存在。
- **offsets 4bit→8B（已落地）**：`offsets[4]`（每字节 2 个 nibble）改为
  `offsets[8]`（每 slot 对 1 字节）。恰好填满原来 `alignas(16)` 的 4B padding，
  `GroupMeta` 仍 32B，零额外内存；饱和从 15 → 255；查找路径省掉 nibble
  的 shift/mask 提取。语义不变（0 = 从未写、1 = 全在 home 组、k = 链到 k−1
  组外、255 = 走完整循环），`set_offset` 仍是单调 max。
- 同批 A/B（A = 默认 15+1，B = `EMILIB1_META32`，6 对 × 3 reps，clang 20，
  int64，负数 = B 更慢；两版同日各跑一遍）：

  | 负载 | 规模 | 4bit nibble (201155c) | 8B offsets | 结论 |
  |---|---|---|---|---|
  | `find_hit` | 100k | −19.7% | **−15.4%** | 8B 好 4.3pp |
  | `find_hit` | 1M | −21.5% | **−14.4%** | 8B 好 7.1pp |
  | `find_miss` | 100k | +61.3%（B 快） | **+75.4%（B 快）** | 8B 赢更多 |
  | `find_miss` | 1M | −18.2% | **−11.6%** | 8B 好 6.6pp |
  | `insert` | 100k | −11.4% | **−9.3%** | 8B 好 2.1pp |
  | `insert` | 1M | −11.0% | **−8.4%** | 8B 好 2.6pp |
  | `insert_erase` | 100k | −18.7% | **−12.7%** | 8B 好 6.0pp |
  | `insert_erase` | 1M | −23.8% | −24.3% | 持平（±0.5pp） |
  | `churn` | 100k | −14.1% | **−8.3%** | 8B 好 5.9pp |
  | `churn` | 1M | −24.2% | −24.4% | 持平 |

  ebench 1M 综合指数 117.7% → 117.4%（持平）。
- 分析：字节读/写在 hit 与写路径全面小赢，且没有任何一项显著变差；
  `insert_erase`/`churn`@1M 的 ~−24% 两版一致，是 1M 规模下 meta cache miss
  的结构性开销，与 offsets 宽度无关。8B 无代价换走查范围 15→255 组
  （常量 hash 深链由"全循环回退"变"精确界"），判定保留。
- 验证：严格编译 4 组合全过；smoke / ASan+UBSan 压力 / 饱和对抗全过
  （const-hash 5k 饱和链 maxoff=255 全量找到，churn/reinsert 正常）；
  头注释同步（28B→32B、nibble/15→byte/255）。
- **插入链内联从 clang-only 改为无条件（同日）**：全套 ebench 记分板
  （gcc，-O3 -march=native）emilib1m 55/159.3% 与 emilib4 并列垫底，
  gcc/clang 双编译对照定位差距来源——`insert_hit`（operator[] 命中）
  gcc +43% vs clang +7%，`insert_l3_cache`/`insert_no_reserve` 也差
  ~25%：大 TU 下 gcc 把 `find_or_allocate` 留在行外。`EMILIB1_INSERT_INLINE`
  改为无条件 `EMH_INLINE` 后 ebench 相对差距 22%→12%（3 轮一致）；
  gcc framework 复测不强制反而全面更差（insert −25/−21.8%、
  churn@1M −40.2% vs 强制后 −19.8/−16.3%、−19.9%）——§5-V"gcc 强制
  回退 4-8%"的结论在 emilib1m 上不成立（该结论出自默认 map 的 TU 形态）。
  find_hit 的差距（clang +33%）是路径本身的结构成本（offset 在 states
  词外 + 跳跃序列），内联救不了，维持 opt-in 判定。

### AF. `emilib1m` 补充实验：走查 prefetch 否决（2026-11-26）

- **走查 prefetch（否决）**：find_filled_bucket / find_or_allocate /
  find_empty_slot 三个走查循环加 lead-1 下一跳 states 预取。全量 A/B
  14 项混在 ±3pp 噪声内（insert@1M −7.4%、churn@1M −22.4% 与基线
  −8.4%、−24.4% 相差 1~2pp，方向不一）——走查平均只有 1~2 组且大多
  L1/L2 命中，预取没有可 hiding 的长延迟。已回退。

### AG. 已落地：`emihmap2` 整数键的指纹改用斐波那契乘法（2026-11-26）

- 背景：§5-AA 给 emihmap3/§5-AD 给 emihmap1/1m 接入了 key 侧斐波那契指纹，唯独
  emihmap2 的 integral 路径还留在 xor 折叠 `((h>>56)^(h>>32)) & 0xFF`——对
  identity hash 的稠密整数键同样退化成 **1 种指纹**（同组 16 槽全命中，
  等价于没有指纹过滤）。本次补齐。
- 改动：`emihmap2::hash_key2` integral 重载改为 `emilib_fp_from_key(key, EFILLED)`
  （config.h 共享实现，乘法 + 顶字节折叠）；非整型路径 `% 253` 不动；
  原来只有 integral 在用的 `fp_from_hash` 包装随之删除。
- 效果（callgrind Ir，100k int64 混洗键，clang 20）：
  - find_hit 4,480,642 → 4,180,626（**−6.7%**）；find_miss 3,865,872 →
    3,567,862（**−7.7%**）；insert 11,508,103 → 10,989,208（**−4.5%**）。
  - wall-clock A/B（6 对 × 3 reps）：find_hit +4.7~5.9%（5:1 / 6:0）；
    find_miss / churn / erase_all / 1M 全在噪声内；insert_erase
    **−1.6~−2.3%（6:0 两次）**，是唯一一致的小代价（乘法落在
    "指纹 → 广播 → `pcmpeqb`"链上，与 §5-AA 在 emihmap3 上的 −6.5% 同源）。
  - 顺序键微基准（10 万顺序 int64，identity hash，同进程取最小）：
    不同指纹 **1 → 253**；find_miss **14.83 → 2.50 ns（5.9x）**、
    find_hit 2.50 → 2.22 ns（+12%）——质量修复主场景。
- 判定：**保留**（与 emihmap1/1m/3 同策略：质量修复 + 混洗键下的
  find/Ir 净收益，写路径小代价）。
- 正确性：emilib2 相关 11 个单测套件（lifecycle/api_contract/crud/full_api/
  iterators/copy_move/reserve_clear/string_keys/edge_cases/stress_correctness/
  hashset）gcc16 + clang20 全过；lifecycle + stress 的 ASan+UBSan 通过；
  严格警告 6 组合（gcc16/clang20 × 默认/AVX2/AVX512，map2+set2 同 namespace
  共编）`-Werror` 干净。

### AH. 已落地：`emihmap2` rehash 重插的 home 组内联快路径（2026-11-26）

- 背景：§5-AE 在 emilib1m 上验证过同款手法（rehash 重插循环内联 home 组扫描，
  满组才走通用探测，insert 负载 Ir −20.7%），且核实过 emilib1/2/3 的 rehash
  重插当时就是内联的——emilib2 的问题不是"没内联"，而是 **TBAA 别名**：states
  是 `int8_t`（signed char）数组，其对任意对象的写都要求编译器重载堆上的
  `src_pair`/`old_states` 读取。
- 改动（`emihmap2::rehash` 重插循环）：
  - 每个重插元素先把 `PairT src(std::move(...))` 拷进寄存器驻留的局部变量，
    之后所有 store（set_states / placement new）都不再污染 `src.first` 的读取；
  - home 组空槽扫描内联进循环（rehash 时新表 LF ≈ 0.42，空槽绝大多数在
    home 组内），满组才回退到通用 `find_empty_slot`；落在 home 组意味着
    offset 簿记为 0，无需 `set_offset`。
- 效果（callgrind Ir，100k int64，clang 20）：insert 10,989,208 →
  **10,438,995（−5.0%）**；erase_all（含 build+erase）15,972,118 →
  **14,602,935（−8.6%）**；find_hit 不变（4,180,626）；Bcm 持平
  （42,147 → 42,324）。
- wall-clock：此刻主机噪声大（同一对二进制 insert 复测 6:0 互反，
  A 端漂移 ±10%），insert A/B 不可信；按纪律以 Ir 裁决保留。
- 判定：**保留**。
- 正确性：同 §5-AG 的 11 套单测 + ASan+UBSan + 严格警告 6 组合全过。

### AI. 已落地：`emihmap2` 管线化 rehash（16 深环 + 目标组预取）（2026-11-26）

- 手法同 emhash9（§5-R）：重插循环拆成生产/消费两段，生产段领先 16 个元素算
  hash 并预取新表的 `_states[home]` 与 `_pairs[home]`（两个区域各一条），消费段
  内联 home 组扫描（§5-AH 的快路径保留）、落位、写 states、placement-new。
  源表是逆序顺序走查，硬件预取友好；随机访问只发生在新表一侧，交给软预取。
- 成本：Ir 明显上升——insert@100k 10,438,995 → 14,376,815（**+38%**）、
  erase_all +26%（环簿记 + 每个重插元素多读一次 `old_pairs[src].first`：
  生产段取 hash、消费段再 move 一次）。
- 收益（wall-clock，6 对 × 3 reps，B = 管线化，全 0:6 一致）：

  | 负载 | 100k | 1M | 4M |
  |---|---|---|---|
  | insert int64 | 噪声（−0.20%，3:3） | **+2.36%** | **+1.71%** |
  | erase_all int64（含 build） | **+2.79%** | **+2.43%** | — |
  | insert string 1M | — | 噪声（+0.49%，2:4，hash 主导） | — |

- 分析：Ir +38% 在墙钟上不可见，因为预取消除的 cache miss 远贵于多出的指令
  ——与 emhash9 管线化的经验一致（Ir 不是缓存内规模的裁判）。@100k insert
  打平不回退：rehash 只占 insert 负载一部分，其余被 erase_all@100k 的 +2.79%
  覆盖。
- 判定：**保留**。
- 正确性：同 §5-AG 的 11 套单测 + ASan+UBSan + 严格警告 6 组合全过。

### AJ. 实测否决：`emhash9` erase_key 的 moved-hash 上提（2026-11-26）

- 热点盘点（callgrind，100k int64）：emhash9 `erase_all` 24.8M Ir = build
  15.1M + erase ≈9.7M（**≈97 Ir/erase**：find_slot 探测 + moved-last 的
  `slot_of_value` 第二次探测 + `unmark_path` + `erase_slot`）。两次探测是
  swap-delete 设计的固有成本（emilib2 的 tombstone 方案只需一次探测 +
  打标记，但 churn-miss 换成扫墓碑）。
- 尝试：把 `hash_key(_pairs[last].first)` 提到 find_slot 之前发射
  （`_num_filled - 1` 不依赖探测结果，与 `erase(const_iterator)` 已有的
  "up-front hash" 同款），让 string 的 hash 延迟藏在探测下面。
- 实测（6 对 × 3 reps）：erase_all string 100k −0.92%（3:3）、1M +0.24%
  （2:4）、int64 100k −0.40%（5:1）、insert_erase int64 +0.66%（1:5）
  ——全部在噪声带内。探测本身的 cache miss 主导，hash 延迟上提无可 hiding。
- 判定：**否决并回退**（子 2% 不可复现，按纪律不动）。emhash9 的 erase
  路径结构上已到头：iterator 版已有坐标 hint，key 版的两次探测与
  unmark_path 是设计代价，不是微优化空间。




---

## 6. 本仓库的落地顺序（按"收益/风险"）

| 顺序 | 动作 | 落点 | 预期 | 判定 |
|---|---|---|---|---|
| 1 | overflow bit → 8×1B counter，erase 递减 | `emihmap4.hpp`（已落地；§5-Y 复核：计数精确、8 轮 churn 不漂移、erase-all 归零、饱和分支正确；精确 in-home 变体 −2.6% 组 → 否决） | churn 不再周期性重建；miss 分支 -60% | `find_miss` 周期 + churn ns |
| 2 | 预广播指纹表 | emhash9/emihmap4 原有；`emihmap1` 已落地（§5-U）：int miss +2~2.8%(6:0)、Ir −3/查找；`emihmap2/3` 实测负收益 → 否决（§5-U） | 文章整型 miss +5–6%；本仓库 emihmap1 只有 +2~2.8% | 指令数 −3 条/查找 + 6 对 A/B |
| 3 | 全局 probe bound：emhash9 换成可递减 counter；emihmap3/4 保持全局 bound | `emhash9`、`emihmap3/4` | emhash9 miss +10.6%（已落地）；emihmap3 per-home 深度读 −2.6~8.7%、写 +2.8~7.1%，净否决；per-class(8) bound 读 −1.8~3.1%、churn −6.4%、insert −25%，Ir +3.7%，否决（§5-X） | `find_miss`/`find_hit`/churn 单 map A/B + 构造 key 不挂死 |
| 4 | rehash 消别名 + 平凡可拷贝值整块 memcpy | `emhash9`（已落地，§5-Q）：200k +2.4~3.1%(6:0)、1M/string/WSL 噪声；`emhash8` 实测否决（§5-S） | clang 单次放置 3.8x、200k build 1.87x；gcc 无变化 | 单 map build ns |
| 5 | 管线化 rehash（16 hash 环 + prefetch） | `emhash9`（已落地，§5-R）：1M int +18~20%(6:0)、4M int +27%、string +12~23%，clang20/gcc16 也 +7~17%；`emhash8` 实测否决（§5-S）；`emihmap2` 已落地（§5-AI）：1M insert +2.4%、4M +1.7%、erase_all +2.4~2.8%（全 0:6），Ir +26~38% 换 cache miss 消除 | 缓存内 1.26x；4M string 2.5x | 单 map build ns / dTLB |
| 6 | move_home（仅写路径命中） | `emhash9`（已默认开启）；`emihmap3` 实测 hit +5.6% / miss −2.4% → 否决（§5-O） | churn miss 1.26–1.30x | 随机 key 固定规模 churn |
| 7 | 单 map 单 workload 二进制 + 指令计数脚本 + churn/octave | `bench/framework`（已落地，§5-T/§5-U）：`bench_single.cpp`、`build_single.sh/.ps1`、`instr_count.sh`、`churn`/`churn_find_miss`、`--octave` | 让 <10% 的结论可信；churn 可测"churn 对查找的影响"而不只是写开销 | 单 map A/B + callgrind Ir |
| 8 | `always_inline` 审计（两个 TU 都测） | `emihmap1/2` 插入链已落地（clang-only，§5-V）；`emilib1m` 插入链无条件强制（§5-AE）：大 TU 下 gcc 也不内联（ebench insert_hit +43%），强制后 ebench 差距 22%→12%，且 gcc framework 强制反而全面更好（churn@1M −40%→−20%）——"gcc 强制回退"不适用于 emilib1m。`emihmap3` 整套实测 insert −21% → 否决；`emihmap4`/`emhash9` 无遗漏内联 | 两个方向都可能；本仓库只剩插入链一处 | 小 TU + 大 TU、clang + gcc 四组 |
| 9 | hash 延迟评估（可选，只在 string 负载） | `bench/framework`（已全面评估，§5-T/U/W）：9 maps × 3 ops × Windows/WSL × 5 key 长度（伪随机 key）；**所有长度全胜**，8B 也有 +4~49%；`emhash9::string_hash` 已 opt-in（默认不变）；`EMH_WYHASH_HASH=1` 的 wyhashstr 比 functor 慢 4~22% → 不用 | Windows/MSVC 32B：find +63~209%、insert +13~44%；libstdc++ find +9~45% | 整表 A/B 全 6:0；Ir −23~28% |
| 10 | 迭代器缓存组指针（省掉每步的地址取整） | `emihmap4.hpp`（已落地）；`emihmap1-3` 实测否决（§5-N） | iterate +15~39%（emihmap4）；emihmap1-3 无净收益 | 单 map iterate A/B |
| 11 | 整数键指纹：Fibonacci 乘法（修 identity hash 退化） | `emihmap3.hpp`（已落地，§5-AA）与 `emihmap2.hpp`（已落地，§5-AG）：顺序 int64 指纹 1→253、find 3.8~6.3x / miss 5.9x；混洗键 `insert` −6.5%(6:0) / `insert_erase` −1.6~2.3%(6:0)，find_hit +4.7~5.9%(6:0)，查找与字符串不动 | 质量修复，不是纯 A/B 赢家 | 指纹多样性 + 顺序键微基准 + 常规 A/B |
| 12 | emihmap1 的 32B meta：v1 indivi 式（per-class 计数 + 4bit 距离，§5-AB）；v2 重设计为 per-slot-pair offset/cnt 探测界（§5-AC）：走查只读、cnt 归 0 收链、erase(it) 免回退链；v3 rehash 内联快路径 + offsets 8B/槽对（§5-AE） | `emihmap1.hpp` + `emihmap1m.hpp` + `emilib/config.h`（opt-in `EMILIB1_META32`）：v1 `find_miss` +33~58%、写 −11~−34%；v3（8B offsets）`find_hit` −14~15%、`find_miss`@100k +75%(6:0)、`insert` −8~9%、ebench 1M 117.4%；rehash 内联另省 insert Ir −20.7% | 不改变默认；小表 miss 为主或插入为主的负载，大表回退待簿记优化 | 单 map A/B（多尺寸）+ callgrind Ir + 饱和对抗 |

---

## 7. 验证工具箱（照文章的纪律来）

1. **交错 A/B**：`bench/framework/ab.ps1`（6 对，看 unanimous winner）；两段独立测量不可信。
2. **一个 map 一个二进制**：`bench_main.cpp` 把所有 map 编进一个 TU，属于"大 TU"，
   会改变内联/布局；小于 10% 的结论要另建只含一个 map 的小程序——
   `bench/framework/build_single.sh --maps X --ops Y` 会编出 `bench_single.cpp`
   （一个 map × 一个 workload），产物可直接喂给 `ab.sh`/`ab.ps1`。
3. **指令数**：`valgrind --tool=callgrind --dump-instr=yes`（Windows 上可用 WSL）；
   它不受代码布局影响，是唯一能裁决小差异的量。也可用 `llvm-mca` 看依赖链。
   现成脚本：`bench/framework/instr_count.sh <single-binary> --key ... --size ...`
   （`build_single.sh --callgrind` 的产物）。三个坑：必须用
   `START/STOP_INSTRUMENTATION` 把 setup/teardown 排除（string map 的析构比查找还贵）、
   必须 `-gdwarf-4`（3.18 不认 DWARF5，否则全归到 `__libc_start_main`）、
   必须读 `totals:` 而不是被 stop 弄坏的 `summary:`（§5-T）。
4. **硬件计数器**：`perf stat -e cycles,instructions,branch-misses,L1-dcache-load-misses,dTLB-load-misses`；
   Windows 上退而用 callgrind 的 Ir/Bc/Bcm。
5. **五规模、五次、取中位数**：单点会踩锯齿（最坏 1.84x；文中同一实现 57 个规模波动 2.91x）。
   五次中任意去掉一次，整型比值变化 ≤4.3%、大 value ≤3.0%、string ≤6.8%。
6. **churn 用随机 key、固定规模**；顺序 key 会把 drift 低估约 4x（文中自己的勘误）。
   现成 workload：`--op churn`（固定规模 erase 旧 key + insert 新 key）与
   `--op churn_find_miss`（计时区外做一整轮 churn，再测 miss）；`--suite --octave`
   一次给 1k/10k/100k/1M/4M 五档。三套 key 按构造不相交，不会"复用刚删掉的 key"。
7. **不要重复测量序列**：每轮的 key 批次若小到可记忆，分支预测器会背下命中模式（实测 2.7x，足以反转排名）。
8. **别在 `-march=native` 下 profile**：SSE2 intrinsic 会被换成 AVX-512，测到的不是发布代码。

---

## 8. 一句话版本的"该先做什么"

0. **先确认热路径真的被内联了**：emilib2o 只是把 `find_filled_bucket`/`find` 加上 always-inline，
   find 就快了 30–40%（§5-I）。这一步零风险、零语义变化，应该排在所有算法改动之前；
   判据是"单 map 小 TU 的指令数"，不是大 TU 的耗时（§5-F1）。
1. 再把 **miss 终止**做成"一条 `cmp byte,0`"（counter 化），这是 emilib 与 boost/ud 最大的差距来源。
2. 然后把 **指纹广播** 换成查表或高字节指纹，压掉关键路径上的 4–5 条指令。
3. 最后才轮到 prefetch、布局、rehash 管线、move_home —— 它们都是几个百分点，
   而且必须在"一个 map 一个二进制"下用指令数确认，否则会被 TU 布局噪声骗。

