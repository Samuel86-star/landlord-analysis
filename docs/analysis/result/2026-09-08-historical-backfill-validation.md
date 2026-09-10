# 历史回填基线与人工验收包

## 状态与范围

- 状态：**只读基线及全历史炸弹桶定位已完成；人工回填尚未执行**。
- 明细修复窗口：2026-06-25 至 2026-07-01；炸弹桶核查覆盖2026-03-01至2026-09-06的190日，需重算范围为2026-03-01至2026-09-01的185日。
- 线上结果：2026-09-08 从用户指定的 `~/.zshrc` 加载凭据，按实际网页的大写 MD5 摘要完成认证，经 `ops/py/sr_exec.py` 执行本文 SELECT。未执行任何回填、DDL 或跨库查询。
- 永久证据：本文 SQL 代码块。`ops/py/tmp/` 下同名文件仅供临时执行，目录被 Git 忽略。

## 明细分区质量

以下查询以七日日历为主表，因此没有 DWS 数据的日期仍输出一行。字段含义：

- `dws_rows`：game 53 全 app 明细行数；`distinct_uids`：去重用户数。
- `hand_cards_nonempty_rows`：手牌字段非空行数；`hand_cards_coverage_pct`：手牌非空覆盖率。
- `shuffle_times_null_rows` / `shuffle_times_minus1_rows`：落表值为 NULL / -1 的行数。
- `shuffle_json_missing_rows`：`extend_content` 中原始 JSON 路径缺失或整数提取失败的行数。
- `shuffle_json_missing_but_not_minus1_rows`：JSON 路径缺失但落表值并非 -1 的异常行数。
- `non_1880053_app_human_source_rows`：满足 allgame DDZ 分支真人、分组条件，但 app 不是 1880053 的源行数。修复前插入分支不筛 app，而删除只删 1880053；本轮已补参数化 app 过滤。该计数用于展示原风险，修复后不要求其他 app 的合法源记录消失。

```sql
-- file: 20260908_handoff_partition_quality.sql
WITH calendar AS (
    SELECT '2026-06-25' AS dt
    UNION ALL SELECT '2026-06-26'
    UNION ALL SELECT '2026-06-27'
    UNION ALL SELECT '2026-06-28'
    UNION ALL SELECT '2026-06-29'
    UNION ALL SELECT '2026-06-30'
    UNION ALL SELECT '2026-07-01'
),
daily_quality AS (
    SELECT
        game.dt,
        COUNT(*) AS dws_rows,
        COUNT(DISTINCT game.uid) AS distinct_uids,
        SUM(CASE WHEN game.hand_cards IS NOT NULL AND game.hand_cards != '' THEN 1 ELSE 0 END)
            AS hand_cards_nonempty_rows,
        SUM(CASE WHEN game.shuffle_times IS NULL THEN 1 ELSE 0 END) AS shuffle_times_null_rows,
        SUM(CASE WHEN game.shuffle_times = -1 THEN 1 ELSE 0 END) AS shuffle_times_minus1_rows,
        SUM(CASE
            WHEN get_json_int(game.extend_content, '$.card_power.shuffle_times') IS NULL THEN 1
            ELSE 0
        END) AS shuffle_json_missing_rows,
        SUM(CASE
            WHEN get_json_int(game.extend_content, '$.card_power.shuffle_times') IS NULL
             AND (game.shuffle_times IS NULL OR game.shuffle_times != -1)
            THEN 1 ELSE 0
        END) AS shuffle_json_missing_but_not_minus1_rows,
        SUM(CASE
            WHEN game.app_id != 1880053
             AND game.robot != 1
             AND game.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
            THEN 1 ELSE 0
        END) AS non_1880053_app_human_source_rows
    FROM tcy_temp.dws_ddz_daily_game game
    WHERE game.game_id = 53
      AND game.dt BETWEEN '2026-06-25' AND '2026-07-01'
    GROUP BY game.dt
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    calendar.dt,
    COALESCE(quality.dws_rows, 0) AS dws_rows,
    COALESCE(quality.distinct_uids, 0) AS distinct_uids,
    COALESCE(quality.hand_cards_nonempty_rows, 0) AS hand_cards_nonempty_rows,
    ROUND(
        COALESCE(quality.hand_cards_nonempty_rows, 0) * 100.0
        / NULLIF(quality.dws_rows, 0),
        2
    ) AS hand_cards_coverage_pct,
    COALESCE(quality.shuffle_times_null_rows, 0) AS shuffle_times_null_rows,
    COALESCE(quality.shuffle_times_minus1_rows, 0) AS shuffle_times_minus1_rows,
    COALESCE(quality.shuffle_json_missing_rows, 0) AS shuffle_json_missing_rows,
    COALESCE(quality.shuffle_json_missing_but_not_minus1_rows, 0)
        AS shuffle_json_missing_but_not_minus1_rows,
    COALESCE(quality.non_1880053_app_human_source_rows, 0)
        AS non_1880053_app_human_source_rows
FROM calendar
LEFT JOIN daily_quality quality ON quality.dt = calendar.dt
ORDER BY calendar.dt;
```

