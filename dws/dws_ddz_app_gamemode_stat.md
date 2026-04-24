# DWS 中间表：APP 端每日游戏行为统计表（按玩法）

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_ddz_app_gamemode_stat` |
| 全名 | `tcy_temp.dws_ddz_app_gamemode_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日游戏行为统计表（按玩法拆分），与 `dws_ddz_app_game_stat` 字段一致，粒度增加 play_mode 维度 |
| 粒度 | uid × dt × play_mode × app_code（一个用户一天一种玩法一个客户端版本一行） |

## 设计背景

`dws_ddz_app_game_stat` 的粒度为 uid × dt，一天内玩了多种玩法（经典/不洗牌/赖子/比赛）的用户数据混合在一起。但不同玩法的倍数分布差异显著：

| 玩法 | 倍数特点 | 影响 |
| ---- | ------- | ---- |
| 经典 | 标准倍数，炸弹频率适中 | 基准线 |
| 不洗牌 | 保留上局牌序，连续炸弹概率更高 | 倍数偏高 |
| 赖子 | 万能牌存在，炸弹概率远高于经典 | 倍数显著偏高 |
| 比赛 | 独立规则 | 单独分析 |

**影响链条**：玩法 → 倍数分布 → 单局输赢 → 经济变化/破产 → 留存

混合分析会将玩法差异误读为用户行为差异。例如赖子玩法用户的"高倍局占比高"是玩法特性而非用户激进。

**解决方案**：新建按玩法拆分的聚合表，保留原表不动。两表并存，按需使用：

- 不关心玩法差异的分析（如按对局数分组）→ 用原表
- 与倍数/经济/胜率/时长相关的分析 → 用本表

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| play_mode | tinyint | 玩法分类：1=经典，2=不洗牌，3=赖子，5=比赛 | 1 |
| uid | int | 玩家唯一标识 | 123456789 |
| dt | date | 对局日期 | 2026-02-10 |
| app_code | varchar(32) | 客户端代码（zgdx=cocos creator, zgda=cocos lua） | "zgdx" |
| game_count | int | 当日该玩法对局总数 | 8 |
| total_play_seconds | int | 当日该玩法总游戏时长（秒） | 2400 |
| avg_game_seconds | double | 该玩法平均每局时长 | 180.5 |
| win_count | int | 该玩法胜利局数 | 5 |
| lose_count | int | 该玩法失败局数 | 3 |
| win_rate | double | 该玩法胜率（百分比） | 62.50 |
| max_win_streak | int | 该玩法最大连胜 | 3 |
| max_lose_streak | int | 该玩法最大连败 | 2 |
| avg_magnification | double | 该玩法平均理论倍数 | 12.5 |
| max_magnification | int | 该玩法最大理论倍数 | 48 |
| avg_real_magnification | double | 该玩法平均实际倍数（ABS） | 10.2 |
| low_multi_games | int | 低倍局数（magnification <= 6） | 3 |
| mid_multi_games | int | 中倍局数（6 < magnification <= 24） | 3 |
| high_multi_games | int | 高倍局数（magnification > 24） | 2 |
| high_multi_wins | int | 高倍局胜利数 | 1 |
| high_multi_losses | int | 高倍局失败数 | 1 |
| total_bomb_count | int | 当日该玩法炸弹总数 | 6 |
| games_with_grab | int | 抢地主局数 | 4 |
| games_player_doubled | int | 玩家加倍局数 | 2 |
| start_money | bigint | 该玩法首局前货币数量 | 10000 |
| end_money | bigint | 该玩法末局后货币数量 | 12000 |
| money_peak | bigint | 该玩法货币峰值 | 15000 |
| money_valley | bigint | 该玩法货币谷值 | 8000 |
| total_diff_money | bigint | 该玩法总输赢（含服务费还原） | 2000 |
| total_fee_paid | int | 该玩法总服务费 | 800 |
| escape_count | int | 该玩法逃跑次数 | 0 |
| distinct_rooms | int | 该玩法游玩房间数 | 2 |

## 客户端开发语言说明

| app_code | 客户端开发语言 | 界面和流程特点 |
| ------- | ------------ | -------------- |
| zgdx | Cocos Creator | 界面和流程较新，体验优化 |
| zgda | Cocos Lua | 界面和流程较传统 |

> **说明**：本表支持按客户端开发语言和玩法双维度分析用户行为差异。通过 `app_code` 和 `play_mode` 字段区分不同客户端版本和玩法的用户，粒度为 uid × dt × play_mode × app_code（一个用户一天一种玩法一个客户端版本一行）。

## 玩法分类说明

