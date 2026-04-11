# 每日数据增量更新操作手册

> 本文档汇总所有 DWS 层中间表的增量数据更新 SQL，供日常运维使用。

---

## 目录

1. [注册数据增量更新](#1-注册数据增量更新)
2. [登录数据增量更新](#2-登录数据增量更新)
3. [APP 端注册用户宽表增量更新](#3-app-端注册用户宽表增量更新)
4. [对局数据增量更新](#4-对局数据增量更新)
5. [用户每日游戏行为聚合增量更新](#5-用户每日游戏行为聚合增量更新)
6. [执行顺序与依赖关系](#6-执行顺序与依赖关系)
7. [常见问题](#7-常见问题)

---

## 1. 注册数据增量更新

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st` | `tcy_temp.dws_dq_daily_reg` |

### 增量更新 SQL

```sql
-- 注册数据增量导入
-- 参数：将 dt 替换为实际日期
INSERT INTO tcy_temp.dws_dq_daily_reg
SELECT
    uid,
    app_id,
    FROM_UNIXTIME(first_login_ts / 1000) AS reg_datetime,  -- 毫秒级时间戳转 datetime
    dt AS reg_date
FROM hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st
WHERE app_id = 1880053
  AND dt = ${DATE};  -- 替换为实际日期，如 20260409
```

### 说明

- `first_login_ts` 为毫秒级时间戳，需除以 1000 转换为秒级
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_dq_daily_reg.md](../dws/dws_dq_daily_reg.md)

---

## 2. 登录数据增量更新

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_dwd.dwd_tcy_userlogin_si` | `tcy_temp.dws_dq_daily_login` |

### 增量更新 SQL

```sql
-- 登录数据增量导入（每日聚合）
-- 参数：将日期范围替换为实际日期
INSERT INTO tcy_temp.dws_dq_daily_login
SELECT 
    uid,
    app_id,
    DATE(dt) AS login_date,
    
    -- 首次登录维度
    MIN(dt) AS first_login_time,
    MIN_BY(channel_id, time_unix) AS first_channel_id,
    MIN_BY(group_id, time_unix) AS first_group_id,
    
    -- 最后登录维度
    MAX(dt) AS last_login_time,
    MAX_BY(channel_id, time_unix) AS last_channel_id,
    MAX_BY(group_id, time_unix) AS last_group_id,
    
    -- 最频繁维度
    CAST(SUBSTR(MAX(CONCAT(LPAD(CAST(cnt_channel AS STRING), 10, '0'), CAST(channel_id AS STRING))), 11) AS BIGINT) AS most_freq_channel_id,
    CAST(SUBSTR(MAX(CONCAT(LPAD(CAST(cnt_group AS STRING), 10, '0'), CAST(group_id AS STRING))), 11) AS BIGINT) AS most_freq_group_id,
    
    -- 统计维度
    COUNT(DISTINCT channel_id) AS channel_id_count,
    COUNT(DISTINCT group_id) AS group_id_count,
    COUNT(1) AS login_count
FROM (
    SELECT 
        *,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), channel_id) AS cnt_channel,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), group_id) AS cnt_group
    FROM tcy_dwd.dwd_tcy_userlogin_si
    WHERE app_id = 1880053
      AND dt >= '${DATE} 00:00:00'  -- 替换为实际日期，如 2026-04-09
      AND dt <= '${DATE} 23:59:59'
) t
GROUP BY uid, app_id, DATE(dt);
```

### 说明

- `time_unix` 为毫秒级时间戳，`MIN_BY/MAX_BY` 基于此排序
- 最频繁维度通过字符串拼接取 MAX 实现
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_dq_daily_login.md](../dws/dws_dq_daily_login.md)

---

## 3. APP 端注册用户宽表增量更新

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_temp.dws_dq_daily_reg`、`tcy_temp.dws_dq_daily_login` | `tcy_temp.dws_dq_app_daily_reg` |

### 增量更新 SQL

```sql
-- APP 端注册用户宽表增量导入
-- 参数：将 ${DATE} 替换为实际日期
INSERT INTO tcy_temp.dws_dq_app_daily_reg
SELECT 
    r.uid,
    r.app_id,
    r.reg_date,
    r.reg_datetime,
    COALESCE(l.first_group_id, -1) AS reg_group_id,
    COALESCE(l.first_channel_id, -1) AS reg_channel_id,
    COALESCE(chn.channel_category_id, -1) AS channel_category_id,
    COALESCE(chn.channel_category_name, '未知/日志丢失') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1) AS channel_category_tag_id,
    CASE WHEN l.uid IS NULL THEN 1 ELSE 0 END AS is_login_log_missing,
    COALESCE(l.login_count, 0) AS first_day_login_cnt
FROM tcy_temp.dws_dq_daily_reg r
INNER JOIN tcy_temp.dws_dq_daily_login l 
    ON r.uid = l.uid 
    AND r.app_id = l.app_id 
    AND CAST(DATE_FORMAT(l.login_date, '%Y%m%d') AS INT) = r.reg_date
LEFT JOIN tcy_temp.dws_channel_category_map chn 
    ON l.first_channel_id = chn.channel_id
WHERE r.app_id = 1880053
  AND r.reg_date between 20260210 and 20260410
  AND l.first_group_id IN (6, 66, 33, 44, 77, 99, 8, 88);
```

### 说明

- 依赖 `dws_dq_daily_reg` 和 `dws_dq_daily_login` 表，需在其之后执行
- 仅包含 APP 端注册用户（Android + iOS），通过 `reg_group_id` 过滤
- `is_login_log_missing = 1` 表示注册当日无登录日志
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_dq_app_daily_reg.md](../dws/dws_dq_app_daily_reg.md)

---

## 4. 对局数据增量更新

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_dwd.dwd_game_combat_si` | `tcy_temp.dws_ddz_daily_game` |

### 增量更新 SQL

```sql
-- 对局数据增量导入
-- 参数：将日期范围替换为实际日期
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

### 说明

- 包含机器人和真人数据，通过 `robot` 字段区分
- 玩法分类 `play_mode`：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房
- 货币字段已统一命名，简化后续分析
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_ddz_daily_game.md](../dws/dws_ddz_daily_game.md)

---

## 5. APP 端每日游戏行为统计增量更新

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_temp.dws_ddz_daily_game` | `tcy_temp.dws_ddz_appdaily_game_stat` |

### 增量更新 SQL

```sql
-- APP 端每日游戏行为统计增量导入
INSERT INTO tcy_temp.dws_ddz_appdaily_game_stat
WITH game_enriched AS (
    -- 1. 预处理：在单层扫描中完成基础过滤和窗口排序
    SELECT
        *,
        -- 确定玩家全天首局和末局顺序，为后续提取 start/end_money 做准备
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY time_unix ASC) AS rank_asc,
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY time_unix DESC) AS rank_desc,
        -- 为连胜连败计算准备：生成全天对局序号
        ROW_NUMBER() OVER (PARTITION BY uid ORDER BY time_unix ASC) AS game_seq
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt = ${DATE}  -- 替换为实际日期
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)  -- 仅 APP 端
      AND play_mode IN (1, 2, 3, 5)  -- 仅银子玩法
),
streaks_calc AS (
    -- 2. 连胜连败逻辑：利用 game_seq - 内部序号的差值分组（经典 Gaps and Islands 算法）
    SELECT 
        uid, 
        result_id,
        COUNT(*) AS streak_len
    FROM (
        SELECT 
            uid, 
            result_id,
            game_seq - ROW_NUMBER() OVER (PARTITION BY uid, result_id ORDER BY game_seq) AS grp
        FROM game_enriched
        WHERE result_id IN (1, 2)
    ) t
    GROUP BY uid, result_id, grp
),
max_streaks AS (
    -- 3. 汇总最大连胜连败
    SELECT 
        uid,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM streaks_calc
    GROUP BY uid
)
-- 4. 最终聚合
SELECT
    g.uid,
    g.dt,
    -- 对局及时间统计
    COUNT(*) AS game_count,
    SUM(g.timecost) AS total_play_seconds,
    ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
    -- 胜负统计
    COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ANY_VALUE(s.max_win_streak) AS max_win_streak,
    ANY_VALUE(s.max_lose_streak) AS max_lose_streak,
    -- 倍数统计（使用更简洁的条件聚合）
    ROUND(AVG(g.magnification), 2) AS avg_magnification,
    MAX(g.magnification) AS max_magnification,
    ROUND(AVG(ABS(g.real_magnification)), 2) AS avg_real_magnification,
    COUNT(CASE WHEN g.magnification <= 6 THEN 1 END) AS low_multi_games,
    COUNT(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 END) AS mid_multi_games,
    COUNT(CASE WHEN g.magnification > 24 THEN 1 END) AS high_multi_games,
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 END) AS high_multi_wins,
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 END) AS high_multi_losses,
    -- 特征特征
    SUM(g.bomb_bet / 2) AS total_bomb_count,
    COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END) AS games_with_grab,
    COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END) AS games_player_doubled,
    -- 经济统计
    MAX(CASE WHEN g.rank_asc = 1 THEN g.start_money END) AS start_money,
    MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END) AS end_money,
    MAX(g.end_money) AS money_peak,
    MIN(g.end_money) AS money_valley,
    SUM(g.diff_money_pre_tax) AS total_diff_money,
    SUM(g.room_fee) AS total_fee_paid,
    -- 逃跑和房间
    COUNT(CASE WHEN g.cut < 0 THEN 1 END) AS escape_count,
    COUNT(DISTINCT g.room_id) AS distinct_rooms,
    GROUP_CONCAT(DISTINCT CAST(g.play_mode AS VARCHAR) ORDER BY g.play_mode) AS play_modes
FROM game_enriched g
LEFT JOIN max_streaks s ON g.uid = s.uid
GROUP BY g.uid, g.dt;
```

### 说明

- 依赖 `dws_ddz_daily_game` 表，需在其之后执行
- **仅统计 APP 端用户**（group_id IN 6,66,8,88,33,44,77,99）
- 仅统计银子玩法（play_mode IN 1,2,3,5），排除积分玩法
- 包含胜负、倍数、经济等汇总指标
- 详细文档：[dws/dws_ddz_appdaily_game_stat.md](../dws/dws_ddz_appdaily_game_stat.md)

---

## 6. 执行顺序与依赖关系

### 表依赖关系

```
dws_dq_daily_reg          ← 无依赖，可优先执行
dws_dq_daily_login        ← 无依赖，可并行执行
dws_ddz_daily_game        ← 无依赖，可并行执行
dws_dq_app_daily_reg      ← 依赖 dws_dq_daily_reg, dws_dq_daily_login
dws_ddz_appdaily_game_stat   ← 依赖 dws_ddz_daily_game
```

### 建议执行顺序

1. **每日凌晨 02:00**：并行执行基础表增量导入（dws_dq_daily_reg、dws_dq_daily_login、dws_ddz_daily_game）
2. **每日凌晨 03:00**：执行依赖表增量导入（dws_dq_app_daily_reg、dws_ddz_appdaily_game_stat）
3. **数据校验**：检查导入数据量是否符合预期

### 批量执行脚本示例

```bash
#!/bin/bash
# 每日数据增量更新脚本
# 用法: ./daily_update.sh 20260409

DATE=$1
DATE_FMT=$(echo $DATE | sed 's/\(....\)\(..\)\(..\)/\1-\2-\3/')

echo "开始执行 ${DATE} 的数据增量更新..."

# 1. 注册数据
echo ">>> 更新注册数据..."
mysql -h<host> -P<port> -u<user> -p<pass> -e "
INSERT INTO tcy_temp.dws_dq_daily_reg
SELECT uid, app_id, FROM_UNIXTIME(first_login_ts / 1000) AS reg_datetime, dt AS reg_date
FROM hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st
WHERE app_id = 1880053 AND dt = ${DATE};
"

# 2. 登录数据
echo ">>> 更新登录数据..."
mysql -h<host> -P<port> -u<user> -p<pass> -e "
INSERT INTO tcy_temp.dws_dq_daily_login
SELECT ... WHERE app_id = 1880053 AND dt >= '${DATE_FMT} 00:00:00' AND dt <= '${DATE_FMT} 23:59:59';
"

# 3. 对局数据
echo ">>> 更新对局数据..."
mysql -h<host> -P<port> -u<user> -p<pass> -e "
INSERT INTO tcy_temp.dws_ddz_daily_game
SELECT ... WHERE game_id = 53 AND dt = ${DATE};
"

echo "数据增量更新完成！"
```

---

## 7. 常见问题

### Q1: 如何补历史数据？

修改 WHERE 条件中的日期范围，如：

```sql
-- 补 2026-03-01 到 2026-03-31 的数据
WHERE dt BETWEEN 20260301 AND 20260331
```

### Q2: 如何检查数据是否已存在？

```sql
-- 检查某日注册数据
SELECT COUNT(*) FROM tcy_temp.dws_dq_daily_reg WHERE reg_date = 20260409;

-- 检查某日登录数据
SELECT COUNT(*) FROM tcy_temp.dws_dq_daily_login WHERE login_date = '2026-04-09';

-- 检查某日对局数据
SELECT COUNT(*) FROM tcy_temp.dws_ddz_daily_game WHERE dt = 20260409;
```

### Q3: 如何删除重复数据？

```sql
-- 删除某日重复数据后重新导入
DELETE FROM tcy_temp.dws_dq_daily_reg WHERE reg_date = 20260409;
DELETE FROM tcy_temp.dws_dq_daily_login WHERE login_date = '2026-04-09';
DELETE FROM tcy_temp.dws_ddz_daily_game WHERE dt = 20260409;
```

---

> **文档版本**：v1.0
> **创建时间**：2026-04-09
> **维护说明**：如有新增 DWS 表，请及时更新本文档
