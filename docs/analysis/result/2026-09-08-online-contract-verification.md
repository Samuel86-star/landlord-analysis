# 线上配置、API 与房间映射核对记录

**状态：** 用户已确认 `makedeal.json` 与 `zgdatbl.cpp` 对应线上配牌配置和发牌源码；AC-ONLINE-01 的配置读取不匹配已证实。`/api/shuffle` 已确认是重构时用于测试的研究接口，本次不改花色响应契约。房间映射只读查询已完成，发现房间 10803 缺映射；历史有效期仍待证据。本次未修改线上代码或配置。

用户确认日期：2026-09-08；配置文件 SHA-256：`5cbef14f1da29ab6a15782fd25cccb10dcb765ec083d060591749d878e727f3a`。用户随后确认 `zgdatbl.cpp` 也是对应线上版本，其 SHA-256 为 `e5d761629a3a59202af39a0ad6ec156292758869aa19d94f2a9bb1cb98905c84`。部署身份依据为用户确认，尚无构建号或运行日志。

## 一、本地证据及结论边界

| 事项 | 已核对证据 | 结论 |
| ---- | ---- | ---- |
| AC-ONLINE-01 | [Local] `algorithm/native/previous/makedeal.json:784` 包含 `HandCardScore.v1` 和 `roomids`；[Local] `algorithm/native/previous/zgdatbl.cpp:11295` 的完整 `InitEvaluateSysForClassic` 读取 `BaseScore` 和 `ShuffleStrategy` | 用户确认部署对应后，可确认该初始化路径不会加载 v1 阈值；不将其扩大为整个配牌系统失效 |
| 配置到达方式 | [Local] `algorithm/native/previous/ConfigManagerSys.cpp:40` 和 `:153` 分别将订阅消息和启动同步内容写入配置缓存 | 只拿磁盘 JSON 不足以证明生效配置；需脱敏的发布版本与运行时配置证据 |
| API 响应 | [Local] `algorithm/src/main/java/com/mamba/landlord/controller/ShuffleAndDealController.java:51` 至该函数结尾 | 当前本地实现已有三家牌力、EHS、`reshuffled`、`reshuffleCnt`；不重复添加这些字段 |
| 花色 | 同一 Controller 的手牌和底牌使用 `rank().name()` 序列化 | 当前本地响应只含点数；用户确认是研究测试接口；保留现有点数响应，无生产花色契约改造 |
| 房间映射 | [Local] `starrocks/config/dq_game_room_config.md:50`、`:87` | 文档声明 DUPLICATE KEY，关联要求 `game_id + room_id`；需实际检查重复，文档不是线上唯一性证明 |

核对工具：`cat` 完整读取 Controller、ConfigManagerSys 和维表文档；Python `json.loads` 完整解析本地 JSON；`sed -n '11290,11390p'` 读取完整 `InitEvaluateSysForClassic` 函数。上述均为本地源码证据，部署对应关系由用户确认；未调取外部源仓或部署包。另完整读取 `StartDeal`（1310 至 1730 行），其 1340 行调用初始化，返回值未检查。

本地关键代码：

```cpp
auto baseScore = handCardScore["BaseScore"];
auto shuffleStrategy = handCardScore["ShuffleStrategy"];
```

```java
result.put("handDataPlayer0", evaluatedDealData.getHandCards(0).stream().map(c -> c.rank().name()).toList());
result.put("handStrengthPlayer0", evaluatedDealData.getHandStrengthPlayer0());
result.put("reshuffled", evaluatedDealData.isReshuffled());
result.put("reshuffleCnt", evaluatedDealData.getReshuffleCnt());
```

## 二、交给研发与运维的证据清单

| 负责人 | 需要提供的内容 | 收到后的判定 |
| ---- | ---- | ---- |
| 部署/配置负责人 | 实际部署 commit 或构建号、房间范围、脱敏的 `HandCardScore` 配置、发布版本及时间、运行时生效值、对应 loader 和发牌入口版本 | 对照键、默认值与调用路径，判为已证实故障/非故障；证据不足继续保留未确认 |
| API 定位（已完成） | 用户确认用于翻译/重构发牌逻辑时的研究测试 | 保留现有点数响应及牌力/重洗元数据，不新增对局出口契约 |
| 房间配置负责人 | 游戏/房间、玩法、等级、历史变更日期、有效起止与来源；11534 是否继续按客户端区分积分/比赛 | 当前维表唯一性检查只是前置条件；历史证据完整后才能制定历史 ETL 映射 |

