# 经典玩法发牌降炸/调体验 — 落地方案

> 目标：经典玩法发牌**贴近真实、牌不烂、好打但不靠堆炸弹**；低等级房炸弹维度**趋近纯随机**，随房间等级升高才放大炸弹/倍数。
> **真值来源（2026-08 重构）**：`algorithm/native/extracted/harness.exe`（C++，发牌管线 1:1 复刻线上）+ **搜索式全局最优拆牌** `optimal_split.h`（非贪心）。决赛 N=20000，三家同策略。完整 TOP20 见 [`../../algorithm/native/extracted/top20_report.md`](../../algorithm/native/extracted/top20_report.md)。
> 日期：2026-08-03 初版 · 2026-08-06 最优拆牌口径重写（旧贪心 Python 模拟结论已作废）。

> ⚠️ **关键更正**：旧版据贪心模拟得出"Type0 control-flow = 0.20 炸/5.84 手，全局最优"。经最优拆牌+按局口径复核，**Type0 在低等级房抱不动随机**（菜单硬含 `MatchBombCardType`，最佳单局炸率仍 0.60+，远超自然 0.461）。故：
> - **低等级经典房（742/420）走 Type1**（已是上线路线）；
> - **Type0 降为"更高等级"备选**（要更多炸弹/倍数/爽感时，多炸从缺点变卖点）。

---

## 一、背景与问题

经典玩法（`game_id=53 AND play_mode=1`，7 档）绝大多数对局（新手+初级+中级 ≈ 87%）原跑 **`old2` 策略（Type0）**。

**现状（harness + 最优拆牌，N=20000）**：

| | 持有炸 | 单局炸率 | 0/1/2/3+炸密度 | 人均最优手数 | 抗衡度 |
|---|---|---|---|---|---|
| **old2（Type0 baseline）** | 0.419 | **0.701** | 0.30/0.33/0.22/0.14 | 5.93 | 1.417 |
| 纯随机（参照） | 0.189 | 0.461 | 0.54/0.37/0.08/0.01 | 7.34 | 1.299 |

old2 桌上 3+炸局占 **14%**（自然仅 1%），玩家反馈"炸弹多、不真实"。根因（代码层，`makedeal.json` 改不了）：

1. 做牌菜单 `MatchBombCardType` 排在**第 2 位**——座位有 3 张同点就补第 4 张成炸；
2. `DoMakeDeal` **第二轮起无条件做牌**（疑似 bug，`else if (i>0)`）。

> Type0 下 `makedeal.json` 真正生效的只有 `BeginMakeNum`、6 个 `First/OtherChair*` 阈值、`BigCardsTo`；`CouPaiStrategy`/`TargetValue`/`TargetRound` 是死字段。

---

## 二、路线选择（新结论）

| 路线 | 改动 | 低等级(742/420)定位 | 适用 |
|---|---|---|---|
| **Type1（CouPai 拼牌）** | **只改配置** | ✅ **已采纳**（742 `new` 上线、420 `new2` 改造中） | 低等级"抱随机"、纯配置灰度 |
| **Type0（菜单做牌）** | 改代码+配置 | ❌ 抱不动随机（最佳单局炸率 0.60+） | **更高等级**（要多炸/倍数/爽感） |

- Type1 TOP1 `bomb-last [4,6,5,3,2,13] b11 s15`：单局炸率 **0.454 ≈ 自然 0.461**、手 5.51、抗衡 1.472、得分 0.913。
- Type0 TOP1 `thr3 bmn14`：单局炸率 0.607、手 6.52、得分 0.688（低等级不及 Type1）。

---

## 三、Type1 路线（已采纳）

### 3.1 现行配置（harness + 最优拆牌，N=20000）

| 房间 | 配置 | 持有炸 | 单局炸率 | 人均手 | 抗衡度 | Type1 排名 | 线上实测 |
|---|---|---|---|---|---|---|---|
| **742** | `new`=`no-pair [4,5,3,6] b11 s15` | 0.139 | 0.354 | 6.00 | 1.443 | 26/64 | bomb_bet 有炸局 67%→32%（07-31 上线）|
| **420** | `new2`=`with-pair [4,6,5,2,3] b14 s17` | 0.140 | 0.360 | 6.16 | 1.424 | 31/64 | 改造中（待灰度）|

