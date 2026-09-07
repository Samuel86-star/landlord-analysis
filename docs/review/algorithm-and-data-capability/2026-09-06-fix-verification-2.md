# 整改验证审查 · 第二批（2026-09-06）

对照 [风险清单](2026-09-06-review.md) 与 [第一批验证](2026-09-06-fix-verification.md) 约定的下一刀：可复现标定、整手向量、空牌面过滤。核对本仓 `0027d4e`、`dcd91b7` 与算法快照 `5395ca8`。

本次只做静态复核，并复跑 `ops/py/test_hand_cards.py`（5/5）。未复跑完整 `verify_offline.py`，未跑 20 万局采样，未访问公司数仓，未执行 DELETE/INSERT。提交说明中的 Java 67 / Python 33 / 定向 5 / C++ 1 项采信为提交方证据。

本文仍是风险清单，不是实施方案。

## 结论

约定的第二刀源码与脚本项已经落地，方向正确。生产配置现为 `-66 / 75 / 112`，采样入口带固定种子，整手向量覆盖上次点名的三带与长顺含炸 2，空 `hand_cards` 不再进入「无炸 / 无王」。四带二 helper 先扣四张再选翼。

还不能宣布「线上过滤效果已验证」或「历史报表已干净」。种子 `20260907` 的全表没有进仓库；PRD 把种子 `20260906` 的详细分位与双种子保守上界写在同一段输出框里；`2026-06-25~07-01` 只写了回填命令。本轮未访问数仓，只能确认历史分区未被本次整改回填、当前库内状态未证，不能仅凭源码断言库内一定仍是旧值。无 scorer 单路径按约定保留为降级，不是回退。

## 本批关闭项

| 标识 | 证据状态 | 验证 |
| ---- | ---- | ---- |
| AC-THOLD-01 写回 | `SOURCE_PROVEN` → 关闭（配置层） | `application.properties` 为 `-66 / 75 / 112`；注释写明 2026-09-06 双种子各 10 万局。潜在地主 / 地主优势仍 `Infinity`，与产品「不启用」一致，注释分别记了 `95.0` 与 `113.5/114.0` |
| AC-THOLD-01 可复现入口 | `SOURCE_PROVEN` → 关闭（入口） | `sampleBaselineDistribution` 读 `-Dlandlord.sampler.seed` / `rounds`，默认 `20260906` / `100000`；用 `DefaultShuffleDealStrategy(seed)` 且过滤关闭。`seededShuffleShouldBeReproducible` 锁同种子牌序 |
| AC-TEST-02 | `SOURCE_PROVEN` → 关闭（约定样本） | Java/C++ 与向量表同步：`333`、`555KK`、`KKKAA`、`AAA33`、长顺+炸 8、长顺+炸 2。断言 combo 类型或「含指定炸弹 + 张数守恒」，没有锁整手 `V_total` |
| AC-PARSE-02 | `SOURCE_PROVEN` → 关闭 | 空串 / `None` / 空白 → `bomb_cnt`、`king_status` 为缺失；模块 E 用 `bomb_cnt.notna()`，模块 G 用 `king_status.notna()`；`has_bomb` 对缺失保持 NA。单测覆盖 |
| AC-SPLIT-03 helper | `SOURCE_PROVEN` → 关闭（helper） | Java/C++ `extractQuadsWithWings` 先 `count[idx] -= 4` 再收集翼，失败回滚。`hand-3333-44` 由 `QuadProbe` 锁，不是 `DefaultComboExtractor` |

快照指针：`algorithm/README.md` 现为 `5395ca8`（2026-09-06）。分析仓相对 `origin/main` 超前 3 个提交：`453d58a`、`0027d4e`、`dcd91b7`。

## 标定是否按约定做成

### 已做成的部分

- 种子构造器：`DefaultShuffleDealStrategy(long seed)` 使用 `new Random(seed)` + `Collections.shuffle`。无参构造仍走 `RandomGenerator.getDefault()`，只给生产随机路径。
- 采样命令写进技术 PRD，可复跑：

```bash
./mvnw -q -Pbenchmark -Dtest=ShuffleAndScoringBenchmarkTest#sampleBaselineDistribution \
  -Dlandlord.sampler.seed=20260906 -Dlandlord.sampler.rounds=100000 test
```