## 炸弹分桶对账

源数据限定 app 1880053、真人、指定 APP 分组和玩法 1/2/3。先按 `uid × dt × play_mode` 计算 `bomb_bet <= 1`、`DIV 2 = 1`、`DIV 2 = 2`、`DIV 2 >= 3` 四桶，再与目标同粒度逐键比较，最后只输出按日汇总，不暴露个人数据。

`invalid_bomb_rows` 识别 `bomb_bet` 为 NULL、小于 1、或大于 1 的奇数。`missing_target_keys`、`extra_target_keys`、`duplicate_target_keys` 和 `target_null_field_keys` 独立暴露目标缺失、额外、重复和 NULL；比较过程不把目标 NULL 全部 `COALESCE` 成 0。

```sql
-- file: 20260908_handoff_bomb_bucket_reconcile.sql
WITH calendar AS (
    SELECT '2026-06-25' AS dt
    UNION ALL SELECT '2026-06-26'
    UNION ALL SELECT '2026-06-27'
    UNION ALL SELECT '2026-06-28'
    UNION ALL SELECT '2026-06-29'
    UNION ALL SELECT '2026-06-30'
    UNION ALL SELECT '2026-07-01'
),
source_by_uid AS (
    SELECT
        game.uid,
        game.dt,
        game.play_mode,
        COUNT(*) AS expected_game_count,
        SUM(CASE WHEN game.bomb_bet <= 1 THEN 1 ELSE 0 END) AS expected_bomb_0_games,
        SUM(CASE WHEN game.bomb_bet DIV 2 = 1 THEN 1 ELSE 0 END) AS expected_bomb_1_games,
        SUM(CASE WHEN game.bomb_bet DIV 2 = 2 THEN 1 ELSE 0 END) AS expected_bomb_2_games,
        SUM(CASE WHEN game.bomb_bet DIV 2 >= 3 THEN 1 ELSE 0 END) AS expected_bomb_3plus_games,
        SUM(CASE
            WHEN game.bomb_bet IS NULL
              OR game.bomb_bet < 1
              OR (game.bomb_bet > 1 AND game.bomb_bet % 2 = 1)
            THEN 1 ELSE 0
        END) AS invalid_bomb_rows
    FROM tcy_temp.dws_ddz_daily_game game
    WHERE game.game_id = 53
      AND game.app_id = 1880053
      AND game.robot != 1
      AND game.group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND game.play_mode IN (1, 2, 3)
      AND game.dt BETWEEN '2026-06-25' AND '2026-07-01'
    GROUP BY game.uid, game.dt, game.play_mode
),
target_by_uid AS (
    SELECT
        target.uid,
        target.dt,
        target.play_mode,
        COUNT(*) AS target_row_count,
        SUM(target.game_count) AS target_game_count,
        SUM(target.bomb_0_games) AS target_bomb_0_games,
        SUM(target.bomb_1_games) AS target_bomb_1_games,
        SUM(target.bomb_2_games) AS target_bomb_2_games,
        SUM(target.bomb_3plus_games) AS target_bomb_3plus_games,
        MAX(CASE
            WHEN target.game_count IS NULL
              OR target.bomb_0_games IS NULL
              OR target.bomb_1_games IS NULL
              OR target.bomb_2_games IS NULL
              OR target.bomb_3plus_games IS NULL
            THEN 1 ELSE 0
        END) AS has_null_field
    FROM tcy_temp.dws_app_allgame_stat target
    WHERE target.app_id = 1880053
      AND target.play_mode IN (1, 2, 3)
      AND target.dt BETWEEN '2026-06-25' AND '2026-07-01'
    GROUP BY target.uid, target.dt, target.play_mode
),
all_keys AS (
    SELECT source.uid, source.dt, source.play_mode
    FROM source_by_uid source
    UNION
    SELECT target.uid, target.dt, target.play_mode
    FROM target_by_uid target
),
key_comparison AS (
    SELECT
        key_set.dt,
        CASE WHEN source.uid IS NULL THEN 1 ELSE 0 END AS is_extra_target,
        CASE WHEN target.uid IS NULL THEN 1 ELSE 0 END AS is_missing_target,
        CASE WHEN target.target_row_count > 1 THEN 1 ELSE 0 END AS is_duplicate_target,
        CASE WHEN target.has_null_field = 1 THEN 1 ELSE 0 END AS has_target_null,
        COALESCE(source.invalid_bomb_rows, 0) AS invalid_bomb_rows,
        CASE
            WHEN source.uid IS NULL OR target.uid IS NULL THEN 1
            WHEN target.target_row_count != 1 OR target.has_null_field = 1 THEN 1
            WHEN source.expected_game_count != target.target_game_count THEN 1
            WHEN source.expected_bomb_0_games != target.target_bomb_0_games THEN 1
            WHEN source.expected_bomb_1_games != target.target_bomb_1_games THEN 1
            WHEN source.expected_bomb_2_games != target.target_bomb_2_games THEN 1
            WHEN source.expected_bomb_3plus_games != target.target_bomb_3plus_games THEN 1
            ELSE 0
        END AS is_mismatch
    FROM all_keys key_set
    LEFT JOIN source_by_uid source
      ON source.uid = key_set.uid
     AND source.dt = key_set.dt
     AND source.play_mode = key_set.play_mode
    LEFT JOIN target_by_uid target
      ON target.uid = key_set.uid
     AND target.dt = key_set.dt
     AND target.play_mode = key_set.play_mode
),
daily_reconcile AS (
    SELECT
        comparison.dt,
        COUNT(*) AS compared_uid_mode_keys,
        SUM(comparison.is_mismatch) AS mismatch_uid_mode_keys,
        SUM(comparison.is_missing_target) AS missing_target_keys,
        SUM(comparison.is_extra_target) AS extra_target_keys,
        SUM(comparison.is_duplicate_target) AS duplicate_target_keys,
        SUM(comparison.has_target_null) AS target_null_field_keys,
        SUM(comparison.invalid_bomb_rows) AS invalid_bomb_rows
    FROM key_comparison comparison
    GROUP BY comparison.dt
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    calendar.dt,
    COALESCE(reconcile.compared_uid_mode_keys, 0) AS compared_uid_mode_keys,
    COALESCE(reconcile.mismatch_uid_mode_keys, 0) AS mismatch_uid_mode_keys,
    COALESCE(reconcile.missing_target_keys, 0) AS missing_target_keys,
    COALESCE(reconcile.extra_target_keys, 0) AS extra_target_keys,
    COALESCE(reconcile.duplicate_target_keys, 0) AS duplicate_target_keys,
    COALESCE(reconcile.target_null_field_keys, 0) AS target_null_field_keys,
    COALESCE(reconcile.invalid_bomb_rows, 0) AS invalid_bomb_rows
FROM calendar
LEFT JOIN daily_reconcile reconcile ON reconcile.dt = calendar.dt
ORDER BY calendar.dt;
```

