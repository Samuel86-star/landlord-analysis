# algorithm/ 目录全景（线上系 vs 新算法系）

> 回答"previous / extracted / src / native/test / include 各是什么、什么关系"。接手 algorithm/ 相关工作前先读这篇；模拟口径与工具用法详见 [makedeal-simulation.md](makedeal-simulation.md)。
> 本文事实均经代码级核查（2026-09-09）。

## 一、一句话定位：两套独立血统

| | 线上系 | 新算法系 |
| ---- | ---- | ---- |
| 回答的问题 | 线上**现在**怎么发牌、会产生什么分布 | 新算法（拆牌/评分/发牌过滤）**怎么设计** |
| 语言 | C++ | Java（`src/`）+ C++ 孪生（`native/include/landlord.h`） |
| 目录 | `native/previous/`、`native/extracted/`、`native/tools/` | `src/`、`native/include/`、`native/test/`、`native/config/`、`pom.xml`、`docs/` |
| 治理 | **本仓自有资产，可改**（previous 改动=线上同步意图，commit 注明） | **本仓权威算法模块，可直接修改并验证** |
| 发牌哲学 | **做牌**：配置驱动凑牌型（CouPaiStrategy 逼近 TargetValue） | **随机+过滤**：均匀洗牌→评分→极端局重洗（PRD 2026-02） |

两系唯一交点：`extracted/harness.cpp` 借用 `include/landlord.h` 的规范拆牌器+评分**算指标**（仅指标层，发牌管线不含它）。

## 二、线上系：还原与验证链条

```text
线上服务器（真实在跑，无法直接观测内部）
   │ 原样拷贝发牌相关编译单元（22a147b 入库，09-08 同步线上现行版 454b611）
   ▼
native/previous/  线上代码参照（纯参照，编译不成程序）
   │ 1:1 逐字剥离发牌/配牌/拆牌/洗牌/随机管线 + Stub 框架依赖
   ▼
native/extracted/harness.cpp（单文件模拟器，自带 main）
   × --cfg ../previous/makedeal.json（房间路由/策略/评分系数全从配置读）
   ▼
harness.exe → JSONL 逐局样本 → tools/（sweep/anchor_check/stats）→ 基准分布
```

### previous/ 为什么没法直接跑

| 文件 | 行数 | 内容 |
| ---- | ---- | ---- |
| `zgdatbl.cpp/.h` | 10,142 | 大杂烩：`CGameTable : public CSkTable` 桌子类、比赛计时、存档补偿等桌务逻辑；发牌相关（`SvrXygRandomSort` 洗牌、CouPaiStrategy 选组）只占一小块 |
| `MakeDealHelper.cpp/.h` | 1,692 | 做牌核心（Type0 控制流 / Type1 CouPaiStrategy 拼牌）+ 递归拆牌逼近目标值 |
| `ConfigManagerSys.cpp/.h` | 211 | 配置管理单例（从服务器配置体系加载 json） |

三个 cpp 均无 `main()`，且长在服务器框架上（`CSkTable` 继承、UWL 日志、`CPlayer*`、JsonCpp、Windows API）——是服务器的**零件**，不是程序。

### extracted/ 是什么

- `harness.cpp`：把上述约 1.2 万行里的**发牌/配牌/拆牌/洗牌/随机**管线**逐字照抄**（含原笔误与 rand/srand），仅用极简 Stub 切断 Table/Player/Config/Windows 依赖，配 `main()` 与 JSONL 输出。**对应线上现行版**（09-01 优化：GroupDataExp 缓存 + memo + 节点预算；2026-09-09 单一化，与优化前版本 A/B 证逐行一致）。
- 指标层为 analysis 自有追加：`optimal_split.h`（搜索式最优拆牌）+ 借用 `landlord.h` 评分；发牌管线本身不含。
- 其余四区：`tools/`（长期工具链）、`runs/`（历史实验脚本）、`results/`（报告与缓存），见 [extracted/README.md](../../algorithm/native/extracted/README.md)。

### makedeal.json 的角色

运行时 `--cfg ../previous/makedeal.json`：RoomRule 房间→策略路由、CouPaiStrategy、GroupDataExp 评分系数全从这份配置读——**配置一变，模拟跟着变**。当前为 2026-09-08 同步的线上态（420/4484/12074/1404→new5、742→new3、6314/11168→new6），复算历史结论时注意配置版本（pre-9.1 快照在 `extracted/results/makedeal_pre91.json`）。

### 基准数据的可信度

同一洗牌算法 → **概率分布忠实**（具体某局不可复现：种子源与线上不同）。已对齐线上实测：742 `new` 持有炸 0.137 ↔ 线上打出 `bomb_bet` 单家 ≈0.13（持有略高于打出，自洽）。

## 三、新算法系：landlord-algorithm 模块

