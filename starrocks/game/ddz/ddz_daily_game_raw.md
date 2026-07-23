# ODS 迁移表：每日对局战绩原始字段表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `ddz_daily_game_raw` |
| 全名 | `tcy_temp.ddz_daily_game_raw` |
| 类型 | ODS 层迁移表（每日增量） |
| 描述 | 将 Hive 原始对局日志迁移至 StarRocks，保持原始字段不变，不做字段统一转换 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |
| 上游表 | `hive_catalog_cdh5.dwd.fact_game_combatgains` |
| 下游表 | `tcy_temp.dws_ddz_daily_game` |

## 设计背景

本表的核心目的是将 Hive 中的原始对局日志 `hive_catalog_cdh5.dwd.fact_game_combatgains` 迁移到 StarRocks 数仓（`tcy_temp` 库），以利用 StarRocks 的 OLAP 查询性能支撑后续分析工作。

原始表存储了所有玩法的对局日志，但不同玩法使用不同的货币字段：

| 玩法 | 货币类型 | 底分字段 | 服务费字段 | 对局前货币 | 对局后货币 | 货币变动值(含服务费) |
| ---- | ------- | ------- | --------- | --------- | --------- | ------- |
| 经典/不洗牌/癞子 | 银子 | `basedeposit` | `fee` | `olddeposit` | `end_deposit` | `depositdiff` |
| 积分/好友房/比赛 | 积分 | `basescore` | `score_fee` | `oldscore` | `end_score` | `scorediff` |

迁移过程中保持原始字段不变，未做统一转换（统一转换由下游 `dws_ddz_daily_game` 表完成）。表中同时保留了 JSON 字段 `magnification_subdivision`（个人操作倍数）和 `extend_content`（手牌等扩展信息），供下游按需解析提取。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_id | int | 游戏 ID | 53 |
| dt | date | 对局日期 | 2026-04-08 |
| uid | int | 玩家 ID | 123456789 |
| game_datetime | datetime | 对局时间 | 2026-04-08 10:30:00 |
| resultguid | varchar(64) | 本局战绩 ID | "abc123xyz" |
| timecost | int | 对局耗时（秒） | 180 |
| room_id | int | 房间号 | 1001 |
| room_currency_lower | bigint | 进入房间所需最少携带货币 | 1000 |
| room_currency_upper | bigint | 进入房间最大携带货币 | 10000 |
| robot | tinyint | 机器人标记：1=机器人，其他=真人 | 0 |
| role | tinyint | 角色：1=地主，2=农民 | 1 |
| chairno | tinyint | 座位号（0/1/2） | 0 |
| result_id | tinyint | 结果：1=获胜，2=失败 | 1 |
| basedeposit | int | 银子玩法房间底分(银子) | 100 |
| olddeposit | bigint | 银子玩法对局前银子数量 | 5500 |
| end_deposit | bigint | 银子玩法对局后银子数量 | 500 |
| fee | int | 银子玩法对局服务费(银子) | 500 |
| depositdiff | bigint | 银子玩法银子变动数量(含服务费) | 5000 |
| basescore | int | 积分玩法房间底分(银子) | 100 |
| oldscore | bigint | 积分玩法对局前积分数量 | 5500 |
| end_score | bigint | 积分玩法对局后积分数量 | 500 |
| score_fee | int | 积分玩法对局服务费(积分) | 500 |
| scorediff | bigint | 积分玩法积分变动数量(含服务费) | 5000 |
| cut | int | 逃跑罚没货币（!=0 代表存在逃跑行为） | 0 |
| safebox_deposit | int | 保险箱存银 | 1000 |
| magnification | int | 个人理论总倍数 | 12 |
| magnification_stacked | int | 个人加倍：1=不加倍，2=加倍，4=超级加倍 | 2 |
| channel_id | int | 渠道号 | 1001 |
| group_id | int | 分端 ID | 6 |
| app_id | int | 应用 ID | 1880053 |
| app_code | varchar(32) | 应用code | zgda |
| afk_turn_cnt | int | 托管出牌次数 | 0 |
| magnification_subdivision | varchar(512) | 倍数细分（公共倍数 + 行为倍数） | {"behavior_bet":{...},"public_bet":{...}} |
| extend_content | varchar(512) | 扩展信息（新格式：牌信息 + 牌力值 + 用户属性 + AI等级） | {"card_info":{...},"card_power":{...},"user_attr":{...},"ai_level":{...}} |

