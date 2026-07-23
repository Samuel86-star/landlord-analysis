-- 01_cohort_first3_detail.sql
-- 首次经典初级房玩家前 3 局明细导出（框架 2.1/2.2/3.1/3.2）
-- 口径：注册窗口 2026-06-18~24；首次经典对局(min_by game_datetime)房间在 4484/12074 且发生在 reg_date；
--      robot!=1, play_mode 1-6；窗口内按 game_datetime,resultguid 编序取前 3 局。
-- 线性 CTE：reg_base -> classic_first -> cohort -> ranked -> SELECT（每个 CTE 只被下游引用一次，
--   避免 StarRocks CTE 内联导致重复扫描大表 -> CloudBeaver 静默失败）。
-- 说明：hand_cards/bottom_cards 在数仓 extend_content 全历史 0 覆盖（已查证），故不取；
--      bomb_cnt/bomb_final（持有炸弹数，表文档未记录）从 card_power 节点现取；
--      role 是 StarRocks 关键字，返回列名带反引号，改别名 player_role 规避。
WITH reg_base AS (
    SELECT
        reg.uid,
        reg.reg_date,
        reg.channel_category_name
    FROM tcy_temp.dws_dq_app_daily_reg reg
    WHERE reg.app_id = 1880053
      AND reg.reg_date BETWEEN '2026-06-18' AND '2026-06-24'
),
classic_first AS (
    SELECT
        game.uid,
        MIN_BY(game.room_id, game.game_datetime) AS first_room_id,
        MIN(game.game_datetime) AS first_game_time
    FROM tcy_temp.dws_ddz_daily_game game
    INNER JOIN reg_base reg ON reg.uid = game.uid
    WHERE game.dt BETWEEN '2026-06-18' AND '2026-06-24'
      AND game.robot != 1
      AND game.play_mode BETWEEN 1 AND 6
    GROUP BY game.uid
),
cohort AS (
    SELECT
        first.uid,
        first.first_room_id,
        reg.reg_date,
        reg.channel_category_name
    FROM classic_first first
    INNER JOIN reg_base reg ON reg.uid = first.uid
    WHERE first.first_room_id IN (4484, 12074)
      AND DATE(first.first_game_time) = reg.reg_date
),
ranked AS (
    SELECT
        cohort.uid,
        cohort.reg_date,
        cohort.first_room_id,
        cohort.channel_category_name,
        ROW_NUMBER() OVER (
            PARTITION BY game.uid
            ORDER BY game.game_datetime, game.resultguid
        ) AS game_seq,
        game.dt,
        game.game_datetime,
        game.resultguid,
        game.room_id,
        game.play_mode,
        game.role AS player_role,
        game.result_id,
        game.timecost,
        game.room_base,
        game.room_fee,
        game.start_money,
        game.end_money,
        game.game_outcome_money,
        game.magnification,
        game.real_magnification,
        game.shuffle_type,
        game.card_id,
        game.card_power,
        game.card_power_final,
        game.cost_time,
        game.is_pass,
        game.shuffle_times,
        game.user_attr_bout,
        IFNULL(get_json_int(game.extend_content, '$.card_power.bomb_cnt'), 0) AS bomb_cnt,
        IFNULL(get_json_int(game.extend_content, '$.card_power.bomb_final'), 0) AS bomb_final
    FROM tcy_temp.dws_ddz_daily_game game
    INNER JOIN cohort ON cohort.uid = game.uid
    WHERE game.dt BETWEEN '2026-06-18' AND '2026-06-24'
      AND game.robot != 1
      AND game.play_mode BETWEEN 1 AND 6
)
SELECT
    uid,
    reg_date,
    first_room_id,
    channel_category_name,
    game_seq,
    dt,
    game_datetime,
    resultguid,
    room_id,
    play_mode,
    player_role,
    result_id,
    timecost,
    room_base,
    room_fee,
    start_money,
    end_money,
    game_outcome_money,
    magnification,
    real_magnification,
    shuffle_type,
    card_id,
    card_power,
    card_power_final,
    cost_time,
    is_pass,
    shuffle_times,
    user_attr_bout,
    bomb_cnt,
    bomb_final
FROM ranked
WHERE game_seq <= 3
ORDER BY uid, game_seq;
