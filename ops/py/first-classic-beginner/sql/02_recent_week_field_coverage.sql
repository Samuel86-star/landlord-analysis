-- 最近完整 7 天手牌、牌力与重洗次数覆盖率校验。
-- 口径：真人、play_mode 1~6，仅查询 tcy_temp DWS，不修改数据。
SELECT
    game.dt,
    COUNT(*) AS player_rows,
    COUNT(DISTINCT game.uid) AS users,
    COUNT(DISTINCT game.resultguid) AS games,
    SUM(CASE WHEN NULLIF(TRIM(game.hand_cards), '') IS NOT NULL THEN 1 ELSE 0 END) AS hand_cards_rows,
    ROUND(
        SUM(CASE WHEN NULLIF(TRIM(game.hand_cards), '') IS NOT NULL THEN 1 ELSE 0 END) * 100.0
        / NULLIF(COUNT(*), 0),
        2
    ) AS hand_cards_pct,
    SUM(CASE WHEN RIGHT(TRIM(game.hand_cards), 1) = ',' THEN 1 ELSE 0 END) AS trailing_comma_rows,
    SUM(CASE WHEN get_json_string(game.extend_content, '$.card_info.hand_cards') IS NOT NULL THEN 1 ELSE 0 END) AS source_hand_cards_rows,
    SUM(CASE
        WHEN get_json_string(game.extend_content, '$.card_info.hand_cards') IS NOT NULL
         AND game.hand_cards != get_json_string(game.extend_content, '$.card_info.hand_cards')
        THEN 1 ELSE 0
    END) AS hand_cards_mismatch_rows,
    SUM(CASE WHEN get_json_int(game.extend_content, '$.card_power.card_power') IS NOT NULL THEN 1 ELSE 0 END) AS source_card_power_rows,
    SUM(CASE WHEN game.card_power IS NULL THEN 1 ELSE 0 END) AS card_power_null_rows,
    SUM(CASE
        WHEN get_json_int(game.extend_content, '$.card_power.card_power') IS NOT NULL
         AND game.card_power != get_json_int(game.extend_content, '$.card_power.card_power')
        THEN 1 ELSE 0
    END) AS card_power_mismatch_rows,
    SUM(CASE WHEN get_json_int(game.extend_content, '$.card_power.shuffle_times') IS NOT NULL THEN 1 ELSE 0 END) AS source_shuffle_times_rows,
    SUM(CASE WHEN game.shuffle_times IS NULL THEN 1 ELSE 0 END) AS shuffle_times_null_rows,
    SUM(CASE WHEN game.shuffle_times < -1 THEN 1 ELSE 0 END) AS shuffle_times_invalid_rows,
    SUM(CASE
        WHEN get_json_int(game.extend_content, '$.card_power.shuffle_times') IS NOT NULL
         AND game.shuffle_times != get_json_int(game.extend_content, '$.card_power.shuffle_times')
        THEN 1 ELSE 0
    END) AS shuffle_times_mismatch_rows
FROM tcy_temp.dws_ddz_daily_game game
WHERE game.dt BETWEEN '2026-08-31' AND '2026-09-06'
  AND game.robot != 1
  AND game.play_mode BETWEEN 1 AND 6
GROUP BY game.dt
ORDER BY game.dt;