- 种子 `20260906` 的详细分位已粘贴：min P5=`-66`，max P95=`75`，spread P95=`111`，potential P95=`95`，advantage P95=`113.5`。
- 生效三维取双种子保守包络：`lower=-66`、`upper=75`、`max-spread=112`。这与「seed1 spread=111，配置写 112」不矛盾，前提是 seed2 的 spread P95 为 112。

### 留档仍不完整

| 缺口 | 证据 | 影响 |
| ---- | ---- | ---- |
| 种子 `20260907` 无全表 | 全仓库只有一处点名该种子，没有第二份 P5–P99 | 无法从仓库独立核对「保守上界」是否真取了两组的更极端值 |
| 同一输出框混了两种数 | PRD 先写「下列为种子 `20260906` 的输出」，推荐阈值块却是 `max-spread=112`、`advantage=114`，与该次详细分位 `111` / `113.5` 不一致 | 读者会以为 112/114 是单次 sampler 打印。`printRecommendedThresholds()` 对单次运行会打出 111.0 / 113.5 |
| 无机器可读产物 | 没有 JSON/CSV，没有首局牌序哈希 | 换 JDK 或 `Random` 算法后，只能靠「再跑 20 万局」对账 |
| C++ 未按同协议标定 | `sampler_test.cpp` 仍是无种子 1 万局；跨语言向量写明发牌 RNG 协议未对齐 | 阈值只对 Java `java.util.Random` 可复现 |
| 重洗策略本身仍无种子 | `DefaultReshuffleDealStrategy` 没有 seed 构造器 | 基线分布可复现；带过滤的拒绝率 / forced-accept 仍不能按种子重放 |

本轮未复跑 20 万局，因此 `-66/75/112` 的数值采信提交方与 PRD 粘贴，不在本机独立复验。

生产过滤仍只开三维：17 张 `V_total` 的 lower / upper / spread。潜在地主、地主优势、单牌数、炸弹数保持关闭。这是产品选择，不是本批漏改。

## 整手向量覆盖了什么

| 编号 | 手牌 | 断言入口 | 锁住的行为 |
| ---- | ---- | ---- | ---- |
| hand-isolated-333 | `333` | `DefaultComboExtractor` | 裸三张，不自带对 |
| hand-555-kk / kkk-aa / aaa-33 | 三带一对 | 同上 | `TRIPLE_WITH_PAIR(主, 翼)`，翼是另一点数 |
| hand-straight-bomb-8 | 长顺 + 四张 8 | 同上 | 最终拆法含 `BOMB(8)`，长度和 = 17 |
| hand-straight-bomb-2 | 长顺 + 四张 2 | 同上 | 最终拆法含 `BOMB(2)`，长度和 = 17 |
| hand-3333-44 | `333344` | `QuadProbe` 只调 `extractQuadsWithWings` | `QUAD_WITH_TWO_SINGLES(3,4,4)` |

`555+KK` 一类已从「走读应正确」变成回归锁。长顺含炸 2 补上了第一批缺口。

两点不要写过满：

1. **没有锁整手 `V_total`。** 评分层 `calcTotalHandScore` 仍不校验 combo 是否剖分手牌。张数守恒只在测试里兜，生产路径仍可能静默吃非法拆法。
2. **`hand-3333-44` 不是生产拆牌合同。** `PlaneBombPrioritizedSplitter` 先 `extractAllBombs`，两条生产路径都不会对这手牌走出四带二：飞机路径是 `BOMB(3)+PAIR(4)`，顺子路径后提炸弹。向量表写在「整手拆牌」节，容易被当成 `DefaultComboExtractor` 的期望。它锁的是 helper 不自吃，这个目的成立。

四带二 helper 已先扣四张。生产飞机路径在提完炸弹后，没有任何点数还剩 4 张，`extractQuadsWithWings` 仍是死代码。修复防的是改序或探针，不是当前 `V_total` 路径的活动故障。与第一批判断一致。

## 空牌面与回填

解析器：空 / 非字符串 / 空白 → `[]` → `count_held_bombs` / `parse_king_status` 返回 `None`。非法 token 仍抛 `ValueError`，脏行会让整次分析退出。这是残留，不是本批回退。

模块 E / G 用同一套缺失过滤，空牌面不再进无炸、无王分母。本机 5/5 单测通过。

