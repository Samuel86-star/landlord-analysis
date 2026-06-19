# DWS 中间表：首日对局战绩表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_firstday_game` |
| 全名 | `tcy_temp.dws_ddz_firstday_game` |
| 类型 | DWS 层中间表（首日快照） |
| 描述 | 基于 dws_ddz_daily_game 的首日数据切片，字段完全一致，仅筛选注册首日记录 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |
| 上游表 | `tcy_temp.dws_ddz_daily_game`、`tcy_temp.dws_dq_daily_reg` |
| 下游表 | — |

## 设计背景

本表是 `dws_ddz_daily_game` 的**首日数据切片**，核心目的是筛选**注册首日**的对局战绩，用于新用户首日行为分析。

上游 `dws_ddz_daily_game` 已完成字段统一（货币字段、玩法分类、JSON 解析），本表不做任何字段新增或修改，仅通过关联注册表筛选数据范围。

**处理逻辑**：

1. 从 `dws_ddz_daily_game` 读取已统一字段的对局数据
2. 关联 `dws_dq_daily_reg` 注册表（`r.app_id = 1880053`），筛选 `reg_date = dt` 的记录
3. 保留 `dws_ddz_daily_game` 所有字段，字段定义完全一致
4. 不新增/修改任何字段

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_id | int | 游戏 ID | 53 |
| dt | date | 日期 | 2026-04-08 |
| uid | int | 用户ID | 123456789 |
| game_datetime | datetime | 对局时间 | 2026-04-08 10:30:00 |
| resultguid | varchar(64) | 对局GUID | "abc123xyz" |
| timecost | int | 耗时 | 180 |
| room_id | int | 房间ID | 1001 |
| room_currency_lower | bigint | 进入房间所需最少携带货币 | 1000 |
| room_currency_upper | bigint | 进入房间最大携带货币 | 10000 |
| robot | tinyint | 机器人标记：1=机器人，其他=真人 | 0 |
| role | tinyint | 角色：1=地主，2=农民 | 1 |
| chairno | tinyint | 座位号（0/1/2） | 0 |
| result_id | tinyint | 结果：1=获胜，2=失败 | 1 |
| play_mode | tinyint | 玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房，0=其他 | 1 |
| room_base | int | 房间底分 | 100 |
| room_fee | int | 房间服务费 | 10 |
| start_money | bigint | 对局前货币数量（统一字段） | 5000 |
| end_money | bigint | 对局后货币数量（统一字段） | 5500 |
| game_outcome_money | bigint | 游戏输赢（不包括服务费，统一字段） | 600 |
| cut | int | 逃跑罚没货币（!=0 代表存在逃跑行为） | 0 |
| safebox_deposit | int | 保险箱存银 | 1000 |
| magnification | int | 个人理论总倍数 | 12 |
| magnification_stacked | int | 个人加倍：1=不加倍，2=加倍，4=超级加倍 | 2 |
| channel_id | int | 渠道号 | 1001 |
| group_id | int | 分端 ID | 6 |
| app_id | int | 应用ID | 1880053 |
| app_code | varchar(32) | 应用code | zgda |
| afk_turn_cnt | int | 托管出牌次数 | 0 |
| magnification_subdivision | varchar(512) | 倍数细分（公共倍数 + 行为倍数） | {"behavior_bet":{...},"public_bet":{...}} |
| extend_content | varchar(512) | 扩展信息（牌信息 + 牌力值 + 用户属性 + AI等级） | {"card_info":{...},"card_power":{...},"user_attr":{...},"ai_level":{...}} |

## 扩展字段

