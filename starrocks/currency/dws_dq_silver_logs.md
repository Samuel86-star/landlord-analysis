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
| app_id | int | 应用 ID | 1880053 |
| dt | date | 日期 | 2026-04-29 |
| uid | int | 玩家唯一标识 | 123456789 |
| app_code | varchar(32) | 应用code | zgda（可以跟group_id结合一起区分客户端开发语言） |
| game_id | int | 游戏 ID | 53 |
| date_time | datetime | 操作时间 | 2026-04-29 10:30:00 |
| op_id | int | 操作 ID | 1001 |
| op_name | varchar(64) | 操作名称 | 对局输赢 |
| op_type_id | int | 操作类型 ID | 1 |
| op_type_name | varchar(64) | 操作类型名称 | 游戏 |
| fin_flow_scn_id | int | 金流场景 ID，关联 `dq_fin_flow_scene_dict.scene_id` | 1001 |
| settlement_type | tinyint | 结算类型：0=经营支出, 1=经营收入, 2=充值直得, 3=保险箱, 4=机器人, 5=沙盒, 6=后备箱 | 1 |
| silver_diff | int | 银两变化（含服务费），正=收入，负=支出。仅 `op_id = 300501` 时有记录值。`silver_diff = silver_balance - silver_initial` | 500 |
| silver_deposit | int | `op_id = 300501` 时为服务费，其余为货币变动值 | 500 |
| silver_amount | int | `op_id = 300501` 时为总变动（`silver_diff + silver_deposit`），其余为货币变动值 | 400 |
| silver_balance | bigint | 操作后银子余额 | 10000 |
| silver_initial | bigint | 操作前银子余额 | 9500 |
| group_id | int | 大厅组号 | 6 |
| channel_id | int | 渠道号 | 1001 |
| channel_category_name | varchar(255) | 渠道分类名称 | 官方 |
| channel_category_tag_id | tinyint | 渠道分类标签：1=官方，2=渠道，3=小游戏 | 1 |
| source_guid | varchar(128) | 关联配置 ID | abc123 |
| guid_title | varchar(255) | 发放名称（如 XX 活动、系统补偿等） | 新手礼包活动 |
| guid_type | tinyint | 发放类型：0=免费，1=付费 | 0 |

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
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `dt` date NOT NULL COMMENT "日期",
  `game_id` int(11) NOT NULL COMMENT "游戏ID",
  `uid` int(11) NOT NULL COMMENT "玩家ID",
  `app_code` varchar(32) NULL COMMENT "应用code",
  `app_vers` varchar(32) NULL COMMENT "应用版本号",
  `date_time` datetime NULL COMMENT "操作时间",
  `op_id` int(11) NULL COMMENT "操作ID",
  `op_name` varchar(64) NULL COMMENT "操作名称",
  `op_type_id` int(11) NULL COMMENT "操作类型ID",
  `op_type_name` varchar(64) NULL COMMENT "操作类型名称",
  `fin_flow_scn_id` int(11) NULL COMMENT "金流场景ID，关联dq_fin_flow_scene_dict.scene_id",
  `settlement_type` tinyint(4) NULL COMMENT "结算类型：0经营支出,1经营收入,2充值直得,3保险箱,4机器人,5沙盒,6后备箱",
  `silver_diff` int(11) NULL COMMENT "银两变化（含服务费），正=收入，负=支出",
  `silver_deposit` int(11) NULL COMMENT "银两变化或服务费",
  `silver_amount` int(11) NULL COMMENT "银两变化（不含服务费）",
  `silver_balance` bigint(20) NULL COMMENT "操作后银子余额",
  `silver_initial` bigint(20) NULL COMMENT "操作前银子余额",
  `group_id` int(11) NULL COMMENT "大厅组号",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `channel_category_name` varchar(255) NULL COMMENT "渠道分类名称",
  `channel_category_tag_id` tinyint(4) NULL COMMENT "渠道标签ID",
  `source_guid` varchar(128) NULL COMMENT "关联配置ID",
  `guid_title` varchar(255) NULL COMMENT "发放名称",
  `guid_type` tinyint(4) NULL COMMENT "发放类型：0免费，1付费"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `dt`, `game_id`, `uid`)
