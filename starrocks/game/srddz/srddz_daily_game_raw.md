# ODS 迁移表：四人斗地主每日对局战绩原始字段表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `srddz_daily_game_raw` |
| 全名 | `tcy_temp.srddz_daily_game_raw` |
| 类型 | ODS 层迁移表（每日增量） |
| 描述 | 将 StarRocks 全量原始对局日志中内嵌于斗地主 app（app_id = 1880053）的四人斗地主（game_id = 105，银子玩法）抽取到 StarRocks，保持原始字段不变 |
| 粒度 | resultguid + uid + 事件（服务费/结算）：同一玩家同一对局通常 2 行（一行服务费、一行结算，详见「双行结构说明」） |
| 上游表 | `tcy_dwd.dwd_game_combatgains_si` |
| 下游表 | `tcy_temp.dws_srddz_daily_game`（待建） |

## 设计背景

本表覆盖 **内嵌于斗地主 app（app_id = 1880053）的四人斗地主玩法**（game_id = 105），为 **4 人玩法（1 地主 + 3 农民）**，属 **银子玩法**，与疯狂斗地主（game_id = 521，同为 1880053 内嵌银子玩法）同构：

| 货币类型 | 底分字段 | 服务费字段 | 对局前货币 | 对局后货币 | 货币变动值(含服务费) |
| ------- | ------- | --------- | --------- | --------- | ------- |
| 银子 | `basedeposit` | `fee` | `olddeposit` | `end_deposit` | `depositdiff` |

迁移过程中保持原始字段不变，未做统一转换（统一转换由下游 `dws_srddz_daily_game` 完成）。

### app_id 与 game_id 业务关系

本表覆盖内嵌于斗地主 app（app_id = 1880053）的四人斗地主银子玩法。对局中真人属 1880053，**陪玩机器人登记在四人斗地主 app（app_id = 1880105）下、按银子玩法结算**——故同局玩家 app_id 混有 1880053（真人）与 1880105（银子机器人），并非经济混用（全部以 `depositdiff` 结算）。用 `has_target_app`（触及 1880053 即保留整局）保证对局完整。

三者严格 1:1 对应（实测 dt=2026-07-02）：

| app_id | 身份 | robot | group_id | 结算行 / unique |
| ------ | ---- | ----- | -------- | --------------- |
| 1880053 | 真人 | 0 | 77 | 99 行 / 28 人 |
| 1880105 | 银子机器人 | 1 | 1 | 269 行 / 30 bot |

> 同桌配比不定：机器人(1880105)在真人不足时补位，一桌真人(1880053)数量可变——07-02 实测 85 局为 1 真人 + 3 机器人、7 局为 2 真人 + 2 机器人（平均 99 真人 / 92 局 ≈ 1.08；随真人流量升高，多真人桌占比会上升）。真人分析用 `robot != 1`（等价于 app_id=1880053 或 group_id=77）。

## 双行结构说明

同一玩家同一对局（resultguid + uid）在 raw 层落 **2 行**，分别记录「服务费」与「结算」两件事（实测 dt=2026-07-02：744 行 / 372 个 player-game，全部为 2 行）：

| 行类型 | result_id | fee | depositdiff | magnification | timecost | 含义 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 服务费行 | NULL | >0（如 150） | = −fee（如 −150） | 0 | 0 | 扣服务费：olddeposit − fee = end_deposit |
| 结算行 | 1/2/3 | 0 | 对局输赢（如 +3600） | 对局倍数 | 对局耗时 | 对局结算：result_id 有值 |

两行余额首尾相接——**服务费行的 `end_deposit` = 结算行的 `olddeposit`**（先扣服务费、再结算）。净盈亏 = 结算行 depositdiff + 服务费行 depositdiff。

