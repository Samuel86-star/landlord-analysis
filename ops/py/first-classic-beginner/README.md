# ops/py/first-classic-beginner/ — 首次经典初级房玩家前 3 局手牌分析

## 一键运行

```powershell
py -3 -u ops/py/first-classic-beginner/run_analysis.py
```

产出 ~17 个 CSV 到 `output/`（`output/` 在 .gitignore 排除，不提交）。

## 文件结构

```text
py/first-classic-beginner/
├── sql/01_cohort_first3_detail.sql   # 唯一的大表查询：cohort + 前 3 局明细（线性 CTE）
├── run_analysis.py                   # 主编排：拉数 + 7 模块聚合 + 落 CSV
├── README.md                         # 本文件
└── output/                           # 聚合结果（.gitignore 排除）
```

## 7 个分析模块

| 模块 | 内容 | 输出 CSV |
| ---- | ---- | ---- |
| A cohort 基线 | 房间 / 日期 / 渠道 / 可达性 | `01a-d_cohort_*.csv` |
| B 局序概览 | 胜率 / 角色 / 倍数 / 输赢 / 房间流向 | `02_game_seq_overview.csv` |
| C 牌力分布 | 整体 P25/P50/P75 分桶 + 分角色 | `03*_card_power_*.csv` |
| D 配牌机制 | 新手保护 / 其他牌库 / 随机 | `04*_shuffle_*.csv` |
| E 持有炸弹 | 从 `hand_cards` 解析四张同点与王炸 | `05*_bomb_*.csv` |
| F 牌力-胜负 | 牌力桶 × 胜率（含新手保护 vs 非保护对照） | `06*_cardpower_result_*.csv` |
| G 手牌结构 | 王情况 × 牌力 × 胜率 | `07*_*.csv` |

## 关键说明

- `hand_cards` 于 2026-06-25 上线；当前 v2 窗口直接解析逗号分隔牌面，持有炸弹口径为四张同点 + 王炸。旧窗口与旧报告仍保留为历史结果
- 本管线依赖 `ops/py/sr_exec.py` 新增的 `query_paged`（用 CloudBeaver `SQLDataFilter.offset/limit` 分页突破 200 行/页硬限制），是项目里第一次在 Python 拉大结果集
- 描述性分析，只呈现可观察事实与机制字段（`shuffle_type=201` 是新手保护机器人），不做因果推断

## 配套报告

`docs/analysis/result/first-classic-beginner-handcards-report.md`

## 窗口与口径

- `sql/01_cohort_first3_detail.sql` 是历史子集口径：先过滤玩法，再选择最早落在 4484/12074 的玩家；不能解释为注册后真实首局
- `sql/03_registered_first3_detail_20260831_20260906.sql` 是修正口径：注册时间后最早的真人 53 游戏对局，不按玩法、房间或 `user_attr_bout` 预筛
- 公共过滤：`robot != 1`，局序按 `game_datetime, resultguid` 排序；只有历史 SQL 额外过滤 `play_mode BETWEEN 1 AND 6`
- 牌力分桶：前 3 局合并的 P25/P50/P75，不硬编码阈值

## 历史窗口重算

`shuffle_times` 缺失值与空手牌口径修复不会自动改写已落库分区。重算当前 v2 窗口前先预览，确认后回填 DWS，再重跑分析：

```powershell
py -3 -u ops/py/batch_insert_ddz_daily_game.py --start 20260625 --end 20260701 --dry-run
py -3 -u ops/py/batch_insert_ddz_daily_game.py --start 20260625 --end 20260701
py -3 -u ops/py/first-classic-beginner/run_analysis.py
```
