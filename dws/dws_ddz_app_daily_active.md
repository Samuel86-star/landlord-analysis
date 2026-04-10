# DWS 中间表：APP 端每日活跃用户表

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_app_daily_active` |
| 全名 | `tcy_temp.dws_ddz_app_daily_active` |
| 类型 | DWS 层聚合表（一次性创建） |
| 描述 | APP 端每日有对局的用户去重清单，**专用于留存 flag 计算** |
| 粒度 | uid × dt（一个用户一天一行） |

## 设计背景

留存计算的本质是：判断用户在注册后特定天数（Day1/Day3/Day7/Day14/Day30）是否再次有对局。

如果直接对原始 `dwd_game_combat_si`（日志级别，数亿行）每次都做 JOIN 计算，在 StarRocks 中性能极差。

**解决方案**：将每日有对局的用户提前聚合到 `uid × dt` 粒度（数百万行），后续留存计算只需在该轻量表上做 JOIN，大幅提升查询性能。

## 时间范围说明

| 注册时间范围 | 最大留存观察期 | daily_active 需覆盖到 |
|------------|-------------|----------------------|
| 20260210 ~ 20260408 | Day30 | 20260408 + 30天 = **20260508** |

因此 `dws_ddz_app_daily_active` 的时间范围设为 **20260210 ~ 20260508**，比注册期多延伸 30 天。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
|--------|------|------|--------|
| uid | bigint | 玩家唯一标识 | 123456789 |
| dt | int | 对局日期（YYYYMMDD int 格式） | 20260215 |

## 构建 SQL

```sql
-- Step 2：构建每日活跃用户去重表（用于留存 flag 计算）
-- 时间范围覆盖注册期 + 最大留存观测期（Day30），上限延伸 30 天至 20260508
CREATE TABLE tcy_temp.dws_ddz_app_daily_active
DISTRIBUTED BY HASH(uid) BUCKETS 64
ORDER BY dt, uid
PROPERTIES("replication_num" = "1")
AS
SELECT
    uid,
    dt
FROM tcy_dwd.dwd_game_combat_si
WHERE dt BETWEEN 20260210 AND 20260508  -- 覆盖注册期 + Day30 观测期
  AND game_id = 53
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99) -- APP 端用户
  AND room_id NOT IN (11534, 14238, 15458)          -- 排除积分场/比赛场
GROUP BY uid, dt;
```

## 留存计算方式

基于此表，通过 `datediff` 计算注册日与对局日的天数差，实现留存 flag：

```sql
-- 示例：计算 Day1 / Day3 / Day7 / Day14 / Day30 留存 flag
SELECT
    r.uid,
    r.reg_date,
    MAX(CASE WHEN datediff(
            date_format(CAST(a.dt AS VARCHAR), 'yyyyMMdd'),
            date_format(CAST(r.reg_date AS VARCHAR), 'yyyyMMdd')
        ) = 1 THEN 1 ELSE 0 END) AS day1_retained,
    MAX(CASE WHEN datediff(...) = 3 THEN 1 ELSE 0 END) AS day3_retained,
    MAX(CASE WHEN datediff(...) = 7 THEN 1 ELSE 0 END) AS day7_retained,
    MAX(CASE WHEN datediff(...) = 14 THEN 1 ELSE 0 END) AS day14_retained,
    MAX(CASE WHEN datediff(...) = 30 THEN 1 ELSE 0 END) AS day30_retained
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login a
    ON r.uid = a.uid
    AND a.dt > r.reg_date  -- 只看注册日之后的活跃，避免混入注册当日行为
GROUP BY r.uid, r.reg_date;
```

> **StarRocks 日期转换说明**：因 `dt` 为 int 类型，需先转为字符串再用 `date_format` 解析，
> 或使用 `str_to_date(CAST(dt AS VARCHAR), '%Y%m%d')` 转为 DATE 类型后再做 `datediff`。

## 与其他 DWS 表的关系

```
tcy_temp.dws_dq_app_daily_reg             （APP 端注册用户宽表）
            ↓  LEFT JOIN uid，login_date > reg_date
tcy_temp.dws_dq_daily_login            （每日登录聚合表）
```

> **文档版本**：v1.1
> **更新说明**：
> - v1.0：初始版本
> - **v1.1**：**优化 Bucket 配置**（32→64）；**添加排序键**（`ORDER BY dt, uid`）
