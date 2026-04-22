# 每日数据增量更新操作手册

> 本文档汇总所有 DWS 层中间表的增量数据更新 SQL，供日常运维使用。

---

## 目录

1. [渠道分类维表初始化](#1-渠道分类维表初始化)
2. [注册数据增量更新](#2-注册数据增量更新)
3. [登录数据增量更新](#3-登录数据增量更新)
4. [APP 端注册用户宽表增量更新](#4-app-端注册用户宽表增量更新)
5. [对局数据增量更新](#5-对局数据增量更新)
6. [APP 端每日游戏活跃用户表初始化](#6-app-端每日游戏活跃用户表初始化)
7. [APP 端每日游戏活跃用户×玩法表初始化](#7-app-端每日游戏活跃用户玩法表初始化)
8. [用户每日游戏行为聚合增量更新（混合玩法）](#8-用户每日游戏行为聚合增量更新-混合玩法)
9. [用户每日游戏行为聚合增量更新（按玩法拆分）](#9-用户每日游戏行为聚合增量更新-按玩法拆分)
10. [执行顺序与依赖关系](#10-执行顺序与依赖关系)
11. [常见问题](#11-常见问题)

---

## 1. 渠道分类维表初始化

### 源表与目标表

| 源表 | 目标表 |
| ---- | ------ |
| `tcy_dim.dim_channel_singletag_dict`、`hive_catalog_cdh5.dim.dim_channel_category` | `tcy_temp.dws_channel_category_map` |

### 初始化 SQL

```sql
INSERT INTO tcy_temp.dws_channel_category_map
SELECT
    t1.channel_id,
    ANY_VALUE(t2.channel_category_id),
    ANY_VALUE(t2.channel_category_name),
    ANY_VALUE(t2.channel_category_tag_id)
FROM tcy_dim.dim_channel_singletag_dict t1
INNER JOIN hive_catalog_cdh5.dim.dim_channel_category t2
    ON t1.channel_type_id = t2.channel_type_id
GROUP BY t1.channel_id;
```

### 说明

- 该表为维表，数据量较小，适合广播 join
- 渠道配置可能发生变化，建议定期更新
- 详细文档：[dws/dws_channel_category_map.md](../dws/dws_channel_category_map.md)

---

## 2. 注册数据增量更新

### 源表与目标表

| 源表 | 目标表 |
| ---- | ------ |
| `hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st` | `tcy_temp.dws_dq_daily_reg` |

### 增量更新 SQL

```sql
-- 注册数据增量导入
-- 参数：将 DATE 替换为实际日期
INSERT INTO tcy_temp.dws_dq_daily_reg
SELECT
    app_id,
    uid,
    str_to_date(CAST(dt AS STRING), '%Y%m%d'),
    FROM_UNIXTIME(first_login_ts / 1000) AS reg_datetime
FROM hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st
WHERE app_id = 1880053
  AND dt between 20260210 and 20260420;
```

### 说明

- `first_login_ts` 为毫秒级时间戳，需除以 1000 转换为秒级
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_dq_daily_reg.md](../dws/dws_dq_daily_reg.md)

---

## 3. 登录数据增量更新

### 源表与目标表

| 源表 | 目标表 |
| ---- | ------ |
| `tcy_dwd.dwd_tcy_userlogin_si` | `tcy_temp.dws_dq_daily_login` |

### 增量更新 SQL

```sql
-- 登录数据增量导入（每日聚合）
-- 参数：将日期范围替换为实际日期
insert into tcy_temp.dws_dq_daily_login
SELECT
    app_id,
    DATE(dt) AS login_date,
    uid,
    MIN(dt) AS first_login_time,
    MIN_BY(app_code, time_unix) AS first_app_code,
    MIN_BY(channel_id, time_unix) AS first_channel_id,
    MIN_BY(group_id, time_unix) AS first_group_id,
    MAX(dt) AS last_login_time,
    MAX_BY(app_code, time_unix) AS last_app_code,
    MAX_BY(channel_id, time_unix) AS last_channel_id,
    MAX_BY(group_id, time_unix) AS last_group_id,
    MAX_BY(channel_id, cnt_channel) AS most_freq_channel_id,
    MAX_BY(group_id, cnt_group) AS most_freq_group_id,
    MAX_BY(app_code, cnt_app_code) AS most_freq_app_code,
    COUNT(DISTINCT channel_id) AS channel_id_count,
    COUNT(DISTINCT group_id) AS group_id_count,
    COUNT(DISTINCT app_code) AS app_code_count,
    COUNT(1) AS login_count
FROM (
    SELECT
        *,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), channel_id) AS cnt_channel,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), group_id) AS cnt_group,
        COUNT(*) OVER(PARTITION BY uid, DATE(dt), app_code) AS cnt_app_code
    FROM tcy_dwd.dwd_tcy_userlogin_si
    WHERE app_id = 1880053
      AND dt >= '2026-02-10 00:00:00'
      AND dt <= '2026-04-20 23:59:59'
) t
GROUP BY app_id, DATE(dt), uid;
```

### 说明

- `time_unix` 为毫秒级时间戳，`MIN_BY/MAX_BY` 基于此排序
- 最频繁维度通过字符串拼接取 MAX 实现
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_dq_daily_login.md](../dws/dws_dq_daily_login.md)

---

## 4. APP 端注册用户宽表增量更新

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_temp.dws_dq_daily_reg`、`tcy_temp.dws_dq_daily_login` | `tcy_temp.dws_dq_app_daily_reg` |

### 增量更新 SQL

```sql
-- APP 端注册用户宽表增量导入
-- 参数：将 ${DATE} 替换为实际日期
insert into tcy_temp.dws_dq_app_daily_reg
SELECT
    r.app_id,
    r.reg_date,
    COALESCE(l.first_channel_id, -1) AS reg_channel_id,
    r.uid,
    r.reg_datetime,
    COALESCE(l.first_group_id, -1) AS reg_group_id,
    COALESCE(l.first_app_code, '') AS reg_app_code,
    COALESCE(chn.channel_category_id, -1) AS channel_category_id,
    COALESCE(chn.channel_category_name, '未知/日志丢失') AS channel_category_name,
    COALESCE(chn.channel_category_tag_id, -1) AS channel_category_tag_id,
    CASE WHEN l.uid IS NULL THEN 1 ELSE 0 END AS is_login_log_missing,
    COALESCE(l.login_count, 0) AS first_day_login_cnt
FROM tcy_temp.dws_dq_daily_reg r
INNER JOIN tcy_temp.dws_dq_daily_login l
    ON r.app_id = l.app_id
    AND r.reg_date = l.login_date
    AND r.uid = l.uid
LEFT JOIN tcy_temp.dws_channel_category_map chn
    ON l.first_channel_id = chn.channel_id
WHERE r.app_id = 1880053
  AND r.reg_date between '2026-02-10' and '2026-04-21'
  AND l.first_group_id IN (6, 66, 33, 44, 77, 99, 8, 88);
```

### 说明

- 依赖 `dws_dq_daily_reg` 和 `dws_dq_daily_login` 表，需在其之后执行
- 仅包含 APP 端注册用户（Android + iOS），通过 `reg_group_id` 过滤
- `is_login_log_missing = 1` 表示注册当日无登录日志
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_dq_app_daily_reg.md](../dws/dws_dq_app_daily_reg.md)

---

## 5. 对局数据增量更新

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
    IFNULL(app_id, 1880053), dt, uid, FROM_UNIXTIME(time_unix / 1000) as game_datetime, resultguid, timecost, room_id,
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
    room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN oldscore ELSE olddeposit END AS start_money,
    CASE WHEN room_id IN (11534,14238,15458,158,159) THEN end_score ELSE end_deposit END AS end_money,
    CASE
        WHEN room_id IN (11534,14238,15458,158,159)
        THEN scorediff + score_fee
        ELSE depositdiff + fee
    END AS diff_money_pre_tax,
    cut, safebox_deposit, magnification, magnification_stacked,
    CASE
        WHEN room_id IN (11534,14238,15458,158,159)
        THEN ROUND((scorediff + score_fee) / NULLIF(basescore, 0), 2)
        ELSE ROUND((depositdiff + fee) / NULLIF(basedeposit, 0), 2)
    END AS real_magnification,
    get_json_int(magnification_subdivision, '$.public_bet.grab_landlord_bet') AS grab_landlord_bet,
    get_json_int(magnification_subdivision, '$.public_bet.complete_victory_bet') AS complete_victory_bet,
    get_json_int(magnification_subdivision, '$.public_bet.bomb_bet') AS bomb_bet, channel_id, group_id, app_code, game_id
FROM tcy_dwd.dwd_game_combat_si
WHERE game_id = 53
  AND dt BETWEEN 20260210 AND 20260416;
```

### 说明

- 包含机器人和真人数据，通过 `robot` 字段区分
- 玩法分类 `play_mode`：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房
- 货币字段已统一命名，简化后续分析
- 建议每日凌晨执行，导入前一日数据
- 详细文档：[dws/dws_ddz_daily_game.md](../dws/dws_ddz_daily_game.md)

---

## 6. APP 端每日游戏活跃用户表初始化

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_temp.dws_ddz_daily_game` | `tcy_temp.dws_app_game_active` |

### 初始化 SQL

```sql
INSERT INTO tcy_temp.dws_app_game_active
SELECT app_id, uid, date(dt)
FROM tcy_temp.dws_ddz_daily_game
WHERE app_id = 1880053
  AND dt BETWEEN '2026-02-10' AND '2026-04-21'
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY 1, 2, 3;
```

### 说明

- 该表专用于留存 flag 计算，粒度为 uid × dt × app_id
- 仅包含 APP 端真人用户（排除机器人和非 APP 端）
- 依赖 `dws_ddz_daily_game` 表，需在其之后执行
- 详细文档：[dws/dws_app_game_active.md](../dws/dws_app_game_active.md)

---

## 7. APP 端每日游戏活跃用户×玩法表初始化

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_temp.dws_ddz_daily_game` | `tcy_temp.dws_app_gamemode_active` |

### 初始化 SQL

```sql
INSERT INTO tcy_temp.dws_app_gamemode_active
SELECT app_id, uid, play_mode, date(dt)
FROM tcy_temp.dws_ddz_daily_game
WHERE app_id = 1880053
  AND dt BETWEEN '2026-02-10' AND '2026-04-21'
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY 1,2,3,4;
```

### 说明

- 该表专用于"同玩法留存"flag 计算，粒度为 uid × dt × app_id × play_mode
- 仅包含 APP 端真人用户
- 依赖 `dws_ddz_daily_game` 表，需在其之后执行
- 详细文档：[dws/dws_app_gamemode_active.md](../dws/dws_app_gamemode_active.md)

---

## 8. 用户每日游戏行为聚合增量更新（混合玩法）

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_temp.dws_ddz_daily_game` | `tcy_temp.dws_ddz_app_game_stat` |

### 增量更新 SQL

```sql
-- APP 端每日游戏行为统计增量导入（混合玩法）
insert into tcy_temp.dws_ddz_app_game_stat
WITH game_enriched AS (
    SELECT
        *,
        ROW_NUMBER() OVER (PARTITION BY uid, app_code ORDER BY game_datetime ASC) AS game_seq,
        ROW_NUMBER() OVER (PARTITION BY uid, app_code ORDER BY game_datetime DESC) AS rank_desc
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt between '2026-04-01' and '2026-04-21'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND play_mode IN (1, 2, 3, 5)
),
streaks_calc AS (
    SELECT
        uid, app_code, result_id, MAX(streak_len) as max_streak
    FROM (
        SELECT
            uid, app_code, result_id,
            COUNT(*) OVER(PARTITION BY uid, app_code, result_id, grp) AS streak_len
        FROM (
            SELECT uid, app_code, result_id,
              game_seq - ROW_NUMBER() OVER (PARTITION BY uid, app_code, result_id ORDER BY game_seq) AS grp
            FROM game_enriched
            WHERE result_id IN (1, 2)
        ) t1
    ) t2
    GROUP BY uid, app_code, result_id
),
max_streaks AS (
    SELECT uid, app_code,
        MAX(CASE WHEN result_id = 1 THEN max_streak ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN max_streak ELSE 0 END) AS max_lose_streak
    FROM streaks_calc
    GROUP BY uid, app_code
)
SELECT
    g.app_id, g.uid, g.dt, g.app_code,
    COUNT(*) AS game_count,
    SUM(g.timecost) AS total_play_seconds,
    ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
    COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ANY_VALUE(s.max_win_streak),
    ANY_VALUE(s.max_lose_streak),
    ROUND(AVG(g.magnification), 2),
    MAX(g.magnification),
    ROUND(AVG(ABS(g.real_magnification)), 2),
    COUNT(CASE WHEN g.magnification <= 6 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 END),
    SUM(g.bomb_bet / 2),
    COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END),
    COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END),
    MAX(CASE WHEN g.game_seq = 1 THEN g.start_money END),
    MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END),
    MAX(g.end_money),
    MIN(g.end_money),
    SUM(g.diff_money_pre_tax),
    SUM(g.room_fee),
    COUNT(CASE WHEN g.cut < 0 THEN 1 END),
    COUNT(DISTINCT g.room_id),
    GROUP_CONCAT(DISTINCT CAST(g.play_mode AS VARCHAR) ORDER BY g.play_mode)
FROM game_enriched g
LEFT JOIN max_streaks s ON g.uid = s.uid AND g.app_code = s.app_code
GROUP BY g.app_id, g.uid, g.dt, g.app_code;
```

### 说明

- 依赖 `dws_ddz_daily_game` 表，需在其之后执行
- **新增 `app_code` 维度**：粒度为 uid × dt × app_code，支持按客户端开发语言（cocos creator vs cocos lua）分析用户行为差异
- **仅统计 APP 端用户**（group_id IN 6,66,8,88,33,44,77,99）
- 仅统计银子玩法（play_mode IN 1,2,3,5），排除积分玩法
- 包含胜负、倍数、经济等汇总指标
- `play_modes` 字段记录当天玩过的所有玩法
- 详细文档：[dws/dws_ddz_app_game_stat.md](../dws/dws_ddz_app_game_stat.md)

---

## 9. 用户每日游戏行为聚合增量更新（按玩法拆分）

### 源表与目标表

| 源表 | 目标表 |
|------|--------|
| `tcy_temp.dws_ddz_daily_game` | `tcy_temp.dws_ddz_app_gamemode_stat` |

### 增量更新 SQL

```sql
-- APP 端每日游戏行为统计增量导入（按玩法拆分）
insert into tcy_temp.dws_ddz_app_gamemode_stat
WITH game_enriched AS (
    SELECT
        *,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode, app_code ORDER BY game_datetime ASC) AS game_seq,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode, app_code ORDER BY game_datetime DESC) AS rank_desc
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt between '2026-02-10' and '2026-04-21'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND play_mode IN (1, 2, 3, 5)
),
streaks_calc AS (
    SELECT
        uid, app_code, play_mode, result_id, COUNT(*) AS streak_len
    FROM (
        SELECT
            uid, app_code, play_mode, result_id,
            game_seq - ROW_NUMBER() OVER (PARTITION BY uid, play_mode, app_code, result_id ORDER BY game_seq) AS grp
        FROM game_enriched
        WHERE result_id IN (1, 2)
    ) t
    GROUP BY uid, app_code, play_mode, result_id, grp
),
max_streaks AS (
    SELECT
        uid, app_code, play_mode,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM streaks_calc
    GROUP BY uid, app_code, play_mode
)
SELECT
    g.app_id,
    g.play_mode,
    g.uid,
    g.dt,
    g.app_code,
    COUNT(*) AS game_count,
    SUM(g.timecost) AS total_play_seconds,
    ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
    COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ANY_VALUE(s.max_win_streak),
    ANY_VALUE(s.max_lose_streak),
    ROUND(AVG(g.magnification), 2),
    MAX(g.magnification),
    ROUND(AVG(ABS(g.real_magnification)), 2),
    COUNT(CASE WHEN g.magnification <= 6 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 END),
    SUM(g.bomb_bet / 2),
    COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END),
    COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END),
    MAX(CASE WHEN g.game_seq = 1 THEN g.start_money END),
    MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END),
    MAX(g.end_money),
    MIN(g.end_money),
    SUM(g.diff_money_pre_tax),
    SUM(g.room_fee),
    COUNT(CASE WHEN g.cut < 0 THEN 1 END),
    COUNT(DISTINCT g.room_id)
FROM game_enriched g
LEFT JOIN max_streaks s ON g.uid = s.uid AND g.play_mode = s.play_mode AND g.app_code = s.app_code
GROUP BY g.app_id, g.play_mode, g.uid, g.dt, g.app_code;
```

### 说明

- 依赖 `dws_ddz_daily_game` 表，需在其之后执行
- **新增 `app_code` 维度**：粒度为 uid × dt × play_mode × app_code，支持按客户端开发语言和玩法双维度分析
- **仅统计 APP 端用户**（group_id IN 6,66,8,88,33,44,77,99）
- 仅统计银子玩法（play_mode IN 1,2,3,5），排除积分玩法
- 与 `dws_ddz_app_game_stat` 字段基本一致，增加 `play_mode` 维度
- 适用于需要控制玩法变量的分析（倍数、胜率、连胜连败、经济变化）
- 详细文档：[dws/dws_ddz_app_gamemode_stat.md](../dws/dws_ddz_app_gamemode_stat.md)

---

## 10. 执行顺序与依赖关系

### 表依赖关系

```
dws_channel_category_map       ← 维表，优先初始化（其他表可能关联）
dws_dq_daily_reg               ← 无依赖，可并行执行
dws_dq_daily_login             ← 无依赖，可并行执行
dws_ddz_daily_game             ← 无依赖，可并行执行
dws_dq_app_daily_reg           ← 依赖 dws_dq_daily_reg, dws_dq_daily_login, dws_channel_category_map
dws_app_game_active            ← 依赖 dws_ddz_daily_game
dws_app_gamemode_active        ← 依赖 dws_ddz_daily_game
dws_ddz_app_game_stat          ← 依赖 dws_ddz_daily_game
dws_ddz_app_gamemode_stat      ← 依赖 dws_ddz_daily_game
```

### 建议执行顺序

1. **初始化阶段**：执行维表初始化（dws_channel_category_map）
2. **每日凌晨 02:00**：并行执行基础表增量导入（dws_dq_daily_reg、dws_dq_daily_login、dws_ddz_daily_game）
3. **每日凌晨 03:00**：执行依赖表增量导入（dws_dq_app_daily_reg、dws_app_game_active、dws_app_gamemode_active、dws_ddz_app_game_stat、dws_ddz_app_gamemode_stat）
4. **数据校验**：检查导入数据量是否符合预期

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

## 11. 常见问题

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

-- 检查某日游戏活跃用户
SELECT COUNT(*) FROM tcy_temp.dws_app_game_active WHERE dt = '2026-04-09';

-- 检查某日某玩法活跃用户
SELECT COUNT(*) FROM tcy_temp.dws_app_gamemode_active WHERE dt = '2026-04-09' AND play_mode = 1;
```

### Q3: 如何删除重复数据？

```sql
-- 删除某日重复数据后重新导入
DELETE FROM tcy_temp.dws_dq_daily_reg WHERE reg_date = 20260409;
DELETE FROM tcy_temp.dws_dq_daily_login WHERE login_date = '2026-04-09';
DELETE FROM tcy_temp.dws_ddz_daily_game WHERE dt = 20260409;
DELETE FROM tcy_temp.dws_app_game_active WHERE dt = '2026-04-09';
DELETE FROM tcy_temp.dws_app_gamemode_active WHERE dt = '2026-04-09';
DELETE FROM tcy_temp.dws_ddz_app_game_stat WHERE dt = 20260409;
DELETE FROM tcy_temp.dws_ddz_app_gamemode_stat WHERE dt = 20260409;
```

### Q4: 如何更新渠道分类维表？

```sql
-- 清空后重新导入
TRUNCATE TABLE tcy_temp.dws_channel_category_map;
INSERT INTO tcy_temp.dws_channel_category_map
SELECT ... -- 见第1节初始化 SQL
```

---

## 表清单速览

| 表名 | 用途 | 更新频率 | 依赖 |
| ---- | ---- | -------- | ---- |
| dws_channel_category_map | 渠道分类维表 | 按需更新 | 无 |
| dws_dq_daily_reg | 用户注册表 | 每日增量 | 无 |
| dws_dq_daily_login | 用户每日登录聚合表 | 每日增量 | 无 |
| dws_dq_app_daily_reg | APP端注册用户宽表 | 每日增量 | dws_dq_daily_reg, dws_dq_daily_login |
| dws_ddz_daily_game | 对局明细表（统一字段） | 每日增量 | 无 |
| dws_app_game_active | APP端每日游戏活跃用户表 | 每日增量 | dws_ddz_daily_game |
| dws_app_gamemode_active | APP端每日游戏活跃用户×玩法表 | 每日增量 | dws_ddz_daily_game |
| dws_ddz_app_game_stat | APP端每日游戏行为统计（混合玩法） | 每日增量 | dws_ddz_daily_game |
| dws_ddz_app_gamemode_stat | APP端每日游戏行为统计（按玩法拆分） | 每日增量 | dws_ddz_daily_game |

---

> **文档版本**：v2.0
> **更新时间**：2026-04-22
> **维护说明**：如有新增 DWS 表，请及时更新本文档
