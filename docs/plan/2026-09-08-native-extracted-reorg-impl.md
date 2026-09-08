# native/extracted 目录归拢 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `algorithm/native/extracted/` 五类混放归拢为 源码顶层/tools/runs/results 四区，390MB 数据外迁，实验脚本纳管（方案 B）。

**Architecture:** 已跟踪文件 `git mv`，未跟踪文件移动+改名+`git add`；模拟器源码与 CMake 引用的测试 cpp 留 extracted/ 顶层不动；脚本只做机械路径层级修补（HERE 相对层级 +1）；全程**单个 commit** 收尾。

**Tech Stack:** git、PowerShell 5.1（无 `&&`）、MSVC（vcvarsall+cl `/utf-8`）、Python `py -3 -u`、CMake。

**Spec:** [docs/plan/2026-09-08-native-extracted-reorg.md](2026-09-08-native-extracted-reorg.md)（文件清单/引用清单/验证标准以 spec 为准）

## Global Constraints

- **不碰快照清单文件**：`algorithm/src/`、`pom.xml`、`include/`、`test/`、`native/CMakeLists.txt`、`algorithm/docs/`、`algorithm/README.md`（只读快照，改须回源仓）
- **单 commit**：Task 1-8 全部完成后 Task 9 一次性提交（spec §九 回滚要求），中途不 commit
- **历史文档不改**：`docs/plan/`、`docs/review/`、`docs/spec/` 下带日期文档里的旧路径一律保留
- Python 一律 `py -3 -u`；PowerShell 5.1 用 `;` 和 `if ($LASTEXITCODE -ne 0) {}` 串接
- `extracted/.gitignore` 现有模式（`*.jsonl`、`sweep_runs/`、`sweep_raw.json` 均无前导斜杠）自动覆盖新子目录，勿删
- 未跟踪文件（`_*.py`、`makedeal_pre91.json` 等）不在 git 历史里，**移动前不可逆操作只有 Task 2 的删除，须先过用户确认门**

---

### Task 1: 数据外迁 sweep_runs

