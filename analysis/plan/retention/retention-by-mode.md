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

> **💡 性能建议（适用全文 SQL）**：JOIN `dws_app_gamemode_active` / `dws_app_game_active` 等活跃事实表时，建议补一个 `date_bounds` CTE 显式给出活跃日期窗口，避免扫描注册窗口之外的历史分区。模板：
>
> ```sql
> date_bounds AS (
>     SELECT
>         DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
>         DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
>     FROM reg_base
> )
> -- 然后在活跃 JOIN 上追加：
> --   AND ma.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
> ```
>
> §2.1 给出完整示范，其余 SQL 同构套用。

---

## 二、玩法留存对比

### 2.1 各玩法新增用户留存率

> 使用 `dws_app_gamemode_active` 表判定同玩法留存（uid × dt × play_mode 粒度），使用 `dws_dq_app_daily_reg` 获取注册数据。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群（🌟 全局唯一需要人工维护的时间窗口，后续所有逻辑自动对齐）
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 核心时间调度中心：同时产出 D+0(首日) 和 D+1(次留) 的绝对物理裁剪边界
    -- 确保在不破坏首日和次留业务天数的前提下，给所有事实表提供静态常量进行最严厉的分区裁剪
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_act_date
    FROM reg_base_raw
),
first_day_modes AS (
    -- 3. 提取首日玩过【经典/不洗牌/癞子】的种子用户（利用 D+0 静态边界提升分区检索性能）
    SELECT
        g.uid, g.dt AS reg_date, g.play_mode
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND g.robot != 1
      AND g.play_mode IN (1, 2, 3)
    GROUP BY g.uid, g.dt, g.play_mode

    UNION ALL

    -- 4. 提取首日玩过【510K】的种子用户（玩法标记硬编码为 7）
    SELECT
        cg.uid, cg.dt AS reg_date, 7 AS play_mode
    FROM tcy_temp.dws_crazyddz_daily_game cg
    WHERE cg.app_id = 1880053
      AND cg.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND cg.robot != 1
    GROUP BY cg.uid, cg.dt
),
user_seed_profile AS (
    -- 5. 🛠️ 核心解耦层：将注册基础人群与首日玩法事实执行 INNER JOIN
    -- 锁定精准的业务分母：即"第一天注册，且第一天切实参与了某玩法"的种子用户群
    SELECT
        r.uid, r.reg_date, f.play_mode
    FROM reg_base_raw r
    INNER JOIN first_day_modes f ON r.uid = f.uid AND r.reg_date = f.reg_date
),
all_events_stream AS (
    -- 6. 垂直管道第一层：种子人群的首日初始玩法基础流（作为计算留存的分母基准）
    SELECT uid, play_mode, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_seed_profile

    UNION ALL

    -- 7. 垂直管道第二层：次日同玩法活跃流（作为计算留存的分子基准）
    -- 强制开启 D+1 静态分区裁剪，且只在 user_seed_profile 种子用户范围内寻找相同玩法
    SELECT
        ma.uid, ma.play_mode, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_seed_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode
)
-- 🌟 8. 主查询行列式转换聚合，并内嵌物理提示（HINT）放宽优化器超时时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,

    -- 分母：第一天新登且真正玩过该玩法的去重总人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS mode_reg_users,

    -- 分子 / 分母 = 同玩法次日活跃留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY play_mode
