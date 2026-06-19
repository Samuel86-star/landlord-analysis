#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_ddz_daily_game via DELETE + INSERT, day by day.

Source: tcy_temp.ddz_daily_game_raw (game_id=53)
Target: tcy_temp.dws_ddz_daily_game (扩展字段对局表，game_id=53，含 JSON 解析)

Usage:
    py -3 -u .\batch_insert_ddz_daily_game.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_ddz_daily_game.py --start 2026-05-15 --end 2026-05-18 --dry-run
"""
from backfill_runner import run_backfill

# 单天过滤：dt = '{dt}'。INSERT 不限定 app_id（raw 含多 app），DELETE 按 game_id=53 + dt 对齐
INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_ddz_daily_game
SELECT
    game_id, dt, uid, game_datetime, resultguid, timecost,
    room_id, room_currency_lower, room_currency_upper,
    robot, role, chairno, result_id,
    CASE
        WHEN room_id IN (742,420,4484,12074,6314,11168,10336,16445) THEN 1 -- 经典
        WHEN room_id IN (421,22039,22040,22041,22042) THEN 2 -- 不洗牌
        WHEN room_id IN (13176,13177,13178) THEN 3 -- 癞子
        WHEN room_id = 11534 AND group_id IN (6,66,33,44,77,99,8,88,56) THEN 5 -- 比赛（APP/小游戏端）
        WHEN room_id IN (11534,14238,15458) THEN 4 -- 积分
        WHEN room_id IN (158,159) THEN 6 -- 好友房
        ELSE 0
    END AS play_mode,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN basescore ELSE basedeposit END AS room_base,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN score_fee ELSE fee END AS room_fee,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN oldscore ELSE olddeposit END AS start_money,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN end_score ELSE end_deposit END AS end_money,
    CASE
        WHEN room_id IN (11534,14238,15458,158,159) THEN scorediff + score_fee
        ELSE depositdiff + fee
    END AS game_outcome_money,
    cut, safebox_deposit, magnification, magnification_stacked,
    channel_id, group_id, app_id, app_code,
    afk_turn_cnt, magnification_subdivision, extend_content,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.initial_bet'), 1) AS initial_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.grab_landlord_bet'), 3) AS grab_landlord_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.complete_victory_bet'), 1) AS complete_victory_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.bomb_bet'), 1) AS bomb_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.behavior_bet.landlord'), 1) AS landlord_double_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.behavior_bet.farmer1'), 1)
        + IFNULL(get_json_int(magnification_subdivision, '$.behavior_bet.farmer2'), 1) AS total_farmer_double_bet,
    CASE
        WHEN room_id IN (11534,14238,15458,158,159)
        THEN ROUND(ABS(scorediff + score_fee) / NULLIF(basescore, 0), 2)
        ELSE ROUND(ABS(depositdiff + fee) / NULLIF(basedeposit, 0), 2)
    END AS real_magnification,
    IFNULL(get_json_string(extend_content, '$.card_info.hand_cards'), '') AS hand_cards,
    IFNULL(get_json_string(extend_content, '$.card_info.bottom_cards'), '') AS bottom_cards,
    IFNULL(CAST(regexp_extract(extend_content, '(?:shuffle_type|protect_type)[^0-9]*([0-9]+)', 1) AS INT), 0) AS shuffle_type,
    IFNULL(CAST(regexp_extract(extend_content, 'card_id[^0-9]*([0-9]+)', 1) AS INT), 0) AS card_id,
    IFNULL(get_json_int(extend_content, '$.card_power.card_power'), 0) AS card_power,
    IFNULL(get_json_int(extend_content, '$.card_power.card_power_final'), 0) AS card_power_final,
    IFNULL(get_json_int(extend_content, '$.card_power.cost_time'), 0) AS cost_time,
    IFNULL(get_json_string(extend_content, '$.card_power.is_pass'), 'false') AS is_pass,
    IFNULL(get_json_int(extend_content, '$.card_power.shuffle_times'), 0) AS shuffle_times,
    IFNULL(CAST(regexp_extract(extend_content, 'bout[^0-9]*([0-9]+)', 1) AS INT), 0) AS user_attr_bout,
    IFNULL(CAST(regexp_extract(extend_content, 'ai_level[^0-9]*type[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_type,
    IFNULL(CAST(regexp_extract(extend_content, 'callflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_callflag,
    IFNULL(CAST(regexp_extract(extend_content, 'robflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_robflag,
    IFNULL(CAST(regexp_extract(extend_content, 'doubleflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_doubleflag,
    IFNULL(CAST(regexp_extract(extend_content, 'throwtileflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_throwtileflag
FROM tcy_temp.ddz_daily_game_raw
WHERE game_id = 53
  AND dt = '{dt}'"""

DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_ddz_daily_game "
    "WHERE game_id = 53 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_ddz_daily_game "
    "WHERE game_id = 53 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_ddz_daily_game",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("ddz_daily_game_raw",),
    )
