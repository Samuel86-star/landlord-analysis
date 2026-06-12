# DWS 中间表：APP 端银子玩法每日统计表（金流 + 参与度）

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_silvergame_stat` |
| 全名 | `tcy_temp.dws_app_silvergame_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日游戏统计表，仅汇总银子玩法的金流与参与度，不含倍数 / 炸弹等玩法体验指标。510K（多轮玩法）金流并入此表，玩法体验信号见 `dws_app_allgame_stat` |
| 粒度 | uid × dt（一个用户一天一行，跨银子玩法） |

> **命名说明**：表名中的 `silvergame` 明确标识本表只覆盖**银子玩法**（经典系 1/2/3 + 510K），不含积分玩法。早期名为 `dws_app_game_stat`，因"game"字面易被误读为"全量玩法"而改名（详见版本历史 v4.3）。

## 姊妹表：银子 / 积分按币种分表

金流字段（`total_diff_money`、`money_valley` 等）只在同币种内可加总，因此按币种拆成两张同构表，命名以币种区分：

| 表名 | 币种 | play_mode | 状态 |
| ---- | ---- | --------- | ---- |
| `dws_app_silvergame_stat`（本表） | 银子 | 1=经典, 2=不洗牌, 3=癞子, 7=510K | ✅ 已建 |
| `dws_app_scoregame_stat` | 积分 | 4=积分(PC), 5=比赛, 6=好友房 | ✅ 已建 |

> **边界约定**：两表 play_mode 互不重叠，并集即全部玩法。`dws_app_scoregame_stat` 建表时直接复用本表结构（去掉银子专属语义即可），金流字段在各自币种内独立加总，**不得**跨表相加。需要跨币种的玩法体验对比走 `dws_app_allgame_stat`（全玩法、按 play_mode 拆分、不加总金流）。

## 设计背景

`dws_ddz_daily_game` 与 `dws_crazyddz_daily_game` 是对局级明细表。在做 APP 新增留存归因时，核心路径是：

```text
玩法体验（倍数/胜率/炸弹/节奏）
        ↓
    金流波动（赢输/破产/暴富）
        ↓
    留存（次日是否还来）
```

为了让金流归因独立于玩法变量，本表只承担**金流 + 参与度**这一维度，玩法体验维度交由 `dws_app_allgame_stat`（uid × dt × play_mode）。

### 与 dws_app_allgame_stat 的分工

| | dws_app_silvergame_stat（本表） | dws_app_allgame_stat |
| ---- | ---- | ---- |
| 粒度 | uid × dt | uid × dt × play_mode |
| 玩法范围 | 仅银子玩法（1, 2, 3, 7） | 所有玩法（1, 2, 3, 4, 5, 6, 7） |
| 核心指标 | 参与度、金流（银子可加总） | 倍数、炸弹、玩法内胜率（玩法间不可比） |
| 典型问题 | "新用户首日银子亏了多少？" | "经典 vs 510K，哪个高倍局更多？" |
| 510K 处理 | 金流 / 参与度完全并入 | 作为 play_mode=7 单独一行 |

### 核心判断标准

> **银子 = 金流 = 跨玩法可加总**（不管怎么赢的，亏 1000 银子就是亏 1000）
> **倍数 = 体验 = 玩法间不可比**（510K 的 24 倍不意味着"高风险"，经典 24 倍却很吓人）

基于这个判断：本表不收任何倍数字段，510K 因为也是银子玩法，金流字段直接并入；510K 的"多轮"特性已在 `dws_crazyddz_daily_game` 中按整局汇总为 `game_outcome_money`，可直接 SUM。

### 510K 多轮结算：上游已合并为一局一行

510K 一局内会多轮结算、银子多次变动，原始日志因此一局有多条记录。这层合并在**上游明细表已经做完**，本表无需关心多轮细节：

| 表 | resultguid + uid 粒度 | 说明 |
| ---- | ---- | ---- |
| `crazyddz_daily_game_raw`（原始日志） | 多条 | 一局内每轮结算一条记录 |
| `dws_crazyddz_daily_game`（明细表） | 一条 | 多轮日志已合并为整局一行，`game_outcome_money` 为整局净输赢 |

> **对本表的意义**：因为 `dws_crazyddz_daily_game` 已是"一局一行"，510K 与经典系（本就一局一行）的粒度一致，UNION ALL 后 `SUM(game_outcome_money)` 即整日净输赢，不会把同一局的多轮重复累加。`game_count` 统计的也是"局数"而非"轮数"。

