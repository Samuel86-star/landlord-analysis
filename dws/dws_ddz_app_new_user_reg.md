# DWS 中间表：新增用户基础信息表

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_app_new_user_reg` |
| 全名 | `tcy_temp.dws_ddz_app_new_user_reg` |
| 类型 | DWS 层宽表（一次性创建） |
| 描述 | APP 端新增用户基础信息宽表，**含注册当日有对局**的用户（即游戏留存的分母口径） |
| 粒度 | uid（一个用户一行） |

## 设计背景与关键决策

### 为什么不直接用 `olap_tcy_userapp_d_p_login1st`？

`olap_tcy_userapp_d_p_login1st` 仅包含 `uid, app_id, first_login_ts, dt`，**没有** `group_id` 和 `channel_id` 字段，因此无法直接区分分端（APP/PC/小游戏）和渠道分类。

**解决方案**：用注册当日的战绩记录（`dwd_game_combat_si`）来获取 `group_id` 和 `channel_id`：
- INNER JOIN 确保只保留"注册当日有对局"的用户，天然对齐游戏留存口径
- 以当日出现次数最多的 `(group_id, channel_id)` 组合作为该用户的归因

### APP 端过滤规则

| device_type | group_id 范围 |
|-------------|---------------|
| Android | 6, 66, 33, 44, 77, 99 |
| iOS | 8, 88 |

排除房间：`room_id NOT IN (11534, 14238, 15458)`（积分场/比赛场，币种不同，不纳入分析）

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
|--------|------|------|--------|
| uid | bigint | 玩家唯一标识 | 123456789 |
| reg_date | int | 注册日期（YYYYMMDD int 格式） | 20260210 |
| group_id | bigint | 分端 ID（注册当日最频繁出现的 group_id） | 6 |
| device_type | string | 设备类型：'iOS' / 'Android' | 'Android' |
| channel_id | bigint | 渠道号（注册当日最频繁出现的 channel_id） | 1001 |
| channel_category | string | 渠道分类名称：'官方' / '渠道' / '小游戏' | '官方' |
| channel_category_tag_id | int | 渠道分类标签：1=官方，2=渠道，3=小游戏 | 1 |
| first_day_game_cnt | int | 注册当日对局场次 | 12 |

## 构建 SQL

```sql
-- Step 1：构建 APP 端新增用户基础信息宽表
-- 前置依赖：tcy_temp.dws_channel_category_map 已存在
CREATE TABLE tcy_temp.dws_ddz_app_new_user_reg
DISTRIBUTED BY HASH(uid) BUCKETS 16
PROPERTIES("replication_num" = "1")
AS
WITH
-- 1. 注册用户清单（仅 app_id 过滤，不做分端过滤，因为该表无 group_id）
new_user_base AS (
    SELECT uid, dt AS reg_date
    FROM hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st
    WHERE app_id = 1880053
      AND dt BETWEEN 20260210 AND 20260408
),
-- 2. 从战绩表中提取注册当日的 group_id/channel_id，并统计各组合出现次数
first_day_dims AS (
    SELECT
        c.uid,
        c.group_id,
        c.channel_id,
        COUNT(*) AS game_cnt
    FROM tcy_dwd.dwd_game_combat_si c
    INNER JOIN new_user_base n ON c.uid = n.uid AND c.dt = n.reg_date
    WHERE c.game_id = 53
      AND c.robot != 1
      AND c.group_id IN (6, 66, 8, 88, 33, 44, 77, 99) -- 仅保留 APP 端
      AND c.room_id NOT IN (11534, 14238, 15458)        -- 排除积分场/比赛场
    GROUP BY c.uid, c.group_id, c.channel_id
),
-- 3. 每个用户取对局次数最多的那条 (group_id, channel_id) 组合（去重）
dims_dedup AS (
    SELECT uid, group_id, channel_id, game_cnt
    FROM first_day_dims
    QUALIFY ROW_NUMBER() OVER (PARTITION BY uid ORDER BY game_cnt DESC) = 1
),
-- 4. 计算用户注册当日总对局数
first_day_total AS (
    SELECT uid, SUM(game_cnt) AS first_day_game_cnt
    FROM first_day_dims
    GROUP BY uid
)
SELECT
    n.uid,
    n.reg_date,
    d.group_id,
    CASE WHEN d.group_id IN (8, 88) THEN 'iOS' ELSE 'Android' END AS device_type,
    d.channel_id,
    COALESCE(chn.channel_category_name, '未知')       AS channel_category,
    COALESCE(chn.channel_category_tag_id, -1)         AS channel_category_tag_id,
    t.first_day_game_cnt
FROM new_user_base n
INNER JOIN dims_dedup d       ON n.uid = d.uid    -- INNER JOIN：只保留注册当日有 APP 对局的用户
LEFT JOIN  tcy_temp.dws_channel_category_map chn ON d.channel_id = chn.channel_id
LEFT JOIN  first_day_total t  ON n.uid = t.uid;
```

## 使用说明

- **游戏留存分母**：`COUNT(DISTINCT uid)` 即为当日各维度的留存分母
- **分端筛选**：通过 `device_type = 'iOS'` 或 `device_type = 'Android'` 过滤
- **渠道分析**：通过 `channel_category` 区分官方 / 渠道 / 小游戏

## 与其他 DWS 表的关系

```
tcy_temp.dws_ddz_app_new_user_reg        （新用户基础信息，一行 = 一个新用户）
            ↓  JOIN uid
tcy_temp.dws_ddz_app_daily_active        （每日活跃，一行 = uid × dt）
            ↓  用于计算留存 flag
tcy_temp.dws_ddz_new_user_first_day_features  （首日行为宽表，最终分析数据集）
```
