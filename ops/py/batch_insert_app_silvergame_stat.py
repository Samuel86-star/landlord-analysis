#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_app_silvergame_stat via DELETE + INSERT, day by day.

Source: dws_ddz_daily_game (play_mode IN 1,2,3) UNION ALL dws_crazyddz_daily_game (play_mode=7)
Target: tcy_temp.dws_app_silvergame_stat (uid × dt 银子玩法金流+参与度)
仅 APP 端真人。

Usage:
    py -3 -u .\batch_insert_app_silvergame_stat.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_app_silvergame_stat.py --start 2026-06-01 --end 2026-06-08 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_app_silvergame_stat
WITH unified AS (
    -- 经典系（单轮）
    SELECT
        app_id, uid, dt, resultguid,
        game_datetime AS event_time,
        timecost AS time_cost,
        room_id, result_id,
        start_money, end_money,
        game_outcome_money, room_fee,
        CASE WHEN cut = 0 THEN 0 ELSE 1 END AS escape_flag,
        play_mode,
        0 AS is_crazyddz,
        1 AS settle_count
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53
      AND dt = '{dt}'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND play_mode IN (1, 2, 3)
    UNION ALL
    -- 510K（多轮累计，已合并为整局一行）
    SELECT
        app_id, uid, dt, resultguid,
        start_datetime AS event_time,
        time_cost,
        room_id, result_id,
        start_money, end_money,
        game_outcome_money, room_fee,
        CASE WHEN is_escape = 0 THEN 0 ELSE 1 END AS escape_flag,
        7 AS play_mode,
        1 AS is_crazyddz,
        settle_count
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521
      AND dt = '{dt}'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
),
ranked AS (
    SELECT *,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY event_time ASC)  AS seq_asc,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY event_time DESC) AS seq_desc
    FROM unified
),
streaks AS (
    SELECT
        app_id, uid, dt,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT app_id, uid, dt, result_id, grp, COUNT(*) AS streak_len
        FROM (
            SELECT app_id, uid, dt, result_id,
                seq_asc - ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt, result_id ORDER BY seq_asc) AS grp
            FROM ranked
            WHERE result_id IN (1, 2)
        ) g
        GROUP BY app_id, uid, dt, result_id, grp
    ) s
    GROUP BY app_id, uid, dt
)
SELECT
    r.app_id,
    r.uid,
    r.dt,
    COUNT(resultguid) AS game_count,
    SUM(r.time_cost) AS total_play_seconds,
    ROUND(AVG(r.time_cost), 1) AS avg_game_seconds,
    COUNT(DISTINCT r.room_id) AS distinct_rooms,
    COUNT(CASE WHEN r.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN r.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN r.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ROUND(COUNT(CASE WHEN r.result_id = 2 THEN 1 END) * 100.0 / COUNT(*), 2) AS lose_rate,
    ANY_VALUE(st.max_win_streak) AS max_win_streak,
    ANY_VALUE(st.max_lose_streak) AS max_lose_streak,
    MAX(CASE WHEN r.seq_asc = 1 THEN r.start_money END) AS start_money,
    MAX(CASE WHEN r.seq_desc = 1 THEN r.end_money END) AS end_money,
    MAX(r.end_money) AS money_peak,
    MIN(r.end_money) AS money_valley,
    SUM(r.game_outcome_money) AS total_diff_money,
    SUM(r.room_fee) AS total_fee_paid,
    SUM(r.escape_flag) AS escape_count
FROM ranked r
LEFT JOIN streaks st ON r.app_id = st.app_id AND r.uid = st.uid AND r.dt = st.dt
GROUP BY r.app_id, r.uid, r.dt"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_app_silvergame_stat "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_app_silvergame_stat "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_app_silvergame_stat",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("dws_ddz_daily_game", "dws_crazyddz_daily_game"),
    )