**Files:**
- Move: `algorithm/native/extracted/sweep_runs/*.jsonl` ×20 → `D:\analysis\sim-data\landlord-sim\sweep_runs\`
- Delete: 清空后的 `algorithm/native/extracted\sweep_runs\`

**Interfaces:**
- Consumes: 无
- Produces: `D:\analysis\sim-data\landlord-sim\sweep_runs\`（19 个 jsonl，Task 8.3 冒烟用其中 1 个）

- [ ] **Step 1: 目标目录建好并迁移**

```powershell
New-Item -ItemType Directory -Force "D:\analysis\sim-data\landlord-sim\sweep_runs" | Out-Null
Move-Item -Path "algorithm\native\extracted\sweep_runs\*" -Destination "D:\analysis\sim-data\landlord-sim\sweep_runs\"
Remove-Item "algorithm\native\extracted\sweep_runs"
```

- [ ] **Step 2: 验证数量与体积分毫不差**

```powershell
(Get-ChildItem "D:\analysis\sim-data\landlord-sim\sweep_runs\*.jsonl").Count   # 期望 20
"{0:N0} bytes" -f (Get-ChildItem "D:\analysis\sim-data\landlord-sim\sweep_runs\*.jsonl" | Measure-Object Length -Sum).Sum
# 期望 ≈ 413,440,793 bytes（±1MB 内）
Test-Path "algorithm\native\extracted\sweep_runs"   # 期望 False
```

---

### Task 2: 清理编译缓存 + tune420 删除（用户确认门）

**Files:**
- Delete: `algorithm/native/extracted/__pycache__/`（2 个 .pyc）、`algorithm/native/extracted/harness_optv2.obj`
- Delete（须用户确认）: `_run_tune420.py`、`_run_tune420b.py`、`_run_tune420c.py`

**Interfaces:**
- Consumes: 无
- Produces: 无（纯清理）

- [ ] **Step 1: 无确认门的三项直接删**

```powershell
Remove-Item -Recurse -Force -Confirm:$false "algorithm\native\extracted\__pycache__"
Remove-Item -Force -Confirm:$false "algorithm\native\extracted\harness_optv2.obj"
```

- [ ] **Step 2: 【HUMAN GATE】向用户出示 tune420×3 全文，等确认后再删**

把三个文件完整内容贴给用户（合计 ~4.4KB），明确问"确认删除？"。**未获确认前不得执行 Step 3。**

- [ ] **Step 3: 用户确认后删除**

```powershell
Remove-Item -Force -Confirm:$false "algorithm\native\extracted\_run_tune420.py","algorithm\native\extracted\_run_tune420b.py","algorithm\native\extracted\_run_tune420c.py"
Test-Path "algorithm\native\extracted\_run_tune420.py"   # 期望 False
```

---

### Task 3: 目录就位（git mv + 移动改名 + git add）

**Files:**
- Create dirs: `extracted/tools/`、`extracted/runs/`、`extracted/results/`、`native/tools/`
- git mv: `sweep.py`/`stats.py`/`anchor_check.py`→`tools/`；`top20_*.{json,md}`×5→`results/`
- Move+add: `harness_optv2.cpp`（顶层不动，add）；`_gen_optv2_harness.py`→`gen_optv2_harness.py`；`_run_*`×8、`_verify_*`×3→`runs/` 去 `_` 前缀；`makedeal_pre91.json`、`sweep_raw.json`→`results/`；`shuffle_prng_compare*`→`native/tools/`

**Interfaces:**
- Consumes: Task 1/2 已清场
- Produces: Task 4/5 修补所需的全部文件新路径

- [ ] **Step 1: git mv 已跟踪 8 文件**

```powershell
New-Item -ItemType Directory -Force "algorithm\native\extracted\tools","algorithm\native\extracted\runs","algorithm\native\extracted\results","algorithm\native\tools" | Out-Null
git mv algorithm/native/extracted/sweep.py algorithm/native/extracted/tools/sweep.py
git mv algorithm/native/extracted/stats.py algorithm/native/extracted/tools/stats.py
git mv algorithm/native/extracted/anchor_check.py algorithm/native/extracted/tools/anchor_check.py
git mv algorithm/native/extracted/top20_configs.json algorithm/native/extracted/results/top20_configs.json
git mv algorithm/native/extracted/top20_configs_real.json algorithm/native/extracted/results/top20_configs_real.json
git mv algorithm/native/extracted/top20_report.md algorithm/native/extracted/results/top20_report.md
git mv algorithm/native/extracted/top20_report_real.md algorithm/native/extracted/results/top20_report_real.md
git mv algorithm/native/extracted/top20_compare.md algorithm/native/extracted/results/top20_compare.md
```

- [ ] **Step 2: 移动+改名未跟踪脚本（12 个进 runs/，gen 留顶层）**

```powershell
# 注意：未跟踪文件不能用 git mv（报 not under version control），一律 Move-Item
Move-Item "algorithm\native\extracted\_gen_optv2_harness.py" "algorithm\native\extracted\gen_optv2_harness.py"
Move-Item "algorithm\native\extracted\_run_optv2_ab.py" "algorithm\native\extracted\runs\run_optv2_ab.py"
Move-Item "algorithm\native\extracted\_run_new4.py" "algorithm\native\extracted\runs\run_new4.py"
Move-Item "algorithm\native\extracted\_run_new4_both.py" "algorithm\native\extracted\runs\run_new4_both.py"
Move-Item "algorithm\native\extracted\_run_new4_seats.py" "algorithm\native\extracted\runs\run_new4_seats.py"
Move-Item "algorithm\native\extracted\_run_new4_strat_seats.py" "algorithm\native\extracted\runs\run_new4_strat_seats.py"
Move-Item "algorithm\native\extracted\_run_era_sim.py" "algorithm\native\extracted\runs\run_era_sim.py"
Move-Item "algorithm\native\extracted\_run_purerand_seeds.py" "algorithm\native\extracted\runs\run_purerand_seeds.py"
Move-Item "algorithm\native\extracted\_run_mix13.py" "algorithm\native\extracted\runs\run_mix13.py"
Move-Item "algorithm\native\extracted\_verify_budget_bug.py" "algorithm\native\extracted\runs\verify_budget_bug.py"
Move-Item "algorithm\native\extracted\_verify_budget_sensitivity.py" "algorithm\native\extracted\runs\verify_budget_sensitivity.py"
Move-Item "algorithm\native\extracted\_verify_extreme_hands.py" "algorithm\native\extracted\runs\verify_extreme_hands.py"
```

- [ ] **Step 3: 其余未跟踪文件归位 + git add**

```powershell
Move-Item "algorithm\native\extracted\makedeal_pre91.json" "algorithm\native\extracted\results\makedeal_pre91.json"
Move-Item "algorithm\native\extracted\sweep_raw.json" "algorithm\native\extracted\results\sweep_raw.json"
Move-Item "algorithm\native\shuffle_prng_compare.py" "algorithm\native\tools\shuffle_prng_compare.py"
Move-Item "algorithm\native\shuffle_prng_compare_README.md" "algorithm\native\tools\shuffle_prng_compare_README.md"
# sweep_raw.json 被 .gitignore 挡住，add 时不会被纳入（预期行为，勿 -f）
git add algorithm/native/extracted/harness_optv2.cpp algorithm/native/extracted/gen_optv2_harness.py algorithm/native/extracted/runs/ algorithm/native/extracted/results/makedeal_pre91.json algorithm/native/tools/
```

- [ ] **Step 4: 布局核验**

```powershell
git status --short -- algorithm/native/
# 期望：R（renamed）8 条 tracked；A：harness_optv2.cpp、gen_optv2_harness.py、runs/ 12 个、makedeal_pre91.json、native/tools/ 2 个；无 U（未合并）、无意外 D
Get-ChildItem "algorithm\native\extracted" -File | Select-Object -ExpandProperty Name
# 期望：.gitignore, README.md, gen_optv2_harness.py, harness.cpp, harness.exe, harness_optv2.cpp, harness_optv2.exe, optimal_split.h, optimal_split_power.h, power_split_test.cpp, split_test.cpp, split_test.exe, verify_split_vs_power.cpp
```

---

### Task 4: sweep.py 路径改造 + --runs-dir 参数化

**Files:**
- Modify: `algorithm/native/extracted/tools/sweep.py:23-27`（路径块）、`:397-405`（argparse）、`:410-411`（REPORT/OUTJSON）、`:561`（compare 输出）

**Interfaces:**
- Consumes: Task 3 的新路径布局
- Produces: `SIM`/`RESULTS` 常量与 `--runs-dir`/`SWEEP_RUNS_DIR`，runs/ 脚本 import 的 `parse_metrics`/`run_one` 不变（签名不动）

- [ ] **Step 1: 替换 L23-27 路径块**

```python
# 旧（L23-27）
HERE = Path(__file__).resolve().parent
HARNESS = HERE / "harness.exe"
CFG = HERE.parent / "previous" / "makedeal.json"
RUNDIR = HERE / "sweep_runs"
RAW = HERE / "sweep_raw.json"

