# 留存分析 SQL 编写规范

> 本规范沉淀自 retention 目录全篇（global / by-mode / by-client-lang / score-game / deepdive-sql / financial 六篇）的 SQL 重构，统一了留存分析的 CTE 骨架、留存计算写法、分区裁剪策略与 StarRocks 兼容性要点。新写或改写留存 SQL 时参照本规范。

---

## 一、CTE 骨架（三段式）

所有留存分析 SQL 统一采用以下四层 CTE 结构，自底向上逐层收敛：

```sql
WITH reg_base_raw AS (
    -- ① 基础人群：全文唯一人工维护时间窗口
    SELECT uid, reg_date, app_id, <业务标签源字段>
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '<start>' AND '<end>'
),
date_bounds AS (
    -- ② 动态时间边界：用于活跃事实表分区裁剪
    SELECT
        MIN(reg_date) AS min_reg, MAX(reg_date) AS max_reg,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL N DAY) AS max_act_date   -- N = 本节最大留存天数
    FROM reg_base_raw
),
user_profile_tags AS (
    -- ③ 标签固化层：JOIN 事实表，把分组标签 + 度量 + 目标日期算死成常量
    SELECT ...
    FROM reg_base_raw r
    LEFT/INNER JOIN <事实表> ON ...
),
all_events_stream AS (
    -- ④ 矩阵坍缩层：UNION ALL 垂直流 + MAX(CASE WHEN) 打留存标记
    SELECT uid, <标签>, 1 AS is_reg, ... FROM user_profile_tags
    UNION ALL
    SELECT uid, <标签>, 0 AS is_reg, MAX(CASE WHEN dt = dN_target THEN 1 ELSE 0 END) ...
    FROM <活跃表> INNER JOIN user_profile_tags ...
    GROUP BY uid, <标签>
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    <标签>, COUNT(*), ROUND(SUM(is_dN) * 100.0 / NULLIF(COUNT(*), 0), 2) AS dayN_rate
FROM all_events_stream
GROUP BY <标签>
```

### 各层职责

| 层 | 职责 | 关键约束 |
| --- | --- | --- |
| `reg_base_raw` | 注册基础人群 | **必须**带 `app_id` + `reg_date BETWEEN` 过滤 |
| `date_bounds` | 动态时间边界 | 上界 = `MAX(reg_date) + 本节最大留存天数`，按需收紧 |
| `user_profile_tags` | 标签固化 | LEFT/INNER JOIN 事实表，分组标签 + 度量 + 目标日期**一次性算死** |
| `all_events_stream` | 矩阵坍缩 | `UNION ALL` 注册流(`is_reg=1`) + 活跃流(`MAX(CASE WHEN)`) |

---

## 二、留存计算核心写法

### 2.1 目标日期预计算成常量（关键提速点）

把留存目标日期在 `user_profile_tags` 里算成常量字段，JOIN 条件用**等值比较**，杜绝 `DATE_ADD` 在 JOIN 上高频重算：

```sql
-- ✅ 正确：目标日期预计算成常量
user_profile_tags AS (
    SELECT uid, reg_date, app_id,
        DATE_ADD(reg_date, INTERVAL 1 DAY)  AS d1_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY)  AS d7_target,
        DATE_ADD(reg_date, INTERVAL 29 DAY) AS d30_target
    FROM reg_base_raw
)
... ON a.dt IN (p.d1_target, p.d7_target, p.d30_target)

-- ❌ 避免：JOIN 条件里调 DATE_ADD（per-row 高频重算）
... ON a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY), ...)
```

### 2.2 矩阵坍缩替代 COUNT(DISTINCT CASE WHEN)

一个 `GROUP BY uid` 把多个留存标记全打出来，下游用 `SUM/COUNT(*)` 替代 `COUNT(DISTINCT CASE WHEN ...)`：

```sql
-- ✅ 矩阵坍缩：MAX(CASE WHEN) 一次打标记
SELECT uid,
    MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
    MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
FROM ... GROUP BY uid

-- 末层聚合
SELECT ..., ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
```

### 2.3 留存口径（含首日）

本文档系列统一用"**含首日**"口径，目标日期 = `reg_date + INTERVAL N DAY`：