## 分阶段判定

### 基线诊断

先运行两段查询保存旧表基线。旧默认值异常、目标旧桶不匹配、目标缺失或重复是**触发重算的证据**，不阻止用于修复这些问题的回填。

### 上游写入前门槛

运行 `batch_insert_ddz_daily_game.py` 前必须满足：

- 独立核验 `ddz_daily_game_raw` 七个分区均有源数据。本次已单独使用带分区过滤的 raw 计数查询确认七日逐日非零且与 DWS 行数相等；写入前应复核其未变化。任一天 raw 为 0 时停止。
- 已处理 `non_1880053_app_human_source_rows > 0` 对下游的跨 app 写入风险。原 allgame DDZ 插入分支不筛 app，而删除只删 1880053；本轮已修复，人工执行端必须使用包含该修复的脚本。
- 已确认源字段不存在无法解释的异常；异常不能依赖默认值或分桶表达式静默归类。

### 上游回填后门槛

上游回填后重跑分区质量 SQL。进入下游回填前必须满足：

- 七天 `dws_rows > 0`。
- `shuffle_times_null_rows = 0`。
- `shuffle_json_missing_but_not_minus1_rows = 0`；JSON 缺失时落表必须使用约定的 -1。
- `non_1880053_app_human_source_rows = 0`，或下游插入已加入并验证 app 1880053 限定。

