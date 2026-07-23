#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_app_retention_flag via DELETE + INSERT, day by day.

Source: tcy_temp.dws_dq_app_daily_reg r (主表，注册用户)
        LEFT JOIN tcy_temp.dws_app_game_active a   (游戏留存)
        LEFT JOIN tcy_temp.dws_dq_daily_login l     (登录留存)
Target: tcy_temp.dws_app_retention_flag (reg_date 维度)

> flag 三态语义：NULL=未到期, 0=到期未留存, 1=到期已留存。
> reg_date+N 天 <= 当天(CURRENT_DATE) 才算到期，否则该 flag 为 NULL。
> retention_flag 依赖 reg_date+N 天的 game_active/login（DWS 按天分区表，不会过期），
> 重算天然正确，适合 daily_retention 每天回扫 35 天。

Usage:
    py -3 -u .\\batch_insert_retention_flag.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_retention_flag.py --start 2026-05-14 --end 2026-05-14 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_app_retention_flag
SELECT
    r.app_id, r.reg_date, r.uid,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 1 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt  = DATE_ADD('{dt}', INTERVAL 1 DAY)  THEN 1 END), 0) ELSE NULL END AS d1_game,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 3 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt  = DATE_ADD('{dt}', INTERVAL 3 DAY)  THEN 1 END), 0) ELSE NULL END AS d3_game,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 7 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt  = DATE_ADD('{dt}', INTERVAL 7 DAY)  THEN 1 END), 0) ELSE NULL END AS d7_game,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 14 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt  = DATE_ADD('{dt}', INTERVAL 14 DAY) THEN 1 END), 0) ELSE NULL END AS d14_game,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 30 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt  = DATE_ADD('{dt}', INTERVAL 30 DAY) THEN 1 END), 0) ELSE NULL END AS d30_game,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 1 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('{dt}', INTERVAL 1 DAY)  THEN 1 END), 0) ELSE NULL END AS d1_login,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 3 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('{dt}', INTERVAL 3 DAY)  THEN 1 END), 0) ELSE NULL END AS d3_login,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 7 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('{dt}', INTERVAL 7 DAY)  THEN 1 END), 0) ELSE NULL END AS d7_login,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 14 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('{dt}', INTERVAL 14 DAY) THEN 1 END), 0) ELSE NULL END AS d14_login,
    CASE WHEN DATE_ADD('{dt}', INTERVAL 30 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('{dt}', INTERVAL 30 DAY) THEN 1 END), 0) ELSE NULL END AS d30_login
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.app_id = r.app_id AND a.uid = r.uid
   AND a.dt IN (
       DATE_ADD('{dt}', INTERVAL 1 DAY),
       DATE_ADD('{dt}', INTERVAL 3 DAY),
       DATE_ADD('{dt}', INTERVAL 7 DAY),
       DATE_ADD('{dt}', INTERVAL 14 DAY),
       DATE_ADD('{dt}', INTERVAL 30 DAY)
   )
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
   AND l.login_date IN (
       DATE_ADD('{dt}', INTERVAL 1 DAY),
       DATE_ADD('{dt}', INTERVAL 3 DAY),
       DATE_ADD('{dt}', INTERVAL 7 DAY),
       DATE_ADD('{dt}', INTERVAL 14 DAY),
       DATE_ADD('{dt}', INTERVAL 30 DAY)
   )
WHERE r.app_id = {app_id} AND r.reg_date = '{dt}'
GROUP BY r.app_id, r.reg_date, r.uid"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_app_retention_flag "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_app_retention_flag "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_app_retention_flag",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("dws_dq_app_daily_reg", "dws_app_game_active", "dws_dq_daily_login"),
    )