> **dws 层合并要求**：下游 `dws_srddz_daily_game` 须将两行合并为 1 行——服务费取自服务费行、`result_id` / `magnification` / `timecost` / 结算 depositdiff 取自结算行、净 depositdiff = 两行相加。**`role` / `robot` 也必须取自结算行**（服务费行是叫地主前的快照，与结算行可能不同，实测 9% player-game 的 role 不一致；若用 MAX 会把地主数算成 60 而非实际的 93）。与 crazyddz dws 处理「多轮结算日志」同类，可对齐其 `ranked_combat` 合并套路。

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_id | int | 游戏 ID | 105 |
| dt | date | 对局日期（跨天对局已归一到 resultguid 首现日） | 2026-07-02 |
| uid | int | 玩家 ID | 299381109 |
| game_datetime | datetime | 对局时间 | 2026-07-02 10:30:00 |
| resultguid | varchar(64) | 本局战绩 ID | "abc123xyz" |
| timecost | int | 对局耗时（秒） | 180 |
| room_id | int | 房间号（源字段 `room`） | 32735 |
| room_currency_lower | bigint | 进入房间所需最少携带银子（随 basedeposit 档：100→1000、250→12000、500→50000） | 1000 |
| room_currency_upper | bigint | 进入房间最大携带银子（100→15000、250→60000、500→20 亿≈无上限） | 15000 |
| robot | tinyint | 机器人标记：1=机器人，其他=真人（实测约 73% 为机器人，真人分析须过滤 robot!=1） | 0 |
| role | tinyint | 角色：1=地主，2=农民（4 人桌为 1 地主 + 3 农民） | 1 |
| chairno | tinyint | 座位号（0/1/2/3，4 人桌；含机器人时四座均匀，座位规则为真人坐 0/3、机器人填 1/2） | 0 |
| result_id | tinyint | 结果：1=获胜，2=失败，3=平局 | 1 |
| basedeposit | int | 银子底分（实测三档：100 占 86%、250 占 5%、500 占 10%） | 100 |
| olddeposit | bigint | 对局前银子数量 | 5500 |
| end_deposit | bigint | 对局后银子数量 | 500 |
| fee | int | 银子服务费（随 basedeposit 定档：100→150、250→500、500→1200） | 150 |
| depositdiff | bigint | 银子变动数量（含服务费） | 5000 |
| cut | int | 逃跑罚没货币（!=0 代表存在逃跑行为） | 0 |
| safebox_deposit | int | 保险箱存银 | 1000 |
| magnification | int | stacked × 对局过程炸弹倍率（4炸1/5炸2/6炸3/7炸4/8炸5/王炸10，累积相乘）。服务费行为 0 | 36 |
| magnification_stacked | int | 角色(农2/地6) × 叫分(1/2/3) × 底牌炸弹倍率（无炸1/4炸2/5+炸3）。服务费行为 1 | 12 |
| channel_id | int | 渠道号 | 1001 |
| group_id | int | 分端 ID（实测：1=机器人、77=真人；1 不在现有聚合层白名单） | 77 |
| app_id | int | 应用 ID | 1880053 |
| app_code | varchar(32) | 应用 code | zgda |
| afk_turn_cnt | int | 托管出牌次数 | 0 |

> **与经典（53）/ 疯狂（521）的差异**：
>
> - **4 人桌**：`chairno` 0/1/2/3（经典/疯狂为 3 人 0/1/2），`role` 为 1 地主 + 3 农民；含机器人时四座均匀，座位规则为真人坐 0/3、机器人填 1/2。
> - **机器人占比高**：实测约 73% 座位为机器人(1880105)；机器人用于真人不足时补位，**同桌真人数量可变**（07-02：85 局 1 真人 + 3 机器人、7 局 2 真人 + 2 机器人，无 3+ 真人桌）。真人分析须 `robot != 1`，过滤后样本很小。
> - **无 `extend_content`**：105 的 `extend_content` 100% 为空，手牌 / 牌力分析不可行（与疯狂 521 同样无此字段）。
> - **倍数**：`magnification_stacked` = 角色(农2/地6) × 叫分(1~3) × 底牌炸弹倍率（无炸1 / 4炸2 / 5+炸3）；`magnification` = stacked × 对局过程炸弹倍率（4炸1 / 5炸2 / 6炸3 / 7炸4 / 8炸5 / 王炸10，累积相乘）。服务费行 stacked=1、magnification=0。

## 构建 SQL

