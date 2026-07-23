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

> **房间等级 / 玩法一律查 `tcy_temp.dq_game_room_config`（字段 `game_rule` + `room_level`），不要用底分(room_base)或事实表 `play_mode` 反推。** 经典(game_id=53) 的 room_level 为 7 档 ladder：练习房 / 新手房 / 初级房 / 中级房 / 高级房 / 大师房 / 宗师房。
>
> 1124/1125/1126 的 room_level 均为 **练习房**（game_rule 分别 经典/不洗牌/癞子）。练习房分两种——**rigged 配牌房**（1124-1126，`play_mode=0`，1真人+2机器人）、**低额练习房**（742，`play_mode=1`，正常 3 人 PvP）。详见 [data-gotchas.md](data-gotchas.md) §1。
>
> 线上另有真正的"新手房"档（room_level=B，经典对应 room 420）——与练习房是不同的 room_level。详见 [dq_game_room_config](../../starrocks/config/dq_game_room_config.md)。

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
- 时长：`AVG(timecost)`（ddz）/ `AVG(time_cost)`（srddz / crazyddz）。ddz 经典对局参考量级约 80–90 秒（随房间/玩法变化，以实际查询为准）。
- 新手保护占比（ddz）：`SUM(CASE WHEN shuffle_type=201 THEN 1 ELSE 0 END) * 100.0 / COUNT(*)`——DWS 已是独立列，无需解析 JSON。

## 五、执行查询

SELECT 走 `ops/py/sr_exec.py`（DDL 禁走脚本）：

```powershell
py -3 -u .\ops\py\sr_exec.py -f <query.sql>
```

临时 SQL 放 `ops/py/tmp/`（已 gitignore）；沉淀分析放 `docs/analysis/plan/`，结论放 `docs/analysis/result/`。

**sr_exec 执行坑**：

- `PERCENTILE` 经 sr_exec 会静默失败（返回空/错乱）——改用 `ROW_NUMBER()` 窗口或 `SUM/CASE` 累计手动算分位值。
- 列名 `role` 是保留字，须加反引号（`` `role` ``）。
- DWS 数据动态回填、早期 dt 会漂移，跑历史窗口前先 `COUNT(*)` 对齐天数；口径"去掉 192+"实际即 `≤96`。
- **INSERT 报 `OK` 但实际 0 行**：SR strict mode 下因数据质量（字段超长/类型不匹配）过滤脏行触发整批回滚时，CloudBeaver GraphQL 层 `statusMessage=Executed` **不可信**。诊断路径：① 在 CloudBeaver 网页端重跑同一句 INSERT 拿真实报错 → ② 报错含 `job_id`，查 `SELECT tracking_log FROM information_schema.load_tracking_logs WHERE job_id=YYY` 定位被过滤的行及原因 → ③ `SELECT COUNT(*)` 复核落盘行数。

## 六、倍数(magnification)口径与费率基准

- **理论倍数 vs 实际倍数**：做倍数分布/费率分析一律用理论 `magnification`（全倍率链）；`real_magnification` 是破产截断后的实现值，偏低、不可用。
- **角色差异**：同局 magnification 地主行 = 2M、农民行 = M（地主赔两边）。统计农民倍数用 `WHERE role=2`，别混入地主翻倍行。
- **≤48 部分贡献均值是费率甜点基准**：`Σ(magnification WHERE mag≤48) / 总局数`（非条件子集均值）。用 ≤24 会偏高、用全部均值会被尾部带飞；≤48 跨房稳定且覆盖大多数对局。详见 [房间费率方法论](../../room-design/classic/00-framework.md)。
- **`compute_all` 截断公式曾多乘 2**，致高估；正确判定用 `start_money < magnification × 底分`。
- **极端高倍非脏数据**：magnification 在不洗牌(play_mode=2)玩法下可达极高值；倍数统计须按 play_mode 分层或用中位数，勿当异常剔除。
