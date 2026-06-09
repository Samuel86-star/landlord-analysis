# 表结构修改操作手册

> 本文档记录 DWS 层中间表的表结构修改操作步骤，用于同步 StarRocks 线上表结构。
>
> **重要说明**：StarRocks 不支持直接修改 DUPLICATE KEY 中的字段类型，需通过「建新表 → 导数据 → 重命名 → 删旧表」的方式操作。

---

## 目录

1. [修改历史](#1-修改历史)
2. [操作流程说明](#2-操作流程说明)
3. [dws_dq_daily_reg 表修改](#3-dws_dq_daily_reg-表修改)
4. [dws_dq_daily_login 表修改](#4-dws_dq_daily_login-表修改)
5. [dws_dq_app_daily_reg 表修改](#5-dws_dq_app_daily_reg-表修改)
6. [dws_ddz_daily_game 表修改](#6-dws_ddz_daily_game-表修改)
7. [dws_app_game_active 表修改](#7-dws_app_game_active-表修改)
8. [dws_app_gamemode_active 表修改](#8-dws_app_gamemode_active-表修改)
9. [dws_ddz_app_game_stat 表修改](#9-dws_ddz_app_game_stat-表修改)
10. [dws_app_allgame_stat 表修改](#10-dws_app_allgame_stat-表修改)
11. [dws_channel_category_map 表修改](#11-dws_channel_category_map-表修改)
12. [执行顺序建议](#12-执行顺序建议)

## 10. dws_app_allgame_stat 表修改

> 该表原名 `dws_app_gamemode_stat`，文档版本 v3.0 起更名为 `dws_app_allgame_stat`，以与姊妹表 `silvergame`/`scoregame` 命名体系统一。StarRocks 线上旧表可通过 `ALTER TABLE ... RENAME` 操作完成重命名。

### 表重命名

```sql
ALTER TABLE tcy_temp.dws_app_gamemode_stat RENAME dws_app_allgame_stat;
```

### 执行顺序建议

```text
dws_dq_daily_reg          ← 基础表
       ↓
dws_dq_daily_login        ← 基础表
       ↓
dws_dq_app_daily_reg      ← 依赖上述两表
       ↓
dws_ddz_daily_game        ← 基础表
       ↓
dws_app_game_active       ← 依赖 dws_ddz_daily_game
dws_app_gamemode_active   ← 依赖 dws_ddz_daily_game
dws_ddz_app_game_stat     ← 依赖 dws_ddz_daily_game
dws_app_allgame_stat      ← 依赖 dws_ddz_daily_game, dws_crazyddz_daily_game
       ↓
dws_channel_category_map  ← 维表（无依赖）
```

### 执行前检查

```sql
SHOW TABLES FROM tcy_temp LIKE 'dws_%';
DESC tcy_temp.dws_app_allgame_stat;
```

### 执行后验证

```sql
DESC tcy_temp.dws_app_allgame_stat;
SHOW CREATE TABLE tcy_temp.dws_app_allgame_stat;
SELECT 'dws_app_allgame_stat' AS tbl, COUNT(*) AS cnt FROM tcy_temp.dws_app_allgame_stat;
```

---

> **文档版本**：v3.1
> **创建时间**：2026-04-23
> **更新时间**：2026-06-09
> **维护说明**：表名从 dws_app_gamemode_stat 更新为 dws_app_allgame_stat。