2026-09-08 用户已确认配置/源码对应及研究接口用途；不再重复索要这些确认。仍需运行时生效值、构建标识和房间历史变更证据。未通过消息工具向其他负责人发消息。

## 三、房间映射只读预检

以下查询只返回聚合计数；事实范围为 `2026-06-25` 至 `2026-07-01` 的 53 游戏全部玩家记录。维表无日期分区，不假造有效期字段。按房间预聚合后计算原始 LEFT JOIN 的行数，可以发现重复维表造成的放大而不生成放大的明细。

永久 SQL 以本文为准，执行副本位于 `ops/py/tmp/20260908_handoff_room_mapping.sql`。已按用户提供的路径加载 `~/.zshrc` 中的 FlowOps 凭据。初次认证失败后核对实际网页前端，确认其对密码使用大写 MD5 摘要；按相同方式经 `sr_exec.py` 登录和查询成功，未保存凭据或认证响应。临时启动脚本仅映射环境变量并调用现有入口，没有新增查询通道。

```sql
-- file: 20260908_handoff_room_mapping.sql
WITH mapping_keys AS (
    SELECT game_id, room_id, COUNT(*) AS mapping_rows,
        COUNT(DISTINCT game_rule) AS rule_versions,
        COUNT(DISTINCT room_level) AS level_versions,
        SUM(CASE WHEN game_rule IS NULL OR TRIM(game_rule) = ''
            OR room_level IS NULL OR TRIM(room_level) = '' THEN 1 ELSE 0 END) AS empty_attributes
    FROM tcy_temp.dq_game_room_config
    GROUP BY game_id, room_id
),
reused_rooms AS (
    SELECT room_id
    FROM tcy_temp.dq_game_room_config
    GROUP BY room_id
    HAVING COUNT(DISTINCT game_id) > 1
),
fact_rooms AS (
    SELECT game_id, room_id, COUNT(*) AS fact_rows
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53 AND dt BETWEEN '2026-06-25' AND '2026-07-01'
    GROUP BY game_id, room_id
),
join_quality AS (
    SELECT COALESCE(SUM(fact.fact_rows), 0) AS fact_rows,
        COALESCE(SUM(fact.fact_rows * COALESCE(mapping.mapping_rows, 1)), 0) AS joined_rows,
        COALESCE(SUM(CASE WHEN mapping.mapping_rows IS NULL THEN fact.fact_rows ELSE 0 END), 0) AS unmapped_rows
    FROM fact_rooms fact
    LEFT JOIN mapping_keys mapping
        ON fact.game_id = mapping.game_id AND fact.room_id = mapping.room_id
)
SELECT /*+ SET_VAR(new_planner_optimize_timeout=15000) */
    (SELECT COUNT(*) FROM mapping_keys) AS mapping_key_count,
    (SELECT COUNT(*) FROM mapping_keys WHERE mapping_rows > 1) AS duplicate_keys,
    (SELECT COUNT(*) FROM mapping_keys WHERE rule_versions > 1 OR level_versions > 1) AS conflicting_keys,
    (SELECT COUNT(*) FROM mapping_keys WHERE empty_attributes > 0) AS incomplete_keys,
    (SELECT COUNT(*) FROM reused_rooms) AS cross_game_reused_room_ids,
    fact_rows, joined_rows, joined_rows - fact_rows AS inflated_rows, unmapped_rows
FROM join_quality;
```

**验收：** `fact_rows > 0`，`duplicate_keys/conflicting_keys/incomplete_keys/inflated_rows/unmapped_rows` 均为 0；跨游戏复用本身不是异常，关联必须保留 game_id。任一异常需定位房间后解释，不能用 `DISTINCT` 静默掩盖冲突。即使全部通过，历史有效期未确认仍不得替换历史玩法。

### 真实查询结果（2026-09-08，Asia/Shanghai）

| 指标 | 结果 |
| ---- | ---- |
| 映射键 / 重复 / 冲突 / 属性不完整 | 38 / 0 / 0 / 0 |
| 跨游戏复用房间 ID | 0 |
| 七天事实行数 / 原始关联行数 | 6,951,315 / 6,951,315 |
| 关联放大行数 / 未映射行数 | 0 / 3 |

