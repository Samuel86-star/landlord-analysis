# DWS 中间表：APP 端首日游戏指标宽表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_firstday_game_stat` |
| 全名 | `tcy_temp.dws_app_firstday_game_stat` |
| 类型 | DWS 层宽表（注册当日写一次，写入后不变） |
| 描述 | APP 端新增用户首日游戏指标宽表，注册信息 + 首日银子/积分/全玩法体验，固化为一张用户级表 |
| 粒度 | uid × reg_date（一个用户一行） |
| 数据延迟 | T+1 可用（依赖 reg_date 当天的 silver/score/allgame stat） |

## 设计背景

留存归因分析的核心是"首日做了什么 → 后续是否留下"。把"做了什么"的首日指标和"是否留下"的留存 flag **拆成两张表**：

- **本表** `dws_app_firstday_game_stat`：注册信息 + 首日游戏指标（银子金流、积分参与度、全玩法体验等），**注册当日写入一次后不再变**。
- 姊妹表 [`dws_app_retention_flag`](dws_app_retention_flag.md)：D+1/D+3/D+7/D+14/D+30 留存 flag，**按到期日逐步回填**。

分析时两表 LEFT JOIN，得到完整画像。这种拆分解决了三个问题：

1. **更新节奏对齐**：首日指标写一次不动 vs flag 按到期日回填，混在一起会导致整行重写浪费 IO。
2. **planner 友好**：以前混在一起的 INSERT 是 `6 表 LEFT JOIN + 2 个 CTE`，复杂到触发 StarRocks 优化器超时（`new_planner_optimize_timeout`）。拆开后本表只剩 3 表 LEFT JOIN，flag 表只有 2 表 LEFT JOIN，planner 几十毫秒搞定。
3. **职责清晰**：本表只看"注册当天发生了什么"，flag 表只看"之后还来不来"，分析时按需 JOIN。

### 与上游表的关系

| 上游表 | 本表对应字段（前缀） |
| ---- | ---- |
| `dws_dq_app_daily_reg` | 注册信息（reg_*、channel_*、first_day_login_cnt） |
| `dws_app_silvergame_stat` | `silver_*` 系列（13 个字段） |
| `dws_app_scoregame_stat` | `score_*` 系列（6 个字段） |
| `dws_app_daily_allgame_stat` | `allgame_*` 系列 + `multi_*` 倍数段（26 个字段） |

### 为什么用 `daily_allgame_stat` 而不是 `allgame_stat`

`dws_app_allgame_stat` 粒度是 uid × dt × play_mode，一个用户一天玩 N 个玩法就 N 行。本表粒度是 uid × reg_date，需要先把 allgame_stat 按 uid×dt 聚合（SUM 跨玩法）。这一步降维由 [`dws_app_daily_allgame_stat`](../game/dws_app_daily_allgame_stat.md) 完成。

## 字段说明

### 用户属性（来自 dws_dq_app_daily_reg，写入后不变）

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| reg_date | date | 注册日期 | 2026-05-14 |
| uid | int | 玩家唯一标识 | 123456789 |
| reg_channel_id | int | 首次登录渠道号 | 1001 |
| reg_group_id | int | 首次登录分端 ID | 6 |
| reg_app_code | varchar(32) | 首次登录应用 code | zgda |
| channel_category_id | int | 渠道分类 ID | 1 |
| channel_category_name | varchar(255) | 渠道分类名称 | 官方 |
| channel_category_tag_id | tinyint | 渠道标签 ID（1=官方 2=渠道 3=小游戏） | 1 |
| first_day_login_cnt | int | 首日登录次数 | 5 |

### 首日行为指标 — 银子金流（来自 dws_app_silvergame_stat，无银子对局则为 NULL）

| 字段名 | 类型 | 说明 |
| ------ | ---- | ---- |
| silver_game_count | int | 首日银子对局总数 |
| silver_win_rate | double | 首日银子胜率（%） |
| silver_max_lose_streak | int | 首日银子最大连败 |
| silver_max_win_streak | int | 首日银子最大连胜 |
| silver_total_diff_money | bigint | 首日银子净输赢 |
| silver_money_valley | bigint | 首日银子谷值 |
| silver_money_peak | bigint | 首日银子峰值 |
| silver_start_money | bigint | 首局前银子余额 |
| silver_end_money | bigint | 末局后银子余额 |
| silver_total_fee_paid | int | 首日总服务费 |
| silver_escape_count | int | 首日银子逃跑次数 |
| silver_distinct_rooms | int | 首日游玩房间数 |
| silver_total_play_seconds | int | 首日银子总时长（秒） |

### 首日行为指标 — 积分玩法（来自 dws_app_scoregame_stat，无积分对局则为 NULL）

