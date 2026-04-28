# 数据分析常用 SQL 手册

> 本文档汇总日常数据分析中常用的 SQL 查询，供快速参考使用。

---

## 目录

1. [注册用户首日游戏情况查询](#1-注册用户首日游戏情况查询)
2. [首局对局胜率与倍数统计](#2-首局对局胜率与倍数统计)
3. [首日游戏局数分布统计](#3-首日游戏局数分布统计)

---

## 1. 注册用户首日游戏情况查询

### 查询用途

查询指定时间段内注册用户的首日游戏情况，区分"已游戏"和"未游戏"用户，并显示首局对局明细。

### SQL 查询语句

```sql
SELECT 
    t.reg_date,
    t.uid,
    t.reg_datetime,
    -- 通过判断对局表的某个必填字段（如 game_datetime）是否为空，来识别"未游戏"用户
    IF(t.game_datetime IS NULL, '未游戏', '已游戏') AS play_status,
    t.* -- 包含对局表的明细
FROM (
    SELECT 
        r.reg_date,
        r.reg_datetime,
        r.uid,
        g.game_datetime,
        g.play_mode,
        -- 注意：ROW_NUMBER 对 LEFT JOIN 产生的 NULL 行也会标记为 1
        ROW_NUMBER() OVER (PARTITION BY r.uid ORDER BY g.game_datetime ASC) as rank_num
    FROM tcy_temp.dws_dq_app_daily_reg r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g 
        ON r.app_id = g.app_id
        AND r.uid = g.uid 
        AND r.reg_date = g.dt 
        AND g.play_mode = 1 -- 这里的条件放在 JOIN 里，确保没玩该模式的人也能留存
    WHERE r.app_id = 1880053
      AND r.reg_datetime BETWEEN '2026-04-27 18:00:00' AND '2026-04-27 23:59:59'
) t
WHERE t.rank_num = 1
ORDER BY t.reg_datetime DESC;
```

### 查询逻辑说明

- 通过 LEFT JOIN 确保未游戏的用户也能保留在结果中
- `play_mode = 1` 条件放在 JOIN 里，用于筛选特定玩法（经典模式），未玩该模式的用户会显示为"未游戏"
- `ROW_NUMBER() OVER (PARTITION BY r.uid ORDER BY g.game_datetime ASC)` 用于获取每个用户的首局对局
- 对于未游戏的用户，LEFT JOIN 产生的 NULL 行也会被 ROW_NUMBER 标记为 1，因此需要通过 `game_datetime IS NULL` 判断游戏状态

### 可调参数列表

| 参数 | 说明 | 示例 |
| ---- | ---- | ---- |
| `reg_datetime BETWEEN ...` | 注册时间段过滤 | `'2026-04-27 18:00:00' AND '2026-04-27 23:59:59'` |
| `play_mode` | 玩法类型过滤，放在 JOIN 条件中 | `1`（经典模式） |
| `app_id` | 应用 ID 过滤 | `1880053` |

### 玩法对照表

| play_mode | 玩法名称 |
| --------- | -------- |
| 1 | 经典 |
| 2 | 不洗牌 |
| 3 | 癞子 |
| 4 | 积分 |
| 5 | 比赛 |
| 6 | 好友房 |

---

## 2. 首局对局胜率与倍数统计

### 统计用途

按日期统计新用户首局对局的胜率、对局数和平均倍数，用于分析首局体验对用户行为的影响。

### 统计 SQL 语句

```sql
SELECT 
  dt,
  count(1) as play_cnt, 
  count(case when result_id = 1 then 1 end) as win_cnt,
  round(count(case when result_id = 1 then 1 end) / count(1), 4) as win_rate,
  round(avg(magnification), 2) as avg_magnification
FROM (
    SELECT 
        r.reg_date,
        r.uid,
        g.*,
        ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.game_datetime ASC) as rank_num
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_firstday_game g 
        ON r.app_id = g.app_id
        AND r.uid = g.uid 
        AND r.reg_date = g.dt 
    WHERE r.app_id = 1880053
      AND r.reg_datetime BETWEEN '2026-04-27 18:00:00' AND '2026-04-27 23:59:59'
      AND g.play_mode = 1 
) t
WHERE t.rank_num = 1
group by dt
order by dt desc;
```

### 统计逻辑说明

- 使用 INNER JOIN 仅统计有对局的用户
- `ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.game_datetime ASC)` 筛选每个用户当天的首局对局
- `result_id = 1` 表示获胜，`result_id = 2` 表示失败
- `magnification` 为对局倍数

### 输出字段含义

| 字段 | 说明 |
| ---- | ---- |
| `dt` | 对局日期 |
| `play_cnt` | 首局对局总数 |
| `win_cnt` | 首局获胜数 |
| `win_rate` | 首局胜率（0-1） |
| `avg_magnification` | 首局平均倍数 |

### 可调参数配置

| 参数 | 说明 | 示例 |
| ---- | ---- | ---- |
| `reg_datetime BETWEEN ...` | 注册时间段过滤 | `'2026-04-27 18:00:00' AND '2026-04-27 23:59:59'` |
| `play_mode` | 玩法类型过滤 | `1`（经典模式） |
| `app_id` | 应用 ID 过滤 | `1880053` |

---

## 3. 首日游戏局数分布统计

### 分析用途

按日期统计新用户首日游戏局数分布，分析用户首日活跃程度和流失风险。

### 分析 SQL 语句

```sql
WITH user_game_counts AS (
    SELECT r.reg_date, r.uid, COUNT(g.uid) AS daily_cnt 
    FROM tcy_temp.dws_dq_app_daily_reg r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g 
        ON r.uid = g.uid 
        AND r.app_id = g.app_id 
        AND r.reg_date = g.dt
    WHERE r.app_id = 1880053
      AND r.reg_datetime BETWEEN '2026-04-19' AND '2026-04-27'
      AND r.is_login_log_missing = 0
    GROUP BY r.reg_date, r.uid
),
daily_metrics AS (
    SELECT 
        reg_date,
        COUNT(1) AS total_users,
        COUNT(CASE WHEN daily_cnt = 0 THEN 1 END) AS count_0,
        COUNT(CASE WHEN daily_cnt = 1 THEN 1 END) AS count_1,
        COUNT(CASE WHEN daily_cnt = 2 THEN 1 END) AS count_2,
        COUNT(CASE WHEN daily_cnt BETWEEN 3 AND 5 THEN 1 END) AS count_3_5,
        COUNT(CASE WHEN daily_cnt BETWEEN 6 AND 10 THEN 1 END) AS count_6_10,
        COUNT(CASE WHEN daily_cnt >= 11 THEN 1 END) AS count_11_plus
    FROM user_game_counts
    GROUP BY reg_date
)
SELECT 
    reg_date,
    total_users AS "总注册人数",
    ROUND(count_0 * 100.0 / total_users, 2) AS "0局占比%",
    ROUND(count_1 * 100.0 / total_users, 2) AS "1局占比%",
    ROUND(count_2 * 100.0 / total_users, 2) AS "2局占比%",
    ROUND(count_3_5 * 100.0 / total_users, 2) AS "3-5局占比%",
    ROUND(count_6_10 * 100.0 / total_users, 2) AS "6-10局占比%",
    ROUND(count_11_plus * 100.0 / total_users, 2) AS "11局+占比%"
FROM daily_metrics
ORDER BY reg_date;
```

### 分析逻辑说明

- 使用 LEFT JOIN 确保未游戏的用户（0局）也能被统计
- `is_login_log_missing = 0` 排除登录日志丢失的用户，确保数据准确性
- 局数分段：0局、1局、2局、3-5局、6-10局、11局以上
- 输出各局数段的人数占比，便于分析首日用户活跃程度

### 输出字段含义

| 字段 | 说明 |
| ---- | ---- |
| `reg_date` | 注册日期 |
| `总注册人数` | 当日注册总人数 |
| `0局占比%` | 未玩游戏用户占比（高流失风险） |
| `1局占比%` | 仅玩1局用户占比 |
| `2局占比%` | 仅玩2局用户占比 |
| `3-5局占比%` | 中度活跃用户占比 |
| `6-10局占比%` | 高活跃用户占比 |
| `11局+占比%` | 超高活跃用户占比 |

### 可调参数说明

| 参数 | 说明 | 示例 |
| ---- | ---- | ---- |
| `reg_datetime BETWEEN ...` | 注册时间段过滤 | `'2026-04-19' AND '2026-04-27'` |
| `is_login_log_missing` | 登录日志完整性过滤 | `0`（排除日志丢失用户） |
| `app_id` | 应用 ID 过滤 | `1880053` |

---

> **文档版本**：v1.2
> **更新时间**：2026-04-28
> **维护说明**：如有新增常用 SQL，请及时更新本文档