# 银子经济层留存分析：金流归因

> 本文档聚焦**银子经济层**留存分析，从金流视角归因新增用户的留存与流失。全局层分析见 [retention-global.md](retention-global.md)。
>
> **分析时间段**：2026-02-10 至 2026-06-15
> **留存口径**：游戏留存（使用 `dws_app_game_active`）+ 登录留存对照
> **币种范围**：仅银子玩法（play_mode 1, 2, 3, 7）

---

## 目录

1. [数据基础与核心指标](#一数据基础与核心指标)
2. [核心金流指标分组](#二核心金流指标分组)
3. [破产分析](#三破产分析)
4. [金流波动与轨迹分析](#四金流波动与轨迹分析)
5. [投入度 × 金流交叉](#五投入度--金流交叉)
6. [510K 金流专项](#六510k-金流专项)
7. [高危经济信号组合](#七高危经济信号组合)

---

## 一、数据基础与核心指标

### 1.1 核心数据表

| 表名 | 粒度 | 说明 | 关键金流字段 |
| ---- | ---- | ---- | ---- |
| `dws_app_silvergame_stat` | uid × dt | 银子玩法金流+参与度 | `start_money`, `end_money`, `money_peak`, `money_valley`, `total_diff_money`, `total_fee_paid` |
| `dws_app_game_active` | uid × dt × app_id | 游戏留存 flag | `dt` |
| `dws_dq_app_daily_reg` | uid | APP端注册用户宽表 | `reg_date`, `reg_app_code`, `reg_group_id`, `channel_category_name` |
| `dws_dq_daily_login` | uid × login_date | 每日登录聚合 | `login_date` |

### 1.2 金流字段说明

| 字段 | 含义 | 分析用途 |
| ---- | ---- | ---- |
| `start_money` | 全天首局前银子余额 | 用户入场资本 |
| `end_money` | 全天末局后银子余额 | 用户离场资本 |
| `money_peak` | 当日银子峰值 | 当日最佳状态 |
| `money_valley` | 当日银子谷值 | **破产信号**：谷值越低越危险 |
| `total_diff_money` | 当日净输赢（不含服务费） | 盈亏方向与幅度 |
| `total_fee_paid` | 当日总服务费 | 系统抽水负担 |

### 1.3 留存 flag 计算（CTE 模板）

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id,
           reg_app_code, reg_group_id, channel_category_name
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date, r.reg_app_code, r.reg_group_id, r.channel_category_name,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS day30_game,
        MAX(CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_login,
        MAX(CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_login,
        MAX(CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS day30_login
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = r.app_id AND l.uid = r.uid
        AND l.login_date > r.reg_date
    GROUP BY r.uid, r.reg_date, r.reg_app_code, r.reg_group_id, r.channel_category_name
)
SELECT * FROM retention_flags;
```

---

## 二、核心金流指标分组

> 核心问题：首日银子盈亏如何影响留存？

### 2.1 按净输赢分组留存

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt,
           s.total_diff_money, s.money_valley, s.money_peak,
           s.game_count, s.win_rate, s.max_lose_streak
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS day30_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'Z: 无对局'
        WHEN s.total_diff_money < -50000 THEN 'A: 巨亏(<-5万)'
        WHEN s.total_diff_money < -10000 THEN 'B: 大亏(-5万~-1万)'
        WHEN s.total_diff_money < 0      THEN 'C: 小亏(-1万~0)'
        WHEN s.total_diff_money < 10000  THEN 'D: 小赚(0~1万)'
        WHEN s.total_diff_money < 50000  THEN 'E: 大赚(1万~5万)'
        ELSE                                  'F: 巨赚(>5万)'
    END AS money_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(s.game_count, 0)), 1) AS avg_games,
    ROUND(AVG(COALESCE(s.total_diff_money, 0)), 0) AS avg_diff,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(SUM(rf.day7_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(SUM(rf.day30_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day30_rate
FROM reg_users r
LEFT JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 2.2 按银子谷值分组（破产信号）

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.money_valley, s.game_count, s.total_diff_money, s.win_rate
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS day30_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'Z: 无对局'
        WHEN s.money_valley <= 0     THEN 'A: 亏光(≤0)'
        WHEN s.money_valley <= 1000  THEN 'B: 濒临破产(0-1k)'
        WHEN s.money_valley <= 5000  THEN 'C: 危险(1k-5k)'
        WHEN s.money_valley <= 20000 THEN 'D: 偏低(5k-2w)'
        ELSE                              'E: 安全(>2w)'
    END AS valley_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(s.game_count, 0)), 1) AS avg_games,
    ROUND(AVG(COALESCE(s.money_valley, 0)), 0) AS avg_valley,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(SUM(rf.day7_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_users r
LEFT JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 2.3 按银子峰值分组

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.money_peak, s.game_count, s.total_diff_money
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'Z: 无对局'
        WHEN s.money_peak <= 5000   THEN 'A: 低峰值(≤5k)'
        WHEN s.money_peak <= 20000  THEN 'B: 中峰值(5k-2w)'
        WHEN s.money_peak <= 100000 THEN 'C: 高峰值(2w-10w)'
        ELSE                             'D: 极高峰值(>10w)'
    END AS peak_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(s.game_count, 0)), 1) AS avg_games,
    ROUND(AVG(COALESCE(s.money_peak, 0)), 0) AS avg_peak,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
LEFT JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 2.4 按入场资本（start_money）分组

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.start_money, s.game_count, s.total_diff_money
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.start_money <= 3000   THEN 'A: 极低(≤3k)'
        WHEN s.start_money <= 10000  THEN 'B: 低(3k-1w)'
        WHEN s.start_money <= 50000  THEN 'C: 中(1w-5w)'
        WHEN s.start_money <= 200000 THEN 'D: 高(5w-20w)'
        ELSE                              'E: 极高(>20w)'
    END AS start_money_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.total_diff_money), 0) AS avg_diff,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

---

## 三、破产分析

> 核心问题：破产如何驱动流失？破产后用户是否还有挽回机会？

### 3.1 破产定义与破产率

```sql
-- 破产定义：银子谷值 <= 最低房间门槛（以 room_currency_lower 为参考）
-- 此处以 money_valley <= 1000 作为近似破产阈值
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.money_valley, s.game_count, s.total_diff_money
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS day30_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.money_valley <= 1000 THEN 'A: 破产(≤1k)'
        ELSE                             'B: 未破产'
    END AS bankrupt_status,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.total_diff_money), 0) AS avg_diff,
    ROUND(AVG(s.money_valley), 0) AS avg_valley,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(SUM(rf.day7_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(SUM(rf.day30_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day30_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 3.2 破产率与对局数的关系

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.money_valley, s.game_count
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
)
SELECT
    CASE
        WHEN s.game_count = 1               THEN 'A: 1局'
        WHEN s.game_count BETWEEN 2 AND 5   THEN 'B: 2-5局'
        WHEN s.game_count BETWEEN 6 AND 10  THEN 'C: 6-10局'
        WHEN s.game_count BETWEEN 11 AND 20 THEN 'D: 11-20局'
        ELSE                                     'E: 20局+'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(SUM(CASE WHEN s.money_valley <= 1000 THEN 1 ELSE 0 END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS bankrupt_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.money_valley), 0) AS avg_valley
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
GROUP BY 1
ORDER BY 1;
```

### 3.3 破产 × 留存交叉分析

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.money_valley, s.game_count, s.total_diff_money, s.win_rate
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.money_valley <= 0     THEN 'A: 亏光(≤0)'
        WHEN s.money_valley <= 1000  THEN 'B: 濒临破产(0-1k)'
        WHEN s.money_valley <= 5000  THEN 'C: 危险(1k-5k)'
        WHEN s.money_valley <= 20000 THEN 'D: 偏低(5k-2w)'
        ELSE                              'E: 安全(>2w)'
    END AS valley_group,
    CASE
        WHEN s.game_count = 1               THEN '1局'
        WHEN s.game_count BETWEEN 2 AND 5   THEN '2-5局'
        WHEN s.game_count BETWEEN 6 AND 10  THEN '6-10局'
        ELSE                                     '10局+'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.win_rate), 2) AS avg_win_rate,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(SUM(rf.day7_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1, 2
ORDER BY 1, 2;
```

---

## 四、金流波动与轨迹分析

> 核心问题：盈亏波动幅度是否比盈亏方向更重要？

### 4.1 金流波动幅度（peak-valley 差）

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.money_peak, s.money_valley, s.game_count,
           (s.money_peak - s.money_valley) AS volatility
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.volatility <= 1000   THEN 'A: 极低波动(≤1k)'
        WHEN s.volatility <= 5000   THEN 'B: 低波动(1k-5k)'
        WHEN s.volatility <= 20000  THEN 'C: 中波动(5k-2w)'
        WHEN s.volatility <= 100000 THEN 'D: 高波动(2w-10w)'
        ELSE                             'E: 极高波动(>10w)'
    END AS volatility_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.volatility), 0) AS avg_volatility,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 4.2 服务费负担分析

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.total_fee_paid, s.game_count,
           CASE WHEN s.start_money > 0 THEN s.total_fee_paid * 100.0 / s.start_money ELSE NULL END AS fee_burden_pct
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
      AND s.start_money > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.fee_burden_pct <= 1   THEN 'A: 极轻(<1%)'
        WHEN s.fee_burden_pct <= 5   THEN 'B: 轻(1-5%)'
        WHEN s.fee_burden_pct <= 15  THEN 'C: 中(5-15%)'
        WHEN s.fee_burden_pct <= 30  THEN 'D: 重(15-30%)'
        ELSE                              'E: 极重(>30%)'
    END AS fee_burden_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.fee_burden_pct), 2) AS avg_fee_pct,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 4.3 盈亏方向与留存（正收益 vs 负收益）

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.total_diff_money, s.game_count, s.win_rate, s.money_valley
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.total_diff_money >= 0 THEN 'A: 正收益'
        ELSE                              'B: 负收益'
    END AS profit_direction,
    CASE
        WHEN s.game_count BETWEEN 1 AND 3   THEN '1-3局'
        WHEN s.game_count BETWEEN 4 AND 10  THEN '4-10局'
        ELSE                                      '10局+'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.total_diff_money), 0) AS avg_diff,
    ROUND(AVG(s.win_rate), 2) AS avg_win_rate,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(SUM(rf.day7_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1, 2
ORDER BY 1, 2;
```

---

## 五、投入度 × 金流交叉

> 核心问题：对局数与盈亏如何交互影响留存？

### 5.1 game_count × total_diff_money 留存热力图

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.game_count, s.total_diff_money, s.money_valley
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.game_count = 1               THEN 'A: 1局'
        WHEN s.game_count BETWEEN 2 AND 5   THEN 'B: 2-5局'
        WHEN s.game_count BETWEEN 6 AND 10  THEN 'C: 6-10局'
        ELSE                                     'D: 10局+'
    END AS game_count_group,
    CASE
        WHEN s.total_diff_money < -50000 THEN '巨亏(<-5w)'
        WHEN s.total_diff_money < -10000 THEN '大亏(-5w~-1w)'
        WHEN s.total_diff_money < 0      THEN '小亏(-1w~0)'
        WHEN s.total_diff_money < 10000  THEN '小赚(0~1w)'
        WHEN s.total_diff_money < 50000  THEN '大赚(1w~5w)'
        ELSE                                  '巨赚(>5w)'
    END AS money_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1, 2
ORDER BY 1, 2;
```

### 5.2 win_rate × money_valley 交叉（"连败破产"信号）

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.win_rate, s.money_valley, s.game_count, s.max_lose_streak
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count >= 3
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.win_rate < 30 THEN 'A: <30%'
        WHEN s.win_rate < 50 THEN 'B: 30-50%'
        WHEN s.win_rate < 70 THEN 'C: 50-70%'
        ELSE                       'D: >=70%'
    END AS win_rate_group,
    CASE
        WHEN s.money_valley <= 1000  THEN '破产(≤1k)'
        WHEN s.money_valley <= 20000 THEN '偏低(1k-2w)'
        ELSE                              '安全(>2w)'
    END AS valley_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.max_lose_streak), 1) AS avg_lose_streak,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1, 2
ORDER BY 1, 2;
```

### 5.3 max_lose_streak × money_valley（"上头"信号）

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.max_lose_streak, s.money_valley, s.game_count, s.total_diff_money
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.max_lose_streak = 0      THEN 'A: 无连败'
        WHEN s.max_lose_streak <= 2     THEN 'B: 1-2连败'
        WHEN s.max_lose_streak <= 5     THEN 'C: 3-5连败'
        ELSE                                 'D: 5+连败'
    END AS lose_streak_group,
    CASE
        WHEN s.money_valley <= 1000  THEN '破产(≤1k)'
        WHEN s.money_valley <= 20000 THEN '偏低(1k-2w)'
        ELSE                              '安全(>2w)'
    END AS valley_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.total_diff_money), 0) AS avg_diff,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1, 2
ORDER BY 1, 2;
```

---

## 六、510K 金流专项

> 核心问题：510K 多轮结算的银子经济特征是否与经典斗地主不同？

### 6.1 510K 玩家 vs 纯经典玩家金流对比

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
allgame_stats AS (
    SELECT a.uid, a.dt, a.play_mode, a.game_count, a.total_diff_money, a.win_rate
    FROM tcy_temp.dws_app_allgame_stat a
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND a.play_mode IN (1, 2, 3, 7)
),
user_mode_flag AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.play_mode = 7 THEN 1 ELSE 0 END) AS has_510k,
        MAX(CASE WHEN a.play_mode IN (1, 2, 3) THEN 1 ELSE 0 END) AS has_classic
    FROM reg_users r
    LEFT JOIN allgame_stats a ON r.uid = a.uid AND r.reg_date = a.dt
    GROUP BY r.uid, r.reg_date
),
silver_stats AS (
    SELECT s.uid, s.dt, s.game_count, s.total_diff_money, s.money_valley, s.win_rate
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN m.has_510k = 1 AND m.has_classic = 1 THEN 'A: 双修(510K+经典)'
        WHEN m.has_510k = 1                       THEN 'B: 仅510K'
        WHEN m.has_classic = 1                    THEN 'C: 仅经典'
        ELSE                                           'D: 无银子对局'
    END AS mode_type,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(COALESCE(s.game_count, 0)), 1) AS avg_games,
    ROUND(AVG(COALESCE(s.total_diff_money, 0)), 0) AS avg_diff,
    ROUND(AVG(COALESCE(s.money_valley, 0)), 0) AS avg_valley,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(SUM(rf.day7_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_users r
LEFT JOIN user_mode_flag m ON r.uid = m.uid AND r.reg_date = m.reg_date
LEFT JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 6.2 510K 多轮结算的金流波动

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
crazyddz_stats AS (
    SELECT a.uid, a.dt, a.avg_settle_rounds, a.total_diff_money, a.outcome_gdp, a.game_count
    FROM tcy_temp.dws_app_allgame_stat a
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND a.play_mode = 7
      AND a.game_count > 0
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN c.avg_settle_rounds <= 3  THEN 'A: 短(≤3轮)'
        WHEN c.avg_settle_rounds <= 6  THEN 'B: 中(4-6轮)'
        WHEN c.avg_settle_rounds <= 10 THEN 'C: 长(7-10轮)'
        ELSE                                'D: 极长(>10轮)'
    END AS round_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(c.game_count), 1) AS avg_games,
    ROUND(AVG(c.avg_settle_rounds), 1) AS avg_rounds,
    ROUND(AVG(c.total_diff_money), 0) AS avg_diff,
    ROUND(AVG(c.outcome_gdp), 0) AS avg_gdp,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN crazyddz_stats c ON r.uid = c.uid AND r.reg_date = c.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

---

## 七、高危经济信号组合

> 核心问题：哪些金流组合是最高危的流失信号？

### 7.1 连败 × 破产 × 高倍输 三维交叉

```sql
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.max_lose_streak, s.money_valley, s.total_diff_money, s.game_count
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count > 0
),
high_multi_loss AS (
    SELECT a.uid, a.dt,
           SUM(a.multi_q4_losses) AS total_q4_losses
    FROM tcy_temp.dws_app_allgame_stat a
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND a.play_mode IN (1, 2, 3, 7)
    GROUP BY a.uid, a.dt
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS day7_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.max_lose_streak >= 3 AND s.money_valley <= 1000 AND COALESCE(h.total_q4_losses, 0) > 0
            THEN 'A: 连败×破产×高倍输（P0）'
        WHEN s.max_lose_streak >= 3 AND s.money_valley <= 1000
            THEN 'B: 连败×破产'
        WHEN s.max_lose_streak >= 3 AND COALESCE(h.total_q4_losses, 0) > 0
            THEN 'C: 连败×高倍输'
        WHEN s.money_valley <= 1000 AND COALESCE(h.total_q4_losses, 0) > 0
            THEN 'D: 破产×高倍输'
        WHEN s.max_lose_streak >= 3
            THEN 'E: 仅连败'
        WHEN s.money_valley <= 1000
            THEN 'F: 仅破产'
        ELSE 'G: 正常'
    END AS risk_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.total_diff_money), 0) AS avg_diff,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(SUM(rf.day7_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN high_multi_loss h ON r.uid = h.uid AND r.reg_date = h.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

### 7.2 金流轨迹异常检测

```sql
-- 识别「先赢后输」型用户：曾经 peak 很高但最终亏损
WITH reg_users AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
silver_stats AS (
    SELECT s.uid, s.dt, s.start_money, s.end_money, s.money_peak, s.money_valley,
           s.total_diff_money, s.game_count, s.win_rate
    FROM tcy_temp.dws_app_silvergame_stat s
    WHERE s.app_id = 1880053
      AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND s.game_count >= 5
),
retention_flags AS (
    SELECT
        r.uid, r.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS day1_game
    FROM reg_users r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON r.uid = a.uid AND a.app_id = r.app_id
        AND a.dt > r.reg_date
    GROUP BY r.uid, r.reg_date
)
SELECT
    CASE
        WHEN s.money_peak > s.start_money AND s.end_money < s.start_money
            THEN 'A: 先赢后输（过山车）'
        WHEN s.money_peak <= s.start_money AND s.end_money < s.start_money
            THEN 'B: 一路亏损'
        WHEN s.end_money >= s.start_money
            THEN 'C: 最终盈利'
        ELSE 'D: 持平'
    END AS trajectory,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.money_peak - s.start_money), 0) AS avg_max_gain,
    ROUND(AVG(s.end_money - s.money_peak), 0) AS avg_giveback,
    ROUND(SUM(rf.day1_game) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_users r
INNER JOIN silver_stats s ON r.uid = s.uid AND r.reg_date = s.dt
LEFT JOIN retention_flags rf ON r.uid = rf.uid AND r.reg_date = rf.reg_date
GROUP BY 1
ORDER BY 1;
```

---

## 八、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| ---- | ---- | ---- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数/胜率/经济差异 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |
| [retention-score-game.md](retention-score-game.md) | 积分玩法层 | 积分参与度与胜负归因 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 多表联合诊断 |
| [retention-analysis-framework.md](retention-analysis-framework.md) | 框架速查 | 指标相关性速查 |

---

> **文档版本**：v1.0
> **创建时间**：2026-06-15
> **更新说明**：
>
> - v1.0：初始版本，基于重构后的 `dws_app_silvergame_stat` 金流表 + `dws_app_game_active` 留存表构建
