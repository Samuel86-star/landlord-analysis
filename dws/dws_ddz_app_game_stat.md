# DWS 中间表：APP 端每日游戏行为统计表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_app_game_stat` |
| 全名 | `tcy_temp.dws_ddz_app_game_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日游戏行为统计表，包含对局数、胜负、倍数、经济等汇总指标 |
| 粒度 | uid × dt × app_code（一个用户一天一个客户端版本一行） |

## 设计背景

`dws_ddz_daily_game` 为对局级明细表，在做用户级别的留存分析时，需要频繁聚合计算用户的每日游戏行为，查询效率较低。

**解决方案**：预聚合用户每日的游戏行为特征，提升留存分析效率。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| uid | bigint | 玩家唯一标识 | 123456789 |
| dt | bigint | 对局日期（YYYYMMDD） | 20260210 |
| app_code | string | 客户端代码（zgdx=cocos creator, zgda=cocos lua） | "zgdx" |
| game_count | bigint | 当日对局总数 | 12 |
| total_play_seconds | bigint | 当日总游戏时长（秒） | 3600 |
| avg_game_seconds | double | 平均每局时长 | 180.5 |
| win_count | bigint | 当日胜利局数 | 7 |
| lose_count | bigint | 当日失败局数 | 5 |
| win_rate | double | 当日胜率（百分比） | 58.33 |
| max_win_streak | int | 当日最大连胜 | 3 |
| max_lose_streak | int | 当日最大连败 | 2 |
| avg_magnification | double | 平均理论倍数 | 12.5 |
| max_magnification | int | 最大理论倍数 | 48 |
| avg_real_magnification | double | 平均实际倍数（ABS） | 10.2 |
| low_multi_games | bigint | 低倍局数（magnification <= 6） | 5 |
| mid_multi_games | bigint | 中倍局数（6 < magnification <= 24） | 4 |
| high_multi_games | bigint | 高倍局数（magnification > 24） | 3 |
| high_multi_wins | bigint | 高倍局胜利数 | 2 |
| high_multi_losses | bigint | 高倍局失败数 | 1 |
| total_bomb_count | bigint | 当日炸弹总数 | 8 |
| games_with_grab | bigint | 抢地主局数 | 6 |
| games_player_doubled | bigint | 玩家加倍局数 | 4 |
| start_money | bigint | 首局前货币数量（按时间第一局） | 10000 |
| end_money | bigint | 末局后货币数量（按时间最后一局） | 15000 |
| money_peak | bigint | 当日货币峰值（所有对局 end_money 最大值） | 18000 |
| money_valley | bigint | 当日货币谷值（所有对局 end_money 最小值） | 8000 |
| total_diff_money | bigint | 当日总输赢（含服务费还原） | 5000 |
| total_fee_paid | bigint | 当日总服务费 | 1200 |
| escape_count | bigint | 当日逃跑次数 | 0 |
| distinct_rooms | bigint | 当日游玩房间数 | 3 |
| play_modes | string | 当日游玩玩法（逗号分隔） | "1,2,5" |

## 客户端开发语言说明

| app_code | 客户端开发语言 | 界面和流程特点 |
| ------- | ------------ | -------------- |
| zgdx | Cocos Creator | 界面和流程较新，体验优化 |
| zgda | Cocos Lua | 界面和流程较传统 |

> **说明**：本表支持按客户端开发语言维度分析用户行为差异。通过 `app_code` 字段区分不同客户端版本的用户，粒度为 uid × dt × app_code（一个用户一天一个客户端版本一行）。

## 玩法分类说明

| play_mode | 玩法 | 币种 |
| --------- | ---- | ---- |
| 1 | 经典 | 银子 |
| 2 | 不洗牌 | 银子 |
| 3 | 癞子 | 银子 |
| 5 | 比赛（APP/小游戏端） | 银子 |
| 4 | 积分（PC端） | 积分 |
| 6 | 好友房 | 积分 |

> **说明**：本表仅统计 APP 端用户（`group_id` IN 6,66,8,88,33,44,77,99）的银子玩法（play_mode IN 1,2,3,5），排除 PC 端积分玩法。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_ddz_app_game_stat (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  `app_code` varchar(64) NULL COMMENT "",
  `game_count` int(11) NULL COMMENT "",
  `total_play_seconds` int(11) NULL COMMENT "",
  `avg_game_seconds` double NULL COMMENT "",
  `win_count` int(11) NULL COMMENT "",
  `lose_count` int(11) NULL COMMENT "",
  `win_rate` double NULL COMMENT "",
  `max_win_streak` int(11) NULL COMMENT "",
  `max_lose_streak` int(11) NULL COMMENT "",
  `avg_magnification` double NULL COMMENT "",
  `max_magnification` int(11) NULL COMMENT "",
  `avg_real_magnification` double NULL COMMENT "",
  `low_multi_games` int(11) NULL COMMENT "",
  `mid_multi_games` int(11) NULL COMMENT "",
  `high_multi_games` int(11) NULL COMMENT "",
  `high_multi_wins` int(11) NULL COMMENT "",
  `high_multi_losses` int(11) NULL COMMENT "",
  `total_bomb_count` int(11) NULL COMMENT "",
  `games_with_grab` int(11) NULL COMMENT "",
  `games_player_doubled` int(11) NULL COMMENT "",
  `start_money` bigint(20) NULL COMMENT "",
  `end_money` bigint(20) NULL COMMENT "",
  `money_peak` bigint(20) NULL COMMENT "",
  `money_valley` bigint(20) NULL COMMENT "",
  `total_diff_money` bigint(20) NULL COMMENT "",
  `total_fee_paid` int(11) NULL COMMENT "",
  `escape_count` int(11) NULL COMMENT "",
  `distinct_rooms` tinyint(4) NULL COMMENT "",
  `play_modes` varchar(256) NULL COMMENT ""
) ENGINE=OLAP 
DUPLICATE KEY(`app_id`, `uid`, `dt`) 
COMMENT "游戏用户对局聚合信息表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "colocate_with" = "group_daily_data", 
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);
```

