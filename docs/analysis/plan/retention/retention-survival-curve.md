# 留存生存曲线分析：D1-D7 逐日流失时段归因

> 本文档定义"逐日留存生存曲线 + 流失时段分群归因"的 SQL 查询与分析框架。从现有二值留存（D1/D7=1/0）升级到 D1-D7 逐日追踪，定位用户在哪个时间窗口流失、不同流失时段的用户首日画像有何差异。
>
> **分析窗口**：`reg_date BETWEEN '2026-06-18' AND '2026-06-22'`（5 天 cohort，D7 全到期：06-22 + 7 = 06-29）
>
> **范围**：全部新增注册用户（含 0 局），观察期 D1-D7。
>
> **数据源**：`dws_dq_app_daily_reg`（注册基础） + `dws_app_game_active`（逐日活跃向量） + `dws_app_firstday_game_stat`（首日画像）

---

## 一、分析框架

### 1.1 核心问题

| 问题 | 现有认知 | 本分析补充 |
| ---- | ---- | ---- |
| 用户在什么时候流失？ | D1≈21%、D7≈12%，知道二值，不知道逐日 | **逐日生存曲线**——D1→D2→...→D7 每天的衰减拐点 |
| 不同时间窗口流失的人有什么不同？ | 知道 D1 流失者 85.5% 单次会话 | **流失时段分群**——D1 流失 vs D2-D3 流失 vs D4-D7 流失的首日画像差异 |
| 0 局用户长什么样？ | 知道 0 局约占 11% | 纳入生存曲线底端，看 0 局占比和属性 |

### 1.2 输出结构

```text
§2 生存曲线：全量 + 分渠道/分客户端 D1-D7 逐日留存率
§3 流失时段分群：按"最后活跃日"分 8 组，各组占比
§4 分群画像：各组首日行为指标（对局数/胜率/登录次数/渠道/客户端）
§5 结论：流失拐点 + 干预窗口优先级
```

---

## 二、SQL 查询

### 2.1 逐日生存曲线（全量 + 渠道 + 客户端）

```sql
WITH reg_base_raw AS (
    -- ① 基础人群
    SELECT
        uid, reg_date, app_id,
        channel_category_name,
        reg_group_id,
        reg_app_code
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-06-18' AND '2026-06-22'
),
date_bounds AS (
    -- ② 动态时间边界（D1-D7）
    SELECT
        MIN(reg_date) AS min_reg, MAX(reg_date) AS max_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
reg_base AS (
    -- ②+ CROSS JOIN 上浮边界
    SELECT i.*, b.min_reg, b.max_reg, b.min_act_date, b.max_act_date
    FROM reg_base_raw i
    CROSS JOIN date_bounds b
),
user_profile_tags AS (
    -- ③ 标签固化：目标日期预计算为常量
    SELECT
        uid, reg_date, app_id,
        channel_category_name,
        CASE
            WHEN reg_group_id IN (8, 88) THEN 'iOS'
            WHEN reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            ELSE 'Other'
        END AS device_type,
        CASE
            WHEN reg_app_code = 'zgdx' THEN 'cocos-creator'
            WHEN reg_app_code = 'zgda' THEN 'cocos-lua'
            ELSE 'Other'
        END AS client_type,
        DATE_ADD(reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(reg_date, INTERVAL 2 DAY) AS d2_target,
        DATE_ADD(reg_date, INTERVAL 3 DAY) AS d3_target,
        DATE_ADD(reg_date, INTERVAL 4 DAY) AS d4_target,
        DATE_ADD(reg_date, INTERVAL 5 DAY) AS d5_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY) AS d6_target,
        DATE_ADD(reg_date, INTERVAL 7 DAY) AS d7_target,
        min_reg, max_reg, min_act_date, max_act_date
    FROM reg_base
),
daily_active_matrix AS (
    -- ④ 矩阵坍缩：一次 GROUP BY uid 打出 D1-D7 7 个标记
    SELECT
        p.uid, p.reg_date,
        p.channel_category_name,
        p.device_type,
        p.client_type,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d2_target THEN 1 ELSE 0 END) AS is_d2,
        MAX(CASE WHEN a.dt = p.d3_target THEN 1 ELSE 0 END) AS is_d3,
        MAX(CASE WHEN a.dt = p.d4_target THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = p.d5_target THEN 1 ELSE 0 END) AS is_d5,
        MAX(CASE WHEN a.dt = p.d6_target THEN 1 ELSE 0 END) AS is_d6,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d2_target, p.d3_target, p.d4_target, p.d5_target, p.d6_target, p.d7_target)
        AND a.dt BETWEEN p.min_act_date AND p.max_act_date
    GROUP BY p.uid, p.reg_date, p.channel_category_name, p.device_type, p.client_type
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'all' AS dim_type,
    'all' AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, dim_value

UNION ALL

SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'channel' AS dim_type,
    channel_category_name AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, channel_category_name

UNION ALL

SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'device' AS dim_type,
    device_type AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, device_type

UNION ALL

SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    'client' AS dim_type,
    client_type AS dim_value,
    COUNT(*) AS total_users,
    ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d1_rate,
    ROUND(SUM(is_d2) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d2_rate,
    ROUND(SUM(is_d3) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d3_rate,
    ROUND(SUM(is_d4) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d4_rate,
    ROUND(SUM(is_d5) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d5_rate,
    ROUND(SUM(is_d6) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d6_rate,
    ROUND(SUM(is_d7) * 100.0 / NULLIF(COUNT(*), 0), 2) AS d7_rate
FROM daily_active_matrix
GROUP BY dim_type, client_type;
```

