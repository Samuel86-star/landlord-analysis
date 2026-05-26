# APP 端注册当天只玩 1 局用户分析 SQL

> 本文档承接 [APP 端注册当天只玩 1 局用户分析方案](one-game-first-day-app-analysis.md)，提供可执行 SQL。主分析口径为 `1局用户` vs `2局及以上用户`，`0局用户` 仅用于每日背景分布。
>
> **分析时间段**：2026-03-07 至 2026-05-25
> **调整分段**：调整前 `2026-03-07` 至 `2026-04-20`；调整后 `2026-04-21` 至 `2026-05-25`
> **应用口径**：`app_id = 1880053`
> **用户口径**：APP 端注册用户，排除首日登录日志缺失用户
> **对局口径**：注册当天真人对局，`robot != 1`

---

## 目录

1. [公共 CTE](#一公共-cte)
2. [每日首日局数分布](#二每日首日局数分布)
3. [画像维度差异](#三画像维度差异)
4. [首局体验差异](#四首局体验差异)
5. [高风险组合](#五高风险组合)
6. [首局失败后继续行为](#六首局失败后继续行为)
7. [调整前后分段对比](#七调整前后分段对比)
8. [经济容错模型](#八经济容错模型)
9. [结果解读建议](#九结果解读建议)

---

## 一、公共 CTE

后续 SQL 均基于以下公共 CTE。执行单条查询时，将本段 CTE 与对应查询拼接运行。

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        CASE
            WHEN r.reg_date BETWEEN '2026-03-07' AND '2026-04-20' THEN '调整前'
            WHEN r.reg_date BETWEEN '2026-04-21' AND '2026-05-25' THEN '调整后'
            ELSE '其他'
        END AS adjust_period,
        CASE
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            ELSE '其他'
        END AS platform,
        CASE
            WHEN r.reg_app_code = 'zgda' THEN 'Cocos-Lua'
            WHEN r.reg_app_code = 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        CASE
            WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨'
            WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午'
            WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午'
            ELSE 'D: 晚间'
        END AS reg_time_bucket,
        CASE
            WHEN r.first_day_login_cnt = 1 THEN 'A: 1次'
            WHEN r.first_day_login_cnt BETWEEN 2 AND 5 THEN 'B: 2-5次'
            WHEN r.first_day_login_cnt > 5 THEN 'C: 5次以上'
            ELSE 'D: 无登录记录'
        END AS first_day_login_bucket
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_base,
        g.room_fee,
        g.room_currency_lower,
        g.room_currency_upper,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        g.magnification_stacked,
        g.real_magnification,
        g.grab_landlord_bet,
        g.complete_victory_bet,
        g.bomb_bet
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.adjust_period,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt,
        MIN(g.game_datetime) AS first_game_datetime,
        TIMESTAMPDIFF(MINUTE, r.reg_datetime, MIN(g.game_datetime)) AS minutes_to_first_game
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.adjust_period,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
)
```

---

## 二、每日首日局数分布

### 2.1 查询目的

按注册日期观察 APP 新用户首日局数结构，判断 `1局用户` 占比是否稳定，以及是否存在异常日期。

### 2.2 每日分布 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        CASE
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            ELSE '其他'
        END AS platform,
        CASE
            WHEN r.reg_app_code = 'zgda' THEN 'Cocos-Lua'
            WHEN r.reg_app_code = 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        CASE
            WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨'
            WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午'
            WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午'
            ELSE 'D: 晚间'
        END AS reg_time_bucket,
        CASE
            WHEN r.first_day_login_cnt = 1 THEN 'A: 1次'
            WHEN r.first_day_login_cnt BETWEEN 2 AND 5 THEN 'B: 2-5次'
            WHEN r.first_day_login_cnt > 5 THEN 'C: 5次以上'
            ELSE 'D: 无登录记录'
        END AS first_day_login_bucket
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_base,
        g.room_fee,
        g.room_currency_lower,
        g.room_currency_upper,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        g.magnification_stacked,
        g.real_magnification,
        g.grab_landlord_bet,
        g.complete_victory_bet,
        g.bomb_bet
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt,
        MIN(g.game_datetime) AS first_game_datetime,
        TIMESTAMPDIFF(MINUTE, r.reg_datetime, MIN(g.game_datetime)) AS minutes_to_first_game
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
)
SELECT
    reg_date,
    COUNT(DISTINCT uid) AS reg_user_cnt,
    COUNT(DISTINCT CASE WHEN game_cnt_group = '0局' THEN uid END) AS zero_game_user_cnt,
    COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) AS one_game_user_cnt,
    COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) AS multi_game_user_cnt,
    ROUND(COUNT(DISTINCT CASE WHEN game_cnt_group = '0局' THEN uid END) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS zero_game_rate,
    ROUND(COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS one_game_rate,
    ROUND(COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS multi_game_rate,
    ROUND(
        COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN game_cnt_group IN ('1局', '2局及以上') THEN uid END), 0),
        2
    ) AS one_game_rate_among_played
FROM user_game_bucket
GROUP BY reg_date
ORDER BY reg_date;
```

---

## 三、画像维度差异

### 3.1 查询目的

比较 `1局用户` 与 `2局及以上用户` 的画像分布，找出在 `1局用户` 中明显偏高的渠道、端类型、客户端语言、注册时段和登录频次。

### 3.2 画像差异 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        CASE
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            ELSE '其他'
        END AS platform,
        CASE
            WHEN r.reg_app_code = 'zgda' THEN 'Cocos-Lua'
            WHEN r.reg_app_code = 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        CASE
            WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨'
            WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午'
            WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午'
            ELSE 'D: 晚间'
        END AS reg_time_bucket,
        CASE
            WHEN r.first_day_login_cnt = 1 THEN 'A: 1次'
            WHEN r.first_day_login_cnt BETWEEN 2 AND 5 THEN 'B: 2-5次'
            WHEN r.first_day_login_cnt > 5 THEN 'C: 5次以上'
            ELSE 'D: 无登录记录'
        END AS first_day_login_bucket
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_base,
        g.room_fee,
        g.room_currency_lower,
        g.room_currency_upper,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        g.magnification_stacked,
        g.real_magnification,
        g.grab_landlord_bet,
        g.complete_victory_bet,
        g.bomb_bet
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt,
        MIN(g.game_datetime) AS first_game_datetime,
        TIMESTAMPDIFF(MINUTE, r.reg_datetime, MIN(g.game_datetime)) AS minutes_to_first_game
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
),
dimension_values AS (
    SELECT uid, game_cnt_group, 'platform' AS dimension_name, platform AS dimension_value
    FROM user_game_bucket
    WHERE game_cnt_group IN ('1局', '2局及以上')
    UNION ALL
    SELECT uid, game_cnt_group, 'client_lang' AS dimension_name, client_lang AS dimension_value
    FROM user_game_bucket
    WHERE game_cnt_group IN ('1局', '2局及以上')
    UNION ALL
    SELECT uid, game_cnt_group, 'channel_category_name' AS dimension_name, channel_category_name AS dimension_value
    FROM user_game_bucket
    WHERE game_cnt_group IN ('1局', '2局及以上')
    UNION ALL
    SELECT uid, game_cnt_group, 'reg_channel_id' AS dimension_name, CAST(reg_channel_id AS VARCHAR) AS dimension_value
    FROM user_game_bucket
    WHERE game_cnt_group IN ('1局', '2局及以上')
    UNION ALL
    SELECT uid, game_cnt_group, 'reg_time_bucket' AS dimension_name, reg_time_bucket AS dimension_value
    FROM user_game_bucket
    WHERE game_cnt_group IN ('1局', '2局及以上')
    UNION ALL
    SELECT uid, game_cnt_group, 'first_day_login_bucket' AS dimension_name, first_day_login_bucket AS dimension_value
    FROM user_game_bucket
    WHERE game_cnt_group IN ('1局', '2局及以上')
),
dimension_counts AS (
    SELECT
        dimension_name,
        dimension_value,
        COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) AS one_game_user_cnt,
        COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) AS multi_game_user_cnt
    FROM dimension_values
    GROUP BY dimension_name, dimension_value
),
group_totals AS (
    SELECT
        COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) AS one_game_total,
        COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) AS multi_game_total
    FROM user_game_bucket
    WHERE game_cnt_group IN ('1局', '2局及以上')
)
SELECT
    c.dimension_name,
    c.dimension_value,
    c.one_game_user_cnt,
    c.multi_game_user_cnt,
    ROUND(c.one_game_user_cnt * 100.0 / NULLIF(t.one_game_total, 0), 2) AS one_game_share,
    ROUND(c.multi_game_user_cnt * 100.0 / NULLIF(t.multi_game_total, 0), 2) AS multi_game_share,
    ROUND(
        (c.one_game_user_cnt * 1.0 / NULLIF(t.one_game_total, 0))
        / NULLIF(c.multi_game_user_cnt * 1.0 / NULLIF(t.multi_game_total, 0), 0),
        2
    ) AS lift
FROM dimension_counts c
CROSS JOIN group_totals t
ORDER BY c.dimension_name, lift DESC, c.one_game_user_cnt DESC;
```

---

## 四、首局体验差异

### 4.1 查询目的

对 `1局用户` 取唯一一局，对 `2局及以上用户` 取首局，比较首局玩法、房间、胜负、经济变化、房间门槛、服务费压力、高倍局和逃跑表现。

### 4.2 首局体验 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        CASE
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            ELSE '其他'
        END AS platform,
        CASE
            WHEN r.reg_app_code = 'zgda' THEN 'Cocos-Lua'
            WHEN r.reg_app_code = 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        CASE
            WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨'
            WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午'
            WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午'
            ELSE 'D: 晚间'
        END AS reg_time_bucket,
        CASE
            WHEN r.first_day_login_cnt = 1 THEN 'A: 1次'
            WHEN r.first_day_login_cnt BETWEEN 2 AND 5 THEN 'B: 2-5次'
            WHEN r.first_day_login_cnt > 5 THEN 'C: 5次以上'
            ELSE 'D: 无登录记录'
        END AS first_day_login_bucket
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_base,
        g.room_fee,
        g.room_currency_lower,
        g.room_currency_upper,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        g.magnification_stacked,
        g.real_magnification,
        g.grab_landlord_bet,
        g.complete_victory_bet,
        g.bomb_bet
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt,
        MIN(g.game_datetime) AS first_game_datetime,
        TIMESTAMPDIFF(MINUTE, r.reg_datetime, MIN(g.game_datetime)) AS minutes_to_first_game
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
),
ranked_first_games AS (
    SELECT
        u.game_cnt_group,
        u.uid,
        fg.play_mode,
        fg.room_id,
        fg.timecost,
        fg.role,
        fg.result_id,
        fg.start_money,
        fg.end_money,
        fg.diff_money_pre_tax,
        fg.room_fee,
        fg.room_currency_lower,
        fg.magnification,
        fg.real_magnification,
        fg.cut,
        ROW_NUMBER() OVER (PARTITION BY fg.uid, fg.dt ORDER BY fg.game_datetime ASC, fg.resultguid ASC) AS rn
    FROM user_game_bucket u
    INNER JOIN first_day_games fg
        ON fg.app_id = u.app_id
        AND fg.uid = u.uid
        AND fg.dt = u.reg_date
    WHERE u.game_cnt_group IN ('1局', '2局及以上')
),
first_game_features AS (
    SELECT
        game_cnt_group,
        uid,
        play_mode,
        room_id,
        CASE
            WHEN play_mode = 1 THEN '经典'
            WHEN play_mode = 2 THEN '不洗牌'
            WHEN play_mode = 3 THEN '癞子'
            WHEN play_mode = 4 THEN '积分'
            WHEN play_mode = 5 THEN '比赛'
            WHEN play_mode = 6 THEN '好友房'
            ELSE '其他'
        END AS play_mode_name,
        timecost,
        CASE WHEN result_id = 1 THEN 1 ELSE 0 END AS is_win,
        diff_money_pre_tax - room_fee AS net_money_change,
        CASE WHEN end_money < room_currency_lower THEN 1 ELSE 0 END AS is_below_room_threshold,
        CASE WHEN start_money > 0 THEN room_fee * 1.0 / start_money ELSE NULL END AS fee_pressure,
        CASE WHEN magnification > 24 THEN 1 ELSE 0 END AS is_high_magnification,
        CASE WHEN cut < 0 THEN 1 ELSE 0 END AS is_escape
    FROM ranked_first_games
    WHERE rn = 1
)
SELECT
    game_cnt_group,
    play_mode_name,
    room_id,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(is_win) * 100.0, 2) AS first_win_rate,
    ROUND(AVG(timecost), 1) AS avg_first_timecost,
    ROUND(percentile_approx(timecost, 0.5), 1) AS p50_first_timecost,
    ROUND(percentile_approx(timecost, 0.9), 1) AS p90_first_timecost,
    ROUND(percentile_approx(timecost, 0.95), 1) AS p95_first_timecost,
    ROUND(percentile_approx(timecost, 0.99), 1) AS p99_first_timecost,
    MAX(timecost) AS max_first_timecost,
    ROUND(AVG(CASE WHEN timecost > 200 THEN 1 ELSE 0 END) * 100.0, 2) AS long_200_timecost_rate,
    ROUND(AVG(CASE WHEN timecost > 300 THEN 1 ELSE 0 END) * 100.0, 2) AS long_300_timecost_rate,
    ROUND(AVG(CASE WHEN timecost > 600 THEN 1 ELSE 0 END) * 100.0, 2) AS long_600_timecost_rate,
    ROUND(AVG(net_money_change), 0) AS avg_net_money_change,
    ROUND(AVG(is_below_room_threshold) * 100.0, 2) AS below_room_threshold_rate,
    ROUND(AVG(fee_pressure) * 100.0, 2) AS avg_fee_pressure,
    ROUND(AVG(is_high_magnification) * 100.0, 2) AS high_magnification_rate,
    ROUND(AVG(is_escape) * 100.0, 2) AS escape_rate
FROM first_game_features
GROUP BY game_cnt_group, play_mode_name, room_id
ORDER BY game_cnt_group, user_count DESC;
```

### 4.3 房间异常耗时组间对比 SQL

在 4.2 SQL 的 `first_game_features` CTE 基础上，追加以下 CTE 并替换最后的 `SELECT`，用于直接比较同一房间下 `1局用户` 与 `2局及以上用户` 的异常耗时差异。

```sql
, room_group_timecost AS (
    SELECT
        play_mode_name,
        room_id,
        game_cnt_group,
        COUNT(DISTINCT uid) AS user_count,
        ROUND(AVG(timecost), 1) AS avg_first_timecost,
        ROUND(percentile_approx(timecost, 0.5), 1) AS p50_first_timecost,
        ROUND(percentile_approx(timecost, 0.95), 1) AS p95_first_timecost,
        ROUND(percentile_approx(timecost, 0.99), 1) AS p99_first_timecost,
        MAX(timecost) AS max_first_timecost,
        ROUND(AVG(CASE WHEN timecost > 200 THEN 1 ELSE 0 END) * 100.0, 2) AS long_200_timecost_rate,
        ROUND(AVG(CASE WHEN timecost > 300 THEN 1 ELSE 0 END) * 100.0, 2) AS long_300_timecost_rate,
        ROUND(AVG(CASE WHEN timecost > 600 THEN 1 ELSE 0 END) * 100.0, 2) AS long_600_timecost_rate
    FROM first_game_features
    GROUP BY
        play_mode_name,
        room_id,
        game_cnt_group
),
room_timecost_compare AS (
    SELECT
        play_mode_name,
        room_id,
        MAX(CASE WHEN game_cnt_group = '1局' THEN user_count END) AS one_game_user_count,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN user_count END) AS multi_game_user_count,
        MAX(CASE WHEN game_cnt_group = '1局' THEN avg_first_timecost END) AS one_game_avg_timecost,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN avg_first_timecost END) AS multi_game_avg_timecost,
        MAX(CASE WHEN game_cnt_group = '1局' THEN p50_first_timecost END) AS one_game_p50_timecost,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN p50_first_timecost END) AS multi_game_p50_timecost,
        MAX(CASE WHEN game_cnt_group = '1局' THEN p95_first_timecost END) AS one_game_p95_timecost,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN p95_first_timecost END) AS multi_game_p95_timecost,
        MAX(CASE WHEN game_cnt_group = '1局' THEN p99_first_timecost END) AS one_game_p99_timecost,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN p99_first_timecost END) AS multi_game_p99_timecost,
        MAX(CASE WHEN game_cnt_group = '1局' THEN max_first_timecost END) AS one_game_max_timecost,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN max_first_timecost END) AS multi_game_max_timecost,
        MAX(CASE WHEN game_cnt_group = '1局' THEN long_200_timecost_rate END) AS one_game_long_200_rate,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN long_200_timecost_rate END) AS multi_game_long_200_rate,
        MAX(CASE WHEN game_cnt_group = '1局' THEN long_300_timecost_rate END) AS one_game_long_300_rate,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN long_300_timecost_rate END) AS multi_game_long_300_rate,
        MAX(CASE WHEN game_cnt_group = '1局' THEN long_600_timecost_rate END) AS one_game_long_600_rate,
        MAX(CASE WHEN game_cnt_group = '2局及以上' THEN long_600_timecost_rate END) AS multi_game_long_600_rate
    FROM room_group_timecost
    GROUP BY
        play_mode_name,
        room_id
)
SELECT
    play_mode_name,
    room_id,
    one_game_user_count,
    multi_game_user_count,
    one_game_avg_timecost,
    multi_game_avg_timecost,
    one_game_p50_timecost,
    multi_game_p50_timecost,
    one_game_p95_timecost,
    multi_game_p95_timecost,
    one_game_p99_timecost,
    multi_game_p99_timecost,
    one_game_max_timecost,
    multi_game_max_timecost,
    one_game_long_200_rate,
    multi_game_long_200_rate,
    ROUND(one_game_long_200_rate - multi_game_long_200_rate, 2) AS long_200_rate_diff,
    one_game_long_300_rate,
    multi_game_long_300_rate,
    ROUND(one_game_long_300_rate - multi_game_long_300_rate, 2) AS long_300_rate_diff,
    one_game_long_600_rate,
    multi_game_long_600_rate,
    ROUND(one_game_long_600_rate - multi_game_long_600_rate, 2) AS long_600_rate_diff,
    CASE
        WHEN one_game_long_200_rate >= multi_game_long_200_rate + 5
            AND one_game_p95_timecost > multi_game_p95_timecost
            THEN '1局用户异常耗时更高'
        WHEN one_game_long_200_rate >= 10
            AND multi_game_long_200_rate >= 10
            THEN '两组均高，优先排查房间或日志口径'
        WHEN one_game_avg_timecost > multi_game_avg_timecost
            AND one_game_p50_timecost <= multi_game_p50_timecost
            AND one_game_p99_timecost > multi_game_p99_timecost
            THEN '疑似少量极端长尾拉高均值'
        ELSE '未见明显异常'
    END AS timecost_risk_type
FROM room_timecost_compare
WHERE one_game_user_count >= 50
  AND multi_game_user_count >= 50
ORDER BY
    long_200_rate_diff DESC,
    one_game_long_200_rate DESC,
    one_game_user_count DESC;
```

---

## 五、高风险组合

### 5.1 查询目的

识别 `1局用户` 是否更集中在几个可行动的高风险组合上，并与 `2局及以上用户` 对比。

### 5.2 高风险组合 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        CASE
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            ELSE '其他'
        END AS platform,
        CASE
            WHEN r.reg_app_code = 'zgda' THEN 'Cocos-Lua'
            WHEN r.reg_app_code = 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        CASE
            WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨'
            WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午'
            WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午'
            ELSE 'D: 晚间'
        END AS reg_time_bucket,
        CASE
            WHEN r.first_day_login_cnt = 1 THEN 'A: 1次'
            WHEN r.first_day_login_cnt BETWEEN 2 AND 5 THEN 'B: 2-5次'
            WHEN r.first_day_login_cnt > 5 THEN 'C: 5次以上'
            ELSE 'D: 无登录记录'
        END AS first_day_login_bucket
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_base,
        g.room_fee,
        g.room_currency_lower,
        g.room_currency_upper,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        g.magnification_stacked,
        g.real_magnification,
        g.grab_landlord_bet,
        g.complete_victory_bet,
        g.bomb_bet
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt,
        MIN(g.game_datetime) AS first_game_datetime,
        TIMESTAMPDIFF(MINUTE, r.reg_datetime, MIN(g.game_datetime)) AS minutes_to_first_game
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
),
ranked_first_games AS (
    SELECT
        u.game_cnt_group,
        u.uid,
        u.channel_category_name,
        u.client_lang,
        u.first_day_login_cnt,
        fg.role,
        fg.result_id,
        fg.start_money,
        fg.end_money,
        fg.diff_money_pre_tax,
        fg.room_fee,
        fg.room_currency_lower,
        fg.magnification,
        ROW_NUMBER() OVER (PARTITION BY fg.uid, fg.dt ORDER BY fg.game_datetime ASC, fg.resultguid ASC) AS rn
    FROM user_game_bucket u
    INNER JOIN first_day_games fg
        ON fg.app_id = u.app_id
        AND fg.uid = u.uid
        AND fg.dt = u.reg_date
    WHERE u.game_cnt_group IN ('1局', '2局及以上')
),
first_game_features AS (
    SELECT
        game_cnt_group,
        uid,
        channel_category_name,
        client_lang,
        first_day_login_cnt,
        role,
        CASE WHEN result_id = 2 THEN 1 ELSE 0 END AS is_loss,
        diff_money_pre_tax - room_fee AS net_money_change,
        CASE WHEN end_money < room_currency_lower THEN 1 ELSE 0 END AS is_below_room_threshold,
        CASE WHEN start_money > 0 THEN room_fee * 1.0 / start_money ELSE NULL END AS fee_pressure,
        CASE WHEN magnification > 24 THEN 1 ELSE 0 END AS is_high_magnification
    FROM ranked_first_games
    WHERE rn = 1
)
SELECT
    game_cnt_group,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(CASE WHEN is_loss = 1 AND is_below_room_threshold = 1 THEN 1 ELSE 0 END) * 100.0, 2) AS loss_below_threshold_rate,
    ROUND(AVG(CASE WHEN is_loss = 1 AND is_high_magnification = 1 THEN 1 ELSE 0 END) * 100.0, 2) AS loss_high_magnification_rate,
    ROUND(AVG(CASE WHEN is_loss = 1 AND role = 1 THEN 1 ELSE 0 END) * 100.0, 2) AS loss_landlord_rate,
    ROUND(AVG(CASE WHEN fee_pressure >= 0.1 AND net_money_change < 0 THEN 1 ELSE 0 END) * 100.0, 2) AS high_fee_net_loss_rate,
    ROUND(AVG(CASE WHEN first_day_login_cnt = 1 THEN 1 ELSE 0 END) * 100.0, 2) AS single_login_rate
FROM first_game_features
GROUP BY game_cnt_group
ORDER BY game_cnt_group;
```

### 5.3 渠道分类与客户端语言组合 SQL

在 5.2 SQL 的 `first_game_features` CTE 基础上，替换最后的 `SELECT`，用于识别 `渠道分类 × 客户端语言` 是否在 `1局用户` 中明显偏高。

```sql
, channel_client_counts AS (
    SELECT
        channel_category_name,
        client_lang,
        COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) AS one_game_user_cnt,
        COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) AS multi_game_user_cnt
    FROM first_game_features
    GROUP BY
        channel_category_name,
        client_lang
),
group_totals AS (
    SELECT
        COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) AS one_game_total,
        COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) AS multi_game_total
    FROM first_game_features
)
SELECT
    c.channel_category_name,
    c.client_lang,
    c.one_game_user_cnt,
    c.multi_game_user_cnt,
    ROUND(c.one_game_user_cnt * 100.0 / NULLIF(t.one_game_total, 0), 2) AS one_game_share,
    ROUND(c.multi_game_user_cnt * 100.0 / NULLIF(t.multi_game_total, 0), 2) AS multi_game_share,
    ROUND(
        (c.one_game_user_cnt * 1.0 / NULLIF(t.one_game_total, 0))
        / NULLIF(c.multi_game_user_cnt * 1.0 / NULLIF(t.multi_game_total, 0), 0),
        2
    ) AS lift
