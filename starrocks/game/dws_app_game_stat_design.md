# APP 游戏统计层两表设计（纳入 510K）

## 一、背景：为什么要重新设计

### 1.1 分析目标

**寻找 APP 新增留存低的原因**。核心路径链：

```text
玩法体验（倍数/胜率/炸弹/节奏）
        ↓
    金流波动（赢输/破产/暴富）
        ↓
    留存（次日是否还来）
```

### 1.2 发现了遗漏

510K 玩法内嵌在 APP 中（`app_id = 1880053`），使用**银子**币种，是玩家金流的一环。但因为局中多轮结算、倍数累积，最初两张表没有覆盖它。

### 1.3 510K 的两个特性及其影响

| 特性 | 对分析的影响 |
| ---- | ---- |
| **局中多轮结算** | 一局内多次银子变动，不能简单当单次输赢处理 |
| **倍数累积** | 多轮倍数叠加可达 100+，与经典（通常 3~48）的量级完全不同 |

---

## 二、两表分工原则

### 2.1 核心判断标准

> **银子 = 金流 = 跨玩法可加总**（不管怎么赢的，亏 1000 银子就是亏 1000）
> **倍数 = 体验 = 玩法间不可比**（510K 的 24 倍不意味着"高风险"，经典 24 倍却很吓人）

基于这个判断，两表分工：

```
dws_app_game_stat（uid × dt）
├─ 角色：金流 + 参与度
├─ 510K：完全并入（因为是银子）
├─ 问题："这个人今天整体怎么样？银子亏了多少？"
└─ 510K 的"多轮"不是障碍：game_outcome_money 在 dws_crazyddz_daily_game
   中已经是整局汇总后的净输赢，直接 SUM 即可

dws_app_gamemode_stat（uid × dt × play_mode）
├─ 角色：玩法体验
├─ 510K：作为 play_mode=7 加入
├─ 问题："经典和 510K 哪个更容易让人输光？510K 的多轮和经典的单轮，
│   体验差异是什么？"
└─ 510K 倍数累积 → 用独立阈值，不与经典共用
```

### 2.2 "金流归并"与"体验隔离"并存

这两件事**不矛盾**，因为它们在留存归因中是被**分开问的**：

| 分析场景 | 用的表 | 510K 如何处理 |
| ---- | ---- | ---- |
| "新用户首日银子亏到谷底的和没亏的，次留差多少？" | game_stat | 510K 的和经典的银子输赢**一起算** |
| "510K 的玩家是不是比经典玩家更容易破产？" | gamemode_stat | 两玩法分开，**对比**各自的破产率 |
| "经典里的高倍局和 510K 的高倍局，哪个更伤留存？" | gamemode_stat | 各自用自己的阈值，不进游戏对比 |

---

## 三、dws_app_game_stat 设计

### 3.1 定位

**纯金流 + 参与度**。不含任何倍数指标。

### 3.2 字段

```text
基础维度
├─ app_id, uid, dt

参与度（含 510K）
├─ game_count           -- 总局数
├─ ddz_game_count       -- 其中经典系对局数（play_mode 1,2,3，均为银子）
├─ crazyddz_game_count  -- 其中 510K 对局数
├─ total_play_seconds   -- 总时长（510K 用 time_cost 对 timecost）
├─ avg_game_seconds     -- 平均时长
├─ distinct_rooms       -- 不同房间数
└─ play_modes           -- 玩法标签（"1,2,7"）

胜负（含 510K，按最终 result_id）
├─ win_count, lose_count
├─ win_rate             -- 已含 510K
├─ max_win_streak       -- 跨玩法按时间序列
└─ max_lose_streak

金流（含 510K，同为银子）
├─ start_money          -- 全天首局前（不管玩法）
├─ end_money            -- 全天末局后
├─ money_peak           -- 最高余额
├─ money_valley         -- 最低余额（破产信号）
├─ total_diff_money     -- 净输赢（不含服务费）
└─ total_fee_paid       -- 服务费
```

### 3.3 510K 专异字段（点缀在 game_stat 末尾）

510K 有哪些东西是"公共指标没法表达的"、但对留存分析有用的？

| 字段 | 类型 | 含义 | 留存分析用途 |
| ---- | ---- | ---- | ---- |
| `crazyddz_total_settle_rounds` | int | 当日 510K 总结算轮数 | 多轮 = 游戏节奏快/刺激 → 可能影响留存 |
| `crazyddz_avg_settle_rounds` | double | 平均每局轮数 | 同上 |
| `crazyddz_outcome_gdp` | bigint | 510K 货币流转绝对值累计 | 游戏内资金进出的总强度（多轮特有的"波动感"） |
| `crazyddz_max_settle_round_single` | int | 单局最多轮数 | 极端体验识别 |