> **注意**：`extend_content` 存在新旧两种格式，更新 SQL 已在 `dws_ddz_daily_game` 中通过 `get_json_*` 与正则混合提取统一覆盖。

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| initial_bet | tinyint | 初始倍数 | 1 |
| grab_landlord_bet | tinyint | 抢地主倍数：3=无人抢，6=1人抢，12=2人抢，24=3人抢 | 6 |
| complete_victory_bet | tinyint | 春天/反春标记：1=无，2=春天或反春 | 1 |
| bomb_bet | int | 炸弹个数：1=无炸弹，否则 bomb_bet/2 为炸弹个数（打出炸弹数，非持有炸弹数） | 4 |
| landlord_double_bet | tinyint | 地主加倍倍数：1=不加倍，2=加倍，4=超级加倍 | 1 |
| total_farmer_double_bet | tinyint | 所有农民加倍倍数 | 2 |
| real_magnification | double | 游戏输赢实际倍数 | 5.0 |
| hand_cards | varchar(64) | 手牌 | "3455677888999JJJQQQKKKAAA222" |
| bottom_cards | varchar(16) | 底牌 | "345" |
| shuffle_type | int | 配牌类型: 0=随机发牌，201=新手保护机器人，202=充值保护机器人匹配，203=老用户每日前N，204=房间前N次触发机器人保护，205=低保次数触发机器人保护，206=连输保护机器人匹配，207=连输银两触发机器人匹配，默认为0 | 0 |
| card_id | int | 牌库编号（默认0，0为随机牌；>0时代表有配牌，对应牌库编号） | 1001 |
| card_power | int | 初始牌分 | 15 |
| card_power_final | int | 算上底牌后的牌分 | 22 |
| cost_time | int | 重洗牌花费时间（ms） | 0 |
| is_pass | boolean | 是否重洗成功 | false |
| shuffle_times | tinyint | 重洗次数：-1=未开启重洗，0=未重洗过，1=重洗过1次，依此类推 | 0 |
| user_attr_bout | int | 用户历史局数（斗地主全局下首局时记录为0） | 2 |
| ai_level_type | tinyint | 机器人类型：0=逻辑机器人，1=算法机器人 | 1 |
| ai_level_callflag | tinyint | 叫地主强度：1=强化叫地主，2=监督叫地主，默认0 | 1 |
| ai_level_robflag | tinyint | 抢地主强度：1=强化抢地主，2=监督抢地主，默认0 | 1 |
| ai_level_doubleflag | tinyint | 加倍强度：1=强化加倍，2=监督加倍，默认0 | 2 |
| ai_level_throwtileflag | tinyint | 打牌强度：1=强化打牌，2=中等监督打牌，3=多风格打牌，默认0 | 1 |

## 玩法分类说明 (play_mode)

| play_mode | 玩法 | room_id 列表 | 备注 |
| --------- | ---- | ----------- | ---- |
| 1 | 经典 | 742, 420, 4484, 12074, 6314, 11168, 10336, 16445 | 银子玩法 |
| 2 | 不洗牌 | 421, 22039, 22040, 22041, 22042 | 银子玩法 |
| 3 | 癞子 | 13176, 13177, 13178 | 银子玩法 |
| 4 | 积分 | 11534(PC端), 14238, 15458 | 积分玩法（PC端） |
| 5 | 比赛 | 11534（APP/小游戏端） | 比赛玩法（仅在APP/小游戏端，共用11534积分房） |
| 6 | 好友房 | 158, 159 | 积分玩法 |
| 0 | 其他 | 其他 room_id | 未识别玩法 |

**比赛玩法判断逻辑**：

- `room_id = 11534` 且 `group_id IN (6,66,33,44,77,99,8,88,56)` 时为比赛玩法（APP/小游戏端），`play_mode = 5`
- `room_id = 11534` 且 `group_id` 为 PC 端时为积分玩法，`play_mode = 4`
- `room_id IN (14238,15458)` 时为积分玩法（仅PC端），`play_mode = 4`

## 货币字段统一逻辑

```sql
-- 房间底分
CASE WHEN room_id IN (11534,14238,15458,158,159) THEN basescore ELSE basedeposit END AS room_base

-- 房间服务费
CASE WHEN room_id IN (11534,14238,15458,158,159) THEN score_fee ELSE fee END AS room_fee

-- 对局前货币
CASE WHEN room_id IN (11534,14238,15458,158,159) THEN oldscore ELSE olddeposit END AS start_money

-- 对局后货币
CASE WHEN room_id IN (11534,14238,15458,158,159) THEN end_score ELSE end_deposit END AS end_money

-- 输赢（还原服务费前）
CASE WHEN room_id IN (11534,14238,15458,158,159) THEN scorediff + score_fee ELSE depositdiff + fee END AS game_outcome_money
```

## 构建 SQL

