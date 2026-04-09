# 斗地主分玩法留存分析方案

> 本文档是对 `new-user-retention-analysis.md` 的补充延伸，专注于按**经典 / 不洗牌 / 癞子**三种玩法维度拆分后的新增用户留存分析。所有 SQL 基于 StarRocks 语法，表结构与主文档一致。

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
    WHEN room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN '经典'
    WHEN room_id IN (421, 22039, 22040, 22041, 22042)                  THEN '不洗牌'
    WHEN room_id IN (13176, 13177, 13178)                              THEN '癞子'
    ELSE '其他'
END AS game_mode
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

与主文档一致采用**游戏留存**口径：

| 概念 | 定义 |
|------|------|
| 分母 | 当日新增**且注册当日有对局**的用户数 |
| 分子（整体留存） | 第 N 日有任意对局的用户数 |
| 分子（玩法留存） | 第 N 日在**同一玩法**有对局的用户数 |

M-01 同时输出「整体留存」和「玩法留存」两个指标，便于区分用户是"回来玩了别的"还是"回来玩了同一个玩法"。

---

## 四、基础数据准备

> 分析时间段：**20260210 至 20260408**。
> 过滤条件：仅限 APP 端，排除积分/比赛房。所有分析依赖轻量级的 DWS 聚合。

### M-00: 分玩法首日对局特征宽表（DWS 优化）

**DWS 三层依赖结构：**

```
Step A: tcy_temp.dws_ddz_app_new_user_reg          （已在主分析文档 8.1.1 建立）
Step B: tcy_temp.dws_ddz_app_daily_active           （已在主分析文档 8.1.2 建立）
Step C: tcy_temp.dws_ddz_app_daily_active_modes     （本文档新建，uid×dt×game_mode）
Step D: tcy_temp.ddz_user_mode_first_day_features   （本文档新建，分玩法分析宽表）
```

**关键修正说明：**

- `olap_tcy_userapp_d_p_login1st` 无 `group_id`/`channel_id`，APP 用户过滤通过战绩表 INNER JOIN 实现
- 倍数相关字段（`grab_landlord_bet`/`complete_victory_bet`/`bomb_bet`）在 `dwd_game_combat_si` 中为**独立列**，直接使用
- 货币字段使用正确名称：`room_base`/`room_fee`/`start_money`/`end_money`/`diff_money`
- `dws_ddz_app_daily_active_modes` 时间上限延至 `20260508`，覆盖 Day30 观测期