未映射均为 `2026-06-25` 的 `room_id=10803`：真人 `robot=0, group_id=56` 1 行，机器人 `robot=1, group_id=1` 2 行。该窗口没有 join 放大，但不能宣布历史映射验收通过。定位 SQL 如下（真实运行仅返回上述两组）：

```sql
-- file: 20260908_unmapped_rooms.sql
SELECT game.dt, game.room_id, game.robot, game.group_id, COUNT(*) AS row_count
FROM tcy_temp.dws_ddz_daily_game game
LEFT JOIN tcy_temp.dq_game_room_config room
    ON room.game_id = game.game_id AND room.room_id = game.room_id
WHERE game.game_id = 53
  AND game.dt BETWEEN '2026-06-25' AND '2026-07-01'
  AND room.game_id IS NULL
GROUP BY game.dt, game.room_id, game.robot, game.group_id
ORDER BY game.dt, game.room_id;
```

执行入口：临时 `ops/py/tmp/20260908_query.py -f <SQL文件>` 在内存加载本地环境，最终通过 `runpy` 调用 `ops/py/sr_exec.py`。运行解释器为 Codex bundled Python 3.12（pandas），复用项目虚拟环境的 requests。两个查询均退出 0 且返回预期聚合列；没有执行任何写入或 DDL。

## 四、下一步与验证

- 配置键：`CONFIRMED_CONFIG_LOADER_MISMATCH`。配置契约不匹配定位于 `InitEvaluateSysForClassic`；修复方向是按当前房间解析 `HandCardScore.roomids → v1` 并验证字段，而不是修改八个房间调用点或直接替换成 Java 阈值。候选修改范围为权威线上源仓的该函数及配置加载回归；本地 `previous/` 保持参照不直接修改。
- API：`RESEARCH_INTERFACE_CONFIRMED`，作为翻译/重构后的研究测试接口保留现有契约，此项关闭；牌力/重洗字段已存在。
- 历史映射：真实查询完成，房间 10803 有 3 条未映射记录；历史改档证据仍待负责人提供，保持历史玩法 ETL。
- 查询离线检查见 `ops/py/test_handoff_room_mapping_sql.py`，验证聚合与关联逻辑；2026-09-08 经 StarRocks 实际运行确认语法和以下聚合结果，历史有效期尚未验证。

配置修复仍需确定所链接评分库和配置对象生命周期。当前 analysis 的 `algorithm/native/include/landlord.h:614` 使用进程静态配置，不能未经部署库核对就把按房间的阈值写入共享对象，避免房间间串用。发布前至少验证映射房间、未映射房间、缺失版本、非法类型、连续切房和热更新；保留生效值证据。只修读取键不等于发牌入口已使用过滤，须继续核对实际消费路径。

## 五、生产版本切换的前置定位

本次进一步核对得到两项实施约束，供计划任务 5/6 使用：

- [Local] `ops/py/batch_insert_ddz_daily_game.py:55` 直接提取 `$.card_power.card_power` 和 `card_power_final`，不在此计算牌力公式。仅重跑该 ETL 不会把旧牌力换算成新版。实际公式上线应追踪上游评分、日志写出及 JSON 到 DWS 的完整版本链。
- [Local] `algorithm/native/previous/zgdatbl.cpp:11387` 的 `GetHandPower` 调用 `calcHandStrengthScores`，但本次局部调用搜索只找到定义/声明，没有取得评分值写入 `extend_content` 的完整线上证据；不据此生成“已定位生产计分入口”的补丁。
- [Local] `docs/knowledge/makedeal-simulation.md:93` 引用了 2026-07 公式变化；仅有该口径提示及当前 PRD，尚不能固定线上旧/新公式版本和预期分数。第二个切点日期保持未发布，不能用本次日期代替生效日期。
- [Local] `algorithm/src/main/resources/application.properties:49` 起，潜在地主/地主优势为 `Infinity`，单牌/炸弹为 `0`；这是当前本地配置，不是新作出的“不启用”产品决定。仍待产品逐项决定适用房间、保护阈值及性能预算，未开启新增标定。

后续发布记录至少包含：计分源 commit、配置摘要、日志格式、ETL commit、实际生效时间/分区、数据版本、同手牌旧新预期结果及只读核验。若同日存在两个版本，按真实版本分开分析或选择完整分区边界切换，不能直接比较跨版本牌力。
