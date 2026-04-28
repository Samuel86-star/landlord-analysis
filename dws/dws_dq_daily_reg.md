# 游戏用户注册表说明

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_dq_daily_reg` |
| 全名 | `tcy_temp.dws_dq_daily_reg` |
| 类型 | 原始数据表 |
| 描述 | 平台用户在游戏中的注册信息表 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
| ----- | ---- | ---- | ------ | ------- |
| app_id | int | 应用 ID | 1880053 | 是 |
| uid | int | 玩家唯一标识 ID | 123456789 | 是 |
| reg_date | date | 注册日期（格式：YYYY-MM-DD） | 2026-02-10 | 是 |
| reg_datetime | datetime | 游戏注册时间 | 2026-02-10 08:00:00 | 是 |

## 构建 SQL

### 数据来源

本表数据来源于 Hive 表 `hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st`，将时间戳转换为 datetime 格式。

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_dq_daily_reg (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `reg_date` DATE NOT NULL COMMENT "注册日期",
  `reg_datetime` datetime NULL COMMENT "注册具体时间"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `reg_date`)
COMMENT "用户日注册汇总表"
PARTITION BY RANGE(`reg_date`) (
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
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "80",
    "colocate_with" = "group_daily_data"
);

ALTER TABLE tcy_temp.dws_dq_daily_reg SET ("colocate_with" = "group_daily_data");
```

### 增量数据导入

```sql
INSERT INTO tcy_temp.dws_dq_daily_reg
SELECT
    app_id,
    uid,
    str_to_date(CAST(dt AS STRING), '%Y%m%d'),
    FROM_UNIXTIME(first_login_ts / 1000) AS reg_datetime
FROM hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st
WHERE app_id = 1880053
  AND dt between 20260210 and 20260420;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

## 如何获取渠道信息

本表仅记录用户注册信息，**不包含** `group_id` 和 `channel_id` 等渠道相关字段。

如需获取注册用户的渠道信息，需要关联首次登录日志（注册当天的首次登录记录）：

### 方案1：获取渠道号和平台分组

直接关联首次登录日志表：

```sql
SELECT
    r.uid,
    r.reg_date,
    r.reg_datetime,
    COALESCE(l.first_channel_id, -1) AS reg_channel_id,
    COALESCE(l.first_group_id, -1)   AS reg_group_id,
    COALESCE(chn.channel_category_name, '未知/日志丢失') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1)          AS channel_category_tag_id,
    CASE WHEN l.uid IS NULL THEN 1 ELSE 0 END AS is_login_log_missing,
    COALESCE(l.login_count, 0) AS first_day_login_cnt
FROM tcy_temp.dws_dq_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND CAST(DATE_FORMAT(l.login_date, '%Y%m%d') AS INT) = r.reg_date
LEFT JOIN tcy_temp.dws_channel_category_map chn
    ON l.first_channel_id = chn.channel_id
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN 20260210 AND 20260210;
```

### 方案2：获取渠道分类标签

在方案1的基础上，再通过 `channel_id` 关联渠道映射表获取分类：

```sql
SELECT
    r.uid,
    r.reg_date,
    r.reg_datetime,
    COALESCE(l.first_channel_id, -1) AS reg_channel_id,
    COALESCE(l.first_group_id, -1)   AS reg_group_id,
    COALESCE(chn.channel_category_name, '未知/日志丢失') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1)          AS channel_category_tag_id,
    CASE WHEN l.uid IS NULL THEN 1 ELSE 0 END AS is_login_log_missing,
    COALESCE(l.login_count, 0) AS first_day_login_cnt
FROM tcy_temp.dws_dq_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid
    AND CAST(DATE_FORMAT(l.login_date, '%Y%m%d') AS INT) = r.reg_date
LEFT JOIN tcy_temp.dws_channel_category_map chn
    ON l.first_channel_id = chn.channel_id
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN 20260210 AND 20260210;
```

## 注意事项

1. `reg_date` 字段格式为 YYYYMMDD（int 类型），而 `reg_datetime` 为 datetime 类型
2. 查询时间段时使用 `reg_date` 或 `reg_datetime` 字段，注意 `reg_date` 为 int 类型的日期（如：20260210）
3. 查询时需使用 `app_id` 字段进行过滤（如：`app_id = 1880053`）
