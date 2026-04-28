# DWS 中间表：首日对局战绩统一字段表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_firstday_game` |
| 全名 | `tcy_temp.dws_ddz_firstday_game` |
| 类型 | DWS 层中间表（首日快照） |
| 描述 | 首日对局战绩统一字段表，仅存储注册当日的游戏战绩，用于分析新用户首日游戏行为 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |

## 设计背景

原始 `dwd_game_combat_si` 战绩表存储了所有玩法的对局日志，但不同玩法使用不同的货币字段：

| 玩法 | 货币类型 | 底分字段 | 服务费字段 | 对局前货币 | 对局后货币 | 输赢字段 |
| ---- | ------- | ------- | --------- | --------- | --------- | ------- |
| 经典/不洗牌/癞子 | 银子 | `basedeposit` | `fee` | `olddeposit` | `end_deposit` | `depositdiff` |
| 积分/好友房/比赛 | 积分 | `basescore` | `score_fee` | `oldscore` | `end_score` | `scorediff` |

此外，个人操作倍数存储在 JSON 字段 `magnification_subdivision` 中，需要解析提取。

**解决方案**：

1. 统一货币字段命名：`start_money`、`end_money`、`diff_money_pre_tax`
2. 统一房间底分和服务费：`room_base`、`room_fee`
3. 添加玩法分类字段：`play_mode`
4. 提取 JSON 倍数字段到独立列：`grab_landlord_bet`、`complete_victory_bet`、`bomb_bet`
5. 计算实际输赢倍数：`real_magnification`
6. 关联注册表，仅筛选注册当日对局

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| dt | date | 游戏日期（同时也是注册日期） | 2026-04-08 |
| uid | int | 玩家 ID | 123456789 |
| game_datetime | datetime | 对局时间 | 2026-04-08 10:30:00 |
| resultguid | varchar(64) | 本局战绩 ID | "abc123xyz" |
| timecost | int | 对局耗时（秒） | 180 |
| room_id | int | 房间号 | 1001 |
| play_mode | tinyint | 玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房，0=其他 | 1 |
| room_base | int | 房间底分（统一字段） | 100 |
| room_fee | int | 房间服务费（统一字段） | 10 |
| room_currency_lower | bigint | 进入房间所需最少携带货币 | 1000 |
| room_currency_upper | bigint | 进入房间最大携带货币 | 10000 |
| robot | tinyint | 机器人标记：1=机器人，其他=真人 | 0 |
| role | tinyint | 角色：1=地主，2=农民 | 1 |
| chairno | tinyint | 座位号（0/1/2） | 0 |
| result_id | tinyint | 结果：1=获胜，2=失败 | 1 |
| start_money | bigint | 对局前货币数量（统一字段） | 5000 |
| end_money | bigint | 对局后货币数量（统一字段） | 5500 |
| diff_money_pre_tax | bigint | 还原服务费前的对局输赢（统一字段） | 500 |
| cut | int | 逃跑罚没货币（<0 代表存在逃跑行为） | 0 |
| safebox_deposit | int | 保险箱存银 | 1000 |
| magnification | int | 个人理论总倍数 | 12 |
| magnification_stacked | int | 个人加倍：1=不加倍，2=加倍，4=超级加倍 | 2 |
| real_magnification | double | 本局实际输赢倍数（含服务费还原） | 5.0 |
| grab_landlord_bet | tinyint | 抢地主倍数：3=无人抢，6=1人抢，12=2人抢 | 6 |
| complete_victory_bet | tinyint | 春天/反春标记：2=存在 | 0 |
| bomb_bet | int | 炸弹倍数，炸弹个数 = bomb_bet/2 | 4 |
| channel_id | int | 渠道号 | 1001 |
| group_id | int | 分端 ID | 6 |
| app_code | varchar(32) | 应用code | zgda |
| game_id | int | 游戏 ID | 53 |

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
CASE WHEN room_id IN (11534,14238,15458,158,159) THEN scorediff + score_fee ELSE depositdiff + fee END AS diff_money_pre_tax
```

## 构建 SQL

```sql
CREATE TABLE tcy_temp.dws_ddz_firstday_game (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `game_datetime` datetime NOT NULL COMMENT "对局时间",
  `resultguid` varchar(64) NULL COMMENT "对局GUID",
  `timecost` int(11) NULL COMMENT "耗时",
  `room_id` int(11) NULL COMMENT "房间ID",
  `play_mode` tinyint(4) NULL COMMENT "玩法模式",
  `room_base` int(11) NULL COMMENT "底分",
  `room_fee` int(11) NULL COMMENT "台费",
  `room_currency_lower` bigint(20) NULL,
  `room_currency_upper` bigint(20) NULL,
  `robot` tinyint(4) NULL COMMENT "是否机器人",
  `role` tinyint(4) NULL COMMENT "角色",
  `chairno` tinyint(4) NULL COMMENT "座位号",
  `result_id` tinyint(4) NULL COMMENT "结果ID",
  `start_money` bigint(20) NULL,
  `end_money` bigint(20) NULL,
  `diff_money_pre_tax` bigint(20) NULL COMMENT "输赢数值",
  `cut` int(11) NULL,
  `safebox_deposit` int(11) NULL,
  `magnification` int(11) NULL COMMENT "倍数",
  `magnification_stacked` int(11) NULL,
  `real_magnification` double NULL COMMENT "实际倍数",
  `grab_landlord_bet` tinyint(4) NULL,
  `complete_victory_bet` tinyint(4) NULL,
  `bomb_bet` int(11) NULL,
  `channel_id` int(11) NULL,
  `group_id` int(11) NULL,
  `app_code` varchar(32) NULL,
  `game_id` int(11) NULL
) ENGINE=OLAP 
DUPLICATE KEY(`app_id`, `dt`, `uid`)
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
    "dynamic_partition.start" = "-80", 
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);
```

## 更新SQL

```sql
INSERT INTO tcy_temp.dws_ddz_firstday_game
SELECT 
    g.app_id,
    g.dt,
    g.uid, g.game_datetime, g.resultguid, g.timecost, g.room_id, g.play_mode,
    g.room_base, g.room_fee, g.room_currency_lower, g.room_currency_upper,
    g.robot, g.role, g.chairno, g.result_id,
    g.start_money, g.end_money, g.diff_money_pre_tax,
    g.cut, g.safebox_deposit, g.magnification, g.magnification_stacked, g.real_magnification,
    g.grab_landlord_bet, g.complete_victory_bet, g.bomb_bet,
    g.channel_id, g.group_id, g.app_code, g.game_id
