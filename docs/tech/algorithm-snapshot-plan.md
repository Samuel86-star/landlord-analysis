# algorithm 只读快照存放方案

## 背景

- **landlord-analysis**（本项目）：斗地主真实数据分析，Python/SQL，含数仓知识、房间设计、留存分析。
- **landlord-algorithm**（`github.com/Samuel86-star/landlord.git`；坐标 `com.mamba.landlord:landlord-algorithm:0.0.1-SNAPSHOT`，Spring Boot 4.0.3 / Java 21 + C++ native）：斗地主算法实现（发牌 / 拆牌 / 评分模拟）。

两者是同一业务的两面：analysis 做数据分析，algorithm 做算法模拟，互补。algorithm 的发牌平衡 / 评分可为 analysis 的房间设计（不洗牌 / cap / 底分）提供算法依据。

## 目标与定调

把 algorithm 的核心内容以 **完整代码 + 文档镜像** 的形式，作为 **只读快照副本** 放入 analysis 顶层 `algorithm/`，使 analysis 自包含算法参考、便于追溯。

| 决策项 | 定调 |
|---|---|
| 搬运内容 | 完整代码 + 文档镜像（Java + C++ + docs） |
| 定位 / 更新 | 只读快照副本（一次性拷贝；algorithm 大改后重新拷） |
| 存放位置 | analysis 顶层 `algorithm/`，保持原项目结构 |
| 快照基准 | **先在 algorithm 提交干净，再基于干净 HEAD 拷贝** |

## 目录结构（拷到 `algorithm/`）

```
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
│   └── test/  (main_test.cpp, sampler_test.cpp)
└── docs/   评价标准 / 发牌平衡 PRD×2 / 拆牌决策规则 / 测试策略 / rules / prompt
```

## 包含 / 排除

**包含**（完整镜像 + 可构建追溯）：`src/main` + `src/test` + `native/{CMakeLists.txt, include, config, test}` + `docs/` + `pom.xml` + `mvnw`/`mvnw.cmd`/`.mvn`。

**排除**（非源码 / 平台产物 / 元数据）：

| 排除项 | 理由 |
|---|---|
| `target/`、`native/build/` | 构建产物 |
| `native/sampler`、`native/test_main` | Mach-O arm64 二进制可执行文件，平台相关 |
| `.idea/`、`.vscode/`、`.cursor/`、`.claude/` | IDE / 工具配置（`.cursor` 是 submodule；`.claude` 是 algorithm 的 Claude Code 项目配置） |
| `.git/`、`.gitmodules`、`.gitattributes` | git 元数据（analysis 有自己的 repo） |
| `**/.DS_Store`、`HELP.md` | 水文件 |

拷贝方式：基于干净 HEAD 的工作区，用 `rsync` 带 `--exclude` 列表精确控制。

## `algorithm/README.md`（新增，快照说明）

记录：来源 `github.com/Samuel86-star/landlord.git` · 快照版本 `<commit>` (main) @ 2026-07-30 · **只读副本，勿改** · 三大模块速览（scoring 牌力评分 / shuffle 发牌分布采样 / splitter 拆牌 + native）· 更新方法（重新拷贝覆盖的命令）。

> 当前 algorithm HEAD 为 `e98f890`，但含 22 个未提交改动；实施时以"提交后的干净 HEAD"为准，README 填入实际 commit。

## 与 analysis 的衔接

1. **`CLAUDE.md`「目录结构」** 加一行：
   `algorithm/  # landlord-algorithm 只读快照（Java/C++ 算法实现 + docs，参考用，勿改）`
2. **analysis 根 `.gitignore`** 加规则，防未来在 analysis 里 `mvn`/`cmake` 产物误提交：
   ```
   algorithm/target/
   algorithm/native/build/
   algorithm/native/sampler
   algorithm/native/test_main
   ```

## 实施步骤

1. （用户）在 `landlord-algorithm` 提交那 22 个未提交改动，记录新的 HEAD commit。
2. 用 `rsync`（带 `--exclude` 列表）从 algorithm 工作区拷到 `analysis/algorithm/`。
3. 写 `algorithm/README.md`（来源 / commit / 日期 / 只读 / 模块 / 更新法）。
4. analysis 根 `.gitignore` 加 algorithm 产物排除规则。
5. `CLAUDE.md`「目录结构」加 `algorithm/` 说明。
6. analysis 提交（commit message：`docs(tech): 引入 algorithm 只读快照（landlord-algorithm <commit>）`）。

## 更新方法（后续 algorithm 演进时）

algorithm 大改后：在 algorithm 提交 → 记录新 commit → 用 `rsync` 重新覆盖 `analysis/algorithm/` → 更新 README 的 commit / 日期 → analysis 提交。
