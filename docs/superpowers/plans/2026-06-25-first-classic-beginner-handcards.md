# 首次经典初级房玩家前 3 局手牌分析 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `analysis/plan/first-classic-beginner-handcards/first-3-games-handcard-analysis-framework.md` 落地为一键运行的 SQL+Python 管线，产出 cohort 前 3 局手牌/牌力/配牌/胜负的事实清单（CSV），并生成描述性分析报告。

**Architecture:** 一条线性 CTE 的 StarRocks SQL 导出 cohort 前 3 局明细宽表（~21,025 行）到 pandas DataFrame；所有多维度聚合（牌力分位数分桶、配牌分组、hand_cards 双字符 token 解析、牌力-胜负一致性）在本地 pandas 完成；每模块结果落 CSV，最终汇总成 md 报告。

**Tech Stack:** StarRocks（经 `py/sr_exec.py` 的 CloudBeaver GraphQL）、Python 3 + pandas、`py -3` 调用。

## Global Constraints

- SQL 仅 SELECT，**禁止 DDL**（CLAUDE.md）；DDL 由用户在 CloudBeaver 手动执行
- 分区裁剪：所有 `dws_ddz_daily_game` 查询带 `dt BETWEEN '2026-06-18' AND '2026-06-24'`
- **CTE 线性引用**：同一 CTE 不得被下游多次引用（StarRocks CTE 默认内联，多次引用会重复扫描 702 万行大表 → CloudBeaver 任务失败 → sr_exec 静默吞错返回空）。多维度统计必须放到 pandas 本地做
- 窗口固定 `2026-06-18 ~ 2026-06-24`，不追踪 06-25；房间 `4484`/`12074` 合并不拆分
- 过滤：`robot != 1`、`play_mode BETWEEN 1 AND 6`；局序 `ORDER BY game_datetime, resultguid`
- 计数用 `COUNT(DISTINCT uid)`；百分比用 `NULLIF(..., 0)` 防零除
- Python 一律 `py -3`（CLAUDE.local.md）；读 SQL 文件强制 `encoding='utf-8'`
- **sr_exec 对失败查询静默返回空**：脚本必须校验结果非空，否则中止报错（这是探查阶段踩到的坑）
- 描述性分析，不做因果推断；牌力分桶用窗口内分位数，不硬编码阈值
- `card_power` 算法 2026-06-15 已修复，窗口在其后，字段可信

## 探查阶段已确认的事实（计划的输入）

| 项 | 值 |
| ---- | ---- |
| cohort 总人数 | 7,529 |
| 第 1 局分母 | 7,529（100%） |
| 第 2 局分母 | 6,945（92.3%） |
| 第 3 局分母 | 6,551（87.0%） |
| 前 3 局明细总行数 | 21,025 |
| 大表 7 天总量 | 702 万行 |

## File Structure

```text
py/first-classic-beginner/
├── sql/
│   └── 01_cohort_first3_detail.sql   # 唯一大表查询：cohort + 前3局明细导出（线性CTE）
├── parse_handcards.py                # hand_cards tokenize + 结构指标（框架 7.5）
├── test_parse_handcards.py           # 解析单元测试（TDD）
├── run_analysis.py                   # 主编排：SQL→DataFrame→各模块聚合→CSV
├── README.md                         # 一键运行说明
└── output/                           # 每模块 CSV（.gitignore 排除）
analysis/result/
└── first-classic-beginner-handcards-report.md   # 最终描述性分析报告
```

---

## Task 1: SQL — cohort + 前 3 局明细导出

**Files:**
- Create: `py/first-classic-beginner/sql/01_cohort_first3_detail.sql`
- Modify: `.gitignore`（新增 `py/first-classic-beginner/output/`）

**Interfaces:**
- Produces: 一条 SELECT，输出 cohort 内每个玩家前 3 局明细，列含 `uid, reg_date, first_room_id, game_seq, dt, game_datetime, resultguid, room_id, play_mode, role, result_id, timecost, room_base, room_fee, start_money, end_money, game_outcome_money, magnification, real_magnification, hand_cards, bottom_cards, shuffle_type, card_id, card_power, card_power_final, cost_time, is_pass, shuffle_times, user_attr_bout`

- [ ] **Step 1: 写 SQL 文件**（线性 CTE：reg_base → classic_first → cohort → ranked → SELECT WHERE game_seq<=3；ranked 只引用 cohort 一次，cohort 只引用 classic_first 一次）

完整 SQL 见执行阶段（基于已验证的 `py/tmp/probe_cohort.sql`，把 ranked 的 SELECT 字段从仅 `uid` 扩展为全部明细字段，外层 `WHERE game_seq <= 3`）。

- [ ] **Step 2: sr_exec 跑通验证**

Run: `py -3 -u ops/py/sr_exec.py -f ops/py/first-classic-beginner/sql/01_cohort_first3_detail.sql`
Expected: 返回 21,025 行（±少量浮动），列名齐全。**若返回空 `[]` → 立即中止**（说明 CTE 被多次引用或字段错误）。

- [ ] **Step 3: 更新 .gitignore** 追加 `py/first-classic-beginner/output/`

---

## Task 2: parse_handcards.py（TDD）

