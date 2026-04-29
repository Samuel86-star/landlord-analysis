# DWS 中间表：斗地主银子变动日志表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_dq_silver_logs` |
| 全名 | `tcy_temp.dws_dq_silver_logs` |
| 类型 | DWS 层明细表（每日增量） |
| 描述 | 斗地主玩家银子变动日志表，从原始 `dwd_silver_si` 中筛选斗地主游戏相关数据，并补充渠道分类维度 |
| 粒度 | uid × 一条流水记录（不做聚合） |

## 设计背景

原始 `dwd_silver_si` 表为全平台玩家银子变动日志，包含多个游戏的混合数据。每次分析斗地主用户的银子变化时，需要过滤 `app_id = 1880053` 并关联渠道维表获取渠道分类，查询复杂度较高。

**解决方案**：预筛选斗地主相关数据，补充渠道分类，构建斗地主专属的银子变动日志表，提升后续分析查询效率。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| dt | date | 日期 | 2026-04-29 |
| app_id | int | 应用 ID | 1880053 |
| game_id | int | 游戏 ID | 53 |
| uid | int | 玩家唯一标识 | 123456789 |
| date_time | datetime | 操作时间 | 2026-04-29 10:30:00 |
| op_id | int | 操作 ID | 1001 |
| op_name | varchar(64) | 操作名称 | 对局输赢 |
| op_type_id | int | 操作类型 ID | 1 |
| op_type_name | varchar(64) | 操作类型名称 | 游戏 |
| silver_diff | int | 银两变化（含服务费），正=收入，负=支出 | 500 |
| silver_deposit | int | 银两变化或服务费 | 500 |
| silver_amount | int | 银两变化（不含服务费） | 400 |
| silver_balance | bigint | 操作后银子余额 | 10000 |
| silver_initial | bigint | 操作前银子余额 | 9500 |
| group_id | int | 大厅组号 | 6 |
| channel_id | int | 渠道号 | 1001 |
| channel_category_name | varchar(255) | 渠道分类名称 | 官方 |
| channel_category_tag_id | tinyint | 渠道分类标签：1=官方，2=渠道，3=小游戏 | 1 |
| source_guid | varchar(128) | 关联配置 ID | abc123 |

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
CREATE TABLE tcy_temp.dws_dq_silver_logs (
  `dt` date NOT NULL COMMENT "日期",
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `game_id` int(11) NOT NULL COMMENT "游戏ID",
  `uid` int(11) NOT NULL COMMENT "玩家ID",
  `date_time` datetime NULL COMMENT "操作时间",
  `op_id` int(11) NULL COMMENT "操作ID",
  `op_name` varchar(64) NULL COMMENT "操作名称",
  `op_type_id` int(11) NULL COMMENT "操作类型ID",
  `op_type_name` varchar(64) NULL COMMENT "操作类型名称",
  `silver_diff` int(11) NULL COMMENT "银两变化（含服务费），正=收入，负=支出",
  `silver_deposit` int(11) NULL COMMENT "银两变化或服务费",
  `silver_amount` int(11) NULL COMMENT "银两变化（不含服务费）",
  `silver_balance` bigint(20) NULL COMMENT "操作后银子余额",
  `silver_initial` bigint(20) NULL COMMENT "操作前银子余额",
  `group_id` int(11) NULL COMMENT "大厅组号",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `channel_category_name` varchar(255) NULL COMMENT "渠道分类名称",
  `channel_category_tag_id` tinyint(4) NULL COMMENT "渠道标签ID",
  `source_guid` varchar(128) NULL COMMENT "关联配置ID"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `dt`, `uid`)
COMMENT "斗地主玩家银子变动日志宽表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 16
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);
```

### 初始化 SQL

```sql
-- 斗地主银子变动日志全量初始化
INSERT INTO tcy_temp.dws_dq_silver_logs
SELECT
    STR_TO_DATE(CAST(s.dt AS VARCHAR), '%Y%m%d') AS dt,
    s.app_id,
    s.game_id,
    s.uid,
    s.date_time,
    s.op_id,
    s.op_name,
    s.op_type_id,
    s.op_type_name,
    s.silver_diff,
    s.silver_deposit,
    s.silver_amount,
    s.silver_balance,
    s.silver_initial,
    s.group_id,
    s.channel_id,
    COALESCE(chn.channel_category_name, '其他') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1) AS channel_category_tag_id,
    s.source_guid
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.dws_channel_category_map chn
    ON s.channel_id = chn.channel_id
WHERE s.app_id = 1880053
  AND s.game_id = 53
  AND s.dt BETWEEN 20260101 AND 20260428;
