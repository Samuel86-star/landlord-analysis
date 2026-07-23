# DWS 中间表：APP 端积分玩法每日统计表（参与度）

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_scoregame_stat` |
| 全名 | `tcy_temp.dws_app_scoregame_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日积分玩法统计表，仅汇总参与度与胜负，不含金流字段（积分玩法免费） |
| 粒度 | uid × dt（一个用户一天一行，跨积分玩法） |

> **命名说明**：表名中的 `scoregame` 明确标识本表只覆盖**积分玩法**（4=积分PC、5=比赛、6=好友房），不含银子玩法。与 `dws_app_silvergame_stat` 为姊妹表，两表 play_mode 互不重叠、并集即全部玩法。

## 姊妹表：银子 / 积分按币种分表

金流字段（`total_diff_money`、`money_valley` 等）只在同币种内可加总，因此按币种拆成两张同构表，命名以币种区分：

| 表名 | 币种 | play_mode | 状态 |
| ---- | ---- | --------- | ---- |
| `dws_app_silvergame_stat` | 银子 | 1=经典, 2=不洗牌, 3=癞子, 7=510K | ✅ 已建 |
| `dws_app_scoregame_stat`（本表） | 积分 | 4=积分(PC), 5=比赛, 6=好友房 | ✅ 已建 |

> **边界约定**：两表 play_mode 互不重叠，并集即全部玩法。`dws_app_scoregame_stat` 因积分玩法免费，去掉了银子表中的全部金流字段（`start_money`、`end_money`、`money_peak`、`money_valley`、`total_diff_money`、`total_fee_paid`），仅保留参与度与胜负指标。需要跨币种的玩法体验对比走 `dws_app_allgame_stat`（全玩法、按 play_mode 拆分）。

## 设计背景

`dws_ddz_daily_game` 是对局级明细表，包含所有玩法（1~6）的数据。银子玩法已有 `dws_app_silvergame_stat` 承接金流与参与度分析；积分玩法（4/5/6）使用积分币种且免费，不存在金流波动问题，其留存归因的核心维度是**参与度与胜负体验**，而非经济压力。

因此本表只承担**参与度 + 胜负**这一维度，不含任何金流字段。玩法体验维度（倍数/炸弹等）交由 `dws_app_allgame_stat`（uid × dt × play_mode）。

### 与 dws_app_silvergame_stat 的分工

| | dws_app_silvergame_stat | dws_app_scoregame_stat（本表） |
| ---- | ---- | ---- |
| 粒度 | uid × dt | uid × dt |
| 玩法范围 | 仅银子玩法（1, 2, 3, 7） | 仅积分玩法（4, 5, 6） |
| 核心指标 | 参与度、金流（银子可加总） | 参与度、胜负（无金流） |
| 典型问题 | "新用户首日银子亏了多少？" | "积分玩家首日打了几局？胜率如何？" |
| 金流字段 | ✅ 含 start_money 等 6 个金流字段 | ❌ 积分玩法免费，无需金流 |

### 与 dws_app_allgame_stat 的分工

| | dws_app_scoregame_stat（本表） | dws_app_allgame_stat |
| ---- | ---- | ---- |
| 粒度 | uid × dt | uid × dt × play_mode |
| 玩法范围 | 仅积分玩法（4, 5, 6） | 所有玩法（1, 2, 3, 4, 5, 6, 7） |
| 核心指标 | 参与度、胜负（跨积分玩法汇总） | 倍数、炸弹、玩法内胜率（玩法间不可比） |
| 典型问题 | "积分玩法整体参与度如何？" | "比赛 vs 好友房，哪个胜率更高？" |

### 核心判断标准

> **积分玩法免费** → 无金流波动 → 不存在银子玩法的"破产信号"问题
> **留存归因路径不同** → 积分玩家留存主要受参与度（局数/时长）和胜负体验驱动，而非经济压力

## 玩法分类说明