# 新
HERE = Path(__file__).resolve().parent      # extracted/tools/
SIM = HERE.parent                            # extracted/（模拟器与源码所在）
RESULTS = SIM / "results"                    # 结论产物与缓存
HARNESS = SIM / "harness.exe"
CFG = SIM.parent / "previous" / "makedeal.json"
RAW = RESULTS / "sweep_raw.json"
RUNDIR = None  # main() 里按 --runs-dir > SWEEP_RUNS_DIR > RESULTS/"sweep_runs" 解析
```

- [ ] **Step 2: argparse 增 --runs-dir（插在 `--rerank` 一行之前）**

```python
    ap.add_argument("--runs-dir", default=None,
                    help="jsonl 样本输出目录；缺省读环境变量 SWEEP_RUNS_DIR，再缺省 results/sweep_runs")
```

main() 解析参数后、首次使用 RUNDIR 前加：

```python
    RUNDIR = Path(args.runs_dir or os.environ.get("SWEEP_RUNS_DIR") or (RESULTS / "sweep_runs"))
```

> 注意：`RUNDIR` 在模块级已改 None，main() 里需 `global RUNDIR` 或直接用局部变量——**采用局部变量方案**：把 main() 内对 RUNDIR 的引用统一改用局部 `rundir`（grep `RUNDIR` 找全引用点，逐个改），避免 global。

- [ ] **Step 3: 输出路径改 RESULTS**

```python
# 旧 L410-411
REPORT = HERE / f"top20_report{suffix}.md"
OUTJSON = HERE / f"top20_configs{suffix}.json"
# 新
REPORT = RESULTS / f"top20_report{suffix}.md"
OUTJSON = RESULTS / f"top20_configs{suffix}.json"

