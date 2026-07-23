# 首次经典初级场玩家前 3 局手牌分析框架

> **分析对象**：2026-06-18 ~ 2026-06-24 期间，首次经典斗地主对局发生在经典玩法初级房 `4484` 或 `12074` 的玩家。
>
> **分析目标**：描述这批玩家前 3 局的手牌、配牌、牌力与胜负表现，形成可复现的事实清单。
>
> **分析性质**：描述性分析，只呈现数据观察，不做因果推断。
>
> **创建日期**：2026-06-25

---

## 一、分析边界与核心口径

### 1.1 分析目标

本框架用于回答：

- 首次经典斗地主对局发生在初级房 `4484` / `12074` 的玩家，前 3 局手牌体验是什么样？
- 前 3 局的牌力分布、配牌机制触发、实际手牌结构、胜负结果之间是否存在可观察差异？
- 这些事实是否与既有保护期观察形成一致的局序画像？

本框架不回答：

- 玩家为什么流失或留下。
- 修改配牌、机器人或房间配置能提升多少留存。
- 牌力、配牌、胜负与留存之间的因果关系。

### 1.2 时间窗口

| 口径项 | 取值 |
| ------ | ---- |
| 注册日期窗口 | `2026-06-18` ~ `2026-06-24` |
| 对局分区窗口 | `2026-06-18` ~ `2026-06-24` |
| 是否追踪到 06-25 | 否 |
| 窗口性质 | 固定 7 日快照 |

若玩家第 2 局或第 3 局发生在 `2026-06-25`，本报告不向后追踪，视为窗口内未观察到对应局序。

### 1.3 牌力数据可信前提

`card_power` 算法已于 2026-06-15 修复。本分析窗口从 2026-06-18 开始，位于修复之后，因此 `card_power` 与 `card_power_final` 可用于描述性统计。

### 1.4 房间口径

`4484` 与 `12074` 均为经典玩法初级房，且配置一致。因此：

- Cohort 构建阶段保留两房间人数分布，用于说明样本来源。
- 后续手牌、牌力、配牌、胜负等主体分析不拆分 `4484` vs `12074`。
- 报告表达使用「经典初级房」作为合并标签。

---

## 二、Cohort 构建

### 2.1 入选规则

玩家进入分析 cohort 需同时满足以下条件：

```text
玩家 P ∈ cohort
  ⟺ P 在 dws_ddz_daily_game 中
     按 game_datetime 最早的那条真人对局记录，room_id ∈ {4484, 12074}
  ∧ 该首局对应注册日期 reg_date ∈ [2026-06-18, 2026-06-24]
  ∧ 该首局发生日期 = reg_date
  ∧ robot != 1
```

解释：

- 「首次」指玩家在经典斗地主明细表 `dws_ddz_daily_game` 中的首次真人对局。
- 不使用 `dws_crazyddz_daily_game` 判定首局；该表仅存 510K 玩法，不参与本次经典玩法初级房分析。
- `reg_date = DATE(first_game_time)` 用于保证该玩家首局是注册首日对局，与首日对局分析口径一致。

### 2.2 Cohort 查询骨架

```sql
WITH reg_base AS (
    SELECT
        reg.uid,
        reg.reg_date,
        reg.app_id,
        reg.channel_id,
        reg.channel_category
    FROM tcy_temp.dws_dq_app_daily_reg reg
    WHERE reg.app_id = 1880053
      AND reg.reg_date BETWEEN '2026-06-18' AND '2026-06-24'
),
classic_first AS (
    SELECT
        game.uid,
        MIN_BY(game.room_id, game.game_datetime) AS first_room_id,
        MIN_BY(game.resultguid, game.game_datetime) AS first_resultguid,
        MIN(game.game_datetime) AS first_game_time
    FROM tcy_temp.dws_ddz_daily_game game
    INNER JOIN reg_base reg ON reg.uid = game.uid
    WHERE game.dt BETWEEN '2026-06-18' AND '2026-06-24'
      AND game.robot != 1
      AND game.play_mode BETWEEN 1 AND 6
    GROUP BY game.uid
),
cohort AS (
    SELECT
        reg.uid,
        reg.reg_date,
        reg.channel_id,
        reg.channel_category,
        first.first_room_id,
        first.first_resultguid,
        first.first_game_time
    FROM classic_first first
    INNER JOIN reg_base reg ON reg.uid = first.uid
    WHERE first.first_room_id IN (4484, 12074)
      AND DATE(first.first_game_time) = reg.reg_date
)
SELECT
    first_room_id,
    COUNT(DISTINCT uid) AS user_count
FROM cohort
GROUP BY first_room_id
ORDER BY first_room_id;
```

