# 发牌模拟方法学（harness + 最优拆牌）

> 做经典/不洗牌发牌分析、评估发牌配置、对账线上炸弹/牌力时先过这篇。
> 真值来源：`algorithm/native/extracted/`（C++ harness + 搜索式最优拆牌器）。旧贪心 Python 模拟（`old2_type*_sim.py`）已作废——贪心拆牌被证次优（1000 随机手 546/1000 比最优多手数）。

## 0. 真值来源定位（三套别混）

| 来源 | 是什么 | 用途 | 陷阱 |
|---|---|---|---|
| **`extracted/harness.exe`** | 线上发牌/配牌/洗牌 C++ 的 1:1 剥离独立可执行 | **发牌概率唯一可信真值** | 指标期拆牌须用 `optimal_split.h`（最优），勿回退贪心 |
| 旧 `old2_type0/1_sim.py` | 贪心拆牌的 Python 模拟 | **已作废**，仅历史参考 | 手数=t0拆、炸弹=贪心拆牌数，与线上/最优口径都不符 |
| 线上 `dws_ddz_daily_game` | 线上实测 | 上线后对账 | `bomb_cnt`(持有) ≠ `bomb_bet`(打出)；`card_power` 2026-07 PRD 改版有断档 |

> harness 用 `--seed 1` 可复现；种子源与线上不同（线上 `GetTickCount()+token*10+socket`）→ 具体某局不可复现线上，但**概率分布忠实**（同一洗牌算法）。

## 1. 严苛口径（统计分子/分母）

- **持有炸(人均)** = `Σ 每家持有炸弹(四张同点/王炸)颗数 / (3N)`。与线上 `bomb_cnt` 对齐。自然≈**0.189**。
- **单局炸弹率** = `#{整桌 3 人有任意炸弹的局} / N`。自然≈**0.461**。低等级房"抱随机"的核心指标。
- **炸弹密度分布** = 桌上炸弹总数 K∈{0,1,2,3+} 的局占比。自然≈`0.54/0.37/0.08/0.01`。
- **人均最优手数** = 最优拆牌(min-combo)下 `Σ 每家组合数 / (3N)`。自然≈**7.34**，越小越顺。
- **人均散牌** = 最优拆牌里 SINGLE 组合数均值。自然≈3.83。
- **首叫诱导度** = `mean(P_max/P_avg)`，P=sigmoid(牌力/40)。自然≈1.33。
- **抗衡度** = `mean((P_mid+P_min)/P_max)`。自然≈1.30，越高农民越能抗。

> P 用 `normalizeHandStrength=sigmoid(V/40)`（非负单调），因原牌力 `calcTotalHandScore` 可为负（纯随机 ≈0.8 但中段易负），直接做比值无意义。
> **炸弹看"持有"**：最优拆牌(min-combo)会把孤立四张吸成"四带二"（省一手），故"拆牌炸弹"系统性偏低（~0.08），不能作炸弹维度主指标。

## 2. 最优拆牌器（`optimal_split.h`）

> 完整方法论（目标函数、记忆化穷举 DFS、完备性、最优性证明、5 案校验、性能）见 [makedeal-optimal-split.md](makedeal-optimal-split.md)。本节仅速记。

- 目标：字典序 **(最小组合数 n → 最大 Σscore)**。min-n 对齐"手数=最小组合"；max-Σscore 破平（如 `6666+7..K` → 炸弹+顺 55 分，胜过长顺+三 24 分，**炸弹不被长顺吞没**）。
- 算法：记忆化穷举 DFS，状态=15 点数计数向量，memo key=base-5 打包；每步取最低非空点，枚举其作主位/作翼的所有合法牌型（完备不漏）。**非贪心**。
- 校验：`split_test.cpp` 5 案全过 + 1000 随机手 0 违反（optimal.n ≤ 贪心.n 恒成立）。
- 仅用于"指标期"拆牌；**发牌/配牌/洗牌管线不动**（故 742/420 持有炸始终复现线上 0.137/0.138）。

## 3. harness / sweep 用法

```bash
# 注入式候选（三家同策略），--pure-random 为基线
algorithm/native/extracted/harness.exe --cfg algorithm/native/previous/makedeal.json \
    --type1 --coupai 4,5,3,6 --begin 11 --select 15 --tv 999 --tr 10 \
    -n 20000 --seed 1 --reals 3
# Type0：--type0 --bmn N --bigcards-to N --first-hc/bomb/big ... --other-hc/bomb/big ...

# 聚合（持有炸/单局炸率/密度/手数/散牌/首叫/抗衡）
py -3 algorithm/native/extracted/anchor_check.py a.jsonl b.jsonl

# 全量 TOP20 扫描（两阶段）
py -3 algorithm/native/extracted/sweep.py --coarse-n 3000 --final-n 20000 --base-n 20000
py -3 algorithm/native/extracted/sweep.py --rerank   # 改适应度权重后秒重排，免重跑
```

- 适应度（基线=纯随机）：`.28·S_bomb(单局炸率抱随机)+.18·S_hand+.12·S_single+.20·S_susp(首叫+抗衡)+.10·S_div+.12·S_hit`，权重在 `sweep.py` 的 `W` dict。
- 产物：`top20_report.md`（完整 TOP20 + 算法证明）、`top20_configs.json`（可落地 JSON）、`sweep_raw.json`（指标缓存）。

## 4. 与线上口径的关系（对账必读）

- 持有炸弹 → `dws_ddz_daily_game.bomb_cnt`（应与 harness 持有炸吻合，如 742≈0.137）。
- 打出炸弹 → `bomb_bet`（**≠ 持有**；打出受玩家决策/牌局影响，恒 < 持有）。
- 牌力 → `card_power`：≥06-15 窗口才可信；2026-07 PRD 改版（对子翼/炸弹加成）将致第二次断档，见 memory `project_cardpower-formula-prd-change-2026-07`。
- 庄家=首叫位 `m_nBanker`（**非地主、不吃底牌**）；地主由竞叫决定，harness 不模拟竞叫。庄闲比较用 17 张口径。

## 5. 关键文件索引

| 文件 | 作用 |
|---|---|
| `algorithm/native/extracted/harness.cpp` | 发牌模拟器（1:1 verbatim），注入式候选 |
| `algorithm/native/extracted/optimal_split.h` | 搜索式最优拆牌器（指标期用） |
| `algorithm/native/extracted/sweep.py` | TOP20 扫描 + 打分 + 报告 |
| `algorithm/native/extracted/anchor_check.py` | 锚点/单配置聚合 |
| `algorithm/native/extracted/top20_report.md` | 完整 TOP20 + 算法证明（决策看这个） |
| `algorithm/native/previous/makedeal.json` | 线上配置只读快照（策略定义） |
| `docs/tech/classic-makedeal-config-topn.md` | TOP 速查（本库口径） |
| `docs/tech/classic-makedeal-debomb-plan.md` | 降炸/调体验落地方案 |

> 关联：源码逆向 `docs/makedeal-strategies/742-420-reverse-analysis.md`、代码审计 `docs/makedeal-strategies/makedeal-code-quality-audit.md`、房间配置 `docs/makedeal-strategies/classic/<room>.md`。
