#!/usr/bin/env python3
"""
CloudBeaver / FlowOps StarRocks 查询客户端
用法:
    python3 sr_exec.py "SELECT * FROM some_table LIMIT 10"
    python3 sr_exec.py -f query.sql

实现说明：
    1. CloudBeaver 异步任务的 statusMessage='Executed' 仅代表"任务跑完"，
       不代表"数据真正落盘"。SR 在 strict mode 下因脏数据回滚整批 INSERT 时，
       任务仍以 Executed 结束、GraphQL 层看不到错误。因此调用方在 INSERT 后
       必须用 SELECT COUNT 复核行数（backfill_runner 的 check_template 即做此事）。
       排查 strict mode 静默回滚见 ops/troubleshooting.md。
    2. asyncSqlExecuteResults 对 INSERT/DELETE 有两种成功返回路径：
         a) 整个 r 字段为 null → GraphQL 抛 NullValueInNonNullableField
         b) r.statusMessage='Executed' 且 r.results[0].resultSet=null
       _verify_status / query 都已处理这两条路径。
    3. -f 读 SQL 文件强制 utf-8，不依赖系统 locale（Windows 下默认 GBK 会炸）。
"""

import argparse
import json
import os
import re
import sys
import time
from urllib.parse import urlparse

import requests


def _required_env(name: str) -> str:
    value = os.environ.get(name, "").strip()
    if not value:
        raise RuntimeError("Missing required environment variable: {}".format(name))
    return value


DDL_KEYWORDS = frozenset({"CREATE", "ALTER", "DROP", "TRUNCATE"})


# ponytail: conservative semicolon handling; use a real tokenizer only if valid
# string-literal semicolons become a recurring query requirement.
def validate_sql(sql):
    statement, delimiter, suffix = sql.partition(";")
    if delimiter:
        trailing = re.sub(r"/\*.*?\*/", " ", suffix, flags=re.DOTALL)
        trailing = re.sub(r"--[^\r\n]*", " ", trailing)
        if ";" in suffix or trailing.strip():
            raise ValueError("Only a single SQL statement is allowed")
    cleaned = re.sub(r"/\*.*?\*/", " ", statement, flags=re.DOTALL)
    cleaned = re.sub(r"--[^\r\n]*", " ", cleaned).strip()
    match = re.match(r"([A-Za-z]+)", cleaned)
    if not match:
        raise ValueError("SQL statement keyword not found")
    if match.group(1).upper() in DDL_KEYWORDS:
        raise ValueError("DDL is forbidden in sr_exec.py; run it manually in CloudBeaver")


