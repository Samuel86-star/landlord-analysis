# 积分玩法层留存分析：参与度与胜负归因

> 本文档聚焦**积分玩法层**留存分析，覆盖积分玩法的参与度、胜负体验、逃跑行为及跨玩法转化分析。积分玩法（play_mode 4, 5, 6）为免费游戏，无银子金流压力，留存驱动力来自参与深度与胜负体验。
>
> **分析时间段**：2026-02-10 至 2026-06-15
> **留存口径**：游戏留存（使用 `dws_app_game_active`，分母为当日注册APP端用户，分子为第N日任意玩法有对局的用户）
> **积分玩法定义**：play_mode IN (4, 5, 6)，即 4=积分PC、5=比赛、6=好友房
>
> **依赖表**：
>
> - `dws_app_scoregame_stat` — 积分玩法用户每日行为聚合（uid × dt）
> - `dws_app_game_active` — 游戏留存 flag（uid × dt × app_id）
> - `dws_dq_app_daily_reg` — APP端注册用户宽表
> - `dws_dq_daily_login` — 每日登录聚合表
> - `dws_ddz_firstday_game` — 首日对局明细表（过滤 play_mode IN 4,5,6）
> - `dws_app_silvergame_stat` — 银子玩法行为聚合（用于跨玩法对比）

---

## 目录

