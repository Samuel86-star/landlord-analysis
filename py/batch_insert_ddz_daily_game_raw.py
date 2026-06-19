#!/usr/bin/env python3
"""Batch backfill tcy_temp.ddz_daily_game_raw via DELETE + INSERT, day by day.

Source: hive_catalog_cdh5.dwd.fact_game_combatgains (Hive, game_id=53, dt 为 int YYYYMMDD 分区)
Target: tcy_temp.ddz_daily_game_raw (ODS 迁移表，保持原始字段)

> 从 Hive 搬运 game_id=53 对局日志到 StarRocks，不做字段转换（下游 dws_ddz_daily_game 再统一）。

Usage:
    py -3 -u .\batch_insert_ddz_daily_game_raw.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_ddz_daily_game_raw.py --start 2026-05-15 --end 2026-05-18 --dry-run
"""
from backfill_runner import run_backfill

# 源 Hive 表 dt 为 int 分区，用 {dt_int}；目标表 dt 为 DATE，DELETE/CHECK 用 '{dt}'
INSERT_TEMPLATE = """INSERT INTO tcy_temp.ddz_daily_game_raw
SELECT
    game_id, dt, uid, FROM_UNIXTIME(time_unix / 1000) AS game_datetime, resultguid, timecost,
    room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
    basedeposit, olddeposit, end_deposit, fee, depositdiff,
    basescore, oldscore, end_score, score_fee, scorediff,
    cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, IFNULL(app_id, 1880053), app_code,
    afk_turn_cnt, magnification_subdivision, extend_content
FROM hive_catalog_cdh5.dwd.fact_game_combatgains
WHERE game_id = 53
  AND dt = {dt_int}"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.ddz_daily_game_raw "
    "WHERE game_id = 53 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.ddz_daily_game_raw "
    "WHERE game_id = 53 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="ddz_daily_game_raw",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("hive_catalog_cdh5.dwd.fact_game_combatgains",),
    )
