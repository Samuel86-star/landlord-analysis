# runs/ — 历次实验与验证脚本

2026-08/09 发牌调参期间驱动 `../tools/sweep.py` 与顶层 harness 的实验入口。结论已沉淀进报告（见各行「结论去向」），脚本保留作证据链与复现入口。

| 脚本 | 用途 | 结论去向 |
|---|---|---|
| `run_optv2_ab.py` | 原版 vs 优化版 harness 同 seed A/B（逐行 diff + 指标对比） | docs/analysis/result/new4-4484-12074-human-advantage-0901-report.md |
| `run_new4.py` | new4 配置理论分布（b13s17 nopair 与 +bombcode 2:1 混合，读样本 jsonl） | 同上 |
| `run_new4_both.py` | new4：原版 harness vs harness_optv2 各自实跑 | 同上 |
| `run_new4_seats.py` | new4 座位视角（--reals 3 全真人，两种子） | 同上 |
| `run_new4_strat_seats.py` | new4 策略×座位交叉 | 同上 |
| `run_era_sim.py` | pre-9.1 vs post-9.1 配置时代模拟（输入 `../results/makedeal_pre91.json`，git show 取 HEAD 版对照） | 9.1 突变归因分析 |
| `run_purerand_seeds.py` | 纯随机多 seed 基线 | sweep 基线对照 |
| `run_mix13.py` | 混 13 炸码对照（调 sweep.run_one） | 低等级策略结论 |
| `verify_budget_bug.py` | 做牌预算 bug 修复验证 | 预算修复验证记录 |
| `verify_budget_sensitivity.py` | 做牌预算敏感性 | 同上 |
| `verify_extreme_hands.py` | 极端手牌边界验证 | 拆牌边界结论 |

运行方式：在 runs/ 内 `py -3 -u <脚本>`（harness 在上一级、makedeal.json 在上两级 previous/、工具链在 ../tools/，路径已配好）。

> 样本 jsonl 已外迁：默认 `../results/sweep_runs`，本机大数据在 `D:\analysis\sim-data\landlord-sim\sweep_runs`（`run_new4.py` 等读样本的脚本设 `SWEEP_RUNS_DIR` 指向它）。