| play_mode | 玩法 | 币种 |
| --------- | ---- | ---- |
| 1 | 经典 | 银子 |
| 2 | 不洗牌 | 银子 |
| 3 | 癞子 | 银子 |
| 5 | 比赛（APP/小游戏端） | 银子 |

> **说明**：本表仅统计 APP 端用户（`group_id` IN 6,66,8,88,33,44,77,99）的银子玩法（play_mode IN 1,2,3,5），排除 PC 端积分玩法。

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_ddz_app_gamemode_stat (
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
```

### 增量数据导入

```sql
insert into tcy_temp.dws_ddz_app_gamemode_stat 
WITH game_enriched AS (
    SELECT
        *,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode, app_code ORDER BY game_datetime ASC) AS game_seq,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode, app_code ORDER BY game_datetime DESC) AS rank_desc
    FROM tcy_temp.dws_ddz_daily_game
    WHERE dt between '2026-02-10' and '2026-04-21'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
      AND play_mode IN (1, 2, 3, 5)
),
streaks_calc AS (
    SELECT 
        uid, app_code, play_mode, result_id, COUNT(*) AS streak_len
    FROM (
        SELECT 
            uid, app_code, play_mode, result_id, 
            game_seq - ROW_NUMBER() OVER (PARTITION BY uid, play_mode, app_code, result_id ORDER BY game_seq) AS grp
        FROM game_enriched
        WHERE result_id IN (1, 2)
    ) t
    GROUP BY uid, app_code, play_mode, result_id, grp
),
max_streaks AS (
    SELECT 
        uid, app_code, play_mode,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM streaks_calc
    GROUP BY uid, app_code, play_mode
)
SELECT
    g.app_id,
    g.play_mode,
    g.uid,
    g.dt,
    g.app_code,
    COUNT(*) AS game_count,
    SUM(g.timecost) AS total_play_seconds,
    ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
    COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
    COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
    ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
    ANY_VALUE(s.max_win_streak),
    ANY_VALUE(s.max_lose_streak),
    ROUND(AVG(g.magnification), 2),
    MAX(g.magnification),
    ROUND(AVG(ABS(g.real_magnification)), 2),
    COUNT(CASE WHEN g.magnification <= 6 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 6 AND g.magnification <= 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 1 THEN 1 END),
    COUNT(CASE WHEN g.magnification > 24 AND g.result_id = 2 THEN 1 END),
    SUM(g.bomb_bet / 2),
    COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END),
    COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END),
    MAX(CASE WHEN g.game_seq = 1 THEN g.start_money END), 
    MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END),
    MAX(g.end_money),
    MIN(g.end_money),
    SUM(g.diff_money_pre_tax),
    SUM(g.room_fee),
    COUNT(CASE WHEN g.cut < 0 THEN 1 END),
    COUNT(DISTINCT g.room_id)
FROM game_enriched g
LEFT JOIN max_streaks s ON g.uid = s.uid AND g.play_mode = s.play_mode AND g.app_code = s.app_code
GROUP BY g.app_id, g.play_mode, g.uid, g.dt, g.app_code;
```

## 注意事项

1. **与原表的关系**：本表是 `dws_ddz_app_game_stat` 的按玩法拆分版本，两表并存互补
   - 原表（uid × dt）：适合不需要区分玩法的分析（如按对局数分组、总体经济变化）
   - 本表（uid × dt × play_mode）：适合需要控制玩法变量的分析（如倍数、胜率、连胜连败、经济变化）
2. **行数膨胀**：一天内玩了 N 种玩法的用户会产生 N 行记录（原表只有 1 行），预计行数约为原表的 1.2-1.5 倍（多数用户只玩一种玩法）
3. **start_money / end_money**：按玩法内的时间顺序取首局/末局，不同玩法之间的银子变化可能交叉（用户在经典和赖子间切换时银子是连续的）
4. **连胜连败**：按玩法内的对局序列计算，跨玩法的连胜连败不统计
5. **数据完整性**：如用户当日在某玩法下无对局，本表无对应记录

## 与其他 DWS 表的关系

```
tcy_temp.dws_ddz_daily_game              （对局明细表）
            ↓  APP端+银子玩法聚合
tcy_temp.dws_ddz_app_game_stat         （用户每日统计 - 混合玩法）
tcy_temp.dws_ddz_app_gamemode_stat （用户每日统计 - 按玩法拆分）  ← 本表
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg              （APP 端注册用户宽表）
tcy_temp.dws_dq_daily_login                （每日登录聚合表）
```

> **文档版本**：v1.0
> **创建时间**：2026-04-13
> **更新说明**：

> - v1.0：初始版本，从 `dws_ddz_app_game_stat` 拆分出按玩法维度的聚合表
