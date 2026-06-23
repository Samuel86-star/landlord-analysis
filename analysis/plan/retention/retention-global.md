# 全局层留存分析：用户属性与核心指标

> 本文档聚焦**全局层**留存分析，覆盖用户属性视角和投入度视角的核心指标，采用双重留存口径（登录留存 + 游戏留存）。分玩法/分客户端/银子经济/积分玩法的专项分析见对应文档。
>
> **分析时间段**：2026-03-01 至 2026-06-21
> **留存口径**：登录留存（基于 `dws_dq_daily_login`）+ 游戏留存（基于 `dws_app_game_active`）

---

## 目录

1. [数据基础](#一数据基础)
2. [用户属性视角](#二用户属性视角)
3. [高相关性指标](#三高相关性指标)
4. [中等相关性指标](#四中等相关性指标)
5. [高危信号组合](#五高危信号组合)
6. [游戏留存与登录留存对比](#六游戏留存与登录留存对比)
7. [专项分析索引](#七专项分析索引)

---

## 一、数据基础

### 1.1 核心数据表

| 表名 | 粒度 | 说明 | 关键字段 |
| ---- | ---- | ---- | ---- |
| `dws_dq_app_daily_reg` | uid | APP端注册用户宽表（预关联登录+渠道分类） | `reg_app_code`, `reg_group_id`, `channel_category_name`, `first_day_login_cnt`, `reg_date` |
| `dws_dq_daily_login` | uid x login_date | 每日登录聚合 | `login_date`, `login_count`, `app_id` |
| `dws_app_game_active` | uid x dt x app_id | **游戏留存 flag**（任意玩法有对局即活跃，含 510K） | `dt`, `app_id` |
| `dws_app_silvergame_stat` | uid x dt | 银子玩法金流+参与度（play_mode 1,2,3,7） | `game_count`, `win_rate`, `max_lose_streak`, `total_diff_money`, `money_valley`, `escape_count` |
| `dws_app_allgame_stat` | uid x dt x play_mode | 全玩法体验分析（play_mode 1~7） | `avg_magnification`, `multi_24_48_win`, `multi_24_48_lose`, `room_base` |
| `dws_ddz_firstday_game` | resultguid x uid | 经典斗地主首日对局明细 | `result_id`, `role`, `magnification`, `room_base`, `start_money`, `end_money`, `game_datetime` |
| `dq_channel_category_map` | — | 渠道分类映射维表 | `channel_category_name` |

### 1.2 留存计算公式

留存计算采用 CTE 模式，核心注册表 `dws_dq_app_daily_reg` 作为左表，分别 JOIN 登录留存表和游戏留存表。

```sql
-- 登录留存：用户在第 N 天有登录行为
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id
    AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL N DAY)

-- 游戏留存：用户在第 N 天有对局行为（任意玩法，含 510K）
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.app_id = r.app_id
    AND a.uid = r.uid
    AND a.dt = DATE_ADD(r.reg_date, INTERVAL N DAY)
```

**留存率计算示例（次留 Day 1 / 7 留 Day 7 / 30 留 Day 30）：**

```sql
-- 次留（Day 1）
ROUND(
    COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY) THEN r.uid END)
    * 100.0 / COUNT(DISTINCT r.uid), 2
) AS login_day1

-- 7 留（Day 7）
ROUND(
    COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 7 DAY) THEN r.uid END)
    * 100.0 / COUNT(DISTINCT r.uid), 2
) AS game_day7

-- 30 留（Day 30）
ROUND(
    COUNT(DISTINCT CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 30 DAY) THEN r.uid END)
    * 100.0 / COUNT(DISTINCT r.uid), 2
) AS game_day30
```

### 1.3 表选用决策树

```text
需要留存 flag？
  ├── 登录留存 → dws_dq_daily_login
  └── 游戏留存 → dws_app_game_active（任意玩法，含 510K）

需要归因指标？
  ├── 银子金流/参与度 → dws_app_silvergame_stat
  ├── 玩法体验（倍数/炸弹/底注） → dws_app_allgame_stat
  └── 首日对局明细 → dws_ddz_firstday_game

需要用户属性？
  └── dws_dq_app_daily_reg（含渠道分类、客户端版本、设备类型）
```

### 1.4 重要字段说明

| 字段 | 来源表 | 类型 | 说明 |
| ---- | ---- | ---- | ---- |
| `reg_date` | `dws_dq_app_daily_reg` | DATE | 注册日期，可直接用于 DATE_ADD 计算 |
| `reg_app_code` | `dws_dq_app_daily_reg` | STRING | `zgda` = Cocos-Lua, `zgdx` = Cocos-Creator |
| `reg_group_id` | `dws_dq_app_daily_reg` | INT | 8,88 = iOS; 6,66,33,44,77,99 = Android |
| `channel_category_name` | `dws_dq_app_daily_reg` | STRING | 渠道分类名称 |
| `first_day_login_cnt` | `dws_dq_app_daily_reg` | INT | 注册当天登录次数（反映稳定性） |
| `money_valley` | `dws_app_silvergame_stat` | BIGINT | 当日银子谷值（最低值） |
| `total_diff_money` | `dws_app_silvergame_stat` | BIGINT | 当日银子净变化（正=赢，负=亏） |
| `multi_24_48_win` | `dws_app_allgame_stat` | INT | 倍数 [24,48) 区间胜局数（高倍局代表性字段） |
| `multi_24_48_lose` | `dws_app_allgame_stat` | INT | 倍数 [24,48) 区间负局数（高倍局代表性字段） |
| `avg_magnification` | `dws_app_allgame_stat` | DOUBLE | 平均倍数 |
| `escape_count` | `dws_app_silvergame_stat` | INT | 当日逃跑次数 |

---

## 二、用户属性视角

> 核心问题："谁"更容易留存/流失？

### 2.1 按渠道分类留存

渠道分类来自 `dws_dq_app_daily_reg.channel_category_name`，可通过 `dq_channel_category_map` 维表获取完整分类映射。

```sql
WITH reg_base AS (
    -- 1. 注册基础数据（抽取渠道分类，供下游复用）
    SELECT
        uid,
        reg_date,
        CASE
            WHEN channel_category_name IN ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
                THEN channel_category_name
            ELSE '其他'
        END AS channel,
        app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
date_bounds AS (
    -- 2. 全局活跃日期窗口：最早注册次日 ~ 最晚注册后 30 天
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
),
all_events_deduped AS (
    SELECT uid, reg_date, channel, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM reg_base

    UNION ALL

    -- 3. 登录活跃数据（按 (uid, reg_date, days_diff) 去重，仅保留目标留存天数）
    SELECT
        l.uid, r.reg_date, r.channel, 0 AS is_reg,
        DATEDIFF(l.login_date, r.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN reg_base r ON l.app_id = r.app_id AND l.uid = r.uid
    WHERE l.app_id = 1880053
      AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, r.reg_date) IN (1, 3, 7, 14, 30)
    GROUP BY l.uid, r.reg_date, r.channel, login_days_diff

    UNION ALL

    -- 4. 游戏活跃数据（同上策略）
    SELECT
        a.uid, r.reg_date, r.channel, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, r.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN reg_base r ON a.app_id = r.app_id AND a.uid = r.uid
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(a.dt, r.reg_date) IN (1, 3, 7, 14, 30)
    GROUP BY a.uid, r.reg_date, r.channel, game_days_diff
)
SELECT
    channel,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS reg_users,

    -- 登录留存
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 3  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d3,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 14 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d14,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d30,

    -- 游戏留存
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 3  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d3,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d7,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 14 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d14,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d30
FROM all_events_deduped
GROUP BY channel
ORDER BY reg_users DESC;
```

> **💡 性能说明**：当 uid 基数很大（数百万级）时，`COUNT(DISTINCT uid)` 的精确去重开销较高。此时可改用 StarRocks 的 `BITMAP` 方案加速（利用 Roaring Bitmap 做精确去重，性能可提升数倍）。完整 SQL 如下：

```sql
WITH reg_base AS (
    SELECT
        CASE
            WHEN r.channel_category_name IN ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
                THEN r.channel_category_name
            ELSE '其他'
        END AS channel,
        r.reg_date,
        r.uid
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
reg_bitmap AS (
    SELECT channel, reg_date, BITMAP_UNION(TO_BITMAP(uid)) AS reg_users_bitmap
    FROM reg_base
    GROUP BY 1, 2
),
date_bounds AS (
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
),
login_bitmap AS (
    SELECT
        l.login_date,
        BITMAP_UNION(TO_BITMAP(l.uid)) AS login_users_bitmap
    FROM tcy_temp.dws_dq_daily_login l
    WHERE l.app_id = 1880053
      AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
    GROUP BY 1
),
game_bitmap AS (
    SELECT
        a.dt AS game_date,
        BITMAP_UNION(TO_BITMAP(a.uid)) AS game_users_bitmap
    FROM tcy_temp.dws_app_game_active a
    WHERE a.app_id = 1880053
      AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
    GROUP BY 1
)
SELECT
    rb.channel,
    BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)) AS reg_users,

    -- 登录留存（注册 bitmap 与目标日登录 bitmap 求交，再按渠道汇总）
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gl1.login_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS login_d1,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gl3.login_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS login_d3,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gl7.login_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS login_d7,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gl14.login_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS login_d14,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gl30.login_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS login_d30,

    -- 游戏留存
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gg1.game_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS game_d1,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gg3.game_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS game_d3,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gg7.game_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS game_d7,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gg14.game_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS game_d14,
    ROUND(SUM(BITMAP_COUNT(BITMAP_AND(rb.reg_users_bitmap, gg30.game_users_bitmap))) * 100.0 / BITMAP_COUNT(BITMAP_UNION(rb.reg_users_bitmap)), 2) AS game_d30
FROM reg_bitmap rb
LEFT JOIN login_bitmap gl1  ON gl1.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
LEFT JOIN login_bitmap gl3  ON gl3.login_date = DATE_ADD(rb.reg_date, INTERVAL 3 DAY)
LEFT JOIN login_bitmap gl7  ON gl7.login_date = DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
LEFT JOIN login_bitmap gl14 ON gl14.login_date = DATE_ADD(rb.reg_date, INTERVAL 14 DAY)
LEFT JOIN login_bitmap gl30 ON gl30.login_date = DATE_ADD(rb.reg_date, INTERVAL 30 DAY)
LEFT JOIN game_bitmap gg1   ON gg1.game_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
LEFT JOIN game_bitmap gg3   ON gg3.game_date = DATE_ADD(rb.reg_date, INTERVAL 3 DAY)
LEFT JOIN game_bitmap gg7   ON gg7.game_date = DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
LEFT JOIN game_bitmap gg14  ON gg14.game_date = DATE_ADD(rb.reg_date, INTERVAL 14 DAY)
LEFT JOIN game_bitmap gg30  ON gg30.game_date = DATE_ADD(rb.reg_date, INTERVAL 30 DAY)
GROUP BY rb.channel
ORDER BY reg_users DESC;
```

#### 2.1.1 渠道分类说明

`dq_channel_category_map` 维表结构参考：

```sql
-- 查询渠道分类映射
SELECT DISTINCT channel_category_name
FROM tcy_temp.dq_channel_category_map
WHERE app_id = 1880053
ORDER BY channel_category_name;
```

### 2.2 按设备类型留存

iOS 与 Android 的用户体验差异可能导致留存偏差（如 iOS 支付流程更顺畅、Android 机型适配问题更多）。

```sql
WITH reg_base AS (
    -- 1. 核心控制层：全 SQL 唯一需要人工修改注册日期的地方
    SELECT
        uid,
        reg_date,
        CASE
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            ELSE '其他'
        END AS platform,
        app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 以后调整时间只需要改这里
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
date_bounds AS (
    -- 2. 动态计算活跃表的分区裁剪边界（1日留存最小值 ~ 30日留存最大值）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
),
all_events_deduped AS (
    -- 3. 注册用户流
    SELECT uid, reg_date, platform, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM reg_base

    UNION ALL

    -- 4. 登录活跃流（刚性裁剪分区 + 局部去重）
    SELECT
        l.uid, r.reg_date, r.platform, 0 AS is_reg,
        DATEDIFF(l.login_date, r.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN reg_base r ON l.app_id = r.app_id AND l.uid = r.uid
    WHERE l.app_id = 1880053
      -- 🌟 静态常量化分区裁剪，绝不走历史全表扫描
      AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, r.reg_date) IN (1, 7, 30)
    GROUP BY l.uid, r.reg_date, r.platform, login_days_diff

    UNION ALL

    -- 5. 游戏活跃流（刚性裁剪分区 + 局部去重）
    SELECT
        a.uid, r.reg_date, r.platform, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, r.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN reg_base r ON a.app_id = r.app_id AND a.uid = r.uid
    WHERE a.app_id = 1880053
      -- 🌟 静态常量化分区裁剪
      AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(a.dt, r.reg_date) IN (1, 7, 30)
    GROUP BY a.uid, r.reg_date, r.platform, game_days_diff
)
SELECT
    platform,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS reg_users,

    -- 登录留存率计算
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d30,

    -- 游戏留存率计算
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d7,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d30
FROM all_events_deduped
GROUP BY platform
ORDER BY reg_users DESC;
```

### 2.3 按客户端版本留存

`reg_app_code` 标识客户端框架版本：`zgda` = Cocos-Lua（旧框架），`zgdx` = Cocos-Creator（新框架）。不同框架在稳定性、性能、包体大小方面存在差异。

```sql
WITH reg_base AS (
    -- 1. 核心控制层：全 SQL 唯一需要人工修改注册日期的地方
    SELECT
        uid,
        reg_date,
        CASE r.reg_app_code
            WHEN 'zgda' THEN 'Cocos-Lua'
            WHEN 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 以后调整时间只需要改这里
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
date_bounds AS (
    -- 2. 动态计算活跃表的分区裁剪边界（最早注册次日 ~ 最晚注册后30天）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
),
all_events_deduped AS (
    -- 3. 注册用户流
    SELECT uid, reg_date, client_lang, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM reg_base

    UNION ALL

    -- 4. 登录活跃流（刚性裁剪分区 + 局部去重）
    SELECT
        l.uid, r.reg_date, r.client_lang, 0 AS is_reg,
        DATEDIFF(l.login_date, r.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN reg_base r ON l.app_id = r.app_id AND l.uid = r.uid
    WHERE l.app_id = 1880053
      -- 🌟 静态常量化分区裁剪，绝不走全表扫描
      AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, r.reg_date) IN (1, 7, 30)
    GROUP BY l.uid, r.reg_date, r.client_lang, login_days_diff

    UNION ALL

    -- 5. 游戏活跃流（刚性裁剪分区 + 局部去重）
    SELECT
        a.uid, r.reg_date, r.client_lang, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, r.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN reg_base r ON a.app_id = r.app_id AND a.uid = r.uid
    WHERE a.app_id = 1880053
      -- 🌟 静态常量化分区裁剪
      AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(a.dt, r.reg_date) IN (1, 7, 30)
    GROUP BY a.uid, r.reg_date, r.client_lang, game_days_diff
)
SELECT
    client_lang,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS reg_users,

    -- 登录留存率计算
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d30,

    -- 游戏留存率计算
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d7,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d30
FROM all_events_deduped
GROUP BY client_lang
ORDER BY reg_users DESC;
```

### 2.4 按注册小时留存

不同注册时段反映用户画像差异（夜猫子型 vs 休闲型 vs 通勤型），其留存行为特征不同。

```sql
WITH reg_base AS (
    -- 1. 核心控制层：全 SQL 唯一需要人工修改注册日期的地方
    SELECT
        uid,
        reg_date,
        HOUR(r.reg_datetime) AS reg_hour,
        app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 以后调整时间只需要改这里
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
      AND r.reg_datetime IS NOT NULL
),
date_bounds AS (
    -- 2. 动态计算活跃表的分区裁剪边界（因为只算次留，最大边界只需+1天）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_act_date
    FROM reg_base
),
all_events_deduped AS (
    -- 3. 注册用户流
    SELECT uid, reg_hour, 1 AS is_reg, 0 AS is_login_d1, 0 AS is_game_d1
    FROM reg_base

    UNION ALL

    -- 4. 登录活跃流（刚性裁剪分区 + 局部去重）
    SELECT
        l.uid, r.reg_hour, 0 AS is_reg, 1 AS is_login_d1, 0 AS is_game_d1
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN reg_base r ON l.app_id = r.app_id AND l.uid = r.uid
    WHERE l.app_id = 1880053
      -- 🌟 静态常量化分区裁剪，只锁下次留那几天的数据
      AND l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    GROUP BY l.uid, r.reg_hour -- 局部去重

    UNION ALL

    -- 5. 游戏活跃流（刚性裁剪分区 + 局部去重）
    SELECT
        a.uid, r.reg_hour, 0 AS is_reg, 0 AS is_login_d1, 1 AS is_game_d1
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN reg_base r ON a.app_id = r.app_id AND a.uid = r.uid
    WHERE a.app_id = 1880053
      -- 🌟 静态常量化分区裁剪
      AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    GROUP BY a.uid, r.reg_hour -- 局部去重
)
SELECT
    CASE
        WHEN reg_hour BETWEEN 6 AND 11 THEN 'A: 早间(6-11点)'
        WHEN reg_hour BETWEEN 12 AND 17 THEN 'B: 午后(12-17点)'
        WHEN reg_hour BETWEEN 18 AND 23 THEN 'C: 晚间(18-23点)'
        ELSE 'D: 凌晨(0-5点)'
    END AS reg_period,
    reg_hour,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS reg_users,

    -- 次留计算（得益于子查询去重，此处 distinct 几乎无压力）
    ROUND(COUNT(DISTINCT CASE WHEN is_login_d1 = 1 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN is_game_d1 = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1
FROM all_events_deduped
GROUP BY reg_hour
ORDER BY reg_hour;
```

---

## 三、高相关性指标

> 核心问题：哪些指标直接影响留存？本节指标均基于 `dws_app_silvergame_stat`（银子金流+参与度）和 `dws_app_allgame_stat`（玩法体验）。

### 3.1 首日对局数（投入度核心指标）

对局数是投入度最直接的指标。零对局用户留存极低，但过高的对局数（10+局）也可能因疲劳导致留存下降。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础数据
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一需要人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    -- 2. 提取并清洗当天对局数（关联注册当天 dt = reg_date）
    SELECT
        rb.uid, rb.reg_date,
        CASE
            WHEN COALESCE(gs.game_count, 0) = 0 THEN 'A: 0局'
            WHEN gs.game_count = 1 THEN 'B: 1局'
            WHEN gs.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
            WHEN gs.game_count BETWEEN 6 AND 10 THEN 'D: 6-10局'
            ELSE 'E: 10局+'
        END AS game_count_group,
        COALESCE(gs.game_count, 0) AS game_count
    FROM reg_base_raw rb
    LEFT JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
),
date_bounds AS (
    -- 3. 动态计算活跃表的分区裁剪边界
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base_raw
),
all_events_deduped AS (
    -- 4. 注册与对局底表流
    SELECT uid, game_count_group, game_count, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM game_stat

    UNION ALL

    -- 5. 登录留存流（🌟 已修复：删除了未被 GROUP BY 错引的 l.login_date）
    SELECT
        g.uid, -- 输出 uid
        g.game_count_group, g.game_count, 0 AS is_reg,
        DATEDIFF(l.login_date, g.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN game_stat g ON l.app_id = 1880053 AND l.uid = g.uid
    WHERE l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, g.reg_date) IN (1, 7, 30)
    GROUP BY g.uid, g.game_count_group, g.game_count, login_days_diff

    UNION ALL

    -- 6. 游戏留存流（🌟 已修复：删除了未被 GROUP BY 错引的 a.dt）
    SELECT
        g.uid, -- 输出 uid
        g.game_count_group, g.game_count, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, g.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN game_stat g ON a.app_id = 1880053 AND a.uid = g.uid
    WHERE a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(a.dt, g.reg_date) IN (1, 7, 30)
    GROUP BY g.uid, g.game_count_group, g.game_count, game_days_diff
)
SELECT
    game_count_group,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,
    ROUND(SUM(CASE WHEN is_reg = 1 THEN game_count ELSE 0 END) * 1.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 1) AS avg_game_count,

    -- 登录留存
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d30,

    -- 游戏留存
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d7,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 30 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d30
FROM all_events_deduped
GROUP BY game_count_group
ORDER BY game_count_group;
```

### 3.2 首日胜率（胜负情绪线）

胜率是新手体验的核心感知指标。胜率过低（<30%）的用户流失风险极高。注意：仅对有对局的用户有意义，零对局用户需单独分组。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础数据
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    -- 2. 核心人群圈定：过滤当天有对局的用户，并划分胜率区间
    SELECT
        rb.uid, rb.reg_date,
        CASE
            WHEN gs.win_rate < 30 THEN 'A: <30%'
            WHEN gs.win_rate < 50 THEN 'B: 30-50%'
            WHEN gs.win_rate < 70 THEN 'C: 50-70%'
            ELSE 'D: >=70%'
        END AS win_rate_group,
        gs.game_count
    FROM reg_base_raw rb
    INNER JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
    WHERE gs.game_count > 0
),
date_bounds AS (
    -- 3. 动态计算活跃表的分区裁剪边界（因为只算到7留，最大边界只需+7天，极大缩减扫描量）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
all_events_deduped AS (
    -- 4. 圈定基础活跃流
    SELECT uid, win_rate_group, game_count, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM game_stat

    UNION ALL

    -- 5. 登录留存流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.win_rate_group, g.game_count, 0 AS is_reg,
        DATEDIFF(l.login_date, g.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN game_stat g ON l.app_id = 1880053 AND l.uid = g.uid
    WHERE l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, g.reg_date) IN (1, 7)
    GROUP BY g.uid, g.win_rate_group, g.game_count, login_days_diff

    UNION ALL

    -- 6. 游戏留存流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.win_rate_group, g.game_count, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, g.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN game_stat g ON a.app_id = 1880053 AND a.uid = g.uid
    WHERE a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(a.dt, g.reg_date) IN (1, 7)
    GROUP BY g.uid, g.win_rate_group, g.game_count, game_days_diff
)
SELECT
    win_rate_group,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,
    -- 锁定在注册流上计算平均局数，防止分母被摊薄
    ROUND(SUM(CASE WHEN is_reg = 1 THEN game_count ELSE 0 END) * 1.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 1) AS avg_games,

    -- 登录留存
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,

    -- 游戏留存
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d7
FROM all_events_deduped
GROUP BY win_rate_group
ORDER BY win_rate_group;
```

### 3.3 连败长度（关键流失预警）

连败是挫败感的直接体现。连败超过 3 局时，用户的流失概率急剧上升。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础数据
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一需要人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    -- 2. 提取当天对局行为并打上"连败标签"
    SELECT
        rb.uid, rb.reg_date,
        CASE
            WHEN gs.game_count IS NULL OR gs.game_count = 0 THEN 'A: 无对局'
            WHEN COALESCE(gs.max_lose_streak, 0) = 0 THEN 'B: 无连败'
            WHEN gs.max_lose_streak <= 2 THEN 'C: 1-2连败'
            WHEN gs.max_lose_streak <= 5 THEN 'D: 3-5连败'
            ELSE 'E: 5+连败'
        END AS lose_streak_group
    FROM reg_base_raw rb
    LEFT JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
),
date_bounds AS (
    -- 3. 动态计算次留所需的分区裁剪边界（因为只看次留，最大边界只需 +1 天，扫描量降到最低）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_act_date
    FROM reg_base_raw
),
all_events_deduped AS (
    -- 4. 基础人群流（包含标签）
    SELECT uid, lose_streak_group, 1 AS is_reg, 0 AS is_login_d1
    FROM game_stat

    UNION ALL

    -- 5. 次日登录活跃流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.lose_streak_group, 0 AS is_reg, 1 AS is_login_d1
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN game_stat g ON l.app_id = 1880053 AND l.uid = g.uid
    WHERE l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND l.login_date = DATE_ADD(g.reg_date, INTERVAL 1 DAY)
    GROUP BY g.uid, g.lose_streak_group -- 在内部直接把单人多设备登录去重
)
SELECT
    lose_streak_group,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,
    
    -- 次留计算
    ROUND(COUNT(DISTINCT CASE WHEN is_login_d1 = 1 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1
FROM all_events_deduped
GROUP BY lose_streak_group
ORDER BY lose_streak_group;
```

### 3.4 银子净变化（经济压力线）

银子净变化反映用户首日的经济得失。巨亏用户留存显著低于盈利用户。分组时需考虑房间底注水平，不同房间的盈亏绝对值含义不同。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础数据
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    -- 2. 提取当天对局输赢数据并提前打上分层标签
    SELECT
        rb.uid, rb.reg_date,
        CASE
            WHEN gs.game_count IS NULL OR gs.game_count = 0 THEN '0: 无对局'
            WHEN gs.total_diff_money < -50000 THEN 'A: 巨亏(<-5万)'
            WHEN gs.total_diff_money < -10000 THEN 'B: 大亏(-5万~-1万)'
            WHEN gs.total_diff_money < 0 THEN 'C: 小亏(-1万~0)'
            WHEN gs.total_diff_money < 10000 THEN 'D: 小赚(0~1万)'
            WHEN gs.total_diff_money < 50000 THEN 'E: 大赚(1万~5万)'
            ELSE 'F: 巨赚(>5万)'
        END AS money_group,
        COALESCE(gs.total_diff_money, 0) AS total_diff_money
    FROM reg_base_raw rb
    LEFT JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
),
date_bounds AS (
    -- 3. 动态计算活跃表的分区裁剪边界（只看到7留，最大边界只需+7天，大幅降低IO扫描）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
all_events_deduped AS (
    -- 4. 圈定基础人群流与行为标签
    SELECT uid, money_group, total_diff_money, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM game_stat

    UNION ALL

    -- 5. 登录留存流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.money_group, g.total_diff_money, 0 AS is_reg,
        DATEDIFF(l.login_date, g.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN game_stat g ON l.app_id = 1880053 AND l.uid = g.uid
    WHERE l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, g.reg_date) IN (1, 7)
    GROUP BY g.uid, g.money_group, g.total_diff_money, login_days_diff

    UNION ALL

    -- 6. 游戏留存流（刚性裁剪分区 + 局部去重，原SQL只查了game_d1）
    SELECT
        g.uid, g.money_group, g.total_diff_money, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, g.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN game_stat g ON a.app_id = 1880053 AND a.uid = g.uid
    WHERE a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      -- 🌟 配合你外层只统计了game_d1，这里仅保留次日
      AND DATEDIFF(a.dt, g.reg_date) = 1
    GROUP BY g.uid, g.money_group, g.total_diff_money, game_days_diff
)
SELECT
    money_group,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,
    -- 🌟 锁定在注册流上计算平均输赢差额，防止结果被外部流摊薄
    ROUND(SUM(CASE WHEN is_reg = 1 THEN total_diff_money ELSE 0 END) * 1.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0) AS avg_money_change,

    -- 登录留存
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,

    -- 游戏留存
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1
FROM all_events_deduped
GROUP BY money_group
ORDER BY money_group;
```

### 3.5 破产状态

破产（银子谷值 ≤ 最低房间门槛）是用户退出的直接经济原因。结合 `money_valley` 字段判断是否触及破产线。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础数据
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    -- 2. 提取当天对局行为及财富谷值，并提前打上破产分层标签
    SELECT
        rb.uid, rb.reg_date,
        CASE
            WHEN gs.game_count IS NULL OR gs.game_count = 0 THEN 'C: 无对局'
            WHEN gs.money_valley <= 1000 THEN 'A: 疑似破产(≤1000)'
            ELSE 'B: 未破产'
        END AS bankrupt_group,
        COALESCE(gs.money_valley, 0) AS money_valley
    FROM reg_base_raw rb
    LEFT JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
),
date_bounds AS (
    -- 3. 动态计算活跃表的分区裁剪边界（因为只算到7留，最大边界只需+7天，大幅降低IO扫描）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
all_events_deduped AS (
    -- 4. 圈定基础人群流与行为标签
    SELECT uid, bankrupt_group, money_valley, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM game_stat

    UNION ALL

    -- 5. 登录留存流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.bankrupt_group, g.money_valley, 0 AS is_reg,
        DATEDIFF(l.login_date, g.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN game_stat g ON l.app_id = 1880053 AND l.uid = g.uid
    WHERE l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, g.reg_date) IN (1, 7)
    GROUP BY g.uid, g.bankrupt_group, g.money_valley, login_days_diff

    UNION ALL

    -- 6. 游戏留存流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.bankrupt_group, g.money_valley, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, g.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN game_stat g ON a.app_id = 1880053 AND a.uid = g.uid
    WHERE a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(a.dt, g.reg_date) IN (1, 7)
    GROUP BY g.uid, g.bankrupt_group, g.money_valley, game_days_diff
)
SELECT
    bankrupt_group,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,
    -- 🌟 修正原 AVG 算法：锁定在注册流（is_reg = 1）上计算平均财富谷值，防止分母被外部流摊薄导致结果偏低
    ROUND(SUM(CASE WHEN is_reg = 1 THEN money_valley ELSE 0 END) * 1.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0) AS avg_money_valley,

    -- 登录留存
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,

    -- 游戏留存
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d7
FROM all_events_deduped
GROUP BY bankrupt_group
ORDER BY bankrupt_group;
```

### 3.6 高倍局输赢

高倍局（倍数 > 24x）的输赢体验对用户情绪影响极大。输高倍局可能导致用户一次性巨额亏损，引发流失。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础数据
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    -- 2. 聚合固定倍数段并【提前封装标签】，降低后续数据流传输和 GROUP BY 的体积开销
    -- 🌟 表结构已变更：multi_q4_win_count/lose_count 已废弃，改为固定倍数段
    --    高倍 >24x = multi_24_48 + multi_48_96 + multi_96_192 + multi_192_384 + multi_384_plus
    SELECT
        rb.uid, rb.reg_date,
        CASE
            WHEN SUM(COALESCE(gs.multi_24_48_win, 0) + COALESCE(gs.multi_48_96_win, 0) + COALESCE(gs.multi_96_192_win, 0) + COALESCE(gs.multi_192_384_win, 0) + COALESCE(gs.multi_384_plus_win, 0)) = 0
             AND SUM(COALESCE(gs.multi_24_48_lose, 0) + COALESCE(gs.multi_48_96_lose, 0) + COALESCE(gs.multi_96_192_lose, 0) + COALESCE(gs.multi_192_384_lose, 0) + COALESCE(gs.multi_384_plus_lose, 0)) = 0
                THEN 'A: 未经历高倍'
            WHEN SUM(COALESCE(gs.multi_24_48_win, 0) + COALESCE(gs.multi_48_96_win, 0) + COALESCE(gs.multi_96_192_win, 0) + COALESCE(gs.multi_192_384_win, 0) + COALESCE(gs.multi_384_plus_win, 0)) > 0
             AND SUM(COALESCE(gs.multi_24_48_lose, 0) + COALESCE(gs.multi_48_96_lose, 0) + COALESCE(gs.multi_96_192_lose, 0) + COALESCE(gs.multi_192_384_lose, 0) + COALESCE(gs.multi_384_plus_lose, 0)) = 0
                THEN 'B: 仅赢高倍'
            WHEN SUM(COALESCE(gs.multi_24_48_win, 0) + COALESCE(gs.multi_48_96_win, 0) + COALESCE(gs.multi_96_192_win, 0) + COALESCE(gs.multi_192_384_win, 0) + COALESCE(gs.multi_384_plus_win, 0)) = 0
             AND SUM(COALESCE(gs.multi_24_48_lose, 0) + COALESCE(gs.multi_48_96_lose, 0) + COALESCE(gs.multi_96_192_lose, 0) + COALESCE(gs.multi_192_384_lose, 0) + COALESCE(gs.multi_384_plus_lose, 0)) > 0
                THEN 'C: 仅输高倍'
            ELSE 'D: 有赢有输'
        END AS high_multi_exp
    FROM reg_base_raw rb
    LEFT JOIN tcy_temp.dws_app_allgame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
    GROUP BY rb.uid, rb.reg_date
),
date_bounds AS (
    -- 3. 动态计算次留所需的分区裁剪边界（1天动态缩减）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_act_date
    FROM reg_base_raw
),
all_events_deduped AS (
    -- 4. 基础人群流
    SELECT uid, high_multi_exp, 1 AS is_reg, 0 AS is_login_d1
    FROM game_stat

    UNION ALL

    -- 5. 次日登录活跃流（🌟 局部去重优化：GROUP BY 只需使用轻量的文本标签字符，大大减轻哈希负担）
    SELECT
        g.uid, g.high_multi_exp, 0 AS is_reg, 1 AS is_login_d1
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN game_stat g ON l.app_id = 1880053 AND l.uid = g.uid
    WHERE l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND l.login_date = DATE_ADD(g.reg_date, INTERVAL 1 DAY)
    GROUP BY g.uid, g.high_multi_exp
)
SELECT
    high_multi_exp,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 次留计算
    ROUND(COUNT(DISTINCT CASE WHEN is_login_d1 = 1 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1
FROM all_events_deduped
GROUP BY high_multi_exp
ORDER BY high_multi_exp;
```

### 3.7 登录频次（稳定性信号）

`first_day_login_cnt` 反映注册当天的登录次数。多次登录（≥3次）可能是客户端崩溃/闪退的信号，这类用户的留存率可能受到稳定性问题影响。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础数据 + 标签提前封装
    SELECT
        r.uid,
        r.reg_date,
        r.app_id,
        CASE
            WHEN r.first_day_login_cnt = 1 THEN 'A: 1次(正常)'
            WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
            WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5次(疑似崩溃)'
            ELSE 'D: 5次+(高频崩溃)'
        END AS login_freq_group
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
date_bounds AS (
    -- 2. 动态计算活跃表的分区裁剪边界（因为只看7留，最大边界只需+7天，消除几百个分区的无用扫描）
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
all_events_deduped AS (
    -- 3. 基础新登用户流
    SELECT uid, login_freq_group, 1 AS is_reg, 0 AS login_days_diff, 0 AS game_days_diff
    FROM reg_base_raw

    UNION ALL

    -- 4. 登录留存流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.login_freq_group, 0 AS is_reg,
        DATEDIFF(l.login_date, g.reg_date) AS login_days_diff,
        0 AS game_days_diff
    FROM tcy_temp.dws_dq_daily_login l
    INNER JOIN reg_base_raw g ON l.app_id = 1880053 AND l.uid = g.uid
    WHERE l.login_date BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(l.login_date, g.reg_date) IN (1, 7)
    GROUP BY g.uid, g.login_freq_group, login_days_diff

    UNION ALL

    -- 5. 游戏留存流（刚性裁剪分区 + 局部去重）
    SELECT
        g.uid, g.login_freq_group, 0 AS is_reg, 0 AS login_days_diff,
        DATEDIFF(a.dt, g.reg_date) AS game_days_diff
    FROM tcy_temp.dws_app_game_active a
    INNER JOIN reg_base_raw g ON a.app_id = 1880053 AND a.uid = g.uid
    WHERE a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
      AND DATEDIFF(a.dt, g.reg_date) IN (1, 7)
    GROUP BY g.uid, g.login_freq_group, game_days_diff
)
SELECT
    login_freq_group,
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 登录留存
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 1 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d1,
    ROUND(COUNT(DISTINCT CASE WHEN login_days_diff = 7 THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS login_d7,

    -- 游戏留存
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 1  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d1,
    ROUND(COUNT(DISTINCT CASE WHEN game_days_diff = 7  THEN uid END) * 100.0 / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 2) AS game_d7
FROM all_events_deduped
GROUP BY login_freq_group
ORDER BY login_freq_group;
```
```

---

## 四、中等相关性指标

> 核心问题：哪些指标在特定场景下影响留存？

### 4.1 平均倍数

平均倍数与留存呈倒 U 型关系：倍数过低（≤6x）体验平淡，倍数过高（>24x）波动过大。

```sql
WITH reg_base AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    SELECT
        rb.uid, rb.reg_date,
        AVG(gs.avg_magnification) AS avg_multi,
        SUM(gs.game_count) AS total_games
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_allgame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
    GROUP BY rb.uid, rb.reg_date
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN gs.total_games IS NULL OR gs.total_games = 0 THEN '0: 无对局'
        WHEN gs.avg_multi <= 6 THEN 'A: ≤6x'
        WHEN gs.avg_multi <= 12 THEN 'B: 6-12x'
        WHEN gs.avg_multi <= 24 THEN 'C: 12-24x'
        ELSE 'D: 24x+'
    END AS multi_group,
    COUNT(DISTINCT rb.uid) AS user_count,
    ROUND(AVG(gs.avg_multi), 1) AS avg_multi,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1
FROM reg_base rb
LEFT JOIN game_stat gs ON rb.uid = gs.uid AND rb.reg_date = gs.reg_date
LEFT JOIN login_ret lr ON rb.uid = lr.uid
GROUP BY 1
ORDER BY 1;
```

### 4.2 首局胜负

首局胜负对用户的第一印象至关重要。使用 `dws_ddz_firstday_game` 表的 `MIN_BY` 函数获取首局结果。

```sql
WITH first_game AS (
    SELECT
        g.uid,
        g.dt,
        MIN_BY(g.result_id, g.game_datetime) AS first_game_result
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-01' AND '2026-06-21'
      AND g.robot != 1
      AND g.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND g.play_mode IN (1, 2, 3, 5)
    GROUP BY g.uid, g.dt
),
reg_base AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS login_d7
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
        )
    GROUP BY rb.uid
),
game_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS game_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS game_d7
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = rb.app_id AND a.uid = rb.uid
        AND a.dt IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
        )
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN fg.first_game_result = 1 THEN 'A: 首局胜'
        WHEN fg.first_game_result = 2 THEN 'B: 首局负'
        ELSE 'C: 无对局或非经典玩法'
    END AS first_game_group,
    COUNT(DISTINCT rb.uid) AS user_count,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1,
    ROUND(SUM(lr.login_d7) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d7,
    ROUND(SUM(gr.game_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS game_d1,
    ROUND(SUM(gr.game_d7) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS game_d7
FROM reg_base rb
LEFT JOIN first_game fg ON rb.uid = fg.uid AND rb.reg_date = fg.dt
LEFT JOIN login_ret lr ON rb.uid = lr.uid
LEFT JOIN game_ret gr ON rb.uid = gr.uid
GROUP BY 1
ORDER BY 1;
```

### 4.3 角色偏好（地主/农民）

地主 vs 农民的角色偏好反映了用户的激进程度。地主体验方差大（大输大赢），偏好地主的用户留存与经济变化强绑定。

```sql
WITH role_stats AS (
    SELECT
        g.uid,
        g.dt,
        COUNT(*) AS game_count,
        SUM(CASE WHEN g.role = 1 THEN 1 ELSE 0 END) AS landlord_count
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-01' AND '2026-06-21'
      AND g.robot != 1
      AND g.play_mode IN (1, 2, 3)
    GROUP BY g.uid, g.dt
),
reg_base AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN rs.landlord_count = 0 THEN 'A: 纯农民'
        WHEN rs.landlord_count * 1.0 / rs.game_count < 0.3 THEN 'B: 偏好农民(<30%)'
        WHEN rs.landlord_count * 1.0 / rs.game_count < 0.6 THEN 'C: 均衡(30-60%)'
        ELSE 'D: 偏好地主(>=60%)'
    END AS role_preference,
    COUNT(DISTINCT rb.uid) AS user_count,
    ROUND(AVG(rs.landlord_count * 1.0 / rs.game_count), 2) AS avg_landlord_ratio,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1
FROM reg_base rb
INNER JOIN role_stats rs ON rb.uid = rs.uid AND rb.reg_date = rs.dt
LEFT JOIN login_ret lr ON rb.uid = lr.uid
GROUP BY 1
ORDER BY 1;
```

### 4.4 逃跑行为

逃跑反映挫败感或操作意外。逃跑率高的用户留存率极低。

```sql
WITH reg_base AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    SELECT
        rb.uid, rb.reg_date,
        gs.game_count,
        COALESCE(gs.escape_count, 0) AS escape_count
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
    WHERE gs.game_count > 0
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN gs.escape_count = 0 THEN 'A: 无逃跑'
        WHEN gs.escape_count = 1 THEN 'B: 逃跑1次'
        WHEN gs.escape_count = 2 THEN 'C: 逃跑2次'
        ELSE 'D: 逃跑3+次'
    END AS escape_group,
    COUNT(DISTINCT rb.uid) AS user_count,
    ROUND(AVG(gs.escape_count * 1.0 / gs.game_count) * 100, 2) AS avg_escape_rate,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1
FROM reg_base rb
INNER JOIN game_stat gs ON rb.uid = gs.uid AND rb.reg_date = gs.reg_date
LEFT JOIN login_ret lr ON rb.uid = lr.uid
GROUP BY 1
ORDER BY 1;
```

### 4.5 房间底注体验

房间底注影响用户的经济压力水平。底注越高，破产风险越大，新手留存压力也越大。

```sql
WITH room_base AS (
    SELECT
        g.uid,
        g.dt,
        AVG(g.room_base) AS avg_room_base
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-01' AND '2026-06-21'
      AND g.robot != 1
      AND g.play_mode IN (1, 2, 3)
    GROUP BY g.uid, g.dt
),
reg_base AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS login_d7
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
        )
    GROUP BY rb.uid
),
game_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS game_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS game_d7
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = rb.app_id AND a.uid = rb.uid
        AND a.dt IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
        )
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN rb.avg_room_base <= 50 THEN 'A: 极低底注(<=50)'
        WHEN rb.avg_room_base <= 200 THEN 'B: 低底注(51-200)'
        WHEN rb.avg_room_base <= 1000 THEN 'C: 中底注(201-1000)'
        ELSE 'D: 高底注(>1000)'
    END AS room_base_group,
    COUNT(DISTINCT reg.uid) AS user_count,
    ROUND(AVG(rb.avg_room_base), 0) AS avg_room_base,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT reg.uid), 2) AS login_d1,
    ROUND(SUM(lr.login_d7) * 100.0 / COUNT(DISTINCT reg.uid), 2) AS login_d7,
    ROUND(SUM(gr.game_d1) * 100.0 / COUNT(DISTINCT reg.uid), 2) AS game_d1,
    ROUND(SUM(gr.game_d7) * 100.0 / COUNT(DISTINCT reg.uid), 2) AS game_d7
FROM reg_base reg
INNER JOIN room_base rb ON reg.uid = rb.uid AND reg.reg_date = rb.dt
LEFT JOIN login_ret lr ON reg.uid = lr.uid
LEFT JOIN game_ret gr ON reg.uid = gr.uid
GROUP BY 1
ORDER BY 1;
```

---

## 五、高危信号组合

> 多维交叉识别最高危流失组合。

### 5.1 连败 × 亏损 × 高倍输

三重高危信号叠加：连败≥3 且银子大亏且经历高倍局失败。

```sql
WITH reg_base AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
silver_stat AS (
    SELECT
        rb.uid, rb.reg_date,
        gs.max_lose_streak,
        gs.total_diff_money,
        gs.game_count
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
),
allgame_stat AS (
    SELECT
        rb.uid, rb.reg_date,
        -- 🌟 高倍 >24x 输局数 = 5 个高倍区间的 lose 之和（multi_q4_lose_count 已废弃，详见表 v1.2）
        SUM(
            COALESCE(gs.multi_24_48_lose, 0) + COALESCE(gs.multi_48_96_lose, 0)
          + COALESCE(gs.multi_96_192_lose, 0) + COALESCE(gs.multi_192_384_lose, 0)
          + COALESCE(gs.multi_384_plus_lose, 0)
        ) AS high_multi_losses
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_allgame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
    GROUP BY rb.uid, rb.reg_date
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS login_d7
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
        )
    GROUP BY rb.uid
),
game_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS game_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS game_d7
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = rb.app_id AND a.uid = rb.uid
        AND a.dt IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY)
        )
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN COALESCE(ss.game_count, 0) = 0 THEN '0: 无对局'
        WHEN ss.max_lose_streak >= 3
             AND ss.total_diff_money < -10000
             AND COALESCE(ags.high_multi_losses, 0) > 0
            THEN 'A: 连败3+×大亏×高倍输'
        WHEN ss.max_lose_streak >= 3 AND ss.total_diff_money < -10000
            THEN 'B: 连败3+×大亏'
        WHEN ss.max_lose_streak >= 3
            THEN 'C: 连败3+'
        WHEN ss.total_diff_money < -10000
            THEN 'D: 大亏'
        ELSE 'E: 正常'
    END AS risk_group,
    COUNT(DISTINCT rb.uid) AS user_count,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1,
    ROUND(SUM(lr.login_d7) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d7,
    ROUND(SUM(gr.game_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS game_d1,
    ROUND(SUM(gr.game_d7) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS game_d7
FROM reg_base rb
LEFT JOIN silver_stat ss ON rb.uid = ss.uid AND rb.reg_date = ss.reg_date
LEFT JOIN allgame_stat ags ON rb.uid = ags.uid AND rb.reg_date = ags.reg_date
LEFT JOIN login_ret lr ON rb.uid = lr.uid
LEFT JOIN game_ret gr ON rb.uid = gr.uid
GROUP BY 1
ORDER BY 1;
```

### 5.2 首局负 × 地主 × 高倍局

首局即输且担任地主且经历高倍，三重负面体验叠加。

```sql
WITH first_game AS (
    SELECT
        g.uid, g.dt,
        MIN_BY(g.result_id, g.game_datetime) AS first_game_result,
        MIN_BY(g.role, g.game_datetime) AS first_game_role,
        MIN_BY(g.magnification, g.game_datetime) AS first_game_magnification
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-03-01' AND '2026-06-21'
      AND g.robot != 1
      AND g.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND g.play_mode IN (1, 2, 3, 5)
    GROUP BY g.uid, g.dt
),
reg_base AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN fg.first_game_result = 2 AND fg.first_game_role = 1 AND fg.first_game_magnification > 24
            THEN 'A: 首局负×地主×高倍'
        WHEN fg.first_game_result = 2 AND fg.first_game_role = 1
            THEN 'B: 首局负×地主'
        WHEN fg.first_game_result = 2
            THEN 'C: 首局负(农民)'
        WHEN fg.first_game_result = 1
            THEN 'D: 首局胜'
        ELSE 'E: 无对局或非经典玩法'
    END AS first_game_risk,
    COUNT(DISTINCT rb.uid) AS user_count,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1
FROM reg_base rb
LEFT JOIN first_game fg ON rb.uid = fg.uid AND rb.reg_date = fg.dt
LEFT JOIN login_ret lr ON rb.uid = lr.uid
GROUP BY 1
ORDER BY 1;
```

### 5.3 零对局 × 高频登录

注册当天登录多次但未进行任何对局，可能是客户端问题（崩溃/卡死）导致无法进入游戏。

```sql
WITH reg_base AS (
    SELECT
        r.uid, r.reg_date, r.app_id,
        r.first_day_login_cnt
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
game_stat AS (
    SELECT
        rb.uid, rb.reg_date,
        COALESCE(gs.game_count, 0) AS game_count
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_silvergame_stat gs
        ON gs.app_id = rb.app_id AND gs.uid = rb.uid AND gs.dt = rb.reg_date
),
login_ret AS (
    SELECT
        rb.uid,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
    GROUP BY rb.uid
)
SELECT
    CASE
        WHEN gs.game_count = 0 AND rb.first_day_login_cnt >= 3 THEN 'A: 0局×登录3+次(崩溃)'
        WHEN gs.game_count = 0 AND rb.first_day_login_cnt = 2 THEN 'B: 0局×登录2次'
        WHEN gs.game_count = 0 THEN 'C: 0局×登录1次'
        WHEN gs.game_count > 0 AND rb.first_day_login_cnt >= 3 THEN 'D: 有对局×高频登录'
        ELSE 'E: 有对局×正常登录'
    END AS crash_risk_group,
    COUNT(DISTINCT rb.uid) AS user_count,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1
FROM reg_base rb
LEFT JOIN game_stat gs ON rb.uid = gs.uid AND rb.reg_date = gs.reg_date
LEFT JOIN login_ret lr ON rb.uid = lr.uid
GROUP BY 1
ORDER BY 1;
```

---

## 六、游戏留存与登录留存对比

> 新增章节：双重口径对比，识别"登录但不游戏"的用户占比。

登录留存和游戏留存之间的差值反映"只看不玩"的用户群体。若差值持续扩大，说明用户虽然打开应用但未进入游戏，需排查登录后的体验卡点。

### 6.1 每日留存对比曲线

```sql
WITH reg_base AS (
    SELECT
        r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
login_ret AS (
    SELECT
        rb.uid, rb.reg_date,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS login_d3,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS login_d7,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 14 DAY) THEN 1 ELSE 0 END) AS login_d14,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 30 DAY) THEN 1 ELSE 0 END) AS login_d30
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 3 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 14 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 30 DAY)
        )
    GROUP BY rb.uid, rb.reg_date
),
game_ret AS (
    SELECT
        rb.uid, rb.reg_date,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS game_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS game_d3,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 7 DAY) THEN 1 ELSE 0 END) AS game_d7,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 14 DAY) THEN 1 ELSE 0 END) AS game_d14,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 30 DAY) THEN 1 ELSE 0 END) AS game_d30
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = rb.app_id AND a.uid = rb.uid
        AND a.dt IN (
            DATE_ADD(rb.reg_date, INTERVAL 1 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 3 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 7 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 14 DAY),
            DATE_ADD(rb.reg_date, INTERVAL 30 DAY)
        )
    GROUP BY rb.uid, rb.reg_date
)
SELECT
    rb.reg_date,
    COUNT(DISTINCT rb.uid) AS reg_users,
    ROUND(SUM(lr.login_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d1,
    ROUND(SUM(gr.game_d1) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS game_d1,
    ROUND((SUM(lr.login_d1) - SUM(gr.game_d1)) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS gap_d1,
    ROUND(SUM(lr.login_d7) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d7,
    ROUND(SUM(gr.game_d7) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS game_d7,
    ROUND((SUM(lr.login_d7) - SUM(gr.game_d7)) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS gap_d7,
    ROUND(SUM(lr.login_d30) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS login_d30,
    ROUND(SUM(gr.game_d30) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS game_d30,
    ROUND((SUM(lr.login_d30) - SUM(gr.game_d30)) * 100.0 / COUNT(DISTINCT rb.uid), 2) AS gap_d30
FROM reg_base rb
LEFT JOIN login_ret lr ON rb.uid = lr.uid AND rb.reg_date = lr.reg_date
LEFT JOIN game_ret gr ON rb.uid = gr.uid AND rb.reg_date = gr.reg_date
GROUP BY rb.reg_date
ORDER BY rb.reg_date;
```

### 6.2 登录但未游戏用户画像

识别"登录留存 - 游戏留存"差值中的用户特征，了解这部分用户是谁、来自什么渠道。

```sql
WITH reg_base AS (
    SELECT
        r.uid, r.reg_date, r.app_id,
        r.channel_category_name,
        r.reg_app_code,
        CASE WHEN r.reg_group_id IN (8, 88) THEN 'iOS' ELSE 'Android' END AS platform
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-03-01' AND '2026-06-21'
),
user_ret AS (
    SELECT
        rb.uid, rb.reg_date,
        MAX(CASE WHEN l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS login_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(rb.reg_date, INTERVAL 1 DAY) THEN 1 ELSE 0 END) AS game_d1,
        rb.channel_category_name,
        rb.reg_app_code,
        rb.platform
    FROM reg_base rb
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = rb.app_id AND l.uid = rb.uid
        AND l.login_date = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = rb.app_id AND a.uid = rb.uid
        AND a.dt = DATE_ADD(rb.reg_date, INTERVAL 1 DAY)
    GROUP BY rb.uid, rb.reg_date, rb.channel_category_name, rb.reg_app_code, rb.platform
)
SELECT
    '按渠道' AS dimension,
    channel_category_name AS value,
    COUNT(DISTINCT uid) AS total_users,
    ROUND(SUM(CASE WHEN login_d1 = 1 AND game_d1 = 0 THEN 1 ELSE 0 END) * 100.0
        / NULLIF(SUM(login_d1), 0), 2) AS login_no_game_pct
FROM user_ret
WHERE login_d1 = 1
GROUP BY channel_category_name
UNION ALL
SELECT
    '按平台' AS dimension,
    platform AS value,
    COUNT(DISTINCT uid) AS total_users,
    ROUND(SUM(CASE WHEN login_d1 = 1 AND game_d1 = 0 THEN 1 ELSE 0 END) * 100.0
        / NULLIF(SUM(login_d1), 0), 2) AS login_no_game_pct
FROM user_ret
WHERE login_d1 = 1
GROUP BY platform
UNION ALL
SELECT
    '按客户端' AS dimension,
    reg_app_code AS value,
    COUNT(DISTINCT uid) AS total_users,
    ROUND(SUM(CASE WHEN login_d1 = 1 AND game_d1 = 0 THEN 1 ELSE 0 END) * 100.0
        / NULLIF(SUM(login_d1), 0), 2) AS login_no_game_pct
FROM user_ret
WHERE login_d1 = 1
GROUP BY reg_app_code
ORDER BY dimension, value;
```

### 6.3 留存口径差异分析说明

| 对比维度 | 登录留存 | 游戏留存 | 差异含义 |
| ---- | ---- | ---- | ---- |
| **计算口径** | 基于 `dws_dq_daily_login` | 基于 `dws_app_game_active` | 游戏留存更严格 |
| **反映行为** | 用户打开应用 | 用户完成对局 | 登录-游戏差值 = 打开未游戏 |
| **正常范围** | 通常高于游戏留存 | 低于登录留存 | 5-15% 差值为正常范围 |
| **差值扩大** | — | — | 需排查登录到游戏流程中的卡点 |
| **差值缩小** | — | — | 用户更加"目标明确"地玩游戏 |

---

## 七、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| ---- | ---- | ---- |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数/胜率/经济差异 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |
| [retention-financial.md](retention-financial.md) | 银子经济层 | 金流归因与留存的关系 |
| [retention-score-game.md](retention-score-game.md) | 积分玩法层 | 积分参与度与留存 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 1局用户、客户端问题、渠道质量 |

> **分析框架速查**：[retention-analysis-framework.md](retention-analysis-framework.md)
> **上一版本**：[retention-global.md](retention-global.md)（v1.0，基于旧表结构）