| play_mode | 玩法 | 币种 | 是否纳入本表 |
| --------- | ---- | ---- | ------------ |
| 4 | 积分（PC 端） | 积分 | ✅ |
| 5 | 比赛（APP/小游戏端） | 积分 | ✅ |
| 6 | 好友房 | 积分 | ✅ |
| 1 | 经典 | 银子 | ❌ 走 silvergame_stat |
| 2 | 不洗牌 | 银子 | ❌ 走 silvergame_stat |
| 3 | 癞子 | 银子 | ❌ 走 silvergame_stat |
| 7 | 510K | 银子 | ❌ 走 silvergame_stat |

> **说明**：本表仅统计 APP 端用户（`group_id IN (6, 66, 8, 88, 33, 44, 77, 99)`）的积分玩法。

### 数据来源说明

与银子玩法不同，积分玩法的数据**只来自 `dws_ddz_daily_game`**，无需 UNION ALL `dws_crazyddz_daily_game`（510K 是银子玩法，无积分版本）。因此本表的数据流更简单，仅从一张上游表聚合。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| uid | int | 玩家唯一标识 | 123456789 |
| dt | date | 对局日期 | 2026-06-08 |
| game_count | int | 当日对局总数 | 8 |
| total_play_seconds | int | 当日总游戏时长（秒） | 2400 |
| avg_game_seconds | double | 平均每局时长（秒） | 300.0 |
| distinct_rooms | int | 当日游玩房间数 | 2 |
| win_count | int | 胜利局数（按 result_id=1） | 5 |
| lose_count | int | 失败局数（按 result_id=2） | 3 |
| win_rate | double | 胜率（百分比）= win_count / game_count | 62.50 |
| lose_rate | double | 负率（百分比）= lose_count / game_count | 37.50 |
| max_win_streak | int | 最大连胜（跨积分玩法按时间序列） | 2 |
| max_lose_streak | int | 最大连败（跨积分玩法按时间序列） | 1 |
| escape_count | int | 当日逃跑次数 | 0 |

> **与银子表对比**：本表去掉了 `start_money`、`end_money`、`money_peak`、`money_valley`、`total_diff_money`、`total_fee_paid` 共 6 个金流字段。积分玩法免费，这些字段无意义。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_app_scoregame_stat (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  -- 参与度
  `game_count` int(11) NULL COMMENT "总局数",
  `total_play_seconds` int(11) NULL COMMENT "总时长（秒）",
  `avg_game_seconds` double NULL COMMENT "平均每局时长（秒）",
  `distinct_rooms` int(11) NULL COMMENT "不同房间数",
  -- 胜负
  `win_count` int(11) NULL COMMENT "胜利局数",
  `lose_count` int(11) NULL COMMENT "失败局数",
  `win_rate` double NULL COMMENT "胜率（%）= win_count/game_count",
  `lose_rate` double NULL COMMENT "负率（%）= lose_count/game_count",
  `max_win_streak` int(11) NULL COMMENT "最大连胜（跨积分玩法）",
  `max_lose_streak` int(11) NULL COMMENT "最大连败（跨积分玩法）",
  -- 行为
  `escape_count` int(11) NULL COMMENT "逃跑次数"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `dt`)
COMMENT "APP端积分玩法用户每日统计（参与度+胜负，无金流）"
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

按天 `DELETE + INSERT`（幂等可重跑），脚本：[`batch_insert_app_scoregame_stat.py`](../../ops/py/batch_insert_app_scoregame_stat.py)

> **依赖**：dws_ddz_daily_game 对应日期需先回填。

```powershell
# 单天
py -3 -u .\batch_insert_app_scoregame_stat.py --start 20260617 --end 20260617

# 区间回填
py -3 -u .\batch_insert_app_scoregame_stat.py --start 2026-06-01 --end 2026-06-08

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_app_scoregame_stat.py --start 20260617 --end 20260617 --dry-run
```


> **说明**：积分玩法数据仅来自 `dws_ddz_daily_game`（经典斗地主明细表），无需 UNION ALL `dws_crazyddz_daily_game`（510K 为银子玩法，无积分版本）。积分玩法不存在平局，`win_rate + lose_rate = 100`。

