## 测试覆盖说明与运行方式

### 1. 单元测试与集成测试覆盖范围

- **牌型评分（scoring）**
  - **`DefaultComboScoringStrategyTest`**
    - 覆盖炸弹、王炸与普通牌型的相对大小关系。
    - 验证同一牌型中，高点数牌型得分高于低点数牌型（例如 A 对 > 3 对）。
    - 验证顺子长度增加、飞机带翅膀时得分随之提升。
  - **`DefaultHandCardsScoringStrategyTest`**
    - 构造“弱牌”（小单牌为主）与“强牌”（包含 2、炸弹、王炸）两组手牌，验证强牌总分明显高于弱牌。
    - 在相同牌面集合下，对比“全部拆成散牌”与“顺子 + 炸弹”的两种拆法，验证结构更好的拆法得到更高总牌力。

- **洗牌与发牌（shuffle / deal）**
  - **`DefaultShuffleDealStrategyTest`**
    - 验证 `shuffle()`：
      - 返回一副 54 张牌。
      - 洗牌结果与 `Deck.copyFullDeckCards()` 在集合意义上完全一致（只改变顺序，不增不减）。
    - 验证 `dealCards()`：
      - 按 17 / 17 / 17 / 3 的规则分配给三家与底牌。
      - 合并三家手牌与底牌后，共 54 张且不重复、不丢失。
    - 验证 `shuffleAndDeal()`：
      - 返回 `EvaluatedDealData`，三家手牌长度为 17、底牌长度为 3。
      - 三家牌力值字段为非 NaN。
      - 默认策略不包含重洗逻辑：`reshuffled=false`，`reshuffleCnt=0`。

- **带重洗逻辑的发牌（reshuffle）**
  - **`DefaultReshuffleDealStrategyTest`**
    - 通过 `ShuffleStrategyDecisionProperties` + `ShuffleStrategyDecisionHolder` 注入配置，覆盖两类场景：
      - **关闭过滤**：`enabled=false`，验证行为等价于默认策略，不发生重洗（`reshuffled=false`、`reshuffleCnt=0`）。
      - **开启过滤且配置最大重洗次数**：验证 `reshuffleCnt` 始终在 \[0, maxReshuffleTimes] 范围内，防止实现错误导致无限重洗或超出上限。

- **HTTP 控制层（controller）**
  - **`ShuffleAndDealControllerTest`**
    - 直接调用 `ShuffleAndDealController.shuffleAndDeal()`。
    - 验证返回的 `Map<String, Object>` 结构：
      - `handDataPlayer0` / `handDataPlayer1` / `handDataPlayer2` 长度均为 17。
      - `bottomCards` 长度为 3。
      - `handStrengthPlayer0` / `handStrengthPlayer1` / `handStrengthPlayer2` 字段非空。
      - `reshuffled` 与 `reshuffleCnt` 字段非空。

### 2. Benchmark / 模拟测试（仅在 profile 下运行）

- **`ShuffleAndScoringBenchmarkTest`**
  - 使用 `@Tag("benchmark")` 标记，仅在开启 benchmark profile 时运行。
  - 通过配置 `ShuffleStrategyDecisionProperties`：
    - 启用过滤（`enabled=true`），设置合理的上下阈值与最大重洗次数。
  - 使用 `DefaultReshuffleDealStrategy` 连续执行多轮 `shuffleAndDeal()`：
    - 统计所有对局的重洗总次数，计算平均重洗次数 `avgReshufflesPerDeal`。
    - 使用极宽松的断言（`avgReshufflesPerDeal >= 0.0`）作为基本健壮性检查，核心用途是观察运行时间与重洗频率的粗略基线。

- **`ScoringAndSplittingBenchmarkTest`**
  - 使用 `@Tag("benchmark")` 标记，仅在开启 benchmark profile 时运行。
  - 覆盖 Combo 评分、整手牌评分、单路径拆牌、双路径提取及完整洗牌发牌的吞吐量基线。

  Benchmark 吞吐量阈值取决于记录基线的机器性能，仅用于性能观察，不是正确性断言。

### 3. 测试运行命令

- **日常开发：只跑普通单测（不含 `ShuffleAndScoringBenchmarkTest` 与 `ScoringAndSplittingBenchmarkTest`）**

```bash
mvn test
```

- **带 benchmark 的完整回归（包含 `ShuffleAndScoringBenchmarkTest` 与 `ScoringAndSplittingBenchmarkTest`）**

```bash
mvn test -Pbenchmark
```

**Native 算法测试**

```bash
cmake -S native -B native/build-release -DCMAKE_BUILD_TYPE=Release
cmake --build native/build-release --target landlord_test
ctest --test-dir native/build-release --output-on-failure
```

说明：

- 默认情况下，`pom.xml` 中的 `maven-surefire-plugin` 会通过：
  - `<excludedGroups>benchmark</excludedGroups>` 排除所有带 `@Tag("benchmark")` 的测试。
- 当通过 `-Pbenchmark` 启用 benchmark profile 时：
  - 会在该 profile 中覆盖 surefire 配置，清空 `excludedGroups`，从而让 benchmark 测试也参与执行。