# 旧 L561
open(HERE / "top20_compare.md", "w", encoding="utf-8").write("".join(cmp))
print(f"对照: {HERE / 'top20_compare.md'}", file=sys.stderr)
# 新
open(RESULTS / "top20_compare.md", "w", encoding="utf-8").write("".join(cmp))
print(f"对照: {RESULTS / 'top20_compare.md'}", file=sys.stderr)
```

- [ ] **Step 4: RUNDIR.mkdir 处确认**（如代码里有 `RUNDIR.mkdir(...)`，保留但作用于解析后的 rundir；grep `RUNDIR` 列出全部引用，逐一处理，不许遗漏）

- [ ] **Step 5: 语法冒烟**

```powershell
py -3 -m py_compile algorithm\native\extracted\tools\sweep.py ; if ($LASTEXITCODE -eq 0) { Write-Output "syntax OK" }
```

---

### Task 5: runs/ 12 脚本路径修补

**Files:** `algorithm/native/extracted/runs/*.py` ×12

**Interfaces:**
- Consumes: sweep.py 在 `../tools/`（Task 4 后 import 可用）
- Produces: 全部脚本从 runs/ 内直接运行可找到 harness/CFG/sweep

**统一修补配方**（每文件按命中模式套用）：

```python
# ① CFG 层级（8 个文件命中）
# 旧
CFG = HERE.parent / "previous" / "makedeal.json"
# 新
CFG = HERE.parent.parent / "previous" / "makedeal.json"

# ② harness 调用层级（凡 HERE / "harness*.exe" 处）
# 旧
str(HERE / "harness.exe")        # 或 str(HERE / exe)
# 新
str(HERE.parent / "harness.exe") # 或 str(HERE.parent / exe)

# ③ sweep import（4 个文件命中：run_optv2_ab / run_new4 / run_new4_both / run_mix13）
# 旧
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# 新
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
```

**各文件命中清单**（先跑 `Select-String -Pattern 'HERE|harness|sweep'` 核对再改）：

| 文件 | ①CFG | ②harness | ③sweep import |
| --- | --- | --- | --- |
| run_optv2_ab.py | ✓ | ✓（`HERE/exe` ×2 处调用+输出文件名里的 `HERE/f"ab_*"` 保留在 runs/ 输出） | ✓ |
| run_new4.py | — | — | ✓ |
| run_new4_both.py | ✓ | ✓（`HERE/exe`） | ✓ |
| run_new4_seats.py | ✓ | ✓ | — |
| run_new4_strat_seats.py | ✓ | ✓ | — |
| run_era_sim.py | — | ✓ | —（sys.path.insert 行保留无害或按③统一改） |
| run_purerand_seeds.py | ✓ | ✓ | — |
| run_mix13.py | — | — | ✓ |
| verify_budget_bug.py | 待 grep | 待 grep | 待 grep |
| verify_budget_sensitivity.py | 待 grep | 待 grep | 待 grep |
| verify_extreme_hands.py | 待 grep | 待 grep | 待 grep |

> "待 grep"：Step 1 先对 3 个 verify 脚本跑 `Select-String -Pattern 'HERE|harness|previous|sweep'`，命中哪条套哪条配方，全不命中则零改动。

- [ ] **Step 1: 逐文件 grep 核对命中 → 按配方修改 12 文件**
- [ ] **Step 2: 输出文件位置说明**——脚本产出（`*.jsonl`/`*.log`）落 runs/ 本目录，被 `*.jsonl`/`*.log` 忽略规则覆盖，不改
- [ ] **Step 3: 全部语法冒烟**

```powershell
Get-ChildItem "algorithm\native\extracted\runs\*.py" | ForEach-Object { py -3 -m py_compile $_.FullName ; if ($LASTEXITCODE -ne 0) { Write-Output "FAIL: $_" } } ; Write-Output "compile sweep done"
```

---

### Task 6: README 两份

**Files:**
- Modify: `algorithm/native/extracted/README.md`（局部改，不重写全文）
- Create: `algorithm/native/extracted/runs/README.md`

**Interfaces:** 无代码接口；文案路径须与 Task 3/4/5 后的实际布局一致

- [ ] **Step 1: extracted/README.md 局部更新**
  - 「## 文件」表格替换为四区导航表：顶层（harness.cpp/harness_optv2.cpp/gen_optv2_harness.py/optimal_split*/测试 cpp）、`tools/`（sweep/stats/anchor_check）、`runs/`（实验脚本+其 README）、`results/`（top20×5、sweep_raw.json 缓存、makedeal_pre91.json）
  - `makedeal.json` 行改为：`--cfg ../previous/makedeal.json` 不变，注明 previous/ 为线上参照副本
  - 「## 聚合统计」命令改 `py -3 tools/stats.py ...`；文末补一行：`sweep.py 大样本输出建议 --runs-dir D:\analysis\sim-data\landlord-sim\sweep_runs（或设 SWEEP_RUNS_DIR）`
  - 忠实度/审计/标定结论各节原样保留
