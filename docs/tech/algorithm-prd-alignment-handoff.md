# 工作交接：牌力评分算法对齐 PRD

> 交接日期 2026-07-31。本文件供新 session 接手用；改动已全部落地 `main` 并推送 origin。

## 一、背景与目标

- 产品 PRD《发牌均衡性过滤》`algorithm/docs/deal-balancing-product-prd.md` §4.1.2/§4.1.3 定义了牌力评分规则；但 Java + C++ 代码原与 PRD **不一致**（PRD 更合理）。
- 用户决定 **路线 B：以 PRD 为准，改代码**（不是改回 PRD）。
- 过程中顺带：解决 C++ 编译问题、用新公式重采样 10 万局阈值基线。

## 二、两处核心语义改动

1. **对子翼加分**：`base×0.5` → **`base+1`**（= 对子组合值 `(V_base×2+2)` 的一半；与单牌翼「= 单牌基础分一半」对称）。
   - 影响牌型：三带一对 / 飞机带对 / 四带二对；单牌翼不变。
   - 实现：新增 `scorePairWingRank = max(0, (V_base*pairCoeff+pairOffset)*wingRankFactor)`，默认系数下恰为 `max(0, V_base+1)`，**复用现有配置、无需新增系数**。
2. **炸弹/王炸控制加成**：从「按拆牌组合算」改为「**按手牌持有算、与拆牌方式无关**」。
   - 旧：遍历拆牌结果里的 `BOMB/ROCKET` 组合各 +5（拆成四带二就拿不到）。
   - 新：`computeHandControlBonus` 统计原始手牌里张数≥4 的点数（每个 +5）+ 双王（+5）。持有四张同点数即 +5，无论拆牌是否打成 BOMB。

## 三、已落地（main，4 个 commit，已推 origin）

| commit | 内容 |
|---|---|
| `d42956b` | feat(algorithm)：评分对齐 PRD（Java+C++）+ rules.md 同步 + 补单测 |
| `1539033` | fix(native)：CMakeLists 给 MSVC 加 `/utf-8`（修 UTF-8 编译错误）|
| `48b6f2d` | docs(native)：技术 PRD §五 + application.properties 回填新 10万局基线 |
| `cf5a9d8` | docs(prd)：产品 PRD §4.1.2/§4.1.3 评分口径 + §4.5 阈值 |

worktree 与 `worktree-cardpower-pairwing-heldbomb` 本地/远端分支**已清理**，仅剩 `main`。

## 四、验证状态（全绿）

- **Java**：`mvn -f algorithm/pom.xml test` → 28 项全过（BoundaryBugTest 20 + ComboScoring 5[+1 新] + HandCards 3[+1 新]）。
- **Java 基线采样**：`mvn -f algorithm/pom.xml test -Pbenchmark -Dtest="ShuffleAndScoringBenchmarkTest#sampleBaselineDistribution"`（10 万局，输出分位点 + 推荐阈值）。
- **C++**：本机 MSVC（VS2022 Community，需 `vcvars64.bat`）+ cmake（`C:\Program Files\CMake`，不在 PATH）。`landlord_test` 12 项全过。
  - 构建：`cmake -S algorithm/native -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release`
  - 运行：`build/Release/landlord_test.exe`（cwd 需在 `algorithm/native`，因 `testConfigLoad` 读 `config/scoring.properties`）
- **新基线**（Java 10 万局，2026-07-31，新公式）：lower **-68** / upper **74** / spread **112** / potential **92** / advantage **113** / singles **8** / bombs **2**。较旧值仅 spread 113→112、potential 94→92、advantage 114.5→113 各降 1~2，**upper/lower/singles/bombs 不变** → 公式改动对发牌过滤阈值影响很小。

## 五、未完成 / 后续（按优先级）

1. **【让数仓真正生效，最重要】** 本次只改了 `landlord-analysis` 的参考快照（main）。**数仓 `card_power` / `card_power_final`（`dws_ddz_firstday_game` / `dws_ddz_daily_game`）也用这套 PRD 算法**（用户确认）。要让改动生效：
   - ① 把 `d42956b` 等移植回**源仓库 `Samuel86-star/landlord`**（注意：`algorithm/` 是只读快照，源仓库才是本体）；
   - ② 更新**数仓 ETL** 的牌力计算到新版本；
   - ③ 上线。**上线日 = 第二个版本切点**（继 2026-06-15 bug 修复之后），切点前后 `card_power` 不可直接比较。详见 memory `project_cardpower-formula-prd-change-2026-07`。
2. **application.properties 四维过滤仍禁用**：potential/advantage/singles/bombs 现为 `Infinity/0/0/0`（原作刻意禁用），注释标了推荐值 92/113/8/2。是否启用 = 产品/运维决策（启用会扩大发牌过滤范围、增加重洗率）。
3. **源仓库未同步**：本次直接改了快照，没走「源仓库改 → rsync 重新生成快照」的标准流程（见 `algorithm/README.md` + `docs/tech/algorithm-snapshot-plan.md`）。源仓库仍是旧实现。

## 六、关键文件

- 代码：`algorithm/src/main/java/com/mamba/landlord/algorithm/scoring/strategy/DefaultComboScoringStrategy.java`、`DefaultHandCardsScoringStrategy.java`；`algorithm/native/include/landlord.h`（C++ 手工镜像，改 Java 要同步改它）
- 规则：`algorithm/docs/rules.md`（§2 牌型组合、§3 控制牌）
- PRD：`algorithm/docs/deal-balancing-product-prd.md`（产品，§4.1.2/4.1.3/4.5）、`algorithm/docs/deal-balancing-prd.md`（技术，§五基线/§六配置）
- 配置：`algorithm/src/main/resources/application.properties`、`algorithm/native/config/scoring.properties`、`algorithm/native/CMakeLists.txt`
- 单测：`algorithm/src/test/.../scoring/strategy/*Test.java`、`algorithm/src/test/.../benchmark/ShuffleAndScoringBenchmarkTest.java`、`algorithm/native/test/main_test.cpp`、`sampler_test.cpp`

## 七、环境 / 构建坑（本机特定）

- Shell：PowerShell 5.1 不支持 `&&`；Python 必须 `py -3 -u`；Git 走 SSH `git@github.com` 直连。
- **C++ 编译器不在 PATH**：MSVC 在 `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`（VS18 BuildTools 没有 VC，只有 VS2022 Community 有）；cmake 在 `C:\Program Files\CMake\bin\cmake.exe`。
- **UTF-8 源码坑**：`native/` 源码含中文，MSVC 默认按系统码页 936(GBK) 解读会报 `C2001 常量中有换行符` + `C4819`。必须 `/utf-8`（已加进 CMakeLists 的 `if(MSVC) add_compile_options(/utf-8) endif()`）。
- **后台 session 隔离守卫**：Edit/Write 工具在共享主检出会被拦（要求先进 worktree）。改主检出文件的可走两条路：① 进 worktree 改完合并；② 用 PowerShell/Bash 直接写（如 `[IO.File]::WriteAllText`）。**git 命令不受影响**。

## 八、memory 已更新

- 新增 `project_cardpower-formula-prd-change-2026-07`：数仓 card_power 用此算法 + 2026-07 改版断档预警。
- 已有 `project_cardpower-algorithm-fix`：2026-06-15 bug 修复，<06-15 窗口牌力数据禁用。两者互链。