`hand_cards_coverage_pct` 与 `shuffle_times_minus1_rows` 用于观察历史完整性；JSON 确实缺字段且正确落为 -1 时不单独判失败。

### 下游回填后验收

下游回填后重跑炸弹分桶对账。`mismatch_uid_mode_keys`、`missing_target_keys`、`extra_target_keys`、`duplicate_target_keys`、`target_null_field_keys` 必须全部为 0。`invalid_bomb_rows > 0` 表示上游异常仍未解决，也不得判定通过。

历史基线已核查190日；人工重算185个受影响日期后仍需按完整历史查询复验，不能把回填前的定位结果当成回填后通过。

## 人工执行（先验收七天，再按全历史清单推进）

仅用户在认证成功且公司内网连接正常的终端逐步执行。每一步必须检查输出，只有对应前置通过才能运行下一步；以下命令不能复制成一键脚本连续执行。智能体不执行回填脚本；本轮算法未修改，不运行Java/C++全量验证。

1. 保存回填前基线：

```powershell
py -3 -u ops/py/sr_exec.py -f ops/py/tmp/20260908_handoff_partition_quality.sql
py -3 -u ops/py/sr_exec.py -f ops/py/tmp/20260908_handoff_bomb_bucket_reconcile.sql
```

2. 独立确认七天 raw 均非零、源异常已解释，并确认其他 app 写入风险已消除。先预览上游 SQL：

```powershell
py -3 -u ops/py/batch_insert_ddz_daily_game.py --start 20260625 --end 20260701 --dry-run
```

3. 预览正确后，由用户执行上游写入：

```powershell
py -3 -u ops/py/batch_insert_ddz_daily_game.py --start 20260625 --end 20260701
```

4. 重跑上游质量查询；只有“上游回填后门槛”全部通过，才预览下游 SQL：

```powershell
py -3 -u ops/py/sr_exec.py -f ops/py/tmp/20260908_handoff_partition_quality.sql
py -3 -u ops/py/batch_insert_allgame_stat.py --start 20260625 --end 20260701 --dry-run
```

5. 预览正确后，由用户执行下游写入：

```powershell
py -3 -u ops/py/batch_insert_allgame_stat.py --start 20260625 --end 20260701
```

6. 重跑下游对账；只有“下游回填后验收”全部通过，才判定这七天通过：

```powershell
py -3 -u ops/py/sr_exec.py -f ops/py/tmp/20260908_handoff_bomb_bucket_reconcile.sql
```

