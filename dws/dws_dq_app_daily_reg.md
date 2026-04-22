# DWS 中间表：APP 端每日注册用户表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_dq_app_daily_reg` |
| 全名 | `tcy_temp.dws_dq_app_daily_reg` |
| 类型 | DWS 层宽表（每日增量） |
| 描述 | APP 端每日注册用户宽表，包含首次登录的渠道和分端信息 |
| 粒度 | uid（一个用户一行） |

## 设计背景

`dws_dq_daily_reg` 表仅包含用户注册的基本信息（uid、注册时间），**不包含** `group_id` 和 `channel_id` 等渠道相关字段。每次分析 APP 端注册用户时都需要关联 `dws_dq_daily_login` 表，查询复杂度较高。

**解决方案**：预计算 APP 端注册用户的渠道信息，构建专用于 APP 端分析的注册用户宽表。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| uid | bigint | 玩家唯一标识 | 123456789 |
| reg_date | int | 注册日期（YYYYMMDD） | 20260210 |
| reg_datetime | datetime | 注册时间 | 2026-02-10 08:00:00 |
| reg_group_id | int | 首次登录分端 ID | 6 |
| reg_channel_id | bigint | 首次登录渠道号 | 1001 |
| reg_app_code | string | 首次登录应用code | zgda |
| channel_category_id | int | 渠道分类 ID | 1 |
| channel_category_name | string | 渠道分类名称 | '官方' |
| channel_category_tag_id | int | 渠道分类标签：1=官方，2=渠道，3=小游戏 | 1 |
| is_login_log_missing | int | 是否登录日志缺失：1=缺失，0=正常 | 0 |
| first_day_login_cnt | bigint | 首日登录次数 | 5 |

## APP 端分端规则

通过 `reg_group_id` 区分 APP 端类型：

| 端类型 | group_id 范围 |
| ------ | ------------- |
| Android | 6, 66, 33, 44, 77, 99 |
| iOS | 8, 88 |

## APP 端分开发语言规则

通过 `reg_app_code` 区分 APP 端开发语言：

| 端开发语言 | reg_app_code |
| ------ | ------------- |
| cocos-lua | zgda |
| cocos-creator | zgdx |


## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_dq_app_daily_reg (
  `app_id` bigint(20) NOT NULL COMMENT "应用ID",
  `reg_date` date NOT NULL COMMENT "注册日期",
  `reg_channel_id` bigint(20) NULL COMMENT "注册渠道ID",
  `uid` bigint(20) NOT NULL COMMENT "用户ID",
  `reg_datetime` datetime NULL COMMENT "注册具体时间",
  `reg_group_id` int(11) NULL COMMENT "注册组ID",
  `reg_app_code` varchar(64) NULL COMMENT "注册代码",
  `channel_category_id` int(11) NULL COMMENT "渠道分类ID",
  `channel_category_name` varchar(128) NULL COMMENT "渠道分类名称",
  `channel_category_tag_id` int(11) NULL COMMENT "渠道标签ID",
  `is_login_log_missing` tinyint(4) NULL DEFAULT '0' COMMENT "是否缺失登录日志: 0-否, 1-是",
  `first_day_login_cnt` int(11) NULL DEFAULT '0' COMMENT "首日登录次数"
) ENGINE=OLAP 
DUPLICATE KEY(`app_id`, `reg_date`, `reg_channel_id`)
COMMENT "App端用户注册首日行为汇总宽表"
-- 分区策略：按天分区，支持动态管理
PARTITION BY RANGE(`reg_date`) (
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
    "dynamic_partition.start" = "-80", 
    "dynamic_partition.end" = "3",   
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "80",
    "bloom_filter_columns" = "uid"
);
```

### 增量数据导入

```sql
insert into tcy_temp.dws_dq_app_daily_reg
SELECT 
    r.app_id,
    r.reg_date,
    COALESCE(l.first_channel_id, -1) AS reg_channel_id,
    r.uid,
    r.reg_datetime,
    COALESCE(l.first_group_id, -1) AS reg_group_id,
    COALESCE(l.first_app_code, '') AS reg_app_code,
    COALESCE(chn.channel_category_id, -1) AS channel_category_id,
    COALESCE(chn.channel_category_name, '未知/日志丢失') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1) AS channel_category_tag_id,
    CASE WHEN l.uid IS NULL THEN 1 ELSE 0 END AS is_login_log_missing,
    COALESCE(l.login_count, 0) AS first_day_login_cnt
FROM tcy_temp.dws_dq_daily_reg r
INNER JOIN tcy_temp.dws_dq_daily_login l 
    ON r.app_id = l.app_id 
    AND r.reg_date = l.login_date
    AND r.uid = l.uid 
LEFT JOIN tcy_temp.dws_channel_category_map chn 
    ON l.first_channel_id = chn.channel_id
