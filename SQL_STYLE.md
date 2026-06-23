# SQL 书写规范（查询与分析）

> 本规范面向 StarRocks 上的**查询与分析 SQL（SELECT）**编写，沉淀自 retention 目录全篇重构经验。AI 在本项目编写 SQL 时必须参照本规范。
>
> **适用范围**：仅查询 SQL。**DDL（CREATE/ALTER/DROP TABLE）一律禁止**，须由用户在 CloudBeaver 手动执行；回填 INSERT 规范见 [py/README.md](py/README.md)。
>
> 项目 SQL 基础约定（别名、CTE、JOIN 检查、StarRocks 优化、代码审查）见 [CLAUDE.md](CLAUDE.md) 的「SQL 编写规范」一节；留存业务专项口径见 [lessons/retention-sql-pattern.md](lessons/retention-sql-pattern.md)。

---

## 一、CTE 三段式骨架

复杂分析 SQL 统一采用四层 CTE 结构，自底向上逐层收敛：

```sql
WITH reg_base_raw AS (
    -- ① 基础人群：唯一人工维护时间窗口
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
        DATE_ADD(MAX(reg_date), INTERVAL N DAY) AS max_act_date   -- N = 本查询最大跨度天数
    FROM reg_base_raw
),
user_profile_tags AS (
    -- ③ 标签固化层：JOIN 事实表，把分组标签 + 度量 + 目标日期算死成常量
    SELECT ...
    FROM reg_base_raw r
    LEFT/INNER JOIN <事实表> ON ...
),
all_events_stream AS (
    -- ④ 矩阵坍缩层：UNION ALL 垂直流 + MAX(CASE WHEN) 打标记
    SELECT uid, <标签>, 1 AS is_reg, ... FROM user_profile_tags
    UNION ALL
    SELECT uid, <标签>, 0 AS is_reg, MAX(CASE WHEN <条件> THEN 1 ELSE 0 END) ...
    FROM <活跃表> INNER JOIN user_profile_tags ...
    GROUP BY uid, <标签>
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */ ...
FROM all_events_stream
GROUP BY <标签>
```

### 各层职责

| 层 | 职责 | 关键约束 |
| --- | --- | --- |
| `reg_base_raw` | 基础人群 | **必须**带 `app_id` + 时间列 `BETWEEN` 过滤，杜绝全表扫描 |
| `date_bounds` | 动态时间边界 | 上界 = `MAX(时间列) + 本查询最大跨度`，按需收紧（只看次留就 +1 天） |
| `user_profile_tags` | 标签固化 | 分组标签 + 度量 + 目标日期**一次性算死**，避免后续高频重算 |
| `all_events_stream` | 矩阵坍缩 | `UNION ALL` 注册流(`is_reg=1`) + 活跃流(`MAX(CASE WHEN)`) |

---

## 二、目标日期预计算成常量（关键提速点）

把目标日期在标签层算成常量字段，JOIN 条件用**等值比较**，杜绝 `DATE_ADD` 在 JOIN 上 per-row 高频重算：

```sql
-- ✅ 正确：目标日期预计算成常量
user_profile_tags AS (
    SELECT uid, reg_date,
        DATE_ADD(reg_date, INTERVAL 1 DAY) AS d1_target,
        DATE_ADD(reg_date, INTERVAL 6 DAY) AS d7_target
    FROM reg_base_raw
)
... ON a.dt IN (p.d1_target, p.d7_target)

-- ❌ 避免：JOIN 条件里调 DATE_ADD
... ON a.dt IN (DATE_ADD(p.reg_date, INTERVAL 1 DAY), ...)
```

---

## 三、矩阵坍缩替代 COUNT(DISTINCT CASE WHEN)

一个 `GROUP BY uid` 把多个标记全打出来，下游用 `SUM/COUNT(*)` 替代 `COUNT(DISTINCT CASE WHEN ...)`，消灭 DISTINCT 的 shuffle 开销：

```sql
-- ✅ 矩阵坍缩：MAX(CASE WHEN) 一次打标记
SELECT uid,
    MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1,
    MAX(CASE WHEN a.dt = p.d7_target THEN 1 ELSE 0 END) AS is_d7
FROM ... GROUP BY uid

-- 末层聚合
SELECT ..., ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2) AS day1_rate
```

---

## 四、分区裁剪

### 4.1 活跃/事实大表必须裁剪

`dws_app_game_active` / `dws_dq_daily_login` 等按 `dt`/`login_date` 分区的大表，JOIN 时**必须**带分区裁剪：

```sql
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.app_id = r.app_id AND a.uid = r.uid
    AND a.dt IN (p.d1_target, p.d7_target)                              -- 精确到目标日期
    AND a.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
```

### 4.2 ⚠️ StarRocks LEFT JOIN ON 标量子查询报 Unknown table

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

> 经验：标量子查询放在 `WHERE` 里**是安全的**，只有放在 `LEFT JOIN ... ON` 里才会炸。写 LEFT JOIN 时优先用 CROSS JOIN 上浮边界。

---

## 五、占比类指标：独立 Broadcast 分母表

占比类（如"某分组在所属维度的占比"）**不要用窗口函数** `SUM(COUNT(*)) OVER (PARTITION BY ...)`，改用独立小分母表 + Broadcast Join（分母表行数少，触发 Broadcast HASH Join，无 shuffle）：

```sql
-- ✅ 独立分母表
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

## 六、防零除与分母锁定

### 6.1 所有百分比除法必须防零除

```sql
ROUND(SUM(is_d1) * 100.0 / NULLIF(COUNT(*), 0), 2)
```

### 6.2 UNION 多流后 AVG 被摊薄

`UNION ALL` 注册流和活跃流都有行，直接 `AVG(度量)` 会被活跃流行稀释。必须锁定注册流：

```sql
-- ✅ 锁定注册流
ROUND(SUM(CASE WHEN is_reg = 1 THEN game_count ELSE 0 END) * 1.0
      / COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 1)