> **注意**：`extend_content` 存在新旧两种格式，详见后文「extend_content 字段结构说明（新格式）」和「extend_content 字段结构说明（旧格式）」章节。

## magnification_subdivision 字段结构说明

`magnification_subdivision` 为 JSON 字段，包含**公共倍数**（`public_bet`）和**行为倍数**（`behavior_bet`）。

### public_bet（公共倍数）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.public_bet.initial_bet` | tinyint | 初始倍数 |
| `$.public_bet.grab_landlord_bet` | tinyint | 抢地主倍数：3=无人抢，6=1人抢，12=2人抢，24=3人抢 |
| `$.public_bet.bomb_bet` | int | 炸弹个数：1=无炸弹，否则 bomb_bet/2 为炸弹个数（打出炸弹数，非持有炸弹数） |
| `$.public_bet.complete_victory_bet` | tinyint | 春天/反春标记：1=无，2=春天或反春 |

### behavior_bet（行为倍数）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.behavior_bet.landlord` | tinyint | 地主加倍：1=不加倍，2=加倍，4=超级加倍 |
| `$.behavior_bet.farmer1` | tinyint | 农民1加倍：1=不加倍，2=加倍，4=超级加倍 |
| `$.behavior_bet.farmer2` | tinyint | 农民2加倍：1=不加倍，2=加倍，4=超级加倍 |

## extend_content 字段结构说明（新格式）

`extend_content` 新格式为 JSON 字段，包含四个顶级分类：**牌信息**（`card_info`）、**牌力值**（`card_power`）、**用户属性**（`user_attr`）、**AI 等级**（`ai_level`）。

### card_info（牌信息）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.card_info.hand_cards` | string | 手牌 |
| `$.card_info.bottom_cards` | string | 底牌 |
| `$.card_info.shuffle_type` | int | 配牌类型: 0=随机发牌，201=新手保护机器人，202=充值保护机器人匹配，203=老用户每日前N，204=房间前N次触发机器人保护，205=低保次数触发机器人保护，206=连输保护机器人匹配，207=连输银两触发机器人匹配，默认为0 |
| `$.card_info.card_id` | int | 牌库编号（默认0，0为随机牌；>0时代表有配牌，对应牌库编号） |

### card_power（牌力值）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.card_power.card_power` | int | 初始牌分 |
| `$.card_power.card_power_final` | int | 算上底牌后的牌分 |
| `$.card_power.cost_time` | int | 重洗牌花费时间（ms） |
| `$.card_power.is_pass` | boolean | 是否重洗成功 |
| `$.card_power.shuffle_times` | tinyint | 重洗次数：-1=未开启重洗，0=未重洗过，1=重洗过1次，依此类推 |

### user_attr（用户属性）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.user_attr.bout` | int | 用户历史局数（斗地主全局下首局时记录为0） |
| `$.user_attr.mode_bout` | int | 用户该玩法的历史局数（该玩法首局时记录为0） |

### ai_level（AI 等级详细字段）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.ai_level.type` | tinyint | 机器人类型：0=逻辑机器人，1=算法机器人 |
| `$.ai_level.callflag` | tinyint | 叫地主强度：1=强化叫地主，2=监督叫地主，默认0 |
| `$.ai_level.robflag` | tinyint | 抢地主强度：1=强化抢地主，2=监督抢地主，默认0 |
| `$.ai_level.doubleflag` | tinyint | 加倍强度：1=强化加倍，2=监督加倍，默认0 |
| `$.ai_level.throwtileflag` | tinyint | 打牌强度：1=强化打牌，2=中等监督打牌，3=多风格打牌，默认0 |

### 示例值（新格式）

```json
{
  "card_info": {
    "hand_cards": "3455677888999JJJQQQKKKAAA222",
    "bottom_cards": "345",
    "shuffle_type": 201,
    "card_id": 1001
  },
  "card_power": {
    "card_power": 15,
    "card_power_final": 22,
    "cost_time": 0,
    "is_pass": false,
    "shuffle_times": 0
  },
  "user_attr": {
    "bout": 2,
    "mode_bout": 1
  },
  "ai_level": {
    "type": 1,
    "callflag": 1,
    "robflag": 1,
    "doubleflag": 2,
    "throwtileflag": 1
  }
}
```