历史窗口命令已写入 `ops/py/first-classic-beginner/README.md`，只覆盖 `dws_ddz_daily_game` 的 `20260625–20260701`：先 `--dry-run`，再回填，再重跑分析。提交方确认本次整改未执行 DELETE/INSERT。本轮未访问数仓，因此只证明「未被本次整改回填」，不证明库内现值。`dws_app_allgame_stat` 的历史 `bomb_0_games` 仍无对等手册。

## 对提交说明的核对

| 提交方说法 | 本次核对 |
| ---- | ---- |
| 权威源码 `5395ca8`：双种子各 10 万局，阈值 `-66/75/112` | 快照指针、`application.properties`、PRD 与采样入口一致。seed2 全表与本机复跑不在仓库 |
| 补齐整手跨语言向量 | 表与 Java/C++ 用例同步；四带二走探针 |
| 修复四带二自吃 | helper 源码与探针测试成立；生产路径仍先提炸弹 |
| 分析仓模块 E 排除空 `hand_cards` | `hand_cards.py` + `run_analysis.py` + 单测成立 |
| Python 33、Java 67、定向 5、C++ 1 项通过 | 采信提交方；本轮只复跑手牌解析 5/5。定向 5 即 `SplitterRegressionTest` 五个用例 |
| 未执行数仓 DELETE/INSERT；命令已写入 README | README 命令存在；本次未回填已确认；库内现值未证 |
| 无 scorer 单路径保留为明确降级 | `DefaultComboExtractor` 无参仍走工厂单路径，类注释已写明 |

未把「配置已写回」写成「双种子全表可独立复核」，也未把「回填命令已写」写成「历史分区已干净」。

## 仍开放

| 标识 | 严重度 | 证据状态 | 问题 |
| ---- | ---- | ---- | ---- |
| AC-THOLD-02 | 过程 / P2 | `SOURCE_PROVEN` | seed2 全表未留档；PRD 推荐块与 seed1 详细分位混排；无机器可读产物 |
| AC-SCORE-01 | 未变 | `SOURCE_PROVEN` | `calcTotalHandScore` 仍不校验剖分 |
| AC-ETL-01b | P2 | `SOURCE_PROVEN` | `bomb_1/2/3+` 仍用浮点 `bomb_bet / 2 = n` |
| AC-ETL-03 | 运维 | 脚本已修；本次未回填；库内现状未证 | `2026-06-25~07-01` 须按 README 预览后再回填；`allgame_stat` 历史无炸桶另算。不能仅凭源码断言库内一定仍是旧值 |
| AC-PARSE-03 | P2 | `SOURCE_PROVEN` | 非法 token 仍让分析进程退出 |
| AC-ONLINE-01 | 未纳入 | `NEEDS_ONLINE_CONFIRM` | 快照 JSON 键与 loader 仍对不上；先问线上 |
| AC-API-01/02 | 未纳入 | `DOC_INFERRED` | 取决于 `/api/shuffle` 是研究接口还是对局出口 |
| play_mode 维表 | 未纳入 | 先核历史 | 维表唯一性与有效期未确认 |
| DA-REL-02 | 未变 | 公司网 | 回填仍是独立 DELETE → INSERT → COUNT |

无 scorer 单路径与四带二生产死代码按约定不升格。

## 建议的下一刀（仍不是任务拆解）

1. **按 README 预览后回填 `2026-06-25~07-01`。** 源码项不再阻塞这条数据项。先 dry-run，对过 `shuffle_times` 缺失与空手牌覆盖，再 INSERT，再重跑 `first-classic-beginner`。
2. **把种子 `20260907` 全表补进 PRD 或独立 JSON，** 并写明保守包络规则（各组 P5 取更负、P95 取更大）。当前配置数字可以继续用，只是留档不完整。
3. **其余继续按原清单确认：** `play_mode` 维表历史、线上 `makedeal` 接线、`/api/shuffle` 定位。本次回填完成并核过覆盖之前，不应比较新旧炸弹持有率。
4. 联合拒绝率 / forced-accept、20 张地主维、对局反证作业，仍排在本次窗口回填并核过之后。

一句话：**牌力真值与可复现标定入口已经在快照里闭合；下一刀是回填 06-25~07-01，并把第二颗种子的全表留下，不是再改拆牌。**
