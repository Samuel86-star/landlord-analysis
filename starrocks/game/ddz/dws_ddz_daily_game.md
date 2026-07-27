# DWS 中间表：每日对局战绩扩展字段表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_daily_game` |
| 全名 | `tcy_temp.dws_ddz_daily_game` |
| 类型 | DWS 层中间表（每日增量） |
| 描述 | 基于 ddz_daily_game_raw 扩展的字段表，完成货币字段统一、玩法分类、JSON 解析等处理 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |
| 上游表 | `tcy_temp.ddz_daily_game_raw` |
| 下游表 | `tcy_temp.dws_ddz_firstday_game` |

## 设计背景

上游 `ddz_daily_game_raw` 保持原始字段不变（两套货币字段、JSON 字段未解析），本表在此基础上进行**字段统一与扩展**：

| 玩法 | 货币类型 | 底分字段 | 服务费字段 | 对局前货币 | 对局后货币 | 货币变动值(含服务费) |
| ---- | ------- | ------- | --------- | --------- | --------- | ------- |
| 经典/不洗牌/癞子 | 银子 | `basedeposit` | `fee` | `olddeposit` | `end_deposit` | `depositdiff` |
| 积分/好友房/比赛 | 积分 | `basescore` | `score_fee` | `oldscore` | `end_score` | `scorediff` |

此外，倍数相关信息存储在 JSON 字段 `magnification_subdivision` 中，需要解析提取。

**处理逻辑**：

1. 统一货币字段命名：`start_money`、`end_money`、`game_outcome_money`
2. 统一房间底分和服务费：`room_base`、`room_fee`
3. 添加玩法分类字段：`play_mode`
4. 解析 JSON 倍数字段到独立列：`initial_bet`、`grab_landlord_bet`、`complete_victory_bet`、`bomb_bet`、`landlord_double_bet`、`total_farmer_double_bet`
5. 解析 JSON 扩展信息到独立列（手牌、牌力值、机器人 AI 等级等）
6. 计算实际输赢倍数：`real_magnification`

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
| magnification_stacked | int | 个人加倍：1=不加倍，2=加倍，4=超级加倍（raw 原生字段，~1.5% 为 NULL；DWS 脚本已 `IFNULL(,1)` 兜底，同其他加倍字段口径） | 2 |
| channel_id | int | 渠道号 | 1001 |
| group_id | int | 分端 ID | 6 |
| app_id | int | 应用ID | 1880053 |
| app_code | varchar(32) | 应用code | zgda |
| afk_turn_cnt | int | 托管出牌次数 | 0 |
| magnification_subdivision | varchar(512) | 倍数细分（公共倍数 + 行为倍数） | {"behavior_bet":{...},"public_bet":{...}} |
| extend_content | varchar(512) | 扩展信息（牌信息 + 牌力值 + 用户属性 + AI等级） | {"card_info":{...},"card_power":{...},"user_attr":{...},"ai_level":{...}} |

## 扩展字段