### 2.3 Cohort 基线输出

| 输出表 | 目的 | 核心字段 |
| ------ | ---- | -------- |
| 首局房间分布 | 说明 cohort 来源，但不做后续拆分 | `first_room_id`, `user_count`, `user_pct` |
| 首局日期分布 | 检查 7 日窗口内样本是否均衡 | `reg_date`, `user_count` |
| 渠道分布 | 判断是否被少数渠道主导 | `channel_category`, `user_count`, `user_pct` |
| 前 3 局可达性 | 明确第 1 / 2 / 3 局各自分母 | `max_observed_seq`, `user_count`, `user_pct` |

---

## 三、前 3 局取样规则

### 3.1 局序定义

Cohort 内每个玩家，在 `dws_ddz_daily_game` 中按以下顺序取窗口内最早 3 局：

```text
PARTITION BY uid
ORDER BY game_datetime, resultguid
```

`game_datetime` 相同时，使用 `resultguid` 做稳定排序兜底。

### 3.2 取样范围

| 口径项 | 规则 |
| ------ | ---- |
| 数据表 | `tcy_temp.dws_ddz_daily_game` |
| 玩法范围 | `play_mode BETWEEN 1 AND 6` |
| 房间范围 | 不限房间；第 2 / 3 局可离开 4484 / 12074 |
| 机器人过滤 | `robot != 1` |
| 分区窗口 | `dt BETWEEN '2026-06-18' AND '2026-06-24'` |
| 最大局序 | `game_seq <= 3` |

### 3.3 局序查询骨架

```sql
WITH cohort AS (
    -- 复用「二、Cohort 构建」中的 cohort CTE
    SELECT uid, reg_date, first_room_id, first_game_time
    FROM <cohort_cte>
),
ranked_games AS (
    SELECT
        game.uid,
        cohort.reg_date,
        ROW_NUMBER() OVER (
            PARTITION BY game.uid
            ORDER BY game.game_datetime, game.resultguid
        ) AS game_seq,
        game.dt,
        game.game_datetime,
        game.resultguid,
        game.room_id,
        game.play_mode,
        game.role,
        game.result_id,
        game.timecost,
        game.room_base,
        game.room_fee,
        game.start_money,
        game.end_money,
        game.game_outcome_money,
        game.magnification,
        game.real_magnification,
        game.hand_cards,
        game.bottom_cards,
        game.shuffle_type,
        game.card_id,
        game.card_power,
        game.card_power_final,
        game.cost_time,
        game.is_pass,
        game.shuffle_times,
        game.user_attr_bout
    FROM tcy_temp.dws_ddz_daily_game game
    INNER JOIN cohort ON cohort.uid = game.uid
    WHERE game.dt BETWEEN '2026-06-18' AND '2026-06-24'
      AND game.robot != 1
      AND game.play_mode BETWEEN 1 AND 6
)
SELECT *
FROM ranked_games
WHERE game_seq <= 3;
```

### 3.4 缺局处理

| 情况 | 处理方式 |
| ---- | -------- |
| 窗口内只有 1 局 | 计入 cohort；第 2 / 3 局指标不纳入该玩家 |
| 窗口内只有 2 局 | 计入 cohort；第 3 局指标不纳入该玩家 |
| 第 2 / 3 局跨到 06-25 | 本报告不追踪，视为窗口内未观察到 |
| 某字段为空 | 对该字段单独统计缺失率；均值 / 分位数计算排除 NULL |

