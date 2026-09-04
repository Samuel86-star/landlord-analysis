# AI 数据分析智能体离线安全改造 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成家庭网络可验证的凭据隔离、SQL 安全拦截、回填失败判定、防零除修正和分析契约。

**Architecture:** 所有 SQL 继续通过 `ops/py/sr_exec.py` 执行，安全拦截和回填校验放在公共 Python 入口。离线阶段只建立确定性控制和分析契约，不连接公司数仓，也不创建 Skill。

**Tech Stack:** Python 3 标准库、`requests`、`unittest`、StarRocks、CloudBeaver GraphQL、Markdown

**Spec:** `docs/spec/2026-09-04-data-analysis-agent-design.md`

## Global Constraints

- DDL 一律禁止通过脚本或 `sr_exec.py` 执行，必须由用户在 CloudBeaver 手动执行。
- 查询优先使用 `dws_*_daily_game`；只有缺字段或排障时回退 raw，并记录原因。
- 房间等级和玩法读取 `tcy_temp.dq_game_room_config`，不得由底分或 `play_mode` 反推。
- 百分比除法使用 `NULLIF(denominator, 0)`；用户数使用 `COUNT(DISTINCT uid)`。
- 数据库凭据不得进入仓库或日志；HTTP 只能作为显式记录的受限内网例外。
- 家庭网络下只允许标记“离线验证通过”，不得声称数仓集成通过。
- 不新增第三方依赖，不构建通用 agent runtime、SQL parser 或工作流框架。
- 回填或恢复演练必须使用用户明确批准的表和日期；未获授权时停止该步骤。

---

## File Map

| 文件 | 职责 |
| ---- | ---- |
| `ops/py/sr_exec.py` | CloudBeaver 配置、SQL 安全校验和唯一执行入口 |
| `ops/py/test_sr_exec.py` | 配置与 SQL 安全规则的离线单元测试 |
| `ops/py/backfill_runner.py` | 公共 DELETE、INSERT、COUNT 流程和结果断言 |
| `ops/py/test_backfill_runner.py` | 回填计数及失败即停的离线单元测试 |
| `ops/py/batch_insert_allgame_stat.py` | 修正执行 SQL 中的防零除表达式 |
| `ops/py/batch_insert_app_silvergame_stat.py` | 修正执行 SQL 中的防零除表达式 |
| `docs/analysis/plan/retention/retention-financial.md` | 修正分析模板中的防零除表达式 |
| `docs/analysis/plan/retention/retention-score-game.md` | 修正分析模板中的防零除表达式 |
| `ops/py/README.md` | 环境变量、失败语义和联网验证说明 |
| `docs/analysis/README.md` | 分析任务契约与正式结果的最小格式 |

## Offline Gate

Tasks 1–5 可在家庭网络完成。完成后只能记录为“离线验证通过”，然后停止；不得跳过公司网络集成验证直接创建 Skill。

### Task 1: Remove Embedded Credentials and Make Connection Configuration Explicit

**Files:**

- Modify: `ops/py/sr_exec.py:22-61`
- Create: `ops/py/test_sr_exec.py`
- Modify: `ops/py/README.md`

**Interfaces:**

- Consumes: 环境变量 `CLOUDBEAVER_BASE_URL`、`CLOUDBEAVER_USERNAME`、`CLOUDBEAVER_PASSWORD_HASH`、`CLOUDBEAVER_PROJECT_ID`。
- Produces: `StarRocksClient()`；缺少必需配置时抛出 `RuntimeError`，HTTP 未显式授权时抛出 `RuntimeError`。
- Error boundary: 登录失败只返回固定错误信息，不输出完整认证响应。
- Optional: `CLOUDBEAVER_ALLOW_HTTP=1` 仅用于已记录的受限内网例外。

- [ ] **Step 1: Write failing configuration tests**

