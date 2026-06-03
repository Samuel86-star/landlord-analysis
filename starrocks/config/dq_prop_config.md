# DWS 中间表：道具配置维表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dq_prop_config` |
| 全名 | `tcy_temp.dq_prop_config` |
| 类型 | DWS 层维表 |
| 描述 | 道具配置维表，记录道具的 ID、名称、类型分类 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| prop_id | int | 道具 ID | 10001 |
| prop_name | varchar(32) | 道具名称 | 记牌器 |
| prop_type | tinyint | 道具类型：0=货币，1=道具，2=装饰，3=表情 | 1 |

## 道具类型说明

| prop_type | 说明 |
| --------- | ---- |
| 0 | 货币 |
| 1 | 道具 |
| 2 | 装饰 |
| 3 | 表情 |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dq_prop_config (
  `prop_id` int(11) NOT NULL COMMENT "道具ID",
  `prop_name` varchar(32) NULL COMMENT "道具名称",
  `prop_type` tinyint(4) NULL COMMENT "道具类型：0货币, 1道具, 2装饰, 3表情"
) ENGINE=OLAP
DUPLICATE KEY(`prop_id`)
COMMENT "道具配置维表"
DISTRIBUTED BY HASH(`prop_id`) BUCKETS 1
PROPERTIES (
  "replication_num" = "1",
  "compression" = "LZ4"
);
```

### 初始化数据

> **TODO**：源数据表与字段映射待确认后补充。

## 使用说明

该表为维表，用于关联 `dws_game_prop_log` 表中的 `prop_id` 字段，获取道具的类型分类信息。

### 关联查询示例

```sql
SELECT
    p.log_date,
    p.uid,
    p.prop_name,
    p.prop_cnt,
    c.prop_type,
    CASE c.prop_type
        WHEN 0 THEN '货币'
        WHEN 1 THEN '道具'
        WHEN 2 THEN '装饰'
        WHEN 3 THEN '表情'
    END AS prop_type_name
FROM tcy_temp.dws_game_prop_log p
LEFT JOIN tcy_temp.dq_prop_config c
    ON p.prop_id = c.prop_id
WHERE p.log_date = '2026-05-13'
  AND p.app_id = 1880053;
```

## 注意事项

1. 该表为配置维表，数据量较小，适合广播 join
2. 需定期更新道具配置信息
3. 关联时使用 `prop_id` 进行匹配