ORDER BY play_mode;
```

### 2.2 各玩法新增用户分布与 7 日同玩法留存

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群（🌟 全局唯一人工维护的时间窗口）
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 核心时间调度中心：同时产出 D0(首日)、D1(次留) 和 D7(7留) 的绝对物理裁剪边界
    -- 确保给所有大事实表提供静态常量，强制触触发集群底层的【刚性分区裁剪】
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date,
        DATE_ADD(MIN(reg_date), INTERVAL 7 DAY) AS min_d7_date,
        DATE_ADD(MAX(reg_date), INTERVAL 7 DAY) AS max_d7_date
    FROM reg_base_raw
),
first_day_modes AS (
    -- 3. 提取首日玩过【经典/不洗牌/癞子】的去重数据
    SELECT
        g.uid, g.dt AS reg_date, g.play_mode
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND g.robot != 1
      AND g.play_mode IN (1, 2, 3)
    GROUP BY g.uid, g.dt, g.play_mode

    UNION ALL

    -- 4. 提取首日玩过【510K】的去重数据（彻底干掉了多余的 INNER JOIN 注册表动作，由下方统一 INNER JOIN 拦截）
    SELECT
        cg.uid, cg.dt AS reg_date, 7 AS play_mode
    FROM tcy_temp.dws_crazyddz_daily_game cg
    WHERE cg.app_id = 1880053
      AND cg.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND cg.robot != 1
    GROUP BY cg.uid, cg.dt
),
user_seed_profile AS (
    -- 5. 🛠️ 核心解耦层：圈定精准的业务分母
    -- 锁定"第一天注册，且第一天切实参与了对应玩法"的种子用户群，打死文本和标签状态
    SELECT
        r.uid, r.reg_date, f.play_mode
    FROM reg_base_raw r
    INNER JOIN first_day_modes f ON r.uid = f.uid AND r.reg_date = f.reg_date
),
all_events_stream AS (
    -- 6. 垂直管道流第一层：基础注册流（作为计算留存的分母基准）
    SELECT uid, play_mode, 1 AS is_reg, 0 AS days_diff_1, 0 AS days_diff_7
    FROM user_seed_profile

    UNION ALL

    -- 7. 垂直管道流第二层：次日(D1)同玩法活跃流
    -- 强制开启 D+1 静态分区裁剪，并局限于种子人群和相同玩法内
    SELECT
        ma.uid, ma.play_mode, 0 AS is_reg, 1 AS days_diff_1, 0 AS days_diff_7
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_seed_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode

    UNION ALL

    -- 8. 垂直管道流第三层：7日(D7)同玩法活跃流
    -- 🌟 自动修正：DATE_ADD(reg_date, 7)，强制开启 D+7 静态分区裁剪
    SELECT
        ma.uid, ma.play_mode, 0 AS is_reg, 0 AS days_diff_1, 1 AS days_diff_7
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_seed_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 7 DAY)
    WHERE ma.dt BETWEEN (SELECT min_d7_date FROM date_bounds) AND (SELECT max_d7_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode
)
-- 🌟 9. 主查询行列式转换聚合，内嵌物理 HINT 防止 Planner 超时
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,

    -- 分母：新登且参与玩法的总人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS reg_users,

    -- 次日(D1)同玩法留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN days_diff_1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1,

    -- 7日(D7)同玩法留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN days_diff_7 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d7
FROM all_events_stream
GROUP BY play_mode
ORDER BY play_mode;
```

---

## 三、玩法内因子分析

> 所有因子分析均使用 `dws_app_allgame_stat` 表（uid × dt × play_mode 粒度），该表覆盖 play_mode 1-7 的全玩法体验数据。

### 3.1 分玩法 × 倍数分组留存

