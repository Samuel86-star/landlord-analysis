# 留存复核报告：连败 / 破产 / 高危组合

> **目的**：retention-analysis-framework §2.1 中"连败≥3 急剧上升""破产后直接退出比例高"两条规律，与 `retention-global-report.md` §3.3/§3.5 实测数据矛盾；§3 五个高危组合中前四个含有可疑前提。本报告用**控制对局数（engagement）+ 严格末局破产定义**重新验证。
>
> **数据口径**：APP 端 `app_id = 1880053`，`reg_date BETWEEN '2026-05-01' AND '2026-05-15'`，n=20,476 新增用户，D7/D30 已全部到期。
>
> **数据源**：`dws_app_firstday_game_stat`（首日指标宽表）LEFT JOIN `dws_app_retention_flag`（留存 flag）。
>
> **核心方法**：连败/破产都高度共线于对局数（连败3+、末局破产都需要多打牌），直接看留存会被"玩得多→留得住"的 engagement proxy 倒置。复核以 `silver_game_count` 分层作控制变量，在**同等对局数水平内**比较连败/破产的留存。

---

## 一、连败长度：证伪（engagement proxy），仅极端尾部有窄信号

**假设**：连败越长留存越低，≥3 局急剧上升。
**SQL**：`py/tmp/recheck_lose_streak.sql`（分层 `silver_game_count` × `silver_max_lose_streak`）。

| 对局段 | 无连败 | 1连败 | 2连败 | 3-4连败 | 5+连败 | 趋势 |
| ---- | ------ | ----- | ----- | ------- | ------ | ---- |
| 1-5局 | 3.60 | 5.23 | 5.63 | 7.74 | 12.12(n=33) | ↑ 连败越多留存越高 |
| 6-10局 | 7.41(n=27) | 11.25 | 10.93 | 11.78 | 14.44 | ↑ 同上 |
| 11-20局 | — | 16.15 | 15.07 | 16.86 | 16.93 | 平 |
| 21-50局 | — | — | 21.62 | 23.32 | 20.30 | 平 |
| 50+局 | — | — | — | 44.62(n=65) | 22.12(n=104) | ↓ 仅此处骤降 |

（表内为 D7 留存 %，n 为该格用户数，n<50 仅作参考。）

**结论**：

- 控对局数后，1-50 局区间内连败与留存**不相关或正相关**，"连败≥3 急剧上升"**证伪**——连败长度纯粹是 engagement 的代理变量。
- 唯一真实信号在**极端尾部**：50+局用户中 5+连败 D7 骤降至 22.12%（约为 3-4连败 44.62% 的一半），但样本仅 104 人。
- **可落地的窄信号**：重度玩家（50+局）遭遇 5+连败是 P1 级流失预警，需独立验证样本量后决定是否上连败保护。

---

## 二、破产状态：证伪（严格定义仍为 proxy），无稳健信号

**假设**：破产后用户直接退出，留存低。
**SQL**：`py/tmp/recheck_bankrupt.sql`。

**定义对比**：

- 旧定义（global §3.5）：`silver_money_valley <= 1000`（首日谷值，途中最低）——过松，捕捉到大量"玩得多"用户。
- 严格定义（本报告）：`silver_end_money < 1000`（末局后余额，打到底没钱且未回血）——更贴近"破产退出"。

| 对局段 | 严格破产(末局<1k) D7 | 未破产(末局≥1k) D7 | 方向 |
| ---- | ---- | ---- | ---- |
| 1-5局 | 5.62 | 4.90 | 破产反高 |
| 6-10局 | 12.31 | 10.76 | 破产反高 |
| 11-20局 | 16.94 | 15.06 | 破产反高 |
| 21-50局 | 23.36 | 19.41 | 破产反高 |
| 50+局 | 28.07(n=114) | 34.48(n=58) | 破产略低（不稳健） |

**结论**：

- 即使采用严格末局破产定义 + 控对局数，1-50 局区间破产用户留存仍**持平或更高**，proxy 倒置未消除。
- "破产后直接退出比例高"**证伪**。50+局尾部破产 D7 略低于未破产（28% vs 34%），但样本仅 114/58，不构成稳健结论。
- 与 global §3.5 的 caveat 一致并加强：**当前数据不足以验证破产→流失**，破产兜底阈值建议暂缺依据。

---

## 三、高危组合验证：连败≥3 + 银子亏损 + 高倍局输

