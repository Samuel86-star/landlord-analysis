# 分玩法层留存分析：玩法内因子对比

> 本文档聚焦**分玩法层**留存分析，对比经典/不洗牌/癞子/510K 四种银两玩法的留存规律差异。全局层分析见 [retention-global.md](retention-global.md)。
>
> **分析时间段**：2026-02-10 至 2026-06-15
> **留存口径**：同玩法游戏留存（分母为当日注册APP端用户，分子为第N日在同一玩法有对局的用户）

---

## 目录

1. [玩法映射关系](#一玩法映射关系)
2. [玩法留存对比](#二玩法留存对比)
3. [玩法内因子分析](#三玩法内因子分析)
4. [跨玩法行为分析](#四跨玩法行为分析)
5. [专项分析索引](#五专项分析索引)

---

## 一、玩法映射关系

### 1.1 play_mode 枚举

| play_mode | 名称 | 银两类型 | 说明 |
| --------- | ---- | -------- | ---- |
| 1 | 经典 | 银两 | 传统三人斗地主 |
| 2 | 不洗牌 | 银两 | 手牌分布更集中，炸弹概率高 |
| 3 | 癞子 | 银两 | 癞子牌可替代任意牌，倍数波动大 |
| 4 | 积分 | 非银两 | PC/网页端积分模式 |
| 5 | 比赛 | 非银两 | 定时赛/锦标赛 |
| 6 | 好友房 | 非银两 | 好友开房 |
| 7 | 510K | 银两 | 多轮制疯狂斗地主，结算机制不同 |

> 本文分析聚焦 **银两玩法（play_mode IN (1, 2, 3, 7)）**，积分/比赛/好友房见专项文档。

### 1.2 room_id 到 play_mode 映射（参考）

```sql
CASE
    WHEN room_id IN (742, 420, 4484, 12074, 6314, 11168, 10336, 16445) THEN 1  -- 经典
    WHEN room_id IN (421, 22039, 22040, 22041, 22042)                  THEN 2  -- 不洗牌
    WHEN room_id IN (13176, 13177, 13178)                              THEN 3  -- 癞子
    WHEN room_id IN (26000, 26001, 26002, 26003, 26004, 26005)         THEN 7  -- 510K
    ELSE 0
END AS play_mode
```

---

## 二、玩法留存对比

### 2.1 各玩法新增用户留存率

> 使用 `dws_app_gamemode_active` 表判定同玩法留存（uid × dt × play_mode 粒度），使用 `dws_dq_app_daily_reg` 获取注册数据。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
-- 注册当天各玩法参与情况（通过首日对局表获取玩法归属）
reg_mode AS (
    SELECT r.uid, r.reg_date, r.app_id,
           g.play_mode,
           COUNT(DISTINCT g.uid) AS played_flag
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
    GROUP BY r.uid, r.reg_date, r.app_id, g.play_mode
),
-- 合并 510K 首日参与（510K 表结构不同，独立处理）
reg_mode_510k AS (
    SELECT r.uid, r.reg_date, r.app_id,
           7 AS play_mode,
           COUNT(DISTINCT cg.uid) AS played_flag
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_crazyddz_daily_game cg
        ON cg.app_id = r.app_id AND cg.uid = r.uid AND cg.dt = r.reg_date
        AND cg.robot != 1
    GROUP BY r.uid, r.reg_date, r.app_id
),
reg_mode_all AS (
    SELECT * FROM reg_mode WHERE play_mode IS NOT NULL
    UNION ALL
    SELECT * FROM reg_mode_510k WHERE played_flag > 0
),
-- 同玩法留存判定
mode_retention AS (
    SELECT rm.*,
           ma.dt AS ret_dt
    FROM reg_mode_all rm
    LEFT JOIN tcy_temp.dws_app_gamemode_active ma
        ON ma.app_id = rm.app_id AND ma.uid = rm.uid
        AND ma.dt = DATE_ADD(rm.reg_date, INTERVAL 1 DAY)
        AND ma.play_mode = rm.play_mode
)
SELECT
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,
    COUNT(DISTINCT uid) AS mode_reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN ret_dt IS NOT NULL THEN uid END) * 100.0
          / COUNT(DISTINCT uid), 2) AS same_mode_d1_rate
FROM mode_retention
GROUP BY play_mode
ORDER BY play_mode;
```

### 2.2 各玩法新增用户分布与 7 日同玩法留存

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
reg_mode AS (
    SELECT r.uid, r.reg_date, r.app_id, g.play_mode
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
    WHERE g.play_mode IS NOT NULL
    UNION ALL
    SELECT r.uid, r.reg_date, r.app_id, 7 AS play_mode
    FROM reg_base r
    INNER JOIN tcy_temp.dws_crazyddz_daily_game cg
        ON cg.app_id = r.app_id AND cg.uid = r.uid AND cg.dt = r.reg_date
        AND cg.robot != 1
)
SELECT
    CASE rm.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
    END AS play_mode_name,
    COUNT(DISTINCT rm.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN ma1.dt IS NOT NULL THEN rm.uid END) * 100.0
          / COUNT(DISTINCT rm.uid), 2) AS same_mode_d1,
    ROUND(COUNT(DISTINCT CASE WHEN ma7.dt IS NOT NULL THEN rm.uid END) * 100.0
          / COUNT(DISTINCT rm.uid), 2) AS same_mode_d7
FROM reg_mode rm
LEFT JOIN tcy_temp.dws_app_gamemode_active ma1
    ON ma1.app_id = rm.app_id AND ma1.uid = rm.uid
    AND ma1.dt = DATE_ADD(rm.reg_date, INTERVAL 1 DAY)
    AND ma1.play_mode = rm.play_mode
LEFT JOIN tcy_temp.dws_app_gamemode_active ma7
    ON ma7.app_id = rm.app_id AND ma7.uid = rm.uid
    AND ma7.dt = DATE_ADD(rm.reg_date, INTERVAL 6 DAY)
    AND ma7.play_mode = rm.play_mode
GROUP BY rm.play_mode
ORDER BY rm.play_mode;
```

---

## 三、玩法内因子分析

> 所有因子分析均使用 `dws_app_allgame_stat` 表（uid × dt × play_mode 粒度），该表覆盖 play_mode 1-7 的全玩法体验数据。

### 3.1 分玩法 × 倍数分组留存

> 字段来源：`dws_app_allgame_stat.avg_magnification`

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
)
SELECT
    CASE st.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
    END AS play_mode_name,
    CASE
        WHEN st.avg_magnification IS NULL THEN '0: 无对局'
        WHEN st.avg_magnification <= 6  THEN 'A: <=6'
        WHEN st.avg_magnification <= 12 THEN 'B: 6-12'
        WHEN st.avg_magnification <= 24 THEN 'C: 12-24'
        ELSE                                 'D: 24+'
    END AS multi_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS same_mode_d1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_allgame_stat st
    ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    AND st.play_mode IN (1, 2, 3, 7)
LEFT JOIN tcy_temp.dws_app_gamemode_active ma
    ON ma.app_id = r.app_id AND ma.uid = r.uid
    AND ma.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND ma.play_mode = st.play_mode
WHERE st.play_mode IS NOT NULL
GROUP BY st.play_mode, multi_group
ORDER BY st.play_mode, multi_group;
```

### 3.2 分玩法 × 胜率分组留存

> 字段来源：`dws_app_allgame_stat.win_rate`

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
)
SELECT
    CASE st.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
    END AS play_mode_name,
    CASE
        WHEN st.win_rate IS NULL THEN '0: 无对局'
        WHEN st.win_rate < 30 THEN 'A: <30%'
        WHEN st.win_rate < 50 THEN 'B: 30-50%'
        WHEN st.win_rate < 70 THEN 'C: 50-70%'
        ELSE                       'D: >=70%'
    END AS winrate_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS same_mode_d1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_allgame_stat st
    ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    AND st.play_mode IN (1, 2, 3, 7)
LEFT JOIN tcy_temp.dws_app_gamemode_active ma
    ON ma.app_id = r.app_id AND ma.uid = r.uid
    AND ma.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND ma.play_mode = st.play_mode
WHERE st.play_mode IS NOT NULL
GROUP BY st.play_mode, winrate_group
ORDER BY st.play_mode, winrate_group;
```

### 3.3 分玩法 × 对局数分组留存

> 字段来源：`dws_app_allgame_stat.game_count`

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
)
SELECT
    CASE st.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
    END AS play_mode_name,
    CASE
        WHEN st.game_count IS NULL OR st.game_count = 0 THEN '0: 无对局'
        WHEN st.game_count = 1   THEN 'A: 1局'
        WHEN st.game_count <= 3  THEN 'B: 2-3局'
        WHEN st.game_count <= 5  THEN 'C: 4-5局'
        WHEN st.game_count <= 10 THEN 'D: 6-10局'
        ELSE                          'E: 10局+'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS same_mode_d1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_allgame_stat st
    ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    AND st.play_mode IN (1, 2, 3, 7)
LEFT JOIN tcy_temp.dws_app_gamemode_active ma
    ON ma.app_id = r.app_id AND ma.uid = r.uid
    AND ma.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND ma.play_mode = st.play_mode
WHERE st.play_mode IS NOT NULL
GROUP BY st.play_mode, game_count_group
ORDER BY st.play_mode, game_count_group;
```

### 3.4 分玩法 × 炸弹分布留存

> 字段来源：`dws_app_allgame_stat.bomb_0_games`, `bomb_1_games`, `bomb_2_games`, `bomb_3plus_games`

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
bomb_class AS (
    SELECT r.uid, r.reg_date, r.app_id, st.play_mode,
           CASE
               WHEN st.bomb_3plus_games > 0 THEN 'D: 高频炸弹(3+炸弹对局)'
               WHEN st.bomb_2_games > 0      THEN 'C: 中频炸弹(2炸弹对局)'
               WHEN st.bomb_1_games > 0      THEN 'B: 低频炸弹(1炸弹对局)'
               WHEN st.bomb_0_games > 0      THEN 'A: 无炸弹'
               ELSE '0: 无对局'
           END AS bomb_level
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
        AND st.play_mode IN (1, 2, 3, 7)
    WHERE st.play_mode IS NOT NULL
)
SELECT
    CASE bc.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
    END AS play_mode_name,
    bc.bomb_level,
    COUNT(DISTINCT bc.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN bc.uid END) * 100.0
          / COUNT(DISTINCT bc.uid), 2) AS same_mode_d1_rate
FROM bomb_class bc
LEFT JOIN tcy_temp.dws_app_gamemode_active ma
    ON ma.app_id = bc.app_id AND ma.uid = bc.uid
    AND ma.dt = DATE_ADD(bc.reg_date, INTERVAL 1 DAY)
    AND ma.play_mode = bc.play_mode
GROUP BY bc.play_mode, bc.bomb_level
ORDER BY bc.play_mode, bc.bomb_level;
```

### 3.5 分玩法 × 多维度四分位体验留存

> 字段来源：`dws_app_allgame_stat.multi_q1`, `multi_q2`, `multi_q3`, `multi_q4`
>
> 将用户首日对局按倍数从低到高分为 Q1-Q4 四组，Q1 为最低倍数对局数，Q4 为最高倍数对局数。观察各倍数段对局数量对留存的影响。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
quartile AS (
    SELECT r.uid, r.reg_date, r.app_id, st.play_mode,
           st.multi_q1, st.multi_q2, st.multi_q3, st.multi_q4,
           (COALESCE(st.multi_q1, 0) + COALESCE(st.multi_q2, 0)
          + COALESCE(st.multi_q3, 0) + COALESCE(st.multi_q4, 0)) AS total_quartile_games,
           CASE
               WHEN COALESCE(st.multi_q4, 0) > 0 THEN 'D: 有高倍对局'
               WHEN COALESCE(st.multi_q3, 0) > 0 THEN 'C: 有中高倍对局'
               WHEN COALESCE(st.multi_q2, 0) > 0 THEN 'B: 有中低倍对局'
               WHEN COALESCE(st.multi_q1, 0) > 0 THEN 'A: 仅低倍对局'
               ELSE '0: 无对局'
           END AS max_quartile_level
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
        AND st.play_mode IN (1, 2, 3, 7)
    WHERE st.play_mode IS NOT NULL
)
SELECT
    CASE q.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
    END AS play_mode_name,
    q.max_quartile_level,
    COUNT(DISTINCT q.uid) AS user_count,
    ROUND(AVG(q.total_quartile_games), 1) AS avg_quartile_games,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN q.uid END) * 100.0
          / COUNT(DISTINCT q.uid), 2) AS same_mode_d1_rate
