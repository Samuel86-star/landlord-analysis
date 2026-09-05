# 项目基础设施离线闭环设计规范

## 一、文档信息

| 项目 | 内容 |
| ---- | ---- |
| 状态 | 已确认 |
| 日期 | 2026-09-05 |
| 适用范围 | 项目审查、安全边界、本地验证、跨方向治理与公司网络验收 |
| 依据 | [项目基础设施审查](../review/project-foundation/2026-09-04-review.md) |
| 实施计划 | [项目基础设施离线闭环计划](../plan/2026-09-05-project-foundation-offline.md) |
| 验证状态 | 离线验证通过，等待公司网络验收 |
| 阶段状态 | 离线范围已完成；公司网络验收未执行 |

## 二、背景与问题

项目基础设施为 AI 数据分析智能体和斗地主发牌算法智能体提供共享的安全、构建、测试、规范与审查能力，不是第三个业务智能体。

2026-09-04 的基础设施审查发现 CloudBeaver 凭据与 HTTP 传输风险，并规划了失败语义、统一验证和跨方向治理。随后代码已经完成凭据配置化、HTTP 默认阻断、敏感错误收敛、DDL 拦截、回填失败判定、Java benchmark 分离和 C++ CTest 接入，但原审查仍保留修复前描述，导致报告状态失真。

家庭网络无法访问公司 CloudBeaver 与 StarRocks。因此，本 Spec 将代码和本地测试能够证明的“离线完成”，与必须在公司网络执行的“集成验收”分开记录。

## 三、目标与非目标

### 3.1 目标

- 为每项审查发现建立稳定标识、状态、证据和关闭条件。
- 用一个确定性入口运行 Python、Java、benchmark 和 C++ 本地验证。
- 更新已经失真的基础设施审查与审查索引。
- 明确两个智能体方向与共享基础设施的目录责任边界。
- 提供不包含真实凭据的公司网络验收清单。
- 保证离线验证不能被误写为真实数仓集成通过。

### 3.2 非目标

- 不连接公司 CloudBeaver、StarRocks 或其他内网服务。
- 不轮换真实凭据，不创建或调整真实账号权限。
- 不新增 CI 工作流、通用 Agent Runtime 或基础设施 Skill。
- 不修改数据口径、SQL 业务逻辑或发牌算法行为。
- 不重写 Git 历史；已泄露凭据以轮换失效为主要处置方式。
- 不把本地 mock、单元测试或编译成功视为公司网络验收证据。

## 四、审查状态模型

每项发现必须使用稳定标识，并采用以下状态之一：

| 状态 | 含义 | 关闭要求 |
| ---- | ---- | ---- |
| `OPEN` | 尚未完成或缺少证据 | 提供实现与验证证据后迁移 |
| `RESOLVED_OFFLINE` | 代码、配置或文档已在本地验证 | 保留验证命令、结果和提交 |
| `BLOCKED_BY_CORP_NETWORK` | 只能在公司网络或真实系统完成 | 写明在线动作、负责人角色和验收证据 |
| `VERIFIED_ON_CORP_NETWORK` | 已在真实公司环境验证 | 记录日期、环境、结果和非敏感证据位置 |
| `DEFERRED` | 有意延期且不阻塞当前交付 | 写明触发条件，不使用模糊的“以后处理” |

状态迁移遵循以下规则：

- 离线单元测试只能把代码类问题迁移到 `RESOLVED_OFFLINE`。
- 真实凭据、传输端点和账号权限只能从 `BLOCKED_BY_CORP_NETWORK` 迁移到 `VERIFIED_ON_CORP_NETWORK`。
- 一个历史发现同时包含离线和在线动作时，必须拆成独立子项，不能用一个“已完成”覆盖全部风险。
- 状态变化必须附当前代码行、提交、命令输出摘要或验收记录，不沿用失效行号。

## 五、当前发现拆分

