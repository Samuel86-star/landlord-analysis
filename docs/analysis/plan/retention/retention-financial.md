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
WITH reg_base_raw AS (
    -- 基础人群（🌟 全文唯一人工维护的时间窗口）
    SELECT uid, reg_date, app_id,
           reg_app_code, reg_group_id, channel_category_name,
           DATE_ADD(reg_date, INTERVAL 1 DAY)  AS d1_target,
           DATE_ADD(reg_date, INTERVAL 6 DAY)  AS d7_target,
           DATE_ADD(reg_date, INTERVAL 29 DAY) AS d30_target
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 动态时间边界：次留 ~ 30 留，用于活跃表分区裁剪
    SELECT
        MIN(reg_date) AS min_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY)  AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 29 DAY) AS max_act_date
    FROM reg_base_raw
),
retention_flags AS (
    -- 矩阵坍缩：把目标日期常量化，dt IN (...) 精确裁剪 + 分区裁剪
    SELECT
        r.uid, r.reg_date, r.reg_app_code, r.reg_group_id, r.channel_category_name,
        MAX(CASE WHEN a.dt = r.d1_target  THEN 1 ELSE 0 END) AS day1_game,
        MAX(CASE WHEN a.dt = r.d7_target  THEN 1 ELSE 0 END) AS day7_game,
        MAX(CASE WHEN a.dt = r.d30_target THEN 1 ELSE 0 END) AS day30_game,
        MAX(CASE WHEN l.login_date = r.d1_target  THEN 1 ELSE 0 END) AS day1_login,
        MAX(CASE WHEN l.login_date = r.d7_target  THEN 1 ELSE 0 END) AS day7_login,
        MAX(CASE WHEN l.login_date = r.d30_target THEN 1 ELSE 0 END) AS day30_login
    FROM reg_base_raw r
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = r.app_id AND a.uid = r.uid
        AND a.dt IN (r.d1_target, r.d7_target, r.d30_target)
        AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = r.app_id AND l.uid = r.uid
        AND l.login_date IN (r.d1_target, r.d7_target, r.d30_target)
        AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
    GROUP BY r.uid, r.reg_date, r.reg_app_code, r.reg_group_id, r.channel_category_name
)
SELECT * FROM retention_flags;
```

---

## 二、核心金流指标分组

> 核心问题：首日银子盈亏如何影响留存？

### 2.1 按净输赢分组留存

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：净输赢分组 + D1/D7/D30 目标日期常量
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'Z: 无对局'
            WHEN s.total_diff_money < -50000 THEN 'A: 巨亏(<-5万)'
            WHEN s.total_diff_money < -10000 THEN 'B: 大亏(-5万~-1万)'
            WHEN s.total_diff_money < 0      THEN 'C: 小亏(-1万~0)'
            WHEN s.total_diff_money < 10000  THEN 'D: 小赚(0~1万)'
            WHEN s.total_diff_money < 50000  THEN 'E: 大赚(1万~5万)'
            ELSE                                  'F: 巨赚(>5万)'
        END AS money_group,
        COALESCE(s.game_count, 0) AS game_count,
        COALESCE(s.total_diff_money, 0) AS total_diff_money,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY)  AS d1_target,
        DATE_ADD(r.reg_date, INTERVAL 6 DAY)  AS d7_target,
        DATE_ADD(r.reg_date, INTERVAL 29 DAY) AS d30_target
    FROM reg_base_raw r
    LEFT JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵坍缩：D1/D7/D30 游戏留存标签
    SELECT
        p.uid, p.money_group, p.game_count, p.total_diff_money,
        MAX(CASE WHEN a.dt = p.d1_target  THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d7_target  THEN 1 ELSE 0 END) AS is_d7,
        MAX(CASE WHEN a.dt = p.d30_target THEN 1 ELSE 0 END) AS is_d30
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d7_target, p.d30_target)
    GROUP BY p.uid, p.money_group, p.game_count, p.total_diff_money
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    money_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(SUM(is_d1)  * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7)  * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(SUM(is_d30) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day30_rate
FROM all_events_stream
GROUP BY money_group
ORDER BY money_group;
```