FROM quartile q
LEFT JOIN tcy_temp.dws_app_gamemode_active ma
    ON ma.app_id = q.app_id AND ma.uid = q.uid
    AND ma.dt = DATE_ADD(q.reg_date, INTERVAL 1 DAY)
    AND ma.play_mode = q.play_mode
GROUP BY q.play_mode, q.max_quartile_level
ORDER BY q.play_mode, q.max_quartile_level;
```

### 3.6 510K 专项：结算轮次与 outcome 分析

> 510K（play_mode=7）为多轮制玩法，一局包含多个 settle round。使用 `dws_app_allgame_stat` 中的 `avg_settle_rounds` 和 `outcome_gdp` 字段分析。

#### 3.6.1 510K 平均结算轮次分组留存

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
)
SELECT
    CASE
        WHEN st.avg_settle_rounds IS NULL THEN '0: 无对局'
        WHEN st.avg_settle_rounds <= 3  THEN 'A: <=3轮'
        WHEN st.avg_settle_rounds <= 5  THEN 'B: 4-5轮'
        WHEN st.avg_settle_rounds <= 8  THEN 'C: 6-8轮'
        ELSE                                 'D: 8轮+'
    END AS settle_rounds_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS same_mode_d1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_allgame_stat st
    ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    AND st.play_mode = 7
LEFT JOIN tcy_temp.dws_app_gamemode_active ma
    ON ma.app_id = r.app_id AND ma.uid = r.uid
    AND ma.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND ma.play_mode = 7
WHERE st.play_mode IS NOT NULL
GROUP BY settle_rounds_group
ORDER BY settle_rounds_group;
```

