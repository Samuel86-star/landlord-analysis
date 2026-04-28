# 三大留存问题下钻 SQL（StarRocks）

> **目的**：基于 [`docs/retention-analysis-2026-04-22.md`](retention-analysis-2026-04-22.md) 识别的三大留存问题，提供可直接执行的下钻 SQL。每条 SQL 给出执行后期望验证的假设，便于分析者按图索骥。
>
> **依赖表**（详见 [`retention-global.md`](retention-global.md) 第二章）：
>
> - `tcy_temp.dws_dq_app_daily_reg` — APP 端注册用户宽表
> - `tcy_temp.dws_dq_daily_login` — 每日登录聚合表
> - `tcy_temp.dws_ddz_daily_game` — 对局战绩明细表
> - `tcy_temp.dws_ddz_app_game_stat` — 用户每日游戏行为聚合表
> - `tcy_temp.dws_ddz_app_gamemode_stat` — 用户每日游戏行为按玩法拆分聚合表
> - `tcy_temp.dws_ddz_firstday_game` — 首日对局明细表
>
> **公共参数**：
>
> ```text
> app_id            = 1880053
> reg_date          BETWEEN '2026-02-10' AND '2026-04-22'
> is_login_log_missing = 0          -- 排除登录日志缺失
> robot             != 1            -- 仅真人对局
> play_mode         IN (1, 2, 3, 5) -- 经典/不洗牌/癞子/比赛
> reg_group_id      IN (8, 88)              -- iOS
> reg_group_id      IN (6, 66, 33, 44, 77, 99) -- Android
> reg_app_code      = 'zgda'  -- Cocos-Lua（旧）
> reg_app_code      = 'zgdx'  -- Cocos-Creator（新）
> ```

---

## 目录

