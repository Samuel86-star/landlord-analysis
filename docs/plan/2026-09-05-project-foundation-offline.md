# 项目基础设施离线闭环 Implementation Plan

> **执行状态：已完成（离线范围）。** 下方未勾选框保留为原始实施步骤，不再表示当前待办；公司网络事项仍按验收清单执行。

**Goal:** 建立可失败、可复现的统一离线验证入口，更新基础设施 Review 生命周期，并把公司网络事项隔离为明确的在线验收清单。

**Architecture:** `ops/py/verify_offline.py` 只用 Python 标准库顺序编排现有 Python、Maven 和 CMake/CTest 命令，首个失败立即返回非零。Review 文档使用稳定状态标识区分历史严重度、离线修复与公司网络验收，不新增 CI、基础设施 Skill 或业务逻辑。

**Tech Stack:** Python 3 标准库、`unittest`、Maven Wrapper、CMake、CTest、Markdown

**Spec:** `docs/spec/2026-09-05-project-foundation-design.md`

**完成提交:** `9c6054c..9f73413`

**实施偏差:** 当前验证器为 macOS 沙箱指定项目内 Python 缓存；Maven 使用 `-o -f algorithm/pom.xml`；CMake build/CTest 显式选择 Release。当前命令以 `ops/py/verify_offline.py` 为准，计划中的早期命令片段仅保留为实施记录。

## Global Constraints

- 不连接公司 CloudBeaver、StarRocks 或其他内网服务。
- 不轮换真实凭据，不创建或调整真实账号权限。
- 不新增第三方依赖、CI 工作流、通用 Agent Runtime 或基础设施 Skill。
- 不修改数据口径、SQL 业务逻辑或发牌算法行为。
- 离线测试只能产生 `RESOLVED_OFFLINE`，不能产生 `VERIFIED_ON_CORP_NETWORK`。
- 验证器只做顺序编排、输出步骤和失败传播，不复制测试逻辑或保存日志。
- 不重写 Git 历史，不接触未跟踪的根目录 `AGENTS.md`。
- 所有代码步骤遵循 TDD；任一步验证失败时停止，不更新完成状态。

---

## File Map

| 文件 | 职责 |
| ---- | ---- |
| `ops/py/verify_offline.py` | 生成并顺序执行统一离线验证命令 |
| `ops/py/test_verify_offline.py` | 验证命令顺序、失败即停和缺失工具退出码 |
| `ops/py/README.md` | 记录统一离线验证命令和边界 |
| `.gitignore` | 忽略所有 `algorithm/native/build*` CMake 构建目录 |
| `algorithm/native/extracted/.gitignore` | 忽略可重建的 Unix `harness` 二进制 |
| `docs/review/project-foundation/2026-09-04-review.md` | 保存历史发现、当前状态、证据和剩余阻塞 |
| `docs/review/README.md` | 区分历史严重度统计与当前基础设施状态 |
| `docs/tech/project-foundation-corp-network-checklist.md` | 公司网络验收操作与非敏感证据模板 |
| `docs/spec/2026-09-05-project-foundation-design.md` | 记录最终离线验证状态 |

## Task 1: Build the Deterministic Offline Verification Entry

**Files:**

- Create: `ops/py/verify_offline.py`
- Create: `ops/py/test_verify_offline.py`
- Modify: `ops/py/README.md`
- Modify: `.gitignore`
- Modify: `algorithm/native/extracted/.gitignore`

**Interfaces:**

- Produces: `build_steps(root: pathlib.Path) -> list[tuple[str, list[str]]]`。
- Produces: `run_steps(steps, root: pathlib.Path, runner=subprocess.run) -> int`。
- Produces: `main() -> int`，成功返回 `0`，命令失败返回其非零码，命令无法启动返回 `127`。
- Consumes: 现有 `algorithm/mvnw`、`algorithm/mvnw.cmd`、`cmake` 和 `ctest`。
- Boundary: 不读取 CloudBeaver 环境变量，不执行 SQL，不捕获或持久化子进程输出。

