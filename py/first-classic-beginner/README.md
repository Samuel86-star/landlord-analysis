# py/first-classic-beginner/ — 首次经典初级房玩家前 3 局手牌分析

## 一键运行

```powershell
py -3 -u py/first-classic-beginner/run_analysis.py
```

产出 ~17 个 CSV 到 `output/`（`output/` 在 .gitignore 排除，不提交）。

## 文件结构

```text
py/first-classic-beginner/
├── sql/01_cohort_first3_detail.sql   # 唯一的大表查询：cohort + 前 3 局明细（线性 CTE）
├── run_analysis.py                   # 主编排：拉数 + 6 模块聚合 + 落 CSV
├── README.md                         # 本文件
└── output/                           # 聚合结果（.gitignore 排除）
```

## 6 个分析模块

| 模块 | 内容 | 输出 CSV |
| ---- | ---- | ---- |
| A cohort 基线 | 房间 / 日期 / 渠道 / 可达性 | `01a-d_cohort_*.csv` |
| B 局序概览 | 胜率 / 角色 / 倍数 / 输赢 / 房间流向 | `02_game_seq_overview.csv` |
| C 牌力分布 | 整体 P25/P50/P75 分桶 + 分角色 | `03*_card_power_*.csv` |
| D 配牌机制 | 新手保护 / 其他牌库 / 随机 | `04*_shuffle_*.csv` |
| E 持有炸弹 | `bomb_cnt` / `bomb_final` 分布（替代手牌结构） | `05*_bomb_*.csv` |
| F 牌力-胜负 | 牌力桶 × 胜率（含新手保护 vs 非保护对照） | `06*_cardpower_result_*.csv` |

## 关键说明

- **`hand_cards` / `bottom_cards` 在数仓 `extend_content` 全历史 0 覆盖**（03~06 月查证），框架原计划第七章"手牌结构解析"无法落地，改用 `card_power` 节点里表文档未记录的 `bomb_cnt` / `bomb_final`（持有炸弹数）替代分析。详见 `analysis/result/first-classic-beginner-handcards-report.md` 第七章不确定性
- 本管线依赖 `py/sr_exec.py` 新增的 `query_paged`（用 CloudBeaver `SQLDataFilter.offset/limit` 分页突破 200 行/页硬限制），是项目里第一次在 Python 拉大结果集
- 描述性分析，只呈现可观察事实与机制字段（`shuffle_type=201` 是新手保护机器人），不做因果推断

## 配套报告

`analysis/result/first-classic-beginner-handcards-report.md`

## 窗口与口径

- 注册与对局窗口：`2026-06-18 ~ 2026-06-24`（7 日固定，不向 06-25 追踪）
- Cohort：首次经典对局在 4484 / 12074（合并不拆分），且发生在 `reg_date` 当日
- 过滤：`robot != 1`、`play_mode BETWEEN 1 AND 6`、局序 `ORDER BY game_datetime, resultguid`
- 牌力分桶：前 3 局合并的 P25/P50/P75，不硬编码阈值
