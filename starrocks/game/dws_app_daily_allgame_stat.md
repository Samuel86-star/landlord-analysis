# DWS 中间表：APP 端每日全玩法体验聚合表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_daily_allgame_stat` |
| 全名 | `tcy_temp.dws_app_daily_allgame_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日跨玩法游戏体验聚合表，按 `uid × dt` 聚合 `dws_app_allgame_stat`，是各类"用户日级体验分析"的公共中间产物 |
| 粒度 | uid × dt（一个用户一天一行，跨所有 play_mode 聚合） |
| 数据延迟 | T+1：依赖 `dws_app_allgame_stat`，T 日数据在 T+1 日可产出 |

## 设计背景

`dws_app_allgame_stat` 的粒度为 `uid × dt × play_mode`（一个用户一天一种玩法一行），适合**按玩法拆分**的体验分析。但有一类高频分析只关心"用户当天整体的玩法体验"，不需要保留玩法维度：

| 分析场景 | 为什么需要跨玩法聚合 |
| ---- | ---- |
| 新增用户首日留存归因 | 关心"首日整体高倍局输赢"，不关心具体哪个玩法 |
| 用户日活跃画像 | 一行一个用户一天，避免一行多玩法导致的 JOIN 膨胀 |
| 留存宽表 `dws_app_firstday_retention` 的全玩法体验字段 | 只需 JOIN 一次 uid×dt 表，而非对 allgame_stat 子查询聚合 |

如果每次分析都对 `dws_app_allgame_stat` 做子查询聚合，既重复又慢。**解决方案**：预计算一张 `uid × dt` 粒度的聚合表，把跨 play_mode 的 SUM/MAX 一次性算好，后续任何用户日级体验分析只扫这一张表。

### 与上游表的定位区别

| 表 | 粒度 | 用途 | 关键特征 |
| ---- | ---- | ---- | ---- |
| `dws_app_allgame_stat` | uid × dt × play_mode | 玩法体验分析（控制玩法变量） | 保留玩法维度，玩法间不可比的指标（倍数/炸弹）在这里分玩法看 |
| **`dws_app_daily_allgame_stat`（本表）** | **uid × dt** | **用户日级整体体验分析** | **跨玩法 SUM/MAX 聚合，一行一用户一天，JOIN 友好** |

> **本质关系**：本表是 `dws_app_allgame_stat` 按 `uid × dt` 的上卷（roll-up）。玩法维度的指标（如"经典 vs 癞子的高倍局分布"）走 allgame_stat；用户整体维度的指标（如"首日整体高倍输赢对留存的影响"）走本表。

### 核心判断标准

> **需要分玩法看**（经典/癞子/510K 体验差异）→ 走 `dws_app_allgame_stat`
> **只看用户当天整体**（首日画像、留存归因、日活跃）→ 走本表

## 聚合规则说明

本表对 `dws_app_allgame_stat` 的字段按 `uid × dt` 聚合，聚合方式分三类：

| 字段类型 | 聚合方式 | 说明 |
| ---- | ---- | ---- |
| 计数类（`game_count`、各倍数段胜/负局数、炸弹局数等） | SUM | 跨玩法相加，反映用户当天总量 |
| 倍数类（`avg_magnification`） | AVG | 跨玩法平均，反映用户当天整体倍数水平 |
| 极值类（`max_magnification`） | MAX | 跨玩法取最大，反映用户当天最高倍数 |
| 种类类（`distinct_modes`） | COUNT(DISTINCT play_mode) | 当天游玩的玩法种类数 |

> **注意**：`avg_magnification` 用 AVG 跨玩法平均时，未按各玩法的对局数加权。若需加权平均倍数，应回到 `dws_app_allgame_stat` 自行计算。本表的 `avg_magnification` 仅供"整体水平"粗略参考。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| dt | date | 对局日期 | 2026-06-01 |
| uid | int | 玩家唯一标识 | 123456789 |
| distinct_modes | int | 当日游玩玩法种类数 | 2 |
| total_games | int | 当日全玩法总局数（跨 play_mode SUM） | 15 |
| avg_magnification | double | 当日全玩法平均倍数（跨 play_mode AVG） | 12.5 |
| max_magnification | int | 当日最高倍数（跨 play_mode MAX） | 96 |
| bomb_count | int | 当日炸弹总数（bomb_0~3plus_games 求和） | 8 |
| spring_count | int | 当日春天/反春总数 | 2 |
| multi_1_win | int | 倍数 = 1 的胜局数 | 0 |
| multi_1_lose | int | 倍数 = 1 的负局数 | 0 |
| multi_2_win | int | 倍数 = 2 的胜局数 | 0 |
| multi_2_lose | int | 倍数 = 2 的负局数 | 0 |
| multi_3_6_win | int | 倍数 [3, 6) 的胜局数 | 4 |
| multi_3_6_lose | int | 倍数 [3, 6) 的负局数 | 3 |
| multi_6_12_win | int | 倍数 [6, 12) 的胜局数 | 5 |
| multi_6_12_lose | int | 倍数 [6, 12) 的负局数 | 3 |
| multi_12_24_win | int | 倍数 [12, 24) 的胜局数 | 3 |
| multi_12_24_lose | int | 倍数 [12, 24) 的负局数 | 2 |
| multi_24_48_win | int | 倍数 [24, 48) 的胜局数 | 2 |
| multi_24_48_lose | int | 倍数 [24, 48) 的负局数 | 1 |
| multi_48_96_win | int | 倍数 [48, 96) 的胜局数 | 1 |
| multi_48_96_lose | int | 倍数 [48, 96) 的负局数 | 0 |
| multi_96_192_win | int | 倍数 [96, 192) 的胜局数 | 0 |
| multi_96_192_lose | int | 倍数 [96, 192) 的负局数 | 0 |
| multi_192_384_win | int | 倍数 [192, 384) 的胜局数 | 0 |
| multi_192_384_lose | int | 倍数 [192, 384) 的负局数 | 0 |
| multi_384_plus_win | int | 倍数 ≥ 384 的胜局数 | 0 |
| multi_384_plus_lose | int | 倍数 ≥ 384 的负局数 | 0 |

> **固定倍数段说明**：覆盖从 1x 到 384x+ 的完整倍数谱（10 区间 × 胜负 = 20 字段）。1x/2x 为未来叫分调整预留，当前斗地主底分固定为 3。每个区间的胜/负局数跨 play_mode SUM 聚合，所有区间胜局数之和应等于 `total_games` 中非平局的部分。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_app_daily_allgame_stat (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  -- 跨玩法聚合指标
  `distinct_modes` int(11) NULL COMMENT "当日游玩玩法种类数",
  `total_games` int(11) NULL COMMENT "当日全玩法总局数",
  `avg_magnification` double NULL COMMENT "当日全玩法平均倍数(跨玩法AVG)",
  `max_magnification` int(11) NULL COMMENT "当日最高倍数(跨玩法MAX)",
  `bomb_count` int(11) NULL COMMENT "当日炸弹总数",
  `spring_count` int(11) NULL COMMENT "当日春天/反春总数",
  -- 固定倍数段（跨玩法 SUM）
  `multi_1_win` int(11) NULL COMMENT "倍数=1胜局数",
  `multi_1_lose` int(11) NULL COMMENT "倍数=1负局数",
  `multi_2_win` int(11) NULL COMMENT "倍数=2胜局数",
  `multi_2_lose` int(11) NULL COMMENT "倍数=2负局数",
  `multi_3_6_win` int(11) NULL COMMENT "倍数[3,6)胜局数",
  `multi_3_6_lose` int(11) NULL COMMENT "倍数[3,6)负局数",
  `multi_6_12_win` int(11) NULL COMMENT "倍数[6,12)胜局数",
  `multi_6_12_lose` int(11) NULL COMMENT "倍数[6,12)负局数",
  `multi_12_24_win` int(11) NULL COMMENT "倍数[12,24)胜局数",
  `multi_12_24_lose` int(11) NULL COMMENT "倍数[12,24)负局数",
  `multi_24_48_win` int(11) NULL COMMENT "倍数[24,48)胜局数",
  `multi_24_48_lose` int(11) NULL COMMENT "倍数[24,48)负局数",
  `multi_48_96_win` int(11) NULL COMMENT "倍数[48,96)胜局数",
  `multi_48_96_lose` int(11) NULL COMMENT "倍数[48,96)负局数",
  `multi_96_192_win` int(11) NULL COMMENT "倍数[96,192)胜局数",
  `multi_96_192_lose` int(11) NULL COMMENT "倍数[96,192)负局数",
  `multi_192_384_win` int(11) NULL COMMENT "倍数[192,384)胜局数",
  `multi_192_384_lose` int(11) NULL COMMENT "倍数[192,384)负局数",
  `multi_384_plus_win` int(11) NULL COMMENT "倍数>=384胜局数",
  `multi_384_plus_lose` int(11) NULL COMMENT "倍数>=384负局数"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `dt`, `uid`)
COMMENT "APP端用户每日全玩法体验聚合表(跨play_mode, uid×dt)"
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

> **说明**：本表是 `dws_app_allgame_stat` 的 `uid × dt` 上卷，无需扫明细表，每天 T+1 跑一次即可。

```sql
-- 参数：将 '2026-06-16' 替换为目标日期（dt）
DELETE FROM tcy_temp.dws_app_daily_allgame_stat
WHERE app_id = 1880053 AND dt = '2026-06-16';