1. [数据基础与口径说明](#一数据基础与口径说明)
2. [积分玩法参与概览](#二积分玩法参与概览)
3. [参与深度与留存](#三参与深度与留存)
4. [胜负体验与留存](#四胜负体验与留存)
5. [逃跑行为分析](#五逃跑行为分析)
6. [跨玩法对比与转化](#六跨玩法对比与转化)
7. [专项分析索引](#七专项分析索引)

---

## 一、数据基础与口径说明

### 1.1 核心数据表

| 表名 | 粒度 | 说明 | 关键字段 |
| ---- | ---- | ---- | ---- |
| `dws_app_scoregame_stat` | uid × dt | 积分玩法用户每日行为聚合 | `game_count`, `total_play_seconds`, `avg_game_seconds`, `distinct_rooms`, `win_count`, `lose_count`, `win_rate`, `lose_rate`, `max_win_streak`, `max_lose_streak`, `escape_count` |
| `dws_app_game_active` | uid × dt × app_id | 游戏留存 flag（任意玩法有对局即活跃） | `app_id` |
| `dws_dq_app_daily_reg` | uid | APP端注册用户宽表 | `reg_app_code`, `reg_group_id`, `channel_category_name` |
| `dws_dq_daily_login` | uid × login_date | 每日登录聚合表 | `login_date`, `login_count` |
| `dws_ddz_firstday_game` | resultguid × uid | 首日对局明细（含 play_mode） | `result_id`, `play_mode`, `role`, `game_datetime` |
| `dws_app_silvergame_stat` | uid × dt | 银子玩法行为聚合 | `game_count`, `win_rate`, `total_diff_money` |

### 1.2 积分玩法说明

| play_mode | 名称 | 货币类型 | 说明 |
| --------- | ---- | -------- | ---- |
| 4 | 积分PC | 积分（免费） | 电脑端积分对局 |
| 5 | 比赛 | 积分（免费） | 比赛模式，非实时结算 |
| 6 | 好友房 | 积分（免费） | 好友邀请开房对局 |

> 积分玩法无银子金流字段，留存驱动力来自参与深度（对局数、时长）和胜负体验（胜率、连败），而非经济压力。

### 1.3 留存计算公式

```sql
-- 次留（Day1）
COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN r.uid END)
  * 100.0 / COUNT(DISTINCT r.uid)

-- 7留（Day7）
COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY) THEN r.uid END)
  * 100.0 / COUNT(DISTINCT r.uid)

-- 30留（Day30）
COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN r.uid END)
  * 100.0 / COUNT(DISTINCT r.uid)
```

### 1.4 通用 WITH 子句（各查询复用）

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id, reg_app_code, reg_group_id, channel_category_name
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 活跃事实表分区裁剪窗口：最早注册次日 ~ 最晚注册后 30 天
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
),
score_reg AS (
    -- 首日有积分玩法对局的注册用户
    SELECT r.uid, r.reg_date, r.app_id, r.reg_app_code, r.reg_group_id, r.channel_category_name,
           s.game_count, s.total_play_seconds, s.avg_game_seconds, s.distinct_rooms,
           s.win_count, s.lose_count, s.win_rate, s.lose_rate,
           s.max_win_streak, s.max_lose_streak, s.escape_count
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
)
```

> 各查询在 JOIN `dws_app_game_active` 等活跃事实表时，建议补 `AND <日期列> BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)` 做分区裁剪，避免扫描注册窗口之外的历史分区。完整 bitmap 写法（含 date_bounds）见 §2.2。

---

## 二、积分玩法参与概览

### 2.1 积分玩法参与率（新增用户首日）

> 核心问题：新增用户中有多少人在首日体验了积分玩法？与银子玩法相比，积分玩法的吸引力如何？

```sql
WITH reg_base AS (
    -- 1. 基础人群与分区裁剪
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_behavior_tags AS (
    -- 2. 标签固化层：提前判定用户在两场地的活跃状态 (0 或 1)
    -- 利用 LEFT JOIN + GROUP BY 确保每个 uid 只有一行结果，消除关联带来的膨胀
    SELECT
        r.uid,
        CASE WHEN s.uid IS NOT NULL THEN 1 ELSE 0 END AS is_score,
        CASE WHEN si.uid IS NOT NULL THEN 1 ELSE 0 END AS is_silver
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date AND s.app_id = r.app_id
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.dt = r.reg_date AND si.app_id = r.app_id
)
-- 3. 极速矩阵聚合：无需再次去重，直接加和求平均
SELECT
    COUNT(*) AS total_reg,
    ROUND(SUM(is_score) * 100.0 / COUNT(*), 2) AS score_game_participation_pct,
    ROUND(SUM(is_silver) * 100.0 / COUNT(*), 2) AS silver_game_participation_pct,
    -- 逻辑重组，利用标签直接判定各分区占比
    ROUND(SUM(CASE WHEN is_score = 1 AND is_silver = 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS score_only_pct,
    ROUND(SUM(is_silver = 1 AND is_score = 0) * 100.0 / COUNT(*), 2) AS silver_only_pct,
    ROUND(SUM(CASE WHEN is_score = 1 AND is_silver = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS both_pct,
    ROUND(SUM(CASE WHEN is_score = 0 AND is_silver = 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS no_game_pct
FROM user_behavior_tags;
```

### 2.2 积分用户 vs 银子用户 vs 双玩法用户留存对比

> 核心问题：积分玩家、银子玩家、双玩法玩家的留存是否存在显著差异？积分免费模式是否能带来更高的留存？

```sql
WITH user_segment AS (
    SELECT r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.uid IS NOT NULL AND si.uid IS NOT NULL THEN 'C: 双玩法'
            WHEN s.uid IS NOT NULL THEN 'B: 仅积分'
            WHEN si.uid IS NOT NULL THEN 'A: 仅银子'
            ELSE 'Z: 无对局'
        END AS user_segment
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.dt = r.reg_date
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    user_segment,
    COUNT(DISTINCT u.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(u.reg_date, INTERVAL 1 DAY)
              THEN u.uid END) * 100.0 / COUNT(DISTINCT u.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(u.reg_date, INTERVAL 3 DAY)
              THEN u.uid END) * 100.0 / COUNT(DISTINCT u.uid), 2) AS day4_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(u.reg_date, INTERVAL 6 DAY)
              THEN u.uid END) * 100.0 / COUNT(DISTINCT u.uid), 2) AS day7_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(u.reg_date, INTERVAL 29 DAY)
              THEN u.uid END) * 100.0 / COUNT(DISTINCT u.uid), 2) AS day30_rate
FROM user_segment u
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = u.uid AND a.app_id = u.app_id
    AND a.dt IN (
        DATE_ADD(u.reg_date, INTERVAL 1 DAY),
        DATE_ADD(u.reg_date, INTERVAL 3 DAY),
        DATE_ADD(u.reg_date, INTERVAL 6 DAY),
        DATE_ADD(u.reg_date, INTERVAL 29 DAY)
    )
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY user_segment
ORDER BY user_segment;
```

> **💡 BITMAP 加速版**：uid 基数大时改用 bitmap —— `seg_bitmap` 按 `(user_segment, reg_date)` 预聚合，活跃按日聚合，`BITMAP_AND` 求交。其余分组查询（首日对局数/胜率等）可同构套用：

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
seg AS (
    -- 分组维度：首日是否参与积分/银子玩法
    SELECT r.uid, r.reg_date,
        CASE
            WHEN s.uid IS NOT NULL AND si.uid IS NOT NULL THEN 'C: 双玩法'
            WHEN s.uid IS NOT NULL THEN 'B: 仅积分'
            WHEN si.uid IS NOT NULL THEN 'A: 仅银子'
            ELSE 'Z: 无对局'
        END AS user_segment
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.app_id = r.app_id AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.app_id = r.app_id AND si.dt = r.reg_date
),
seg_bitmap AS (
    SELECT user_segment, reg_date, BITMAP_UNION(TO_BITMAP(uid)) AS reg_users_bitmap
    FROM seg
    GROUP BY 1, 2
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
),
game_bitmap AS (
    SELECT a.dt AS game_date, BITMAP_UNION(TO_BITMAP(a.uid)) AS game_users_bitmap
    FROM tcy_temp.dws_app_game_active a
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
    GROUP BY 1
)
SELECT
    sb.user_segment,
    BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)) AS user_count,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(sb.reg_users_bitmap, g1.game_users_bitmap))) * 100.0
          / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day1_rate,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(sb.reg_users_bitmap, g4.game_users_bitmap))) * 100.0
          / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day4_rate,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(sb.reg_users_bitmap, g7.game_users_bitmap))) * 100.0
          / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day7_rate,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(sb.reg_users_bitmap, g30.game_users_bitmap))) * 100.0
          / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day30_rate
FROM seg_bitmap sb
LEFT JOIN game_bitmap g1  ON g1.game_date = DATE_ADD(sb.reg_date, INTERVAL 1 DAY)
LEFT JOIN game_bitmap g4  ON g4.game_date = DATE_ADD(sb.reg_date, INTERVAL 3 DAY)
LEFT JOIN game_bitmap g7  ON g7.game_date = DATE_ADD(sb.reg_date, INTERVAL 6 DAY)
LEFT JOIN game_bitmap g30 ON g30.game_date = DATE_ADD(sb.reg_date, INTERVAL 29 DAY)
GROUP BY sb.user_segment
ORDER BY sb.user_segment;
```

### 2.3 积分玩法人群体特征（渠道、平台、客户端分布）

> 核心问题：积分玩法的用户群体在渠道、设备、客户端方面有什么特征？与全体用户相比，积分玩法是否吸引了特定类型的用户？

```sql
SELECT
    CASE r.channel_category_name
        WHEN 'OPPO' THEN 'OPPO'
        WHEN 'IOS' THEN 'iOS'
        WHEN 'vivo' THEN 'vivo'
        WHEN '华为' THEN '华为'
        WHEN '咪咕' THEN '咪咕'
        WHEN '官方(非CPS)' THEN '官方'
        WHEN '荣耀' THEN '荣耀'
        ELSE '其他'
    END AS channel,
    COUNT(DISTINCT r.uid) AS total_reg,
    ROUND(COUNT(DISTINCT CASE WHEN s.uid IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS score_participation_pct,
    ROUND(COUNT(DISTINCT CASE WHEN si.uid IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS silver_participation_pct
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_silvergame_stat si
    ON si.uid = r.uid AND si.dt = r.reg_date
GROUP BY 1
ORDER BY score_participation_pct DESC;
```

```sql
-- 积分玩法用户平台分布
SELECT
    CASE WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
         WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
         ELSE '其他'
    END AS platform,
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN s.uid IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS score_participation_pct,
    ROUND(AVG(CASE WHEN s.uid IS NOT NULL THEN s.game_count END), 1) AS avg_score_games
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
GROUP BY 1, 2
ORDER BY platform, client_lang;
```

### 2.4 各积分玩法子模式参与分布

> 核心问题：积分玩法内部（积分PC、比赛、好友房），哪个子模式用户量最大？不同子模式用户的留存是否有差异？

```sql
WITH score_firstday AS (
    SELECT r.uid, r.reg_date, r.app_id, g.play_mode
    FROM reg_base r
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (4, 5, 6)
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE play_mode
        WHEN 4 THEN '积分PC'
        WHEN 5 THEN '比赛'
        WHEN 6 THEN '好友房'
        ELSE '其他'
    END AS play_mode_name,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(s.reg_date, INTERVAL 1 DAY)
              THEN s.uid END) * 100.0 / COUNT(DISTINCT s.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(s.reg_date, INTERVAL 6 DAY)
              THEN s.uid END) * 100.0 / COUNT(DISTINCT s.uid), 2) AS day7_rate
FROM score_firstday s
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = s.uid AND a.app_id = s.app_id
    AND a.dt IN (DATE_ADD(s.reg_date, INTERVAL 1 DAY), DATE_ADD(s.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY play_mode
ORDER BY play_mode;
```

---

## 三、参与深度与留存

### 3.1 按首日对局数分组留存

> 核心问题：积分玩法中，首日打多少局对应最佳留存？是否存在一个"最优对局区间"？与银子玩法的最优区间是否有差异？

```sql
SELECT
    CASE
        WHEN s.game_count = 0  THEN 'A: 0局'
        WHEN s.game_count = 1  THEN 'B: 1局'
        WHEN s.game_count <= 3 THEN 'C: 2-3局'
        WHEN s.game_count <= 5 THEN 'D: 4-5局'
        WHEN s.game_count <= 10 THEN 'E: 6-10局'
        WHEN s.game_count <= 20 THEN 'F: 11-20局'
        ELSE                        'G: 20局+'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 3 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day4_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (
        DATE_ADD(r.reg_date, INTERVAL 1 DAY),
        DATE_ADD(r.reg_date, INTERVAL 3 DAY),
        DATE_ADD(r.reg_date, INTERVAL 6 DAY)
    )
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

### 3.2 按首日总时长分组留存

> 核心问题：积分玩法的总投入时长与留存的关系。总时长是否比对局数更能预测留存？

```sql
SELECT
    CASE
        WHEN s.total_play_seconds < 60 THEN 'A: <1分钟'
        WHEN s.total_play_seconds < 300 THEN 'B: 1-5分钟'
        WHEN s.total_play_seconds < 600 THEN 'C: 5-10分钟'
        WHEN s.total_play_seconds < 1800 THEN 'D: 10-30分钟'
        WHEN s.total_play_seconds < 3600 THEN 'E: 30-60分钟'
        ELSE 'F: 60分钟+'
    END AS play_duration_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

### 3.3 按平均单局时长分组留存

> 核心问题：平均单局时长反映用户专注度。是"快速刷局"还是"深度对局"类型的用户留存更高？

```sql
SELECT
    CASE
        WHEN s.avg_game_seconds < 30 THEN 'A: <30秒（极速）'
        WHEN s.avg_game_seconds < 60 THEN 'B: 30-60秒'
        WHEN s.avg_game_seconds < 120 THEN 'C: 1-2分钟'
        WHEN s.avg_game_seconds < 300 THEN 'D: 2-5分钟'
        ELSE 'E: 5分钟+'
    END AS avg_session_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.total_play_seconds), 0) AS avg_total_seconds
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

### 3.4 按首日访问房间数（多样性）分组留存

> 核心问题：愿意在多个房间（distinct_rooms）之间切换的用户，是否意味着更高的探索意愿和留存率？

```sql
SELECT
    CASE
        WHEN s.distinct_rooms = 1 THEN 'A: 单一房间'
        WHEN s.distinct_rooms = 2 THEN 'B: 2个房间'
        WHEN s.distinct_rooms <= 4 THEN 'C: 3-4个房间'
        WHEN s.distinct_rooms <= 6 THEN 'D: 5-6个房间'
        ELSE 'E: 7个房间+'
    END AS room_variety_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.total_play_seconds), 0) AS avg_total_seconds
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

---

## 四、胜负体验与留存

### 4.1 按首日胜率分组留存

> 核心问题：积分玩法中胜率对留存的影响。由于积分玩法没有银子压力，胜率是否成为更核心的留存驱动因素？是否比银子玩法中胜率的影响更大？

```sql
SELECT
    CASE
        WHEN s.win_rate < 20 THEN 'A: <20%'
        WHEN s.win_rate < 30 THEN 'B: 20-30%'
        WHEN s.win_rate < 40 THEN 'C: 30-40%'
        WHEN s.win_rate < 50 THEN 'D: 40-50%'
        WHEN s.win_rate < 60 THEN 'E: 50-60%'
        WHEN s.win_rate < 70 THEN 'F: 60-70%'
        ELSE 'G: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 3 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day4_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (
        DATE_ADD(r.reg_date, INTERVAL 1 DAY),
        DATE_ADD(r.reg_date, INTERVAL 3 DAY),
        DATE_ADD(r.reg_date, INTERVAL 6 DAY)
    )
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

### 4.2 按最大连胜长度分组留存

> 核心问题：连胜体验对留存的拉动效果。连胜长度越长，是否留存提升越明显？

```sql
SELECT
    CASE
        WHEN s.max_win_streak = 0 THEN 'A: 无连胜'
        WHEN s.max_win_streak = 1 THEN 'B: 2连胜'
        WHEN s.max_win_streak = 2 THEN 'C: 3连胜'
        WHEN s.max_win_streak <= 4 THEN 'D: 4-5连胜'
        WHEN s.max_win_streak <= 9 THEN 'E: 6-10连胜'
        ELSE 'F: 10连胜+'
    END AS win_streak_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.win_rate), 1) AS avg_win_rate
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

### 4.3 按最大连败长度分组留存

> 核心问题：连败对留存的杀伤力。积分玩法中连败是否比银子玩法中的连败更致命（因为没有经济压力兜底，纯粹的情绪挫败）？

```sql
SELECT
    CASE
        WHEN s.max_lose_streak = 0 THEN 'A: 无连败'
        WHEN s.max_lose_streak = 1 THEN 'B: 2连败'
        WHEN s.max_lose_streak = 2 THEN 'C: 3连败'
        WHEN s.max_lose_streak <= 4 THEN 'D: 4-5连败'
        WHEN s.max_lose_streak <= 9 THEN 'E: 6-10连败'
        ELSE 'F: 10连败+'
    END AS lose_streak_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.win_rate), 1) AS avg_win_rate
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

### 4.4 首局胜负结果与留存

> 核心问题：积分玩法中首局胜负对留存的影响。需要从 `dws_ddz_firstday_game` 中获取 play_mode IN (4, 5, 6) 的对局记录，取首局胜负结果。

```sql
WITH score_first_game AS (
    SELECT r.uid, r.reg_date, r.app_id,
           MIN_BY(g.result_id, g.game_datetime) AS first_result,
           MIN_BY(g.role, g.game_datetime) AS first_role,
           MIN_BY(g.play_mode, g.game_datetime) AS first_mode
    FROM reg_base r
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (4, 5, 6)
    GROUP BY r.uid, r.reg_date, r.app_id
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE f.first_mode
        WHEN 4 THEN '积分PC'
        WHEN 5 THEN '比赛'
        WHEN 6 THEN '好友房'
        ELSE '其他'
    END AS play_mode,
    CASE f.first_result
        WHEN 1 THEN 'A: 首局胜'
        WHEN 2 THEN 'B: 首局负'
        ELSE 'C: 平局/异常'
    END AS first_result,
    CASE f.first_role
        WHEN 1 THEN '地主'
        WHEN 2 THEN '农民'
        ELSE '异常'
    END AS first_role,
    COUNT(DISTINCT f.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(f.reg_date, INTERVAL 1 DAY)
              THEN f.uid END) * 100.0 / COUNT(DISTINCT f.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(f.reg_date, INTERVAL 6 DAY)
              THEN f.uid END) * 100.0 / COUNT(DISTINCT f.uid), 2) AS day7_rate
FROM score_first_game f
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = f.uid AND a.app_id = f.app_id
    AND a.dt IN (DATE_ADD(f.reg_date, INTERVAL 1 DAY), DATE_ADD(f.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2, 3
ORDER BY play_mode, first_result, first_role;
```

### 4.5 胜率与连败交叉分析（高危信号识别）

> 核心问题：胜率低且连败长的双重打击用户，留存率有多低？这组用户是否构成积分玩法的流失高危群体？

```sql
SELECT
    CASE
        WHEN s.win_rate >= 50 AND s.max_lose_streak <= 1 THEN 'A: 高胜率+少连败（健康）'
        WHEN s.win_rate >= 50 AND s.max_lose_streak > 1 THEN 'B: 高胜率+有连败'
        WHEN s.win_rate < 50 AND s.max_lose_streak <= 1 THEN 'C: 低胜率+少连败'
        WHEN s.win_rate < 50 AND s.max_lose_streak BETWEEN 2 AND 3 THEN 'D: 低胜率+中连败（高危）'
        WHEN s.win_rate < 50 AND s.max_lose_streak > 3 THEN 'E: 低胜率+长连败（极高危）'
        ELSE 'Z: 异常'
    END AS risk_segment,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

---

## 五、逃跑行为分析

### 5.1 按逃跑次数分组留存

> 核心问题：积分玩法中的逃跑行为如何影响留存？逃跑是否反映挫败感，逃跑次数越多的用户留存是否越低？

```sql
SELECT
    CASE
        WHEN s.escape_count = 0 THEN 'A: 未逃跑'
        WHEN s.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN s.escape_count = 2 THEN 'C: 逃跑2次'
        WHEN s.escape_count <= 5 THEN 'D: 逃跑3-5次'
        ELSE 'E: 逃跑5次+'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.win_rate), 1) AS avg_win_rate,
    ROUND(AVG(s.escape_count), 1) AS avg_escape
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

### 5.2 积分玩法 vs 银子玩法逃跑率对比

> 核心问题：积分玩法（免费）和银子玩法（有经济成本）的逃跑率差异。免费模式下是否更容易逃跑？逃跑对留存的杀伤力在两种玩法中是否一致？

```sql
SELECT
    '积分玩法' AS game_type,
    CASE
        WHEN s.escape_count = 0 THEN 'A: 未逃跑'
        WHEN s.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN s.escape_count = 2 THEN 'C: 逃跑2次'
        WHEN s.escape_count <= 5 THEN 'D: 逃跑3-5次'
        ELSE 'E: 逃跑5次+'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(AVG(s.game_count), 1) AS avg_games,
    ROUND(AVG(s.win_rate), 1) AS avg_win_rate
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2

UNION ALL

SELECT
    '银子玩法' AS game_type,
    CASE
        WHEN si.escape_count = 0 THEN 'A: 未逃跑'
        WHEN si.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN si.escape_count = 2 THEN 'C: 逃跑2次'
        WHEN si.escape_count <= 5 THEN 'D: 逃跑3-5次'
        ELSE 'E: 逃跑5次+'
    END AS escape_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(AVG(si.game_count), 1) AS avg_games,
    ROUND(AVG(si.win_rate), 1) AS avg_win_rate
FROM reg_base r
INNER JOIN tcy_temp.dws_app_silvergame_stat si
    ON si.uid = r.uid AND si.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1, 2
ORDER BY game_type, escape_group;
```

### 5.3 逃跑用户的胜率分布

> 核心问题：逃跑用户的胜率是否显著低于平均水平？验证"逃跑 = 挫败感"假设。

```sql
SELECT
    CASE
        WHEN s.win_rate < 20 THEN 'A: <20%'
        WHEN s.win_rate < 30 THEN 'B: 20-30%'
        WHEN s.win_rate < 40 THEN 'C: 30-40%'
        WHEN s.win_rate < 50 THEN 'D: 40-50%'
        WHEN s.win_rate < 60 THEN 'E: 50-60%'
        ELSE 'F: >=60%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS total_users,
    ROUND(COUNT(DISTINCT CASE WHEN s.escape_count > 0 THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS escape_pct,
    ROUND(AVG(CASE WHEN s.escape_count > 0 THEN s.escape_count END), 2) AS avg_escape_among_escapers,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM reg_base r
INNER JOIN tcy_temp.dws_app_scoregame_stat s
    ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = r.uid AND a.app_id = r.app_id
    AND a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
GROUP BY 1
ORDER BY 1;
```

---

## 六、跨玩法对比与转化

### 6.1 积分玩家 vs 银子玩家留存基线对比

> 核心问题：仅玩积分玩法的用户和仅玩银子玩法的用户，留存基线对比。积分免费模式是否带来更高的初期留存？

```sql
WITH user_cohort AS (
    SELECT r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.uid IS NOT NULL AND si.uid IS NULL THEN '仅积分'
            WHEN si.uid IS NOT NULL AND s.uid IS NULL THEN '仅银子'
            WHEN s.uid IS NOT NULL AND si.uid IS NOT NULL THEN '双玩法'
            ELSE '无对局'
        END AS cohort_type
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.dt = r.reg_date
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    cohort_type,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 1 DAY)
              THEN c.uid END) * 100.0 / COUNT(DISTINCT c.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 3 DAY)
              THEN c.uid END) * 100.0 / COUNT(DISTINCT c.uid), 2) AS day4_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 6 DAY)
              THEN c.uid END) * 100.0 / COUNT(DISTINCT c.uid), 2) AS day7_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 13 DAY)
              THEN c.uid END) * 100.0 / COUNT(DISTINCT c.uid), 2) AS day14_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 29 DAY)
              THEN c.uid END) * 100.0 / COUNT(DISTINCT c.uid), 2) AS day30_rate