class StarRocksClient:
    def __init__(self):
        self.base_url = _required_env("CLOUDBEAVER_BASE_URL")
        self.username = _required_env("CLOUDBEAVER_USERNAME")
        self.password_hash = _required_env("CLOUDBEAVER_PASSWORD_HASH")
        self.project_id = _required_env("CLOUDBEAVER_PROJECT_ID")
        scheme = urlparse(self.base_url).scheme
        if scheme != "https" and not (
            scheme == "http" and os.environ.get("CLOUDBEAVER_ALLOW_HTTP") == "1"
        ):
            raise RuntimeError(
                "CLOUDBEAVER_BASE_URL must use HTTPS; "
                "CLOUDBEAVER_ALLOW_HTTP=1 permits HTTP"
            )
        self.s = requests.Session()
        # flowops 是内网域名，强制忽略环境变量里的代理设置（HTTP_PROXY / HTTPS_PROXY 等），
        # 避免在配了代理的机器上走代理失败。团队任何机器都能直连内网，无需代理软件。
        self.s.trust_env = False
        self.conn_id = None
        self.ctx_id = None

    def gql(self, query, variables=None, op_name=None):
        body = {"query": query}
        if variables:
            body["variables"] = variables
        if op_name:
            body["operationName"] = op_name
        r = self.s.post(self.base_url, json=body, timeout=60)
        # 端点异常（如被本机代理 fake-ip 劫持返回 HTML 403/502）时，r.json() 会抛
        # 晦涩的 JSONDecodeError(char 0)。先校验状态码/内容类型，给出可定位的报错。
        if r.status_code != 200 or "json" not in (r.headers.get("content-type") or "").lower():
            raise Exception(
                f"gql non-JSON response: HTTP {r.status_code} "
                f"content-type={r.headers.get('content-type')!r} body[:200]={r.text[:200]!r}"
            )
        return r.json()

    def login(self):
        """登录 CloudBeaver"""
        try:
            self.gql("mutation { session: openSession(defaultLocale: \"zh\") { createTime valid } }")
            res = self.gql("""
                query login($p: ID!, $c: Object) {
                    authInfo: authLogin(provider: $p, credentials: $c) { authStatus }
                }
            """, {"p": "local", "c": {"user": self.username, "password": self.password_hash}})
        except Exception:
            raise RuntimeError("CloudBeaver login failed") from None
        if res.get("data", {}).get("authInfo", {}).get("authStatus") != "SUCCESS":
            raise RuntimeError("CloudBeaver login failed")
        return self

    def connect(self):
        """获取 StarRocks 连接并创建 SQL 上下文"""
        res = self.gql(
            "query connections($projectId: ID!) { "
            "connections: userConnections(projectIds: [$projectId]) { id name } }",
            {"projectId": self.project_id},
        )
        conns = res.get("data", {}).get("connections", [])
        if not conns:
            raise Exception("No connections found")
        self.conn_id = conns[0]["id"]
        res = self.gql(
            "mutation createCtx($cid: ID!) { ctx: sqlContextCreate(connectionId: $cid) { id } }",
            {"cid": self.conn_id}
        )
        self.ctx_id = res["data"]["ctx"]["id"]
        return self

    def _submit_and_wait(self, sql, filter=None):
        """提交 SQL 异步任务并轮询至完成，返回 task_id。

        filter: 可选 CloudBeaver SQLDataFilter dict，如 {"offset": N, "limit": M}，
                用于分页/限制结果集（突破默认 200 行/页）。见 query_paged。
        """
        validate_sql(sql)
        if filter is not None:
            res = self.gql(
                "mutation execSql($cid: ID!, $ctx: ID!, $sql: String!, $filter: SQLDataFilter)"
                " { result: asyncSqlExecuteQuery(connectionId: $cid, contextId: $ctx, sql: $sql, filter: $filter) { id } }",
                {"cid": self.conn_id, "ctx": self.ctx_id, "sql": sql, "filter": filter}
            )
        else:
            res = self.gql(
                "mutation execSql($cid: ID!, $ctx: ID!, $sql: String!) { result: asyncSqlExecuteQuery(connectionId: $cid, contextId: $ctx, sql: $sql) { id } }",
                {"cid": self.conn_id, "ctx": self.ctx_id, "sql": sql}
            )
        if "errors" in res:
            raise Exception("SQL execution error: {}".format(res['errors']))
        task_id = res["data"]["result"]["id"]

        task_running = True
        for _ in range(600):
            time.sleep(1)
            info = self.gql(
                "mutation check($id: String!) { t: asyncTaskInfo(id: $id, removeOnFinish: false) { id running } }",
                {"id": task_id}
            )
            task = info.get("data", {}).get("t", {})
            task_running = task.get("running", True)
            if not task_running:
                break
        if task_running:
            raise Exception("Task timed out after 600s")
        return task_id

    def _verify_status(self, task_id):
        """查询任务结果状态。

        CloudBeaver 对 INSERT/DELETE 有两种成功返回路径：
        1. 整个 r 为 null → GraphQL 抛 NullValueInNonNullableField（旧路径）
        2. r.statusMessage='Executed'，但 r.results[0].resultSet=null（新路径）
        两者都视为成功。注意 statusMessage='Executed' 不代表数据落盘（见模块 docstring 1）。
        """
        res = self.gql("""
            mutation getRes($tid: ID!) {
                r: asyncSqlExecuteResults(taskId: $tid) {
                    statusMessage
                    duration
                    results { title resultSet { columns { name } rows } }
                }
            }
        """, {"tid": task_id})
        if "errors" in res:
            for err in res["errors"]:
                if "NullValueInNonNullableField" in err.get("extensions", {}).get("classification", ""):
                    return None
            raise Exception("Result fetch error: {}".format(res["errors"]))
        return res.get("data", {}).get("r")

    def execute_write(self, sql):
        """执行 INSERT/DELETE/UPDATE，提交后校验任务执行状态。

        注意：本方法只能拦"任务真没跑"或"GraphQL 层错误"，拦不住 strict mode 下
        SR 静默回滚。调用方必须事后用 SELECT COUNT 复核数据落盘情况。
        """
        task_id = self._submit_and_wait(sql)
        result_data = self._verify_status(task_id)
        if result_data is None:
            return  # 路径 1：r=null，视为已执行
        status = result_data.get("statusMessage")
        if status != "Executed":
            raise Exception("Write not executed (status={}): {}".format(status, str(result_data)[:300]))

    def execute(self, sql):
        """执行 SQL 并返回结果"""
        task_id = self._submit_and_wait(sql)
        result_data = self._verify_status(task_id)
        if result_data is None:
            return {"statusMessage": "Executed", "results": []}
        if result_data.get("statusMessage") != "Executed":
            raise Exception(f"Query not executed: {result_data.get('statusMessage')}")
        return result_data

    def query(self, sql):
        """执行 SQL 并返回 pandas DataFrame（如果可用）"""
        data = self.execute(sql)
        results = data.get("results", [])
        if not results:
            return []
        rs = results[0].get("resultSet")
        # INSERT/DELETE 路径 2：r 存在但 resultSet=null，无数据可返回
        if rs is None:
            return []
        columns = [c["name"] for c in rs.get("columns", [])]
        rows = rs.get("rows", [])

        try:
            import pandas as pd
            return pd.DataFrame(rows, columns=columns)
        except ImportError:
            return {"columns": columns, "rows": rows}

    def query_paged(self, sql, page_size=5000):
        """分页拉取 SQL 全量结果为 pandas DataFrame。

        CloudBeaver 异步查询默认只返回前 200 行（单页硬限制），无法满足把明细
        拉回 Python 处理的需求（如 hand_cards 解析）。本方法用 SQLDataFilter
        的 offset/limit 循环分页，拼成单个 DataFrame。

        page_size: 每页行数，默认 5000（实测可正常返回，远超默认 200）。
        """
        import pandas as pd
        frames = []
        columns = None
        offset = 0
        while True:
            task_id = self._submit_and_wait(
                sql, filter={"offset": offset, "limit": page_size}
            )
            rd = self._verify_status(task_id)
            if rd is None:
                raise Exception(
                    f"query_paged: offset={offset} 查询失败（CloudBeaver r=null，可能 SQL 错误）"
                )
            results = rd.get("results", [])
            if not results:
                break
            rs = results[0].get("resultSet")
            if rs is None:
                break
            if columns is None:
                columns = [c["name"] for c in rs.get("columns", [])]
            rows = rs.get("rows", [])
            if not rows:
                break
            frames.append(pd.DataFrame(rows, columns=columns))
            if len(rows) < page_size:
                break
            offset += page_size
        if not frames:
            return pd.DataFrame()
        return pd.concat(frames, ignore_index=True)


def main():
    parser = argparse.ArgumentParser(description="StarRocks query via CloudBeaver")
    parser.add_argument("sql", nargs="?", help="SQL query string")
    parser.add_argument("-f", "--file", help="Read SQL from file")
    args = parser.parse_args()

    if args.file:
        with open(args.file, encoding="utf-8") as f:
            sql = f.read()
    elif args.sql:
        sql = args.sql
    else:
        print("Reading SQL from stdin (Ctrl+D to execute)...")
        sql = sys.stdin.read()

    if not sql.strip():
        print("No SQL provided")
        sys.exit(1)

    client = StarRocksClient()
    client.login().connect()

    result = client.query(sql)
    if isinstance(result, list):
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print(result.to_string())


if __name__ == "__main__":
    main()