INSERT INTO tcy_temp.dws_app_daily_allgame_stat
SELECT
    app_id, dt, uid,
    COUNT(DISTINCT play_mode)     AS distinct_modes,
    SUM(game_count)               AS total_games,
    AVG(avg_magnification)        AS avg_magnification,
    MAX(max_magnification)        AS max_magnification,
    SUM(bomb_0_games + bomb_1_games + bomb_2_games + bomb_3plus_games) AS bomb_count,
    SUM(spring_count)             AS spring_count,
    SUM(multi_1_win)          AS multi_1_win,          SUM(multi_1_lose)         AS multi_1_lose,
    SUM(multi_2_win)          AS multi_2_win,          SUM(multi_2_lose)         AS multi_2_lose,
    SUM(multi_3_6_win)        AS multi_3_6_win,        SUM(multi_3_6_lose)       AS multi_3_6_lose,
    SUM(multi_6_12_win)       AS multi_6_12_win,       SUM(multi_6_12_lose)      AS multi_6_12_lose,
    SUM(multi_12_24_win)      AS multi_12_24_win,      SUM(multi_12_24_lose)     AS multi_12_24_lose,
    SUM(multi_24_48_win)      AS multi_24_48_win,      SUM(multi_24_48_lose)     AS multi_24_48_lose,
    SUM(multi_48_96_win)      AS multi_48_96_win,      SUM(multi_48_96_lose)     AS multi_48_96_lose,
    SUM(multi_96_192_win)     AS multi_96_192_win,     SUM(multi_96_192_lose)    AS multi_96_192_lose,
    SUM(multi_192_384_win)    AS multi_192_384_win,    SUM(multi_192_384_lose)   AS multi_192_384_lose,
    SUM(multi_384_plus_win)   AS multi_384_plus_win,   SUM(multi_384_plus_lose)  AS multi_384_plus_lose
