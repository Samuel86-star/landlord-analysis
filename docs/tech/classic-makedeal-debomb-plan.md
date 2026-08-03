# 经典玩法做牌降炸弹 — 落地方案

> 目标:让经典玩法(`old2`/Type0)的发牌**贴近真实打牌、牌不烂、好打但不靠给炸弹**。
> 结论基于 `algorithm/native/old2_type0_sim.py`(线上 old2/Type0 的 1:1 Python 复刻)的二维参数扫描。
> 日期:2026-08-03 · 作者:分析团队

---

## 一、背景与问题

经典玩法(`game_id=53 AND play_mode=1`,7 档)绝大多数对局(新手+初级+中级 ≈ 87%)跑在 **`old2` 策略(Type0)**。新手/初级房(420/4484/12074 等)用的都是它。

**现状:做牌系统性凑炸弹,单家平均炸弹 0.42(纯随机 0.19,翻 2.2 倍)**,玩家反馈"炸弹多、不真实"。

根因(都在代码层,`makedeal.json` 改不了):

1. 做牌菜单 `MatchBombCardType` 排在**第 2 位**——座位有 3 张同点就补第 4 张成炸;
2. `DoMakeDeal` **第二轮起无条件做牌**(疑似 bug,`else if (i>0)`),第一轮因"牌强"没触发的家,第二轮起也被补牌。

> Type0 下 `makedeal.json` 真正生效的只有 `BeginMakeNum`、6 个 `First/OtherChair*` 阈值、`BigCardsTo`;`CouPaiStrategy`/`TargetValue`/`TargetRound` 是死字段。所以降炸弹必须改代码。

---

## 二、目标组合(模拟器结论)

**`control-flow` 菜单 + `BeginMakeNum=10`**(对应 CLI:`run --fix control-flow --bmn 10`):

| | 炸弹 | 手数 | 庄闲差 |
|---|---|---|---|
| 现状(baseline, bmn=12) | 0.42 | 6.08 | 0.033 |
| **目标(control-flow, bmn=10)** | **0.20** | **5.84** | 0.025 |
| 纯随机(参照) | 0.19 | 7.50 | — |

- 炸弹 0.42 → **0.20**(砍半多,接近自然 0.19);
- 手数 6.08 → **5.84**(更好打);
- 庄闲差 0.025(均衡);
- 2 王前置 = 大牌控制优先补;炸弹垫后 = 保留自然炸弹爽点;
- 牌型结构:顺子/连对/飞机 反而更多,单牌更少(**牌不散**,手感更顺)。

---

## 三、改动点

### 改动 1(主):做牌菜单顺序【改代码】

**文件**:`common/zgdatbl.cpp`(线上;本仓库参照副本 `algorithm/native/previous/zgdatbl.cpp`)

**函数**:`MatchFirstChairCards`(快照 `4291`)、`MatchOtherChairCards`(快照 `4324`)——两个函数菜单完全相同,**都改**。

**现状**(快照 `4299-4315` / `4332-4348`):

```cpp
nMatchedCardID = Match2OrKingCardType(...);   // ① 2/王
nMatchedCardID = MatchBombCardType(...);      // ② 炸弹   ← 凑炸根源,在第2位
nMatchedCardID = MatchThreeCardType(...);     // ③ 三
nMatchedCardID = MatchABTCardType(...);       // ④ 顺
nMatchedCardID = MatchABTCoupleCardType(...); // ⑤ 连对
nMatchedCardID = MatchCoupleCardType(...);    // ⑥ 对
```

**改为 control-flow**(`2王 → 顺 → 连对 → 三 → 对 → 炸`):

```cpp
nMatchedCardID = Match2OrKingCardType(...);   // ① 2/王(前置:大牌控制优先)
nMatchedCardID = MatchABTCardType(...);       // ② 顺
nMatchedCardID = MatchABTCoupleCardType(...); // ③ 连对
nMatchedCardID = MatchThreeCardType(...);     // ④ 三
nMatchedCardID = MatchCoupleCardType(...);    // ⑤ 对
nMatchedCardID = MatchBombCardType(...);      // ⑥ 炸弹(垫后:几乎不主动凑)
```

