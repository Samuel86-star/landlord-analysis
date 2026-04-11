# DWS 中间表：每日对局战绩统一字段表

## 表基本信息

| 项目 | 说明 |
|------|------|
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_daily_game` |
| 全名 | `tcy_temp.dws_ddz_daily_game` |
| 类型 | DWS 层中间表（每日增量） |
| 描述 | 对局战绩统一字段表，将不同玩法的货币、倍数字段统一，简化分析查询 |
| 粒度 | resultguid + uid（一个对局的单个玩家一行） |

## 设计背景

原始 `dwd_game_combat_si` 战绩表存储了所有玩法的对局日志，但不同玩法使用不同的货币字段：

| 玩法 | 货币类型 | 底分字段 | 服务费字段 | 对局前货币 | 对局后货币 | 输赢字段 |
|------|---------|---------|-----------|-----------|-----------|---------|
| 经典/不洗牌/癞子 | 银子 | `basedeposit` | `fee` | `olddeposit` | `end_deposit` | `depositdiff` |
| 积分/好友房/比赛 | 积分 | `basescore` | `score_fee` | `oldscore` | `end_score` | `scorediff` |

此外，个人操作倍数存储在 JSON 字段 `magnification_subdivision` 中，需要解析提取。

**解决方案**：
1. 统一货币字段命名：`start_money`、`end_money`、`diff_money_pre_tax`
2. 统一房间底分和服务费：`room_base`、`room_fee`
3. 添加玩法分类字段：`play_mode`
4. 提取 JSON 倍数字段到独立列：`grab_landlord_bet`、`complete_victory_bet`、`bomb_bet`
5. 计算实际输赢倍数：`real_magnification`

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
|--------|------|------|--------|
| dt | bigint | 对局日期（YYYYMMDD） | 20260408 |
| time_unix | bigint | 对局时间戳（毫秒级） | 1712577000000 |
| resultguid | string | 本局战绩 ID | "abc123xyz" |
| timecost | int | 对局耗时（秒） | 180 |
| room_id | int | 房间号 | 1001 |
| play_mode | int | 玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房，0=其他 | 1 |
| room_base | int | 房间底分（统一字段） | 100 |
| room_fee | int | 房间服务费（统一字段） | 10 |
| room_currency_lower | bigint | 进入房间所需最少携带货币 | 1000 |
| room_currency_upper | bigint | 进入房间最大携带货币 | 10000 |
| uid | bigint | 玩家 ID | 123456789 |
| robot | int | 机器人标记：1=机器人，其他=真人 | 0 |
| role | int | 角色：1=地主，2=农民 | 1 |
| chairno | int | 座位号（0/1/2） | 0 |
| result_id | int | 结果：1=获胜，2=失败 | 1 |
| start_money | bigint | 对局前货币数量（统一字段） | 5000 |
| end_money | bigint | 对局后货币数量（统一字段） | 5500 |
| diff_money_pre_tax | bigint | 还原服务费前的对局输赢（统一字段） | 500 |
| cut | bigint | 逃跑罚没货币（<0 代表存在逃跑行为） | 0 |
| safebox_deposit | bigint | 保险箱存银 | 1000 |
| magnification | int | 个人理论总倍数 | 12 |
| magnification_stacked | int | 个人加倍：1=不加倍，2=加倍，4=超级加倍 | 2 |
| real_magnification | double | 本局实际输赢倍数（含服务费还原） | 5.0 |
| grab_landlord_bet | int | 抢地主倍数：3=无人抢，6=1人抢，12=2人抢 | 6 |
| complete_victory_bet | int | 春天/反春标记：2=存在 | 0 |
| bomb_bet | int | 炸弹倍数，炸弹个数 = bomb_bet/2 | 4 |
| channel_id | bigint | 渠道号 | 1001 |
| group_id | bigint | 分端 ID | 6 |
| app_id | bigint | 应用 ID | 1880053 |
| game_id | bigint | 游戏 ID | 53 |

## 玩法分类说明 (play_mode)

| play_mode | 玩法 | room_id 列表 | 备注 |
|-----------|------|-------------|------|
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
CREATE TABLE tcy_temp.dws_ddz_daily_game
AS
SELECT 
    dt,
    time_unix,
    resultguid,
    timecost,
    room_id,
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
    room_currency_lower,
    room_currency_upper,
    uid,
    robot,
    role,
    chairno,
    result_id,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN oldscore ELSE olddeposit END AS start_money,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN end_score ELSE end_deposit END AS end_money,
    CASE 
        WHEN room_id IN (11534,14238,15458,158,159) THEN scorediff + score_fee 
        ELSE depositdiff + fee 
    END AS diff_money_pre_tax,
    cut,
    safebox_deposit,
    magnification,
    magnification_stacked,
    CASE 
        WHEN room_id IN (11534,14238,15458,158,159) 
        THEN ROUND((scorediff + score_fee) / NULLIF(basescore, 0), 2)
        ELSE ROUND((depositdiff + fee) / NULLIF(basedeposit, 0), 2)
    END AS real_magnification,
    get_json_int(magnification_subdivision, '$.public_bet.grab_landlord_bet') AS grab_landlord_bet,
    get_json_int(magnification_subdivision, '$.public_bet.complete_victory_bet') AS complete_victory_bet,
    get_json_int(magnification_subdivision, '$.public_bet.bomb_bet') AS bomb_bet,
    channel_id,
    group_id,
    app_id,
    game_id
FROM tcy_dwd.dwd_game_combat_si
WHERE game_id = 53
  AND dt BETWEEN 20260210 AND 20260508;
```

