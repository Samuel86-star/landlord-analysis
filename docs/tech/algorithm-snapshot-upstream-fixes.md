# algorithm 源仓待落地修改包（landlord-algorithm）

> 来源：2026-09-09/10 对快照 `algorithm/docs/` 的代码级审计（对照 `src/`、`application.properties`、`pom.xml`、`CMakeLists.txt`、测试源码）。
> 用途：以下修改**必须在源仓 [Samuel86-star/landlord](https://github.com/Samuel86-star/landlord.git) 落地**，本仓快照只读（治理见 [algorithm-snapshot-plan.md](algorithm-snapshot-plan.md)、[algorithm/README.md](../../algorithm/README.md)）。源仓合入后按快照同步流程带回本仓。
> **Mac 侧执行指引**：[../handoff/2026-09-10-landlord-source-repo-fixes.md](../handoff/2026-09-10-landlord-source-repo-fixes.md)（自包含步骤 0~7，验证前置、单次推送）。
>
> 快照基准：`f2dbcf6`（2026-09-06）。若源仓文件已前进，patch 打不上时按各项"操作说明"手动执行。
>
> **2026-09-10 复审修订**（外部复审 4 条意见全采纳，另据第 1 条衍生新增 B4）：①删除"高置信单路径 ≈ 双路径"错误结论（a4b53b9 已移除该短路，`SplitterRegressionTest` 有高置信反例）；②"双种子校准"归属更正（该表只含确定性向量，双种子标定属 deal-balancing-prd）；③测试类改名由可选转**必做**（否则旧名 grep 永远命中）；④验证前移（先全量验证再推送）+ 补 Native 构建/CTest。快照侧对应修订见 analysis 仓 `9d05210`。

## 总览

| # | 类型 | 文件 | 内容 | 快照侧状态 |
|---|---|---|---|---|
| A1 | 文档纠错 | `docs/split-strategy-decision-rules.md` | 旧类名/方法名 → 现行；单/双路径分工补全；置信度标记定性更正 | 已例外修复（analysis 9a84ab4 + 9d05210），待源仓同名落地 |
| A2 | 文档纠错 | `docs/testing-strategy-and-commands.md` | 覆盖清单补 5 个现存测试；拆牌向量归属更正 | 同上 |
| A3 | 文档纠错 | `docs/deal-balancing-prd.md` | §六标注推荐值 ≠ 现行配置 | 已例外修复（9a84ab4），待源仓同名落地 |
| B1 | 注释纠错 | `src/main/java/com/mamba/landlord/algorithm/splitter/AbstractHandSplitter.java` | Javadoc 旧类名 | 未动（代码区） |
| B2 | 测试整理 | `src/test/java/com/mamba/landlord/algorithm/split/SplitterAlgorithmFactoryTest.java` | 包 `split` → `splitter` 迁移 + 改名 `DefaultSplitterFactoryTest`（**必做**） | 未动（代码区） |
| B3 | 文件清理 | `docs/prompt.md` | 删除（1 行 AI 指令，非文档） | 未动 |
| B4 | 注释纠错 | `src/main/java/com/mamba/landlord/algorithm/splitter/DefaultSplitterFactory.java` 约 80~81 行 | `chooseStrategyWithConfidence` Javadoc"可安全使用单路径"→ 更正为历史启发式残留（复审新增） | 未动（代码区） |

## A 组：文档纠错（diff 可直接应用）

独立 patch 文件：[algorithm-snapshot-upstream-fixes.diff](algorithm-snapshot-upstream-fixes.diff)（git 原生输出，无 BOM，`algorithm/docs/` 路径）。在**源仓（Mac）根目录**执行（`-p2` 剥掉 `algorithm/` 前缀对齐源仓路径）：

```bash
git apply --check -p2 algorithm-snapshot-upstream-fixes.diff && \
git apply -p2 algorithm-snapshot-upstream-fixes.diff
```

> patch 基线 = 快照 `f2dbcf6` 时的 docs（即 analysis 仓 9a84ab4 + 9d05210 修改前状态）。若 Mac 库在 f2dbcf6 之后又改过这三份文档导致打不上，按下方内嵌 diff 手动合入。

内嵌 diff（与 `.diff` 文件**逐字一致**，含 `algorithm/` 路径前缀与 index 行；2026-09-10 复审修订版）：

```diff
diff --git a/algorithm/docs/deal-balancing-prd.md b/algorithm/docs/deal-balancing-prd.md
index 5119438..7e35640 100644
--- a/algorithm/docs/deal-balancing-prd.md
+++ b/algorithm/docs/deal-balancing-prd.md
@@ -209,6 +209,8 @@ landlord.shuffle-strategy.threshold-relax-step=0.15
 landlord.shuffle-strategy.version=dealing_filter_v2
 ```

+> **注意**：上块为**推荐配置形态**（§三各表「推荐值」的汇总）。当前仓库 `src/main/resources/application.properties` 实际只启用维度一/二（`lower-threshold=-66`、`upper-threshold=75`、`max-spread=112`），维度三/四/五暂以下列值关闭：`max-potential-landlord-score=Infinity`、`max-landlord-advantage=Infinity`、`max-singles-per-hand=0`、`max-bombs-per-hand=0`。灰度开启各维度时按上块推荐值调整。
+
 ---

diff --git a/algorithm/docs/split-strategy-decision-rules.md b/algorithm/docs/split-strategy-decision-rules.md
index 8fcf968..b5fe872 100644
--- a/algorithm/docs/split-strategy-decision-rules.md
+++ b/algorithm/docs/split-strategy-decision-rules.md
@@ -1,6 +1,6 @@
 # 拆牌策略决策规则（顺子连对优先 vs 飞机炸弹优先）

-根据手牌点数分布，在「顺子/连对优先」与「飞机/炸弹优先（贪心）」之间自动选择一种策略，只跑一套拆牌，在保证质量的前提下减少计算。
+根据手牌点数分布，在「顺子/连对优先」与「飞机/炸弹优先（贪心）」之间自动选择策略。发牌评分主路径为**双路径取优**（两套都跑、取高分，不经过本文规则，见第五节）；本文决策规则用于**单路径**场景，并附历史遗留的「置信度」标记（注意：高置信**不代表**单路径与双路径等价，见第五节末注记）。

 ---

@@ -59,7 +59,7 @@

 7. **默认**
    - 条件：以上均不满足。
-   - 决策：**飞机/炸弹优先（贪心）**。与 `PlaneBombFirstSplitAlgorithm` 行为一致，保证下限。
+   - 决策：**飞机/炸弹优先（贪心）**。与 `PlaneBombPrioritizedSplitter` 行为一致，保证下限。

 ---

@@ -77,7 +77,16 @@

 ## 五、与实现的对应关系

-- **顺子/连对优先** → 使用 `StraightFirstSplitAlgorithm.split(hand)`（先顺子、连对，再三张/对子/单牌，最后炸弹）。
-- **飞机/炸弹优先（贪心）** → 使用 `PlaneBombFirstSplitAlgorithm.split(hand)`（王炸→炸弹→飞机→四带二→顺子→连对→三带→对子→单）。
+- **顺子/连对优先** → `StraightPrioritizedSplitter.extractAllCombos(hand, count)`（先顺子、连对，再三张/对子/单牌，最后炸弹）。
+- **飞机/炸弹优先（贪心）** → `PlaneBombPrioritizedSplitter.extractAllCombos(hand, count)`（王炸→炸弹→飞机→四带二→顺子→连对→三带→对子→单）。

-实现上由 `SplitterAlgorithmFactory` 根据 `HandCardUtils.buildRankCountsFromHand(hand)` 得到 `count`，在顺子区上计算特征并按第三节顺序判断，再通过 `getSplitter(hand)` 返回对应算法，`split(hand)` 只执行该算法的一套拆牌。
+实现上由 `DefaultSplitterFactory` 根据 `HandCardUtils.buildRankCounts(handCards)` 得到 `count`，在顺子区上计算特征并按第三节顺序判断，再通过 `getSplitter(count)` 返回对应算法，`extractAllCombos(handCards)` 只执行该算法的一套拆牌。
+
+### 单路径与双路径的分工
+
+| 路径 | 入口 | 行为 |
+|------|------|------|
+| **双路径取优**（发牌评分主路径） | `DefaultComboExtractor`（注入 `IHandCardsScoringStrategy`；`AbstractShuffleDealStrategy` 默认构造即如此装配） | 两套拆牌各算一遍总分，取分高者，**不经过本文决策规则**（另见 [deal-balancing-prd.md](deal-balancing-prd.md) §2.2） |
+| **单路径**（本文规则的用武之地） | `DefaultSplitterFactory.extractAllCombos` / `getSplitter` | 按第三节规则只跑一套；`DefaultComboExtractor` 未注入评分策略时退化为该路径 |
+
+- **置信度**：`DefaultSplitterFactory.chooseStrategyWithConfidence` 额外输出是否「高置信度」（仅第三节规则 1 / 规则 2 命中时为 true）。该标记是**历史启发式残留**：曾有实现据此短路为单路径，因结论错误已移除；发牌评分主路径**始终双路径取优，不读取该标记**。「高置信 ⇒ 单路径与双路径等价」**不成立**——反例见 `SplitterRegressionTest.strongStraightStillComparesBombPreservingSplit`（`34567 8888 9TJQKA 2 sj` 命中规则 1 高置信，单路径拆法即丢 `BOMB(8)`，仍须比较双路径）。

diff --git a/algorithm/docs/testing-strategy-and-commands.md b/algorithm/docs/testing-strategy-and-commands.md
index dd3899f..03d23fc 100644
--- a/algorithm/docs/testing-strategy-and-commands.md
+++ b/algorithm/docs/testing-strategy-and-commands.md
@@ -30,6 +30,21 @@
       - **关闭过滤**：`enabled=false`，验证行为等价于默认策略，不发生重洗（`reshuffled=false`、`reshuffleCnt=0`）。
       - **开启过滤且配置最大重洗次数**：验证 `reshuffleCnt` 始终在 \[0, maxReshuffleTimes] 范围内，防止实现错误导致无限重洗或超出上限。

+- **拆牌（splitter / split）**
+  - **`DefaultSplitterFactoryTest`**（原名 `SplitterAlgorithmFactoryTest`，随包迁移同步改名）
+    - 对 `DefaultSplitterFactory.chooseStrategy(int[])` 的单元测试，逐条覆盖 [split-strategy-decision-rules.md](split-strategy-decision-rules.md) 第三节的决策规则及边界值（顺子区为 Rank 0~11，即 3~A）。
+  - **`SplitterRegressionTest`**
+    - 经 `DefaultComboExtractor`（注入评分，双路径取优）的拆牌回归：孤立三张不作自身对翼、强顺子下仍保留炸弹、三带一的对翼须来自其他点数、双炸弹保留、四带二不得以自身对翼带牌等。
+    - 整手拆牌用例与 [cross-language-regression-vectors.md](cross-language-regression-vectors.md) §整手拆牌向量 一致（如 `hand-straight-bomb-8` / `hand-straight-bomb-2`）；随机发牌的跨语言一致性不在该表范围内（双种子标定属 [deal-balancing-prd.md](deal-balancing-prd.md)）。
+
+- **边界与领域模型（boundary / model）**
+  - **`BoundaryBugTest`**
+    - 覆盖 null / 空集合 / 非法索引 / 非法配置等场景：ComboScoring、HandScoring、HandCardUtils、DefaultSplitterFactory、DefaultComboExtractor、DealData、dealCards、ScoringStrategyProperties，以及 `ehsScale=0` 时发牌流程不产生 NaN。
+  - **`ComboTest`**
+    - `Combo` 领域模型工厂方法的构造与判定。
+  - **`LandlordAlgorithmApplicationTests`**
+    - Spring 上下文加载冒烟测试（`contextLoads`）。
+
```

> A2 文档行已使用新类名 `DefaultSplitterFactoryTest`——与 B2 的必做改名同批落地，无需二次同步。

## 源仓拓扑与落地方位（2026-09-10 探明）

| 位置 | 状态 |
|---|---|
| **Mac** `/Users/maerun/Projects/landlord` | **权威源仓**（`docs/plan/2026-09-04-dealing-algorithm-agent-foundation.md:52` 记载）。其 main 已推进到快照基准 `f2dbcf6`（含 2026-09-06 同步的 `e4be61a..a4b53b9` 批次：三带自吃修复、移除高置信单路径短路、SplitterRegressionTest、product-prd 大改、landlord.h 变更）——**这批提交从未推上 GitHub** |
| GitHub `Samuel86-star/landlord` main | 停在 `e4be61a`（落后权威库一批提交，无其他分支）。本机 SSH 身份即 Samuel86-star |
| Windows `D:\Coding\landlord` | 2026-09-10 新鲜克隆（GitHub 态），仅作对照/查历史，**勿在此直接落地** |
| Windows `D:\Coding\landlord-algorithm` | 无 `.git` 的旧工程残骸（布局也不同：顶层 `src/`+`test/`+`CMakeLists.txt`），与源仓无关，勿用 |

**结论：A/B 组修复应在 Mac 权威库执行，验证通过后连同积压提交一并 push 到 GitHub**（单次推送）；若从 Windows 直接推 GitHub 会与权威库分叉。

## B 组：代码侧清理（源仓执行）

### B1. AbstractHandSplitter.java Javadoc 旧类名（约 15 行处）

```java
// 改前
 * 不包含任何“优先级/策略”逻辑。具体的拆牌策略类（如 PlaneBombFirstSplitAlgorithm、StraightFirstSplitAlgorithm 等），
// 改后
 * 不包含任何“优先级/策略”逻辑。具体的拆牌策略类（如 PlaneBombPrioritizedSplitter、StraightPrioritizedSplitter 等），
```

注释级修改，零行为影响。

### B2. SplitterAlgorithmFactoryTest 包迁移 + 改名（必做）

已核实全仓**无任何代码引用**该测试类与 `algorithm.split.` 包（无 import、无 pom/surefire 包级配置），迁移自包含。改名与包迁移同批做（A2 文档行已用新名）：

```bash
git mv src/test/java/com/mamba/landlord/algorithm/split/SplitterAlgorithmFactoryTest.java \
       src/test/java/com/mamba/landlord/algorithm/splitter/DefaultSplitterFactoryTest.java
f=src/test/java/com/mamba/landlord/algorithm/splitter/DefaultSplitterFactoryTest.java
sed -i '' 's/^package com\.mamba\.landlord\.algorithm\.split;/package com.mamba.landlord.algorithm.splitter;/' "$f"
sed -i '' '/^import com\.mamba\.landlord\.algorithm\.splitter\.DefaultSplitterFactory;$/d' "$f"
sed -i '' 's/class SplitterAlgorithmFactoryTest/class DefaultSplitterFactoryTest/' "$f"
grep -n "SplitterAlgorithmFactoryTest" "$f"   # 应零输出（有残留则逐处手改）
```

三处文件内修改：package 行、删除第 3 行同包冗余 import、class 声明改名（17 行）。旧类名指早已不存在的 `SplitterAlgorithmFactory`，保留会使"旧名零命中"验证（见落地后）永远失败——故必做。

验证：`mvn test -Dtest='DefaultSplitterFactoryTest'`。

### B3. 删除 docs/prompt.md

```bash
git rm docs/prompt.md
```

内容仅 1 行英文 AI 指令（"Run the benchmarks. Identify the worst performing cases..."），无文档价值。

### B4. DefaultSplitterFactory.java 置信度 Javadoc 更正（复审新增，约 80~81 行）

`chooseStrategyWithConfidence` 的 Javadoc 现称"高置信度时（前两条规则命中）**可安全使用单路径**"——与 a4b53b9 移除错误短路的结论矛盾（A1 文档同源错误的代码侧残留），反例同 `SplitterRegressionTest.strongStraightStillComparesBombPreservingSplit`。

```bash
f=src/main/java/com/mamba/landlord/algorithm/splitter/DefaultSplitterFactory.java
sed -i '' 's/高置信度时（前两条规则命中）可安全使用单路径，否则建议双路径取优。/该标记为历史启发式残留：生产评分主路径始终双路径取优、不据此短路；高置信不代表单路径与双路径结果等价。/' "$f"
```

注释级修改，零行为影响；与 B1、A1 置信度注记三处口径统一。

## 落地后

1. 源仓（Mac）合入并**全量验证**：`mvn test` 全绿；Native `cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Release && cmake --build native/build && ctest --test-dir native/build`（`landlord_test` 通过；源仓 CMakeLists 无 analysis 侧的 extracted 目标）；`grep -rn "FirstSplitAlgorithm\|SplitterAlgorithmFactory\|buildRankCountsFromHand\|可安全使用单路径" docs/ src/` 零命中（git 历史除外）。
2. 验证通过后**单次 push**：连同积压的 `e4be61a..f2dbcf6` 提交一并推上 GitHub（顺带清掉 GitHub 落后欠账）；push 前先 `git fetch origin` 确认远端仍停在 `e4be61a`、无分叉。
3. 记录 source base/head，按 [algorithm-snapshot-plan.md](algorithm-snapshot-plan.md) §更新方法 增量同步回本仓 `algorithm/`。
4. A 组文件届时与本仓 9a84ab4 + 9d05210 的例外修复**内容收敛**（同步时按"内容不同只合入源仓 hunks"处理，结果应一致或仅余源仓新增内容）；B 组随同步首次进入快照。
5. 同步完成后本文件 A/B 组状态更新为"已落地"，或在快照 README 记录后归档本文件。

## 复审修订记录（2026-09-10）

外部复审（GPT-5.6）意见 4 条全采纳，另衍生新增 B4：

| # | 意见 | 证据 | 处置 |
|---|---|---|---|
| 1 | "规则 1/2 命中 ⇒ 单路径 ≈ 双路径"是错误结论（阻断项） | `DefaultComboExtractor` 注入评分后无条件双路径；a4b53b9 批次"移除错误的高置信单路径短路"（algorithm-snapshot-plan.md:66）；反例 `strongStraightStillComparesBombPreservingSplit`（强顺子高置信仍须双路径保炸） | A1 置信度句改写为"历史启发式残留、不代表等价"；衍生 **B4**（同类错误残留在 `DefaultSplitterFactory` Javadoc） |
| 2 | "SplitterRegressionTest 对应双种子校准证据"混淆两类验证 | cross-language-regression-vectors.md:27 明确随机发牌一致性不在表内；双种子 P95 标定属 deal-balancing-prd | A2 该句改为"§整手拆牌向量一致"并注明归属 |
| 3 | 改名"可选"与旧名 grep 零命中互相矛盾 | 不改名则 `SplitterAlgorithmFactoryTest` 永远命中 grep | B2 改名转**必做**，A2 文档行直接用新名 |
| 4 | 先 push 6 个行为提交再验证，且漏 Native 测试 | snapshot-plan:98 标准即"Java 测试 + Release native + CTest" | handoff 重排为 fetch 防分叉 → 应用 → 全量验证 → 单次推送 |