所有表格必须显式标注分母，例如：

- 第 1 局指标分母 = cohort 人数。
- 第 2 局指标分母 = 窗口内观察到第 2 局的人数。
- 第 3 局指标分母 = 窗口内观察到第 3 局的人数。

---

## 四、局序总体体验概览

### 4.1 分析目的

在进入手牌细节前，先建立第 1 / 2 / 3 局的整体游戏结果背景，避免孤立解读牌力或配牌数据。

### 4.2 指标清单

按 `game_seq` 输出：

| 指标 | 说明 |
| ---- | ---- |
| 观察用户数 | 该局序有记录的去重用户数 |
| 房间仍在初级房占比 | `room_id IN (4484, 12074)` 的占比 |
| 玩法分布 | `play_mode` 分布 |
| 角色分布 | 地主 / 农民占比 |
| 胜率 | `result_id = 1` 的占比 |
| 平均耗时 | `AVG(timecost)` |
| 平均理论倍数 | `AVG(magnification)` |
| 平均实际倍数 | `AVG(real_magnification)` |
| 平均输赢 | `AVG(game_outcome_money)` |
| 期初 / 期末银子 | `AVG(start_money)`, `AVG(end_money)` |

### 4.3 输出表示例

| 局序 | 用户数 | 留在初级房占比 | 地主占比 | 胜率 | 平均耗时 | 平均理论倍数 | 平均实际倍数 | 平均输赢 |
| ---- | ------ | -------------- | -------- | ---- | -------- | ------------ | ------------ | -------- |
| 第 1 局 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 |
| 第 2 局 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 |
| 第 3 局 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 | 待跑数 |

---

## 五、牌力分布分析

### 5.1 分析目的

描述前 3 局 `card_power` 与 `card_power_final` 的分布形态及局序变化。

### 5.2 指标清单

按 `game_seq` 输出：

- `card_power`：均值、中位数、P25、P75。
- `card_power_final`：均值、中位数、P25、P75。
- 牌力分桶人数与占比。
- 地主 / 农民分角色的牌力分布。
- 字段缺失率。

### 5.3 分桶原则

不硬编码「好牌 = X 分」。建议基于本窗口前 3 局整体分布生成分位数阈值：

| 桶名 | 定义 |
| ---- | ---- |
| 低牌力 | `< P25` |
| 中低牌力 | `P25 ~ P50` |
| 中高牌力 | `P50 ~ P75` |
| 高牌力 | `>= P75` |

优点：

- 避免把牌力算法的绝对分值解释成固定业务含义。
- 适应后续算法尺度变化。
- 适合描述性分析。

### 5.4 查询骨架

```sql
WITH first_three_games AS (
    -- 复用「三、前 3 局取样规则」中的结果
    SELECT * FROM <first_three_games_cte>
),
power_quantiles AS (
    SELECT
        percentile_approx(card_power, 0.25) AS card_power_p25,
        percentile_approx(card_power, 0.50) AS card_power_p50,
        percentile_approx(card_power, 0.75) AS card_power_p75
    FROM first_three_games
    WHERE card_power IS NOT NULL
),
power_bucketed AS (
    SELECT
        game.*,
        CASE
            WHEN game.card_power IS NULL THEN 'Z: 牌力缺失'
            WHEN game.card_power < quant.card_power_p25 THEN 'A: 低牌力'
            WHEN game.card_power < quant.card_power_p50 THEN 'B: 中低牌力'
            WHEN game.card_power < quant.card_power_p75 THEN 'C: 中高牌力'
            ELSE 'D: 高牌力'
        END AS card_power_bucket
    FROM first_three_games game
    CROSS JOIN power_quantiles quant
)
SELECT
    game_seq,
    card_power_bucket,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(AVG(card_power), 2) AS avg_card_power,
    ROUND(AVG(card_power_final), 2) AS avg_card_power_final
FROM power_bucketed
GROUP BY game_seq, card_power_bucket
ORDER BY game_seq, card_power_bucket;
```