```

### 增量更新 SQL

```sql
-- 斗地主银子变动日志增量导入
-- 参数：将 ${DATE} 替换为实际日期（int 格式，如 20260429）
INSERT INTO tcy_temp.dws_dq_silver_logs
SELECT
    STR_TO_DATE(CAST(s.dt AS VARCHAR), '%Y%m%d') AS dt,
    s.app_id,
    s.game_id,
    s.uid,
    s.date_time,
    s.op_id,
    s.op_name,
    s.op_type_id,
    s.op_type_name,
    s.silver_diff,
    s.silver_deposit,
    s.silver_amount,
    s.silver_balance,
    s.silver_initial,
    s.group_id,
    s.channel_id,
    COALESCE(chn.channel_category_name, '其他') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1) AS channel_category_tag_id,
    s.source_guid
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.dws_channel_category_map chn
    ON s.channel_id = chn.channel_id
WHERE s.app_id = 1880053
  AND s.game_id = 53
  AND s.dt = ${DATE};
```

## 使用场景

### 1. 日银子变动汇总

```sql
-- 按操作类型统计每日银子变动
SELECT
    dt,
    op_type_name,
    COUNT(*) AS op_count,
    COUNT(DISTINCT uid) AS user_count,
    SUM(silver_diff) AS total_diff,
    SUM(silver_amount) AS total_amount
FROM tcy_temp.dws_dq_silver_logs
WHERE dt = '2026-04-29'
GROUP BY dt, op_type_name
ORDER BY total_diff DESC;
```

### 2. 留存 × 银子变动分析

```sql
-- 关联注册表，分析首日银子变动与留存的关系
SELECT
    r.reg_date,
    CASE
        WHEN s.total_diff IS NULL THEN 'A: 无银子变动'
        WHEN s.total_diff < -50000 THEN 'B: 巨亏（<-5万）'
        WHEN s.total_diff < -10000 THEN 'C: 大亏（-5万~-1万）'
        WHEN s.total_diff < 0 THEN 'D: 小亏（-1万~0）'
        WHEN s.total_diff < 10000 THEN 'E: 小赚（0~1万）'
        WHEN s.total_diff < 50000 THEN 'F: 大赚（1万~5万）'
        ELSE 'G: 巨赚（>5万）'
    END AS money_group,
    COUNT(DISTINCT r.uid) AS user_count
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN (
    SELECT uid, SUM(silver_diff) AS total_diff
    FROM tcy_temp.dws_dq_silver_logs
    WHERE dt = '2026-04-29'
    GROUP BY uid
) s ON r.uid = s.uid
WHERE r.reg_date = '2026-04-29'
GROUP BY r.reg_date, money_group
ORDER BY money_group;
```

### 3. 操作类型分布

```sql
-- 按操作名称排名，了解银子变动来源
SELECT
    op_name,
    op_type_name,
    COUNT(*) AS event_count,
    COUNT(DISTINCT uid) AS user_count,
    SUM(silver_diff) AS total_diff,
    ROUND(AVG(silver_diff), 0) AS avg_diff
FROM tcy_temp.dws_dq_silver_logs
WHERE dt BETWEEN '2026-04-20' AND '2026-04-29'
GROUP BY op_name, op_type_name
ORDER BY event_count DESC;
```

### 4. 分端银子变动对比

```sql
-- 按平台对比银子变动特征（通过 group_id 动态判定分端）
SELECT
    CASE
        WHEN group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        WHEN group_id IN (8, 88) THEN 'iOS'
        WHEN group_id = 56 THEN '小游戏'
        WHEN group_id NOT IN (55, 69, 0, 68) THEN 'PC'
    END AS platform,
    COUNT(DISTINCT uid) AS user_count,
    COUNT(*) AS event_count,
    SUM(silver_diff) AS total_diff,
    ROUND(AVG(silver_diff), 0) AS avg_diff
FROM tcy_temp.dws_dq_silver_logs
WHERE dt = '2026-04-29'
  AND op_type_name = '游戏'
GROUP BY platform
ORDER BY user_count DESC;
```

## 表数据流向

```text
tcy_dwd.dwd_silver_si              （原始银子变动日志，全游戏混合）
            ↓  过滤斗地主 + 补充维度
tcy_temp.dws_dq_silver_logs        （斗地主专属银子变动日志）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg      （APP 端注册用户宽表）
tcy_temp.dws_ddz_daily_game        （对局战绩统一字段表）
```

## 注意事项

1. 仅包含斗地主游戏数据（`app_id = 1880053`，`game_id = 53`）
2. `silver_diff` 含服务费，`silver_amount` 不含服务费，分析实际输赢时使用 `silver_amount`
3. `silver_balance` 为操作后余额，`silver_initial` 为操作前余额
4. 渠道分类通过 `LEFT JOIN dws_channel_category_map` 获取，未匹配到的标记为 `'其他'`
5. 不包含渠道分类信息的 `channel_id` 可通过关联 `dws_channel_category_map` 补全

> **文档版本**：v1.0
> **创建时间**：2026-04-29
