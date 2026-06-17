# 高危信号下钻 SQL：早期流失问题验证

> 本文档提供早期流失等高危留存问题的下钻 SQL，每条 SQL 给出期望验证的假设，用于定位流失根因。
>
> **分析时间段**：2026-02-10 至 2026-05-10
> **依赖表**：
>
> - `dws_dq_app_daily_reg` — APP端注册用户宽表（含渠道分类、客户端版本、设备类型）
> - `dws_dq_daily_login` — 每日登录聚合表
> - `dws_app_game_active` — 留存 flag 表（任意玩法有对局即活跃）
> - `dws_app_silvergame_stat` — 银子玩法金流 + 参与度聚合
> - `dws_app_allgame_stat` — 全玩法体验分析（倍数 / 炸弹 / 胜率）
> - `dws_ddz_firstday_game` — 经典斗地主首日对局明细
> - `dws_ddz_daily_game` — 经典斗地主全量对局明细（含机器人标识）

---

## 目录

- [问题1：早期流失用户下钻（1-3局）](#问题1早期流失用户下钻1-3局)
- [问题2：客户端稳定性下钻](#问题2客户端稳定性下钻)
- [问题3：渠道质量下钻](#问题3渠道质量下钻)
- [问题4：破产与补助下钻](#问题4破产与补助下钻)
- [问题5：高倍局创伤下钻](#问题5高倍局创伤下钻)

---

## 问题1：早期流失用户下钻（1-3局）

> **核心疑问**：全局数据显示 1 局用户次留仅 10%（比完全不玩游戏的人还低），2-5 局用户次留仅 16%。为何这部分用户快速放弃？需从「对手构成 / 博弈烈度 / 经济变化 / 客户端稳定性 / 渠道质量」五个角度交叉验证。

### Q1.1 1局用户首局对手构成（验证新手保护设计）

> **假设**：首局应当匹配 2 个机器人做新手保护。如果真人对手占比高，说明保护失效，用户被真人打败后流失。
>
> **逻辑**：从 `dws_ddz_firstday_game` 获取 1 局用户的首局 resultguid，关联 `dws_ddz_daily_game` 查找该 resultguid 的所有参与者，区分机器人和真人。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
),
first_game_resultguid AS (
    SELECT u.uid AS new_uid, u.reg_date, g.resultguid, g.dt
    FROM one_game_users u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
),
table_composition AS (
    SELECT f.new_uid,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot = 1 THEN 1 ELSE 0 END) AS opp_robot_cnt,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot <> 1 THEN 1 ELSE 0 END) AS opp_human_cnt
    FROM first_game_resultguid f
    INNER JOIN tcy_temp.dws_ddz_daily_game d
        ON d.dt = f.dt AND d.resultguid = f.resultguid
    GROUP BY f.new_uid
)
SELECT
    CASE
        WHEN opp_robot_cnt = 2 THEN 'A: 2机器人（符合新手保护）'
        WHEN opp_robot_cnt = 1 THEN 'B: 1机器人 + 1真人'
        WHEN opp_robot_cnt = 0 THEN 'C: 2真人（无新手保护）'
        ELSE 'Z: 异常'
    END AS opponent_pattern,
    COUNT(*) AS user_count,
    ROUND(COUNT(*) * 100.0 / SUM(COUNT(*)) OVER (), 2) AS pct
FROM table_composition
GROUP BY 1
ORDER BY 1;
```

### Q1.2 1局用户首局博弈烈度

> **假设**：抢地主、倍数、炸弹都会放大首局的输赢波动，博弈烈度越高，"一把就走"的概率越大。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count = 1
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
)
SELECT
    CASE g.grab_landlord_bet
        WHEN 3  THEN 'A: 无人抢（默认叫地主）'
        WHEN 6  THEN 'B: 1人抢'
        WHEN 12 THEN 'C: 2人抢'
        ELSE        'D: 异常'
    END AS grab_pattern,
    CASE WHEN g.bomb_bet >= 4 THEN 'Y: 有炸弹' ELSE 'N: 无炸弹' END AS has_bomb,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(AVG(g.magnification), 1) AS avg_multi,
    ROUND(AVG(g.game_outcome_money), 0) AS avg_outcome
FROM one_game_users u
INNER JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date
GROUP BY 1, 2
ORDER BY grab_pattern, has_bomb;
```

### Q1.3 1-3局用户首局胜负结果

> **假设**：1-3 局用户可能在首局输了之后情绪转负，直接退出。验证首局胜负是否决定早退。

```sql
WITH early_churn_users AS (
    SELECT r.uid, r.reg_date, r.app_id, s.game_count
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND s.game_count BETWEEN 1 AND 3
),
first_game AS (
    SELECT g.uid, g.dt,
           MIN_BY(g.result_id, g.game_datetime) AS first_result,
           MIN_BY(g.role, g.game_datetime)      AS first_role,
           MIN_BY(g.game_outcome_money, g.game_datetime) AS first_outcome
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
    GROUP BY g.uid, g.dt
)
SELECT
    e.game_count,
    CASE f.first_result
        WHEN 1 THEN 'A: 首局胜'
        WHEN 2 THEN 'B: 首局负'
        ELSE 'C: 无对手数据'
    END AS first_result,
    CASE f.first_role
        WHEN 1 THEN '1: 地主'
        WHEN 2 THEN '2: 农民'
        ELSE 'X: 异常'
    END AS first_role,
    COUNT(DISTINCT e.uid) AS user_count,
    ROUND(AVG(f.first_outcome), 0) AS avg_first_outcome
FROM early_churn_users e
LEFT JOIN first_game f ON f.uid = e.uid AND f.dt = e.reg_date
GROUP BY 1, 2, 3
ORDER BY 1, 2, 3;
```

### Q1.4 1-3局用户首日最大单局亏损

> **假设**：如果在前 1-3 局中遭遇单局"一把清空"（如 game_outcome_money < -5000），用户会因瞬间挫败直接退出。

```sql
WITH early_churn_users AS (
    SELECT r.uid, r.reg_date, r.app_id, s.game_count, s.total_diff_money
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND s.game_count BETWEEN 1 AND 3
),
max_loss_game AS (
    SELECT g.uid, g.dt,
           MIN(g.game_outcome_money) AS worst_outcome,
           MAX(g.magnification) AS worst_multi
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
    GROUP BY g.uid, g.dt
)
SELECT
    e.game_count,
    CASE
        WHEN m.worst_outcome IS NULL           THEN 'X: 无对局数据'
        WHEN m.worst_outcome >= 0              THEN 'A: 未输过'
        WHEN m.worst_outcome >= -1000          THEN 'B: 小输（-0 ~ -1000）'
        WHEN m.worst_outcome >= -5000          THEN 'C: 中输（-1000 ~ -5000）'
        WHEN m.worst_outcome >= -20000         THEN 'D: 大输（-5000 ~ -2万）'
        ELSE                                        'E: 爆亏（<-2万）'
    END AS worst_loss_group,
    COUNT(DISTINCT e.uid) AS user_count,
    ROUND(AVG(m.worst_multi), 1) AS avg_worst_multi
FROM early_churn_users e
LEFT JOIN max_loss_game m ON m.uid = e.uid AND m.dt = e.reg_date
GROUP BY 1, 2
ORDER BY 1, 2;
```

### Q1.5 1-3局用户客户端稳定性交叉

> **假设**：如果 1-3 局早退用户的首日登录次数异常（>=3），说明可能存在闪退 / 掉线等稳定性问题。结合客户端版本进一步定位是否某版本导致。

```sql
SELECT
    CASE s.game_count
        WHEN 1 THEN 'A: 1局'
        WHEN 2 THEN 'B: 2局'
        WHEN 3 THEN 'C: 3局'
    END AS game_count,
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1          THEN 'L1: 1次'
        WHEN r.first_day_login_cnt = 2          THEN 'L2: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'L3: 3-5次（可疑）'
        ELSE 'L4: 6次以上（高度异常）'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND s.game_count BETWEEN 1 AND 3
GROUP BY 1, 2, 3
ORDER BY 1, 2, 3;
```

### Q1.6 1-3局用户渠道分布（验证渠道流量质量）

> **假设**：如果某些渠道的 "1-3 局即走" 占比显著高于大盘，则"渠道流量不匹配"是关键流失原因，而非产品体验问题。

```sql
WITH reg_with_channel AS (
    SELECT r.uid, r.reg_date, r.app_id,
        CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀')
             THEN r.channel_category_name ELSE '其他' END AS channel
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
),
game_bucket AS (
    SELECT rc.channel, rc.uid, rc.reg_date, rc.app_id,
        CASE
            WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'A: 0局'
            WHEN s.game_count = 1                         THEN 'B: 1局'
            WHEN s.game_count BETWEEN 2 AND 3             THEN 'C: 2-3局'
            WHEN s.game_count BETWEEN 4 AND 10            THEN 'D: 4-10局'
            ELSE                                               'E: 10局以上'
        END AS game_count_group
    FROM reg_with_channel rc
    LEFT JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = rc.app_id AND s.uid = rc.uid AND s.dt = rc.reg_date
)
SELECT
    channel,
    game_count_group,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT uid) * 100.0 /
          SUM(COUNT(DISTINCT uid)) OVER (PARTITION BY channel), 2) AS pct_in_channel,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(gb.reg_date, INTERVAL 1 DAY)
              THEN gb.uid END) * 100.0 / NULLIF(COUNT(DISTINCT gb.uid), 0), 2) AS day1_rate
FROM game_bucket gb
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = gb.app_id AND l.uid = gb.uid
    AND l.login_date = DATE_ADD(gb.reg_date, INTERVAL 1 DAY)
GROUP BY channel, game_count_group
ORDER BY channel, game_count_group;
```

---

## 问题2：客户端稳定性下钻

> **核心疑问**：首日多次登录（>=3 次）反映客户端闪退 / 掉线问题。这些用户的游戏参与率和留存如何？是否为崩溃导致无法正常对局？

### Q2.1 多次登录用户游戏参与率（按客户端版本）

> **假设**：首日登录 >=3 次的用户中，游戏参与率（有对局比例）可能低于单次登录用户。如果某客户端版本的参与率显著更低，则稳定性问题更严重。

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1          THEN 'A: 1次（正常）'
        WHEN r.first_day_login_cnt = 2          THEN 'B: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5次（可疑）'
        WHEN r.first_day_login_cnt >= 6         THEN 'D: 6次以上（异常）'
        ELSE 'Z: 未知'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN ga.dt = r.reg_date AND ga.is_game_active = 1
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS game_active_pct,
    ROUND(COUNT(DISTINCT CASE WHEN s.game_count IS NOT NULL AND s.game_count > 0
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS silvergame_pct,
    ROUND(AVG(COALESCE(s.game_count, 0)), 1) AS avg_games
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_game_active ga
    ON ga.app_id = r.app_id AND ga.uid = r.uid AND ga.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
GROUP BY 1, 2
ORDER BY client_lang, login_cnt_group;
```

### Q2.2 多次登录用户登录次数分布（按客户端 x 平台）

> **假设**：不同客户端版本和平台组合的崩溃率不同，Cocos-Lua iOS 可能在部分设备上存在闪退问题。

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
        WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END AS platform,
    CASE
        WHEN r.first_day_login_cnt = 1          THEN 'L1: 1次'
        WHEN r.first_day_login_cnt = 2          THEN 'L2: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'L3: 3-5次'
        WHEN r.first_day_login_cnt >= 6         THEN 'L4: 6次以上'
        ELSE 'LZ: 未知'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT r.uid) * 100.0 /
          SUM(COUNT(DISTINCT r.uid)) OVER (PARTITION BY r.reg_app_code, r.reg_group_id), 2) AS pct_in_group,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt
FROM tcy_temp.dws_dq_app_daily_reg r
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.first_day_login_cnt >= 3
GROUP BY 1, 2, 3
ORDER BY client_lang, platform, login_cnt_group;
```

### Q2.3 多次登录 + 低对局数用户：崩溃是否阻止了对局？

> **假设**：如果用户多次登录（>=3）但游戏参与度极低（0-1局），说明崩溃发生在进入游戏前或对局中，稳定性问题直接导致了低游戏参与。

```sql
SELECT
    CASE r.reg_app_code
        WHEN 'zgda' THEN 'Cocos-Lua'
        WHEN 'zgdx' THEN 'Cocos-Creator'
        ELSE '其他'
    END AS client_lang,
    CASE
        WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
        WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
        ELSE '其他'
    END AS platform,
    CASE
        WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'A: 0局（注册即崩）'
        WHEN s.game_count = 1 THEN 'B: 1局'
        WHEN s.game_count BETWEEN 2 AND 5 THEN 'C: 2-5局'
        ELSE 'D: 5局以上'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.first_day_login_cnt >= 3
GROUP BY 1, 2, 3
ORDER BY client_lang, platform, game_count_group;
```

---

## 问题3：渠道质量下钻

> **核心疑问**：不同渠道新增用户的质量差异显著，某些渠道的"注册即流失"比例远高于大盘。需要量化各渠道的"游戏参与率"、"登录活跃度"和"留存"。

### Q3.1 渠道 x 游戏参与率（无对局率）

> **假设**：部分渠道（如买量渠道）用户可能对斗地主游戏本身不感兴趣，注册后根本不进入游戏。

```sql
SELECT
    CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE
              WHEN ga.dt = r.reg_date AND (ga.is_game_active IS NULL OR ga.is_game_active = 0)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS no_game_active_pct,
    ROUND(COUNT(DISTINCT CASE
              WHEN s.game_count IS NULL OR s.game_count = 0
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS no_silvergame_pct,
    ROUND(AVG(COALESCE(s.game_count, 0)), 1) AS avg_games,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_game_active ga
    ON ga.app_id = r.app_id AND ga.uid = r.uid AND ga.dt = r.reg_date
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
GROUP BY 1
ORDER BY no_silvergame_pct DESC;
```

### Q3.2 渠道 x 首日登录次数分布

> **假设**：某些渠道的"注册即走"（login_cnt=1）比例更高。如果某渠道的 login_cnt=1 占比异常高，说明渠道流量本身质量较低（非目标用户）。

```sql
SELECT
    CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    CASE
        WHEN r.first_day_login_cnt = 1          THEN 'A: 1次（注册即走）'
        WHEN r.first_day_login_cnt = 2          THEN 'B: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5次'
        WHEN r.first_day_login_cnt >= 6         THEN 'D: 6次以上'
        ELSE 'Z: 未知'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT r.uid) * 100.0 /
          SUM(COUNT(DISTINCT r.uid)) OVER (PARTITION BY r.channel_category_name), 2) AS pct_in_channel,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
GROUP BY 1, 2
ORDER BY channel, login_cnt_group;
```

### Q3.3 渠道 x 首日对局数分布与留存

> **假设**：高质量渠道用户即使首日对局数少，留存也应高于低质渠道的同对局数组。如果某渠道在相同 game_count 组内留存显著偏低，说明渠道用户匹配度差。

```sql
SELECT
    CASE WHEN r.channel_category_name IN ('OPPO', 'IOS', 'vivo', '华为', '咪咕', '官方(非CPS)', '荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    CASE
        WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'A: 0局'
        WHEN s.game_count = 1                         THEN 'B: 1局'
        WHEN s.game_count BETWEEN 2 AND 5             THEN 'C: 2-5局'
        WHEN s.game_count BETWEEN 6 AND 10            THEN 'D: 6-10局'
        ELSE                                               'E: 10局以上'
    END AS game_count_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
GROUP BY 1, 2
ORDER BY channel, game_count_group;
```

---

## 问题4：破产与补助下钻

> **核心疑问**：用户在首日是否因经济破产（银子谷值 <= 最低房间门槛）而无法继续对局？破产的时间点、破产后的行为是什么？

### Q4.1 破产定义与留存

> **假设**：破产定义：`dws_app_silvergame_stat.money_valley` <= 1000（破产线）。破产用户留存率应显著低于未破产用户。
>
> **逻辑**：使用 `dws_app_silvergame_stat` 的 `money_valley`（首日银子谷值），与固定破产线 1000 比较。

```sql
SELECT
    CASE
        WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'C: 无对局'
        WHEN s.money_valley <= 1000     THEN 'A: 破产（谷值 <= 1000）'
        ELSE 'B: 未破产'
    END AS bankrupt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day7_rate,
    ROUND(AVG(s.money_valley), 0) AS avg_money_valley,
    1000 AS bankruptcy_threshold
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_app_silvergame_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND s.game_count > 0
GROUP BY 1
ORDER BY 1;
```

### Q4.2 破产时机：在第几局破产？

> **假设**：破产通常发生在连续大亏的几局后。如果破产集中在开局前几局（1-3局），说明新手保护不足，用户被高倍局清空。

```sql
WITH bankrupt_users AS (
    SELECT r.uid, r.reg_date, r.app_id,
           s.money_valley, s.game_count
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND s.money_valley <= 1000
      AND s.game_count > 0
),
game_sequence AS (
    SELECT bu.uid, bu.reg_date, g.game_datetime, g.game_outcome_money,
           g.magnification, g.role, g.result_id,
           ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.game_datetime) AS game_seq
    FROM bankrupt_users bu
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = bu.app_id AND g.uid = bu.uid AND g.dt = bu.reg_date
),
-- 取每个用户最后一局（破产通常发生在最后几局）
last_game AS (
    SELECT uid, reg_date, game_seq AS bankrupt_game_seq
    FROM (
        SELECT uid, reg_date, game_seq,
               ROW_NUMBER() OVER (PARTITION BY uid, reg_date ORDER BY game_seq DESC) AS rn
        FROM game_sequence
    ) t
    WHERE rn = 1
)
SELECT
    CASE
        WHEN lg.bankrupt_game_seq = 1 THEN 'A: 第1局'
        WHEN lg.bankrupt_game_seq = 2 THEN 'B: 第2局'
        WHEN lg.bankrupt_game_seq = 3 THEN 'C: 第3局'
        WHEN lg.bankrupt_game_seq <= 5 THEN 'D: 第4-5局'
        WHEN lg.bankrupt_game_seq <= 10 THEN 'E: 第6-10局'
        ELSE 'F: 10局以上'
    END AS bankrupt_game_seq,
    COUNT(DISTINCT gs.uid) AS user_count,
    ROUND(AVG(gs.game_outcome_money), 0) AS avg_outcome,
    ROUND(AVG(gs.magnification), 1) AS avg_multi,
    SUM(CASE WHEN gs.result_id = 2 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0) AS loss_rate
FROM last_game lg
INNER JOIN game_sequence gs ON lg.uid = gs.uid AND lg.reg_date = gs.reg_date
    AND lg.bankrupt_game_seq = gs.game_seq
GROUP BY 1
ORDER BY 1;
```

### Q4.3 破产后行为：用户是否继续对局？

> **假设**：破产后部分用户可能领取救济金继续游戏，部分可能直接退出。领取救济金的用户留存是否高于未领取的？

```sql
WITH bankrupt_users AS (
    SELECT r.uid, r.reg_date, r.app_id,
           s.money_valley, s.game_count
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_app_silvergame_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND s.money_valley <= 1000
      AND s.game_count > 0
),
bankrupt_timing AS (
    SELECT bu.uid, bu.reg_date, bu.app_id,
           MIN(g.game_datetime) AS first_game_time,
           MAX(g.game_datetime) AS last_game_time,
           COUNT(*) AS total_games
    FROM bankrupt_users bu
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = bu.app_id AND g.uid = bu.uid AND g.dt = bu.reg_date
    GROUP BY bu.uid, bu.reg_date, bu.app_id
),
post_bankrupt_play AS (
    SELECT bt.uid, bt.reg_date, bt.total_games,
           CASE
               WHEN bt.total_games <= 1 THEN 'A: 仅1局'
               WHEN bt.total_games <= 3 THEN 'B: 2-3局'
               ELSE 'C: 3局以上'
           END AS game_count_after_bankrupt
    FROM bankrupt_timing bt
)
SELECT
    game_count_after_bankrupt,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT uid) * 100.0 /
          SUM(COUNT(DISTINCT uid)) OVER (), 2) AS pct,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(pb.reg_date, INTERVAL 1 DAY)
              THEN pb.uid END) * 100.0 / NULLIF(COUNT(DISTINCT pb.uid), 0), 2) AS day1_rate
FROM post_bankrupt_play pb
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = 1880053 AND l.uid = pb.uid
    AND l.login_date = DATE_ADD(pb.reg_date, INTERVAL 1 DAY)
GROUP BY 1
ORDER BY 1;
```

---

## 问题5：高倍局创伤下钻

> **核心疑问**：`dws_app_allgame_stat` 提供了 `multi_q4_losses`（最高 25% 倍数的输局数）字段。经历高倍输局的用户，留存是否显著降低？高倍输局是否与玩法（经典 / 癞子 / 不洗牌）相关？

### Q5.1 高倍输局（Q4 倍数）用户的留存

> **假设**：经历了 Q4 高倍输局的用户，留存率显著低于未经历用户。multi_q4_losses >= 1 即触发高危信号。

```sql
SELECT
    CASE
        WHEN a.multi_q4_losses IS NULL OR a.multi_q4_losses = 0 THEN 'A: 未经历高倍输局'
        WHEN a.multi_q4_losses = 1                              THEN 'B: 经历1次高倍输'
        WHEN a.multi_q4_losses = 2                              THEN 'C: 经历2次高倍输'
        ELSE                                                         'D: 经历3次以上高倍输'
    END AS q4_loss_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day7_rate,
    ROUND(AVG(a.multi_q4_losses), 1) AS avg_q4_losses,
    ROUND(AVG(a.avg_multi), 1) AS avg_multi
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_app_allgame_stat a
    ON a.app_id = r.app_id AND a.uid = r.uid AND a.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
GROUP BY 1
ORDER BY 1;
```

### Q5.2 高倍输局 x 玩法交叉（癞子是否更危险？）

> **假设**：癞子玩法（play_mode=3）天然高倍，高倍输局发生率更高。验证不同玩法中高倍输局对留存的影响差异。

```sql
SELECT
    CASE a.play_mode
        WHEN 1 THEN '经典'
        WHEN 2 THEN '不洗牌'
        WHEN 3 THEN '癞子'
        ELSE '其他'
    END AS play_mode_name,
    CASE
        WHEN a.multi_q4_losses IS NULL OR a.multi_q4_losses = 0 THEN 'A: 未经历高倍输'
        ELSE 'B: 经历高倍输'
    END AS q4_loss_flag,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day7_rate,
    ROUND(AVG(a.avg_multi), 1) AS avg_multi,
    ROUND(AVG(a.bomb_0_games + a.bomb_1_games + a.bomb_2_games + a.bomb_3plus_games), 1) AS avg_bomb_games
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_app_allgame_stat a
    ON a.app_id = r.app_id AND a.uid = r.uid AND a.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND a.play_mode IN (1, 2, 3)
GROUP BY 1, 2
ORDER BY play_mode_name, q4_loss_flag;
```

### Q5.3 首局即高倍输的用户留存

> **假设**：如果用户首局即遭遇高倍（Q4 倍数范围）且输了，其留存率可能极低（<5%）。这是最严重的高危信号组合。

```sql
WITH first_game_multi AS (
    SELECT g.uid, g.dt, g.app_id,
           g.magnification, g.result_id, g.play_mode,
           ROW_NUMBER() OVER (PARTITION BY g.uid, g.dt ORDER BY g.game_datetime) AS rn
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
),
high_multi_first_loss AS (
    SELECT fm.uid, fm.dt, fm.app_id, fm.play_mode
    FROM first_game_multi fm
    WHERE fm.rn = 1
      AND fm.result_id = 2
      AND fm.magnification >= (
          SELECT PERCENTILE_APPROX(magnification, 0.75)
          FROM tcy_temp.dws_ddz_firstday_game
          WHERE app_id = 1880053 AND dt BETWEEN '2026-02-10' AND '2026-05-10'
              AND robot != 1 AND play_mode IN (1, 2, 3)
      )
)
SELECT
    CASE
        WHEN hm.uid IS NOT NULL THEN 'A: 首局高倍输'
        ELSE 'B: 其他'
    END AS first_game_exp,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day1_rate,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 6 DAY)
              THEN r.uid END) * 100.0 / NULLIF(COUNT(DISTINCT r.uid), 0), 2) AS day7_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN high_multi_first_loss hm
    ON hm.app_id = r.app_id AND hm.uid = r.uid AND hm.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid
    AND l.login_date IN (DATE_ADD(r.reg_date, INTERVAL 1 DAY), DATE_ADD(r.reg_date, INTERVAL 6 DAY))
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
GROUP BY 1
ORDER BY 1;
```

---

## 专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数 / 胜率 / 经济差异 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性 / 性能 / 登录次数异常 |
| [retention-analysis-framework.md](retention-analysis-framework.md) | 分析框架 | 视角与指标速查 |

---

> **文档版本**：v2.0
> **创建时间**：2026-06-15
> **更新说明**：
>
> - v2.0：基于 2026-06-11 重构后的数仓表结构全面更新。移除已废弃的 `is_login_log_missing` 过滤条件。移除已下线的 `dws_ddz_app_game_stat`。游戏留存改用 `dws_app_game_active`，银子玩法指标改用 `dws_app_silvergame_stat`，全玩法体验改用 `dws_app_allgame_stat`。新增问题5（高倍局创伤下钻）。所有 SQL 使用 `DATE` 类型和 `DATE_ADD` 语法。

