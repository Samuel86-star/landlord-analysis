# 最优拆牌方法论（optimal_split.h）

> 斗地主手牌的「全局最优拆牌」算法参考——目标、算法、完备性、最优性证明、校验、性能、集成口径。
> 实现：[`../../algorithm/native/extracted/optimal_split.h`](../../algorithm/native/extracted/optimal_split.h)（header-only）；校验：[`../../algorithm/native/extracted/split_test.cpp`](../../algorithm/native/extracted/split_test.cpp)。
> 入门/口径速查先看 [makedeal-simulation.md](makedeal-simulation.md)；本篇是拆牌算法的完整深度参考。

## 0. 为什么需要「最优」拆牌（贪心的缺陷）

旧拆牌器 `landlord.h::DefaultSplitterFactory` 是**贪心**——两条**固定优先级单趟**拆牌：
- `PlaneBombPrioritizedSplitter`：rocket→bombs→planes→straights→conspairs→triples→pairs→singles；
- `StraightPrioritizedSplitter`：rocket→straights→conspairs→triples→pairs→singles→bombs。

工厂按启发式（`chooseStrategyWithConfidence`）二选一跑**一趟**，无回溯、无搜索。即便有"两趟取高分"的分支，也只在**两个贪心结果**间比较，非穷举。两类缺陷：

1. **漏翼位组合**：贪心只会"见三张就提三"，但 `{3,6,6,6}` 的最优解是 `三带一(6,翼=3)`（n=1），贪心给出 `三(6)+单(3)`（n=2）。
2. **顺序敏感、炸弹/顺取舍错**：贪心按固定顺序提牌，可能在"炸弹+散牌"与"长顺+散牌"间选错（见 §6 案例 1）。

**实证**（`split_test`，1000 随机手）：最优解比贪心**更少手数 546/1000**、等手数 454/1000、**0 违反**（optimal.n ≤ 贪心.n 恒成立）。⇒ 贪心确为次优，所有依赖它的指标（手数/单牌/拆牌炸弹）不可信。

## 1. 目标函数：字典序 (最小组合数 n → 最大 Σscore)

```
对一手牌的所有合法拆解 D，求 argmin  ( n(D), −Σscore(D) )   // 字典序
```

- **min-n 居首**：用户定义「手数 = 最小组合数」，故最小化组合数是第一目标。
- **为什么不是纯 max-Σscore**：它会**拒绝组低对子**。两张 3：`对(3)` 的 score = `2·(−7)+2 = −12`，而 `单(3)+单(3)` 的 score = `0+0 = 0`；纯 max-score 选两单牌，把 n 从 1 误判成 2，污染手数/单牌。低对子/低三张/低飞机的 score 都可能为负（rank base 见下）。
- **max-Σscore 破平**：在等 n 中最大化 Σscore，对齐「全局牌力最优」——`6666+7..K` 下 `炸(6)+顺(7..K)`（Σ=55）胜过 `顺(6..K)+三(6)`（Σ=24），**炸弹不被长顺吞没**。
- **等价性**：`(min n, max Σscore)` == `(min n, max calcTotalHandScore)`，因为 `controlBonus` 仅由原手牌决定（拆牌无关），而 penalty `(n−1)·8` 在等 n 中是常数。实现里用 Σscore 做比较键。

**牌力 base**（`defaultScoringConfig().rankBaseValues`）：点数 `3,4,5,6,7,8,9,T,J,Q,K,A,2,sj,bj` → `[−7,−6,−5,−4,−3,−2,−1,0,1,2,4,6,10,15,18]`。故低分牌型 score 可负；炸弹 `base·2+35 ≥ 21`、王炸 `60` 恒正且大。

## 2. 算法：记忆化穷举 DFS

**状态**：15 点数计数向量 `std::array<int,15>`（THREE=0…BIG_JOKER=14），由 `HandCardUtils::buildRankCounts` 构造。

**memo key**：base-5 打包 `key = Σ c[i]·5^i`（< 2^35，双射）；递归中增量更新 `key −= Σ comboCount[r]·pow5[r]`。进程级 `static unordered_map<uint64_t,MemoEntry>`，`MemoEntry{n:int16, score:double, moveCode:uint64}`。

**递归核心**：

