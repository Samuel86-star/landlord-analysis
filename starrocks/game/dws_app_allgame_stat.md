# DWS 中间表：APP 端每日游戏行为统计表（全玩法）

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_allgame_stat` |
| 全名 | `tcy_temp.dws_app_allgame_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日游戏行为统计表，涵盖所有玩法（含积分玩法），按 play_mode 拆分，专注控制玩法变量的体验分析。倍数/炸弹等玩法特异指标见本表，金流归因见 `dws_app_silvergame_stat`（银子）与 `dws_app_scoregame_stat`（积分） |
| 粒度 | uid × dt × play_mode（一个用户一天一种玩法一行） |

> **命名说明**：表名中的 `allgame` 标识本表覆盖**全部玩法**（1~7），含银子与积分，按 play_mode 拆分。早期名为 `dws_app_gamemode_stat`，因与姊妹表 `silvergame`/`scoregame` 命名体系统一而改名。

## 姊妹表：银子 / 积分按币种分表 + 全玩法体验表

金流字段（`total_diff_money`、`money_valley` 等）只在同币种内可加总，因此金流层按币种拆成两张同构表；玩法体验层不分币种，统一走本表：

| 表名 | 粒度 | 币种 | play_mode | 用途 |
| ---- | ---- | ---- | --------- | ---- |
| `dws_app_silvergame_stat` | uid × dt | 银子 | 1=经典, 2=不洗牌, 3=癞子, 7=510K | 银子金流 + 参与度（跨玩法可加总） |
| `dws_app_scoregame_stat` | uid × dt | 积分 | 4=积分(PC), 5=比赛, 6=好友房 | 积分参与度 + 胜负（无金流） |
| `dws_app_allgame_stat`（本表） | uid × dt × play_mode | 银子/积分 | 1~7 全部 | 玩法体验分析（倍数/炸弹/胜率等玩法特异指标） |

> **边界约定**：silvergame_stat 与 scoregame_stat 的 play_mode 互不重叠，并集即全部玩法。本表（allgame_stat）涵盖所有玩法并按 play_mode 拆分，不跨玩法加总金流。需要跨币种的玩法体验对比走本表。

## 设计背景

`dws_app_silvergame_stat` 与 `dws_app_scoregame_stat` 的粒度为 uid × dt，专注**按币种的金流归因与参与度**（silvergame 含银子金流，scoregame 仅参与度），不含玩法特异的倍数/炸弹指标。但做留存分析时，很多问题必须固定玩法变量才能回答：

| 分析问题 | 为什么必须按玩法拆 |
| ---- | ---- |
| "首日经历高倍局的用户，留存更高还是更低？" | 赖子天然高倍多，不拆玩法 = 把"玩了赖子"当成"经历高倍" |
| "首日大赢/大输对留存的影响？" | 赖子单局波动大，不拆 = 把"玩法波动"当成"用户运气" |
| "510K 多轮体验 vs 经典单局体验，哪个更伤留存？" | 结算方式根本不同 |
| "比赛/好友房（积分）对留存有正向作用吗？" | 积分玩法不消耗银子，走的是社交/竞技路径 |

**影响链条**：玩法 → 倍数分布/结算方式 → 单局输赢 → 经济变化/破产 → 留存

**本表的定位**：涵盖所有玩法（含积分玩法和 510K），按 play_mode 拆分，固定玩法变量，让倍数/炸弹/胜率等被玩法污染的指标变得可比。

### 与 silvergame_stat / scoregame_stat 的分工

| | dws_app_silvergame_stat | dws_app_scoregame_stat | dws_app_allgame_stat（本表） |
| ---- | ---- | ---- | ---- |
| 粒度 | uid × dt | uid × dt | uid × dt × play_mode |
| 玩法范围 | 仅银子玩法（1,2,3,7） | 仅积分玩法（4,5,6） | 所有玩法（1~7） |
| 核心指标 | 参与度、金流（银子可加总） | 参与度、胜负（无金流） | 倍数、炸弹、玩法内胜率（玩法间不可比） |
| 典型问题 | "新用户首日银子亏了多少？" | "积分玩家首日打了几局？" | "经典 vs 赖子，哪个高倍局更多？" |

### 核心判断标准

> **银子 = 金流 = 跨玩法可加总**（不管怎么赢的，亏 1000 银子就是亏 1000）→ 走 silvergame_stat
> **积分 = 免费 = 无金流压力**（留存由参与度/胜负驱动）→ 走 scoregame_stat
> **倍数 = 体验 = 玩法间不可比**（510K 的 24 倍不意味着"高风险"，经典 24 倍却很吓人）→ 走本表