> 输出：三组逐日留存率曲线（全量 / 分渠道 / 分设备），用于定位衰减拐点。

### 2.2 流失时段分群（最后活跃日标签）

```sql
WITH reg_base_raw AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-06-18' AND '2026-06-22'
),
date_bounds AS (
    SELECT
        MIN(reg_date) AS min_reg, MAX(reg_date) AS max_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
reg_base AS (
    SELECT i.*, b.min_reg, b.max_reg, b.min_act_date, b.max_act_date
    FROM reg_base_raw i
    CROSS JOIN date_bounds b
),
user_profile_tags AS (
    SELECT
        uid, reg_date, app_id,
        DATE_ADD(reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(reg_date, INTERVAL 2 DAY) AS d2_target,
        DATE_ADD(reg_date, INTERVAL 3 DAY) AS d3_target,
        DATE_ADD(reg_date, INTERVAL 4 DAY) AS d4_target,
        DATE_ADD(reg_date, INTERVAL 5 DAY) AS d5_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY) AS d6_target,
        DATE_ADD(reg_date, INTERVAL 7 DAY) AS d7_target,
        min_reg, max_reg, min_act_date, max_act_date
    FROM reg_base
),
daily_active_matrix AS (
    SELECT
        p.uid, p.reg_date,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d2_target THEN 1 ELSE 0 END) AS is_d2,
        MAX(CASE WHEN a.dt = p.d3_target THEN 1 ELSE 0 END) AS is_d3,
        MAX(CASE WHEN a.dt = p.d4_target THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = p.d5_target THEN 1 ELSE 0 END) AS is_d5,
        MAX(CASE WHEN a.dt = p.d6_target THEN 1 ELSE 0 END) AS is_d6,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d2_target, p.d3_target, p.d4_target, p.d5_target, p.d6_target, p.d7_target)
        AND a.dt BETWEEN p.min_act_date AND p.max_act_date
    GROUP BY p.uid, p.reg_date
),
last_active_label AS (
    SELECT
        uid, reg_date,
        is_d1, is_d2, is_d3, is_d4, is_d5, is_d6, is_d7,
        CASE
            WHEN is_d7 = 1 THEN 'D7留存'
            WHEN is_d6 = 1 THEN 'D6流失'
            WHEN is_d5 = 1 THEN 'D5流失'
            WHEN is_d4 = 1 THEN 'D4流失'
            WHEN is_d3 = 1 THEN 'D3流失'
            WHEN is_d2 = 1 THEN 'D2流失'
            WHEN is_d1 = 1 THEN 'D1流失'
            ELSE '从未回来'
        END AS churn_period
    FROM daily_active_matrix
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    churn_period,
    COUNT(*) AS users,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 2) AS pct
FROM last_active_label
GROUP BY churn_period
ORDER BY
    CASE churn_period
        WHEN '从未回来' THEN 1 WHEN 'D1流失' THEN 2 WHEN 'D2流失' THEN 3
        WHEN 'D3流失' THEN 4 WHEN 'D4流失' THEN 5 WHEN 'D5流失' THEN 6
        WHEN 'D6流失' THEN 7 WHEN 'D7留存' THEN 8
    END;
```

