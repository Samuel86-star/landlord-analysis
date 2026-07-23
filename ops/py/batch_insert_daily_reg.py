#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_dq_daily_reg via DELETE + INSERT, day by day.

Source: hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st (dt = int YYYYMMDD)
Target: tcy_temp.dws_dq_daily_reg (reg_date = DATE)

Usage:
    py -3 -u .\\batch_insert_daily_reg.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_daily_reg.py --start 2026-02-10 --end 2026-04-20 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_dq_daily_reg
SELECT
    app_id,
    uid,
    str_to_date(CAST(dt AS STRING), '%Y%m%d') AS reg_date,
    FROM_UNIXTIME(first_login_ts / 1000)      AS reg_datetime
FROM hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st
WHERE app_id = {app_id}
  AND dt = {dt_int}"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_dq_daily_reg "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_dq_daily_reg "
    "WHERE app_id = {app_id} AND reg_date = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_dq_daily_reg",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st",),
    )
