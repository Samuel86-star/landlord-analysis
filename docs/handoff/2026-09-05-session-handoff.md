# 2026-09-05 项目审查与离线闭环交接

## 一、当前基线

| 项目 | 当前值 |
| ---- | ---- |
| 分支 | `main` |
| 实现基线 | `9f73413`（本 handoff 文档提交另计） |
| 远端状态 | 本地 `main` 含未推送提交，具体超前数以 `git status` 为准 |
| 工作区 | 仅有用户原有未跟踪文件 `AGENTS.md`，本轮未修改 |
| 网络边界 | 家庭网络，未访问 CloudBeaver、StarRocks 或其他公司内网服务 |

## 二、本轮完成事项

### 2.1 AI 数据分析智能体

- 创建并确认[设计规范](../spec/2026-09-04-data-analysis-agent-design.md)和[离线实施计划](../plan/2026-09-04-data-analysis-agent-offline-safety.md)。
- 完成凭据配置化、HTTPS 默认约束、敏感登录错误收敛、DDL 与多语句拦截。
- 回填公共入口会拒绝缺失、畸形、负数或默认不允许的零行验证结果，并在失败时停止。
- 修复已审查 SQL 的防零除表达式，补齐分析任务与正式结果契约。
- 当前 Review 状态：3 项 `RESOLVED_OFFLINE`、1 项 `BLOCKED_BY_CORP_NETWORK`、0 项 `OPEN`。

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
- 当前状态：7 项 `RESOLVED_OFFLINE`、3 项 `BLOCKED_BY_CORP_NETWORK`、0 项 `OPEN`。

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

### 4.1 必须在公司网络完成

按[公司网络验收清单](../tech/project-foundation-corp-network-checklist.md)执行并保存非敏感证据：

1. `PF-SEC-01B`：轮换历史暴露凭据，并验证旧凭据失效、新凭据可用。
2. `PF-SEC-01D`：验证 HTTPS 端点、证书链、真实登录、代表性 DWS 查询以及失败/超时响应脱敏。
3. `PF-SEC-02`：验证查询账号最小权限；如保留写账号，确认用途、授权范围和审批边界。
4. `DA-REL-02`：确认 StarRocks/CloudBeaver 是否支持所需事务语义，并在用户批准的安全表和日期上演练 DELETE 后失败恢复。
5. 运行 AI 数据分析智能体的代表性查询、回填、超时、权限拒绝和恢复端到端场景。

完成前不得将这些事项标记为 `VERIFIED_ON_CORP_NETWORK` 或“集成验证通过”。

### 4.2 智能体能力与 Skill

- `data-analysis-agent` Skill 未创建：先完成公司网络端到端验证，再积累 5 至 10 个真实分析任务，确认重复交互流程稳定。
- `dealing-algorithm-agent` Skill 未创建：先积累至少 5 个覆盖正确性、策略比较、调参、公平性、性能或同步的真实任务记录。
- `project-foundation` Skill 明确不创建；确定性安全和验证控制继续留在代码、配置及未来 CI 中。
- 两个业务方向的阶段 C 智能体闭环、真实任务回归集和反馈闭环尚未实施。

### 4.3 其他待办

- 本地 `main` 的 10 个基础设施提交尚未推送到 `origin/main`。
- 数仓 ETL 尚未应用牌力 PRD 新版本；上线时必须记录第二个 `card_power` 版本切点。
- 四维发牌过滤参数是否启用仍是产品/运维决策。
- CI 工作流暂不创建；仅在团队确认平台、成本、密钥边界和触发规则后调用现有统一验证入口。
- 未跟踪的 `AGENTS.md` 归用户所有；后续需由用户决定提交、忽略或删除。

## 五、下一轮建议顺序

1. 在确认推送范围后，将本地 `main` 推送到 `origin/main`。
2. 回到公司网络后执行基础设施与数据分析联合验收，优先处理凭据轮换和最小权限。
3. 为数据分析方向建立 5 至 10 个真实任务回归集，并记录失败点。
4. 为发牌算法方向建立至少 5 个真实任务记录，再决定是否创建两个最小 project Skill。
5. 另开生产变更任务处理数仓 ETL 牌力版本上线与切点记录。

## 六、关键入口

- 审查总索引：[项目审查索引](../review/README.md)
- 数据分析 Review：[AI 数据分析智能体审查](../review/data-analysis-agent/2026-09-04-review.md)
- 发牌算法 Review：[斗地主发牌算法智能体审查](../review/dealing-algorithm-agent/2026-09-04-review.md)
- 基础设施 Review：[项目基础设施审查](../review/project-foundation/2026-09-04-review.md)
- 统一离线验证：`ops/py/verify_offline.py`
- 公司网络验收：[项目基础设施公司网络验收清单](../tech/project-foundation-corp-network-checklist.md)