> 字段来源：`dws_app_allgame_stat.avg_magnification`

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
user_profile AS (
    -- 3. 🛠️ 标签解耦层：底层直接 INNER JOIN 圈定目标分子，并把"玩法+倍数区间"打成确定文本
    -- 这样做将复杂的双字段 Case When 逻辑直接在最底层锁死，彻底为后方的聚合和 Shuffle 减负
    SELECT
        r.uid, r.reg_date,
        st.play_mode,
        CASE
            WHEN st.avg_magnification <= 6  THEN 'A: <=6'
            WHEN st.avg_magnification <= 12 THEN 'B: 6-12'
            WHEN st.avg_magnification <= 24 THEN 'C: 12-24'
            ELSE                                 'D: 24+'
        END AS multi_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    WHERE st.play_mode IN (1, 2, 3, 7)
      AND st.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
),
all_events_stream AS (
    -- 4. 垂直管道第一层：基础注册种子人群玩法流（作为计算留存的分母基准）
    SELECT uid, play_mode, multi_group, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_profile

    UNION ALL

    -- 5. 垂直管道第二层：次日(D1)同玩法活跃流（作为计算留存的分子基准）
    -- 强制开启 D+1 静态分区裁剪，并且只在种子用户和相同玩法的网格范围内匹配
    SELECT
        ma.uid, ma.play_mode, p.multi_group, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode, p.multi_group
)
-- 🌟 6. 主查询双维度聚合，内嵌物理 HINT 防止 Planner 超时
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,
    multi_group,

    -- 分母：各玩法各倍数分组下的去重总注册人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 分子 / 分母 = 同玩法次日留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY play_mode, multi_group
ORDER BY play_mode, multi_group;
```

### 3.2 分玩法 × 胜率分组留存

> 字段来源：`dws_app_allgame_stat.win_rate`

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
user_profile AS (
    -- 3. 🛠️ 标签解耦层：底层直接 INNER JOIN 圈定首日切实参与玩法的目标用户
    -- 提前将"胜率区间"在最底层转换为轻量文本标签，防止复杂表达式向后方的聚合流传递
    SELECT
        r.uid, r.reg_date,
        st.play_mode,
        CASE
            WHEN st.win_rate < 30 THEN 'A: <30%'
            WHEN st.win_rate < 50 THEN 'B: 30-50%'
            WHEN st.win_rate < 70 THEN 'C: 50-70%'
            ELSE                       'D: >=70%'
        END AS winrate_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    WHERE st.play_mode IN (1, 2, 3, 7)
      AND st.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
),
all_events_stream AS (
    -- 4. 垂直管道第一层：基础注册种子人群流（作为计算同玩法留存的分母）
    SELECT uid, play_mode, winrate_group, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_profile

    UNION ALL

    -- 5. 垂直管道第二层：次日(D1)同玩法活跃流（作为计算同玩法留存的分子）
    -- 强制开启 D+1 静态分区裁剪，并在 user_profile 种子网格范围内进行去重合拢
    SELECT
        ma.uid, ma.play_mode, p.winrate_group, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode, p.winrate_group
)
-- 🌟 6. 主查询行列式转换聚合，内嵌物理 HINT 放宽优化器编译时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,
    winrate_group,

    -- 分母：该玩法、该胜率区间下的去重新登总人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 分子 / 分母 = 次日同玩法留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY play_mode, winrate_group
ORDER BY play_mode, winrate_group;
```

### 3.3 分玩法 × 对局数分组留存

> 字段来源：`dws_app_allgame_stat.game_count`

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
user_profile AS (
    -- 3. 🛠️ 标签解耦层：底层直接 INNER JOIN 圈定首日切实参与玩法的目标用户
    -- 提前将"对局数区间"在最底层转换为轻量文本标签，防止复杂表达式向后方的聚合流传递
    SELECT
        r.uid, r.reg_date,
        st.play_mode,
        CASE
            WHEN st.game_count = 1   THEN 'A: 1局'
            WHEN st.game_count <= 3  THEN 'B: 2-3局'
            WHEN st.game_count <= 5  THEN 'C: 4-5局'
            WHEN st.game_count <= 10 THEN 'D: 6-10局'
            ELSE                          'E: 10局+'
        END AS game_count_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    WHERE st.play_mode IN (1, 2, 3, 7)
      AND st.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
),
all_events_stream AS (
    -- 4. 垂直管道第一层：基础注册种子人群流（作为计算同玩法留存的分母）
    SELECT uid, play_mode, game_count_group, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_profile

    UNION ALL

    -- 5. 垂直管道第二层：次日(D1)同玩法活跃流（作为计算同玩法留存的分子）
    -- 强制开启 D+1 静态分区裁剪，并在 user_profile 种子网格范围内进行去重合拢
    SELECT
        ma.uid, ma.play_mode, p.game_count_group, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode, p.game_count_group
)
-- 🌟 6. 主查询双维度矩阵聚合，内嵌物理 HINT 放宽优化器编译时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,
    game_count_group,

    -- 分母：该玩法、该局数区间下的去重新登总人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 分子 / 分母 = 次日同玩法留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY play_mode, game_count_group
