# 整改验证审查（2026-09-06）

> 本文只覆盖第一批（`453d58a` / `a4b53b9`）。第二批见 [2026-09-06-fix-verification-2.md](2026-09-06-fix-verification-2.md)。

对照 [2026-09-06 风险清单](2026-09-06-review.md) 二次复核后的落地顺序，核对本仓 `453d58a` 与算法快照 `a4b53b9`。本次只做静态复核，并复跑 `ops/py/test_hand_cards.py`（4/4）。未复跑完整 `verify_offline.py`，未访问公司数仓。提交说明中的 Java 72 / Python 32 / CTest 1/1 采信为提交方证据。

本文仍是风险清单，不是实施方案。

## 结论

约定的第一批源码项已经落地，方向正确：先修牌力真值，再谈采样与线上。生产注入 scorer 的路径现在会先扣三条再选翼、永远双路径取优、不再共享 `segmentResult`。本仓口径脚本与两份留存模板也已按清单收口。

还不能宣布「牌力闭环完成」。整手回归只锁了两个样本；空 `hand_cards` 仍会被算成无炸；10 万局采样会随 benchmark 跑，但没有种子、留档或写回配置；已落库分区不会被这次脚本改写。

## 本批关闭项

| 标识 | 证据状态 | 验证 |
| ---- | ---- | ---- |
| AC-SPLIT-01 | `SOURCE_PROVEN` → 关闭 | Java/C++ 均先 `count[r] -= 3` 再 `collectPairs` / `collectSingles`。孤立 `333` 回归断言裸三张，且 combo 长度和等于手牌张数 |
| AC-SPLIT-02 短路 | `SOURCE_PROVEN` → 关闭（现象层） | 注入 scorer 时不再读 `confidentOut`，永远跑飞机/炸弹与顺子两条路径比分。未改顺子路径「先提全部炸弹」 |
| AC-CONC-01 | `SOURCE_PROVEN` → 关闭 | `segmentResult` 实例字段已删，`findLongestConsecutiveSegment` 返回 `new int[]{startIdx, maxLen}` |
| AC-ETL-01 无炸桶 | `SOURCE_PROVEN` → 关闭 | `bomb_bet <= 1` 计入 `bomb_0_games`；表示例改为 `bomb_bet > 1` |
| `shuffle_times` 缺省 | `SOURCE_PROVEN` → 关闭（脚本） | `IFNULL(..., -1)`，未开启与未重洗可分 |
| 手牌解析 | `SOURCE_PROVEN` → 关闭（解析器） | `hand_cards.py` 同时吃逗号分隔与紧凑串，`10`/`T`/`sj`/`bj` 有单测；分析 SQL 已 SELECT `hand_cards` |
| AC-CAL-01 / 防零除 | `SOURCE_PROVEN` → 关闭（这两份模板） | `retention-global.md`、`retention-by-mode.md` 的 D7/D30 改为 `+6`/`+29`，百分比用 `NULLIF` |

快照指针：`algorithm/README.md` 现为 `a4b53b9`（2026-09-06）。Spring 生产 Bean 注入带 scorer 的 `DefaultComboExtractor`（`AlgorithmConfig.java:52-54`）。

## 关键实现是否按约定修

### 三带：先扣主牌，不是排除列表

`AbstractHandSplitter.java:229-243` 与 `landlord.h` 同构：先减 3，再在剩余计数里选翼。孤立三条不再自吃。`555+KK`、`KKK+AA`、`AAA+33` 按走读应变正确，但**没有回归锁住**。

评分层仍不校验 combo 是否剖分手牌。本批靠测试里的 `length()` 求和兜住两个样本，生产路径仍可能静默吃非法拆法。

### 双路径：删短路，不改顺子提取顺序

`DefaultComboExtractor` Java/C++ 在有 scorer 时直接双路径比分。`StraightPrioritizedSplitter` 仍是顺子 → 裸三 → 对 → 单 → 炸弹。这符合二次复核：「不要先让顺子路径提全部炸弹」。

回归 `strongStraightStillComparesBombPreservingSplit` 用 17 张：`3–A` 各至少 1 张 + 四张 8 + 2 + 小王。断言最终拆法含炸弹 8，且张数守恒。这足以证明「高置信不再挡住炸弹路径」，不足以给出 `V_total` 分差，也没有覆盖四张 2。

### 无 scorer 回退仍是单路径

`DefaultComboExtractor()` 与 `DefaultSplitterFactory.extractAllCombos` 仍走 `chooseStrategy`（C++ 仍写 `chooseStrategyWithConfidence`）。生产 Spring 路径不走这里。谁直接调工厂，高置信短路还在。这是残留脚踏，不是本批回退。

## 本仓口径

### 已修对

