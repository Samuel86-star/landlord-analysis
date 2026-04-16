# DWS 中间表：APP 端每日游戏活跃用户×玩法表

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_temp` |
| 表名 | `dws_app_gamemode_active` |
| 全名 | `tcy_temp.dws_app_gamemode_active` |
| 类型 | DWS 层聚合表（一次性创建） |
| 描述 | APP 端每日按玩法活跃用户去重清单，**专用于"同玩法留存"flag 计算** |
| 粒度 | uid × dt × app_id × game_mode（一个用户一天一个应用一种玩法一行） |

## 与 `dws_app_game_active` 的区别

| 表 | 用途 |
|----|------|
| `dws_app_game_active` | 整体留存（任意玩法有对局即算留存）|
| `dws_app_gamemode_active` | 同玩法留存（需在同一玩法有对局才算该玩法留存）|

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
|--------|------|------|--------|
| uid | bigint | 玩家唯一标识 | 123456789 |
| dt | int | 对局日期（YYYYMMDD int 格式） | 20260215 |
| app_id | bigint | 应用 ID | 1880053 |
| play_mode | int | 玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房，0=其他 | 1 |

## 构建 SQL

```sql
-- 时间范围覆盖注册期 + Day30 观测期（20260210 ~ 20260508）
CREATE TABLE tcy_temp.dws_app_gamemode_active
AS
SELECT uid, dt, app_id, play_mode
FROM tcy_temp.dws_ddz_daily_game
WHERE dt BETWEEN 20260210 AND 20260508
  AND game_id = 53
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY uid, dt, app_id, play_mode;
```

## 与其他 DWS 表的关系

```
tcy_temp.dws_dq_app_daily_reg                 （APP 端注册用户宽表）
            ↓  LEFT JOIN uid + app_id，dt > reg_date，game_mode = target_mode
tcy_temp.dws_app_gamemode_active              （每日游戏活跃用户×玩法表，同玩法留存 flag 专用）  ← 本表
            ↓  用于计算"同玩法留存 flag"
tcy_temp.ddz_gamemode_firstday_features     （分玩法分析宽表）
```

> **文档版本**：v2.0
> **更新说明**：
> - v1.0：初始版本（原名 `dws_ddz_daily_play_by_mode`）
> - v1.1：优化 Bucket 配置（32→64）；添加排序键（`ORDER BY dt, uid, game_mode`）
> - **v2.0**：重命名为 `dws_app_gamemode_active`；新增 `app_id` 字段；更新与配对表的对比说明
