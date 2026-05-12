# 高危信号下钻 SQL：早期流失问题验证

> 本文档提供早期流失等高危留存问题的下钻 SQL，每条 SQL 给出期望验证的假设，用于定位流失根因。
>
> **依赖表**：
>
> - `dws_dq_app_daily_reg` — APP端注册用户宽表
> - `dws_dq_daily_login` — 每日登录聚合表
> - `dws_ddz_app_game_stat` — 用户每日游戏行为聚合表
> - `dws_ddz_firstday_game` — 首日对局明细表

---

## 目录

- [问题1：早期流失用户下钻（1-3局）](#问题1早期流失用户下钻1-3局)
- [问题2：Cocos-Lua iOS客户端下钻](#问题2cocos-lua-ios客户端下钻)
- [问题3：咪咕渠道下钻](#问题3咪咕渠道下钻)
- [问题4：破产与补助下钻](#问题4破产与补助下钻)

---

## 问题1：早期流失用户下钻（1-3局）

> **核心疑问**：全局数据显示 1 局用户次留仅 10%（比完全不玩游戏的人还低），2-5 局用户次留仅 16%。为何这部分用户快速放弃？需从「对手构成 / 博弈烈度 / 经济变化 / 客户端稳定性」四个角度交叉验证。

### Q1.1 1局用户首局对手构成（验证新手保护设计）

> **假设**：首局应当匹配 2 个机器人做新手保护。如果真人对手占比高，说明保护失效，用户被真人打败后流失。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count = 1
    WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
),
first_game_resultguid AS (
    SELECT u.uid AS new_uid, u.reg_date, g.resultguid, g.dt
    FROM one_game_users u
    INNER JOIN tcy_temp.dws_ddz_firstday_game g
        ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date AND g.robot != 1
),
table_composition AS (
    SELECT f.new_uid,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot = 1 THEN 1 ELSE 0 END) AS opp_robot_cnt,
        SUM(CASE WHEN d.uid <> f.new_uid AND d.robot <> 1 THEN 1 ELSE 0 END) AS opp_human_cnt
    FROM first_game_resultguid f
    INNER JOIN tcy_temp.dws_ddz_daily_game d ON d.dt = f.dt AND d.resultguid = f.resultguid
    GROUP BY f.new_uid
)
SELECT
    CASE
        WHEN opp_robot_cnt = 2 THEN 'A: 2机器人（符合新手保护）'
        WHEN opp_robot_cnt = 1 THEN 'B: 1机器人+1真人'
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
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count = 1
    WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
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
    ROUND(AVG(g.diff_money_pre_tax), 0) AS avg_diff
FROM one_game_users u
INNER JOIN tcy_temp.dws_ddz_firstday_game g
    ON g.app_id = u.app_id AND g.uid = u.uid AND g.dt = u.reg_date AND g.robot != 1
GROUP BY 1, 2
ORDER BY grab_pattern, has_bomb;
```

### Q1.3 1-3局用户首局胜负结果

> **假设**：1-3 局用户可能在首局输了之后情绪转负，直接退出。验证首局胜负是否决定早退。

```sql
WITH early_churn_users AS (
    SELECT r.uid, r.reg_date, r.app_id, s.game_count
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND s.game_count BETWEEN 1 AND 3
),
first_game AS (
    SELECT g.uid, g.dt,
           MIN_BY(g.result_id, g.game_datetime) AS first_result,
           MIN_BY(g.role, g.game_datetime)      AS first_role,
           MIN_BY(g.diff_money_pre_tax, g.game_datetime) AS first_diff
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
      AND g.robot != 1
    GROUP BY g.uid, g.dt
)
SELECT
    e.game_count,
    CASE f.first_result WHEN 1 THEN 'A: 首局胜' WHEN 2 THEN 'B: 首局负' ELSE 'C: 无对手数据' END AS first_result,
    CASE f.first_role   WHEN 1 THEN '1: 地主'   WHEN 2 THEN '2: 农民'  ELSE 'X: 异常' END AS first_role,
    COUNT(DISTINCT e.uid) AS user_count,
    ROUND(AVG(f.first_diff), 0) AS avg_first_diff
FROM early_churn_users e
LEFT JOIN first_game f ON f.uid = e.uid AND f.dt = e.reg_date
GROUP BY 1, 2, 3
ORDER BY 1, 2, 3;
```

### Q1.4 1-3局用户首日经济体验（单局最大亏损）

> **假设**：如果在前 1-3 局中遭遇单局"一把清空"（如 diff < -5000），用户会因瞬间挫败直接退出。

```sql
WITH early_churn_users AS (
    SELECT r.uid, r.reg_date, r.app_id, s.game_count, s.total_diff_money
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND s.game_count BETWEEN 1 AND 3
),
max_loss_game AS (
    SELECT g.uid, g.dt, MIN(g.diff_money_pre_tax) AS worst_diff, MAX(g.magnification) AS worst_multi
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
      AND g.robot != 1
    GROUP BY g.uid, g.dt
)
SELECT
    e.game_count,
    CASE
        WHEN m.worst_diff IS NULL          THEN 'X: 无真人对局'
        WHEN m.worst_diff >= 0             THEN 'A: 未输过'
        WHEN m.worst_diff >= -1000         THEN 'B: 小输(-0~-1000)'
        WHEN m.worst_diff >= -5000         THEN 'C: 中输(-1000~-5000)'
        WHEN m.worst_diff >= -20000        THEN 'D: 大输(-5000~-2万)'
        ELSE                                    'E: 爆亏(<-2万)'
    END AS worst_loss_group,
    COUNT(DISTINCT e.uid) AS user_count,
    ROUND(AVG(m.worst_multi), 1) AS avg_worst_multi
FROM early_churn_users e
LEFT JOIN max_loss_game m ON m.uid = e.uid AND m.dt = e.reg_date
GROUP BY 1, 2
ORDER BY 1, 2;
```

### Q1.5 1-3局用户客户端与登录稳定性交叉

> **假设**：如果 1-3 局早退用户的首日登录次数异常（≥3），说明可能存在闪退/掉线等稳定性问题。结合客户端版本进一步定位是否某版本导致。

```sql
SELECT
    CASE s.game_count
        WHEN 1 THEN 'A: 1局'
        WHEN 2 THEN 'B: 2局'
        WHEN 3 THEN 'C: 3局'
    END AS game_count,
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'L1: 1次'
        WHEN r.first_day_login_cnt = 2 THEN 'L2: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'L3: 3-5次（可疑）'
        ELSE 'L4: 6+次（高度异常）'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt
FROM tcy_temp.dws_dq_app_daily_reg r
INNER JOIN tcy_temp.dws_ddz_app_game_stat s
    ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date
WHERE r.app_id = 1880053
  AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND s.game_count BETWEEN 1 AND 3
GROUP BY 1, 2, 3
ORDER BY 1, 2, 3;
```

### Q1.6 1-3局用户渠道分布（验证渠道流量质量）

> **假设**：如果咪咕、iOS 等低质渠道的「1-3局即走」占比显著高于大盘，则"渠道流量不匹配"是关键流失原因，而非产品体验问题。

```sql
WITH reg_with_channel AS (
    SELECT r.uid, r.reg_date, r.app_id,
        CASE WHEN r.channel_category_name IN ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
             THEN r.channel_category_name ELSE '其他' END AS channel
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND r.is_login_log_missing = 0
),
game_bucket AS (
    SELECT rc.channel, rc.uid, rc.reg_date, rc.app_id,
        CASE
            WHEN s.game_count IS NULL OR s.game_count = 0 THEN 'A: 0局'
            WHEN s.game_count = 1                         THEN 'B: 1局'
            WHEN s.game_count BETWEEN 2 AND 3             THEN 'C: 2-3局'
            WHEN s.game_count BETWEEN 4 AND 10            THEN 'D: 4-10局'
            ELSE                                               'E: 10+局'
        END AS game_count_group
    FROM reg_with_channel rc
    LEFT JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = rc.app_id AND s.uid = rc.uid AND s.dt = rc.reg_date
)
SELECT
    channel,
    game_count_group,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(COUNT(DISTINCT uid) * 100.0 /
          SUM(COUNT(DISTINCT uid)) OVER (PARTITION BY channel), 2) AS pct_in_channel,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(gb.reg_date, INTERVAL 1 DAY)
              THEN gb.uid END) * 100.0 / COUNT(DISTINCT gb.uid), 2) AS day1_rate