| 留存 | INTERVAL | 含义（注册当天为第 1 天） |
| ---- | -------- | ---- |
| 次留 D1 | `+1 DAY` | 注册后第 2 天 |
| 7 留 D7 | `+6 DAY` | 注册后第 7 天 |
| 30 留 D30 | `+29 DAY` | 注册后第 30 天 |

> 注意：这与 `DATEDIFF = N`（注册次日起算）口径不同。retention 系列文档统一用含首日口径，**不要混用**。

### 2.4 防零除

所有百分比除法必须用 `NULLIF(COUNT(*), 0)` 兜底，避免分组为空时除零。

---

## 三、分区裁剪策略

### 3.1 活跃表必须裁剪

`dws_app_game_active` / `dws_dq_daily_login` 等按 `dt`/`login_date` 分区的大表，JOIN 时**必须**带分区裁剪：

```sql
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.app_id = r.app_id AND a.uid = r.uid
    AND a.dt IN (p.d1_target, p.d7_target, p.d30_target)   -- 精确到目标日期
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
```

### 3.2 ⚠️ StarRocks LEFT JOIN ON 标量子查询报 Unknown table

StarRocks 在 `LEFT JOIN ... ON` 条件里嵌 `(SELECT ... FROM date_bounds)` 标量子查询会报 `Unknown table`。规避方法：用 `CROSS JOIN date_bounds` 把边界上浮成普通列，ON 条件改为纯列对列比较。

```sql
-- ❌ StarRocks 报错：LEFT JOIN ON 里嵌标量子查询
LEFT JOIN game_active ga ON ... AND ga.dt BETWEEN (SELECT min_reg FROM date_bounds) AND (SELECT max_reg FROM date_bounds)

-- ✅ 正确：CROSS JOIN 上浮边界为列
reg_base AS (
    SELECT i.*, b.min_reg, b.max_reg
    FROM reg_base_init i
    CROSS JOIN date_bounds b
)
LEFT JOIN game_active ga ON ... AND ga.dt BETWEEN r.min_reg AND r.max_reg
```

> 经验：标量子查询放在 `WHERE` 里**是安全的**（global/by-mode/score-game 普遍这么用），只有放在 `LEFT JOIN ... ON` 里才会炸。写 LEFT JOIN 时优先用 CROSS JOIN 上浮。

---

## 四、占比类指标：分母用独立 Broadcast 表

占比类（如"某渠道内某分组占比"）**不要用窗口函数** `SUM(COUNT(*)) OVER (PARTITION BY ...)`，改用独立小分母表 + Broadcast Join：

```sql
-- ✅ 独立分母表（行数少，触发 Broadcast HASH Join，无 shuffle 开销）
channel_total_counts AS (
    SELECT channel, COUNT(*) AS channel_total
    FROM user_profile_tags GROUP BY channel
)
SELECT gb.channel, ...,
    ROUND(COUNT(*) * 100.0 / NULLIF(c.channel_total, 0), 2) AS pct_in_channel
FROM game_bucket gb
INNER JOIN channel_total_counts c ON gb.channel = c.channel
GROUP BY gb.channel, c.channel_total
```

---

## 五、分母口径要点

### 5.1 AVG 被外部流摊薄

`UNION ALL` 后注册流和活跃流都有行，直接 `AVG(度量)` 会被活跃流的行稀释。必须锁定注册流：

```sql
-- ✅ 锁定注册流
ROUND(SUM(CASE WHEN is_reg = 1 THEN game_count ELSE 0 END) * 1.0
      / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 1)
```

### 5.2 无对局组保留与否

- **保留**（分母含全部注册用户）：`user_profile_tags` 用 `LEFT JOIN` 事实表，分组里有 `'Z: 无对局'`
- **剔除**（仅含有对局用户）：`user_profile_tags` 用 `INNER JOIN` 且 `WHERE s.game_count > 0`

每节按业务语义选择，**不要混用**。retention-financial/score-game 多数保留无对局组，global/by-mode 多数剔除。

---

## 六、已下线 / 不存在的字段（踩坑记录）