> 两房持有炸与线上标定 0.137/0.138 吻合。降炸达标、手顺、均衡，已是好配置；**唯单局炸率 0.354/0.360 略低于自然 0.461**，按"抱随机"理念轻度减炸过头。

### 3.2 若要更贴自然（Type1 TOP 家族）

| 配置 | 单局炸率 | 人均手 | 抗衡 | 得分 |
|---|---|---|---|---|
| `bomb-last [4,6,5,3,2,13] b11 s15` | **0.454** | 5.51 | 1.472 | **0.913** |
| `no-pair [4,5,3,6] b14 s17` | 0.425 | 5.92 | 1.426 | 0.895 |
| `no-bomb [4,6,5,3,2] b11 s15` | 0.432 | 5.54 | 1.470 | 0.889 |

> 在 742/420 现行 CouPai 基础上**加收对(2)+末位炸码(13)** 即得 TOP1，让炸弹分布更贴自然同时更顺。可落地 JSON 见 [`../../algorithm/native/extracted/top20_configs.json`](../../algorithm/native/extracted/top20_configs.json)。

### 3.3 配置写法（`makedeal.json` → `MakeDealStrategy`）

```json
"new": {
    "MakeDealType": 1, "BeginMakeNum": 11, "BeginSelectBanker": 15,
    "TargetValue": 999, "TargetRound": 10,
    "CouPaiStrategy": [[4, 5, 3, 6]]
}
```

> 码：2对 3三 4顺 5连对 6飞机 13炸。`TargetValue=999`=恒拼牌（最可控）。Type1 下 `BigCardsTo`/6 个 `First/OtherChair*` 惰性。

---

## 四、Type0 路线（更高等级备选）

> Type0 经最优拆牌复核，低等级抱不动随机；但其"系统性给炸弹"在高等级房是**卖点**（更多炸弹→更高倍数→更刺激→更高服务费）。下列 TOP1 供高等级房选用。

| 配置 | 持有炸 | 单局炸率 | 人均手 | 抗衡 | 得分 |
|---|---|---|---|---|---|
| `thr3 bmn14`（Type0 TOP1） | 0.302 | 0.607 | 6.52 | 1.375 | 0.688 |

> Type0 的 `CouPaiStrategy`/`TargetValue` 惰性，生效的是 `BeginMakeNum=14` + 6 阈值(thr3) + `BigCardsTo=2`。

---

## 五、改动点（Type0 改代码，适用于"降 old2 极端多炸"或高等级微调）

> 本节为 Type0 代码改动；**Type1 路线只改配置，无需改代码**（见 §三）。

### 改动 1（主）：做牌菜单顺序【改代码】

**文件**：线上 `common/zgdatbl.cpp`（仓库参照副本 `algorithm/native/previous/zgdatbl.cpp`）
**函数**：`MatchFirstChairCards`（快照 `4291`）、`MatchOtherChairCards`（快照 `4324`）——两函数菜单相同，都改。

**现状**（快照 `4299-4315`）：
```cpp
nMatchedCardID = Match2OrKingCardType(...);   // ① 2/王
nMatchedCardID = MatchBombCardType(...);      // ② 炸弹   ← 凑炸根源,在第2位
nMatchedCardID = MatchThreeCardType(...);     // ③ 三
nMatchedCardID = MatchABTCardType(...);       // ④ 顺
nMatchedCardID = MatchABTCoupleCardType(...); // ⑤ 连对
nMatchedCardID = MatchCoupleCardType(...);    // ⑥ 对
```

**改为 control-flow**（`2王 → 顺 → 连对 → 三 → 对 → 炸`，把 `MatchBomb` 垫后）：
```cpp
nMatchedCardID = Match2OrKingCardType(...);   // ① 2/王(大牌控制优先)
nMatchedCardID = MatchABTCardType(...);       // ② 顺
nMatchedCardID = MatchABTCoupleCardType(...); // ③ 连对
nMatchedCardID = MatchThreeCardType(...);     // ④ 三
nMatchedCardID = MatchCoupleCardType(...);    // ⑤ 对
nMatchedCardID = MatchBombCardType(...);      // ⑥ 炸弹(垫后:几乎不主动凑)
```

