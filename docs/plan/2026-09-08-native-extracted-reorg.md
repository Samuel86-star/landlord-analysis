# 方案：algorithm/native/ 目录归拢（extracted 五类混放 → 四区）

> **历史方案**：文中的双仓与只读快照约束已于 2026-09-10 停用；当前直接维护本仓 `algorithm/`。

| 项 | 值 |
| --- | --- |
| 日期 | 2026-09-08 |
| 状态 | 待审（未实施） |
| 范围 | `algorithm/native/`（extracted/ 内部归拢 + native 根散文件归位 + 数据外迁） |
| 已采纳 | 方案 B（就地归拢，extracted/ 名字与模拟器源码位置不动） |

## 一、背景与问题

`native/extracted/` 自 2026-08 起累积了五类性质不同的东西，全部平铺在一层：

| 类别 | 文件 | 性质 |
| --- | --- | --- |
| 模拟器源码 | `harness.cpp`（原版）、`harness_optv2.cpp`（研发 09-01 优化版复刻） | 核心资产 |
| 生成器 | `_gen_optv2_harness.py`（生成 harness_optv2.cpp） | 核心资产，未跟踪 |
| 拆牌头文件+测试 | `optimal_split.h`、`optimal_split_power.h`、`split_test.cpp`、`power_split_test.cpp`、`verify_split_vs_power.cpp` | 已跟踪，CMakeLists 引用 |
| 长期工具链 | `sweep.py`、`stats.py`、`anchor_check.py` | 已跟踪 |
| 一次性实验脚本 | **15 个**未跟踪 `_*.py`（`_gen`×1、`_run`×11、`_verify`×3） | 未纳管、无索引 |
| 可再生产物 | `top20_*.{json,md}`×5、`sweep_raw.json`、`makedeal_pre91.json` | 结论产物与缓存混在源码间 |
| 大体积数据 | `sweep_runs/*.jsonl`×20 ≈ **413MB**（已 gitignore，占本地盘） | 原始样本 |
| 编译产物 | `*.exe`×3、`*.obj`×1、`__pycache__/` | 已 gitignore |

另有 `native/` 根部散放 `shuffle_prng_compare.py` + `shuffle_prng_compare_README.md`。

> 数量勘误：此前讨论中口径"13 个脚本、10 留 3 删"有误，实际 **15 个：12 留 3 删**，以本文清单为准。

## 二、治理约束（快照同步边界）

2026-09-06 起 `algorithm/` 为 landlord-algorithm **只读快照**（`algorithm/README.md`、`docs/tech/algorithm-snapshot-plan.md`），但快照计划明确：`native/extracted/`、`native/previous/`、`shuffle_prng_compare*` 为 **analysis 专用资产，同步时保留**。本方案只动这三处 + analysis 自有文档，**不触碰快照清单文件**（`src/`、`pom.xml`、`include/`、`test/`、`native/CMakeLists.txt`、`algorithm/docs/`）。

`native/CMakeLists.txt` 引用 `extracted/power_split_test.cpp`、`extracted/verify_split_vs_power.cpp` —— 两个 cpp 留在 extracted/ 顶层不动，CMake 零改动。

## 三、目标结构

```text
algorithm/native/
├── CMakeLists.txt                  # 不动
├── include/landlord.h              # 不动（harness*/optimal_split 的公共依赖）
├── config/scoring.properties       # 不动
├── test/                           # 不动
├── previous/                       # 不动（线上参照副本，2026-09-08 已同步 MakeDealHelper 新版）
├── tools/                          # ① 新建：native 根散文件归位
│   ├── shuffle_prng_compare.py
│   └── shuffle_prng_compare_README.md
└── extracted/                      # 内部四区：源码顶层 / tools / runs / results
    ├── README.md                   # 重写（新布局 + 编译运行 + runs 索引）
    ├── harness.cpp                 # 留顶层（include "../include/landlord.h" 不变）
    ├── harness_optv2.cpp           # 留顶层 + 纳入 git（原未跟踪）
    ├── gen_optv2_harness.py        # ← _gen 去 _ 前缀，纳入 git（HERE/DST 仍指 extracted/，零改路径）
    ├── optimal_split.h / optimal_split_power.h
    ├── split_test.cpp / power_split_test.cpp / verify_split_vs_power.cpp   # 留顶层（CMake 引用）
    ├── tools/                      # ② 长期工具链（git mv）
    │   ├── sweep.py                #    改 HERE 层级 + 输出路径参数化（见 §六.1）
    │   ├── stats.py
    │   └── anchor_check.py
    ├── runs/                       # ③ 实验脚本精选（12 个，去 _ 前缀，纳入 git）
    │   ├── README.md               #    每脚本一行用途 + 结论指向
    │   ├── run_optv2_ab.py         #    optv2 vs 原版 A/B（证据链）
    │   ├── run_new4.py / run_new4_both.py / run_new4_seats.py / run_new4_strat_seats.py
    │   ├── run_era_sim.py          #    pre/post-9.1 时代模拟
    │   ├── run_purerand_seeds.py / run_mix13.py
    │   └── verify_budget_bug.py / verify_budget_sensitivity.py / verify_extreme_hands.py
    └── results/                    # ④ 结论产物与缓存
        ├── top20_configs.json / top20_configs_real.json          # git mv
        ├── top20_report.md / top20_report_real.md / top20_compare.md  # git mv
        ├── sweep_raw.json          # move（仍被 .gitignore 忽略，rerank 缓存）
        └── makedeal_pre91.json     # move + git add（era_sim 输入：pre-9.1 线上配置快照）

D:\analysis\sim-data\landlord-sim\   # 仓库外数据区
└── sweep_runs\*.jsonl ×19           # 390MB 外迁目的地
```

