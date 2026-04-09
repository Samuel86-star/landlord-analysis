# 玩家登录日志表说明

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_dwd` |
| 表名 | `dwd_tcy_userlogin_si` |
| 全名 | `tcy_dwd.dwd_tcy_userlogin_si` |
| 类型 | 原始数据表 |
| 描述 | 玩家登录日志信息表 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
|-------|------|------|--------|---------|
| log_id | string | 日志唯一 ID | "abc123xyz" | 是 |
| dt | datetime | 登录时间 | 2026-04-08 10:30:00 | 是 |
| time_unix | bigint | 登录时间戳 | 1712577000 | 是 |
| uid | bigint | 玩家唯一标识 ID | 123456789 | 是 |
| client_ipv4 | string | 玩家登录 IP | "192.168.1.1" | 否 |
| app_id | bigint | 应用 ID | 1880053 | 是 |
| app_vers | string | 应用版本 | "1.0.0" | 否 |
| channel_id | bigint | 渠道号 | 1001 | 是 |
| group_id | bigint | 分端 ID（区分 PC/APP/小游戏） | 6 | 是 |

## 全局字段说明

以下全局字段在本表中同样适用，详细说明可参考 [README-data.md](../README-data.md)：

| 字段名 | 类型 | 说明 |
|--------|------|------|
| uid | bigint | 玩家唯一标识 ID |
| app_id | bigint | 应用 ID（1880053 = 斗地主游戏应用） |
| group_id | bigint | 分端 ID（区分 PC/APP/小游戏） |
| channel_id | bigint | 渠道号 |

**分端说明 (group_id)：**
- `PC 端`：`group_id not in (6,66,8,88,55,69,0,56,68,33,44,77,99)`
- `APP 端`：`group_id in (6,66,33,44,77,99)` 为安卓，`group_id in (8,88)` 为 iOS
- `小游戏`：`group_id = 56`

**渠道分类说明：**
- 本表仅记录 `channel_id`（渠道号），不包含渠道分类信息
- 需关联 `tcy_temp.dws_channel_category_map` 表获取渠道分类
- 渠道分类标签：`1`=官方，`2`=渠道，`3`=小游戏

## 注意事项

1. `log_id` 为日志的唯一标识
2. `dt` 为 datetime 类型，包含完整的日期和时间信息（如：2026-04-08 10:30:00）
3. `time_unix` 为 Unix 时间戳（秒级）
4. `client_ipv4` 记录玩家登录时的 IP 地址
5. 查询时需使用 `app_id` 字段进行过滤（如：`app_id = 1880053`）
6. 查询时间段时使用 `dt` 字段，注意 `dt` 为 datetime 类型
