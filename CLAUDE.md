# 项目协作指南

本项目使用 Claude Code / Cursor 进行开发协作，请遵循以下规范。

---

## Markdown 书写规范

创建或编辑 Markdown 文件时，必须遵循 [style/markdown-style-guide.md](style/markdown-style-guide.md) 规范。

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

---

## 数据库表命名规范

| 前缀 | 说明 | 示例 |
| ---- | ---- | ---- |
| `dws_` | DWS 层中间表 | `dws_ddz_daily_game` |
| `dwd_` | DWD 层明细表 | `dwd_game_combat_si` |

---

## 文档目录结构

```text
raw/          # ODS 源数据表说明文档
dws/          # DWS 层中间表说明文档
docs/         # 分析文档（包含分析思路和 SQL 查询）
data/         # 分析数据（docs 中查询产出的数据结果）
report/       # 分析报告（结合 data 和 docs 产出的结论性报告）
ops/          # 运维操作手册
style/        # 编码/书写规范
```

---

## 数据源表（ODS 层）

| 表名 | 库名 | 说明 | 文档 |
| ---- | ---- | ---- | ---- |
| `dwd_game_combat_si` | `tcy_dwd` | 玩家游戏对局战绩日志 | [raw/dwd_game_combat_si.md](raw/dwd_game_combat_si.md) |
| `dwd_tcy_userlogin_si` | `tcy_dwd` | 玩家登录日志 | [raw/dwd_tcy_userlogin_si.md](raw/dwd_tcy_userlogin_si.md) |
| `olap_tcy_userapp_d_p_login1st` | `hive_catalog_cdh5.dm` | 游戏用户首次注册登录信息 | [raw/olap_tcy_userapp_d_p_login1st.md](raw/olap_tcy_userapp_d_p_login1st.md) |
