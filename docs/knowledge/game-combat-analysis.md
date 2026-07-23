# 对局 / 房间 / 战绩分析

## 零、取数铁律：优先 DWS，raw 仅作 fallback

> **分析一律查 `dws_*_daily_game`，不要直接查 `*_daily_game_raw`。**

DWS 是 raw 的矫正层：货币字段统一（`game_outcome_money` / `start_money` / `end_money`）、玩法分类（`play_mode`）、JSON 已解析为独立列（`shuffle_type` / `hand_cards` / `card_power` 等）、双行已合并（srddz）、跨天已归一。**raw 会有脏数据 / 缺损 / 双行 / 未解析 JSON，统计会出错。** 仅当 DWS 缺字段、或排障需对账时才回 raw。

**真人过滤口径（各表不同，别用错）**：

| 表 | 真人过滤 | 备注 |
| -- | -------- | ---- |
| `dws_ddz_daily_game` / `dws_crazyddz_daily_game` | `robot != 1` | robot 透传自 raw |
| `dws_srddz_daily_game` | `is_robot = 0` | 字典叠加判据，比 robot 可靠；**别用 robot!=1** |

## 一、对局表矩阵（分析用 DWS）

| game_id | DWS 分析表（优先） | raw 源表（fallback） | 时长字段 | 银子净盈亏（最准） |
| ------- | ------------------ | -------------------- | -------- | ----------------- |
| 53 | `tcy_temp.dws_ddz_daily_game` | `ddz_daily_game_raw` | `timecost` | `end_money - start_money` |
| 521 | `tcy_temp.dws_crazyddz_daily_game` | `crazyddz_daily_game_raw` | `time_cost` | `end_money - start_money` |
| 105 | `tcy_temp.dws_srddz_daily_game` | `srddz_daily_game_raw` | `time_cost` | `end_money - start_money`（仅 room 927/928/930） |

通用字段（三表同）：`dt`、`uid`、`resultguid`、`room_id`、`app_code`、`group_id`、`channel_id`、`result_id`(1胜2败)、`role`、`room_base`、`room_fee`、`start_money`、`end_money`、`game_outcome_money`、`play_mode`。

- 银子语义：`game_outcome_money` = 不含服务费的输赢（正=赢）；账户净变动 = `end_money - start_money`（含服务费与一切补贴，**最准**）= `game_outcome_money - room_fee`。
- ddz DWS 独有已解析列：`shuffle_type`（201=新手保护机器人）、`hand_cards`、`card_power` 等；srddz 独有 `is_robot`。

## 二、房间定位法（用 DWS）

给一批 room_id，直接探查归属 app_code / 平台 / 日期：

```sql
SELECT room_id, app_code,
    CASE WHEN group_id IN (8,88) THEN 'iOS'
         WHEN group_id IN (6,66,33,44,77,99) THEN 'Android'
         ELSE CONCAT('other:',group_id) END AS platform,
    COUNT(DISTINCT uid) AS users, COUNT(DISTINCT resultguid) AS games,
    MIN(dt) AS min_dt, MAX(dt) AS max_dt
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id=53 AND room_id IN (<ids>) AND dt BETWEEN '<start>' AND '<end>'
GROUP BY 1,2,3 ORDER BY 1,2;
```

> **房间等级 / 玩法一律查 `tcy_temp.dq_game_room_config`（字段 `game_rule` + `room_level`），不要用底分(room_base)或事实表 `play_mode` 反推。** 经典(game_id=53) 的 room_level 为 7 档 ladder：练习房 / 新手房 / 初级房 / 中级房 / 高级房 / 大师房 / 宗师房（字母前缀 A-G 是各玩法 ladder 内序号，语义看中文名）。
>
> 已知：**1124 / 1125 / 1126 ∈ game_id 53**，room_level 均为 **A:练习房**（**不是"新手房"**），`game_rule` 分别为 经典 / 不洗牌 / 癞子（三玩法各一个练习房）。它们在事实表里 `play_mode=0` 只是练习房的日志标记，并非"其他玩法"。经典玩法的**主力练习房是 742**（play_mode=1、底分 30、约 46 万局/月），1124-1126 合计仅 ~1 万局/月。
>
> **练习房 = "1 真人 + 2 机器人" 纯陪练房**：过滤 `robot!=1` 后 `COUNT(*)`（真人对局人次）恒等于 `COUNT(DISTINCT resultguid)`（局数），即每局恰好 1 真人，对手全是机器人。故练习房真人胜率 85%+、账户净盈亏为正（系统送银子）是**设计预期，非异常**——做胜率/牌力分析时练习房须单独标注，勿与正常房混算。线上另有真正的"新手房"档（room_level=B，经典对应 room 420）。详见 [练习房规模分析报告](../../analysis/result/beginner-room-zgde-zgdx-scale-report.md)（6/30~7/14）。