## 玩法分类说明

| play_mode | 玩法 | 币种 | 备注 |
| --------- | ---- | ---- | ---- |
| 1 | 经典 | 银子 | |
| 2 | 不洗牌 | 银子 | |
| 3 | 癞子 | 银子 | |
| 4 | 积分 | 积分 | PC 端 |
| 5 | 比赛（APP/小游戏端） | 积分 | 共用 room_id=11534 积分房 |
| 6 | 好友房 | 积分 | |
| 7 | 510K | 银子 | 多轮结算，内嵌于 APP |

> **币种说明**：经济字段（`start_money` / `end_money` / `total_diff_money` 等）对银子玩法记录银子，对积分玩法记录积分。**跨币种不可直接比较金额**，分析时需按 play_mode 或币种分组。
>
> **与姊妹表的分工**：本表涵盖所有玩法（含积分玩法），按 play_mode 拆分，专注玩法体验分析。金流归因走 silvergame_stat（银子）或 scoregame_stat（积分），两表粒度均为 uid × dt，跨玩法金流可加总。

### 510K 多轮结算：上游已合并为一局一行

510K 一局内会多轮结算、银子多次变动，原始日志因此一局有多条记录。这层合并在**上游明细表已经做完**，本表无需关心多轮细节：

| 表 | resultguid + uid 粒度 | 说明 |
| ---- | ---- | ---- |
| `crazyddz_daily_game_raw`（原始日志） | 多条 | 一局内每轮结算一条记录 |
| `dws_crazyddz_daily_game`（明细表） | 一条 | 多轮日志已合并为整局一行，`game_outcome_money` 为整局净输赢 |

