# DWS 中间表：货币操作类型配置表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dq_currency_op_config` |
| 全名 | `tcy_temp.dq_currency_op_config` |
| 类型 | DWS 层维表 |
| 描述 | 货币操作类型配置表，记录银子变动的操作类型和结算类型 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| op_id | int | 操作类型 ID | 1001 |
| op_name | varchar(64) | 操作类型名称 | 对局输赢 |
| settlement_type | tinyint | 结算类型：0=经营支出, 1=经营收入, 2=充值直得, 3=保险箱, 4=机器人, 5=沙盒, 6=后备箱 | 1 |

## 结算类型说明

| settlement_type | 说明 |
| --------------- | ---- |
| 0 | 经营支出 |
| 1 | 经营收入 |
| 2 | 充值直得 |
| 3 | 保险箱 |
| 4 | 机器人 |
| 5 | 沙盒 |
| 6 | 后备箱 |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dq_currency_op_config (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `op_id` int(11) NOT NULL COMMENT "操作类型ID",
  `op_name` varchar(64) NULL COMMENT "操作类型名称",
  `settlement_type` tinyint(4) NULL DEFAULT "0" COMMENT "结算类型：0经营支出, 1经营收入, 2充值直得, 3保险箱, 4机器人, 5沙盒, 6后备箱"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `op_id`)
COMMENT "货币操作类型配置表"
DISTRIBUTED BY HASH(`op_id`) BUCKETS 1
PROPERTIES (
  "replication_num" = "1",
  "compression" = "LZ4"
);
```

### 初始化数据

```sql
INSERT INTO tcy_temp.dq_currency_op_config
SELECT
    app_id,
    op_id,
    op_name,
    settlement_type
FROM hive_catalog_cdh5.dwd.dim_currency_op_config
WHERE app_id = 1880053;
```

## 使用说明

该表为维表，用于关联 `dwd_silver_si` 表中的 `op_id` 字段，获取操作类型的结算类型信息。

### 关联查询示例

```sql
SELECT
    s.dt,
    s.uid,
    s.op_name,
    s.silver_diff,
    c.settlement_type,
    CASE c.settlement_type
        WHEN 0 THEN '经营支出'
        WHEN 1 THEN '经营收入'
        WHEN 2 THEN '充值直得'
        WHEN 3 THEN '保险箱'
        WHEN 4 THEN '机器人'
        WHEN 5 THEN '沙盒'
        WHEN 6 THEN '后备箱'
    END AS settlement_type_name
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.dq_currency_op_config c
    ON s.app_id = c.app_id
    AND s.op_id = c.op_id
WHERE s.dt = 20260429
  AND s.app_id = 1880053;
```

## 注意事项

1. 该表为配置维表，数据量较小，适合广播 join
2. 需定期更新操作类型配置信息
3. 关联时使用 `app_id` 和 `op_id` 进行匹配
