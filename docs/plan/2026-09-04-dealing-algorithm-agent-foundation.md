# 斗地主发牌算法智能体基础能力 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 Java/C++ 发牌结果正确性和测试入口，并建立可复现、可追溯的算法实验基础。

**Architecture:** 算法计算继续留在现有 Java/C++ 实现，线上概率分析继续复用 `native/extracted/harness.cpp`，不创建新的模拟平台。算法改动先在权威源仓完成，验证后同步到本仓库只读快照；本计划只交付 Spec 阶段 A/B，阶段 C/D 在真实任务门槛通过后另行规划。

**Tech Stack:** Java 21、JUnit 5、Maven、C++11、CMake/CTest、Python 3 标准库、Markdown

**Spec:** `docs/spec/2026-09-04-dealing-algorithm-agent-design.md`

## Global Constraints

- `algorithm/` 当前按只读快照处理；算法改动先提交到 [landlord-algorithm](https://github.com/Samuel86-star/landlord.git)，再同步本仓库。
- `algorithm/native/previous/` 是线上代码参照副本，不得静默修改。
- 牌型、拆牌、评分、发牌、随机数和统计计算必须由确定性代码完成，不能移入提示词或 Skill。
- 正确性不变量未通过时，不运行参数调优或输出公平性结论。
- 同一规则涉及 Java/C++ 时，两端修复和等价回归用例必须同时交付。
- 复用现有 harness、测试和统计脚本；不新增第三方依赖、通用实验平台或 agent runtime。
- 静态牌力、模拟分布和线上胜率必须分别表述，不得互相替代。
- 算法、配置和线上参照修改必须获得用户确认。

---

## File Map

以下算法路径均相对于权威 `landlord-algorithm` 源仓；同步完成后对应本仓库的 `algorithm/<path>`。

| 文件 | 职责 |
| ---- | ---- |
| `src/main/java/com/mamba/landlord/core/model/Combo.java` | 修正 Java 连对组合的实际牌数 |
| `src/test/java/com/mamba/landlord/core/model/ComboTest.java` | 验证 Java 组合长度不变量 |
| `src/main/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategy.java` | 每次重洗后完整重算并返回匹配的牌力 |
| `src/test/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategyTest.java` | 复现并防止重洗结果携带旧分数 |
| `src/test/java/com/mamba/landlord/benchmark/ShuffleAndScoringBenchmarkTest.java` | 标记 Java 发牌 benchmark |
| `src/test/java/com/mamba/landlord/benchmark/ScoringAndSplittingBenchmarkTest.java` | 标记 Java 拆牌与评分 benchmark |
| `native/include/landlord.h` | 修正 C++ 连对长度、重洗结果和悬空指针风险 |
| `native/test/main_test.cpp` | 提供 Release 构建仍有效的 C++ 检查和等价回归案例 |
| `native/CMakeLists.txt` | 用 CTest 注册 C++ 正确性测试 |
| `docs/testing-strategy-and-commands.md` | 记录普通测试、benchmark、CTest 和等价向量命令 |
| `docs/deal-balancing-prd.md` | 删除不再成立的快速失败设计说明 |
| `docs/cross-language-regression-vectors.md` | 记录 Java/C++ 共同遵守的最小确定性案例 |
| `docs/makedeal-strategies/_experiment-template.md` | 固化实验任务契约、版本、种子、指标和结论边界 |
| `docs/tech/algorithm-snapshot-plan.md` | 固化经验证的快照同步命令与排除项 |
| `algorithm/README.md` | 同步后更新源 commit 和快照日期 |

## Source Repository Gate

Tasks 1–6 必须在权威 `landlord-algorithm` 源仓执行。若本机尚无 `/Users/maerun/Projects/landlord`，先获得网络和目录写入批准，再运行：

```bash
git clone https://github.com/Samuel86-star/landlord.git /Users/maerun/Projects/landlord
```

进入该目录后运行：

```bash
git remote get-url origin
git status --short
git rev-parse HEAD
```

Expected:

- remote 为 `https://github.com/Samuel86-star/landlord.git` 或其 SSH 等价地址。
- 工作区状态已确认，不覆盖用户未提交修改。
- 当前 commit 已记录到执行日志，后续可作为同步来源。

若无法定位权威源仓，停止算法修改；不得直接编辑本仓库 `algorithm/` 快照绕过此门槛。

### Task 1: Correct Java Consecutive-Pair Length

**Files:**

- Modify: `src/main/java/com/mamba/landlord/core/model/Combo.java:119-134`
- Create: `src/test/java/com/mamba/landlord/core/model/ComboTest.java`

**Interfaces:**

- Consumes: `Combo.consecutivePairs(List<Rank>)`。
- Produces: `Combo.length()`；`CONSECUTIVE_PAIRS` 返回 `mainRanks.size() * 2`。

- [ ] **Step 1: Write the failing Java regression test**

```java
package com.mamba.landlord.core.model;

import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;

class ComboTest {

    @Test
    void consecutivePairsLengthCountsBothCardsInEveryPair() {
        Combo combo = Combo.consecutivePairs(List.of(Rank.THREE, Rank.FOUR, Rank.FIVE));

        assertEquals(6, combo.length());
    }
}
```

- [ ] **Step 2: Run the focused test and verify the defect**

Run: `mvn -Dtest=ComboTest test`

Expected: FAIL with expected `6` but actual `3`.

- [ ] **Step 3: Make the minimal Java fix**

Split the combined switch arm so only consecutive pairs multiply by two:

```java
case STRAIGHT -> mainRanks.size();
case CONSECUTIVE_PAIRS -> mainRanks.size() * 2;
```

- [ ] **Step 4: Run the focused test**

Run: `mvn -Dtest=ComboTest test`

Expected: PASS with one test and zero failures.

- [ ] **Step 5: Commit Task 1 in the source repository**

```bash
git add src/main/java/com/mamba/landlord/core/model/Combo.java src/test/java/com/mamba/landlord/core/model/ComboTest.java
git commit -m "fix(core): count cards in consecutive pairs"
```

### Task 2: Make C++ Correctness Tests Effective in Release Builds

**Files:**

- Modify: `native/include/landlord.h:227-247`
- Modify: `native/test/main_test.cpp:1-297`
- Modify: `native/CMakeLists.txt:13-18`

**Interfaces:**

- Produces: `Combo::length()` returns `2 * mainRanks.size()` for `CONSECUTIVE_PAIRS`。
- Produces: `CHECK(expression)` throws `std::runtime_error` independently of `NDEBUG`。
- Produces: CTest test named `landlord_test` with working directory `native/`。

- [ ] **Step 1: Add the failing C++ consecutive-pair check**

In `testComboFactories()` add:

```cpp
auto consecutivePairs = Combo::consecutivePairs({Rank::THREE, Rank::FOUR, Rank::FIVE});
assert(consecutivePairs.type == ComboType::CONSECUTIVE_PAIRS);
assert(consecutivePairs.length() == 6);
```

- [ ] **Step 2: Build and run the Debug executable to verify the defect**

```bash
cmake -S native -B native/build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build native/build-debug --target landlord_test
./native/build-debug/landlord_test
```

Expected: process aborts at `consecutivePairs.length() == 6`.

- [ ] **Step 3: Correct the C++ length calculation**

```cpp
case ComboType::STRAIGHT:
    return static_cast<int>(mainRanks.size());
case ComboType::CONSECUTIVE_PAIRS:
    return static_cast<int>(mainRanks.size()) * 2;
```

- [ ] **Step 4: Replace disabled Release assertions with one local check macro**

Replace `<cassert>` with `<stdexcept>` and add:

```cpp
#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            throw std::runtime_error("CHECK failed: " #expression); \
        } \
    } while (false)
```

Mechanically replace every `assert(expression)` in `native/test/main_test.cpp` with `CHECK(expression)`. Do not change benchmark loops or introduce a test framework.

Also strengthen `testDeckSize()` so the native suite checks uniqueness, not only the total count. Add `<set>` and insert `(rank, suit)` pairs:

```cpp
std::set<std::pair<int, int>> uniqueCards;
for (const auto& card : deck) {
    uniqueCards.insert({static_cast<int>(card.rank), static_cast<int>(card.suit)});
}
CHECK(uniqueCards.size() == 54);
```

- [ ] **Step 5: Register the executable with CTest**

Add after `add_executable(landlord_test ...)`:

```cmake
enable_testing()
add_test(NAME landlord_test COMMAND landlord_test)
set_tests_properties(landlord_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
```

- [ ] **Step 6: Verify Release tests are discovered and enforce checks**

```bash
cmake -S native -B native/build-release -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-release --target landlord_test
ctest --test-dir native/build-release --output-on-failure
```

Expected: CTest discovers exactly one `landlord_test`; it passes with zero failures in Release mode.

- [ ] **Step 7: Commit Task 2 in the source repository**

```bash
git add native/include/landlord.h native/test/main_test.cpp native/CMakeLists.txt
git commit -m "fix(native): enforce correctness checks in release builds"
```

### Task 3: Return Scores for the Final Java Deal

**Files:**

- Modify: `src/main/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategy.java:69-137,224-243`
- Modify: `src/test/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategyTest.java`
- Modify: `docs/deal-balancing-prd.md:99-114`

**Interfaces:**

- Consumes: `DefaultReshuffleDealStrategy.shuffleAndDeal()`。
- Produces: all `handStrengthPlayerN` values are recomputed from the returned `handCardsPlayerN`。
- Removes: private fast-fail state and `findProblematicSeat(...)` helper。

- [ ] **Step 1: Add a deterministic invariant assertion to the Java reshuffle test**

Add imports for `DefaultHandCardsScoringStrategy`, `DefaultComboExtractor`, `Card`, `Deck`, `ArrayList`, `Collections`, and `List`, then add:

```java
private static final class SequenceStrategy extends DefaultReshuffleDealStrategy {
    private final List<List<Card>> decks;
    private int index;

    private SequenceStrategy() {
        List<Card> first = Deck.copyFullDeckCards();
        List<Card> second = new ArrayList<>(first);
        Collections.reverse(second);
        decks = List.of(first, second);
    }

    @Override
    public List<Card> shuffle() {
        return decks.get(index++);
    }
}

private static double recomputeScore(List<Card> cards) {
    DefaultHandCardsScoringStrategy scorer = new DefaultHandCardsScoringStrategy();
    DefaultComboExtractor extractor = new DefaultComboExtractor(scorer);
    return scorer.calcTotalHandScore(cards, extractor.extractAllCombos(cards));
}

@Test
void returnedScoresAlwaysBelongToReturnedHandsAfterMaxReshuffles() {
    ShuffleStrategyDecisionProperties props = new ShuffleStrategyDecisionProperties();
    props.setEnabled(true);
    props.setLowerThreshold(100_000.0);
    props.setUpperThreshold(100_001.0);
    props.setMaxReshuffleTimes(1);
    props.setThresholdRelaxStep(0.0);
    ShuffleStrategyDecisionHolder.set(props);

    EvaluatedDealData data = new SequenceStrategy().shuffleAndDeal();

    assertEquals(1, data.getReshuffleCnt());
    assertEquals(recomputeScore(data.getHandCards(0)), data.getHandStrengthPlayer0(), 0.001);
    assertEquals(recomputeScore(data.getHandCards(1)), data.getHandStrengthPlayer1(), 0.001);
    assertEquals(recomputeScore(data.getHandCards(2)), data.getHandStrengthPlayer2(), 0.001);
}
```

- [ ] **Step 2: Run the focused test and verify stale scores are exposed**

Run: `mvn -Dtest=DefaultReshuffleDealStrategyTest#returnedScoresAlwaysBelongToReturnedHandsAfterMaxReshuffles test`

Expected: FAIL because the final reversed deck is returned with scores from the initial ordered deck.

- [ ] **Step 3: Remove the unsafe fast-fail path**

Delete `problematicSeat`, the block that calculates `quickScore` and calls `continue`, and the now-unused `findProblematicSeat(...)` method. After every `dealCards(shuffledCards)`, unconditionally retain:

```java
seatScores = calcAllSeatHandStrength(dealData);
potentialLandlordScore = calcPotentialLandlordScore(dealData);
landlordAdvantage = calcMaxLandlordAdvantage(dealData, seatScores);
structureFeatures = calcAllStructureFeatures(dealData);
```

Update the class and method Javadocs to remove fast-fail claims.

- [ ] **Step 4: Run the Java reshuffle suite**

Run: `mvn -Dtest=DefaultReshuffleDealStrategyTest test`

Expected: all tests PASS with zero failures.

- [ ] **Step 5: Correct the PRD**

Remove the “快速失败（Fast-Fail）” subsection from `docs/deal-balancing-prd.md`. Replace it with one sentence under performance optimization: every accepted or max-attempt deal is fully evaluated so returned scores always match returned cards.

- [ ] **Step 6: Commit Task 3 in the source repository**

```bash
git add src/main/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategy.java src/test/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategyTest.java docs/deal-balancing-prd.md
git commit -m "fix(shuffle): evaluate every returned Java deal"
```

### Task 4: Align C++ Reshuffle Results and Remove the Borrowed Scorer Lifetime

**Files:**

- Modify: `native/include/landlord.h:995-1042,1138-1195,1249-1256`
- Modify: `native/test/main_test.cpp`

**Interfaces:**

- Produces: `ShuffleDealStrategy::shuffleAndDeal()` returns scores recomputed from the returned hands。
- Removes: unused `ShuffleDealStrategy(const DefaultHandCardsScoringStrategy&, const DefaultComboExtractor&)`。
- Produces: `ShuffleDealStrategy(std::uint32_t seed)` for deterministic native regression tests。
- Preserves: `DefaultComboExtractor(const DefaultHandCardsScoringStrategy&)` and the default dual-strategy scoring path。

- [ ] **Step 1: Add C++ score-consistency checks at the max-reshuffle boundary**

Add to `native/test/main_test.cpp`:

```cpp
void testSeededShuffleIsReproducible() {
    ShuffleDealStrategy first(7);
    ShuffleDealStrategy second(7);
    CHECK(first.shuffle() == second.shuffle());
}

void testReturnedScoresMatchReturnedHands() {
    auto& cfg = shuffleConfig();
    cfg.enabled = true;
    cfg.lowerThreshold = 100000.0;
    cfg.upperThreshold = 100001.0;
    cfg.maxReshuffleTimes = 1;
    cfg.thresholdRelaxStep = 0.0;

    ShuffleDealStrategy strategy(7);
    auto result = strategy.shuffleAndDeal();

    CHECK(result.reshuffleCnt == 1);
    for (int seat = 0; seat < 3; ++seat) {
        double recomputed = strategy.calcHandStrengthScores(result.handCards[seat]);
        CHECK(std::abs(recomputed - result.handStrength[seat]) < 0.001);
    }
}
```

Call both functions from `main()` before benchmarks.

- [ ] **Step 2: Build and verify the seeded constructor is not implemented yet**

Run: `cmake --build native/build-release --target landlord_test`

Expected: compilation FAILS because `ShuffleDealStrategy(std::uint32_t)` does not exist.

- [ ] **Step 3: Replace the unsafe injection constructor with deterministic RNG ownership**

Delete only this unused overload:

```cpp
ShuffleDealStrategy(const DefaultHandCardsScoringStrategy& hs,
                    const DefaultComboExtractor& ce)
    : handScorer_(hs), comboExtractor_(ce) {}
```

Keep `DefaultComboExtractor(const DefaultHandCardsScoringStrategy&)`; the default strategy needs it for dual-path scoring. Add `<cstdint>`, make the RNG an instance member, and provide default and seeded construction:

```cpp
ShuffleDealStrategy()
    : handScorer_()
    , comboExtractor_(handScorer_)
    , rng_(std::random_device{}()) {}

explicit ShuffleDealStrategy(std::uint32_t seed)
    : handScorer_()
    , comboExtractor_(handScorer_)
    , rng_(seed) {}
```

Replace the function-local static RNG in `shuffle()` with `rng_` and add this member after `comboExtractor_`:

```cpp
std::mt19937 rng_;
```

- [ ] **Step 4: Run CTest and verify the C++ stale-score failure deterministically**

```bash
cmake --build native/build-release --target landlord_test
ctest --test-dir native/build-release --output-on-failure
```

Expected: seeded shuffle reproducibility passes, then `landlord_test` FAILS at a returned score comparison.

- [ ] **Step 5: Remove the C++ fast-fail path**

Delete `problematicSeat`, the `quickScore` block, and `findProblematicSeat(...)`. After every new `dealData`, always execute:

```cpp
seatScores        = calcAllSeatHandStrength(dealData);
potentialLandlord = calcPotentialLandlordScore(dealData);
landlordAdv       = calcMaxLandlordAdvantage(dealData, seatScores);
structureFeats    = calcAllStructureFeatures(dealData);
```

- [ ] **Step 6: Confirm only the unsafe constructor was removed**

Confirm no callers remain:

Run: `rg -n "DefaultComboExtractor\([^)]*scorer|ShuffleDealStrategy\([^)]*DefaultHandCardsScoringStrategy" native`

Expected: `DefaultComboExtractor(const DefaultHandCardsScoringStrategy&)` remains and the two-argument `ShuffleDealStrategy` constructor has no matches.

Confirm the remaining default constructor still binds the extractor to the owned scorer:

```cpp
ShuffleDealStrategy()
    : handScorer_()
    , comboExtractor_(handScorer_)
    , rng_(std::random_device{}()) {}
```

Do not add smart pointers or a new ownership abstraction for the removed, unused injection path.

- [ ] **Step 7: Run all C++ correctness tests**

```bash
cmake --build native/build-release --target landlord_test
ctest --test-dir native/build-release --output-on-failure
```

Expected: one CTest test passes with zero failures, including seeded reproducibility, combo length and returned-score consistency.

- [ ] **Step 8: Commit Task 4 in the source repository**

```bash
git add native/include/landlord.h native/test/main_test.cpp
git commit -m "fix(native): keep deal scores and ownership consistent"
```

### Task 5: Separate Java Benchmarks from Daily Tests

**Files:**

- Modify: `src/test/java/com/mamba/landlord/benchmark/ShuffleAndScoringBenchmarkTest.java:1-24`
- Modify: `src/test/java/com/mamba/landlord/benchmark/ScoringAndSplittingBenchmarkTest.java:1-24`
- Modify: `docs/testing-strategy-and-commands.md`

**Interfaces:**

- Produces: both benchmark classes carry JUnit 5 `@Tag("benchmark")`。
- Preserves: `pom.xml` existing `excludedGroups=benchmark` and `benchmark` profile。

- [ ] **Step 1: Add benchmark tags to both classes**

In both files import `org.junit.jupiter.api.Tag` and annotate the class:

```java
@Tag("benchmark")
class ShuffleAndScoringBenchmarkTest {
```

```java
@Tag("benchmark")
class ScoringAndSplittingBenchmarkTest {
```

- [ ] **Step 2: Verify daily tests exclude benchmark output**

Run: `mvn test`

Expected: build succeeds; Surefire does not execute tests from either `com.mamba.landlord.benchmark` class.

- [ ] **Step 3: Verify the benchmark profile includes both classes**

Run: `mvn -Pbenchmark -Dtest=ShuffleAndScoringBenchmarkTest,ScoringAndSplittingBenchmarkTest test`

Expected: both benchmark classes execute and the build succeeds with zero failures.

- [ ] **Step 4: Make test documentation match executable behavior**

In `docs/testing-strategy-and-commands.md`, retain these commands and explicitly name both tagged classes:

```bash
mvn test
mvn test -Pbenchmark
cmake -S native -B native/build-release -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-release --target landlord_test
ctest --test-dir native/build-release --output-on-failure
```

State that benchmark throughput thresholds depend on the recorded machine and are not correctness assertions.

- [ ] **Step 5: Commit Task 5 in the source repository**

```bash
git add src/test/java/com/mamba/landlord/benchmark/ShuffleAndScoringBenchmarkTest.java src/test/java/com/mamba/landlord/benchmark/ScoringAndSplittingBenchmarkTest.java docs/testing-strategy-and-commands.md
git commit -m "test: isolate algorithm benchmarks"
```

### Task 6: Record Cross-Language Regression Vectors

**Files:**

- Create: `docs/cross-language-regression-vectors.md`
- Modify: `src/test/java/com/mamba/landlord/core/model/ComboTest.java`
- Modify: `native/test/main_test.cpp`

**Interfaces:**

- Produces: one documented vector table mirrored by Java and C++ correctness tests。
- Scope: deterministic factories and scoring only; random shuffle order is explicitly excluded。

- [ ] **Step 1: Create the shared vector document**

Create this table in `docs/cross-language-regression-vectors.md`:

```markdown
# Java/C++ 斗地主算法回归向量

| 编号 | 构造 | 期望类型 | 期望长度 | 期望 Combo 分 |
| ---- | ---- | ---- | ---- | ---- |
| combo-single-ace | `single(ACE)` | `SINGLE` | 1 | 0.0 |
| combo-pair-ace | `pair(ACE)` | `PAIR` | 2 | 14.0 |
| combo-triple-ace | `triple(ACE)` | `TRIPLE` | 3 | 23.0 |
| combo-straight-3-7 | `straight(THREE..SEVEN)` | `STRAIGHT` | 5 | 15.0 |
| combo-consecutive-pairs-3-5 | `consecutivePairs(THREE..FIVE)` | `CONSECUTIVE_PAIRS` | 6 | 12.0 |
| combo-bomb-king | `bomb(KING)` | `BOMB` | 4 | 43.0 |
| combo-rocket | `rocket()` | `ROCKET` | 2 | 60.0 |
```

Add two rules below the table: any scoring-formula change updates both test suites and this table in the same commit; random deal parity is out of scope until both implementations share an RNG protocol.

- [ ] **Step 2: Mirror every listed length case in Java and C++ tests**

Use the existing factories in `ComboTest` and `testComboFactories()`. For exact scores, use `assertEquals(expected, scorer.score(combo), 0.001)` in Java and `CHECK(std::abs(expected - scorer.score(combo)) < 0.001)` in C++.

- [ ] **Step 3: Run both correctness suites**

```bash
mvn test
cmake --build native/build-release --target landlord_test
ctest --test-dir native/build-release --output-on-failure
```

Expected: Maven and CTest both succeed with zero failures; default Maven execution still excludes benchmark classes.

- [ ] **Step 4: Commit Task 6 in the source repository**

```bash
git add docs/cross-language-regression-vectors.md src/test/java/com/mamba/landlord/core/model/ComboTest.java native/test/main_test.cpp
git commit -m "test: align Java and native regression vectors"
```

### Task 7: Add the Reproducible Experiment Contract

**Files:**

- Create in analysis repository: `docs/makedeal-strategies/_experiment-template.md`
- Modify in analysis repository: `docs/knowledge/makedeal-simulation.md`

**Interfaces:**

- Consumes: existing `algorithm/native/extracted/harness.cpp`, `anchor_check.py`, and `sweep.py` commands。
- Produces: a Markdown-only task contract and result record; no new runner or dependency。

- [ ] **Step 1: Create the experiment template**

Use the following required sections and fields:

```markdown
# 发牌算法实验：<任务名称>

## 任务契约

| 字段 | 值 |
| ---- | ---- |
| 任务类型 | 正确性 / 策略比较 / 参数调优 / 公平性 / 性能 |
| 业务目标 | |
| 主指标与阈值 | |
| 保护指标与阈值 | |
| 房间与玩家构成 | |

## 可复现输入

| 字段 | 值 |
| ---- | ---- |
| 源仓 commit | |
| 分析仓快照 commit | |
| 配置版本或摘要 | |
| 运行环境与构建类型 | |
| harness 参数 | |
| 样本量 | |
| 训练种子范围 | |
| 独立复验种子范围 | |
| 执行命令 | |

## 结果与质量检查

| 指标 | 基线 | 候选 | 差异 | 是否通过 |
| ---- | ---- | ---- | ---- | ---- |
| 有效牌局数 | | | | |
| 无效牌局数 | | | | |
| 座位 / 房间 / 玩家类型分层 | | | | |
| 持有炸与单局炸弹率 | | | | |
| 最优手数与散牌 | | | | |
| 首叫诱导度与抗衡度 | | | | |
| 重洗率与达到上限比例 | | | | |
| 总耗时与单局耗时 | | | | |

## 结论边界与审批

- 静态牌力或模拟分布能够支持的结论：
- 需要真实牌谱或线上数据才能支持的结论：
- 推荐方案与风险：
- 用户审批结果：未审批 / 采纳 / 拒绝
```

The literal `<任务名称>` is an intentional template field, not an implementation placeholder; keep the template file name prefixed with `_` so it is not mistaken for a completed experiment.

- [ ] **Step 2: Link the template from the simulation methodology**

Add a short “实验留档” subsection to `docs/knowledge/makedeal-simulation.md`. Link `../makedeal-strategies/_experiment-template.md` and require source commit, snapshot commit, config, runtime environment, sample size, seed ranges and exact command for every recommendation.

- [ ] **Step 3: Verify the existing harness is reproducible without changing code**

Compile according to `algorithm/native/extracted/README.md`, then run the same small sample twice with identical arguments and compare outputs:

```bash
algorithm/native/extracted/harness --room 742 --reals 3 -n 100 --seed 1 --cfg algorithm/native/previous/makedeal.json > /tmp/dealing-seed-1-a.jsonl
algorithm/native/extracted/harness --room 742 --reals 3 -n 100 --seed 1 --cfg algorithm/native/previous/makedeal.json > /tmp/dealing-seed-1-b.jsonl
cmp /tmp/dealing-seed-1-a.jsonl /tmp/dealing-seed-1-b.jsonl
```

Expected: `cmp` exits 0. If it differs, stop and create a correctness task before using the template for comparisons.

- [ ] **Step 4: Verify the template and methodology links**

Run: `rg -n "源仓 commit|分析仓快照 commit|训练种子范围|独立复验种子范围|_experiment-template" docs/makedeal-strategies/_experiment-template.md docs/knowledge/makedeal-simulation.md`

Expected: every required field and the methodology link are present.

- [ ] **Step 5: Commit Task 7 in the analysis repository**

```bash
git add docs/makedeal-strategies/_experiment-template.md docs/knowledge/makedeal-simulation.md
git commit -m "docs(algorithm): define reproducible experiment contract"
```

### Task 8: Sync the Verified Source Snapshot

**Files:**

- Incrementally sync the 13 files changed by source commits `71599ad..e4be61a` into `algorithm/`
- Modify after sync: `algorithm/README.md:5-14`
- Modify: `docs/tech/algorithm-snapshot-plan.md`
- Preserve: `algorithm/native/extracted/`, `algorithm/native/previous/`, `shuffle_prng_compare*` and all other analysis-only assets
- Preserve unless a historical statement becomes false for its reviewed revision: `docs/review/dealing-algorithm-agent/2026-09-04-review.md`

**Interfaces:**

- Consumes: tested source repository commits `71599ad..e4be61a` from Tasks 1–6。
- Produces: analysis repository snapshot traceable to one source commit。

- [ ] **Step 1: Record the verified source commit**

In the verified source worktree run:

```bash
git -C /private/tmp/landlord-dealing-agent status --short
git -C /private/tmp/landlord-dealing-agent rev-parse HEAD
git -C /private/tmp/landlord-dealing-agent log -1 --oneline
```

Expected: `e4be61a` is HEAD and contains Tasks 1–6. Build directories may be untracked; there must be no tracked source changes, and synchronization reads commit objects rather than the mutable worktree.

- [ ] **Step 2: Derive and review the exact manifest**

Run:

```bash
git -C /private/tmp/landlord-dealing-agent diff --name-status 71599ad..e4be61a
```

Expected manifest:

| 状态 | 源仓路径 |
| ---- | ---- |
| A | `docs/cross-language-regression-vectors.md` |
| M | `docs/deal-balancing-prd.md` |
| M | `docs/testing-strategy-and-commands.md` |
| M | `native/CMakeLists.txt` |
| M | `native/include/landlord.h` |
| M | `native/test/main_test.cpp` |
| M | `pom.xml` |
| M | `src/main/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategy.java` |
| M | `src/main/java/com/mamba/landlord/core/model/Combo.java` |
| M | `src/test/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategyTest.java` |
| M | `src/test/java/com/mamba/landlord/benchmark/ScoringAndSplittingBenchmarkTest.java` |
| M | `src/test/java/com/mamba/landlord/benchmark/ShuffleAndScoringBenchmarkTest.java` |
| A | `src/test/java/com/mamba/landlord/core/model/ComboTest.java` |

- [ ] **Step 3: Classify target divergence against source base**

For each modified path, compare `algorithm/<path>` with `git -C /private/tmp/landlord-dealing-agent show 71599ad:<path>`.

Expected:

- Base-equal: `docs/testing-strategy-and-commands.md`, `pom.xml`, both Java main files, the reshuffle Java test and both benchmark tests. Apply only each file's final `71599ad..e4be61a` delta.
- Diverged: `docs/deal-balancing-prd.md`, `native/CMakeLists.txt`, `native/include/landlord.h`, `native/test/main_test.cpp`. Merge only Tasks 1–6 hunks while preserving analysis-only threshold calibration, extracted-tool targets, scoring rules and scoring tests.
- New: add the two `A` files from the manifest.

- [ ] **Step 4: Apply the incremental synchronization**

Use `apply_patch` for the 13 manifest files. Do not run deletion-based full-tree synchronization. Do not modify or delete `.git`, `.superpowers/`, build outputs, `native/extracted/`, `native/previous/`, `shuffle_prng_compare*` or any path absent from the manifest.

- [ ] **Step 5: Update snapshot metadata and synchronization documentation**

Set `algorithm/README.md` “快照 commit” to `e4be61a` and “快照日期” to `2026-09-05`. Keep the read-only warning. Update `docs/tech/algorithm-snapshot-plan.md` and this task to retain the safe manifest-and-merge procedure.

- [ ] **Step 6: Run the complete snapshot verification**

```bash
cd algorithm
./mvnw test
cmake -S native -B native/build-release -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-release --target landlord_test
ctest --test-dir native/build-release --output-on-failure
```

Expected: Maven succeeds with 62 tests and no benchmark classes: 60 from the verified source suite plus two preserved analysis-only scoring regressions. CTest discovers exactly one test and succeeds in Release mode.

- [ ] **Step 7: Check documentation, preservation and diff integrity**

```bash
git diff --check
git status --short
git diff --stat
git diff -- algorithm/native/extracted algorithm/native/previous
```

Expected: no whitespace errors; only the 13 source-manifest files, snapshot README and the two synchronization documents changed. Protected paths have no tracked diff, and the pre-existing untracked `algorithm/native/extracted/harness` remains untouched.

- [ ] **Step 8: Commit Task 8 in the analysis repository**

```bash
git add algorithm/README.md \
  algorithm/docs/cross-language-regression-vectors.md \
  algorithm/docs/deal-balancing-prd.md \
  algorithm/docs/testing-strategy-and-commands.md \
  algorithm/native/CMakeLists.txt \
  algorithm/native/include/landlord.h \
  algorithm/native/test/main_test.cpp \
  algorithm/pom.xml \
  algorithm/src/main/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategy.java \
  algorithm/src/main/java/com/mamba/landlord/core/model/Combo.java \
  algorithm/src/test/java/com/mamba/landlord/algorithm/shuffle/strategy/DefaultReshuffleDealStrategyTest.java \
  algorithm/src/test/java/com/mamba/landlord/benchmark/ScoringAndSplittingBenchmarkTest.java \
  algorithm/src/test/java/com/mamba/landlord/benchmark/ShuffleAndScoringBenchmarkTest.java \
  algorithm/src/test/java/com/mamba/landlord/core/model/ComboTest.java \
  docs/tech/algorithm-snapshot-plan.md \
  docs/plan/2026-09-04-dealing-algorithm-agent-foundation.md
git commit -m "fix(algorithm): sync verified dealing foundation"
```

## Deferred Phase C/D Gate

Do not create `dealing-algorithm-agent` Skill in this plan. First complete and retain five real task records covering:

1. 重洗返回分数一致性。
2. Java/C++ 连对长度一致性。
3. Java benchmark 隔离。
4. C++ Release/CTest 有效执行。
5. 至少一组房间策略的同种子基线与候选比较。

After those records show that the task contract, commands and output fields are stable, create a separate Phase C/D plan. That plan must use `superpowers:writing-skills` and create only `.agents/skills/dealing-algorithm-agent/SKILL.md`; add a wrapper script only if repeated manual command assembly has caused an observed error.