## 币种口径（重要）

本表只汇总**银子玩法**：经典系 `play_mode IN (1, 2, 3)` + 510K（`play_mode=7`）。

比赛（5）、积分（4）、好友房（6）是**积分**玩法，币种与银子不可加总，**不得**并入本表的金流字段，否则 `total_diff_money`、`money_valley` 等会把积分和银子混加而失真。积分玩法的分析走 `dws_app_allgame_stat`。

## 玩法分类说明

| play_mode | 玩法 | 币种 | 是否纳入本表 |
| --------- | ---- | ---- | ------------ |
| 1 | 经典 | 银子 | ✅ |
| 2 | 不洗牌 | 银子 | ✅ |
| 3 | 癞子 | 银子 | ✅ |
| 7 | 510K | 银子 | ✅（多轮，从 dws_crazyddz_daily_game 接入） |
| 4 | 积分（PC 端） | 积分 | ❌ 走 allgame_stat |
| 5 | 比赛（APP/小游戏端） | 积分 | ❌ 走 allgame_stat |
| 6 | 好友房 | 积分 | ❌ 走 allgame_stat |

> **说明**：本表仅统计 APP 端用户（`group_id IN (6, 66, 8, 88, 33, 44, 77, 99)`）的银子玩法。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| uid | int | 玩家唯一标识 | 123456789 |
| dt | date | 对局日期 | 2026-06-08 |
| game_count | int | 当日对局总数（含 510K） | 12 |
| total_play_seconds | int | 当日总游戏时长（秒，510K 用 time_cost 对 timecost） | 3600 |
| avg_game_seconds | double | 平均每局时长（秒） | 180.5 |
| distinct_rooms | int | 当日游玩房间数 | 3 |
| win_count | int | 胜利局数（含 510K，按 result_id=1） | 7 |
| lose_count | int | 失败局数（含 510K，按 result_id=2） | 5 |
| win_rate | double | 胜率（百分比）= win_count / game_count | 58.33 |
| lose_rate | double | 负率（百分比）= lose_count / game_count；510K 有平局，故 win_rate + lose_rate 不一定等于 100 | 41.67 |
| max_win_streak | int | 最大连胜（跨玩法按时间序列） | 3 |
| max_lose_streak | int | 最大连败（跨玩法按时间序列） | 2 |
| start_money | bigint | 全天首局前银子（不分玩法，按时间最早一局） | 10000 |
| end_money | bigint | 全天末局后银子（不分玩法，按时间最晚一局） | 15000 |
| money_peak | bigint | 当日银子峰值（所有对局 end_money 最大值） | 18000 |
| money_valley | bigint | 当日银子谷值（破产信号） | 8000 |
| total_diff_money | bigint | 当日净输赢（不含服务费） | 5000 |
| total_fee_paid | int | 当日总服务费 | 1200 |
| escape_count | int | 当日逃跑次数 | 0 |

> **设计取舍**：本表移除了 510K 专属字段（`total_settle_rounds` / `avg_settle_rounds` / `outcome_gdp` / `max_settle_round_single`），510K 体验信号统一由 `dws_app_allgame_stat`（play_mode=7）承载，保持本表的纯金流边界。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_app_silvergame_stat (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  -- 参与度
  `game_count` int(11) NULL COMMENT "总局数（含510K）",
  `total_play_seconds` int(11) NULL COMMENT "总时长（秒）",
  `avg_game_seconds` double NULL COMMENT "平均每局时长（秒）",
  `distinct_rooms` int(11) NULL COMMENT "不同房间数",
  -- 胜负
  `win_count` int(11) NULL COMMENT "胜利局数（含510K）",
  `lose_count` int(11) NULL COMMENT "失败局数（含510K）",
  `win_rate` double NULL COMMENT "胜率（%）= win_count/game_count",
  `lose_rate` double NULL COMMENT "负率（%）= lose_count/game_count，510K有平局故与胜率不互补",
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
  `escape_count` int(11) NULL COMMENT "逃跑次数"
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
    "dynamic_partition.start" = "-120",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);
