# DWS 中间表：游戏道具流水日志表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_prop_log` |
| 全名 | `tcy_temp.dws_prop_log` |
| 类型 | DWS 层明细表（每日增量） |
| 描述 | 游戏道具流水日志表，从原始 `fact_gtpl_prop_detail` 中筛选斗地主游戏相关数据 |
| 粒度 | uid × 一条道具变动记录（不做聚合） |

## 设计背景

道具流水分散在多个业务子系统的日志中，且原始日志通常按操作类型拆分。每次分析道具消耗、发放、留存影响时，需要跨表关联补充用户维度与渠道维度，查询复杂度较高。

**解决方案**：预筛选斗地主相关数据，将所有道具变动事件汇总到一张统一的流水日志表，作为道具相关分析的基础宽表。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| dt | date | 日期 | 2026-05-13 |
| game_id | int | 游戏 ID | 53 |
| uid | int | 玩家唯一标识 | 123456789 |
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
CREATE TABLE tcy_temp.dws_prop_log (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `dt` date NOT NULL COMMENT "日期",
  `game_id` int(11) NOT NULL COMMENT "游戏ID",
  `uid` int(11) NOT NULL COMMENT "玩家ID",
  `game_code` varchar(32) NULL COMMENT "游戏code",
  `game_vers` varchar(32) NULL COMMENT "游戏版本",
  `app_code` varchar(32) NULL COMMENT "应用code",
  `app_vers` varchar(32) NULL COMMENT "应用版本",
  `log_datetime` datetime NULL COMMENT "日志具体时间",
  `prop_id` int(11) NULL COMMENT "道具ID",
  `prop_name` varchar(32) NULL COMMENT "道具名称",
  `prop_cnt` int(11) NULL COMMENT "变动数量，正=获得，负=消耗",
  `op_type` tinyint(4) NULL COMMENT "操作类型",
  `remain` int(11) NULL COMMENT "道具变动后剩余数量",
  `deadline_datetime` datetime NULL COMMENT "道具过期时间",
  `mod_name` varchar(32) NULL COMMENT "模块名称",
  `mod_detail` varchar(256) NULL COMMENT "变动详情/扩展字段",
  `source` varchar(64) NULL COMMENT "来源",
  `ip` varchar(64) NULL COMMENT "IP地址",
  `group_id` int(11) NULL COMMENT "平台分组ID",
  `channel_id` int(11) NULL COMMENT "渠道号"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `dt`, `game_id`, `uid`)
COMMENT "游戏道具流水日志表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-365",
    "dynamic_partition.history_partition_num" = "365",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);
```

### 数据导入 SQL

```sql
-- 游戏道具流水日志导入
-- 参数：${START_DATE} / ${END_DATE} 替换为实际日期（int 格式，如 20260513）
--       全量初始化时使用日期范围，增量更新时 START_DATE = END_DATE
INSERT INTO tcy_temp.dws_prop_log
SELECT
    p.app_id,
    STR_TO_DATE(CAST(p.dt AS VARCHAR), '%Y%m%d') AS dt,
    p.game_id,
    p.uid,
    p.game_code,
    p.game_vers,
    p.app_code,
    p.app_vers,
    FROM_UNIXTIME(p.time_unix / 1000) AS log_datetime,
    p.prop_id,
    p.prop_name,
    p.prop_cnt,
    p.op_type,
    p.remain,
    FROM_UNIXTIME(p.deadline_ts) AS deadline_datetime,
    CASE 
        WHEN regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1) REGEXP '\\((platform|buy)\\)'
            THEN regexp_replace(regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1), '\\((platform|buy)\\)', '')
        WHEN regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1) REGEXP '\\(.+\\)'
            THEN regexp_extract(regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1), '\\(([^()]+)\\)', 1)
        ELSE regexp_extract(regexp_replace(p.mod_detail, '^(client-notify-|pick\\(payresult\\)-|game-notify-)', ''), '^([^-]+)', 1)
    END AS mod_name,
    p.mod_detail,
    p.source,
    p.ip,
    p.group_id,
    p.channel_id
FROM hive_catalog_cdh5.dwd.fact_gtpl_prop_detail p
WHERE p.app_id = 1880053
  AND p.game_id = 53
  AND p.dt BETWEEN ${START_DATE} AND ${END_DATE};