```sql
CREATE TABLE tcy_temp.srddz_daily_game_raw (
  `game_id` int(11) NULL COMMENT "游戏ID",
  `dt` DATE NOT NULL COMMENT "对局日期",
  `uid` int(11) NOT NULL COMMENT "玩家ID",
  `game_datetime` datetime NOT NULL COMMENT "对局时间",
  `resultguid` varchar(64) NULL COMMENT "本局战绩ID",
  `timecost` int(11) NULL COMMENT "对局耗时（秒）",
  `room_id` int(11) NULL COMMENT "房间号",
  `room_currency_lower` bigint(20) NULL COMMENT "进入房间最少携带银子（随basedeposit档）",
  `room_currency_upper` bigint(20) NULL COMMENT "进入房间最大携带银子（高底分档upper=20亿≈无上限）",
  `robot` tinyint(4) NULL COMMENT "机器人标记：1=机器人，其他=真人",
  `role` tinyint(4) NULL COMMENT "角色：1=地主，2=农民（4人桌：1地主+3农民）",
  `chairno` tinyint(4) NULL COMMENT "座位号（0/1/2/3，4人桌）",
  `result_id` tinyint(4) NULL COMMENT "结果：1=获胜，2=失败，3=平局",
  `basedeposit` int(11) NULL COMMENT "银子底分（实测100/250/500三档）",
  `olddeposit` bigint(20) NULL COMMENT "对局前银子数量",
  `end_deposit` bigint(20) NULL COMMENT "对局后银子数量",
  `fee` int(11) NULL COMMENT "银子服务费",
  `depositdiff` bigint(20) NULL COMMENT "银子变动数量（含服务费）",
  `cut` int(11) NULL COMMENT "逃跑罚没货币（!=0代表存在逃跑行为）",
  `safebox_deposit` int(11) NULL COMMENT "保险箱存银",
  `magnification` int(11) NULL COMMENT "stacked×对局炸弹倍率(4炸1/5炸2/6炸3/7炸4/8炸5/王炸10)；服费行0",
  `magnification_stacked` int(11) NULL COMMENT "角色(农2/地6)×叫分(1~3)×底牌炸弹(无1/4炸2/5+炸3)；服费行1",
  `channel_id` int(11) NULL COMMENT "渠道号",
  `group_id` int(11) NULL COMMENT "分端ID（实测1=机器人/77=真人）",
  `app_id` int(11) NOT NULL COMMENT "应用ID",
  `app_code` varchar(32) NULL COMMENT "应用code",
  `afk_turn_cnt` int(11) NULL COMMENT "托管出牌次数"
) ENGINE=OLAP
DUPLICATE KEY(`game_id`, `dt`, `uid`)
COMMENT "四人斗地主每日游戏明细表"
PARTITION BY RANGE(`dt`) (
    START ("2026-01-01") END ("2027-01-01") EVERY (INTERVAL 1 DAY)
)
DISTRIBUTED BY HASH(`uid`) BUCKETS 8
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4",
    "dynamic_partition.enable" = "true",
    "dynamic_partition.time_unit" = "DAY",
    "dynamic_partition.start" = "-120",
    "dynamic_partition.end" = "3",
    "dynamic_partition.prefix" = "p",
    "colocate_with" = "group_daily_data"
);
```

> **DDL 红线**：本项目禁止通过脚本执行 DDL，建表须由用户在 CloudBeaver 网页端手动执行（见根目录 CLAUDE.md）。

## 更新 SQL

按天 `DELETE + INSERT`（幂等可重跑），脚本：`batch_insert_srddz_daily_game_raw.py`（待新建，对照 `batch_insert_crazyddz_daily_game_raw.py`）。

> **依赖**：`tcy_dwd.dwd_game_combatgains_si` 对应日期需先就绪。

```powershell
# 单天
py -3 -u .\batch_insert_srddz_daily_game_raw.py --start 20260702 --end 20260702

# 区间回填
py -3 -u .\batch_insert_srddz_daily_game_raw.py --start 2026-06-01 --end 2026-06-08

# 先看 SQL 不实际执行
py -3 -u .\batch_insert_srddz_daily_game_raw.py --start 20260702 --end 20260702 --dry-run
```

四人斗地主同样存在跨天对局（同一 resultguid 的玩家记录可能分布在 T 日和 T+1 日），沿用 `MIN(dt) OVER (PARTITION BY resultguid)` 归一到首现日。下游读取直接 `WHERE dt = T` 即可拿到完整对局。

