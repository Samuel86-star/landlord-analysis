# ODS 迁移表：每日对局战绩原始字段表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `crazyddz_daily_game_raw` |
| 全名 | `tcy_temp.crazyddz_daily_game_raw` |
| 类型 | ODS 层迁移表（每日增量） |
| 描述 | 将 Hive 原始对局日志迁移至 StarRocks，保持原始字段不变，不做字段统一转换 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |
| 上游表 | `hive_catalog_cdh5.dwd.fact_game_combatgains` |
| 下游表 | `tcy_temp.dws_crazyddz_daily_game` |

## 设计背景

本表的核心目的是将 Hive 中的原始对局日志 `hive_catalog_cdh5.dwd.fact_game_combatgains` 迁移到 StarRocks 数仓（`tcy_temp` 库），以利用 StarRocks 的 OLAP 查询性能支撑后续分析工作。

原始表存储了所有玩法的对局日志，但不同玩法使用不同的货币字段：

| 玩法 | 货币类型 | 底分字段 | 服务费字段 | 对局前货币 | 对局后货币 | 货币变动值(含服务费) |
| ---- | ------- | ------- | --------- | --------- | --------- | ------- |
| 经典/不洗牌/癞子 | 银子 | `basedeposit` | `fee` | `olddeposit` | `end_deposit` | `depositdiff` |
| 积分/好友房/比赛 | 积分 | `basescore` | `score_fee` | `oldscore` | `end_score` | `scorediff` |

迁移过程中保持原始字段不变，未做统一转换（统一转换由下游 `dws_crazyddz_daily_game` 表完成）。表中同时保留了 JSON 字段 `magnification_subdivision`（个人操作倍数）和 `extend_content`（手牌等扩展信息），供下游按需解析提取。

### app_id 与 game_id 业务关系

| 游戏 | app_id | game_id | 备注 |
| ---- | ---- | ---- | ---- |
| 疯狂斗地主 | 1880521 | 521 | 疯狂斗地主独立 app |
| 疯狂斗地主 | 1880053 | 521 | 斗地主 app 内嵌的疯狂斗地主玩法 |

> **注意**：疯狂斗地主有独立 app（app_id = 1880521），同时也被内嵌在斗地主 app（app_id = 1880053）中。因此 game_id = 521 的战绩中，同一局的玩家 app_id 可能不同（1880053 或 1880521）。查询时需根据分析目的决定是否合并两个 app_id 的数据。

### 已知数据质量问题：fee 重复上报

同一 `resultguid + uid` 下存在多条 `fee > 0 AND result_id IS NOT NULL` 的记录，正常情况应仅有一条。这是研发端重复上报导致的，下游聚合时需注意此情况。

排查 SQL：

```sql
SELECT resultguid, uid, COUNT(*) AS cnt
FROM tcy_temp.crazyddz_daily_game_raw
WHERE game_id = 521
  AND fee != 0
  AND result_id IS NOT NULL
GROUP BY resultguid, uid
HAVING COUNT(*) > 1
LIMIT 20;
```

> **校验状态**：已与下游 `dws_crazyddz_daily_game` 对盘确认，数据整体无误。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_id | int | 游戏 ID | 53 |
| dt | date | 对局日期 | 2026-04-08 |
| uid | int | 玩家 ID | 123456789 |
| game_datetime | datetime | 对局时间 | 2026-04-08 10:30:00 |
| resultguid | varchar(64) | 本局战绩 ID | "abc123xyz" |
| timecost | int | 对局耗时（秒） | 180 |
| room_id | int | 房间号 | 1001 |
| room_currency_lower | bigint | 进入房间所需最少携带货币 | 1000 |
| room_currency_upper | bigint | 进入房间最大携带货币 | 10000 |
| robot | tinyint | 机器人标记：1=机器人，其他=真人 | 0 |
| role | tinyint | 角色：1=地主，2=农民 | 1 |
| chairno | tinyint | 座位号（0/1/2） | 0 |
| result_id | tinyint | 结果：1=获胜，2=失败 | 1 |
| basedeposit | int | 银子玩法房间底分(银子) | 100 |
| olddeposit | bigint | 银子玩法对局前银子数量 | 5500 |
| end_deposit | bigint | 银子玩法对局后银子数量 | 500 |
| fee | int | 银子玩法对局服务费(银子) | 500 |
| depositdiff | bigint | 银子玩法银子变动数量(含服务费) | 5000 |
| cut | int | 逃跑罚没货币（<0 代表存在逃跑行为） | 0 |
| safebox_deposit | int | 保险箱存银 | 1000 |
| magnification | int | 个人理论总倍数 | 12 |
| magnification_stacked | int | 个人加倍：1=不加倍，2=加倍，4=超级加倍 | 2 |
| channel_id | int | 渠道号 | 1001 |
| group_id | int | 分端 ID | 6 |
| app_id | int | 应用 ID | 1880053 |
| app_code | varchar(32) | 应用code | zgda |
| afk_turn_cnt | int | 托管出牌次数 | 0 |