> 注意：不设 `crazyddz_outcome_money` 和 `crazyddz_win_rate`，因为 510K 的输赢/胜负已并入公共字段，避免重复存储。

### 3.4 建表 SQL

```sql
CREATE TABLE tcy_temp.dws_app_game_stat (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  -- 参与度
  `game_count` int(11) NULL COMMENT "总局数（含510K）",
  `ddz_game_count` int(11) NULL COMMENT "经典系对局数（1,2,3，均为银子）",
  `crazyddz_game_count` int(11) NULL COMMENT "510K对局数",
  `total_play_seconds` int(11) NULL COMMENT "总时长（秒）",
  `avg_game_seconds` double NULL COMMENT "平均每局时长（秒）",
  `distinct_rooms` int(11) NULL COMMENT "不同房间数",
  `play_modes` varchar(64) NULL COMMENT "玩法标签，如'1,2,7'",
  -- 胜负
  `win_count` int(11) NULL COMMENT "胜利局数（含510K）",
  `lose_count` int(11) NULL COMMENT "失败局数（含510K）",
  `win_rate` double NULL COMMENT "胜率（%）",
  `max_win_streak` int(11) NULL COMMENT "最大连胜（跨玩法）",
  `max_lose_streak` int(11) NULL COMMENT "最大连败（跨玩法）",
  -- 金流
  `start_money` bigint(20) NULL COMMENT "全天首局前银子",
  `end_money` bigint(20) NULL COMMENT "全天末局后银子",
  `money_peak` bigint(20) NULL COMMENT "最高余额",
  `money_valley` bigint(20) NULL COMMENT "最低余额",
  `total_diff_money` bigint(20) NULL COMMENT "净输赢（不含服务费）",
  `total_fee_paid` int(11) NULL COMMENT "服务费",
  -- 行为
  `escape_count` int(11) NULL COMMENT "逃跑次数",
  -- 510K 多轮信号
  `crazyddz_total_settle_rounds` int(11) NULL COMMENT "510K总结算轮数",
  `crazyddz_avg_settle_rounds` double NULL COMMENT "510K平均每局轮数",
  `crazyddz_outcome_gdp` bigint(20) NULL COMMENT "510K货币流转绝对值累计",
  `crazyddz_max_settle_round_single` int(11) NULL COMMENT "510K单局最多轮数"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `dt`)
COMMENT "APP端用户每日游戏统计（金流+参与度，不含倍数）"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "colocate_with" = "group_daily_data",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);
```

### 3.5 增量 SQL

> **币种口径（重要）**：本表只汇总**银子玩法**，即经典系 `play_mode IN (1,2,3)` + 510K（`play_mode=7`）。
> 比赛(5)、积分(4)、好友房(6) 是**积分**玩法，币种与银子不可加总，**不得**并入本表的金流字段，否则 `total_diff_money`、`money_valley` 等会把积分和银子混加而失真。积分玩法的分析走 `dws_app_gamemode_stat`。

