# 排查经验

> 本文档记录数据回填、查询过程中遇到的疑难问题及排查方法，供后续遇到类似情况参考。每条按"症状 → 排查 → 根因 → 修复"组织。

---

## 1. INSERT 脚本报 OK 但目标表 0 行（strict mode 静默回滚）

### 症状

回填脚本输出 `OK (0 rows, Xs)`，未抛任何异常，但目标表对应日期查不到数据。表现为：

- 小批量（如 LIMIT 100、1 万行）能成功写入
- 全量或大批量 INSERT 报 OK 却 0 行
- 不同 uid 切片批量看似随机成功/失败（实际取决于该批是否踩到脏行）

### 排查

核心结论：**CloudBeaver GraphQL API 的 `statusMessage=Executed` 不可信**。SR 在 strict mode 下因数据质量过滤回滚整批时，任务仍会以 `running=false` + `statusMessage=Executed` 结束，GraphQL 层看不到 SR 内部的 load 失败。在 API 层改任何 statusMessage 校验都拦不住这类失败。

正确的排查路径是查 SR 的 load 追踪日志：

1. 在 CloudBeaver 网页端跑那条失败的 INSERT（网页端会显示 SR 的真实报错，不像 GraphQL API 会吞错）。
2. 报错形如：

   ```text
   SQL Error [1064] [42000]: Insert has filtered data in strict mode, txn_id=XXX tracking sql = select tracking_log from information_schema.load_tracking_logs where job_id=YYY
   ```

3. 用报错里的 `job_id` 查脏行详情：

   ```sql
   SELECT tracking_log
   FROM information_schema.load_tracking_logs
   WHERE job_id = YYY;
   ```

4. `tracking_log` 会列出被过滤的行及原因，例如字段长度超限、类型不匹配、NULL 写入 NOT NULL 列等。

### 根因（本次案例）

`dws_ddz_daily_game.hand_cards` 定义为 `varchar(32)`，但部分对局的手牌字符串超过 32 字符。strict mode 下这些行被过滤，触发整批回滚。小批量之所以成功，是恰好没踩到超长行。

### 修复

1. 修改目标表字段定义（DDL 必须在 CloudBeaver 网页端手动执行，不走 `sr_exec.py`）：

   ```sql
   ALTER TABLE tcy_temp.dws_ddz_daily_game
   MODIFY COLUMN `hand_cards` varchar(64) NULL COMMENT "手牌";

   ALTER TABLE tcy_temp.dws_ddz_firstday_game
   MODIFY COLUMN `hand_cards` varchar(64) NULL COMMENT "手牌";
   ```

2. 同步更新对应 md 文档里的建表 SQL 和字段说明表（保持文档与数仓一致）。
3. 重跑回填脚本（脚本内 DELETE 会先清掉之前部分写入的不完整数据）。

### 预防

- 建表时对从 JSON 提取的字符串字段（hand_cards、bottom_cards、路径字段等）预留足够长度，别用刚好够的尺寸。
- 回填脚本跑完后，养成用 `SELECT COUNT(*)` 复核行数的习惯；若与上游对不上，第一时间查 `load_tracking_logs`，不要在 API 层猜原因。

---

## 2. `sr_exec.py` 的 INSERT 返回值处理

### 背景

CloudBeaver 对 INSERT/DELETE 的 `asyncSqlExecuteResults` 有两种成功返回路径，`sr_exec.py` 需同时识别：

1. 整个 `r` 字段为 null → GraphQL 抛 `NullValueInNonNullableField` 错误（旧路径）
2. `r.statusMessage='Executed'`，但 `r.results[0].resultSet=null`（新路径）

两者都代表"任务提交并完成"，但**不代表数据真正落盘**（见上文 strict mode 静默回滚）。

### 注意

- `execute_write` 已加 `statusMessage` 校验，能拦住"任务真没跑"或"GraphQL 层错误"，但拦不住 strict mode 回滚。
- `query` 方法已处理 `resultSet=null`，不会再因 INSERT 返回空结果集而崩溃。
- 真正的写入完整性校验依赖调用方的事后 COUNT 复核（`backfill_runner` 的 `check_template` 即做此事）。