| 目录 | 内容 |
| ---- | ---- |
| `src/main/java/com/mamba/landlord/` | Spring Boot 4 / Java 21 算法服务：`scoring/`（牌型+手牌评分）、`splitter/`（拆牌）、`shuffle/`（发牌分布采样：均匀洗牌 + `DefaultReshuffleDealStrategy` 极端局重洗过滤）、`core/model/`（Card/Deck/Combo 领域模型）、`controller/` |
| `native/include/landlord.h` | **同一套算法的 C++ 高性能孪生**（header-only，`namespace landlord`）；Java/C++ 同步维护（如 `calcTotalHandScore` 双端校验） |
| `native/test/` | 测的是 `landlord.h`：`main_test.cpp` 单测、`sampler_test.cpp` 采样入口；配置用 `native/config/scoring.properties`——**与 makedeal.json、extracted 均无关** |
| `algorithm/docs/` | PRD（发牌均衡过滤系统 2026-02）、评价标准、拆牌决策规则、测试策略 |

治理：`src/`、`include/`、`test/`、`pom.xml`、`algorithm/docs/`、`native/CMakeLists.txt` 均由 `landlord-analysis` 直接维护。原独立仓库最终提交为 `b729ef0`，已停止作为修改入口；当前规则见 [algorithm/README.md](../../algorithm/README.md)。

## 四、常见误读澄清（均已代码级核查）

| 误读 | 事实 |
| ---- | ---- |
| "`src/` 是 extracted 的 Java 版" | 两系无血缘：线上关键词（CouPaiStrategy/MakeDealByCfg/SvrXygRandomSort）在 `src/`、`include/`、`test/` **零命中**；代码断代（UWL/Windows 遗产 vs C++11/Java21）；发牌哲学相反（做牌 vs 随机+过滤） |
| "新算法系是 extracted 的增强版（拆牌优化、牌力值）" | 无版本递进关系：新算法系不含做牌发牌管线。extracted 复刻"线上**怎么**发牌"（求真），新算法系设计"发牌**应该怎么**做"（候选方案，求好）；"拆牌/牌力"三处实现各归各，见下表 |
| "previous 的 MakeDealHelper 从新算法系提炼" | 方向反了：previous 是线上遗产（old2 体系已跑数月），新算法 PRD 2026-02 才立项。真实关系 = **遗产 vs 重构候选**——若流动，方向是新算法系→线上（重构上线）；对照工具 `native/tools/shuffle_prng_compare.py`（遗产 `SvrXygRandomSort` vs 新法 MT19937+Fisher-Yates，还实锤了遗产 `srand(time(NULL))` 同秒跨桌同流缺陷） |
| "native/test 是 extracted 的模拟测试" | `test/` 测 `landlord.h`（新算法系）；extracted 的模拟入口是它自己顶层的 `harness.exe`（另两个 CMake 目标 power_split_test/split_vs_power 编的是 extracted 顶层的拆牌对比 cpp，也与 native/test 无关） |
| "previous 是整个服务器的拷贝" | 只拷了发牌相关 3 个 cpp + 配置；但 zgdatbl 一万行里发牌只占小块，其余是桌务逻辑 |

### 拆牌/牌力的三个实现位置（易混，各归各）

| 概念 | 线上系（previous/extracted） | 新算法系（src + landlord.h） | analysis 实验扩展 |
| ---- | ---- | ---- | ---- |
| 拆牌 | `SpliteCard`/`GetBestCardType`/`get_MaxHandCardValue` 递归拆牌——做牌时逼近 TargetValue 用 | `splitter/`（IComboExtractor，飞机/炸弹/顺子优先）——评分前把手拆成组合 | `optimal_split.h` 搜索式全局最优（min-combo→max-Σscore）——**指标期口径**，真正的"拆牌优化"在这里 |
| 牌力 | `get_GroupData`（makedeal.json GroupDataExp 公式）+ `CalHandCardValue`——做牌内部估值 | `calcTotalHandScore`（Java/C++ 双端同步）——**与数仓 card_power 的 PRD 同源**；harness 借它算首叫/抗衡指标 | —（指标直接借用左两家） |

> harness 的**指标层** = landlord.h 评分 + optimal_split 最优拆牌（比线上递归拆牌更能反映牌面质量）——这是两系唯一"借用"关系，仅限指标计算，发牌管线本体无关。

## 五、变更记录

| 日期 | 事件 |
| ---- | ---- |
| 2026-08-03 | previous 线上 old2 发牌代码副本入库（22a147b） |
| 2026-08-06 | algorithm/ 整体解禁可改（6511da3） |
| 2026-09-06 | 原独立算法仓以只读快照方式纳入（f2dbcf6） |
| 2026-09-08 | previous 同步线上 09-01 优化版 + makedeal.json new3~new6 铺开态（454b611）；extracted 归拢 tools/runs/results 四区（568921c） |
| 2026-09-09 | harness 单一化：harness_optv2.cpp 并回 harness.cpp（6b8b6e0，500 局同 seed 逐行验证一致） |
| 2026-09-10 | 原源仓修复落地至 `b729ef0`；经验证合入本仓后切换为单仓治理（6ddfdc1），`algorithm/` 成为本仓权威模块，独立仓归档保留历史 |
