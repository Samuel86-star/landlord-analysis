# 游戏用户注册表说明

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `hive_catalog_cdh5.dm` |
| 表名 | `olap_tcy_userapp_d_p_login1st` |
| 全名 | `hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st` |
| 类型 | 原始数据表 |
| 描述 | 游戏用户首次注册登录信息表 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
| ----- | ---- | ---- | ------ | ------- |
| uid | bigint | 玩家唯一标识 ID | 123456789 | 是 |
| app_id | bigint | 应用 ID | 1880053 | 是 |
| first_login_ts | bigint | 游戏注册时间戳 | 1707523200 | 是 |
| dt | int | 注册日期（格式：YYYYMMDD） | 20260210 | 是 |

## 注意事项

1. `first_login_ts` 为 Unix 时间戳格式（毫秒级）
2. `dt` 字段为分区字段，格式为 YYYYMMDD（int 类型）
3. 查询时需使用 `app_id` 字段进行过滤（如：`app_id = 1880053`）
4. 查询时间段时使用 `dt` 字段，注意 `dt` 为 int 类型的日期（如：20260210）
