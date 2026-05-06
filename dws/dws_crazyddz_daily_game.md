# DWS 中间表：疯狂斗地主每日对局战绩表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_crazyddz_daily_game` |
| 全名 | `tcy_temp.dws_crazyddz_daily_game` |
| 类型 | DWS 层中间表（每日增量） |
| 描述 | 疯狂斗地主对局战绩表，从原始战绩表中提取并聚合疯狂斗地主玩法数据 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |

## 设计背景

疯狂斗地主（game_id = 521）的对局日志存储在原始 `dwd_game_combat_si` 战绩表中，但其数据结构与传统斗地主不同：

1. **多轮结算**：疯狂斗地主一局游戏内可能包含多轮结算，每轮结算产生一条记录
2. **跨天对局**：部分对局可能跨越两天，如 27 号开局、28 号结束，需要跨天查询获取完整数据
3. **累计统计**：需要通过 `row_start` 和 `row_end` 识别开局和结束记录，聚合计算整局数据
4. **路径追踪**：记录每轮的输赢金额和倍数变化路径

**解决方案**：

1. 先通过 `target_resultguids` 找出指定日期有服务费记录（`fee != 0`）的对局 GUID
2. 通过 `INNER JOIN` 关联这些 resultguid，扩大日期范围获取跨天对局的完整数据
3. 使用 `ROW_NUMBER()` 窗口函数识别开局（row_start = 1）和结束（row_end = 1）记录
4. 聚合计算整局的累计统计数据
5. 使用 `GROUP_CONCAT` 记录输赢金额和倍数的变化路径

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| resultguid | varchar(64) | 本局战绩 ID | "abc123xyz" |
| uid | bigint | 玩家 ID | 123456789 |
| app_id | int | 应用 ID | 1880053 |
| game_id | int | 游戏 ID | 521 |
| game_date | date | 对局日期 | 2026-04-27 |
| room_id | int | 房间号 | 1001 |
| room_base | int | 房间底分 | 100 |
| room_fee | int | 房间服务费 | 10 |
| chairno | int | 座位号 | 0 |
| robot | tinyint | 机器人标记：1=机器人，其他=真人 | 0 |
| start_datetime | datetime | 开局时间 | 2026-04-27 10:30:00 |
| start_money | bigint | 开局时货币数量 | 5000 |
| end_datetime | datetime | 结束时间 | 2026-04-27 10:35:00 |
| end_money | bigint | 结束时货币数量 | 5500 |
| final_result_id | tinyint | 最终结果：1=获胜，2=失败 | 1 |
| is_escape | int | 逃跑标记（<0 代表存在逃跑行为） | 0 |
| settle_count | int | 结算轮数 | 3 |
| total_magnification | bigint | 累计倍数 | 36 |
| game_deposit_gdp | bigint | 游戏内货币变化绝对值累计 | 1500 |
| game_deposit_diff | bigint | 游戏内货币变化累计（不含服务费） | 500 |
| total_deposit_diff | bigint | 总货币变化（含服务费） | 600 |
| total_time_cost | int | 总耗时（秒） | 180 |
| deposit_diff_path | varchar(65533) | 每轮输赢金额路径（用 | 分隔） | "-100|200|-50|450" |
| deposit_magnification_path | varchar(65533) | 每轮倍数路径（用 | 分隔） | "3|6|2|9" |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_crazyddz_daily_game (
  `resultguid` varchar(64) NOT NULL COMMENT "对局GUID",
  `uid` bigint(20) NOT NULL COMMENT "用户ID",
  `app_id` int(11) NULL COMMENT "应用ID",
  `game_id` int(11) NULL COMMENT "游戏ID",
  `game_date` date NULL COMMENT "对局日期",
  `room_id` int(11) NULL COMMENT "房间ID",
  `room_base` int(11) NULL COMMENT "房间底分",
  `room_fee` int(11) NULL COMMENT "房间服务费",
  `chairno` int(11) NULL COMMENT "座位号",
  `robot` tinyint(4) NULL COMMENT "是否机器人",
  `start_datetime` datetime NULL COMMENT "开局时间",
  `start_money` bigint(20) NULL COMMENT "开局时货币数量",
  `end_datetime` datetime NULL COMMENT "结束时间",
  `end_money` bigint(20) NULL COMMENT "结束时货币数量",
  `final_result_id` tinyint(4) NULL COMMENT "最终结果",
  `is_escape` int(11) NULL COMMENT "逃跑标记",
  `settle_count` int(11) NULL COMMENT "结算轮数",
  `total_magnification` bigint(20) NULL COMMENT "累计倍数",
  `game_deposit_gdp` bigint(20) NULL COMMENT "游戏内货币变化绝对值累计",
  `game_deposit_diff` bigint(20) NULL COMMENT "游戏内货币变化累计",
  `total_deposit_diff` bigint(20) NULL COMMENT "总货币变化",
  `total_time_cost` int(11) NULL COMMENT "总耗时",
  `deposit_diff_path` varchar(65533) NULL COMMENT "每轮输赢金额路径",
  `deposit_magnification_path` varchar(65533) NULL COMMENT "每轮倍数路径"
) ENGINE=OLAP
DUPLICATE KEY(`resultguid`, `uid`)
COMMENT "疯狂斗地主每日对局战绩表"
PARTITION BY RANGE(`game_date`) (
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
    "dynamic_partition.prefix" = "p"
);
```

### 初始化 SQL

```sql
INSERT INTO tcy_temp.dws_crazyddz_daily_game
WITH target_resultguids AS (
    SELECT DISTINCT resultguid 
    FROM tcy_dwd.dwd_game_combat_si
    WHERE date = 20260427
      AND app_id = 1880053 
      AND game_id = 521
      AND fee != 0 
),
ranked_combat AS (
    SELECT 
        dgcs.*,
        ROW_NUMBER() OVER(PARTITION BY dgcs.resultguid, uid ORDER BY dgcs.result_id, time_unix) as row_start,
        ROW_NUMBER() OVER(PARTITION BY dgcs.resultguid, uid ORDER BY dgcs.result_id desc, time_unix desc) as row_end
    FROM tcy_dwd.dwd_game_combat_si dgcs
    INNER JOIN target_resultguids tr ON dgcs.resultguid = tr.resultguid 
    WHERE dgcs.date BETWEEN 20260427 AND 20260428
      AND dgcs.app_id = 1880053 
      AND dgcs.game_id = 521
)
SELECT
    resultguid,
    uid,
    MAX(CASE WHEN row_start = 1 THEN app_id END) AS app_id,
    MAX(CASE WHEN row_start = 1 THEN game_id END) AS game_id,
    MAX(CASE WHEN row_start = 1 THEN DATE(dt) END) AS game_date,
    MAX(CASE WHEN row_start = 1 THEN room_id END) AS room_id,
    MAX(CASE WHEN row_start = 1 THEN basedeposit END) AS room_base,
    MAX(CASE WHEN row_start = 1 THEN fee END) AS room_fee,
    MAX(CASE WHEN row_start = 1 THEN chairno END) AS chairno,
    MAX(CASE WHEN row_start = 1 THEN robot END) AS robot,
    MAX(CASE WHEN row_start = 1 THEN FROM_UNIXTIME(time_unix / 1000) END) AS start_datetime,
    MAX(CASE WHEN row_start = 1 THEN olddeposit END) AS start_money,
    MAX(CASE WHEN row_end = 1 THEN FROM_UNIXTIME(time_unix / 1000) END) AS end_datetime,
    MAX(CASE WHEN row_end = 1 THEN end_deposit END) AS end_money,
    MAX(CASE WHEN row_end = 1 THEN result_id END) AS final_result_id,
    MAX(CASE WHEN row_end = 1 THEN cut END) AS is_escape,
    COUNT(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN 1 END) AS settle_count,
    SUM(ABS(magnification)) AS total_magnification,
    SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN abs(depositdiff) END) AS game_deposit_gdp,
    SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN depositdiff END) AS game_deposit_diff,
    SUM(depositdiff) AS total_deposit_diff,
    SUM(timecost) AS total_time_cost,
    GROUP_CONCAT(CAST(depositdiff AS STRING) ORDER BY result_id, time_unix ASC SEPARATOR '|') AS deposit_diff_path,
    GROUP_CONCAT(CAST(magnification AS STRING) ORDER BY result_id, time_unix ASC SEPARATOR '|') AS deposit_magnification_path
