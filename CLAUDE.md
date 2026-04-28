# 项目协作指南

本项目使用 Claude Code 进行开发协作，请遵循以下规范。

---

## Markdown 书写规范

创建或编辑 Markdown 文件时，必须遵循 [style/markdown-style-guide.md](style/markdown-style-guide.md) 规范。

### 核心要点

1. **代码块必须指定语言类型**：` ```sql ` 而非空的 ` ``` `
2. **表格分隔符格式**：`| ---- |`（管道符与内容间有空格）
3. **标题层级递进**：不跳级，同级标题内容不重复
4. **文件末尾空行**：每个 md 文件末尾必须有且仅有一个空行

---

## 数据库表命名规范

| 前缀 | 说明 | 示例 |
| ---- | ---- | ---- |
| `dws_` | DWS 层中间表 | `dws_ddz_daily_game` |
| `dwd_` | DWD 层明细表 | `dwd_game_combat_si` |

---

## 文档目录结构

```text
dws/          # DWS 表说明文档
docs/         # 分析文档
ops/          # 运维操作手册
style/        # 编码/书写规范
```