| 标识 | 事项 | 目标状态 | 证据或后续动作 |
| ---- | ---- | ---- | ---- |
| `PF-SEC-01A` | 移除当前源码中的 CloudBeaver 连接凭据 | `RESOLVED_OFFLINE` | 客户端从必填环境变量读取配置，测试验证缺失时在联网前失败；历史凭据由 `PF-SEC-01B` 处置 |
| `PF-SEC-01B` | 轮换历史提交中暴露的凭据 | `BLOCKED_BY_CORP_NETWORK` | 在公司账号系统轮换并验证旧凭据失效 |
| `PF-SEC-01C` | 默认拒绝 CloudBeaver 明文 HTTP | `RESOLVED_OFFLINE` | 默认要求 HTTPS，受限内网 HTTP 需显式设置例外变量 |
| `PF-SEC-01D` | 验证公司 HTTPS 端点与证书链 | `BLOCKED_BY_CORP_NETWORK` | 从公司网络完成真实登录和查询 |
| `PF-SEC-02` | 查询与写入账号最小权限 | `BLOCKED_BY_CORP_NETWORK` | 核对账号授权，证明日常查询账号不能执行高风险操作 |
| `PF-REL-01` | 脚本失败返回非零并停止下游 | `RESOLVED_OFFLINE` | Python 测试覆盖零行、异常结果和失败即停 |
| `PF-TEST-01` | Java 默认测试与 benchmark 分离 | `RESOLVED_OFFLINE` | 默认测试不执行 benchmark，显式 profile 执行 benchmark |
| `PF-TEST-02` | C++ 接入 CTest 且 Release 保留检查 | `RESOLVED_OFFLINE` | Release 构建后 CTest 能发现并执行测试 |
| `PF-GOV-01` | 审查发现生命周期 | `RESOLVED_OFFLINE` | Review 与索引采用本 Spec 的状态字段和当前证据 |
| `PF-GOV-02` | 跨方向责任边界 | `RESOLVED_OFFLINE` | 按目录和职责定义维护者角色，不虚构具体人员 |

## 六、统一离线验证入口

新增 `ops/py/verify_offline.py`，只负责编排现有命令，不复制测试逻辑，也不访问公司服务。

### 6.1 固定步骤

验证器从仓库根目录依次运行：

1. `python -m unittest discover -s ops/py -p test_*.py -v`。
2. `python -m compileall -q ops/py`。
3. `algorithm/mvnw test`，Windows 使用 `algorithm/mvnw.cmd test`。
4. `algorithm/mvnw -Pbenchmark test`，Windows 使用对应 wrapper。
5. `cmake -S algorithm/native -B algorithm/native/build-release -DCMAKE_BUILD_TYPE=Release`。
6. `cmake --build algorithm/native/build-release --target landlord_test`。
7. `ctest --test-dir algorithm/native/build-release --output-on-failure`。

验证器使用 Python 标准库 `subprocess` 顺序执行；任一步返回非零时立即停止并透传非零退出码。缺少 Java、Maven wrapper、CMake 或 CTest 时明确失败，不把未运行步骤标记为通过。

### 6.2 输出与边界

- 每一步开始前输出序号、名称和可复制命令。
- 成功时输出完成摘要；失败时输出失败步骤和退出码。
- 不读取 CloudBeaver 环境变量，不运行 SQL，不连接公司网络。
- 不保存命令输出副本，不建立日志框架；调用者需要留档时自行重定向输出。
- CMake 构建目录与现有 harness 二进制加入忽略规则，避免验证污染 Git 状态。

### 6.3 最小测试

为验证器保留一个标准库单元测试文件，至少证明：

- 命令按固定顺序执行。
- 某一步失败后不执行后续命令。
- 最终退出码等于首个失败步骤的退出码。

测试通过注入假的命令执行函数完成，不实际递归运行 Maven 或 CMake。

## 七、Review 生命周期

`docs/review/project-foundation/2026-09-04-review.md` 保留历史发现，但必须增加当前状态区：

