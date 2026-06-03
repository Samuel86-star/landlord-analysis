# 全局层留存分析：用户属性与核心指标

> 本文档聚焦**全局层**留存分析，覆盖用户属性视角和投入度视角的核心指标。分玩法/分客户端的专项分析见对应文档。
>
> **分析时间段**：2026-02-10 至 2026-05-10
> **留存口径**：新增用户留存（分母为当日注册APP端用户，分子为第N日登录用户）

---

## 目录

1. [数据基础](#一数据基础)
2. [用户属性视角](#二用户属性视角)
3. [高相关性指标](#三高相关性指标)
4. [中等相关性指标](#四中等相关性指标)
5. [高危信号组合](#五高危信号组合)
6. [专项分析索引](#六专项分析索引)

---

## 一、数据基础

### 1.1 核心数据表

| 表名 | 说明 | 关键字段 |
| ---- | ---- | ---- |
| `dws_dq_app_daily_reg` | APP端注册用户宽表 | `reg_app_code`, `reg_group_id`, `channel_category_name`, `first_day_login_cnt` |
| `dws_dq_daily_login` | 每日登录聚合表 | `login_date`, `login_count` |
| `dws_ddz_app_game_stat` | 用户每日行为聚合 | `game_count`, `win_rate`, `avg_magnification`, `total_diff_money`, `escape_count` |
| `dws_ddz_firstday_game` | 首日对局明细 | `magnification`, `result_id`, `role`, `room_base`, `start_money`, `end_money` |

### 1.2 留存计算公式

```sql
-- 次留（Day1）
COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN r.uid END)
  * 100.0 / COUNT(DISTINCT r.uid)

-- 7留（Day7）
COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY) THEN r.uid END)
  * 100.0 / COUNT(DISTINCT r.uid)
```

---

## 二、用户属性视角

> 核心问题："谁"更容易留存/流失？

### 2.1 按渠道分类留存

```sql
SELECT
    CASE WHEN r.channel_category_name IN ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    r.reg_date,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1, 2
ORDER BY channel, r.reg_date DESC;
```

### 2.2 按设备类型留存

```sql
SELECT
    CASE WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
         WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
         ELSE '其他' END AS platform,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1;
```

### 2.3 按客户端版本留存

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua'
                        WHEN 'zgdx' THEN 'Cocos-Creator'
                        ELSE '其他' END AS client_lang,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1;
```

---

## 三、高相关性指标

> 核心问题：哪些指标直接影响留存？

### 3.1 首日对局数（投入度核心指标）

```sql
SELECT
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'A: 0局'
        WHEN g.game_count = 1 THEN 'B: 1局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN 'D: 6-10局'
        ELSE 'E: 10局+'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

### 3.2 首日胜率（胜负情绪线）

```sql
SELECT
    CASE
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.game_count), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
  AND g.game_count > 0
GROUP BY 1
ORDER BY 1;
```

### 3.3 连败长度（关键流失预警）

```sql
SELECT
    CASE
        WHEN g.max_lose_streak = 0 THEN 'A: 无连败'
        WHEN g.max_lose_streak <= 2 THEN 'B: 1-2连败'
        WHEN g.max_lose_streak <= 5 THEN 'C: 3-5连败'
        ELSE 'D: 5+连败'
    END AS lose_streak_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

### 3.4 银子净变化（经济压力线）

```sql
SELECT
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.total_diff_money < -50000 THEN 'A: 巨亏(<-5万)'
        WHEN g.total_diff_money < -10000 THEN 'B: 大亏(-5万~-1万)'
        WHEN g.total_diff_money < 0 THEN 'C: 小亏(-1万~0)'
        WHEN g.total_diff_money < 10000 THEN 'D: 小赚(0~1万)'
        WHEN g.total_diff_money < 50000 THEN 'E: 大赚(1万~5万)'
        ELSE 'F: 巨赚(>5万)'
    END AS money_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

### 3.5 破产状态

```sql
SELECT
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'C: 无对局'
        WHEN g.money_valley <= 1000 THEN 'A: 疑似破产(≤1000)'
        ELSE 'B: 未破产'
    END AS bankrupt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

### 3.6 高倍局输赢

```sql
SELECT
    CASE
        WHEN g.high_multi_games = 0 OR g.high_multi_games IS NULL THEN 'A: 未经历高倍'
        WHEN g.high_multi_wins > 0 AND g.high_multi_losses = 0 THEN 'B: 仅赢高倍'
        WHEN g.high_multi_wins = 0 AND g.high_multi_losses > 0 THEN 'C: 仅输高倍'
        ELSE 'D: 有赢有输'
    END AS high_multi_exp,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

---

## 四、中等相关性指标

### 4.1 首日平均倍数

```sql
SELECT
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN '0: 无对局'
        WHEN g.avg_magnification <= 6 THEN 'A: ≤6'
        WHEN g.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN g.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE 'D: 24+'
    END AS multi_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

### 4.2 首局胜负

```sql
-- 首局胜负对留存的影响
WITH first_game AS (
    SELECT
        g.uid, g.dt,
        MIN_BY(g.result_id, g.game_datetime) AS first_game_result
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
      AND g.robot != 1
      AND g.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND g.play_mode IN (1, 2, 3, 5)
    GROUP BY g.uid, g.dt
)
SELECT
    CASE
        WHEN fg.first_game_result = 1 THEN 'A: 首局胜'
        WHEN fg.first_game_result = 2 THEN 'B: 首局负'
        ELSE 'C: 无对局'
    END AS first_game_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN first_game fg ON r.uid = fg.uid AND r.reg_date = fg.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

### 4.3 角色扮演（地主/农民）胜率

```sql
SELECT
    CASE 
        WHEN g.landlord_count = 0 THEN 'A: 纯农民'
        WHEN g.landlord_count * 1.0 / g.game_count < 0.3 THEN 'B: 偏好农民(<30%)'
        WHEN g.landlord_count * 1.0 / g.game_count < 0.6 THEN 'C: 均衡(30-60%)'
        ELSE 'D: 偏好地主(≥60%)'
    END AS role_preference,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN (
    SELECT app_id, uid, dt, 
           COUNT(*) AS game_count,
           SUM(CASE WHEN role = 1 THEN 1 ELSE 0 END) AS landlord_count
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE robot != 1 AND play_mode IN (1, 2, 3)
    GROUP BY app_id, uid, dt
) g ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
  AND g.game_count > 0
GROUP BY 1
ORDER BY 1;
```

### 4.4 逃跑行为

```sql
SELECT
    CASE
        WHEN g.escape_count IS NULL OR g.escape_count = 0 THEN 'A: 无逃跑'
        WHEN g.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN g.escape_count = 2 THEN 'C: 逃跑2次'
        ELSE 'D: 逃跑3+次'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
  AND g.game_count > 0
GROUP BY 1
ORDER BY 1;
```

### 4.5 房间底注体验

```sql
SELECT
    CASE 
        WHEN g.avg_room_base <= 50 THEN 'A: 极低底注(≤50)'
        WHEN g.avg_room_base <= 200 THEN 'B: 低底注(51-200)'
        WHEN g.avg_room_base <= 1000 THEN 'C: 中底注(201-1000)'
        ELSE 'D: 高底注(>1000)'
    END AS room_base_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN (
    SELECT app_id, uid, dt, AVG(room_base) AS avg_room_base
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE robot != 1 AND play_mode IN (1, 2, 3)
    GROUP BY app_id, uid, dt
) g ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
  AND g.avg_room_base IS NOT NULL
GROUP BY 1
ORDER BY 1;
```

---

## 五、高危信号组合

> 多维交叉识别最高危流失组合

```sql
-- 高危信号组合：连败×破产×高倍输
SELECT
    CASE
        WHEN g.max_lose_streak >= 3 AND g.total_diff_money < -10000 THEN 'A: 连败3+×大亏'
        WHEN g.max_lose_streak >= 3 THEN 'B: 连败3+'
        WHEN g.total_diff_money < -10000 THEN 'C: 大亏'
        ELSE 'D: 正常'
    END AS risk_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY 1;
```

---

## 六、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| ---- | ---- | ---- |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数/胜率/经济差异 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 1局用户、客户端问题、渠道质量 |

> **分析框架速查**：[retention-analysis-framework.md](retention-analysis-framework.md)
> **历史结论报告**：已备份至 [archive/retention-analysis-2026-04-22.md](archive/retention-analysis-2026-04-22.md)
