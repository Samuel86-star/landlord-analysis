# 注册后真实前三局分析定义

## 概念定义

| 概念 | 定义 | 是否限制房间或玩法 |
| ---- | ---- | ------------------ |
| 注册后首局 | 注册时间之后，`dws_ddz_daily_game` 中按 `game_datetime, resultguid` 排序的第一条真人 `game_id=53` 对局 | 否 |
| 注册后第 2/3 局 | 同一排序下的第 2/3 条对局 | 否 |
| 首次进入经典初级房 | 注册后第一次进入配置维表标记为经典初级房的对局 | 是，单独计算进入局序 |
| 旧 4484/12074 cohort | 先过滤 `play_mode`，再要求最早剩余记录落在 4484/12074 | 历史子集，不代表注册首局 |

## 判定规则

- 注册表：`tcy_temp.dws_dq_app_daily_reg`，限定 `app_id=1880053` 和注册窗口。
- 对局表：`tcy_temp.dws_ddz_daily_game`，限定同一 `app_id`、`game_id=53`、`robot != 1`，且 `game_datetime >= reg_datetime`。
- 排序：`ROW_NUMBER() OVER (PARTITION BY uid ORDER BY game_datetime, resultguid)`。
- 房间玩法与等级：关联 `tcy_temp.dq_game_room_config`，不从 `play_mode` 或底分反推。
- `user_attr_bout` 只用于质检，不作为首局条件。2026-08-31 至 2026-09-06 的 7,407 名有局用户中，时间排序首局均为正数，主流值为 1，与旧文档的“首局为 0”不符。

## 最近一周验证

2026-08-31 至 2026-09-06 共识别 7,407 名注册后至少有一局斗地主的用户。真实首局房间以 1124 练习房为主，占 82.26%；4484/12074 合计仅占 9.25%。这证明旧 SQL 的 4484/12074 “首局”是预筛后的子集锚点。

正式查询见 [03_registered_first3_detail_20260831_20260906.sql](../../../ops/py/first-classic-beginner/sql/03_registered_first3_detail_20260831_20260906.sql)。