FROM ranked_combat
GROUP BY resultguid, uid;
```

### 增量更新 SQL

```sql
-- 参数：将 ${DATE} 替换为实际日期（int 格式，如 20260428）
-- 注意：查询范围包含 ${DATE} 和 ${DATE+1}，以获取跨天对局的完整数据
INSERT INTO tcy_temp.dws_crazyddz_daily_game
WITH target_resultguids AS (
    SELECT DISTINCT resultguid 
    FROM tcy_dwd.dwd_game_combat_si
    WHERE date = ${DATE}
      AND app_id = 1880053 
      AND game_id = 521
      AND fee != 0 
),
ranked_combat AS (
    SELECT 
        dgcs.*,
        ROW_NUMBER() OVER(PARTITION BY dgcs.resultguid, uid ORDER BY dgcs.result_id, time_unix) as row_start,
        ROW_NUMBER() OVER(PARTITION BY dgcs.resultguid, uid ORDER BY dgcs.result_id desc, time_unix desc) as row_end
    FROM tcy_dwd.dwd_game_combat_si dgcs
    INNER JOIN target_resultguids tr ON dgcs.resultguid = tr.resultguid 
    WHERE dgcs.date BETWEEN ${DATE} AND ${DATE} + 1
      AND dgcs.app_id = 1880053 
      AND dgcs.game_id = 521
)
SELECT
    resultguid,
    uid,
    MAX(CASE WHEN row_start = 1 THEN app_id END) AS app_id,
    MAX(CASE WHEN row_start = 1 THEN game_id END) AS game_id,
    MAX(CASE WHEN row_start = 1 THEN DATE(dt) END) AS game_date,
    MAX(CASE WHEN row_start = 1 THEN room_id END) AS room_id,
    MAX(CASE WHEN row_start = 1 THEN basedeposit END) AS room_base,
    MAX(CASE WHEN row_start = 1 THEN fee END) AS room_fee,
    MAX(CASE WHEN row_start = 1 THEN chairno END) AS chairno,
    MAX(CASE WHEN row_start = 1 THEN robot END) AS robot,
    MAX(CASE WHEN row_start = 1 THEN FROM_UNIXTIME(time_unix / 1000) END) AS start_datetime,
    MAX(CASE WHEN row_start = 1 THEN olddeposit END) AS start_money,
    MAX(CASE WHEN row_end = 1 THEN FROM_UNIXTIME(time_unix / 1000) END) AS end_datetime,
    MAX(CASE WHEN row_end = 1 THEN end_deposit END) AS end_money,
    MAX(CASE WHEN row_end = 1 THEN result_id END) AS final_result_id,
    MAX(CASE WHEN row_end = 1 THEN cut END) AS is_escape,
    COUNT(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN 1 END) AS settle_count,
    SUM(ABS(magnification)) AS total_magnification,
    SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN abs(depositdiff) END) AS game_deposit_gdp,
    SUM(CASE WHEN fee = 0 AND cut = 0 AND result_id IS NULL THEN depositdiff END) AS game_deposit_diff,
    SUM(depositdiff) AS total_deposit_diff,
    SUM(timecost) AS total_time_cost,
    GROUP_CONCAT(CAST(depositdiff AS STRING) ORDER BY result_id, time_unix ASC SEPARATOR '|') AS deposit_diff_path,
    GROUP_CONCAT(CAST(magnification AS STRING) ORDER BY result_id, time_unix ASC SEPARATOR '|') AS deposit_magnification_path