## 四、迁移清单

### 4.1 git mv（已跟踪 → 新路径）

| 源（extracted/） | 目标 |
| --- | --- |
| `sweep.py` / `stats.py` / `anchor_check.py` | `tools/` |
| `top20_configs.json` / `top20_configs_real.json` / `top20_report.md` / `top20_report_real.md` / `top20_compare.md` | `results/` |

### 4.2 移动 + git add（原未跟踪 → 纳入版本管理）

| 源（extracted/） | 目标 | 备注 |
| --- | --- | --- |
| `harness_optv2.cpp` | 顶层（不动，仅 add） | 研发优化版复刻，与生成器一起入库可复现 |
| `_gen_optv2_harness.py` | `gen_optv2_harness.py` | 去 _ 前缀；HERE/DST 无需改 |
| `_run_optv2_ab.py` 等 8 个 `_run_*` | `runs/run_*.py` | 去 _ 前缀；改 HERE 层级（§六.2） |
| `_verify_budget_bug.py` 等 3 个 | `runs/verify_*.py` | 同上 |
| `makedeal_pre91.json` | `results/` | |
| `sweep_raw.json` | `results/` | 仍被忽略不入库，仅随迁 |
| `native/shuffle_prng_compare.py` + `shuffle_prng_compare_README.md` | `native/tools/` | |

### 4.3 删除（实施前向用户出示原文确认）

| 文件 | 理由 |
| --- | --- |
| `_run_tune420.py` / `_run_tune420b.py` / `_run_tune420c.py` | 420 调参一次性脚本，结论已沉淀在报告与 `docs/makedeal-strategies/classic/420.md` |
| `extracted/__pycache__/`（2 个 .pyc） | 编译缓存 |
| `extracted/harness_optv2.obj`（1.27MB） | 编译产物，重编译即得 |

`harness.exe` / `harness_optv2.exe` / `split_test.exe` **保留**在 extracted/ 顶层（脚本按同目录引用，已被 .gitignore 挡住）。

## 五、数据外迁