```

## 数据校验 SQL

> 本节提供一组数据校验 SQL，用于在初始化或增量更新后验证数据正确性。建议每次导入后依次执行。

### 1. 源数据 vs 目标数据行数对比

> **校验目标**：源表 `fact_gtpl_prop_detail` 中满足 `app_id = 1880053 AND game_id = 53` 的行数应与目标表当日行数一致，防止数据遗漏或重复。

```sql
WITH src AS (
    SELECT COUNT(*) AS src_cnt
    FROM hive_catalog_cdh5.dwd.fact_gtpl_prop_detail
    WHERE app_id = 1880053
      AND game_id = 53
      AND dt = 20260513
),
tgt AS (
    SELECT COUNT(*) AS tgt_cnt
    FROM tcy_temp.dws_prop_log
    WHERE app_id = 1880053
      AND dt = '2026-05-13'
)
SELECT src.src_cnt, tgt.tgt_cnt, (src.src_cnt - tgt.tgt_cnt) AS diff
FROM src, tgt;
```

> **期望结果**：`diff = 0`（完全一致）。

### 2. 必填字段非空校验

> **校验目标**：`app_id`、`dt`、`uid`、`game_id`、`log_datetime` 等关键字段应非空。若出现 NULL，说明源表数据异常或 INSERT 逻辑有问题。

```sql
SELECT
    SUM(CASE WHEN app_id IS NULL THEN 1 ELSE 0 END) AS null_app_id,
    SUM(CASE WHEN dt IS NULL THEN 1 ELSE 0 END) AS null_dt,
    SUM(CASE WHEN uid IS NULL THEN 1 ELSE 0 END) AS null_uid,
    SUM(CASE WHEN game_id IS NULL THEN 1 ELSE 0 END) AS null_game_id,
    SUM(CASE WHEN log_datetime IS NULL THEN 1 ELSE 0 END) AS null_log_datetime,
    COUNT(*) AS total_cnt
FROM tcy_temp.dws_prop_log
WHERE app_id = 1880053
  AND dt = '2026-05-13';
```

> **期望结果**：所有 `null_*` 字段应为 0。

### 3. 道具数量连续性校验

> **校验目标**：同一用户同一道具，前后两条流水的 `remain` 应连续（`上一条 remain + 当前 prop_cnt = 当前 remain`）。按 uid、prop_id、log_datetime 排序后检查是否有断链。

```sql
WITH ordered AS (
    SELECT
        uid,
        prop_id,
        log_datetime,
        prop_cnt,
        remain,
        LAG(remain, 1, 0) OVER (PARTITION BY uid, prop_id ORDER BY log_datetime) AS prev_remain
    FROM tcy_temp.dws_prop_log
    WHERE app_id = 1880053
      AND dt = '2026-05-13'
)
SELECT
    uid,
    prop_id,
    log_datetime,
    prev_remain,
    prop_cnt,
    remain,
    (prev_remain + prop_cnt - remain) AS residual
FROM ordered
WHERE ABS(prev_remain + prop_cnt - remain) > 0
  AND prev_remain != 0
LIMIT 10;
```

> **期望结果**：返回 0 行。若有少量不闭环记录，可能是跨天流水导致，扩大时间范围后复核。

### 4. 游戏/应用过滤正确性校验

> **校验目标**：目标表应只包含 `app_id = 1880053` 且 `game_id = 53` 的记录。若出现其他值，说明过滤条件失效。

```sql
SELECT
    app_id,
    game_id,
    COUNT(*) AS cnt
FROM tcy_temp.dws_prop_log
WHERE app_id = 1880053
  AND dt = '2026-05-13'
GROUP BY app_id, game_id
HAVING app_id != 1880053 OR game_id != 53;
```

> **期望结果**：返回 0 行。

---

## 注意事项

1. **查询必须加 `WHERE app_id = 1880053`**：使用本表时 SQL 一律需要带上该条件，否则可能导致全表扫描或结果异常
2. 仅包含斗地主游戏数据（`app_id = 1880053`，`game_id = 53`）
3. `prop_cnt` 采用正负数语义：正数代表获得，负数代表消耗，统计净变化使用 `SUM(prop_cnt)`
4. `remain` 为道具变动后剩余数量，可用于校验流水连续性（前一条 `remain + 当前 prop_cnt = 当前 remain`）
5. `deadline_datetime` 为道具过期时间，过期失效操作通常作为一条单独流水记录
6. `mod_detail` 字段用于扩展业务上下文（如对局 ID、活动 ID 等），具体格式视来源系统而定
7. 分析时建议结合 `op_type` 区分发放/消耗/过期等不同事件类型

## 表数据流向

```text
hive_catalog_cdh5.dwd.fact_gtpl_prop_detail   （原始道具流水日志，全游戏混合）
            ↓  过滤斗地主 + 抽取字段
tcy_temp.dws_prop_log                    （游戏道具流水日志）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg                 （APP 端注册用户宽表）
```

> **文档版本**：v1.1
> **更新时间**：2026-06-12