```sql
-- Step C: 每日活跃用户×玩法聚合表（用于同玩法留存计算）
-- 完整设计见 dws/dws_ddz_app_daily_active_modes.md
CREATE TABLE tcy_temp.dws_ddz_app_daily_active_modes
DISTRIBUTED BY HASH(uid) BUCKETS 32
PROPERTIES("replication_num" = "1")
AS
SELECT
    uid, dt,
    CASE
        WHEN room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN '经典'
        WHEN room_id IN (421, 22039, 22040, 22041, 22042)                  THEN '不洗牌'
        WHEN room_id IN (13176, 13177, 13178)                              THEN '癞子'
        ELSE '其他'
    END AS game_mode
FROM tcy_dwd.dwd_game_combat_si
WHERE dt BETWEEN 20260210 AND 20260508  -- 覆盖注册期 + Day30 观测期
  AND game_id = 53 AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
  AND room_id NOT IN (11534, 14238, 15458)
GROUP BY uid, dt,
    CASE
        WHEN room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN '经典'
        WHEN room_id IN (421, 22039, 22040, 22041, 22042)                  THEN '不洗牌'
        WHEN room_id IN (13176, 13177, 13178)                              THEN '癞子'
        ELSE '其他'
    END;

-- Step D: 分玩法首日宽表（核心分析数据集）
CREATE TABLE tcy_temp.ddz_user_mode_first_day_features
DISTRIBUTED BY HASH(uid) BUCKETS 16
PROPERTIES("replication_num" = "1")
AS
WITH
-- 1. 从 DWS 基础表获取新用户清单（已完成 APP 过滤、渠道归因）
new_user_reg AS (
    SELECT uid, reg_date, group_id, device_type,
           channel_category, channel_category_tag_id
    FROM tcy_temp.dws_ddz_app_new_user_reg
),
-- 2. 提取新用户注册当日的原始战绩，按玩法打标
first_day_games_raw AS (
    SELECT
        c.uid,
        c.resultguid,
        c.timecost,
        c.room_id,
        CASE
            WHEN c.room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN '经典'
            WHEN c.room_id IN (421, 22039, 22040, 22041, 22042)                  THEN '不洗牌'
            WHEN c.room_id IN (13176, 13177, 13178)                              THEN '癞子'
            ELSE '其他'
        END                          AS game_mode,
        c.role,
        c.chairno,
        c.result_id,
        c.cut,
        c.magnification,
        c.magnification_stacked,
        c.grab_landlord_bet,         -- 独立列，直接使用
        c.complete_victory_bet,      -- 独立列，直接使用
        c.bomb_bet,                  -- 独立列，直接使用
        c.room_base,                 -- 正确字段名
        c.room_fee,                  -- 正确字段名
        c.start_money,               -- 正确字段名
        c.end_money,                 -- 正确字段名
        c.diff_money,                -- 正确字段名（不含服务费）
        ROW_NUMBER() OVER (
            PARTITION BY c.uid,
                CASE
                    WHEN c.room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN '经典'
                    WHEN c.room_id IN (421, 22039, 22040, 22041, 22042)                  THEN '不洗牌'
                    WHEN c.room_id IN (13176, 13177, 13178)                              THEN '癞子'
                    ELSE '其他'
                END
            ORDER BY c.time_unix
        )                            AS mode_game_seq,
        ROW_NUMBER() OVER (
            PARTITION BY c.uid,
                CASE
                    WHEN c.room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN '经典'
                    WHEN c.room_id IN (421, 22039, 22040, 22041, 22042)                  THEN '不洗牌'
                    WHEN c.room_id IN (13176, 13177, 13178)                              THEN '癞子'
                    ELSE '其他'
                END
            ORDER BY c.time_unix DESC
        )                            AS mode_game_seq_desc,
        ROW_NUMBER() OVER (PARTITION BY c.uid ORDER BY c.time_unix) AS global_game_seq
    FROM tcy_dwd.dwd_game_combat_si c
    INNER JOIN new_user_reg r ON c.uid = r.uid AND c.dt = r.reg_date
    WHERE c.game_id = 53
      AND c.robot != 1
      AND c.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND c.room_id NOT IN (11534, 14238, 15458)
),
-- 3. 分玩法连胜/连败（gaps-and-islands）
mode_streaks AS (
    SELECT uid, game_mode,
           MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
           MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT uid, game_mode, result_id, COUNT(*) AS streak_len
        FROM (
            SELECT uid, game_mode, result_id, mode_game_seq,
                   mode_game_seq - ROW_NUMBER() OVER (PARTITION BY uid, game_mode, result_id ORDER BY mode_game_seq) AS grp
            FROM first_day_games_raw
        ) t GROUP BY uid, game_mode, result_id, grp
    ) t2 GROUP BY uid, game_mode
),
-- 4. 同玩法留存 flag（基于 dws_ddz_app_daily_active_modes，只看注册日之后）
day_flags_mode AS (
    SELECT r.uid, a.game_mode,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1  DAY), '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day1_same_mode,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 7  DAY), '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day7_same_mode,
           MAX(CASE WHEN a.dt = CAST(DATE_FORMAT(DATE_ADD(STR_TO_DATE(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 30 DAY), '%Y%m%d') AS INT) THEN 1 ELSE 0 END) AS is_retained_day30_same_mode
    FROM new_user_reg r
    LEFT JOIN tcy_temp.dws_ddz_app_daily_active_modes a ON r.uid = a.uid AND a.dt > r.reg_date
    GROUP BY r.uid, a.game_mode
)
-- 5. 聚合最终分玩法宽表
SELECT
    r.uid,
    r.reg_date,
    g.game_mode,
    r.group_id,
    r.device_type,
    r.channel_category,
    r.channel_category_tag_id,

    COUNT(*)                                                                  AS game_count,
    SUM(g.timecost)                                                           AS total_play_seconds,
    ROUND(AVG(g.timecost), 1)                                                 AS avg_game_seconds,

    MAX(CASE WHEN g.mode_game_seq = 1 THEN g.result_id END)                   AS first_mode_game_result,
    MAX(CASE WHEN g.mode_game_seq = 1 THEN g.magnification END)               AS first_mode_game_magnification,
    MAX(CASE WHEN g.mode_game_seq_desc = 1 THEN g.result_id END)              AS last_mode_game_result,
    MAX(CASE WHEN g.mode_game_seq_desc = 1 THEN (CASE WHEN g.cut < 0 THEN 1 ELSE 0 END) END) AS last_mode_game_escaped,

    SUM(CASE WHEN g.result_id = 1 THEN 1 ELSE 0 END)                          AS win_count,
    SUM(CASE WHEN g.result_id = 2 THEN 1 ELSE 0 END)                          AS lose_count,
    ROUND(SUM(CASE WHEN g.result_id = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS win_rate,
    MAX(ms.max_win_streak)                                                    AS max_win_streak,
    MAX(ms.max_lose_streak)                                                   AS max_lose_streak,

    ROUND(AVG(g.magnification), 2)                                            AS avg_magnification,
    MAX(g.magnification)                                                      AS max_magnification,
    ROUND(AVG(g.magnification * 1.0 / NULLIF(g.magnification_stacked, 0)), 2) AS avg_public_multi,
    SUM(CASE WHEN g.magnification <= 6  THEN 1 ELSE 0 END)                    AS low_multi_games,
    SUM(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 ELSE 0 END) AS mid_multi_games,
    SUM(CASE WHEN g.magnification > 24 THEN 1 ELSE 0 END)                    AS high_multi_games,
    SUM(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 ELSE 0 END) AS high_multi_wins,
    SUM(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 ELSE 0 END) AS high_multi_losses,
    ROUND(AVG(ABS(g.diff_money) * 1.0 / NULLIF(g.room_base, 0)), 2)           AS avg_realized_multi,

    SUM(g.diff_money)                                                         AS total_diff_money,
    SUM(g.room_fee)                                                           AS total_fee_paid,
    SUM(CASE WHEN g.cut < 0 THEN 1 ELSE 0 END)                               AS escape_count,
    MAX(CASE WHEN g.global_game_seq = 1 THEN 1 ELSE 0 END)                    AS is_first_game_mode,

    COALESCE(MAX(dfm.is_retained_day1_same_mode),  0)                        AS is_retained_day1_same_mode,
    COALESCE(MAX(dfm.is_retained_day7_same_mode),  0)                        AS is_retained_day7_same_mode,
    COALESCE(MAX(dfm.is_retained_day30_same_mode), 0)                        AS is_retained_day30_same_mode
FROM first_day_games_raw g
INNER JOIN new_user_reg r ON g.uid = r.uid
LEFT JOIN  mode_streaks ms  ON g.uid = ms.uid AND g.game_mode = ms.game_mode
LEFT JOIN  day_flags_mode dfm ON g.uid = dfm.uid AND g.game_mode = dfm.game_mode
GROUP BY r.uid, r.reg_date, g.game_mode, r.group_id, r.device_type, r.channel_category, r.channel_category_tag_id;
```

