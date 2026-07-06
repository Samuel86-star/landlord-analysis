# DWS 中间表：四人斗地主每日对局战绩表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_srddz_daily_game` |
| 全名 | `tcy_temp.dws_srddz_daily_game` |
| 类型 | DWS 层中间表（每日增量，**T-1 可用**） |
| 描述 | 四人斗地主（game_id=105，斗地主内嵌银子玩法）对局战绩表，将 raw 层的「服务费 + 结算」双行合并为整局一行 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |
| 数据延迟 | **T-1**：上游 raw 层已通过 min_dt 机制回补跨天对局，T 日数据在 T+1 日可产出 |

## 设计背景

raw 层 `srddz_daily_game_raw` 中，同一玩家同一对局落 **2 行**——一行服务费（`result_id` 为空、`fee>0`）、一行结算（`result_id` 有值、`fee=0`、余额相接），详见 raw doc「双行结构说明」。本表将其合并为 1 行。

**与疯狂斗地主（521）dws 的关键区别**：521 是**多轮结算**（一局 N 个结算行，用 `ranked_combat` 累积 + 路径字段）；105 银子版是**单轮双行**（固定 1 服务费 + 1 结算），故本表用**条件聚合**（`result_id IS NULL` 取服务费行、`IS NOT NULL` 取结算行）直接合并，不需要 521 的多轮累积逻辑，也没有 `settle_count` / `*_path` 字段。

跨天对局已在 raw 层 min_dt 处理，本表直接 `WHERE dt = T` 即可。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_id | int | 游戏 ID | 105 |
| dt | date | 对局日期（raw 已归一到 resultguid 首现日） | 2026-07-02 |
| uid | int | 玩家 ID | 299119168 |
| resultguid | varchar(64) | 本局战绩 ID | "abc123xyz" |
| start_datetime | datetime | 服务费行时间（≈开局） | 2026-07-02 10:30:00 |
| end_datetime | datetime | 结算行时间（≈结束） | 2026-07-02 10:32:40 |
| time_cost | int | 对局耗时（秒，取自结算行） | 160 |
| room_id | int | 房间号（仅 927/928/930） | 927 |
| room_currency_lower | bigint | 进入房间最少携带银子 | 1000 |
| room_currency_upper | bigint | 进入房间最大携带银子（高底分档=20 亿≈无上限） | 15000 |
| robot | tinyint | 机器人标记：1=机器人（登记在 app_id=1880105），其他=真人 | 0 |
| role | tinyint | 角色：1=地主，2=农民（4 人桌 1 地主 + 3 农民） | 1 |
| chairno | tinyint | 座位号（0/1/2/3；真人 0/3、机器人 1/2） | 0 |
| result_id | tinyint | 最终结果：1=获胜，2=失败，3=平局（fee-only 对局为 NULL） | 1 |
| play_mode | tinyint | 玩法分类：8=四人斗地主 | 8 |
| room_base | int | 房间底分（100/250/500） | 100 |
| room_fee | int | 房间服务费（100→150、250→500、500→1200） | 150 |
| start_money | bigint | 开局前银子（服务费行 olddeposit） | 6674 |
| end_money | bigint | 结算后银子（结算行 end_deposit） | 10124 |
| game_outcome_money | bigint | 游戏输赢（不含服务费，取自结算行 depositdiff） | 3600 |
| is_escape | int | 逃跑罚没（!=0 代表逃跑，实测银子版基本为 0） | 0 |
| magnification | int | stacked × 对局过程炸弹倍率（4炸1/5炸2/6炸3/7炸4/8炸5/王炸10，累积相乘） | 36 |
| magnification_stacked | int | 角色(农2/地6) × 叫分(1~3) × 底牌炸弹倍率（无炸1/4炸2/5+炸3） | 12 |
| app_id | int | 应用 ID（1880053=真人，1880105=银子机器人） | 1880053 |
| app_code | varchar(32) | 应用 code（zgda / snda） | zgda |
| group_id | int | 分端 ID（77=真人，1=机器人） | 77 |
| channel_id | int | 渠道号（真人有值 1000002xxx，机器人=0） | 1000002906 |
| afk_turn_cnt | int | 托管出牌次数（取自结算行） | 0 |