```

### 增量数据导入

> **说明**：经典系（`dws_ddz_daily_game`，单轮）与 510K（`dws_crazyddz_daily_game`，多轮）通过 UNION ALL 拼接为统一明细，再做用户级聚合。

```sql
-- 参数：将 '2026-06-08' 替换为目标日期
INSERT INTO tcy_temp.dws_app_silvergame_stat
WITH unified AS (
    -- 经典系（单轮）
    SELECT
        app_id, uid, dt, resultguid,
        game_datetime AS event_time,
        timecost AS time_cost,
        room_id, result_id,
        start_money, end_money,
        game_outcome_money, room_fee,
        CASE WHEN cut = 0 THEN 0 ELSE 1 END AS escape_flag,
        play_mode,
        0 AS is_crazyddz,
        1 AS settle_count
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53
      AND dt BETWEEN '2026-03-01' AND '2026-06-07'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND play_mode IN (1, 2, 3)
    UNION ALL
    -- 510K（多轮累计）
    SELECT
        app_id, uid, dt, resultguid,
        start_datetime AS event_time,
        time_cost,
        room_id, result_id,
        start_money, end_money,
        game_outcome_money, room_fee,
        CASE WHEN is_escape = 0 THEN 0 ELSE 1 END AS escape_flag,
        7 AS play_mode,
        1 AS is_crazyddz,
        settle_count
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521
      AND dt BETWEEN '2026-03-01' AND '2026-06-07'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
),
ranked AS (
    SELECT *,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY event_time ASC)  AS seq_asc,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY event_time DESC) AS seq_desc
    FROM unified
),
streaks AS (
    SELECT
        app_id, uid, dt,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT app_id, uid, dt, result_id, grp, COUNT(*) AS streak_len
        FROM (
            SELECT app_id, uid, dt, result_id,
                seq_asc - ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt, result_id ORDER BY seq_asc) AS grp
            FROM ranked
            WHERE result_id IN (1, 2)
        ) g
        GROUP BY app_id, uid, dt, result_id, grp
    ) s
    GROUP BY app_id, uid, dt
)
SELECT
    r.app_id,
    r.uid,
    r.dt,
    COUNT(resultguid) AS game_count,
    SUM(r.time_cost) AS total_play_seconds,
    ROUND(AVG(r.time_cost), 1) AS avg_game_seconds,
    COUNT(DISTINCT r.room_id) AS distinct_rooms,
    COUNT(CASE WHEN r.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN r.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN r.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ROUND(COUNT(CASE WHEN r.result_id = 2 THEN 1 END) * 100.0 / COUNT(*), 2) AS lose_rate,
    ANY_VALUE(st.max_win_streak) AS max_win_streak,
    ANY_VALUE(st.max_lose_streak) AS max_lose_streak,
    MAX(CASE WHEN r.seq_asc = 1 THEN r.start_money END) AS start_money,
    MAX(CASE WHEN r.seq_desc = 1 THEN r.end_money END) AS end_money,
    MAX(r.end_money) AS money_peak,
    MIN(r.end_money) AS money_valley,
    SUM(r.game_outcome_money) AS total_diff_money,
    SUM(r.room_fee) AS total_fee_paid,
    SUM(r.escape_flag) AS escape_count
FROM ranked r
LEFT JOIN streaks st ON r.app_id = st.app_id AND r.uid = st.uid AND r.dt = st.dt
GROUP BY r.app_id, r.uid, r.dt;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../ops/daily_data_ops.md)

## 注意事项

1. **玩法过滤**：仅汇总银子玩法（`play_mode IN (1, 2, 3, 7)`），积分玩法（4 / 5 / 6）请走 `dws_app_allgame_stat`
2. **APP 端过滤**：仅统计 APP 端用户（`group_id IN (6, 66, 8, 88, 33, 44, 77, 99)`）
3. **510K 接入**：通过 UNION ALL 把 `dws_crazyddz_daily_game` 拼成 play_mode=7，金流（`game_outcome_money`、`room_fee`）字段含义与经典系一致，可直接 SUM。510K 一局多轮结算的原始多条日志已在上游 `dws_crazyddz_daily_game` 合并为整局一行（见"510K 多轮结算"小节），本表无需处理多轮
4. **首末局**：`start_money` / `end_money` 按用户当日跨玩法时间序列取最早 / 最晚一局，不分玩法
5. **平局处理**：510K 存在平局（`result_id` 非 1/2），平局既不计胜也不计负，但计入 `game_count`。因此 `win_rate` 与 `lose_rate` 各以 `game_count` 为分母、**不互补**（`win_rate + lose_rate + 平局率 = 100`）；经典系无平局，两者之和约等于 100
6. **连胜连败**：只统计**连续的**胜（或负）局，平局视为中断。即 W-平-W 记为两段 1 连胜，而非一段 2 连胜。实现上 `seq_asc` 在含平局的全序列上生成，过滤平局（仅留 `result_id IN (1, 2)`）后序号出现空位，平局两侧自然断开，符合该定义
7. **数据完整性**：如用户当日无银子玩法对局，本表无对应记录