| 旧字段 | 状态 | 替代写法 |
| --- | --- | --- |
| `multi_q4_win_count` / `multi_q4_lose_count`（allgame_stat） | 已废弃（表 v1.2 移除） | 固定倍数段：高倍 >24x 输赢 = `multi_24_48` + `multi_48_96` + `multi_96_192` + `multi_192_384` + `multi_384_plus` 的 `_win`/`_lose` 求和 |
| `multi_q1~q4` / `multi_q4_losses`（NTILE 四分位） | 已废弃 | 同上，按固定倍数段归档（低/中低/中高/高） |
| `is_game_active`（game_active） | **不存在** | 该表**行存在即活跃**，用 `ga.uid IS NOT NULL` 判定 |
| `reg_time`（daily_reg） | 已改名 | `reg_datetime` |
| `game_datetime`（crazyddz_daily_game） | 已改名 | `start_datetime` |

> 高倍输局数示例：

```sql
SUM(
    COALESCE(multi_24_48_lose, 0) + COALESCE(multi_48_96_lose, 0)
  + COALESCE(multi_96_192_lose, 0) + COALESCE(multi_192_384_lose, 0)
  + COALESCE(multi_384_plus_lose, 0)
) AS high_multi_losses
```

---

## 七、常见逻辑 bug（重构中发现并修正）

### 7.1 写死日期替代 reg_date

```sql
-- ❌ 写死成单日，只看 6-15 那天的对局
LEFT JOIN silvergame_stat s ON s.dt = '2026-06-15'

-- ✅ 取注册当天
LEFT JOIN silvergame_stat s ON s.dt = r.reg_date
```

### 7.2 JOIN 无上界全表扫

```sql
-- ❌ si.dt > reg_date 无上界，扫到表的最后一天
LEFT JOIN silvergame_stat si ON si.uid = u.uid AND si.dt > u.reg_date

-- ✅ 补上界
LEFT JOIN silvergame_stat si ON si.uid = u.uid
    AND si.dt > u.reg_date AND si.dt <= DATE_ADD(u.reg_date, INTERVAL 29 DAY)
```

### 7.3 标签偏移（max_streak 系列字段）

`max_win_streak` / `max_lose_streak` 是"最大连胜/连败局数"。`max_win_streak = 1` 表示"最长连胜 1 局"，**不要标成"2连胜"**：

```sql
-- ✅ 正确
WHEN s.max_win_streak = 0 THEN 'A: 无连胜'
WHEN s.max_win_streak = 1 THEN 'B: 1连胜'
WHEN s.max_win_streak = 2 THEN 'C: 2连胜'
WHEN s.max_win_streak <= 4 THEN 'D: 3-4连胜'
```

### 7.4 GROUP BY 列与 SELECT 不一致

内层 `GROUP BY p.uid, p.win_rate_group` 但 SELECT 引用了 `p.win_rate_group` 作为标签 + 多个 `MAX(CASE WHEN)` 聚合列，这是合法的（聚合列不在 GROUP BY 是对的，标签列在 GROUP BY 是对的）。但若 SELECT 出现**非聚合、非分组键**的列会报错。写两层聚合时（内层 per-uid 坍缩 + 外层按标签聚合）要逐列核对。

---

## 八、其他约定

- **Hint**：所有主 SELECT 加 `/*+ SET_VAR(new_planner_optimize_timeout=15000) */`（复杂查询用 30000），放宽优化器筹备时间，规避 `Plan Search Timeout`。
- **代码块**：markdown 里 SQL 块必须用 ```` ```sql ```` 标注语言类型（见 [markdown-style-guide.md](../markdown-style-guide.md)）。
- **JOIN 防膨胀**：活跃表 LEFT JOIN 时带 `dt IN (...)` 精确匹配，避免一个用户多日活跃导致行膨胀；用 `GROUP BY uid` + `MAX(CASE WHEN)` 坍缩成单行。
- **分母锁定**：`COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END)` 或注册流 `COUNT(*)`，永远不要用活跃流行数当分母。

---

> 相关文档：[retention-global.md](../analysis/plan/retention/retention-global.md)（全局层，范式起源）、[starrocks-ops.md](../knowledge/starrocks-ops.md)（读/写放大排查）、[troubleshooting.md](troubleshooting.md)（回填/查询疑难排查）。