> **对本表的意义**：因为 `dws_crazyddz_daily_game` 已是"一局一行"，510K 与经典系（本就一局一行）的粒度一致，UNION ALL 后聚合逻辑与经典系一致。`game_count` 统计的也是"局数"而非"轮数"。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| app_id | int | 应用 ID | 1880053 |
| play_mode | tinyint | 玩法分类：1=经典，2=不洗牌，3=癞子，4=积分，5=比赛，6=好友房，7=510K | 1 |
| uid | int | 玩家唯一标识 | 123456789 |
| dt | date | 对局日期 | 2026-02-10 |
| game_count | int | 当日该玩法对局总数 | 8 |
| total_play_seconds | int | 当日该玩法总游戏时长（秒） | 2400 |
| avg_game_seconds | double | 该玩法平均每局时长（秒） | 180.5 |
| win_count | int | 该玩法胜利局数 | 5 |
| lose_count | int | 该玩法失败局数 | 3 |
| win_rate | double | 该玩法胜率（百分比）= win_count / game_count | 62.50 |
| lose_rate | double | 该玩法负率（百分比）= lose_count / game_count；510K 有平局，与 win_rate 不互补 | 37.50 |
| max_win_streak | int | 该玩法内最大连胜 | 3 |
| max_lose_streak | int | 该玩法内最大连败 | 2 |
| avg_magnification | double | 该玩法平均理论倍数 | 12.5 |
| max_magnification | int | 该玩法最大理论倍数 | 48 |
| avg_real_magnification | double | 该玩法平均实际倍数（ABS） | 10.2 |
| multi_q1_games | int | Q1 局数（倍数的 0-25 分位） | 2 |
| multi_q2_games | int | Q2 局数（倍数的 25-50 分位） | 2 |
| multi_q3_games | int | Q3 局数（倍数的 50-75 分位） | 2 |
| multi_q4_games | int | Q4 局数（倍数的 75-100 分位） | 2 |
| multi_q4_wins | int | Q4 局中胜利数 | 1 |
| multi_q4_losses | int | Q4 局中失败数 | 1 |
| bomb_0_games | int | 0 炸弹局数（仅 play_mode 1,2,3 有值） | 2 |
| bomb_1_games | int | 1 炸弹局数（仅 play_mode 1,2,3 有值） | 3 |
| bomb_2_games | int | 2 炸弹局数（仅 play_mode 1,2,3 有值） | 2 |
| bomb_3plus_games | int | 3 及以上炸弹局数（仅 play_mode 1,2,3 有值） | 1 |
| games_with_grab | int | 抢地主局数（仅 play_mode 1,2,3 有值） | 4 |
| games_player_doubled | int | 玩家加倍局数（仅 play_mode 1,2,3 有值） | 2 |
| start_money | bigint | 该玩法首局前货币数量（见币种说明） | 10000 |
| end_money | bigint | 该玩法末局后货币数量（见币种说明） | 12000 |
| money_peak | bigint | 该玩法货币峰值 | 15000 |
| money_valley | bigint | 该玩法货币谷值 | 8000 |
| total_diff_money | bigint | 该玩法总输赢（含服务费还原） | 2000 |
| total_fee_paid | int | 该玩法总服务费 | 800 |
| escape_count | int | 该玩法逃跑次数 | 0 |
| distinct_rooms | tinyint | 该玩法游玩房间数 | 2 |
| total_settle_rounds | int | 总结算轮数（非 510K 玩法默认 1，510K 多轮累计） | 25 |
| avg_settle_rounds | double | 平均每局轮数（非 510K 玩法默认 1.0，510K 多轮平均） | 5.2 |
| outcome_gdp | bigint | 510K 货币流转绝对值累计（仅 play_mode=7 有值，其他玩法为 0） | 150000 |
| max_settle_round_single | int | 单局最多轮数（非 510K 玩法默认 1） | 12 |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE tcy_temp.dws_app_allgame_stat (
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `play_mode` tinyint(4) NULL COMMENT "游戏玩法",
  `uid` int(11) NOT NULL COMMENT "用户ID",
  `dt` DATE NOT NULL COMMENT "游戏日期",
  -- 参与度
  `game_count` int(11) NULL COMMENT "该玩法对局总数",
  `total_play_seconds` int(11) NULL COMMENT "该玩法总时长（秒）",
  `avg_game_seconds` double NULL COMMENT "该玩法平均每局时长（秒）",
  `distinct_rooms` tinyint(4) NULL COMMENT "该玩法游玩房间数",
  -- 胜负
  `win_count` int(11) NULL COMMENT "该玩法胜利局数",
  `lose_count` int(11) NULL COMMENT "该玩法失败局数",
  `win_rate` double NULL COMMENT "该玩法胜率（%）= win_count/game_count",
  `lose_rate` double NULL COMMENT "该玩法负率（%）= lose_count/game_count，510K有平局故与胜率不互补",
  `max_win_streak` int(11) NULL COMMENT "该玩法最大连胜",
  `max_lose_streak` int(11) NULL COMMENT "该玩法最大连败",
  -- 倍数
  `avg_magnification` double NULL COMMENT "该玩法平均理论倍数",
  `max_magnification` int(11) NULL COMMENT "该玩法最大理论倍数",
  `avg_real_magnification` double NULL COMMENT "该玩法平均实际倍数（ABS）",
  `multi_q1_games` int(11) NULL COMMENT "倍数Q1(0-25分位)局数",
  `multi_q2_games` int(11) NULL COMMENT "倍数Q2(25-50分位)局数",
  `multi_q3_games` int(11) NULL COMMENT "倍数Q3(50-75分位)局数",
  `multi_q4_games` int(11) NULL COMMENT "倍数Q4(75-100分位)局数",
  `multi_q4_wins` int(11) NULL COMMENT "Q4局中胜利数",
  `multi_q4_losses` int(11) NULL COMMENT "Q4局中失败数",
  -- 玩法特异指标（仅经典系有值）
  `bomb_0_games` int(11) NULL COMMENT "0炸弹局数",
  `bomb_1_games` int(11) NULL COMMENT "1炸弹局数",
  `bomb_2_games` int(11) NULL COMMENT "2炸弹局数",
  `bomb_3plus_games` int(11) NULL COMMENT "3及以上炸弹局数",
  `games_with_grab` int(11) NULL COMMENT "抢地主局数",
  `games_player_doubled` int(11) NULL COMMENT "玩家加倍局数",
  -- 金流
  `start_money` bigint(20) NULL COMMENT "该玩法首局前货币",
  `end_money` bigint(20) NULL COMMENT "该玩法末局后货币",
  `money_peak` bigint(20) NULL COMMENT "该玩法货币峰值",
  `money_valley` bigint(20) NULL COMMENT "该玩法货币谷值",
  `total_diff_money` bigint(20) NULL COMMENT "该玩法总输赢",
  `total_fee_paid` int(11) NULL COMMENT "该玩法总服务费",
  -- 行为
  `escape_count` int(11) NULL COMMENT "该玩法逃跑次数",
  -- 510K 专属（其他玩法设默认值：settle_rounds=1, outcome_gdp=0, max_settle_round_single=1）
  `total_settle_rounds` int(11) NULL COMMENT "总结算轮数（非510K默认1）",
  `avg_settle_rounds` double NULL COMMENT "平均每局轮数（非510K默认1.0）",
  `outcome_gdp` bigint(20) NULL COMMENT "510K货币流转绝对值累计",
  `max_settle_round_single` int(11) NULL COMMENT "单局最多轮数（非510K默认1）"
) ENGINE=OLAP
DUPLICATE KEY(`app_id`, `play_mode`, `uid`, `dt`)
COMMENT "APP端用户每日游戏行为统计（全玩法，按play_mode拆分）"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "colocate_with" = "group_daily_data",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-120",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p"
);
```

### 增量数据导入

> **说明**：经典系 / 积分玩法（`dws_ddz_daily_game`，单轮，play_mode 1~6）与 510K（`dws_crazyddz_daily_game`，多轮，play_mode=7）通过 UNION ALL 拼接，各自独立聚合后合并写入。

```sql
-- 批量初始化指定时间段内的数据
-- 参数说明：
--   ${START_DATE}：起始日期（date 格式，如 '2026-06-01'）
--   ${END_DATE}：结束日期（date 格式，如 '2026-06-08'）
INSERT INTO tcy_temp.dws_app_allgame_stat
WITH ddz_modes AS (
    -- 经典/不洗牌/癞子/比赛/积分/好友房（从 dws_ddz_daily_game 统一聚合）
    SELECT
        *,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode ORDER BY game_datetime ASC) AS game_seq,
        ROW_NUMBER() OVER (PARTITION BY uid, play_mode ORDER BY game_datetime DESC) AS rank_desc
    FROM tcy_temp.dws_ddz_daily_game
    WHERE game_id = 53
      AND dt BETWEEN '2026-06-01' AND '2026-06-08'
      AND robot != 1
      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
),
ddz_modes_qt AS (
    SELECT *,
        NTILE(4) OVER (PARTITION BY play_mode ORDER BY magnification) AS multi_quartile
    FROM ddz_modes
),
ddz_streaks AS (
    SELECT
        uid, play_mode, dt,
        MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS max_win_streak,
        MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS max_lose_streak
    FROM (
        SELECT uid, play_mode, dt, result_id, grp, COUNT(*) AS streak_len
        FROM (
            SELECT uid, play_mode, dt, result_id,
                game_seq - ROW_NUMBER() OVER (PARTITION BY uid, play_mode, dt, result_id ORDER BY game_seq) AS grp
            FROM ddz_modes_qt
            WHERE result_id IN (1, 2)
        ) g
        GROUP BY uid, play_mode, dt, result_id, grp
    ) s
    GROUP BY uid, play_mode, dt
),
ddz_agg AS (
    SELECT
        g.app_id, g.play_mode, g.uid, g.dt,
        COUNT(*) AS game_count,
        SUM(g.timecost) AS total_play_seconds,
        ROUND(AVG(g.timecost), 1) AS avg_game_seconds,
        COUNT(DISTINCT g.room_id) AS distinct_rooms,
        COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
        COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
        ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
        ROUND(COUNT(CASE WHEN g.result_id = 2 THEN 1 END) * 100.0 / COUNT(*), 2) AS lose_rate,
        ANY_VALUE(s.max_win_streak),
        ANY_VALUE(s.max_lose_streak),
        ROUND(AVG(g.magnification), 2) AS avg_magnification,
        MAX(g.magnification) AS max_magnification,
        ROUND(AVG(ABS(g.real_magnification)), 2) AS avg_real_magnification,
        COUNT(CASE WHEN g.multi_quartile = 1 THEN 1 END) AS multi_q1_games,
        COUNT(CASE WHEN g.multi_quartile = 2 THEN 1 END) AS multi_q2_games,
        COUNT(CASE WHEN g.multi_quartile = 3 THEN 1 END) AS multi_q3_games,
        COUNT(CASE WHEN g.multi_quartile = 4 THEN 1 END) AS multi_q4_games,
        COUNT(CASE WHEN g.multi_quartile = 4 AND g.result_id = 1 THEN 1 END) AS multi_q4_wins,
        COUNT(CASE WHEN g.multi_quartile = 4 AND g.result_id = 2 THEN 1 END) AS multi_q4_losses,
        COUNT(CASE WHEN g.bomb_bet / 2 = 0 THEN 1 END) AS bomb_0_games,
        COUNT(CASE WHEN g.bomb_bet / 2 = 1 THEN 1 END) AS bomb_1_games,
        COUNT(CASE WHEN g.bomb_bet / 2 = 2 THEN 1 END) AS bomb_2_games,
        COUNT(CASE WHEN g.bomb_bet / 2 >= 3 THEN 1 END) AS bomb_3plus_games,
        COUNT(CASE WHEN g.grab_landlord_bet > 3 THEN 1 END) AS games_with_grab,
        COUNT(CASE WHEN g.magnification_stacked > 1 THEN 1 END) AS games_player_doubled,
        MAX(CASE WHEN g.game_seq = 1 THEN g.start_money END) AS start_money,
        MAX(CASE WHEN g.rank_desc = 1 THEN g.end_money END) AS end_money,
        MAX(g.end_money) AS money_peak,
        MIN(g.end_money) AS money_valley,
        SUM(g.game_outcome_money) AS total_diff_money,
        SUM(g.room_fee) AS total_fee_paid,
        COUNT(CASE WHEN g.cut != 0 THEN 1 END) AS escape_count,
        1 AS total_settle_rounds,
        1.0 AS avg_settle_rounds,
        0 AS outcome_gdp,
        1 AS max_settle_round_single
    FROM ddz_modes_qt g
    LEFT JOIN ddz_streaks s ON g.uid = s.uid AND g.play_mode = s.play_mode AND g.dt = s.dt
    GROUP BY g.app_id, g.play_mode, g.uid, g.dt
),
crazyddz_agg AS (
    -- 510K（从 dws_crazyddz_daily_game 聚合，倍数用 total_magnification）
    SELECT
        g.app_id,
        7 AS play_mode,
        g.uid, g.dt,
        COUNT(*) AS game_count,
        SUM(g.time_cost) AS total_play_seconds,
        ROUND(AVG(g.time_cost), 1) AS avg_game_seconds,
        COUNT(DISTINCT g.room_id) AS distinct_rooms,
        COUNT(CASE WHEN g.result_id = 1 THEN 1 END) AS win_count,
        COUNT(CASE WHEN g.result_id = 2 THEN 1 END) AS lose_count,
        ROUND(COUNT(CASE WHEN g.result_id = 1 THEN 1 END) * 100.0 / COUNT(*), 2) AS win_rate,
        ROUND(COUNT(CASE WHEN g.result_id = 2 THEN 1 END) * 100.0 / COUNT(*), 2) AS lose_rate,
        ANY_VALUE(str.win_streak),
        ANY_VALUE(str.lose_streak),
        ROUND(AVG(g.total_magnification), 2) AS avg_magnification,
        MAX(g.total_magnification) AS max_magnification,
        ROUND(AVG(ABS(g.game_outcome_money) / NULLIF(g.room_base, 0)), 2) AS avg_real_magnification,
        COUNT(CASE WHEN g.multi_quartile = 1 THEN 1 END) AS multi_q1_games,
        COUNT(CASE WHEN g.multi_quartile = 2 THEN 1 END) AS multi_q2_games,
        COUNT(CASE WHEN g.multi_quartile = 3 THEN 1 END) AS multi_q3_games,
        COUNT(CASE WHEN g.multi_quartile = 4 THEN 1 END) AS multi_q4_games,
        COUNT(CASE WHEN g.multi_quartile = 4 AND g.result_id = 1 THEN 1 END) AS multi_q4_wins,
        COUNT(CASE WHEN g.multi_quartile = 4 AND g.result_id = 2 THEN 1 END) AS multi_q4_losses,
        0 AS bomb_0_games,
        0 AS bomb_1_games,
        0 AS bomb_2_games,
        0 AS bomb_3plus_games,
        0 AS games_with_grab,
        0 AS games_player_doubled,
        MAX(CASE WHEN g.seq_asc = 1 THEN g.start_money END) AS start_money,
        MAX(CASE WHEN g.seq_desc = 1 THEN g.end_money END) AS end_money,
        MAX(g.end_money) AS money_peak,
        MIN(g.end_money) AS money_valley,
        SUM(g.game_outcome_money) AS total_diff_money,
        SUM(g.room_fee) AS total_fee_paid,
        COUNT(CASE WHEN g.is_escape != 0 THEN 1 END) AS escape_count,
        SUM(g.settle_count) AS total_settle_rounds,
        ROUND(AVG(g.settle_count), 2) AS avg_settle_rounds,
        SUM(g.game_outcome_gdp) AS outcome_gdp,
        MAX(g.settle_count) AS max_settle_round_single
    FROM (
        SELECT *,
            ROW_NUMBER() OVER (PARTITION BY app_id, uid ORDER BY start_datetime ASC) AS seq_asc,
            ROW_NUMBER() OVER (PARTITION BY app_id, uid ORDER BY start_datetime DESC) AS seq_desc,
            NTILE(4) OVER (PARTITION BY app_id ORDER BY total_magnification) AS multi_quartile
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE game_id = 521
          AND app_id = 1880053
          AND dt BETWEEN '2026-06-01' AND '2026-06-08'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    ) g
    LEFT JOIN (
        SELECT app_id, uid, dt,
            MAX(CASE WHEN result_id = 1 THEN streak_len ELSE 0 END) AS win_streak,
            MAX(CASE WHEN result_id = 2 THEN streak_len ELSE 0 END) AS lose_streak
        FROM (
            SELECT app_id, uid, dt, result_id, grp, COUNT(*) AS streak_len
            FROM (
                SELECT app_id, uid, dt, result_id,
                    seq_asc - ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt, result_id ORDER BY seq_asc) AS grp
                FROM (
                    SELECT app_id, uid, dt, result_id,
                        ROW_NUMBER() OVER (PARTITION BY app_id, uid, dt ORDER BY start_datetime ASC) AS seq_asc
                    FROM tcy_temp.dws_crazyddz_daily_game
                    WHERE game_id = 521
                      AND app_id = 1880053
                      AND dt BETWEEN '2026-06-01' AND '2026-06-08'
                      AND robot != 1
                      AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
                      AND result_id IN (1, 2)
                ) r
            ) g
            GROUP BY app_id, uid, dt, result_id, grp
        ) s
        GROUP BY app_id, uid, dt
    ) str ON g.app_id = str.app_id AND g.uid = str.uid AND g.dt = str.dt
    GROUP BY g.app_id, g.uid, g.dt
)
SELECT * FROM ddz_agg
UNION ALL
SELECT * FROM crazyddz_agg;
```

> **分位分桶说明**：`multi_q1~q4` 基于当天该玩法内所有对局的倍数分布，用 NTILE(4) 计算。Q1=0-25分位，Q2=25-50分位，Q3=50-75分位，Q4=75-100分位。分位阈值每天动态变化，跨天对比时需注意阈值漂移。`avg_magnification` 字段保留绝对倍数，可与分位字段配合使用。

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../ops/daily_data_ops.md)

## 注意事项

1. **币种差异**：经济字段（`start_money` / `end_money` / `total_diff_money` / `money_peak` / `money_valley`）对 play_mode IN (1,2,3,7) 记录银子，对 play_mode IN (4,5,6) 记录积分。**跨币种的金额不可直接比较或加总**，分析经济指标时须按 play_mode 或币种分组
2. **分位分桶**：`multi_q1~q4` 基于当天该玩法内所有对局的倍数分布用 NTILE(4) 动态计算，各玩法独立分桶。玩法间 Q4 的绝对倍数值不同（如经典的 Q4 可能 >24 倍，510K 的 Q4 可能 >100 倍），但含义一致——都是当天该玩法的最高 25% 倍数区间。跨天对比时注意分位阈值漂移，结合 `avg_magnification` 绝对倍数一起看
3. **与姊妹表的分工**：
   - `dws_app_silvergame_stat`（uid × dt）：仅含银子玩法（1,2,3,7），专注金流归因，金流跨玩法可加总
   - `dws_app_scoregame_stat`（uid × dt）：仅含积分玩法（4,5,6），专注参与度与胜负，无金流字段
   - 本表（uid × dt × play_mode）：涵盖所有玩法，专注控制玩法变量的体验分析，不跨玩法加总
4. **行数膨胀**：一天内玩了 N 种玩法的用户产生 N 行，预计约为 silvergame_stat 的 1.2-1.5 倍
5. **平局与 lose_rate**：经典系无平局，`win_rate + lose_rate ≈ 100`；510K 存在平局（`result_id` 非 1/2），平局既不计胜也不计负但计入 `game_count`，因此 `win_rate + lose_rate < 100`（差值即平局率）
6. **连胜连败**：玩法内的对局序列独立计算，不跨玩法
7. **数据完整性**：如用户当日在某玩法下无对局，本表无对应记录
8. **510K 专属字段**：`total_settle_rounds` / `avg_settle_rounds` / `outcome_gdp` / `max_settle_round_single` 仅 play_mode=7 有实际意义。其他玩法设默认值（`total_settle_rounds=1`、`avg_settle_rounds=1.0`、`max_settle_round_single=1`、`outcome_gdp=0`），方便跨玩法 UNION 查询时字段对齐。`outcome_gdp` 为货币流转绝对值累计（非净输赢），反映多轮结算的资金进出总强度
9. **玩法特异字段**：`bomb_0~3plus_games` / `games_with_grab` / `games_player_doubled` 仅 play_mode IN (1,2,3) 有值，其他玩法为 0。炸弹分布比汇总总数更能区分「每局都炸」和「偶尔炸一局」的用户行为模式

## 表数据流向

```text
tcy_temp.dws_ddz_daily_game              （单轮对局明细：经典/不洗牌/癞子/比赛/积分/好友房，play_mode 1~6）
tcy_temp.dws_crazyddz_daily_game         （多轮对局明细：510K，play_mode=7）
            ↓  按 play_mode 聚合（UNION ALL）