FROM ranked_combat
GROUP BY resultguid, uid;
```

## 使用示例

### 1. 查询某日对局概况

```sql
SELECT
    game_date,
    COUNT(*) AS game_count,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(total_time_cost), 1) AS avg_time_cost,
    ROUND(AVG(total_magnification), 2) AS avg_magnification,
    SUM(CASE WHEN final_result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    ROUND(SUM(CASE WHEN final_result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_date = '2026-04-27'
  AND robot != 1
GROUP BY game_date;
```

### 2. 用户每日游戏统计

```sql
SELECT
    uid,
    game_date,
    COUNT(*) AS game_count,
    SUM(total_deposit_diff) AS total_diff,
    SUM(CASE WHEN final_result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    ROUND(SUM(CASE WHEN final_result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ROUND(AVG(total_magnification), 2) AS avg_magnification,
    ROUND(AVG(settle_count), 2) AS avg_settle_count
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_date BETWEEN '2026-04-20' AND '2026-04-27'
  AND robot != 1
GROUP BY uid, game_date;
```

### 3. 多轮结算分析

```sql
SELECT
    settle_count,
    COUNT(*) AS game_count,
    ROUND(AVG(total_time_cost), 1) AS avg_time_cost,
    ROUND(AVG(total_magnification), 2) AS avg_magnification,
    ROUND(AVG(ABS(total_deposit_diff)), 2) AS avg_deposit_diff
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_date = '2026-04-27'
  AND robot != 1
GROUP BY settle_count
ORDER BY settle_count;
```

## 字段使用注意

1. **跨天对局**：
   - 部分对局可能跨天（如 27 号开局、28 号结束）
   - `game_date` 取开局日期（row_start = 1 时的 dt）
   - 增量更新时查询范围包含 `${DATE}` 和 `${DATE} + 1`
2. **多轮结算**：`settle_count` 表示一局内的结算轮数，大于 1 表示多轮结算
3. **路径字段**：
   - `deposit_diff_path`：每轮输赢金额变化路径，用 `|` 分隔
   - `deposit_magnification_path`：每轮倍数变化路径，用 `|` 分隔
4. **货币字段区分**：
   - `game_deposit_diff`：游戏内货币变化累计（不含服务费）
   - `total_deposit_diff`：总货币变化（含服务费）
5. **机器人标记**：分析时建议添加 `robot != 1` 条件过滤真人数据

## 表数据流向

```text
tcy_dwd.dwd_game_combat_si           （原始战绩日志，全游戏混合）
            ↓  过滤疯狂斗地主 + 多轮聚合
tcy_temp.dws_crazyddz_daily_game     （疯狂斗地主对局战绩表）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg        （APP 端注册用户宽表）
```

> **文档版本**：v1.0
> **创建时间**：2026-04-30