```sql
-- 批量初始化指定时间段内的数据
-- 参数说明：
--   ${START_DATE}：起始日期（date 格式，如 '2026-03-01'）
--   ${END_DATE}：结束日期（date 格式，如 '2026-06-10'）
INSERT INTO tcy_temp.dws_app_scoregame_stat
WITH ranked AS (
    SELECT *,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY game_datetime ASC)  AS seq_asc,
        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY game_datetime DESC) AS seq_desc
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53
      AND dt between '${START_DATE}' and '${END_DATE}'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND play_mode IN (4, 5, 6)
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
    COUNT(*) AS game_count,
    SUM(r.timecost) AS total_play_seconds,
    ROUND(AVG(r.timecost), 1) AS avg_game_seconds,
    COUNT(DISTINCT r.room_id) AS distinct_rooms,
    COUNT(CASE WHEN r.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN r.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN r.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ROUND(COUNT(CASE WHEN r.result_id = 2 THEN 1 END) * 100.0 / COUNT(*), 2) AS lose_rate,
    ANY_VALUE(st.max_win_streak) AS max_win_streak,
    ANY_VALUE(st.max_lose_streak) AS max_lose_streak,
    SUM(CASE WHEN r.cut != 0 THEN 1 ELSE 0 END) AS escape_count
FROM ranked r
LEFT JOIN streaks st ON r.app_id = st.app_id AND r.uid = st.uid AND r.dt = st.dt
GROUP BY r.app_id, r.uid, r.dt;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../ops/daily_data_ops.md)

## 注意事项

1. **玩法过滤**：仅汇总积分玩法（`play_mode IN (4, 5, 6)`），银子玩法（1 / 2 / 3 / 7）请走 `dws_app_silvergame_stat`
2. **APP 端过滤**：仅统计 APP 端用户（`group_id IN (6, 66, 8, 88, 33, 44, 77, 99)`）
3. **无金流字段**：积分玩法免费，本表不含 `start_money`、`end_money`、`money_peak`、`money_valley`、`total_diff_money`、`total_fee_paid` 字段
4. **单数据源**：积分玩法仅来自 `dws_ddz_daily_game`，无需 UNION ALL `dws_crazyddz_daily_game`（510K 为银子玩法）
5. **无平局**：积分玩法不存在平局，`win_rate + lose_rate = 100`
6. **连胜连败**：跨积分玩法（4/5/6）按时间序列计算，逻辑与银子表一致
7. **数据完整性**：如用户当日无积分玩法对局，本表无对应记录

## 表数据流向

```text
tcy_temp.dws_ddz_daily_game ──────→ dws_app_scoregame_stat（参与度+胜负, uid×dt）
(积分/比赛/好友房，play_mode 4,5,6)
                                  └─ 同时写入 dws_app_allgame_stat（玩法体验, uid×dt×play_mode）
                                      play_mode = 4, 5, 6
```

## 分析示例

### 1. 首日积分玩法参与度 → 次留

```sql
SELECT
    CASE
        WHEN g.game_count = 1               THEN '0:1局'
        WHEN g.game_count BETWEEN 2 AND 5   THEN '1:2-5局'
        WHEN g.game_count BETWEEN 6 AND 10  THEN '2:6-10局'
        ELSE '3:10局以上'
    END AS game_count_group,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT g.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
JOIN tcy_temp.dws_app_scoregame_stat g ON r.uid = g.uid AND r.app_id = g.app_id AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND r.app_id = l.app_id AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260601
  AND r.app_id = 1880053