> **字段缺失说明**：`extend_content` 中的各顶级分类及子字段并非 100% 存在，可能单独缺失或部分缺失。查询时建议使用 `get_json_int` / `get_json_string` 并配合 `COALESCE` 或 `IFNULL` 处理 NULL 值。

---

## extend_content 字段结构说明（旧格式）

旧格式为线上当前实际存储的结构，包含两个顶级分类：**牌力值**（`card_power`）、**用户保护和AI 等级**（`new_user_protect`）。与新格式的主要差异：

| 对比项 | 新格式 | 旧格式 |
| ---- | ---- | ---- |
| 顶级分类 | 4 个（card_info / card_power / user_attr / ai_level） | 2 个（card_power / new_user_protect） |
| 手牌信息 | `$.card_info.hand_cards`、`$.card_info.bottom_cards` | 无此分类 |
| 配牌类型 | `$.card_info.shuffle_type` | `$.new_user_protect.protect_type` |
| 牌库编号 | `$.card_info.card_id` | `$.new_user_protect.card_id` |
| 用户局数 | `$.user_attr.bout`、`$.user_attr.mode_bout` | `$.new_user_protect.bout` |
| AI 等级 | `$.ai_level.*`（标准 JSON 对象） | `$.new_user_protect.ai_level`（⚠️ 残缺 JSON 字符串） |
| 炸弹信息 | 无 | `$.card_power.bomb_cnt`、`$.card_power.bomb_final` |

### card_power（牌力值）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.card_power.card_power` | int | 初始牌分 |
| `$.card_power.card_power_final` | int | 算上底牌后的牌分 |
| `$.card_power.bomb_cnt` | int | 拥有炸弹个数（旧格式独有） |
| `$.card_power.bomb_final` | int | 算上底牌后的炸弹个数（旧格式独有） |
| `$.card_power.cost_time` | int | 重洗牌花费时间（ms） |
| `$.card_power.is_pass` | boolean | 是否重洗成功 |
| `$.card_power.shuffle_times` | tinyint | 重洗次数：-1=未开启重洗，0=未重洗过，1=重洗过1次，依此类推 |

### new_user_protect（用户保护和AI 等级详细字段）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.new_user_protect.ai_level` | string | AI 等级配置（⚠️ 线上为残缺 JSON 字符串，非标准 JSON 对象。内含 callflag/robflag/doubleflag/throwtileflag，含义同新格式 `$.ai_level.*`） |
| `$.new_user_protect.bout` | int | 用户历史局数（斗地主全局下首局时记录为0） |
| `$.new_user_protect.card_id` | int | 牌库编号（默认0，0为随机牌；>0时代表有配牌，对应牌库编号） |
| `$.new_user_protect.protect_type` | int | 配牌类型: 0=随机发牌，201=新手保护机器人，202=充值保护机器人匹配，203=老用户每日前N，204=房间前N次触发机器人保护，205=低保次数触发机器人保护，206=连输保护机器人匹配，207=连输银两触发机器人匹配，默认为0 |
| `$.new_user_protect.err_pool_id` | int | 错误池编号 |
| `$.new_user_protect.faild_reason` | int | 失败原因 |
| `$.new_user_protect.road` | int | 路径编号 |

**ai_level 内部字段含义**（需用正则提取，无法用 `get_json_int` 解析）：

| 字段名 | 类型 | 说明 |
| ---- | ---- | ---- |
| callflag | tinyint | 叫地主强度：1=强化叫地主，2=监督叫地主，默认0 |
| robflag | tinyint | 抢地主强度：1=强化抢地主，2=监督抢地主，默认0 |
| doubleflag | tinyint | 加倍强度：1=强化加倍，2=监督加倍，默认0 |
| throwtileflag | tinyint | 打牌强度：1=强化打牌，2=中等监督打牌，3=多风格打牌，默认0 |

提取示例：`REGEXP_EXTRACT(extend_content, 'callflag[^0-9]*(\\d+)', 1)`