FROM channel_client_counts c
CROSS JOIN group_totals t
WHERE c.one_game_user_cnt >= 50
ORDER BY
    lift DESC,
    c.one_game_user_cnt DESC;
```

---

## 六、首局失败后继续行为

### 6.1 查询目的

只看首局失败用户，比较 `首局失败后停止` 与 `首局失败后继续第2局` 的差异。该查询用于回答：同样首局失败，哪些首局后状态会影响玩家是否继续下一局。

### 6.2 首局失败后继续行为 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        CASE
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            ELSE '其他'
        END AS platform,
        CASE
            WHEN r.reg_app_code = 'zgda' THEN 'Cocos-Lua'
            WHEN r.reg_app_code = 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_fee,
        g.room_currency_lower,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        g.real_magnification,
        g.grab_landlord_bet,
        g.complete_victory_bet,
        g.bomb_bet,
        ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.game_datetime ASC, g.resultguid ASC) AS game_rank
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY r.app_id, r.uid, r.reg_date
),
first_loss_users AS (
    SELECT
        r.platform,
        r.client_lang,
        r.channel_category_name,
        s.uid,
        s.reg_date,
        s.first_day_game_cnt,
        CASE
            WHEN s.first_day_game_cnt = 1 THEN '首局失败后停止'
            ELSE '首局失败后继续第2局'
        END AS first_loss_continue_group,
        f.game_datetime AS first_game_datetime,
        f.room_id AS first_room_id,
        f.play_mode AS first_play_mode,
        f.role AS first_role,
        f.timecost AS first_timecost,
        f.start_money AS first_start_money,
        f.end_money AS first_end_money,
        f.diff_money_pre_tax - f.room_fee AS first_net_money_change,
        CASE WHEN f.end_money < f.room_currency_lower THEN 1 ELSE 0 END AS first_below_room_threshold,
        CASE WHEN f.room_currency_lower > 0 THEN f.end_money * 1.0 / f.room_currency_lower ELSE NULL END AS first_end_money_to_threshold,
        CASE WHEN f.start_money > 0 THEN f.room_fee * 1.0 / f.start_money ELSE NULL END AS first_fee_pressure,
        CASE WHEN f.magnification > 24 THEN 1 ELSE 0 END AS first_high_magnification,
        CASE WHEN f.role = 1 THEN 1 ELSE 0 END AS first_is_landlord,
        CASE WHEN f.bomb_bet >= 4 THEN 1 ELSE 0 END AS first_has_bomb,
        CASE WHEN f.complete_victory_bet = 2 THEN 1 ELSE 0 END AS first_has_spring,
        n.game_datetime AS second_game_datetime,
        n.room_id AS second_room_id,
        n.play_mode AS second_play_mode,
        n.result_id AS second_result_id,
        n.diff_money_pre_tax - n.room_fee AS second_net_money_change
    FROM user_game_summary s
    INNER JOIN app_reg_users r
        ON r.app_id = s.app_id
        AND r.uid = s.uid
        AND r.reg_date = s.reg_date
    INNER JOIN first_day_games f
        ON f.app_id = s.app_id
        AND f.uid = s.uid
        AND f.dt = s.reg_date
        AND f.game_rank = 1
        AND f.result_id = 2
    LEFT JOIN first_day_games n
        ON n.app_id = s.app_id
        AND n.uid = s.uid
        AND n.dt = s.reg_date
        AND n.game_rank = 2
)
SELECT
    first_loss_continue_group,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(first_timecost), 1) AS avg_first_timecost,
    ROUND(AVG(first_net_money_change), 0) AS avg_first_net_money_change,
    ROUND(AVG(first_below_room_threshold) * 100.0, 2) AS first_below_room_threshold_rate,
    ROUND(AVG(first_end_money_to_threshold), 2) AS avg_first_end_money_to_threshold,
    ROUND(AVG(first_fee_pressure) * 100.0, 2) AS avg_first_fee_pressure,
    ROUND(AVG(first_high_magnification) * 100.0, 2) AS first_high_magnification_rate,
    ROUND(AVG(first_is_landlord) * 100.0, 2) AS first_landlord_rate,
    ROUND(AVG(first_has_bomb) * 100.0, 2) AS first_has_bomb_rate,
    ROUND(AVG(first_has_spring) * 100.0, 2) AS first_has_spring_rate,
    ROUND(AVG(CASE WHEN second_game_datetime IS NOT NULL THEN TIMESTAMPDIFF(SECOND, first_game_datetime, second_game_datetime) END), 1) AS avg_seconds_to_second_game,
    ROUND(AVG(CASE WHEN second_game_datetime IS NOT NULL THEN CASE WHEN second_room_id != first_room_id THEN 1 ELSE 0 END END) * 100.0, 2) AS second_game_change_room_rate,
    ROUND(AVG(CASE WHEN second_game_datetime IS NOT NULL THEN CASE WHEN second_play_mode != first_play_mode THEN 1 ELSE 0 END END) * 100.0, 2) AS second_game_change_play_mode_rate,
    ROUND(AVG(CASE WHEN second_game_datetime IS NOT NULL THEN CASE WHEN second_result_id = 1 THEN 1 ELSE 0 END END) * 100.0, 2) AS second_game_win_rate,
    ROUND(AVG(second_net_money_change), 0) AS avg_second_net_money_change
FROM first_loss_users
GROUP BY first_loss_continue_group
ORDER BY first_loss_continue_group;
```