```
solve(count向量 c, key):
  若 c 全空: return (n=0, score=0)
  若 memo[key] 命中: return memo[key]
  r ← 最低非空点
  best ← (+∞, −∞, +∞)            // (n, −score, moveCode) 字典序 min
  对「以 r 为主位的所有合法牌型」+「以 r 为翼的牌型」每个候选 m:
      c2 ← c − m；key2 ← key − Δ(m)
      (n2, s2) ← solve(c2, key2)
      cand ← (n2+1, s2 + score(m), moveCode(m))
      best ← min 字典序 (n↑, score↓, moveCode↑) of {best, cand}
  memo[key] ← best；return best
optimalSplit(hand): solve(根) → 沿 memo.moveCode 重建组合序列
```

**moveCode**（28 位、确定性重建用）：`(type:4 | mainStart:5 | mainLen:4 | wingMask:15)`；wingMask = 翼点数集合（单翼每点 1 张、对翼每点代表一对，张数由 type 决定）。

## 3. 枚举完备性（关键）

**规范**：每次取**最低非空点 r**枚举。在任一完整拆解里，r 必被**恰好一个** combo 消费——作**主位**（r 是该 combo 的最低主点）或作**翼**（被更高主位的三/四/飞机吸收）。枚举覆盖全部「消费 r 的合法牌型」⇒ 不漏。

**主位枚举**（r 为 combo 最低主点；顺/连对/飞机只在点数 3..A 即索引 0..11）：

| 牌型 | 条件 | 翼选择 |
|---|---|---|
| 单/对/三/炸 | c[r]≥1/2/3/4 | — |
| 三带一 / 三带二 | c[r]≥3 | 任一散单 / 任一散对 |
| 四带二单 / 四带二对 | c[r]≥4 | C(散单,2) / C(散对,2) |
| 顺 / 连对 / 飞机 | 连续 c≥1/2/3 | r 起 L=5/3/2..max |
| 飞机带单 / 飞机带对 | 连续 c≥3 | C(散单,L) / C(散对,L) |

**翼位枚举**（r 被更高主位吸收为翼——**必需**，否则漏 n=1 解）：

| 牌型 | 条件 |
|---|---|
| 三带一(t, r) | c[t]≥3, t>r |
| 三带二(t, r) [r 作对翼] | c[r]≥2, c[t]≥3, t>r |
| 四带二单(q, r, w2) | c[q]≥4, q>r, w2 散单 |
| 四带二对(q, r, p2) [r 作对翼] | c[r]≥2, c[q]≥4, q>r |
| 飞机带单(飞机@t, 翼∋r) | t>r, 选其余 L−1 单翼 |
| 飞机带对(飞机@t, 对翼∋r) | t>r, c[r]≥2, 选其余 L−1 对翼 |

**王炸**：r == 小王且有大王（r 必为最低非空 ⇒ 0..12 已空）。

**反例**（证翼位必需）：`{3,6,6,6}` 唯一最优解 `三带一(6,翼=3)` n=1，最低点 3 作翼；无翼位枚举则退化为 `单(3)+三(6)` n=2。

## 4. 最优性证明（归纳）

**定理**：`solve(c)` 返回 `c` 在所有合法拆解上的字典序最优 `(n, Σscore)`。

**证明**（对 `|c|=Σc[i]` 归纳）：
- *基*：`|c|=0` 仅空拆解，`(0,0)` 平凡最优。
- *步*：设 D* 为最优拆解，其首 combo m*（最低点规范序）消费最低非空点 r（作主位或翼位）；由 §3 枚举覆盖，m* ∈ 候选集。令 `D* = {m*} ∪ D_rest`，D_rest 是 `c−m*` 的拆解；由归纳假设 `solve(c−m*)` 返回 `c−m*` 的最优拆解（≥ D_rest）。故候选 `(1+n(c−m*), score(m*)+s(c−m*))` ≥ D*；又 m* ∈ 枚举 ⇒ 至少一候选达最优；循环 min 即全局最优。∎

**memo 正确性**：(a) 存值仅依赖状态（最低点规范 + 确定性 tie-break），与到达路径无关；(b) 满足最优子结构（最优拆解的尾是子状态的最优拆解）。

## 5. 确定性 tie-break

候选比较键 `(n asc, score desc, moveCode asc)`：等 `(n, Σscore)` 时取 moveCode 最小者。moveCode 是 combo 的纯函数 ⇒ memo 存值与调用路径/顺序无关，可安全跨父状态复用。校验：同手牌二跑字节同（`split_test`）。