ORDER BY play_mode, game_count_group;
```

### 3.4 分玩法 × 炸弹分布留存

> 字段来源：`dws_app_allgame_stat.bomb_0_games`, `bomb_1_games`, `bomb_2_games`, `bomb_3plus_games`

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：精准产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
user_profile AS (
    -- 3. 🛠️ 标签解耦层：底层直接 INNER JOIN 圈定首日切实参与玩法的目标用户
    -- 剔除死代码，并在这一层直接将"炸弹频次区间"转换为轻量文本标签
    SELECT
        r.uid, r.reg_date,
        st.play_mode,
        CASE
            WHEN st.bomb_3plus_games > 0 THEN 'D: 高频炸弹(3+炸弹对局)'
            WHEN st.bomb_2_games > 0     THEN 'C: 中频炸弹(2炸弹对局)'
            WHEN st.bomb_1_games > 0     THEN 'B: 低频炸弹(1炸弹对局)'
            ELSE                              'A: 无炸弹'
        END AS bomb_level
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    WHERE st.play_mode IN (1, 2, 3, 7)
      AND st.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
),
all_events_stream AS (
    -- 4. 垂直管道第一层：基础注册种子人群玩法流（作为计算同玩法留存的分母基准）
    SELECT uid, play_mode, bomb_level, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_profile

    UNION ALL

    -- 5. 垂直管道第二层：次日(D1)同玩法活跃流（作为计算同玩法留存的分子基准）
    -- 强制开启 D+1 静态分区裁剪，并且只在种子用户和相同玩法的网格范围内匹配去重
    SELECT
        ma.uid, ma.play_mode, p.bomb_level, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode, p.bomb_level
)
-- 🌟 6. 主查询双维度聚合输出，内嵌物理 HINT 放宽优化器时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,
    bomb_level,

    -- 分母：该玩法、该炸弹频次下的去重新登总人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 分子 / 分母 = 同玩法次日留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY play_mode, bomb_level
ORDER BY play_mode, bomb_level;
```

### 3.5 分玩法 × 多维度四分位体验留存

