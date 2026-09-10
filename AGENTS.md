# 项目协作指南

## 规范索引

- **Markdown**：[markdown-style-guide.md](markdown-style-guide.md)
- **SQL 编写**：[SQL_STYLE.md](SQL_STYLE.md)（查询规范 + StarRocks 兼容坑 + 已下线字段）
- **留存 SQL**：[docs/lessons/retention-sql-pattern.md](docs/lessons/retention-sql-pattern.md)（CTE 骨架、矩阵坍缩）
- **数仓知识**：[docs/knowledge/](docs/knowledge/)——对局表、字段定义、陷阱、房间等级、倍数口径
- **银子金流**：[docs/knowledge/game-currency-analysis.md](docs/knowledge/game-currency-analysis.md)——净消耗、服务费、运营支出、充值或三端对账时先读
- **运维入口**：[ops/daily_data_ops.md](ops/daily_data_ops.md)（增量更新手册）、[ops/py/README.md](ops/py/README.md)（脚本使用说明）

## 关键红线

- **DDL 一律禁止**通过脚本/`sr_exec.py` 执行，须用户在 CloudBeaver 手动执行
- **智能体联机取数固定使用公司内网 HTTP**：`http://flowops.tcy365.net:7788/api/gql`；该风险已由用户接受，不再要求 HTTPS
- **智能体只读 `tcy_temp`**：只允许 `SELECT`（含最终为 `SELECT` 的 CTE），SQL 中不得引用其他 catalog/database，也不得执行 `SHOW`、`SET`、`ADMIN`、`INSERT`、`UPDATE`、`DELETE` 或 DDL
- 上述只读限制针对智能体联机取数；现有 `ops/py/batch_insert_*.py` 仅保留给用户明确发起的人工运维，智能体不得自行执行
- **取数优先 DWS**（`dws_*_daily_game`），raw 仅 fallback；dws 缺字段或排障才回 raw
- **别名有意义**（`reg`/`st` 非 `a`/`b`/`c`）；优先 CTE；分区列必带 WHERE
- **房间等级/玩法**查 `tcy_temp.dq_game_room_config`，别用底分或 play_mode 反推
- **SQL 查询走 `ops/py/sr_exec.py`**（`py -3 -u ops/py/sr_exec.py -f file.sql`），临时 SQL 放 `ops/py/tmp/`
- **百分比除法**必须 `NULLIF(分母,0)` 防零除；`COUNT(DISTINCT uid)` 非 `COUNT(uid)`

## 目录结构

```
docs/               # 团队知识
  knowledge/        #  参考手册（表-字段-映射-陷阱）
  lessons/          #  案例库 + 留存 SQL 规范
  analysis/         #  分析方案(plan/) + 结论(result/)
  tech/             #  技术文档
  room-design/      #  房间设计方案
  requirements/     #  研发需求文档
ops/                # 运维
  py/               #  回填脚本集（调度器 + sr_exec + batch_insert）
  daily_data_ops.md #  每日数据增量更新手册
starrocks/          # 数仓表文档（account / game / currency / config / retention）
algorithm/          # 本仓权威算法模块 + docs（Java/C++；previous/ 为线上代码参照副本，改动需与线上同步并记录）
```

## Git 操作

- **CLI 环境**可直接 `git commit`/`git push`（SSH 直连）
- **cowork 沙箱环境**拒绝执行 Git 操作，提示用户切到 CLI
- 不确定时先 `git remote -v` 验证
