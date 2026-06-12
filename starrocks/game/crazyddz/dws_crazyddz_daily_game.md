# DWS 中间表：疯狂斗地主每日对局战绩表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_crazyddz_daily_game` |
| 全名 | `tcy_temp.dws_crazyddz_daily_game` |
| 类型 | DWS 层中间表（每日增量，**T-1 可用**） |
| 描述 | 疯狂斗地主对局战绩表，从原始战绩表中提取并聚合疯狂斗地主玩法数据 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |
| 数据延迟 | **T-1**：上游 raw 层已通过 min_dt 机制回补跨天对局，T 日数据在 T+1 日可产出 |

## 设计背景

疯狂斗地主（game_id = 521）的对局日志存储在原始 `dwd_game_combat_si` 战绩表中，但其数据结构与传统斗地主不同：

1. **多轮结算**：疯狂斗地主一局游戏内可能包含多轮结算，每轮结算产生一条记录
2. **跨天对局**：部分对局可能跨越两天（如 T 日开局、T+1 日结束），上游 `crazyddz_daily_game_raw` 已通过 min_dt 机制回补跨天记录，本表直接读取即可
3. **累计统计**：需要通过 `row_start` 和 `row_end` 识别开局和结束记录，聚合计算整局数据
4. **路径追踪**：记录每轮的输赢金额和倍数变化路径

**解决方案**：

