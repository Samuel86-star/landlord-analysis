#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_srddz_daily_game via DELETE + INSERT, day by day.

Source: tcy_temp.srddz_daily_game_raw (game_id=105 AND room_id IN (927, 928, 930),
        double row: service fee + settlement; raw already filtered, no need to
        re-filter by room_id here)
Target: tcy_temp.dws_srddz_daily_game (one row per player-game, T-1 available)

Pipeline (raw already guarantees room_id IN 927/928/930):
  1) dedup         ROW_NUMBER() OVER (PARTITION BY resultguid, uid, result_id)
                   -> keep rn=1 (kill cross-day duplicates)
  2) fee-only drop valid_pg keeps only result_id IS NOT NULL (settle rows)
                   -> fee-only rows (no settlement) are excluded
  3) role/robot    take settlement row explicitly (service-fee row is pre-call
                   snapshot, 19.6% role drift)
  4) 927/928/930   CASE WHEN room_id hard-codes basedeposit / fee /
                   room_currency_lower / room_currency_upper from product spec
  5) start_dt/end_dt  shared per resultguid (option a):
                     start = MIN(fee game_datetime) or MAX(end_dt) - MAX(timecost)
                     end   = MAX(settle game_datetime)
  6) is_robot      dim_game_robot OR robot=1 OR app_id=1880105

Usage:
    py -3 -u .\batch_insert_dws_srddz_daily_game.py --start 20260702 --end 20260702
    py -3 -u .\batch_insert_dws_srddz_daily_game.py --start 20260702 --end 20260705 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_srddz_daily_game
WITH
-- (1) dedup: same (resultguid, uid, result_id) -> keep first by game_datetime
dedup_raw AS (
    SELECT *,
        ROW_NUMBER() OVER (
            PARTITION BY resultguid, uid, result_id
            ORDER BY game_datetime
        ) AS rn
    FROM tcy_temp.srddz_daily_game_raw
    WHERE game_id = 105
      AND dt = '{dt}'
),
-- (2) valid player-games: settle rows only (fee-only dropped)
valid_pg AS (
    SELECT dr.*
    FROM dedup_raw dr
    WHERE dr.rn = 1
      AND dr.result_id IS NOT NULL
),
-- (3) per-resultguid shared end_datetime (MAX of settle rows' game_datetime)
rg_end AS (
    SELECT resultguid, MAX(game_datetime) AS rg_end_datetime
    FROM valid_pg
    GROUP BY resultguid
),
-- (4) per-resultguid shared start_datetime (4 players share same value)
--     normal: MIN(service fee game_datetime) across the resultguid
--     all-settle-only: rg_end_datetime - MAX(timecost) across the resultguid
rg_start AS (
    SELECT
        dr.resultguid,
        MIN(CASE WHEN dr.result_id IS NULL THEN dr.game_datetime END) AS fee_min_datetime,
        MAX(CASE WHEN dr.result_id IS NOT NULL THEN dr.timecost END) AS max_settle_timecost
    FROM dedup_raw dr
    WHERE dr.rn = 1
    GROUP BY dr.resultguid
),
rg_start_datetime AS (
    SELECT
        rs.resultguid,
        COALESCE(rs.fee_min_datetime, re.rg_end_datetime - rs.max_settle_timecost) AS rg_start_datetime
    FROM rg_start rs
    JOIN rg_end re ON rs.resultguid = re.resultguid
),
-- (5) per-(resultguid, uid) service fee info (for start_money)
rg_fee AS (
    SELECT resultguid, uid, MAX(olddeposit) AS start_money_raw
    FROM dedup_raw
    WHERE rn = 1 AND result_id IS NULL
    GROUP BY resultguid, uid
)
SELECT
    vpg.game_id,
    vpg.dt,
    vpg.uid,
    vpg.resultguid,
    -- shared (per resultguid)
    rgsd.rg_start_datetime AS start_datetime,
    re.rg_end_datetime     AS end_datetime,
    -- per-player
    MAX(vpg.timecost) AS time_cost,
    MAX(vpg.room_id)  AS room_id,
    -- room_currency_* : hard-code 927/928/930 (raw only has these 3 rooms)
    CASE MAX(vpg.room_id)
        WHEN 927 THEN 1000
        WHEN 928 THEN 12000
        WHEN 930 THEN 50000
        ELSE MAX(vpg.room_currency_lower)
    END AS room_currency_lower,
    CASE MAX(vpg.room_id)
        WHEN 927 THEN 15000
        WHEN 928 THEN 60000
        WHEN 930 THEN 2000000000
        ELSE MAX(vpg.room_currency_upper)
    END AS room_currency_upper,
    -- role/robot: from settle row
    MAX(vpg.robot)         AS robot,
    MAX(vpg.`role`)        AS `role`,
    MAX(vpg.chairno)       AS chairno,
    MAX(vpg.result_id)     AS result_id,
    8 AS play_mode,
    -- basedeposit: hard-code 927/928/930
    CASE MAX(vpg.room_id)
        WHEN 927 THEN 100
        WHEN 928 THEN 250
        WHEN 930 THEN 500
        ELSE MAX(vpg.basedeposit)
    END AS room_base,
    -- room_fee: hard-code 927/928/930 (always overwrite, even for settle-only)
    CASE MAX(vpg.room_id)
        WHEN 927 THEN 150
        WHEN 928 THEN 500
        WHEN 930 THEN 1200
        ELSE MAX(vpg.fee)
    END AS room_fee,
    -- start_money: from service fee olddeposit (NULL if all-settle-only)
    rgfee.start_money_raw AS start_money,
    -- end_money / game_outcome_money: per-player, from settle row
    MAX(vpg.end_deposit) AS end_money,
    MAX(vpg.depositdiff) AS game_outcome_money,
    IFNULL(MAX(vpg.cut), 0) AS is_escape,
    MAX(vpg.magnification)         AS magnification,
    MAX(vpg.magnification_stacked) AS magnification_stacked,
    MAX(vpg.app_id)     AS app_id,
    MAX(vpg.app_code)   AS app_code,
    MAX(vpg.group_id)   AS group_id,
    MAX(vpg.channel_id) AS channel_id,
    MAX(vpg.afk_turn_cnt) AS afk_turn_cnt,
    -- is_robot: dim_game_robot dict > robot=1 > app_id=1880105
    CASE
        WHEN vpg.uid IN (SELECT uid FROM tcy_dim.dim_game_robot) THEN 1
        WHEN MAX(vpg.robot)  = 1      THEN 1
        WHEN MAX(vpg.app_id) = 1880105 THEN 1
        ELSE 0
    END AS is_robot
FROM valid_pg vpg
JOIN rg_end          re   ON vpg.resultguid = re.resultguid
JOIN rg_start_datetime rgsd ON vpg.resultguid = rgsd.resultguid
LEFT JOIN rg_fee     rgfee ON vpg.resultguid = rgfee.resultguid
                          AND vpg.uid       = rgfee.uid
GROUP BY vpg.game_id, vpg.dt, vpg.uid, vpg.resultguid,
         re.rg_end_datetime, rgsd.rg_start_datetime, rgfee.start_money_raw"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_srddz_daily_game "
    "WHERE game_id = 105 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_srddz_daily_game "
    "WHERE game_id = 105 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_srddz_daily_game",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("srddz_daily_game_raw",),
    )