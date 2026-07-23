#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_crazyddz_daily_game via DELETE + INSERT, day by day.

Source: tcy_temp.crazyddz_daily_game_raw (game_id=521，多轮结算日志)
Target: tcy_temp.dws_crazyddz_daily_game (整局一行，T-1 可用)

> 上游 raw 已通过 min_dt 覆盖 dt，单天 dt = '{dt}' 即可正确收集跨天对局。
> INSERT 不限定 app_id（含 1880053 与 1880521 共服），DELETE 按 game_id=521 + dt 对齐。

Usage:
    py -3 -u .\batch_insert_crazyddz_daily_game.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_crazyddz_daily_game.py --start 2026-04-01 --end 2026-04-27 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_crazyddz_daily_game
WITH target_resultguids AS (
    SELECT DISTINCT resultguid
    FROM tcy_temp.crazyddz_daily_game_raw
    WHERE game_id = 521
      AND dt = '{dt}'
      AND fee != 0
),
ranked_combat AS (
    SELECT
        raw.*,
        SUM(CASE WHEN raw.fee = 0 AND raw.cut = 0 AND raw.result_id IS NULL THEN raw.depositdiff ELSE 0 END) OVER(PARTITION BY raw.resultguid, raw.uid) AS game_win_loss,
        ROW_NUMBER() OVER(PARTITION BY raw.resultguid, raw.uid ORDER BY raw.result_id, raw.game_datetime) as row_start,
        ROW_NUMBER() OVER(PARTITION BY raw.resultguid, raw.uid ORDER BY raw.result_id desc, raw.game_datetime desc) as row_end
    FROM tcy_temp.crazyddz_daily_game_raw raw
    INNER JOIN target_resultguids tr ON raw.resultguid = tr.resultguid
    WHERE raw.game_id = 521
       AND raw.dt = '{dt}'
)
SELECT
    game_id,
    MAX(CASE WHEN row_start = 1 THEN dt END) AS dt,
    uid,
    resultguid,
    MAX(CASE WHEN row_start = 1 THEN game_datetime END) AS start_datetime,
    MAX(CASE WHEN row_end = 1 THEN game_datetime END) AS end_datetime,
    SUM(timecost) AS time_cost,
    MAX(CASE WHEN row_start = 1 THEN room_id END) AS room_id,
    MAX(CASE WHEN row_start = 1 THEN room_currency_lower END) AS room_currency_lower,
    MAX(CASE WHEN row_start = 1 THEN room_currency_upper END) AS room_currency_upper,
    MAX(CASE WHEN row_start = 1 THEN robot END) AS robot,
    MAX(CASE WHEN row_start = 1 THEN `role` END) AS role,
    MAX(CASE WHEN row_start = 1 THEN chairno END) AS chairno,
    COALESCE(
        MAX(result_id),
        CASE WHEN MAX(game_win_loss) > 0 THEN 1
             WHEN MAX(game_win_loss) < 0 THEN 2
             ELSE 3
        END
    ) AS result_id,
    7 AS play_mode,
    MAX(basedeposit) AS room_base,
    MAX(fee) AS room_fee,
    MAX(CASE WHEN row_start = 1 THEN olddeposit END) AS start_money,
    MAX(CASE WHEN row_start = 1 THEN olddeposit END) + SUM(depositdiff) AS end_money,
    IFNULL(SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN depositdiff END), 0) AS game_outcome_money,
    IFNULL(SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN ABS(depositdiff) END), 0) AS game_outcome_gdp,
    IFNULL(MAX(CASE WHEN cut != 0 THEN cut END), 0) AS is_escape,
    SUM(ABS(magnification)) AS total_magnification,
    MAX(CASE WHEN row_start = 1 THEN app_id END) AS app_id,
    COALESCE(MAX(CASE WHEN row_start = 1 THEN app_code END), CASE WHEN MAX(CASE WHEN row_start = 1 THEN app_id END) = 1880521 THEN 'gfso' ELSE 'zgda' END) AS app_code,
    MAX(CASE WHEN row_start = 1 THEN group_id END) AS group_id,
    MAX(CASE WHEN row_start = 1 THEN channel_id END) AS channel_id,
    MAX(CASE WHEN row_start = 1 THEN afk_turn_cnt END) AS afk_turn_cnt,
    COUNT(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN 1 END) AS settle_count,
    GROUP_CONCAT(CASE WHEN fee = 0 AND result_id IS NULL THEN CAST(depositdiff AS STRING) END ORDER BY game_datetime ASC SEPARATOR '#') AS deposit_diff_path,
    GROUP_CONCAT(CASE WHEN fee = 0 AND result_id IS NULL THEN CAST(magnification AS STRING) END ORDER BY game_datetime ASC SEPARATOR '#') AS deposit_magnification_path
FROM ranked_combat
GROUP BY game_id, uid, resultguid"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_crazyddz_daily_game "
    "WHERE game_id = 521 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_crazyddz_daily_game "
    "WHERE game_id = 521 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_crazyddz_daily_game",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("crazyddz_daily_game_raw",),
    )
