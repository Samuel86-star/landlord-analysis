# 高危信号下钻 SQL：三大留存问题验证

> 本文档提供三大留存问题的下钻 SQL，每条 SQL 给出期望验证的假设，用于定位流失根因。
>
> **依赖表**：
> - `dws_dq_app_daily_reg` — APP端注册用户宽表
> - `dws_dq_daily_login` — 每日登录聚合表
> - `dws_ddz_app_game_stat` — 用户每日游戏行为聚合表
> - `dws_ddz_firstday_game` — 首日对局明细表

---

## 目录

- [问题1：1局用户下钻](#问题1-1局用户下钻)
- [问题2：Cocos-Lua iOS客户端下钻](#问题2-cocos-lua-ios客户端下钻)
- [问题3：咪咕渠道下钻](#问题3-咪咕渠道下钻)

---

## 问题1：1局用户下钻

### 已知事实

- 1局用户占新增9.0%，次留10.04%（全分组最低）
- 角色：地主77.1%（异常偏地主）
- 时长：1-2分钟66.8%

### Q1.1 1局用户首局对手构成（验证新手保护设计）

> **产品设计**：首局必匹配2个机器人做新手保护。如果数据出现真人对手，说明保护失效。

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count = 1
    WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
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

**预期产出**：
- A组占比应接近100%（设计意图）
- 若C组出现 → 新手保护被绕过，需修复匹配逻辑

### Q1.2 1局用户首局博弈烈度

```sql
WITH one_game_users AS (
    SELECT r.uid, r.reg_date, r.app_id
    FROM tcy_temp.dws_dq_app_daily_reg r
    INNER JOIN tcy_temp.dws_ddz_app_game_stat s
        ON s.app_id = r.app_id AND s.uid = r.uid AND s.dt = r.reg_date AND s.game_count = 1
    WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
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

---

## 问题2：Cocos-Lua iOS客户端下钻

### 已知事实

- Cocos-Lua iOS：2,119用户，D1=11.70%
- 同iOS平台Cocos-Creator：D1=27.30%（差15.6pp）

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
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22' AND r.reg_group_id IN (8, 88)
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
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.reg_group_id IN (8, 88)
GROUP BY 1, 2
ORDER BY client_lang, login_cnt_group;
```

**预期产出**：
- 若Cocos-Lua iOS「6+次登录」占比显著高于Cocos-Creator → 强烈崩溃信号

---

## 问题3：咪咕渠道下钻

### 已知事实

- 咪咕渠道：4,685用户，次留仅5%
- 全部使用Cocos-Lua，100%是Android
- 同版本其他渠道留存正常（华为23.60%、官方25.91%）

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
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
GROUP BY 1
ORDER BY no_game_pct DESC;
```

**预期产出**：
- 若咪咕no_game_pct显著高于其他渠道（>40%）→ 刷量/低意愿用户信号

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
WHERE r.app_id = 1880053 AND r.reg_date BETWEEN '2026-02-10' AND '2026-04-22'
  AND r.channel_category_name = '咪咕'
GROUP BY 1
ORDER BY 1;
```

**预期产出**：
- 若「1次登录」占比>90% → 强刷量信号

---

## 执行建议

1. **优先级**：
   - **P0：Q1.1 首局对手构成** — 直接验证新手保护设计是否落地
   - **P0：Q2.2 登录次数** — 检测Cocos-Lua iOS崩溃问题
   - **P0：Q3.1/Q3.2** — 判定咪咕是刷量还是体验问题

2. **高危信号组合优先级**：
   | 组合特征 | 留存预期 | 优先级 |
   | -------- | -------- | ------ |
   | 连败≥3 + 银子亏损 + 高倍局输 | 极低（<10%） | P0 |
   | 首局负 + 地主角色 + 高倍局 | 低（<15%） | P0 |
   | 0局/1局 + 多次登录（≥3） | 低（崩溃/掉线） | P0 |

---

## 专项分析索引

| 专项文档 | 分析视角 | 核心问题 |
| -------- | -------- | -------- |
| [retention-global.md](retention-global.md) | 全局层 | 用户属性与核心指标 |
| [retention-by-mode.md](retention-by-mode.md) | 分玩法层 | 玩法内倍数/胜率/经济差异 |
| [retention-by-client-lang.md](retention-by-client-lang.md) | 分客户端层 | 稳定性/性能/登录次数异常 |