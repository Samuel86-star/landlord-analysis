# 配置维表：游戏房间映射表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dq_game_room_config` |
| 全名 | `tcy_temp.dq_game_room_config` |
| 类型 | 配置维表 |
| 描述 | 游戏房间映射表，记录 game_id + room_id 对应的玩法与房间等级 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_id | int | 游戏 ID（53=经典、105=四人斗地主、521=疯狂斗地主） | 105 |
| room_id | int | 房间号（源字段 `room`） | 927 |
| area_id | tinyint | 分区 ID | 1 |
| game_rule | varchar(32) | 游戏玩法：经典 / 不洗牌 / 癞子 / 连炸 | "不洗牌" |
| room_level | varchar(32) | 房间等级名称（如 新手场 / 初级场 / 中级场 / 高级场，以运营配置为准） | "新手场" |

## 玩法与等级取值说明

### game_rule 取值

| game_rule | 说明 |
| --------- | ---- |
| 经典 | 经典斗地主玩法 |
| 不洗牌 | 不洗牌玩法 |
| 癞子 | 癞子（万能牌）玩法 |
| 连炸 | 连炸玩法 |

### room_level 取值

房间等级名称按运营分场命名，常见如：新手场 / 初级场 / 中级场 / 高级场。实际取值以运营配置为准，待补充。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dq_game_room_config (
  `game_id` int(11) NOT NULL COMMENT "游戏ID",
  `room_id` int(11) NOT NULL COMMENT "房间号",
  `area_id` tinyint(4) NULL COMMENT "分区ID",
  `game_rule` varchar(32) NULL COMMENT "游戏玩法：经典/不洗牌/癞子/连炸",
  `room_level` varchar(32) NULL COMMENT "房间等级名称"
) ENGINE=OLAP
DUPLICATE KEY(`game_id`, `room_id`)
COMMENT "游戏房间映射配置表"
DISTRIBUTED BY HASH(`room_id`) BUCKETS 1
PROPERTIES (
  "replication_num" = "1",
  "compression" = "LZ4"
);
```

> **DDL 红线**：本项目禁止通过脚本执行 DDL，建表须由用户在 CloudBeaver 网页端手动执行（见根目录 CLAUDE.md）。

> **初始化数据**：本表暂只定结构，房间清单（game_id + room_id → 玩法/等级）待补。

## 使用说明

该表为维表，用于关联各游戏明细表的 `room_id`，获取房间所属玩法与等级。

### 关联查询示例

```sql
SELECT
    g.dt,
    g.room_id,
    c.game_rule,
    c.room_level,
    COUNT(DISTINCT g.uid) AS player_cnt
FROM tcy_temp.srddz_daily_game_raw g
LEFT JOIN tcy_temp.dq_game_room_config c
    ON g.game_id = c.game_id
    AND g.room_id = c.room_id
WHERE g.dt = '2026-07-02'
GROUP BY g.dt, g.room_id, c.game_rule, c.room_level;
```

## 注意事项

1. 该表为配置维表，数据量较小，适合广播 join
2. 关联时须同时匹配 `game_id` 与 `room_id`（不同游戏可能复用同一 room_id）
3. 玩法/等级取值以运营配置为准，房间清单需定期更新
4. 现仅定结构、未灌数据；下游关联前需先补齐房间映射，否则 game_rule / room_level 为 NULL