**背景**：§2.1 中连败、银子净变化单项均已修订/证伪，但 framework §3 将三者组合列为 P0"极低(<10%)"。单项无害不等于组合无害，故独立验证。同样控对局数——组合命中者必然是重度玩家（engagement proxy 嫌疑）。
**SQL**：`py/tmp/recheck_combo.sql`。

三条件：`silver_max_lose_streak>=3` AND `silver_total_diff_money<0` AND `sum(multi_24_48..384_plus _lose)>0`。

| 对局段 | 命中组合 D7 | 未命中 D7 | 命中 D30 | 未命中 D30 | D7 方向 |
| ---- | ---- | ---- | ---- | ---- | ---- |
| 1-5局 | 9.65(n=114) | 5.08 | 8.77 | 4.10 | 命中反高 |
| 6-10局 | 13.84(n=795) | 11.35 | 8.81 | 8.20 | 命中略高 |
| 11-20局 | 16.10(n=1304) | 16.54 | **11.96** | **15.34** | D7 平 / D30 低 3.4pp |
| 21-50局 | 23.79(n=580) | 21.55 | **14.66** | **19.38** | D7 略高 / D30 低 4.7pp |
| 50+局 | 21.95(n=41) | 32.82 | 12.20 | 25.95 | 命中低（n 小） |

**结论**：

- "极低(<10%)"**证伪**：组合命中者 D7 在 9.65%~23.79%，主要可行段(6-50局)均 ≥ 同段未命中基线，非灾难性低留存。
- **存在弱 D30 长期信号**：11-50局重度玩家中，组合命中者 D30 较同段低 3-5pp。即组合不致短期(D7)流失，但拖累长期(D30)留存。
- **降级**：P0 极低(<10%) → P2 弱 D30 信号。若做连败保护，应针对"重度玩家长期留存"而非"急性流失拦截"。
- **"破产 + 不再对局"**（§3 另一组合）：由 §二 复核可直接判定——末局破产用户 D1 回归率不比同段非破产低（1-5局外多数更高），"不再对局"不成立，证伪。

---

## 四、其余高危组合验证

### 4.1 首局负 + 地主角色 + 高倍局（framework §3 原 P0）

**SQL**：`py/tmp/recheck_combo_firstgame.sql`。用 `dws_ddz_firstday_game` 取每 uid 最早一局（`MIN_BY` 按 `game_datetime`），判定首局 `result_id=2`(负) AND `role=1`(地主) AND `magnification>=24`(高倍)。

| 对局段 | 命中 D7 | 未命中 D7 | 命中 n |
| ---- | ---- | ---- | ---- |
| 1-5局 | **1.66** | 5.20 | 181 |
| 6-10局 | 4.88 | 11.82 | 41 |
| 11-20局 | 10.53 | 16.45 | 19 |
| 21-50局 | 28.57 | 22.21 | 7 |

**结论**：

- **信号真实**：1-5局段命中组 D7=1.66%，约为同段基线(5.20%)的 1/3；6-10局段 4.88 vs 11.82 同样显著低。
- **但覆盖极窄**：5月15天仅 248 人命中（首局即"地主+高倍+负"是低概率事件），其中对局数 ≥6 的仅 67 人。
- **降级**：P0 → P1。信号真、可针对首局地主+高倍+负群体触发保护，但目标人群极小，非主力流失拦截点。注：1局段 D7=0/2 用户的胜率=0 与本组合高度重叠（见 4.3）。

### 4.2 0局/1局 + 多次登录（≥3）（framework §3 原 P0）

**SQL**：`py/tmp/recheck_combo_login.sql`。`first_day_login_cnt>=3`（宽表字段）× 0/1局。

| 段 | 多次登录(≥3) D7 | 登录<3 D7 | 多次登录 n |
| ---- | ---- | ---- | ---- |
| 0局 | 2.86 | 2.40 | 35 |
| 1局 | 0.00 | 2.66 | 21 |

**结论**：

- **未观察到稳健信号**：1局段多次登录≥3 D7=0 看似惨，但 n=21 不稳健；0局段命中反略高。
- 0-1局本身已是最低留存群体（D7<3%），多次登录≥3 在该群体内无额外预测力——"崩溃/掉线"假设在本窗口数据不支持。
- **降级**：P0 → P2（证据不足）。0-1局流失主因是对局数不足而非客户端稳定性；若要验证崩溃需走客户端崩溃日志而非登录次数代理。

### 4.3 胜率<30% + 对局数少（≤2）（framework §3 原 P1）