```sql
-- 参数：将 '2026-06-08' 替换为目标日期
INSERT INTO tcy_temp.dws_app_game_stat
WITH unified AS (
    -- 经典系（单轮）
    SELECT
        app_id, uid, dt,
        game_datetime AS event_time,
        timecost AS time_cost,
        room_id, result_id,
        start_money, end_money,
        game_outcome_money, room_fee,
        CASE WHEN cut < 0 THEN 1 ELSE 0 END AS escape_flag,
        play_mode,
        0 AS is_crazyddz,
        NULL AS settle_count,
        NULL AS outcome_gdp
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt = '2026-06-08'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND play_mode IN (1, 2, 3)
    UNION ALL
    -- 510K（多轮累计）
    SELECT
        app_id, uid, dt,
        start_datetime AS event_time,
        time_cost,
        room_id, result_id,
        start_money, end_money,
        game_outcome_money, room_fee,
        CASE WHEN is_escape < 0 THEN 1 ELSE 0 END AS escape_flag,
        7 AS play_mode,
        1 AS is_crazyddz,
        settle_count,
        game_outcome_gdp AS outcome_gdp
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521
      AND dt = '2026-06-08'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
),
ranked AS (
    SELECT *,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid ORDER BY event_time ASC)  AS seq_asc,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid ORDER BY event_time DESC) AS seq_desc
    FROM unified
),
streaks AS (
    SELECT
        app_id, uid,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT app_id, uid, result_id, grp, COUNT(*) AS streak_len
        FROM (
            SELECT app_id, uid, result_id,
                seq_asc - ROW_NUMBER() OVER (PARTITION BY app_id, uid, result_id ORDER BY seq_asc) AS grp
            FROM ranked
            WHERE result_id IN (1, 2)
        ) g
        GROUP BY app_id, uid, result_id, grp
    ) s
    GROUP BY app_id, uid
)
SELECT
    r.app_id,
    r.uid,
    r.dt,
    COUNT(*) AS game_count,
    COUNT(CASE WHEN r.is_crazyddz = 0 THEN 1 END) AS ddz_game_count,
    COUNT(CASE WHEN r.is_crazyddz = 1 THEN 1 END) AS crazyddz_game_count,
    SUM(r.time_cost) AS total_play_seconds,
    ROUND(AVG(r.time_cost), 1) AS avg_game_seconds,
    COUNT(DISTINCT r.room_id) AS distinct_rooms,
    GROUP_CONCAT(DISTINCT CAST(r.play_mode AS VARCHAR) ORDER BY r.play_mode SEPARATOR ',') AS play_modes,
    COUNT(CASE WHEN r.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN r.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN r.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ANY_VALUE(st.max_win_streak) AS max_win_streak,
    ANY_VALUE(st.max_lose_streak) AS max_lose_streak,
    MAX(CASE WHEN r.seq_asc = 1 THEN r.start_money END) AS start_money,
    MAX(CASE WHEN r.seq_desc = 1 THEN r.end_money END) AS end_money,
    MAX(r.end_money) AS money_peak,
    MIN(r.end_money) AS money_valley,
    SUM(r.game_outcome_money) AS total_diff_money,
    SUM(r.room_fee) AS total_fee_paid,
    SUM(r.escape_flag) AS escape_count,
    SUM(CASE WHEN r.is_crazyddz = 1 THEN r.settle_count ELSE 0 END) AS crazyddz_total_settle_rounds,
    ROUND(AVG(CASE WHEN r.is_crazyddz = 1 THEN r.settle_count END), 2) AS crazyddz_avg_settle_rounds,
    SUM(CASE WHEN r.is_crazyddz = 1 THEN r.outcome_gdp ELSE 0 END) AS crazyddz_outcome_gdp,
    MAX(CASE WHEN r.is_crazyddz = 1 THEN r.settle_count END) AS crazyddz_max_settle_round_single
FROM ranked r
LEFT JOIN streaks st ON r.app_id = st.app_id AND r.uid = st.uid
GROUP BY r.app_id, r.uid, r.dt;
```

---

## 四、dws_app_gamemode_stat 设计（扩展）

### 4.1 定位

**玩法体验**。按 play_mode 拆分，每个玩法用自己的标准衡量倍数/炸弹/胜负等。新增 play_mode=7（510K）。

### 4.2 字段改动

| 字段 | 经典系(1,2,3) | 510K(7) | 说明 |
| ---- | ---- | ---- | ---- |
| `game_count` | ✅ | ✅ | 对局数 |
| `total_play_seconds` | ✅ | ✅ | 多轮总耗时已汇整 |
| `win_count/lose_count/win_rate` | ✅ | ✅ | |
| `max_win_streak/max_lose_streak` | ✅ | ✅ | 玩法内连胜 |
| `avg_magnification` | ✅ `magnification` | ✅ `total_magnification` | **数值含义不同，不可跨玩法对比** |
| `max_magnification` | ✅ | ✅ | 同上 |
| `avg_real_magnification` | ✅ | ✅ | `ABS(outcome)/room_base`，可跨玩法对比 |
| `low_multi_games`（≤6） | ✅ | ⚠️ 510K 几乎全 ≥6 | 阈值对 510K 偏严，建议分析 SQL 层用玩法专属分桶 |
| `mid_multi_games`（6~24） | ✅ | ⚠️ 同上 | |
| `high_multi_games`（>24） | ✅ | ⚠️ 510K 累积倍数常 >24 | 对 510K 过于宽松，几乎全是高倍 |
| `total_bomb_count` | ✅ | ❌ NULL | 510K 无炸弹概念 |
| `games_with_grab` | ✅ | ❌ NULL | 510K 无抢地主 |
| `games_player_doubled` | ✅ | ❌ NULL | 无加倍 |
| `start_money/end_money/peak/valley` | ✅ | ✅ | 玩法内首尾局 |
| `total_diff_money/total_fee_paid` | ✅ | ✅ | |
| `escape_count` | ✅ | ✅ | |
| `distinct_rooms` | ✅ | ✅ | |

### 4.3 510K 倍数阈值问题

这是 gamemode_stat 纳入 510K 后**最大的设计问题**：

