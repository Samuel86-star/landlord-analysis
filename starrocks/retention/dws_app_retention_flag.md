# DWS 中间表：APP 端首日留存 flag 表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_retention_flag` |
| 全名 | `tcy_temp.dws_app_retention_flag` |
| 类型 | DWS 层 flag 表（按到期日逐步回填） |
| 描述 | APP 端注册用户首日留存 flag 表，记录 D+1/D+3/D+7/D+14/D+30 是否游戏/登录留存 |
| 粒度 | uid × reg_date（一个用户一行） |

## 设计背景

留存 flag 是"按到期日逐步回填"的——D+1 在 reg_date+1 才能确定，D+30 要等到 reg_date+30。这种"随时间推进逐步刷新"的更新节奏，跟"注册当日写一次"的首日游戏指标语义完全不同。把两者混在一张表里会导致：

- 想刷 flag 必须整行重写（StarRocks DUPLICATE KEY 表不支持高效 UPDATE 单列）
- 浪费写 IO（首日指标根本没变）
- 优化器超时（首日宽表 + flag 计算混在一条 SQL，CTE 嵌套太深）

**解决方案**：把留存 flag 拆出来独立成本表，专门承载"按到期日刷新"的语义。首日游戏指标走 [`dws_app_firstday_game_stat`](dws_app_firstday_game_stat.md)，分析时两表 LEFT JOIN。

## 字段说明

| 字段名 | 类型 | 说明 | 取值 |
| ------ | ---- | ---- | ---- |
| app_id | int | 应用 ID | 1880053 |
| reg_date | date | 注册日期 | 2026-05-14 |
| uid | int | 玩家唯一标识 | 123456789 |
| d1_game | tinyint | D+1 游戏留存（注册次日是否有对局） | NULL/0/1 |
| d3_game | tinyint | D+3 游戏留存 | NULL/0/1 |
| d7_game | tinyint | D+7 游戏留存（第 7 日是否有对局） | NULL/0/1 |
| d14_game | tinyint | D+14 游戏留存 | NULL/0/1 |
| d30_game | tinyint | D+30 游戏留存（第 30 日是否有对局） | NULL/0/1 |
| d1_login | tinyint | D+1 登录留存 | NULL/0/1 |
| d3_login | tinyint | D+3 登录留存 | NULL/0/1 |
| d7_login | tinyint | D+7 登录留存 | NULL/0/1 |
| d14_login | tinyint | D+14 登录留存 | NULL/0/1 |
| d30_login | tinyint | D+30 登录留存 | NULL/0/1 |

### NULL/0/1 三态语义

| 取值 | 含义 |
| ---- | ---- |
| **NULL** | 该里程碑**未到期**（reg_date+N > 当天）。例：今天是 2026-06-18，对 2026-06-15 注册的用户，d7 (reg_date+7=2026-06-22) 还未到期，记 NULL。 |
| **0** | 已到期但**未留存**（到期日没有对局/登录）。 |
| **1** | 已到期且**已留存**。 |

> **分析提示**：留存率计算时分母只统计已到期用户。例：`SUM(d7_game) / COUNT(d7_game)` 自动忽略 NULL 行（COUNT(d7_game) 不计 NULL），等价于"在已到期用户里算 d7 留存率"。

## 留存日期口径

Day 0 = 注册日，里程碑日期为 reg_date + N：

| 里程碑 | 对应日期 | 含义 |
| ---- | ---- | ---- |
| D+1（次留） | reg_date + 1 | 注册日次日是否有对局/登录 |
| D+3 | reg_date + 3 | 第 3 日（即 reg_date 之后第 3 天） |
| D+7（7 日留存） | reg_date + 7 | 第 7 日 |
| D+14 | reg_date + 14 | 第 14 日 |
| D+30（30 日留存） | reg_date + 30 | 第 30 日 |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_app_retention_flag (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `reg_date` date NOT NULL COMMENT "注册日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `d1_game` tinyint(4) NULL COMMENT "D+1 游戏留存（注册日次日是否有对局）",
  `d3_game` tinyint(4) NULL COMMENT "D+3 游戏留存",
  `d7_game` tinyint(4) NULL COMMENT "D+7 游戏留存（第7日是否有对局）",
  `d14_game` tinyint(4) NULL COMMENT "D+14 游戏留存",
  `d30_game` tinyint(4) NULL COMMENT "D+30 游戏留存（第30日是否有对局）",
  `d1_login` tinyint(4) NULL COMMENT "D+1 登录留存",
  `d3_login` tinyint(4) NULL COMMENT "D+3 登录留存",
  `d7_login` tinyint(4) NULL COMMENT "D+7 登录留存",
  `d14_login` tinyint(4) NULL COMMENT "D+14 登录留存",
  `d30_login` tinyint(4) NULL COMMENT "D+30 登录留存"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `reg_date`, `uid`)
