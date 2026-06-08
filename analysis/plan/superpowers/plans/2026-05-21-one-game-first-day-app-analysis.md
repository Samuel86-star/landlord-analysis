# APP One-Game First-Day Analysis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the executable SQL and report workflow for analyzing APP users who register and play exactly 1 completed real-player game on registration day.

**Architecture:** Keep the approved analysis design in `docs/one-game-first-day-app-analysis.md` as the product/spec layer. Add one focused SQL document under `docs/` that defines reusable CTEs and query blocks for daily distribution, profile lift, first-game experience, and risk-combination analysis. Optionally run the SQL later and write a separate report under `report/` from query outputs.

**Tech Stack:** StarRocks SQL, Markdown documentation, existing DWS tables in `tcy_temp`, project Markdown style guide.

---

## File Structure

| File | Action | Responsibility |
| ---- | ---- | ---- |
| `docs/one-game-first-day-app-sql.md` | Create | Store executable SQL blocks for the approved APP 1局 analysis, with exact date range `2026-02-10` to `2026-05-10` and no mixed 1-3局口径 |
| `docs/one-game-first-day-app-analysis.md` | Read-only | Approved analysis design and口径 reference |
| `report/one-game-first-day-app-report.md` | Create later after SQL execution | Store narrative conclusions once query results are available |

---

## Task 1: Create SQL Analysis Document

**Files:**

- Create: `docs/one-game-first-day-app-sql.md`
- Reference: `docs/one-game-first-day-app-analysis.md`
- Reference: `dws/dws_dq_app_daily_reg.md`
- Reference: `dws/dws_ddz_firstday_game.md`

- [ ] **Step 1: Create the SQL document header**

Add this document skeleton:

```markdown
# APP 端注册当天只玩 1 局用户分析 SQL

> 本文档承接 [APP 端注册当天只玩 1 局用户分析方案](one-game-first-day-app-analysis.md)，提供可执行 SQL。主分析口径为 `1局用户` vs `2局及以上用户`，`0局用户` 仅用于每日背景分布。
>
> **分析时间段**：2026-02-10 至 2026-05-10
> **应用口径**：`app_id = 1880053`
> **用户口径**：APP 端注册用户，排除首日登录日志缺失用户
> **对局口径**：注册当天真人对局，`robot != 1`

---

## 目录

1. [公共 CTE](#一公共-cte)
2. [每日首日局数分布](#二每日首日局数分布)
3. [画像维度差异](#三画像维度差异)
4. [首局体验差异](#四首局体验差异)
5. [高风险组合](#五高风险组合)
6. [结果解读建议](#六结果解读建议)
```

- [ ] **Step 2: Add the public CTE block**

Add this SQL under `## 一、公共 CTE`:

```sql
WITH app_reg_users AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        CASE
            WHEN r.reg_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
            WHEN r.reg_group_id IN (8, 88) THEN 'iOS'
            ELSE '其他'
        END AS platform,
        CASE
            WHEN r.reg_app_code = 'zgda' THEN 'Cocos-Lua'
            WHEN r.reg_app_code = 'zgdx' THEN 'Cocos-Creator'
            ELSE '其他'
        END AS client_lang,
        CASE
            WHEN HOUR(r.reg_datetime) BETWEEN 0 AND 5 THEN 'A: 凌晨'
            WHEN HOUR(r.reg_datetime) BETWEEN 6 AND 11 THEN 'B: 上午'
            WHEN HOUR(r.reg_datetime) BETWEEN 12 AND 17 THEN 'C: 下午'
            ELSE 'D: 晚间'
        END AS reg_time_bucket,
        CASE
            WHEN r.first_day_login_cnt = 1 THEN 'A: 1次'
            WHEN r.first_day_login_cnt BETWEEN 2 AND 5 THEN 'B: 2-5次'
            WHEN r.first_day_login_cnt > 5 THEN 'C: 5次以上'
            ELSE 'D: 无登录记录'
        END AS first_day_login_bucket
    FROM tcy_temp.dws_dq_app_daily_reg r
    WHERE r.app_id = 1880053
      AND r.reg_date BETWEEN '2026-02-10' AND '2026-05-10'
      AND r.is_login_log_missing = 0
      AND r.reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)
),
first_day_games AS (
    SELECT
        g.app_id,
        g.uid,
        g.dt,
        g.game_datetime,
        g.resultguid,
        g.timecost,
        g.room_id,
        g.play_mode,
        g.room_base,
        g.room_fee,
        g.room_currency_lower,
        g.room_currency_upper,
        g.role,
        g.result_id,
        g.start_money,
        g.end_money,
        g.diff_money_pre_tax,
        g.cut,
        g.magnification,
        g.magnification_stacked,
        g.real_magnification,
        g.grab_landlord_bet,
        g.complete_victory_bet,
        g.bomb_bet
    FROM tcy_temp.dws_ddz_firstday_game g
    WHERE g.app_id = 1880053
      AND g.dt BETWEEN '2026-02-10' AND '2026-05-10'
      AND g.robot != 1
),
user_game_summary AS (
    SELECT
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket,
        COUNT(DISTINCT g.resultguid) AS first_day_game_cnt,
        MIN(g.game_datetime) AS first_game_datetime,
        TIMESTAMPDIFF(MINUTE, r.reg_datetime, MIN(g.game_datetime)) AS minutes_to_first_game
    FROM app_reg_users r
    LEFT JOIN first_day_games g
        ON g.app_id = r.app_id
        AND g.uid = r.uid
        AND g.dt = r.reg_date
    GROUP BY
        r.app_id,
        r.uid,
        r.reg_date,
        r.reg_datetime,
        r.reg_channel_id,
        r.reg_group_id,
        r.reg_app_code,
        r.channel_category_name,
        r.channel_category_tag_id,
        r.first_day_login_cnt,
        r.platform,
        r.client_lang,
        r.reg_time_bucket,
        r.first_day_login_bucket
),
user_game_bucket AS (
    SELECT
        *,
        CASE
            WHEN first_day_game_cnt = 0 THEN '0局'
            WHEN first_day_game_cnt = 1 THEN '1局'
            ELSE '2局及以上'
        END AS game_cnt_group
    FROM user_game_summary
)
```

- [ ] **Step 3: Add daily distribution SQL**

Add a query that reuses the public CTE and returns `reg_date`, total registrations, 0局 count/rate, 1局 count/rate, 2局及以上 count/rate, and 1局在有对局用户中的占比. Use `COUNT(DISTINCT uid)` and `NULLIF` for division by zero.

- [ ] **Step 4: Add profile lift SQL**

Add one `UNION ALL` query for these dimensions:

- `platform`
- `client_lang`
- `channel_category_name`
- `reg_channel_id`
- `reg_time_bucket`
- `first_day_login_bucket`

For each dimension, output `dimension_name`, `dimension_value`, `one_game_user_cnt`, `multi_game_user_cnt`, `one_game_share`, `multi_game_share`, and `lift`. Exclude `0局` from the denominator.

- [ ] **Step 5: Add first-game experience SQL**

Add a query that selects only users with `game_cnt_group IN ('1局', '2局及以上')`, ranks each user's first game by `game_datetime`, and compares:

- first play mode
- first room
- first win rate
- average first-game timecost
- average net money change
- below room threshold rate
- average fee pressure
- high magnification rate
- escape rate

Use these derived fields exactly:

```sql
CASE WHEN fg.result_id = 1 THEN 1 ELSE 0 END AS is_win,
fg.diff_money_pre_tax - fg.room_fee AS net_money_change,
CASE WHEN fg.end_money < fg.room_currency_lower THEN 1 ELSE 0 END AS is_below_room_threshold,
CASE WHEN fg.start_money > 0 THEN fg.room_fee * 1.0 / fg.start_money ELSE NULL END AS fee_pressure,
CASE WHEN fg.magnification >= 24 THEN 1 ELSE 0 END AS is_high_magnification,
CASE WHEN fg.cut < 0 THEN 1 ELSE 0 END AS is_escape
```

- [ ] **Step 6: Add high-risk combination SQL**

Add a query that compares `1局` and `2局及以上` on these combinations:

- first loss and below room threshold
- first loss and high magnification
- first loss and landlord role
- high fee pressure and net loss
- one first-day login only

Use `AVG(CASE WHEN condition THEN 1 ELSE 0 END)` to output rates by `game_cnt_group`.

- [ ] **Step 7: Add result interpretation guidance**

