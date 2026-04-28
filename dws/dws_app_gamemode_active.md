# DWS 中间表：APP 端每日游戏活跃用户×玩法表

## 表基本信息

| 项目 | 说明 |
| ------ | ------ |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_gamemode_active` |
| 全名 | `tcy_temp.dws_app_gamemode_active` |
| 类型 | DWS 层聚合表（一次性创建） |
| 描述 | APP 端每日按玩法活跃用户去重清单，**专用于"同玩法留存"flag 计算** |
| 粒度 | uid × dt × app_id × game_mode（一个用户一天一个应用一种玩法一行） |

## 与 `dws_app_game_active` 的区别

| 表 | 用途 |
| ---- | ------ |
| `dws_app_game_active` | 整体留存（任意玩法有对局即算留存） |
| `dws_app_gamemode_active` | 同玩法留存（需在同一玩法有对局才算该玩法留存） |

## 字段说明

| 字段名 | 类型 | 说明 | 礷例值 |
| -------- | ------ | ------ | -------- |
| app_id | int | 应用 ID | 1880053 |
| uid | int | 玩家唯一标识 | 123456789 |
| play_mode | tinyint | 玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房，0=其他 | 1 |
| dt | date | 对局日期 | 2026-02-15 |

## 构建 SQL

```sql
-- 时间范围覆盖注册期 + Day30 观测期（20260210 ~ 20260508）
CREATE TABLE tcy_temp.dws_app_gamemode_active (
  `app_id` INT NOT NULL COMMENT "应用ID",
  `uid` INT NOT NULL COMMENT "用户ID",
  `play_mode` TINYINT NOT NULL COMMENT "游戏玩法模式",
  `dt` DATE NOT NULL COMMENT "日期"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `play_mode`, `dt`)
COMMENT "玩家玩法活跃明细表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "colocate_with" = "group_daily_data", -- 依然入组，保证 JOIN 性能
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);
```

## 初始化数据SQL

```sql
INSERT INTO tcy_temp.dws_app_gamemode_active
SELECT app_id, uid, play_mode, date(dt)
FROM tcy_temp.dws_ddz_daily_game
WHERE app_id = 1880053
  AND dt BETWEEN '2026-02-10' AND '2026-04-21'
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY 1,2,3,4;
```

## 表依赖关系

```text
tcy_temp.dws_dq_app_daily_reg                 （APP 端注册用户宽表）
            ↓  LEFT JOIN uid + app_id，dt > reg_date，game_mode = target_mode
tcy_temp.dws_app_gamemode_active              （每日游戏活跃用户×玩法表，同玩法留存 flag 专用）  ← 本表
            ↓  用于计算"同玩法留存 flag"
tcy_temp.ddz_gamemode_firstday_features     （分玩法分析宽表）
```

> **文档版本**：v2.0
> **更新说明**：

>
> - v1.0：初始版本（原名 `dws_ddz_daily_play_by_mode`）
> - v1.1：优化 Bucket 配置（32→64）；添加排序键（`ORDER BY dt, uid, game_mode`）
> - **v2.0**：重命名为 `dws_app_gamemode_active`；新增 `app_id` 字段；更新与配对表的对比说明