> 注：此改动**降低** Type0 的炸弹（菜单不再优先凑炸），但即便如此 Type0 低等级仍抱不到自然（单局炸率 0.60+）。用于把 old2 的 0.70 拉低到可接受范围，或给高等级房微调爽感。

### 改动 2：配牌起点【改配置】

`makedeal.json` → `MakeDealStrategy.old2`：`"BeginMakeNum": 12 → 14`（Type0 TOP1 即 bmn14；干预更轻）。

### 改动 3（可选，不阻塞）：修第二轮无条件做牌 bug【改代码】

`zgdatbl.cpp` `DoMakeDeal`（快照 `4187`），三处 `else if (i > 0)`（快照 `4214`/`4231`/`4248`）。建议加 `triggered[chair]` 标记，第一轮满足阈值的家后续轮才续做。边际效果小，可单独评估。

---

## 六、风险与监控

**风险**：
- 炸弹减少 → 玩家"爽感"可能下降（但贴近真实，符合低等级产品方向）；**王炸不受影响**（不动双王成炸）。
- Type1 单局炸率若压得过低（<0.40）→ 偏离"抱随机"，低等级也未必好。
- 庄闲/抗衡变化需观察是否影响真人胜率。

**上线必看指标**（对照灰度前）：
- 单家持有炸弹 `dws_ddz_daily_game.bomb_cnt`（742 应 ~0.139、old2 房 ~0.42→目标更低）；
- 对局时长 / 手数分布；
- 真人胜率 / 地主占比（防过度偏移）；
- **留存**（尤其经典初级房新手，最敏感）、**付费**。

---

## 七、灰度建议

1. 420 `new2` 灰度（继 742 `new` 之后）——A/B 对比炸弹/胜率/留存/付费 vs 对照组；
2. 观察 1~2 周无异常 → 全量经典低等级房（Type1）；
3. 更高等级房如要多炸/爽感，单独评估 Type0 `thr3 bmn14`。

---

## 八、验证方法（harness 真值）

```bash
# 纯随机基线
algorithm/native/extracted/harness.exe --cfg algorithm/native/previous/makedeal.json \
    --pure-random -n 20000 --seed 1 --reals 3

# Type1 注入任意候选（742 new）
algorithm/native/extracted/harness.exe --cfg algorithm/native/previous/makedeal.json \
    --type1 --coupai 4,5,3,6 --begin 11 --select 15 --tv 999 --tr 10 -n 20000 --seed 1 --reals 3

# Type0 注入任意候选（thr3 bmn14）
algorithm/native/extracted/harness.exe --cfg algorithm/native/previous/makedeal.json \
    --type0 --bmn 14 --bigcards-to 2 --first-hc 5 --first-bomb 2 --first-big 4 \
    --other-hc 6 --other-bomb 2 --other-big 3 -n 20000 --seed 1 --reals 3
```

聚合：`py -3 algorithm/native/extracted/anchor_check.py <jsonl...>`。
全量 TOP20 扫描：`py -3 algorithm/native/extracted/sweep.py --coarse-n 3000 --final-n 20000 --base-n 20000`（改权重加 `--rerank` 秒重排）。
线上：灰度后取 `dws_ddz_daily_game` 的 `bomb_cnt`（持有）/`bomb_bet`（打出，≠持有）分布对照（注意 `card_power` 在 2026-07 PRD 改版后有断档，见 memory `project_cardpower-formula-prd-change-2026-07`）。

---

## 九、附录

- 模拟器（C++ 真值）：[`../../algorithm/native/extracted/`](../../algorithm/native/extracted/)（`harness.cpp` + `optimal_split.h` + `sweep.py`，README 有忠实度说明）。
- 完整 TOP20：[`../../algorithm/native/extracted/top20_report.md`](../../algorithm/native/extracted/top20_report.md)。
- 源码逆向：[`../makedeal-strategies/742-420-reverse-analysis.md`](../makedeal-strategies/742-420-reverse-analysis.md)；代码质量审计：[`../makedeal-strategies/makedeal-code-quality-audit.md`](../makedeal-strategies/makedeal-code-quality-audit.md)。
- 相关 memory：`project_makedeal-debomb-plan-2026-08`、`project_cardpower-formula-prd-change-2026-07`、`feedback_avoid-narrative-data-interpretation`。