- [ ] **Step 1: Write failing verification-runner tests**

Create `ops/py/test_verify_offline.py`:

```python
import os
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, os.path.dirname(__file__))

from verify_offline import build_steps, run_steps


class OfflineVerificationTest(unittest.TestCase):
    def test_build_steps_has_the_fixed_order(self):
        root = Path("/repo")
        steps = build_steps(root)

        self.assertEqual(
            [name for name, _ in steps],
            [
                "Python unit tests",
                "Python compile check",
                "Java unit tests",
                "Java benchmark profile",
                "C++ Release configure",
                "C++ Release build",
                "C++ CTest",
            ],
        )
        wrapper = "mvnw.cmd" if os.name == "nt" else "mvnw"
        self.assertEqual(steps[2][1][0], str(root / "algorithm" / wrapper))

    def test_run_steps_executes_in_order(self):
        calls = []

        def fake_run(command, cwd):
            calls.append((command, cwd))
            return SimpleNamespace(returncode=0)

        root = Path("/repo")
        result = run_steps(
            [("first", ["one"]), ("second", ["two"])],
            root,
            runner=fake_run,
        )

        self.assertEqual(result, 0)
        self.assertEqual(calls, [(["one"], root), (["two"], root)])

    def test_run_steps_stops_at_first_failure(self):
        calls = []

        def fake_run(command, cwd):
            calls.append(command)
            return SimpleNamespace(returncode=7 if command == ["two"] else 0)

        result = run_steps(
            [("first", ["one"]), ("second", ["two"]), ("third", ["three"])],
            Path("/repo"),
            runner=fake_run,
        )

        self.assertEqual(result, 7)
        self.assertEqual(calls, [["one"], ["two"]])

    def test_run_steps_reports_missing_tool(self):
        def missing_tool(command, cwd):
            raise FileNotFoundError(command[0])

        result = run_steps(
            [("missing", ["not-installed"])],
            Path("/repo"),
            runner=missing_tool,
        )

        self.assertEqual(result, 127)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
python3 -m unittest ops/py/test_verify_offline.py -v
```

Expected: FAIL with `ModuleNotFoundError: No module named 'verify_offline'`.

- [ ] **Step 3: Implement the minimal standard-library runner**

Create `ops/py/verify_offline.py`:

```python
#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


def build_steps(root):
    wrapper = root / "algorithm" / ("mvnw.cmd" if os.name == "nt" else "mvnw")
    native = root / "algorithm" / "native"
    build = native / "build-release"
    return [
        (
            "Python unit tests",
            [sys.executable, "-m", "unittest", "discover", "-s", "ops/py", "-p", "test_*.py", "-v"],
        ),
        ("Python compile check", [sys.executable, "-m", "compileall", "-q", "ops/py"]),
        ("Java unit tests", [str(wrapper), "test"]),
        ("Java benchmark profile", [str(wrapper), "-Pbenchmark", "test"]),
        (
            "C++ Release configure",
            ["cmake", "-S", str(native), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"],
        ),
        ("C++ Release build", ["cmake", "--build", str(build), "--target", "landlord_test"]),
        ("C++ CTest", ["ctest", "--test-dir", str(build), "--output-on-failure"]),
    ]


def run_steps(steps, root, runner=subprocess.run):
    total = len(steps)
    for index, (name, command) in enumerate(steps, start=1):
        print(
            "[{}/{}] {}: {}".format(
                index, total, name, subprocess.list2cmdline(command)
            ),
            flush=True,
        )
        try:
            result = runner(command, cwd=root)
        except OSError as error:
            print("{} could not start: {}".format(name, error), file=sys.stderr)
            return 127
        if result.returncode != 0:
            print(
                "{} failed with exit code {}".format(name, result.returncode),
                file=sys.stderr,
            )
            return result.returncode
    print("Offline verification passed.")
    return 0


def main():
    return run_steps(build_steps(REPO_ROOT), REPO_ROOT)


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run the focused test and verify GREEN**

Run:

```bash
python3 -m unittest ops/py/test_verify_offline.py -v
```

Expected: 4 tests PASS.

- [ ] **Step 5: Ignore only reproducible native build artifacts**

In root `.gitignore`, replace:

```gitignore
algorithm/native/build/
```

with:

```gitignore
algorithm/native/build*/
```

Append to `algorithm/native/extracted/.gitignore` under compiled artifacts:

```gitignore
/harness
```

Do not add a broad executable or native-directory ignore rule.

- [ ] **Step 6: Document the single offline command**

Append this section to `ops/py/README.md` after the existing `sr_exec.py` configuration section:

````markdown
#### 项目离线全量验证

从仓库根目录执行：

```bash
python3 ops/py/verify_offline.py
```

该命令依次运行 Python 单元测试与编译检查、Java 普通测试、Java benchmark profile、C++ Release 构建和 CTest。任一步失败都会立即停止并返回非零退出码。

该入口不会读取 CloudBeaver 配置、执行 SQL 或访问公司数仓。通过只代表离线验证完成，不能标记为公司网络集成通过。
````

- [ ] **Step 7: Run the complete Python unit suite**

Run:

```bash
python3 -m unittest discover -s ops/py -p 'test_*.py' -v
```

Expected: 25 tests PASS: 21 existing tests plus 4 new verifier tests.

- [ ] **Step 8: Verify formatting and commit Task 1**

Run:

```bash
git diff --check
git status --short
```

Expected: only the five Task 1 files are changed; no build directory or `AGENTS.md` appears.

Commit:

```bash
git add .gitignore algorithm/native/extracted/.gitignore ops/py/verify_offline.py ops/py/test_verify_offline.py ops/py/README.md
git commit -m "build: add offline verification entry"
```

## Task 2: Close the Review Lifecycle and Add the Online Checklist

**Files:**

- Modify: `docs/review/project-foundation/2026-09-04-review.md`
- Modify: `docs/review/README.md`
- Create: `docs/tech/project-foundation-corp-network-checklist.md`

**Interfaces:**

- Consumes: 状态集合 `OPEN`、`RESOLVED_OFFLINE`、`BLOCKED_BY_CORP_NETWORK`、`VERIFIED_ON_CORP_NETWORK`、`DEFERRED`。
- Produces: `PF-SEC-01A` 至 `PF-GOV-02` 的状态表和当前证据。
- Produces: 一份只能在公司网络填写的非敏感验收清单。
- Boundary: 保留原始严重度和历史影响，不删除或改写为“从未发生”。

- [ ] **Step 1: Rewrite the foundation review as historical finding plus current status**

Keep the original title and direction definition in `docs/review/project-foundation/2026-09-04-review.md`. Replace the remaining sections with these sections in order:

1. `## 原始发现`：保留 P0 凭据与 HTTP 风险、发现时证据和影响，并明确行号属于 2026-09-04 快照。
2. `## 当前处置状态`：写入 Spec 第五章的 10 行状态表。
3. `## 当前代码证据`：引用 `ops/py/sr_exec.py:32-35,44-57,62-73,97-109`、`ops/py/test_sr_exec.py:24-96,99-147`、`algorithm/src/test/java/com/mamba/landlord/benchmark/ScoringAndSplittingBenchmarkTest.java:29`、`algorithm/src/test/java/com/mamba/landlord/benchmark/ShuffleAndScoringBenchmarkTest.java:25`、`algorithm/native/CMakeLists.txt:15-20` 和 `algorithm/native/test/main_test.cpp:9-15`。
4. `## 公司网络阻塞项`：只列 `PF-SEC-01B`、`PF-SEC-01D`、`PF-SEC-02`，链接在线清单。
5. `## 责任边界`：写入 Spec 第八章的四个角色及其负责范围，不填写个人姓名。
6. `## 离线验证`：明确统一入口尚未在本分支执行，Task 3 必须用真实结果替换该说明。
7. `## 基础设施 Skill 决策`：保留“不创建”的结论，引用确定性控制理由。

