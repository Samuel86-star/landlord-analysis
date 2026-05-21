# DWS 中间表：游戏道具流水日志表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_game_prop_log` |
| 全名 | `tcy_temp.dws_game_prop_log` |
| 类型 | DWS 层明细表（每日增量） |
| 描述 | 游戏道具流水日志表，记录玩家道具的发放、消耗、过期等所有变动事件 |
| 粒度 | uid × 一条道具变动记录（不做聚合） |

## 设计背景

道具流水分散在多个业务子系统的日志中，且原始日志通常按操作类型拆分。每次分析道具消耗、发放、留存影响时，需要跨表关联补充用户维度与渠道维度，查询复杂度较高。

**解决方案**：将所有道具变动事件汇总到一张统一的流水日志表，补充应用、渠道、平台分组等通用维度，作为道具相关分析的基础宽表。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| log_date | date | 分区字段：日志日期 | 2026-05-13 |
| app_id | int | 应用 ID | 1880053 |
| uid | int | 玩家唯一标识 ID | 123456789 |
| game_id | int | 游戏 ID | 53 |
| game_code | varchar(32) | 游戏 code | zgda |
| game_vers | varchar(32) | 游戏版本 | 1.2.3 |
| app_code | varchar(32) | 应用 code | zgda |
| app_vers | varchar(32) | 应用版本 | 5.1.0 |
| log_datetime | datetime | 日志具体时间 | 2026-05-13 10:30:00 |
| prop_id | int | 道具 ID | 10001 |
| prop_name | varchar(32) | 道具名称 | 记牌器 |
| prop_cnt | int | 变动数量（正=获得，负=消耗） | -1 |
| op_type | tinyint | 操作类型 | 1 |
| remain | int | 道具变动后剩余数量 | 5 |
| deadline_datetime | datetime | 道具过期时间 | 2026-06-13 23:59:59 |
| mod_name | varchar(32) | 模块名称 | lottery |
| mod_detail | varchar(256) | 变动详情/扩展字段（JSON 或描述文本） | 对局使用记牌器 |
| source | varchar(64) | 来源（活动/系统/对局等标识） | activity_20260513 |
| ip | varchar(64) | IP 地址 | 192.168.1.1 |
| group_id | int | 平台分组 ID（区分 PC/APP/小游戏） | 6 |
| channel_id | int | 渠道号 | 1001 |

## 分端规则

通过 `group_id` 判定分端类型：

| platform | group_id 范围 |
| -------- | ------------- |
| Android | 6, 66, 33, 44, 77, 99 |
| iOS | 8, 88 |
| 小游戏 | 56 |
| PC | 不在以上范围，且不在 55, 69, 0, 68 中 |

详细分端说明见 [README-data.md](../README-data.md)。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_game_prop_log (
  `log_date` date NOT NULL COMMENT "分区字段：日志日期",
  `app_id` int(11) NOT NULL COMMENT "应用 ID",
  `uid` int(11) NOT NULL COMMENT "玩家唯一标识 ID",
  `game_id` int(11) NULL COMMENT "游戏 ID",
  `game_code` varchar(32) NULL COMMENT "游戏 code",
  `game_vers` varchar(32) NULL COMMENT "游戏版本",
  `app_code` varchar(32) NULL COMMENT "应用 code",
  `app_vers` varchar(32) NULL COMMENT "应用版本",
  `log_datetime` datetime NOT NULL COMMENT "日志具体时间",
  `prop_id` int(11) NULL COMMENT "道具 ID",
  `prop_name` varchar(32) NULL COMMENT "道具名称",
  `prop_cnt` int(11) NULL COMMENT "变动数量",
  `op_type` tinyint(4) NULL COMMENT "操作类型",
  `remain` int(11) NULL COMMENT "道具剩余数量",
  `deadline_datetime` datetime NULL COMMENT "道具过期时间",
  `mod_name` varchar(32) NULL COMMENT "模块名称",
  `mod_detail` varchar(256) NULL COMMENT "变动详情/扩展字段",
  `source` varchar(64) NULL COMMENT "来源",
  `ip` varchar(64) NULL COMMENT "IP 地址",
  `group_id` int(11) NOT NULL COMMENT "平台分组 ID",
  `channel_id` int(11) NOT NULL COMMENT "渠道号"
) ENGINE=OLAP
DUPLICATE KEY(`log_date`, `app_id`, `uid`)
COMMENT "游戏道具流水日志表"
PARTITION BY RANGE(`log_date`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "storage_format" = "V2",
    "enable_persistent_index" = "true",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-100",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "100",
    "colocate_with" = "group_daily_data"
);
```

### 初始化 SQL

```sql
-- 游戏道具流水日志初始化
-- 参数：将 ${DATE} 替换为实际日期（int 格式，如 20260513）
INSERT INTO tcy_temp.dws_game_prop_log
SELECT
    DATE(dt) AS log_date,
    app_id,
    uid,
    game_id,
    game_code,
    game_vers,
    app_code,
    app_vers,
    FROM_UNIXTIME(time_unix / 1000) AS log_datetime,
    prop_id,
    prop_name,
    prop_cnt,
    op_type,
    remain,
    FROM_UNIXTIME(deadline_ts) AS deadline_datetime,
    CASE 
        WHEN regexp_extract(regexp_replace(mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1) REGEXP '\\((platform|buy)\\)'
            THEN regexp_replace(regexp_extract(regexp_replace(mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1), '\\((platform|buy)\\)', '')
        WHEN regexp_extract(regexp_replace(mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1) REGEXP '\\(.+\\)'
            THEN regexp_extract(regexp_extract(regexp_replace(mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1), '\\(([^()]+)\\)', 1)
        ELSE regexp_extract(regexp_replace(mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1)
    END as mod_name,
    mod_detail,
    source,
    ip,
    group_id,
    channel_id
FROM hive_catalog_cdh5.dwd.fact_gtpl_prop_detail
WHERE dt = 20260513
  AND app_id = 1880053
  AND prop_id NOT IN (21770,-1);
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

## 注意事项

1. `prop_cnt` 采用正负数语义：正数代表获得，负数代表消耗，统计净变化使用 `SUM(prop_cnt)`
2. `remain` 为道具变动后剩余数量，可用于校验流水连续性（前一条 `remain + 当前 prop_cnt = 当前 remain`）
3. `deadline_datetime` 为道具过期时间，过期失效操作通常作为一条单独流水记录
4. `mod_detail` 字段用于扩展业务上下文（如对局 ID、活动 ID 等），具体格式视来源系统而定
5. 分析时建议结合 `op_type` 区分发放/消耗/过期等不同事件类型

## 表数据流向

```text
hive_catalog_cdh5.dwd.fact_gtpl_prop_detail   （原始道具流水日志，全游戏混合）
            ↓  抽取 + 补充维度
tcy_temp.dws_game_prop_log                    （游戏道具流水日志）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg                 （APP 端注册用户宽表）
```

> **文档版本**：v1.0
> **创建时间**：2026-05-14