**SQL**：`py/tmp/recheck_combo_winrate.sql`。

| 段 | 命中 D7 | 未命中 D7 | 命中 n |
| ---- | ---- | ---- | ---- |
| 1局 | **1.90** | 3.05 | 631 |
| 2局 | 3.47 | 3.72 | 202 |

**结论**：

- **信号弱**：1局段命中组 D7=1.90 vs 3.05（低 1.15pp，命中组占该段 36.8%）；2局段命中与未命中几乎相同。
- **本质是对局数效应**：≤2局下胜率<30% 几乎必然是 0/2 或 0/1（胜率=0%），此组合在 1局段实际是"首局输"的变体，与 4.1 高度重叠。
- **维持 P1**，但标注为对局数代理：真正的可干预点是"1局且输"群体的首局体验，而非胜率本身。

---

## 五、对 framework 的影响

| 位置 | 修订前 | 修订后 |
| ---- | ---- | ---- |
| §2.1 连败长度 | 与实测矛盾，待复核 | 证伪：控对局数后 1-50 局区间无负相关，纯 engagement proxy；仅 50+局 ×5+连败有窄 P1 信号 |
| §2.1 是否破产 | 与实测矛盾，待复核 | 证伪：严格末局定义 + 控对局数后仍 proxy 倒置，无稳健破产→流失信号 |
| §3 "连败≥3 + 银子亏损 + 高倍局输" | P0 极低(<10%) | 降级 P2：D7 非<10%(证伪)，D30 重度玩家低 3-5pp 弱长期信号 |
| §3 "破产 + 不再对局" | P1 极低 | 证伪：末局破产用户 D1 回归率不比同段非破产低 |
| §3 "首局负 + 地主角色 + 高倍局" | P0 低(<15%) | 降级 P1：信号真(D7 约为基线1/3)但覆盖极窄(n=248)，非主力拦截点 |
| §3 "0局/1局 + 多次登录(≥3)" | P0 低(崩溃/掉线) | 降级 P2：证据不足，0-1局流失主因是对局数非崩溃，需走崩溃日志验证 |
| §3 "胜率<30% + 对局数少(≤2)" | P1 低 | 维持 P1，标注对局数代理：≤2局胜率<30%≈首局输，可干预点是首局体验 |

---

## 六、复核 SQL

### 6.1 连败长度 × 对局数（控制 engagement）

```sql
-- 复核 1：连败长度 × 对局数（控制 engagement 后看真实因果）
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count BETWEEN 1  AND 5  THEN 'A: 1-5局'
        WHEN g.silver_game_count BETWEEN 6  AND 10 THEN 'B: 6-10局'
        WHEN g.silver_game_count BETWEEN 11 AND 20 THEN 'C: 11-20局'
        WHEN g.silver_game_count BETWEEN 21 AND 50 THEN 'D: 21-50局'
        WHEN g.silver_game_count > 50              THEN 'E: 50+局'
    END AS game_count_bucket,
    CASE
        WHEN COALESCE(g.silver_max_lose_streak, 0) = 0 THEN '1: 无连败'
        WHEN g.silver_max_lose_streak = 1              THEN '2: 1连败'
        WHEN g.silver_max_lose_streak = 2              THEN '3: 2连败'
        WHEN g.silver_max_lose_streak BETWEEN 3 AND 4  THEN '4: 3-4连败'
        ELSE                                                '5: 5+连败'
    END AS lose_streak_bucket,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d1_game)  * 100.0 / NULLIF(COUNT(rf.d1_game),  0), 2) AS d1_rate,
    ROUND(SUM(rf.d7_game)  * 100.0 / NULLIF(COUNT(rf.d7_game),  0), 2) AS d7_rate,
    ROUND(SUM(rf.d30_game) * 100.0 / NULLIF(COUNT(rf.d30_game), 0), 2) AS d30_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND g.silver_game_count > 0
GROUP BY game_count_bucket, lose_streak_bucket
ORDER BY game_count_bucket, lose_streak_bucket;
```

### 6.2 破产（严格末局定义）× 对局数（控制 engagement）

