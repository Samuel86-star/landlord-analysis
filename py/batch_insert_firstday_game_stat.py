#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_app_firstday_game_stat via DELETE + INSERT, day by day.

Source: tcy_temp.dws_dq_app_daily_reg r (主表，注册用户)
        LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        LEFT JOIN tcy_temp.dws_app_scoregame_stat sc
        LEFT JOIN tcy_temp.dws_app_daily_allgame_stat ag
Target: tcy_temp.dws_app_firstday_game_stat (注册首日游戏指标，无 retention flag)

> 表设计：注册当日的首日指标，写入后不变（一次写入永久不动）。
> 留存 flag 已拆出独立表 dws_app_retention_flag，分析时 LEFT JOIN 即可。
> 字段名带 silver_/score_/allgame_ 前缀，明确数据来源。

依赖：本表依赖 dws_dq_app_daily_reg、dws_app_silvergame_stat、dws_app_scoregame_stat、
      dws_app_daily_allgame_stat（reg_date 当天数据）。

Usage:
    py -3 -u .\\batch_insert_firstday_game_stat.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_firstday_game_stat.py --start 2026-05-14 --end 2026-05-14 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_app_firstday_game_stat
SELECT
    r.app_id, r.reg_date, r.uid,
    r.reg_channel_id, r.reg_group_id, r.reg_app_code,
    r.channel_category_id, r.channel_category_name, r.channel_category_tag_id,
    r.first_day_login_cnt,
    si.game_count          AS silver_game_count,
    si.win_rate            AS silver_win_rate,
    si.max_lose_streak     AS silver_max_lose_streak,
    si.max_win_streak      AS silver_max_win_streak,
    si.total_diff_money    AS silver_total_diff_money,
    si.money_valley        AS silver_money_valley,
    si.money_peak          AS silver_money_peak,
    si.start_money         AS silver_start_money,
    si.end_money           AS silver_end_money,
    si.total_fee_paid      AS silver_total_fee_paid,
    si.escape_count        AS silver_escape_count,
    si.distinct_rooms      AS silver_distinct_rooms,
    si.total_play_seconds  AS silver_total_play_seconds,
    sc.game_count          AS score_game_count,
    sc.win_rate            AS score_win_rate,
    sc.max_lose_streak     AS score_max_lose_streak,
    sc.max_win_streak      AS score_max_win_streak,
    sc.escape_count        AS score_escape_count,
    sc.total_play_seconds  AS score_total_play_seconds,
    ag.distinct_modes      AS allgame_distinct_modes,
    ag.total_games         AS allgame_total_games,
    ag.avg_magnification   AS allgame_avg_magnification,
    ag.max_magnification   AS allgame_max_magnification,
    ag.bomb_count          AS allgame_bomb_count,
    ag.spring_count        AS allgame_spring_count,
    ag.multi_1_win, ag.multi_1_lose,
    ag.multi_2_win, ag.multi_2_lose,
    ag.multi_3_6_win, ag.multi_3_6_lose,
    ag.multi_6_12_win, ag.multi_6_12_lose,
    ag.multi_12_24_win, ag.multi_12_24_lose,
    ag.multi_24_48_win, ag.multi_24_48_lose,
    ag.multi_48_96_win, ag.multi_48_96_lose,
    ag.multi_96_192_win, ag.multi_96_192_lose,
    ag.multi_192_384_win, ag.multi_192_384_lose,
    ag.multi_384_plus_win, ag.multi_384_plus_lose
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_silvergame_stat si
    ON si.app_id = r.app_id AND si.uid = r.uid AND si.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_scoregame_stat sc
    ON sc.app_id = r.app_id AND sc.uid = r.uid AND sc.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_daily_allgame_stat ag
    ON ag.app_id = r.app_id AND ag.uid = r.uid AND ag.dt = r.reg_date
WHERE r.app_id = {app_id} AND r.reg_date = '{dt}'"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_app_firstday_game_stat "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_app_firstday_game_stat "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_app_firstday_game_stat",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=(
            "dws_dq_app_daily_reg",
            "dws_app_silvergame_stat",
            "dws_app_scoregame_stat",
            "dws_app_daily_allgame_stat",
        ),
    )
