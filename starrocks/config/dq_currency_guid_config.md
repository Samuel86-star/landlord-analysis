# DWS 中间表：货币奖池配置表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dq_currency_guid_config` |
| 全名 | `tcy_temp.dq_currency_guid_config` |
| 类型 | DWS 层维表 |
| 描述 | 货币奖池配置表，记录银子发放的奖池标识和类型信息 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| guid | varchar(128) | 发放标识（如活动 ID、奖励编号等） | activity_001 |
| guid_title | varchar(255) | 发放名称（如 XX 活动、系统补偿等） | 新手礼包活动 |
| guid_type | tinyint | 发放类型：0=免费，1=付费 | 0 |

## 发放类型说明

| guid_type | 说明 |
| --------- | ---- |
| 0 | 免费 |
| 1 | 付费 |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dq_currency_guid_config (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `guid` varchar(128) NOT NULL COMMENT "发放标识(如活动ID、奖励编号等)",
  `guid_title` varchar(255) NOT NULL COMMENT "发放名称(如XX活动、系统补偿等)",
  `guid_type` tinyint(4) NOT NULL COMMENT "发放类型(0免费，1付费)"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `guid`)
COMMENT "货币奖池配置表"
DISTRIBUTED BY HASH(`guid`) BUCKETS 1
PROPERTIES (
  "replication_num" = "1",
  "compression" = "LZ4"
);
```

### 初始化数据

```sql
INSERT INTO tcy_temp.dq_currency_guid_config
SELECT
    app_id,
    guid,
    guid_title,
    guid_type
FROM hive_catalog_cdh5.dwd.dim_currency_guid_config
WHERE app_id = 1880053;
```

## 使用说明

该表为维表，用于关联 `dwd_silver_si` 表中的 `source_guid` 字段，获取奖池的详细信息。

### 关联查询示例

```sql
SELECT
    s.dt,
    s.uid,
    s.silver_diff,
    g.guid_title,
    CASE g.guid_type
        WHEN 0 THEN '免费'
        WHEN 1 THEN '付费'
    END AS guid_type_name
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.dq_currency_guid_config g
    ON s.app_id = g.app_id
    AND s.source_guid = g.guid
WHERE s.dt = 20260429
  AND s.app_id = 1880053;
```

## 注意事项

1. 该表为配置维表，数据量较小，适合广播 join
2. 需定期更新奖池配置信息
3. 关联时使用 `app_id` 和 `source_guid = guid` 进行匹配
