#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_prop_log via DELETE + INSERT, day by day.

Source: hive_catalog_cdh5.dwd.fact_gtpl_prop_detail  (dt = int YYYYMMDD)
Target: tcy_temp.dws_prop_log  (dt = DATE)

> 过滤斗地主游戏 (app_id=1880053, game_id=53)，mod_name 用正则从 mod_detail 提取。

Usage:
    py -3 -u .\\batch_insert_prop_log.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_prop_log.py --start 2026-05-13 --end 2026-05-13 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_prop_log
SELECT
    p.app_id,
    STR_TO_DATE(CAST(p.dt AS VARCHAR), '%Y%m%d') AS dt,
    p.game_id,
    p.uid,
    p.game_code,
    p.game_vers,
    p.app_code,
    p.app_vers,
    FROM_UNIXTIME(p.time_unix / 1000) AS log_datetime,
    p.prop_id,
    p.prop_name,
    p.prop_cnt,
    p.op_type,
    p.remain,
    FROM_UNIXTIME(p.deadline_ts) AS deadline_datetime,
    CASE
        WHEN regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1) REGEXP '\\((platform|buy)\\)'
            THEN regexp_replace(regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1), '\\((platform|buy)\\)', '')
        WHEN regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1) REGEXP '\\(.+\\)'
            THEN regexp_extract(regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1), '\\(([^()]+)\\)', 1)
        ELSE regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1)
    END AS mod_name,
    p.mod_detail,
    p.source,
    p.ip,
    p.group_id,
    p.channel_id
FROM hive_catalog_cdh5.dwd.fact_gtpl_prop_detail p
WHERE p.app_id = {app_id}
  AND p.game_id = 53
  AND p.dt = {dt_int}"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_prop_log "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_prop_log "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_prop_log",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("hive_catalog_cdh5.dwd.fact_gtpl_prop_detail",),
    )
