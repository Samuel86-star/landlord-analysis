# 最少手数拆牌 vs 牌力值最大拆牌 —— 等价性验证方案

> 验证 `optimalSplit`（字典序：先最小手数、再最大 Σscore）是否恒为「牌力值最大拆牌」的真解。
> 仅指标期/离线校验，不影响线上发牌、配牌、洗牌管线。

## 一、问题背景

仓内有两套拆牌/牌力逻辑：

- **最少手数拆牌** —— `algorithm/native/extracted/optimal_split.h` 的 `optimalSplit(hand)`。记忆化穷举 DFS，目标为**字典序**：
  1. 先最小化组合数 `n`（即「手数」）
  2. 等 `n` 中最大化 `Σscore(combo)`（用 `DefaultComboScoringStrategy::score` 的裸分加和）
- **牌力值** —— `algorithm/native/include/landlord.h` 的 `calcTotalHandScore(hand, combos)`：

```text
牌力 = Σscore(combo) − penaltyPerCombo·(n−1) + controlBonus
```

其中 `penaltyPerCombo`（`config/scoring.properties`，默认 `8.0`），`controlBonus` 为控制牌加成（大小王、双 2、持有炸弹/王炸）。

待验证命题：**`optimalSplit` 产出的拆牌，是否总是「所有合法拆分中牌力最大者」？**

## 二、理论关系（已推导）

### 2.1 两个目标本不相同

`optimalSplit` 是「先 min-n、再 max-Σscore」的字典序目标；「牌力最大」是 `max(Σscore − ppc·(n−1) + controlBonus)`。二者目标函数不同，**不因构造而恒等**。

### 2.2 牌力最大 = max Σ(score − ppc)

`controlBonus` 仅依赖整手牌、与拆法无关（`landlord.h` 注释明示「与拆牌方式无关」）；`ppc` 是常量。故对**固定一手牌**，三项常数在比较中抵消：

```text
argmax(牌力) = argmax( Σscore_i − ppc·n ) = argmax Σ( score_i − ppc )
```

即「牌力值最大拆牌」等价于**每个牌型权重记 `score − ppc` 的最大权拆分**。

### 2.3 分歧的充要条件

在最小手数集合内，`牌力 = Σscore − ppc·(n_min−1) + const` 随 `Σscore` 单调，而 `optimalSplit` 恰在 min-n 内取 max Σscore。因此：

- **min-n 集合内两者必然一致**（都取 max Σscore）。
- **分歧只可能来自 `n > n_min` 的拆分**：若存在某拆分比 min-n 最优解的 `Σscore` 高出 `> ppc`，则它牌力更高。
- 单张 `score=0`、权重 `−ppc`，过拆天然受罚；故分歧**罕见，但不被构造保证**，完全取决于评分配置数值。

### 2.4 约简空间比较（无需算 controlBonus）

比较只需在约简空间进行，`controlBonus` 两边同手牌抵消：

```text
reduced_opt = Σscore(optimalSplit) − ppc·n_opt
maxPower    = max over ALL splits of Σ(score_i − ppc)     ← power-DFS
diff        = maxPower − reduced_opt        ← 恒 ≥ 0；diff > ε 即反例
```

`diff < 0` 永不应出现 → 程序内置 sanity 断言。

## 三、验证方案（思路 A：值对比）

对随机均匀采样的 N 手牌，分别算 `optimalSplit` 的约简牌力与全局最大约简牌力，比较二者：

- `diff > ε` → **反例**：dump 手牌、两套拆分、`n_opt`/`n_max`、两边 Σscore。
- 汇总：分歧数 / N、`diff` 分布、`n_max > n_opt` 占比（直接验证分歧机制——最大牌力拆用**更多**手数）。

## 四、实现细节

### 4.1 新文件 `algorithm/native/extracted/optimal_split_power.h`

`#include "optimal_split.h"`，复用 `optdetail::` 的 `enumMoves` / `encodeMove` / `decodeMove` / `consumeBuf` / `pow5` / `packCounts`（均为 `inline`，直接调用），仅新增：

- `powerMemoMap()` —— **独立** memo（与 `optimalSplit` 的 `memoMap()` 分离；key 仍为 rank-counts 的 base-5 打包 `uint64`，跨手复用）。
- `solvePower(key, c)` —— 纯 `max Σ(score_i − ppc)`，递推：

```text
best(key) = max over moves m of [ score(m) − ppc + best(key − m) ]      // base: best(empty)=0
```

  并列按 `moveCode` 定决心（与 `optimalSplit` 一致，保证确定性）。
- `maxPowerValue(hand)` —— 返回 `maxPower` 数值（仅算 yes/no + 率时用）。
- `optimalSplitByPower(hand)` —— decode 还原 argmax 拆分（**仅在命中反例时调用**，稀疏，省时）。
- `clearPowerMemo()` —— 改 `scoringConfig` 后清 memo。

### 4.2 校验程序 `algorithm/native/extracted/verify_split_vs_power.cpp`

镜像 `split_test.cpp` 风格与依赖（`handFromRanks` / `comboStr` / `sumScore` / `handStr` 复用其写法）：

