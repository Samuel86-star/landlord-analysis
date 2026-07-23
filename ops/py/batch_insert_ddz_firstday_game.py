#!/usr/bin/env python3
"""Batch backfill tcy_temp.dws_ddz_firstday_game via DELETE + INSERT, day by day.

Source: tcy_temp.dws_ddz_daily_game JOIN tcy_temp.dws_dq_daily_reg (reg_date = dt)
Target: tcy_temp.dws_ddz_firstday_game (注册首日对局切片，字段与 daily_game 一致)

依赖：本表依赖 dws_ddz_daily_reg、dws_ddz_daily_game，对应日期需先回填。

Usage:
    py -3 -u .\batch_insert_ddz_firstday_game.py --start 20260617 --end 20260617
    py -3 -u .\batch_insert_ddz_firstday_game.py --start 2026-02-10 --end 2026-04-22 --dry-run
"""
from backfill_runner import run_backfill

INSERT_TEMPLATE = """INSERT INTO tcy_temp.dws_ddz_firstday_game
SELECT
    g.game_id, g.dt, g.uid, g.game_datetime, g.resultguid, g.timecost, g.room_id,
    g.room_currency_lower, g.room_currency_upper,
    g.robot, g.role, g.chairno, g.result_id,
    g.play_mode, g.room_base, g.room_fee,
    g.start_money, g.end_money, g.game_outcome_money,
    g.cut, g.safebox_deposit, g.magnification, g.magnification_stacked,
    g.channel_id, g.group_id, g.app_id, g.app_code,
    g.afk_turn_cnt, g.magnification_subdivision, g.extend_content,
    g.initial_bet, g.grab_landlord_bet, g.complete_victory_bet, g.bomb_bet,
    g.landlord_double_bet, g.total_farmer_double_bet, g.real_magnification,
    g.hand_cards, g.bottom_cards, g.shuffle_type, g.card_id,
    g.card_power, g.card_power_final, g.cost_time, g.is_pass,
    g.shuffle_times, g.user_attr_bout,
    g.ai_level_type, g.ai_level_callflag, g.ai_level_robflag,
    g.ai_level_doubleflag, g.ai_level_throwtileflag
FROM tcy_temp.dws_ddz_daily_game g
INNER JOIN tcy_temp.dws_dq_daily_reg r
    ON r.uid = g.uid AND r.reg_date = g.dt
WHERE r.app_id = {app_id}
    AND g.game_id = 53
    AND g.dt = '{dt}'"""

# DELETE 用 dt 过滤即可（首日切片本身按 reg_date=dt 写入，单天对齐）
DELETE_TEMPLATE = (
    "DELETE FROM tcy_temp.dws_ddz_firstday_game "
    "WHERE app_id = {app_id} AND game_id = 53 AND dt = '{dt}'"
)

CHECK_TEMPLATE = (
    "SELECT COUNT(*) AS cnt FROM tcy_temp.dws_ddz_firstday_game "
    "WHERE app_id = {app_id} AND game_id = 53 AND dt = '{dt}'"
)


if __name__ == "__main__":
    run_backfill(
        table_label="dws_ddz_firstday_game",
        delete_template=DELETE_TEMPLATE,
        insert_template=INSERT_TEMPLATE,
        check_template=CHECK_TEMPLATE,
        depends_on=("dws_ddz_daily_game", "dws_dq_daily_reg"),
    )