**Files:**
- Create: `py/first-classic-beginner/test_parse_handcards.py`
- Create: `py/first-classic-beginner/parse_handcards.py`

**Interfaces:**
- Produces: `tokenize_hand_cards(hand_cards: str) -> list[str]`；`parse_hand_structure(tokens: list[str]) -> dict`（含 hand_size, count_2, sj, bj, has_rocket, bomb_count, pair_count, triple_count, quad_count, counts_A/K/Q/J 等）

- [ ] **Step 1: 写失败测试**（覆盖：常规17张、sj/bj 双字符 token、王炸、四张炸弹、对子/三张计数、空串、异常 token）

- [ ] **Step 2: 跑测试确认失败** — `py -3 -m pytest ops/py/first-classic-beginner/test_parse_handcards.py -v`（Expected: FAIL，模块不存在）

- [ ] **Step 3: 实现 parse_handcards.py**（tokenize 用框架 7.5 的双字符优先扫描；结构指标按框架 7.3 基础版）

- [ ] **Step 4: 跑测试确认通过**（Expected: PASS）

- [ ] **Step 5: 增强版指标**（顺子/连对/飞机潜力，框架 7.4）— 作为可选函数，测试覆盖

---

## Task 3: run_analysis.py 主编排

**Files:**
- Create: `py/first-classic-beginner/run_analysis.py`

**Interfaces:**
- Consumes: `py/sr_exec.py` 的 `StarRocksClient.query()`；`parse_handcards.py`；`sql/01_cohort_first3_detail.sql`
- Produces: `output/` 下若干 CSV（cohort 基线、局序概览、牌力分布、配牌机制、手牌结构、牌力-胜负一致性）

- [ ] **Step 1: 骨架** — import StarRocksClient + pandas + parse_handcards；读 SQL 文件 utf-8；query → DataFrame；**校验 len(df)>0 否则 raise**

- [ ] **Step 2: 模块A cohort 基线** — 首局房间分布、首局日期分布、渠道分布、可达性（game_seq max 分布）→ `output/01_cohort_baseline.csv`

- [ ] **Step 3: 模块B 局序概览**（框架四）— 按 game_seq：用户数、仍在初级房占比、玩法/角色分布、胜率、平均耗时/倍数/输赢/期初期末银子 → `output/02_game_seq_overview.csv`

- [ ] **Step 4: 模块C 牌力分布**（框架五）— 用整体前 3 局 card_power 算 P25/P50/P75 → 分桶（低/中低/中高/高）→ 按 game_seq × bucket 输出人数、avg card_power/card_power_final；另输出分角色 → `output/03_card_power_distribution.csv`

- [ ] **Step 5: 模块D 配牌机制**（框架六）— shuffle_group 标签（A 新手保护201 / B 其他牌库 card_id>0 / C 随机）→ 按 game_seq × shuffle_group：占比、card_power 均值对比、is_pass 成功率、shuffle_times 分布 → `output/04_shuffle_mechanism.csv`

- [ ] **Step 6: 模块E 手牌结构**（框架七）— 对每行 hand_cards 调 parse_hand_structure → 按 game_seq 汇总：平均张数、2/王/炸弹/王炸持有率、对子/三张/四张数、顺子潜力率 → `output/05_handcard_structure.csv`；并输出异常 token 清单

- [ ] **Step 7: 模块F 牌力-胜负一致性**（框架八）— 按 game_seq × card_power_bucket：胜率、平均输赢、平均实际倍数、好牌输占比、差牌赢占比；并按地主/农民 + 新手保护/非新手保护拆分 → `output/06_cardpower_result_alignment.csv`

- [ ] **Step 8: 跑通整条管线** — `py -3 -u ops/py/first-classic-beginner/run_analysis.py`，6 个 CSV 全部落盘且非空

---

## Task 4: 执行 + 产出报告

**Files:**
- Create: `analysis/result/first-classic-beginner-handcards-report.md`

- [ ] **Step 1: 跑 run_analysis.py 产出 6 个 CSV**

- [ ] **Step 2: 读 CSV 结果**，按框架第九章节结构写描述性报告（报告说明 / cohort 基线 / 前3局整体体验 / 牌力分布 / 配牌机制 / 手牌结构 / 牌力-胜负一致性 / 不确定性 / SQL 脚本索引）。**只呈现事实，不做因果推断**

- [ ] **Step 3: 清理 ops/py/tmp/**（probe_cohort.sql、debug_query.py 为探查临时文件）

- [ ] **Step 4: 写 ops/py/first-classic-beginner/README.md**（一键运行说明）

- [ ] **Step 5: git 提交**（脚本 + 报告，output/ 与 tmp/ 不提交）

---

## Self-Review

- **Spec coverage**：框架二(cohort)→T1/T3-A；三(局序取样)→T1；四(概览)→T3-B；五(牌力)→T3-C；六(配牌)→T3-D；七(手牌结构)→T2+T3-E；八(胜负一致性)→T3-F；九(报告)→T4。全覆盖。
- **Placeholder**：SQL 与 Python 完整代码在执行阶段给出，本计划已明确字段清单、接口签名、验证命令与期望值。
- **Type consistency**：`parse_hand_structure` 返回 dict 的 key 在 T2 定义、T3-E 消费，已对齐（hand_size/bomb_count/has_rocket 等）。