### 示例值（旧格式）

```json
{
    "card_power": {
        "bomb_cnt": 0,
        "bomb_final": 0,
        "card_power": -17,
        "card_power_final": -17,
        "cost_time": 0,
        "is_pass": true,
        "shuffle_times": 2
    },
    "new_user_protect": {
        "ai_level": "{\"callflag\":2,robflag\":2,doubleflag\":2,throwtileflag\":2\"}",
        "bout": 2,
        "card_id": 11,
        "err_pool_id": 0,
        "faild_reason": 0,
        "protect_type": 201,
        "road": 1
    }
}
```

## 构建 SQL

```sql
CREATE TABLE tcy_temp.ddz_daily_game_raw (
  `game_id` int(11) NULL COMMENT "游戏ID",
  `dt` DATE NOT NULL COMMENT "对局日期",
  `uid` int(11) NOT NULL COMMENT "玩家ID",
  `game_datetime` datetime NOT NULL COMMENT "对局时间",
  `resultguid` varchar(64) NULL COMMENT "本局战绩ID",
  `timecost` int(11) NULL COMMENT "对局耗时（秒）",
  `room_id` int(11) NULL COMMENT "房间号",
  `room_currency_lower` bigint(20) NULL COMMENT "进入房间所需最少携带货币",
  `room_currency_upper` bigint(20) NULL COMMENT "进入房间最大携带货币",
  `robot` tinyint(4) NULL COMMENT "机器人标记：1=机器人，其他=真人",
  `role` tinyint(4) NULL COMMENT "角色：1=地主，2=农民",
  `chairno` tinyint(4) NULL COMMENT "座位号（0/1/2）",
  `result_id` tinyint(4) NULL COMMENT "结果：1=获胜，2=失败",
  `basedeposit` int(11) NULL COMMENT "银子玩法房间底分(银子)",
  `olddeposit` bigint(20) NULL COMMENT "银子玩法对局前银子数量",
  `end_deposit` bigint(20) NULL COMMENT "银子玩法对局后银子数量",
  `fee` int(11) NULL COMMENT "银子玩法对局服务费(银子)",
  `depositdiff` bigint(20) NULL COMMENT "银子玩法银子变动数量(含服务费)",
  `basescore` int(11) NULL COMMENT "积分玩法房间底分(积分)",
  `oldscore` bigint(20) NULL COMMENT "积分玩法对局前积分数量",
  `end_score` bigint(20) NULL COMMENT "积分玩法对局后积分数量",
  `score_fee` int(11) NULL COMMENT "积分玩法对局服务费(积分)",
  `scorediff` bigint(20) NULL COMMENT "积分玩法积分变动数量(含服务费)",
  `cut` int(11) NULL COMMENT "逃跑罚没货币（!=0代表存在逃跑行为）",
  `safebox_deposit` int(11) NULL COMMENT "保险箱存银",
  `magnification` int(11) NULL COMMENT "个人理论总倍数",
  `magnification_stacked` int(11) NULL COMMENT "个人加倍：1=不加倍，2=加倍，4=超级加倍",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `group_id` int(11) NULL COMMENT "分端ID",
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `app_code` varchar(32) NULL COMMENT "应用code",
  `afk_turn_cnt` int(11) NULL COMMENT "托管出牌次数",
  `magnification_subdivision` varchar(512) NULL COMMENT "倍数细分（公共倍数+行为倍数）",
  `extend_content` varchar(512) NULL COMMENT "扩展信息（牌信息+牌力值+用户属性+AI等级）"
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
按天 `DELETE + INSERT`（幂等可重跑），脚本：[`batch_insert_ddz_daily_game_raw.py`](../../../ops/py/batch_insert_ddz_daily_game_raw.py)

> **依赖**：hive_catalog_cdh5.dwd.fact_game_combatgains 对应日期需先回填。

```powershell
# 单天
py -3 -u .\batch_insert_ddz_daily_game_raw.py --start 20260617 --end 20260617

