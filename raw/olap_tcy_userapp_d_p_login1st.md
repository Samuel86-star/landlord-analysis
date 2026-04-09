# 游戏用户注册表说明

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `hive_catalog_cdh5.dm` |
| 表名 | `olap_tcy_userapp_d_p_login1st` |
| 全名 | `hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st` |
| 类型 | 原始数据表 |
| 描述 | 游戏用户首次注册登录信息表 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
|-------|------|------|--------|---------|
| uid | bigint | 玩家唯一标识 ID | 123456789 | 是 |
| app_id | bigint | 应用 ID | 1880053 | 是 |
| first_login_ts | bigint | 游戏注册时间戳 | 1707523200 | 是 |
| dt | int | 注册日期（格式：YYYYMMDD） | 20260210 | 是 |

## 全局字段说明

以下全局字段在本表中同样适用：

| 字段名 | 类型 | 说明 |
|--------|------|------|
| uid | bigint | 玩家唯一标识 ID |
| app_id | bigint | 应用 ID（1880053 = 斗地主游戏应用） |
| group_id | bigint | 平台分组 ID（区分 PC/APP/小游戏） |
| channel_id | bigint | 渠道号 |

**渠道分类说明：**
- 本表不直接记录渠道分类，需通过关联 `tcy_temp.dws_channel_category_map` 获取
- 渠道分类标签：`1`=官方，`2`=渠道，`3`=小游戏

## 注意事项

1. `first_login_ts` 为 Unix 时间戳格式（秒级）
2. `dt` 字段为分区字段，格式为 YYYYMMDD（int 类型）
3. 查询时需使用 `app_id` 字段进行过滤（如：`app_id = 1880053`）
4. 查询时间段时使用 `dt` 字段，注意 `dt` 为 int 类型的日期（如：20260210）