### 2.2 按银子谷值分组（破产信号）

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：银子谷值分组 + D1/D7 目标日期常量
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'Z: 无对局'
            WHEN s.money_valley <= 0     THEN 'A: 亏光(≤0)'
            WHEN s.money_valley <= 1000  THEN 'B: 濒临破产(0-1k)'
            WHEN s.money_valley <= 5000  THEN 'C: 危险(1k-5k)'
            WHEN s.money_valley <= 20000 THEN 'D: 偏低(5k-2w)'
            ELSE                              'E: 安全(>2w)'
        END AS valley_group,
        COALESCE(s.game_count, 0) AS game_count,
        COALESCE(s.money_valley, 0) AS money_valley,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(r.reg_date, INTERVAL 6 DAY) AS d7_target
    FROM reg_base_raw r
    LEFT JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.valley_group, p.game_count, p.money_valley,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d7_target)
    GROUP BY p.uid, p.valley_group, p.game_count, p.money_valley
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    valley_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(money_valley), 0) AS avg_valley,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream
GROUP BY valley_group
ORDER BY valley_group;
```

### 2.3 按银子峰值分组

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：银子峰值分组 + D1 目标日期常量
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'Z: 无对局'
            WHEN s.money_peak <= 5000   THEN 'A: 低峰值(≤5k)'
            WHEN s.money_peak <= 20000  THEN 'B: 中峰值(5k-2w)'
            WHEN s.money_peak <= 100000 THEN 'C: 高峰值(2w-10w)'
            ELSE                             'D: 极高峰值(>10w)'
        END AS peak_group,
        COALESCE(s.game_count, 0) AS game_count,
        COALESCE(s.money_peak, 0) AS money_peak,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    LEFT JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.peak_group, p.game_count, p.money_peak,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.peak_group, p.game_count, p.money_peak
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    peak_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(money_peak), 0) AS avg_peak,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY peak_group
ORDER BY peak_group;
```

### 2.4 按入场资本（start_money）分组

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：入场资本分组（INNER JOIN 仅含有对局用户）+ D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.start_money <= 3000   THEN 'A: 极低(≤3k)'
            WHEN s.start_money <= 10000  THEN 'B: 低(3k-1w)'
            WHEN s.start_money <= 50000  THEN 'C: 中(1w-5w)'
            WHEN s.start_money <= 200000 THEN 'D: 高(5w-20w)'
            ELSE                              'E: 极高(>20w)'
        END AS start_money_group,
        s.game_count, s.total_diff_money,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.start_money_group, p.game_count, p.total_diff_money,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.start_money_group, p.game_count, p.total_diff_money
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    start_money_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY start_money_group
ORDER BY start_money_group;
```

---

## 三、破产分析

> 核心问题：破产如何驱动流失？破产后用户是否还有挽回机会？

### 3.1 破产定义与破产率

```sql
-- 破产定义：银子谷值 <= 最低房间门槛（以 room_currency_lower 为参考）
-- 此处以 money_valley <= 1000 作为近似破产阈值
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：破产状态（INNER JOIN 仅含有对局用户）+ D1/D7/D30 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.money_valley <= 1000 THEN 'A: 破产(≤1k)'
            ELSE                             'B: 未破产'
        END AS bankrupt_status,
        s.game_count, s.total_diff_money, s.money_valley,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY)  AS d1_target,
        DATE_ADD(r.reg_date, INTERVAL 6 DAY)  AS d7_target,
        DATE_ADD(r.reg_date, INTERVAL 29 DAY) AS d30_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.bankrupt_status, p.game_count, p.total_diff_money, p.money_valley,
        MAX(CASE WHEN a.dt = p.d1_target  THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d7_target  THEN 1 ELSE 0 END) AS is_d7,
        MAX(CASE WHEN a.dt = p.d30_target THEN 1 ELSE 0 END) AS is_d30
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d7_target, p.d30_target)
    GROUP BY p.uid, p.bankrupt_status, p.game_count, p.total_diff_money, p.money_valley
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    bankrupt_status,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(AVG(money_valley), 0) AS avg_valley,
    ROUND(SUM(is_d1)  * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7)  * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(SUM(is_d30) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day30_rate
FROM all_events_stream
GROUP BY bankrupt_status
ORDER BY bankrupt_status;
```

### 3.2 破产率与对局数的关系

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：对局数分组 + 破产标记（INNER JOIN 仅含有对局用户）
    SELECT
        r.uid,
        CASE
            WHEN s.game_count = 1               THEN 'A: 1局'
            WHEN s.game_count BETWEEN 2 AND 5   THEN 'B: 2-5局'
            WHEN s.game_count BETWEEN 6 AND 10  THEN 'C: 6-10局'
            WHEN s.game_count BETWEEN 11 AND 20 THEN 'D: 11-20局'
            ELSE                                     'E: 20局+'
        END AS game_count_group,
        s.game_count, s.money_valley,
        CASE WHEN s.money_valley <= 1000 THEN 1 ELSE 0 END AS is_bankrupt
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    game_count_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_bankrupt) * 100.0 / NULLIF(COUNT(*), 0), 2) AS bankrupt_rate,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(money_valley), 0) AS avg_valley
FROM user_profile_tags
GROUP BY game_count_group
ORDER BY game_count_group;
```

### 3.3 破产 × 留存交叉分析

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：破产分组 × 对局数分组（INNER JOIN 仅含有对局用户）+ D1/D7 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
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
        s.win_rate,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(r.reg_date, INTERVAL 6 DAY) AS d7_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.valley_group, p.game_count_group, p.win_rate,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d7_target)
    GROUP BY p.uid, p.valley_group, p.game_count_group, p.win_rate
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    valley_group,
    game_count_group,
    COUNT(*) AS user_count,
    ROUND(AVG(win_rate), 2) AS avg_win_rate,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream
