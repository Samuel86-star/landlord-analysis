#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_app_daily_allgame_stat via DELETE + INSERT, day by day.

Source: tcy_temp.dws_app_allgame_stat (uid × dt × play_mode)
Target: tcy_temp.dws_app_daily_allgame_stat (uid × dt 聚合)

依赖：本表依赖 dws_app_allgame_stat，执行前请确保对应日期已回填。

Usage:
    py -3 -u .\\batch_insert_daily_allgame_stat.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_daily_allgame_stat.py --start 2026-03-01 --end 2026-06-16 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_app_daily_allgame_stat
SELECT
    app_id, dt, uid,
    COUNT(DISTINCT play_mode)     AS distinct_modes,
    SUM(game_count)               AS total_games,
    AVG(avg_magnification)        AS avg_magnification,
    MAX(max_magnification)        AS max_magnification,
    SUM(bomb_0_games + bomb_1_games + bomb_2_games + bomb_3plus_games) AS bomb_count,
    SUM(spring_count)             AS spring_count,
    SUM(multi_1_win)          AS multi_1_win,          SUM(multi_1_lose)         AS multi_1_lose,
    SUM(multi_2_win)          AS multi_2_win,          SUM(multi_2_lose)         AS multi_2_lose,
    SUM(multi_3_6_win)        AS multi_3_6_win,        SUM(multi_3_6_lose)       AS multi_3_6_lose,
    SUM(multi_6_12_win)       AS multi_6_12_win,       SUM(multi_6_12_lose)      AS multi_6_12_lose,
    SUM(multi_12_24_win)      AS multi_12_24_win,      SUM(multi_12_24_lose)     AS multi_12_24_lose,
    SUM(multi_24_48_win)      AS multi_24_48_win,      SUM(multi_24_48_lose)     AS multi_24_48_lose,
    SUM(multi_48_96_win)      AS multi_48_96_win,      SUM(multi_48_96_lose)     AS multi_48_96_lose,
    SUM(multi_96_192_win)     AS multi_96_192_win,     SUM(multi_96_192_lose)    AS multi_96_192_lose,
    SUM(multi_192_384_win)    AS multi_192_384_win,    SUM(multi_192_384_lose)   AS multi_192_384_lose,
    SUM(multi_384_plus_win)   AS multi_384_plus_win,   SUM(multi_384_plus_lose)  AS multi_384_plus_lose
FROM tcy_temp.dws_app_allgame_stat
WHERE app_id = {app_id}
  AND dt = '{dt}'
GROUP BY app_id, uid, dt"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_app_daily_allgame_stat "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_app_daily_allgame_stat "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_app_daily_allgame_stat",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("dws_app_allgame_stat",),
    )