## 表数据流向

```text
tcy_temp.dws_ddz_daily_game ──────┐
(经典/不洗牌/癞子，play_mode 1,2,3) │
                                   ├─ UNION ALL ─→ dws_app_silvergame_stat（金流+参与, uid×dt）
tcy_temp.dws_crazyddz_daily_game ──┘
(510K，多轮，game_id=521)
                                   └─ 同时写入 dws_app_allgame_stat（玩法体验, uid×dt×play_mode）
                                       play_mode = 1, 2, 3, 7
```

## 留存分析示例

### 1. 首日银子谷值 → 次留

```sql
-- 用 game_stat：不需要分玩法
SELECT
    CASE
        WHEN s.money_valley < 0       THEN 'A:亏光(负)'
        WHEN s.money_valley < 5000    THEN 'B:底部<5k'
        WHEN s.money_valley < 10000   THEN 'C:底部5k-1w'
        ELSE 'D:安全>1w'
    END AS bottom_group,
    COUNT(DISTINCT s.uid) AS reg_user_count,
    COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) AS day1_retained,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT s.uid), 2) AS day1_rate
FROM tcy_temp.dws_app_silvergame_stat s
JOIN tcy_temp.dws_dq_app_daily_reg r ON s.uid = r.uid AND s.app_id = r.app_id AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND r.app_id = l.app_id AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260601
  AND r.app_id = 1880053
GROUP BY 1
ORDER BY 1;
```

### 2. 按首日对局数分析留存

```sql
SELECT
    r.reg_date,
    CASE
        WHEN g.game_count = 1               THEN '0:1局'
        WHEN g.game_count BETWEEN 2 AND 5   THEN '1:2-5局'
        WHEN g.game_count BETWEEN 6 AND 10  THEN '2:6-10局'
        ELSE '3:10局以上'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 6 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_silvergame_stat g ON r.uid = g.uid AND r.app_id = g.app_id AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND r.app_id = l.app_id AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260601
  AND r.app_id = 1880053
GROUP BY r.reg_date, game_count_group
ORDER BY r.reg_date, game_count_group;
```

### 3. 按首日胜率分析留存

```sql
SELECT
    r.reg_date,
    CASE
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(g.game_count), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_silvergame_stat g ON r.uid = g.uid AND r.app_id = g.app_id AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND r.app_id = l.app_id AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260601
  AND r.app_id = 1880053
  AND g.game_count > 0
GROUP BY r.reg_date, win_rate_group
ORDER BY r.reg_date, win_rate_group;
```

### 4. 与 allgame_stat 联用：金流画像 + 玩法体验

```sql
-- silvergame_stat 给整体金流画像，allgame_stat 给玩法体验
SELECT
    g.play_mode,
    CASE
        WHEN s.money_valley < 0 THEN 'A:亏光'
        WHEN s.money_valley < 5000 THEN 'B:底部<5k'
        ELSE 'C:安全'
    END AS bottom_group,
    COUNT(DISTINCT s.uid) AS user_count,
    ROUND(AVG(g.win_rate), 2) AS avg_win_rate,
    ROUND(AVG(g.total_diff_money), 0) AS avg_outcome
FROM tcy_temp.dws_app_silvergame_stat s
JOIN tcy_temp.dws_app_allgame_stat g
  ON s.app_id = g.app_id AND s.uid = g.uid AND s.dt = g.dt
WHERE s.dt = 20260601
  AND g.game_count > 0
GROUP BY g.play_mode, bottom_group
ORDER BY g.play_mode, bottom_group;
```

## 数据校验

### 上游明细层校验（dws_ddz_daily_game + dws_crazyddz_daily_game）