#### 3.6.2 510K outcome_gdp 分组留存

> `outcome_gdp` 表示 510K 多轮结算后的总盈亏结果。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
)
SELECT
    CASE
        WHEN st.outcome_gdp IS NULL THEN '0: 无对局'
        WHEN st.outcome_gdp < -50000  THEN 'A: 巨亏(<-5万)'
        WHEN st.outcome_gdp < -10000  THEN 'B: 大亏(-5万~-1万)'
        WHEN st.outcome_gdp < 0       THEN 'C: 小亏(-1万~0)'
        WHEN st.outcome_gdp < 10000   THEN 'D: 小赚(0~1万)'
        WHEN st.outcome_gdp < 50000   THEN 'E: 大赚(1万~5万)'
        ELSE                               'F: 巨赚(>5万)'
    END AS outcome_gdp_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS same_mode_d1_rate
FROM reg_base r
LEFT JOIN tcy_temp.dws_app_allgame_stat st
    ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    AND st.play_mode = 7
LEFT JOIN tcy_temp.dws_app_gamemode_active ma
    ON ma.app_id = r.app_id AND ma.uid = r.uid
    AND ma.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND ma.play_mode = 7
WHERE st.play_mode IS NOT NULL
GROUP BY outcome_gdp_group
ORDER BY outcome_gdp_group;
```

---

## 四、跨玩法行为分析

### 4.1 首局玩法选择与留存

> 使用 `dws_ddz_firstday_game` 和 `dws_crazyddz_daily_game` 获取首局玩法，观察各玩法作为"第一局"时的用户量和留存。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
first_game_ddz AS (
    SELECT r.uid, r.reg_date, r.app_id,
           MIN_BY(g.play_mode, g.game_datetime) AS first_play_mode,
           MIN(g.game_datetime) AS first_game_time
    FROM reg_base r
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
    GROUP BY r.uid, r.reg_date, r.app_id
),
first_game_510k AS (
    SELECT r.uid, r.reg_date, r.app_id,
           7 AS first_play_mode,
           MIN(cg.game_datetime) AS first_game_time
    FROM reg_base r
    INNER JOIN tcy_temp.dws_crazyddz_daily_game cg
        ON cg.app_id = r.app_id AND cg.uid = r.uid AND cg.dt = r.reg_date
        AND cg.robot != 1
    GROUP BY r.uid, r.reg_date, r.app_id
),
first_game_union AS (
    SELECT * FROM first_game_ddz
    UNION ALL
    SELECT * FROM first_game_510k
),
-- 去重：如果同用户同时有 ddz 和 510k，取时间最早的
first_game_dedup AS (
    SELECT uid, reg_date, app_id,
           MIN_BY(first_play_mode, first_game_time) AS first_play_mode
    FROM first_game_union
    GROUP BY uid, reg_date, app_id
)
SELECT
    CASE fg.first_play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS first_mode_name,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma1.dt IS NOT NULL THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS d1_rate_any_mode,
    ROUND(COUNT(DISTINCT CASE
              WHEN ma1.dt IS NOT NULL AND ma1.play_mode = fg.first_play_mode
              THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS d1_rate_same_mode
FROM reg_base r
LEFT JOIN first_game_dedup fg
    ON fg.uid = r.uid AND fg.reg_date = r.reg_date AND fg.app_id = r.app_id
LEFT JOIN tcy_temp.dws_app_gamemode_active ma1
    ON ma1.app_id = r.app_id AND ma1.uid = r.uid
    AND ma1.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    AND ma1.play_mode IN (1, 2, 3, 7)
WHERE fg.first_play_mode IS NOT NULL
GROUP BY fg.first_play_mode
ORDER BY fg.first_play_mode;
```