tcy_temp.dws_app_allgame_stat            （玩法体验分析：uid × dt × play_mode）  ← 本表
            ↓  同时写入
tcy_temp.dws_app_silvergame_stat         （银子金流+参与度，uid × dt，play_mode 1,2,3,7）
tcy_temp.dws_app_scoregame_stat          （积分参与度+胜负，uid × dt，play_mode 4,5,6）
            ↓  关联分析
tcy_temp.dws_dq_app_daily_reg            （APP 端注册用户宽表）
tcy_temp.dws_dq_daily_login              （每日登录聚合表）
```

## 留存分析示例

### 1. 玩法体验对比：各玩法 Q4（最高 25% 倍数区间）对留存的影响

```sql
-- 用 allgame_stat：分玩法看 Q4 局体验
SELECT
    g.play_mode,
    CASE
        WHEN g.multi_q4_games > 0 THEN '有Q4局'
        ELSE '无Q4局'
    END AS has_q4,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(AVG(g.win_rate), 2) AS avg_win_rate,
    ROUND(AVG(g.total_diff_money), 0) AS avg_outcome
FROM tcy_temp.dws_app_allgame_stat g
WHERE g.app_id = 1880053
  AND g.dt = '2026-06-01'
  AND g.game_count > 0
GROUP BY g.play_mode, has_q4
ORDER BY g.play_mode, has_q4;
```

### 2. 510K 玩家专项：多轮是否更"刺激"→ 更黏还是更伤？

```sql
-- 用 allgame_stat：play_mode=7，按 settle_rounds 分桶
SELECT
    CASE
        WHEN g.total_settle_rounds <= 10 THEN '短(≤10轮)'
        WHEN g.total_settle_rounds <= 25 THEN '中(11-25轮)'
        ELSE '长(>25轮)'
    END AS round_group,
    COUNT(DISTINCT g.uid) AS user_count,
    ROUND(AVG(g.outcome_gdp), 0) AS avg_gdp,
    ROUND(AVG(g.win_rate), 2) AS avg_win_rate