| 字段名 | 类型 | 说明 |
| ------ | ---- | ---- |
| score_game_count | int | 首日积分对局总数 |
| score_win_rate | double | 首日积分胜率（%） |
| score_max_lose_streak | int | 首日积分最大连败 |
| score_max_win_streak | int | 首日积分最大连胜 |
| score_escape_count | int | 首日积分逃跑次数 |
| score_total_play_seconds | int | 首日积分总时长（秒） |

### 首日行为指标 — 全玩法体验（来自 dws_app_daily_allgame_stat，无对局则为 NULL）

| 字段名 | 类型 | 说明 |
| ------ | ---- | ---- |
| allgame_distinct_modes | int | 首日游玩玩法种类数 |
| allgame_total_games | int | 首日全玩法总局数 |
| allgame_avg_magnification | double | 首日全玩法平均倍数 |
| allgame_max_magnification | int | 首日最高倍数 |
| allgame_bomb_count | int | 首日炸弹总数 |
| allgame_spring_count | int | 首日春天总数 |

### 首日固定倍数段统计（来自 dws_app_daily_allgame_stat 跨玩法 SUM 聚合）

| 字段名 | 类型 | 说明 |
| ------ | ---- | ---- |
| multi_1_win / multi_1_lose | int | 倍数=1 胜/负局数 |
| multi_2_win / multi_2_lose | int | 倍数=2 胜/负局数 |
| multi_3_6_win / multi_3_6_lose | int | 倍数 [3,6) 胜/负局数 |
| multi_6_12_win / multi_6_12_lose | int | 倍数 [6,12) 胜/负局数 |
| multi_12_24_win / multi_12_24_lose | int | 倍数 [12,24) 胜/负局数 |
| multi_24_48_win / multi_24_48_lose | int | 倍数 [24,48) 胜/负局数 |
| multi_48_96_win / multi_48_96_lose | int | 倍数 [48,96) 胜/负局数 |
| multi_96_192_win / multi_96_192_lose | int | 倍数 [96,192) 胜/负局数 |
| multi_192_384_win / multi_192_384_lose | int | 倍数 [192,384) 胜/负局数 |
| multi_384_plus_win / multi_384_plus_lose | int | 倍数 ≥384 胜/负局数 |

## 构建 SQL

### 增量数据导入

按 reg_date `DELETE + INSERT`（幂等可重跑），脚本：[`batch_insert_firstday_game_stat.py`](../../ops/py/batch_insert_firstday_game_stat.py)

> **依赖**：`dws_dq_app_daily_reg`、`dws_app_silvergame_stat`、`dws_app_scoregame_stat`、`dws_app_daily_allgame_stat` 在 reg_date 当天数据需已就位。

```powershell
# 单天
py -3 -u .\batch_insert_firstday_game_stat.py --start 20260514 --end 20260514

# 区间回填（推荐通过 daily_retention 调度）
py -3 -u .\batch_insert_firstday_game_stat.py --start 2026-05-14 --end 2026-06-17

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_firstday_game_stat.py --start 20260514 --end 20260514 --dry-run
```

```sql
-- 单天 reg_date 的 INSERT 模板（${DT} 替换为 reg_date）
INSERT INTO tcy_temp.dws_app_firstday_game_stat
SELECT
    r.app_id, r.reg_date, r.uid,
    r.reg_channel_id, r.reg_group_id, r.reg_app_code,
    r.channel_category_id, r.channel_category_name, r.channel_category_tag_id,
    r.first_day_login_cnt,
    si.game_count          AS silver_game_count,
    si.win_rate            AS silver_win_rate,
    si.max_lose_streak     AS silver_max_lose_streak,
    si.max_win_streak      AS silver_max_win_streak,
    si.total_diff_money    AS silver_total_diff_money,
    si.money_valley        AS silver_money_valley,
    si.money_peak          AS silver_money_peak,
    si.start_money         AS silver_start_money,
    si.end_money           AS silver_end_money,
    si.total_fee_paid      AS silver_total_fee_paid,
    si.escape_count        AS silver_escape_count,
    si.distinct_rooms      AS silver_distinct_rooms,
    si.total_play_seconds  AS silver_total_play_seconds,
    sc.game_count          AS score_game_count,
    sc.win_rate            AS score_win_rate,
    sc.max_lose_streak     AS score_max_lose_streak,
    sc.max_win_streak      AS score_max_win_streak,
    sc.escape_count        AS score_escape_count,
    sc.total_play_seconds  AS score_total_play_seconds,
    ag.distinct_modes      AS allgame_distinct_modes,
    ag.total_games         AS allgame_total_games,
    ag.avg_magnification   AS allgame_avg_magnification,
    ag.max_magnification   AS allgame_max_magnification,
    ag.bomb_count          AS allgame_bomb_count,
    ag.spring_count        AS allgame_spring_count,
    ag.multi_1_win, ag.multi_1_lose,
    ag.multi_2_win, ag.multi_2_lose,
    ag.multi_3_6_win, ag.multi_3_6_lose,
    ag.multi_6_12_win, ag.multi_6_12_lose,
    ag.multi_12_24_win, ag.multi_12_24_lose,
    ag.multi_24_48_win, ag.multi_24_48_lose,
    ag.multi_48_96_win, ag.multi_48_96_lose,
    ag.multi_96_192_win, ag.multi_96_192_lose,
    ag.multi_192_384_win, ag.multi_192_384_lose,
    ag.multi_384_plus_win, ag.multi_384_plus_lose
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_silvergame_stat si
    ON si.app_id = r.app_id AND si.uid = r.uid AND si.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_scoregame_stat sc
    ON sc.app_id = r.app_id AND sc.uid = r.uid AND sc.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_daily_allgame_stat ag
    ON ag.app_id = r.app_id AND ag.uid = r.uid AND ag.dt = r.reg_date
WHERE r.app_id = 1880053 AND r.reg_date = '${DT}';
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../ops/daily_data_ops.md)

## 使用示例

### 1. 首日胜率分组 × 留存（联合 retention_flag）

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
    ROUND(SUM(rf.d7_game) * 100.0 / COUNT(rf.d7_game), 2) AS d7_rate,
    ROUND(SUM(rf.d30_game) * 100.0 / COUNT(rf.d30_game), 2) AS d30_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND g.silver_game_count > 0
GROUP BY win_rate_group
ORDER BY win_rate_group;
```