```

### 6.3 分母永远锁定注册流

`COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END)` 或注册流 `COUNT(*)`，**不要用活跃流行数当分母**。

---

## 七、JOIN 防膨胀

活跃表 LEFT JOIN 时一个用户可能多日有多行，导致行膨胀。两种收敛方式：

- **精确匹配**：`ON a.dt IN (目标日期列表)`，只拉关心的几天
- **坍缩成单行**：`GROUP BY uid` + `MAX(CASE WHEN)`，把多行压成一行多标记

```sql
-- ✅ GROUP BY uid + MAX(CASE WHEN) 坍缩
SELECT uid,
    MAX(CASE WHEN a.dt = p.d1_target THEN 1 ELSE 0 END) AS is_d1
FROM user_profile p
LEFT JOIN game_active a ON a.uid = p.uid AND a.dt IN (p.d1_target, p.d7_target)
GROUP BY uid
```

---

## 八、已下线 / 不存在的字段（踩坑记录）

写 SQL 前先确认字段是否仍在表结构中。下表为已知的踩坑字段：

| 旧字段 | 状态 | 替代写法 |
| --- | --- | --- |
| `multi_q4_win_count` / `multi_q4_lose_count`（allgame_stat） | 已废弃（表 v1.2 移除） | 固定倍数段：高倍 >24x 输赢 = `multi_24_48` + `multi_48_96` + `multi_96_192` + `multi_192_384` + `multi_384_plus` 的 `_win`/`_lose` 求和 |
| `multi_q1~q4` / `multi_q4_losses`（NTILE 四分位） | 已废弃 | 同上，按固定倍数段归档 |
| `is_game_active`（game_active） | **不存在** | 该表**行存在即活跃**，用 `ga.uid IS NOT NULL` 判定 |
| `reg_time`（daily_reg） | 已改名 | `reg_datetime` |
| `game_datetime`（crazyddz_daily_game） | 已改名 | `start_datetime` |

> 表结构以 `starrocks/` 下各表 `.md` 文档的「字段说明」为准。不确定字段是否存在时，先查对应表文档，不要凭记忆写。

### 高倍输局数示例

```sql
SUM(
    COALESCE(multi_24_48_lose, 0) + COALESCE(multi_48_96_lose, 0)
  + COALESCE(multi_96_192_lose, 0) + COALESCE(multi_192_384_lose, 0)
  + COALESCE(multi_384_plus_lose, 0)
) AS high_multi_losses
```

---

## 九、常见逻辑 bug

### 9.1 写死日期替代 reg_date

```sql
-- ❌ 写死成单日，只看某天的对局
LEFT JOIN silvergame_stat s ON s.dt = '2026-06-15'

-- ✅ 取注册当天
LEFT JOIN silvergame_stat s ON s.dt = r.reg_date
```

### 9.2 JOIN 无上界全表扫

```sql
-- ❌ si.dt > reg_date 无上界，扫到表的最后一天
LEFT JOIN silvergame_stat si ON si.uid = u.uid AND si.dt > u.reg_date

-- ✅ 补上界
LEFT JOIN silvergame_stat si ON si.uid = u.uid
    AND si.dt > u.reg_date AND si.dt <= DATE_ADD(u.reg_date, INTERVAL 29 DAY)
```

### 9.3 标签偏移（max_streak 系列字段）

`max_win_streak` / `max_lose_streak` 是"最大连胜/连败局数"。`max_win_streak = 1` 表示"最长连胜 1 局"，**不要标成"2连胜"**：

```sql
-- ✅ 正确
WHEN s.max_win_streak = 0 THEN 'A: 无连胜'
WHEN s.max_win_streak = 1 THEN 'B: 1连胜'
WHEN s.max_win_streak = 2 THEN 'C: 2连胜'
WHEN s.max_win_streak <= 4 THEN 'D: 3-4连胜'
```

### 9.4 GROUP BY 列与 SELECT 不一致

`GROUP BY` 的列必须覆盖 SELECT 中所有**非聚合**列；聚合列（`SUM`/`MAX`/`COUNT`）不进 GROUP BY。两层聚合时（内层 per-uid 坍缩 + 外层按标签聚合）要逐列核对。

---

## 十、其他约定

- **Hint**：主 SELECT 加 `/*+ SET_VAR(new_planner_optimize_timeout=15000) */`（复杂查询用 30000），放宽优化器筹备时间，规避 `Plan Search Timeout`。
- **CTE 优先**：复杂逻辑用 `WITH` 子句分层，不用嵌套子查询。
- **有意义的别名**：用 `reg`/`st` 等有语义的别名，禁用 `a`/`b`/`c`（项目基础约定，见 CLAUDE.md）。
- **`COUNT(DISTINCT uid)`**：计数去重用 `COUNT(DISTINCT uid)`，除非已用矩阵坍缩保证每 uid 单行（此时 `COUNT(*)` 等价且更快）。
- **markdown 代码块**：SQL 块必须用 ```` ```sql ```` 标注语言类型（见 [markdown-style-guide.md](markdown-style-guide.md)）。

---

> 相关文档：[CLAUDE.md](CLAUDE.md)（项目协作指南，含 SQL 基础约定）、[lessons/retention-sql-pattern.md](lessons/retention-sql-pattern.md)（留存业务专项口径）、[lessons/starrocks.md](lessons/starrocks.md)（读/写放大排查）。