## 6. 校验：5 案例 + 1000 随机手实证

**5 典型案例**（`split_test` 全过；score 手算对齐 `defaultScoringConfig`）：

| # | 手牌 | 最优拆解 | n | Σscore | 考察点 |
|---|---|---|---|---|---|
| 1 | 6666 7 8 9 T J Q K | 炸[6]+顺[7..K] | 2 | 55 | §1.2 头例：炸弹不被长顺吞没（vs 顺[6..K]+三[6]=24） |
| 2 | 8888 3 4 | 四带二单[8;3,4] | 1 | 6 | min-n 把孤立四张吸为四带二（vs 炸+单+单 n=3）⇒ **持有炸弹才稳** |
| 3 | 555 666 3 4 | 飞机带单[5,6;3,4] | 1 | 5 | 飞机带翼减手数（vs 两组三带一 n=2） |
| 4 | 55 66 77 | 连对[5,6,7] | 1 | 12 | 连对打包（vs 三组对子 n=3, −18） |
| 5 | sj bj 3 4 5 6 7 | 王炸+顺[3..7] | 2 | 75 | 王炸优先于两王单 |

**实证**（1000 随机 17 张手）：optimal 比 `DefaultSplitterFactory`（贪心）**更少手数 546/1000**、等手数 454/1000、**违反 0**（optimal.n ≤ 贪心.n 恒成立，等 n 时 Σscore ≥ 贪心）。

复现：
```bash
algorithm/native/extracted/split_test.exe   # vcvarsall+cl 编译 split_test.cpp
```

## 7. 性能

- memo 稳态约 **4.7 万状态/进程**；17 张冷手 ~5–50ms，热手（memo 命中）~0.1–1ms。
- 全量 TOP 扫描（~9.6M 次 solve、进程间并行）约 **5–10 分钟**，无需采样。
- 瓶颈是 memo 探针 + 翼组合枚举；分支实测每状态 ~30–150（典型 <50）。memo 进程内跨手复用 ⇒ 摊销。
- 无需线程化（扫描并行在 harness 进程间）。如遇病态长飞机多翼手，可加 `ceil(余牌/最大牌型)` 下界剪枝（默认未开，保精确）。

## 8. 集成与口径

- **仅指标期拆牌**：`optimalSplit` 只替换 harness 计算指标时的拆牌；**发牌/配牌/洗牌管线（`MakeDealByCfg`/`SpliteCard`/`MakeDeal_ComposeCard`）一字不动** ⇒ 持有炸弹等物理量与线上完全对齐（锚点：742=0.139 / 420=0.140 / old2=0.420 / 纯随机=0.189）。
- **controlBonus 排除于优化**：它仅由原手牌决定（双王/双2/持有炸），与拆牌无关；正确地被排除于 argmax，仅在指标期由 `calcTotalHandScore` 加回。
- **拆牌炸弹偏低 → 持有炸为主**：min-n 会把孤立四张吸成"四带二"（案例 2），故"拆牌炸弹"系统性偏低（~0.08）；**炸弹维度必须看持有炸弹**（与线上 `bomb_cnt` 对齐）。见 [makedeal-simulation.md](makedeal-simulation.md) §1。
- **配置耦合**：memo 缓存的 `(n, score, moveCode)` 依赖全局 `scoringConfig()`；改 `setScoringConfig`/`loadScoringConfigFromFile` 后须调 `clearOptimalSplitMemo()`（扫描每进程一配置，天然安全）。
- **手数口径**：人均最优手数 = `Σ 每家组合数 / (3N)`，直接读最优拆解的 n。

## 9. 复用的 landlord.h 构件

`Combo`/`Card`/`Rank`/`ComboType` 类型与 `Combo` 静态工厂；`DefaultComboScoringStrategy::score` 做 Σscore；`STRAIGHT_RANKS`/`isStraightRank` 约束顺/连对/飞机只在 0..11；`HandCardUtils::buildRankCounts` 构造计数向量；`normalizeHandStrength`(sigmoid) 用于诱导/抗衡的归一化牌力。

> 关联：[makedeal-simulation.md](makedeal-simulation.md)（口径/速查）、[`../../algorithm/native/extracted/top20_report.md`](../../algorithm/native/extracted/top20_report.md)（含算法证明节）、[../makedeal-strategies/742-420-reverse-analysis.md](../makedeal-strategies/742-420-reverse-analysis.md)（发牌源码逆向）。
