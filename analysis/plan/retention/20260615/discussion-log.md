# 留存分析框架讨论记录

> 记录 Samuel 与 Claude 关于留存分析框架方案的讨论过程与结论。
> 讨论目录：`analysis/plan/retention/20260615/`

---

## 2026-06-17：高相关性指标区间划分讨论

### 议题一：首日胜率区间划分（进行中）

**现状**：`<30% / 30-50% / 50-70% / 70%+`

**Samuel 观点**：

- 正常胜率大约在 40% 左右，30-50% 这个区间正好卡在中间，会囊括大部分用户，区分度不够
- 区间跨度 20% 偏粗

**Claude 查询线上数据发现**：

- 1-2 局用户（~17%）胜率极端集中在 0% 和 100%，是离散值，无连续分析意义
- 6 局以上用户（~62%）分布开始收敛：均值 50-52%，标准差随局数增加而下降（20 局+ 仅 7.9%）
- 5 月单月 6 局+用户按 10% 档位分布：40-50% 和 50-60% 两档各占约 25-30%，合计过半；30-40% 和 60-70% 各约 12-15%
- 带留存 JOIN 的大范围查询在 StarRocks 中超时，确认需要先建 `dws_app_firstday_retention` 宽表后再做留存分组分析

**结论**：待宽表建好后用 10% 档位跑留存数据确认区分度。

---

### 建模方案：dws_app_firstday_retention 宽表（进行中）

**背景**：每次调整分组阈值都需要重复 JOIN 大表，性能瓶颈严重。

**方案**：将"注册 → 首日指标 → 留存 flag"预计算为一张用户级宽表。首日指标写入后不变，留存 flag 按到期日逐步回填（D1→T+2, D7→T+8, D30→T+31）。

**目录决策**：新建 `starrocks/retention/`，与 `analysis/plan/retention/` 形成数据层与分析层对应。

**文档**：[starrocks/retention/dws_app_firstday_game_stat.md](../../../../starrocks/retention/dws_app_firstday_game_stat.md)（v2 已拆分，见下）

**待确认**：

- 字段设计是否合理（已新增固定倍数段字段 `multi_24_48_*` / `multi_48_96_*` / `multi_96_plus_*`，与 Q4 四分位并存）
- UPDATE 回填方案在 StarRocks DUPLICATE KEY 下的性能是否可接受
- 是否需要 D14 留存 flag

---

> **创建时间**：2026-06-17
> **状态**：进行中

---

## 2026-06-18：retention 域拆分重构（v2 设计）

### 背景：v1 一站式宽表遇阻

v1 的 `dws_app_firstday_retention` 把"注册信息 + 首日游戏指标 + 留存 flag"全塞一张表，INSERT SQL 是 `6 表 LEFT JOIN + 2 个 CTE`（game_retention / login_retention 各自聚合 d1/d7/d30）。落地时发现：

- **StarRocks 优化器超时**：跑 reg_date=2026-05-14 的 INSERT，网页端报 `StarRocks planner use long time 3000 ms in memo phase`（`new_planner_optimize_timeout` 默认 3s）。SQL 太复杂，planner 在搜索 join 顺序时超时，连执行计划都没生成。
- **脚本假成功**：CloudBeaver 异步 API 把优化器超时吞了，任务以 `running=false` + `statusMessage=Executed` 结束，`sr_exec.py` 判定成功，但目标表 0 行。retention_flag 跑出来全是 0 行就是这么来的。
- **更新节奏冲突**：首日指标"注册当日写一次不变"，flag"按到期日逐步回填"，混在一张 DUPLICATE KEY 表里，刷 flag 必须整行 DELETE+INSERT 重写，浪费 IO。

### Samuel 的关键提问

问"firstday_retention 是不是每天得把年度新注册玩家全刷一次"——引出对"首日指标 vs 留存 flag 更新节奏不同"的重新审视。查 `retention-analysis-framework.md` 确认：所有高相关性指标（首日对局数/胜率/连败/银子净变化）都是**注册当日**快照，来源是 `dws_app_*_stat` 当天数据。这印证了"首日指标写一次不变"的设计本意。

### 方案：拆成两张表

参照已有的 `dws_app_daily_allgame_stat`（把 allgame_stat 从 uid×dt×play_mode 降维到 uid×dt）的降维思路，把 retention flag 也"降维"成独立表：

- **`dws_app_firstday_game_stat`**（由 firstday_retention 改名 + 删 flag 列）：注册信息 + 首日游戏指标，silver_/score_/allgame_ 前缀，注册当日写一次不再动。INSERT 降到 `3 表 LEFT JOIN 无 CTE`。
- **`dws_app_retention_flag`**（新建）：D+1/D+3/D+7/D+14/D+30 留存 flag，**NULL/0/1 三态**（NULL=未到期、0=到期未留存、1=到期已留存），按到期日逐步回填。INSERT 是 `2 表 LEFT JOIN`。
- 分析时 `firstday_game_stat LEFT JOIN retention_flag ON (app_id, reg_date, uid)`。

### 关键决策

1. **flag 列从 firstday 表移走**：DUPLICATE KEY 表不支持高效 UPDATE 单列，flag 留在宽表里刷不动，独立成表才能各自按节奏更新。
2. **NULL/0/1 三态**：取代 v1 的 `COALESCE(..., 0)`（把未到期也填 0，丢失语义）。三态下 `SUM(d_n)/COUNT(d_n)` 自动忽略 NULL，留存率分母天然只算已到期用户。
3. **加 D3/D14**：v1 只有 d1/d7/d30，这次借拆分机会补齐 d3/d14，覆盖更多留存里程碑。
4. **firstday_game_stat 由 daily_retention 调度（35 天回扫）**：虽然首日指标写一次不变，回扫是幂等浪费，但保持 retention 域一个入口更省心（选 a）。daily_backfill 仍只管 15 张当天表。
5. **不选物化视图 / 调大 timeout**：前者维护成本高、后者治标不治本（SQL 复杂度没降）。

### 落地验证

2026-06-18 实跑 reg_date=2026-05-14：

```text
A1 daily_allgame_stat   43480 rows  3.9s
A2 retention_flag        1214 rows 24.0s
B  firstday_game_stat    1214 rows  6.6s   ← 不再 0 行，优化器超时解决
```

firstday_game_stat 从 v1 的 0 行变 1214 行，跟主表 app_daily_reg 行数一致，LEFT JOIN 无丢行。

### 产出

- 新表 `dws_app_retention_flag`（DDL 手动建）
- `dws_app_firstday_retention` RENAME 为 `dws_app_firstday_game_stat` + DROP 6 个 flag 列
- 脚本：`batch_insert_retention_flag.py` / `batch_insert_firstday_game_stat.py`
- `daily_retention.py` 调度器从 2 层升 3 层（A1/A2/B）
- 文档：[dws_app_retention_flag.md](../../../../starrocks/retention/dws_app_retention_flag.md) / [dws_app_firstday_game_stat.md](../../../../starrocks/retention/dws_app_firstday_game_stat.md) v2.0

### 经验沉淀

- StarRocks 复杂 SQL（多 CTE + 多 JOIN）容易触发优化器超时，优先**拆表降维**而非调 timeout。
- CloudBeaver 异步 API 会吞 SR 内部错误（优化器超时、strict mode 回滚都表现为 statusMessage=Executed），INSERT 后必须 COUNT 复核，排查走 `load_tracking_logs` 或网页端真实报错（详见 [ops/troubleshooting.md](../../../../ops/troubleshooting.md)）。