> **注意**：`extend_content` 存在新旧两种格式，更新 SQL 已通过 `get_json_*` 与正则混合提取统一覆盖，无需按格式分支。

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
CREATE TABLE tcy_temp.dws_ddz_daily_game (
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
COMMENT "斗地主每日游戏明细表"
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

按天 `DELETE + INSERT`（幂等可重跑），脚本：[`batch_insert_ddz_daily_game.py`](../../../ops/py/batch_insert_ddz_daily_game.py)

> **依赖**：ddz_daily_game_raw 对应日期需先回填。

```powershell
# 单天
py -3 -u .\batch_insert_ddz_daily_game.py --start 20260617 --end 20260617

# 区间回填
py -3 -u .\batch_insert_ddz_daily_game.py --start 2026-06-01 --end 2026-06-08

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_ddz_daily_game.py --start 20260617 --end 20260617 --dry-run
```


`extend_content` 字段兼容新旧两种 JSON 格式，以下 SQL 通过 `get_json_*` 与正则混合提取，同时覆盖两种格式。

**新格式**（4 个顶级分类，标准 JSON）：

```json
{
  "card_info": {"hand_cards": "...", "bottom_cards": "...", "shuffle_type": 0, "card_id": 0},
  "card_power": {"card_power": 15, "card_power_final": 22, "cost_time": 0, "is_pass": "false", "shuffle_times": 0},
  "user_attr": {"bout": 2},
  "ai_level": {"type": 1, "callflag": 1, "robflag": 1, "doubleflag": 2, "throwtileflag": 1}
}
```

**旧格式**（2 个顶级分类，`ai_level` 值为残缺 JSON 字符串，无引号包裹 key，无法用 `get_json_*` 解析）：

```json
{
  "card_power": {"card_power": 15, "card_power_final": 22, "cost_time": 0, "is_pass": "false", "shuffle_times": 0},
  "new_user_protect": {"protect_type": 0, "card_id": 0, "bout": 2},
  "ai_level": "{type:1, callflag:1, robflag:1, doubleflag:2, throwtileflag:1}"
}
```

```sql
INSERT INTO tcy_temp.dws_ddz_daily_game
SELECT
    game_id, dt, uid, game_datetime, resultguid, timecost,
    room_id, room_currency_lower, room_currency_upper, 
    robot, role, chairno, result_id,
    CASE
        WHEN room_id IN (742,420,4484,12074,6314,11168,10336,16445) THEN 1 -- 经典
        WHEN room_id IN (421,22039,22040,22041,22042) THEN 2 -- 不洗牌
        WHEN room_id IN (13176,13177,13178) THEN 3 -- 癞子
        WHEN room_id = 11534 AND group_id IN (6,66,33,44,77,99,8,88,56) THEN 5 -- 比赛（APP/小游戏端）
        WHEN room_id IN (11534,14238,15458) THEN 4 -- 积分
        WHEN room_id IN (158,159) THEN 6 -- 好友房
        ELSE 0
    END AS play_mode,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN basescore ELSE basedeposit END AS room_base,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN score_fee ELSE fee END AS room_fee,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN oldscore ELSE olddeposit END AS start_money,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN end_score ELSE end_deposit END AS end_money,
    CASE
        WHEN room_id IN (11534,14238,15458,158,159) THEN scorediff + score_fee
        ELSE depositdiff + fee
    END AS game_outcome_money,
    cut, safebox_deposit, magnification, IFNULL(magnification_stacked, 1) AS magnification_stacked,
    channel_id, group_id, app_id, app_code,
    afk_turn_cnt, magnification_subdivision, extend_content,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.initial_bet'), 1) AS initial_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.grab_landlord_bet'), 3) AS grab_landlord_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.complete_victory_bet'), 1) AS complete_victory_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.public_bet.bomb_bet'), 1) AS bomb_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.behavior_bet.landlord'), 1) AS landlord_double_bet,
    IFNULL(get_json_int(magnification_subdivision, '$.behavior_bet.farmer1'), 1)
        + IFNULL(get_json_int(magnification_subdivision, '$.behavior_bet.farmer2'), 1) AS total_farmer_double_bet,
    CASE
        WHEN room_id IN (11534,14238,15458,158,159)
        THEN ROUND(ABS(scorediff + score_fee) / NULLIF(basescore, 0), 2)
        ELSE ROUND(ABS(depositdiff + fee) / NULLIF(basedeposit, 0), 2)
    END AS real_magnification,
    IFNULL(get_json_string(extend_content, '$.card_info.hand_cards'), '') AS hand_cards,
    IFNULL(get_json_string(extend_content, '$.card_info.bottom_cards'), '') AS bottom_cards,
    IFNULL(CAST(regexp_extract(extend_content, '(?:shuffle_type|protect_type)[^0-9]*([0-9]+)', 1) AS INT), 0) AS shuffle_type,
    IFNULL(CAST(regexp_extract(extend_content, 'card_id[^0-9]*([0-9]+)', 1) AS INT), 0) AS card_id,
    IFNULL(get_json_int(extend_content, '$.card_power.card_power'), 0) AS card_power,
    IFNULL(get_json_int(extend_content, '$.card_power.card_power_final'), 0) AS card_power_final,
    IFNULL(get_json_int(extend_content, '$.card_power.cost_time'), 0) AS cost_time,
    IFNULL(get_json_string(extend_content, '$.card_power.is_pass'), 'false') AS is_pass,
    IFNULL(get_json_int(extend_content, '$.card_power.shuffle_times'), 0) AS shuffle_times,
    IFNULL(CAST(regexp_extract(extend_content, 'bout[^0-9]*([0-9]+)', 1) AS INT), 0) AS user_attr_bout,
    IFNULL(CAST(regexp_extract(extend_content, 'ai_level[^0-9]*type[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_type,
    IFNULL(CAST(regexp_extract(extend_content, 'callflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_callflag,
    IFNULL(CAST(regexp_extract(extend_content, 'robflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_robflag,
    IFNULL(CAST(regexp_extract(extend_content, 'doubleflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_doubleflag,
    IFNULL(CAST(regexp_extract(extend_content, 'throwtileflag[^0-9]*([0-9]+)', 1) AS INT), 0) AS ai_level_throwtileflag
FROM tcy_temp.ddz_daily_game_raw
WHERE game_id = 53
  AND dt BETWEEN '2026-05-15' AND '2026-05-18';
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../../ops/daily_data_ops.md)

新旧格式中提取方式不同的字段对照：

| 字段 | 新格式路径 | 旧格式路径 | 统一方案 |
| ---- | ---- | ---- | ---- |
| `shuffle_type` | `$.card_info.shuffle_type` | `$.new_user_protect.protect_type` | `regexp_extract(..., '(?:shuffle_type\|protect_type)[^0-9]*([0-9]+)', 1)` |
| `card_id` | `$.card_info.card_id` | `$.new_user_protect.card_id` | `regexp_extract(..., 'card_id[^0-9]*([0-9]+)', 1)` |
| `user_attr_bout` | `$.user_attr.bout` | `$.new_user_protect.bout` | `regexp_extract(..., 'bout[^0-9]*([0-9]+)', 1)` |
| `ai_level_type` | `$.ai_level.type` | 残缺 JSON | `regexp_extract(..., 'ai_level[^0-9]*type[^0-9]*([0-9]+)', 1)` |
| `ai_level_callflag` | `$.ai_level.callflag` | 残缺 JSON | `regexp_extract(..., 'callflag[^0-9]*([0-9]+)', 1)` |
| `ai_level_robflag` | `$.ai_level.robflag` | 残缺 JSON | `regexp_extract(..., 'robflag[^0-9]*([0-9]+)', 1)` |
| `ai_level_doubleflag` | `$.ai_level.doubleflag` | 残缺 JSON | `regexp_extract(..., 'doubleflag[^0-9]*([0-9]+)', 1)` |
| `ai_level_throwtileflag` | `$.ai_level.throwtileflag` | 残缺 JSON | `regexp_extract(..., 'throwtileflag[^0-9]*([0-9]+)', 1)` |

## 使用示例

### 1. 按玩法统计对局数据

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
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id = 53
  AND dt BETWEEN '2026-02-10' AND '2026-02-10'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)  -- 仅银子玩法
GROUP BY play_mode
ORDER BY play_mode;
```

### 2. 计算用户首日经济变化

```sql
SELECT
    uid,
    SUM(game_outcome_money) AS total_outcome_money,
    SUM(room_fee) AS total_fee,
    SUM(game_outcome_money) - SUM(room_fee) AS net_outcome_money,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    SUM(CASE WHEN result_id = 2 THEN 1 ELSE 0 END) AS lose_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id = 53
  AND dt = '2026-02-10'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)  -- 仅银子玩法
GROUP BY uid;
```

### 3. 高倍局分析

```sql
SELECT
    play_mode,
    CASE WHEN magnification > 24 THEN '高倍' ELSE '非高倍' END AS multi_type,
    COUNT(*) AS game_count,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(real_magnification), 2) AS avg_real_multi,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id = 53
  AND dt BETWEEN '2026-02-10' AND '2026-02-15'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)
GROUP BY play_mode, CASE WHEN magnification > 24 THEN '高倍' ELSE '非高倍' END
ORDER BY play_mode, multi_type;
```

### 4. 炸弹频率分析

```sql
SELECT
    play_mode,
    CASE WHEN bomb_bet > 0 THEN '有炸弹' ELSE '无炸弹' END AS bomb_type,
    COUNT(*) AS game_count,
    ROUND(AVG(magnification), 2) AS avg_magnification,
    ROUND(AVG(real_magnification), 2) AS avg_real_magnification,
    ROUND(AVG(timecost), 1) AS avg_timecost
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id = 53
  AND dt BETWEEN '2026-02-10' AND '2026-02-15'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)
GROUP BY play_mode, CASE WHEN bomb_bet > 0 THEN '有炸弹' ELSE '无炸弹' END;
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

5. **时间范围**：
   - 默认覆盖 `20260210` 至 `20260508`（注册期 + Day30 观测期）
   - 可根据实际需求调整

6. **机器人标记**：
   - 表中包含机器人和真人数据，通过 `robot` 字段区分
   - `robot = 1` 为机器人，其他为真人
   - 分析时建议添加 `robot != 1` 条件过滤真人数据

## 与原始表的字段映射

| dws_ddz_daily_game 统一字段 | ddz_daily_game_raw 银子玩法字段 | ddz_daily_game_raw 积分玩法字段 |
| --------------------------- | ------------------------------- | ------------------------------- |
| `room_base` | `basedeposit` | `basescore` |
| `room_fee` | `fee` | `score_fee` |
| `start_money` | `olddeposit` | `oldscore` |
| `end_money` | `end_deposit` | `end_score` |
| `game_outcome_money` | `depositdiff + fee` | `scorediff + score_fee` |

## 表数据流向

```text
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