> 输出：8 组流失时段分群，各组人数与占比。

### 2.3 流失时段 × 首日画像归因

```sql
WITH reg_base_raw AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-06-18' AND '2026-06-22'
),
date_bounds AS (
    SELECT
        MIN(reg_date) AS min_reg, MAX(reg_date) AS max_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_act_date
    FROM reg_base_raw
),
reg_base AS (
    SELECT i.*, b.min_reg, b.max_reg, b.min_act_date, b.max_act_date
    FROM reg_base_raw i
    CROSS JOIN date_bounds b
),
user_profile_tags AS (
    SELECT
        uid, reg_date, app_id,
        DATE_ADD(reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(reg_date, INTERVAL 2 DAY) AS d2_target,
        DATE_ADD(reg_date, INTERVAL 3 DAY) AS d3_target,
        DATE_ADD(reg_date, INTERVAL 4 DAY) AS d4_target,
        DATE_ADD(reg_date, INTERVAL 5 DAY) AS d5_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY) AS d6_target,
        DATE_ADD(reg_date, INTERVAL 7 DAY) AS d7_target,
        min_reg, max_reg, min_act_date, max_act_date
    FROM reg_base
),
daily_active_matrix AS (
    SELECT
        p.uid, p.reg_date,
        MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
        MAX(CASE WHEN a.dt = p.d2_target THEN 1 ELSE 0 END) AS is_d2,
        MAX(CASE WHEN a.dt = p.d3_target THEN 1 ELSE 0 END) AS is_d3,
        MAX(CASE WHEN a.dt = p.d4_target THEN 1 ELSE 0 END) AS is_d4,
        MAX(CASE WHEN a.dt = p.d5_target THEN 1 ELSE 0 END) AS is_d5,
        MAX(CASE WHEN a.dt = p.d6_target THEN 1 ELSE 0 END) AS is_d6,
        MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
    FROM user_profile_tags p
    LEFT JOIN tcy_temp.dws_app_game_active a
        ON a.app_id = p.app_id AND a.uid = p.uid
        AND a.dt IN (p.d1_target, p.d2_target, p.d3_target, p.d4_target, p.d5_target, p.d6_target, p.d7_target)
        AND a.dt BETWEEN p.min_act_date AND p.max_act_date
    GROUP BY p.uid, p.reg_date
),
last_active_label AS (
    SELECT
        uid, reg_date,
        CASE
            WHEN is_d7 = 1 THEN 'D7留存'
            WHEN is_d6 = 1 THEN 'D6流失'
            WHEN is_d5 = 1 THEN 'D5流失'
            WHEN is_d4 = 1 THEN 'D4流失'
            WHEN is_d3 = 1 THEN 'D3流失'
            WHEN is_d2 = 1 THEN 'D2流失'
            WHEN is_d1 = 1 THEN 'D1流失'
            ELSE '从未回来'
        END AS churn_period
    FROM daily_active_matrix
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=30000) */
    l.churn_period,
    COUNT(*) AS users,
    ROUND(COUNT(*) * 100.0 / NULLIF(SUM(COUNT(*)) OVER(), 0), 2) AS pct,

    -- 首日对局数分布（NULL = 0 局，需 COALESCE 处理）
    ROUND(SUM(CASE WHEN COALESCE(g.silver_game_count, 0) = 0 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_0game,
    ROUND(SUM(CASE WHEN g.silver_game_count = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_1game,
    ROUND(SUM(CASE WHEN g.silver_game_count BETWEEN 2 AND 5 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_2_5game,
    ROUND(SUM(CASE WHEN g.silver_game_count BETWEEN 6 AND 10 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_6_10game,
    ROUND(SUM(CASE WHEN g.silver_game_count > 10 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_10plus_game,
    ROUND(AVG(g.silver_game_count), 1) AS avg_game_cnt,

    -- 胜率
    ROUND(AVG(g.silver_win_rate), 1) AS avg_win_rate,
    ROUND(SUM(CASE WHEN g.silver_win_rate < 30 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_low_winrate,

    -- 首日登录次数（单次会话 vs 多次）
    ROUND(AVG(g.first_day_login_cnt), 2) AS avg_login_cnt,
    ROUND(SUM(CASE WHEN g.first_day_login_cnt = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_single_session,

    -- 银子指标
    ROUND(AVG(g.silver_total_diff_money), 0) AS avg_silver_diff,
    ROUND(AVG(g.silver_end_money), 0) AS avg_end_money,

    -- 平均每局时长（<60s 高倍偏好组占比）
    ROUND(SUM(CASE
        WHEN g.silver_total_play_seconds > 0 AND g.silver_game_count > 0
         AND g.silver_total_play_seconds * 1.0 / g.silver_game_count < 60 THEN 1
        ELSE 0
    END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_fast_game,

    -- 渠道分布（top 渠道）
    ROUND(SUM(CASE WHEN r.channel_category_name = '官方(非CPS)' THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_official,
    ROUND(SUM(CASE WHEN r.channel_category_name = '华为' THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_huawei,

    -- 客户端版本（zgdx = cocos-creator, zgda = cocos-lua）
    ROUND(SUM(CASE WHEN r.reg_app_code = 'zgdx' THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 1) AS pct_creator

FROM last_active_label l
LEFT JOIN tcy_temp.dws_app_firstday_game_stat g
    ON g.app_id = 1880053 AND g.reg_date = l.reg_date AND g.uid = l.uid
LEFT JOIN tcy_temp.dws_dq_app_daily_reg r
    ON r.app_id = 1880053 AND r.reg_date = l.reg_date AND r.uid = l.uid
GROUP BY l.churn_period
ORDER BY
    CASE l.churn_period
        WHEN '从未回来' THEN 1 WHEN 'D1流失' THEN 2 WHEN 'D2流失' THEN 3
        WHEN 'D3流失' THEN 4 WHEN 'D4流失' THEN 5 WHEN 'D5流失' THEN 6
        WHEN 'D6流失' THEN 7 WHEN 'D7留存' THEN 8
    END;
```