```
现有硬编码阈值（≤6 / 6~24 / >24）对 510K 的累积倍数不适用：
- 510K cumulative total_magnification: 典型分布 20~200+
- 经典 magnification: 典型分布 3~48
```

**推荐方案**：gamemode_stat 表的 `low/mid/high_multi_*` 字段**仍然按现有 SQL 填入**（不改表结构，向后兼容），但文档中**明确标注**：

> 510K（play_mode=7）的 low/mid/high 分类基于经典玩法阈值（≤6 / 6~24 / >24），由于 510K 累积倍数天然偏高，几乎全部落入 high_multi_games。**分析 510K 倍数时，建议不走这几个分类字段，而是在查询 SQL 中用 `NTILE` 或分位数自行分桶。**

### 4.4 510K 纳入 gamemode_stat 的增量 SQL（核心改动）

只需要在现有 SQL 中新增一个 UNION ALL 分支：

```sql
-- 510K 对局数据（新增）
SELECT
    app_id,
    7 AS play_mode,
    uid,
    dt,
    COUNT(*) AS game_count,
    SUM(time_cost) AS total_play_seconds,
    ROUND(AVG(time_cost), 1) AS avg_game_seconds,
    COUNT(CASE WHEN result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    -- 连胜连败（510K 玩法内）
    -- ... (需要计算，逻辑与经典系相同)
    -- 倍数：用 total_magnification
    ROUND(AVG(total_magnification), 2) AS avg_magnification,
    MAX(total_magnification) AS max_magnification,
    ROUND(AVG(ABS(game_outcome_money) / NULLIF(room_base, 0)), 2) AS avg_real_magnification,
    -- 高/中/低倍（注意：对510K意义不同，见文档说明）
    COUNT(CASE WHEN total_magnification <= 6 THEN 1 END) AS low_multi_games,
    COUNT(CASE WHEN total_magnification > 6 AND total_magnification <= 24 THEN 1 END) AS mid_multi_games,
    COUNT(CASE WHEN total_magnification > 24 THEN 1 END) AS high_multi_games,
    COUNT(CASE WHEN total_magnification > 24 AND result_id = 1 THEN 1 END) AS high_multi_wins,
    COUNT(CASE WHEN total_magnification > 24 AND result_id = 2 THEN 1 END) AS high_multi_losses,
    0 AS total_bomb_count,        -- 510K 无此概念
    0 AS games_with_grab,         -- 510K 无抢地主
    0 AS games_player_doubled,    -- 510K 无加倍
    -- 经济（玩法内）
    MAX(CASE WHEN seq_asc = 1 THEN start_money END) AS start_money,
    MAX(CASE WHEN seq_desc = 1 THEN end_money END) AS end_money,
    MAX(end_money) AS money_peak,
    MIN(end_money) AS money_valley,
    SUM(game_outcome_money) AS total_diff_money,
    SUM(room_fee) AS total_fee_paid,
    COUNT(CASE WHEN is_escape < 0 THEN 1 END) AS escape_count,
    COUNT(DISTINCT room_id) AS distinct_rooms
FROM (
    SELECT *,
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY start_datetime ASC)  AS seq_asc,
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY start_datetime DESC) AS seq_desc
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521
      AND dt = '2026-06-08'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
) g
GROUP BY app_id, uid, dt;
```

---

## 五、留存分析典型 SQL 示例

### 5.1 整体参与度 + 金流 → 留存

```sql
-- 用 game_stat：不需要分玩法
SELECT
    CASE
        WHEN s.money_valley < 0 THEN 'A:亏光(负)'
        WHEN s.money_valley < 5000 THEN 'B:底部<5k'
        WHEN s.money_valley < 10000 THEN 'C:底部5k-1w'
        ELSE 'D:安全>1w'
    END AS bottom_group,
    COUNT(DISTINCT s.uid) AS n,
    ROUND(COUNT(DISTINCT r.day1.uid) * 100.0 / COUNT(DISTINCT s.uid), 2) AS day1_rate
FROM tcy_temp.dws_app_game_stat s
JOIN tcy_temp.dws_dq_app_daily_reg r ON s.uid = r.uid
WHERE s.dt = r.reg_date AND r.reg_date = '20260601'
GROUP BY 1;
```

### 5.2 玩法体验对比 → 留存