1. 先通过 `target_resultguids` 找出指定日期有服务费记录（`fee != 0`）的对局 GUID
2. 通过 `INNER JOIN` 关联这些 resultguid，上游 raw 层已通过 min_dt 覆盖 dt，直接 `WHERE dt = T` 即可
3. 使用 `ROW_NUMBER()` 窗口函数识别开局（row_start = 1）和结束（row_end = 1）记录
4. 聚合计算整局的累计统计数据
5. 使用 `GROUP_CONCAT` 记录输赢金额和倍数的变化路径

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_id | int | 游戏 ID | 521 |
| dt | date | 对局日期 | 2026-04-27 |
| uid | int | 玩家 ID | 123456789 |
| resultguid | varchar(64) | 本局战绩 ID | "abc123xyz" |
| start_datetime | datetime | 开局时间 | 2026-04-27 10:30:00 |
| end_datetime | datetime | 结束时间 | 2026-04-27 10:35:00 |
| time_cost | int | 总耗时（秒） | 180 |
| room_id | int | 房间号 | 1001 |
| room_currency_lower | bigint | 进入房间所需最少携带货币 | 1000 |
| room_currency_upper | bigint | 进入房间最大携带货币 | 10000 |
| robot | tinyint | 机器人标记：1=机器人，其他=真人 | 0 |
| role | tinyint | 角色：1=地主，2=农民 | 1 |
| chairno | int | 座位号 | 0 |
| result_id | tinyint | 最终结果：1=获胜，2=失败，3=平局（row_end 的 result_id 为 NULL 时根据 game_deposit_diff 兜底推断） | 1 |
| play_mode | tinyint | 玩法分类：7=五十K | 7 |
| room_base | int | 房间底分 | 100 |
| room_fee | int | 房间服务费 | 10 |
| start_money | bigint | 开局时货币数量 | 5000 |
| end_money | bigint | 结束时货币数量 | 5500 |
| game_outcome_money | bigint | 游戏输赢（不包括服务费，统一字段） | 600 |
| game_outcome_gdp | bigint | 游戏内货币变化绝对值累计 | 1500 |
| is_escape | int | 逃跑标记（!=0 代表存在逃跑行为） | 0 |
| total_magnification | bigint | 累计倍数 | 36 |
| app_id | int | 应用 ID | 1880053 |
| app_code | varchar(32) | 应用 code（配合 group_id 区分客户端开发语言） | zgda |
| group_id | int | 平台分组 ID（区分 PC/APP/小游戏） | 6 |
| channel_id | int | 渠道号 | 1001 |
| afk_turn_cnt | int | 托管出牌次数 | 0 |
| settle_count | int | 结算轮数 | 3 |
| deposit_diff_path | varchar(65533) | 每轮输赢金额路径（用 # 分隔） | "-100#200#-50#450" |
| deposit_magnification_path | varchar(65533) | 每轮倍数路径（用 # 分隔） | "3#6#2#9" |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_crazyddz_daily_game (
  `game_id` int(11) NULL COMMENT "游戏ID",
  `dt` date NOT NULL COMMENT "对局日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `resultguid` varchar(64) NOT NULL COMMENT "对局GUID",
  `start_datetime` datetime NULL COMMENT "开局时间",
  `end_datetime` datetime NULL COMMENT "结束时间",
  `time_cost` int(11) NULL COMMENT "总耗时（秒）",
  `room_id` int(11) NULL COMMENT "房间ID",
  `room_currency_lower` bigint(20) NULL COMMENT "进入房间所需最少携带货币",
  `room_currency_upper` bigint(20) NULL COMMENT "进入房间最大携带货币",
  `robot` tinyint(4) NULL COMMENT "机器人标记：1=机器人，其他=真人",
  `role` tinyint(4) NULL COMMENT "角色：1=地主，2=农民",
  `chairno` int(11) NULL COMMENT "座位号",
  `result_id` tinyint(4) NULL COMMENT "最终结果：1=获胜，2=失败，3=平局",
  `play_mode` tinyint(4) NULL COMMENT "玩法分类：7=五十K",
  `room_base` int(11) NULL COMMENT "房间底分",
  `room_fee` int(11) NULL COMMENT "房间服务费",
  `start_money` bigint(20) NULL COMMENT "开局时货币数量",
  `end_money` bigint(20) NULL COMMENT "结束时货币数量",
  `game_outcome_money` bigint(20) NULL COMMENT "游戏输赢（不包括服务费）",
  `game_outcome_gdp` bigint(20) NULL COMMENT "游戏内货币变化绝对值累计",
  `is_escape` int(11) NULL COMMENT "逃跑标记（!=0代表存在逃跑行为）",
  `total_magnification` bigint(20) NULL COMMENT "累计倍数",
  `app_id` int(11) NULL COMMENT "应用ID",
  `app_code` varchar(32) NULL COMMENT "应用code",
  `group_id` int(11) NULL COMMENT "平台分组ID",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `afk_turn_cnt` int(11) NULL COMMENT "托管出牌次数",
  `settle_count` int(11) NULL COMMENT "结算轮数",
  `deposit_diff_path` varchar(65533) NULL COMMENT "每轮输赢金额路径",
  `deposit_magnification_path` varchar(65533) NULL COMMENT "每轮倍数路径"
) ENGINE=OLAP
DUPLICATE KEY(`game_id`, `dt`, `uid`)
COMMENT "疯狂斗地主每日对局战绩表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-120",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);
```

### 数据初始化SQL

```sql
-- 初始化指定时间段内的对局数据
-- 参数说明：
--   ${START_DATE}：起始日期（date 格式，如 '2026-04-01'）
--   ${END_DATE}：结束日期（date 格式，如 '2026-04-27'）
-- 上游 raw 层已通过 min_dt 覆盖 dt，无需扩展日期范围处理跨天对局
INSERT INTO tcy_temp.dws_crazyddz_daily_game
WITH target_resultguids AS (
    SELECT DISTINCT resultguid
    FROM tcy_temp.crazyddz_daily_game_raw
    WHERE game_id = 521 
      AND dt BETWEEN '2026-03-01' AND '2026-06-01'
      AND fee != 0
),
ranked_combat AS (
    SELECT
        raw.*,
        SUM(CASE WHEN raw.fee = 0 AND raw.cut = 0 AND raw.result_id IS NULL THEN raw.depositdiff ELSE 0 END) OVER(PARTITION BY raw.resultguid, raw.uid) AS game_win_loss,
        ROW_NUMBER() OVER(PARTITION BY raw.resultguid, raw.uid ORDER BY raw.result_id, raw.game_datetime) as row_start,
        ROW_NUMBER() OVER(PARTITION BY raw.resultguid, raw.uid ORDER BY raw.result_id desc, raw.game_datetime desc) as row_end
    FROM tcy_temp.crazyddz_daily_game_raw raw
    INNER JOIN target_resultguids tr ON raw.resultguid = tr.resultguid
    WHERE raw.game_id = 521
       AND raw.dt BETWEEN '2026-03-01' AND '2026-06-01'
)
SELECT
    game_id,
    MAX(CASE WHEN row_start = 1 THEN dt END) AS dt,
    uid,
    resultguid,
    MAX(CASE WHEN row_start = 1 THEN game_datetime END) AS start_datetime,
    MAX(CASE WHEN row_end = 1 THEN game_datetime END) AS end_datetime,
    SUM(timecost) AS time_cost,
    MAX(CASE WHEN row_start = 1 THEN room_id END) AS room_id,
    MAX(CASE WHEN row_start = 1 THEN room_currency_lower END) AS room_currency_lower,
    MAX(CASE WHEN row_start = 1 THEN room_currency_upper END) AS room_currency_upper,
    MAX(CASE WHEN row_start = 1 THEN robot END) AS robot,
    MAX(CASE WHEN row_start = 1 THEN `role` END) AS role,
    MAX(CASE WHEN row_start = 1 THEN chairno END) AS chairno,
    COALESCE(
        MAX(result_id),
        CASE WHEN MAX(game_win_loss) > 0 THEN 1
             WHEN MAX(game_win_loss) < 0 THEN 2
             ELSE 3
        END
    ) AS result_id,
    7 AS play_mode,
    MAX(basedeposit) AS room_base,
    MAX(fee) AS room_fee,
    MAX(CASE WHEN row_start = 1 THEN olddeposit END) AS start_money,
    MAX(CASE WHEN row_start = 1 THEN olddeposit END) + SUM(depositdiff) AS end_money,
    IFNULL(SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN depositdiff END), 0) AS game_outcome_money,
    IFNULL(SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN ABS(depositdiff) END), 0) AS game_outcome_gdp,
    IFNULL(MAX(CASE WHEN cut != 0 THEN cut END), 0) AS is_escape,
    SUM(ABS(magnification)) AS total_magnification,
    MAX(CASE WHEN row_start = 1 THEN app_id END) AS app_id,
    COALESCE(MAX(CASE WHEN row_start = 1 THEN app_code END), CASE WHEN MAX(CASE WHEN row_start = 1 THEN app_id END) = 1880521 THEN 'gfso' ELSE 'zgda' END) AS app_code,
    MAX(CASE WHEN row_start = 1 THEN group_id END) AS group_id,
    MAX(CASE WHEN row_start = 1 THEN channel_id END) AS channel_id,
    MAX(CASE WHEN row_start = 1 THEN afk_turn_cnt END) AS afk_turn_cnt,
    COUNT(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN 1 END) AS settle_count,
    GROUP_CONCAT(CASE WHEN fee = 0 AND result_id IS NULL THEN CAST(depositdiff AS STRING) END ORDER BY game_datetime ASC SEPARATOR '#') AS deposit_diff_path,
    GROUP_CONCAT(CASE WHEN fee = 0 AND result_id IS NULL THEN CAST(magnification AS STRING) END ORDER BY game_datetime ASC SEPARATOR '#') AS deposit_magnification_path