GROUP BY game_count_group
ORDER BY game_count_group;
```

### 2. 按首日胜率分析留存

```sql
SELECT
    CASE
        WHEN g.win_rate < 30 THEN 'A: <30%'
        WHEN g.win_rate < 50 THEN 'B: 30-50%'
        WHEN g.win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(AVG(g.game_count), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d'), INTERVAL 1 DAY) THEN r.uid END) * 100.0 / COUNT(DISTINCT g.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
JOIN tcy_temp.dws_app_scoregame_stat g ON r.uid = g.uid AND r.app_id = g.app_id AND r.reg_date = g.dt
LEFT JOIN tcy_temp.dws_dq_daily_login l ON r.uid = l.uid AND r.app_id = l.app_id AND l.login_date > DATE_FORMAT(CAST(r.reg_date AS VARCHAR), '%Y%m%d')
WHERE r.reg_date = 20260601
  AND r.app_id = 1880053
  AND g.game_count > 0
GROUP BY win_rate_group
ORDER BY win_rate_group;
```

### 3. 银子 vs 积分玩法参与度对比

```sql
SELECT
    CASE
        WHEN s.uid IS NOT NULL AND g.uid IS NOT NULL THEN 'both'
        WHEN s.uid IS NOT NULL THEN 'silver_only'
        ELSE 'score_only'
    END AS player_type,
    COUNT(DISTINCT COALESCE(s.uid, g.uid)) AS user_count,
    ROUND(AVG(COALESCE(s.game_count, 0)), 1) AS avg_silver_games,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_score_games
FROM tcy_temp.dws_app_silvergame_stat s
FULL OUTER JOIN tcy_temp.dws_app_scoregame_stat g
  ON s.uid = g.uid AND s.app_id = g.app_id AND s.dt = g.dt
WHERE COALESCE(s.dt, g.dt) = '2026-06-08'
  AND COALESCE(s.app_id, g.app_id) = 1880053
GROUP BY player_type
ORDER BY player_type;
```

## 数据校验

### 聚合层校验（dws_app_scoregame_stat ↔ 上游明细）

```sql
-- 1. 用户覆盖一致：scoregame_stat 去重 uid 数 = 上游明细去重 uid 数
WITH detail_users AS (
    SELECT DISTINCT app_id, uid, dt
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt BETWEEN '${START_DATE}' AND '${END_DATE}'
      AND game_id = 53
      AND play_mode IN (4, 5, 6)
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
)
SELECT g.dt,
       COUNT(DISTINCT g.uid) AS stat_users,
       COUNT(DISTINCT d.uid) AS detail_users
FROM tcy_temp.dws_app_scoregame_stat g
FULL OUTER JOIN detail_users d
  ON g.uid = d.uid AND g.dt = d.dt AND g.app_id = d.app_id
WHERE g.dt BETWEEN '${START_DATE}' AND '${END_DATE}'
GROUP BY g.dt;

-- 2. 总局数一致性：scoregame_stat 总局数 = 上游明细总行数
WITH detail_agg AS (
    SELECT app_id, uid, dt, COUNT(*) AS detail_games
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt BETWEEN '${START_DATE}' AND '${END_DATE}'
      AND game_id = 53
      AND play_mode IN (4, 5, 6)
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    GROUP BY app_id, uid, dt
)
SELECT SUM(g.game_count) AS stat_total_games,
       SUM(d.detail_games) AS detail_total_games
FROM tcy_temp.dws_app_scoregame_stat g
FULL OUTER JOIN detail_agg d
  ON g.uid = d.uid AND g.dt = d.dt AND g.app_id = d.app_id
WHERE COALESCE(g.dt, d.dt) BETWEEN '${START_DATE}' AND '${END_DATE}';
```

### 跨表边界校验（silvergame_stat ↔ scoregame_stat）

```sql
-- 3. 两表覆盖用户无交叉：银子与积分玩法用户不重叠检查
SELECT COUNT(DISTINCT s.uid) AS silver_users,
       COUNT(DISTINCT g.uid) AS score_users,
       COUNT(DISTINCT CASE WHEN s.uid = g.uid THEN s.uid END) AS overlap_users
FROM tcy_temp.dws_app_silvergame_stat s
FULL OUTER JOIN tcy_temp.dws_app_scoregame_stat g
  ON s.uid = g.uid AND s.app_id = g.app_id AND s.dt = g.dt
WHERE COALESCE(s.dt, g.dt) BETWEEN '${START_DATE}' AND '${END_DATE}'
  AND COALESCE(s.app_id, g.app_id) = 1880053;
```

## 版本历史

> **文档版本**：v1.0
> **创建时间**：2026-06-11
> **更新说明**：
>
> - v1.0：初始版本
