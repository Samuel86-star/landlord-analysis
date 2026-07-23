# 数据陷阱 / 反直觉点（解读结果前过一遍）

## 0. 取数铁律：优先 DWS，raw 仅 fallback

分析一律查 `dws_*_daily_game`（矫正层：货币统一 / JSON 已解析 / 双行已合并 / 玩法已分类），**不要直接查 `*_daily_game_raw`**——raw 有脏数据 / 缺损 / 双行 / 未解析 JSON，统计会出错。仅 DWS 缺字段或对账排障才回 raw。详见 [game-combat-analysis.md](game-combat-analysis.md) §零。

## 1. `robot` 字段在练习房不可信

- 练习房（1124–1126，room_level=A:练习房；**注意不是"新手房"**——线上"新手房"是另一档 room_level=B，经典对应 room 420）里 `robot` **全为 0**，但真人 seat 胜率高达 87–93%（三人 PvP 理论上限 67%）→ 对手其实是未标记的 AI / 陪玩，或机器人行未入战绩表。
- **结论**：不要用 `robot=1` 判断练习房的机器人占比。要坐实保护机制，用 DWS 的 `shuffle_type` 列（=201 新手保护机器人；ddz DWS 已解析为独立列）。srddz 用 `is_robot`（字典叠加）更可靠。
- **房间等级 / 玩法以 `tcy_temp.dq_game_room_config` 为准**（`game_rule` + `room_level`），不要用底分或 `play_mode` 反推——1124-1126 的事实表 `play_mode=0` 只是练习房日志标记。

## 2. 高胜率（>67%）= 新手保护信号

任何房间真人 seat 胜率 > 67%（农民方理论上限）都说明存在保护 / 陪玩；胜率越高、保护越强。渠道间保护强度可能不同（实测 zgdx-iOS 93% > zgdx 安卓 87% > zgde 65%）。

保护强度可直接量化：`shuffle_type=201`（新手保护机器人）的行占比越高 → 保护越强、胜率越高（实测 1124 房 zgdx-iOS 97%→胜率 93%）；**占比 = 0 的房间（实测 1126）是无保护的真实 PvP 房**，胜率回归 40–52%、用户净亏银。

## 3. 银子输赢字段（DWS 统一后）

- DWS 已统一：`game_outcome_money` = 不含服务费的输赢（正=赢）；`room_fee` = 服务费。
- **账户真实净盈亏用 `end_money - start_money`**（含服务费与一切补贴，最准），等价 `game_outcome_money - room_fee`。
- raw 的 `depositdiff`（含服务费变动）已不推荐直接用，改用 DWS。
- 新手房常见用户净赚（保护补贴）；若某房间用户净亏（如 1126），可能是保护递减 / 更高赌注的"硬房间"。

## 4. app_code ≠ channel_id

两个独立维度（见 [identifier-map.md](identifier-map.md)）。"zgdx 用户"指 `app_code`，别去 join channel 维表。

## 5. iOS zgdx：登录 DAU 早于对局

`zgdx` iOS 登录 DAU 自 2026-06-25 即存在（~2600/天），但进入特定房间（如新手房）是 2026-07-13 才有。算 iOS 渗透率前先确认：是包体 7/13 上线，还是房间 7/13 才对 iOS 开放——影响分母口径。

## 6. 活跃表无 is_game_active

`dws_app_game_active` 无 `is_game_active` 字段，**行存在即活跃**，用 `uid IS NOT NULL` 判定。

## 7. 已下线 / 改名字段

写 SQL 前查 [SQL_STYLE.md](../../SQL_STYLE.md) 第八节：`multi_q4_*`、`is_game_active`、`reg_time`→`reg_datetime`、`game_datetime`→`start_datetime` 等，别凭记忆写。

## 8. 测试号污染

`zgda` + `group_id=1` 为内部测试号（每房间固定 ~50 人、人均上百局）。分析真实用户时按 `app_code IN (真实集合)` 剔除。
