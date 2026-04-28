# 斗地主分玩法留存分析方案

> 本文档是 [`retention-global.md`](retention-global.md) 的补充延伸，专注于按**经典 / 不洗牌 / 癞子**三种玩法维度拆分后的新增用户留存分析。所有 SQL 基于 StarRocks 语法，表结构与主文档一致。
>
> **共享基础**：本文档共享全局文档的 **一~七章**（业务背景、数据基础、指标体系、分析方法论），此处不再重复。如需查阅指标定义或方法论，请参见全局文档。
>
> **覆盖范围**：本文档承接全局文档中"游戏模式偏好"相关的分析职责，包含各玩法留存对比、玩法内因子分析、玩法行为分析等全部分玩法维度内容。

---

## 目录

1. [分析背景](#一分析背景)
2. [玩法映射关系](#二玩法映射关系)
3. [分析框架与维度设计](#三分析框架与维度设计)
4. [基础数据准备](#四基础数据准备)
5. [分析SQL](#五分析sql)
6. [分析思路与预期产出](#六分析思路与预期产出)

---

## 一、分析背景

### 1.1 为什么需要分玩法分析

在主文档的倍数维度分析（8.3）中已发现：

- **70.5% 的新用户**首日平均理论倍数处于 24+ 的极高倍区间
- 倍数与留存呈**倒 U 型关系**，12-24x 留存最优而非最高倍

三种玩法在倍数机制上存在结构性差异：

| 玩法 | 倍数特点 | 预期影响 |
|------|---------|---------|
| 经典 | 标准倍数机制，新手默认进入 | 基线水平，最大用户群 |
| 不洗牌 | 牌序延续，连续好牌/差牌概率更高，倍数可能更极端 | 波动大，高倍局更频繁 |
| 癞子 | 万能牌存在，炸弹概率大幅增加，公共倍数普遍偏高 | 倍数天然更高，经济波动剧烈 |

如果不区分玩法，整体分析可能被**玩法混合效应**干扰——例如癞子玩法的高倍特征可能拉高整体均值，掩盖经典玩法的真实留存规律。

### 1.2 核心问题

1. 三种玩法的留存率是否存在显著差异？
2. 同一留存因子（如倍数、胜率、对局数）在不同玩法下的影响是否一致？
3. 首日玩法选择和玩法切换行为对留存有何影响？
4. 各玩法是否需要差异化的新手保护策略？

---

## 二、玩法映射关系

### 2.1 room_id → 玩法映射

```sql
CASE 
    WHEN room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN 1 -- 经典
    WHEN room_id IN (421, 22039, 22040, 22041, 22042)                  THEN 2 -- 不洗牌
    WHEN room_id IN (13176, 13177, 13178)                              THEN 3 -- 癞子
    WHEN room_id IN (158, 159)                                         THEN 6 -- 好友房
    ELSE 0
END AS play_mode
```

### 2.2 玩法 room_id 列表

| 玩法 | room_id 列表 | 房间数量 |
|------|-------------|---------|
| 经典 | 742, 420, 4484, 12074, 6314, 11168, 10336, 16445 | 8 |
| 不洗牌 | 421, 22039, 22040, 22041, 22042 | 5 |
| 癞子 | 13176, 13177, 13178 | 3 |

> **注意**：不在上述映射内的 room_id 归入「其他」分类，分析时需关注其占比。若占比过高需补充映射。

---

## 三、分析框架与维度设计

### 3.1 分析结构总览

```
├── 第一层：玩法级概览
│   ├── M-01: 各玩法新增用户留存率（整体对比）
│   ├── M-02: 玩法参与分布与主玩法留存对比
│   └── M-03: 玩法数量与留存（单/多玩法用户对比）
│
├── 第二层：玩法内因子分析（分别在经典/不洗牌/癞子内做）
│   ├── M-04: 分玩法 × 倍数分组留存
│   ├── M-05: 分玩法 × 胜率分组留存
│   ├── M-06: 分玩法 × 对局数分组留存
│   ├── M-07: 分玩法 × 经济变化分组留存
│   └── M-08: 分玩法 × 高倍局体验留存
│
├── 第三层：玩法行为分析
│   ├── M-09: 首局玩法选择与留存
│   └── M-10: 玩法切换路径与留存
│
└── 第四层：多维交叉
    └── M-11: 玩法 × 倍数 × 胜率 三维交叉留存
```

### 3.2 留存口径说明

与主文档一致采用**新增用户留存**口径（分母为当日新增的所有 APP 端用户，不要求注册日有对局）：

| 概念 | 定义 |
|------|------|
| 分母 | 当日新增的 APP 端用户数（所有注册用户，不要求注册日有对局） |
| 分子（整体留存） | 第 N 日在**任意玩法**有对局的用户数（用 `dws_app_game_active` 判定） |
| 分子（玩法留存） | 第 N 日在**同一玩法**有对局的用户数（用 `dws_app_gamemode_active` 判定） |

> **口径说明**：本层留存的分子用"对局"判定（而非 global / client-lang 用的"登录"判定），是因为"同玩法留存"必须通过对局才能识别玩法归属。三层文档分母统一为新增用户，分子数据源不同，因此同一批用户的整体留存数字在 global 与 by-mode 之间会有差异（登录用户 ≥ 有对局用户），这是预期的。

M-01 同时输出「整体留存」和「玩法留存」两个指标，便于区分用户是"回来玩了别的"还是"回来玩了同一个玩法"。

> **CTE 实现注意**：由于分母为"所有新增用户"（含注册无对局者），`ddz_gamemode_firstday_features` 宽表通过 `INNER JOIN first_day_games_raw` 只保留了"有对局"的用户。分析时如需严格按新增用户作分母，应从 `dws_dq_app_daily_reg` 出发 LEFT JOIN 宽表；目前宽表驱动的分析 SQL（M-01~M-11）的分母实际是"新增且有对局"，此为已知限制。

---

## 四、基础数据准备

> 分析时间段：**20260210 至 20260422**。
> 过滤条件：仅限 APP 端，排除积分/比赛房。所有分析依赖轻量级的 DWS 聚合。

### M-00: 分玩法首日对局特征宽表（DWS 优化）

**DWS 依赖结构：**

```
Step A: tcy_temp.dws_dq_app_daily_reg       （APP 端注册用户宽表）
Step B: tcy_temp.dws_ddz_daily_game         （对局战绩统一字段表，用于首日宽表计算）
Step C: tcy_temp.dws_app_game_active        （每日游戏活跃用户表，整体留存 flag 用）  → 见 dws/dws_app_game_active.md
Step D: tcy_temp.dws_app_gamemode_active    （每日游戏活跃用户×玩法表，同玩法留存 flag 用）  → 见 dws/dws_app_gamemode_active.md
→ 输出：tcy_temp.ddz_gamemode_firstday_features
```

**关键说明：**

- `dws_app_game_active` / `dws_app_gamemode_active` 为预构建的轻量 DWS 表，执行本 SQL 前需确认已构建完成
- 倍数相关字段（`grab_landlord_bet`/`complete_victory_bet`/`bomb_bet`）在 `dwd_game_combat_si` 中为**独立列**，直接使用
- 货币字段直接读 `dws_ddz_daily_game` 中的统一字段：`room_base`/`room_fee`/`start_money`/`end_money`/`diff_money_pre_tax`
- `dws_app_gamemode_active` 的玩法字段为 `play_mode`（整数），宽表 `ddz_gamemode_firstday_features` 统一使用 `play_mode` 整数，与 `dws_ddz_daily_game` 保持一致

```sql
CREATE TABLE tcy_temp.ddz_gamemode_firstday_features (
  `app_id` int(11) NULL COMMENT "应用ID",
  `uid` int(11) NULL COMMENT "用户ID", 
  `play_mode` tinyint(4) NULL COMMENT "玩法模式",
  `reg_date` date NULL COMMENT "注册日期",
  `reg_group_id` int(11) NULL COMMENT "注册组ID",
  `channel_category` varchar(255) NULL COMMENT "渠道分类",
  `channel_category_tag_id` tinyint NULL COMMENT "渠道标签ID",
  `game_count` int(11) NOT NULL COMMENT "当日对局数",
  `total_play_seconds` int(11) NULL COMMENT "累计时长(秒)",
  `avg_timecost` double NULL COMMENT "平均单局时长",
  `avg_magnification` double NULL COMMENT "平均倍数",
  `max_magnification` int(11) NULL COMMENT "最大倍数",
  `total_diff_money` bigint(20) NULL COMMENT "输赢总额",
  `total_room_fee` bigint(20) NULL COMMENT "台费消耗",
  `escape_count` int(11) NULL COMMENT "逃跑次数",
  `first_mode_game_result` tinyint NULL COMMENT "该模式首局结果",
  `last_mode_game_result` tinyint NULL COMMENT "该模式末局结果",
  `win_count` int(11) NULL COMMENT "胜局数",
  `win_rate` double NULL COMMENT "胜率",
  `max_win_streak` int(11) NULL COMMENT "最大连胜",
  `max_lose_streak` int(11) NULL COMMENT "最大连负",
  `first_day_mode_count` int(11) NULL COMMENT "当日玩过的总模式数",
  `first_global_play_mode` tinyint(4) NULL COMMENT "全天第一局玩法",
  `is_retained_day1_same_mode` tinyint(4) NULL COMMENT "同玩法次留",
  `is_retained_day7_same_mode` tinyint(4) NULL COMMENT "同玩法7留",
  `is_retained_day30_same_mode` tinyint(4) NULL COMMENT "同玩法30留",
  `is_retained_day1_global` tinyint(4) NULL COMMENT "全盘次留",
  `is_retained_day7_global` tinyint(4) NULL COMMENT "全盘7留",
  `is_retained_day30_global` tinyint(4) NULL COMMENT "全盘30留"
) ENGINE=OLAP 
DUPLICATE KEY(`app_id`, `uid`, `play_mode`, `reg_date`)
COMMENT "新用户注册首日玩法特征表"
PARTITION BY RANGE(`reg_date`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80", 
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);

-- 分玩法首日宽表（核心分析数据集）
insert into tcy_temp.ddz_gamemode_firstday_features
WITH new_user_reg AS (
    SELECT uid, app_id, reg_date, reg_group_id, channel_category_name, channel_category_tag_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053 AND reg_date BETWEEN '2026-02-10' AND '2026-04-22'
),
first_day_games_raw AS (
    SELECT
        c.app_id, c.uid, c.play_mode, c.result_id, c.timecost, c.magnification,
        c.magnification_stacked, c.room_base, c.room_fee, c.diff_money_pre_tax, c.cut,
        ROW_NUMBER() OVER (PARTITION BY c.uid, c.play_mode ORDER BY c.game_datetime) AS mode_game_seq,
        ROW_NUMBER() OVER (PARTITION BY c.uid, c.play_mode ORDER BY c.game_datetime DESC) AS mode_game_seq_desc,
        ROW_NUMBER() OVER (PARTITION BY c.uid ORDER BY c.game_datetime) AS global_game_seq
    FROM tcy_temp.dws_ddz_firstday_game c
    INNER JOIN new_user_reg r ON c.app_id = r.app_id AND c.uid = r.uid AND c.dt = r.reg_date
    WHERE c.app_id = 1880053 
      AND c.dt BETWEEN '2026-02-10' AND '2026-04-22'
      AND c.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
),
mode_streaks AS (
    SELECT uid, play_mode,
           MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
           MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT uid, play_mode, result_id, COUNT(*) AS streak_len
        FROM (
            SELECT uid, play_mode, result_id, mode_game_seq,
                   mode_game_seq - ROW_NUMBER() OVER (PARTITION BY uid, play_mode, result_id ORDER BY mode_game_seq) AS grp
            FROM first_day_games_raw
            WHERE result_id IN (1, 2)
        ) t GROUP BY uid, play_mode, result_id, grp
    ) t2 GROUP BY uid, play_mode
),
day_flags_global AS (
    SELECT 
        r.uid,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)   THEN 1 ELSE 0 END) AS is_ret_d1_global,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS is_ret_d7_global,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS is_ret_d30_global
    FROM new_user_reg r
    INNER JOIN tcy_temp.dws_app_game_active a ON r.app_id = a.app_id AND r.uid = a.uid
    WHERE a.dt > r.reg_date 
      AND a.dt <= '2026-05-30' 
    GROUP BY r.uid
),
day_flags_agg AS (
    SELECT 
        r.uid,
        a.play_mode,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)   THEN 1 ELSE 0 END) AS is_ret_d1,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 6 DAY)  THEN 1 ELSE 0 END) AS is_ret_d7,
        MAX(CASE WHEN a.dt = DATE_ADD(r.reg_date, INTERVAL 29 DAY) THEN 1 ELSE 0 END) AS is_ret_d30
    FROM new_user_reg r
    INNER JOIN tcy_temp.dws_app_gamemode_active a ON r.app_id = a.app_id AND r.uid = a.uid
    WHERE a.app_id = 1880053
      AND a.dt IN (r.reg_date, DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY), DATE_ADD(r.reg_date, INTERVAL 29 DAY))
    GROUP BY r.uid, a.play_mode
),
uid_mode_meta AS (
    SELECT
        uid,
        COUNT(DISTINCT play_mode)           AS first_day_mode_count,
        MIN_BY(play_mode, global_game_seq)  AS first_global_play_mode
    FROM first_day_games_raw
    GROUP BY uid
)
SELECT
    g.app_id, r.uid, g.play_mode, r.reg_date, r.reg_group_id,
    r.channel_category_name AS channel_category, r.channel_category_tag_id,
    -- 对局量与时长
    COUNT(*)                                                               AS game_count,
    SUM(g.timecost)                                                        AS total_play_seconds,
    ROUND(SUM(g.timecost) * 1.0 / NULLIF(COUNT(*), 0), 1)                 AS avg_timecost,
    -- 倍数
    ROUND(AVG(g.magnification), 2)                                         AS avg_magnification,
    MAX(g.magnification)                                                   AS max_magnification,
    -- 经济
    SUM(g.diff_money_pre_tax)                                              AS total_diff_money,
    SUM(g.room_fee)                                                        AS total_room_fee,
    -- 逃跑
    SUM(CASE WHEN g.cut < 0 THEN 1 ELSE 0 END)                            AS escape_count,
    -- 首末局结果
    MIN(CASE WHEN g.mode_game_seq = 1 THEN g.result_id END)               AS first_mode_game_result,
    MAX(CASE WHEN g.mode_game_seq_desc = 1 THEN g.result_id END)          AS last_mode_game_result,
    -- 胜负
    SUM(CASE WHEN g.result_id = 1 THEN 1 ELSE 0 END)                      AS win_count,
    ROUND(SUM(CASE WHEN g.result_id = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS win_rate,
    COALESCE(MAX(ms.max_win_streak), 0)                                    AS max_win_streak,
    COALESCE(MAX(ms.max_lose_streak), 0)                                   AS max_lose_streak,
    -- 玩法探索（uid 级，每行重复同一值）
    MAX(um.first_day_mode_count)                                           AS first_day_mode_count,
    MAX(um.first_global_play_mode)                                         AS first_global_play_mode,
    -- 同玩法留存
    COALESCE(MAX(df.is_ret_d1), 0)                                         AS is_retained_day1_same_mode,
    COALESCE(MAX(df.is_ret_d7), 0)                                         AS is_retained_day7_same_mode,
    COALESCE(MAX(df.is_ret_d30), 0)                                        AS is_retained_day30_same_mode,
    -- 整体留存
    COALESCE(MAX(dfg.is_ret_d1_global), 0)                                 AS is_retained_day1_global,
    COALESCE(MAX(dfg.is_ret_d7_global), 0)                                 AS is_retained_day7_global,
    COALESCE(MAX(dfg.is_ret_d30_global), 0)                                AS is_retained_day30_global
FROM first_day_games_raw g
INNER JOIN new_user_reg r ON g.uid = r.uid
LEFT JOIN mode_streaks ms ON g.uid = ms.uid AND g.play_mode = ms.play_mode
LEFT JOIN day_flags_agg df ON g.uid = df.uid AND g.play_mode = df.play_mode
LEFT JOIN day_flags_global dfg ON g.uid = dfg.uid
LEFT JOIN uid_mode_meta um ON g.uid = um.uid
GROUP BY g.app_id, r.uid,  g.play_mode, r.reg_date, r.reg_group_id, r.channel_category_name, r.channel_category_tag_id;
```

---

## 五、分析SQL

> 以下所有 SQL 均基于 `tcy_temp.ddz_gamemode_firstday_features` 宽表。
> M-01 同时输出同玩法留存和整体留存，M-02～M-08 主要考察**同玩法留存**（`is_retained_day1_same_mode`）。
> M-09/M-10（首局玩法选择、玩法切换路径）需要全局首局所在玩法字段，宽表暂不支持，待补充。

### M-01: 各玩法新增用户留存率（含渠道拆分）

```sql
SELECT
    play_mode,
    channel_category,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games_in_mode,
    ROUND(AVG(avg_magnification), 1) AS avg_multi,
    -- 同玩法留存（在同一玩法有对局）
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate_same_mode,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate_same_mode,
    ROUND(SUM(is_retained_day30_same_mode) * 100.0 / COUNT(*), 2) AS day30_rate_same_mode,
    -- 整体留存（任意玩法有对局）
    ROUND(SUM(is_retained_day1_global) * 100.0 / COUNT(*), 2) AS day1_rate_global,
    ROUND(SUM(is_retained_day7_global) * 100.0 / COUNT(*), 2) AS day7_rate_global,
    ROUND(SUM(is_retained_day30_global) * 100.0 / COUNT(*), 2) AS day30_rate_global
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)  -- 1=经典，2=不洗牌，3=癞子
GROUP BY play_mode, channel_category
ORDER BY play_mode, channel_category;
```

### M-02: 玩法参与分布与主玩法留存对比

```sql
-- first_day_mode_count / first_global_play_mode 已预计算，无需子查询
SELECT 
    main_play_mode,
    first_day_mode_count,
    COUNT(DISTINCT uid) AS user_count,
    -- 在外层进行汇总计算
    ROUND(SUM(is_ret_d1_same) * 100.0 / COUNT(DISTINCT uid), 2) AS day1_rate_same_mode,
    ROUND(SUM(is_ret_d7_same) * 100.0 / COUNT(DISTINCT uid), 2) AS day7_rate_same_mode,
    ROUND(SUM(is_ret_d1_global) * 100.0 / COUNT(DISTINCT uid), 2) AS day1_rate_global
FROM (
    -- 第一步：子查询算出每个 uid 的主玩法状态
    SELECT
        uid,
        reg_date,
        first_day_mode_count,
        MAX_BY(play_mode, game_count) AS main_play_mode,
        MAX_BY(is_retained_day1_same_mode, game_count) AS is_ret_d1_same,
        MAX_BY(is_retained_day7_same_mode, game_count) AS is_ret_d7_same,
        MAX(is_retained_day1_global) AS is_ret_d1_global  -- 全局留存只要任意记录为1即为1
    FROM tcy_temp.ddz_gamemode_firstday_features
    WHERE play_mode IN (1, 2, 3)
    GROUP BY uid, reg_date, first_day_mode_count
) t
GROUP BY main_play_mode, first_day_mode_count; -- 第二步：按玩法汇总
```

### M-03: 玩法数量与留存（单玩法 vs 多玩法用户对比）

```sql
-- first_day_mode_count 已预计算，uid 去重后直接聚合
SELECT
    first_day_mode_count,
    COUNT(*) AS user_count,
    ROUND(SUM(is_ret_d1_global) * 100.0 / COUNT(*), 2) AS day1_rate_global,
    ROUND(SUM(is_ret_d7_global) * 100.0 / COUNT(*), 2) AS day7_rate_global,
    ROUND(SUM(is_ret_d30_global) * 100.0 / COUNT(*), 2) AS day30_rate_global
FROM (
    SELECT uid, first_day_mode_count,
           MAX(is_retained_day1_global)  AS is_ret_d1_global,
           MAX(is_retained_day7_global)  AS is_ret_d7_global,
           MAX(is_retained_day30_global) AS is_ret_d30_global
    FROM tcy_temp.ddz_gamemode_firstday_features
    WHERE play_mode IN (1, 2, 3)
    GROUP BY uid, first_day_mode_count
) t
GROUP BY first_day_mode_count
ORDER BY first_day_mode_count;
```

### M-04: 分玩法 × 倍数分组留存

```sql
SELECT
    play_mode, channel_category,
    CASE
        WHEN avg_magnification <= 6  THEN 'A: <=6'
        WHEN avg_magnification <= 12 THEN 'B: 6-12'
        WHEN avg_magnification <= 24 THEN 'C: 12-24'
        WHEN avg_magnification <= 48 THEN 'D: 24-48'
        ELSE                              'E: 48+'
    END AS multi_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)
GROUP BY play_mode, channel_category,
    CASE
        WHEN avg_magnification <= 6  THEN 'A: <=6'
        WHEN avg_magnification <= 12 THEN 'B: 6-12'
        WHEN avg_magnification <= 24 THEN 'C: 12-24'
        WHEN avg_magnification <= 48 THEN 'D: 24-48'
        ELSE                              'E: 48+'
    END
ORDER BY play_mode, channel_category, multi_group;
```

### M-05: 分玩法 × 胜率分组留存

```sql
SELECT
    play_mode, channel_category,
    CASE
        WHEN win_rate < 30 THEN 'A: <30%'
        WHEN win_rate < 40 THEN 'B: 30-40%'
        WHEN win_rate < 50 THEN 'C: 40-50%'
        WHEN win_rate < 60 THEN 'D: 50-60%'
        ELSE                    'E: 60%+'
    END AS winrate_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)
GROUP BY play_mode, channel_category,
    CASE
        WHEN win_rate < 30 THEN 'A: <30%'
        WHEN win_rate < 40 THEN 'B: 30-40%'
        WHEN win_rate < 50 THEN 'C: 40-50%'
        WHEN win_rate < 60 THEN 'D: 50-60%'
        ELSE                    'E: 60%+'
    END
ORDER BY play_mode, channel_category, winrate_group;
```

### M-06: 分玩法 × 对局数分组留存

```sql
SELECT
    play_mode, channel_category,
    CASE
        WHEN game_count = 1   THEN 'A: 1局'
        WHEN game_count <= 3  THEN 'B: 2-3局'
        WHEN game_count <= 5  THEN 'C: 4-5局'
        WHEN game_count <= 10 THEN 'D: 6-10局'
        ELSE                       'E: 10局+'
    END AS game_count_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)
GROUP BY play_mode, channel_category,
    CASE
        WHEN game_count = 1   THEN 'A: 1局'
        WHEN game_count <= 3  THEN 'B: 2-3局'
        WHEN game_count <= 5  THEN 'C: 4-5局'
        WHEN game_count <= 10 THEN 'D: 6-10局'
        ELSE                       'E: 10局+'
    END
ORDER BY play_mode, channel_category, game_count_group;
```

### M-07: 分玩法 × 经济变化分组留存

```sql
SELECT
    play_mode, channel_category,
    CASE
        WHEN total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN total_diff_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN total_diff_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN total_diff_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                'F: 巨赚 (>5万)'
    END AS money_change_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)
GROUP BY play_mode, channel_category,
    CASE
        WHEN total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN total_diff_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN total_diff_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN total_diff_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                'F: 巨赚 (>5万)'
    END
ORDER BY play_mode, channel_category, money_change_group;
```

### M-08: 分玩法 × 最大连败分组留存

```sql
SELECT
    play_mode, channel_category,
    CASE
        WHEN max_lose_streak = 0  THEN 'A: 无连败'
        WHEN max_lose_streak <= 2 THEN 'B: 1-2连败'
        WHEN max_lose_streak <= 5 THEN 'C: 3-5连败'
        WHEN max_lose_streak <= 9 THEN 'D: 6-9连败'
        ELSE                           'E: 10连败+'
    END AS lose_streak_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)
GROUP BY play_mode, channel_category,
    CASE
        WHEN max_lose_streak = 0  THEN 'A: 无连败'
        WHEN max_lose_streak <= 2 THEN 'B: 1-2连败'
        WHEN max_lose_streak <= 5 THEN 'C: 3-5连败'
        WHEN max_lose_streak <= 9 THEN 'D: 6-9连败'
        ELSE                           'E: 10连败+'
    END
ORDER BY play_mode, channel_category, lose_streak_group;
```

### M-09: 玩法 × 倍数 × 胜率 三维交叉留存

```sql
SELECT
    play_mode,
    CASE
        WHEN avg_magnification <= 12 THEN 'A: <=12'
        WHEN avg_magnification <= 24 THEN 'B: 12-24'
        WHEN avg_magnification <= 48 THEN 'C: 24-48'
        ELSE                              'D: 48+'
    END AS multi_group,
    CASE
        WHEN win_rate < 40 THEN 'L: <40%'
        WHEN win_rate < 50 THEN 'M: 40-50%'
        ELSE                    'H: 50%+'
    END AS winrate_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)
GROUP BY play_mode,
    CASE
        WHEN avg_magnification <= 12 THEN 'A: <=12'
        WHEN avg_magnification <= 24 THEN 'B: 12-24'
        WHEN avg_magnification <= 48 THEN 'C: 24-48'
        ELSE                              'D: 48+'
    END,
    CASE
        WHEN win_rate < 40 THEN 'L: <40%'
        WHEN win_rate < 50 THEN 'M: 40-50%'
        ELSE                    'H: 50%+'
    END
HAVING COUNT(*) >= 30  -- 过滤样本过少的分组
ORDER BY play_mode, multi_group, winrate_group;
```

### M-10: 首局玩法选择与留存

```sql
-- first_global_play_mode 已预计算，直接按"首局是否为本玩法"拆分
-- 每个 uid 只保留一行（避免重复计入）：取对局数最多的那个玩法行代表该用户
SELECT
    first_global_play_mode,
    play_mode,
    CASE WHEN play_mode = first_global_play_mode THEN '是' ELSE '否' END AS is_first_mode,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate_same_mode,
    ROUND(SUM(is_retained_day1_global)    * 100.0 / COUNT(*), 2) AS day1_rate_global,
    ROUND(SUM(is_retained_day7_global)    * 100.0 / COUNT(*), 2) AS day7_rate_global
FROM tcy_temp.ddz_gamemode_firstday_features
WHERE play_mode IN (1, 2, 3)
  AND first_global_play_mode IN (1, 2, 3)
GROUP BY first_global_play_mode, play_mode, is_first_mode
ORDER BY first_global_play_mode, play_mode;
```

---

## 六、分析思路与预期产出

### 6.1 分析路径

```
Step 1: 跑 M-00 建宽表
  ↓
Step 2: 跑 M-01 ~ M-03 → 玩法级留存概览
  → 明确各玩法基线留存、用户规模、玩法偏好分布
  ↓
Step 3: 跑 M-04 ~ M-08 → 玩法内因子拆解
  → 每个玩法内部的倍数/胜率/对局数/经济/高倍的留存规律
  → 与整体分析对比: 整体的倒U型规律在分玩法后是否仍成立？
  ↓
Step 4: 跑 M-09 ~ M-10 → 玩法行为分析
  → 首局玩法选择、玩法切换路径对留存的影响
  ↓
Step 5: 跑 M-11 → 多维交叉
  → 各玩法的"最优留存画像"
  ↓
Step 6: 综合结论 → 差异化策略
```

### 6.2 预期核心产出

| 产出项 | 内容 |
|--------|------|
| 各玩法留存基线 | 经典/不洗牌/癞子的 Day1、Day7 留存率对比，含整体留存和同玩法留存 |
| 玩法倍数分布画像 | 各玩法的倍数分位数、高倍局占比、炸弹频率对比 |
| 分玩法最优倍数区间 | 整体的 12-24x 最优是否需要按玩法调整（如癞子可能 24-48x 才是最优） |
| 分玩法留存因子差异 | 同一因子（如胜率、对局数）在不同玩法下的影响力排序 |
| 玩法切换行为洞察 | 多玩法探索用户 vs 单玩法用户的留存差异 |
| 差异化保护策略建议 | 基于数据为各玩法提出针对性的新手保护参数（倍数上限、连败干预阈值等） |

### 6.3 对比分析要点

执行分析时重点关注以下**跨玩法对比**：

| 对比维度 | 核心问题 |
|---------|---------|
| 倍数分布 | 癞子的基线倍数比经典高多少？"低倍"在癞子玩法中是否需要重新定义？ |
| 高倍局占比 | 经典中仅经历高倍局的用户占比 vs 癞子中的占比差距多大？ |
| 最优倍数区间 | 经典的最优是 12-24x，癞子是否右移到 24-48x？ |
| 胜率影响 | 癞子因随机性大，胜率对留存的影响是否弱于经典？ |
| 对局数拐点 | 癞子因单局时间可能更短，"玩够多少局"的留存拐点是否不同？ |
| 经济波动 | 癞子/不洗牌的银子波动是否更剧烈？亏损阈值是否需要玩法差异化？ |
| 首局玩法 | 默认进入经典，如果新手首局就进入癞子/不洗牌，是自主选择还是误操作？留存如何？ |

---

> **文档版本**：v2.2
> **创建日期**：2026-03-25
> **更新说明**：
> - v2.0：重构 DWS 层架构（对齐主文档 v3.0）；修正字段名（`room_base`/`diff_money` 等）；倍数字段改为直接读列；新增 `device_type` 维度；`dws_app_gamemode_active` 时间上限延至 20260508；同玩法留存新增 Day30 指标；修正 `day_flags_mode` 添加 `a.dt > r.reg_date` 限制
> - v2.1：修复 StarRocks 日期函数；优化 Bucket 配置；添加全局留存字段；修正连败/连胜计算；修正首末局特征提取
> - v2.2：补充共享基础声明（明确引用全局文档一~七章）；承接"游戏模式偏好"分析职责（从全局文档迁入）
> - v2.3：DWS 表重命名（`dws_ddz_daily_play_by_mode` → `dws_app_gamemode_active`，`dws_ddz_daily_play` → `dws_app_game_active`）；留存 JOIN 补充 `app_id` 条件
> - v2.4：删除 Step C 内联 CREATE TABLE（`dws_app_gamemode_active` 已独立文档维护）；`new_user_reg` CTE 补充 `app_id` 字段；`day_flags_mode` 修复字段错误
> - v2.5：`first_day_games_raw` 玩法字段从字符串 `game_mode` 改为整数 `play_mode`，与 `dws_ddz_daily_game` 保持一致；同步更新 `mode_streaks`、`day_flags_mode`、最终 SELECT/JOIN/GROUP BY 及分析 SQL M-01~M-03
> - v2.6：`first_day_games_raw` 数据源从 `dwd_game_combat_si` 切换为 `dws_ddz_daily_game`；`play_mode`/字段名/WHERE 过滤均由 DWS 层承担，CTE 大幅简化；`diff_money` → `diff_money_pre_tax`
> - v2.7：补充 CREATE TABLE 中缺失的 `avg_magnification`、`total_diff_money` 字段；统一渠道字段名为 `channel_category`（原 `channel_category_name`）
> - v2.8：补全分析 SQL M-02～M-08、M-11（原仅有 M-01、M-04、M-07）；对现有 M-02/M-03 重新编号为 M-04/M-07 与框架对齐；修正分析段注释（全局留存字段已存在）；说明 M-09/M-10 因宽表缺少全局首局玩法字段暂不实现
> - **v2.9**：宽表补充 6 个字段（`avg_timecost`、`max_magnification`、`total_room_fee`、`escape_count`、`first_day_mode_count`、`first_global_play_mode`）；新增 `uid_mode_meta` CTE；M-02/M-03 SQL 利用预计算字段简化；补充 M-09 首局玩法选择分析；修复 M-11 WHERE 聚合别名问题（改为 HAVING）
>
> **关联文档**：
> - [`retention-global.md`](retention-global.md)（全局分析框架，含共享基础设定）
> - [`retention-by-client-lang.md`](retention-by-client-lang.md)（分客户端语言分析）
> - `dws/dws_dq_app_daily_reg.md`（APP 端注册用户宽表）
> - `dws/dws_ddz_daily_game.md`（对局战绩统一字段表）
>
> **使用说明**：
> 1. 确认 `dws_dq_app_daily_reg`、`dws_app_game_active`、`dws_app_gamemode_active` 已构建
> 2. 执行分析 SQL 进行各维度分析
> 3. 将查询结果填入对应的"查询结果"区域，用于后续分析结论生成
