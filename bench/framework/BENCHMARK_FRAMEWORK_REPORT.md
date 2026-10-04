# 基准体系固化 — 工作总结报告

**日期**：2026-09-19
**范围**：`bench/framework/` 新建，不含 `bench/` 既有历史文件
**状态**：已完成并通过端到端验证

---

## 1. 背景与动机

项目既有基准设施是碎片化且部分失效的，导致每次性能优化都要重新搭测量脚手架，且结果无法跨会话比较。调研确认的现状：

| 现状 | 证据 | 问题 |
|---|---|---|
| CI 门禁走 Google Benchmark | `tests/CMakeLists.txt` 经 `FetchContent` 拉 v1.9.4；`ci.yml` 用 `benchmark_results.json` + 20% 阈值 | 需联网，离线/沙箱环境直接失败 |
| `bench/emilib_bench/` 是死代码 | `bench_registry.hpp` 与 `main.cpp` include 了仓库中不存在的 `emihmap2_v2.hpp` | 设计完整但无法编译 |
| `bench/` 根目录 50+ 历史文件 | 手写 `Makefile`，无统一框架，公共头仅 `util.h` | 结果不可比、无方法学 |
| A/B 配对方法学只存在于文档 | `include/2026-07-08_1030-map-performance-optimization.md`（handoff 文档）记录的方法学未被固化 | 方法学未沉淀，每次重做 `±8%`/`>10%` 噪声的坑 |

核心痛点来自 handoff 文档的测量结论：同一二进制反复运行波动 **±8%**、主机数分钟漂移 **>10%**，朴素的"改前一次性测、改后一次性测"对比**必然**产生假阳性/假阴性。

**目标**：为 emhash 核心容器建立单一、可复现、零依赖的基准设施 + 配对 A/B 对比工具，作为后续所有性能工作（运行时 ISA 分发、SIMD 分组探测等）的验证基础。

---

## 2. 决策

| 维度 | 选择 | 理由 |
|---|---|---|
| 框架 | nanobench v4.3.7（vendored 单头） | 零依赖、无需联网、秒级编译，与 handoff 方法学一致 |
| 改动范围 | 精简版（只新增，不碰历史文件） | 风险最低、可快速落地 |
| 覆盖容器 | emhash5/6/7/8 + emilib1/2/3/4，boost/absl 参照基线 | 覆盖主力实现，基线可选可关 |

---

## 3. 交付物

新增目录 `bench/framework/`：

| 文件 | 职责 |
|---|---|
| `bench_framework.hpp` | 6 个自包含工作负载、可复现键生成、hit/miss 键按构造不相交、测量参数 |
| `bench_main.cpp` | CLI 驱动器 + 容器注册表（唯一定义 `ANKERL_NANOBENCH_IMPLEMENT` 的 TU） |
| `CMakeLists.txt` | 独立构建；boost/absl 基线按头+源文件存在性自动启停 |
| `ab.sh` / `ab.ps1` | A/B 配对对比器（bash 主实现 + PowerShell 镜像） |
| `README.md` | 方法学、用法、实测噪声数据 |

顶层 `CMakeLists.txt` 新增一处：在 `if(WITH_BENCHMARKS)` 块内 `add_subdirectory(bench/framework)`。

### 工作负载设计（自包含单元）

`Bench::run(name, op)` 无 pause/resume 且逐 epoch 重复调用 `op()`，因此每个工作负载都是完整、幂等、自包含的单元：

| 工作负载 | 测量内容 |
|---|---|
| `insert` | 建含 n 键的 map，不 reserve（含 rehash 成本） |
| `find_hit` / `find_miss` | 命/未命中查找（对预建 map） |
| `iterate` | 累加全部 value |
| `erase_all` | 建 n + 删 n（含构建成本） |
| `insert_erase` | 稳态：n×(erase+insert)，维持 n 存活 |

**hit/miss 键按构造不相交**（整数分两个不重叠区间、字符串按索引错位），而非依赖"运气好的种子"。`erase_all − insert` 提供纯 erase 的合理推导值。

---

## 4. 实现中定位并解决的问题

### 4.1 nanobench 单位是秒，不是纳秒
首次数值输出 `0.001`。追查到 `nanobench.h` 的 `detail::d(Clock::duration)` 转成 `std::chrono::duration<double>`（ratio=1），`Measure::elapsed` 以**秒**计。补 `* 1e9` 后数值全部合理。

