-- 校验指定日期各 DWS 表的行数，快速定位回填是否齐全。
-- 用法：py -3 -u .\sr_exec.py -f check_data.sql
-- 改日期：全局替换 2026-06-17（共 18 处 dt / reg_date / login_date）
SELECT 'ddz_daily_game_raw' AS table_name, COUNT(*) AS cnt FROM tcy_temp.ddz_daily_game_raw WHERE game_id = 53 AND dt = '2026-06-17'
UNION ALL
SELECT 'crazyddz_daily_game_raw', COUNT(*) FROM tcy_temp.crazyddz_daily_game_raw WHERE game_id = 521 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_dq_daily_reg', COUNT(*) FROM tcy_temp.dws_dq_daily_reg WHERE app_id = 1880053 AND reg_date = '2026-06-17'
UNION ALL
SELECT 'dws_dq_daily_login', COUNT(*) FROM tcy_temp.dws_dq_daily_login WHERE app_id = 1880053 AND login_date = '2026-06-17'
UNION ALL
SELECT 'dws_dq_app_daily_reg', COUNT(*) FROM tcy_temp.dws_dq_app_daily_reg WHERE app_id = 1880053 AND reg_date = '2026-06-17'
UNION ALL
SELECT 'dws_ddz_daily_game', COUNT(*) FROM tcy_temp.dws_ddz_daily_game WHERE game_id = 53 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_crazyddz_daily_game', COUNT(*) FROM tcy_temp.dws_crazyddz_daily_game WHERE game_id = 521 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_app_game_active', COUNT(*) FROM tcy_temp.dws_app_game_active WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_app_gamemode_active', COUNT(*) FROM tcy_temp.dws_app_gamemode_active WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_app_silvergame_stat', COUNT(*) FROM tcy_temp.dws_app_silvergame_stat WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_app_scoregame_stat', COUNT(*) FROM tcy_temp.dws_app_scoregame_stat WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_app_allgame_stat', COUNT(*) FROM tcy_temp.dws_app_allgame_stat WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_dq_silver_logs', COUNT(*) FROM tcy_temp.dws_dq_silver_logs WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_prop_log', COUNT(*) FROM tcy_temp.dws_prop_log WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_ddz_firstday_game', COUNT(*) FROM tcy_temp.dws_ddz_firstday_game WHERE app_id = 1880053 AND game_id = 53 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_app_daily_allgame_stat', COUNT(*) FROM tcy_temp.dws_app_daily_allgame_stat WHERE app_id = 1880053 AND dt = '2026-06-17'
UNION ALL
SELECT 'dws_app_retention_flag', COUNT(*) FROM tcy_temp.dws_app_retention_flag WHERE app_id = 1880053 AND reg_date = '2026-06-17'
UNION ALL
SELECT 'dws_app_firstday_game_stat', COUNT(*) FROM tcy_temp.dws_app_firstday_game_stat WHERE app_id = 1880053 AND reg_date = '2026-06-17'
ORDER BY table_name