---

## 七、调整前后分段对比

### 7.1 查询目的

按调整时间点拆分为两段，观察调整前后 `0局`、`1局`、`2局及以上` 分布，以及 `1局用户` 在首日有对局用户中的占比变化。

调整分段：

| 分段 | 日期范围 |
| ---- | ---- |
| 调整前 | 2026-03-07 至 2026-04-20 |
| 调整后 | 2026-04-21 至 2026-05-25 |

### 7.2 分段每日分布 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        CASE
            WHEN r.reg_date BETWEEN '2026-03-07' AND '2026-04-20' THEN '调整前'
            WHEN r.reg_date BETWEEN '2026-04-21' AND '2026-05-25' THEN '调整后'
            ELSE '其他'
        END AS adjust_period
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.resultguid
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.adjust_period,
        r.reg_date,
        r.uid,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY r.adjust_period, r.reg_date, r.uid
),
user_game_bucket AS (
    SELECT
        adjust_period,
        reg_date,
        uid,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
)
SELECT
    adjust_period,
    COUNT(DISTINCT reg_date) AS day_count,
    COUNT(DISTINCT uid) AS reg_user_cnt,
    COUNT(DISTINCT CASE WHEN game_cnt_group = '0局' THEN uid END) AS zero_game_user_cnt,
    COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) AS one_game_user_cnt,
    COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) AS multi_game_user_cnt,
    ROUND(COUNT(DISTINCT CASE WHEN game_cnt_group = '0局' THEN uid END) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS zero_game_rate,
    ROUND(COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS one_game_rate,
    ROUND(COUNT(DISTINCT CASE WHEN game_cnt_group = '2局及以上' THEN uid END) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS multi_game_rate,
    ROUND(
        COUNT(DISTINCT CASE WHEN game_cnt_group = '1局' THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN game_cnt_group IN ('1局', '2局及以上') THEN uid END), 0),
        2
    ) AS one_game_rate_among_played
FROM user_game_bucket
GROUP BY adjust_period
ORDER BY adjust_period;
```

### 7.3 分段首局体验 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        CASE
            WHEN r.reg_date BETWEEN '2026-03-07' AND '2026-04-20' THEN '调整前'
            WHEN r.reg_date BETWEEN '2026-04-21' AND '2026-05-25' THEN '调整后'
            ELSE '其他'
        END AS adjust_period
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_fee,
        g.room_currency_lower,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.game_datetime ASC, g.resultguid ASC) AS game_rank
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.adjust_period,
        r.app_id,
        r.uid,
        r.reg_date,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY r.adjust_period, r.app_id, r.uid, r.reg_date
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
),
first_game_features AS (
    SELECT
        u.adjust_period,
        u.game_cnt_group,
        u.uid,
        f.play_mode,
        f.room_id,
        f.timecost,
        CASE WHEN f.result_id = 1 THEN 1 ELSE 0 END AS is_win,
        f.diff_money_pre_tax - f.room_fee AS net_money_change,
        CASE WHEN f.end_money < f.room_currency_lower THEN 1 ELSE 0 END AS is_below_room_threshold,
        CASE WHEN f.start_money > 0 THEN f.room_fee * 1.0 / f.start_money ELSE NULL END AS fee_pressure,
        CASE WHEN f.magnification > 24 THEN 1 ELSE 0 END AS is_high_magnification,
        CASE WHEN f.cut < 0 THEN 1 ELSE 0 END AS is_escape
    FROM user_game_bucket u
    INNER JOIN first_day_games f
        ON f.app_id = u.app_id
        AND f.uid = u.uid
        AND f.dt = u.reg_date
        AND f.game_rank = 1
    WHERE u.game_cnt_group IN ('1局', '2局及以上')
)
SELECT
    adjust_period,
    game_cnt_group,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(is_win) * 100.0, 2) AS first_win_rate,
    ROUND(AVG(timecost), 1) AS avg_first_timecost,
    ROUND(percentile_approx(timecost, 0.5), 1) AS p50_first_timecost,
    ROUND(percentile_approx(timecost, 0.9), 1) AS p90_first_timecost,
    ROUND(percentile_approx(timecost, 0.95), 1) AS p95_first_timecost,
    ROUND(percentile_approx(timecost, 0.99), 1) AS p99_first_timecost,
    MAX(timecost) AS max_first_timecost,
    ROUND(AVG(CASE WHEN timecost > 200 THEN 1 ELSE 0 END) * 100.0, 2) AS long_200_timecost_rate,
    ROUND(AVG(CASE WHEN timecost > 300 THEN 1 ELSE 0 END) * 100.0, 2) AS long_300_timecost_rate,
    ROUND(AVG(CASE WHEN timecost > 600 THEN 1 ELSE 0 END) * 100.0, 2) AS long_600_timecost_rate,
    ROUND(AVG(net_money_change), 0) AS avg_net_money_change,
    ROUND(AVG(is_below_room_threshold) * 100.0, 2) AS below_room_threshold_rate,
    ROUND(AVG(fee_pressure) * 100.0, 2) AS avg_fee_pressure,
    ROUND(AVG(is_high_magnification) * 100.0, 2) AS high_magnification_rate,
    ROUND(AVG(is_escape) * 100.0, 2) AS escape_rate
FROM first_game_features
GROUP BY adjust_period, game_cnt_group
ORDER BY adjust_period, game_cnt_group;
```

---

## 八、经济容错模型

### 8.1 查询目的

基于首局失败用户的房间、角色、底分、服务费、首局前资产和房间实际门槛，解释首局失败后低于门槛是否由经济规则必然导致。

注意：`room_currency_lower` 必须保留原始房间门槛，不要用固定经验值替代。经典初级房在本批数据中的首局失败用户平均门槛约为 `2,500` 银子，经济容错会明显低于按 `1,000` 门槛估算的结果。

### 8.2 经济容错模型 SQL

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-07' AND '2026-05-25'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.room_id,
        g.play_mode,
        g.room_base,
        g.room_fee,
        g.room_currency_lower,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.magnification,
        ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.game_datetime ASC, g.resultguid ASC) AS game_rank
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-07' AND '2026-05-25'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY r.app_id, r.uid, r.reg_date
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
),
first_loss_features AS (
    SELECT
        u.game_cnt_group,
        f.uid,
        CASE
            WHEN f.play_mode = 1 THEN '经典'
            WHEN f.play_mode = 2 THEN '不洗牌'
            WHEN f.play_mode = 3 THEN '癞子'
            WHEN f.play_mode = 4 THEN '积分'
            WHEN f.play_mode = 5 THEN '比赛'
            WHEN f.play_mode = 6 THEN '好友房'
            ELSE '其他'
        END AS play_mode_name,
        f.room_id,
        f.role,
        f.start_money,
        f.end_money,
        f.room_base,
        f.room_fee,
        f.room_currency_lower,
        f.magnification,
        f.start_money - f.room_fee - f.room_currency_lower AS loss_tolerance,
        CASE WHEN f.start_money > 0 THEN f.room_fee * 1.0 / f.start_money ELSE NULL END AS fee_pressure,
        CASE WHEN f.end_money < f.room_currency_lower THEN 1 ELSE 0 END AS is_below_room_threshold
    FROM user_game_bucket u
    INNER JOIN first_day_games f
        ON f.app_id = u.app_id
        AND f.uid = u.uid
        AND f.dt = u.reg_date
        AND f.game_rank = 1
        AND f.result_id = 2
    WHERE u.game_cnt_group IN ('1局', '2局及以上')
)
SELECT
    game_cnt_group,
    play_mode_name,
    room_id,
    CASE role WHEN 1 THEN '地主' WHEN 2 THEN '农民' ELSE '其他' END AS role_name,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(start_money), 0) AS avg_start_money,
    ROUND(AVG(room_fee), 0) AS avg_room_fee,
    ROUND(AVG(fee_pressure) * 100.0, 2) AS avg_fee_pressure,
    ROUND(AVG(room_base), 0) AS avg_room_base,
    ROUND(AVG(room_currency_lower), 0) AS avg_room_currency_lower,
    ROUND(AVG(loss_tolerance), 0) AS avg_loss_tolerance,
    ROUND(AVG(CASE WHEN loss_tolerance < 0 THEN 1 ELSE 0 END) * 100.0, 2) AS already_below_safe_boundary_rate,
    ROUND(AVG(CASE WHEN room_base > 0 THEN CASE WHEN loss_tolerance < 0 THEN 0 ELSE FLOOR(loss_tolerance / room_base) + 1 END END), 1) AS farmer_break_even_magnification,
    ROUND(AVG(CASE WHEN room_base > 0 THEN CASE WHEN loss_tolerance < 0 THEN 0 ELSE FLOOR(loss_tolerance / (room_base * 2)) + 1 END END), 1) AS landlord_break_even_magnification,
    ROUND(AVG(is_below_room_threshold) * 100.0, 2) AS below_room_threshold_rate,
    ROUND(AVG(magnification), 1) AS avg_magnification