```sql
-- 复核 2：破产 — 严格定义(末局后余额) vs 旧定义(谷值)，均控对局数
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count BETWEEN 1  AND 5  THEN 'A: 1-5局'
        WHEN g.silver_game_count BETWEEN 6  AND 10 THEN 'B: 6-10局'
        WHEN g.silver_game_count BETWEEN 11 AND 20 THEN 'C: 11-20局'
        WHEN g.silver_game_count BETWEEN 21 AND 50 THEN 'D: 21-50局'
        WHEN g.silver_game_count > 50              THEN 'E: 50+局'
    END AS game_count_bucket,
    CASE
        WHEN g.silver_end_money < 1000 THEN '1: 严格破产(末局<1k)'
        ELSE                                '2: 未破产(末局>=1k)'
    END AS strict_bankrupt,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d1_game)  * 100.0 / NULLIF(COUNT(rf.d1_game),  0), 2) AS d1_rate,
    ROUND(SUM(rf.d7_game)  * 100.0 / NULLIF(COUNT(rf.d7_game),  0), 2) AS d7_rate,
    ROUND(SUM(rf.d30_game) * 100.0 / NULLIF(COUNT(rf.d30_game), 0), 2) AS d30_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND g.silver_game_count > 0
  AND g.silver_end_money IS NOT NULL
GROUP BY game_count_bucket, strict_bankrupt
ORDER BY game_count_bucket, strict_bankrupt;
```

### 6.3 高危组合"连败≥3 + 银子亏损 + 高倍局输"（控对局数）

```sql
-- 复核 3：高危组合"连败≥3 + 银子亏损 + 高倍局输" 的组合级验证（控对局数）
-- 三条件：silver_max_lose_streak>=3 AND silver_total_diff_money<0 AND sum(multi_24_48..384_plus _lose)>0
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count BETWEEN 1  AND 5  THEN 'A: 1-5局'
        WHEN g.silver_game_count BETWEEN 6  AND 10 THEN 'B: 6-10局'
        WHEN g.silver_game_count BETWEEN 11 AND 20 THEN 'C: 11-20局'
        WHEN g.silver_game_count BETWEEN 21 AND 50 THEN 'D: 21-50局'
        WHEN g.silver_game_count > 50              THEN 'E: 50+局'
    END AS game_count_bucket,
    CASE
        WHEN COALESCE(g.silver_max_lose_streak, 0) >= 3
         AND g.silver_total_diff_money < 0
         AND (COALESCE(g.multi_24_48_lose, 0) + COALESCE(g.multi_48_96_lose, 0)
            + COALESCE(g.multi_96_192_lose, 0) + COALESCE(g.multi_192_384_lose, 0)
            + COALESCE(g.multi_384_plus_lose, 0)) > 0
        THEN '1: 命中组合'
        ELSE '2: 未命中'
    END AS combo_flag,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d1_game)  * 100.0 / NULLIF(COUNT(rf.d1_game),  0), 2) AS d1_rate,
    ROUND(SUM(rf.d7_game)  * 100.0 / NULLIF(COUNT(rf.d7_game),  0), 2) AS d7_rate,
    ROUND(SUM(rf.d30_game) * 100.0 / NULLIF(COUNT(rf.d30_game), 0), 2) AS d30_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND g.silver_game_count > 0
GROUP BY game_count_bucket, combo_flag
ORDER BY game_count_bucket, combo_flag;
```

### 6.4 高危组合"首局负 + 地主 + 高倍局"（控对局数）

```sql
-- 复核 4-A：取每 uid 最早一局 (MIN_BY 按 game_datetime)，判定首局 result/role/magnification
WITH first_game AS (
    SELECT
        app_id, dt AS reg_date, uid,
        MIN_BY(result_id,     game_datetime) AS first_result,
        MIN_BY(role,          game_datetime) AS first_role,
        MIN_BY(magnification, game_datetime) AS first_multi
    FROM tcy_temp.dws_ddz_firstday_game
    WHERE app_id = 1880053
      AND dt BETWEEN '2026-05-01' AND '2026-05-15'
      AND robot != 1
      AND play_mode IN (1, 2, 3)
    GROUP BY app_id, dt, uid
),
combo AS (
    SELECT
        app_id, reg_date, uid,
        CASE
            WHEN first_result = 2 AND first_role = 1 AND first_multi >= 24 THEN '1: 命中组合'
            ELSE '2: 未命中'
        END AS combo_flag
    FROM first_game
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count BETWEEN 1  AND 5  THEN 'A: 1-5局'
        WHEN g.silver_game_count BETWEEN 6  AND 10 THEN 'B: 6-10局'
        WHEN g.silver_game_count BETWEEN 11 AND 20 THEN 'C: 11-20局'
        WHEN g.silver_game_count BETWEEN 21 AND 50 THEN 'D: 21-50局'
        WHEN g.silver_game_count > 50              THEN 'E: 50+局'
    END AS game_count_bucket,
    c.combo_flag,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d1_game)  * 100.0 / NULLIF(COUNT(rf.d1_game),  0), 2) AS d1_rate,
    ROUND(SUM(rf.d7_game)  * 100.0 / NULLIF(COUNT(rf.d7_game),  0), 2) AS d7_rate,
    ROUND(SUM(rf.d30_game) * 100.0 / NULLIF(COUNT(rf.d30_game), 0), 2) AS d30_rate
FROM combo c
INNER JOIN tcy_temp.dws_app_firstday_game_stat g
    ON g.app_id = c.app_id AND g.reg_date = c.reg_date AND g.uid = c.uid
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.silver_game_count > 0
GROUP BY game_count_bucket, c.combo_flag
ORDER BY game_count_bucket, c.combo_flag;
```