FROM game_bucket gb
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = gb.app_id AND l.uid = gb.uid
    AND l.login_date = DATE_ADD(gb.reg_date, INTERVAL 1 DAY)
GROUP BY channel, game_count_group
ORDER BY channel, game_count_group;
```

---

## 问题2：Cocos-Lua iOS客户端下钻

### Q2.1 Cocos-Lua iOS的对局参与率

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END AS client_lang,
    CASE WHEN r.reg_group_id IN (8, 88) THEN 'iOS' WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android' ELSE '其他' END AS platform,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN g.game_count IS NULL OR g.game_count = 0 THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS no_game_pct,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10' AND r.reg_group_id IN (8, 88)
GROUP BY 1, 2;
```

### Q2.2 Cocos-Lua iOS首日登录次数（检测崩溃）

```sql
SELECT
    CASE r.reg_app_code WHEN 'zgda' THEN 'Cocos-Lua' WHEN 'zgdx' THEN 'Cocos-Creator' ELSE '其他' END AS client_lang,
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1次'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5次'
        ELSE 'D: 6次以上（高度异常）'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(AVG(r.first_day_login_cnt), 1) AS avg_login_cnt
FROM tcy_temp.dws_dq_app_daily_reg r
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.reg_group_id IN (8, 88)
GROUP BY 1, 2
ORDER BY client_lang, login_cnt_group;
```