## 三、Recipe：房间对局 × 对应渠道 DAU 渗透率（DWS，可直接套用）

改三处：room 列表、app_code 集合、日期窗口。换 srddz / crazyddz 时改表名 + `time_cost` + 真人过滤（srddz 用 `is_robot=0`）。

```sql
WITH dau AS (   -- 对应渠道日活，按 app_code+平台分桶
    SELECT login_date AS dt,
        CASE WHEN most_freq_app_code='zgde' THEN 'zgde'
             WHEN most_freq_app_code='zgdx' AND most_freq_group_id IN (8,88) THEN 'zgdx_iOS'
             WHEN most_freq_app_code='zgdx' AND most_freq_group_id IN (6,66,33,44,77,99) THEN 'zgdx_Android'
        END AS channel,
        COUNT(DISTINCT uid) AS channel_dau
    FROM tcy_temp.dws_dq_daily_login
    WHERE app_id=1880053 AND login_date BETWEEN '<s>' AND '<e>'
    GROUP BY 1,2
),
room_players AS (   -- 房间真人对局用户，同口径分桶
    SELECT dt,
        CASE WHEN app_code='zgde' THEN 'zgde'
             WHEN app_code='zgdx' AND group_id IN (8,88) THEN 'zgdx_iOS'
             WHEN app_code='zgdx' AND group_id IN (6,66,33,44,77,99) THEN 'zgdx_Android'
        END AS channel,
        COUNT(DISTINCT uid) AS room_users, COUNT(DISTINCT resultguid) AS room_games
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id=53
      AND room_id IN (1124,1125,1126) AND robot!=1
      AND dt BETWEEN '<s>' AND '<e>'
    GROUP BY 1,2
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    d.dt, d.channel, d.channel_dau,
    IFNULL(p.room_users,0) AS room_users,
    IFNULL(p.room_games,0) AS room_games,
    ROUND(IFNULL(p.room_users,0)*100.0/NULLIF(d.channel_dau,0),2) AS penetration_pct
FROM dau d
LEFT JOIN room_players p ON p.dt=d.dt AND p.channel=d.channel
WHERE d.channel IS NOT NULL
ORDER BY d.dt, d.channel;
```

## 四、常用对局指标（DWS 字段）

- 局数：`COUNT(DISTINCT resultguid)`；人均局：`COUNT(*) / COUNT(DISTINCT uid)`。
- 胜率：`SUM(CASE WHEN result_id=1 THEN 1 ELSE 0 END) * 100.0 / COUNT(*)`（⚠️ 新手房异常高，见 [data-gotchas.md](data-gotchas.md) §2）。
- 银子净盈亏：`SUM(end_money - start_money)`；单局输赢（不含费）：`AVG(game_outcome_money)`。
- 时长：`AVG(timecost)`（ddz）/ `AVG(time_cost)`（srddz / crazyddz）。
- 新手保护占比（ddz）：`SUM(CASE WHEN shuffle_type=201 THEN 1 ELSE 0 END) * 100.0 / COUNT(*)`——DWS 已是独立列，无需解析 JSON。

## 五、执行查询

SELECT 走 `py/sr_exec.py`（DDL 禁走脚本）：

```powershell
py -3 -u .\py\sr_exec.py -f <query.sql>
```

临时 SQL 放 `py/tmp/`（已 gitignore）；沉淀分析放 `analysis/plan/`，结论放 `analysis/result/`。