---

## 五、分析SQL

> 以下所有 SQL 均基于 `tcy_temp.ddz_user_mode_first_day_features` 宽表。
> 注意：宽表中不再重复携带"全局留存"字段以节约空间，本部分分析主要考察 **同玩法留存**（`is_retained_day1_same_mode`）。

### M-01: 各玩法新增用户留存率（含渠道拆分）
```sql
SELECT
    game_mode,
    channel_category,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(game_count), 1) AS avg_games_in_mode,
    ROUND(AVG(avg_magnification), 1) AS avg_multi,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate_same_mode,
    ROUND(SUM(is_retained_day7_same_mode) * 100.0 / COUNT(*), 2) AS day7_rate_same_mode
FROM tcy_temp.ddz_user_mode_first_day_features
WHERE game_mode IN ('经典', '不洗牌', '癞子')
GROUP BY game_mode, channel_category
ORDER BY game_mode, channel_category;
```

### M-02: 分玩法 × 倍数分组留存
```sql
SELECT
    game_mode, channel_category,
    CASE
        WHEN avg_magnification <= 6  THEN 'A: <=6'
        WHEN avg_magnification <= 12 THEN 'B: 6-12'
        WHEN avg_magnification <= 24 THEN 'C: 12-24'
        WHEN avg_magnification <= 48 THEN 'D: 24-48'
        ELSE                              'E: 48+'
    END AS multi_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate
FROM tcy_temp.ddz_user_mode_first_day_features
WHERE game_mode IN ('经典', '不洗牌', '癞子')
GROUP BY game_mode, channel_category,
    CASE
        WHEN avg_magnification <= 6  THEN 'A: <=6'
        WHEN avg_magnification <= 12 THEN 'B: 6-12'
        WHEN avg_magnification <= 24 THEN 'C: 12-24'
        WHEN avg_magnification <= 48 THEN 'D: 24-48'
        ELSE                              'E: 48+'
    END
ORDER BY game_mode, channel_category, multi_group;
```

