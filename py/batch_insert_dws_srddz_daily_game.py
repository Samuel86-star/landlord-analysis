#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_srddz_daily_game via DELETE + INSERT, day by day.

Source: tcy_temp.srddz_daily_game_raw (game_id=105，双行：服务费+结算)
Target: tcy_temp.dws_srddz_daily_game (整局一行，T-1 可用)

> raw 层双行（服务费行 result_id IS NULL + 结算行 result_id IS NOT NULL）通过条件聚合合并为 1 行。
> role/robot 取自结算行（服务费行是叫地主前快照，与结算行可能不同）。
> 上游 raw 已通过 min_dt 覆盖 dt，单天 dt = '{dt}' 即可正确收集跨天对局。

Usage:
    py -3 -u .\batch_insert_dws_srddz_daily_game.py --start 20260702 --end 20260702
    py -3 -u .\batch_insert_dws_srddz_daily_game.py --start 2026-07-02 --end 2026-07-05 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_srddz_daily_game
SELECT
    game_id,
    dt,
    uid,
    resultguid,
    MAX(CASE WHEN result_id IS NULL THEN game_datetime END) AS start_datetime,
    MAX(CASE WHEN result_id IS NOT NULL THEN game_datetime END) AS end_datetime,
    MAX(CASE WHEN result_id IS NOT NULL THEN timecost END) AS time_cost,
    MAX(room_id) AS room_id,
    MAX(room_currency_lower) AS room_currency_lower,
    MAX(room_currency_upper) AS room_currency_upper,
    MAX(CASE WHEN result_id IS NOT NULL THEN robot END) AS robot,
    MAX(CASE WHEN result_id IS NOT NULL THEN `role` END) AS role,
    MAX(chairno) AS chairno,
    MAX(result_id) AS result_id,
    8 AS play_mode,
    MAX(basedeposit) AS room_base,
    MAX(CASE WHEN result_id IS NULL THEN fee END) AS room_fee,
    MAX(CASE WHEN result_id IS NULL THEN olddeposit END) AS start_money,
    MAX(CASE WHEN result_id IS NOT NULL THEN end_deposit END) AS end_money,
    MAX(CASE WHEN result_id IS NOT NULL THEN depositdiff END) AS game_outcome_money,
    IFNULL(MAX(cut), 0) AS is_escape,
    MAX(CASE WHEN result_id IS NOT NULL THEN magnification END) AS magnification,
    MAX(CASE WHEN result_id IS NOT NULL THEN magnification_stacked END) AS magnification_stacked,
    MAX(CASE WHEN result_id IS NOT NULL THEN app_id END) AS app_id,
    MAX(CASE WHEN result_id IS NOT NULL THEN app_code END) AS app_code,
    MAX(CASE WHEN result_id IS NOT NULL THEN group_id END) AS group_id,
    MAX(CASE WHEN result_id IS NOT NULL THEN channel_id END) AS channel_id,
    MAX(CASE WHEN result_id IS NOT NULL THEN afk_turn_cnt END) AS afk_turn_cnt
FROM tcy_temp.srddz_daily_game_raw
WHERE game_id = 105
  AND dt = '{dt}'
GROUP BY game_id, dt, uid, resultguid"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_srddz_daily_game "
    "WHERE game_id = 105 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_srddz_daily_game "
    "WHERE game_id = 105 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_srddz_daily_game",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("srddz_daily_game_raw",),
    )