> 即:`MatchBomb` 从第 2 挪到最后;`MatchABT`(顺)、`MatchABTCouple`(连对)提前到 `MatchThree` 之前;`MatchCouple`(对)在 `MatchThree` 之后。**两个 `Match*ChairCards` 函数都要改,顺序一致。**

### 改动 2:配牌起点【改配置】

**文件**:`makedeal.json` → `MakeDealStrategy.old2`

```diff
- "BeginMakeNum": 12,
+ "BeginMakeNum": 10,
```

前 10 张固定发,第 11 张起做牌(做牌范围 7 张,比现状 5 张多 2 张,换更低手数)。

### 改动 3(可选,不阻塞):修第二轮无条件做牌 bug【改代码】

**文件**:`zgdatbl.cpp` `DoMakeDeal`(快照 `4187`),三处 `else if (i > 0)`(快照 `4214`/`4231`/`4248`)。

**现状**:第二轮起对所有家无条件做牌。**建议**:加 `triggered[chair]` 标记,第一轮满足阈值的家才标 `triggered`,后续轮只续做 `triggered` 的家。

> 模拟器显示修这个 bug 边际效果很小(炸弹 0.027→0.026),**不是主因**,可单独评估、不阻塞改动 1+2。

---

## 四、改前后数据(模拟器,N=4000~8000 局 × 3 家)

| 改法 | 炸弹 | 手数 | 庄炸 | 闲炸 | 庄闲差 |
|---|---|---|---|---|---|
| 现状(baseline, bmn=12) | 0.42 | 6.08 | 0.44 | 0.41 | 0.033 |
| **改动1+2(control-flow, bmn=10)** | **0.20** | **5.84** | 0.18 | 0.21 | 0.025 |
| 对照:只改菜单(control-flow, bmn=12) | 0.155 | 6.27 | 0.14 | 0.16 | 0.025 |
| 对照:纯随机 | 0.19 | 7.50 | — | — | — |

牌型结构(`structure` 子命令):去 `MatchBomb` 后,**顺子/连对/飞机 增多、单牌减少**——牌更连贯成型,不是变散。

---

## 五、回归风险与监控

**风险**:
- 炸弹减少 → 玩家"爽感"可能下降(但贴近真实,符合产品方向);**王炸不受影响**(做牌不动双王成炸)。
- 做牌范围 5→7 张 → 牌被干预略多(仍远好于纯随机)。
- 庄闲炸弹差距方向变为"闲略多于庄",需观察是否影响真人胜率。

**上线必看指标**(对照灰度前):
- 单家平均炸弹数 `bomb_cnt`(应 ~0.42 → ~0.20);
- 对局时长 / 手数分布(手数应略降);
- 真人胜率 / 地主占比(防过度偏移);
- **留存**(尤其经典初级房新手,最敏感);
- **付费**(炸弹减少是否影响付费/爽感相关指标)。

---

## 六、灰度建议

1. 先**经典初级房**(4484/12074)灰度——体量大、影响可控;
2. 灰度期 A/B 对比 炸弹/胜率/留存/付费 vs 对照组;
3. 观察 1~2 周无异常 → 全量经典房(中高级房同步 old2 改动)。

---

## 七、验证方法

- **线下**:`algorithm/native/old2_type0_sim.py`
  ```bash
  py -3 old2_type0_sim.py run --fix control-flow --bmn 10 --n 20000
  py -3 old2_type0_sim.py structure --fix control-flow --bmn 10
  ```
- **线上**:灰度后取 `dws_ddz_daily_game` 的 `bomb_cnt` / `card_power` 分布对照(注意 `card_power` 在 2026-07 PRD 改版后有断档,见 memory `project_cardpower-formula-prd-change-2026-07`)。

---

## 八、附录:模拟器与参照源

- 模拟器:`algorithm/native/old2_type0_sim.py`(+ `old2_type0_sim_README.md`)
- 线上代码参照:`algorithm/native/previous/`(`zgdatbl.cpp` 等,全 UTF-8)+ `makedeal.json`
- 相关 memory:`project_cardpower-formula-prd-change-2026-07`、`feedback_avoid-narrative-data-interpretation`
