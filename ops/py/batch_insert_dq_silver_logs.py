#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_dq_silver_logs via DELETE + INSERT, day by day.

Source: tcy_dwd.dwd_silver_si  (dt = int YYYYMMDD)
        LEFT JOIN tcy_temp.dq_channel_category_map
        LEFT JOIN tcy_temp.dq_currency_op_config
        LEFT JOIN tcy_temp.dq_currency_guid_config
Target: tcy_temp.dws_dq_silver_logs  (dt = DATE)

依赖：本表依赖 dq_channel_category_map 等 3 张维表（按需更新）。

Usage:
    py -3 -u .\\batch_insert_dq_silver_logs.py --start 20260617 --end 20260617
    py -3 -u .\\batch_insert_dq_silver_logs.py --start 2026-04-29 --end 2026-04-29 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_dq_silver_logs
SELECT
    s.app_id,
    STR_TO_DATE(CAST(s.dt AS VARCHAR), '%Y%m%d') AS dt,
    s.game_id,
    s.uid,
    COALESCE(s.game_code, s.app_code) AS app_code,
    COALESCE(s.game_vers, s.app_vers) AS app_vers,
    s.op_date AS date_time,
    s.op_id,
    s.op_name,
    s.op_type_id,
    s.op_type_name,
    s.fin_flow_scn_id,
    COALESCE(op.settlement_type, -1) AS settlement_type,
    s.silver_diff,
    s.silver_deposit,
    s.silver_amount,
    s.silver_balance,
    s.silver_initial,
    s.group_id,
    s.channel_id,
    COALESCE(chn.channel_category_name, '其他') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1) AS channel_category_tag_id,
    s.source_guid,
    COALESCE(gc.guid_title, '') AS guid_title,
    COALESCE(gc.guid_type, CASE WHEN s.op_id = 300104 THEN 0 ELSE -1 END) AS guid_type
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.dq_channel_category_map chn
    ON s.channel_id = chn.channel_id
LEFT JOIN tcy_temp.dq_currency_op_config op
    ON s.app_id = op.app_id AND s.op_id = op.op_id
LEFT JOIN tcy_temp.dq_currency_guid_config gc
    ON s.app_id = gc.app_id AND s.source_guid = gc.guid
WHERE s.app_id = {app_id}
  AND s.game_id = 53
  AND s.dt = {dt_int}"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_dq_silver_logs "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_dq_silver_logs "
    "WHERE app_id = {app_id} AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_dq_silver_logs",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("tcy_dwd.dwd_silver_si", "dq_channel_category_map"),
    )