```sql
CREATE TABLE tcy_temp.dws_ddz_firstday_game (
  `game_id` int(11) NULL COMMENT "游戏 ID",
  `dt` DATE NOT NULL COMMENT "日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `game_datetime` datetime NOT NULL COMMENT "对局时间",
  `resultguid` varchar(64) NULL COMMENT "对局GUID",
  `timecost` int(11) NULL COMMENT "耗时",
  `room_id` int(11) NULL COMMENT "房间ID",
  `room_currency_lower` bigint(20) NULL COMMENT "进入房间所需最少携带货币",
  `room_currency_upper` bigint(20) NULL COMMENT "进入房间最大携带货币",
  `robot` tinyint(4) NULL COMMENT "机器人标记：1=机器人，其他=真人",
  `role` tinyint(4) NULL COMMENT "角色：1=地主，2=农民",
  `chairno` tinyint(4) NULL COMMENT "座位号（0/1/2）",
  `result_id` tinyint(4) NULL COMMENT "结果：1=获胜，2=失败",
  `play_mode` tinyint(4) NULL COMMENT "玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房，0=其他",
  `room_base` int(11) NULL COMMENT "房间底分",
  `room_fee` int(11) NULL COMMENT "房间服务费",
  `start_money` bigint(20) NULL COMMENT "对局前货币数量（统一字段）",
  `end_money` bigint(20) NULL COMMENT "对局后货币数量（统一字段）",
  `game_outcome_money` bigint(20) NULL COMMENT "游戏输赢（不包括服务费，统一字段）",
  `cut` int(11) NULL COMMENT "逃跑罚没货币（!=0 代表存在逃跑行为）",
  `safebox_deposit` int(11) NULL COMMENT "保险箱存银",
  `magnification` int(11) NULL COMMENT "个人理论总倍数",
  `magnification_stacked` int(11) NULL COMMENT "个人加倍：1=不加倍，2=加倍，4=超级加倍",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `group_id` int(11) NULL COMMENT "分端 ID",
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `app_code` varchar(32) NULL COMMENT "应用code",
  `afk_turn_cnt` int(11) NULL COMMENT "托管出牌次数",
  `magnification_subdivision` varchar(512) NULL COMMENT "倍数细分（公共倍数 + 行为倍数）",
  `extend_content` varchar(512) NULL COMMENT "扩展信息（牌信息 + 牌力值 + 用户属性 + AI等级）",
  `initial_bet` tinyint(4) NULL COMMENT "初始倍数",
  `grab_landlord_bet` tinyint(4) NULL COMMENT "抢地主倍数：3=无人抢，6=1人抢，12=2人抢，24=3人抢",
  `complete_victory_bet` tinyint(4) NULL COMMENT "春天/反春标记：1=无，2=春天或反春",
  `bomb_bet` int(11) NULL COMMENT "炸弹个数：1=无炸弹，否则 bomb_bet/2 为炸弹个数（打出炸弹数，非持有炸弹数）",
  `landlord_double_bet` tinyint(4) NULL COMMENT "地主加倍倍数：1=不加倍，2=加倍，4=超级加倍",
  `total_farmer_double_bet` tinyint(4) NULL COMMENT "所有农民加倍倍数",
  `real_magnification` double NULL COMMENT "游戏输赢实际倍数",
  `hand_cards` varchar(64) NULL COMMENT "手牌",
  `bottom_cards` varchar(16) NULL COMMENT "底牌",
  `shuffle_type` int(11) NULL COMMENT "配牌类型: 0=随机发牌，201=新手保护机器人，202=充值保护机器人匹配，203=老用户每日前N，204=房间前N次触发机器人保护，205=低保次数触发机器人保护，206=连输保护机器人匹配，207=连输银两触发机器人匹配，默认为0",
  `card_id` int(11) NULL COMMENT "牌库编号（默认0，0为随机牌；>0时代表有配牌，对应牌库编号）",
  `card_power` int(11) NULL COMMENT "初始牌分",
  `card_power_final` int(11) NULL COMMENT "算上底牌后的牌分",
  `cost_time` int(11) NULL COMMENT "重洗牌花费时间（ms）",
  `is_pass` boolean NULL COMMENT "是否重洗成功",
  `shuffle_times` tinyint(4) NULL COMMENT "重洗次数：-1=未开启重洗，0=未重洗过，1=重洗过1次，依此类推",
  `user_attr_bout` int(11) NULL COMMENT "用户历史局数（斗地主全局下首局时记录为0）",
  `ai_level_type` tinyint(4) NULL COMMENT "机器人类型：0=逻辑机器人，1=算法机器人",
  `ai_level_callflag` tinyint(4) NULL COMMENT "叫地主强度：1=强化叫地主，2=监督叫地主，默认0",
  `ai_level_robflag` tinyint(4) NULL COMMENT "抢地主强度：1=强化抢地主，2=监督抢地主，默认0",
  `ai_level_doubleflag` tinyint(4) NULL COMMENT "加倍强度：1=强化加倍，2=监督加倍，默认0",
  `ai_level_throwtileflag` tinyint(4) NULL COMMENT "打牌强度：1=强化打牌，2=中等监督打牌，3=多风格打牌，默认0"
) ENGINE=OLAP
DUPLICATE KEY(`game_id`, `dt`, `uid`)
COMMENT "斗地主首日游戏明细表"
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

## 更新SQL

按天 `DELETE + INSERT`（幂等可重跑），脚本：[`batch_insert_ddz_firstday_game.py`](../../../py/batch_insert_ddz_firstday_game.py)

> **依赖**：dws_ddz_daily_game、dws_dq_daily_reg 对应日期需先回填。

```powershell
# 单天
py -3 -u .\batch_insert_ddz_firstday_game.py --start 20260617 --end 20260617