## 2026-09-08 实际基线结果

| 日期 | raw = DWS 行数 | 非空手牌覆盖率 | 源重洗次数提取缺失但未落 -1 | 炸弹桶差异键 |
| ---- | ---- | ---- | ---- | ---- |
| 2026-06-25 | 1,006,713 | 92.52% | 66,567 | 24,860 |
| 2026-06-26 | 987,900 | 100% | 67,494 | 24,809 |
| 2026-06-27 | 955,191 | 100% | 61,110 | 24,732 |
| 2026-06-28 | 959,712 | 100% | 63,612 | 25,276 |
| 2026-06-29 | 1,010,631 | 100% | 68,190 | 25,218 |
| 2026-06-30 | 1,014,615 | 100% | 68,628 | 25,481 |
| 2026-07-01 | 1,016,553 | 100% | 67,131 | 25,251 |

七天合计6,951,315条DWS记录，raw逐日一致；源重洗次数提取缺失却未落为 -1 的记录共462,732条。`shuffle_times_null_rows=0`，七天其他app真人源为0。非空手牌覆盖率不是合法手牌覆盖率，非法手牌行数仍需回填后运行现有解析流程统计；不能据此横比新旧持有炸率。

全历史190日共比较8,496,949个“日期×用户×玩法”键，其中4,708,367个键不一致，集中在2026-03-01至2026-09-01的连续185日。9月2日至6日全部一致。190日目标缺失、额外、重复、NULL字段及适用玩法异常bomb计数全部为0。差异键数不是去重用户数。

逐日证据：[炸弹桶历史核验CSV](2026-09-08-bomb-bucket-history.csv)。本地既有 `ops/py/logs/backfill_2026-09-02.log` 至 `backfill_2026-09-06.log` 的第146行起记录了对应脚本运行；日志不含运行commit，当前一致性结论以本次真实对账为准。

### raw 与历史范围补充查询

raw用于证明人工回填的上游就绪，DWS不能替代该证明；仅回退到同库、同七日分区的计数。

```sql
-- file: 20260908_raw_partition_counts.sql
SELECT dt, COUNT(*) AS raw_rows
FROM tcy_temp.ddz_daily_game_raw
WHERE game_id = 53 AND dt BETWEEN '2026-06-25' AND '2026-07-01'
GROUP BY dt
ORDER BY dt;
```

```sql
-- file: 20260908_history_bounds.sql
SELECT 'dws_app_allgame_stat' AS source_table, MIN(dt) AS first_dt, MAX(dt) AS last_dt, COUNT(DISTINCT dt) AS date_count
FROM tcy_temp.dws_app_allgame_stat
WHERE app_id = 1880053 AND dt BETWEEN '2026-01-01' AND '2026-09-07'
UNION ALL
SELECT 'dws_ddz_daily_game', MIN(dt), MAX(dt), COUNT(DISTINCT dt)
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id = 53 AND dt BETWEEN '2026-01-01' AND '2026-09-07';
```

范围查询实际返回两表均为2026-03-01至09-06、190日。历史炸弹桶查询沿用本文七日对账SQL，仅替换事实/目标WHERE范围为该190日，并将calendar改为：

```sql
SELECT DISTINCT dt FROM tcy_temp.dws_app_allgame_stat
WHERE app_id = 1880053 AND dt BETWEEN '2026-03-01' AND '2026-09-06';
```

完整执行副本：`ops/py/tmp/20260908_handoff_bomb_history.sql`。本次返回190行，未触及客户端200行默认上限；以后若扩大到超过200个日期，应按月份分别运行，不能把截断结果当全历史。SQL2首次使用保留字别名 `keys` 时客户端返回 `[]`，未接受为成功；改为 `key_set` 后七日/190日均返回预期列和行数。客户端退出码0不足以单独证明验收通过。

### 全历史重算前的跨app修复

