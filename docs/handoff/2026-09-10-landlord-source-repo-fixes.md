# Handoff：landlord 源仓落地文档纠错 + 四项清理（Mac 侧执行）

> **已完成并停用**：源仓修复已提交为 `b729ef0` 并推送；算法随后合并到 `landlord-analysis/algorithm/`。本指引仅保留历史，不再执行。

> 写给：Mac 上的操作者（人或 Claude）。
> 来源：Windows 侧 2026-09-09/10 对 `landlord-analysis` 仓 `algorithm/docs/` 快照的代码级审计，已在本仓（landlord-analysis）完成文档修复与修改包，**剩余动作须在权威源仓执行**。2026-09-10 外部复审 4 条意见已采纳并入本指引（详见修改包文档"复审修订记录"）。
> 详细背景与逐项说明：`docs/tech/algorithm-snapshot-upstream-fixes.md`（本仓）。

## 背景速览（30 秒）

1. 审计发现源仓 3 份文档与代码不符（`split-strategy-decision-rules.md` 旧类名/"只跑一套"架构过期/置信度结论错误、`testing-strategy-and-commands.md` 漏 5 个测试且向量归属写错、`deal-balancing-prd.md` §六配置示例值与实际不符）+ 4 项代码侧小清理（Javadoc 旧类名、测试类包迁移**并改名**、删 1 行的 `docs/prompt.md`、置信度 Javadoc 更正）。
2. 文档修复已在 analysis 仓快照区例外完成（analysis commits `9a84ab4` + `9d05210`），**同名修复须在源仓落地**，之后快照同步时两边收敛。
3. 源仓拓扑（2026-09-10 探明）：**本机（Mac）`/Users/maerun/Projects/landlord` 是权威库**；GitHub `Samuel86-star/landlord` main 停在 `e4be61a`，落后一批未推送提交（`e4be61a..f2dbcf6`：三带自吃修复、`SplitterRegressionTest`、product-prd 大改、landlord.h 变更等）。**勿从别处 clone 落地**，会分叉。

## 操作对象

- 权威库：`/Users/maerun/Projects/landlord`（预期 main ≈ `f2dbcf6`，2026-09-06 后的状态）
- 材料：patch 文件在 landlord-analysis 仓 `docs/tech/algorithm-snapshot-upstream-fixes.diff`（git 原生输出，需拉取本仓；附录说明为何不重复内嵌）

## 步骤

> 顺序原则：**全部修改与验证完成之前不 push**——积压提交含 Java/C++ 行为变更，与本次修复合并单次推送。

### Step 0 · 自检 + 防分叉确认

```bash
cd /Users/maerun/Projects/landlord
git status --short          # 应干净；有未提交内容先处理
git log --oneline -5        # 应见 f2dbcf6（或更新的提交）
git fetch origin
git log --oneline origin/main -1      # 应为 e4be61a
git log --oneline origin/main..main   # 应列出 6 个积压提交
```

若 `origin/main` 已不是 `e4be61a`（有人推过/分叉）、或 main 与预期状态不符——**停下，把输出带回来讨论**，勿继续。

若 main 已超过 f2dbcf6 且改过 `docs/` 下三份文档（split-strategy-decision-rules / testing-strategy-and-commands / deal-balancing-prd），Step 1 的 `--check` 可能失败——改用修改包文档里的内嵌 diff 手动合。

### Step 1 · A 组：文档纠错 patch

```bash
# Mac 上克隆/更新 landlord-analysis（git@github.com:Samuel86-star/landlord-analysis.git）：
git -C <landlord-analysis路径> pull

# 干跑校验后应用（-p2 剥掉路径里的 algorithm/ 前缀对齐源仓布局）：
git apply --check -p2 <landlord-analysis路径>/docs/tech/algorithm-snapshot-upstream-fixes.diff && \
git apply -p2 <landlord-analysis路径>/docs/tech/algorithm-snapshot-upstream-fixes.diff
```

若手头暂无 landlord-analysis 克隆：`git clone git@github.com:Samuel86-star/landlord-analysis.git`（浅拉即可），或从任一已同步机器取那份 `.diff` 单文件。

### Step 2 · B1：Javadoc 旧类名（`AbstractHandSplitter.java` 约 15 行）

```bash
f=src/main/java/com/mamba/landlord/algorithm/splitter/AbstractHandSplitter.java
sed -i '' 's/PlaneBombFirstSplitAlgorithm、StraightFirstSplitAlgorithm/PlaneBombPrioritizedSplitter、StraightPrioritizedSplitter/' "$f"
```

