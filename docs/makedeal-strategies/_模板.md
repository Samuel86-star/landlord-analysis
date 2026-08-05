---
room_id: 房间ID
房间名: （填，如"经典初级房"）
玩法: 经典玩法        # 经典玩法 / 不洗牌 / 四人斗地主 / ...
底分: （填）
当前策略: old2         # makedeal.json 里的策略名：old2 / new / b1 / level4 / robot / ...
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

| 指标 | 模拟器预测 | 线上实测 | 备注 |
|---|---|---|---|
| 炸弹数 | | | 自然≈0.19 |
| 手数 | | | |
| 庄闲炸差 | | | |
| 单牌 / 庄闲大牌差 | | | 参考列 |

> 模拟器口径：`algorithm/native/old2_type0_sim.py`（Type0）/ `old2_type1_sim.py`（Type1），手数=t0拆。
> 线上口径：`dws_ddz_daily_game.bomb_cnt`（持有）/ `bomb_bet`（打出，注意 ≠ 持有）。

## 改造建议

- 目标：炸弹贴自然 0.19、手数低、庄闲均衡。
- 候选：（参考 `docs/tech/classic-makedeal-config-topn.md` 的 TOP 排名选）
- 选定配置：

## 备注

- 上线/灰度状态、切点窗口、关联 memory 等。