# 区间回填
py -3 -u .\batch_insert_ddz_daily_game_raw.py --start 2026-06-01 --end 2026-06-08

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_ddz_daily_game_raw.py --start 20260617 --end 20260617 --dry-run
```

```sql
INSERT INTO tcy_temp.ddz_daily_game_raw
SELECT
    game_id, dt, uid, FROM_UNIXTIME(time_unix / 1000) AS game_datetime, resultguid, timecost,
    room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
    basedeposit, olddeposit, end_deposit, fee, depositdiff,
    basescore, oldscore, end_score, score_fee, scorediff,
    cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, IFNULL(app_id, 1880053), app_code, 
    afk_turn_cnt, magnification_subdivision, extend_content
FROM hive_catalog_cdh5.dwd.fact_game_combatgains
WHERE game_id = 53
  AND dt BETWEEN 20260515 AND 20260518
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../../ops/daily_data_ops.md)

## 使用示例

### 1. 计算用户银子经济变化

```sql
SELECT
    uid,
    SUM(depositdiff) AS total_deposit_diff,
    SUM(fee) AS total_fee,
    SUM(depositdiff) + SUM(fee) AS net_win_lose,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    SUM(CASE WHEN result_id = 2 THEN 1 ELSE 0 END) AS lose_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.ddz_daily_game_raw
WHERE game_id = 53
  AND dt BETWEEN '2026-03-11' AND '2026-05-27'
  AND robot != 1               -- 仅真人
  AND basedeposit > 0          -- 银子玩法
GROUP BY uid;
```

### 2. 高倍局分析

```sql
SELECT
    CASE WHEN magnification > 24 THEN '高倍' ELSE '非高倍' END AS multi_type,
    COUNT(*) AS game_count,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(magnification), 2) AS avg_magnification,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.ddz_daily_game_raw
WHERE game_id = 53
  AND dt BETWEEN '2026-03-11' AND '2026-05-27'
  AND robot != 1               -- 仅真人
  AND basedeposit > 0          -- 银子玩法
GROUP BY CASE WHEN magnification > 24 THEN '高倍' ELSE '非高倍' END
ORDER BY multi_type;
```

### 3. 炸弹频率分析（从 JSON 提取炸弹倍数）

```sql
SELECT
    CASE
        WHEN get_json_int(magnification_subdivision, '$.public_bet.bomb_bet') > 1 THEN '有炸弹'
        ELSE '无炸弹'
    END AS bomb_type,
    COUNT(*) AS game_count,
    ROUND(AVG(magnification), 2) AS avg_magnification,
    ROUND(AVG(timecost), 1) AS avg_timecost
FROM tcy_temp.ddz_daily_game_raw
WHERE game_id = 53
  AND dt BETWEEN '2026-03-11' AND '2026-05-27'
  AND robot != 1               -- 仅真人
  AND basedeposit > 0          -- 银子玩法
GROUP BY CASE
    WHEN get_json_int(magnification_subdivision, '$.public_bet.bomb_bet') > 1 THEN '有炸弹'
    ELSE '无炸弹'
END
ORDER BY bomb_type;
```

### 4. 提取牌力值与配牌类型分析

```sql
SELECT
    IFNULL(get_json_int(extend_content, '$.card_info.shuffle_type'), 0) AS shuffle_type,
    COUNT(*) AS game_count,
    ROUND(AVG(IFNULL(get_json_int(extend_content, '$.card_power.card_power'), 0)), 2) AS avg_card_power,
    ROUND(AVG(IFNULL(get_json_int(extend_content, '$.card_power.card_power_final'), 0)), 2) AS avg_card_power_final,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.ddz_daily_game_raw
WHERE game_id = 53
  AND dt BETWEEN '2026-03-11' AND '2026-05-27'
  AND robot != 1               -- 仅真人
GROUP BY IFNULL(get_json_int(extend_content, '$.card_info.shuffle_type'), 0)
ORDER BY shuffle_type;
```

### 5. 倍数一致性校验（验证 `magnification` 与各倍数字段是否匹配）

**公共倍数计算逻辑**：

- `initial_bet × grab_landlord_bet × 2^(bomb_bet/2) × complete_victory_bet`
- `bomb_bet = 1` 时无炸弹（`2^0 = 1`），否则炸弹个数为 `bomb_bet/2`

**个人倍数计算逻辑**：