> 字段来源：`dws_app_allgame_stat` 的固定倍数段（`multi_1` ~ `multi_384_plus` 系列的 `_win`/`_lose`）
>
> 将固定倍数段归并为四档：低倍 `<6x`、中低 `6-12x`、中高 `12-24x`、高倍 `≥24x`。取用户首日落到过的最高档位，观察各档位对留存的影响。绝对阈值跨玩法可比（510K 与经典的 24x 同义）。
>
> 注：表 v1.2（2026-06-17）已移除旧 NTILE 四分位字段 `multi_q1~q4`，改为固定绝对阈值段，本节据此改写。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：精准产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
user_profile AS (
    -- 3. 🛠️ 标签解耦层：在最底层直接 INNER JOIN，并将高能消耗的 Case When 表达式一次性算死
    -- 剔除死代码和未使用字段，仅保留最终聚合所需要的维度和度量
    SELECT
        r.uid, r.reg_date,
        st.play_mode,
        COALESCE(st.game_count, 0) AS total_quartile_games,
        CASE
            WHEN (COALESCE(st.multi_24_48_win, 0) + COALESCE(st.multi_24_48_lose, 0)
                + COALESCE(st.multi_48_96_win, 0) + COALESCE(st.multi_48_96_lose, 0)
                + COALESCE(st.multi_96_192_win, 0) + COALESCE(st.multi_96_192_lose, 0)
                + COALESCE(st.multi_192_384_win, 0) + COALESCE(st.multi_192_384_lose, 0)
                + COALESCE(st.multi_384_plus_win, 0) + COALESCE(st.multi_384_plus_lose, 0)) > 0 THEN 'D: 有高倍对局(≥24x)'
            WHEN (COALESCE(st.multi_12_24_win, 0) + COALESCE(st.multi_12_24_lose, 0)) > 0 THEN 'C: 有中高倍对局(12-24x)'
            WHEN (COALESCE(st.multi_6_12_win, 0) + COALESCE(st.multi_6_12_lose, 0)) > 0 THEN 'B: 有中低倍对局(6-12x)'
            ELSE 'A: 仅低倍对局(<6x)'
        END AS max_quartile_level
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    WHERE st.play_mode IN (1, 2, 3, 7)
      AND st.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
),
all_events_stream AS (
    -- 4. 垂直管道第一层：基础注册种子人群玩法流（保留总局数度量，打上 is_reg 标记）
    SELECT uid, play_mode, max_quartile_level, total_quartile_games, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_profile

    UNION ALL

    -- 5. 垂直管道第二层：次日(D1)同玩法活跃流（总局数置为 0 避免重复累加）
    -- 强制开启 D+1 静态分区裁剪，并且只在种子用户和相同玩法的网格范围内匹配
    SELECT
        ma.uid, ma.play_mode, p.max_quartile_level, 0 AS total_quartile_games, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.play_mode = p.play_mode
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, ma.play_mode, p.max_quartile_level
)
-- 🌟 6. 主查询双维度聚合输出，内嵌物理 HINT 放宽优化器时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS play_mode_name,
    max_quartile_level,

    -- 分母：该玩法、该刺激度分组下的去重总人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 平均对局数：仅对首日注册流中的总局数求平均（排除了 UNION 追加流的干扰）
    ROUND(SUM(total_quartile_games) / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0), 1) AS avg_quartile_games,

    -- 分子 / 分母 = 同玩法次日留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY play_mode, max_quartile_level
ORDER BY play_mode, max_quartile_level;
```

### 3.6 510K 专项：结算轮次与 outcome 分析

> 510K（play_mode=7）为多轮制玩法，一局包含多个 settle round。使用 `dws_app_allgame_stat` 中的 `avg_settle_rounds` 和 `outcome_gdp` 字段分析。

#### 3.6.1 510K 平均结算轮次分组留存

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：精准产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
user_profile AS (
    -- 3. 🛠️ 标签解耦层：底层直接 INNER JOIN 锁定 510K（play_mode = 7）首日有行为的新人
    -- 剔除无对局死代码，在最底层一次性把 510K 的"平均结算轮数区间"转换为轻量文本标签
    SELECT
        r.uid, r.reg_date,
        CASE
            WHEN st.avg_settle_rounds <= 3  THEN 'A: <=3轮'
            WHEN st.avg_settle_rounds <= 5  THEN 'B: 4-5轮'
            WHEN st.avg_settle_rounds <= 8  THEN 'C: 6-8轮'
            ELSE                                 'D: 8轮+'
        END AS settle_rounds_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    WHERE st.play_mode = 7
      AND st.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
),
all_events_stream AS (
    -- 4. 垂直管道第一层：510K 新登种子人群基础流（作为计算同玩法留存的分母基准）
    SELECT uid, settle_rounds_group, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_profile

    UNION ALL

    -- 5. 垂直管道第二层：次日(D1) 510K 玩法活跃流（作为计算同玩法留存的分子基准）
    -- 强制开启 D+1 静态分区裁剪，并且只在 510K 种子用户的分组范围内匹配去重
    SELECT
        ma.uid, p.settle_rounds_group, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.play_mode = 7
      AND ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, p.settle_rounds_group
)
-- 🌟 6. 主查询单维度聚合输出，内嵌物理 HINT 放宽优化器时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    settle_rounds_group,

    -- 分母：第一天玩了 510K 且属于该轮数分组下的去重总注册人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 分子 / 分母 = 次日 510K 同玩法留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY settle_rounds_group
ORDER BY settle_rounds_group;
```

