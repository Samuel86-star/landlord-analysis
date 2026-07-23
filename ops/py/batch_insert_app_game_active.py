#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_app_game_active via DELETE + INSERT, day by day.

Source: dws_ddz_daily_game (game_id=53) UNION ALL dws_crazyddz_daily_game (game_id=521)
Target: tcy_temp.dws_app_game_active (uid × dt × app_id 活跃清单，留存 flag 专用)
仅 APP 端真人（group_id IN (...)，robot != 1）。

Usage:
    py -3 -u .\batch_insert_app_game_active.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_app_game_active.py --start 2026-03-01 --end 2026-06-01 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_app_game_active
SELECT app_id, uid, dt
FROM (
    -- 经典斗地主
    SELECT app_id, uid, DATE(dt) AS dt
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53
      AND dt = '{dt}'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    UNION ALL
    -- 疯狂斗地主（game_id = 521）
    SELECT app_id, uid, dt
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521
      AND dt = '{dt}'
      AND app_id = {app_id}
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
) t
GROUP BY app_id, uid, dt"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_app_game_active "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_app_game_active "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_app_game_active",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("dws_ddz_daily_game", "dws_crazyddz_daily_game"),
    )
