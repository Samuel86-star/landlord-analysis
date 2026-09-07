# 2026-09-05 项目审查与离线闭环交接

> 2026-09-07 已补充公司网络只读验收结果和用户确认的风险处置决定；相关状态以本文件更新内容为准。

## 一、当前基线

| 项目 | 当前值 |
| ---- | ---- |
| 分支 | `main` |
| 实现基线 | `9411833`（状态文档修改尚未提交） |
| 远端状态 | 本地 `main` 比 `origin/main` 超前 5 个提交 |
| 工作区 | 存在其他进行中的未提交修改；本次只更新状态文档，具体以 `git status` 为准 |
| 网络边界 | 2026-09-05 为家庭网络；2026-09-07 已完成公司网络只读验证 |

## 二、本轮完成事项

### 2.1 AI 数据分析智能体

- 创建并确认[设计规范](../spec/2026-09-04-data-analysis-agent-design.md)和[离线实施计划](../plan/2026-09-04-data-analysis-agent-offline-safety.md)。
- 完成凭据配置化、HTTPS 默认约束、敏感登录错误收敛、DDL 与多语句拦截。
- 回填公共入口会拒绝缺失、畸形、负数或默认不允许的零行验证结果，并在失败时停止。
- 修复已审查 SQL 的防零除表达式，补齐分析任务与正式结果契约。
- 当前 Review 状态：3 项 `RESOLVED_OFFLINE`、1 项 `DEFERRED`、0 项 `BLOCKED_BY_CORP_NETWORK`、0 项 `OPEN`。

### 2.2 斗地主发牌算法智能体

- 创建并确认[设计规范](../spec/2026-09-04-dealing-algorithm-agent-design.md)和[阶段 A/B 实施计划](../plan/2026-09-04-dealing-algorithm-agent-foundation.md)。
- 在权威源仓完成重洗结果一致性、连对长度、C++ 生命周期、benchmark 分离、CTest 和回归向量修复。
- 将源仓 `e4be61a` 同步为本仓 `algorithm/` 只读快照，并建立可复现实验契约。
- 当前 Review 状态：5 项 `RESOLVED_OFFLINE`、0 项 `OPEN`；阶段 C/D 未启动。

### 2.3 项目基础设施

- 创建并确认[基础设施设计规范](../spec/2026-09-05-project-foundation-design.md)和[离线闭环计划](../plan/2026-09-05-project-foundation-offline.md)。
- 新增统一离线验证入口 `python3 ops/py/verify_offline.py`，顺序执行 Python、compileall、Java 默认测试、benchmark、CMake Release 构建和 CTest，首个失败立即返回非零。
- 处理 macOS Python 缓存、Maven 根目录与离线缓存、CMake 多配置 Release 三个本地执行差异。
- 更新 Review 生命周期、责任边界、生成物忽略规则和[公司网络验收清单](../tech/project-foundation-corp-network-checklist.md)。
- 当前状态：7 项 `RESOLVED_OFFLINE`、2 项 `DEFERRED`、1 项 `RISK_ACCEPTED`、0 项 `BLOCKED_BY_CORP_NETWORK`、0 项 `OPEN`。

## 三、最终验证证据

2026-09-05 在合并后的本地 `main` 执行：

```bash
python3 ops/py/verify_offline.py
```

结果：退出码 `0`，最终输出 `Offline verification passed.`。

| 验证项 | 结果 |
| ---- | ---- |
| Python 单元测试 | 28/28 通过 |
| Python compileall | 通过 |
| Java 默认测试 | 62/62 通过，不含 benchmark |
| Java benchmark profile | 70/70 通过，包含 8 项 benchmark |
| C++ Release build | 通过 |
| CTest | 1/1 通过 |

Maven 仍输出 Mockito 自附加弃用警告，不影响本轮通过结果；升级 JDK/Mockito 或警告转为失败时再处理。

## 四、未完成事项

### 4.1 公司网络验收后处置

2026-09-07 已完成公司网络只读验证并保存非敏感证据。用户确认以下处置：

1. `PF-SEC-01B`：凭据轮换暂不推进，状态为 `DEFERRED`。
2. `PF-SEC-01D`：接受受限公司内网 HTTP，状态为 `RISK_ACCEPTED`。
3. `PF-SEC-02`：账号权限收敛暂不推进，状态为 `DEFERRED`。
4. `DA-REL-02`：写入恢复演练不纳入当前范围，状态为 `DEFERRED`。

以上事项均不阻塞只读数据分析智能体；智能体固定使用公司内网 HTTP，只对 `tcy_temp` 执行 `SELECT`。

### 4.2 智能体能力与 Skill

- `data-analysis-agent` Skill 未创建：先积累 5 至 10 个真实分析任务，确认重复交互流程稳定。
- `dealing-algorithm-agent` Skill 未创建：先积累至少 5 个覆盖正确性、策略比较、调参、公平性、性能或同步的真实任务记录。
- `project-foundation` Skill 明确不创建；确定性安全和验证控制继续留在代码、配置及未来 CI 中。
- 两个业务方向的阶段 C 智能体闭环、真实任务回归集和反馈闭环尚未实施。

### 4.3 其他待办

- 本地 `main` 比 `origin/main` 超前 5 个提交，且存在未提交修改；推送不在本次状态清理范围内。
- 数仓 ETL 尚未应用牌力 PRD 新版本；上线时必须记录第二个 `card_power` 版本切点。
- 四维发牌过滤参数是否启用仍是产品/运维决策。
- CI 工作流暂不创建；仅在团队确认平台、成本、密钥边界和触发规则后调用现有统一验证入口。
- 未跟踪的 `AGENTS.md` 归用户所有；后续需由用户决定提交、忽略或删除。

## 五、下一轮建议顺序

1. 在确认推送范围后，将本地 `main` 推送到 `origin/main`。
2. 为数据分析方向建立 5 至 10 个真实任务回归集，并记录失败点。
3. 为发牌算法方向建立至少 5 个真实任务记录，再决定是否创建两个最小 project Skill。
4. 另开生产变更任务处理数仓 ETL 牌力版本上线与切点记录。

## 六、关键入口

- 审查总索引：[项目审查索引](../review/README.md)
- 数据分析 Review：[AI 数据分析智能体审查](../review/data-analysis-agent/2026-09-04-review.md)
- 发牌算法 Review：[斗地主发牌算法智能体审查](../review/dealing-algorithm-agent/2026-09-04-review.md)
- 基础设施 Review：[项目基础设施审查](../review/project-foundation/2026-09-04-review.md)
- 统一离线验证：`ops/py/verify_offline.py`
- 公司网络验收：[项目基础设施公司网络验收清单](../tech/project-foundation-corp-network-checklist.md)