### 增量数据导入

```sql
insert into tcy_temp.dws_ddz_app_game_stat
WITH game_enriched AS (
    SELECT
        *,
        ROW_NUMBER() OVER (PARTITION BY uid, app_code ORDER BY game_datetime ASC) AS game_seq,
        ROW_NUMBER() OVER (PARTITION BY uid, app_code ORDER BY game_datetime DESC) AS rank_desc
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt between '2026-04-01' and '2026-04-21'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)  
      AND play_mode IN (1, 2, 3, 5)  
),
streaks_calc AS (
    SELECT 
        uid, app_code, result_id, MAX(streak_len) as max_streak
    FROM (
        SELECT 
            uid, app_code, result_id, 
            COUNT(*) OVER(PARTITION BY uid, app_code, result_id, grp) AS streak_len
        FROM (
            SELECT uid, app_code, result_id, 
              game_seq - ROW_NUMBER() OVER (PARTITION BY uid, app_code, result_id ORDER BY game_seq) AS grp
            FROM game_enriched
            WHERE result_id IN (1, 2)
        ) t1
    ) t2
    GROUP BY uid, app_code, result_id
),
max_streaks AS (
    SELECT uid, app_code, 
        MAX(CASE WHEN result_id = 1 THEN max_streak ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN max_streak ELSE 0 END) AS max_lose_streak
    FROM streaks_calc
    GROUP BY uid, app_code
)
SELECT
    g.app_id, g.uid, g.dt, g.app_code,
    COUNT(*) AS game_count,
    SUM(g.timecost) AS total_play_seconds,
    ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
    COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ANY_VALUE(s.max_win_streak),
    ANY_VALUE(s.max_lose_streak),
    ROUND(AVG(g.magnification), 2),
    MAX(g.magnification),
    ROUND(AVG(ABS(g.real_magnification)), 2),
    COUNT(CASE WHEN g.magnification <= 6 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 END),
    SUM(g.bomb_bet / 2),
    COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END),
    COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END),
    MAX(CASE WHEN g.game_seq = 1 THEN g.start_money END), 
    MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END),
    MAX(g.end_money),
    MIN(g.end_money),
    SUM(g.diff_money_pre_tax),
    SUM(g.room_fee),
    COUNT(CASE WHEN g.cut < 0 THEN 1 END),
    COUNT(DISTINCT g.room_id),
    GROUP_CONCAT(DISTINCT CAST(g.play_mode AS VARCHAR) ORDER BY g.play_mode)
FROM game_enriched g
LEFT JOIN max_streaks s ON g.uid = s.uid AND g.app_code = s.app_code
GROUP BY g.app_id, g.uid, g.dt, g.app_code;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

## 注意事项

1. **玩法过滤**：仅统计银子玩法（play_mode IN 1,2,3,5），排除积分玩法
2. **APP 端过滤**：仅统计 APP 端用户（group_id IN 6,66,8,88,33,44,77,99）
3. **比赛玩法**：play_mode=5 为 APP/小游戏端的比赛玩法，使用银子币种
4. **实际倍数**：`avg_real_magnification` 已使用 `ABS()` 处理，保证非负
5. **连胜连败**：仅统计胜负局，排除无效局
6. **数据完整性**：如用户当日无对局，本表无对应记录

## 与其他 DWS 表的关系

```
tcy_temp.dws_ddz_daily_game        （对局明细表）
            ↓  APP端+银子玩法聚合
