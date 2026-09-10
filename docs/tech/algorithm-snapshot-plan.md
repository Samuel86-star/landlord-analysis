# algorithm 单仓治理与迁移记录

## 当前决策

自 2026-09-10 起，`landlord-analysis/algorithm/` 是算法代码的唯一权威位置。算法实现、测试、配置和文档直接在 `landlord-analysis` 提交，不再维护源仓与只读快照之间的同步流程。

文件名保留不变，是为了维持历史链接；原“只读快照存放方案”已经停用。

## 合并结果

| 项 | 结果 |
| ---- | ---- |
| 主仓库 | [Samuel86-star/landlord-analysis](https://github.com/Samuel86-star/landlord-analysis) |
| 算法模块 | `algorithm/` |
| 原算法仓库最终提交 | `Samuel86-star/landlord@b729ef0` |
| 原算法仓库状态 | 保留历史并归档，不再接收新修改 |
| analysis 专用资产 | `native/previous/`、`native/extracted/`、`native/tools/` 等全部保留 |

合并时以主仓已有 `f2dbcf6` 快照为基线，纳入原算法仓库 `f2dbcf6..b729ef0` 的最终差异。三份算法文档此前已收敛，本次同步两处 Javadoc、测试包迁移及改名，并删除孤立的 `docs/prompt.md`。

## 当前工作流

1. 在 `landlord-analysis` 创建分支并直接修改 `algorithm/`。
2. 同时涉及 Java/C++ 的规则时更新两端和共同测试向量。
3. 涉及线上参照 `algorithm/native/previous/` 时记录来源版本、同步时间和影响。
4. 在仓库根目录运行 `python3 ops/py/verify_offline.py`。
5. 与分析代码一起提交到 `landlord-analysis`。

## 回退与历史

原 `landlord` GitHub 仓库保留完整提交历史，最终权威点为 `b729ef0`。如需核查迁移前状态，可按该提交读取；恢复双仓模式必须作为新的治理决策实施，不能继续使用本文已停用的快照同步流程。