FROM first_loss_features
GROUP BY game_cnt_group, play_mode_name, room_id, role
ORDER BY game_cnt_group, user_count DESC;
```

---

## 九、结果解读建议

首次看数时按以下顺序解读：

- 先看每日 `one_game_rate_among_played`，确认 `1局用户` 是稳定问题还是局部日期异常。
- 调整前后必须分开解读，优先比较 `2026-03-07` 至 `2026-04-20` 与 `2026-04-21` 至 `2026-05-25` 的差异。
- 画像差异优先看 `lift >= 1.2` 且样本量足够的维度值，避免被小样本误导。
- 首局体验差异优先看 `1局用户` 相比 `2局及以上用户` 是否在首局胜率、首局耗时、首局净收益、门槛不足、高倍局、逃跑率上明显更差。
- 房间异常耗时优先看同一 `room_id` 下 `long_200_rate_diff`、`long_300_rate_diff` 和 `long_600_rate_diff`。如果 `1局用户` 明显更高且 P95/P99 同步偏高，优先排查该房间首局链路；如果两组都高，优先排查房间玩法或日志口径；如果 P50 正常但 P99/最大值异常，优先拉长尾明细。
- 首局失败后继续行为优先看失败后余额门槛、是否换房间、到第 2 局时间间隔和第 2 局反馈。
- 经济容错模型优先看首局失败用户的 `loss_tolerance`、角色、倍数和 `below_room_threshold_rate`，判断低于门槛是体验问题还是规则必然结果。
- 高风险组合优先看同时满足高占比和高差异的组合，再判断对应产品或运营动作。
- 如果风险集中在渠道和客户端语言，优先判断为流量质量或客户端体验问题；如果风险集中在首局失败、净亏损和门槛不足，优先判断为首局体验或经济系统问题。