## 更新SQL

```sql
INSERT INTO tcy_temp.dws_ddz_daily_game
SELECT 
    dt,
    time_unix,
    resultguid,
    timecost,
    room_id,
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
    room_currency_lower,
    room_currency_upper,
    uid,
    robot,
    role,
    chairno,
    result_id,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN oldscore ELSE olddeposit END AS start_money,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN end_score ELSE end_deposit END AS end_money,
    CASE 
        WHEN room_id IN (11534,14238,15458,158,159) THEN scorediff + score_fee 
        ELSE depositdiff + fee 
    END AS diff_money_pre_tax,
    cut,
    safebox_deposit,
    magnification,
    magnification_stacked,
    CASE 
        WHEN room_id IN (11534,14238,15458,158,159) 
        THEN ROUND((scorediff + score_fee) / NULLIF(basescore, 0), 2)
        ELSE ROUND((depositdiff + fee) / NULLIF(basedeposit, 0), 2)
    END AS real_magnification,
    get_json_int(magnification_subdivision, '$.public_bet.grab_landlord_bet') AS grab_landlord_bet,
    get_json_int(magnification_subdivision, '$.public_bet.complete_victory_bet') AS complete_victory_bet,
    get_json_int(magnification_subdivision, '$.public_bet.bomb_bet') AS bomb_bet,
    channel_id,
    group_id,
    app_id,
    game_id
FROM tcy_dwd.dwd_game_combat_si
WHERE game_id = 53
  AND dt BETWEEN 20260401 AND 20260408;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../ops/daily_data_ops.md)

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
    ROUND(AVG(ABS(real_magnification)), 2) AS avg_real_magnification
FROM tcy_temp.dws_ddz_daily_game
WHERE dt BETWEEN 20260210 AND 20260210
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)  -- 仅银子玩法
GROUP BY play_mode
ORDER BY play_mode;
```

### 2. 计算用户首日经济变化
```sql
SELECT
    uid,
    SUM(diff_money_pre_tax) AS total_diff_money,
    SUM(room_fee) AS total_fee,
    SUM(diff_money_pre_tax) - SUM(room_fee) AS net_diff_money,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    SUM(CASE WHEN result_id = 2 THEN 1 ELSE 0 END) AS lose_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_ddz_daily_game
WHERE dt = 20260210
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
    ROUND(AVG(ABS(real_magnification)), 2) AS avg_real_multi,
    SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) AS win_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS win_rate
FROM tcy_temp.dws_ddz_daily_game
WHERE dt BETWEEN 20260210 AND 20260215
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
    ROUND(AVG(ABS(real_magnification)), 2) AS avg_real_magnification,
    ROUND(AVG(timecost), 1) AS avg_timecost
FROM tcy_temp.dws_ddz_daily_game
WHERE dt BETWEEN 20260210 AND 20260215
  AND robot != 1               -- 仅真人
  AND play_mode IN (1, 2, 3)
GROUP BY play_mode, CASE WHEN bomb_bet > 0 THEN '有炸弹' ELSE '无炸弹' END;
```

## 注意事项

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

5. **时间范围**：
   - 默认覆盖 `20260210` 至 `20260508`（注册期 + Day30 观测期）
   - 可根据实际需求调整

6. **机器人标记**：
   - 表中包含机器人和真人数据，通过 `robot` 字段区分
   - `robot = 1` 为机器人，其他为真人
   - 分析时建议添加 `robot != 1` 条件过滤真人数据

## 与原始表的字段映射

| 统一字段 | 经典/不洗牌/癞子玩法 | 积分/比赛/好友房玩法 |
|---------|---------------------|---------------------|
| `room_base` | `basedeposit` | `basescore` |
| `room_fee` | `fee` | `score_fee` |
| `start_money` | `olddeposit` | `oldscore` |
| `end_money` | `end_deposit` | `end_score` |
| `diff_money_pre_tax` | `depositdiff + fee` | `scorediff + score_fee` |

## 与其他 DWS 表的关系

```
tcy_dwd.dwd_game_combat_si        （原始对局日志，多货币字段）
            ↓  字段统一
tcy_temp.dws_ddz_daily_game       （统一字段对局表）
            ↓  聚合分析
tcy_temp.dws_dq_app_daily_reg        （APP 端注册用户宽表）
tcy_temp.dws_dq_daily_login       （每日登录聚合表）
```

> **文档版本**：v1.0
> **创建时间**：2026-04-09
> **更新说明**：
> - v1.0：初始版本，统一货币字段、添加玩法分类、提取 JSON 倍数字段