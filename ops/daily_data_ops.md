# 每日数据增量更新操作手册

> 本文档汇总所有 DWS 层中间表的增量数据更新 SQL，供日常运维使用。

---

## 目录

1. [渠道分类维表初始化](#1-渠道分类维表初始化)
2. [注册数据增量更新](#2-注册数据增量更新)
3. [登录数据增量更新](#3-登录数据增量更新)
4. [APP 端注册用户宽表增量更新](#4-app-端注册用户宽表增量更新)
5. [对局战绩统一字段表增量更新](#5-对局战绩统一字段表增量更新)
6. [APP 端每日游戏活跃用户表增量更新](#6-app-端每日游戏活跃用户表增量更新)
7. [APP 端每日游戏活跃用户×玩法表增量更新](#7-app-端每日游戏活跃用户玩法表增量更新)
8. [用户每日游戏行为聚合增量更新（混合玩法）](#8-用户每日游戏行为聚合增量更新混合玩法)
9. [用户每日游戏行为聚合增量更新（按玩法拆分）](#9-用户每日游戏行为聚合增量更新按玩法拆分)
10. [首日对局数据初始化](#10-首日对局数据初始化)
11. [分玩法首日对局特征宽表初始化](#11-分玩法首日对局特征宽表初始化)
12. [银子变动日志增量更新](#12-银子变动日志增量更新)
13. [疯狂斗地主对局数据增量更新](#13-疯狂斗地主对局数据增量更新)
14. [游戏道具流水日志增量更新](#14-游戏道具流水日志增量更新)
15. [对局数据增量更新](#15-对局数据增量更新)
16. [执行顺序与依赖关系](#16-执行顺序与依赖关系)
17. [常见问题](#17-常见问题)

---

## 9. 用户每日游戏行为聚合增量更新（按玩法拆分）

### 源表与目标表

| 源表 | 目标表 |
| ------ | -------- |
| `tcy_temp.dws_ddz_daily_game` | `tcy_temp.dws_app_allgame_stat` |

> 完整增量 SQL 和说明详见 [game/dws_app_allgame_stat.md](../starrocks/game/dws_app_allgame_stat.md)。

---

## 16. 执行顺序与依赖关系

### 表依赖关系

```text
dws_channel_category_map       ← 维表，优先初始化（其他表可能关联）
dws_dq_daily_reg               ← 无依赖，可并行执行
dws_dq_daily_login             ← 无依赖，可并行执行
dws_ddz_daily_game             ← 无依赖，可并行执行
dws_crazyddz_daily_game        ← 无依赖，可并行执行
dws_game_prop_log              ← 无依赖，可并行执行
dws_dq_silver_logs             ← 依赖 dws_channel_category_map
dws_dq_app_daily_reg           ← 依赖 dws_dq_daily_reg, dws_dq_daily_login, dws_channel_category_map
dws_app_game_active            ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
dws_app_gamemode_active        ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
dws_ddz_app_game_stat          ← 依赖 dws_ddz_daily_game
dws_app_allgame_stat           ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
dws_ddz_firstday_game          ← 依赖 dws_ddz_daily_game, dws_dq_daily_reg
ddz_gamemode_firstday_features ← 依赖 dws_dq_app_daily_reg, dws_ddz_firstday_game, dws_app_game_active, dws_app_gamemode_active
```

### 建议执行顺序

1. **初始化阶段**：执行维表初始化（dws_channel_category_map）
2. **每日凌晨 02:00**：并行执行基础表增量导入（dws_dq_daily_reg、dws_dq_daily_login、dws_ddz_daily_game、dws_crazyddz_daily_game、dws_game_prop_log、dws_dq_silver_logs）
3. **每日凌晨 03:00**：执行依赖表增量导入（dws_dq_app_daily_reg、dws_app_game_active、dws_app_gamemode_active、dws_ddz_app_game_stat、dws_app_allgame_stat）
4. **首日数据构建**：执行首日对局数据和宽表初始化（dws_ddz_firstday_game、ddz_gamemode_firstday_features）
5. **数据校验**：检查导入数据量是否符合预期

## 17. 常见问题

### Q3: 如何删除重复数据？

```sql
-- 删除某日重复数据后重新导入
DELETE FROM tcy_temp.dws_dq_daily_reg WHERE reg_date = 20260409;
DELETE FROM tcy_temp.dws_dq_daily_login WHERE login_date = '2026-04-09';
DELETE FROM tcy_temp.dws_dq_silver_logs WHERE dt = '2026-04-09';
DELETE FROM tcy_temp.dws_ddz_daily_game WHERE dt = 20260409;
DELETE FROM tcy_temp.dws_app_game_active WHERE dt = '2026-04-09';
DELETE FROM tcy_temp.dws_app_gamemode_active WHERE dt = '2026-04-09';
DELETE FROM tcy_temp.dws_ddz_app_game_stat WHERE dt = 20260409;
DELETE FROM tcy_temp.dws_app_allgame_stat WHERE dt = 20260409;
```

---

## 表清单速览

| 表名 | 用途 | 更新频率 | 依赖 |
| ---- | ---- | -------- | ---- |
| dws_channel_category_map | 渠道分类维表 | 按需更新 | 无 |
| dws_dq_daily_reg | 用户注册表 | 每日增量 | 无 |
| dws_dq_daily_login | 用户每日登录聚合表 | 每日增量 | 无 |
| dws_dq_silver_logs | 斗地主银子变动日志表 | 每日增量 | dws_channel_category_map |
| dws_dq_app_daily_reg | APP端注册用户宽表 | 每日增量 | dws_dq_daily_reg, dws_dq_daily_login |
| dws_ddz_daily_game | 对局明细表（统一字段） | 每日增量 | 无 |
| dws_crazyddz_daily_game | 疯狂斗地主对局战绩表 | 每日增量 | 无 |
| dws_game_prop_log | 游戏道具流水日志表 | 每日增量 | 无 |
| dws_app_game_active | APP端每日游戏活跃用户表 | 每日增量 | dws_ddz_daily_game, dws_crazyddz_daily_game |
| dws_app_gamemode_active | APP端每日游戏活跃用户×玩法表 | 每日增量 | dws_ddz_daily_game, dws_crazyddz_daily_game |
| dws_ddz_app_game_stat | APP端每日游戏行为统计（混合玩法） | 每日增量 | dws_ddz_daily_game |
| dws_app_allgame_stat | APP端每日游戏行为统计（按玩法拆分） | 每日增量 | dws_ddz_daily_game, dws_crazyddz_daily_game |
| dws_ddz_firstday_game | 首日对局明细表 | 初始化 | dws_ddz_daily_game, dws_dq_daily_reg |
| ddz_gamemode_firstday_features | 分玩法首日对局特征宽表 | 初始化 | dws_dq_app_daily_reg, dws_ddz_firstday_game, dws_app_game_active, dws_app_gamemode_active |

---

> **文档版本**：v2.2
> **更新时间**：2026-06-09
> **维护说明**：如有新增 DWS 表，请及时更新本文档。表名从 dws_app_gamemode_stat 更新为 dws_app_allgame_stat。
