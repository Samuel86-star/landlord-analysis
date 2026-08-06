---
room_id: 房间ID
房间名: （填，如"经典初级房"）
玩法: 经典玩法        # 经典玩法 / 不洗牌 / 四人斗地主 / ...
底分: （填）
当前策略: old2         # makedeal.json 里的策略名：old2 / new / new2 / b1 / level4 / robot / ...
MakeDealType: 0       # 0=Type0(菜单做牌) / 1=Type1(CouPaiStrategy 拼牌)
状态: 现状             # 现状 / 灰度中 / 已改造
---

# 房间 {{room_id}} · {{房间名}}

## 当前发牌参数

> 来源：`makedeal.json` → `MakeDealStrategy.<当前策略>`

| 参数 | 值 |
|---|---|
| MakeDealType | |
| BeginMakeNum | |
| BeginSelectBanker | |
| CouPaiStrategy | |
| TargetValue | |
| TargetRound | |
| BigCardsTo | |
| FirstChairHandCount / BombCount / BigCardsCount | |
| OtherChairHandCount / BombCount / BigCardsCount | |

（不洗牌房改记 `NoShuffStrategy` / `NoShuffProbability` / `NoShuff2KProbability` 等。）

## 现状指标

> 口径：harness（C++ 真值）+ **最优拆牌** `optimal_split.h`；炸弹=**持有**、手数=**人均最优手数**、P=sigmoid(牌力/40)。

| 指标 | 模拟器预测(N=20000) | 线上实测 | 备注 |
|---|---|---|---|
| 持有炸(人均) | | | 自然≈0.189 |
| 单局炸弹率 | | | 自然≈0.461 |
| 0/1/2/3+炸密度 | | | |
| 人均最优手数 | | | 自然≈7.34（越小越顺）|
| 人均散牌 | | | 自然≈3.83 |
| 首叫诱导度 P_max/P_avg | | | 自然≈1.328 |
| 抗衡度 (P_mid+P_min)/P_max | | | 自然≈1.299 |
| 牌力 V | | | |

> 模拟器：`algorithm/native/extracted/harness.exe`（注入式候选，三家同策略），聚合用 `anchor_check.py`。
> 线上口径：`dws_ddz_daily_game.bomb_cnt`（持有）/ `bomb_bet`（打出，注意 ≠ 持有）。

## 改造建议

- 目标（经典低等级房）：炸弹维度**趋近纯随机**（单局炸率≈0.461、持有≈0.189）、手数顺、庄闲均衡有悬念。
- 候选：参考 [`../../algorithm/native/extracted/top20_report.md`](../../algorithm/native/extracted/top20_report.md) 的 Type0/Type1 TOP20 选。
- 选定配置：

## 备注

- 上线/灰度状态、切点窗口、关联 memory 等。
- 关联：源码逆向 [`./742-420-reverse-analysis.md`](./742-420-reverse-analysis.md)、审计 [`./makedeal-code-quality-audit.md`](./makedeal-code-quality-audit.md)。