1. `extracted/sweep_runs/*.jsonl` ×20 → `D:\analysis\sim-data\landlord-sim\sweep_runs\`（纯文件移动，git 无感）。
2. `sweep.py` 输出目录参数化（向后兼容）：
   - 新增 `--runs-dir <path>` 参数；优先级：命令行 > 环境变量 `SWEEP_RUNS_DIR` > 缺省 `extracted/results/sweep_runs/`（`.gitignore` 的 `sweep_runs/` 无前导斜杠，匹配任意层级，缺省目录仍被忽略）。
   - 本机大扫描用 `--runs-dir D:\analysis\sim-data\landlord-sim\sweep_runs` 或设环境变量；README 注明。

## 六、引用修补清单

### 6.1 代码（extracted/ 内）

| 文件 | 修改 |
| --- | --- |
| `tools/sweep.py` | L23-25：`HERE` → `SIM = HERE.parent`；`HARNESS = SIM/"harness.exe"`；`CFG = SIM.parent/"previous"/"makedeal.json"`；`sweep_raw.json`/`top20_*` 读写改 `SIM/"results"/...`；§五的 `--runs-dir` 参数化 |
| `runs/*.py` ×12 | 统一模式：harness 在 `HERE.parent`、previous 在 `HERE.parent.parent`（各脚本 2-4 行机械改动） |
| `README.md`（extracted） | 重写：四区布局、编译命令（不变）、运行示例、`tools/`/`runs/` 索引、数据外迁说明 |
| `runs/README.md` | 新增：12 脚本各一行用途 + 对应结论报告链接 |
| `.gitignore`（extracted） | 微调注释；模式本身已覆盖新布局（`*.jsonl`、`sweep_runs/`、`sweep_raw.json` 均无前导斜杠） |

### 6.2 活文档（改路径引用；**带日期的历史文档 plan/review/spec 一律不改**）

| 文件 | 改动点 |
| --- | --- |
| `docs/knowledge/makedeal-simulation.md` | harness/anchor_check/sweep 命令路径 → `tools/`；top20 产物路径 → `results/`（约 8 处） |
| `docs/knowledge/makedeal-evaluation-sop.md` | `sweep.py` 路径 ×2 |
| `docs/makedeal-strategies/README.md` | `top20_report.md` 链接、sweep/anchor 命令路径（约 6 处） |
| `docs/makedeal-strategies/classic/420.md` | top20 链接 ×1（`extracted/harness.exe` 字样不变） |
| `docs/makedeal-strategies/classic/742.md` | top20 链接 ×1 |
| `docs/makedeal-strategies/_template.md` | 模板路径 ×2 |
| `docs/tech/algorithm-snapshot-plan.md` | 保留清单中 `shuffle_prng_compare*` 路径注明新位置 `native/tools/` |

### 6.3 不需要改的（已核实）

- `native/CMakeLists.txt`（两个测试 cpp 留原地）
- `harness*.cpp` / `optimal_split*.h` 的 `#include "../include/landlord.h"`（cpp 留顶层）
- `gen_optv2_harness.py` 的 `DST`（仍在 extracted/ 顶层）
- `ops/py/verify_offline.py`（grep 证实不引用这些路径）
- `docs/plan/`、`docs/review/`、`docs/spec/` 下历史文档（含 2026-09-04/05 快照流程文档中的旧路径描述，属历史记录）

## 七、实施顺序

1. 数据外迁：`sweep_runs/` → `D:\analysis\sim-data\landlord-sim\sweep_runs\`
2. 删除项（先向用户出示 tune420×3 确认）+ 清 `__pycache__`、`*.obj`
3. `git mv` §4.1 八个文件；移动+`git add` §4.2（含 `native/tools/` 归位）
4. 代码修补 §6.1（sweep.py → runs×12 → 两个 README → .gitignore）
5. 活文档修补 §6.2
6. 验证闭环 §八
7. 单 commit：`refactor(native): extracted 归拢 tools/runs/results，数据外迁，实验脚本纳管`

## 八、验证闭环（全绿才算完成）

| # | 验证 | 通过标准 |
| --- | --- | --- |
| 1 | MSVC 重编译 `harness.exe` + `harness_optv2.exe`（README 命令） | 编译零错误，exe 生成 |
| 2 | `py -3 tools/sweep.py --rerank` | 能读 `results/sweep_raw.json` 并输出报告到 `results/` |
| 3 | `py -3 tools/stats.py <外迁的一个 jsonl>` | 聚合输出正常 |
| 4 | runs 冒烟：`run_optv2_ab.py` 缩小 N 跑通 | harness/CFG 相对路径解析正确 |
| 5 | CMake 构建 `native/`（power_split_test、split_vs_power 目标） | 编译链接通过 |
| 6 | `git status` | 仅预期的新跟踪文件与文档改动，无意外 |
| 7 | 全仓 grep 兜底：`extracted[/\\](_run|_verify|_gen|sweep\.py|stats\.py|anchor_check|top20)` | 仅剩历史文档命中 |

## 九、回滚

- git 跟踪操作（mv/add/commit）单一 commit 可 `git revert`。
- 外迁数据原样保留在 `sim-data/`，可整目录移回。
- 删除的 tune420×3 若需找回：实施前将其内容贴入本方案附录留档。

## 十、风险与对策

| 风险 | 对策 |
| --- | --- |
| 路径引用漏改导致脚本失效 | §八.7 全仓 grep 兜底 + 4 项冒烟实测 |
| 未来快照同步误伤新布局 | 本方案只动 analysis 专用资产；`algorithm-snapshot-plan.md` 保留清单同步更新 |
| `--runs-dir` 缺省变化破坏老用法 | 缺省仍指仓库内 `results/sweep_runs`（自动建、被忽略），大扫描才显式传参 |
| 误删仍有价值的脚本 | 12/15 全保留，仅删 tune420×3 且实施前出示原文；结论文档均在 |
