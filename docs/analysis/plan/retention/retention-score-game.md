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
  * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0)

-- 7留（Day7）
COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY) THEN r.uid END)
  * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0)

-- 30留（Day30）
COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN r.uid END)
  * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0)
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
    ROUND(SUM(is_score) * 100.0 / NULLIF(COUNT(*), 0), 2) AS score_game_participation_pct,
    ROUND(SUM(is_silver) * 100.0 / NULLIF(COUNT(*), 0), 2) AS silver_game_participation_pct,
    -- 逻辑重组，利用标签直接判定各分区占比
    ROUND(SUM(CASE WHEN is_score = 1 AND is_silver = 0 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS score_only_pct,
    ROUND(SUM(is_silver = 1 AND is_score = 0) * 100.0 / NULLIF(COUNT(*), 0), 2) AS silver_only_pct,
    ROUND(SUM(CASE WHEN is_score = 1 AND is_silver = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS both_pct,
    ROUND(SUM(CASE WHEN is_score = 0 AND is_silver = 0 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS no_game_pct
FROM user_behavior_tags;
```

### 2.2 积分用户 vs 银子用户 vs 双玩法用户留存对比

> 核心问题：积分玩家、银子玩家、双玩法玩家的留存是否存在显著差异？积分免费模式是否能带来更高的留存？

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群 + 分区限制
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_seed_profile AS (
    -- 2. 预固化标签：在这一层一次性关联玩法 stat，消除重复 Join
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.uid IS NOT NULL AND si.uid IS NOT NULL THEN 'C: 双玩法'
            WHEN s.uid IS NOT NULL THEN 'B: 仅积分'
            WHEN si.uid IS NOT NULL THEN 'A: 仅银子'
            ELSE 'Z: 无对局'
        END AS segment_label
    FROM reg_base_raw r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si ON si.uid = r.uid AND si.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵坍缩：将不同留存日期转化为列，彻底消灭横向 Join 膨胀
    SELECT
        p.uid, p.segment_label, 1 AS is_reg,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS is_d30
    FROM user_seed_profile p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        -- 仅匹配我们关注的 4 个留存日期，直接通过日期范围过滤掉无效分区
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY), DATE_ADD(p.reg_date, INTERVAL 3 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY), DATE_ADD(p.reg_date, INTERVAL 29 DAY))
    GROUP BY p.uid, p.segment_label
)
-- 4. 主查询：矩阵坍缩后的轻量聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    segment_label,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS day1_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS day4_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS day7_rate,
    ROUND(SUM(is_d30) * 100.0 / NULLIF(COUNT(DISTINCT uid), 0), 2) AS day30_rate
FROM all_events_stream
GROUP BY segment_label
ORDER BY segment_label;
```

> **💡 BITMAP 加速版**：uid 基数大时改用 bitmap —— `seg_bitmap` 按 `(user_segment, reg_date)` 预聚合，活跃按日聚合，`BITMAP_AND` 求交。其余分组查询（首日对局数/胜率等）可同构套用：

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 精准定义我们需要关注的最小和最大行为日期
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 29 DAY) AS max_act_date
    FROM reg_base
),
seg AS (
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
-- 🌟 核心优化点：在 Bitmap 生成时就锁定时间边界
game_bitmap AS (
    SELECT dt AS game_date, BITMAP_UNION(TO_BITMAP(uid)) AS game_users_bitmap
    FROM tcy_temp.dws_app_game_active
    WHERE app_id = 1880053
      AND dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
    GROUP BY 1
)
-- 🌟 矩阵分析：利用 INTERSECT_COUNT 实现 O(1) 内存消耗的留存计算
SELECT
    sb.user_segment,
    BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)) AS user_count,
    ROUND(SUM(BITMAP_INTERSECT_COUNT(sb.reg_users_bitmap, g1.game_users_bitmap)) * 100.0 / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day1_rate,
    ROUND(SUM(BITMAP_INTERSECT_COUNT(sb.reg_users_bitmap, g4.game_users_bitmap)) * 100.0 / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day4_rate,
    ROUND(SUM(BITMAP_INTERSECT_COUNT(sb.reg_users_bitmap, g7.game_users_bitmap)) * 100.0 / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day7_rate,
    ROUND(SUM(BITMAP_INTERSECT_COUNT(sb.reg_users_bitmap, g30.game_users_bitmap)) * 100.0 / BITMAP_COUNT(BITMAP_UNION(sb.reg_users_bitmap)), 2) AS day30_rate
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
WITH reg_base AS (
    -- 1. 基础人群与物理过滤：添加必要的 WHERE 条件
    SELECT uid, reg_date, app_id, reg_app_code, channel_category_name
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_behavior_tags AS (
    -- 2. 标签固化层：将复杂的 JOIN 转化为简单的 0/1 标记
    -- 使用 LEFT JOIN 匹配首日对局状态（s.dt = r.reg_date），利用 GROUP BY 确保每个 UID 只有一条记录
    SELECT
        r.uid,
        CASE r.channel_category_name
            WHEN 'OPPO' THEN 'OPPO' WHEN 'IOS' THEN 'iOS' WHEN 'vivo' THEN 'vivo'
            WHEN '华为' THEN '华为' WHEN '咪咕' THEN '咪咕' WHEN '官方(非CPS)' THEN '官方'
            WHEN '荣耀' THEN '荣耀' ELSE '其他'
        END AS channel,
        MAX(CASE WHEN s.uid IS NOT NULL THEN 1 ELSE 0 END) AS is_score,
        MAX(CASE WHEN si.uid IS NOT NULL THEN 1 ELSE 0 END) AS is_silver
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid AND si.dt = r.reg_date
    GROUP BY 1, 2
)
-- 3. 主查询：矩阵坍缩聚合，彻底消灭 DISTINCT
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    channel,
    COUNT(*) AS total_reg,
    ROUND(SUM(is_score) * 100.0 / NULLIF(COUNT(*), 0), 2) AS score_participation_pct,
    ROUND(SUM(is_silver) * 100.0 / NULLIF(COUNT(*), 0), 2) AS silver_participation_pct
FROM user_behavior_tags
GROUP BY channel
ORDER BY score_participation_pct DESC;
```

```sql
-- 积分玩法用户平台分布
WITH reg_base AS (
    -- 1. 基础人群定义
    SELECT uid, reg_date, app_id, reg_app_code, reg_group_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_behavior_flatten AS (
    -- 2. 预压平层：一次性关联，并为每个 UID 生成行为标签
    -- 通过聚合消灭后续关联可能出现的重复行；s.dt = r.reg_date 取首日对局
    SELECT
        r.uid,
        CASE
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            ELSE '其他'
        END AS platform,
        CASE r.reg_app_code
            WHEN 'zgda' THEN 'Cocos-Lua'
            WHEN 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        -- 使用 MAX 压平对局数，处理异常多行记录
        MAX(s.game_count) AS max_game_count
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
    GROUP BY 1, 2, 3
)
-- 3. 最终矩阵聚合：只进行简单的 SUM 和 AVG
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    platform,
    client_lang,
    COUNT(*) AS reg_users,
    ROUND(SUM(CASE WHEN max_game_count > 0 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS score_participation_pct,
    ROUND(AVG(NULLIF(max_game_count, 0)), 1) AS avg_score_games
FROM user_behavior_flatten
GROUP BY platform, client_lang
ORDER BY platform, client_lang;
```

### 2.4 各积分玩法子模式参与分布

> 核心问题：积分玩法内部（积分PC、比赛、好友房），哪个子模式用户量最大？不同子模式用户的留存是否有差异？

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群定义：明确过滤范围
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
score_firstday AS (
    -- 2. 预打标：固化首日玩法偏好，消灭 JOIN 异常膨胀
    SELECT r.uid, r.reg_date, r.app_id, g.play_mode,
           DATE_ADD(r.reg_date, INTERVAL 1 DAY) AS d1_target,
           DATE_ADD(r.reg_date, INTERVAL 6 DAY) AS d7_target
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.uid = r.uid AND g.dt = r.reg_date
    WHERE g.robot != 1 AND g.play_mode IN (4, 5, 6)
),
all_events_stream AS (
    -- 3. 矩阵坍缩：将不同留存日期转化为列
    -- 强制只读取目标日期范围内的活跃数据，大幅减少 I/O
    SELECT
        s.uid, s.play_mode,
        1 AS is_reg,
        MAX(CASE WHEN a.dt = s.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = s.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM score_firstday s
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = s.uid AND a.app_id = s.app_id
        AND a.dt IN (s.d1_target, s.d7_target)
    GROUP BY s.uid, s.play_mode
)
-- 4. 最终矩阵聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 4 THEN '积分PC' WHEN 5 THEN '比赛' WHEN 6 THEN '好友房' ELSE '其他'
    END AS play_mode_name,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream
GROUP BY play_mode
ORDER BY play_mode;
```

---

## 三、参与深度与留存

### 3.1 按首日对局数分组留存

> 核心问题：积分玩法中，首日打多少局对应最佳留存？是否存在一个"最优对局区间"？与银子玩法的最优区间是否有差异？

```sql
WITH reg_base_raw AS (
    -- 基础数据：只取最小必要集
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 预打标：一次扫描，直接产出分层维度
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.game_count = 0  THEN 'A: 0局'
            WHEN s.game_count = 1  THEN 'B: 1局'
            WHEN s.game_count <= 3 THEN 'C: 2-3局'
            WHEN s.game_count <= 5 THEN 'D: 4-5局'
            WHEN s.game_count <= 10 THEN 'E: 6-10局'
            WHEN s.game_count <= 20 THEN 'F: 11-20局'
            ELSE 'G: 20局+'
        END AS game_count_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 矩阵流：利用布尔值坍缩行数，消灭 JOIN 膨胀
    SELECT
        p.uid, p.game_count_group,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 3 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.game_count_group
)
-- 最终聚合：极简算术
SELECT
    game_count_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day4_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream
GROUP BY game_count_group
ORDER BY game_count_group;
```

### 3.2 按首日总时长分组留存

> 核心问题：积分玩法的总投入时长与留存的关系。总时长是否比对局数更能预测留存？

```sql
WITH reg_base_raw AS (
    -- 1. 定义数据源：确保此处表名和列名与你的环境完全一致
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 预打标：一次性计算出游戏时长分组
    SELECT
        r.uid, r.reg_date, r.app_id, s.game_count,
        CASE
            WHEN s.total_play_seconds < 60 THEN 'A: <1分钟'
            WHEN s.total_play_seconds < 300 THEN 'B: 1-5分钟'
            WHEN s.total_play_seconds < 600 THEN 'C: 5-10分钟'
            WHEN s.total_play_seconds < 1800 THEN 'D: 10-30分钟'
            WHEN s.total_play_seconds < 3600 THEN 'E: 30-60分钟'
            ELSE 'F: 60分钟+'
        END AS play_duration_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵坍缩：将不同留存日期转化为标签列
    -- 使用 MAX(CASE) 替代 DISTINCT，彻底消除 JOIN 带来的多行膨胀
    SELECT
        p.uid, p.play_duration_group, p.game_count,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.play_duration_group, p.game_count
)
-- 4. 最终聚合：直接加和计算
SELECT
    play_duration_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games
FROM all_events_stream
GROUP BY play_duration_group
ORDER BY play_duration_group;
```

### 3.3 按平均单局时长分组留存

> 核心问题：平均单局时长反映用户专注度。是"快速刷局"还是"深度对局"类型的用户留存更高？

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群定义 (从物理表获取，确保可独立执行)
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 预打标：一次性计算单局时长分组，并压平数据行
    SELECT
        r.uid, r.reg_date, r.app_id, s.game_count, s.total_play_seconds,
        CASE
            WHEN s.avg_game_seconds < 30 THEN 'A: <30秒（极速）'
            WHEN s.avg_game_seconds < 60 THEN 'B: 30-60秒'
            WHEN s.avg_game_seconds < 120 THEN 'C: 1-2分钟'
            WHEN s.avg_game_seconds < 300 THEN 'D: 2-5分钟'
            ELSE 'E: 5分钟+'
        END AS avg_session_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵流坍缩：将不同留存日期转化为列，消灭 JOIN 后的多行数据
    SELECT
        p.uid, p.avg_session_group, p.game_count, p.total_play_seconds,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.avg_session_group, p.game_count, p.total_play_seconds
)
-- 4. 最终聚合：极简算术求和，无需 DISTINCT
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    avg_session_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(total_play_seconds), 0) AS avg_total_seconds
FROM all_events_stream
GROUP BY avg_session_group
ORDER BY avg_session_group;
```

### 3.4 按首日访问房间数（多样性）分组留存

> 核心问题：愿意在多个房间（distinct_rooms）之间切换的用户，是否意味着更高的探索意愿和留存率？

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群定义 (单一入口，方便加过滤)
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_stats AS (
    -- 2. 预关联统计数据：降低 JOIN 复杂度
    SELECT
        r.uid, r.reg_date, r.app_id,
        s.game_count, s.total_play_seconds,
        CASE
            WHEN s.distinct_rooms = 1 THEN 'A: 单一房间'
            WHEN s.distinct_rooms = 2 THEN 'B: 2个房间'
            WHEN s.distinct_rooms <= 4 THEN 'C: 3-4个房间'
            WHEN s.distinct_rooms <= 6 THEN 'D: 5-6个房间'
            ELSE 'E: 7个房间+'
        END AS room_variety_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
retention_flags AS (
    -- 3. 矩阵坍缩：将活跃判定转化为布尔标签
    -- 这是避免 DISTINCT shuffle 的关键
    SELECT
        uid, room_variety_group, game_count, total_play_seconds,
        MAX(CASE WHEN dt = DATE_ADD(reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN dt = DATE_ADD(reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM (
        SELECT p.uid, p.room_variety_group, p.game_count, p.total_play_seconds, p.reg_date, a.dt
        FROM user_stats p
        LEFT JOIN tcy_temp.dws_app_game_active a
            ON a.uid = p.uid AND a.app_id = p.app_id
            AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY), DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    ) t
    GROUP BY uid, room_variety_group, game_count, total_play_seconds
)
-- 4. 最终汇总
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    room_variety_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(total_play_seconds), 0) AS avg_total_seconds
FROM retention_flags
GROUP BY room_variety_group
ORDER BY room_variety_group;
```

---

## 四、胜负体验与留存

### 4.1 按首日胜率分组留存

> 核心问题：积分玩法中胜率对留存的影响。由于积分玩法没有银子压力，胜率是否成为更核心的留存驱动因素？是否比银子玩法中胜率的影响更大？

```sql
WITH reg_base_raw AS (
    -- 1. 定义基础人群
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 预打标：将胜率分层，压平数据
    SELECT
        r.uid, r.reg_date, r.app_id, s.game_count,
        CASE
            WHEN s.win_rate < 20 THEN 'A: <20%'
            WHEN s.win_rate < 30 THEN 'B: 20-30%'
            WHEN s.win_rate < 40 THEN 'C: 30-40%'
            WHEN s.win_rate < 50 THEN 'D: 40-50%'
            WHEN s.win_rate < 60 THEN 'E: 50-60%'
            WHEN s.win_rate < 70 THEN 'F: 60-70%'
            ELSE 'G: >=70%'
        END AS win_rate_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵流坍缩：将多日期活跃转化为布尔向量
    -- 移除嵌套的 date_bounds 子查询，改为直接在 Join 条件中使用列计算
    SELECT
        p.uid, p.win_rate_group, p.game_count,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 3 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.win_rate_group, p.game_count
)
-- 4. 最终矩阵聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    win_rate_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day4_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games
FROM all_events_stream
GROUP BY win_rate_group
ORDER BY win_rate_group;
```

### 4.2 按最大连胜长度分组留存

> 核心问题：连胜体验对留存的拉动效果。连胜长度越长，是否留存提升越明显？

```sql
WITH reg_base_raw AS (
    -- 1. 基础人群定义
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 预打标：一次性计算连胜分层，并压平数据行
    SELECT
        r.uid, r.reg_date, r.app_id, s.game_count, s.win_rate,
        CASE
            WHEN s.max_win_streak = 0 THEN 'A: 无连胜'
            WHEN s.max_win_streak = 1 THEN 'B: 1连胜'
            WHEN s.max_win_streak = 2 THEN 'C: 2连胜'
            WHEN s.max_win_streak <= 4 THEN 'D: 3-4连胜'
            WHEN s.max_win_streak <= 9 THEN 'E: 5-9连胜'
            ELSE 'F: 10连胜+'
        END AS win_streak_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵流坍缩：利用布尔值坍缩行数，消灭 JOIN 膨胀与去重开销
    SELECT
        p.uid, p.win_streak_group, p.game_count, p.win_rate,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.win_streak_group, p.game_count, p.win_rate
)
-- 4. 最终矩阵聚合：纯数学运算
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    win_streak_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(win_rate), 1) AS avg_win_rate
FROM all_events_stream
GROUP BY win_streak_group
ORDER BY win_streak_group;
```

### 4.3 按最大连败长度分组留存

> 核心问题：连败对留存的杀伤力。积分玩法中连败是否比银子玩法中的连败更致命（因为没有经济压力兜底，纯粹的情绪挫败）？

```sql
WITH reg_base_raw AS (
    -- 1. 定义核心人群范围
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_profile_tags AS (
    -- 2. 预打标：将连败分层，规避后续重复计算
    SELECT
        r.uid, r.reg_date, r.app_id, s.game_count, s.win_rate,
        CASE
            WHEN s.max_lose_streak = 0 THEN 'A: 无连败'
            WHEN s.max_lose_streak = 1 THEN 'B: 1连败'
            WHEN s.max_lose_streak = 2 THEN 'C: 2连败'
            WHEN s.max_lose_streak <= 4 THEN 'D: 3-4连败'
            WHEN s.max_lose_streak <= 9 THEN 'E: 5-9连败'
            ELSE 'F: 10连败+'
        END AS lose_streak_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵流坍缩：将活跃判定转化为布尔标签
    -- 这是消灭 DISTINCT 运算的核心
    SELECT
        p.uid, p.lose_streak_group, p.game_count, p.win_rate,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.lose_streak_group, p.game_count, p.win_rate
)
-- 4. 最终汇总
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    lose_streak_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(win_rate), 1) AS avg_win_rate
FROM all_events_stream
GROUP BY lose_streak_group
ORDER BY lose_streak_group;
```

### 4.4 首局胜负结果与留存

> 核心问题：积分玩法中首局胜负对留存的影响。需要从 `dws_ddz_firstday_game` 中获取 play_mode IN (4, 5, 6) 的对局记录，取首局胜负结果。

```sql
WITH score_first_game AS (
    -- 1. 提取首局数据：通过 ROW_NUMBER 筛选出每个用户的首局记录
    SELECT 
        r.uid, r.reg_date, r.app_id, 
        g.result_id, g.role, g.play_mode,
        ROW_NUMBER() OVER(PARTITION BY r.uid ORDER BY g.game_datetime ASC) as rn
    FROM tcy_temp.dws_dq_app_daily_reg r -- <--- 请确保这是你的基础注册表名
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (4, 5, 6)
),
filtered_first_game AS (
    -- 2. 锁定首局：确保只取每个用户的第一条记录
    SELECT * FROM score_first_game WHERE rn = 1
),
all_events_stream AS (
    -- 3. 矩阵坍缩：将不同留存日期转化为标签列
    -- 使用 MAX(CASE) 替代 COUNT(DISTINCT) 以提升性能
    SELECT 
        f.uid, f.play_mode, f.result_id, f.role, f.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(f.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(f.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM filtered_first_game f
    LEFT JOIN tcy_temp.dws_app_game_active a 
        ON a.uid = f.uid AND a.app_id = f.app_id
        AND a.dt IN (DATE_ADD(f.reg_date, INTERVAL 1 DAY), 
                     DATE_ADD(f.reg_date, INTERVAL 6 DAY))
    GROUP BY f.uid, f.play_mode, f.result_id, f.role, f.reg_date
)
-- 4. 最终聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    CASE f.play_mode
        WHEN 4 THEN '积分PC' WHEN 5 THEN '比赛' WHEN 6 THEN '好友房' ELSE '其他'
    END AS play_mode,
    CASE f.result_id
        WHEN 1 THEN 'A: 首局胜' WHEN 2 THEN 'B: 首局负' ELSE 'C: 平局/异常'
    END AS first_result,
    CASE f.role
        WHEN 1 THEN '地主' WHEN 2 THEN '农民' ELSE '异常'
    END AS first_role,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate
FROM all_events_stream f
GROUP BY 1, 2, 3
ORDER BY 1, 2, 3;
```

### 4.5 胜率与连败交叉分析（高危信号识别）

> 核心问题：胜率低且连败长的双重打击用户，留存率有多低？这组用户是否构成积分玩法的流失高危群体？

```sql
WITH reg_base AS (
    -- 【请确认】将下方物理表名替换为你实际的注册表名
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_risk_profile AS (
    -- 预打标：计算风控分层，避免重复复杂的 Case When 判断
    SELECT
        r.uid, r.reg_date, r.app_id, s.game_count,
        CASE
            WHEN s.win_rate >= 50 AND s.max_lose_streak <= 1 THEN 'A: 高胜率+少连败（健康）'
            WHEN s.win_rate >= 50 AND s.max_lose_streak > 1 THEN 'B: 高胜率+有连败'
            WHEN s.win_rate < 50 AND s.max_lose_streak <= 1 THEN 'C: 低胜率+少连败'
            WHEN s.win_rate < 50 AND s.max_lose_streak BETWEEN 2 AND 3 THEN 'D: 低胜率+中连败（高危）'
            WHEN s.win_rate < 50 AND s.max_lose_streak > 3 THEN 'E: 低胜率+长连败（极高危）'
            ELSE 'Z: 异常'
        END AS risk_segment
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 矩阵流坍缩：将活跃判定转化为布尔标签 (彻底消灭 DISTINCT)
    -- 此步骤将用户的留存行为"压平"为单行数据，极大降低计算内存开销
    SELECT
        p.uid, p.risk_segment, p.game_count,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_risk_profile p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.risk_segment, p.game_count
)
-- 最终聚合：极简计算
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    risk_segment,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games
FROM all_events_stream
GROUP BY risk_segment
ORDER BY risk_segment;
```

---

## 五、逃跑行为分析

### 5.1 按逃跑次数分组留存

> 核心问题：积分玩法中的逃跑行为如何影响留存？逃跑是否反映挫败感，逃跑次数越多的用户留存是否越低？

```sql
WITH reg_base AS (
    -- 1. 基础人群定义 (请根据你的环境修改为真实的注册表名)
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_escape_profile AS (
    -- 2. 预打标：将逃跑次数分层，并压平指标数据
    SELECT
        r.uid, r.reg_date, r.app_id, s.game_count, s.win_rate, s.escape_count,
        CASE
            WHEN s.escape_count = 0 THEN 'A: 未逃跑'
            WHEN s.escape_count = 1 THEN 'B: 逃跑1次'
            WHEN s.escape_count = 2 THEN 'C: 逃跑2次'
            WHEN s.escape_count <= 5 THEN 'D: 逃跑3-5次'
            ELSE 'E: 逃跑5次+'
        END AS escape_group
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 3. 矩阵流坍缩：将不同留存日期转化为标签列，彻底消灭 DISTINCT
    -- 通过 MAX(CASE) 将用户的留存状态转化为布尔值，消灭冗余行
    SELECT
        p.uid, p.escape_group, p.game_count, p.win_rate, p.escape_count,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_d7
    FROM user_escape_profile p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY))
    GROUP BY p.uid, p.escape_group, p.game_count, p.win_rate, p.escape_count
)
-- 4. 最终矩阵聚合：纯数学运算，无开销
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    escape_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(win_rate), 1) AS avg_win_rate,
    ROUND(AVG(escape_count), 1) AS avg_escape
FROM all_events_stream
GROUP BY escape_group
ORDER BY escape_group;
```

### 5.2 积分玩法 vs 银子玩法逃跑率对比

> 核心问题：积分玩法（免费）和银子玩法（有经济成本）的逃跑率差异。免费模式下是否更容易逃跑？逃跑对留存的杀伤力在两种玩法中是否一致？

```sql
WITH reg_base AS (
    -- 【关键修复】请将下方的物理表名替换为你在 SHOW TABLES 中查到的真实名称
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
all_game_stats AS (
    -- 合并两种玩法的统计数据
    SELECT
        r.uid, r.reg_date, r.app_id,
        '积分玩法' AS game_type,
        s.escape_count, s.game_count, s.win_rate
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s ON s.uid = r.uid AND s.dt = r.reg_date
    UNION ALL
    SELECT
        r.uid, r.reg_date, r.app_id,
        '银子玩法' AS game_type,
        si.escape_count, si.game_count, si.win_rate
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_silvergame_stat si ON si.uid = r.uid AND si.dt = r.reg_date
),
all_events_stream AS (
    -- 矩阵坍缩：将活跃判定转化为 0/1 标签，消灭 DISTINCT 运算
    SELECT
        p.uid, p.game_type, p.escape_count, p.game_count, p.win_rate,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1
    FROM all_game_stats p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    GROUP BY p.uid, p.game_type, p.escape_count, p.game_count, p.win_rate
)
-- 最终聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    game_type,
    CASE
        WHEN escape_count = 0 THEN 'A: 未逃跑'
        WHEN escape_count = 1 THEN 'B: 逃跑1次'
        WHEN escape_count = 2 THEN 'C: 逃跑2次'
        WHEN escape_count <= 5 THEN 'D: 逃跑3-5次'
        ELSE 'E: 逃跑5次+'
    END AS escape_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(AVG(game_count), 1) AS avg_games,
    ROUND(AVG(win_rate), 1) AS avg_win_rate
FROM all_events_stream
GROUP BY game_type, escape_group
ORDER BY game_type, escape_group;
```

### 5.3 逃跑用户的胜率分布

> 核心问题：逃跑用户的胜率是否显著低于平均水平？验证"逃跑 = 挫败感"假设。

```sql
WITH reg_base AS (
    -- 这是你提供的人群定义头部
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_behavior_metrics AS (
    -- 1. 预打标：将胜率分层与基础属性合并，减少后续 JOIN 次数
    SELECT
        r.uid, r.reg_date, r.app_id, s.escape_count, s.win_rate,
        CASE
            WHEN s.win_rate < 20 THEN 'A: <20%'
            WHEN s.win_rate < 30 THEN 'B: 20-30%'
            WHEN s.win_rate < 40 THEN 'C: 30-40%'
            WHEN s.win_rate < 50 THEN 'D: 40-50%'
            WHEN s.win_rate < 60 THEN 'E: 50-60%'
            ELSE 'F: >=60%'
        END AS win_rate_group
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid AND s.dt = r.reg_date
),
all_events_stream AS (
    -- 2. 矩阵坍缩：将活跃判定压平为 0/1 标签，每 uid 一行（外层再按 win_rate_group 聚合）
    SELECT
        p.uid, p.win_rate_group, p.escape_count,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1
    FROM user_behavior_metrics p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    GROUP BY p.uid, p.win_rate_group, p.escape_count
)
-- 3. 最终聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    win_rate_group,
    COUNT(*) AS total_users,
    ROUND(SUM(CASE WHEN escape_count > 0 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS escape_pct,
    ROUND(AVG(CASE WHEN escape_count > 0 THEN escape_count END), 2) AS avg_escape_among_escapers,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
FROM all_events_stream
GROUP BY win_rate_group
ORDER BY win_rate_group;
```

---

## 六、跨玩法对比与转化

### 6.1 积分玩家 vs 银子玩家留存基线对比

> 核心问题：仅玩积分玩法的用户和仅玩银子玩法的用户，留存基线对比。积分免费模式是否带来更高的初期留存？

```sql
WITH reg_base AS (
    -- 人群定义
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
user_cohort AS (
    -- 1. 预打标：一次性判断玩法类型，消灭外部 LEFT JOIN
    SELECT
        r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.uid IS NOT NULL AND si.uid IS NULL THEN '仅积分'
            WHEN si.uid IS NOT NULL AND s.uid IS NULL THEN '仅银子'
            WHEN s.uid IS NOT NULL AND si.uid IS NOT NULL THEN '双玩法'
            ELSE '无对局'
        END AS cohort_type
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_scoregame_stat s ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si ON si.uid = r.uid AND si.dt = r.reg_date
),
all_events_stream AS (
    -- 2. 矩阵坍缩：将不同留存日期转化为标签列
    -- 使用 MAX(CASE) 消灭所有 DISTINCT，这是性能提速的关键
    SELECT
        c.cohort_type, c.uid,
        MAX(CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS d1,
        MAX(CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS d4,
        MAX(CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS d7,
        MAX(CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 13 DAY) THEN 1 ELSE 0 END) AS d14,
        MAX(CASE WHEN a.dt = DATE_ADD(c.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS d30
    FROM user_cohort c
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = c.uid AND a.app_id = c.app_id
        AND a.dt IN (DATE_ADD(c.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(c.reg_date, INTERVAL 3 DAY),
                     DATE_ADD(c.reg_date, INTERVAL 6 DAY),
                     DATE_ADD(c.reg_date, INTERVAL 13 DAY),
                     DATE_ADD(c.reg_date, INTERVAL 29 DAY))
    WHERE c.cohort_type != '无对局'
    GROUP BY c.cohort_type, c.uid
)
-- 3. 最终聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    cohort_type,
    COUNT(*) AS user_count,
    ROUND(SUM(d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day4_rate,
    ROUND(SUM(d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(SUM(d14) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day14_rate,
    ROUND(SUM(d30) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day30_rate
FROM all_events_stream
GROUP BY cohort_type
ORDER BY cohort_type;
```

### 6.2 积分玩家向银子玩法的转化率

> 核心问题：积分用户是否会在后续日子转向银子玩法？积分玩法是否起到"入门引导"作用？转化时间窗口是多长？

```sql
WITH reg_base AS (
    -- 人群定义
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-05-16'
),
score_only_day0 AS (
    -- 1. 预打标：仅限首日玩积分、未玩银子的用户
    SELECT r.uid, r.reg_date
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si ON si.uid = r.uid AND si.dt = r.reg_date
    WHERE si.uid IS NULL
),
migration_stream AS (
    -- 2. 行为矩阵坍缩：将转化判定压平，只扫描一次银子表
    -- 🌟 上界裁剪：si.dt <= reg_date+29，避免 si.dt > reg_date 单条件无上界扫描
    SELECT
        u.uid,
        MAX(CASE WHEN si.dt = DATE_ADD(u.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN si.dt BETWEEN DATE_ADD(u.reg_date, INTERVAL 1 DAY) AND DATE_ADD(u.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS is_w1,
        MAX(CASE WHEN si.dt BETWEEN DATE_ADD(u.reg_date, INTERVAL 7 DAY) AND DATE_ADD(u.reg_date, INTERVAL 13 DAY) THEN 1 ELSE 0 END) AS is_w2,
        MAX(CASE WHEN si.dt BETWEEN DATE_ADD(u.reg_date, INTERVAL 14 DAY) AND DATE_ADD(u.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS is_m1
    FROM score_only_day0 u
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = u.uid
        AND si.dt > u.reg_date
        AND si.dt <= DATE_ADD(u.reg_date, INTERVAL 29 DAY)
    GROUP BY u.uid
)
-- 3. 最终聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    COUNT(*) AS score_only_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS converted_d1_pct,
    ROUND(SUM(is_w1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS converted_week1_pct,
    ROUND(SUM(is_w2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS converted_week2_pct,
    ROUND(SUM(is_m1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS converted_month1_pct
FROM migration_stream;
```

### 6.3 积分用户转化到银子后的留存变化

> 核心问题：成功从积分玩法转化到银子玩法的用户，留存是否比始终只玩积分的用户更高？"入门→进阶"路径是否有效？

```sql
WITH reg_base AS (
    -- 1. 基础人群定义
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053 and reg_date between '2026-03-01' and '2026-06-08'
),
migration_labels AS (
    -- 2. 预打标：一次性完成路径识别，消灭明细 Join 带来的数据膨胀
    SELECT
        r.uid, r.reg_date, r.app_id,
        MAX(CASE
            WHEN s.uid IS NOT NULL AND si.uid IS NULL THEN 1
            ELSE 0
        END) AS is_score_only_d0,
        MAX(CASE
            WHEN si_w1.uid IS NOT NULL THEN 1
            ELSE 0
        END) AS played_silver_week1
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s ON s.uid = r.uid AND s.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si ON si.uid = r.uid AND si.dt = r.reg_date
    LEFT JOIN tcy_temp.dws_app_silvergame_stat si_w1
        ON si_w1.uid = r.uid
        AND si_w1.dt BETWEEN DATE_ADD(r.reg_date, INTERVAL 1 DAY) AND DATE_ADD(r.reg_date, INTERVAL 6 DAY)
    GROUP BY r.uid, r.reg_date, r.app_id
),
all_events_stream AS (
    -- 3. 矩阵坍缩：将活跃判定转化为布尔标签
    SELECT
        p.uid, p.app_id, p.reg_date,
        CASE
            WHEN is_score_only_d0 = 1 AND played_silver_week1 = 1 THEN 'A: 积分入门→银子转化'
            WHEN is_score_only_d0 = 1 AND played_silver_week1 = 0 THEN 'B: 积分入门→未转化'
            ELSE 'C: 其他'
        END AS user_path,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS d1,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 6 DAY) THEN 1 ELSE 0 END) AS d7,
        MAX(CASE WHEN a.dt = DATE_ADD(p.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS d30
    FROM migration_labels p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.uid = p.uid AND a.app_id = p.app_id
        AND a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 6 DAY),
                     DATE_ADD(p.reg_date, INTERVAL 29 DAY))
    WHERE is_score_only_d0 = 1 -- 直接在矩阵层过滤掉非目标用户
    GROUP BY p.uid, p.app_id, p.reg_date, is_score_only_d0, played_silver_week1
)
-- 4. 最终聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    user_path,
    COUNT(*) AS user_count,
    ROUND(SUM(d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate,
    ROUND(SUM(d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day7_rate,
    ROUND(SUM(d30) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day30_rate
FROM all_events_stream
GROUP BY user_path
ORDER BY user_path;
```

### 6.4 积分玩法人均游戏天数与银子玩法的相关性

> 核心问题：积分玩法的参与深度是否与银子玩法的参与深度正相关？即"爱玩积分的人，是否也爱玩银子"？

```sql
WITH reg_base AS (
    -- 1. 基础人群定义
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-03-01' AND '2026-05-16'
),
date_bounds AS (
    -- 2. 动态日期边界：用于事实表全局分区裁剪
    SELECT
        MIN(reg_date) AS min_reg,
        DATE_ADD(MAX(reg_date), INTERVAL 29 DAY) AS max_act
    FROM reg_base
),
score_summary AS (
    -- 3. 每用户注册后 30 天内积分活跃天数（per-user 滚动窗口 + 全局分区裁剪）
    SELECT
        r.uid,
        COUNT(DISTINCT s.dt) AS score_game_days,
        SUM(s.game_count) AS total_score_games
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_scoregame_stat s
        ON s.uid = r.uid
        AND s.dt >= r.reg_date
        AND s.dt <= DATE_ADD(r.reg_date, INTERVAL 29 DAY)
    CROSS JOIN date_bounds db
    WHERE s.dt BETWEEN db.min_reg AND db.max_act
    GROUP BY r.uid
),
silver_summary AS (
    -- 4. 每用户注册后 30 天内银子活跃天数（per-user 滚动窗口 + 全局分区裁剪）
    SELECT
        r.uid,
        COUNT(DISTINCT si.dt) AS silver_game_days,
        SUM(si.game_count) AS total_silver_games
    FROM reg_base r
    INNER JOIN tcy_temp.dws_app_silvergame_stat si
        ON si.uid = r.uid
        AND si.dt >= r.reg_date
        AND si.dt <= DATE_ADD(r.reg_date, INTERVAL 29 DAY)
    CROSS JOIN date_bounds db
    WHERE si.dt BETWEEN db.min_reg AND db.max_act
    GROUP BY r.uid
)
-- 5. 最终聚合
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    CASE
        WHEN s.score_game_days = 1 THEN 'A: 仅1天'
        WHEN s.score_game_days <= 3 THEN 'B: 2-3天'
        WHEN s.score_game_days <= 7 THEN 'C: 4-7天'
        WHEN s.score_game_days <= 14 THEN 'D: 8-14天'
        ELSE 'E: 15天+'
    END AS score_active_days_group,
    COUNT(r.uid) AS user_count,
    ROUND(AVG(si.silver_game_days), 1) AS avg_silver_active_days,
    ROUND(AVG(si.total_silver_games), 1) AS avg_silver_games,
    ROUND(AVG(s.total_score_games), 1) AS avg_score_games
FROM reg_base r
INNER JOIN score_summary s ON s.uid = r.uid
LEFT JOIN silver_summary si ON si.uid = r.uid
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