# 区间回填
py -3 -u .\batch_insert_ddz_firstday_game.py --start 2026-06-01 --end 2026-06-08

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_ddz_firstday_game.py --start 20260617 --end 20260617 --dry-run
```


```sql
INSERT INTO tcy_temp.dws_ddz_firstday_game
SELECT
    g.game_id, g.dt, g.uid, g.game_datetime, g.resultguid, g.timecost, g.room_id,
    g.room_currency_lower, g.room_currency_upper,
    g.robot, g.role, g.chairno, g.result_id,
    g.play_mode, g.room_base, g.room_fee,
    g.start_money, g.end_money, g.game_outcome_money,
    g.cut, g.safebox_deposit, g.magnification, g.magnification_stacked,
    g.channel_id, g.group_id, g.app_id, g.app_code, 
    g.afk_turn_cnt, g.magnification_subdivision, g.extend_content,
    g.initial_bet, g.grab_landlord_bet, g.complete_victory_bet, g.bomb_bet,
    g.landlord_double_bet, g.total_farmer_double_bet, g.real_magnification,
    g.hand_cards, g.bottom_cards, g.shuffle_type, g.card_id,
    g.card_power, g.card_power_final, g.cost_time, g.is_pass,
    g.shuffle_times, g.user_attr_bout,
    g.ai_level_type, g.ai_level_callflag, g.ai_level_robflag,
    g.ai_level_doubleflag, g.ai_level_throwtileflag
FROM tcy_temp.dws_ddz_daily_game g
INNER JOIN tcy_temp.dws_dq_daily_reg r
    ON r.uid = g.uid AND r.reg_date = g.dt
WHERE r.app_id = 1880053
    AND g.game_id = 53
    AND g.dt BETWEEN '2026-02-10' AND '2026-04-22';
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../../ops/daily_data_ops.md)

## 使用示例

### 1. 统计首日游戏概况

```sql
SELECT
    play_mode,
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 4 THEN '积分'
        WHEN 5 THEN '比赛'
        WHEN 6 THEN '好友房'
        ELSE '其他'
    END AS play_mode_name,
    COUNT(*) AS game_count,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(timecost), 1) AS avg_timecost,
    ROUND(AVG(magnification), 2) AS avg_magnification,
    ROUND(AVG(real_magnification), 2) AS avg_real_magnification
FROM tcy_temp.dws_ddz_firstday_game
WHERE game_id = 53
  AND dt BETWEEN '2026-02-10' AND '2026-04-22'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)  -- 仅银子玩法
GROUP BY play_mode
ORDER BY play_mode;
```

### 2. 计算用户首日经济变化

```sql
SELECT
    uid,
    dt,
    COUNT(*) AS first_day_game_cnt,
    SUM(game_outcome_money) AS total_outcome_money,
    SUM(room_fee) AS total_fee,
    SUM(game_outcome_money) - SUM(room_fee) AS net_outcome_money,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    SUM(CASE WHEN result_id = 2 THEN 1 ELSE 0 END) AS lose_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_ddz_firstday_game
WHERE game_id = 53
  AND dt BETWEEN '2026-02-10' AND '2026-04-22'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)  -- 仅银子玩法
GROUP BY uid, dt;
```

### 3. 首日局数分布分析

```sql
SELECT
    CASE
        WHEN game_cnt = 0 THEN '0局'
        WHEN game_cnt BETWEEN 1 AND 5 THEN '1-5局'
        WHEN game_cnt BETWEEN 6 AND 10 THEN '6-10局'
        WHEN game_cnt BETWEEN 11 AND 20 THEN '11-20局'
        WHEN game_cnt > 20 THEN '20局以上'
    END AS game_cnt_range,
    COUNT(*) AS user_count,
    ROUND(COUNT(*) * 100.0 / SUM(COUNT(*)) OVER(), 2) AS user_pct
FROM (
    SELECT uid, COUNT(*) AS game_cnt
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE game_id = 53
      AND dt BETWEEN '2026-02-10' AND '2026-04-22'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY uid
) t
GROUP BY CASE
    WHEN game_cnt = 0 THEN '0局'
    WHEN game_cnt BETWEEN 1 AND 5 THEN '1-5局'
    WHEN game_cnt BETWEEN 6 AND 10 THEN '6-10局'
    WHEN game_cnt BETWEEN 11 AND 20 THEN '11-20局'
    WHEN game_cnt > 20 THEN '20局以上'
END
ORDER BY MIN(game_cnt);
```

