# DWS 中间表：APP 端每日游戏活跃用户表

## 表基本信息

| 项目 | 说明 |
| ------ | ------ |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_game_active` |
| 全名 | `tcy_temp.dws_app_game_active` |
| 类型 | DWS 层聚合表（一次性创建，**T-1 可用**） |
| 描述 | APP 端每日有对局的用户去重清单，**专用于留存 flag 计算** |
| 粒度 | uid × dt × app_id（一个用户一天一个应用一行） |
| 数据延迟 | **T-1**：依赖 `dws_crazyddz_daily_game`，上游 raw 层已通过 min_dt 机制处理跨天对局，T 日活跃数据在 T+1 日可产出 |

## 设计背景

留存计算的本质是：判断用户在注册后特定天数（Day1/Day3/Day7/Day14/Day30）是否再次有对局。

如果直接对原始 `dws_ddz_daily_game` 和 `dws_crazyddz_daily_game`（日志级别，数亿行）每次都做 JOIN 计算，在 StarRocks 中性能极差。

**解决方案**：将每日有对局的用户提前聚合到 `uid × dt × app_id` 粒度（数百万行），后续留存计算只需在该轻量表上做 JOIN，大幅提升查询性能。

> **活跃口径**：只要在经典斗地主（`dws_ddz_daily_game`）或疯狂斗地主（`dws_crazyddz_daily_game`）任一存在对局，即算当日游戏活跃。

## 与统计类姊妹表的定位区别

| 表 | 用途 | 粒度 | 列数 |
| ---- | ------ | ------ | ------ |
| `dws_app_game_active` | 留存 flag 计算（当日是否有对局） | uid × dt × app_id | 3 列 |
| `dws_app_silvergame_stat` | 银子玩法金流 + 参与度统计 | uid × dt | 19 列 |
| `dws_app_scoregame_stat` | 积分玩法参与度 + 胜负统计（无金流） | uid × dt | 14 列 |
| `dws_app_allgame_stat` | 全玩法体验分析（倍数/炸弹/胜率等） | uid × dt × play_mode | 40 列 |

留存计算需对**注册期 + 30 天**全量数据做高频 LEFT JOIN，`dws_app_game_active` 以极少列数保证 JOIN 性能。三张统计类姊妹表各有定位——银子表专注金流归因、积分表专注参与度与胜负、全玩法表控制玩法变量做体验分析——均不适合承担高频留存 flag JOIN 的职责。

## 时间范围说明

| 注册时间范围 | 最大留存观察期 | 表需覆盖到 |
| ------------ | ------------- | ----------- |
| 20260210 ~ 20260408 | Day30 | 20260408 + 30天 = **20260508** |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| -------- | ------ | ------ | -------- |
| app_id | int | 应用 ID | 1880053 |
| uid | int | 玩家唯一标识 | 123456789 |
| dt | date | 对局日期 | 2026-02-15 |

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
    "dynamic_partition.start" = "-120",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "120",
    "colocate_with" = "group_daily_data"
);
```

## 初始化数据

按天 `DELETE + INSERT`（幂等可重跑），脚本：[`batch_insert_app_game_active.py`](../../py/batch_insert_app_game_active.py)

> **依赖**：dws_ddz_daily_game、dws_crazyddz_daily_game 对应日期需先回填。

```powershell
# 单天
py -3 -u .\batch_insert_app_game_active.py --start 20260617 --end 20260617

# 区间回填
py -3 -u .\batch_insert_app_game_active.py --start 2026-06-01 --end 2026-06-08

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_app_game_active.py --start 20260617 --end 20260617 --dry-run
```


> **说明**：活跃用户 = 经典斗地主活跃用户 ∪ 疯狂斗地主活跃用户（去重）

```sql
-- 初始化数据（经典斗地主 + 疯狂斗地主去重合并）
INSERT INTO tcy_temp.dws_app_game_active
SELECT app_id, uid, dt
FROM (
    -- 经典斗地主
    SELECT app_id, uid, DATE(dt) AS dt
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53
      AND dt BETWEEN '2026-03-01' AND '2026-06-01'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    UNION ALL
    -- 疯狂斗地主（game_id = 521）
    SELECT app_id, uid, dt
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521
      AND dt BETWEEN '2026-03-01' AND '2026-06-01'
      AND app_id = 1880053
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
) t
GROUP BY app_id, uid, dt;
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
>
> - 次留：Day 0 注册的用户在次日（+1）登录的占比 → `reg_date + 1`
> - 7 留："第 7 天留存"，把注册日记为第 1 天 → `reg_date + 6`
> - 30 留：同理，第 30 天 → `reg_date + 29`
>
> **StarRocks 日期运算说明**：`dt` 为 DATE 类型，可使用 `DATE_ADD(dt, INTERVAL 1 DAY)` 等日期函数计算留存偏移天数，也可使用 `dt = r.reg_date + INTERVAL 1 DAY` 进行比较。

## 表依赖关系

```text
tcy_temp.dws_ddz_daily_game           （经典斗地主对局明细表）
tcy_temp.dws_crazyddz_daily_game      （疯狂斗地主对局明细表）
            ↓  UNION 去重聚合到 uid × dt × app_id
tcy_temp.dws_app_game_active          （每日游戏活跃用户表，留存 flag 专用）  ← 本表
            ↑  LEFT JOIN uid + app_id，dt > reg_date
tcy_temp.dws_dq_app_daily_reg         （APP 端注册用户宽表）
```

> **文档版本**：v1.0
> **创建时间**：2026-06-11
> **更新说明**：
>
> - v1.0：初始版本