---

## 问题3：咪咕渠道下钻

### Q3.1 咪咕渠道对局参与率

```sql
SELECT
    CASE WHEN r.channel_category_name IN ('OPPO','IOS','vivo','华为','咪咕','官方(非CPS)','荣耀')
         THEN r.channel_category_name ELSE '其他' END AS channel,
    COUNT(DISTINCT r.uid) AS reg_users,
    ROUND(COUNT(DISTINCT CASE WHEN g.game_count IS NULL OR g.game_count = 0 THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS no_game_pct,
    ROUND(AVG(COALESCE(g.game_count, 0)), 1) AS avg_games
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
GROUP BY 1
ORDER BY no_game_pct DESC;
```

### Q3.2 咪咕渠道首日登录次数分布

```sql
SELECT
    CASE
        WHEN r.first_day_login_cnt = 1 THEN 'A: 1次（注册即走）'
        WHEN r.first_day_login_cnt = 2 THEN 'B: 2次'
        WHEN r.first_day_login_cnt BETWEEN 3 AND 5 THEN 'C: 3-5次'
        ELSE 'D: 5+次'
    END AS login_cnt_group,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT r.uid) * 100.0 / SUM(COUNT(DISTINCT r.uid)) OVER (), 2) AS pct
FROM tcy_temp.dws_dq_app_daily_reg r
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND r.channel_category_name = '咪咕'
GROUP BY 1
ORDER BY 1;
```

## 问题4：破产与补助下钻

### Q4.1 破产后是否领取补助与留存

> **产品设计**：破产后系统通常会提供每日有限次数的低保/救济金。领取救济金的用户留存率理论上应高于未领取直接退出的用户。

```sql
SELECT
    CASE 
        WHEN g.money_valley > 1000 THEN 'A: 未破产'
        WHEN g.money_valley <= 1000 AND s.subsidy_count > 0 THEN 'B: 破产且领补助'
        WHEN g.money_valley <= 1000 AND (s.subsidy_count IS NULL OR s.subsidy_count = 0) THEN 'C: 破产未领补助'
        ELSE 'D: 异常'
    END AS bankrupt_behavior,
    COUNT(DISTINCT r.uid) AS user_count,
    ROUND(COUNT(DISTINCT CASE WHEN l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
              THEN r.uid END) * 100.0 / COUNT(DISTINCT r.uid), 2) AS day1_rate
FROM tcy_temp.dws_dq_app_daily_reg r
LEFT JOIN tcy_temp.dws_ddz_app_game_stat g
    ON g.app_id = r.app_id AND g.uid = r.uid AND g.dt = r.reg_date
LEFT JOIN (
    -- 假设存在免费金币/救济金领取日志表
    SELECT uid, dt, COUNT(*) AS subsidy_count
    FROM tcy_temp.dws_dq_silver_logs
    WHERE op_type = '破产补助' -- 根据实际表结构调整
    GROUP BY uid, dt
) s ON s.uid = r.uid AND s.dt = r.reg_date
LEFT JOIN tcy_temp.dws_dq_daily_login l
    ON l.app_id = r.app_id AND l.uid = r.uid AND l.login_date = DATE_ADD(r.reg_date, INTERVAL 1 DAY)
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
  AND g.game_count > 0
GROUP BY 1
ORDER BY 1;
```

---

## 专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数/胜率/经济差异 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |