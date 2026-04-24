# DWS 中间表：用户每日登录维度聚合表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_dq_daily_login` |
| 全名 | `tcy_temp.dws_dq_daily_login` |
| 类型 | DWS 层聚合表（一次性创建） |
| 描述 | 用户每日登录多维度聚合表，包含首次/最后/最频繁登录的渠道和分端信息 |
| 粒度 | uid × login_date（一个用户一天一行） |

## 设计背景

原始 `dwd_tcy_userlogin_si` 为登录日志表（分钟级粒度），如需分析用户登录行为的多维度特征（首次登录、最后登录、最频繁渠道等），直接查询性能较差。

**解决方案**：将登录日志聚合到天级粒度，预计算用户每日的多维度登录特征，提升后续分析查询性能。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| login_date | date | 登录日期（天级聚合） | 2026-02-10 |
| uid | int | 玩家唯一标识 | 123456789 |
| first_login_time | datetime | 当日首次登录时间 | 2026-02-10 09:30:00 |
| first_app_code | varchar(32) | 当日首次登录应用code | zgdx |
| first_channel_id | int | 当日首次登录渠道号 | 1001 |
| first_group_id | int | 当日首次登录分端 ID | 6 |
| last_login_time | datetime | 当日最后登录时间 | 2026-02-10 22:15:00 |
| last_app_code | varchar(32) | 当日最后登录应用code | zgdx |
| last_channel_id | int | 当日最后登录渠道号 | 1001 |
| last_group_id | int | 当日最后登录分端 ID | 6 |
| most_freq_channel_id | int | 当日最频繁登录渠道号 | 1001 |
| most_freq_group_id | int | 当日最频繁登录分端 ID | 6 |
| most_freq_app_code | varchar(32) | 当日最频繁登录应用code | zgdx |
| channel_id_count | int | 当日接触渠道数（去重） | 2 |
| group_id_count | int | 当日切换分端数（去重） | 1 |
| app_code_count | int | 当日切换应用code数（去重） | 1 |
| login_count | int | 当日总登录次数 | 5 |

## 字段分类

### 时间维度

- `login_date`：登录日期（天级）
- `first_login_time`：当日首次登录时间
- `last_login_time`：当日最后登录时间

### 渠道维度

- `first_channel_id`：首次登录渠道（基于 `time_unix` 最小值）
- `last_channel_id`：最后登录渠道（基于 `time_unix` 最大值）
- `most_freq_channel_id`：最频繁登录渠道（基于出现次数最多）

### 分端维度

- `first_group_id`：首次登录分端（基于 `time_unix` 最小值）
- `last_group_id`：最后登录分端（基于 `time_unix` 最大值）
- `most_freq_group_id`：最频繁登录分端（基于出现次数最多）

### 统计维度

- `channel_id_count`：当日接触的不同渠道数量
- `group_id_count`：当日切换的不同分端数量
- `login_count`：当日总登录次数

## 构建 SQL

```sql
CREATE TABLE tcy_temp.dws_dq_daily_login (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `login_date` date NOT NULL COMMENT "登录日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `first_login_time` datetime NULL,
  `first_app_code` varchar(32) NULL, 
  `first_channel_id` int(11) NULL,
  `first_group_id` int(11) NULL,
  `last_login_time` datetime NULL,
  `last_app_code` varchar(32) NULL,
  `last_channel_id` int(11) NULL,
  `last_group_id` int(11) NULL,
  `most_freq_channel_id` int(11) NULL,
  `most_freq_group_id` int(11) NULL,
  `most_freq_app_code` varchar(32) NULL,
  `channel_id_count` int(11) NOT NULL,
  `group_id_count` int(11) NOT NULL,
  `app_code_count` int(11) NOT NULL,
  `login_count` int(11) NOT NULL
) ENGINE=OLAP 
DUPLICATE KEY(`app_id`, `login_date`, `uid`)
COMMENT "玩家日登录汇总表"
PARTITION BY RANGE(`login_date`) (
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
    "colocate_with" = "group_daily_data"
);
```

## 数据SQL