- [ ] **Step 2: 新建 runs/README.md**

```markdown
# runs/ — 一次性实验与验证脚本

多为 2026-08/09 发牌调参期间驱动 `../tools/sweep.py` 与顶层 harness 的实验入口，
结论已沉淀进报告（见各行「结论去向」），脚本保留作证据链与复现入口。

| 脚本 | 用途 | 结论去向 |
|---|---|---|
| run_optv2_ab.py | 原版 vs 优化版 harness 同 seed A/B | docs/analysis/result/new4-*-0901-report.md；memory: harness-optv2 |
| run_new4.py / _both / _seats / _strat_seats | new4 配置理论分布（单/双模拟器/座位视角） | 同上 |
| run_era_sim.py | pre-9.1 vs post-9.1 配置时代模拟（输入 ../results/makedeal_pre91.json） | 9.1 突变归因分析 |
| run_purerand_seeds.py | 纯随机多 seed 基线 | sweep 基线对照 |
| run_mix13.py | 混 13 炸码对照 | 低等级策略结论 |
| verify_budget_bug.py / verify_budget_sensitivity.py | 做牌预算 bug 修复验证 / 敏感性 | 预算修复验证记录 |
| verify_extreme_hands.py | 极端手牌边界验证 | 拆牌边界结论 |

运行方式：在 runs/ 内 `py -3 -u <脚本>`（路径按 ../ 层级已配好）。
```

- [ ] **Step 3: gen_optv2_harness.py 文档串核对**——脚本头 docstring 若提及输出路径，确认仍为 `extracted/harness_optv2.cpp`（不变则零改动）

---

### Task 7: .gitignore + 活文档路径修补

**Files:**
- Modify: `algorithm/native/extracted/.gitignore`（仅注释）
- Modify: 7 个活文档（下表精确行）

**Interfaces:** 无

- [ ] **Step 1: .gitignore 注释补一行**（模式不动）：`# sweep_runs/ 缺省落 results/ 子目录，模式无前导斜杠自动覆盖`

- [ ] **Step 2: 活文档逐文件改**（全部为 `extracted/xxx` → `extracted/tools/xxx` 或 `extracted/results/xxx`；`extracted/harness*.exe` 引用**不变**）