#### 3.6.2 510K outcome_gdp 分组留存

> `outcome_gdp` 表示 510K 多轮结算后的总盈亏结果。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：精准产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
user_profile AS (
    -- 3. 🛠️ 标签解耦层：底层直接 INNER JOIN 锁定 510K（play_mode = 7）首日有对局行为的新人
    -- 剔除死代码，在最底层一次性把 510K 的"输赢 GDP 区间"转换为轻量文本标签
    SELECT
        r.uid, r.reg_date,
        CASE
            WHEN st.outcome_gdp < -50000 THEN 'A: 巨亏(<-5万)'
            WHEN st.outcome_gdp < -10000 THEN 'B: 大亏(-5万~-1万)'
            WHEN st.outcome_gdp < 0      THEN 'C: 小亏(-1万~0)'
            WHEN st.outcome_gdp < 10000  THEN 'D: 小赚(0~1万)'
            WHEN st.outcome_gdp < 50000  THEN 'E: 大赚(1万~5万)'
            ELSE                              'F: 巨赚(>5万)'
        END AS outcome_gdp_group
    FROM reg_base_raw r
    INNER JOIN tcy_temp.dws_app_allgame_stat st
        ON st.app_id = r.app_id AND st.uid = r.uid AND st.dt = r.reg_date
    WHERE st.play_mode = 7
      AND st.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
),
all_events_stream AS (
    -- 4. 垂直管道第一层：510K 新登种子人群基础流（作为计算同玩法留存的分母基准）
    SELECT uid, outcome_gdp_group, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_profile

    UNION ALL

    -- 5. 垂直管道第二层：次日(D1) 510K 玩法活跃流（作为计算同玩法留存的分子基准）
    -- 强制开启 D+1 静态分区裁剪，并且只在 510K 种子用户的分组范围内匹配去重
    SELECT
        ma.uid, p.outcome_gdp_group, 0 AS is_reg, 1 AS is_ret_d1
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.play_mode = 7
      AND ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, p.outcome_gdp_group
)
-- 🌟 6. 主查询单维度群组聚合输出，内嵌物理 HINT 放宽优化器时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    outcome_gdp_group,

    -- 分母：第一天玩了 510K 且属于该经济输赢分组下的去重总注册人数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 分子 / 分母 = 次日 510K 同玩法留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS same_mode_d1_rate