---

## 六、配牌机制分析

### 6.1 分析目的

描述前 3 局中配牌机制相关字段的分布，以及配牌记录与随机发牌记录的牌力差异。

### 6.2 字段说明

| 字段 | 含义 | 分析用法 |
| ---- | ---- | -------- |
| `shuffle_type` | 配牌类型，`201` 为新手保护机器人 | 统计各类型占比，重点观察 `201` |
| `card_id` | 牌库编号，`>0` 代表有配牌 | 统计有牌库编号的记录占比 |
| `is_pass` | 是否重洗成功 | 统计成功率 |
| `shuffle_times` | 重洗次数，`-1` 为未开启重洗 | 统计分布 |
| `cost_time` | 重洗牌花费时间，单位 ms | 统计均值 / 分位数 |

### 6.3 指标清单

按 `game_seq` 输出：

- `shuffle_type` 分布。
- `shuffle_type = 201` 占比。
- `card_id > 0` 占比。
- `is_pass = true` 占比。
- `shuffle_times` 分布。
- 配牌记录 vs 随机记录的 `card_power` / `card_power_final` 均值与分位数。

表达方式只呈现事实，例如：

- 「第 1 局 `shuffle_type = 201` 的记录占比为 X%。」
- 「第 1 局 `card_id > 0` 记录的平均 `card_power` 为 Y，`card_id = 0` 记录为 Z。」

避免写成因果或动机判断。

### 6.4 配牌标签建议

```sql
CASE
    WHEN shuffle_type = 201 THEN 'A: 新手保护配牌'
    WHEN card_id > 0 THEN 'B: 其他牌库配牌'
    WHEN card_id = 0 OR card_id IS NULL THEN 'C: 随机或无牌库'
    ELSE 'D: 其他'
END AS shuffle_group
```

---

## 七、手牌结构解析

### 7.1 分析目的

从 `hand_cards` 直接解析实际手牌结构，补充 `card_power` 之外的可解释牌面特征。

### 7.2 牌面编码

当前确认的牌面编码：

| 牌面 | 编码 |
| ---- | ---- |
| 3 ~ 9 | `3` ~ `9` |
| J / Q / K / A / 2 | `J` / `Q` / `K` / `A` / `2` |
| 小王 | `sj` |
| 大王 | `bj` |

注意：`sj` 与 `bj` 是双字符 token，解析时不能直接用字符串长度作为张数。应先将 `hand_cards` 解析成牌面 token 序列，再统计张数与结构。

### 7.3 基础版指标

优先实现基础版，确保稳定可复现：

| 指标 | 定义 |
| ---- | ---- |
| 手牌张数 | token 数量，常规初始牌应为 17 |
| 2 的张数 | `2` token 数量 |
| 小王 / 大王张数 | `sj` / `bj` token 数量 |
| 王炸标记 | 同时包含 `sj` 与 `bj` |
| A/K/Q/J 张数 | 各点数 token 数量 |
| 炸弹持有标记 | 任一点数出现 4 次；王炸单独统计 |
| 对子数 | 任一点数出现次数 >= 2 的点数个数 |
| 三张数 | 任一点数出现次数 >= 3 的点数个数 |
| 四张数 | 任一点数出现次数 = 4 的点数个数 |

### 7.4 增强版指标

若基础版执行稳定，再增加以下结构潜力指标：

| 指标 | 简化定义 |
| ---- | -------- |
| 顺子潜力 | 3 ~ A 中存在连续 5 个及以上点数，每个点数至少 1 张 |
| 连对潜力 | 3 ~ A 中存在连续 3 对及以上点数，每个点数至少 2 张 |
| 飞机潜力 | 3 ~ A 中存在连续 2 个及以上三张点数 |