FROM user_cohort c
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = c.uid AND a.app_id = c.app_id
    AND a.dt IN (
        DATE_ADD(c.reg_date, INTERVAL 1 DAY),
        DATE_ADD(c.reg_date, INTERVAL 3 DAY),
        DATE_ADD(c.reg_date, INTERVAL 6 DAY),
        DATE_ADD(c.reg_date, INTERVAL 13 DAY),
        DATE_ADD(c.reg_date, INTERVAL 29 DAY)
    )
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
WHERE cohort_type IN ('仅积分', '仅银子', '双玩法')
GROUP BY cohort_type
ORDER BY cohort_type;
```

### 6.2 积分玩家向银子玩法的转化率

> 核心问题：积分用户是否会在后续日子转向银子玩法？积分玩法是否起到"入门引导"作用？转化时间窗口是多长？

```sql
-- 首日仅玩积分玩法的用户，后续是否开始玩银子玩法
WITH score_only_day0 AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.dt = r.reg_date
    WHERE si.uid IS NULL  -- 首日未玩银子玩法
)
SELECT
    COUNT(DISTINCT u.uid) AS score_only_users,
    ROUND(COUNT(DISTINCT CASE WHEN si_d1.uid IS NOT NULL THEN u.uid END) * 100.0
          / COUNT(DISTINCT u.uid), 2) AS converted_d1_pct,
    ROUND(COUNT(DISTINCT CASE WHEN si_w1.uid IS NOT NULL THEN u.uid END) * 100.0
          / COUNT(DISTINCT u.uid), 2) AS converted_week1_pct,
    ROUND(COUNT(DISTINCT CASE WHEN si_w2.uid IS NOT NULL THEN u.uid END) * 100.0
          / COUNT(DISTINCT u.uid), 2) AS converted_week2_pct,
    ROUND(COUNT(DISTINCT CASE WHEN si_m1.uid IS NOT NULL THEN u.uid END) * 100.0
          / COUNT(DISTINCT u.uid), 2) AS converted_month1_pct