FROM tcy_temp.dws_app_allgame_stat g
WHERE g.app_id = 1880053
  AND g.dt = '2026-06-01'
  AND g.play_mode = 7
  AND g.game_count > 0
GROUP BY round_group
ORDER BY round_group;
```

### 3. 与 silvergame_stat 联用：金流画像 + 玩法体验

```sql
-- silvergame_stat 给整体金流画像，allgame_stat 给玩法体验
SELECT
    g.play_mode,
    CASE
        WHEN s.money_valley < 0 THEN 'A:亏光'
        WHEN s.money_valley < 5000 THEN 'B:底部<5k'
        ELSE 'C:安全'
    END AS bottom_group,
    COUNT(DISTINCT s.uid) AS user_count,
    ROUND(AVG(g.win_rate), 2) AS avg_win_rate,
    ROUND(AVG(g.total_diff_money), 0) AS avg_outcome
FROM tcy_temp.dws_app_silvergame_stat s
JOIN tcy_temp.dws_app_allgame_stat g
  ON s.app_id = g.app_id AND s.uid = g.uid AND s.dt = g.dt
WHERE s.app_id = 1880053
  AND s.dt = '2026-06-01'
  AND g.game_count > 0
GROUP BY g.play_mode, bottom_group
ORDER BY g.play_mode, bottom_group;
```

## 数据校验

### 上游明细层校验（dws_ddz_daily_game + dws_crazyddz_daily_game）

```sql
-- 1. 每局会计等式：game_outcome_money + room_fee 应等于 end_money - start_money
-- 经典系（单轮）
SELECT 'classic' AS source,
       dt,
       COUNT(*) AS total_rows,
       SUM(CASE WHEN ABS((game_outcome_money + room_fee) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) AS mismatch_rows,
       ROUND(SUM(CASE WHEN ABS((game_outcome_money + room_fee) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS mismatch_pct
FROM tcy_temp.dws_ddz_daily_game
WHERE game_id = 53
  AND dt = '2026-06-08'
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY dt
UNION ALL
-- 510K（多轮，已在 dws_crazyddz_daily_game 中汇总为整局一行）
SELECT '510k' AS source,
       dt,
       COUNT(*) AS total_rows,
       SUM(CASE WHEN ABS((game_outcome_money + room_fee) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) AS mismatch_rows,
       ROUND(SUM(CASE WHEN ABS((game_outcome_money + room_fee) - (end_money - start_money)) > 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 2) AS mismatch_pct
FROM tcy_temp.dws_crazyddz_daily_game
WHERE game_id = 521
  AND app_id = 1880053
  AND dt = '2026-06-08'
  AND robot != 1
  AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
GROUP BY dt;
```

### 聚合层校验（dws_app_allgame_stat ↔ 上游明细）

```sql
-- 2. 用户覆盖一致：allgame_stat 去重 uid 数 = 上游明细去重 uid 数
WITH detail_users AS (
    SELECT DISTINCT app_id, uid, dt
    FROM (
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_ddz_daily_game
        WHERE game_id = 53
          AND dt = '2026-06-08'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
        UNION ALL
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE game_id = 521
          AND dt = '2026-06-08'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    ) u
)
SELECT g.dt,
       COUNT(DISTINCT g.uid) AS stat_users,
       COUNT(DISTINCT d.uid) AS detail_users
FROM tcy_temp.dws_app_allgame_stat g
FULL OUTER JOIN detail_users d
  ON g.uid = d.uid AND g.dt = d.dt AND g.app_id = d.app_id
WHERE g.app_id = 1880053
  AND g.dt = '2026-06-08'
GROUP BY g.dt;

-- 3. 总局数一致性：allgame_stat 总局数 = 上游明细总行数
WITH detail_agg AS (
    SELECT app_id, uid, dt, COUNT(*) AS detail_games
    FROM (
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_ddz_daily_game
        WHERE game_id = 53
          AND dt = '2026-06-08'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
        UNION ALL
        SELECT app_id, uid, dt
        FROM tcy_temp.dws_crazyddz_daily_game
        WHERE game_id = 521
          AND dt = '2026-06-08'
          AND robot != 1
          AND group_id IN (6, 66, 8, 88, 33, 44, 77, 99)
    ) u
    GROUP BY app_id, uid, dt
)
SELECT SUM(g.game_count) AS stat_total_games,
       SUM(d.detail_games) AS detail_total_games
FROM tcy_temp.dws_app_allgame_stat g
FULL OUTER JOIN detail_agg d
  ON g.uid = d.uid AND g.dt = d.dt AND g.app_id = d.app_id
WHERE g.app_id = 1880053
  AND COALESCE(g.dt, d.dt) = '2026-06-08';

-- 4. 510K 专属：allgame_stat 中 play_mode=7 的倍数分布
SELECT
    PERCENTILE_APPROX(avg_magnification, 0.25) AS p25,
    PERCENTILE_APPROX(avg_magnification, 0.50) AS p50,
    PERCENTILE_APPROX(avg_magnification, 0.75) AS p75,
    PERCENTILE_APPROX(avg_magnification, 0.90) AS p90
FROM tcy_temp.dws_app_allgame_stat
WHERE app_id = 1880053
  AND play_mode = 7 AND dt BETWEEN '2026-06-01' AND '2026-06-07';
```

## 版本历史

> **文档版本**：v1.0
> **创建时间**：2026-06-11
> **更新说明**：
>
> - v1.0：初始版本