FROM tcy_temp.dws_app_allgame_stat
WHERE app_id = 1880053
  AND dt = '2026-06-16'
GROUP BY app_id, uid, dt;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../ops/daily_data_ops.md)

## 注意事项

1. **上游依赖**：本表依赖 `dws_app_allgame_stat`（v1.2，含固定倍数段字段）。`dws_app_allgame_stat` 未初始化的日期，本表也无法产出
2. **聚合方向**：本表只做跨 play_mode 聚合，**不保留玩法维度**。需要分玩法分析时回到 `dws_app_allgame_stat`
3. **avg_magnification 非加权**：跨 play_mode 用 AVG 简单平均，未按对局数加权。需精确加权倍数请回 allgame_stat 自行计算
4. **无金流字段**：本表不含 `total_diff_money` / `money_valley` 等金流字段。金流归因走 `dws_app_silvergame_stat`（银子）/ `dws_app_scoregame_stat`（积分），它们本身就是 uid × dt 粒度，无需再聚合
5. **DUPLICATE KEY**：与 `dws_app_allgame_stat` 一致采用 DUPLICATE KEY 模型，增量导入用 DELETE + INSERT（不支持高效 UPDATE）
6. **colocate_with**：与上游表共用 `group_daily_data` Colocation Group，uid JOIN 时本地计算