> 输出：各流失时段群组的首日画像对比，识别不同流失窗口的行为差异。

---

## 三、分析方法

### 3.1 生存曲线解读

1. 画出 D1-D7 逐日留存率折线图（全量 + 渠道 + 设备三条线叠加）
2. 标注衰减最快的拐点（如 D1→D2 骤降 40% vs D6→D7 仅降 5%）
3. 如果渠道/设备间曲线**形态一致但基线不同**→ 归因渠道质量差异；如果**形态分叉**→ 归因不同渠道的留存衰减模式不同

### 3.2 流失时段分群解读

| 信号 | 含义 |
| ---- | ---- |
| "从未回来" 占比高 | 首日体验或获客质量问题（与归因报告 §5 "试用即走"对应） |
| "D1流失" 占比高 | 次日来了但 D2 起不再来——D1 体验没形成习惯 |
| D2-D3 流失集中 | 首周中段衰减——可能是内容深度不足 |
| D4-D7 流失尾部 | 保护期后/中段流失——与"撤保护断崖"对应（已证伪非主因） |

### 3.3 分群画像交叉解读

对每个流失时段群组，看首日画像的差异：

- **对局数分布**：0 局集中在"从未回来"？1 局集中在"D1 流失"？10+局集中在"D7 留存"？
- **单次会话占比**：D1 流失者是否也像归因报告 §6A 那样 85%+ 单次会话？
- **高倍偏好占比**：`<60s/局` 组是否集中在特定流失窗口？
- **渠道差异**：低质渠道（如咪咕）是否集中在"从未回来"？

---

## 四、与现有文档的关系

| 现有文档 | 本分析补充 |
| ---- | ---- |
| [retention-gameplayers-attribution-report.md](../../result/retention-gameplayers-attribution-report.md) §6A | D1 视角（D1 流失者 85.5% 单次会话）→ 扩展到 D2-D7 全时间线 |
| [retention-global-report.md](../../result/retention-global-report.md) §3.2 | 对局数×D7 二值 → 对局数×流失时段交叉 |
| [retention-analysis-framework.md](retention-analysis-framework.md) §5 | 四条主线 → 增加"时间窗口"第五维度 |

---

> **创建时间**：2026-06-29
>
> **关联**：基于 [retention-analysis-framework.md](retention-analysis-framework.md) 的框架和 [retention-gameplayers-attribution-report.md](../../result/retention-gameplayers-attribution-report.md) 的 D1 发现，将二值留存升级为逐日生存曲线。
