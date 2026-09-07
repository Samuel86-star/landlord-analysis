-- APP 移动端最近 3 个完整注册日、真实首局在 1124 的用户级流失分析底表。
-- 仅用于本地匿名聚合；2026-09-06 尚未形成完整 D1，不进入 D1 分母。
WITH reg_base AS (
    SELECT
        reg.uid,
        reg.app_id,
        reg.reg_date,
        reg.reg_datetime,
        reg.reg_group_id,
        reg.channel_category_name
    FROM tcy_temp.dws_dq_app_daily_reg reg
    WHERE reg.app_id = 1880053
      AND reg.reg_date BETWEEN '2026-09-04' AND '2026-09-06'
      AND reg.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
ranked_0904 AS (
    SELECT
        reg.uid,
        reg.app_id,
        reg.reg_date,
        reg.reg_group_id,
        reg.channel_category_name,
        game.room_id,
        game.role AS player_role,
        game.result_id,
        game.timecost,
        game.card_power,
        game.shuffle_type,
        game.start_money,
        game.end_money,
        ROW_NUMBER() OVER (
            PARTITION BY reg.uid
            ORDER BY game.game_datetime, game.resultguid
        ) AS game_seq,
        COUNT(*) OVER (PARTITION BY reg.uid) AS same_day_games
    FROM reg_base reg
    INNER JOIN tcy_temp.dws_ddz_daily_game game
        ON game.uid = reg.uid
       AND game.app_id = reg.app_id
       AND game.dt = reg.reg_date
    WHERE reg.reg_date = '2026-09-04'
      AND game.dt = '2026-09-04'
      AND game.game_id = 53
      AND game.robot != 1
      AND game.game_datetime >= reg.reg_datetime
),
ranked_0905 AS (
    SELECT
        reg.uid,
        reg.app_id,
        reg.reg_date,
        reg.reg_group_id,
        reg.channel_category_name,
        game.room_id,
        game.role AS player_role,
        game.result_id,
        game.timecost,
        game.card_power,
        game.shuffle_type,
        game.start_money,
        game.end_money,
        ROW_NUMBER() OVER (
            PARTITION BY reg.uid
            ORDER BY game.game_datetime, game.resultguid
        ) AS game_seq,
        COUNT(*) OVER (PARTITION BY reg.uid) AS same_day_games
    FROM reg_base reg
    INNER JOIN tcy_temp.dws_ddz_daily_game game
        ON game.uid = reg.uid
       AND game.app_id = reg.app_id
       AND game.dt = reg.reg_date
    WHERE reg.reg_date = '2026-09-05'
      AND game.dt = '2026-09-05'
      AND game.game_id = 53
      AND game.robot != 1
      AND game.game_datetime >= reg.reg_datetime
),
ranked_0906 AS (
    SELECT
        reg.uid,
        reg.app_id,
        reg.reg_date,
        reg.reg_group_id,
        reg.channel_category_name,
        game.room_id,
        game.role AS player_role,
        game.result_id,
        game.timecost,
        game.card_power,
        game.shuffle_type,
        game.start_money,
        game.end_money,
        ROW_NUMBER() OVER (
            PARTITION BY reg.uid
            ORDER BY game.game_datetime, game.resultguid
        ) AS game_seq,
        COUNT(*) OVER (PARTITION BY reg.uid) AS same_day_games
    FROM reg_base reg
    INNER JOIN tcy_temp.dws_ddz_daily_game game
        ON game.uid = reg.uid
       AND game.app_id = reg.app_id
       AND game.dt = reg.reg_date
    WHERE reg.reg_date = '2026-09-06'
      AND game.dt = '2026-09-06'
      AND game.game_id = 53
      AND game.robot != 1
      AND game.game_datetime >= reg.reg_datetime
),
ranked_same_day_game AS (
    SELECT * FROM ranked_0904
    UNION ALL
    SELECT * FROM ranked_0905
    UNION ALL
    SELECT * FROM ranked_0906
),
cohort AS (
    SELECT
        game.uid,
        game.app_id,
        game.reg_date,
        game.reg_group_id,
        game.channel_category_name,
        game.player_role,
        game.result_id,
        game.timecost,
        game.card_power,
        game.shuffle_type,
        game.start_money,
        game.end_money,
        game.same_day_games
    FROM ranked_same_day_game game
    WHERE game.game_seq = 1
      AND game.room_id = 1124
),
retention_flag AS (
    SELECT
        retention.uid,
        retention.app_id,
        retention.reg_date,
        MAX(retention.d1_login) AS d1_login,
        MAX(retention.d1_game) AS d1_game
    FROM tcy_temp.dws_app_retention_flag retention
    WHERE retention.app_id = 1880053
      AND retention.reg_date BETWEEN '2026-09-04' AND '2026-09-06'
    GROUP BY retention.uid, retention.app_id, retention.reg_date
)
SELECT
    cohort.uid,
    cohort.reg_date,
    cohort.reg_group_id,
    cohort.channel_category_name,
    cohort.player_role,
    cohort.result_id,
    cohort.timecost,
    cohort.card_power,
    cohort.shuffle_type,
    cohort.end_money - cohort.start_money AS net_money,
    cohort.same_day_games,
    CASE WHEN cohort.reg_date <= '2026-09-05' THEN 1 ELSE 0 END AS is_d1_eligible,
    CASE
        WHEN cohort.reg_date <= '2026-09-05' THEN COALESCE(retention.d1_login, 0)
        ELSE 0
    END AS is_d1_login,
    CASE
        WHEN cohort.reg_date <= '2026-09-05' THEN COALESCE(retention.d1_game, 0)
        ELSE 0
    END AS is_d1_game
FROM cohort
LEFT JOIN retention_flag retention
    ON retention.uid = cohort.uid
   AND retention.app_id = cohort.app_id
   AND retention.reg_date = cohort.reg_date
ORDER BY cohort.uid;
