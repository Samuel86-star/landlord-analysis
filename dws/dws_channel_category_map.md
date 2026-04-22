# 渠道分类维表说明

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_channel_category_map` |
| 全名 | `tcy_temp.dws_channel_category_map` |
| 类型 | DWS 层中间表（维表） |
| 描述 | 渠道号与渠道分类映射关系表 |
| 更新频率 | 每日更新（随渠道配置变化） |

## 表生成逻辑

```sql
CREATE TABLE tcy_temp.dws_channel_category_map (
  `channel_id` int(11) NOT NULL COMMENT "渠道ID",
  `channel_category_id` int(11) NULL COMMENT "分类ID",
  `channel_category_name` varchar(255) NULL COMMENT "分类名称",
  `channel_category_tag_id` int(11) NULL COMMENT "标签ID"
) ENGINE=OLAP 
DUPLICATE KEY(`channel_id`)
COMMENT "渠道分类映射配置表"
DISTRIBUTED BY HASH(`channel_id`) BUCKETS 1 
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4"
);
```

## 初始化数据SQL

```sql
INSERT INTO tcy_temp.dws_channel_category_map
SELECT
    t1.channel_id,
    ANY_VALUE(t2.channel_category_id),
    ANY_VALUE(t2.channel_category_name),
    ANY_VALUE(t2.channel_category_tag_id)
FROM tcy_dim.dim_channel_singletag_dict t1
INNER JOIN hive_catalog_cdh5.dim.dim_channel_category t2
    ON t1.channel_type_id = t2.channel_type_id
GROUP BY t1.channel_id;
```

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
| ------ | ---- | ---- | ------ | ------- |
| channel_id | bigint | 渠道号 | 1001 | 是 |
| channel_category_id | int | 渠道分类 ID | 1 | 是 |
| channel_category_name | string | 渠道分类名称 | "官方" | 是 |
| channel_category_tag_id | int | 渠道分类标签 ID | 1 | 是 |

## 渠道分类层级说明

该表建立了三层渠道分类体系：

### 层级结构

```view
channel_category_tag_id (渠道大类)
    └─ channel_category_id (渠道细分类)
        └─ channel_id (具体渠道号)
```

### 渠道分类标签（大类）

| channel_category_tag_id | 说明 |
| ---------------------- | ---- |
| 1 | 官方 |
| 2 | 渠道 |
| 3 | 小游戏 |

### 渠道分类明细（细分类）

同一个 `channel_category_tag_id` 下可能存在多个 `channel_category_id`，示例：

**官方大类 (tag_id = 1)：**

| channel_category_id | channel_category_name | 说明 |
| ------------------- | -------------------- | ---- |
| 1 | 官方(非CPS) | 官方自有渠道，非CPS结算 |
| 2 | 官方(CPS) | 官方渠道，采用CPS结算 |
| 3 | IOS | 官方IOS渠道 |
| 9 | 鸿蒙 | 官方鸿蒙渠道 |

**渠道大类 (tag_id = 2)：**

| channel_category_id | channel_category_name | 说明 |
| ------------------- | -------------------- | ---- |
| 4 | 渠道(非CPS) | 第三方渠道，非CPS结算 |
| 5 | 渠道(CPS) | 第三方渠道，采用CPS结算 |

**小游戏大类 (tag_id = 3)：**

| channel_category_id | channel_category_name | 说明 |
| ------------------- | -------------------- | ---- |
| 6 | 小游戏(非CPS) | 小游戏平台，非CPS结算 |
| 7 | 小游戏(CPS) | 小游戏平台，采用CPS结算 |

### 渠道号分布

每个 `channel_category_id` 下可能包含多个 `channel_id`，例如：

- `channel_category_id = 1` (官方-非CPS) 可能包含：1001, 1002, 1003...
- `channel_category_id = 2` (官方-CPS) 可能包含：2001, 2002, 2003...

## 使用建议

### 按渠道大类分析（官方/渠道/小游戏）

```sql
SELECT
    channel_category_tag_id,
    CASE channel_category_tag_id
        WHEN 1 THEN '官方'
        WHEN 2 THEN '渠道'
        WHEN 3 THEN '小游戏'
    END AS tag_name,
    COUNT(DISTINCT channel_id) AS channel_count
FROM tcy_temp.dws_channel_category_map
GROUP BY channel_category_tag_id;
```

### 按渠道细分类分析

```sql
SELECT
    channel_category_name,
    COUNT(DISTINCT channel_id) AS channel_count
FROM tcy_temp.dws_channel_category_map
GROUP BY channel_category_name;
```

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