GROUP BY valley_group, game_count_group
ORDER BY valley_group, game_count_group;
```

---

## 四、金流波动与轨迹分析

> 核心问题：盈亏波动幅度是否比盈亏方向更重要？

### 4.1 金流波动幅度（peak-valley 差）

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：波动幅度分组（INNER JOIN 仅含有对局用户）+ D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        (s.money_peak - s.money_valley) AS volatility,
        s.game_count,
        CASE
            WHEN (s.money_peak - s.money_valley) <= 1000   THEN 'A: 极低波动(≤1k)'
            WHEN (s.money_peak - s.money_valley) <= 5000   THEN 'B: 低波动(1k-5k)'
            WHEN (s.money_peak - s.money_valley) <= 20000  THEN 'C: 中波动(5k-2w)'
            WHEN (s.money_peak - s.money_valley) <= 100000 THEN 'D: 高波动(2w-10w)'
            ELSE                                                'E: 极高波动(>10w)'
        END AS volatility_group,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.volatility_group, p.volatility, p.game_count,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.volatility_group, p.volatility, p.game_count
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    volatility_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(volatility), 0) AS avg_volatility,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY volatility_group
ORDER BY volatility_group;
```

### 4.2 服务费负担分析

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：服务费负担分组（INNER JOIN，限 start_money>0）+ D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        s.total_fee_paid * 100.0 / NULLIF(s.start_money, 0) AS fee_burden_pct,
        s.game_count,
        CASE
            WHEN s.total_fee_paid * 100.0 / NULLIF(s.start_money, 0) <= 1   THEN 'A: 极轻(<1%)'
            WHEN s.total_fee_paid * 100.0 / NULLIF(s.start_money, 0) <= 5   THEN 'B: 轻(1-5%)'
            WHEN s.total_fee_paid * 100.0 / NULLIF(s.start_money, 0) <= 15  THEN 'C: 中(5-15%)'
            WHEN s.total_fee_paid * 100.0 / NULLIF(s.start_money, 0) <= 30  THEN 'D: 重(15-30%)'
            ELSE                                                   'E: 极重(>30%)'
        END AS fee_burden_group,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count > 0 AND s.start_money > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.fee_burden_group, p.fee_burden_pct, p.game_count,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.fee_burden_group, p.fee_burden_pct, p.game_count
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    fee_burden_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(fee_burden_pct), 2) AS avg_fee_pct,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY fee_burden_group
ORDER BY fee_burden_group;
```

### 4.3 盈亏方向与留存（正收益 vs 负收益）

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：盈亏方向 × 对局数分组（INNER JOIN）+ D1/D7 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.total_diff_money >= 0 THEN 'A: 正收益'
            ELSE                              'B: 负收益'
        END AS profit_direction,
        CASE
            WHEN s.game_count BETWEEN 1 AND 3   THEN '1-3局'
            WHEN s.game_count BETWEEN 4 AND 10  THEN '4-10局'
            ELSE                                      '10局+'
        END AS game_count_group,
        s.total_diff_money, s.win_rate,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(r.reg_date, INTERVAL 6 DAY) AS d7_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.profit_direction, p.game_count_group, p.total_diff_money, p.win_rate,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d7_target)
    GROUP BY p.uid, p.profit_direction, p.game_count_group, p.total_diff_money, p.win_rate
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    profit_direction,
    game_count_group,
    COUNT(*) AS user_count,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(AVG(win_rate), 2) AS avg_win_rate,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream
GROUP BY profit_direction, game_count_group
ORDER BY profit_direction, game_count_group;
```