### M-03: 分玩法 × 经济变化分组留存
```sql
SELECT
    game_mode, channel_category,
    CASE
        WHEN total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN total_diff_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN total_diff_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN total_diff_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                'F: 巨赚 (>5万)'
    END AS money_change_group,
    COUNT(*) AS user_count,
    ROUND(SUM(is_retained_day1_same_mode) * 100.0 / COUNT(*), 2) AS day1_rate
FROM tcy_temp.ddz_user_mode_first_day_features
WHERE game_mode IN ('经典', '不洗牌', '癞子')
GROUP BY game_mode, channel_category,
    CASE
        WHEN total_diff_money < -50000 THEN 'A: 巨亏 (<-5万)'
        WHEN total_diff_money < -10000 THEN 'B: 大亏 (-5万~-1万)'
        WHEN total_diff_money < 0      THEN 'C: 小亏 (-1万~0)'
        WHEN total_diff_money < 10000  THEN 'D: 小赚 (0~1万)'
        WHEN total_diff_money < 50000  THEN 'E: 大赚 (1万~5万)'
        ELSE                                'F: 巨赚 (>5万)'
    END
ORDER BY game_mode, channel_category, money_change_group;
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

> **文档版本**：v2.0
> **创建日期**：2026-03-25
> **更新说明**：
> - v2.0：重构 DWS 层架构（对齐主文档 v3.0）；修正字段名（`room_base`/`diff_money` 等）；倍数字段改为直接读列；新增 `device_type` 维度；`dws_ddz_app_daily_active_modes` 时间上限延至 20260508；同玩法留存新增 Day30 指标；修正 `day_flags_mode` 添加 `a.dt > r.reg_date` 限制
>
> **关联文档**：
> - `docs/new-user-retention-analysis.md`（主分析框架，需先完成 8.1.1 和 8.1.2）
> - `dws/dws_ddz_app_daily_active_modes.md`（本文档 M-00 Step C 的详细设计）
>
> **使用说明**：
> 1. 确认主文档 `8.1.1` 和 `8.1.2` 已执行（建立 `dws_ddz_app_new_user_reg` 和 `dws_ddz_app_daily_active`）
> 2. 执行 M-00 Step C：创建分玩法活跃表 `tcy_temp.dws_ddz_app_daily_active_modes`
> 3. 执行 M-00 Step D：创建分玩法首日宽表 `tcy_temp.ddz_user_mode_first_day_features`
> 4. 依次执行 M-01 ~ M-03 进行各维度分析
> 5. 将查询结果填入对应的"查询结果"区域，用于后续分析结论生成