COMMENT "斗地主玩家银子变动日志宽表"
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
-- 斗地主银子变动日志导入
-- 参数：${START_DATE} / ${END_DATE} 替换为实际日期（int 格式，如 20260429）
--       全量初始化时使用日期范围，增量更新时 START_DATE = END_DATE
INSERT INTO tcy_temp.dws_dq_silver_logs
SELECT
    s.app_id,
    STR_TO_DATE(CAST(s.dt AS VARCHAR), '%Y%m%d') AS dt,
    s.game_id,
    s.uid,
    COALESCE(s.game_code, s.app_code) as app_code,
    COALESCE(s.game_vers, s.app_vers) as app_vers,
    s.date_time,
    s.op_id,
    s.op_name,
    s.op_type_id,
    s.op_type_name,
    s.fin_flow_scn_id,
    COALESCE(op.settlement_type, -1) AS settlement_type,
    s.silver_diff,
    s.silver_deposit,
    s.silver_amount,
    s.silver_balance,
    s.silver_initial,
    s.group_id,
    s.channel_id,
    COALESCE(chn.channel_category_name, '其他') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1) AS channel_category_tag_id,
    s.source_guid,
    COALESCE(gc.guid_title, '') AS guid_title,
    COALESCE(gc.guid_type, CASE WHEN s.op_id = 300104 THEN 0 ELSE -1 END) AS guid_type
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.dq_channel_category_map chn
    ON s.channel_id = chn.channel_id
LEFT JOIN tcy_temp.dq_currency_op_config op
    ON s.app_id = op.app_id AND s.op_id = op.op_id
LEFT JOIN tcy_temp.dq_currency_guid_config gc
    ON s.app_id = gc.app_id AND s.source_guid = gc.guid
WHERE s.app_id = 1880053
  AND s.game_id = 53
  AND s.dt BETWEEN ${START_DATE} AND ${END_DATE};
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

## 数据校验 SQL

> 本节提供一组数据校验 SQL，用于在初始化或增量更新后验证数据正确性。建议每次导入后依次执行。

### 1. 源数据 vs 目标数据行数对比

> **校验目标**：源表 `dwd_silver_si` 中满足 `app_id = 1880053 AND game_id = 53` 的行数应与目标表当日行数一致，防止数据遗漏或重复。

```sql
WITH src AS (
    SELECT COUNT(*) AS src_cnt
    FROM tcy_dwd.dwd_silver_si
    WHERE app_id = 1880053
      AND game_id = 53
      AND dt = 20260429
),
tgt AS (
    SELECT COUNT(*) AS tgt_cnt
    FROM tcy_temp.dws_dq_silver_logs
    WHERE app_id = 1880053
      AND dt = '2026-04-29'
)
SELECT src.src_cnt, tgt.tgt_cnt, (src.src_cnt - tgt.tgt_cnt) AS diff
FROM src, tgt;
```

> **期望结果**：`diff = 0`（完全一致）。

### 2. 必填字段非空校验

> **校验目标**：`app_id`、`dt`、`uid`、`game_id`、`date_time` 等关键字段应非空。若出现 NULL，说明源表数据异常或 INSERT 逻辑有问题。

```sql
SELECT
    SUM(CASE WHEN app_id IS NULL THEN 1 ELSE 0 END) AS null_app_id,
    SUM(CASE WHEN dt IS NULL THEN 1 ELSE 0 END) AS null_dt,
    SUM(CASE WHEN uid IS NULL THEN 1 ELSE 0 END) AS null_uid,
    SUM(CASE WHEN game_id IS NULL THEN 1 ELSE 0 END) AS null_game_id,
    SUM(CASE WHEN date_time IS NULL THEN 1 ELSE 0 END) AS null_date_time,
    COUNT(*) AS total_cnt
FROM tcy_temp.dws_dq_silver_logs
WHERE app_id = 1880053
  AND dt = '2026-04-29';
```

> **期望结果**：所有 `null_*` 字段应为 0。

### 3. 银子余额连续性校验

> **校验目标**：操作前后的余额变动应闭环（`silver_diff = silver_balance - silver_initial`）。
> - `op_id = 300501`（对局货币变动）：`silver_initial + silver_diff = silver_balance`
> - 其他 op_id：`silver_diff` 无记录值，`silver_amount` 即为货币变动值，校验 `silver_initial + silver_amount = silver_balance`