### 4.2 自校验抓出对比器自身的系统性偏差（最有价值）
同一二进制互为 A/B 时，最初出现 **5:1 胜率、-3.73% 差异**。根因：每对里 A 总是先跑，吃到首次运行的缓存/睿频优势。修复：**配对内交替 A/B 的运行顺序**。修复后自校验降为 3:3、-0.33%。

### 4.3 absl 基线开箱即报链接错误
`EMH_BENCH_HAVE_ABSL` 仅凭头文件存在就启用会导致未定义符号（`GetHashRefForEmptyHasher`、`MixingHashState::kSeed`）——vendored absl 的 `flat_hash_map` **不是 header-only**。修复：同时要求 4 个 `.cc` 源文件存在（`raw_hash_set.cc`、`hash.cc`、`city.cc`、`low_level_hash.cc`），缺任一则跳过基线而非报错。boost 基线开箱可用。

### 4.4 量化主机噪声并据此调参
8 次独立测量同一工作负载：558700–604350 ns（±4%），单次 harness 调用约 43ms。因此把 A/B 默认 `reps` 从 1 提到 3（min-of-three），成本可忽略、鲁棒性明显提升。另记录到主机偶发漂移（某轮全部跳到 ~690k ns，导致假 1:5），印证了"需近乎全胜才可信"的判定规则。

### 4.5 PowerShell 计数 bug（次要）
PS5 下单对象的 `Where-Object.Count` 返回空。改用 `@(...)` 强制数组后再取 `.Count`。

---

## 5. 验证结果

| 项 | 结果 |
|---|---|
| 独立 CMake 构建 / 顶层 `-DWITH_BENCHMARKS=ON` 集成 | 均成功，**零警告** |
| 8 容器 × 3 键注册 | 全部可用 |
| boost / absl 参照基线 | 实测可用（absl 约 4.95ns/op 快于 boost 7.4ns/op） |
| 单点模式裸数值契约 | 仅输出一个 float，退出码规范（错误 2 / 成功 0） |
| 套件模式 + 推导 erase + JSON | 输出正确 |
| 自校验（同一二进制 A=B） | int64：4:2 / −1.91% → NOISE；string：5:1 / −0.29% → NOISE |

### 数值合理性抽样（int64，size=100k，本机）
- insert：5.58 ms / 100k ≈ 56ns/元素
- find_hit：0.67 ms / 100k ≈ 6.7ns/次
- iterate：0.11 ms（纯读，最快）
- erase（推导 `erase_all − insert`）：约 1.9 ms / 100k

量级与 `docs/performance_tracking.md` 记录的基线一致。

---

## 6. 方法论沉淀（写入 README）

1. **交错**运行 A/B，绝不"先全 A 再全 B"。
2. **交替**每对内的先后顺序（两脚本自动处理），否则产生了假胜率。
3. **默认 ≥5-6 对、≥3 次**。2 对时幸运跑曾产生假 +10%+ 判定。
4. **近乎全胜才可信**；<2% 均值差异且胜率五五开 = 噪声。
5. **先用同一二进制自校验工具**（应得 NOISE）；任何系统性偏向说明工具或机器有问题。

---

## 7. 明确不在本次范围内

- 不修复/归档 `bench/emilib_bench/`（缺 `emihmap2_v2.hpp`，属后续"中等版"）。
- 不收敛 `bench/` 根目录 50+ 历史文件（属"较大版"）。
- 不改动 CI 门禁（不把微基准接进 `ci.yml`）。
- 不覆盖 set2/3/4/8 与 lru_size/lru_time。

---

## 8. 后续建议

- **即刻可用的场景**：用 `ab.sh` 验证运行时 ISA 分发、SIMD 分组探测等性能改动的真实收益（正/负均以近乎全胜为准）。
- **基准自动化**：将该框架接入 `ci.yml` 作为现有 Google Benchmark 门禁之外的第二道更灵敏的信号（需先处理其离线构建约束）。
- **清理历史负债**：修复或归档 `bench/emilib_bench/`；收敛 `bench/` 根目录散落文件。
- **覆盖补齐**：set2/3/4/8 与 lru 系列在核心容器验证稳定后纳入矩阵。

---

## 附：本任务完成的代码改动清单

- 新增 `bench/framework/`（6 个文件，约 47 KB 源码）
- 修改 `CMakeLists.txt`（1 处：`add_subdirectory(bench/framework)`）
- 未改动任何 `include/` 下哈希实现；未改动 `tests/` 与 CI。