FROM all_events_stream
GROUP BY outcome_gdp_group
ORDER BY outcome_gdp_group;
```

---

## 四、跨玩法行为分析

### 4.1 首局玩法选择与留存

> 使用 `dws_ddz_firstday_game` 和 `dws_crazyddz_daily_game` 获取首局玩法，观察各玩法作为"第一局"时的用户量和留存。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：精准产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    -- 严格卡死在 INTERVAL 1 DAY，物理切断随后 29 天的无效大分区扫描
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
first_game_union AS (
    -- 3. 提取经典/不洗牌/癞子的首局时间（下推首日 D0 分区过滤条件）
    SELECT
        g.uid, g.dt AS reg_date,
        MIN_BY(g.play_mode, g.game_datetime) AS first_play_mode,
        MIN(g.game_datetime) AS first_game_time
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND g.robot != 1
      AND g.play_mode IN (1, 2, 3)
    GROUP BY g.uid, g.dt

    UNION ALL

    -- 4. 提取 510K 的首局时间（下推首日 D0 分区过滤条件）
    SELECT
        cg.uid, cg.dt AS reg_date,
        7 AS first_play_mode,
        MIN(cg.start_datetime) AS first_game_time
    FROM tcy_temp.dws_crazyddz_daily_game cg
    WHERE cg.app_id = 1880053
      AND cg.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND cg.robot != 1
    GROUP BY cg.uid, cg.dt
),
user_seed_profile AS (
    -- 5. 🛠️ 标签与人群解耦层：多流交叉去重，锁定"绝对第一局玩法"标签
    -- 改用标准的 INNER JOIN 彻底剔除无对局行为的注册僵尸用户，减轻后续数据 Shuffle 负担
    SELECT
        r.uid, r.reg_date,
        MIN_BY(u.first_play_mode, u.first_game_time) AS first_play_mode
    FROM reg_base_raw r
    INNER JOIN first_game_union u ON r.uid = u.uid AND r.reg_date = u.reg_date
    GROUP BY r.uid, r.reg_date
),
all_events_stream AS (
    -- 6. 垂直管道第一层：基础注册种子人群首局流（计算两项留存的分母基准）
    SELECT uid, first_play_mode, 1 AS is_reg, 0 AS is_ret_any, 0 AS is_ret_same
    FROM user_seed_profile

    UNION ALL

    -- 7. 垂直管道第二层：次日(D1)玩法活跃流（核心行列大合拢）
    -- 强制开启 D+1 物理分区裁剪，并且只和圈定出的种子用户进行轻量关联
    -- 🌟 修正别名报错：将 INNER JOIN 后的 user_profile 修正为 user_seed_profile (别名 p)
    SELECT
        ma.uid,
        p.first_play_mode,
        0 AS is_reg,
        -- 只要次日在指定玩法中有活跃，即满足"任意玩法次留"
        1 AS is_ret_any,
        -- 次日玩的玩法如果命中其首局玩法，则赋予"同玩法次留"标识，在外层用 MAX 压缩
        MAX(CASE WHEN ma.play_mode = p.first_play_mode THEN 1 ELSE 0 END) AS is_ret_same
    FROM tcy_temp.dws_app_gamemode_active ma
    INNER JOIN user_seed_profile p
        ON ma.app_id = 1880053
       AND ma.uid = p.uid
       AND ma.dt = DATE_ADD(p.reg_date, INTERVAL 1 DAY)
    WHERE ma.play_mode IN (1, 2, 3, 7)
      AND ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, p.first_play_mode
)
-- 🌟 8. 主查询单维度聚合输出，内嵌物理 HINT 放宽优化器时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE first_play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        WHEN 7 THEN '510K'
        ELSE '其他'
    END AS first_mode_name,

    -- 分母：首局玩了该玩法的去重总用户数
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 指标 A：任意玩法次留率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_any = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS d1_rate_any_mode,

    -- 指标 B：同玩法次留率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_same = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS d1_rate_same_mode
FROM all_events_stream
GROUP BY first_play_mode
ORDER BY first_play_mode;
```

### 4.2 玩法数量与留存

> 注册当天玩过的玩法数量（play_mode 1,2,3,7）与整体留存的关系。

