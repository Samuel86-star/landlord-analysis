# 数据陷阱 / 反直觉点（解读结果前过一遍）

## 0. 取数铁律：优先 DWS，raw 仅 fallback

分析一律查 `dws_*_daily_game`（矫正层：货币统一 / JSON 已解析 / 双行已合并 / 玩法已分类），**不要直接查 `*_daily_game_raw`**——raw 有脏数据 / 缺损 / 双行 / 未解析 JSON，统计会出错。仅 DWS 缺字段或对账排障才回 raw。详见 [game-combat-analysis.md](game-combat-analysis.md) §零。

## 1. 练习房分两种：rigged 配牌房 vs 低额练习房

练习房 tier（room_level=A:练习房，取自 `dq_game_room_config`）实际含两类，查询时须区分：

- **rigged 配牌练习房**（1124/1125/1126，`play_mode=0`，game_rule 分别 经典/不洗牌/癞子）：`robot` 字段**可信**（0=真人、1=机器人），每局恰好 1 真人（`WHERE robot=0` 的行数 = `COUNT(DISTINCT resultguid)`）。真人胜率远超 67% 是机器人被配置为陪玩/放水所致（`shuffle_type=201` 标记保护局），不是 robot 字段有问题。
- **低额练习房**（742，`play_mode=1`，经典）：正常 3 人 PvP，`robot` 同标准用法 `robot!=1` 过滤即可。

**结论**：先 JOIN `dq_game_room_config` 拿 room_level；如需区分 rigged vs 低额，用 `play_mode` 判定（0=rigged 配牌，1=正常 PvP）。srddz 用 `is_robot`（字典叠加判据）。

## 2. 高胜率（>67%）= 保护 / 陪玩信号

三人斗地主正常 PvP 真人 seat 胜率理论区间约 33–67%（1 地主 vs 2 农民的结构使然）。**真人 seat 胜率持续 >67% 说明存在保护/陪玩机制**；胜率越高、保护越强。渠道/平台之间保护强度可能不同。

保护强度可通过 `shuffle_type=201`（新手保护机器人）的行占比量化——占比越高 → 保护越强。`shuffle_type=201` 占比=0 的房间若胜率回归 40–52%，通常为无保护的真实 PvP。

## 3. 银子输赢字段（DWS 统一后）

- DWS 已统一：`game_outcome_money` = 不含服务费的输赢（正=赢）；`room_fee` = 服务费。
- **账户真实净盈亏用 `end_money - start_money`**（含服务费与一切补贴，最准），等价 `game_outcome_money - room_fee`。
- raw 的 `depositdiff`（含服务费变动）已不推荐直接用，改用 DWS。
- 练习房（rigged 配牌房）常见用户净赚（保护补贴）；若某房间用户净亏，可能保护较弱或为正常 PvP。

## 4. app_code ≠ channel_id

两个独立维度（见 [identifier-map.md](identifier-map.md)）。"zgdx 用户"指 `app_code`，别去 join channel 维表。

## 5. app_code × 平台的子群上线时间可能不同步

新包体/新平台的 DAU 可能早于其对局数据出现（登录先有，房间逐步开放）。做渗透率/覆盖率分析前，先确认目标子群（app_code × group_id）在目标房间是否已有对局——避免 DAU 分母有值但对局分子为空。

> 典型 case：zgdx 的 iOS 端登录 DAU 早于其进入特定房间的对局数据。

## 6. 活跃表无 is_game_active

`dws_app_game_active` 无 `is_game_active` 字段，**行存在即活跃**，用 `uid IS NOT NULL` 判定。

## 7. 已下线 / 改名字段

写 SQL 前查 [SQL_STYLE.md](../../SQL_STYLE.md) 第八节：`multi_q4_*`、`is_game_active`、`reg_time`→`reg_datetime`、`game_datetime`→`start_datetime` 等，别凭记忆写。

## 8. 测试号污染

`zgda` + `group_id=1` 为内部测试号（极高人均局数、跨多房间分布）。分析真实用户时按 `app_code IN (真实集合)` 剔除，或加 `NOT (app_code='zgda' AND group_id=1)`。

## 9. 字段有效性窗口（用到前先确认上线日）

- **`card_power`（牌力）**：2026-06-15 修复算法后才可信；做牌力分析必须用 **≥ 2026-06-15** 的窗口，之前数据不可用。
- **`hand_cards`（手牌）**：上报功能 2026-06-25 上线；DWS 层历史分区 hand_cards 列存在但为**空字符串**（非 NULL），须用 `LENGTH(CAST(hand_cards AS STRING)) > 0` 判定是否有数据，不要用 `IS NOT NULL`。分析窗口必须 ≥ 2026-06-25。

## 10. 描述与数字的解读规范

- **箭头 A→B→C 描述是时序衰减，不是菜单选项**；解读游戏机制文档时别把状态流转当成并列项。
- **"默认" = 无操作的结果**（如明牌不点 = 不明牌 ×1），不是"高亮推荐档"。
- **禁止叙事化解读数字差异**：两个数字不同，不准编"用户心理/策略"故事；必须先查机制字段（`shuffle_type` / `role` / `play_mode` / `magnification` 等）才能下结论。
