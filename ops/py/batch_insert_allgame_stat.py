#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_app_allgame_stat via DELETE + INSERT, day by day.

Source: dws_ddz_daily_game (play_mode 1~6) UNION ALL dws_crazyddz_daily_game (play_mode=7)
Target: tcy_temp.dws_app_allgame_stat (uid × dt × play_mode 全玩法体验统计，v1.2 固定倍数段)
仅 APP 端真人。

> SQL 已对齐 md v1.2：固定倍数段 20 字段，经典系取 ABS(real_magnification)、510K 取 total_magnification。

Usage:
    py -3 -u .\batch_insert_allgame_stat.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_allgame_stat.py --start 2026-06-01 --end 2026-06-08 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_app_allgame_stat
WITH ddz_modes AS (
    SELECT
        *,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode ORDER BY game_datetime ASC) AS game_seq,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode ORDER BY game_datetime DESC) AS rank_desc
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53
      AND dt = '{dt}'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
),
ddz_streaks AS (
    SELECT
        uid, play_mode, dt,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT uid, play_mode, dt, result_id, grp, COUNT(*) AS streak_len
        FROM (
            SELECT uid, play_mode, dt, result_id,
                game_seq - ROW_NUMBER() OVER (PARTITION BY uid, play_mode, dt, result_id ORDER BY game_seq) AS grp
            FROM ddz_modes
            WHERE result_id IN (1, 2)
        ) g
        GROUP BY uid, play_mode, dt, result_id, grp
    ) s
    GROUP BY uid, play_mode, dt
),
ddz_agg AS (
    SELECT
        g.app_id, g.play_mode, g.uid, g.dt,
        COUNT(*) AS game_count,
        SUM(g.timecost) AS total_play_seconds,
        ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
        COUNT(DISTINCT g.room_id) AS distinct_rooms,
        COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
        COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
        ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS win_rate,
        ROUND(COUNT(CASE WHEN g.result_id = 2 THEN 1 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS lose_rate,
        ANY_VALUE(s.max_win_streak),
        ANY_VALUE(s.max_lose_streak),
        ROUND(AVG(g.magnification), 2) AS avg_magnification,
        MAX(g.magnification) AS max_magnification,
        ROUND(AVG(ABS(g.real_magnification)), 2) AS avg_real_magnification,
        COUNT(CASE WHEN ABS(g.real_magnification) = 1 AND g.result_id = 1 THEN 1 END) AS multi_1_win,
        COUNT(CASE WHEN ABS(g.real_magnification) = 1 AND g.result_id = 2 THEN 1 END) AS multi_1_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) = 2 AND g.result_id = 1 THEN 1 END) AS multi_2_win,
        COUNT(CASE WHEN ABS(g.real_magnification) = 2 AND g.result_id = 2 THEN 1 END) AS multi_2_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 3 AND ABS(g.real_magnification) < 6 AND g.result_id = 1 THEN 1 END) AS multi_3_6_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 3 AND ABS(g.real_magnification) < 6 AND g.result_id = 2 THEN 1 END) AS multi_3_6_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 6 AND ABS(g.real_magnification) < 12 AND g.result_id = 1 THEN 1 END) AS multi_6_12_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 6 AND ABS(g.real_magnification) < 12 AND g.result_id = 2 THEN 1 END) AS multi_6_12_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 12 AND ABS(g.real_magnification) < 24 AND g.result_id = 1 THEN 1 END) AS multi_12_24_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 12 AND ABS(g.real_magnification) < 24 AND g.result_id = 2 THEN 1 END) AS multi_12_24_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 24 AND ABS(g.real_magnification) < 48 AND g.result_id = 1 THEN 1 END) AS multi_24_48_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 24 AND ABS(g.real_magnification) < 48 AND g.result_id = 2 THEN 1 END) AS multi_24_48_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 48 AND ABS(g.real_magnification) < 96 AND g.result_id = 1 THEN 1 END) AS multi_48_96_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 48 AND ABS(g.real_magnification) < 96 AND g.result_id = 2 THEN 1 END) AS multi_48_96_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 96 AND ABS(g.real_magnification) < 192 AND g.result_id = 1 THEN 1 END) AS multi_96_192_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 96 AND ABS(g.real_magnification) < 192 AND g.result_id = 2 THEN 1 END) AS multi_96_192_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 192 AND ABS(g.real_magnification) < 384 AND g.result_id = 1 THEN 1 END) AS multi_192_384_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 192 AND ABS(g.real_magnification) < 384 AND g.result_id = 2 THEN 1 END) AS multi_192_384_lose,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 384 AND g.result_id = 1 THEN 1 END) AS multi_384_plus_win,
        COUNT(CASE WHEN ABS(g.real_magnification) >= 384 AND g.result_id = 2 THEN 1 END) AS multi_384_plus_lose,
        COUNT(CASE WHEN g.bomb_bet / 2 = 0 THEN 1 END) AS bomb_0_games,
        COUNT(CASE WHEN g.bomb_bet / 2 = 1 THEN 1 END) AS bomb_1_games,
        COUNT(CASE WHEN g.bomb_bet / 2 = 2 THEN 1 END) AS bomb_2_games,
        COUNT(CASE WHEN g.bomb_bet / 2 >= 3 THEN 1 END) AS bomb_3plus_games,
        COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END) AS games_with_grab,
        COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END) AS games_player_doubled,
        COUNT(CASE WHEN g.complete_victory_bet = 2 THEN 1 END) AS spring_count,
        MAX(CASE WHEN g.game_seq = 1 THEN g.start_money END) AS start_money,
        MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END) AS end_money,
        MAX(g.end_money) AS money_peak,
        MIN(g.end_money) AS money_valley,
        SUM(g.game_outcome_money) AS total_diff_money,
        SUM(g.room_fee) AS total_fee_paid,
        COUNT(CASE WHEN g.cut != 0 THEN 1 END) AS escape_count,
        1 AS total_settle_rounds,
        1.0 AS avg_settle_rounds,
        0 AS outcome_gdp,
        1 AS max_settle_round_single
    FROM ddz_modes g
    LEFT JOIN ddz_streaks s ON g.uid = s.uid AND g.play_mode = s.play_mode AND g.dt = s.dt
    GROUP BY g.app_id, g.play_mode, g.uid, g.dt
),
crazyddz_agg AS (
    SELECT
        g.app_id,
        7 AS play_mode,
        g.uid, g.dt,
        COUNT(*) AS game_count,
        SUM(g.time_cost) AS total_play_seconds,
        ROUND(AVG(g.time_cost), 1) AS avg_game_seconds,
        COUNT(DISTINCT g.room_id) AS distinct_rooms,
        COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
        COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
        ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS win_rate,
        ROUND(COUNT(CASE WHEN g.result_id = 2 THEN 1 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS lose_rate,
        ANY_VALUE(str.win_streak),
        ANY_VALUE(str.lose_streak),
        ROUND(AVG(g.total_magnification), 2) AS avg_magnification,
        MAX(g.total_magnification) AS max_magnification,
        ROUND(AVG(ABS(g.game_outcome_money) / NULLIF(g.room_base, 0)), 2) AS avg_real_magnification,
        COUNT(CASE WHEN g.total_magnification = 1 AND g.result_id = 1 THEN 1 END) AS multi_1_win,
        COUNT(CASE WHEN g.total_magnification = 1 AND g.result_id = 2 THEN 1 END) AS multi_1_lose,
        COUNT(CASE WHEN g.total_magnification = 2 AND g.result_id = 1 THEN 1 END) AS multi_2_win,
        COUNT(CASE WHEN g.total_magnification = 2 AND g.result_id = 2 THEN 1 END) AS multi_2_lose,
        COUNT(CASE WHEN g.total_magnification >= 3 AND g.total_magnification < 6 AND g.result_id = 1 THEN 1 END) AS multi_3_6_win,
        COUNT(CASE WHEN g.total_magnification >= 3 AND g.total_magnification < 6 AND g.result_id = 2 THEN 1 END) AS multi_3_6_lose,
        COUNT(CASE WHEN g.total_magnification >= 6 AND g.total_magnification < 12 AND g.result_id = 1 THEN 1 END) AS multi_6_12_win,
        COUNT(CASE WHEN g.total_magnification >= 6 AND g.total_magnification < 12 AND g.result_id = 2 THEN 1 END) AS multi_6_12_lose,
        COUNT(CASE WHEN g.total_magnification >= 12 AND g.total_magnification < 24 AND g.result_id = 1 THEN 1 END) AS multi_12_24_win,
        COUNT(CASE WHEN g.total_magnification >= 12 AND g.total_magnification < 24 AND g.result_id = 2 THEN 1 END) AS multi_12_24_lose,
        COUNT(CASE WHEN g.total_magnification >= 24 AND g.total_magnification < 48 AND g.result_id = 1 THEN 1 END) AS multi_24_48_win,
        COUNT(CASE WHEN g.total_magnification >= 24 AND g.total_magnification < 48 AND g.result_id = 2 THEN 1 END) AS multi_24_48_lose,
        COUNT(CASE WHEN g.total_magnification >= 48 AND g.total_magnification < 96 AND g.result_id = 1 THEN 1 END) AS multi_48_96_win,
        COUNT(CASE WHEN g.total_magnification >= 48 AND g.total_magnification < 96 AND g.result_id = 2 THEN 1 END) AS multi_48_96_lose,
        COUNT(CASE WHEN g.total_magnification >= 96 AND g.total_magnification < 192 AND g.result_id = 1 THEN 1 END) AS multi_96_192_win,
        COUNT(CASE WHEN g.total_magnification >= 96 AND g.total_magnification < 192 AND g.result_id = 2 THEN 1 END) AS multi_96_192_lose,
        COUNT(CASE WHEN g.total_magnification >= 192 AND g.total_magnification < 384 AND g.result_id = 1 THEN 1 END) AS multi_192_384_win,
        COUNT(CASE WHEN g.total_magnification >= 192 AND g.total_magnification < 384 AND g.result_id = 2 THEN 1 END) AS multi_192_384_lose,
        COUNT(CASE WHEN g.total_magnification >= 384 AND g.result_id = 1 THEN 1 END) AS multi_384_plus_win,
        COUNT(CASE WHEN g.total_magnification >= 384 AND g.result_id = 2 THEN 1 END) AS multi_384_plus_lose,
        0 AS bomb_0_games,
        0 AS bomb_1_games,
        0 AS bomb_2_games,
        0 AS bomb_3plus_games,
        0 AS games_with_grab,
        0 AS games_player_doubled,
        0 AS spring_count,
        MAX(CASE WHEN g.seq_asc = 1 THEN g.start_money END) AS start_money,
        MAX(CASE WHEN g.seq_desc = 1 THEN g.end_money END) AS end_money,
        MAX(g.end_money) AS money_peak,
        MIN(g.end_money) AS money_valley,
        SUM(g.game_outcome_money) AS total_diff_money,
        SUM(g.room_fee) AS total_fee_paid,
        COUNT(CASE WHEN g.is_escape != 0 THEN 1 END) AS escape_count,
        SUM(g.settle_count) AS total_settle_rounds,
        ROUND(AVG(g.settle_count), 2) AS avg_settle_rounds,
        SUM(g.game_outcome_gdp) AS outcome_gdp,
        MAX(g.settle_count) AS max_settle_round_single
    FROM (
        SELECT *,
            ROW_NUMBER() OVER (PARTITION BY app_id, uid ORDER BY start_datetime ASC) AS seq_asc,
            ROW_NUMBER() OVER (PARTITION BY app_id, uid ORDER BY start_datetime DESC) AS seq_desc
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE game_id = 521
          AND app_id = {app_id}
          AND dt = '{dt}'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    ) g
    LEFT JOIN (
        SELECT app_id, uid, dt,
            MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS win_streak,
            MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS lose_streak
        FROM (
            SELECT app_id, uid, dt, result_id, grp, COUNT(*) AS streak_len
            FROM (
                SELECT app_id, uid, dt, result_id,
                    seq_asc - ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt, result_id ORDER BY seq_asc) AS grp
                FROM (
                    SELECT app_id, uid, dt, result_id,
                        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY start_datetime ASC) AS seq_asc
                    FROM tcy_temp.dws_crazyddz_daily_game
                    WHERE game_id = 521
                      AND app_id = {app_id}
                      AND dt = '{dt}'
                      AND robot != 1
                      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
                      AND result_id IN (1, 2)
                ) r
            ) g
            GROUP BY app_id, uid, dt, result_id, grp
        ) s
        GROUP BY app_id, uid, dt
    ) str ON g.app_id = str.app_id AND g.uid = str.uid AND g.dt = str.dt
    GROUP BY g.app_id, g.uid, g.dt
)
SELECT * FROM ddz_agg
UNION ALL
SELECT * FROM crazyddz_agg"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_app_allgame_stat "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_app_allgame_stat "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_app_allgame_stat",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("dws_ddz_daily_game", "dws_crazyddz_daily_game"),
    )
