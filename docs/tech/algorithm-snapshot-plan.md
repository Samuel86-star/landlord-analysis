# algorithm 只读快照存放方案

## 背景

- **landlord-analysis**（本项目）：斗地主真实数据分析，Python/SQL，含数仓知识、房间设计、留存分析。
- **landlord-algorithm**（`github.com/Samuel86-star/landlord.git`；坐标 `com.mamba.landlord:landlord-algorithm:0.0.1-SNAPSHOT`，Spring Boot 4.0.3 / Java 21 + C++ native）：斗地主算法实现（发牌 / 拆牌 / 评分模拟）。

两者是同一业务的两面：analysis 做数据分析，algorithm 做算法模拟，互补。algorithm 的发牌平衡 / 评分可为 analysis 的房间设计（不洗牌 / cap / 底分）提供算法依据。

## 目标与定调

把 algorithm 的核心代码和文档作为 **只读快照基线** 放入 analysis 顶层 `algorithm/`，同时保留 analysis 专用实验资产，使 analysis 自包含算法参考、便于追溯。

| 决策项 | 定调 |
|---|---|
| 搬运内容 | 完整代码 + 文档基线（Java + C++ + docs），保留 analysis 专用扩展 |
| 定位 / 更新 | 只读快照副本（按源仓 commit 区间增量同步） |
| 存放位置 | analysis 顶层 `algorithm/`，保持原项目结构 |
| 快照基准 | **先在 algorithm 提交干净，再基于明确 base/head 同步** |

## 目录结构（拷到 `algorithm/`）

```text
algorithm/
├── README.md                 ← 新增：快照来源/版本/只读/更新说明（替换原空 README）
├── pom.xml                   com.mamba.landlord:landlord-algorithm:0.0.1-SNAPSHOT
├── mvnw  mvnw.cmd  .mvn/     ← Maven wrapper，保留以支撑"可构建追溯"
├── src/
│   ├── main/java/com/mamba/landlord/   scoring/shuffle/splitter + core + controller（约 3109 行）
│   ├── main/resources/application.properties
│   └── test/java/...                    10 个测试类（含 benchmark）
├── native/
│   ├── CMakeLists.txt
│   ├── include/landlord.h               C++ 实现（header-only；native/src 实际为空）
│   ├── config/scoring.properties
│   ├── test/  (main_test.cpp, sampler_test.cpp)
│   ├── extracted/                        analysis 专用实验资产，同步时保留
│   └── previous/                         线上代码参照副本，同步时保留
└── docs/   评价标准 / 发牌平衡 PRD×2 / 拆牌决策规则 / 测试策略 / rules / prompt
```

## 包含 / 排除

**包含**（源码基线 + 可构建追溯）：源仓 commit 区间内变更的 `src/main`、`src/test`、`native/{CMakeLists.txt, include, config, test}`、`docs/`、`pom.xml`、`mvnw`/`mvnw.cmd`/`.mvn`。

**排除**（非源码 / 平台产物 / 元数据）：

| 排除项 | 理由 |
|---|---|
| `target/`、`native/build*/` | 构建产物 |
| `native/sampler`、`native/test_main` | Mach-O arm64 二进制可执行文件，平台相关 |
| `.idea/`、`.vscode/`、`.cursor/`、`.claude/` | IDE / 工具配置（`.cursor` 是 submodule；`.claude` 是 algorithm 的 Claude Code 项目配置） |
| `.git`、`.gitmodules`、`.gitattributes` | git 元数据（普通 checkout 的 `.git/` 与 worktree 的 `.git` 文件都不进入快照） |
| `.superpowers/` | 源仓任务审查与执行记录，不属于算法源码 |
| `**/.DS_Store`、`HELP.md` | 水文件 |

同步方式：先列出 source base 到 source head 的文件清单，再逐文件应用最终源仓差异。目标文件等于 source base 时直接应用该文件的最终差异；目标已分叉时只合入源仓任务 hunks，保留 analysis 专用行为和文档。新增文件显式添加，清单外文件不修改、不删除；禁止对 `algorithm/` 使用全量删除式同步。

## `algorithm/README.md`（新增，快照说明）

记录：来源 `github.com/Samuel86-star/landlord.git` · 快照版本 `<commit>` (`<source branch>`) @ `<同步日期>` · **只读副本，勿改** · 三大模块速览（scoring 牌力评分 / shuffle 发牌分布采样 / splitter 拆牌 + native）· 更新方法（按源仓 commit 区间增量同步）。

> 2026-09-05 已从验证工作树 `codex/dealing-agent-foundation` 同步 `71599ad..e4be61a`；README 记录实际 source head。

## 与 analysis 的衔接

1. **`CLAUDE.md`「目录结构」** 加一行：
   `algorithm/  # landlord-algorithm 只读快照（Java/C++ 算法实现 + docs，参考用，勿改）`
2. **analysis 根 `.gitignore`** 加规则，防未来在 analysis 里 `mvn`/`cmake` 产物误提交：
   ```text
   algorithm/target/
   algorithm/native/build*/
   algorithm/native/sampler
   algorithm/native/test_main
   ```

## 实施步骤

1. 在 `landlord-algorithm` 提交并验证源仓改动，记录 source base/head。
2. 记录 source base/head，用 `git diff --name-status <base>..<head>` 得到唯一同步清单。
3. 对每个修改文件比较 analysis 目标与 source base：相同则应用最终源仓差异；已分叉则合入源仓 hunks并保留 analysis 专用内容。显式添加新文件，清单外文件保持不变。
4. 写 `algorithm/README.md`（来源 / commit / 日期 / 只读 / 模块 / 更新法）。
5. analysis 根 `.gitignore` 加 algorithm 产物排除规则。
6. `CLAUDE.md`「目录结构」加 `algorithm/` 说明。
7. 验证 Java、Release native/CTest 与差异范围后提交 analysis。

## 更新方法（后续 algorithm 演进时）

1. 确认源仓只有允许忽略的构建产物，记录当前快照 commit 为 `source_base`、已验证源仓 commit 为 `source_head`。
2. 运行 `git -C <源仓> diff --name-status <source_base>..<source_head>`，将输出保存为本次唯一文件清单。
3. 对清单中的每个修改文件，把 `git -C <源仓> show <source_base>:<path>` 与 `algorithm/<path>` 比较：
   - 内容相同：只应用 `git -C <源仓> diff <source_base>..<source_head> -- <path>` 的最终差异。
   - 内容不同：同时审阅 analysis 相对 source base 的差异和源仓最终差异，只合入本次源仓 hunks，保留 analysis 专用实现与文档。
4. 对清单中的新增文件显式添加；不修改或删除清单外文件。特别保留 `native/extracted/`、`native/previous/`、`shuffle_prng_compare*` 和其他 analysis 专用资产。
5. 更新 `algorithm/README.md` 的 commit/日期，运行 Java 默认测试、Release native 构建与 CTest，再检查 `git diff --check`、`git status --short` 和 `git diff --stat`。