### Step 3 · B2：测试类包迁移 + 改名（必做）

```bash
git mv src/test/java/com/mamba/landlord/algorithm/split/SplitterAlgorithmFactoryTest.java \
       src/test/java/com/mamba/landlord/algorithm/splitter/DefaultSplitterFactoryTest.java
f=src/test/java/com/mamba/landlord/algorithm/splitter/DefaultSplitterFactoryTest.java
sed -i '' 's/^package com\.mamba\.landlord\.algorithm\.split;/package com.mamba.landlord.algorithm.splitter;/' "$f"
sed -i '' '/^import com\.mamba\.landlord\.algorithm\.splitter\.DefaultSplitterFactory;$/d' "$f"
sed -i '' 's/class SplitterAlgorithmFactoryTest/class DefaultSplitterFactoryTest/' "$f"
grep -n "SplitterAlgorithmFactoryTest" "$f"   # 应零输出（有残留逐处手改）
```

改名是必做项（非可选）：旧类名指早已不存在的 `SplitterAlgorithmFactory`，保留会使 Step 5 的旧名 grep 永远命中。

### Step 4 · B3 + B4：删 prompt.md + 置信度 Javadoc 更正

```bash
git rm docs/prompt.md

f=src/main/java/com/mamba/landlord/algorithm/splitter/DefaultSplitterFactory.java
sed -i '' 's/高置信度时（前两条规则命中）可安全使用单路径，否则建议双路径取优。/该标记为历史启发式残留：生产评分主路径始终双路径取优、不据此短路；高置信不代表单路径与双路径结果等价。/' "$f"
```

B4 背景：该 Javadoc 声称高置信可安全单路径，与 a4b53b9"移除错误的高置信单路径短路"矛盾（反例：`SplitterRegressionTest.strongStraightStillComparesBombPreservingSplit`——强顺子高置信，单路径即丢 `BOMB(8)`）。

### Step 5 · 全量验证（push 前置门）

```bash
mvn test          # 或 ./mvnw test；全绿

# Native（landlord.h 孪生回归；源仓 CMakeLists 目标为 landlord_test / landlord_sampler）：
cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release && \
cmake --build native/build && \
ctest --test-dir native/build

# 旧名/错误结论零残留（git 历史除外）：
grep -rn "FirstSplitAlgorithm\|SplitterAlgorithmFactory\|buildRankCountsFromHand\|可安全使用单路径" docs/ src/ \
  && echo "尚有残留，检查上面输出" || echo CLEAN
```

任何一步失败：停下修复或带报错回来，勿推送。

### Step 6 · 提交并单次推送

```bash
git add -A
git commit -m "docs: fix stale splitter class names, test coverage list, PRD config note; chore: move+rename factory test to splitter package, drop prompt.md, fix confidence javadoc"
git push origin main      # 单次推送：积压 e4be61a..f2dbcf6 + 本次修复合并清账
```

> 备选：若希望先验证再分开推，也可在 Step 0 确认后先跑 Step 5 的 mvn+ctest 验证积压提交、单独 `git push origin main`，再做本次修改与第二次推送——多一轮验证，结果等价。

## 完成后

回到 Windows 侧 landlord-analysis 仓：按 `docs/tech/algorithm-snapshot-plan.md` §更新方法 做快照增量同步（base=f2dbcf6，head=本次提交），A 组三份文档会与本仓 `9a84ab4`+`9d05210` 的例外修复内容收敛，B 组首次进入快照。也可以直接在 Windows 开个 Claude 会话说"按 handoff 同步 algorithm 快照"。

## 注意

- **勿用** Windows 机上的两个克隆落地：`D:\Coding\landlord`（对照用）、`D:\Coding\landlord-algorithm`（无关残骸）。
- patch 基线 = f2dbcf6 时的三份文档（2026-09-10 复审修订版，含置信度/向量归属两处更正）；打不上就手动合（修改包文档内嵌了完整 diff 与逐项说明）。
- B2 改名后 `mvn test -Dtest='DefaultSplitterFactoryTest'` 可单跑该类；A2 文档行已用新名，无需二次同步。

## 附录：为何不内嵌 patch 全文

patch 唯一正文 = landlord-analysis 仓 `docs/tech/algorithm-snapshot-upstream-fixes.diff`（同仓 `docs/tech/algorithm-snapshot-upstream-fixes.md` §A 组也内嵌了同一份 diff，打不上时可照着手动合）。为避免多份拷贝漂移，本 handoff 不再复制第三份。