FROM score_only_day0 u
-- 次日转化到银子
LEFT JOIN tcy_temp.dws_app_silvergame_stat si_d1
    ON si_d1.uid = u.uid AND si_d1.dt = DATE_ADD(u.reg_date, INTERVAL 1 DAY)
-- 首周内（D2-D7）转化到银子
LEFT JOIN tcy_temp.dws_app_silvergame_stat si_w1
    ON si_w1.uid = u.uid
    AND si_w1.dt BETWEEN DATE_ADD(u.reg_date, INTERVAL 1 DAY) AND DATE_ADD(u.reg_date, INTERVAL 6 DAY)
-- 次周（D8-D14）转化到银子
LEFT JOIN tcy_temp.dws_app_silvergame_stat si_w2
    ON si_w2.uid = u.uid
    AND si_w2.dt BETWEEN DATE_ADD(u.reg_date, INTERVAL 7 DAY) AND DATE_ADD(u.reg_date, INTERVAL 13 DAY)
-- 月内（D15-D30）转化到银子
LEFT JOIN tcy_temp.dws_app_silvergame_stat si_m1
    ON si_m1.uid = u.uid
    AND si_m1.dt BETWEEN DATE_ADD(u.reg_date, INTERVAL 14 DAY) AND DATE_ADD(u.reg_date, INTERVAL 29 DAY)