tcy_temp.dws_ddz_app_game_stat   （APP端用户每日游戏统计表）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg      （APP 端注册用户宽表）
tcy_temp.dws_dq_daily_login        （每日登录聚合表）
```

## 关联查询示例

### 1. 计算新增用户留存（注册日有对局的用户）

```sql
-- 新增用户留存（注册且有对局）
SELECT
    r.reg_date,
    COUNT(DISTINCT r.uid) AS reg_user_count,
    COUNT(DISTINCT g.uid) AS game_user_count,
    COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) AS day1_retained,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260215
GROUP BY r.reg_date
ORDER BY r.reg_date;
```

### 2. 按首日对局数分析留存

```sql
SELECT
    r.reg_date,
    CASE 
        WHEN g.game_count = 1 THEN '1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN '2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN '6-10局'
        ELSE '10局以上'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260210
GROUP BY r.reg_date,
    CASE 
        WHEN g.game_count = 1 THEN '1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN '2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN '6-10局'
        ELSE '10局以上'
    END
ORDER BY r.reg_date, game_count_group;
```

### 3. 按首日胜率分析留存

```sql
SELECT
    r.reg_date,
    CASE 
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.game_count), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260210
  AND g.game_count > 0
GROUP BY r.reg_date,
    CASE 
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END
ORDER BY r.reg_date, win_rate_group;
```

### 4. 按首日高倍局经历分析留存

```sql
SELECT
    r.reg_date,
    CASE
        WHEN g.high_multi_games = 0 OR g.high_multi_games IS NULL THEN 'A: 未经历高倍'
        WHEN g.high_multi_wins > 0 AND g.high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN g.high_multi_wins = 0 AND g.high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END AS high_multi_exp,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260210
GROUP BY r.reg_date,
    CASE
        WHEN g.high_multi_games = 0 OR g.high_multi_games IS NULL THEN 'A: 未经历高倍'
        WHEN g.high_multi_wins > 0 AND g.high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN g.high_multi_wins = 0 AND g.high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END
ORDER BY r.reg_date, high_multi_exp;
```

## 注意事项

1. **玩法过滤**：本表仅聚合银子玩法（play_mode IN 1,2,3,5），排除积分玩法
2. **比赛玩法**：play_mode=5 为 APP/小游戏端的比赛玩法，使用银子币种
3. **实际倍数**：`avg_real_magnification` 已使用 `ABS()` 处理，保证非负
4. **连胜连败**：仅统计胜负局，排除无效局
5. **数据完整性**：如用户当日无对局，本表无对应记录

## 与其他 DWS 表的关系

```
tcy_temp.dws_ddz_daily_game        （对局明细表）
            ↓  APP端+银子玩法聚合
tcy_temp.dws_ddz_app_game_stat   （APP端用户每日游戏统计表）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg      （APP 端注册用户宽表）
tcy_temp.dws_dq_daily_login        （每日登录聚合表）
```

## 关联查询示例

### 1. 计算新增用户留存（注册日有对局的用户）

```sql
-- 新增用户留存（注册且有对局）
SELECT
    r.reg_date,
    COUNT(DISTINCT r.uid) AS reg_user_count,
    COUNT(DISTINCT g.uid) AS game_user_count,
    COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) AS day1_retained,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date BETWEEN 20260210 AND 20260215
GROUP BY r.reg_date
ORDER BY r.reg_date;
```

### 2. 按首日对局数分析留存

```sql
SELECT
    r.reg_date,
    CASE 
        WHEN g.game_count = 1 THEN '0:1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN '1:2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN '2:6-10局'
        ELSE '3:10局以上'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260210
GROUP BY r.reg_date,
    CASE 
        WHEN g.game_count = 1 THEN '0:1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN '1:2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN '2:6-10局'
        ELSE '3:10局以上'
    END
ORDER BY r.reg_date, game_count_group;
```

### 3. 按首日胜率分析留存

```sql
SELECT
    r.reg_date,
    CASE 
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.game_count), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260210
  AND g.game_count > 0
GROUP BY r.reg_date,
    CASE 
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END
ORDER BY r.reg_date, win_rate_group;
```

### 4. 按首日高倍局经历分析留存

```sql
SELECT
    r.reg_date,
    CASE
        WHEN g.high_multi_games = 0 OR g.high_multi_games IS NULL THEN 'A: 未经历高倍'
        WHEN g.high_multi_wins > 0 AND g.high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN g.high_multi_wins = 0 AND g.high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END AS high_multi_exp,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260210
GROUP BY r.reg_date,
    CASE
        WHEN g.high_multi_games = 0 OR g.high_multi_games IS NULL THEN 'A: 未经历高倍'
        WHEN g.high_multi_wins > 0 AND g.high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN g.high_multi_wins = 0 AND g.high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END
ORDER BY r.reg_date, high_multi_exp;
```

> **文档版本**：v2.0
> **创建时间**：2026-04-09
> **更新说明**：
> - v1.0：初始版本
> - **v2.0**：重命名为 `dws_ddz_app_game_stat`，增加 `group_id IN (6,66,8,88,33,44,77,99)` APP 端过滤