> **货币闭环**：理论上 `end_money − start_money = game_outcome_money − room_fee`。实测 ~99.7% 成立；极少数（07-02 为 1/372）因服务费与结算之间夹有余额变动（如补贴/补偿）而有小残差。**净盈亏以 `end_money − start_money` 为准**（含一切变动），`game_outcome_money` 仅是结算行对局输赢（不含服务费与夹带变动）。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_srddz_daily_game (
  `game_id` int(11) NULL COMMENT "游戏ID",
  `dt` date NOT NULL COMMENT "对局日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `resultguid` varchar(64) NOT NULL COMMENT "对局GUID",
  `start_datetime` datetime NULL COMMENT "服务费行时间（约开局）",
  `end_datetime` datetime NULL COMMENT "结算行时间（约结束）",
  `time_cost` int(11) NULL COMMENT "对局耗时（秒）",
  `room_id` int(11) NULL COMMENT "房间ID",
  `room_currency_lower` bigint(20) NULL COMMENT "进入房间最少携带银子",
  `room_currency_upper` bigint(20) NULL COMMENT "进入房间最大携带银子",
  `robot` tinyint(4) NULL COMMENT "机器人标记：1=机器人（登记在1880105），其他=真人",
  `role` tinyint(4) NULL COMMENT "角色：1=地主，2=农民（4人桌1地+3农）",
  `chairno` tinyint(4) NULL COMMENT "座位号（0/1/2/3）",
  `result_id` tinyint(4) NULL COMMENT "结果：1=获胜，2=失败，3=平局（fee-only为NULL）",
  `play_mode` tinyint(4) NULL COMMENT "玩法分类：8=四人斗地主",
  `room_base` int(11) NULL COMMENT "房间底分（100/250/500）",
  `room_fee` int(11) NULL COMMENT "房间服务费（100→150/250→500/500→1200）",
  `start_money` bigint(20) NULL COMMENT "开局前银子",
  `end_money` bigint(20) NULL COMMENT "结算后银子",
  `game_outcome_money` bigint(20) NULL COMMENT "游戏输赢（不含服务费）",
  `is_escape` int(11) NULL COMMENT "逃跑罚没（!=0代表逃跑）",
  `magnification` int(11) NULL COMMENT "stacked×对局炸弹倍率(4炸1/5炸2/6炸3/7炸4/8炸5/王炸10)",
  `magnification_stacked` int(11) NULL COMMENT "角色(农2/地6)×叫分(1~3)×底牌炸弹(无1/4炸2/5+炸3)",
  `app_id` int(11) NULL COMMENT "应用ID（1880053真人/1880105机器人）",
  `app_code` varchar(32) NULL COMMENT "应用code（zgda/snda）",
  `group_id` int(11) NULL COMMENT "分端ID（77真人/1机器人）",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `afk_turn_cnt` int(11) NULL COMMENT "托管出牌次数"
) ENGINE=OLAP
DUPLICATE KEY(`game_id`, `dt`, `uid`)
COMMENT "四人斗地主每日对局战绩表"
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
    "colocate_with" = "group_daily_data"
);
```

> **DDL 红线**：禁止脚本执行 DDL，建表须由用户在 CloudBeaver 手动执行（见根目录 CLAUDE.md）。

### 数据初始化 SQL

按天 `DELETE + INSERT`（幂等可重跑），脚本：`batch_insert_dws_srddz_daily_game.py`（待新建，对照 `batch_insert_crazyddz_daily_game.py`，但合并逻辑改用下面的条件聚合）。

> **依赖**：`srddz_daily_game_raw` 对应日期需先回填。

```powershell
# 单天
py -3 -u .\batch_insert_dws_srddz_daily_game.py --start 20260702 --end 20260702