WHERE u.reg_date <= '2026-05-16';  -- 确保有足够的时间窗口观察转化
```

### 6.3 积分用户转化到银子后的留存变化

> 核心问题：成功从积分玩法转化到银子玩法的用户，留存是否比始终只玩积分的用户更高？"入门→进阶"路径是否有效？

```sql
WITH user_path AS (
    SELECT r.uid, r.reg_date, r.app_id,
        -- 首日是否仅积分
        CASE WHEN s.uid IS NOT NULL AND si.uid IS NULL THEN 1 ELSE 0 END AS is_score_only_d0,
        -- 首周内是否玩过银子
        MAX(CASE WHEN si_w1.uid IS NOT NULL THEN 1 ELSE 0 END) AS played_silver_week1
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si_w1
        ON si_w1.uid = r.uid
        AND si_w1.dt BETWEEN DATE_ADD(r.reg_date, INTERVAL 1 DAY) AND DATE_ADD(r.reg_date, INTERVAL 6 DAY)
    WHERE r.reg_date <= '2026-06-08'
    GROUP BY r.uid, r.reg_date, r.app_id, s.uid, si.uid
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
)
SELECT
    CASE
        WHEN is_score_only_d0 = 1 AND played_silver_week1 = 1 THEN 'A: 积分入门→银子转化'
        WHEN is_score_only_d0 = 1 AND played_silver_week1 = 0 THEN 'B: 积分入门→未转化'
        ELSE 'C: 其他'
    END AS user_path,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(u.reg_date, INTERVAL 1 DAY)
              THEN u.uid END) * 100.0 / COUNT(DISTINCT u.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(u.reg_date, INTERVAL 6 DAY)
              THEN u.uid END) * 100.0 / COUNT(DISTINCT u.uid), 2) AS day7_rate,
    ROUND(COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(u.reg_date, INTERVAL 29 DAY)
              THEN u.uid END) * 100.0 / COUNT(DISTINCT u.uid), 2) AS day30_rate
