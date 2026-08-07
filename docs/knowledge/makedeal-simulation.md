# 发牌模拟方法学（harness + 最优拆牌）

> 做经典/不洗牌发牌分析、评估发牌配置、对账线上炸弹/牌力时先过这篇。
> 真值来源：`algorithm/native/extracted/`（C++ harness + 搜索式最优拆牌器）。旧贪心 Python 模拟 `old2_type*_sim.py` **已从仓库删除**——贪心拆牌被证次优（1000 随机手 546/1000 比最优多手数），被 harness + `optimal_split.h` 取代。

## 0. 真值来源定位（三套别混）

| 来源 | 是什么 | 用途 | 陷阱 |
|---|---|---|---|
| **`extracted/harness.exe`** | 线上发牌/配牌/洗牌 C++ 的 1:1 剥离独立可执行 | **发牌概率唯一可信真值** | 指标期拆牌须用 `optimal_split.h`（最优），勿回退贪心 |
| ~~`old2_type0/1_sim.py`~~ | 贪心拆牌的 Python 模拟（**已删除**） | 被 harness + `optimal_split.h` 取代 | 手数=t0拆、炸弹=贪心拆牌数，与线上/最优口径都不符 |
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

**线上持有炸弹 = 从 `hand_cards` 解析**：DWS `tcy_temp.dws_ddz_daily_game` **无 `bomb_cnt` 列**（早期文档误记），持有炸弹须从 `hand_cards`（如 `6,q,8,7,...`）解析——四张同点 + 王炸。打出炸弹 → `bomb_bet`（≠持有，受玩家决策影响）。

> ⚠️ **对账 SQL 三坑**：① 必带 `dt` 分区过滤（不带返回空 `[]`）；② 一个文件只跑一条语句（`sr_exec` 多语句静默返回 `[]`）；③ 列名查 `information_schema.columns`。`hand_cards` 炸弹解析：先 `10→T, sj→S, bj→B`、去逗号得 17 字符串，再 `LENGTH(h)-LENGTH(REPLACE(h,'x',''))` 数各点频次≥4（每牌 1 字符，无连续歧义）。

### ⚠️ 核心差距：模拟 3×17 是真实牌面的【下界】（重要）

模拟用 **3 家 × 17 张**（庄家=首叫位、不吃底牌、不建模竞叫）。线上真实牌面**地主有 20 张**（17+底牌）且**地主=竞叫赢家（自选强牌方）**——两层叠加使线上真实炸弹 ≫ 模拟。2026-08-05 真人实测（420/742，`hand_cards` 解析）：

| | new 真实 | new 模拟 | new2 真实 | new2 模拟 |
|---|---|---|---|---|
| 每局炸弹数 | 0.55 | 0.42 | **0.86** | 0.42 |
| 有炸局占比 | 44% | 35% | **62%** | 36% |

| 地主拆解 | new | new2 |
|---|---|---|
| 地主 17 张炸弹 | 0.29 | 0.30 |
| 地主 20 张(+底牌) | 0.39 | **0.68** |
| 底牌加成 | +0.10 | **+0.37** |

**底牌对 new2 加成巨大(+0.37)**：with-pair `[4,6,5,2,3]` 凑出大量三张，底牌 3 张极易补成炸；new(no-pair) 牌散只 +0.10。**含义**：3×17 口径掩盖了 new2 的多炸本性——3×17 下 new2≈new，但真实(地主20) new2(0.86) ≫ new(0.55)。低等级要"少炸"，`new` 比 `new2` 实战干净得多。

**对账用同口径**：比"持有炸/人"或"每局总数(3×17)"，别拿模拟"有炸局占比 0.36"对线上"每局个数 0.46"（计数 vs 占比，错位）。

### 对齐线上真实：`--landlord-bottom`

harness 加 `--landlord-bottom` 标志：每局取**牌力最强座作地主**（竞叫赢家近似；注意 `m_nBanker`=首叫位 ≠ 地主），算其 20 张(手牌+底牌)持有炸，输出 `landlord_bomb20` / `table_real_bombs` / `table_3x17_bombs`。`table_real = 地主20 + 2农民17`，对齐线上"真实牌面"。

**实测校验（N=20000，vs 2026-08-05 线上）**：sim `new2` landlord_bomb20=**0.63**（线上 0.68）、table_real=**0.77**（线上 0.86）；`new`=0.34/0.49（线上 0.39/0.55）。开 `--landlord-bottom` 后 sim 从 3×17 的 0.42 跃到真实 ~0.77（**≈线上的 90%**，余 ~10% 为 newuser 混入 + 最强座代理略低于真实竞叫赢家），且 new2(0.77) ≫ new(0.49) 与线上同构。

- 牌力 → `card_power`：≥06-15 窗口才可信；2026-07 PRD 改版（对子翼/炸弹加成）将致第二次断档，见 memory `project_cardpower-formula-prd-change-2026-07`。

## 5. 关键文件索引

| 文件 | 作用 |
|---|---|
| `algorithm/native/extracted/harness.cpp` | 发牌模拟器（1:1 verbatim），注入式候选 |
| `algorithm/native/extracted/optimal_split.h` | 搜索式最优拆牌器（指标期用） |
| `algorithm/native/extracted/sweep.py` | TOP20 扫描 + 打分 + 报告 |
| `algorithm/native/extracted/anchor_check.py` | 锚点/单配置聚合 |
| `algorithm/native/extracted/top20_report.md` | 完整 TOP20 + 算法证明（决策看这个） |
| `algorithm/native/previous/makedeal.json` | 线上配置参照副本（策略定义，可改） |
| `docs/tech/classic-makedeal-config-topn.md` | TOP 速查（本库口径） |
| `docs/tech/classic-makedeal-debomb-plan.md` | 降炸/调体验落地方案 |

> 关联：源码逆向 `docs/makedeal-strategies/742-420-reverse-analysis.md`、代码审计 `docs/makedeal-strategies/makedeal-code-quality-audit.md`、房间配置 `docs/makedeal-strategies/classic/<room>.md`。
