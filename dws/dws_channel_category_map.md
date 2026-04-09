# 渠道分类维表说明

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_temp` |
| 表名 | `dws_channel_category_map` |
| 全名 | `tcy_temp.dws_channel_category_map` |
| 类型 | DWS 层中间表（维表） |
| 描述 | 渠道号与渠道分类映射关系表 |
| 更新频率 | 每日更新（随渠道配置变化） |

## 表生成逻辑

```sql
CREATE TABLE tcy_temp.dws_channel_category_map AS
SELECT
    t1.channel_id,
    t2.channel_category_id,
    t2.channel_category_name,
    t2.channel_category_tag_id
FROM tcy_dim.dim_channel_singletag_dict t1
INNER JOIN hive_catalog_cdh5.dim.dim_channel_category t2
    ON t1.channel_type_id = t2.channel_type_id;
```

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
|--------|------|------|--------|---------|
| channel_id | bigint | 渠道号 | 1001 | 是 |
| channel_category_id | int | 渠道分类 ID | 1 | 是 |
| channel_category_name | string | 渠道分类名称 | "官方" | 是 |
| channel_category_tag_id | int | 渠道分类标签 ID | 1 | 是 |

## 渠道分类标签说明

| channel_category_tag_id | 说明 |
|------------------------|------|
| 1 | 官方 |
| 2 | 渠道 |
| 3 | 小游戏 |

## 使用示例

### 查询某渠道号所属分类

```sql
SELECT
    channel_id,
    channel_category_name,
    channel_category_tag_id
FROM dws.dws_channel_category_map
WHERE channel_id = 1001;
```

### 关联主表进行渠道分类分析

```sql
SELECT
    t1.dt,
    t2.channel_category_name,
    COUNT(DISTINCT t1.uid) AS user_count
FROM tcy_dwd.dwd_tcy_userlogin_si t1
LEFT JOIN tcy_temp.dws_channel_category_map t2
    ON t1.channel_id = t2.channel_id
WHERE t1.app_id = 1880053
  AND t1.dt BETWEEN 20260101 AND 20260408
GROUP BY t1.dt, t2.channel_category_name;
```

## 注意事项

1. 该表为维表，数据量较小，适合广播 join
2. 渠道配置可能发生变化，建议每日更新
3. 部分历史 `channel_id` 可能在维表中找不到映射，需处理 null 值
4. 如有新增渠道类型，需及时更新维表
