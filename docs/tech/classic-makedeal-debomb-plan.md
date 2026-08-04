# 经典玩法做牌降炸弹 — 落地方案

> 目标:让经典玩法的发牌**贴近真实打牌、牌不烂、好打但不靠给炸弹**。
> 结论基于模拟器多维扫描(`old2_type0_sim.py` 为线上 Type0 的 1:1 复刻;`old2_type1_sim.py` 为 Type1 复刻)。
> **两条路线**:① **Type0 控制流**(改代码,主推,§二/§三);② **Type1 with-pair**(只改配置,替代,§二B)。
> 日期:2026-08-03 · 2026-08-04 增补 Type1 路线与路线对比 · 作者:分析团队

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

## 二B、替代路线:Type1(只改配置,不动代码)

> Type1 = `MakeDealType=1`,拼牌走 `CouPaiStrategy`(配置驱动,见 `zgdatbl.cpp:4041` + `MakeDealHelper.cpp:849`)。**免改 C++**,只改 `makedeal.json`,适合"纯配置灰度、不动代码"的场景。

### 推荐配置(`makedeal.json` → `MakeDealStrategy` 新增/改一套)

```json
"with-pair": {
    "MakeDealType": 1,
    "BeginMakeNum": 14,
    "BeginSelectBanker": 17,
    "TargetValue": 999,
    "TargetRound": 10,
    "CouPaiStrategy": [[4, 6, 5, 2, 3]]
}
```

> 码:2对 3三 4顺 5连对 6飞机 13炸。`[4,6,5,2,3]` = 顺→飞机→连对→对→三(**收对**),不含炸码 13(不主动凑炸)。
> `with-pair`(收对,成型好打,炸弹略高) ↔ `no-pair [4,5,3,6]`(不收对,少炸偏自然,=线上 `new`)。两者就差对子码 `2`。
> `TargetValue=999` = 恒拼牌(最可控);`tv=0.6/tr=4` 同效;避免 `tv=0.6/tr=10`(炸弹会升到 0.29)。

### 效果(N=10000,与 Type0 同口径:同副牌、同拆牌 t0拆)

| | 炸弹 | 手数 | 庄闲差 |
|---|---|---|---|
| **Type1 with-pair b14 sel17** | **0.202** | **6.17** | **+0.002** |
| 纯随机参照 | 0.194 | 7.50 | — |

b14 sel17 = 14 随机 + 3 张拼满(干预最轻)。炸弹 0.202 贴经典、庄闲差 +0.002 几乎均衡;手数 6.17(好于自然 7.5,但不及 Type0 控制流 5.82)。

### 路线对比

| 路线 | 改动 | 炸弹 | 手数 | 庄闲差 | 适用 |
|---|---|---|---|---|---|
| **Type0 control-flow bmn10**(主推) | 改代码+配置 | **0.196** | **5.82** | -0.025 | 炸弹&手数双优 |
| **Type1 with-pair b14 sel17**(替代) | **只改配置** | 0.202 | 6.17 | +0.002 | 免改代码、灰度快 |
| 现状(Type0 baseline) | — | 0.433 | 6.09 | — | 改前 |
| 纯随机参照 | — | 0.194 | 7.50 | — | — |

**怎么选**:
- 能改代码 → **Type0 控制流**(双指标都更好)。
- 只能改配置 → **Type1 with-pair b14 sel17**(到经典级炸弹,代价是手数比 Type0 高 ~0.35 手)。
- 经典 0.19 炸档的前沿点是 Type0 控制流;**Type1 给不出"0.19 炸 + 低手数"**——它的低手数靠收对收三,必然带炸。

### 机制差别(为什么 Type0 手数更低)

两算法每次都只拉 1 张牌(不是"多张 vs 单张"),差在两点:

1. **覆盖率**:Type0 把所有空位都走菜单补满;Type1 只补 `[BeginMakeNum, BeginSelectBanker)`,**剩下随机填**(select=15 时只拼一部分)。推荐配置用 **sel17 补满空位 + b14 只拼 3 张**,干预最轻 → 0.20 炸/6.17 手;若把 bmn 再降到 12(sel17)拼更多,手数降到 5.63 但炸弹升到 0.25。
2. **收尾脾气**:同覆盖率下,Type1 爱收对/三(每家对 2.2、三 1.0 vs Type0 对 1.6、三 0.7),手数更低但炸弹更多;Type0 控制流留单牌(单 3.7 vs 2.7),手数偏高但炸弹贴自然。两者坐在"手数↔炸弹"权衡的两端。

> 即"硬编码菜单 vs 配置 CouPaiStrategy"只是表层;底层差在**覆盖率**与**收尾脾气**。

### 备注:Type1 `new` 策略(房间 742)已接近自然

线上 `new`(`[4,5,3,6]`,bmn11)实测 **炸弹 0.160**(比纯随机还低,因不含对子码 2、不聚集),手数 6.85。**742 房本身不是"多炸"问题房**;多炸的是跑 `old2`/Type0 baseline 的经典房(0.43)。

---

## 三、改动点

> 本节为 **Type0 主路线**的代码改动;**Type1 替代路线**只改配置,见 §二B。

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

- **线下 Type0**:`algorithm/native/old2_type0_sim.py`
  ```bash
  py -3 old2_type0_sim.py run --fix control-flow --bmn 10 --n 20000
  py -3 old2_type0_sim.py structure --fix control-flow --bmn 10
  ```
- **线下 Type1**:`algorithm/native/old2_type1_sim.py`
  ```bash
  py -3 old2_type1_sim.py run --coupai with-pair --begin 14 --select 17 --tv 999 --tr 10 --n 20000
  py -3 old2_type1_sim.py sweep --n 5000
  ```
- **路线对比**:`old2_type1_sim.py` 的 `_test_grid_t1` 输出 Type1 Pareto 前沿(策略×begin)。
- **线上**:灰度后取 `dws_ddz_daily_game` 的 `bomb_cnt` / `card_power` 分布对照(注意 `card_power` 在 2026-07 PRD 改版后有断档,见 memory `project_cardpower-formula-prd-change-2026-07`)。

---

## 八、附录:模拟器与参照源

- 模拟器(Type0):`algorithm/native/old2_type0_sim.py`(+ `old2_type0_sim_README.md`)
- 模拟器(Type1):`algorithm/native/old2_type1_sim.py`(+ `old2_type1_sim_README.md`)
- 线上代码参照:`algorithm/native/previous/`(`zgdatbl.cpp` Type1 分支 `3993`、`MakeDealHelper.cpp` `MakeDeal_ComposeCard 771`,全 UTF-8)+ `makedeal.json`
- 相关 memory:`project_cardpower-formula-prd-change-2026-07`、`feedback_avoid-narrative-data-interpretation`