```sql
SELECT
    uid,
    date_time,
    op_id,
    op_name,
    silver_initial,
    silver_diff,
    silver_amount,
    silver_balance,
    CASE
        WHEN op_id = 300501 THEN ABS(silver_initial + silver_diff - silver_balance)
        ELSE ABS(silver_initial + silver_amount - silver_balance)
    END AS residual
FROM tcy_temp.dws_dq_silver_logs
WHERE app_id = 1880053
  AND dt = '2026-04-29'
  AND CASE
        WHEN op_id = 300501 THEN ABS(silver_initial + silver_diff - silver_balance)
        ELSE ABS(silver_initial + silver_amount - silver_balance)
      END > 0
LIMIT 10;
```

> **期望结果**：返回 0 行。若有大量不闭环记录，需排查源表 `dwd_silver_si` 数据质量。

### 4. 服务费拆分一致性校验（仅 op_id = 300501）

> **校验目标**：对局货币变动场景下，`silver_diff + silver_deposit = silver_amount`。
> **注意**：非 `op_id = 300501` 时 `silver_diff` 无记录值，不适用此校验。

```sql
SELECT
    uid,
    date_time,
    op_id,
    op_name,
    silver_diff,
    silver_deposit,
    silver_amount,
    (silver_diff + silver_deposit - silver_amount) AS residual
FROM tcy_temp.dws_dq_silver_logs
WHERE app_id = 1880053
  AND dt = '2026-04-29'
  AND op_id = 300501
  AND ABS(silver_diff + silver_deposit - silver_amount) > 0
LIMIT 10;
```

> **期望结果**：返回 0 行。若存在偏差，说明源表字段定义与假设不符，需重新确认字段含义。

### 5. 维表匹配覆盖率校验

> **校验目标**：渠道分类、结算类型、奖池配置均通过 `LEFT JOIN` 维表获取，若未匹配比例过高，说明维表需要更新。

```sql
SELECT
    COUNT(*) AS total_cnt,
    ROUND(SUM(CASE WHEN channel_category_name = '其他' THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS unmatched_channel_pct,
    ROUND(SUM(CASE WHEN settlement_type = -1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS unmatched_settlement_pct,
    ROUND(SUM(CASE WHEN guid_type = -1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS unmatched_guid_pct
FROM tcy_temp.dws_dq_silver_logs
WHERE app_id = 1880053
  AND dt = '2026-04-29';
```

> **期望结果**：`unmatched_channel_pct` 与 `unmatched_settlement_pct` 通常 <5%。`unmatched_guid_pct` 可较高（大部分流水无 `source_guid`），但需跟踪其趋势稳定性。

### 6. 游戏/应用过滤正确性校验

> **校验目标**：目标表应只包含 `app_id = 1880053` 且 `game_id = 53` 的记录。若出现其他值，说明过滤条件失效。

```sql
SELECT
    app_id,
    game_id,
    COUNT(*) AS cnt
FROM tcy_temp.dws_dq_silver_logs
WHERE app_id = 1880053
  AND dt = '2026-04-29'
GROUP BY app_id, game_id
HAVING app_id != 1880053 OR game_id != 53;
```

> **期望结果**：返回 0 行。

---

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

1. **查询必须加 `WHERE app_id = 1880053`**：使用本表时 SQL 一律需要带上该条件，否则可能导致全表扫描或结果异常
2. 仅包含斗地主游戏数据（`app_id = 1880053`，`game_id = 53`）
3. **`op_id = 300501`（对局货币变动）**：`silver_diff = silver_balance - silver_initial`（含服务费的变动），`silver_deposit` 为服务费，`silver_amount = silver_diff + silver_deposit`（总变动）
4. **其他 op_id**：`silver_diff` 无记录值，`silver_amount` 和 `silver_deposit` 均为货币变动值，分析时建议直接使用 `silver_amount`
5. `silver_balance` 为操作后余额，`silver_initial` 为操作前余额
6. 渠道分类通过 `LEFT JOIN dq_channel_category_map` 获取，未匹配到的标记为 `'其他'`
7. 不包含渠道分类信息的 `channel_id` 可通过关联 `dq_channel_category_map` 补全

> **文档版本**：v1.0
> **创建时间**：2026-04-29
