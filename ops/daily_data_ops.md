# 每日数据增量更新操作手册

> 本文档汇总所有 DWS 层中间表的增量更新方式（脚本引用 + 执行命令），供日常运维使用。各表完整 SQL 与字段说明见对应的 md 文档。

---

## 目录

1. [渠道分类维表初始化](#1-渠道分类维表初始化)
2. [注册数据增量更新](#2-注册数据增量更新)
3. [登录数据增量更新](#3-登录数据增量更新)
4. [APP 端注册用户宽表增量更新](#4-app-端注册用户宽表增量更新)
5. [Hive→SR 对局战绩原始字段表迁移](#5-hivesr-对局战绩原始字段表迁移)
6. [疯狂斗地主原始字段表迁移](#6-疯狂斗地主原始字段表迁移)
7. [对局战绩统一字段表增量更新](#7-对局战绩统一字段表增量更新)
8. [疯狂斗地主对局战绩表增量更新](#8-疯狂斗地主对局战绩表增量更新)
9. [APP 端每日游戏活跃用户表增量更新](#9-app-端每日游戏活跃用户表增量更新)
10. [APP 端每日游戏活跃用户×玩法表增量更新](#10-app-端每日游戏活跃用户玩法表增量更新)
11. [银子玩法每日统计表增量更新](#11-银子玩法每日统计表增量更新)
12. [积分玩法每日统计表增量更新](#12-积分玩法每日统计表增量更新)
13. [全玩法体验统计表增量更新（按玩法拆分）](#13-全玩法体验统计表增量更新按玩法拆分)
14. [首日对局数据初始化](#14-首日对局数据初始化)
15. [分玩法首日对局特征宽表初始化](#15-分玩法首日对局特征宽表初始化)
16. [银子变动日志增量更新](#16-银子变动日志增量更新)
17. [游戏道具流水日志增量更新](#17-游戏道具流水日志增量更新)
18. [执行顺序与依赖关系](#18-执行顺序与依赖关系)
19. [常见问题](#19-常见问题)

---

## 1. 渠道分类维表初始化

| 目标表 | 说明 |
| ------ | ---- |
| `tcy_temp.dws_channel_category_map` | 渠道分类维表，优先初始化（其他表可能关联） |

> 维表初始化 SQL 详见 [config/dq_channel_category_map.md](../starrocks/config/dq_channel_category_map.md)。暂无回填脚本，按需手动维护。

---

## 2. 注册数据增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st` | `tcy_temp.dws_dq_daily_reg` | [py/batch_insert_daily_reg.py](../py/batch_insert_daily_reg.py) |

按天 `DELETE + INSERT`（幂等可重跑），无前置依赖。

```powershell
py -3 -u .\batch_insert_daily_reg.py --start 20260617 --end 20260617
```

> 完整说明详见 [account/dws_dq_daily_reg.md](../starrocks/account/dws_dq_daily_reg.md)。

---

## 3. 登录数据增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `tcy_dwd.dwd_tcy_userlogin_si` | `tcy_temp.dws_dq_daily_login` | [py/batch_insert_daily_login.py](../py/batch_insert_daily_login.py) |

按天 `DELETE + INSERT`（幂等可重跑），无前置依赖。

```powershell
py -3 -u .\batch_insert_daily_login.py --start 20260617 --end 20260617
```

> 完整说明详见 [account/dws_dq_daily_login.md](../starrocks/account/dws_dq_daily_login.md)。

---

## 4. APP 端注册用户宽表增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `dws_dq_daily_reg` JOIN `dws_dq_daily_login` JOIN `dq_channel_category_map` | `tcy_temp.dws_dq_app_daily_reg` | [py/batch_insert_app_daily_reg.py](../py/batch_insert_app_daily_reg.py) |

按天 `DELETE + INSERT`（幂等可重跑）。**依赖**：`dws_dq_daily_reg`、`dws_dq_daily_login` 对应日期需先回填。

```powershell
py -3 -u .\batch_insert_app_daily_reg.py --start 20260617 --end 20260617
```

> 完整说明详见 [account/dws_dq_app_daily_reg.md](../starrocks/account/dws_dq_app_daily_reg.md)。

---

## 5. Hive→SR 对局战绩原始字段表迁移

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `hive_catalog_cdh5.dwd.fact_game_combatgains`（Hive，game_id=53） | `tcy_temp.ddz_daily_game_raw` | [py/batch_insert_ddz_daily_game_raw.py](../py/batch_insert_ddz_daily_game_raw.py) |

按天 `DELETE + INSERT`（幂等可重跑），从 Hive 搬运到 StarRocks，保持原始字段不做转换。**依赖**：上游 Hive 表对应日期分区已就绪。

```powershell
py -3 -u .\batch_insert_ddz_daily_game_raw.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/ddz/ddz_daily_game_raw.md](../starrocks/game/ddz/ddz_daily_game_raw.md)。

---

## 6. 疯狂斗地主原始字段表迁移

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `tcy_dwd.dwd_game_combatgains_si`（SR 全量日志，game_id=521） | `tcy_temp.crazyddz_daily_game_raw` | [py/batch_insert_crazyddz_daily_game_raw.py](../py/batch_insert_crazyddz_daily_game_raw.py) |

按天 `DELETE + INSERT`（幂等可重跑）。**跨天对局处理**：脚本内部扫描 `[T, T+1]`，用 `MIN(dt) OVER(PARTITION BY resultguid)` 归属为 `min_dt = T` 的对局。日常回填 dt-1（T-1 可用），首次跑某天时需保证 T+1 源数据已就绪。

```powershell
py -3 -u .\batch_insert_crazyddz_daily_game_raw.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/crazyddz/crazyddz_daily_game_raw.md](../starrocks/game/crazyddz/crazyddz_daily_game_raw.md)。

---

## 7. 对局战绩统一字段表增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `tcy_temp.ddz_daily_game_raw`（game_id=53） | `tcy_temp.dws_ddz_daily_game` | [py/batch_insert_ddz_daily_game.py](../py/batch_insert_ddz_daily_game.py) |

按天 `DELETE + INSERT`（幂等可重跑）。完成货币字段统一、玩法分类、JSON 解析。**依赖**：`ddz_daily_game_raw` 对应日期需先回填。

```powershell
py -3 -u .\batch_insert_ddz_daily_game.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/ddz/dws_ddz_daily_game.md](../starrocks/game/ddz/dws_ddz_daily_game.md)。

---

## 8. 疯狂斗地主对局战绩表增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `tcy_temp.crazyddz_daily_game_raw`（game_id=521） | `tcy_temp.dws_crazyddz_daily_game` | [py/batch_insert_crazyddz_daily_game.py](../py/batch_insert_crazyddz_daily_game.py) |

按天 `DELETE + INSERT`（幂等可重跑）。多轮结算聚合为整局一行，**T-1 可用**（上游 raw 已通过 min_dt 覆盖跨天对局）。

```powershell
py -3 -u .\batch_insert_crazyddz_daily_game.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/crazyddz/dws_crazyddz_daily_game.md](../starrocks/game/crazyddz/dws_crazyddz_daily_game.md)。

---

## 9. APP 端每日游戏活跃用户表增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `dws_ddz_daily_game` ∪ `dws_crazyddz_daily_game`（APP 端真人） | `tcy_temp.dws_app_game_active` | [py/batch_insert_app_game_active.py](../py/batch_insert_app_game_active.py) |

按天 `DELETE + INSERT`（幂等可重跑），留存 flag 专用活跃清单。**依赖**：`dws_ddz_daily_game`、`dws_crazyddz_daily_game`。

```powershell
py -3 -u .\batch_insert_app_game_active.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/dws_app_game_active.md](../starrocks/game/dws_app_game_active.md)。

---

## 10. APP 端每日游戏活跃用户×玩法表增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `dws_ddz_daily_game` ∪ `dws_crazyddz_daily_game`（APP 端真人） | `tcy_temp.dws_app_gamemode_active` | [py/batch_insert_app_gamemode_active.py](../py/batch_insert_app_gamemode_active.py) |

按天 `DELETE + INSERT`（幂等可重跑），同玩法留存 flag 专用。**依赖**：`dws_ddz_daily_game`、`dws_crazyddz_daily_game`。

```powershell
py -3 -u .\batch_insert_app_gamemode_active.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/dws_app_gamemode_active.md](../starrocks/game/dws_app_gamemode_active.md)。

---

## 11. 银子玩法每日统计表增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `dws_ddz_daily_game`(play_mode 1,2,3) ∪ `dws_crazyddz_daily_game`(play_mode=7) | `tcy_temp.dws_app_silvergame_stat` | [py/batch_insert_app_silvergame_stat.py](../py/batch_insert_app_silvergame_stat.py) |

按天 `DELETE + INSERT`（幂等可重跑），银子玩法金流 + 参与度（uid × dt）。**依赖**：`dws_ddz_daily_game`、`dws_crazyddz_daily_game`。

```powershell
py -3 -u .\batch_insert_app_silvergame_stat.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/dws_app_silvergame_stat.md](../starrocks/game/dws_app_silvergame_stat.md)。

---

## 12. 积分玩法每日统计表增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `dws_ddz_daily_game`(play_mode 4,5,6) | `tcy_temp.dws_app_scoregame_stat` | [py/batch_insert_app_scoregame_stat.py](../py/batch_insert_app_scoregame_stat.py) |

按天 `DELETE + INSERT`（幂等可重跑），积分玩法参与度 + 胜负（无金流，uid × dt）。**依赖**：`dws_ddz_daily_game`。

```powershell
py -3 -u .\batch_insert_app_scoregame_stat.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/dws_app_scoregame_stat.md](../starrocks/game/dws_app_scoregame_stat.md)。

---

## 13. 全玩法体验统计表增量更新（按玩法拆分）

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `dws_ddz_daily_game`(play_mode 1~6) ∪ `dws_crazyddz_daily_game`(play_mode=7) | `tcy_temp.dws_app_allgame_stat` | [py/batch_insert_allgame_stat.py](../py/batch_insert_allgame_stat.py) |

按天 `DELETE + INSERT`（幂等可重跑），全玩法体验统计（uid × dt × play_mode，v1.2 固定倍数段）。**依赖**：`dws_ddz_daily_game`、`dws_crazyddz_daily_game`。

```powershell
py -3 -u .\batch_insert_allgame_stat.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/dws_app_allgame_stat.md](../starrocks/game/dws_app_allgame_stat.md)。

---

## 14. 首日对局数据初始化

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `dws_ddz_daily_game` JOIN `dws_dq_daily_reg`（reg_date = dt） | `tcy_temp.dws_ddz_firstday_game` | [py/batch_insert_ddz_firstday_game.py](../py/batch_insert_ddz_firstday_game.py) |

按天 `DELETE + INSERT`（幂等可重跑），注册首日对局切片。**依赖**：`dws_ddz_daily_game`、`dws_dq_daily_reg` 对应日期需先回填。

```powershell
py -3 -u .\batch_insert_ddz_firstday_game.py --start 20260617 --end 20260617
```

> 完整说明详见 [game/ddz/dws_ddz_firstday_game.md](../starrocks/game/ddz/dws_ddz_firstday_game.md)。

---

## 15. 分玩法首日对局特征宽表初始化

| 目标表 | 说明 |
| ------ | ---- |
| `tcy_temp.ddz_gamemode_firstday_features` | 分玩法首日对局特征宽表 |

> 暂无回填脚本。依赖 `dws_dq_app_daily_reg`、`dws_ddz_firstday_game`、`dws_app_game_active`、`dws_app_gamemode_active`。

---

## 16. 银子变动日志增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `tcy_dwd.dwd_silver_si` JOIN `dq_channel_category_map` / `dq_currency_op_config` / `dq_currency_guid_config` | `tcy_temp.dws_dq_silver_logs` | [py/batch_insert_dq_silver_logs.py](../py/batch_insert_dq_silver_logs.py) |

按天 `DELETE + INSERT`（幂等可重跑），斗地主银子变动日志（不聚合）。**依赖**：`dwd_silver_si`、`dq_channel_category_map` 等维表。

```powershell
py -3 -u .\batch_insert_dq_silver_logs.py --start 20260617 --end 20260617
```

> 完整说明详见 [currency/dws_dq_silver_logs.md](../starrocks/currency/dws_dq_silver_logs.md)。

---

## 17. 游戏道具流水日志增量更新

| 源表 | 目标表 | 脚本 |
| ------ | -------- | ---- |
| `hive_catalog_cdh5.dwd.fact_gtpl_prop_detail`（Hive，game_id=53） | `tcy_temp.dws_prop_log` | [py/batch_insert_prop_log.py](../py/batch_insert_prop_log.py) |

按天 `DELETE + INSERT`（幂等可重跑），斗地主道具流水日志（不聚合，含 mod_name 正则提取）。**依赖**：上游 Hive 表对应日期分区已就绪。

```powershell
py -3 -u .\batch_insert_prop_log.py --start 20260617 --end 20260617
```

> 完整说明详见 [currency/dws_prop_log.md](../starrocks/currency/dws_prop_log.md)。

---

## 18. 执行顺序与依赖关系

### 表依赖关系

```text
dws_channel_category_map       ← 维表，优先初始化（其他表可能关联）
dws_dq_daily_reg               ← 无依赖，可并行执行
dws_dq_daily_login             ← 无依赖，可并行执行
ddz_daily_game_raw             ← 依赖 hive_catalog_cdh5.dwd.fact_game_combatgains (Hive)
crazyddz_daily_game_raw        ← 依赖 tcy_dwd.dwd_game_combatgains_si (SR 全量日志)
dws_ddz_daily_game             ← 依赖 ddz_daily_game_raw
dws_crazyddz_daily_game        ← 依赖 crazyddz_daily_game_raw
dws_dq_silver_logs             ← 依赖 dwd_silver_si, dq_channel_category_map 等 3 张维表
dws_prop_log                   ← 依赖 hive_catalog_cdh5.dwd.fact_gtpl_prop_detail (Hive)
dws_dq_app_daily_reg           ← 依赖 dws_dq_daily_reg, dws_dq_daily_login, dws_channel_category_map
dws_app_game_active            ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
dws_app_gamemode_active        ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
dws_app_silvergame_stat        ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
dws_app_scoregame_stat         ← 依赖 dws_ddz_daily_game
dws_app_allgame_stat           ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
dws_ddz_firstday_game          ← 依赖 dws_ddz_daily_game, dws_dq_daily_reg
ddz_gamemode_firstday_features ← 依赖 dws_dq_app_daily_reg, dws_ddz_firstday_game, dws_app_game_active, dws_app_gamemode_active
```

### 建议执行顺序

#### 推荐方式：使用调度器（一键完成）

每日数据初始化用 [py/daily_backfill.py](../py/daily_backfill.py)，自动按依赖三层串行调用 15 张表（不含 retention 域）：

```powershell
# 跑某天全部 15 张表
py -3 -u .\daily_backfill.py --start 20260617 --end 20260617

# 跑区间
py -3 -u .\daily_backfill.py --start 2026-06-01 --end 2026-06-08

# 只跑某层（补数据用，--layer 1/2/3）
py -3 -u .\daily_backfill.py --start 20260617 --end 20260617 --layer 2

# 先看执行计划不连库
py -3 -u .\daily_backfill.py --start 20260617 --end 20260617 --dry-run
```

调度器特性：
- 失败即停（避免下游空跑），提示从哪一层续跑
- 行数为 0 的表打 `⚠️ WARNING` 并汇总到结尾，需人工确认（见 [troubleshooting.md](./troubleshooting.md#1-insert-脚本报-ok-但目标表-0-行strict-mode-静默回滚)）
- 每天日志写 `py/logs/backfill_YYYY-MM-DD.log`，含每张表的完整 stdout 便于回溯

retention 域（daily_allgame_stat + retention_flag + firstday_game_stat）走独立调度 [py/daily_retention.py](../py/daily_retention.py)，默认回扫 35 天 reg_date 重算，因为 retention_flag 的 d30 留存需要 reg_date+30 天的数据齐全：

```powershell
py -3 -u .\daily_retention.py                                  # 默认回扫 today-35..today
py -3 -u .\daily_retention.py --start 20260501 --end 20260519  # 指定 reg_date 区间
py -3 -u .\daily_retention.py --window 60                      # 自定义回扫窗口
```

#### 备用：手动分步执行

调度器底层调用的就是各 `batch_insert_*.py`，需要时也可单独跑某张表：

1. **维表初始化**：dws_channel_category_map（按需手动）
2. **L1 无依赖**：daily_reg / daily_login / ddz_daily_game_raw / crazyddz_daily_game_raw / prop_log
3. **L2 依赖 L1**：app_daily_reg / ddz_daily_game / crazyddz_daily_game / dq_silver_logs
4. **L3 依赖 L2**：app_game_active / app_gamemode_active / silvergame_stat / scoregame_stat / allgame_stat / ddz_firstday_game
5. **retention 域**：daily_allgame_stat / retention_flag / firstday_game_stat（按 reg_date 回扫，详见 [py/daily_retention.py](../py/daily_retention.py)）
6. **数据校验**：`py -3 -u .\sr_exec.py -f check_data.sql`（改 sql 里日期），若 INSERT 报 OK 但行数为 0 或对不上，参见 [troubleshooting.md](./troubleshooting.md)。

## 19. 常见问题

### Q1: INSERT 脚本报 OK 但目标表 0 行 / 行数对不上

CloudBeaver GraphQL API 的 `statusMessage=Executed` **不可信**——SR 在 strict mode 下因数据质量过滤回滚整批时，任务仍会以"Executed"结束。常见触发原因是字段长度不够、类型不匹配等。

排查步骤：在 CloudBeaver 网页端跑同一句 INSERT 看真实报错，根据报错里的 `job_id` 查 `information_schema.load_tracking_logs` 定位脏行。完整流程详见 [troubleshooting.md](./troubleshooting.md#1-insert-脚本报-ok-但目标表-0-行strict-mode-静默回滚)。

### Q2: 如何手动清理某日重复数据

各回填脚本已按天 `DELETE + INSERT`，重跑即可去重，无需手动 DELETE。如需手动清理某日数据：

```sql
DELETE FROM tcy_temp.dws_dq_daily_reg WHERE app_id=1880053 AND reg_date = '2026-04-09';
DELETE FROM tcy_temp.dws_dq_daily_login WHERE app_id=1880053 AND login_date = '2026-04-09';
DELETE FROM tcy_temp.dws_ddz_daily_game WHERE game_id=53 AND dt = '2026-04-09';
DELETE FROM tcy_temp.dws_crazyddz_daily_game WHERE game_id=521 AND dt = '2026-04-09';
DELETE FROM tcy_temp.dws_app_game_active WHERE app_id=1880053 AND dt = '2026-04-09';
DELETE FROM tcy_temp.dws_app_gamemode_active WHERE app_id=1880053 AND dt = '2026-04-09';
DELETE FROM tcy_temp.dws_app_allgame_stat WHERE app_id=1880053 AND dt = '2026-04-09';
```

---

## 表清单速览

| 表名 | 用途 | 更新频率 | 依赖 | 脚本 |
| ---- | ---- | -------- | ---- | ---- |
| dws_channel_category_map | 渠道分类维表 | 按需更新 | 无 | — |
| dws_dq_daily_reg | 用户注册表 | 每日增量 | 无 | batch_insert_daily_reg.py |
| dws_dq_daily_login | 用户每日登录聚合表 | 每日增量 | 无 | batch_insert_daily_login.py |
| dws_dq_silver_logs | 斗地主银子变动日志表 | 每日增量 | dwd_silver_si, dq_channel_category_map 等 | batch_insert_dq_silver_logs.py |
| dws_dq_app_daily_reg | APP端注册用户宽表 | 每日增量 | dws_dq_daily_reg, dws_dq_daily_login | batch_insert_app_daily_reg.py |
| ddz_daily_game_raw | 对局原始字段表（Hive→SR） | 每日增量 | hive_catalog_cdh5.dwd.fact_game_combatgains | batch_insert_ddz_daily_game_raw.py |
| crazyddz_daily_game_raw | 疯狂斗地主原始字段表（SR 抽取） | 每日增量 | tcy_dwd.dwd_game_combatgains_si | batch_insert_crazyddz_daily_game_raw.py |
| dws_ddz_daily_game | 对局明细表（统一字段） | 每日增量 | ddz_daily_game_raw | batch_insert_ddz_daily_game.py |
| dws_crazyddz_daily_game | 疯狂斗地主对局战绩表 | 每日增量 | crazyddz_daily_game_raw | batch_insert_crazyddz_daily_game.py |
| dws_prop_log | 游戏道具流水日志表 | 每日增量 | hive_catalog_cdh5.dwd.fact_gtpl_prop_detail | batch_insert_prop_log.py |
| dws_app_game_active | APP端每日游戏活跃用户表 | 每日增量 | dws_ddz_daily_game, dws_crazyddz_daily_game | batch_insert_app_game_active.py |
| dws_app_gamemode_active | APP端每日游戏活跃用户×玩法表 | 每日增量 | dws_ddz_daily_game, dws_crazyddz_daily_game | batch_insert_app_gamemode_active.py |
| dws_app_silvergame_stat | 银子玩法每日统计（金流+参与度） | 每日增量 | dws_ddz_daily_game, dws_crazyddz_daily_game | batch_insert_app_silvergame_stat.py |
| dws_app_scoregame_stat | 积分玩法每日统计（参与度+胜负） | 每日增量 | dws_ddz_daily_game | batch_insert_app_scoregame_stat.py |
| dws_app_allgame_stat | 全玩法体验统计（按玩法拆分） | 每日增量 | dws_ddz_daily_game, dws_crazyddz_daily_game | batch_insert_allgame_stat.py |
| dws_ddz_firstday_game | 首日对局明细表 | 初始化 | dws_ddz_daily_game, dws_dq_daily_reg | batch_insert_ddz_firstday_game.py |
| dws_app_daily_allgame_stat | 全玩法日聚合（uid×dt） | retention 域 | dws_app_allgame_stat | batch_insert_daily_allgame_stat.py |
| dws_app_retention_flag | 首日留存 flag（NULL/0/1 三态） | retention 域 | dws_dq_app_daily_reg + game_active + daily_login | batch_insert_retention_flag.py |
| dws_app_firstday_game_stat | 首日游戏指标宽表（无 flag） | retention 域 | dws_dq_app_daily_reg + silver/score/daily_allgame_stat | batch_insert_firstday_game_stat.py |
| ddz_gamemode_firstday_features | 分玩法首日对局特征宽表 | 初始化 | dws_dq_app_daily_reg, dws_ddz_firstday_game, dws_app_game_active, dws_app_gamemode_active | — |

---

## 调度器一览

| 调度器 | 覆盖 | 何时跑 |
| ---- | ---- | ---- |
| [py/daily_backfill.py](../py/daily_backfill.py) | 15 张表（L1~L3，account + game + currency） | 每天初始化当天数据 |
| [py/daily_retention.py](../py/daily_retention.py) | 3 张表（daily_allgame_stat、retention_flag、firstday_game_stat） | 每天回扫 35 天 reg_date |

详细用法见上文「[18. 执行顺序与依赖关系](#18-执行顺序与依赖关系) → 推荐方式：使用调度器」。

---

> **文档版本**：v4.1
> **更新时间**：2026-06-18
> **维护说明**：retention 域拆分重构——`dws_app_firstday_retention` 改名为 `dws_app_firstday_game_stat`（去 flag），新增 `dws_app_retention_flag`（NULL/0/1 三态，d1/d3/d7/d14/d30）。daily_retention 调度器从 2 层升级为 3 层（A1/A2/B）。原因：解决 firstday_retention INSERT 触发 StarRocks 优化器超时（new_planner_optimize_timeout）+ 对齐"首日指标写一次不变 / 留存 flag 按到期日回填"的更新节奏。日志写入 py/logs/（已加 .gitignore 排除）。