```python
import os
import unittest
from unittest.mock import Mock, patch

from sr_exec import StarRocksClient


VALID_ENV = {
    "CLOUDBEAVER_BASE_URL": "https://flowops.example.internal/api/gql",
    "CLOUDBEAVER_USERNAME": "analyst",
    "CLOUDBEAVER_PASSWORD_HASH": "redacted-test-hash",
    "CLOUDBEAVER_PROJECT_ID": "analytics",
}


class StarRocksClientConfigTest(unittest.TestCase):
    @patch.dict(os.environ, {}, clear=True)
    def test_missing_configuration_fails_before_network_access(self):
        with self.assertRaisesRegex(RuntimeError, "CLOUDBEAVER_BASE_URL"):
            StarRocksClient()

    @patch.dict(os.environ, VALID_ENV, clear=True)
    def test_reads_connection_configuration_from_environment(self):
        client = StarRocksClient()
        self.assertEqual(client.base_url, VALID_ENV["CLOUDBEAVER_BASE_URL"])
        self.assertEqual(client.username, "analyst")
        self.assertEqual(client.project_id, "analytics")

    @patch.dict(
        os.environ,
        {
            **VALID_ENV,
            "CLOUDBEAVER_BASE_URL": "http://flowops.example.internal/api/gql",
        },
        clear=True,
    )
    def test_http_requires_explicit_opt_in(self):
        with self.assertRaisesRegex(RuntimeError, "CLOUDBEAVER_ALLOW_HTTP=1"):
            StarRocksClient()

    @patch.dict(os.environ, VALID_ENV, clear=True)
    def test_login_failure_does_not_echo_authentication_response(self):
        client = StarRocksClient()
        client.gql = Mock(
            side_effect=[
                {},
                {"data": {"authInfo": {"authStatus": "FAILED"}}, "secret": "hidden"},
            ]
        )
        with self.assertRaisesRegex(RuntimeError, "CloudBeaver login failed") as error:
            client.login()
        self.assertNotIn("hidden", str(error.exception))
```

- [ ] **Step 2: Run the tests and confirm the current hard-coded client fails them**

Run: `python3 -m unittest ops/py/test_sr_exec.py -v`

Expected: FAIL because the constructor does not expose instance configuration and does not reject missing or HTTP configuration.

- [ ] **Step 3: Replace module credentials with required environment configuration**

Implement `_required_env(name: str) -> str` and load the four required variables in `StarRocksClient.__init__`. Store them as `self.base_url`, `self.username`, `self.password_hash`, and `self.project_id`. Reject non-HTTPS URLs unless `CLOUDBEAVER_ALLOW_HTTP` equals `1`. Update `gql`, `login`, and `connect` to use instance values.

```python
def _required_env(name):
    value = os.environ.get(name, "").strip()
    if not value:
        raise RuntimeError("Missing required environment variable: {}".format(name))
    return value
```

Replace the current login exception with `RuntimeError("CloudBeaver login failed")`. Do not include the previous endpoint, username, project ID, password hash, or complete authentication response anywhere in replacement code, tests, or logs.

- [ ] **Step 4: Run the configuration tests**

Run: `python3 -m unittest ops/py/test_sr_exec.py -v`

Expected: all configuration tests PASS without network access.

- [ ] **Step 5: Document environment setup and credential rotation**

Add an `sr_exec.py` configuration table to `ops/py/README.md`. State that the real values belong in the shell or an ignored local file, HTTP requires `CLOUDBEAVER_ALLOW_HTTP=1`, and the previously committed credential must be rotated from the company network.

- [ ] **Step 6: Verify the removed values are absent**

Run: `rg -n "https?://|PASSWORD_HASH\\s*=\\s*['\\\"][A-Fa-f0-9]{32}|^USERNAME\\s*=" ops/py/sr_exec.py`

Expected: no matches containing the removed endpoint or credential; documentation may contain only environment variable names.

- [ ] **Step 7: Commit Task 1**

```bash
git add ops/py/sr_exec.py ops/py/test_sr_exec.py ops/py/README.md
git commit -m "fix(data): move CloudBeaver credentials out of source"
```