# 区间回填
py -3 -u .\batch_insert_dws_srddz_daily_game.py --start 2026-06-01 --end 2026-06-08

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_dws_srddz_daily_game.py --start 20260702 --end 20260702 --dry-run
```

```sql
-- 合并 raw 双行（服务费行 result_id IS NULL + 结算行 result_id IS NOT NULL）为整局一行
INSERT INTO tcy_temp.dws_srddz_daily_game
SELECT
    game_id,
    dt,
    uid,
    resultguid,
    MAX(CASE WHEN result_id IS NULL THEN game_datetime END) AS start_datetime,
    MAX(CASE WHEN result_id IS NOT NULL THEN game_datetime END) AS end_datetime,
    MAX(CASE WHEN result_id IS NOT NULL THEN timecost END) AS time_cost,
    MAX(room_id) AS room_id,
    MAX(room_currency_lower) AS room_currency_lower,
    MAX(room_currency_upper) AS room_currency_upper,
    MAX(CASE WHEN result_id IS NOT NULL THEN robot END) AS robot,
    MAX(CASE WHEN result_id IS NOT NULL THEN `role` END) AS role,
    MAX(chairno) AS chairno,
    MAX(result_id) AS result_id,
    8 AS play_mode,
    MAX(basedeposit) AS room_base,
    MAX(CASE WHEN result_id IS NULL THEN fee END) AS room_fee,
    MAX(CASE WHEN result_id IS NULL THEN olddeposit END) AS start_money,
    MAX(CASE WHEN result_id IS NOT NULL THEN end_deposit END) AS end_money,
    MAX(CASE WHEN result_id IS NOT NULL THEN depositdiff END) AS game_outcome_money,
    IFNULL(MAX(cut), 0) AS is_escape,
    MAX(CASE WHEN result_id IS NOT NULL THEN magnification END) AS magnification,
    MAX(CASE WHEN result_id IS NOT NULL THEN magnification_stacked END) AS magnification_stacked,
    MAX(app_id) AS app_id,
    MAX(app_code) AS app_code,
    MAX(group_id) AS group_id,
    MAX(channel_id) AS channel_id,
    MAX(CASE WHEN result_id IS NOT NULL THEN afk_turn_cnt END) AS afk_turn_cnt
FROM tcy_temp.srddz_daily_game_raw
WHERE game_id = 105
  AND dt = '{dt}'
GROUP BY game_id, dt, uid, resultguid;
```

> **合并原理**：服务费行（`result_id IS NULL`）贡献 `room_fee` / `start_money`；结算行（`result_id IS NOT NULL`）贡献 `result_id` / `magnification` / `time_cost` / `game_outcome_money` / `end_money`；room/role/chairno/app 等 two 行一致的字段用 `MAX` 取值。

## 使用示例

### 1. 某日真人玩家对局概况

```sql
SELECT
    dt,
    COUNT(*) AS games,
    COUNT(DISTINCT uid) AS players,
    ROUND(AVG(time_cost), 1) AS avg_time,
    ROUND(AVG(magnification), 2) AS avg_multi,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_srddz_daily_game
WHERE game_id = 105
  AND robot != 1
  AND dt BETWEEN '2026-06-01' AND '2026-07-02'
GROUP BY dt
ORDER BY dt DESC;
```

### 2. 地主 vs 农民胜率（4 人桌零和校验）

```sql
SELECT
    role,
    COUNT(*) AS games,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS win_rate
FROM tcy_temp.dws_srddz_daily_game
WHERE game_id = 105
  AND robot != 1
  AND result_id IS NOT NULL
  AND dt = '2026-07-02'
