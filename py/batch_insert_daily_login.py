#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_dq_daily_login via DELETE + INSERT, day by day.

Source: tcy_dwd.dwd_tcy_userlogin_si (dt 为 datetime，分钟级粒度)
Target: tcy_temp.dws_dq_daily_login (login_date = DATE，按天聚合)

Usage:
    py -3 -u .\\batch_insert_daily_login.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_daily_login.py --start 2026-02-10 --end 2026-04-20 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_dq_daily_login
SELECT
    app_id,
    DATE(dt) AS login_date,
    uid,
    MIN(dt) AS first_login_time,
    MIN_BY(app_code, time_unix)   AS first_app_code,
    MIN_BY(channel_id, time_unix) AS first_channel_id,
    MIN_BY(group_id, time_unix)   AS first_group_id,
    MAX(dt) AS last_login_time,
    MAX_BY(app_code, time_unix)   AS last_app_code,
    MAX_BY(channel_id, time_unix) AS last_channel_id,
    MAX_BY(group_id, time_unix)   AS last_group_id,
    MAX_BY(channel_id, cnt_channel)  AS most_freq_channel_id,
    MAX_BY(group_id, cnt_group)      AS most_freq_group_id,
    MAX_BY(app_code, cnt_app_code)   AS most_freq_app_code,
    COUNT(DISTINCT channel_id) AS channel_id_count,
    COUNT(DISTINCT group_id)   AS group_id_count,
    COUNT(DISTINCT app_code)   AS app_code_count,
    COUNT(1) AS login_count
FROM (
    SELECT
        app_id, dt, uid, time_unix, channel_id, group_id, app_code,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), channel_id) AS cnt_channel,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), group_id)   AS cnt_group,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), app_code)   AS cnt_app_code
    FROM tcy_dwd.dwd_tcy_userlogin_si
    WHERE app_id = {app_id}
      AND dt >= '{dt} 00:00:00'
      AND dt <= '{dt} 23:59:59'
) t
GROUP BY app_id, DATE(dt), uid"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_dq_daily_login "
    "WHERE app_id = {app_id} AND login_date = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_dq_daily_login "
    "WHERE app_id = {app_id} AND login_date = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_dq_daily_login",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("tcy_dwd.dwd_tcy_userlogin_si",),
    )
