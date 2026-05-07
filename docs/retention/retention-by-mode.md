# 分玩法层留存分析：玩法内因子对比

> 本文档聚焦**分玩法层**留存分析，对比经典/不洗牌/癞子三种玩法的留存规律差异。全局层分析见 [retention-global.md](retention-global.md)。
>
> **分析时间段**：2026-02-10 至 2026-04-22
> **留存口径**：玩法留存（分母为当日注册APP端用户，分子为第N日在同一玩法有对局用户）

---

## 目录

1. [玩法映射关系](#一玩法映射关系)
2. [玩法留存对比](#二玩法留存对比)
3. [玩法内因子分析](#三玩法内因子分析)
4. [玩法行为分析](#四玩法行为分析)

---

## 一、玩法映射关系

### 1.1 room_id → 玩法映射

```sql
CASE
    WHEN room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN 1 -- 经典
    WHEN room_id IN (421, 22039, 22040, 22041, 22042)                  THEN 2 -- 不洗牌
    WHEN room_id IN (13176, 13177, 13178)                              THEN 3 -- 癞子
    ELSE 0
END AS play_mode
```

### 1.2 玩法特点对比

| 玩法 | 倍数特点 | 预期影响 |
| ---- | -------- | -------- |
| 经典 | 标准倍数机制，新手默认进入 | 基线水平，最大用户群 |
| 不洗牌 | 牌序延续，连续好牌/差牌概率更高，倍数更极端 | 波动大，高倍局更频繁 |
| 癞子 | 万能牌存在，炸弹概率大幅增加，公共倍数普遍偏高 | 倍数天然更高，经济波动剧烈 |

---

## 二、玩法留存对比

### 2.1 各玩法新增用户留存率

```sql
SELECT
    CASE g.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        ELSE '其他'
    END AS play_mode_name,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
    AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY 1;
```

### 2.2 玩法参与分布

```sql
-- 主玩法判定：取用户对局数最多的玩法
SELECT
    CASE main_play_mode WHEN 1 THEN '经典' WHEN 2 THEN '不洗牌' WHEN 3 THEN '癞子' ELSE '其他' END AS main_play_mode,
    first_day_mode_count,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(SUM(is_ret_d1) * 100.0 / COUNT(DISTINCT uid), 2) AS day1_rate
FROM (
    SELECT uid, first_day_mode_count,
           MAX_BY(play_mode, game_count) AS main_play_mode,
           MAX(CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_ret_d1
    FROM tcy_temp.dws_dq_app_daily_reg r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
    GROUP BY uid, r.reg_date, first_day_mode_count
) t
GROUP BY main_play_mode, first_day_mode_count;
```

---

## 三、玩法内因子分析

### 3.1 分玩法 × 倍数分组留存

```sql
SELECT
    CASE g.play_mode WHEN 1 THEN '经典' WHEN 2 THEN '不洗牌' WHEN 3 THEN '癞子' ELSE '其他' END AS play_mode,
    CASE
        WHEN AVG(g.magnification) <= 6  THEN 'A: <=6'
        WHEN AVG(g.magnification) <= 12 THEN 'B: 6-12'
        WHEN AVG(g.magnification) <= 24 THEN 'C: 12-24'
        WHEN AVG(g.magnification) <= 48 THEN 'D: 24-48'
        ELSE                                  'E: 48+'
    END AS multi_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
    AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1, 2
ORDER BY play_mode, multi_group;
```

**对比要点**：
- 经典最优区间：12-24x
- 癞子是否右移到 24-48x？

### 3.2 分玩法 × 胜率分组留存

```sql
SELECT
    CASE g.play_mode WHEN 1 THEN '经典' WHEN 2 THEN '不洗牌' WHEN 3 THEN '癞子' ELSE '其他' END AS play_mode,
    CASE
        WHEN win_rate < 30 THEN 'A: <30%'
        WHEN win_rate < 50 THEN 'B: 30-50%'
        WHEN win_rate < 70 THEN 'C: 50-70%'
        ELSE                    'D: >=70%'
    END AS winrate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN (
    SELECT app_id, uid, dt, play_mode,
           SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*) AS win_rate
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE robot != 1 AND play_mode IN (1, 2, 3)
    GROUP BY app_id, uid, dt, play_mode
) g ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1, 2
ORDER BY play_mode, winrate_group;
```

**对比要点**：
- 癞子因随机性大，胜率对留存的影响是否弱于经典？

### 3.3 分玩法 × 对局数分组留存

```sql
SELECT
    CASE g.play_mode WHEN 1 THEN '经典' WHEN 2 THEN '不洗牌' WHEN 3 THEN '癞子' ELSE '其他' END AS play_mode,
    CASE
        WHEN game_count = 1   THEN 'A: 1局'
        WHEN game_count <= 3  THEN 'B: 2-3局'
        WHEN game_count <= 5  THEN 'C: 4-5局'
        WHEN game_count <= 10 THEN 'D: 6-10局'
        ELSE                       'E: 10局+'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN (
    SELECT app_id, uid, dt, play_mode, COUNT(*) AS game_count
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE robot != 1 AND play_mode IN (1, 2, 3)
    GROUP BY app_id, uid, dt, play_mode
) g ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1, 2
ORDER BY play_mode, game_count_group;
```

---

## 四、玩法行为分析

### 4.1 首局玩法选择与留存

```sql
SELECT
    CASE first_global_play_mode WHEN 1 THEN '经典' WHEN 2 THEN '不洗牌' WHEN 3 THEN '癞子' ELSE '其他' END AS first_mode,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM (
    SELECT r.uid, r.reg_date, r.app_id,
           MIN_BY(g.play_mode, g.time_unix) AS first_global_play_mode
    FROM tcy_temp.dws_dq_app_daily_reg r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
    WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
    GROUP BY r.uid, r.reg_date, r.app_id
) t
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = t.app_id AND l.uid = t.uid AND l.login_date = DATE_ADD(t.reg_date, INTERVAL 1 DAY)
GROUP BY first_global_play_mode;
```

### 4.2 玩法数量与留存

```sql
SELECT
    mode_count,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM (
    SELECT r.uid, r.reg_date, r.app_id, COUNT(DISTINCT g.play_mode) AS mode_count
    FROM tcy_temp.dws_dq_app_daily_reg r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
    WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
    GROUP BY r.uid, r.reg_date, r.app_id
) t
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = t.app_id AND l.uid = t.uid AND l.login_date = DATE_ADD(t.reg_date, INTERVAL 1 DAY)
GROUP BY mode_count
ORDER BY mode_count;
```

**留存规律**：多玩法探索用户留存更高（兴趣广）

---

## 五、对比分析要点

| 对比维度 | 核心问题 |
| -------- | -------- |
| 倍数分布 | 癞子的基线倍数比经典高多少？"低倍"在癞子玩法中是否需要重新定义？ |
| 高倍局占比 | 经典中仅经历高倍局的用户占比 vs 癞子中的占比差距多大？ |
| 最优倍数区间 | 经典的最优是 12-24x，癞子是否右移到 24-48x？ |
| 胜率影响 | 癞子因随机性大，胜率对留存的影响是否弱于经典？ |
| 对局数拐点 | 癞子因单局时间可能更短，"玩够多少局"的留存拐点是否不同？ |
| 经济波动 | 癞子/不洗牌的银子波动是否更剧烈？亏损阈值是否需要玩法差异化？ |

---

## 六、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 1局用户、客户端问题、渠道质量 |