- **农民** = 公共倍数 × 地主加倍 × 自己加倍(`magnification_stacked`)
- **地主** = 公共倍数 × 自己加倍 × 农民1加倍 + 公共倍数 × 自己加倍 × 农民2加倍

```sql
WITH game_base AS (
    SELECT
        resultguid,
        uid,
        role,
        magnification,
        magnification_stacked,
        get_json_int(magnification_subdivision, '$.public_bet.initial_bet')
            * get_json_int(magnification_subdivision, '$.public_bet.grab_landlord_bet')
            * POW(2, IF(get_json_int(magnification_subdivision, '$.public_bet.bomb_bet') = 1, 0,
                get_json_int(magnification_subdivision, '$.public_bet.bomb_bet') / 2))
            * get_json_int(magnification_subdivision, '$.public_bet.complete_victory_bet') AS public_mult,
        get_json_int(magnification_subdivision, '$.behavior_bet.landlord') AS landlord_bet
    FROM tcy_temp.ddz_daily_game_raw
    WHERE game_id = 53
      AND dt BETWEEN '2026-05-15' AND '2026-05-18'
      AND magnification_subdivision IS NOT NULL
),
farmer_sum AS (
    SELECT resultguid, SUM(magnification_stacked) AS farmer_stacked_sum
    FROM game_base
    WHERE role = 2
    GROUP BY resultguid
)
SELECT
    gb.resultguid,
    gb.uid,
    gb.role,
    gb.magnification,
    CASE
        WHEN gb.role = 2 THEN gb.public_mult * gb.landlord_bet * gb.magnification_stacked
        WHEN gb.role = 1 THEN gb.public_mult * gb.magnification_stacked * fs.farmer_stacked_sum
    END AS calc_magnification
FROM game_base gb
LEFT JOIN farmer_sum fs ON gb.resultguid = fs.resultguid
WHERE gb.magnification != (
    CASE
        WHEN gb.role = 2 THEN gb.public_mult * gb.landlord_bet * gb.magnification_stacked
        WHEN gb.role = 1 THEN gb.public_mult * gb.magnification_stacked * fs.farmer_stacked_sum
    END
)
LIMIT 100;
```

## 字段使用注意

1. **银子/积分玩法区分**：
   - 积分玩法：`room_id IN (11534, 14238, 15458, 158, 159)`，货币相关字段使用 `basescore`、`oldscore`、`end_score`、`score_fee`、`scorediff`
   - 银子玩法：`room_id NOT IN (11534, 14238, 15458, 158, 159)`，货币相关字段使用 `basedeposit`、`olddeposit`、`end_deposit`、`fee`、`depositdiff`
   - 混合分析时需注意货币单位不同

2. **货币变动字段**：
   - `depositdiff` / `scorediff` 为含服务费的变动值
   - 如需净输赢（不含服务费），银子玩法使用 `depositdiff + fee`，积分玩法使用 `scorediff + score_fee`

3. **JSON 倍数提取**：
   - 使用 `get_json_int` 函数从 `magnification_subdivision` 提取，如 `get_json_int(magnification_subdivision, '$.public_bet.bomb_bet')`
   - 如果 JSON 无对应字段，返回 NULL（建议用 `IFNULL` 处理）

4. **JSON 扩展信息提取**：
   - 新格式：使用 `get_json_int` / `get_json_string` 从 `extend_content` 提取，如 `get_json_string(extend_content, '$.card_info.hand_cards')`
   - 旧格式：路径不同（如 `$.new_user_protect.protect_type` 对应新格式的 `$.card_info.shuffle_type`），且 `$.new_user_protect.ai_level` 为残缺 JSON 字符串，需用正则提取：`REGEXP_EXTRACT(extend_content, 'callflag[^0-9]*(\\d+)', 1)`
   - 各顶级分类及子字段可能缺失，建议配合 `IFNULL` 处理 NULL 值

5. **机器人标记**：
   - 表中包含机器人和真人数据，通过 `robot` 字段区分
   - `robot = 1` 为机器人，其他为真人
   - 分析时建议添加 `robot != 1` 条件过滤真人数据

## 表数据流向

```text
hive_catalog_cdh5.dwd.fact_game_combatgains        （Hive 原始对局日志，多货币字段）
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