The status table must contain exactly these mappings:

```text
PF-SEC-01A RESOLVED_OFFLINE
PF-SEC-01B BLOCKED_BY_CORP_NETWORK
PF-SEC-01C RESOLVED_OFFLINE
PF-SEC-01D BLOCKED_BY_CORP_NETWORK
PF-SEC-02 BLOCKED_BY_CORP_NETWORK
PF-REL-01 RESOLVED_OFFLINE
PF-TEST-01 RESOLVED_OFFLINE
PF-TEST-02 RESOLVED_OFFLINE
PF-GOV-01 RESOLVED_OFFLINE
PF-GOV-02 RESOLVED_OFFLINE
```

- [ ] **Step 2: Update the review index without rewriting history**

In `docs/review/README.md`:

- Rename `## 本轮审查` to `## 原始审查结果`.
- State that P0/P1/P2 counts are the 2026-09-04 discovery counts, not current open counts.
- Add `## 当前基础设施状态` with totals: 7 `RESOLVED_OFFLINE`, 3 `BLOCKED_BY_CORP_NETWORK`, 0 `OPEN`.
- Replace the stale verification bullets that say 68 Java tests include benchmark and CMake is missing with links to the current foundation Review and unified verifier.
- Keep the two business-agent review links and the decision not to create `project-foundation` Skill.

- [ ] **Step 3: Create the company-network acceptance checklist**

Create `docs/tech/project-foundation-corp-network-checklist.md` with:

```markdown
# 项目基础设施公司网络验收清单

## 一、使用边界

本清单只能在公司网络执行。不得把 credential、session token、完整认证响应或敏感查询结果写入本文件。

执行前由用户确认目标账号、环境和允许的只读验证范围。DDL 继续由用户在 CloudBeaver 手动执行，项目脚本不得执行 DDL。

## 二、验收记录

| 字段 | 内容 |
| ---- | ---- |
| 日期 | |
| 执行者角色 | |
| 环境 | |
| 代码提交 | |
| 非敏感证据位置 | |

## 三、检查项

| 标识 | 检查项 | 通过条件 | 状态 | 非敏感证据 |
| ---- | ---- | ---- | ---- | ---- |
| `PF-SEC-01B` | 轮换历史暴露凭据 | 旧凭据登录失败，新凭据登录成功 | 未执行 | |
| `PF-SEC-01D` | 验证 HTTPS、证书链与失败响应 | HTTPS 登录和代表性只读查询成功，未使用 HTTP 例外；登录失败、查询失败和超时不泄露认证材料 | 未执行 | |
| `PF-SEC-02` | 验证最小权限 | 查询账号可读目标 DWS，DDL 与未授权写入被服务端拒绝；如保留写入账号，确认其用途、授权范围和审批边界 | 未执行 | |

允许的状态只有 `未执行`、`通过`、`失败`。任何一项为 `未执行` 或 `失败` 时，对应 Review 状态保持 `BLOCKED_BY_CORP_NETWORK`。

## 四、执行后动作

- 全部通过：把对应 Review 状态改为 `VERIFIED_ON_CORP_NETWORK`，记录日期、环境、代码提交和非敏感证据位置。
- 任一失败：保留 `BLOCKED_BY_CORP_NETWORK`，记录失败类型和下一动作，不记录敏感响应正文。
- 发现代码问题：创建独立修复任务，完成离线回归后重新执行本清单。
```

- [ ] **Step 4: Verify status integrity and Markdown formatting**