---

## 五、投入度 × 金流交叉

> 核心问题：对局数与盈亏如何交互影响留存？

### 5.1 game_count × total_diff_money 留存热力图

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：对局数 × 净输赢双维度（INNER JOIN）+ D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
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
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.game_count_group, p.money_group,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.game_count_group, p.money_group
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    game_count_group,
    money_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY game_count_group, money_group
ORDER BY game_count_group, money_group;
```

### 5.2 win_rate × money_valley 交叉（"连败破产"信号）

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：胜率 × 谷值双维度（INNER JOIN，限 game_count>=3）+ D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
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
        s.max_lose_streak,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count >= 3
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.win_rate_group, p.valley_group, p.max_lose_streak,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.win_rate_group, p.valley_group, p.max_lose_streak
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    win_rate_group,
    valley_group,
    COUNT(*) AS user_count,
    ROUND(AVG(max_lose_streak), 1) AS avg_lose_streak,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY win_rate_group, valley_group
ORDER BY win_rate_group, valley_group;
```

### 5.3 max_lose_streak × money_valley（"上头"信号）

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：连败长度 × 谷值双维度（INNER JOIN）+ D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
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
        s.total_diff_money,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.lose_streak_group, p.valley_group, p.total_diff_money,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.lose_streak_group, p.valley_group, p.total_diff_money
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    lose_streak_group,
    valley_group,
    COUNT(*) AS user_count,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY lose_streak_group, valley_group
ORDER BY lose_streak_group, valley_group;
```

---

## 六、510K 金流专项

> 核心问题：510K 多轮结算的银子经济特征是否与经典斗地主不同？

### 6.1 510K 玩家 vs 纯经典玩家金流对比

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_mode_flag AS (
    -- 2. 判定首日玩法归属（510K / 经典系），dt 范围裁剪
    SELECT a.uid, a.dt,
        MAX(CASE WHEN a.play_mode = 7 THEN 1 ELSE 0 END) AS has_510k,
        MAX(CASE WHEN a.play_mode IN (1, 2, 3) THEN 1 ELSE 0 END) AS has_classic
    FROM tcy_temp.dws_app_allgame_stat a
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND a.play_mode IN (1, 2, 3, 7)
    GROUP BY a.uid, a.dt
),
user_profile_tags AS (
    -- 3. 标签固化：玩法归属分组 + 银子金流指标 + D1/D7 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN m.has_510k = 1 AND m.has_classic = 1 THEN 'A: 双修(510K+经典)'
            WHEN m.has_510k = 1                       THEN 'B: 仅510K'
            WHEN m.has_classic = 1                    THEN 'C: 仅经典'
            ELSE                                           'D: 无银子对局'
        END AS mode_type,
        COALESCE(s.game_count, 0) AS game_count,
        COALESCE(s.total_diff_money, 0) AS total_diff_money,
        COALESCE(s.money_valley, 0) AS money_valley,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(r.reg_date, INTERVAL 6 DAY) AS d7_target
    FROM reg_base_raw r
    LEFT JOIN user_mode_flag m ON r.uid = m.uid AND r.reg_date = m.dt
    LEFT JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
),
all_events_stream AS (
    -- 4. 矩阵坍缩
    SELECT
        p.uid, p.mode_type, p.game_count, p.total_diff_money, p.money_valley,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d7_target)
    GROUP BY p.uid, p.mode_type, p.game_count, p.total_diff_money, p.money_valley
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    mode_type,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(AVG(money_valley), 0) AS avg_valley,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream
GROUP BY mode_type
ORDER BY mode_type;
```

