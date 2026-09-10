# Handoff：landlord 源仓落地文档纠错 + 三项清理（Mac 侧执行）

> 写给：Mac 上的操作者（人或 Claude）。
> 来源：Windows 侧 2026-09-09/10 对 `landlord-analysis` 仓 `algorithm/docs/` 快照的代码级审计，已在本仓（landlord-analysis）完成文档修复与修改包，**剩余动作须在权威源仓执行**。
> 详细背景与逐项说明：`docs/tech/algorithm-snapshot-upstream-fixes.md`（本仓）。

## 背景速览（30 秒）

1. 审计发现源仓 3 份文档与代码不符（`split-strategy-decision-rules.md` 旧类名/"只跑一套"架构过期、`testing-strategy-and-commands.md` 漏 5 个测试、`deal-balancing-prd.md` §六配置示例值与实际不符）+ 3 项代码侧小清理（Javadoc 旧类名、测试类包名 `split`→`splitter`、删 1 行的 `docs/prompt.md`）。
2. 文档修复已在 analysis 仓快照区例外完成（analysis commit `9a84ab4`），**同名修复须在源仓落地**，之后快照同步时两边收敛。
3. 源仓拓扑（2026-09-10 探明）：**本机（Mac）`/Users/maerun/Projects/landlord` 是权威库**；GitHub `Samuel86-star/landlord` main 停在 `e4be61a`，落后一批未推送提交（`e4be61a..f2dbcf6`：三带自吃修复、`SplitterRegressionTest`、product-prd 大改、landlord.h 变更等）。**勿从别处 clone 落地**，会分叉。

## 操作对象

- 权威库：`/Users/maerun/Projects/landlord`（预期 main ≈ `f2dbcf6`，2026-09-06 后的状态）
- 材料：patch 文件在 landlord-analysis 仓 `docs/tech/algorithm-snapshot-upstream-fixes.diff`（git 原生输出，需拉取本仓；附录说明为何不重复内嵌）

## 步骤

### Step 0 · 自检

```bash
cd /Users/maerun/Projects/landlord
git status --short        # 应干净；有未提交内容先处理
git log --oneline -5      # 应见 f2dbcf6（或更新的提交）
```

若 main 已超过 f2dbcf6 且改过 `docs/` 下三份文档（split-strategy-decision-rules / testing-strategy-and-commands / deal-balancing-prd），Step 2 的 `--check` 可能失败——改用修改包文档里的逐条手动说明。

### Step 1 · 清 GitHub 欠账（推荐先做，独立一步）

```bash
git push origin main      # 把积压的 e4be61a..f2dbcf6 一批推上 GitHub
```

### Step 2 · A 组：文档纠错 patch

```bash
# Mac 上克隆/更新 landlord-analysis（git@github.com:Samuel86-star/landlord-analysis.git）：
git -C <landlord-analysis路径> pull

# 干跑校验后应用（-p2 剥掉路径里的 algorithm/ 前缀对齐源仓布局）：
git apply --check -p2 <landlord-analysis路径>/docs/tech/algorithm-snapshot-upstream-fixes.diff && \
git apply -p2 <landlord-analysis路径>/docs/tech/algorithm-snapshot-upstream-fixes.diff
```

若手头暂无 landlord-analysis 克隆：`git clone git@github.com:Samuel86-star/landlord-analysis.git`（浅拉即可），或从任一已同步机器取那份 `.diff` 单文件。

### Step 3 · B1：Javadoc 旧类名（`AbstractHandSplitter.java` 约 15 行）

```bash
f=src/main/java/com/mamba/landlord/algorithm/splitter/AbstractHandSplitter.java
sed -i '' 's/PlaneBombFirstSplitAlgorithm、StraightFirstSplitAlgorithm/PlaneBombPrioritizedSplitter、StraightPrioritizedSplitter/' "$f"
```

### Step 4 · B2：测试类包迁移（已核实全仓无引用，自包含）

```bash
git mv src/test/java/com/mamba/landlord/algorithm/split/SplitterAlgorithmFactoryTest.java \
       src/test/java/com/mamba/landlord/algorithm/splitter/SplitterAlgorithmFactoryTest.java
f=src/test/java/com/mamba/landlord/algorithm/splitter/SplitterAlgorithmFactoryTest.java
sed -i '' 's/^package com\.mamba\.landlord\.algorithm\.split;/package com.mamba.landlord.algorithm.splitter;/' "$f"
sed -i '' '/^import com\.mamba\.landlord\.algorithm\.splitter\.DefaultSplitterFactory;$/d' "$f"
```

可选：类名 `SplitterAlgorithmFactoryTest` 是旧工厂名遗留，可顺手改 `DefaultSplitterFactoryTest`（改的话 Step 2 打进去的 `testing-strategy-and-commands.md` 里该类名行同步改）。

### Step 5 · B3：删除非文档文件

```bash
git rm docs/prompt.md
```

### Step 6 · 验证

```bash
mvn test    # 或 ./mvnw test；全绿
grep -rn "FirstSplitAlgorithm\|SplitterAlgorithmFactory\|buildRankCountsFromHand" docs/ src/ \
  && echo "尚有残留，检查上面输出" || echo CLEAN
```

### Step 7 · 提交并推送

```bash
git add -A
git commit -m "docs: fix stale splitter class names, test coverage list, PRD config note; chore: move factory test to splitter package, drop prompt.md"
git push origin main
```

## 完成后

回到 Windows 侧 landlord-analysis 仓：按 `docs/tech/algorithm-snapshot-plan.md` §更新方法 做快照增量同步（base=f2dbcf6，head=本次提交），A 组三份文档会与本仓 `9a84ab4` 的例外修复内容收敛，B 组首次进入快照。也可以直接在 Windows 开个 Claude 会话说"按 handoff 同步 algorithm 快照"。

## 注意

- **勿用** Windows 机上的两个克隆落地：`D:\Coding\landlord`（对照用）、`D:\Coding\landlord-algorithm`（无关残骸）。
- patch 基线 = f2dbcf6 时的三份文档；打不上就手动合（修改包文档 `docs/tech/algorithm-snapshot-upstream-fixes.md` 内嵌了完整 diff 与逐项说明）。

## 附录：为何不内嵌 patch 全文

patch 唯一正文 = landlord-analysis 仓 `docs/tech/algorithm-snapshot-upstream-fixes.diff`（同仓 `docs/tech/algorithm-snapshot-upstream-fixes.md` §A 组也内嵌了同一份 diff，打不上时可照着手动合）。为避免多份拷贝漂移，本 handoff 不再复制第三份。
