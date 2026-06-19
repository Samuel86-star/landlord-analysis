#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_dq_app_daily_reg via DELETE + INSERT, day by day.

Source: tcy_temp.dws_dq_daily_reg r
        INNER JOIN tcy_temp.dws_dq_daily_login l  (注册当天登录聚合)
        LEFT JOIN  tcy_temp.dq_channel_category_map chn
Target: tcy_temp.dws_dq_app_daily_reg (APP 端注册宽表)

依赖：本表依赖 dws_dq_daily_reg 与 dws_dq_daily_login，执行前请确保两者对应日期已回填。

Usage:
    py -3 -u .\\batch_insert_app_daily_reg.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_app_daily_reg.py --start 2026-02-10 --end 2026-04-20 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_dq_app_daily_reg
SELECT
    r.app_id,
    r.reg_date,
    l.first_channel_id AS reg_channel_id,
    r.uid,
    r.reg_datetime,
    l.first_group_id   AS reg_group_id,
    l.first_app_code   AS reg_app_code,
    chn.channel_category_id,
    chn.channel_category_name,
    chn.channel_category_tag_id,
    0 AS is_login_log_missing,
    l.login_count AS first_day_login_cnt
FROM tcy_temp.dws_dq_daily_reg r
INNER JOIN tcy_temp.dws_dq_daily_login l
    ON r.app_id = l.app_id
    AND r.reg_date = l.login_date
    AND r.uid = l.uid
    AND l.first_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
LEFT JOIN tcy_temp.dq_channel_category_map chn
    ON l.first_channel_id = chn.channel_id
WHERE r.app_id = {app_id}
  AND r.reg_date = '{dt}'"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_dq_app_daily_reg "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_dq_app_daily_reg "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_dq_app_daily_reg",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("dws_dq_daily_reg", "dws_dq_daily_login"),
    )