1. `loadScoringConfigFromFile("config/scoring.properties")`；`ppc = scoringConfig().penaltyPerCombo`。
2. 对 17 张与 20 张各跑 N 手（`std::mt19937(12345)`，`std::shuffle` 取前 17 / 前 20）：
   - `opt = optimalSplit(h)`；`reduced_opt = sumScore(opt) − ppc·n_opt`。
   - `maxPower = maxPowerValue(h)`；`diff = maxPower − reduced_opt`。
   - `assert(maxPower >= reduced_opt − 1e-9)`（sanity）。
   - `diff > 1e-6` → 反例 dump。
3. 汇总输出。

### 4.3 正确性保证

- 两套 DFS 共用同一 `scoringConfig()`；config 在程序入口只 `load` 一次，运行中不变。
- power memo 与 `optimalSplit` memo 独立，互不污染。
- `optimalSplit` 的并列破平选 max Σscore（min-n 内），故 `reduced_opt` 即 min-n 集合的最大约简值；`maxPower` 为全空间最大。`diff > 0` 当且仅当存在更优的高 `n` 拆分。

## 五、参数与输出

| 项 | 取值 |
| ---- | ---- |
| 采样源 | 随机均匀（`fullDeck` 洗牌取前 N 张） |
| 手牌规模 | 17 张（农民）+ 20 张（地主）各一份 |
| 样本量 N | 50000（17/20 各 5 万） |
| 随机种子 | `mt19937(12345)`，可复现 |
| 评分配置 | `config/scoring.properties`（与数仓 `card_power` 同口径） |
| 判定阈值 | `diff > 1e-6` 记为分歧 |

输出：

- `algorithm/native/extracted/divergence_17.jsonl`、`divergence_20.jsonl` —— 反例逐手记录（手牌、两套拆分、`n`/Σscore/牌力）。
- stdout 汇总（可重定向到 `extracted/` 下文本，对齐 `sweep_runs/*.jsonl` 风格）。

## 六、编译与运行

与 `split_test.cpp` 同款，MSVC：

```bash
vcvarsall x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fesplit_vs_power.exe verify_split_vs_power.cpp
```

并在 `algorithm/native/CMakeLists.txt` 增加：

```cmake
add_executable(split_vs_power extracted/verify_split_vs_power.cpp)
```

运行：`split_vs_power.exe`（工作目录须能定位到 `config/scoring.properties`，即 `algorithm/native/`）。

## 七、判定标准

- **0 分歧（17/20 全 0）**：命题成立——最少手数拆牌恒达最大牌力。结论可写入 `docs/knowledge/` 并记 memory。
- **存在分歧**：命题不成立。反例 dump 用于刻画触发条件（预期为炸弹 + 飞机 + 长顺共存的极端结构），并在文档给出「为何 `penaltyPerCombo=8` 不足以压制」的机制解释；后续可讨论是否调整 `ppc` 或拆牌口径。

## 八、验证结果（2026-08-06）

全量 N=50000（seed=12345，17/20 各一份，`scoring.properties` 口径，运行 47s）：

| 手牌规模 | 分歧数 / N | 分歧率 | n_pw > n_opt 占分歧 | maxDiff |
| -------- | ---------- | ----- | ------------------- | ------- |
| 17 张 | 9789 / 50000 | 19.58% | 100% | 60.5 |
| 20 张 | 11228 / 50000 | 22.46% | 100% | 63.5 |

**结论：命题不成立。** 最少手数拆牌并非牌力最大拆牌——约 1/5（17 张）至 1/4.5（20 张）的手牌二者给出不同拆法；且在 100% 的分歧案例中，牌力最大拆用了**更多**手数（`n_pw > n_opt`）。地主手（20 张）分歧率更高：牌张多 ⇒ 中段重复多 ⇒ 重叠顺子机会多。

### 8.1 机制（已逐手复核）

分歧恒满足理论条件「Σscore 增量 > penaltyPerCombo·Δn」。两种典型形态：

1. **重叠顺子 vs 强行合并**（最常见，diff 多在 2–5）：手牌有 2 份以上中段点数时，`optimalSplit` 为压手数把它们塞进四带二/飞机翼（低分），而牌力拆保留两条重叠顺（高分）。例（20 张 `2AKKTT88777666655443`）：opt `Q2[6]+45`（Σscore 6）→ power `ST[345678]+ST[45678]`（33）；Σscore +19 > penalty 8。
2. **高价值牌被当翼吞没**（少见但 diff 极大）：`optimalSplit` 把王炸/炸弹塞进翼位以减手数。例（17 张 `bsKK9888866555444`，maxDiff=60.5）：opt `PL1[45]+sb`（**王炸当飞机单翼**）+ `Q2[8]+6K`，Σscore 30.5；power `R[bs]`(60) + `B[8]`(31) + `PL2[45]+6K`(8)，Σscore 99。王炸独立值 60 被当翼只算 16.5。

diff 分布（17 张）：0–4 占 72%，5–9 占 23%，长尾至 60.5。多数分歧幅度小，但「高价值吞没」类可达数十分。

### 8.2 含义

- 「最小手数」与「最大牌力」是**不同目标**，不可互相替代；`optimalSplit`（指标期 min-n）与数仓 `card_power`（牌力）度量的不是同一件事，约 20% 手牌给出不同拆法。
- 若希望二者一致，需调大 `penaltyPerCombo` 使过拆罚则压过 Σscore 增量；当前 8.0 下二者显著背离。
- 反例全集见 `algorithm/native/extracted/divergence_{17,20}.jsonl`（50000 手，seed 固定可复现）。