## 表数据流向

```text
tcy_temp.dws_app_allgame_stat                （玩法体验明细, uid × dt × play_mode）
            ↓  按 uid × dt 聚合（SUM/MAX/AVG/COUNT DISTINCT）
tcy_temp.dws_app_daily_allgame_stat          （用户日级体验, uid × dt）  ← 本表
            ↓  关联分析
tcy_temp.dws_app_firstday_retention          （首日留存宽表，JOIN 本表取首日体验字段）
tcy_temp.dws_dq_app_daily_reg                （注册用户宽表）
```

## 使用示例

### 1. 用户首日整体高倍局输赢 → 留存

```sql
-- 用 daily_allgame_stat：一行一用户，无需子查询聚合
SELECT
    CASE
        WHEN multi_384_plus_lose > 0 THEN 'A: 输384x+'
        WHEN multi_192_384_lose > 0 THEN 'B: 输192-384x'
        WHEN multi_96_192_lose > 0  THEN 'C: 输96-192x'
        WHEN multi_48_96_lose > 0   THEN 'D: 输48-96x'
        WHEN multi_24_48_lose > 0   THEN 'E: 输24-48x'
        WHEN multi_384_plus_win > 0 THEN 'F: 仅赢高倍(384x+)'
        WHEN total_games > 0        THEN 'G: 仅低倍(<24x)'
        ELSE 'Z: 无对局'
    END AS multi_group,
    COUNT(DISTINCT t.uid) AS user_count,
    ROUND(AVG(t.total_games), 1) AS avg_games
FROM tcy_temp.dws_app_daily_allgame_stat t
JOIN tcy_temp.dws_dq_app_daily_reg r
    ON r.app_id = t.app_id AND r.uid = t.uid AND r.reg_date = t.dt
WHERE t.app_id = 1880053
  AND t.dt BETWEEN '2026-03-01' AND '2026-05-17'
GROUP BY 1
ORDER BY 1;
```

### 2. 与 firstday_retention 联用：首日体验 × 留存

```sql
-- firstday_retention 已内嵌首日体验字段（来自本表的 reg_date 当天聚合）
SELECT
    CASE
        WHEN multi_24_48_lose + multi_48_96_lose + multi_96_192_lose
           + multi_192_384_lose + multi_384_plus_lose > 0
            THEN 'A: 有高倍输(>=24x)'
        WHEN multi_24_48_win + multi_48_96_win + multi_96_192_win
           + multi_192_384_win + multi_384_plus_win > 0
            THEN 'B: 有高倍赢(>=24x)'
        ELSE 'C: 无高倍对局'
    END AS high_multi_group,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(d1_game), 2) AS d1_rate,
    ROUND(AVG(d7_game), 2) AS d7_rate,
    ROUND(AVG(d30_game), 2) AS d30_rate
FROM tcy_temp.dws_app_firstday_retention
WHERE app_id = 1880053
  AND reg_date BETWEEN '2026-03-01' AND '2026-05-17'
  AND d30_game IS NOT NULL
GROUP BY 1
ORDER BY 1;
```

## 版本历史

> **文档版本**：v1.0
> **创建时间**：2026-06-17
> **更新说明**：
>
> - v1.0：初始版本，作为 `dws_app_allgame_stat` 的 uid × dt 上卷表，服务首日留存宽表及各类用户日级体验分析