```sql
insert into tcy_temp.dws_dq_daily_login 
SELECT 
    app_id,
    DATE(dt) AS login_date,
    uid, 
    MIN(dt) AS first_login_time,
    MIN_BY(app_code, time_unix) AS first_app_code,
    MIN_BY(channel_id, time_unix) AS first_channel_id,
    MIN_BY(group_id, time_unix) AS first_group_id,
    MAX(dt) AS last_login_time,
    MAX_BY(app_code, time_unix) AS last_app_code,
    MAX_BY(channel_id, time_unix) AS last_channel_id,
    MAX_BY(group_id, time_unix) AS last_group_id,
    MAX_BY(channel_id, cnt_channel) AS most_freq_channel_id,
    MAX_BY(group_id, cnt_group) AS most_freq_group_id,
    MAX_BY(app_code, cnt_app_code) AS most_freq_app_code,
    COUNT(DISTINCT channel_id) AS channel_id_count,
    COUNT(DISTINCT group_id) AS group_id_count,
    COUNT(DISTINCT app_code) AS app_code_count,
    COUNT(1) AS login_count
FROM (
    SELECT 
        *,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), channel_id) AS cnt_channel,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), group_id) AS cnt_group,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), app_code) AS cnt_app_code
    FROM tcy_dwd.dwd_tcy_userlogin_si
    WHERE app_id = 1880053
      AND dt >= '2026-02-10 00:00:00' 
      AND dt <= '2026-04-20 23:59:59'
) t
GROUP BY app_id, DATE(dt), uid;

ALTER TABLE tcy_temp.dws_dq_daily_reg SET ("colocate_with" = "group_daily_data");
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

## 使用场景

### 1. 渠道切换分析

```sql
-- 统计用户当日首次与最后登录渠道是否一致
SELECT
    login_date,
    channel_switch_users,
    total_users,
    channel_switch_users * 1.0 / total_users AS switch_rate
FROM (
    SELECT
        login_date,
        COUNT(DISTINCT CASE WHEN first_channel_id != last_channel_id THEN uid END) AS channel_switch_users,
        COUNT(DISTINCT uid) AS total_users
    FROM tcy_temp.dws_dq_daily_login
    WHERE app_id = 1880053
    GROUP BY login_date
) t
ORDER BY login_date DESC;
```

### 2. 分端使用偏好分析

```sql
-- 分析用户当日最频繁使用的分端
SELECT
    login_date,
    most_freq_group_id,
    COUNT(DISTINCT uid) AS user_count
FROM tcy_temp.dws_dq_daily_login
WHERE app_id = 1880053
GROUP BY login_date, most_freq_group_id
ORDER BY 1 desc, 3 desc;
```

### 3. 登录活跃度分析

```sql
-- 统计不同登录频次的用户分布
SELECT
    CASE 
        WHEN login_count = 1 THEN '0:1次'
        WHEN login_count BETWEEN 2 AND 5 THEN '1:2-5次'
        WHEN login_count BETWEEN 6 AND 10 THEN '2:6-10次'
        ELSE '3:10次以上'
    END AS login_freq_bucket,
    login_date,
    COUNT(DISTINCT uid) AS user_count
FROM tcy_temp.dws_dq_daily_login
WHERE app_id = 1880053
GROUP BY login_date, 
    CASE 
        WHEN login_count = 1 THEN '0:1次'
        WHEN login_count BETWEEN 2 AND 5 THEN '1:2-5次'
        WHEN login_count BETWEEN 6 AND 10 THEN '2:6-10次'
        ELSE '3:10次以上'
    END
order by 1 desc, 2 desc;
```

### 4. 注册用户登录行为分析

```sql
-- 关联新用户注册表，分析注册当日的登录特征
SELECT
    r.uid,
    r.reg_date,
    l.first_login_time,
    l.first_channel_id,
    l.first_group_id,
    l.login_count
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON r.uid = l.uid 
    AND CAST(DATE_FORMAT(l.login_date, '%Y%m%d') AS INT) = r.reg_date
WHERE r.reg_date = 20260210;
```

## 注意事项

1. **时间戳精度**：`dwd_tcy_userlogin_si.time_unix` 为毫秒级时间戳，`MIN_BY/MAX_BY` 基于此排序
2. **最频繁维度实现**：通过字符串拼接 `LPAD(count, 10, '0') + channel_id` 取 MAX，再截取后半部分实现
3. **数据范围**：默认覆盖 `2026-02-10` 至 `2026-04-08`，可根据实际需求调整
4. **去重统计**：`channel_id_count` 和 `group_id_count` 为去重计数，反映用户当日接触的渠道/分端多样性
5. **关联使用**：可与 `dws_dq_app_daily_reg` 等表关联，丰富用户行为分析维度

## 与其他 DWS 表的关系

```
tcy_dwd.dwd_tcy_userlogin_si        （原始登录日志，分钟级）
            ↓  聚合
tcy_temp.dws_dq_daily_login         （每日登录多维度聚合）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg          （APP 端注册用户宽表）
```

> **文档版本**：v1.0
> **创建时间**：2026-04-09
> **更新说明**：
> - v1.0：初始版本，包含首次/最后/最频繁登录维度，以及统计维度
