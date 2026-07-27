#!/usr/bin/env python3
"""Batch backfill tcy_temp.srddz_daily_game_raw via DELETE + INSERT, day by day.

Source: tcy_dwd.dwd_game_combatgains_si (SR 内表，game_id=105，dt 为 int YYYYMMDD)
Target: tcy_temp.srddz_daily_game_raw (ODS 迁移表，四人斗地主银子玩法原始字段)

> 覆盖内嵌于斗地主 app(app_id=1880053)的四人斗地主银子玩法。陪玩机器人登记在 1880105 下、按银子结算，
> 用 has_target_app(触及 1880053)保留整局，与 crazyddz raw 同构。
> 跨天对局处理：扫描范围 [T-1 23:00, T+1 01:00]（{dt_prev} 23:00 至 {dt_next_str} 01:00），用 MIN(dt) OVER(PARTITION BY resultguid)
> 归属为 min_dt，仅保留 min_dt = {dt_int} 的记录。
> 同玩家同局保留 2 行（服务费行 + 结算行），由下游 dws_srddz_daily_game 合并。
> 房间过滤：WHERE room IN (927, 928, 930)（上游 dwd_game_combatgains_si 的房间口径是 `room`，不是 `room_id`），与 game_id=105 联合防御。输出到 raw 时起别名为 room_id。

Usage:
    py -3 -u .\batch_insert_srddz_daily_game_raw.py --start 20260702 --end 20260702
    py -3 -u .\batch_insert_srddz_daily_game_raw.py --start 2026-06-01 --end 2026-06-08 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.srddz_daily_game_raw
WITH base_data AS (
    -- Source scan window: [T-1 23:00, T+1 01:00]
    --   - T-1 partition: only the 23:00-23:59 tail (cross-day from previous day)
    --   - T partition  : full day
    --   - T+1 partition: only the 00:00-00:59 head (cross-day spillover)
    -- min_dt then folds the whole resultguid to its earliest day
    -- so {dt} catches T's records AND the cross-day tails attached to T's resultguids.
    SELECT
        game_id, uid, time_unix, resultguid, timecost,
        room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
        basedeposit, olddeposit, end_deposit, fee, depositdiff,
        cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, app_id, app_code, afk_turn_cnt,
        MIN(dt) OVER (PARTITION BY resultguid) AS min_dt,
        MAX(CASE WHEN app_id = 1880053 THEN 1 ELSE 0 END) OVER (PARTITION BY resultguid) AS has_target_app
    FROM tcy_dwd.dwd_game_combatgains_si
    WHERE game_id = 105
      AND room IN (927, 928, 930)
      AND dt IN ({dt_prev_int}, {dt_int}, {dt_next_int})
      AND time_unix >= UNIX_TIMESTAMP("{dt_prev} 23:00:00") * 1000
      AND time_unix <  UNIX_TIMESTAMP("{dt_next_str} 01:00:00") * 1000
)
SELECT
    game_id, min_dt AS dt, uid, FROM_UNIXTIME(time_unix / 1000) AS game_datetime, resultguid, timecost,
    room AS room_id, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
    basedeposit, olddeposit, end_deposit, fee, depositdiff,
    cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, IFNULL(app_id, 1880053) AS app_id, app_code, afk_turn_cnt
FROM base_data
WHERE has_target_app = 1
  AND min_dt = {dt_int}"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.srddz_daily_game_raw "
    "WHERE game_id = 105 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.srddz_daily_game_raw "
    "WHERE game_id = 105 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="srddz_daily_game_raw",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("tcy_dwd.dwd_game_combatgains_si",),
    )
