# PRD：斗地主发牌均衡性过滤系统

**文档版本**：v1.0
**更新时间**：2026-02-27
**负责模块**：`landlord-algorithm` · 洗牌发牌子系统

---

## 一、背景与目标

### 1.1 问题背景

斗地主游戏中，纯随机发牌会产生一定比例的「极端局面」，例如：

- 某玩家手牌全是渣牌（单牌堆积，几乎无法主动出牌）
- 某玩家手牌集中了大量炸弹，对其他玩家形成碾压
- 三家手牌强弱悬殊，游戏体验极差

这些情况在数学上是合法的，但从玩家体验角度需要被过滤掉。

### 1.2 设计目标

在洗牌/发牌阶段，通过**静态牌力评估 + 多维均衡性检测**自动识别极端局面并触发重洗，同时满足：

1. 过滤率可控（基于统计分位点配置，不过度干预随机性）
2. 性能可接受（每局发牌耗时在毫秒级，不影响游戏吞吐量）
3. 参数可调（所有阈值外置到 `application.properties`，无需改代码）

### 1.3 适用范围

仅作用于**发牌阶段**（底牌揭晓前），不涉及游戏中的出牌策略或 AI 决策。

---

## 二、核心概念

### 2.1 静态牌力评估（V_total）

对一手牌拆分成若干 Combo（组合），按公式计算总分：

```
V_total = Σ V_combo - (N - 1) × 8 + Control_Bonus
```

- `V_combo`：单个 Combo 的得分，由牌型、牌面大小、带牌等因素决定
- `N`：Combo 总数（出牌步数），步数越多扣分越多
- `Control_Bonus`：控制牌加成（大王 +5、小王 +3、持有 ≥2 张 2 时 +4、每个炸弹/王炸 +5）

> 此分值**仅用于发牌均衡性判断**，不代表实际游戏胜率。

### 2.2 拆牌策略

同一手牌有多种拆分方式，系统通过 `DefaultComboExtractor` 运行两套策略并取最优解：

- `PlaneBombPrioritizedSplitter`：优先保留飞机和炸弹结构
- `StraightPrioritizedSplitter`：优先保留顺子结构
- `DefaultSplitterFactory`：根据手牌特征（顺子长度、三张牌数量）自动选择策略

---

## 三、均衡性检测维度

系统共使用五个维度判断一次发牌是否为「极端局面」，任意一项触发即重洗。

### 维度一：手牌牌力上下限

| 参数 | 含义 | 推荐值 |
|---|---|---|
| `lower-threshold` | 三家中最弱一家的牌力下限（低于此值触发） | `-68.0`（P5） |
| `upper-threshold` | 三家中最强一家的牌力上限（高于此值触发） | `74.0`（P95） |

**检测逻辑**：任意一家手牌 V_total 超出 `[lower, upper]` 区间即触发重洗。

---

### 维度二：三家牌力极差

| 参数 | 含义 | 推荐值 |
|---|---|---|
| `max-spread` | 三家牌力最大值与最小值之差的上限 | `113.0`（P95） |

**检测逻辑**：`max(scores) - min(scores) > max-spread` 触发重洗。

> 补充维度一的不足：两家分值都在合法区间内，但彼此差距过大时仍需重洗。

---

### 维度三：底牌协同增益（潜在地主牌力）

| 参数 | 含义 | 推荐值 |
|---|---|---|
| `max-potential-landlord-score` | 任意一家手牌 + 底牌后的最高牌力上限 | `94.0`（P95） |

**检测逻辑**：枚举三家分别拿走底牌后的手牌牌力，取最大值，超过阈值触发重洗。

> 解决原有「max(scores) + bottomBonus × weight」粗估方式的问题，能识别底牌与某家手牌协同补全炸弹、顺子等情况。

---

### 维度四：地主优势枚举

| 参数 | 含义 | 推荐值 |
|---|---|---|
| `max-landlord-advantage` | 潜在地主牌力 - 两个农民手牌均值 的上限 | `114.5`（P95） |

**检测逻辑**：枚举三种叫地主方案，计算每种方案的 `landlordScore - avgFarmerScore`，取最大值，超过阈值触发重洗。

> 发牌阶段无法确认谁叫地主，通过枚举预防最坏情况：若某玩家一旦叫地主便形成压倒性优势，则提前干预。

---

### 维度五：手牌结构特征

| 参数 | 含义 | 推荐值 |
|---|---|---|
| `max-singles-per-hand` | 单家手牌中单牌数量上限（`0` 表示不启用） | `8`（P95） |
| `max-bombs-per-hand` | 单家手牌中炸弹数量上限（`0` 表示不启用） | `2`（P99） |

**检测逻辑**：对每家手牌拆牌后统计 `SINGLE` / `BOMB` / `ROCKET` 类型的 Combo 数量，任意一家超过上限触发重洗。

> 纯分值无法识别「大量单牌堆积」（可打出性极差）或「炸弹集中」（牌力结构过于强势）等极端结构，此维度作为补充。

---

## 四、性能优化

### 4.1 渐进式阈值放宽