```sql
-- 用 gamemode_stat：分玩法看
-- 510K 的高倍局阈值换成 NTILE 分桶
SELECT
    play_mode,
    NTILE(4) OVER (PARTITION BY play_mode ORDER BY avg_magnification) AS multi_quartile,
    COUNT(DISTINCT g.uid) AS n,
    ROUND(AVG(g.win_rate), 2) AS avg_win_rate,
    ROUND(AVG(g.total_diff_money), 0) AS avg_outcome
FROM tcy_temp.dws_app_gamemode_stat g
WHERE g.dt = '20260601'
  AND g.game_count > 0
GROUP BY play_mode, multi_quartile;
```

### 5.3 510K 玩家专项：多轮是否更"刺激"→ 更黏还是更伤？

```sql
-- 用 game_stat 的 crazyddz_* 字段
SELECT
    CASE
        WHEN crazyddz_avg_settle_rounds <= 3 THEN '短(≤3轮)'
        WHEN crazyddz_avg_settle_rounds <= 6 THEN '中(4-6轮)'
        ELSE '长(>6轮)'
    END AS round_group,
    COUNT(DISTINCT s.uid) AS n,
    ROUND(AVG(s.crazyddz_outcome_gdp), 0) AS avg_gdp
FROM tcy_temp.dws_app_game_stat s
WHERE s.dt = '20260601' AND s.crazyddz_game_count > 0
GROUP BY 1;
```

---

## 六、两表关系总结

```text
dws_ddz_daily_game ──────┐
(经典/不洗牌/癞子/比赛)    │
                          ├─ UNION ALL ─→ dws_app_game_stat （金流+参与, uid×dt）
dws_crazyddz_daily_game ──┘                  │
(510K, 多轮)                      510K: 银子并进去，轮数单独标
                                              │
dws_ddz_daily_game ───→ dws_app_gamemode_stat（玩法体验, uid×dt×play_mode）
                             play_mode = 1, 2, 3
dws_crazyddz_daily_game ─→ dws_app_gamemode_stat
                             play_mode = 7 (new)
                             倍数分桶阈值不适用，标注后由分析 SQL 处理
```

| 分析问题 | 用哪张表 | 510K 怎么处理 |
| ---- | ---- | ---- |
| 首日银子波动 → 留存 | game_stat | 并入 |
| 首日总对局数 → 留存 | game_stat | 并入 |
| 510K 多轮时长 → 留存 | game_stat | crazyddz_* 字段 |
| 经典高倍局 → 留存 | gamemode_stat | 不涉及 |
| 510K 高倍局 → 留存 | gamemode_stat, play_mode=7 | NTILE 分桶 |
| 各玩法破产率对比 | gamemode_stat | 分玩法对比 |

---

## 七、数据校验

```sql
-- 1. game_stat 行数与 gamemode_stat 的用户覆盖一致
SELECT s.dt, COUNT(DISTINCT s.uid) AS stat_users, COUNT(DISTINCT g.uid) AS gms_users
FROM dws_app_game_stat s
LEFT JOIN dws_app_gamemode_stat g ON s.uid = g.uid AND s.dt = g.dt AND s.app_id = g.app_id
WHERE s.dt = '20260608'
GROUP BY s.dt;

-- 2. 510K 局数一致性
SELECT s.dt, SUM(s.crazyddz_game_count) AS stat_crazyddz,
       SUM(CASE WHEN g.play_mode = 7 THEN g.game_count ELSE 0 END) AS gms_crazyddz
FROM dws_app_game_stat s
LEFT JOIN dws_app_gamemode_stat g ON s.uid = g.uid AND s.dt = g.dt
WHERE s.dt = '20260608'
GROUP BY s.dt;

-- 3. gamemode_stat 中 play_mode=7 的倍数分布（了解真实数据）
SELECT
    PERCENTILE_APPROX(avg_magnification, 0.25) AS p25,
    PERCENTILE_APPROX(avg_magnification, 0.50) AS p50,
    PERCENTILE_APPROX(avg_magnification, 0.75) AS p75,
    PERCENTILE_APPROX(avg_magnification, 0.90) AS p90
FROM dws_app_gamemode_stat
WHERE play_mode = 7 AND dt BETWEEN '20260601' AND '20260607';
```

---

## 八、版本历史

> **文档版本**：v4.0（终版）
> **设计时间**：2026-06-08
> **设计目标**：APP 新增留存归因 → 金流 + 玩法体验两维度分层分析
> **核心决策**：
>
> - game_stat = 金流 + 参与度，不含任何倍数指标，510K 完全并入
> - gamemode_stat = 玩法体验，新增 play_mode=7(510K)，倍数阈值差异标注在文档中
> - 两表都从明细表（dws_ddz_daily_game + dws_crazyddz_daily_game）直接 UNION ALL 重算
> - 粒度：game_stat = uid × dt，gamemode_stat = uid × dt × play_mode
> - app_code 维度去除