## 构建 SQL

```sql
CREATE TABLE tcy_temp.crazyddz_daily_game_raw (
  `game_id` int(11) NULL COMMENT "游戏ID",
  `dt` DATE NOT NULL COMMENT "对局日期",
  `uid` int(11) NOT NULL COMMENT "玩家ID",
  `game_datetime` datetime NOT NULL COMMENT "对局时间",
  `resultguid` varchar(64) NULL COMMENT "本局战绩ID",
  `timecost` int(11) NULL COMMENT "对局耗时（秒）",
  `room_id` int(11) NULL COMMENT "房间号",
  `room_currency_lower` bigint(20) NULL COMMENT "进入房间所需最少携带货币",
  `room_currency_upper` bigint(20) NULL COMMENT "进入房间最大携带货币",
  `robot` tinyint(4) NULL COMMENT "机器人标记：1=机器人，其他=真人",
  `role` tinyint(4) NULL COMMENT "角色：1=地主，2=农民",
  `chairno` tinyint(4) NULL COMMENT "座位号（0/1/2）",
  `result_id` tinyint(4) NULL COMMENT "结果：1=获胜，2=失败",
  `basedeposit` int(11) NULL COMMENT "银子玩法房间底分(银子)",
  `olddeposit` bigint(20) NULL COMMENT "银子玩法对局前银子数量",
  `end_deposit` bigint(20) NULL COMMENT "银子玩法对局后银子数量",
  `fee` int(11) NULL COMMENT "银子玩法对局服务费(银子)",
  `depositdiff` bigint(20) NULL COMMENT "银子玩法银子变动数量(含服务费)",
  `cut` int(11) NULL COMMENT "逃跑罚没货币（<0代表存在逃跑行为）",
  `safebox_deposit` int(11) NULL COMMENT "保险箱存银",
  `magnification` int(11) NULL COMMENT "个人理论总倍数",
  `magnification_stacked` int(11) NULL COMMENT "个人加倍：1=不加倍，2=加倍，4=超级加倍",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `group_id` int(11) NULL COMMENT "分端ID",
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `app_code` varchar(32) NULL COMMENT "应用code",
  `afk_turn_cnt` int(11) NULL COMMENT "托管出牌次数"
) ENGINE=OLAP
DUPLICATE KEY(`game_id`, `dt`, `uid`)
COMMENT "疯狂斗地主每日游戏明细表"
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

## 更新SQL

```sql
INSERT INTO tcy_temp.crazyddz_daily_game_raw
WITH base_data AS (
    SELECT 
        game_id, dt, uid, time_unix, resultguid, timecost,
        room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
        basedeposit, olddeposit, end_deposit, fee, depositdiff,
        cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, app_id, app_code, afk_turn_cnt,
        MAX(CASE WHEN app_id = 1880053 THEN 1 ELSE 0 END) OVER (PARTITION BY resultguid) AS has_target_app
    FROM hive_catalog_cdh5.dwd.fact_game_combatgains
    WHERE dt between 20260525 and 20260531
      AND game_id = 521
)
SELECT 
    game_id, dt, uid, FROM_UNIXTIME(time_unix / 1000) AS game_datetime, resultguid, timecost,
    room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
    basedeposit, olddeposit, end_deposit, fee, depositdiff,
    cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, IFNULL(app_id, 1880521) AS app_id, app_code, afk_turn_cnt
FROM base_data
WHERE has_target_app = 1
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

## 表数据流向

```text
hive_catalog_cdh5.dwd.fact_game_combatgains        （Hive 原始对局日志，多货币字段）
            ↓  迁移至 StarRocks
tcy_temp.crazyddz_daily_game_raw       （StarRocks 原始对局表，保持原始字段）
```

> **文档版本**：v1.0
> **创建时间**：2026-06-02
> **更新说明**：
>
> - v1.0：初始版本，从hive抽取到starrocks中，保留原始货币字段
  