### 6.2 510K 多轮结算的金流波动

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：510K（play_mode=7）结算轮数分组 + D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN a.avg_settle_rounds <= 3  THEN 'A: 短(≤3轮)'
            WHEN a.avg_settle_rounds <= 6  THEN 'B: 中(4-6轮)'
            WHEN a.avg_settle_rounds <= 10 THEN 'C: 长(7-10轮)'
            ELSE                                'D: 极长(>10轮)'
        END AS round_group,
        a.avg_settle_rounds, a.game_count, a.total_diff_money, a.outcome_gdp,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat a
        ON a.app_id = r.app_id AND a.uid = r.uid AND a.dt = r.reg_date
        AND a.play_mode = 7 AND a.game_count > 0
        AND a.dt BETWEEN '2026-02-10' AND '2026-06-15'
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.round_group, p.avg_settle_rounds, p.game_count, p.total_diff_money, p.outcome_gdp,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.round_group, p.avg_settle_rounds, p.game_count, p.total_diff_money, p.outcome_gdp
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    round_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(avg_settle_rounds), 1) AS avg_rounds,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(AVG(outcome_gdp), 0) AS avg_gdp,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY round_group
ORDER BY round_group;
```

---

## 七、高危经济信号组合

> 核心问题：哪些金流组合是最高危的流失信号？

### 7.1 连败 × 破产 × 高倍输 三维交叉

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
high_multi_loss AS (
    -- 2. 高倍 >24x 输局数 = 5 个高倍区间 lose 之和（表 v1.2 固定倍数段，multi_q4_losses 已废弃）
    SELECT a.uid, a.dt,
           SUM(
               COALESCE(a.multi_24_48_lose, 0) + COALESCE(a.multi_48_96_lose, 0)
             + COALESCE(a.multi_96_192_lose, 0) + COALESCE(a.multi_192_384_lose, 0)
             + COALESCE(a.multi_384_plus_lose, 0)
           ) AS total_q4_losses
    FROM tcy_temp.dws_app_allgame_stat a
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN '2026-02-10' AND '2026-06-15'
      AND a.play_mode IN (1, 2, 3, 7)
    GROUP BY a.uid, a.dt
),
user_profile_tags AS (
    -- 3. 标签固化：连败×破产×高倍输 三维风险组合（INNER JOIN silver）+ D1/D7 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
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
        s.game_count, s.total_diff_money,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(r.reg_date, INTERVAL 6 DAY) AS d7_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
        AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
    LEFT JOIN high_multi_loss h ON r.uid = h.uid AND r.reg_date = h.dt
),
all_events_stream AS (
    -- 4. 矩阵坍缩
    SELECT
        p.uid, p.risk_group, p.game_count, p.total_diff_money,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d7_target)
    GROUP BY p.uid, p.risk_group, p.game_count, p.total_diff_money
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    risk_group,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(total_diff_money), 0) AS avg_diff,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream
GROUP BY risk_group
ORDER BY risk_group;
```

### 7.2 金流轨迹异常检测

```sql
-- 识别「先赢后输」型用户：曾经 peak 很高但最终亏损
WITH reg_base_raw AS (
    -- 1. 基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 标签固化：金流轨迹分组（INNER JOIN，限 game_count>=5）+ D1 目标日期
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.money_peak > s.start_money AND s.end_money < s.start_money
                THEN 'A: 先赢后输（过山车）'
            WHEN s.money_peak <= s.start_money AND s.end_money < s.start_money
                THEN 'B: 一路亏损'
            WHEN s.end_money >= s.start_money
                THEN 'C: 最终盈利'
            ELSE 'D: 持平'
        END AS trajectory,
        s.game_count, s.start_money, s.money_peak, s.end_money,
        DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count >= 5
        AND s.dt BETWEEN '2026-02-10' AND '2026-06-15'
),
all_events_stream AS (
    -- 3. 矩阵坍缩
    SELECT
        p.uid, p.trajectory, p.game_count, p.start_money, p.money_peak, p.end_money,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt = p.d1_target
    GROUP BY p.uid, p.trajectory, p.game_count, p.start_money, p.money_peak, p.end_money
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    trajectory,
    COUNT(*) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(money_peak - start_money), 0) AS avg_max_gain,
    ROUND(AVG(end_money - money_peak), 0) AS avg_giveback,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY trajectory
ORDER BY trajectory;
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