### 4. 首日流失用户游戏特征分析

```sql
-- 分析首日仅玩1-5局后流失的用户特征
SELECT
    play_mode,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(timecost), 1) AS avg_timecost,
    ROUND(AVG(real_magnification), 2) AS avg_real_multi,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ROUND(AVG(room_base), 2) AS avg_room_base
FROM tcy_temp.dws_ddz_firstday_game g
INNER JOIN (
    SELECT uid
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE game_id = 53
      AND dt BETWEEN '2026-02-10' AND '2026-04-22'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY uid
    HAVING COUNT(*) BETWEEN 1 AND 5
) low_players ON g.game_id = 53 AND g.uid = low_players.uid
WHERE g.dt BETWEEN '2026-02-10' AND '2026-04-22'
  AND g.robot != 1
  AND g.play_mode IN (1, 2, 3)
GROUP BY play_mode;
```

## 字段使用注意

1. **货币类型区分**：
   - `play_mode IN (1, 2, 3)`：银子玩法，货币单位为银子
   - `play_mode IN (4, 5, 6)`：积分玩法，货币单位为积分
   - 混合分析时需注意货币单位不同

2. **game_outcome_money 字段**：
   - 该字段为统一后的游戏输赢（不含服务费），即净输赢
   - 如需含服务费的账户变动金额，应使用 `game_outcome_money - room_fee`

3. **real_magnification 字段**：
   - 计算公式：`ABS(game_outcome_money) / room_base`
   - 使用 `ABS` 取绝对值，始终为正数，反映本局实际输赢倍数大小
   - 如需区分输赢方向，配合 `result_id` 使用

4. **JSON 提取**：
   - `magnification_subdivision` 为标准 JSON，使用 `get_json_int` 提取
   - `extend_content` 兼容新旧两种格式，路径不一致的字段（shuffle_type / card_id / user_attr_bout / ai_level.*）使用 `regexp_extract` 统一提取
   - 如果 JSON 无对应字段，返回 NULL（建议用 `COALESCE` 处理）

5. **首日限定**：
   - 本表仅包含注册当日的对局数据
   - 通过关联 `dws_dq_daily_reg` 表筛选 `dt = reg_date` 的对局
   - 如需分析多日数据，请使用 `dws_ddz_daily_game` 表

6. **机器人标记**：
   - 表中包含机器人和真人数据，通过 `robot` 字段区分
   - `robot = 1` 为机器人，其他为真人
   - 分析时建议添加 `robot != 1` 条件过滤真人数据

7. **与 daily_game 的关系**：
   - `dws_ddz_firstday_game` 是 `dws_ddz_daily_game` 的子集
   - 首日数据可从两个表查询，结果一致
   - 本表专为首日行为分析优化，查询更高效

## 与原始表的字段映射

| 统一字段 | 经典/不洗牌/癞子玩法 | 积分/比赛/好友房玩法 |
| --------- | --------------------- | --------------------- |
| `room_base` | `basedeposit` | `basescore` |
| `room_fee` | `fee` | `score_fee` |
| `start_money` | `olddeposit` | `oldscore` |
| `end_money` | `end_deposit` | `end_score` |
| `game_outcome_money` | `depositdiff + fee` | `scorediff + score_fee` |

## 表数据流向

```text
hive_catalog_cdh5.dwd.fact_game_combatgains        （StarRocks 原始对局日志，多货币字段）
            ↓  迁移至 StarRocks
tcy_temp.ddz_daily_game_raw       （StarRocks 原始对局表，保持原始字段）
            ↓  统一字段、玩法分类、JSON 解析
tcy_temp.dws_ddz_daily_game       （扩展字段对局表）
            ↓  关联注册表筛选首日
tcy_temp.dws_ddz_firstday_game    （首日对局战绩表）
```

> **文档版本**：v1.0
> **创建时间**：2026-06-11
> **更新说明**：
>
> - v1.0：初始版本