### 4.2 玩法数量与留存

> 注册当天玩过的玩法数量（play_mode 1,2,3,7）与整体留存的关系。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
mode_count AS (
    SELECT r.uid, r.reg_date, r.app_id,
           COUNT(DISTINCT play_mode) AS mode_count
    FROM reg_base r
    LEFT JOIN (
        SELECT app_id, uid, dt, play_mode
        FROM tcy_temp.dws_ddz_firstday_game
        WHERE robot != 1 AND play_mode IN (1, 2, 3)
        UNION ALL
        SELECT app_id, uid, dt, 7 AS play_mode
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE robot != 1
    ) g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
    GROUP BY r.uid, r.reg_date, r.app_id
)
SELECT
    mc.mode_count,
    COUNT(DISTINCT mc.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN ma.dt IS NOT NULL THEN mc.uid END) * 100.0
          / COUNT(DISTINCT mc.uid), 2) AS d1_rate_any_mode
FROM mode_count mc
LEFT JOIN tcy_temp.dws_app_game_active ma
    ON ma.app_id = mc.app_id AND ma.uid = mc.uid
    AND ma.dt = DATE_ADD(mc.reg_date, INTERVAL 1 DAY)
GROUP BY mc.mode_count
ORDER BY mc.mode_count;
```

### 4.3 玩法切换行为

> 注册当天玩过多个玩法的用户中，各玩法对的交叉留存（在 A 玩法注册，第 2 天是否在 B 玩法活跃）。

```sql
WITH reg_base AS (
    SELECT uid, reg_date, app_id
    FROM tcy_temp.dws_dq_app_daily_reg
    WHERE app_id = 1880053
      AND reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
reg_mode_pairs AS (
    -- 注册当天各玩法参与标记
    SELECT r.uid, r.reg_date, r.app_id,
           g.play_mode
    FROM reg_base r
    LEFT JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
        AND g.robot != 1 AND g.play_mode IN (1, 2, 3)
    WHERE g.play_mode IS NOT NULL
    UNION ALL
    SELECT r.uid, r.reg_date, r.app_id, 7 AS play_mode
    FROM reg_base r
    INNER JOIN tcy_temp.dws_crazyddz_daily_game cg
        ON cg.app_id = r.app_id AND cg.uid = r.uid AND cg.dt = r.reg_date
        AND cg.robot != 1
),
cross_retention AS (
    SELECT
        rm.play_mode AS reg_mode,
        ma.play_mode AS ret_mode,
        COUNT(DISTINCT rm.uid) AS user_count
    FROM reg_mode_pairs rm
    INNER JOIN tcy_temp.dws_app_gamemode_active ma
        ON ma.app_id = rm.app_id AND ma.uid = rm.uid
        AND ma.dt = DATE_ADD(rm.reg_date, INTERVAL 1 DAY)
        AND ma.play_mode IN (1, 2, 3, 7)
    GROUP BY rm.play_mode, ma.play_mode
)
SELECT
    CASE reg_mode WHEN 1 THEN '经典' WHEN 2 THEN '不洗牌' WHEN 3 THEN '癞子' WHEN 7 THEN '510K' END AS from_mode,
    CASE ret_mode WHEN 1 THEN '经典' WHEN 2 THEN '不洗牌' WHEN 3 THEN '癞子' WHEN 7 THEN '510K' END AS to_mode,
    user_count,
    ROUND(user_count * 100.0 / SUM(user_count) OVER (PARTITION BY reg_mode), 2) AS pct_of_reg_mode
FROM cross_retention
ORDER BY reg_mode, ret_mode;
```

---

## 五、专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |
| [retention-deepdive-sql.md](retention-deepdive-sql.md) | 高危信号下钻 | 1局用户、客户端问题、渠道质量 |

> **分析框架速查**：[retention-analysis-framework.md](retention-analysis-framework.md)

---

> **文档版本**：v3.0
> **创建时间**：2026-06-16
> **更新说明**：
>
> - v3.0：基于 2026-06-11 重构后的数仓表结构全面更新，使用 `dws_app_gamemode_active` 做同玩法留存判定、`dws_app_allgame_stat` 做玩法内因子分析；新增 play_mode=7（510K）全量分析；新增跨玩法行为分析章节（首局选择、玩法数量、玩法切换）
> - v2.0：旧版本（基于 `dws_ddz_app_game_stat` 表，仅覆盖 play_mode 1,2,3）
                                                                                                                                                                                                                                                                                                                                                                                                                                                