Add a final section explaining that the first readout should prioritize:

- `1局` among played users by date.
- dimension values with `lift >= 1.2` and enough users.
- experience metrics where `1局` is materially worse than `2局及以上`.
- risk combinations that are both high-rate and high-lift.

## Task 2: Static Review The SQL Document

**Files:**

- Review: `docs/one-game-first-day-app-sql.md`
- Reference: `markdown-style-guide.md`

- [ ] **Step 1: Check Markdown style**

Run:

```bash
sed -n '1,260p' docs/one-game-first-day-app-sql.md
tail -20 docs/one-game-first-day-app-sql.md
```

Expected:

- Every fenced block has a language label.
- Tables use `| ---- |` separator format.
- Heading levels progress from `#` to `##` to `###`.
- File ends with exactly one newline.

- [ ] **Step 2: Check SQL口径**

Manually verify these conditions in `docs/one-game-first-day-app-sql.md`:

- APP users come from `tcy_temp.dws_dq_app_daily_reg`.
- Registration date range is exactly `2026-02-10` to `2026-05-10`.
- APP端过滤 includes `reg_group_id IN (6, 66, 33, 44, 77, 99, 8, 88)`.
- Game rows come from `tcy_temp.dws_ddz_firstday_game`.
- Game rows filter `robot != 1`.
- Main profile and experience comparisons exclude `0局`.
- No query uses the older `1-3局`口径.

- [ ] **Step 3: Search for placeholders**

Run:

```bash
rg "TBD|TODO|\\$\\{|start_date|end_date|1-3局" docs/one-game-first-day-app-sql.md
```

Expected:

- No matches.

## Task 3: Optional Execute SQL And Write Report

**Files:**

- Read: `docs/one-game-first-day-app-sql.md`
- Create: `data/one-game-first-day-app/`
- Create: `report/one-game-first-day-app-report.md`

- [ ] **Step 1: Execute the daily distribution query**

Run the daily distribution SQL in StarRocks and save the output as:

```text
data/one-game-first-day-app/01_daily_distribution.md
```

Expected columns:

- `reg_date`
- `reg_user_cnt`
- `zero_game_user_cnt`
- `one_game_user_cnt`
- `multi_game_user_cnt`
- `zero_game_rate`
- `one_game_rate`
- `multi_game_rate`
- `one_game_rate_among_played`

- [ ] **Step 2: Execute the profile lift query**

Run the profile lift SQL and save the output as:

```text
data/one-game-first-day-app/02_profile_lift.md
```

Expected columns:

- `dimension_name`
- `dimension_value`
- `one_game_user_cnt`
- `multi_game_user_cnt`
- `one_game_share`
- `multi_game_share`
- `lift`

- [ ] **Step 3: Execute the first-game experience query**

Run the first-game experience SQL and save the output as:

```text
data/one-game-first-day-app/03_first_game_experience.md
```

Expected columns include:

- `game_cnt_group`
- `play_mode_name`
- `room_id`
- `user_count`
- `first_win_rate`
- `avg_first_timecost`
- `avg_net_money_change`
- `below_room_threshold_rate`
- `avg_fee_pressure`
- `high_magnification_rate`
- `escape_rate`

- [ ] **Step 4: Execute the high-risk combination query**

Run the high-risk combination SQL and save the output as:

```text
data/one-game-first-day-app/04_risk_combinations.md
```

Expected columns include:

- `game_cnt_group`
- `user_count`
- `loss_below_threshold_rate`
- `loss_high_magnification_rate`
- `loss_landlord_rate`
- `high_fee_net_loss_rate`
- `single_login_rate`

- [ ] **Step 5: Write the narrative report**

Create `report/one-game-first-day-app-report.md` with this structure:

```markdown
# APP 端注册当天只玩 1 局用户分析报告

> 本报告基于 `docs/one-game-first-day-app-sql.md` 的查询结果，分析 APP 端注册当天只玩 1 局用户的共性特征。

---

## 一、结论摘要

## 二、首日局数分布

## 三、1局用户画像共性

## 四、1局用户首局体验共性

## 五、高风险组合

## 六、原因判断

## 七、产品与运营建议

## 八、附录
```

Use only values from the saved data files. Do not claim a pattern unless it is visible in the query output.