```sql
-- file: 20260908_history_app_scope.sql
SELECT COUNT(*) AS app_human_rows,
    COUNT(DISTINCT dt) AS covered_dates,
    SUM(CASE WHEN app_id != 1880053 OR app_id IS NULL THEN 1 ELSE 0 END) AS other_app_rows
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id = 53 AND dt BETWEEN '2026-03-01' AND '2026-09-01'
  AND robot != 1 AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99);
```

结果：109,115,085条APP真人源记录、185个日期，其中81条属于其他app。七日样本的0值不能外推到全历史。原脚本 DDZ 插入分支缺app过滤，DELETE/CHECK却按指定app，本轮已在共同源CTE增加 `AND app_id = {app_id}`，与DELETE/CHECK范围一致。`ops/py/test_batch_insert_allgame_stat.py` 使用真实CTE和同uid跨app样例复现红测，修复后通过；人工执行端必须使用此修复版本。此修复不改已有落库数据。

### 全历史人工操作与下游依赖

通过七日回填验收、跨app修复回归，并确认 `dws_crazyddz_daily_game` 对应日期就绪后，数据负责人按已核实范围预览：

```powershell
py -3 -u ops/py/batch_insert_allgame_stat.py --start 20260301 --end 20260901 --app-id 1880053 --dry-run
```

预览与依赖确认通过后，由数据负责人执行同一命令去掉 `--dry-run` 的版本，并重新运行190日对账。185日可分月推进，每批失败即停止；不得用整个 daily_backfill 调度器代替这次窄范围重算。

[Local] `ops/py/batch_insert_daily_allgame_stat.py:22` 将四桶之和写为 `bomb_count`；[Local] `ops/py/batch_insert_firstday_game_stat.py:52` 将其传为 `allgame_bomb_count`。因此 allgame 重算后，应先核对这两级同日期、同用户的值，存在变化时依次由数据负责人运行 daily_allgame_stat、firstday_game_stat 的单表脚本。后者还依赖注册、银子、积分统计分区，不能无条件重跑。`bomb_count` 是四桶覆盖的局数口径，不是持有炸弹数。无需因此重跑留存flag。

## 验证与剩余

- 实际查询均通过现有 `sr_exec.py`，只提交 `tcy_temp` SELECT；没有运行 `batch_insert_*`（含dry-run）。
- 七日与全历史基线已完成；人工写入和回填后验收未执行，原分析报告未覆盖。
- SQL离线边界检查：`python3 -m unittest discover -s ops/py -p 'test_handoff*.py' -v`。本轮以 Codex bundled Python 3.12 复用项目 requests，执行 `unittest` 的 `test_*.py` 全部42项，均通过，无跳过；`git diff --check`通过。

### 疯狂玩法依赖核验

allgame 的单日重算会同时重建玩法7，因此检查其上游完整性。2026-03-01至09-01实际返回185日，源/目标均852,774局，逐日差异数为0；仅说明计数一致，执行前源数据仍需保持就绪。

```sql
-- file: 20260908_history_crazy_source.sql
WITH daily_counts AS (
    SELECT dt, COUNT(*) AS source_games, 0 AS target_games
    FROM tcy_temp.dws_crazyddz_daily_game
    WHERE game_id = 521 AND app_id = 1880053
      AND dt BETWEEN '2026-03-01' AND '2026-09-01'
      AND robot != 1 AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    GROUP BY dt
    UNION ALL
    SELECT dt, 0, SUM(game_count)
    FROM tcy_temp.dws_app_allgame_stat
    WHERE app_id = 1880053 AND play_mode = 7
      AND dt BETWEEN '2026-03-01' AND '2026-09-01'
    GROUP BY dt
),
reconciled AS (
    SELECT dt, SUM(source_games) AS source_games, SUM(target_games) AS target_games
    FROM daily_counts
    GROUP BY dt
)
SELECT COUNT(*) AS observed_days,
    SUM(source_games) AS source_games,
    SUM(target_games) AS target_games,
    SUM(CASE WHEN source_games != target_games THEN 1 ELSE 0 END) AS mismatch_days
FROM reconciled;
```