增强版只作为结构特征，不推断玩家是否一定能打出该牌型。

### 7.5 实现建议

由于 StarRocks 中直接解析 `sj` / `bj` 双字符 token 较繁琐，建议采用两步：

1. SQL 侧输出前 3 局明细字段：`uid`, `game_seq`, `hand_cards`, `role`, `result_id`, `shuffle_type`, `card_id`, `card_power`。
2. Python 侧解析 `hand_cards`，产出结构指标后再汇总。

Python 解析函数应遵循：

```python
def tokenize_hand_cards(hand_cards: str) -> list[str]:
    """Parse landlord hand card string into card tokens.

    Recognized double-character tokens:
    - sj: small joker
    - bj: big joker
    """
    tokens = []
    idx = 0
    while idx < len(hand_cards):
        token = hand_cards[idx:idx + 2]
        if token in {"sj", "bj"}:
            tokens.append(token)
            idx += 2
        else:
            tokens.append(hand_cards[idx])
            idx += 1
    return tokens
```

### 7.6 注意事项

- `bomb_bet` 是打出炸弹数，不是持有炸弹数，不可替代手牌解析。
- `hand_cards` 是初始牌；地主拿到底牌后的最终手牌结构需另结合 `bottom_cards`，本框架默认先分析初始牌。
- 若需要地主最终手牌结构，可在增强版中将地主局的 `hand_cards + bottom_cards` 合并解析，并与 `card_power_final` 对齐。

---

## 八、牌力与胜负一致性

### 8.1 分析目的

描述不同牌力桶下的胜率、输赢和反常组合占比，观察「牌力」与实际结果是否方向一致。

### 8.2 指标清单

按 `game_seq` + `card_power_bucket` 输出：

| 指标 | 定义 |
| ---- | ---- |
| 用户数 | 该局序、该牌力桶内的去重用户数 |
| 胜率 | `result_id = 1` 占比 |
| 平均输赢 | `AVG(game_outcome_money)` |
| 平均实际倍数 | `AVG(real_magnification)` |
| 好牌输占比 | 高牌力桶中 `result_id = 2` 的占比 |
| 差牌赢占比 | 低牌力桶中 `result_id = 1` 的占比 |

建议拆分：

- 地主 / 农民。
- 新手保护配牌 / 非新手保护配牌。
- 初始牌力 `card_power` / 算底牌后牌力 `card_power_final`。

### 8.3 查询骨架

```sql
WITH power_bucketed AS (
    -- 复用「五、牌力分布分析」中的牌力分桶结果
    SELECT * FROM <power_bucketed_cte>
)
SELECT
    game_seq,
    card_power_bucket,
    COUNT(DISTINCT uid) AS user_count,
    ROUND(SUM(CASE WHEN result_id = 1 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(*), 0), 2) AS win_rate,
    ROUND(AVG(game_outcome_money), 2) AS avg_outcome_money,
    ROUND(AVG(real_magnification), 2) AS avg_real_magnification
FROM power_bucketed
WHERE card_power_bucket != 'Z: 牌力缺失'
GROUP BY game_seq, card_power_bucket
ORDER BY game_seq, card_power_bucket;
```

### 8.4 表达原则

推荐表达：

- 「第 1 局高牌力桶胜率为 X%，低牌力桶胜率为 Y%。」
- 「第 2 局低牌力桶中仍有 X% 记录获胜。」

避免表达：

- 「高牌力导致胜利。」
- 「系统故意让低牌力玩家也赢。」
- 「某类手牌造成玩家流失。」

---

## 九、输出报告结构建议

后续结果报告建议放置在：

```text
analysis/result/first-classic-beginner-handcards-report.md
```

建议章节：