```sql
-- 批量初始化（以 START_DATE = 20260601、END_DATE = 20260610 为例）
-- dt 扫描范围扩展到 END_DATE + 1，确保最后一天跨天对局不丢记录
INSERT INTO tcy_temp.srddz_daily_game_raw
WITH base_data AS (
    SELECT 
        game_id, uid, time_unix, resultguid, timecost,
        room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
        basedeposit, olddeposit, end_deposit, fee, depositdiff,
        cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, app_id, app_code, afk_turn_cnt,
        MIN(dt) OVER (PARTITION BY resultguid) AS min_dt,
        MAX(CASE WHEN app_id = 1880053 THEN 1 ELSE 0 END) OVER (PARTITION BY resultguid) AS has_target_app
    FROM tcy_dwd.dwd_game_combatgains_si
    WHERE game_id = 105
      AND dt BETWEEN 20260601 AND 20260611
)
SELECT 
    game_id, min_dt AS dt, uid, FROM_UNIXTIME(time_unix / 1000) AS game_datetime, resultguid, timecost,
    room, room_currency_lower, room_currency_upper, robot, role, chairno, result_id,
    basedeposit, olddeposit, end_deposit, fee, depositdiff,
    cut, safebox_deposit, magnification, magnification_stacked, channel_id, group_id, IFNULL(app_id, 1880053) AS app_id, app_code, afk_turn_cnt
FROM base_data
WHERE has_target_app = 1
  AND min_dt BETWEEN 20260601 AND 20260610;
```

> **与 crazyddz 同构的 `has_target_app`**：105 存在共服对局（约 4.6% 的 resultguid 跨 2 个 app），用 `has_target_app`（触及 app_id = 1880053 即保留整局）保证对局完整，与 crazyddz raw 一致。

> **性能注意**：上游表 `tcy_dwd.dwd_game_combatgains_si` 为全量游戏日志，查询时必须同时指定 `game_id = 105` 和 `dt` 范围（int YYYYMMDD 分区），避免全表扫描。

### 跨天对局验证

```sql
-- 查 dt（min_dt 归属日期）与 game_datetime 不在同一天的记录
SELECT
    resultguid,
    uid,
    dt,
    DATE(game_datetime) AS actual_date,
    game_datetime
FROM tcy_temp.srddz_daily_game_raw
WHERE game_id = 105
  AND dt BETWEEN '2026-06-01' AND '2026-06-10'
  AND dt != DATE(game_datetime)
ORDER BY dt, resultguid;
```

> **增量更新操作手册**：详见 [ops/daily_data_ops.md](../../../ops/daily_data_ops.md)

## 表数据流向

```text
tcy_dwd.dwd_game_combatgains_si        （StarRocks 全量游戏原始对局日志，game_id=105 且触及 app_id=1880053）
            ↓  迁移至 StarRocks（按天 DELETE + INSERT，跨天对局归一）
tcy_temp.srddz_daily_game_raw          （四人斗地主原始对局表，银子玩法，保持原始字段）  ← 本表
            ↓  字段统一转换 / 衍生（倍数分桶等）
tcy_temp.dws_srddz_daily_game          （四人斗地主对局明细表，待建）
```

> **文档版本**：v0.12（草案，基于探查 + raw 实数据，倍数/叫分经产品确认）
> **创建时间**：2026-07-03
> **更新说明**：
>
> - v0.12：订正 magnification/magnification_stacked——前者=叫分×炸弹理论总倍数（服费行0），后者=叫分倍数（1/2/3，服费行1）；农体现×2、地体现×6
> - v0.9：双行结构补注——`role`/`robot` 在服务费行与结算行可能不同（服务费行是叫地主前快照），dws 须取自结算行
> - v0.8：订正同桌配比——并非固定「1 真人 + 3 机器人」，机器人仅补位；07-02 实测 85 局 1 真人+3 机器人、7 局 2 真人+2 机器人
> - v0.7：补 room_currency_lower/upper 取值（随 basedeposit 档：100→1000~15000、250→12000~60000、500→50000~20亿）；raw 字段盘完
> - v0.6：订正 app_id 结构——1880105 行是登记在该 app 下的「银子陪玩机器人」（非积分混用），app_id↔robot↔group_id 严格 1:1（1880053/0/77=真人，1880105/1/1=bot）；group_id 加注
> - v0.5：落 raw 字段实测——破译 chairno 座位规则（真人 0/3、机器人 1/2，含机器人均匀）与 magnification_stacked（加倍基础步长，mag 为其整数倍）；补 fee 随 basedeposit 定档、机器人占比 73%、timecost 均值~186s
> - v0.4：补充「双行结构说明」——同玩家同局 2 行（服务费行 + 结算行，余额相接），修正「粒度」描述，明确 dws 层合并要求
> - v0.3：表体收窄到 1880053 银子口径——去掉积分字段、恢复 `has_target_app`(1880053)、INSERT/字段对齐 crazyddz 模板；订正银子版倍数（6 倍 ladder，max~92K，非积分版的 6.3e7）
> - v0.2：订正经济模型（按 app 分积分/银子两种）
> - v0.1：首版，对照 `crazyddz_daily_game_raw.md` 模板