### 2. 首日银子破产分析

```sql
SELECT
    CASE
        WHEN silver_money_valley < 0       THEN 'A: 亏光(负)'
        WHEN silver_money_valley < 5000    THEN 'B: 底部<5k'
        WHEN silver_money_valley < 10000   THEN 'C: 底部5k-1w'
        ELSE 'D: 安全>1w'
    END AS bottom_group,
    COUNT(*) AS user_count,
    ROUND(AVG(silver_total_diff_money), 0) AS avg_diff
FROM tcy_temp.dws_app_firstday_game_stat
WHERE app_id = 1880053
  AND reg_date BETWEEN '2026-05-01' AND '2026-05-31'
  AND silver_game_count > 0
GROUP BY bottom_group
ORDER BY bottom_group;
```

### 3. 高倍局体验 × 留存

```sql
SELECT
    CASE
        WHEN g.allgame_max_magnification < 12 THEN 'A: <12x'
        WHEN g.allgame_max_magnification < 24 THEN 'B: 12-24x'
        WHEN g.allgame_max_magnification < 48 THEN 'C: 24-48x'
        ELSE 'D: >=48x'
    END AS multi_group,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d7_game) * 100.0 / COUNT(rf.d7_game), 2) AS d7_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND g.allgame_total_games > 0
GROUP BY multi_group
ORDER BY multi_group;
```

## 字段使用注意

1. **NULL 语义**：silver_*/score_*/allgame_* 字段为 NULL 表示该用户当日没有对应玩法的对局（不是数据缺失）。
2. **跨币种不可加总**：silver_total_diff_money（银子）和 score_* 都是积分，跨字段加金额无意义。
3. **跟 retention_flag 配合**：本表不含 d1/d7/d30 等留存 flag，分析时 LEFT JOIN [`dws_app_retention_flag`](dws_app_retention_flag.md)。
4. **写入后不变**：注册日 reg_date 当天的数据写入后就固定了，daily_retention 35 天回扫时虽然会重写，但结果幂等无变化。

## 表数据流向

```text
tcy_temp.dws_dq_app_daily_reg          （APP 端注册用户）
tcy_temp.dws_app_silvergame_stat       （reg_date 当天银子金流）
tcy_temp.dws_app_scoregame_stat        （reg_date 当天积分参与度）
tcy_temp.dws_app_daily_allgame_stat    （reg_date 当天全玩法体验，已 uid×dt 降维）
            ↓  LEFT JOIN
tcy_temp.dws_app_firstday_game_stat    （本表，首日游戏指标）
            ↓  LEFT JOIN
tcy_temp.dws_app_retention_flag        （留存 flag，姊妹表）
            ↓  联合分析
留存归因报表（首日指标 × 留存 flag）
```

> **文档版本**：v2.0
> **变更**：
>
> - v2.0（2026-06-18）：从 `dws_app_firstday_retention` 重命名为 `dws_app_firstday_game_stat`，去掉 d1/d7/d30 留存 flag 字段（拆出独立表 `dws_app_retention_flag`）。原因：留存 flag 按到期日逐步回填，跟首日指标"写一次不变"的语义不同；混在一起导致整行重写浪费、CTE 嵌套触发优化器超时。详见 [analysis/plan/retention/20260615/discussion-log.md](../../docs/analysis/plan/retention/20260615/discussion-log.md) v2 设计记录。
> - v1.0（2026-06-15）：初始版本，注册信息 + 首日指标 + 留存 flag 一站式宽表。