每次重洗失败后，阈值区间按比例放宽，避免无限重洗：

```
实际阈值 = 原始阈值 × (1 + i × relaxStep)
```

- `i`：当前已重洗次数
- `threshold-relax-step`：放宽步长，默认 `0.15`（每次放宽 15%）
- `max-reshuffle-times`：最大重洗次数，默认 `5`，超过后接受当前结果

### 4.2 快速失败（Fast-Fail）

重洗循环中引入「定向检查」优化：

1. 当某次判定为极端局面时，记录触发原因的具体座位（`problematicSeat`）
2. 下一轮重洗后，**优先只计算该座位的牌力**
3. 若该座位仍然超限，直接跳过完整的全局计算，进入下一次重洗

**效果**：在「同一座位反复触发阈值」的场景下，减少约 60%~70% 的无效计算。

> 注：当触发原因是「三家极差过大」（非单一座位问题）时，`problematicSeat = -1`，退化为完整计算，不影响正确性。

---

## 五、参数标定方法

系统提供 `DealDistributionSampler` 采样工具类，用于在纯随机发牌（不过滤）下统计各指标的分布，输出推荐阈值。

### 使用方式

运行测试 `ShuffleAndScoringBenchmarkTest#sampleBaselineDistribution`，输出如下（10 万局示例）：

```
===== DealDistributionSampler 详细分布 =====
minScore（最差一家）： P5=-68.0 P50=-35.0 P95=-1.0  P99=12.5
maxScore（最强一家）： P5=-18.0 P50=19.0  P95=74.0  P99=98.0
spread（极差）      ： P5=14.0  P50=53.0  P95=113.0 P99=140.0
potentialLandlord  ： P5=-3.0  P50=37.0  P95=94.0  P99=116.0
landlordAdvantage  ： P5=15.3  P50=56.0  P95=114.5 P99=141.5
maxSingles（单牌数）： P5=4.0   P50=6.0   P95=8.0   P99=9.0
maxBombs（炸弹数） ： P5=0.0   P50=0.0   P95=1.0   P99=2.0
============================================
===== DealDistributionSampler 推荐阈值 =====
# landlord.shuffle-strategy.lower-threshold=-68.0
# landlord.shuffle-strategy.upper-threshold=74.0
# landlord.shuffle-strategy.max-spread=113.0
# landlord.shuffle-strategy.max-potential-landlord-score=94.0
# landlord.shuffle-strategy.max-landlord-advantage=114.5
# landlord.shuffle-strategy.max-singles-per-hand=8
# landlord.shuffle-strategy.max-bombs-per-hand=2
============================================
```

### 标定原则

| 阈值选取 | 适用场景 |
|---|---|
| **P95**（推荐） | 过滤最极端 5% 的局面，重洗率约 10%~15%，兼顾体验与性能 |
| P90 | 过滤更激进，重洗率约 20%~25%，体验更好但吞吐量下降 |
| P99 | 过滤保守，重洗率 < 5%，适合对吞吐量要求极高的场景 |

> **重要**：评分公式或拆牌策略每次有变更，必须重新运行采样测试更新基线，旧阈值不能复用。

---

## 六、配置项汇总

```properties
# ===== 发牌均衡性过滤开关 =====
landlord.shuffle-strategy.enabled=true

# ===== 五维均衡性阈值（基于10万局P95/P99采样） =====
landlord.shuffle-strategy.lower-threshold=-68.0
landlord.shuffle-strategy.upper-threshold=74.0
landlord.shuffle-strategy.max-spread=113.0
landlord.shuffle-strategy.max-potential-landlord-score=94.0
landlord.shuffle-strategy.max-landlord-advantage=114.5
landlord.shuffle-strategy.max-singles-per-hand=8
landlord.shuffle-strategy.max-bombs-per-hand=2

# ===== 重洗控制 =====
landlord.shuffle-strategy.max-reshuffle-times=5
landlord.shuffle-strategy.threshold-relax-step=0.15

# ===== 版本标识（用于日志追踪配置变更） =====
landlord.shuffle-strategy.version=dealing_filter_v2
```

---

## 七、关键类说明

| 类名 | 职责 |
|---|---|
| `DefaultReshuffleDealStrategy` | 核心重洗入口，整合五维检测逻辑与快速失败优化 |
| `AbstractShuffleDealStrategy` | 基类，提供牌力计算、结构特征统计等通用方法 |
| `ShuffleStrategyDecisionProperties` | 配置属性类，对应所有 `application.properties` 参数 |
| `DealDistributionSampler` | 采样工具，用于统计基线分布、输出推荐阈值（非生产逻辑） |
| `DefaultComboExtractor` | 双策略拆牌，取最优拆法用于牌力评分 |

---

## 八、已知局限

1. **静态评估非胜率**：V_total 是启发式分值，与真实胜率存在偏差，极端局面可能有漏网情况
2. `max-singles-per-hand` / `max-bombs-per-hand` 设为 `0` 时默认不启用，初次上线建议先开启观察
3. 阈值与评分公式强耦合，任何评分逻辑变更必须重新采样标定