GROUP BY role;
```

## 字段使用注意

1. **查询必须含 `game_id = 105`**：`game_id` 是 DUPLICATE KEY 首列，分区裁剪必需。
2. **真人分析用 `robot != 1`**：银子版约 73% 座位是机器人（登记在 app_id=1880105），等价于 `app_id = 1880053` 或 `group_id = 77`，但 `robot != 1` 是最稳的语义过滤。
3. **不可用 `COUNT(DISTINCT uid) = 4` 校验桌级完整性**：共服 + 机器人占座，同局 uid 来自混合 app。
5. **跨天对局**：raw 已用 min_dt 归一 dt，直接 `WHERE dt = T` 即可。
5. **底分维度**：`room_base`（100/250/500）是银子版底分，离散三档，可作为分桶维度。
6. **role/robot 取自结算行**：服务费行是叫地主前的快照，`role`/`robot` 与结算行可能不同（实测 9% player-game 的 role 不一致；若用 `MAX(role)` 会把地主数算成 60 而非实际的 93）。本表 `role`/`robot` 取自结算行（`result_id IS NOT NULL`）为权威值，`chairno` 两行一致仍用 MAX。

## 数据校验 SQL

### 1. 主键唯一性（resultguid + uid 全表唯一）

```sql
SELECT resultguid, uid, COUNT(*) AS cnt
FROM tcy_temp.dws_srddz_daily_game
WHERE game_id = 105 AND dt = '2026-07-02'
GROUP BY resultguid, uid
HAVING COUNT(*) > 1;
```

> 期望 0 行（双行已合并为 1 行）。

### 2. raw vs dws 对局数一致

```sql
SELECT
    (SELECT COUNT(DISTINCT resultguid) FROM tcy_temp.srddz_daily_game_raw WHERE game_id=105 AND dt='2026-07-02') AS raw_games,
    (SELECT COUNT(DISTINCT resultguid) FROM tcy_temp.dws_srddz_daily_game WHERE game_id=105 AND dt='2026-07-02') AS dws_games;
```

> 期望两者相等。

### 3. 货币闭环（end − start ≈ outcome − fee）

```sql
SELECT
    SUM(CASE WHEN ABS((end_money - start_money) - (game_outcome_money - room_fee)) > 1 THEN 1 ELSE 0 END) AS broken,
    COUNT(*) AS total
FROM tcy_temp.dws_srddz_daily_game
WHERE game_id = 105 AND dt = '2026-07-02' AND result_id IS NOT NULL;
```

> 期望 `broken` 接近 0（实测 ~0.3% 因服务费与结算间夹带余额变动而有小残差，属正常；净盈亏以 `end−start` 为准）。

## 表数据流向

```text
tcy_temp.srddz_daily_game_raw     （四人斗地主原始战绩，双行：服务费 + 结算）
            ↓  双行合并（条件聚合）为整局一行
tcy_temp.dws_srddz_daily_game     （四人斗地主对局战绩表）  ← 本表
            ↓  关联分析 / 喂聚合层
tcy_temp.dws_dq_app_daily_reg     （APP 端注册用户宽表，用于留存）
```

> **文档版本**：v0.3（草案，合并逻辑已用 07-02 raw 验证 + 订正 role/robot 取数）
> **创建时间**：2026-07-03
> **更新说明**：
>
> - v0.3：订正合并——`role`/`robot` 改取自结算行（服务费行是叫地主前快照、与结算行可能不同；`MAX(role)` 会把地主算成 60 / 实际 93）；fee-only 经查是跨天捕获时序问题，重跑回填即补齐
> - v0.2：合并逻辑用 raw 实数据验证——372 player-game / 93 局 / 4 fee-only；货币闭环 ~99.7%（1/372 因服务费与结算间夹带 +288 余额变动有小残差），净盈亏以 `end−start` 为准
> - v0.1：首版。对照 `dws_crazyddz_daily_game`，但因 105 银子版是单轮双行（非 521 多轮），合并改用条件聚合（`result_id IS NULL`/`IS NOT NULL` 区分服务费/结算行），去掉多轮字段（settle_count / *_path / total_magnification），新增 `magnification_stacked`