### 6.5 高危组合"0局/1局 + 多次登录(≥3)"

```sql
-- 复核 4-B：first_day_login_cnt 在宽表；0局=无 silver_game_count(LEFT JOIN 后 NULL)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count IS NULL            THEN '0局'
        WHEN g.silver_game_count = 1                THEN '1局'
        WHEN g.silver_game_count BETWEEN 2  AND 5   THEN '2-5局'
        WHEN g.silver_game_count BETWEEN 6  AND 10  THEN '6-10局'
        ELSE                                            '11+局'
    END AS game_count_bucket,
    CASE
        WHEN COALESCE(g.first_day_login_cnt, 0) >= 3 THEN '1: 多次登录>=3'
        ELSE                                              '2: 登录<3次'
    END AS login_flag,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d1_game)  * 100.0 / NULLIF(COUNT(rf.d1_game),  0), 2) AS d1_rate,
    ROUND(SUM(rf.d7_game)  * 100.0 / NULLIF(COUNT(rf.d7_game),  0), 2) AS d7_rate,
    ROUND(SUM(rf.d30_game) * 100.0 / NULLIF(COUNT(rf.d30_game), 0), 2) AS d30_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND COALESCE(g.silver_game_count, 0) <= 1            -- 聚焦 0局/1局
GROUP BY game_count_bucket, login_flag
ORDER BY game_count_bucket, login_flag;
```

### 6.6 高危组合"胜率<30% + 对局数少(≤2)"

```sql
-- 复核 4-C：silver_win_rate<30 AND silver_game_count<=2，同段内对比
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    CASE
        WHEN g.silver_game_count = 1                THEN '1局'
        WHEN g.silver_game_count = 2                THEN '2局'
        WHEN g.silver_game_count BETWEEN 3 AND 5    THEN '3-5局'
    END AS game_count_bucket,
    CASE
        WHEN g.silver_win_rate < 30 AND g.silver_game_count <= 2 THEN '1: 命中组合(<30%且<=2局)'
        ELSE                                                          '2: 未命中'
    END AS combo_flag,
    COUNT(*) AS user_count,
    ROUND(SUM(rf.d1_game)  * 100.0 / NULLIF(COUNT(rf.d1_game),  0), 2) AS d1_rate,
    ROUND(SUM(rf.d7_game)  * 100.0 / NULLIF(COUNT(rf.d7_game),  0), 2) AS d7_rate,
    ROUND(SUM(rf.d30_game) * 100.0 / NULLIF(COUNT(rf.d30_game), 0), 2) AS d30_rate
FROM tcy_temp.dws_app_firstday_game_stat g
LEFT JOIN tcy_temp.dws_app_retention_flag rf
    ON rf.app_id = g.app_id AND rf.reg_date = g.reg_date AND rf.uid = g.uid
WHERE g.app_id = 1880053
  AND g.reg_date BETWEEN '2026-05-01' AND '2026-05-15'
  AND g.silver_game_count BETWEEN 1 AND 5
GROUP BY game_count_bucket, combo_flag
ORDER BY game_count_bucket, combo_flag;
```

---

> **创建时间**：2026-06-24
>
> **复核窗口**：2026-05-01 ~ 2026-05-15（n=20,476）
>
> **关联**：订正 [retention-analysis-framework.md](../plan/retention/retention-analysis-framework.md) §2.1 连败/破产两行 + §3 五个高危组合的优先级；与 [retention-global-report.md](retention-global-report.md) §3.3/§3.5 互印证。
