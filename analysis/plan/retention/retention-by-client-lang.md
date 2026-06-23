# 分客户端层留存分析：稳定性问题专项

> 本文档聚焦**分客户端层**留存分析，对比 Cocos-Lua 与 Cocos-Creator 两个客户端版本的留存差异，重点识别稳定性问题信号。全局层分析见 [retention-global.md](retention-global.md)。
>
> **分析时间段**：2026-02-10 至 2026-06-15
> **留存口径**：登录留存 + 游戏留存双重口径

---

## 目录

1. [客户端版本映射](#一客户端版本映射)
2. [版本留存对比](#二版本留存对比)
3. [稳定性信号识别](#三稳定性信号识别)
4. [版本特有分析](#四版本特有分析)
5. [专项分析索引](#五专项分析索引)

---

## 一、客户端版本映射

### 1.1 app_code 与平台映射

```sql
-- 客户端版本映射
CASE r.reg_app_code
    WHEN 'zgda' THEN 'Cocos-Lua'
    WHEN 'zgdx' THEN 'Cocos-Creator'
    ELSE '其他'
END AS client_lang

-- 平台映射
CASE
    WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
    WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
    ELSE '其他'
END AS platform
```

### 1.2 字段来源说明

| 字段 | 来源表 | 说明 |
| ---- | ------ | ---- |
| `reg_app_code` | `dws_dq_app_daily_reg` | 用户**注册时**使用的客户端版本 |
| `reg_group_id` | `dws_dq_app_daily_reg` | 用户注册时的设备平台分组 |
| `first_day_login_cnt` | `dws_dq_app_daily_reg` | 注册当日登录次数，稳定性代理指标 |
| `first_app_code` | `dws_dq_daily_login` | 用户**当日首次登录**使用的客户端版本 |
| `is_game_active` | `dws_app_game_active` | 当日是否有任意玩法对局活跃 |
| `game_count` | `dws_app_silvergame_stat` | 银子玩法当日对局数 |
| `escape_count` | `dws_app_silvergame_stat` | 银子玩法当日逃跑次数 |
| `avg_game_seconds` | `dws_app_silvergame_stat` | 银子玩法平均对局时长（秒） |

### 1.3 关键说明

- 游戏留存口径使用 `dws_app_game_active`（任意玩法有对局即活跃）
- 游戏行为指标使用 `dws_app_silvergame_stat`（替代已下线的 `dws_ddz_app_game_stat`）

> **💡 性能建议（适用全文 SQL）**：本文 SQL 直接 `FROM dws_dq_app_daily_reg r` + `LEFT JOIN` 活跃事实表，JOIN 上的 `login_date IN (DATE_ADD(...))` / `dt IN (DATE_ADD(...))` 是 per-row 计算。建议先抽 `reg_base` + `date_bounds` CTE，再在活跃 JOIN 上追加 `AND <日期列> BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)` 做分区裁剪。通用模板：
>
> ```sql
> WITH reg_base AS (
>     SELECT uid, reg_date, app_id, reg_app_code, reg_group_id
>     FROM tcy_temp.dws_dq_app_daily_reg
>     WHERE app_id = 1880053
>       AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
> ),
> date_bounds AS (
>     SELECT
>         DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
>         DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
>     FROM reg_base
> )
> ```
>
> §2.1 给出完整示范，其余 SQL 同构套用。

---

## 二、版本留存对比

> **核心问题**：Cocos-Lua 与 Cocos-Creator 两个客户端版本之间的留存是否存在系统性差异？

### 2.1 按客户端版本登录留存

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 2 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day2_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 3 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day3_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 13 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day14_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 29 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day30_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (
        DATE_ADD(r.reg_date, INTERVAL 1 DAY),
        DATE_ADD(r.reg_date, INTERVAL 2 DAY),
        DATE_ADD(r.reg_date, INTERVAL 3 DAY),
        DATE_ADD(r.reg_date, INTERVAL 6 DAY),
        DATE_ADD(r.reg_date, INTERVAL 13 DAY),
        DATE_ADD(r.reg_date, INTERVAL 29 DAY)
    )
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY client_lang;
```

### 2.2 按客户端版本 x 平台登录留存

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code, reg_group_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
        WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END AS platform,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 29 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day30_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (
        DATE_ADD(r.reg_date, INTERVAL 1 DAY),
        DATE_ADD(r.reg_date, INTERVAL 6 DAY),
        DATE_ADD(r.reg_date, INTERVAL 29 DAY)
    )
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY client_lang, platform;
```

### 2.3 按客户端版本游戏留存（使用 dws_app_game_active）

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN ga.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) AND ga.is_game_active = 1
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS game_day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN ga.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY) AND ga.is_game_active = 1
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS game_day7_rate,
    ROUND(COUNT(DISTINCT CASE WHEN ga.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) AND ga.is_game_active = 1
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS game_day30_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_game_active ga
    ON ga.app_id = r.app_id AND ga.uid = r.uid
    AND ga.dt IN (
        DATE_ADD(r.reg_date, INTERVAL 1 DAY),
        DATE_ADD(r.reg_date, INTERVAL 6 DAY),
        DATE_ADD(r.reg_date, INTERVAL 29 DAY)
    )
    AND ga.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY client_lang;
```

---

## 三、稳定性信号识别

> **核心问题**：客户端是否存在闪退 / 掉线 / 卡顿 / 操作卡顿问题？

### 3.1 首日登录次数分布（稳定性代理指标）

> 多次登录（>=3 次）可能反映：闪退、掉线后重连、进程被杀等稳定性问题。`first_day_login_cnt` 来自 `dws_dq_app_daily_reg` 预聚合字段。

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1次（正常）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5次（可疑）'
        WHEN r.first_day_login_cnt >= 6 THEN 'D: 6次以上（异常）'
        ELSE 'Z: 未知'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT r.uid) * 100.0 / SUM(COUNT(DISTINCT r.uid)) OVER (PARTITION BY r.reg_app_code), 2) AS pct_in_client,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt
FROM tcy_temp.dws_dq_app_daily_reg r
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
GROUP BY 1, 2
ORDER BY client_lang, login_cnt_group;
```

### 3.2 按客户端 x 登录次数分组的留存

> 验证：多次登录组的留存是否显著低于单次登录组？如果是，则稳定性问题直接影响用户留存。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code, first_day_login_cnt
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1次（正常）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5次（可疑）'
        WHEN r.first_day_login_cnt >= 6 THEN 'D: 6次以上（异常）'
        ELSE 'Z: 未知'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY client_lang, login_cnt_group;
```

### 3.3 按客户端版本的逃跑率（检测操作体验问题）

> 使用 `dws_app_silvergame_stat.escape_count` 统计首日逃跑行为。逃跑率高可能反映操作卡顿、闪退导致掉线等客户端问题。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN s.escape_count IS NULL OR s.escape_count = 0 THEN 'A: 无逃跑'
        WHEN s.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN s.escape_count = 2 THEN 'C: 逃跑2次'
        ELSE 'D: 逃跑3次以上'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN s.escape_count IS NOT NULL AND s.escape_count > 0
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS escape_rate_pct,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY client_lang, escape_group;
```

### 3.4 按客户端版本的对局时长（检测渲染 / 网络卡顿）

> 使用 `dws_app_silvergame_stat.avg_game_seconds` 分析平均对局时长。异常短的对局（<30s）可能反映闪退或断线，异常长的对局可能反映网络卡顿导致超时。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN s.avg_game_seconds < 30 THEN 'A: <30s（异常短）'
        WHEN s.avg_game_seconds < 90 THEN 'B: 30-90s'
        WHEN s.avg_game_seconds < 180 THEN 'C: 90-180s'
        WHEN s.avg_game_seconds < 300 THEN 'D: 180-300s'
        ELSE 'E: 300s以上（异常长）'
    END AS duration_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(s.avg_game_seconds), 1) AS avg_duration_seconds,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_base r
INNER JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count > 0
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY client_lang, duration_group;
```

### 3.5 按客户端 x 平台的无对局率

> "注册后无对局"可能是稳定性问题的极端表现（闪退导致无法进入游戏）。计算各版本 x 平台组合中当日 game_count = 0 或无游戏活跃记录的用户比例。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code, reg_group_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
        WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END AS platform,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE
              WHEN ga.dt = r.reg_date AND (ga.is_game_active IS NULL OR ga.is_game_active = 0)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS no_game_active_pct,
    ROUND(COUNT(DISTINCT CASE
              WHEN s.game_count IS NULL OR s.game_count = 0
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS no_silvergame_pct,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_game_active ga
    ON ga.app_id = r.app_id AND ga.uid = r.uid AND ga.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY client_lang, platform;
```

---

## 四、版本特有分析

### 4.1 版本切换行为分析

> **核心问题**：用户注册时使用 A 版本，但首次登录时是否切换到了 B 版本？版本切换率高可能意味着用户主动卸载 / 更新了客户端，或渠道分发了错误的包体。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS reg_client_lang,
    CASE
        WHEN login1.first_app_code IS NULL THEN 'X: 首日无登录'
        WHEN login1.first_app_code = r.reg_app_code THEN 'A: 版本未切换'
        ELSE 'B: 版本已切换'
    END AS switch_status,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_dq_daily_login login1
    ON login1.app_id = r.app_id AND login1.uid = r.uid AND login1.login_date = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY reg_client_lang, switch_status;
```

### 4.2 首日对局数分布（游戏参与度对比）

> 使用 `dws_app_silvergame_stat.game_count` 分析两个客户端版本的首日对局参与度差异。如果某版本 "0局" 占比显著更高，说明该版本可能存在稳定性或兼容性问题。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'A: 0局'
        WHEN s.game_count = 1 THEN 'B: 1局'
        WHEN s.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
        WHEN s.game_count BETWEEN 6 AND 10 THEN 'D: 6-10局'
        ELSE 'E: 10局以上'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT r.uid) * 100.0 / SUM(COUNT(DISTINCT r.uid)) OVER (PARTITION BY r.reg_app_code), 2) AS pct_in_client,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY client_lang, game_count_group;
```

---

## 五、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数 / 胜率 / 经济差异 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 1局用户、客户端问题、渠道质量 |
| [retention-analysis-framework.md](retention-analysis-framework.md) | 分析框架 | 视角与指标速查 |

---

> **文档版本**：v2.0
> **创建时间**：2026-06-15
> **更新说明**：
>
> - v2.0：基于 2026-06-11 重构后数仓表全面更新。移除已废弃的 `is_login_log_missing` 过滤条件。移除已下线的 `dws_ddz_app_game_stat`，游戏留存改用 `dws_app_game_active`，游戏行为指标改用 `dws_app_silvergame_stat`。新增游戏留存口径（2.3）、游戏时长分析（3.4）、无对局率分析（3.5）。分析时间段延长至 2026-06-15。