# DWS 中间表：APP 端每日游戏活跃用户表

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_temp` |
| 表名 | `dws_app_game_active` |
| 全名 | `tcy_temp.dws_app_game_active` |
| 类型 | DWS 层聚合表（一次性创建） |
| 描述 | APP 端每日有对局的用户去重清单，**专用于留存 flag 计算** |
| 粒度 | uid × dt × app_id（一个用户一天一个应用一行） |

## 设计背景

留存计算的本质是：判断用户在注册后特定天数（Day1/Day3/Day7/Day14/Day30）是否再次有对局。

如果直接对原始 `dws_ddz_daily_game`（日志级别，数亿行）每次都做 JOIN 计算，在 StarRocks 中性能极差。

**解决方案**：将每日有对局的用户提前聚合到 `uid × dt × app_id` 粒度（数百万行），后续留存计算只需在该轻量表上做 JOIN，大幅提升查询性能。

## 与 `dws_app_game_stat` 的区别

| 表 | 用途 | 列数 |
|----|------|------|
| `dws_app_game_active` | 留存 flag 计算（当日是否有对局） | 3 列 |
| `dws_app_game_stat` | 完整游戏行为统计（对局数、胜率、倍数、经济等） | 30+ 列 |

留存计算需对**注册期 + 30 天**全量数据做高频 LEFT JOIN，`dws_app_game_active` 以极少列数保证 JOIN 性能。

## 时间范围说明

| 注册时间范围 | 最大留存观察期 | 表需覆盖到 |
|------------|-------------|-----------|
| 20260210 ~ 20260408 | Day30 | 20260408 + 30天 = **20260508** |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
|--------|------|------|--------|
| uid | bigint | 玩家唯一标识 | 123456789 |
| dt | int | 对局日期（YYYYMMDD int 格式） | 20260215 |
| app_id | bigint | 应用 ID | 1880053 |

## 构建 SQL

```sql
CREATE TABLE tcy_temp.dws_app_game_active (
  `app_id` INT NOT NULL COMMENT "应用ID",
  `uid` INT NOT NULL COMMENT "用户ID",
  `dt` date NOT NULL COMMENT "日期如20260422"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `dt`)
COMMENT "用户游戏活跃信息表"
PARTITION BY RANGE(`dt`) (
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
```

## 初始化数据

```sql
INSERT INTO tcy_temp.dws_app_game_active
SELECT app_id, uid, date(dt)
FROM tcy_temp.dws_ddz_daily_game
WHERE app_id = 1880053
  AND dt BETWEEN '2026-02-10' AND '2026-04-21'
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)  
GROUP BY 1, 2, 3;
```


## 留存计算方式

```sql
-- 示例：计算次留 / 7留 / 30留 的留存 flag
SELECT
    r.uid,
    r.reg_date,
    MAX(CASE WHEN a.dt = r.reg_date + 1  THEN 1 ELSE 0 END) AS day1_retained,   -- 次留：注册日之后的次日登录
    MAX(CASE WHEN a.dt = r.reg_date + 6  THEN 1 ELSE 0 END) AS day7_retained,   -- 7留：第 7 天登录（注册日记为第 1 天）
    MAX(CASE WHEN a.dt = r.reg_date + 29 THEN 1 ELSE 0 END) AS day30_retained   -- 30留：第 30 天登录
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_game_active a
    ON r.uid = a.uid
    AND a.app_id = r.app_id
    AND a.dt > r.reg_date  -- 只看注册日之后的活跃，避免混入注册当日行为
GROUP BY r.uid, r.reg_date;
```

> **留存日期口径**（Day 0 = 注册日）：

> - 次留：Day 0 注册的用户在次日（+1）登录的占比 → `reg_date + 1`
> - 7 留："第 7 天留存"，把注册日记为第 1 天 → `reg_date + 6`
> - 30 留：同理，第 30 天 → `reg_date + 29`
>
> **StarRocks 日期转换说明**：`dt` 为 int 类型，可直接做整数加法（`r.reg_date + 1`）比较，
> 也可使用 `str_to_date(CAST(dt AS VARCHAR), '%Y%m%d')` 转为 DATE 类型后再做 `datediff`。

## 与其他 DWS 表的关系

```
tcy_temp.dws_dq_app_daily_reg         （APP 端注册用户宽表）
            ↓  LEFT JOIN uid + app_id，dt > reg_date
tcy_temp.dws_app_game_active          （每日游戏活跃用户表，留存 flag 专用）  ← 本表
```

> **文档版本**：v2.0
> **更新说明**：

> - v1.0：初始版本（原名 `dws_ddz_daily_play`）
> - v1.1：优化 Bucket 配置（32→64）；添加排序键（`ORDER BY dt, uid`）
> - **v2.0**：重命名为 `dws_app_game_active`；新增 `app_id` 字段；补充与 `dws_app_game_stat` 的对比说明