1. 报告说明：时间、cohort、描述性边界。
2. Cohort 基线：样本量、首局房间来源、局序可达性。
3. 前 3 局整体体验：胜率、角色、房间流向、倍数、输赢。
4. 牌力分布：`card_power` / `card_power_final` 局序变化。
5. 配牌机制：`shuffle_type` / `card_id` / `shuffle_times` 分布。
6. 手牌结构：基础结构指标与增强结构指标。
7. 牌力与胜负一致性：牌力桶胜率、好牌输 / 差牌赢。
8. 明确不确定性：无法回答的因果问题、需要补充的数据。
9. SQL 与脚本索引：每个数字对应的查询文件。

---

## 十、SQL 与脚本复现清单

### 10.1 SQL 模块

| 文件名 | 目的 | 输出 |
| ------ | ---- | ---- |
| `01_cohort.sql` | 构建 cohort 与首局房间分布 | cohort 人数、首局日期、渠道分布 |
| `02_game_seq_overview.sql` | 前 3 局总体体验概览 | 局序 × 胜率 / 角色 / 倍数 / 输赢 |
| `03_card_power_distribution.sql` | 牌力分布 | 局序 × 牌力分桶 / 分位数 |
| `04_shuffle_mechanism.sql` | 配牌机制 | 局序 × shuffle_group |
| `05_handcard_export.sql` | 导出手牌明细给 Python 解析 | uid × game_seq × hand_cards |
| `06_cardpower_result_alignment.sql` | 牌力与胜负一致性 | 局序 × 牌力桶 × 胜负 |

### 10.2 Python 模块

| 文件名 | 目的 | 输出 |
| ------ | ---- | ---- |
| `parse_handcards.py` | 解析 `hand_cards`，生成结构指标 | 手牌结构明细与汇总 |
| `summarize_handcard_structure.py` | 汇总基础版 / 增强版结构指标 | 局序 × 结构指标表 |

Python 脚本执行时使用：

```powershell
py -3 -u .\parse_handcards.py
```

---

## 十一、SQL 编写原则

本分析所有 SQL 必须遵循 [SQL_STYLE.md](../../../SQL_STYLE.md) 与项目 SQL 规范：

1. 只写查询 SQL，不写 DDL。
2. 复杂逻辑使用 CTE 分层。
3. 分区列必须裁剪：`dt BETWEEN '2026-06-18' AND '2026-06-24'`。
4. 注册表必须带 `app_id = 1880053` 与 `reg_date` 窗口。
5. 用户计数默认使用 `COUNT(DISTINCT uid)`。
6. 百分比除法必须使用 `NULLIF(..., 0)` 防零除。
7. 局序排序必须使用 `ORDER BY game_datetime, resultguid`。
8. 别名必须有意义，例如 `reg`、`game`、`cohort`，禁止使用 `a`、`b`、`c`。
9. 不使用 `SELECT *` 作为最终查询输出。
10. 不把 `bomb_bet` 当作持有炸弹，持有炸弹只能从 `hand_cards` 解析。

---

## 十二、明确不确定性

| 问题 | 当前框架处理方式 | 后续需要 |
| ---- | ---------------- | -------- |
| 用户为什么留下或离开 | 不回答，只描述前 3 局事实 | 用户调研、退出问卷或 A/B 实验 |
| 牌力阈值是否有固定业务含义 | 不硬编码，使用窗口内分位数 | 研发提供牌力分值解释文档 |
| `hand_cards` 是否覆盖所有牌面编码 | 已确认 `sj` / `bj`，仍需在样本中校验异常 token | 执行解析脚本后输出异常 token 清单 |
| 地主最终手牌是否要纳入结构分析 | 基础版先分析初始牌；增强版可合并 `bottom_cards` | 结果阶段根据样本情况决定 |
| 配牌记录是否代表某种策略意图 | 不推断意图，只统计字段分布 | 研发配置记录或实验日志 |

---

> **文档版本**：v1.0
>
> **更新说明**：
>
> - v1.0：建立首次经典初级房玩家前 3 局手牌分析框架，确认 4484/12074 合并分析、固定 7 日窗口、局序纵向刻画、四个手牌主体模块（牌力分布、配牌机制、手牌结构、牌力-胜负一致性）。