- 原始发现日期和影响不删除。
- 修复前行号改为“历史证据”，当前证据引用现行代码与测试。
- 在线阻塞项单独列出，不把整个 P0 标记为已关闭。
- 验证限制更新为当前实际结果，不继续保留 benchmark 混入或 CMake 缺失等过期结论。

`docs/review/README.md` 保留原始严重度计数，同时增加状态汇总。严重度表示发现时风险，状态表示当前处置进度，两者不能相互替代。

后续 Review 的每项发现至少包含：

- 稳定标识与严重度。
- 当前状态。
- 原始证据和当前证据。
- 影响与处置结果。
- 未完成时的下一动作和触发条件。

## 八、责任边界

| 角色 | 负责范围 | 不负责事项 |
| ---- | ---- | ---- |
| 数据分析维护者 | `ops/py/`、`starrocks/`、数据知识与分析产物 | 发牌算法正确性 |
| 发牌算法维护者 | 权威算法源仓、`algorithm/` 快照、模拟与算法测试 | 数仓账号与回填权限 |
| 项目维护者 | 根目录规范、Review 生命周期、统一离线验证入口 | 代替业务维护者决定指标或算法阈值 |
| 公司系统管理员 | 凭据轮换、HTTPS 服务与账号最小权限 | 仓库内测试实现 |

具体人员由团队在仓库外的组织系统分配。本仓库只定义责任边界，避免把容易过期的个人姓名写入长期规范。

## 九、公司网络验收清单

公司网络验收记录必须逐项填写日期、执行者角色、环境、结果和非敏感证据位置：

1. 轮换历史暴露的 CloudBeaver 凭据。
2. 使用旧凭据登录失败，使用新凭据登录成功。
3. HTTPS 端点证书校验成功，查询和结果未经过明文 HTTP。
4. 查询账号可以执行代表性只读 DWS 查询。
5. 查询账号执行 DDL 或未授权写入时被服务端拒绝。
6. 如保留写入账号，确认其用途、授权范围和审批边界。
7. 验证登录失败、查询失败和超时不会泄露 credential、token 或完整认证响应。

验收记录不得包含真实 credential、session token、完整认证响应或敏感查询结果。任何一项未执行或失败时，对应发现保持 `BLOCKED_BY_CORP_NETWORK`，不得整体标记为集成通过。

## 十、Skill 与 CI 决策

本阶段不创建 `project-foundation` Skill。安全拦截、退出码和测试门禁必须由确定性代码执行；Skill 无法替代这些控制。

本阶段不创建 CI 工作流。统一验证入口先用于本地和人工审查；只有团队确认持续集成平台、运行成本、密钥边界和触发规则后，CI 才调用同一入口，不能再复制一套命令。

## 十一、验收标准

### 11.1 离线验收

- 基础设施 Review 的历史描述、当前状态和剩余阻塞可以明确区分。
- 审查索引不再把已修复问题描述成当前缺陷。
- 一个命令可以运行全部 Python、Java、benchmark 和 C++ 本地验证。
- 任一步失败时验证器返回非零，并且不继续后续步骤。
- 验证过程不访问 CloudBeaver 或 StarRocks。
- Git 状态不出现验证生成的 CMake 目录或 harness 二进制。
- 所有公司网络事项均保持 `BLOCKED_BY_CORP_NETWORK`。

### 11.2 在线验收

- 历史凭据已经轮换，旧凭据确认失效。
- HTTPS、真实登录和代表性查询通过。
- 查询与写入权限满足最小权限要求。
- 失败响应不泄露认证材料。
- 每项均有不包含敏感信息的验收记录。

在线验收全部通过后，只将对应发现迁移到 `VERIFIED_ON_CORP_NETWORK`。离线验证结果保持原样，不覆盖真实环境证据。

## 十二、实施约束

- 优先复用现有测试、Maven wrapper 和 CMake 入口。
- 验证器只做顺序编排与失败传播，不新增插件、配置层或日志系统。
- 不修改业务代码来迁就验证器。
- 不接触未跟踪的根目录 `AGENTS.md`。
- 实施必须遵循关联计划，不以本 Spec 的章节顺序替代任务拆分。