COMMENT "APP端首日留存flag表（按reg_date分区，逐步回填d1/d3/d7/d14/d30）"
PARTITION BY RANGE(`reg_date`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-365",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "365",
    "colocate_with" = "group_daily_data"
);
```

### 增量数据导入

按 reg_date `DELETE + INSERT`（幂等可重跑），脚本：[`batch_insert_retention_flag.py`](../../ops/py/batch_insert_retention_flag.py)

> **依赖**：`dws_dq_app_daily_reg`、`dws_app_game_active`、`dws_dq_daily_login` 在 reg_date 当天和 reg_date+N 天的数据需已回填。

```powershell
# 单天
py -3 -u .\batch_insert_retention_flag.py --start 20260514 --end 20260514

# 区间回填（推荐通过 daily_retention 调度）
py -3 -u .\batch_insert_retention_flag.py --start 2026-05-14 --end 2026-06-17

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_retention_flag.py --start 20260514 --end 20260514 --dry-run
```

```sql
-- 单天 reg_date 的 INSERT 模板（${DT} 替换为 reg_date）
INSERT INTO tcy_temp.dws_app_retention_flag
SELECT
    r.app_id, r.reg_date, r.uid,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 1 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt = DATE_ADD('${DT}', INTERVAL 1 DAY)  THEN 1 END), 0) ELSE NULL END AS d1_game,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 3 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt = DATE_ADD('${DT}', INTERVAL 3 DAY)  THEN 1 END), 0) ELSE NULL END AS d3_game,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 7 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt = DATE_ADD('${DT}', INTERVAL 7 DAY)  THEN 1 END), 0) ELSE NULL END AS d7_game,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 14 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt = DATE_ADD('${DT}', INTERVAL 14 DAY) THEN 1 END), 0) ELSE NULL END AS d14_game,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 30 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN a.dt = DATE_ADD('${DT}', INTERVAL 30 DAY) THEN 1 END), 0) ELSE NULL END AS d30_game,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 1 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('${DT}', INTERVAL 1 DAY)  THEN 1 END), 0) ELSE NULL END AS d1_login,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 3 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('${DT}', INTERVAL 3 DAY)  THEN 1 END), 0) ELSE NULL END AS d3_login,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 7 DAY)  <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('${DT}', INTERVAL 7 DAY)  THEN 1 END), 0) ELSE NULL END AS d7_login,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 14 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('${DT}', INTERVAL 14 DAY) THEN 1 END), 0) ELSE NULL END AS d14_login,
    CASE WHEN DATE_ADD('${DT}', INTERVAL 30 DAY) <= CURRENT_DATE THEN COALESCE(MAX(CASE WHEN l.login_date = DATE_ADD('${DT}', INTERVAL 30 DAY) THEN 1 END), 0) ELSE NULL END AS d30_login
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_game_active a
    ON a.app_id = r.app_id AND a.uid = r.uid
   AND a.dt IN (
       DATE_ADD('${DT}', INTERVAL 1 DAY),
       DATE_ADD('${DT}', INTERVAL 3 DAY),
       DATE_ADD('${DT}', INTERVAL 7 DAY),
       DATE_ADD('${DT}', INTERVAL 14 DAY),
       DATE_ADD('${DT}', INTERVAL 30 DAY)
   )
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
   AND l.login_date IN (
       DATE_ADD('${DT}', INTERVAL 1 DAY),
       DATE_ADD('${DT}', INTERVAL 3 DAY),
       DATE_ADD('${DT}', INTERVAL 7 DAY),
       DATE_ADD('${DT}', INTERVAL 14 DAY),
       DATE_ADD('${DT}', INTERVAL 30 DAY)
   )