FROM ranked_combat
GROUP BY game_id, uid, resultguid;
```

## 使用示例

### 1. 查询某日对局概况

```sql
SELECT
    dt,
    COUNT(*) AS game_count,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(time_cost), 1) AS avg_time_cost,
    ROUND(AVG(total_magnification), 2) AS avg_magnification,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate,
    SUM(CASE WHEN result_id = 2 THEN 1 ELSE 0 END) AS lose_count,
    ROUND(SUM(CASE WHEN result_id = 2 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS lose_rate
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-05-01' and '2026-06-03'
  AND robot != 1
GROUP BY dt
ORDER BY dt desc 
```

### 2. 用户每日游戏统计

```sql
SELECT
    uid,
    dt,
    COUNT(*) AS game_count,
    SUM(game_outcome_money) AS total_outcome,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ROUND(AVG(total_magnification), 2) AS avg_magnification,
    ROUND(AVG(settle_count), 2) AS avg_settle_count
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-05-01' and '2026-06-03'
  AND robot != 1
GROUP BY uid, dt;
```

### 3. 多轮结算分析

```sql
SELECT
    settle_count,
    COUNT(*) AS game_count,
    ROUND(AVG(time_cost), 1) AS avg_time_cost,
    ROUND(AVG(total_magnification), 2) AS avg_magnification,
    ROUND(AVG(ABS(game_outcome_money)), 2) AS avg_deposit_diff
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-05-01' and '2026-06-03'
  AND robot != 1
GROUP BY settle_count
ORDER BY settle_count;
```

## 字段使用注意

1. **查询必须包含 game_id = 521**：`game_id` 为 DUPLICATE KEY 的首列，所有查询必须包含 `game_id = 521` 条件以确保查询性能
2. **跨天对局**：上游 raw 层已通过 min_dt 覆盖 dt，下游查询无需扩展日期范围，直接 `WHERE dt = T` 即可
3. **多轮结算**：`settle_count` 表示一局内的结算轮数，大于 1 表示多轮结算
4. **路径字段**：
   - `deposit_diff_path`：每轮输赢金额变化路径，用 `#` 分隔
   - `deposit_magnification_path`：每轮倍数变化路径，用 `#` 分隔
5. **货币字段区分**：
   - `game_outcome_money`：游戏内货币变化累计（不含服务费）
   - `game_outcome_gdp`：游戏内货币变化绝对值累计
6. **机器人标记**：分析时建议添加 `robot != 1` 条件过滤真人数据
7. **跨 app_id 共服**：疯狂斗地主有独立 app（`app_id = 1880521`），同时也被内嵌在斗地主 app（`app_id = 1880053`）中。同一个 `resultguid` 下的 3 位玩家可能分别归属不同 `app_id`。因此**不可通过 `COUNT(DISTINCT uid) = 3` 来校验桌级完整性**
8. **`result_id` 兜底逻辑**：部分对局（如中途退出、异常结束）在 `row_end = 1` 的 `result_id` 为 NULL，此时通过 `game_outcome_money`（不含服务费的游戏内货币变化累计）兜底判定：
   - `game_outcome_money > 0` → `result_id = 1`（获胜）
   - `game_outcome_money < 0` → `result_id = 2`（失败）
   - `game_outcome_money = 0` → `result_id = 3`（平局）

## 数据校验 SQL

> 本节提供一组必要的数据校验 SQL，用于在初始化或增量更新后验证数据正确性。建议每次导入后依次执行。

### 1. 主键唯一性校验

> **校验目标**：`(resultguid, uid)` 应为全表唯一。若存在重复，说明增量导入或多轮聚合逻辑异常。

```sql
SELECT resultguid, uid, COUNT(*) AS cnt
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-03-01' and '2026-06-01'
GROUP BY resultguid, uid
HAVING COUNT(*) > 1
LIMIT 10;
```

> **期望结果**：返回 0 行。

### 2. 源数据 vs 目标数据对局数对比

> **校验目标**：源表中当日有服务费（`fee != 0`）的对局数量应与目标表当日对局数量一致，防止跨天对局遗漏或重复计算。

```sql
WITH src AS (
    SELECT COUNT(DISTINCT resultguid) AS src_cnt
    FROM tcy_temp.crazyddz_daily_game_raw
    WHERE game_id = 521
    AND app_id = 1880053
      AND dt between '2026-03-01' and '2026-06-01'
      AND fee != 0
),
tgt AS (
    SELECT COUNT(DISTINCT resultguid) AS tgt_cnt
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521
    AND app_id = 1880053
      AND dt between '2026-03-01' and '2026-06-01'
      AND room_fee != 0 
)
SELECT src.src_cnt, tgt.tgt_cnt, (src.src_cnt - tgt.tgt_cnt) AS diff
FROM src, tgt;
```

> **期望结果**：`diff = 0`（完全一致）。

### 3. 必填字段非空校验

> **校验目标**：`start_datetime`、`end_datetime`、`result_id`、`start_money`、`end_money` 等关键聚合字段应非空。若出现 NULL，说明 `row_start` 或 `row_end` 未能识别开局/结束记录。

```sql
SELECT
    SUM(CASE WHEN start_datetime IS NULL THEN 1 ELSE 0 END) AS null_start_dt,
    SUM(CASE WHEN end_datetime IS NULL THEN 1 ELSE 0 END) AS null_end_dt,
    SUM(CASE WHEN result_id IS NULL THEN 1 ELSE 0 END) AS null_result_id,
    SUM(CASE WHEN start_money IS NULL THEN 1 ELSE 0 END) AS null_start_money,
    SUM(CASE WHEN end_money IS NULL THEN 1 ELSE 0 END) AS null_end_money,
    COUNT(*) AS total_cnt
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-03-01' and '2026-06-01';
```

> **期望结果**：所有 `null_*` 字段应为 0。

### 4. 货币闭环校验

> **校验目标**：`end_money - start_money` 应约等于 `game_outcome_money`（允许因 fee 字段归集差异产生小幅误差）。若误差过大，说明聚合逻辑异常。

```sql
SELECT
    resultguid,
    uid,
    start_money,
    end_money,
    room_fee,
    is_escape,
    game_outcome_money,
    (end_money - start_money) AS money_delta,
    (end_money - start_money - game_outcome_money + is_escape) AS residual
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-03-01' and '2026-06-01'
  AND ABS(end_money - start_money - game_outcome_money + room_fee + is_escape) > 1
LIMIT 10;
```

> **期望结果**：返回 0 行（所有对局货币闭环）。

### 5. 多轮结算路径一致性校验

> **校验目标**：`deposit_diff_path` 和 `deposit_magnification_path` 中用 `#` 分隔的段数应等于 `settle_count`。若不一致，说明 `GROUP_CONCAT` 聚合范围与 `settle_count` 统计范围不一致。

```sql
SELECT
    resultguid,
    uid,
    settle_count,
    (LENGTH(deposit_diff_path) - LENGTH(REPLACE(deposit_diff_path, '#', '')) + 1) AS diff_path_seg,
    (LENGTH(deposit_magnification_path) - LENGTH(REPLACE(deposit_magnification_path, '#', '')) + 1) AS mag_path_seg
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-03-01' and '2026-06-01'
  AND settle_count > 0
  AND (
      (LENGTH(deposit_diff_path) - LENGTH(REPLACE(deposit_diff_path, '#', '')) + 1) <> settle_count
      OR
      (LENGTH(deposit_magnification_path) - LENGTH(REPLACE(deposit_magnification_path, '#', '')) + 1) <> settle_count
  )
LIMIT 10;
```

> **期望结果**：返回 0 行。

### 6. 跨天对局识别

> **校验目标**：识别跨天对局并统计其占比，确保跨天对局被正确收集（`dt` 为开局日期，但 `end_datetime` 晚于次日 00:00）。

```sql
SELECT
    dt,
    COUNT(*) AS total_games,
    SUM(CASE WHEN DATE(end_datetime) > dt THEN 1 ELSE 0 END) AS cross_day_games,
    ROUND(SUM(CASE WHEN DATE(end_datetime) > dt THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS cross_day_pct
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt between '2026-03-01' and '2026-06-01'
GROUP BY dt;
```

> **期望结果**：`cross_day_pct` 通常 <5%。若显著偏高，需检查源表是否存在异常长对局。

---

## 表数据流向

```text
tcy_temp.crazyddz_daily_game_raw      （原始战绩日志，全游戏混合）
            ↓  过滤疯狂斗地主 + 多轮聚合
tcy_temp.dws_crazyddz_daily_game     （疯狂斗地主对局战绩表）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg        （APP 端注册用户宽表）
```

> **文档版本**：v1.0
> **创建时间**：2026-06-11
> **更新说明**：
>
> - v1.0：初始版本