- `batch_insert_allgame_stat.py:81`：`bomb_bet <= 1` 作为无炸。
- `dws_ddz_daily_game.md:401`：`bomb_bet > 1` 才算有炸。
- `batch_insert_ddz_daily_game.py:59`：缺失 `shuffle_times` 写成 `-1`。
- `hand_cards.py`：逗号串与紧凑串；`T`→`10`；非法 token 抛错。
- 持有炸 = 四张同点点数个数 + 王炸，与「拆牌炸弹」分开。
- 分析管线不再读已删除的 `bomb_cnt` JSON。

### 仍开放或新发现

| 标识 | 严重度 | 证据状态 | 问题 |
| ---- | ---- | ---- | ---- |
| AC-PARSE-02 | P2 | `SOURCE_PROVEN` | 空串 / 非字符串 → `tokenize` 返回 `[]`，`count_held_bombs=0`，`parse_king_status=无王`。模块 E 对全表 `map`，不排除空牌面；模块 G 才过滤。06-25 覆盖约 92.5% 时，E 会把缺失当成无炸 |
| AC-ETL-01b | P2 | `SOURCE_PROVEN` | `bomb_1/2/3+` 仍用浮点 `bomb_bet / 2 = n`。合法偶数编码（2/4/6）能命中；脏值 `3` 会漏出所有桶。更稳的是 `bomb_bet DIV 2` |
| AC-ETL-03 | 运维 | 脚本已修；本次未回填；库内现状未证 | 本次提交不会改写已落库分区；当前取值未访库核验，须重跑对应 `batch_insert_*` 后才能关闭数据项 |
| AC-TEST-02 | P2 | `SOURCE_PROVEN` | 整手向量只有孤立三条 + 长顺含炸 8。清单里点名的 `555+KK`、`KKK+AA`、`AAA+33`、四张 2 未进测试；`cross-language-regression-vectors.md` 仍只有手写 Combo 分 |
| AC-SCORE-01 | 未变 | `SOURCE_PROVEN` | `calcTotalHandScore` 仍不校验剖分 |
| AC-SPLIT-03 | 未变 | `SOURCE_PROVEN` | `extractQuadsWithWings` 仍在扣四张前 `collectPairs`，生产飞机路径先提炸弹，仍是死代码隐患 |
| AC-THOLD-01 | 标定未完成 | 路径 `SOURCE_PROVEN`；数字待留档 | 完整离线检查会跑 `sampleBaselineDistribution`（10 万局，无种子）。生产配置仍是 `-68/74/112`（`application.properties:43-47`），仓库无本次采样产物。提交方口述约为 `-65/74/111`，本轮无法从仓库独立复验 |

未纳入本批、也未回退的项仍维持原证据状态：`play_mode` 维表化（先核历史）、AC-ONLINE-01（问线上）、API 定位、`DA-REL-02`。

## 对提交说明的核对

| 提交方说法 | 本次核对 |
| ---- | ---- |
| 源仓 `a4b53b9` 修三带、短路、`segmentResult`，Java/C++ 回归 | 快照源码与测试文件一致 |
| 分析仓 `453d58a` 同步快照，并修炸弹桶、`shuffle_times`、手牌解析、D7/D30、NULLIF | 文件级一致 |
| Java 72 / Python 32 / CTest 1/1 / 离线检查通过 | 采信提交方；本轮只复跑 `test_hand_cards` 4/4。CTest 仍是 1 个可执行文件，内部新增 2 个用例 |

未把「测试通过」写成「阈值已重标定」或「历史 DWS 已干净」。提交方称修复后采样约为 `-65/74/111`：benchmark 路径能跑出这类数，但仓库没有日志或文件可对。

## 建议的下一刀（仍不是任务拆解）

1. **把已有的临时采样升级为可复现阈值标定。** 不是「从没跑过」，而是缺固定种子、独立复验、结果留档，以及是否把 `-65/74/111` 写进配置的明确决策。生产仍用 2026-07-31 基线 `-68/74/112`。
2. **补整手向量。** 至少加上 `555+KK`、`KKK+AA`、四张 2 + 长顺；断言 combo 序列或 `V_total`。
3. **空牌面不要进持有炸分母。** 空或非法时 `bomb_cnt` / `king_status` 为缺失，模块 E 与 G 用同一覆盖过滤。
4. **然后才安排历史回填。** 脚本已改的 `shuffle_times`、无炸桶，须重跑 `batch_insert_*` 才算数据项关闭。
5. 其余（维表、线上 JSON、API 定位）继续按原清单。无 scorer 单路径与四带二自吃不是当前 Spring 生产路径的活动故障。

一句话：**主故障链已经从快照拿掉；下一刀是把 10 万局采样做成可复现标定，并补整手向量和空牌面过滤，不是再跑一遍无种子 benchmark。**