### Task 2: Enforce the DDL and Single-Statement Boundary

**Files:**

- Modify: `ops/py/sr_exec.py`
- Modify: `ops/py/test_sr_exec.py`

**Interfaces:**

- Produces: `validate_sql(sql: str) -> None`.
- Raises: `ValueError` for empty SQL, DDL as the first statement, or more than one non-empty statement.
- Enforcement point: `_submit_and_wait` calls `validate_sql` before any GraphQL request, covering CLI and imported clients.

- [ ] **Step 1: Add failing SQL validation tests**

```python
from sr_exec import validate_sql


class SqlValidationTest(unittest.TestCase):
    def test_select_with_leading_comments_is_allowed(self):
        validate_sql("-- purpose\n/* source */\nSELECT 1;")

    def test_cte_query_is_allowed(self):
        validate_sql("WITH sample AS (SELECT 1 AS id) SELECT id FROM sample")

    def test_ddl_is_rejected_case_insensitively(self):
        for keyword in ("CREATE", "alter", "Drop", "TRUNCATE"):
            with self.subTest(keyword=keyword):
                with self.assertRaisesRegex(ValueError, "DDL is forbidden"):
                    validate_sql("{} TABLE unsafe_target".format(keyword))

    def test_multiple_statements_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "single SQL statement"):
            validate_sql("SELECT 1; SELECT 2;")
```

- [ ] **Step 2: Run only the validation tests and confirm they fail**

Run: `python3 -m unittest discover -s ops/py -p 'test_sr_exec.py' -v`

Expected: FAIL because `validate_sql` does not exist.

- [ ] **Step 3: Implement conservative validation using the standard library**

Remove leading line and block comments, split on semicolons, ignore an optional trailing semicolon, and reject multiple non-empty parts. Inspect the first keyword against `CREATE`, `ALTER`, `DROP`, and `TRUNCATE`.

```python
DDL_KEYWORDS = frozenset({"CREATE", "ALTER", "DROP", "TRUNCATE"})


def validate_sql(sql):
    cleaned = re.sub(r"/\*.*?\*/", " ", sql, flags=re.DOTALL)
    cleaned = re.sub(r"--[^\r\n]*", " ", cleaned).strip()
    statements = [part.strip() for part in cleaned.split(";") if part.strip()]
    if len(statements) != 1:
        raise ValueError("Only a single SQL statement is allowed")
    match = re.match(r"([A-Za-z]+)", statements[0])
    if not match:
        raise ValueError("SQL statement keyword not found")
    if match.group(1).upper() in DDL_KEYWORDS:
        raise ValueError("DDL is forbidden in sr_exec.py; run it manually in CloudBeaver")
```

Add this deliberate limitation beside the implementation:

```python
# ponytail: conservative semicolon handling; use a real tokenizer only if valid
# string-literal semicolons become a recurring query requirement.
```

- [ ] **Step 4: Call validation at the shared submission boundary**

Add `validate_sql(sql)` as the first statement of `_submit_and_wait`. Do not add duplicate checks in `main`, `execute`, `query`, or the 20 batch scripts.

- [ ] **Step 5: Run all `sr_exec` tests**

Run: `python3 -m unittest ops/py/test_sr_exec.py -v`

Expected: all tests PASS and no network request is made.

- [ ] **Step 6: Commit Task 2**

```bash
git add ops/py/sr_exec.py ops/py/test_sr_exec.py
git commit -m "fix(data): block DDL in SQL execution path"
```

### Task 3: Fail Backfills on Missing or Zero Verification Counts

**Files:**

- Modify: `ops/py/backfill_runner.py:69-140`
- Create: `ops/py/test_backfill_runner.py`
- Modify: `ops/py/README.md`

**Interfaces:**

- Produces: `checked_row_count(result: dict, allow_zero: bool = False) -> int`.
- Modifies: `run_backfill(..., depends_on=(), allow_zero=False)`.
- Compatibility: all 20 existing callers inherit the safe default without modification.

