#!/usr/bin/env python3
"""
CloudBeaver / FlowOps StarRocks 查询客户端
用法:
    python3 sr_query.py "SELECT * FROM some_table LIMIT 10"
    python3 sr_query.py -f query.sql
"""

import requests, json, time, sys, argparse

BASE = "http://flowops.tcy365.net:7788/api/gql"
USERNAME = "caohh"
# 密码已做 MD5 处理，与 CloudBeaver 前端一致
PASSWORD_HASH = "AE7810A8EB5BF4D967CFA9F63B34E770"

class StarRocksClient:
    def __init__(self):
        self.s = requests.Session()
        self.conn_id = None
        self.ctx_id = None

    def gql(self, query, variables=None, op_name=None):
        body = {"query": query}
        if variables:
            body["variables"] = variables
        if op_name:
            body["operationName"] = op_name
        r = self.s.post(BASE, json=body, timeout=60)
        return r.json()

    def login(self):
        """登录 CloudBeaver"""
        self.gql("mutation { session: openSession(defaultLocale: \"zh\") { createTime valid } }")
        res = self.gql("""
            query login($p: ID!, $c: Object) {
                authInfo: authLogin(provider: $p, credentials: $c) { authStatus }
            }
        """, {"p": "local", "c": {"user": USERNAME, "password": PASSWORD_HASH}})
        if res.get("data", {}).get("authInfo", {}).get("authStatus") != "SUCCESS":
            raise Exception(f"Login failed: {res}")
        return self

    def connect(self):
        """获取 StarRocks 连接并创建 SQL 上下文"""
        res = self.gql("query { connections: userConnections(projectIds: [\"u_caohh\"]) { id name } }")
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

    def execute(self, sql):
        """执行 SQL 并返回结果"""
        # 提交查询
        res = self.gql(
            "mutation execSql($cid: ID!, $ctx: ID!, $sql: String!) { result: asyncSqlExecuteQuery(connectionId: $cid, contextId: $ctx, sql: $sql) { id } }",
            {"cid": self.conn_id, "ctx": self.ctx_id, "sql": sql}
        )
        if "errors" in res:
            raise Exception(f"SQL execution error: {res['errors']}")
        task_id = res["data"]["result"]["id"]

        # 轮询等待完成
        for _ in range(60):
            time.sleep(1)
            info = self.gql(
                "mutation check($id: String!) { t: asyncTaskInfo(id: $id, removeOnFinish: false) { id status running } }",
                {"id": task_id}
            )
            task = info.get("data", {}).get("t", {})
            if not task.get("running"):
                break

        # 获取结果
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
            raise Exception(f"Result fetch error: {res['errors']}")

        result_data = res.get("data", {}).get("r", {})
        if result_data.get("statusMessage") != "Executed":
            raise Exception(f"Query not executed: {result_data.get('statusMessage')}")

        return result_data

    def query(self, sql):
        """执行 SQL 并返回 pandas DataFrame（如果可用）"""
        data = self.execute(sql)
        results = data.get("results", [])
        if not results:
            return []
        rs = results[0].get("resultSet", {})
        columns = [c["name"] for c in rs.get("columns", [])]
        rows = rs.get("rows", [])

        try:
            import pandas as pd
            return pd.DataFrame(rows, columns=columns)
        except ImportError:
            return {"columns": columns, "rows": rows}


def main():
    parser = argparse.ArgumentParser(description="StarRocks query via CloudBeaver")
    parser.add_argument("sql", nargs="?", help="SQL query string")
    parser.add_argument("-f", "--file", help="Read SQL from file")
    args = parser.parse_args()

    if args.file:
        with open(args.file) as f:
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