WHERE r.app_id = 1880053
  AND r.reg_date between '2026-02-10' and '2026-04-21'
  AND l.first_group_id IN (6, 66, 33, 44, 77, 99, 8, 88);
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

## 使用示例

### 1. 按日期统计 APP 端注册用户

```sql
SELECT
    reg_date,
    COUNT(DISTINCT uid) AS total_users,
    SUM(CASE WHEN reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 1 ELSE 0 END) AS android_users,
    SUM(CASE WHEN reg_group_id IN (8, 88) THEN 1 ELSE 0 END) AS ios_users,
    SUM(CASE WHEN is_login_log_missing = 1 THEN 1 ELSE 0 END) AS missing_log_users
FROM tcy_temp.dws_dq_app_daily_reg
WHERE reg_date BETWEEN 20260210 AND 20260215
GROUP BY reg_date
ORDER BY reg_date;
```

### 2. 按渠道分类统计注册用户

```sql
SELECT
    reg_date,
    channel_category_name,
    CASE 
        WHEN reg_group_id IN (8, 88) THEN 'iOS'
        WHEN reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END AS platform,
    COUNT(DISTINCT uid) AS user_count
FROM tcy_temp.dws_dq_app_daily_reg
WHERE reg_date = 20260210
  AND is_login_log_missing = 0
GROUP BY reg_date, channel_category_name, 
    CASE 
        WHEN reg_group_id IN (8, 88) THEN 'iOS'
        WHEN reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END
ORDER BY reg_date, channel_category_name, platform;
```

### 3. 分析首日登录行为

```sql
SELECT
    CASE 
        WHEN first_day_login_cnt = 1 THEN '0：1次'
        WHEN first_day_login_cnt BETWEEN 2 AND 5 THEN '1：2-5次'
        WHEN first_day_login_cnt > 5 THEN '2：5次以上'
        ELSE '3：无登录记录'
    END AS login_bucket,
    reg_date,
    COUNT(DISTINCT uid) AS user_count
FROM tcy_temp.dws_dq_app_daily_reg
WHERE reg_date BETWEEN 20260210 AND 20260215
GROUP BY reg_date,
    CASE 
        WHEN first_day_login_cnt = 1 THEN '0：1次'
        WHEN first_day_login_cnt BETWEEN 2 AND 5 THEN '1：2-5次'
        WHEN first_day_login_cnt > 5 THEN '2：5次以上'
        ELSE '3：无登录记录'
    END
ORDER BY login_bucket desc, reg_date desc;
```

### 4. 关联对局数据计算游戏留存分母

```sql
-- 获取注册当日有对局的 APP 端用户（游戏留存分母口径）
SELECT
    r.uid,
    r.reg_date,
    r.reg_group_id,
    r.channel_category_name,
    COUNT(DISTINCT g.resultguid) AS first_day_game_cnt
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_daily_game g
    ON r.uid = g.uid
    AND r.reg_date = g.dt
WHERE r.reg_date = 20260210
  AND r.is_login_log_missing = 0
  AND g.robot != 1
GROUP BY r.uid, r.reg_date, r.reg_group_id, r.channel_category_name;
```

## 注意事项

1. **APP 端过滤**：本表仅包含 APP 端用户（Android + iOS），通过 `reg_group_id` 区分
2. **登录日志缺失**：`is_login_log_missing = 1` 表示注册当日无登录日志，可能是数据缺失或异常
3. **渠道分类**：通过关联 `dws_channel_category_map` 获取渠道分类信息
4. **与 dws_dq_daily_reg 的关系**：本表是 `dws_dq_daily_reg` 的 APP 端扩展视图，包含更多维度字段
5. **留存口径**：新增用户留存 = Day1注册且Day2登录 / Day1注册人数，与是否游戏无关

## 与其他 DWS 表的关系

```
tcy_temp.dws_dq_daily_reg          （全端注册信息）
            ↓  关联登录聚合表
tcy_temp.dws_dq_daily_login        （每日登录多维度聚合）
            ↓  过滤 APP 端 + 关联渠道映射
tcy_temp.dws_dq_app_daily_reg         （APP 端注册用户宽表）
            ↓  关联对局数据
tcy_temp.dws_ddz_daily_game        （对局战绩统一字段表）
```

## 数据流向

```
dws_dq_daily_reg ──┐
                   ├── INNER JOIN ──→ dws_dq_app_daily_reg
dws_dq_daily_login ─┘
                   │
                   └── LEFT JOIN ──→ dws_channel_category_map
```

> **文档版本**：v1.0
> **创建时间**：2026-04-09
> **更新说明**：
> - v1.0：初始版本，基于 `dws_dq_daily_reg` 方案2扩展设计，专用于 APP 端注册用户分析