```sql
-- 1. 每局会计等式：game_outcome_money + room_fee 应等于 end_money - start_money
-- 银子玩法有金流波动，等式偏差意味着数据异常，聚合前必须先验
-- 经典系（单轮）
SELECT 'classic' AS source,
       dt,
       COUNT(*) AS total_rows,
       SUM(CASE WHEN ABS((game_outcome_money - room_fee + cut) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) AS mismatch_rows,
       ROUND(SUM(CASE WHEN ABS((game_outcome_money - room_fee + cut) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS mismatch_pct
FROM tcy_temp.dws_ddz_daily_game
WHERE dt BETWEEN '${START_DATE}' AND '${END_DATE}'
  AND play_mode IN (1, 2, 3)
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY dt
UNION ALL
-- 510K（多轮，已在 dws_crazyddz_daily_game 中汇总为整局一行）
SELECT '510k' AS source,
       dt,
       COUNT(*) AS total_rows,
       SUM(CASE WHEN ABS((game_outcome_money - room_fee + is_escape) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) AS mismatch_rows,
       ROUND(SUM(CASE WHEN ABS((game_outcome_money - room_fee + is_escape) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS mismatch_pct
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND dt BETWEEN '${START_DATE}' AND '${END_DATE}'
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY dt;
```

### 聚合层校验（dws_app_silvergame_stat ↔ 上游明细）

```sql
-- 2. 用户覆盖一致：silvergame_stat 去重 uid 数 = 上游明细去重 uid 数
WITH detail_users AS (
    SELECT DISTINCT app_id, uid, dt
    FROM (
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_ddz_daily_game
        WHERE dt BETWEEN '${START_DATE}' AND '${END_DATE}'
          AND play_mode IN (1, 2, 3)
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
        UNION ALL
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE game_id = 521
          AND dt BETWEEN '${START_DATE}' AND '${END_DATE}'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    ) u
)
SELECT s.dt,
       COUNT(DISTINCT s.uid) AS stat_users,
       COUNT(DISTINCT d.uid) AS detail_users
FROM tcy_temp.dws_app_silvergame_stat s
FULL OUTER JOIN detail_users d
  ON s.uid = d.uid AND s.dt = d.dt AND s.app_id = d.app_id
WHERE s.dt BETWEEN '${START_DATE}' AND '${END_DATE}'
GROUP BY s.dt;

-- 3. 总局数一致性：silvergame_stat 总局数 = 上游明细总行数
WITH detail_agg AS (
    SELECT app_id, uid, dt, COUNT(*) AS detail_games
    FROM (
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_ddz_daily_game
        WHERE dt BETWEEN '${START_DATE}' AND '${END_DATE}'
          AND play_mode IN (1, 2, 3)
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
        UNION ALL
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE game_id = 521
          AND dt BETWEEN '${START_DATE}' AND '${END_DATE}'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    ) u
    GROUP BY app_id, uid, dt
)
SELECT SUM(s.game_count) AS stat_total_games,
       SUM(d.detail_games) AS detail_total_games
FROM tcy_temp.dws_app_silvergame_stat s
FULL OUTER JOIN detail_agg d
  ON s.uid = d.uid AND s.dt = d.dt AND s.app_id = d.app_id
WHERE COALESCE(s.dt, d.dt) = '2026-06-08';

-- 4. 净输赢一致性：silvergame_stat.total_diff_money = 上游明细 SUM(game_outcome_money)
WITH detail_agg AS (
    SELECT app_id, uid, dt, SUM(game_outcome_money) AS detail_diff
    FROM (
        SELECT app_id, uid, dt, game_outcome_money
        FROM tcy_temp.dws_ddz_daily_game
        WHERE dt BETWEEN '${START_DATE}' AND '${END_DATE}'
          AND play_mode IN (1, 2, 3)
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
        UNION ALL
        SELECT app_id, uid, dt, game_outcome_money
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE game_id = 521
          AND dt BETWEEN '${START_DATE}' AND '${END_DATE}'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    ) u
    GROUP BY app_id, uid, dt
)
SELECT s.uid, s.dt,
       s.total_diff_money AS stat_diff,
       d.detail_diff
FROM tcy_temp.dws_app_silvergame_stat s
LEFT JOIN detail_agg d
  ON s.uid = d.uid AND s.dt = d.dt AND s.app_id = d.app_id
WHERE s.dt BETWEEN '${START_DATE}' AND '${END_DATE}'
  AND s.total_diff_money != d.detail_diff;
```

## 版本历史

> **文档版本**：v1.0
> **创建时间**：2026-06-11
> **更新说明**：
>
> - v1.0：初始版本
