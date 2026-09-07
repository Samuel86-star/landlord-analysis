-- 注册用户真实前 3 局斗地主明细。
-- 首局定义：注册时间之后，dws_ddz_daily_game 中最早的真人 53 游戏对局；
-- 不按 play_mode、room_id 或 user_attr_bout 预筛。
WITH reg_base AS (
    SELECT
        reg.uid,
        reg.app_id,
        reg.reg_date,
        reg.reg_datetime,
        reg.channel_category_name
    FROM tcy_temp.dws_dq_app_daily_reg reg
    WHERE reg.app_id = 1880053
      AND reg.reg_date BETWEEN '2026-08-31' AND '2026-09-06'
),
first_game AS (
    SELECT
        game.uid,
        MIN_BY(game.room_id, game.game_datetime) AS first_room_id,
        MIN(game.game_datetime) AS first_game_time
    FROM tcy_temp.dws_ddz_daily_game game
    INNER JOIN reg_base reg
        ON reg.uid = game.uid
       AND reg.app_id = game.app_id
    WHERE game.dt BETWEEN '2026-08-31' AND '2026-09-06'
      AND game.game_id = 53
      AND game.robot != 1
      AND game.game_datetime >= reg.reg_datetime
    GROUP BY game.uid
),
cohort AS (
    SELECT
        first.uid,
        first.first_room_id,
        first.first_game_time,
        reg.reg_date,
        reg.reg_datetime,
        reg.channel_category_name
    FROM first_game first
    INNER JOIN reg_base reg ON reg.uid = first.uid
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
        game.hand_cards
    FROM tcy_temp.dws_ddz_daily_game game
    INNER JOIN cohort
        ON cohort.uid = game.uid
       AND game.game_datetime >= cohort.first_game_time
    WHERE game.dt BETWEEN '2026-08-31' AND '2026-09-06'
      AND game.game_id = 53
      AND game.robot != 1
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
    hand_cards
FROM ranked
WHERE game_seq <= 3
ORDER BY uid, game_seq;
