# DWS 中间表：APP 端每日游戏行为统计表

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_appdaily_game_stat` |
| 全名 | `tcy_temp.dws_ddz_appdaily_game_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日游戏行为统计表，包含对局数、胜负、倍数、经济等汇总指标 |
| 粒度 | uid × dt（一个用户一天一行） |

## 设计背景

`dws_ddz_daily_game` 为对局级明细表，在做用户级别的留存分析时，需要频繁聚合计算用户的每日游戏行为，查询效率较低。

**解决方案**：预聚合用户每日的游戏行为特征，提升留存分析效率。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
|--------|------|------|--------|
| uid | bigint | 玩家唯一标识 | 123456789 |
| dt | bigint | 对局日期（YYYYMMDD） | 20260210 |
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

## 玩法分类说明

| play_mode | 玩法 | 币种 |
|-----------|------|------|
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
CREATE TABLE tcy_temp.dws_ddz_appdaily_game_stat (
    uid BIGINT,
    dt BIGINT,
    game_count BIGINT,
    total_play_seconds BIGINT,
    avg_game_seconds DOUBLE,
    win_count BIGINT,
    lose_count BIGINT,
    win_rate DOUBLE,
    max_win_streak INT,
    max_lose_streak INT,
    avg_magnification DOUBLE,
    max_magnification INT,
    avg_real_magnification DOUBLE,
    low_multi_games BIGINT,
    mid_multi_games BIGINT,
    high_multi_games BIGINT,
    high_multi_wins BIGINT,
    high_multi_losses BIGINT,
    total_bomb_count BIGINT,
    games_with_grab BIGINT,
    games_player_doubled BIGINT,
    start_money BIGINT,
    end_money BIGINT,
    money_peak BIGINT,
    money_valley BIGINT,
    total_diff_money BIGINT,
    total_fee_paid BIGINT,
    escape_count BIGINT,
    distinct_rooms BIGINT,
    play_modes STRING
)
DUPLICATE KEY(uid, dt)
DISTRIBUTED BY HASH(uid) BUCKETS 32
ORDER BY dt, uid
PROPERTIES("replication_num" = "1");
```

### 增量数据导入

```sql
INSERT INTO tcy_temp.dws_ddz_appdaily_game_stat
WITH game_enriched AS (
    -- 1. 预处理：在单层扫描中完成基础过滤和窗口排序
    SELECT
        *,
        -- 确定玩家全天首局和末局顺序，为后续提取 start/end_money 做准备
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY time_unix ASC) AS rank_asc,
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY time_unix DESC) AS rank_desc,
        -- 为连胜连败计算准备：生成全天对局序号
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY time_unix ASC) AS game_seq
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt = 20260408
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)  -- 仅 APP 端
      AND play_mode IN (1, 2, 3, 5)  -- 仅银子玩法
),
streaks_calc AS (
    -- 2. 连胜连败逻辑：利用 game_seq - 内部序号的差值分组（经典 Gaps and Islands 算法）
    SELECT 
        uid, 
        result_id,
        COUNT(*) AS streak_len
    FROM (
        SELECT 
            uid, 
            result_id,
            game_seq - ROW_NUMBER() OVER (PARTITION BY uid, result_id ORDER BY game_seq) AS grp
        FROM game_enriched
        WHERE result_id IN (1, 2)
    ) t
    GROUP BY uid, result_id, grp
),
max_streaks AS (
    -- 3. 汇总最大连胜连败
    SELECT 
        uid,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM streaks_calc
    GROUP BY uid
)
-- 4. 最终聚合
SELECT
    g.uid,
    g.dt,
    -- 对局及时间统计
    COUNT(*) AS game_count,
    SUM(g.timecost) AS total_play_seconds,
    ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
    -- 胜负统计
    COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ANY_VALUE(s.max_win_streak) AS max_win_streak,
    ANY_VALUE(s.max_lose_streak) AS max_lose_streak,
    -- 倍数统计（使用更简洁的条件聚合）
    ROUND(AVG(g.magnification), 2) AS avg_magnification,
    MAX(g.magnification) AS max_magnification,
    ROUND(AVG(ABS(g.real_magnification)), 2) AS avg_real_magnification,
    COUNT(CASE WHEN g.magnification <= 6 THEN 1 END) AS low_multi_games,
    COUNT(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 END) AS mid_multi_games,
    COUNT(CASE WHEN g.magnification > 24 THEN 1 END) AS high_multi_games,
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 END) AS high_multi_wins,
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 END) AS high_multi_losses,
    -- 特征特征
    SUM(g.bomb_bet / 2) AS total_bomb_count,
    COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END) AS games_with_grab,
    COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END) AS games_player_doubled,
    -- 经济统计
    MAX(CASE WHEN g.rank_asc = 1 THEN g.start_money END) AS start_money,
    MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END) AS end_money,
    MAX(g.end_money) AS money_peak,
    MIN(g.end_money) AS money_valley,
    SUM(g.diff_money_pre_tax) AS total_diff_money,
    SUM(g.room_fee) AS total_fee_paid,
    -- 逃跑和房间
    COUNT(CASE WHEN g.cut < 0 THEN 1 END) AS escape_count,
    COUNT(DISTINCT g.room_id) AS distinct_rooms,
    GROUP_CONCAT(DISTINCT CAST(g.play_mode AS VARCHAR) ORDER BY g.play_mode) AS play_modes
FROM game_enriched g
LEFT JOIN max_streaks s ON g.uid = s.uid
GROUP BY g.uid, g.dt;
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
tcy_temp.dws_ddz_appdaily_game_stat   （APP端用户每日游戏统计表）
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
tcy_temp.dws_ddz_appdaily_game_stat   （APP端用户每日游戏统计表）
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
LEFT JOIN tcy_temp.dws_ddz_appdaily_game_stat g ON r.uid = g.uid AND r.reg_date = g.dt
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
> - **v2.0**：重命名为 `dws_ddz_appdaily_game_stat`，增加 `group_id IN (6,66,8,88,33,44,77,99)` APP 端过滤