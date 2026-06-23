# 项目协作指南

本项目使用 Claude Code / Cursor 进行开发协作，请遵循以下规范。

---

## Markdown 书写规范

创建或编辑 Markdown 文件时，必须遵循 [markdown-style-guide.md](markdown-style-guide.md) 规范。

### 核心要点

1. **代码块必须指定语言类型**：` ```sql ` 而非空的 ` ``` `
2. **表格分隔符格式**：`| ---- |`（管道符与内容间有空格）
3. **标题层级递进**：不跳级，同级标题内容不重复
4. **文件末尾空行**：每个 md 文件末尾必须有且仅有一个空行
5. **避免连续空行**：代码块与相邻标题/段落之间保留一个空行，不得有多个连续空行

---

## SQL 编写规范

### 别名规范

- **必须有意义的别名**：使用 `reg` for `registration`、`st` for `stat` 等
- **禁止使用无意义别名**：不要使用 `a`、`b`、`c`

### 查询结构

- **优先使用 CTE**：复杂逻辑使用 `WITH` 子句提高可读性
- **JOIN 逻辑检查**：
  - 验证 JOIN 是否导致数据膨胀（1:N vs 1:1）
  - 计算转化/漏斗指标时使用 `LEFT JOIN` 保留"零活跃"用户
- **安全优先**：
  - 分区列必须包含 `WHERE` 条件（如 `dt`、`reg_date`）
  - 使用 `COUNT(DISTINCT uid)` 而非 `COUNT(uid)`（除非明确需要计数重复）

### StarRocks 优化

| 场景 | 优化方案 |
| ---- | -------- |
| UV / 留存计算 | 使用 `BITMAP_UNION` 和 `BITMAP_COUNT` |
| 获取首/末事件 | 使用 `MIN_BY(value, time)` |
| 大表 JOIN | 使用 `Colocate Join` 或 `Bucket Shuffle Join` |
| 慢查询分析 | 使用 `EXPLAIN ANALYZE`，检查 "Plan Search Timeout" 或 "Giant Dispatch" |

### 分析工作流

1. **明确分母**：计算百分比前，明确"总数"定义（如：全部注册用户 vs 全部登录用户）
2. **处理零值**：除法运算必须处理 `NULL` 或 0 的情况，使用 `IFNULL` 或 `CASE WHEN`
3. **时间序列分析**：确保日期连续，说明数据可能存在的空缺

### 代码审查要点

- 看到 `SELECT *` 时警告并建议指定具体列
- 遇到"可用但慢"的 SQL，建议"预聚合"或"中间表"方案

> **编写查询 SQL 时必须参照 [SQL_STYLE.md](SQL_STYLE.md)**：涵盖 CTE 三段式骨架、目标日期常量化、矩阵坍缩、分区裁剪、StarRocks 兼容坑（LEFT JOIN ON 标量子查询）、已下线字段、常见逻辑 bug 等。仅管查询 SQL（SELECT）；**DDL 一律禁止**，须用户在 CloudBeaver 手动执行。

---

## 数据库表命名规范

| 前缀 | 说明 | 示例 |
| ---- | ---- | ---- |
| `dws_` | DWS 层中间表 | `dws_ddz_daily_game` |
| `dwd_` | DWD 层明细表 | `dwd_game_combat_si` |
| `dq_` | 配置维表（业务配置、元数据映射） | `dq_currency_guid_config` |

---

## 文档目录结构

```text
analysis/            # 数据分析工作区
  plan/              # 分析方案（分析思路和 SQL 查询）
  result/            # 分析结论（结合 plan 产出的结论性报告）
starrocks/           # StarRocks 数仓能力
  account/           # 账号域（注册、登录等）
  game/              # 游戏域（对局、战绩等）
  currency/          # 货币域（银子、积分等）
  config/            # 配置表（道具、渠道、货币配置等）
  retention/         # 留存域（首日指标、留存 flag）
py/                  # 回填脚本集（调度器 + 单表脚本 + 工具，详见 py/README.md）
  daily_backfill.py  # 每日初始化调度器（15 张表）
  daily_retention.py # 留存回扫调度器（35 天）
  README.md          # ← 脚本使用说明，团队入口
  tmp/               # 分析执行过程的临时文件（临时 SQL、探查脚本等，不提交，见 .gitignore）
ops/                 # 运维操作手册（daily_data_ops.md）
lessons/             # 经验总结（starrocks 技术排查 / troubleshooting）
requirements/        # 研发需求文档
```

> **日常运维入口**：[py/README.md](py/README.md) —— 两条命令完成每天数据回填（`daily_backfill.py` 初始化当天 + `daily_retention.py` 刷留存）。

---

## Git 操作规范

- **先判断运行环境再决定是否执行 Git 操作**：
  - 当前为 **CLI 环境**时，可直接执行 `git commit`、`git push` 等操作（依据：远程已配置、当前分支可提交）
  - 当前为 **cowork 沙箱环境**时，Git 操作无法实际生效，收到请求时**拒绝执行**并提示用户切换到 CLI
- 判断方式：检查 `git remote -v` 是否有远程、`git status` 是否正常工作；沙箱环境通常无远程或操作报错
- 若无法确定环境，先尝试 `git remote -v` 验证，而非默认拒绝

---

## DDL 操作规范

- **禁止通过 sr_exec.py 执行 DDL**：建表（CREATE TABLE）、改表（ALTER TABLE）、删表（DROP TABLE）等 DDL 操作一律不通过 `py/sr_exec.py` 或任何脚本执行
- DDL 操作必须由用户在 CloudBeaver 网页端手动执行 