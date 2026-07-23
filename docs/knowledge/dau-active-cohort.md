# DAU / 活跃 / 人群口径

## 一、日活（DAU）来源

表 `tcy_temp.dws_dq_daily_login`，粒度 **uid × login_date**（一人一天一行）。

- 基础 DAU：`COUNT(DISTINCT uid)` GROUP BY login_date，必带 `app_id = 1880053` + 分区裁剪。
- **分渠道/平台 DAU**：按 `most_freq_app_code` + `most_freq_group_id` 分桶（当日最频繁值）。
  - `most_freq_app_code`：当日最频繁 app_code
  - `first_app_code` / `last_app_code`：首次/末次登录 app_code（稳定性、版本切换分析用）

## 二、app_code × 平台 DAU 模板

```sql
SELECT
    login_date AS dt,
    most_freq_app_code AS app_code,
    CASE WHEN most_freq_group_id IN (8, 88) THEN 'iOS'
         WHEN most_freq_group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
         ELSE CONCAT('other:', most_freq_group_id) END AS platform,
    COUNT(DISTINCT uid) AS dau
FROM tcy_temp.dws_dq_daily_login
WHERE app_id = 1880053
  AND login_date BETWEEN '<start>' AND '<end>'
GROUP BY 1, 2, 3;
```

> 上游是分钟级登录日志 `tcy_dwd.dwd_tcy_userlogin_si`（查它必须带 dt 分区 + app_id）。

## 三、注册人群表

`tcy_temp.dws_dq_app_daily_reg`：注册宽表，粒度 uid × reg_date。常用字段 `reg_app_code`、`reg_group_id`、`first_day_login_cnt`。

- 留存 / cohort 分析以本表为分母基础人群，模板见 [SQL_STYLE.md](../../SQL_STYLE.md) 与 [lessons/retention-sql-pattern.md](../../lessons/retention-sql-pattern.md)。

## 四、游戏活跃

- `tcy_temp.dws_app_game_active`（任意玩法对局活跃）：**无 `is_game_active` 字段，行存在即活跃**，用 `ga.uid IS NOT NULL` 判定。
- 游戏行为指标（局数 / 逃跑 / 时长）用 `dws_app_silvergame_stat`（银子玩法）。

## 五、渗透率 = 某子集用户 / 对应人群 DAU

把"子集活跃"（如某房间对局用户，按 dt + app_code + platform）与"人群 DAU"（按 login_date + most_freq_*）在 **（日期, 渠道桶）** 上 LEFT JOIN，分子 ÷ 分母。完整可套用 recipe 见 [game-combat-analysis.md](game-combat-analysis.md) §三。