FROM user_path u
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.uid = u.uid AND a.app_id = u.app_id
    AND a.dt IN (
        DATE_ADD(u.reg_date, INTERVAL 1 DAY),
        DATE_ADD(u.reg_date, INTERVAL 6 DAY),
        DATE_ADD(u.reg_date, INTERVAL 29 DAY)
    )
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
WHERE user_path IN ('A: 积分入门→银子转化', 'B: 积分入门→未转化')
GROUP BY user_path
ORDER BY user_path;
```

### 6.4 积分玩法人均游戏天数与银子玩法的相关性

> 核心问题：积分玩法的参与深度是否与银子玩法的参与深度正相关？即"爱玩积分的人，是否也爱玩银子"？

```sql
WITH user_activity AS (
    SELECT r.uid, r.reg_date, r.app_id,
        COUNT(DISTINCT s.dt) AS score_game_days,
        COUNT(DISTINCT si.dt) AS silver_game_days,
        SUM(s.game_count) AS total_score_games,
        SUM(si.game_count) AS total_silver_games
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt >= r.reg_date
        AND s.dt <= DATE_ADD(r.reg_date, INTERVAL 29 DAY)
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.dt >= r.reg_date
        AND si.dt <= DATE_ADD(r.reg_date, INTERVAL 29 DAY)
    WHERE r.reg_date <= '2026-05-16'
    GROUP BY r.uid, r.reg_date, r.app_id
)
SELECT
    CASE
        WHEN score_game_days = 1 THEN 'A: 仅1天'
        WHEN score_game_days <= 3 THEN 'B: 2-3天'
        WHEN score_game_days <= 7 THEN 'C: 4-7天'
        WHEN score_game_days <= 14 THEN 'D: 8-14天'
        ELSE 'E: 15天+'
    END AS score_active_days_group,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(silver_game_days), 1) AS avg_silver_active_days,
    ROUND(AVG(total_silver_games), 1) AS avg_silver_games,
    ROUND(AVG(total_score_games), 1) AS avg_score_games
FROM user_activity
GROUP BY 1
ORDER BY 1;
```

---

## 七、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 经典/不洗牌/癞子玩法内因子 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |
| [retention-analysis-framework.md](retention-analysis-framework.md) | 分析框架 | 视角、指标层级、高危信号 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 1局用户、客户端问题、渠道质量 |

---

> **文档版本**：v1.0
> **创建时间**：2026-06-15
> **分析时间段**：2026-02-10 至 2026-06-15
> **核心表**：`dws_app_scoregame_stat`、`dws_app_game_active`、`dws_dq_app_daily_reg`
> **积分玩法**：play_mode IN (4, 5, 6) — 积分PC、比赛、好友房