- [问题 1：1 局用户下钻](#问题-11-局用户下钻为什么打了一局就走)
- [问题 2：Cocos-Lua iOS 客户端下钻](#问题-2cocos-lua-ios-客户端下钻d1-仅-1170)
- [问题 3：咪咕渠道下钻](#问题-3咪咕渠道下钻)
- [附录：参考查询](#附录参考查询用于交叉验证)

---

## 问题 1：1 局用户下钻（为什么打了一局就走）

### 已知事实（来自分析报告）

- 1 局用户共 **8,803 人**，占新增 **9.0%**，次留 **10.04%**（全分组最低）
- 玩法分布：经典 95.5%、不洗牌 4.2%
- 角色：**地主 77.1%**、农民 22.8%（异常偏地主）
- 时长：1-2 分钟 **66.8%**
- 完成情况：100% 正常完成（无逃跑）
- 经济：小亏 54.2%、小赚 44.2%

### 产品设计上下文（必须验证）

> **设计意图**：第一局必匹配 2 个机器人，对新手做保护（让赢/降低难度），避免被真人按地上摩擦后流失。
>
> 因此，1 局用户的首局对手构成是必查项 —— 如果数据反过来出现真人对手，说明"新手保护"设计没落地，是首要修复点。详见 [Q1.7](#q17-1-局用户首局对手构成机器人-vs-真人)。

### 下钻假设清单

| ID | 假设 | 对应 SQL |
| --- | --- | --- |
| H1.1 | 1 局用户的"地主"身份是抢/默认产生？ | [Q1.1](#q11-1-局用户首局的地主成因抢地主-vs-默认叫地主) |
| H1.2 | 1 局用户在「注册到首局」之间是否经历了长等待/异常？ | [Q1.2](#q12-注册到首局的间隔分布) |
| H1.3 | 1 局用户的首局上下文（抢地主、加倍、炸弹、春天）有何特点？ | [Q1.3](#q13-1-局用户首局博弈烈度抢地主加倍炸弹春天) |
| H1.4 | 1 局用户首局相比 2-5 局用户的首局，体验有何系统差异？ | [Q1.4](#q14-1-局-vs-2-5-局首局体验对比) |
| H1.5 | 1 局用户次留下来的 10% 与流失的 90%，首局有何关键区别？ | [Q1.5](#q15-1-局用户内部留存差异画像) |
| H1.6 | 1 局用户中是否存在"系统秒退"信号（首日登录次数异常）？ | [Q1.6](#q16-1-局用户首日登录次数与渠道交叉) |
| H1.7 | 1 局用户的首局对手是机器人还是真人？是否符合"首局匹配 2 机器人"的新手保护设计？ | [Q1.7](#q17-1-局用户首局对手构成机器人-vs-真人) |

---

### Q1.1 1 局用户首局的地主成因（抢地主 vs 默认叫地主）

> 验证 H1.1：77% 是地主，到底是用户主动抢的还是 0 号位默认叫的。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
)
SELECT
    CASE g.role WHEN 1 THEN '地主' WHEN 2 THEN '农民' ELSE '其他' END AS role,
    -- 抢地主烈度：3=无人抢、6=1 人抢、12=2 人抢
    CASE g.grab_landlord_bet
        WHEN 3  THEN 'A: 无人抢（默认）'
        WHEN 6  THEN 'B: 1 人抢'
        WHEN 12 THEN 'C: 2 人抢'
        ELSE        'D: 异常'
    END AS grab_pattern,
    g.result_id,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(AVG(g.magnification), 1) AS avg_magnification,
    ROUND(AVG(g.diff_money_pre_tax), 0) AS avg_diff_money,
    ROUND(AVG(g.timecost), 0) AS avg_seconds
FROM one_game_users u
INNER JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
    AND g.robot != 1
    AND g.play_mode IN (1, 2, 3, 5)
GROUP BY 1, 2, 3
ORDER BY role, grab_pattern, g.result_id;
```

**预期产出**：

- 若「地主 + 无人抢」占比远高于均值，说明 1 局用户的地主身份大多来自 0 号位默认叫地主（非主动博弈）
- 若该组的 `avg_magnification` 远高于其他渠道平均，说明默认叫地主的用户被卷入高倍局而流失

---

### Q1.2 注册到首局的间隔分布

> 验证 H1.2：注册之后是不是隔了很久才进对局，导致体验已经"凉了"。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id, r.reg_datetime
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
),
first_game_time AS (
    SELECT
        u.uid, u.reg_date, u.reg_datetime,
        FROM_UNIXTIME(MIN(g.time_unix) / 1000) AS first_game_datetime
    FROM one_game_users u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
        AND g.robot != 1
        AND g.play_mode IN (1, 2, 3, 5)
    GROUP BY u.uid, u.reg_date, u.reg_datetime
)
SELECT
    CASE
        WHEN gap_seconds < 60     THEN 'A: <1 分钟'
        WHEN gap_seconds < 300    THEN 'B: 1-5 分钟'
        WHEN gap_seconds < 1800   THEN 'C: 5-30 分钟'
        WHEN gap_seconds < 3600   THEN 'D: 30-60 分钟'
        WHEN gap_seconds < 7200   THEN 'E: 1-2 小时'
        ELSE                            'F: 2 小时+'
    END AS reg_to_firstgame_gap,
    COUNT(*) AS user_count,
    ROUND(AVG(gap_seconds), 0) AS avg_gap_seconds
FROM (
    SELECT
        uid,
        TIMESTAMPDIFF(SECOND, reg_datetime, first_game_datetime) AS gap_seconds
    FROM first_game_time
) t
GROUP BY 1
ORDER BY 1;
```

**预期产出**：

- 如果 1 局用户注册到首局间隔 <1 分钟占比极高 → 进游戏立即对局，1 局退出是首局体验问题
- 如果 30 分钟+ 间隔占比异常高 → 引导路径长/卡顿，用户冷却后失去耐心

---

### Q1.3 1 局用户首局博弈烈度（抢地主、加倍、炸弹、春天）

> 验证 H1.3：首局是不是因为公共/个人倍数因子叠加，对新手太"重"。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
)
SELECT
    -- 公共倍数 = magnification / magnification_stacked
    CASE
        WHEN g.magnification / NULLIF(g.magnification_stacked, 0) <= 3  THEN 'A: 公倍=3（叫地主无抢无炸）'
        WHEN g.magnification / NULLIF(g.magnification_stacked, 0) <= 6  THEN 'B: 公倍 3-6'
        WHEN g.magnification / NULLIF(g.magnification_stacked, 0) <= 12 THEN 'C: 公倍 6-12'
        WHEN g.magnification / NULLIF(g.magnification_stacked, 0) <= 24 THEN 'D: 公倍 12-24'
        ELSE                                                                'E: 公倍 24+'
    END AS public_multi,
    CASE g.magnification_stacked
        WHEN 1 THEN 'a: 不加倍'
        WHEN 2 THEN 'b: 加倍'
        WHEN 4 THEN 'c: 超级加倍'
        ELSE        'z: 异常'
    END AS personal_stack,
    CASE WHEN g.bomb_bet >= 4 THEN 'Y: 有炸弹' ELSE 'N: 无炸弹' END AS has_bomb,
    CASE WHEN g.complete_victory_bet = 2 THEN 'Y: 春天/反春' ELSE 'N: 无' END AS spring,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(AVG(g.magnification), 1) AS avg_total_multi,
    ROUND(AVG(g.diff_money_pre_tax), 0) AS avg_diff_money,
    ROUND(AVG(g.timecost), 0) AS avg_seconds
FROM one_game_users u
INNER JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
    AND g.robot != 1
    AND g.play_mode IN (1, 2, 3, 5)
GROUP BY 1, 2, 3, 4
ORDER BY user_count DESC
LIMIT 50;
```

**预期产出**：

- 关注「公倍 24+」+「超级加倍」+「有炸弹」组合的用户量与 `avg_diff_money` —— 这是流失高危信号
- 若超级加倍率（`magnification_stacked=4`）显著高于均值，说明新手在首局就主动激进加倍

---

### Q1.4 1 局 vs 2-5 局首局体验对比

> 验证 H1.4：到底 1 局用户的首局比 2-5 局用户的首局体验差在哪。

```sql
WITH classified_users AS (
    SELECT r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.game_count = 1                  THEN '1局用户'
            WHEN s.game_count BETWEEN 2 AND 5      THEN '2-5局用户'
        END AS user_seg
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
      AND s.game_count BETWEEN 1 AND 5
),
first_game AS (
    SELECT
        u.user_seg, u.uid, u.reg_date,
        MIN_BY(g.role,                   g.time_unix) AS first_role,
        MIN_BY(g.result_id,              g.time_unix) AS first_result,
        MIN_BY(g.magnification,          g.time_unix) AS first_multi,
        MIN_BY(g.magnification_stacked,  g.time_unix) AS first_stack,
        MIN_BY(g.grab_landlord_bet,      g.time_unix) AS first_grab,
        MIN_BY(g.bomb_bet,               g.time_unix) AS first_bomb,
        MIN_BY(g.complete_victory_bet,   g.time_unix) AS first_spring,
        MIN_BY(g.diff_money_pre_tax,     g.time_unix) AS first_diff,
        MIN_BY(g.timecost,               g.time_unix) AS first_timecost,
        MIN_BY(g.room_base,              g.time_unix) AS first_room_base
    FROM classified_users u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = 1880053 AND g.uid = u.uid AND g.dt = u.reg_date
        AND g.robot != 1
        AND g.play_mode IN (1, 2, 3, 5)
    GROUP BY u.user_seg, u.uid, u.reg_date
)
SELECT
    user_seg,
    COUNT(*)                                                     AS user_count,
    ROUND(AVG(CASE WHEN first_role = 1 THEN 1.0 ELSE 0 END)*100, 2) AS pct_landlord,
    ROUND(AVG(CASE WHEN first_result = 1 THEN 1.0 ELSE 0 END)*100, 2) AS first_win_rate,
    ROUND(AVG(first_multi), 1)                                   AS avg_first_multi,
    ROUND(AVG(CASE WHEN first_stack > 1 THEN 1.0 ELSE 0 END)*100, 2) AS pct_personal_stack,
    ROUND(AVG(CASE WHEN first_grab > 3 THEN 1.0 ELSE 0 END)*100, 2)  AS pct_grab_landlord,
    ROUND(AVG(CASE WHEN first_bomb >= 4 THEN 1.0 ELSE 0 END)*100, 2) AS pct_bomb,
    ROUND(AVG(CASE WHEN first_spring = 2 THEN 1.0 ELSE 0 END)*100, 2) AS pct_spring,
    ROUND(AVG(first_diff), 0)                                    AS avg_first_diff,
    ROUND(AVG(first_timecost), 0)                                AS avg_first_timecost,
    ROUND(AVG(first_room_base), 0)                               AS avg_first_room_base
FROM first_game
GROUP BY user_seg
ORDER BY user_seg;
```

**预期产出**：

- 如果两组的 `pct_landlord`、`avg_first_multi`、`first_win_rate` 接近 → 体验差异不在首局参数本身，而在用户主观决定是否继续
- 如果 1 局用户的 `avg_first_room_base` 显著更高 → 携银不足却进了高底分场
- 如果 1 局用户 `avg_first_timecost` 显著更短 → 可能是被春天/超级加倍秒杀

---

### Q1.5 1 局用户内部留存差异画像

> 验证 H1.5：留下来的 10% vs 流失的 90% 首局有什么差。

```sql
WITH one_game AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
),
labeled AS (
    SELECT
        u.uid, u.reg_date,
        CASE WHEN l.uid IS NOT NULL THEN 'A: 次留' ELSE 'B: 流失' END AS retention_flag,
        g.role, g.result_id, g.magnification, g.magnification_stacked,
        g.grab_landlord_bet, g.bomb_bet, g.complete_victory_bet,
        g.diff_money_pre_tax, g.timecost, g.room_base, g.play_mode,
        g.start_money, g.end_money, g.cut
    FROM one_game u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
        AND g.robot != 1
        AND g.play_mode IN (1, 2, 3, 5)
    LEFT JOIN tcy_temp.dws_dq_daily_login l
        ON l.app_id = u.app_id AND l.uid = u.uid
        AND l.login_date = DATE_ADD(u.reg_date, INTERVAL 1 DAY)
)
SELECT
    retention_flag,
    COUNT(*) AS user_count,
    ROUND(AVG(CASE WHEN role = 1 THEN 1.0 ELSE 0 END)*100, 2)         AS pct_landlord,
    ROUND(AVG(CASE WHEN result_id = 1 THEN 1.0 ELSE 0 END)*100, 2)    AS win_rate,
    ROUND(AVG(magnification), 1)                                       AS avg_multi,
    ROUND(AVG(CASE WHEN magnification_stacked > 1 THEN 1.0 ELSE 0 END)*100, 2) AS pct_personal_stack,
    ROUND(AVG(CASE WHEN grab_landlord_bet > 3 THEN 1.0 ELSE 0 END)*100, 2)     AS pct_grab,
    ROUND(AVG(diff_money_pre_tax), 0)                                  AS avg_diff,
    ROUND(AVG(timecost), 0)                                            AS avg_seconds,
    ROUND(AVG(room_base), 0)                                           AS avg_room_base,
    ROUND(AVG(start_money), 0)                                         AS avg_start_money,
    ROUND(AVG(end_money), 0)                                           AS avg_end_money
FROM labeled
GROUP BY retention_flag
ORDER BY retention_flag;
```

**预期产出**：

- 直接对比留存与流失用户在每个维度上的差距，差距最大的几个变量即"导致流失的关键因子候选"

---

### Q1.6 1 局用户首日登录次数与渠道交叉

> 验证 H1.6：1 局用户是否登录次数异常少（注册即走）/ 异常多（崩溃重连）。

```sql
SELECT
    CASE WHEN r.channel_category_name IN
         ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE             '其他'
    END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1 次（注册即走）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2 次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5 次'
        ELSE                                            'D: 5+ 次（异常多）'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_app_game_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    AND s.game_count = 1
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY 1, 2, 3
ORDER BY channel, client_lang, login_cnt_group;
```

**预期产出**：

- 若某渠道/客户端组合下「5+ 次登录」占比高 → 怀疑闪退/掉线
- 若「1 次登录」占比极高 → 玩完一局直接关 App，是产品体验/任务完成型流失

---

### Q1.7 1 局用户首局对手构成（机器人 vs 真人）

> 验证 H1.7：**产品设计上"首局必匹配 2 机器人"做新手保护**。如果数据反过来——大量 1 局用户首局碰到了真人对手，说明保护机制失效或被绕过；同时机器人对手的"输赢倾向"（让赢/正常对抗）也直接决定首局体验。
>
> **判定方法**：通过 `resultguid`（同桌 3 人共享）反查同桌另外 2 人的 `robot` 字段（1=机器人，其他=真人）。
>
> **数据源**：必须用 `dws_ddz_daily_game`（含全部玩家的对局记录），不能只用 `dws_ddz_firstday_game`（后者只覆盖首日新增用户的对局，对手如果不是首日新增就不在表里）。

#### Q1.7.1 1 局用户首局对手机器人数量分布

```sql
WITH one_game_users AS (
    -- 找出 1 局用户
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
),
first_game_resultguid AS (
    -- 取这些用户首局的 resultguid（1 局用户的"首局"=唯一一局）
    SELECT
        u.uid           AS new_uid,
        u.reg_date,
        u.app_id,
        g.resultguid,
        g.dt,
        g.role          AS new_user_role,
        g.result_id     AS new_user_result,
        g.diff_money_pre_tax AS new_user_diff
    FROM one_game_users u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
        AND g.robot != 1
        AND g.play_mode IN (1, 2, 3, 5)
),
table_composition AS (
    -- 反查同桌 3 个座位的机器人/真人数量（不含新手自己）
    SELECT
        f.new_uid,
        f.reg_date,
        f.resultguid,
        f.new_user_role,
        f.new_user_result,
        f.new_user_diff,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot = 1  THEN 1 ELSE 0 END) AS opp_robot_cnt,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot <> 1 THEN 1 ELSE 0 END) AS opp_human_cnt,
        COUNT(*)                                                            AS total_seats
    FROM first_game_resultguid f
    INNER JOIN tcy_temp.dws_ddz_daily_game d
        ON d.dt = f.dt AND d.resultguid = f.resultguid
    GROUP BY f.new_uid, f.reg_date, f.resultguid,
             f.new_user_role, f.new_user_result, f.new_user_diff
)
SELECT
    CASE
        WHEN total_seats <> 3                  THEN 'Z: 异常（座位数≠3）'
        WHEN opp_robot_cnt = 2                 THEN 'A: 2 机器人（符合新手保护）'
        WHEN opp_robot_cnt = 1                 THEN 'B: 1 机器人 + 1 真人'
        WHEN opp_robot_cnt = 0                 THEN 'C: 2 真人（无新手保护）'
        ELSE                                        'Z: 异常'
    END AS opponent_pattern,
    COUNT(*)                                                          AS user_count,
    ROUND(COUNT(*) * 100.0 / SUM(COUNT(*)) OVER (), 2)                AS pct,
    -- 当前组中"地主"占比
    ROUND(SUM(CASE WHEN new_user_role = 1 THEN 1.0 ELSE 0 END)*100.0
          / COUNT(*), 2)                                              AS pct_landlord,
    -- 当前组中"胜"占比
    ROUND(SUM(CASE WHEN new_user_result = 1 THEN 1.0 ELSE 0 END)*100.0
          / COUNT(*), 2)                                              AS first_win_rate,
    -- 平均输赢
    ROUND(AVG(new_user_diff), 0)                                      AS avg_diff_money
FROM table_composition
GROUP BY 1
ORDER BY 1;
```

**预期产出**：

| opponent_pattern | 期望 | 异常信号 |
| --- | --- | --- |
| A: 2 机器人 | 占比应接近 100%（设计意图） | 占比 <80% → 新手保护失效 |
| B: 1 机器人 + 1 真人 | 应极低 | 出现即设计漏洞 |
| C: 2 真人 | 应近 0 | 出现即新手保护被完全绕过 |

- 如果实际 A 占比 < 80%，说明"首局必匹配 2 机器人"的设计未落地或存在漏出
- 对比 A/B/C 三组的 `first_win_rate` 与 `avg_diff_money`：
  - 若 A 组胜率仍偏低（<50%）→ 机器人配合"放水"逻辑失效
  - 若 C 组胜率显著低于 A 组 → 真人对手把新手按地上摩擦，是流失关键来源

#### Q1.7.2 对手构成 × 留存交叉

> 在 Q1.7.1 基础上叠加次留判定，看哪种对手组合的留存最差。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
),
first_game_resultguid AS (
    SELECT u.uid AS new_uid, u.reg_date, u.app_id, g.resultguid, g.dt
    FROM one_game_users u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
        AND g.robot != 1
        AND g.play_mode IN (1, 2, 3, 5)
),
table_composition AS (
    SELECT
        f.new_uid, f.reg_date, f.app_id,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot = 1  THEN 1 ELSE 0 END) AS opp_robot_cnt,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot <> 1 THEN 1 ELSE 0 END) AS opp_human_cnt
    FROM first_game_resultguid f
    INNER JOIN tcy_temp.dws_ddz_daily_game d
        ON d.dt = f.dt AND d.resultguid = f.resultguid
    GROUP BY f.new_uid, f.reg_date, f.app_id
)
SELECT
    CASE
        WHEN t.opp_robot_cnt = 2 THEN 'A: 2 机器人'
        WHEN t.opp_robot_cnt = 1 THEN 'B: 1 机器人 + 1 真人'
        WHEN t.opp_robot_cnt = 0 THEN 'C: 2 真人'
        ELSE                          'Z: 异常'
    END AS opponent_pattern,
    COUNT(DISTINCT t.new_uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(t.reg_date, INTERVAL 1 DAY)
              THEN t.new_uid END) * 100.0 / COUNT(DISTINCT t.new_uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(t.reg_date, INTERVAL 6 DAY)
              THEN t.new_uid END) * 100.0 / COUNT(DISTINCT t.new_uid), 2) AS day7_rate
FROM table_composition t
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = t.app_id AND l.uid = t.new_uid
    AND l.login_date IN (DATE_ADD(t.reg_date, INTERVAL 1 DAY),
                         DATE_ADD(t.reg_date, INTERVAL 6 DAY))
GROUP BY 1
ORDER BY 1;
```

**预期产出**：

- 直接对比"匹配到机器人 vs 真人"对留存的影响量级
- 若 A 组（2 机器人）的 day1_rate 仍只有 10% 左右 → 机器人匹配做了，但首局体验本身仍有问题（参见 Q1.3 的倍数/加倍维度）
- 若 C 组（2 真人）的 day1_rate 显著低于 A 组（如低 5+ pp）→ "首局碰真人"是独立的负面因素，应优先修复匹配逻辑

#### Q1.7.3 推广到 0 局/2-5 局/6+局用户的首局对手构成对比

> 把分析扩展到所有局数分组，看新手保护是否随局数推进而退出（设计应是只前 N 局保护）。

```sql
WITH active_users AS (
    SELECT r.uid, r.reg_date, r.app_id,
        CASE
            WHEN s.game_count = 1            THEN 'A: 1 局'
            WHEN s.game_count BETWEEN 2 AND 5 THEN 'B: 2-5 局'
            WHEN s.game_count BETWEEN 6 AND 10 THEN 'C: 6-10 局'
            ELSE                                  'D: 10+ 局'
        END AS user_seg
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count >= 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
),
first_game_resultguid AS (
    -- 取每个用户的首局（按 time_unix 最小）
    SELECT
        u.user_seg, u.uid AS new_uid, u.reg_date, u.app_id,
        MIN_BY(g.resultguid, g.time_unix) AS resultguid,
        u.reg_date AS dt
    FROM active_users u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
        AND g.robot != 1
        AND g.play_mode IN (1, 2, 3, 5)
    GROUP BY u.user_seg, u.uid, u.reg_date, u.app_id
),
table_composition AS (
    SELECT
        f.user_seg, f.new_uid,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot = 1  THEN 1 ELSE 0 END) AS opp_robot_cnt
    FROM first_game_resultguid f
    INNER JOIN tcy_temp.dws_ddz_daily_game d
        ON d.dt = f.dt AND d.resultguid = f.resultguid
    GROUP BY f.user_seg, f.new_uid
)
SELECT
    user_seg,
    COUNT(*) AS user_count,
    ROUND(SUM(CASE WHEN opp_robot_cnt = 2 THEN 1.0 ELSE 0 END)*100.0/COUNT(*), 2) AS pct_2robot,
    ROUND(SUM(CASE WHEN opp_robot_cnt = 1 THEN 1.0 ELSE 0 END)*100.0/COUNT(*), 2) AS pct_1robot,
    ROUND(SUM(CASE WHEN opp_robot_cnt = 0 THEN 1.0 ELSE 0 END)*100.0/COUNT(*), 2) AS pct_0robot
FROM table_composition
GROUP BY user_seg
ORDER BY user_seg;
```

**预期产出**：

- 验证"首局必匹配 2 机器人"是否对所有局数分组都成立（首局总是新手第 1 局）
- 如果 A（1 局用户）和 B-D（多局用户）的 `pct_2robot` 显著不同，说明匹配逻辑可能有"用户标记/路径差异"分支，需要追溯设计

---

## 问题 2：Cocos-Lua iOS 客户端下钻（D1 仅 11.70%）

### 已知事实

- Cocos-Lua iOS：**2,119 用户，D1=11.70%、D7=4.67%**
- 同 iOS 平台 Cocos-Creator：4,506 用户，D1=27.30%、D7=12.07%（差 15.6 pp）
- 同 Cocos-Lua 在 Android：89,260 用户，D1=22.52%、D7=9.85%
- → 问题集中在 **Cocos-Lua + iOS** 这一交叉单元格

### 下钻假设清单

| ID | 假设 | 对应 SQL |
| --- | --- | --- |
| H2.1 | 是否登录后无法进对局（0 局占比异常）？ | [Q2.1](#q21-cocos-lua-ios-的对局参与率) |
| H2.2 | 进对局的用户首日对局数分布是否被压缩？ | [Q2.2](#q22-cocos-lua-ios-vs-cocos-creator-ios-首日对局数分布) |
| H2.3 | 单局时长是否异常（卡顿/超时/掉线）？ | [Q2.3](#q23-cocos-lua-ios-的对局时长分布) |
| H2.4 | 首日登录次数是否异常多（崩溃重连）？ | [Q2.4](#q24-cocos-lua-ios-的首日登录次数分布) |
| H2.5 | 用户是否在次日切换到 Cocos-Creator？ | [Q2.5](#q25-cocos-lua-ios-的版本切换行为) |
| H2.6 | 渠道分布上是否集中在某些不健康的渠道？ | [Q2.6](#q26-cocos-lua-ios-的渠道分布) |
| H2.7 | 注册时段是否有规律（夜间/白天 BUG）？ | [Q2.7](#q27-cocos-lua-ios-按注册时段的留存) |

---

### Q2.1 Cocos-Lua iOS 的对局参与率

> 验证 H2.1：是否登录后没法进对局（性能/兼容问题导致进不去对局）。

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua'
                        WHEN 'zgdx' THEN 'Cocos-Creator'
                        ELSE '其他' END AS client_lang,
    CASE WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
         WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
         ELSE '其他' END AS platform,
    COUNT(DISTINCT r.uid) AS reg_users,
    COUNT(DISTINCT CASE WHEN g.game_count IS NULL OR g.game_count = 0
                        THEN r.uid END) AS no_game_users,
    ROUND(COUNT(DISTINCT CASE WHEN g.game_count IS NULL OR g.game_count = 0
                              THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2) AS no_game_pct,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY 1, 2
ORDER BY 1, 2;
```

**预期产出**：

- 如果 Cocos-Lua iOS 的 `no_game_pct` 显著高于其他三组 → 登录后进对局存在阻塞（兼容/性能问题）
- 如果 `no_game_pct` 接近 → 问题不在"进入对局"，而在"对局体验"，下钻 Q2.2/Q2.3

---

### Q2.2 Cocos-Lua iOS vs Cocos-Creator iOS 首日对局数分布

> 验证 H2.2：进了对局的用户对局数被压缩。

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua'
                        WHEN 'zgdx' THEN 'Cocos-Creator'
                        ELSE '其他' END AS client_lang,
    CASE
        WHEN g.game_count IS NULL OR g.game_count = 0 THEN 'A: 0 局'
        WHEN g.game_count = 1 THEN 'B: 1 局'
        WHEN g.game_count BETWEEN 2 AND 5 THEN 'C: 2-5 局'
        WHEN g.game_count BETWEEN 6 AND 10 THEN 'D: 6-10 局'
        ELSE 'E: 10+ 局'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY),
                         DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
  AND r.reg_group_id IN (8, 88)        -- 仅 iOS
GROUP BY 1, 2
ORDER BY client_lang, game_count_group;
```

**预期产出**：

- 同 iOS 下两个客户端的对局数分布对比，重点看 0/1 局占比差距，以及高对局数组（10+）留存差距
- 如果 Cocos-Lua iOS 的 0/1 局占比异常高 → 用户进对局门槛卡住

---

### Q2.3 Cocos-Lua iOS 的对局时长分布

> 验证 H2.3：单局耗时是否异常（卡顿/超时/掉线/被踢）。

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua'
                        WHEN 'zgdx' THEN 'Cocos-Creator'
                        ELSE '其他' END AS client_lang,
    CASE
        WHEN g.timecost < 30   THEN 'A: <30s（异常短）'
        WHEN g.timecost < 60   THEN 'B: 30-60s'
        WHEN g.timecost < 120  THEN 'C: 1-2 分钟'
        WHEN g.timecost < 240  THEN 'D: 2-4 分钟'
        WHEN g.timecost < 480  THEN 'E: 4-8 分钟'
        ELSE                       'F: 8+ 分钟（异常长）'
    END AS timecost_group,
    COUNT(*) AS game_count,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(AVG(g.timecost), 0) AS avg_seconds,
    ROUND(AVG(CASE WHEN g.cut < 0 THEN 1.0 ELSE 0 END) * 100, 2) AS escape_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
    AND g.robot != 1
    AND g.play_mode IN (1, 2, 3, 5)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
  AND r.reg_group_id IN (8, 88)        -- 仅 iOS
GROUP BY 1, 2
ORDER BY client_lang, timecost_group;
```

**预期产出**：

- 关注 `<30s` 与 `8+ 分钟` 两端的占比 —— 极短可能是闪退/被踢，极长可能是网络卡顿
- 与 `escape_rate` 交叉：异常时长 + 高逃跑率 = 强卡顿信号

---

### Q2.4 Cocos-Lua iOS 的首日登录次数分布

> 验证 H2.4：登录次数异常多 = 崩溃后反复重连。

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua'
                        WHEN 'zgdx' THEN 'Cocos-Creator'
                        ELSE '其他' END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1                  THEN 'A: 1 次'
        WHEN r.first_day_login_cnt = 2                  THEN 'B: 2 次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5      THEN 'C: 3-5 次'
        WHEN r.first_day_login_cnt BETWEEN 6 AND 10     THEN 'D: 6-10 次'
        ELSE                                                'E: 10+ 次（高度异常）'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
  AND r.reg_group_id IN (8, 88)        -- 仅 iOS
GROUP BY 1, 2
ORDER BY client_lang, login_cnt_group;
```

**预期产出**：

- 如果 Cocos-Lua iOS 的「6+ 次登录」占比显著高于 Cocos-Creator iOS → 强烈崩溃/掉线信号
- 如果两组「1 次登录」占比都很高 → 反而说明用户开了一次就走，不是崩溃问题

---

### Q2.5 Cocos-Lua iOS 的版本切换行为

> 验证 H2.5：用户在 Lua 上注册后是否切换到 Creator（被动/主动迁移）。

```sql
WITH lua_ios_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (8, 88)        -- iOS
      AND r.reg_app_code = 'zgda'          -- 注册时是 Cocos-Lua
)
SELECT
    CASE l.first_app_code
        WHEN 'zgda' THEN 'A: 仍用 Cocos-Lua'
        WHEN 'zgdx' THEN 'B: 切到 Cocos-Creator'
        WHEN NULL   THEN 'C: 未登录'
        ELSE             'D: 其他/未知'
    END AS next_day_client,
    COUNT(DISTINCT u.uid) AS user_count
FROM lua_ios_users u
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = u.app_id AND l.uid = u.uid
    AND l.login_date = DATE_ADD(u.reg_date, INTERVAL 1 DAY)
GROUP BY 1
ORDER BY 1;
```

**预期产出**：

- 如果"切到 Cocos-Creator"用户数显著 → 说明客户端推送了升级/迁移逻辑，留存数据应剔除该部分单独看
- 如果几乎没人切换 → 留存低不是切换造成的，是产品本身问题

---

### Q2.6 Cocos-Lua iOS 的渠道分布

> 验证 H2.6：是否集中于某些低质量渠道。

```sql
SELECT
    CASE WHEN r.channel_category_name IN
         ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY),
                         DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
  AND r.reg_group_id IN (8, 88)
  AND r.reg_app_code = 'zgda'
GROUP BY 1
ORDER BY reg_users DESC;
```

**预期产出**：

- 看用户来自哪些渠道，结合渠道整体留存判断渠道质量影响占比
- 若主要来自 iOS 渠道（应用商店）→ 渠道无问题，问题是客户端版本本身

---

### Q2.7 Cocos-Lua iOS 按注册时段的留存

> 验证 H2.7：是否在某时段集中出问题（服务端波动 / 推送计划）。

```sql
SELECT
    HOUR(r.reg_datetime) AS reg_hour,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
  AND r.reg_group_id IN (8, 88)
  AND r.reg_app_code = 'zgda'
GROUP BY 1
ORDER BY 1;
```

**预期产出**：

- 各时段 `day1_rate` 是否平稳（即时段无关）—— 若是，问题与具体小时无关，是常态故障
- 若某时段断崖式低 → 服务端/推送/CDN 在该时段有问题

---

## 问题 3：咪咕渠道下钻

### 已知事实

- 咪咕渠道：4,685 用户，全部使用 Cocos-Lua
- 该渠道用户 100% 是 Android（无 iOS）
- 同样使用 Cocos-Lua 的其他渠道（华为 23.60%、官方 25.91%）留存正常
- 故"客户端语言"不是直接病因，**渠道本身**是

### 下钻假设清单

| ID | 假设 | 对应 SQL |
| --- | --- | --- |
| H3.1 | 是否多数用户登录后没进对局（用户质量差/刷量）？ | [Q3.1](#q31-咪咕渠道用户的对局参与率) |
| H3.2 | 0 局用户剔除后，咪咕的体验是否正常？ | [Q3.2](#q32-咪咕渠道有对局用户的留存) |
| H3.3 | 咪咕用户的注册时段/日期是否集中（活动刷量）？ | [Q3.3](#q33-咪咕渠道每日新增量与注册时段) |
| H3.4 | 咪咕用户的首日登录次数是否异常（注册即走）？ | [Q3.4](#q34-咪咕渠道首日登录次数) |
| H3.5 | 咪咕中有对局的用户与其他渠道用户首局体验是否一致？ | [Q3.5](#q35-咪咕-vs-其他渠道首局体验对比仅有对局用户) |
| H3.6 | 咪咕渠道中是否有规律性的"机器人式"特征？ | [Q3.6](#q36-咪咕渠道用户行为特征聚合可视为刷量画像) |

---

### Q3.1 咪咕渠道用户的对局参与率

> 验证 H3.1：是否绝大多数用户登录后根本没进对局。

```sql
SELECT
    CASE WHEN r.channel_category_name IN
         ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    COUNT(DISTINCT r.uid)                                                AS reg_users,
    COUNT(DISTINCT CASE WHEN g.game_count IS NULL OR g.game_count = 0
                        THEN r.uid END)                                  AS no_game_users,
    ROUND(COUNT(DISTINCT CASE WHEN g.game_count IS NULL OR g.game_count = 0
                              THEN r.uid END) * 100.0
          / COUNT(DISTINCT r.uid), 2)                                    AS no_game_pct,
    COUNT(DISTINCT CASE WHEN g.game_count = 1 THEN r.uid END)            AS one_game_users,
    ROUND(COUNT(DISTINCT CASE WHEN g.game_count = 1 THEN r.uid END)*100.0
          / COUNT(DISTINCT r.uid), 2)                                    AS one_game_pct,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1)                             AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2)        AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY day1_rate;
```

**预期产出**：

- 比较咪咕的 `no_game_pct` 与其他渠道 —— 若咪咕显著偏高，说明大量用户登录后没进对局，更接近"刷量/低意愿用户"画像
- 全局 0 局用户占比 14.1%，咪咕若 >40% 即明显异常

---

### Q3.2 咪咕渠道有对局用户的留存

> 验证 H3.2：剔除 0 局用户后，留存是否回到正常水平。

```sql
SELECT
    CASE WHEN r.channel_category_name IN
         ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    COUNT(DISTINCT r.uid) AS active_users,
    ROUND(AVG(g.game_count), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
    AND g.game_count >= 1                  -- 仅看进了对局的用户
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY),
                         DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY 1
ORDER BY day1_rate;
```

**预期产出**：

- 若咪咕的 `day1_rate` 在剔除 0 局后接近其他渠道（20%+）→ 问题是"用户质量"，不是"产品体验"
- 若仍显著偏低 → 咪咕用户进了对局后体验也差，需进一步看 Q3.5

---

### Q3.3 咪咕渠道每日新增量与注册时段

> 验证 H3.3：是否集中爆量（活动/刷量），可识别异常日期。

```sql
SELECT
    r.reg_date,
    COUNT(DISTINCT r.uid)                    AS reg_users,
    ROUND(AVG(r.first_day_login_cnt), 1)     AS avg_login_cnt,
    SUM(CASE WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 1 ELSE 0 END) AS reg_dawn,
    SUM(CASE WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 1 ELSE 0 END) AS reg_morning,
    SUM(CASE WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 1 ELSE 0 END) AS reg_afternoon,
    SUM(CASE WHEN HOUR(r.reg_datetime) BETWEEN 18 AND 21 THEN 1 ELSE 0 END) AS reg_evening,
    SUM(CASE WHEN HOUR(r.reg_datetime) BETWEEN 22 AND 23 THEN 1 ELSE 0 END) AS reg_night,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
  AND r.channel_category_name = '咪咕'
GROUP BY r.reg_date
ORDER BY r.reg_date;
```

**预期产出**：

- 关注是否有特定日期 `reg_users` 远高于均值（峰值 = 活动/刷量/事故）
- 若注册时段集中在凌晨 0-5 → 异常人工/机器人来源
- 极端低 `avg_login_cnt`（接近 1.0）+ 极低 `day1_rate` = 注册即走的批量信号

---

### Q3.4 咪咕渠道首日登录次数

> 验证 H3.4：登录次数是否极度偏 1（注册一次就走）。

```sql
SELECT
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1 次（注册即走）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2 次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5 次'
        ELSE                                            'D: 5+ 次'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT r.uid) * 100.0 /
          SUM(COUNT(DISTINCT r.uid)) OVER (), 2) AS pct,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
  AND r.channel_category_name = '咪咕'
GROUP BY 1
ORDER BY 1;
```

**预期产出**：

- 与 L.11 全局数据（1 次登录占 76%）对比，若咪咕「1 次登录」占比 >90% → 强刷量/低意愿信号

---

### Q3.5 咪咕 vs 其他渠道首局体验对比（仅有对局用户）

> 验证 H3.5：进入对局的咪咕用户首局参数是否与其他渠道一致。

```sql
WITH active_users AS (
    SELECT r.uid, r.reg_date, r.app_id,
        CASE WHEN r.channel_category_name = '咪咕' THEN '咪咕' ELSE '其他' END AS ch_seg
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
        AND s.game_count >= 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
)
SELECT
    a.ch_seg,
    COUNT(DISTINCT a.uid) AS user_count,
    ROUND(AVG(g.magnification), 1)                                     AS avg_first_multi,
    ROUND(AVG(CASE WHEN g.role = 1 THEN 1.0 ELSE 0 END)*100, 2)        AS pct_landlord,
    ROUND(AVG(CASE WHEN g.result_id = 1 THEN 1.0 ELSE 0 END)*100, 2)   AS first_win_rate,
    ROUND(AVG(g.diff_money_pre_tax), 0)                                AS avg_first_diff,
    ROUND(AVG(g.timecost), 0)                                          AS avg_first_seconds,
    ROUND(AVG(g.room_base), 0)                                         AS avg_room_base,
    ROUND(AVG(g.start_money), 0)                                       AS avg_start_money,
    ROUND(AVG(CASE WHEN g.cut < 0 THEN 1.0 ELSE 0 END)*100, 2)         AS first_escape_rate
FROM active_users a
INNER JOIN (
    -- 取每个用户的首局
    SELECT app_id, uid, dt,
           MIN_BY(magnification,         time_unix) AS magnification,
           MIN_BY(role,                  time_unix) AS role,
           MIN_BY(result_id,             time_unix) AS result_id,
           MIN_BY(diff_money_pre_tax,    time_unix) AS diff_money_pre_tax,
           MIN_BY(timecost,              time_unix) AS timecost,
           MIN_BY(room_base,             time_unix) AS room_base,
           MIN_BY(start_money,           time_unix) AS start_money,
           MIN_BY(cut,                   time_unix) AS cut
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE app_id = 1880053
      AND dt BETWEEN '2026-02-10' AND '2026-04-22'
      AND robot != 1
      AND play_mode IN (1, 2, 3, 5)
    GROUP BY app_id, uid, dt
) g ON g.app_id = a.app_id AND g.uid = a.uid AND g.dt = a.reg_date
GROUP BY 1
ORDER BY 1;
```

**预期产出**：

- 若两组首局参数（倍数、胜率、底分）接近 → 体验一致，问题不在产品
- 若咪咕的 `avg_room_base` 显著高、`avg_start_money` 显著低 → 携银不足却进高底分，渠道用户的"经济起点"不健康（可能与渠道送的礼包不一致）

---

### Q3.6 咪咕渠道用户行为特征聚合（可视为刷量画像）

> 验证 H3.6：咪咕用户是否有"机器化"特征（注册时段集中、登录次数固定、对局数极低且分布同质化）。

```sql
SELECT
    -- 注册时段聚集程度（标准差）：值越小越同质化
    ROUND(STDDEV_POP(HOUR(r.reg_datetime)), 2)                       AS reg_hour_stddev,
    ROUND(AVG(r.first_day_login_cnt), 2)                             AS avg_login_cnt,
    ROUND(STDDEV_POP(r.first_day_login_cnt), 2)                      AS login_cnt_stddev,
    -- 对局数
    ROUND(AVG(COALESCE(g.game_count, 0)), 2)                         AS avg_games,
    ROUND(STDDEV_POP(COALESCE(g.game_count, 0)), 2)                  AS games_stddev,
    -- 0 局用户占比
    ROUND(SUM(CASE WHEN g.game_count IS NULL OR g.game_count = 0
                   THEN 1.0 ELSE 0 END) * 100.0 / COUNT(*), 2)       AS no_game_pct,
    -- 仅 1 次登录用户占比
    ROUND(SUM(CASE WHEN r.first_day_login_cnt = 1
                   THEN 1.0 ELSE 0 END) * 100.0 / COUNT(*), 2)       AS one_login_pct,
    COUNT(DISTINCT r.uid)                                            AS user_count,
    CASE WHEN r.channel_category_name = '咪咕' THEN '咪咕' ELSE '其他渠道' END AS seg
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0
GROUP BY CASE WHEN r.channel_category_name = '咪咕' THEN '咪咕' ELSE '其他渠道' END
ORDER BY seg;
```

**预期产出**：

- 若咪咕的注册小时标准差极小（接近 0-2）+ 登录次数标准差极小 + 0 局占比极高 → 强烈刷量信号
- 若分布与其他渠道相似 → 是真实用户但质量低，需要从渠道侧（投放素材/落地页）查问题

---

## 附录：参考查询（用于交叉验证）

### A.1 三大问题用户重叠度（看是否同一批人）

> 1 局用户、Cocos-Lua iOS、咪咕渠道是否高度重叠？

```sql
SELECT
    SUM(CASE WHEN one_game = 1 AND lua_ios = 1 THEN 1 ELSE 0 END) AS one_game_AND_lua_ios,
    SUM(CASE WHEN one_game = 1 AND migu = 1    THEN 1 ELSE 0 END) AS one_game_AND_migu,
    SUM(CASE WHEN lua_ios = 1 AND migu = 1     THEN 1 ELSE 0 END) AS lua_ios_AND_migu,
    SUM(one_game) AS one_game_total,
    SUM(lua_ios)  AS lua_ios_total,
    SUM(migu)     AS migu_total
FROM (
    SELECT
        r.uid,
        CASE WHEN s.game_count = 1 THEN 1 ELSE 0 END AS one_game,
        CASE WHEN r.reg_app_code = 'zgda' AND r.reg_group_id IN (8, 88) THEN 1 ELSE 0 END AS lua_ios,
        CASE WHEN r.channel_category_name = '咪咕' THEN 1 ELSE 0 END AS migu
    FROM tcy_temp.dws_dq_app_daily_reg r
    LEFT JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
      AND r.is_login_log_missing = 0
) t;
```

**用途**：若问题用户重叠度高（例如 1 局用户中很多是 Cocos-Lua iOS），那么解决其中一个问题可能联动改善另一个，节省优化成本。

### A.2 关键指标快速校验（与本报告一致性检查）

```sql
-- 验证整体 D1=23.05%、D7=10.94% 是否与已落盘报告一致
SELECT
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY),
                         DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.is_login_log_missing = 0;
```

---

## 执行建议

1. **先跑 A.1**：确认三个问题用户的重叠度，决定优先级
2. **优先级建议**（按数据信号强度）：
   - **P0：Cocos-Lua iOS** — 单一变量影响 15.6 pp，技术问题易定位易修复，先跑 Q2.1 → Q2.4
   - **P0：咪咕渠道** — 留存仅 5%，先跑 Q3.1 + Q3.3 判定是刷量还是体验问题
   - **P0：1 局用户首局对手构成（Q1.7）** — 直接验证"新手保护设计"是否落地，**只需一个 SQL 即可证伪/证实**，是性价比最高的下钻
   - **P1：1 局用户其他维度** — 成因复杂，先跑 Q1.1 + Q1.4 + Q1.5 三组对比
3. **每个 SQL 跑完后**，回填到 [`retention-analysis-2026-04-22.md`](retention-analysis-2026-04-22.md) 的对应问题章节

---

> **文档版本**：v1.1（新增 Q1.7：1 局用户首局对手机器人/真人识别）
> **创建日期**：2026-04-27
> **关联文档**：
>
> - [`retention-analysis-2026-04-22.md`](retention-analysis-2026-04-22.md)（数据分析报告）
> - [`retention-global.md`](retention-global.md)（全局分析框架与 SQL）
> - [`retention-by-mode.md`](retention-by-mode.md)（玩法层分析）
> - [`retention-by-client-lang.md`](retention-by-client-lang.md)（客户端语言层分析）