WHERE r.app_id = 1880053 AND r.reg_date = '${DT}'
GROUP BY r.app_id, r.reg_date, r.uid;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../ops/daily_data_ops.md)

## 使用示例

### 1. 计算各注册日的 d1/d7/d30 留存率

```sql
SELECT
    reg_date,
    COUNT(*) AS total_users,
    -- COUNT(d1_game) 自动忽略 NULL，等价于"已到期用户数"
    ROUND(SUM(d1_game) * 100.0 / COUNT(d1_game), 2) AS d1_game_rate,
    ROUND(SUM(d7_game) * 100.0 / COUNT(d7_game), 2) AS d7_game_rate,
    ROUND(SUM(d30_game) * 100.0 / COUNT(d30_game), 2) AS d30_game_rate
FROM tcy_temp.dws_app_retention_flag
WHERE app_id = 1880053
  AND reg_date BETWEEN '2026-05-01' AND '2026-05-31'
GROUP BY reg_date
ORDER BY reg_date;
```

### 2. 联合首日游戏指标分析（首日胜率 × d7 留存）

```sql
SELECT
    CASE
        WHEN g.silver_win_rate < 30 THEN 'A: <30%'
        WHEN g.silver_win_rate < 50 THEN 'B: 30-50%'
        WHEN g.silver_win_rate < 70 THEN 'C: 50-70%'
        ELSE 'D: >=70%'
    END AS win_rate_group,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d1_game) * 100.0 / COUNT(rf.d1_game), 2) AS d1_rate,
    ROUND(SUM(rf.d7_game) * 100.0 / COUNT(rf.d7_game), 2) AS d7_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND g.silver_game_count > 0
GROUP BY win_rate_group
ORDER BY win_rate_group;
```

### 3. 留存漏斗（d1 → d7 → d30 衰减）

```sql
SELECT
    reg_date,
    COUNT(*) AS total_users,
    SUM(d1_game) AS d1_retained,
    SUM(d7_game) AS d7_retained,
    SUM(d30_game) AS d30_retained,
    -- d7 占 d1 的比例（仅看 d1 已留存的人，d7 是否还在）
    ROUND(SUM(CASE WHEN d1_game = 1 THEN d7_game END) * 100.0 / NULLIF(SUM(d1_game), 0), 2) AS d7_in_d1_rate
FROM tcy_temp.dws_app_retention_flag
WHERE app_id = 1880053
  AND reg_date = '2026-05-14'
GROUP BY reg_date;
```

## 字段使用注意

1. **NULL 不是缺数据**：未到期里程碑就该是 NULL，不要错算成"流失"。计算留存率时用 `SUM(d_n) / COUNT(d_n)`（COUNT 自动忽略 NULL）。
2. **回扫策略**：retention_flag 表的依赖（game_active / daily_login）按天分区不会过期，所以可以随时回扫——`daily_retention` 调度器默认回扫 35 天 reg_date 重算所有未到期 flag 的最新值。
3. **跟 firstday_game_stat 的关系**：本表只放 flag，不放注册信息和首日指标。分析时 `firstday_game_stat LEFT JOIN retention_flag` 拿到完整画像。
4. **D+N 语义统一**：D+N 指 reg_date 之后第 N 天（如 D+1 = reg_date+1，跟 retention-analysis-framework.md 一致）。

## 表数据流向

```text
tcy_temp.dws_dq_app_daily_reg     （APP 端注册用户）
tcy_temp.dws_app_game_active      （reg_date+N 天的游戏活跃）
tcy_temp.dws_dq_daily_login       （reg_date+N 天的登录）
            ↓  按 reg_date 聚合到 flag
tcy_temp.dws_app_retention_flag   （本表，留存 flag NULL/0/1）
            ↓  LEFT JOIN
tcy_temp.dws_app_firstday_game_stat  （首日游戏指标）
            ↓  联合分析
留存归因报表（首日指标 × 留存 flag）
```

> **文档版本**：v1.0
> **创建时间**：2026-06-18
> **设计来源**：[analysis/plan/retention/20260615/discussion-log.md](../../docs/analysis/plan/retention/20260615/discussion-log.md) 讨论的"首日指标 + 留存 flag"拆分方案