- [ ] **Step 1: Write failing count validation tests**

```python
import unittest

from backfill_runner import checked_row_count


def result_with_count(value):
    return {
        "results": [
            {"resultSet": {"columns": [{"name": "cnt"}], "rows": [[value]]}}
        ]
    }


class CheckedRowCountTest(unittest.TestCase):
    def test_positive_count_is_returned(self):
        self.assertEqual(checked_row_count(result_with_count("12")), 12)

    def test_zero_count_fails_by_default(self):
        with self.assertRaisesRegex(RuntimeError, "0 rows"):
            checked_row_count(result_with_count(0))

    def test_zero_count_can_be_explicitly_allowed(self):
        self.assertEqual(checked_row_count(result_with_count(0), allow_zero=True), 0)

    def test_missing_result_shape_fails(self):
        with self.assertRaisesRegex(RuntimeError, "verification count"):
            checked_row_count({"results": []})
```

- [ ] **Step 2: Run the tests and confirm the helper is missing**

Run: `python3 -m unittest ops/py/test_backfill_runner.py -v`

Expected: FAIL because `checked_row_count` does not exist.

- [ ] **Step 3: Implement the count helper and safe default**

```python
def checked_row_count(result, allow_zero=False):
    try:
        rows = result["results"][0]["resultSet"]["rows"]
        count = int(rows[0][0])
    except (KeyError, IndexError, TypeError, ValueError) as exc:
        raise RuntimeError("Could not read verification count") from exc
    if count < 0:
        raise RuntimeError("Invalid verification count: {}".format(count))
    if count == 0 and not allow_zero:
        raise RuntimeError("Verification returned 0 rows")
    return count
```

Add `allow_zero=False` to `run_backfill`, replace the current nested `.get(...)` expression with `checked_row_count(check_result, allow_zero)`, and leave the existing exception path to print `FAIL` and exit with code 1.

- [ ] **Step 4: Add a failure-propagation test**

Patch `parse_args` and `StarRocksClient` with a one-day fake run whose COUNT result is zero. Assert that `run_backfill` raises `SystemExit` with code 1 and that the fake client receives DELETE, INSERT, then COUNT in that order.

- [ ] **Step 5: Run the backfill tests**

Run: `python3 -m unittest ops/py/test_backfill_runner.py -v`

Expected: all tests PASS; the fake client performs no network access.

- [ ] **Step 6: Update operator documentation**

Change `ops/py/README.md` from “0 行警告” to “0 行默认失败”。Document `allow_zero=True` as an explicit per-table exception and state that schedulers stop because the child process returns code 1.

- [ ] **Step 7: Run the complete offline Python suite**

Run: `python3 -m unittest discover -s ops/py -p 'test_*.py' -v`

Expected: all tests PASS.

- [ ] **Step 8: Commit Task 3**

```bash
git add ops/py/backfill_runner.py ops/py/test_backfill_runner.py ops/py/README.md
git commit -m "fix(data): fail backfills when verification is empty"
```

### Task 4: Apply the Existing Division-by-Zero Rule

**Files:**

- Modify: `ops/py/batch_insert_allgame_stat.py:54-55,114-115`
- Modify: `ops/py/batch_insert_app_silvergame_stat.py:86-87`
- Modify: `docs/analysis/plan/retention/retention-financial.md:535-541`
- Modify: `docs/analysis/plan/retention/retention-score-game.md:60-68,133-139,293-294,336`

**Interfaces:** None; this task only changes SQL expressions.

- [ ] **Step 1: Capture current violations**

Run:

```bash
rg -n '/[[:space:]]*(COUNT\(\*\)|COUNT\(DISTINCT|([A-Za-z_][A-Za-z0-9_]*\.)?[A-Za-z_][A-Za-z0-9_]*_money)' \
  ops/py/batch_insert_allgame_stat.py \
  ops/py/batch_insert_app_silvergame_stat.py \
  docs/analysis/plan/retention/retention-financial.md \
  docs/analysis/plan/retention/retention-score-game.md
```