Run:

```bash
rg -n "PF-(SEC|REL|TEST|GOV)-" docs/review/project-foundation/2026-09-04-review.md docs/tech/project-foundation-corp-network-checklist.md
rg -n '68 项|混入 8 项 benchmark|未安装 `cmake`' docs/review
git diff --check
```

Expected: every foundation identifier is present where required; stale verification phrases have no matches; diff check passes.

- [ ] **Step 5: Commit Task 2**

```bash
git add docs/review/README.md docs/review/project-foundation/2026-09-04-review.md docs/tech/project-foundation-corp-network-checklist.md
git commit -m "docs(review): track foundation remediation status"
```

## Task 3: Run the Offline Gate and Record the Result

**Files:**

- Modify: `docs/review/project-foundation/2026-09-04-review.md`
- Modify: `docs/spec/2026-09-05-project-foundation-design.md`

**Interfaces:**

- Consumes: `python3 ops/py/verify_offline.py` from Task 1.
- Produces: dated offline verification evidence in the foundation Review.
- Produces: Spec `验证状态 | 离线验证通过，等待公司网络验收` only if the full command returns `0`.
- Boundary: no `BLOCKED_BY_CORP_NETWORK` item changes state in this task.

- [ ] **Step 1: Run the single offline verification entry**

Run from the repository root:

```bash
python3 ops/py/verify_offline.py
```

Expected:

- Python suite passes 25 tests.
- Python compile check exits `0`.
- Java default suite passes without benchmark classes.
- Java benchmark profile passes with benchmark classes included.
- CMake Release configure and `landlord_test` build exit `0`.
- CTest discovers and passes `1/1` test.
- Final output is `Offline verification passed.` and exit code is `0`.

If any step fails, stop this task. Record the failing step and do not mark the Spec as passed.

- [ ] **Step 2: Verify the runner left no visible build artifacts**

Run:

```bash
git status --short
git check-ignore algorithm/native/build-release algorithm/native/extracted/harness
```

Expected: build paths are ignored; status is clean before the Task 3 documentation edits and does not contain `AGENTS.md`.

- [ ] **Step 3: Record exact offline evidence**

In the foundation Review `## 离线验证` section, record:

- execution date `2026-09-05`;
- command `python3 ops/py/verify_offline.py`;
- Python, Java, benchmark and CTest counts from the actual output;
- final exit code `0`;
- explicit statement that CloudBeaver and StarRocks were not accessed.

In the Spec information table, replace:

```text
验证状态 | 待实施
```

with:

```text
验证状态 | 离线验证通过，等待公司网络验收
```

Do not change the three `BLOCKED_BY_CORP_NETWORK` rows.

- [ ] **Step 4: Run final consistency checks**

Run:

```bash
rg -n "BLOCKED_BY_CORP_NETWORK" docs/spec/2026-09-05-project-foundation-design.md docs/review/project-foundation/2026-09-04-review.md docs/tech/project-foundation-corp-network-checklist.md
rg -n "VERIFIED_ON_CORP_NETWORK" docs/spec/2026-09-05-project-foundation-design.md docs/review/project-foundation/2026-09-04-review.md
git diff --check
git status --short
```

Expected: the three online findings remain blocked; `VERIFIED_ON_CORP_NETWORK` appears only as a permitted future state, not a current result; only Task 3 documentation changes are uncommitted.

- [ ] **Step 5: Commit Task 3**

```bash
git add docs/review/project-foundation/2026-09-04-review.md docs/spec/2026-09-05-project-foundation-design.md
git commit -m "docs(foundation): record offline verification"
```

## Final Verification

Run:

```bash
python3 ops/py/verify_offline.py
git diff --check 7508dcc..HEAD
git status --short
```

Expected: the unified verifier exits `0`; the branch contains only the Spec, Plan and Tasks 1–3 changes; the working tree is clean; no company-network status is marked verified.
