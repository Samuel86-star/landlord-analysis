#!/usr/bin/env python3
"""Batch backfill tcy_temp.crazyddz_daily_game_raw via DELETE + INSERT, day by day.

Source: tcy_dwd.dwd_game_combatgains_si (SR 内表，game_id=521，dt 为 int YYYYMMDD)
Target: tcy_temp.crazyddz_daily_game_raw (ODS 迁移表，510K 原始字段)

> 跨天对局处理：扫描范围 [T, T+1]（{dt_int} ~ {dt_next_int}），用 MIN(dt) OVER(PARTITION BY resultguid)
> 归属为 min_dt，仅保留 min_dt = {dt_int} 的记录；并过滤 has_target_app=1（含 app_id=1880053 的对局）。
> T 日回填时 T+1 源数据需已产出（510K 为 T-1 可用，日常回填 dt-1 即可）。

Usage:
    py -3 -u .\batch_insert_crazyddz_daily_game_raw.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_crazyddz_daily_game_raw.py --start 2026-06-01 --end 2026-06-10 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.crazyddz_daily_game_raw
WITH base_data AS (
    SELECT
        game_id, uid, time_unix, resultguid, timecost,
        room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
        basedeposit, olddeposit, end_deposit, fee, depositdiff,
        cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, app_id, app_code, afk_turn_cnt,
        MIN(dt) OVER (PARTITION BY resultguid) AS min_dt,
        MAX(CASE WHEN app_id = 1880053 THEN 1 ELSE 0 END) OVER (PARTITION BY resultguid) AS has_target_app
    FROM tcy_dwd.dwd_game_combatgains_si
    WHERE game_id = 521
      AND dt BETWEEN {dt_int} AND {dt_next_int}
)
SELECT
    game_id, min_dt AS dt, uid, FROM_UNIXTIME(time_unix / 1000) AS game_datetime, resultguid, timecost,
    room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
    basedeposit, olddeposit, end_deposit, fee, depositdiff,
    cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, IFNULL(app_id, 1880521) AS app_id, app_code, afk_turn_cnt
FROM base_data
WHERE has_target_app = 1
  AND min_dt = {dt_int}"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.crazyddz_daily_game_raw "
    "WHERE game_id = 521 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.crazyddz_daily_game_raw "
    "WHERE game_id = 521 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="crazyddz_daily_game_raw",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("tcy_dwd.dwd_game_combatgains_si",),
    )
