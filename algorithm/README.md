# landlord-algorithm（只读快照副本）

> ⚠️ **只读**：本目录是 [landlord-algorithm](https://github.com/Samuel86-star/landlord.git) 的只读快照，归 landlord-analysis 参考用，**请勿在此修改**。任何改动请回到源仓库。

## 快照信息

| 项 | 值 |
|---|---|
| 源仓库 | https://github.com/Samuel86-star/landlord.git |
| 快照 commit | `e4be61a` (`codex/dealing-agent-foundation`) |
| 快照日期 | 2026-09-05 |
| 坐标 | `com.mamba.landlord:landlord-algorithm:0.0.1-SNAPSHOT` |
| 技术栈 | Spring Boot 4.0.3 / Java 21 + C++（native） |
| 对应方案 | [docs/tech/algorithm-snapshot-plan.md](../docs/tech/algorithm-snapshot-plan.md) |

## 这是什么

斗地主**算法实现**（发牌 / 拆牌 / 评分模拟），与 landlord-analysis（真实数据分析）互补：本目录提供算法逻辑依据，analysis 的房间设计（不洗牌 / cap / 底分）可据此校准。

## 模块速览

- **scoring/** — 牌力评分：牌型评分（`IComboScoringStrategy`）+ 手牌评分（`IHandCardsScoringStrategy`）；手牌强度已支持外部配置（`2c49dd3`）。
- **shuffle/** — 发牌 / 洗牌分布采样（`DealDistributionSampler`、各 `IDealStrategy`）。
- **splitter/** — 拆牌：飞机 / 炸弹 / 顺子优先拆解器。
- **core/model/** — Card / Combo / Deck / Rank / Suit 等领域模型。
- **native/** — C++ 高性能采样器（header-only `include/landlord.h` + `test/`）。
- **docs/** — 评价标准、发牌平衡 PRD、拆牌决策规则、测试策略、rules。

## 构建与验证

- 本仓只允许通过根目录 `python3 ops/py/verify_offline.py` 验证快照；生成物已加入忽略规则。
- 算法实现修改仍必须在权威源仓完成，验证后按 commit 差异同步到本目录。

## 更新方法

源仓库演进后：在 landlord-algorithm 提交并记录 source base/head → 按 commit 差异清单增量合入本目录并保留 analysis 专用资产 → 更新上方「快照 commit / 日期」→ 验证后在 analysis 提交。完整步骤见 [docs/tech/algorithm-snapshot-plan.md](../docs/tech/algorithm-snapshot-plan.md)。
