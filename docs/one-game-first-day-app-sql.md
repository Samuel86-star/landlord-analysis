# APP 端注册当天只玩 1 局用户分析 SQL

> 本文档承接 [APP 端注册当天只玩 1 局用户分析方案](one-game-first-day-app-analysis.md)，提供可执行 SQL。主分析口径为 `1局用户` vs `2局及以上用户`，`0局用户` 仅用于每日背景分布。
>
> **分析时间段**：2026-02-10 至 2026-05-10
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
6. [结果解读建议](#六结果解读建议)

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
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
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
        CASE WHEN magnification >= 24 THEN 1 ELSE 0 END AS is_high_magnification,
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
    ROUND(AVG(net_money_change), 0) AS avg_net_money_change,
    ROUND(AVG(is_below_room_threshold) * 100.0, 2) AS below_room_threshold_rate,
    ROUND(AVG(fee_pressure) * 100.0, 2) AS avg_fee_pressure,
    ROUND(AVG(is_high_magnification) * 100.0, 2) AS high_magnification_rate,
    ROUND(AVG(is_escape) * 100.0, 2) AS escape_rate
FROM first_game_features
GROUP BY game_cnt_group, play_mode_name, room_id
ORDER BY game_cnt_group, user_count DESC;
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
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
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
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
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
        first_day_login_cnt,
        role,
        CASE WHEN result_id = 2 THEN 1 ELSE 0 END AS is_loss,
        diff_money_pre_tax - room_fee AS net_money_change,
        CASE WHEN end_money < room_currency_lower THEN 1 ELSE 0 END AS is_below_room_threshold,
        CASE WHEN start_money > 0 THEN room_fee * 1.0 / start_money ELSE NULL END AS fee_pressure,
        CASE WHEN magnification >= 24 THEN 1 ELSE 0 END AS is_high_magnification
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

---

## 六、结果解读建议

首次看数时按以下顺序解读：

- 先看每日 `one_game_rate_among_played`，确认 `1局用户` 是稳定问题还是局部日期异常。
- 画像差异优先看 `lift >= 1.2` 且样本量足够的维度值，避免被小样本误导。
- 首局体验差异优先看 `1局用户` 相比 `2局及以上用户` 是否在首局胜率、首局净收益、门槛不足、高倍局、逃跑率上明显更差。
- 高风险组合优先看同时满足高占比和高差异的组合，再判断对应产品或运营动作。
- 如果风险集中在渠道和客户端语言，优先判断为流量质量或客户端体验问题；如果风险集中在首局失败、净亏损和门槛不足，优先判断为首局体验或经济系统问题。