FROM tcy_temp.dws_ddz_daily_game g
INNER JOIN tcy_temp.dws_dq_daily_reg r 
    ON r.app_id = g.app_id AND r.uid = g.uid AND r.reg_date = g.dt
WHERE g.dt BETWEEN '2026-02-10' AND '2026-04-22';
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

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
    ROUND(AVG(ABS(real_magnification)), 2) AS avg_real_magnification
FROM tcy_temp.dws_ddz_firstday_game
WHERE reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)  -- 仅银子玩法
GROUP BY play_mode
ORDER BY play_mode;
```

### 2. 计算用户首日经济变化

```sql
SELECT
    uid,
    reg_date,
    COUNT(*) AS first_day_game_cnt,
    SUM(diff_money_pre_tax) AS total_diff_money,
    SUM(room_fee) AS total_fee,
    SUM(diff_money_pre_tax) - SUM(room_fee) AS net_diff_money,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    SUM(CASE WHEN result_id = 2 THEN 1 ELSE 0 END) AS lose_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_ddz_firstday_game
WHERE reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)  -- 仅银子玩法
GROUP BY uid, reg_date;
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
    WHERE reg_date BETWEEN '2026-02-10' AND '2026-04-22'
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
    ROUND(AVG(ABS(real_magnification)), 2) AS avg_real_multi,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ROUND(AVG(room_base), 2) AS avg_room_base
FROM tcy_temp.dws_ddz_firstday_game g
INNER JOIN (
    SELECT uid
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY uid
    HAVING COUNT(*) BETWEEN 1 AND 5
) low_players ON g.app_id = 1880053 AND g.uid = low_players.uid
WHERE g.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND g.robot != 1
  AND g.play_mode IN (1, 2, 3)
GROUP BY play_mode;
```

## 字段使用注意

1. **货币类型区分**：
   - `play_mode IN (1, 2, 3)`：银子玩法，货币单位为银子
   - `play_mode IN (4, 5, 6)`：积分玩法，货币单位为积分
   - 混合分析时需注意货币单位不同

2. **diff_money_pre_tax 字段**：
   - 该字段已还原服务费，即 `输赢 + 服务费`
   - 如需净输赢（不含服务费），应使用 `diff_money_pre_tax - room_fee`

3. **real_magnification 字段**：
   - 计算公式：`(diff_money_pre_tax) / room_base`
   - 反映实际输赢相对于底分的倍数
   - **可能为负数**（输局时），求平均时需使用 `AVG(ABS(real_magnification))`

4. **JSON 倍数提取**：
   - 使用 `get_json_int` 函数从 `magnification_subdivision` 提取
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
| `diff_money_pre_tax` | `depositdiff + fee` | `scorediff + score_fee` |

## 表数据流向

```structure
tcy_dwd.dwd_game_combat_si        （原始对局日志，多货币字段）
            ↓  字段统一
tcy_temp.dws_ddz_daily_game       （统一字段对局表，每日增量）
            ↓  关联注册表筛选首日
tcy_temp.dws_ddz_firstday_game    （首日对局战绩表）
            ↓  聚合分析
tcy_temp.dws_dq_daily_reg         （注册表）
tcy_temp.dws_dq_daily_login       （登录表）
```

> **文档版本**：v1.0
> **创建时间**：2026-04-24
> **更新说明**：

- v1.0：初始版本，参照 dws_ddz_daily_game 结构，限定首日数据范围