| 文件 | 改动 |
| --- | --- |
| docs/knowledge/makedeal-simulation.md | L48 `anchor_check.py` → `extracted/tools/anchor_check.py`；L51-52 两条 `sweep.py` 命令 → `extracted/tools/sweep.py`；L56 产物 `top20_report.md`/`top20_configs.json`/`sweep_raw.json` → `extracted/results/`；L99-103 资产表 harness.cpp 不变、sweep/anchor_check/top20 行 → tools/、results/ |
| docs/knowledge/makedeal-evaluation-sop.md | L5、L196 `sweep.py` 路径 → `extracted/tools/sweep.py` |
| docs/makedeal-strategies/README.md | L4、L40 `top20_report.md` 链接 → `../../algorithm/native/extracted/results/top20_report.md`；L60-72 命令块 harness 路径不变、L71 `anchor_check.py` → `extracted/tools/`、L72 `sweep.py` → `extracted/tools/sweep.py` |
| docs/makedeal-strategies/classic/420.md | L56 链接 → `.../extracted/results/top20_report.md` |
| docs/makedeal-strategies/classic/742.md | L68 链接 → `.../extracted/results/top20_report.md` |
| docs/makedeal-strategies/_template.md | L46 `anchor_check.py` → tools/；L52 top20 链接 → results/ |
| docs/tech/algorithm-snapshot-plan.md | L96 `shuffle_prng_compare*` 后注 `（2026-09-08 起位于 native/tools/）`；L37 树同步 |

---

### Task 8: 验证闭环（spec §八 全绿才进 Task 9）

- [ ] **8.1 重编译双 harness**（源码未动，编译必须过）

```powershell
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
Set-Location algorithm\native\extracted
cmd /c "`"$vcvars`" x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Feharness.exe harness.cpp"
cmd /c "`"$vcvars`" x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Feharness_optv2.exe harness_optv2.cpp"
Set-Location ..\..\..
```

- [ ] **8.2 sweep --rerank**：`py -3 -u algorithm\native\extracted\tools\sweep.py --rerank` → 读到 `results/sweep_raw.json`、报告写回 `results/`（对比 rerank 前后 top20_report.md 头部一致）
- [ ] **8.3 stats 冒烟**：`py -3 -u algorithm\native\extracted\tools\stats.py D:\analysis\sim-data\landlord-sim\sweep_runs\old2_n20000.jsonl` 正常聚合
- [ ] **8.4 runs 冒烟**：`Set-Location algorithm\native\extracted\runs; py -3 -u run_optv2_ab.py 200 7; Set-Location ..\..\..\..`（小 N 跑通、无路径报错后回仓库根）
- [ ] **8.5 CMake**：`native/` 下构建 power_split_test、split_vs_power 目标通过
- [ ] **8.6 git status**：仅预期 R/A/M，无意外
- [ ] **8.7 grep 兜底**：全仓 `rg "extracted[/\\](_run|_verify|_gen|sweep\.py|stats\.py|anchor_check|top20_(report|configs|compare))"` 命中仅剩 docs/plan、docs/review、docs/spec 历史文档

---

### Task 9: 单 commit

- [ ] **Step 1: 复核暂存集**

```powershell
git status --short
# 期望且仅期望：algorithm/native/extracted 与 algorithm/native/tools 下 R×8 + A×15 + M（README、.gitignore）+ docs/ 下 7 个 M
# ⚠ previous/ 的 3 个 M（makedeal.json、MakeDealHelper×2，线上同步遗留）不得进入本次 commit
```

- [ ] **Step 2: 提交（路径点名，勿用 -A algorithm/native 以免误收 previous/）**

```powershell
git add -A algorithm/native/extracted algorithm/native/tools
git add docs/knowledge/makedeal-simulation.md docs/knowledge/makedeal-evaluation-sop.md docs/makedeal-strategies/README.md docs/makedeal-strategies/classic/420.md docs/makedeal-strategies/classic/742.md docs/makedeal-strategies/_template.md docs/tech/algorithm-snapshot-plan.md
git commit -m "refactor(native): extracted 归拢 tools/runs/results 四区，实验脚本纳管，413MB 样本外迁 sim-data"
```

- [ ] **Step 3: push 并回报**：`git push origin main`，向用户汇报验证闭环结果摘要