Expected: matches include the reviewed direct divisions by `COUNT(*)`, `COUNT(DISTINCT r.uid)`, and `start_money`.

- [ ] **Step 2: Wrap every reviewed denominator with `NULLIF`**

Use these forms without changing numerators, aliases, filters, joins, or grouping:

```sql
... / NULLIF(COUNT(*), 0)
... / NULLIF(COUNT(DISTINCT r.uid), 0)
... / NULLIF(s.start_money, 0)
```

- [ ] **Step 3: Verify the affected Python files still compile**

Run: `python3 -m compileall -q ops/py`

Expected: exit code 0.

- [ ] **Step 4: Verify the known unsafe forms are absent**

Run the Task 4 Step 1 command again.

Expected: no direct division matches in the four files; all reviewed denominators use `NULLIF`.

- [ ] **Step 5: Review the SQL-only diff**

Run: `git diff --word-diff -- ops/py/batch_insert_allgame_stat.py ops/py/batch_insert_app_silvergame_stat.py docs/analysis/plan/retention/retention-financial.md docs/analysis/plan/retention/retention-score-game.md`

Expected: only denominator wrappers change.

- [ ] **Step 6: Commit Task 4**

```bash
git add ops/py/batch_insert_allgame_stat.py ops/py/batch_insert_app_silvergame_stat.py docs/analysis/plan/retention/retention-financial.md docs/analysis/plan/retention/retention-score-game.md
git commit -m "fix(sql): guard reviewed percentage denominators"
```

### Task 5: Define the Analysis Contract and Finish Offline Verification

**Files:**

- Create: `docs/analysis/README.md`
- Modify: `docs/spec/2026-09-04-data-analysis-agent-design.md`

**Interfaces:**

- Produces: one Markdown contract used by later human workflows and the project skill.
- Status values: `离线验证通过`, `等待公司网络联调`, `集成验证通过`, `验证失败`.

- [ ] **Step 1: Create the minimal task contract**

Add a section to `docs/analysis/README.md` requiring these fields before SQL execution:

```markdown
## 分析任务契约

- 业务问题：
- 指标口径：
- 时间范围与时区：
- 统计粒度：
- 过滤条件与对照组：
- 数据来源：
- 期望产物：临时查询、分析方案或正式结论
```

- [ ] **Step 2: Add the formal result contract**

Require: conclusion summary, metric definition, date range, filters, actual SQL path, sample size, quality checks, known limitations, generation time, and data cutoff time. State that failed or unexecuted SQL cannot produce a formal conclusion.

- [ ] **Step 3: Run all offline verification commands**

```bash
python3 -m unittest discover -s ops/py -p 'test_*.py' -v
python3 -m compileall -q ops/py
git diff --check
```

Expected: unit tests PASS, compileall exits 0, and `git diff --check` prints no errors.

- [ ] **Step 4: Record the offline gate**

In the Spec document information table, add `验证状态 | 离线验证通过，等待公司网络联调` only if Step 3 passes. If any command fails, record `验证状态 | 验证失败` and include the failing command; do not start the company network plan.

- [ ] **Step 5: Commit Task 5**

```bash
git add docs/analysis/README.md docs/spec/2026-09-04-data-analysis-agent-design.md
git commit -m "docs(data): define analysis input and result contracts"
```

## Deferred Plans

本计划完成后停止，不继续执行以下工作：

- **公司网络集成计划**：验证 CloudBeaver 连接、HTTPS、权限和 StarRocks 恢复语义。恢复方案必须基于实际能力重新设计，并在用户给出准确安全表、日期及写入授权后执行。
- **真实分析评估计划**：集成验证通过后，使用 Spec 规定的 5 至 10 个真实任务验证任务契约、SQL、质检和结论溯源。
- **Project Skill 计划**：真实任务证明流程稳定后，再创建 `.agents/skills/data-analysis-agent/SKILL.md`；首版只编排现有流程，不新增脚本。
