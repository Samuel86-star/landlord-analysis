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
10. [dws_ddz_app_gamemode_stat 表修改](#10-dws_ddz_app_gamemode_stat-表修改)
11. [dws_channel_category_map 表修改](#11-dws_channel_category_map-表修改)
12. [执行顺序建议](#12-执行顺序建议)

---

## 1. 修改历史

| 日期 | 修改内容 | 涉及表 |
| ---- | -------- | ------ |
| 2026-04-23 | 统一字段类型：bigint→int, int→tinyint, string→varchar, decimal→double | 多表 |
| 2026-04-23 | app_code 字段长度从 varchar(64) 改为 varchar(32) | 多表 |
| 2026-04-23 | 添加 colocate_with 属性 | 多表 |

---

## 2. 操作流程说明

### 重建表的标准步骤

```sql
-- Step 1: 创建新表（使用新的字段类型）
CREATE TABLE tcy_temp.{table_name}_new (...);

-- Step 2: 从旧表导入数据到新表
INSERT INTO tcy_temp.{table_name}_new SELECT * FROM tcy_temp.{table_name};

-- Step 3: 重命名旧表为备份表
ALTER TABLE tcy_temp.{table_name} RENAME tcy_temp.{table_name}_old;

-- Step 4: 重命名新表为正式表
ALTER TABLE tcy_temp.{table_name}_new RENAME tcy_temp.{table_name};

-- Step 5: 验证数据正确性后，删除旧表
DROP TABLE tcy_temp.{table_name}_old;
```

### 注意事项

1. **执行时间**：建议在业务低峰期执行，避免影响线上查询
2. **数据验证**：Step 3 后务必验证数据行数和样本数据是否正确
3. **回滚方案**：如发现问题，可通过重命名快速回滚
4. **分区数据**：大表数据导入可能耗时较长，需预留充足时间

---

## 3. dws_dq_daily_reg 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| uid | bigint(20) | int(11) | **是** | 重建表 |
| reg_date | int | date | **是** | 重建表 |

> **说明**：app_id、uid、reg_date 均为 DUPLICATE KEY 字段，必须重建表。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_dq_daily_reg_new (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `reg_date` DATE NOT NULL COMMENT "注册日期",
  `reg_datetime` datetime NULL COMMENT "注册具体时间"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `reg_date`)
COMMENT "用户日注册汇总表"
PARTITION BY RANGE(`reg_date`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "80",
    "colocate_with" = "group_daily_data"
);

-- ============================================
-- Step 2: 导入数据（需要转换 reg_date 类型）
-- ============================================
INSERT INTO tcy_temp.dws_dq_daily_reg_new
SELECT * FROM tcy_temp.dws_dq_daily_reg;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_dq_daily_reg;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_dq_daily_reg_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_dq_daily_reg RENAME dws_dq_daily_reg_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_dq_daily_reg_new RENAME dws_dq_daily_reg;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_dq_daily_reg_old;
```

---

## 4. dws_dq_daily_login 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| login_date | int | date | **是** | 重建表 |
| uid | bigint(20) | int(11) | **是** | 重建表 |
| first_app_code | varchar(64) | varchar(32) | 否 | 可 ALTER |
| first_channel_id | bigint(20) | int(11) | 否 | 可 ALTER |
| first_group_id | bigint(20) | int(11) | 否 | 可 ALTER |
| last_app_code | varchar(64) | varchar(32) | 否 | 可 ALTER |
| last_channel_id | bigint(20) | int(11) | 否 | 可 ALTER |
| last_group_id | bigint(20) | int(11) | 否 | 可 ALTER |
| most_freq_channel_id | bigint(20) | int(11) | 否 | 可 ALTER |
| most_freq_group_id | bigint(20) | int(11) | 否 | 可 ALTER |
| most_freq_app_code | varchar(64) | varchar(32) | 否 | 可 ALTER |
| channel_id_count | bigint(20) | int(11) | 否 | 可 ALTER |
| group_id_count | bigint(20) | int(11) | 否 | 可 ALTER |
| app_code_count | bigint(20) | int(11) | 否 | 可 ALTER |
| login_count | bigint(20) | int(11) | 否 | 可 ALTER |

> **说明**：app_id、login_date、uid 为 DUPLICATE KEY 字段，必须重建表。其他字段虽然可以用 ALTER，但为简化操作，统一采用重建表方式。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_dq_daily_login_new (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `login_date` date NOT NULL COMMENT "登录日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `first_login_time` datetime NULL,
  `first_app_code` varchar(32) NULL,
  `first_channel_id` int(11) NULL,
  `first_group_id` int(11) NULL,
  `last_login_time` datetime NULL,
  `last_app_code` varchar(32) NULL,
  `last_channel_id` int(11) NULL,
  `last_group_id` int(11) NULL,
  `most_freq_channel_id` int(11) NULL,
  `most_freq_group_id` int(11) NULL,
  `most_freq_app_code` varchar(32) NULL,
  `channel_id_count` int(11) NOT NULL,
  `group_id_count` int(11) NOT NULL,
  `app_code_count` int(11) NOT NULL,
  `login_count` int(11) NOT NULL
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `login_date`, `uid`)
COMMENT "玩家日登录汇总表"
PARTITION BY RANGE(`login_date`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_dq_daily_login_new
SELECT * FROM tcy_temp.dws_dq_daily_login;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_dq_daily_login;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_dq_daily_login_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_dq_daily_login RENAME dws_dq_daily_login_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_dq_daily_login_new RENAME dws_dq_daily_login;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_dq_daily_login_old;
```

---

## 5. dws_dq_app_daily_reg 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| reg_date | int | date | **是** | 重建表 |
| reg_channel_id | bigint(20) | int(11) | **是** | 重建表 |
| uid | bigint(20) | int(11) | 否 | 重建表 |
| reg_app_code | varchar(64) | varchar(32) | 否 | 重建表 |
| channel_category_name | varchar(128) | varchar(255) | 否 | 重建表 |
| channel_category_tag_id | int(11) | tinyint(4) | 否 | 重建表 |

> **说明**：app_id、reg_date、reg_channel_id 为 DUPLICATE KEY 字段，必须重建表。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_dq_app_daily_reg_new (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `reg_date` date NOT NULL COMMENT "注册日期",
  `reg_channel_id` int(11) NULL COMMENT "注册渠道ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `reg_datetime` datetime NULL COMMENT "注册具体时间",
  `reg_group_id` int(11) NULL COMMENT "注册组ID",
  `reg_app_code` varchar(32) NULL COMMENT "注册代码",
  `channel_category_id` int(11) NULL COMMENT "渠道分类ID",
  `channel_category_name` varchar(255) NULL COMMENT "渠道分类名称",
  `channel_category_tag_id` tinyint(4) NULL COMMENT "渠道标签ID",
  `is_login_log_missing` tinyint(4) NULL DEFAULT '0' COMMENT "是否缺失登录日志: 0-否, 1-是",
  `first_day_login_cnt` int(11) NULL DEFAULT '0' COMMENT "首日登录次数"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `reg_date`, `reg_channel_id`)
COMMENT "App端用户注册首日行为汇总宽表"
PARTITION BY RANGE(`reg_date`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "storage_format" = "V2",
    "enable_persistent_index" = "true",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "80",
    "bloom_filter_columns" = "uid",
    "colocate_with" = "group_daily_data"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_dq_app_daily_reg_new
SELECT * FROM tcy_temp.dws_dq_app_daily_reg;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_dq_app_daily_reg;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_dq_app_daily_reg_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_dq_app_daily_reg RENAME dws_dq_app_daily_reg_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_dq_app_daily_reg_new RENAME dws_dq_app_daily_reg;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_dq_app_daily_reg_old;
```

---

## 6. dws_ddz_daily_game 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| dt | date | date | **是** | 无需修改 |
| uid | bigint(20) | int(11) | **是** | 重建表 |
| real_magnification | decimal(10,2) | double | 否 | 可 ALTER |
| app_code | varchar(64) | varchar(32) | 否 | 可 ALTER |

> **说明**：app_id、uid 为 DUPLICATE KEY 字段，需重建表。real_magnification 和 app_code 可直接 ALTER，但统一采用重建表方式。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_ddz_daily_game_new (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `dt` DATE NOT NULL COMMENT "日期",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `game_datetime` datetime NOT NULL COMMENT "对局时间",
  `resultguid` varchar(64) NULL COMMENT "对局GUID",
  `timecost` int(11) NULL COMMENT "耗时",
  `room_id` int(11) NULL COMMENT "房间ID",
  `play_mode` tinyint(4) NULL COMMENT "玩法模式",
  `room_base` int(11) NULL COMMENT "底分",
  `room_fee` int(11) NULL COMMENT "台费",
  `room_currency_lower` bigint(20) NULL,
  `room_currency_upper` bigint(20) NULL,
  `robot` tinyint(4) NULL COMMENT "是否机器人",
  `role` tinyint(4) NULL COMMENT "角色",
  `chairno` tinyint(4) NULL COMMENT "座位号",
  `result_id` tinyint(4) NULL COMMENT "结果ID",
  `start_money` bigint(20) NULL,
  `end_money` bigint(20) NULL,
  `diff_money_pre_tax` bigint(20) NULL COMMENT "输赢数值",
  `cut` int(11) NULL,
  `safebox_deposit` int(11) NULL,
  `magnification` int(11) NULL COMMENT "倍数",
  `magnification_stacked` int(11) NULL,
  `real_magnification` double NULL COMMENT "实际倍数",
  `grab_landlord_bet` tinyint(4) NULL,
  `complete_victory_bet` tinyint(4) NULL,
  `bomb_bet` int(11) NULL,
  `channel_id` int(11) NULL,
  `group_id` int(11) NULL,
  `app_code` varchar(32) NULL,
  `game_id` int(11) NULL
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `dt`, `uid`)
COMMENT "斗地主每日游戏明细表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_ddz_daily_game_new
SELECT * FROM tcy_temp.dws_ddz_daily_game;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_ddz_daily_game;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_ddz_daily_game_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_ddz_daily_game RENAME dws_ddz_daily_game_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_ddz_daily_game_new RENAME dws_ddz_daily_game;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_ddz_daily_game_old;
```

---

## 7. dws_app_game_active 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| uid | bigint(20) | int(11) | **是** | 重建表 |
| dt | int | date | **是** | 重建表 |

> **说明**：所有字段均为 DUPLICATE KEY，必须重建表。同时添加 colocate_with 属性。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_app_game_active_new (
  `app_id` INT NOT NULL COMMENT "应用ID",
  `uid` INT NOT NULL COMMENT "用户ID",
  `dt` date NOT NULL COMMENT "日期"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `dt`)
COMMENT "用户游戏活跃信息表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "dynamic_partition.history_partition_num" = "80",
    "colocate_with" = "group_daily_data"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_app_game_active_new
SELECT * FROM tcy_temp.dws_app_game_active;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_app_game_active;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_app_game_active_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_app_game_active RENAME dws_app_game_active_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_app_game_active_new RENAME dws_app_game_active;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_app_game_active_old;
```

---

## 8. dws_app_gamemode_active 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| uid | bigint(20) | int(11) | **是** | 重建表 |
| play_mode | int | tinyint | **是** | 重建表 |
| dt | int | date | **是** | 重建表 |

> **说明**：所有字段均为 DUPLICATE KEY，必须重建表。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_app_gamemode_active_new (
  `app_id` INT NOT NULL COMMENT "应用ID",
  `uid` INT NOT NULL COMMENT "用户ID",
  `play_mode` TINYINT NOT NULL COMMENT "游戏玩法模式",
  `dt` DATE NOT NULL COMMENT "日期"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `play_mode`, `dt`)
COMMENT "玩家玩法活跃明细表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "colocate_with" = "group_daily_data",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_app_gamemode_active_new
SELECT * FROM tcy_temp.dws_app_gamemode_active;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_app_gamemode_active;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_app_gamemode_active_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_app_gamemode_active RENAME dws_app_gamemode_active_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_app_gamemode_active_new RENAME dws_app_gamemode_active;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_app_gamemode_active_old;
```

---

## 9. dws_ddz_app_game_stat 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| uid | bigint(20) | int(11) | **是** | 重建表 |
| dt | date | date | **是** | 无需修改 |
| app_code | varchar(64) | varchar(32) | 否 | 可 ALTER |

> **说明**：app_id、uid 为 DUPLICATE KEY 字段，需重建表。由于涉及 DUPLICATE KEY，统一采用重建表方式。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_ddz_app_game_stat_new (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  `app_code` varchar(32) NULL COMMENT "",
  `game_count` int(11) NULL COMMENT "",
  `total_play_seconds` int(11) NULL COMMENT "",
  `avg_game_seconds` double NULL COMMENT "",
  `win_count` int(11) NULL COMMENT "",
  `lose_count` int(11) NULL COMMENT "",
  `win_rate` double NULL COMMENT "",
  `max_win_streak` int(11) NULL COMMENT "",
  `max_lose_streak` int(11) NULL COMMENT "",
  `avg_magnification` double NULL COMMENT "",
  `max_magnification` int(11) NULL COMMENT "",
  `avg_real_magnification` double NULL COMMENT "",
  `low_multi_games` int(11) NULL COMMENT "",
  `mid_multi_games` int(11) NULL COMMENT "",
  `high_multi_games` int(11) NULL COMMENT "",
  `high_multi_wins` int(11) NULL COMMENT "",
  `high_multi_losses` int(11) NULL COMMENT "",
  `total_bomb_count` int(11) NULL COMMENT "",
  `games_with_grab` int(11) NULL COMMENT "",
  `games_player_doubled` int(11) NULL COMMENT "",
  `start_money` bigint(20) NULL COMMENT "",
  `end_money` bigint(20) NULL COMMENT "",
  `money_peak` bigint(20) NULL COMMENT "",
  `money_valley` bigint(20) NULL COMMENT "",
  `total_diff_money` bigint(20) NULL COMMENT "",
  `total_fee_paid` int(11) NULL COMMENT "",
  `escape_count` int(11) NULL COMMENT "",
  `distinct_rooms` tinyint(4) NULL COMMENT "",
  `play_modes` varchar(256) NULL COMMENT ""
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `uid`, `dt`)
COMMENT "游戏用户对局聚合信息表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "colocate_with" = "group_daily_data",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_ddz_app_game_stat_new
SELECT * FROM tcy_temp.dws_ddz_app_game_stat;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_ddz_app_game_stat;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_ddz_app_game_stat_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_ddz_app_game_stat RENAME dws_ddz_app_game_stat_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_ddz_app_game_stat_new RENAME dws_ddz_app_game_stat;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_ddz_app_game_stat_old;
```

---

## 10. dws_ddz_app_gamemode_stat 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| app_id | bigint(20) | int(11) | **是** | 重建表 |
| play_mode | int | tinyint | **是** | 重建表 |
| uid | bigint(20) | int(11) | **是** | 重建表 |
| dt | date | date | **是** | 无需修改 |
| app_code | varchar(64) | varchar(32) | 否 | 可 ALTER |

> **说明**：app_id、play_mode、uid 为 DUPLICATE KEY 字段，必须重建表。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_ddz_app_gamemode_stat_new (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `play_mode` tinyint(4) NULL COMMENT "游戏玩法",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  `app_code` varchar(32) NULL COMMENT "",
  `game_count` int(11) NULL COMMENT "",
  `total_play_seconds` int(11) NULL COMMENT "",
  `avg_game_seconds` double NULL COMMENT "",
  `win_count` int(11) NULL COMMENT "",
  `lose_count` int(11) NULL COMMENT "",
  `win_rate` double NULL COMMENT "",
  `max_win_streak` int(11) NULL COMMENT "",
  `max_lose_streak` int(11) NULL COMMENT "",
  `avg_magnification` double NULL COMMENT "",
  `max_magnification` int(11) NULL COMMENT "",
  `avg_real_magnification` double NULL COMMENT "",
  `low_multi_games` int(11) NULL COMMENT "",
  `mid_multi_games` int(11) NULL COMMENT "",
  `high_multi_games` int(11) NULL COMMENT "",
  `high_multi_wins` int(11) NULL COMMENT "",
  `high_multi_losses` int(11) NULL COMMENT "",
  `total_bomb_count` int(11) NULL COMMENT "",
  `games_with_grab` int(11) NULL COMMENT "",
  `games_player_doubled` int(11) NULL COMMENT "",
  `start_money` bigint(20) NULL COMMENT "",
  `end_money` bigint(20) NULL COMMENT "",
  `money_peak` bigint(20) NULL COMMENT "",
  `money_valley` bigint(20) NULL COMMENT "",
  `total_diff_money` bigint(20) NULL COMMENT "",
  `total_fee_paid` int(11) NULL COMMENT "",
  `escape_count` int(11) NULL COMMENT "",
  `distinct_rooms` tinyint(4) NULL COMMENT ""
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `play_mode`, `uid`, `dt`)
COMMENT "游戏玩法用户对局聚合信息表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "colocate_with" = "group_daily_data",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-80",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_ddz_app_gamemode_stat_new
SELECT * FROM tcy_temp.dws_ddz_app_gamemode_stat;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_ddz_app_gamemode_stat;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_ddz_app_gamemode_stat_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_ddz_app_gamemode_stat RENAME dws_ddz_app_gamemode_stat_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_ddz_app_gamemode_stat_new RENAME dws_ddz_app_gamemode_stat;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_ddz_app_gamemode_stat_old;
```

---

## 11. dws_channel_category_map 表修改

### 修改说明

| 字段名 | 原类型 | 新类型 | 是否 DUPLICATE KEY | 修改方式 |
| ------ | ------ | ------ | ------------------ | -------- |
| channel_id | int | int | **是** | 无需修改 |
| channel_category_name | string | varchar(255) | 否 | 重建表 |
| channel_category_tag_id | int(11) | tinyint(4) | 否 | 重建表 |

> **说明**：StarRocks 不支持 int → tinyint 类型修改，即使字段不在 DUPLICATE KEY 中，也需重建表。

### 操作步骤

```sql
-- ============================================
-- Step 1: 创建新表
-- ============================================
CREATE TABLE tcy_temp.dws_channel_category_map_new (
  `channel_id` int(11) NOT NULL COMMENT "渠道ID",
  `channel_category_id` int(11) NULL COMMENT "分类ID",
  `channel_category_name` varchar(255) NULL COMMENT "分类名称",
  `channel_category_tag_id` tinyint(4) NULL COMMENT "标签ID"
) ENGINE=OLAP
DUPLICATE KEY(`channel_id`)
COMMENT "渠道分类映射配置表"
DISTRIBUTED BY HASH(`channel_id`) BUCKETS 1
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4"
);

-- ============================================
-- Step 2: 导入数据
-- ============================================
INSERT INTO tcy_temp.dws_channel_category_map_new
SELECT * FROM tcy_temp.dws_channel_category_map;

-- ============================================
-- Step 3: 验证数据
-- ============================================
SELECT COUNT(*) FROM tcy_temp.dws_channel_category_map;      -- 旧表行数
SELECT COUNT(*) FROM tcy_temp.dws_channel_category_map_new;  -- 新表行数

-- ============================================
-- Step 4: 重命名旧表
-- ============================================
ALTER TABLE tcy_temp.dws_channel_category_map RENAME dws_channel_category_map_old;

-- ============================================
-- Step 5: 重命名新表
-- ============================================
ALTER TABLE tcy_temp.dws_channel_category_map_new RENAME dws_channel_category_map;

-- ============================================
-- Step 6: 验证完成后删除旧表
-- ============================================
DROP TABLE tcy_temp.dws_channel_category_map_old;
```

---

## 12. 执行顺序建议

### 依赖关系分析

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
dws_ddz_app_gamemode_stat ← 依赖 dws_ddz_daily_game
       ↓
dws_channel_category_map  ← 维表（无依赖）
```

### 建议执行顺序

1. **dws_dq_daily_reg** - 基础注册表
2. **dws_dq_daily_login** - 基础登录表
3. **dws_dq_app_daily_reg** - 依赖上述两表
4. **dws_ddz_daily_game** - 基础对局表
5. **dws_app_game_active** - 依赖对局表
6. **dws_app_gamemode_active** - 依赖对局表
7. **dws_ddz_app_game_stat** - 依赖对局表
8. **dws_ddz_app_gamemode_stat** - 依赖对局表
9. **dws_channel_category_map** - 维表（无依赖，可随时执行）

### 执行前检查

```sql
-- 检查表是否存在
SHOW TABLES FROM tcy_temp LIKE 'dws_%';

-- 检查当前字段定义
DESC tcy_temp.dws_dq_daily_reg;
DESC tcy_temp.dws_dq_daily_login;
DESC tcy_temp.dws_dq_app_daily_reg;
DESC tcy_temp.dws_ddz_daily_game;
DESC tcy_temp.dws_app_game_active;
DESC tcy_temp.dws_app_gamemode_active;
DESC tcy_temp.dws_ddz_app_game_stat;
DESC tcy_temp.dws_ddz_app_gamemode_stat;
DESC tcy_temp.dws_channel_category_map;
```

### 执行后验证

```sql
-- 验证字段修改结果
DESC tcy_temp.dws_dq_daily_reg;
DESC tcy_temp.dws_dq_daily_login;
DESC tcy_temp.dws_dq_app_daily_reg;
DESC tcy_temp.dws_ddz_daily_game;
DESC tcy_temp.dws_app_game_active;
DESC tcy_temp.dws_app_gamemode_active;
DESC tcy_temp.dws_ddz_app_game_stat;
DESC tcy_temp.dws_ddz_app_gamemode_stat;
DESC tcy_temp.dws_channel_category_map;

-- 验证 colocate_with 属性
SHOW CREATE TABLE tcy_temp.dws_dq_daily_reg;
SHOW CREATE TABLE tcy_temp.dws_dq_daily_login;
SHOW CREATE TABLE tcy_temp.dws_dq_app_daily_reg;
SHOW CREATE TABLE tcy_temp.dws_ddz_daily_game;
SHOW CREATE TABLE tcy_temp.dws_app_game_active;
SHOW CREATE TABLE tcy_temp.dws_app_gamemode_active;
SHOW CREATE TABLE tcy_temp.dws_ddz_app_game_stat;
SHOW CREATE TABLE tcy_temp.dws_ddz_app_gamemode_stat;

-- 验证数据行数
SELECT 'dws_dq_daily_reg' AS tbl, COUNT(*) AS cnt FROM tcy_temp.dws_dq_daily_reg
UNION ALL SELECT 'dws_dq_daily_login', COUNT(*) FROM tcy_temp.dws_dq_daily_login
UNION ALL SELECT 'dws_dq_app_daily_reg', COUNT(*) FROM tcy_temp.dws_dq_app_daily_reg
UNION ALL SELECT 'dws_ddz_daily_game', COUNT(*) FROM tcy_temp.dws_ddz_daily_game
UNION ALL SELECT 'dws_app_game_active', COUNT(*) FROM tcy_temp.dws_app_game_active
UNION ALL SELECT 'dws_app_gamemode_active', COUNT(*) FROM tcy_temp.dws_app_gamemode_active
UNION ALL SELECT 'dws_ddz_app_game_stat', COUNT(*) FROM tcy_temp.dws_ddz_app_game_stat
UNION ALL SELECT 'dws_ddz_app_gamemode_stat', COUNT(*) FROM tcy_temp.dws_ddz_app_gamemode_stat
UNION ALL SELECT 'dws_channel_category_map', COUNT(*) FROM tcy_temp.dws_channel_category_map;
```

---

## 附录：快速执行脚本

> **警告**：以下脚本用于快速执行，请在测试环境验证后再在生产环境执行。

```sql
-- ============================================
-- 快速执行所有表修改（按依赖顺序）
-- 执行前请确认已在测试环境验证
-- ============================================

-- 1. dws_channel_category_map（维表，无依赖，可直接 ALTER）
ALTER TABLE tcy_temp.dws_channel_category_map MODIFY COLUMN channel_category_name varchar(255) NULL COMMENT "渠道分类名称";
ALTER TABLE tcy_temp.dws_channel_category_map MODIFY COLUMN channel_category_tag_id tinyint(4) NULL COMMENT "渠道标签ID";

-- 2. dws_dq_daily_reg（需重建）
-- ... 执行上述 Step 1-6 ...

-- 3. dws_dq_daily_login（需重建）
-- ... 执行上述 Step 1-6 ...

-- 4. dws_dq_app_daily_reg（需重建）
-- ... 执行上述 Step 1-6 ...

-- 5. dws_ddz_daily_game（需重建）
-- ... 执行上述 Step 1-6 ...

-- 6. dws_app_game_active（需重建）
-- ... 执行上述 Step 1-6 ...

-- 7. dws_app_gamemode_active（需重建）
-- ... 执行上述 Step 1-6 ...

-- 8. dws_ddz_app_game_stat（需重建）
-- ... 执行上述 Step 1-6 ...

-- 9. dws_ddz_app_gamemode_stat（需重建）
-- ... 执行上述 Step 1-6 ...
```

---

> **文档版本**：v3.0
> **创建时间**：2026-04-23
> **更新时间**：2026-04-23
> **维护说明**：
>
> - v1.0：初始版本，使用 ALTER SQL
> - v2.0：补充 colocate_with 属性
> - **v3.0**：改用重建表方式处理 DUPLICATE KEY 字段类型修改