```sql
WITH reg_base_raw AS (
    -- 1. 注册基础人群
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      -- 🌟 全局唯一人工维护的时间窗口
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-06-15'
),
date_bounds AS (
    -- 2. 统一时间调度：精准产出 D0(首日) 和 D1(次留) 的绝对物理裁剪边界
    -- 严格限制在 INTERVAL 1 DAY，物理抹除随后的无用分区扫描
    SELECT
        MIN(reg_date) AS min_reg_date,
        MAX(reg_date) AS max_reg_date,
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_d1_date,
        DATE_ADD(MAX(reg_date), INTERVAL 1 DAY) AS max_d1_date
    FROM reg_base_raw
),
firstday_game_stream AS (
    -- 3. 收集首日斗地主玩法（前置下推 D0 分区裁剪）
    SELECT g.uid, g.dt AS reg_date, g.play_mode
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND g.robot != 1
      AND g.play_mode IN (1, 2, 3)

    UNION ALL

    -- 4. 收集首日 510K 玩法（前置下推 D0 分区裁剪）
    SELECT cg.uid, cg.dt AS reg_date, 7 AS play_mode
    FROM tcy_temp.dws_crazyddz_daily_game cg
    WHERE cg.app_id = 1880053
      AND cg.dt BETWEEN (SELECT min_reg_date FROM date_bounds) AND (SELECT max_reg_date FROM date_bounds)
      AND cg.robot != 1
),
user_seed_profile AS (
    -- 5. 🛠️ 标签解耦层：在底层直接对首日参与过对局的用户计算去重玩法多元度
    SELECT
        u.uid, u.reg_date,
        COUNT(DISTINCT u.play_mode) AS mode_count
    FROM firstday_game_stream u
    GROUP BY u.uid, u.reg_date
),
all_events_stream AS (
    -- 6. 垂直管道第一层：有对局玩法的种子人群流（分母基准 A）
    SELECT uid, mode_count, 1 AS is_reg, 0 AS is_ret_d1
    FROM user_seed_profile

    UNION ALL

    -- 7. 垂直管道第二层：补齐【0: 无对局】的纯注册僵尸人群（分母基准 B）
    -- 找出在注册表但完全没有对局流行为的用户，打上 mode_count = 0 的标签
    SELECT r.uid, 0 AS mode_count, 1 AS is_reg, 0 AS is_ret_d1
    FROM reg_base_raw r
    LEFT JOIN user_seed_profile p ON r.uid = p.uid AND r.reg_date = p.reg_date
    WHERE p.uid IS NULL

    UNION ALL

    -- 8. 垂直管道第三层：次日(D1) 大盘活跃回流（分子基准）
    -- 强制开启 D+1 物理分区裁剪，通过将次日大盘表与注册基础表轻量关联，继承其 mode_count 标签
    SELECT
        ma.uid,
        COALESCE(p.mode_count, 0) AS mode_count, -- 没对局的用户次日如果活跃了，归入0次档
        0 AS is_reg,
        1 AS is_ret_d1
    FROM tcy_temp.dws_app_game_active ma
    INNER JOIN reg_base_raw r
        ON ma.app_id = r.app_id AND ma.uid = r.uid AND ma.dt = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
    LEFT JOIN user_seed_profile p
        ON p.uid = r.uid AND p.reg_date = r.reg_date
    WHERE ma.dt BETWEEN (SELECT min_d1_date FROM date_bounds) AND (SELECT max_d1_date FROM date_bounds)
    GROUP BY ma.uid, COALESCE(p.mode_count, 0)
)
-- 🌟 9. 主查询多元度单维度聚合，内嵌物理 HINT 放宽优化器时限至 15 秒
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE mode_count
        WHEN 0 THEN '0: 无对局'
        WHEN 1 THEN '1: 仅1种玩法'
        WHEN 2 THEN '2: 尝试2种玩法'
        WHEN 3 THEN '3: 尝试3种玩法'
        WHEN 4 THEN '4: 4种玩法全体验'
        ELSE CONCAT(CAST(mode_count AS VARCHAR), ' 种玩法')
    END AS mode_count_group,

    -- 分母：体验了对应玩法数量的去重总新登人数（含无对局人群）
    COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END) AS user_count,

    -- 分子 / 分母 = 次日大盘任意玩法留存率
    ROUND(
        COUNT(DISTINCT CASE WHEN is_ret_d1 = 1 THEN uid END) * 100.0
        / NULLIF(COUNT(DISTINCT CASE WHEN is_reg = 1 THEN uid END), 0),
        2
    ) AS d1_rate_any_mode
FROM all_events_stream
GROUP BY mode_count
ORDER BY mode_count;
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
date_bounds AS (
    -- 活跃事实表分区裁剪窗口
    SELECT
        DATE_ADD(MIN(reg_date), INTERVAL 1 DAY) AS min_act_date,
        DATE_ADD(MAX(reg_date), INTERVAL 30 DAY) AS max_act_date
    FROM reg_base
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
        AND ma.dt BETWEEN (SELECT min_act_date FROM date_bounds) AND (SELECT max_act_date FROM date_bounds)
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
