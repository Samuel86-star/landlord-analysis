# DWD 明细表：全平台对局战绩日志表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_dwd` |
| 表名 | `dwd_game_combatgains_si` |
| 全名 | `tcy_dwd.dwd_game_combatgains_si` |
| 类型 | DWD 层明细表（PRIMARY KEY 主键模型） |
| 描述 | 全平台对局战绩事件日志，覆盖所有游戏（约 200 个 game_id）、所有 app |
| 粒度 | trigger_id + time_unix + uid + dt + game_id + room（主键，事件级） |
| 分区 | `dt`（int，yyyyMMdd） |
| 维护方 | 数仓（本项目**只读**，仅 SELECT，不写入不回填） |
| 下游表 | `tcy_temp.crazyddz_daily_game_raw`、`tcy_temp.srddz_daily_game_raw` |

## 定位与设计背景

本表是数仓 DWD 层的全量原始对局日志，所有游戏、所有 app 的对局战绩事件统一落在此表。单日数千万行，是本项目接触的最大明细表之一。

> ⚠️ **本表只用于当日实时查询**：DWS 为 T-1 回填，只有查"今天"的数据时才直接查本表，且 WHERE 必带 `dt` 和 `app_id`；历史日期取数一律走 DWS（`dws_*_daily_game`）或项目 raw 表，勿用本表跑历史窗口（体量大且非本项目权威源）。

在本项目数据链路中的角色（**别查错源**，详见 [identifier-map](../../docs/knowledge/identifier-map.md)）：

| game_id | 玩法 | 本项目取数源 |
| ------- | ---- | ------------ |
| 53 | 三人斗地主 | Hive `fact_game_combatgains`（→ `tcy_temp.ddz_daily_game_raw`），不走本表（**当日实时查询是唯一例外**，见上方备注） |
| 521 | 疯狂斗地主 | **本表**（→ `tcy_temp.crazyddz_daily_game_raw`） |
| 105 | 四人斗地主 | **本表**（→ `tcy_temp.srddz_daily_game_raw`） |

与 Hive 源 `hive_catalog_cdh5.dwd.fact_game_combatgains` 字段高度同构（`room` 对应下游 raw 表的 `room_id`，`time_unix` 对应 `game_datetime`），但为两张独立摄入的表，53 与 521/105 分别走不同源。

## 字段说明

### 主键与对局标识

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| trigger_id | varchar | 触发 ID（事件唯一标识，30 位十六进制） | 5d8e756104f13b49a66663e1dcc71c |
| time_unix | bigint | 事件时间戳（ms） | 1789565195000 |
| uid | bigint | 用户 ID | 296231092 |
| dt | int | 分区（事件日期 yyyyMMdd） | 20260916 |
| game_id | int | 游戏 ID（53=三人斗地主、105=四人、521=疯狂） | 53 |
| room | int | 房间 ID（下游 raw 表改名为 room_id） | 1404 |
| date | int | 事件日期（yyyyMMdd，与 dt 冗余同步） | 20260916 |
| time | int | 事件时间（HHmmss） | 212635 |
| resultguid | varchar | 局结果 GUID | 53_1404_172_sign36b9a2156aaa98c7 |
| startguid | varchar | 开局 GUID（实测与 resultguid 同值） | 53_1404_172_sign36b9a2156aaa98c7 |
| tableno | int | 桌号 | 172 |
| chairno | int | 椅子号（座位号） | 1 |

### 游戏与应用归属

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| game_code | varchar | 游戏代码 | zgda |
| game_name | varchar | 游戏名称 | 斗地主 |
| small_game_id | int | 小玩法 ID（实测 game_id=53 经典对局为 1053） | 1053 |
| game_type | varchar | 游戏类型（实测 53 取值 1/2，口径未确认） | 1 |
| play_type | varchar | 对局类型（实测 53 取值 0/3，口径未确认） | 3 |
| app_id | int | 应用 ID（1880053=斗地主 app；有 Bitmap 索引） | 1880053 |
| app_code | varchar | 应用代码 | zgda |
| app_name | varchar | 应用名称 | 斗地主 |
| from_app_id | bigint | 来源 App ID（拉起方大厅；有 Bitmap 索引） | 3003 |
| from_app_code | varchar | 来源 App 代码 | tcylagt |
| from_app_name | varchar | 来源 App 名称 | 同城游合集 |
| group_id | int | 分组 ID（分端口径同 `dwd_silver_si`：安卓 6/66/33/44/77/99、iOS 8/88、小游戏 56，其余多为 PC；有 Bitmap 索引） | 66 |

### 货币与结算（银子 deposit 系）

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| basedeposit | bigint | 银子底分 | 80 |
| olddeposit | bigint | 对局前银子 | 1702 |
| end_deposit | bigint | 对局后银子 | 1362 |
| depositdiff | bigint | 银子变动（**含服务费**，负数=支出） | -340 |
| fee | int | 服务费 | 100 |
| cut | int | 抽水 | 0 |
| safebox_deposit | bigint | 保险箱存款 | 0 |
| silver_extra | bigint | 银币额外值（如存在） | NULL |
| silver_balance | bigint | 银币余额（如存在） | NULL |
| stash_deposit_balance | bigint | 仓库存款余额 | 1362 |

### 货币与结算（积分 score 系）

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| basescore | bigint | 积分底分 | 1 |
| oldscore | bigint | 对局前积分 | 0 |
| end_score | bigint | 对局后积分 | 0 |
| scorediff | bigint | 积分变动（含服务费） | 0 |
| score_fee | bigint | 积分服务费 | 0 |
| stash_score_balance | bigint | 仓库积分余额 | 1362 |

### 结果、角色与机器人

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| result_id | int | 结果 ID：1=胜、2=负、3=平；**服务费行为 NULL**（见「落行结构」） | 2 |
| result_name | varchar | 结果名称（胜/负/平） | 负 |
| win | int | 赢标记（1/0，与 result_id 冗余一致） | 0 |
| loss | int | 输标记（1/0） | 1 |
| standoff | int | 平标记（1/0） | 0 |
| role | int | 角色 ID（斗地主类：1=地主(庄家)、2=农民(普通)，与 role_name 实测对应） | 2 |
| role_name | varchar | 角色名称（庄家/普通） | 普通 |
| robot | int | 机器人标记：1=机器人，其他=真人 | 0 |
| robot_name | varchar | 机器人/玩家 | 玩家 |
| robot_provider | int | 机器人提供方 | NULL |
| user_type | int | 用户类型（实测 0，口径未确认） | 0 |
| bankruptcy | int | 破产标记 | 0 |
| breakoff | int | 断线标记 | 0 |

### 倍数

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| magnification | int | 个人理论总倍数（原表 COMMENT：「倍率(含特例53)」） | 3 |
| magnification_stacked | int | 个人加倍：1=不加倍、2=加倍、4=超级加倍 | 1 |
| magnification_subdivision | varchar | 倍数细分 JSON（公共倍数+行为倍数，结构见「JSON 字段结构」） | {"behavior_bet":{...},"public_bet":{...}} |

### 对局过程

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| timecost | int | 对局耗时（秒） | 67 |
| wait_time | bigint | 等待耗时（-1 表示无） | 0 |
| experience | int | 经验值 | 1 |
| bout | int | 局次（实测样本恒为 1，与 extend_content 内历史局数不同，口径未确认） | 1 |
| turn_cnt | bigint | 总轮数 | 3 |
| afk_turn_cnt | bigint | 挂机/托管轮数 | 0 |
| history_win | int | 历史赢（斗地主类玩法未填） | NULL |
| history_loss | int | 历史输（斗地主类玩法未填） | NULL |
| history_standoff | int | 历史平（斗地主类玩法未填） | NULL |
| total_kills | bigint | 总击杀数（对战类游戏用，斗地主未填） | NULL |

### 规则与牌局

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| rules | varchar | 规则原串（实测 53 取值 single/double2；玩法权威口径查 [dq_game_room_config](../config/dq_game_room_config.md)） | single |
| rules_cards_name | varchar | 规则牌型名称 | NULL |
| special_cards | varchar | 特殊牌原串 | NULL |
| special_cards_name | varchar | 特殊牌型名称 | NULL |
| start_cards | varchar | 起手牌原串 | NULL |
| start_cards_name | varchar | 起手牌名称 | NULL |
| deposit_limit | varchar | 押注上限 | NULL |
| room_currency_lower | bigint | 进房最少携带货币 | 1000 |
| room_currency_upper | bigint | 进房最大携带货币 | 4000 |

### 渠道、设备与环境

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| channel_id | int | 渠道 ID | 1000000866 |
| channel_name | varchar | 渠道名称 | 10003_lagt_oppo |
| system | varchar | 系统（实测为 16/32 位十六进制指纹串，口径未确认） | c0bae03848484581 |
| ip | varchar | 客户端 IP | 36.57.15.77 |
| package_type_id | int | 安装包类型 ID | 1000 |
| package_name | varchar | 包名 | com.uc108.mobile.lagt.nearme.gamecenter |
| sub_package_id | varchar | 子包 ID | NULL |
| screen_resolution | varchar | 分辨率 | 1080,2124 |
| os_type_id | int | 系统类型 ID（实测按 os_name 对应：1=安卓、2=iOS） | 1 |
| os_name | varchar | 操作系统名称 | oneplus / iOS 26.6 |
| device_name | varchar | 设备名称 | iPhone15,3 / CET-AL60 |
| language | varchar | 语言 | zh |
| carrier_id | int | 运营商 ID | 300 |
| network_type_id | int | 网络类型 ID | 100 |
| user_agent | varchar | User-Agent | Mozilla/5.0 (Linux; Android 12; ...) |
| app_vers | varchar | 客户端版本 | 14.9.20260724 |
| game_vers | varchar | 游戏版本 | 14.9.20260724 |
| from_app_vers | varchar | 来源版本 | 6.9.43 |

### 归因与追踪

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| promotion_code | varchar | 推广码 | 1000000866 |
| fpid | varchar | 设备 FPID | 10_18a353c4723b219437ada9e464a08803 |
| device_idf | varchar | 设备 IDF | NULL |
| ct_open_id | varchar | 开放平台 ID | t9e7fkpybyqDiHEAtI0DIHib |
| ct_app_id | varchar | CT 应用 ID | cta2993c25613c2276 |
| external_app_id | varchar | 外部 APP ID | NULL |
| click_id | varchar | 广告点击 ID | NULL |
| enter_app_type_id | int | 进入 App 类型 ID | NULL |
| media_sdk | varchar | 媒体 SDK | NULL |
| di_sdk_vers | varchar | DI SDK 版本 | andr.2.8.7 |

### 入仓与运维

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| storage_unix | bigint | 入仓时间戳（ms，实测滞后 time_unix 约 5 分钟） | 1789565533000 |
| collect_points | bigint | 抓分数 | NULL |
| mirrorguid | varchar | 镜像对局 ID | NULL |

## 表规模与 game_id 分布

dt=20260916 实测：约 **200 个 game_id** 有数据，全表单日合计数千万行。与本项目相关的玩法：

| game_id | 单日行数 | 玩法 |
| ------- | -------- | ---- |
| 53 | 125.2 万 | 三人斗地主 |
| 521 | 115.7 万 | 疯狂斗地主 |
| 105 | 104.5 万 | 四人斗地主 |

行数最多的 game_id=323（单日 413 万行），本表**绝非只含斗地主**，查询不带 game_id 会扫全平台数据。

## 落行结构（按玩法）

行粒度是「事件」而非「人×局」，不同玩法的落行行为不同（dt=20260916 实测）：

| game_id | 落行结构 | 实测行数 |
| ------- | -------- | -------- |
| 53 | 一局一人**一行**：fee 与结算同行，depositdiff 已含 fee | 结算行 125.2 万（无服务费行） |
| 105 | 一局一人**两行**：服务费行 + 结算行 | 服务费行 51.1 万 / 结算行 53.4 万 |
| 521 | **多轮结算**：服务费行多于结算行 | 服务费行 62.3 万 / 结算行 53.4 万 |

### 服务费行特征

| 字段 | 服务费行 | 结算行 |
| ---- | -------- | ------ |
| result_id | NULL | 1/2/3 |
| fee | >0 | 0 |
| depositdiff | = −fee（如 −150） | 对局输赢 |
| magnification | 0 | 对局倍数 |
| timecost | 0 | 对局耗时 |

两行余额首尾相接：**服务费行的 end_deposit = 结算行的 olddeposit**（先扣服务费、再结算）。

### 双行合并口径

下游 dws 须将两行合并为 1 行：服务费取自服务费行；`result_id` / `magnification` / `timecost` / 结算 depositdiff 取自结算行；净 depositdiff = 两行相加。**`role` / `robot` 也必须取自结算行**（服务费行是叫地主前的快照，与结算行可能不同）。详见 [srddz_daily_game_raw「双行结构说明」](srddz/srddz_daily_game_raw.md)；521 多轮结算的合并套路见 crazyddz dws 的 `ranked_combat`。

## JSON 字段结构

### magnification_subdivision

结构与 Hive 源一致：`public_bet`（initial_bet / grab_landlord_bet / bomb_bet / complete_victory_bet）+ `behavior_bet`（landlord / farmer1 / farmer2），字段明细见 [ddz_daily_game_raw](ddz/ddz_daily_game_raw.md) 对应章节。

```json
{"behavior_bet":{"farmer1":1,"farmer2":1,"landlord":1},"public_bet":{"bomb_bet":1,"complete_victory_bet":1,"grab_landlord_bet":3,"initial_bet":1}}
```

### extend_content

实测样本均为新格式四分类：`card_info` / `card_power` / `user_attr` / `ai_level`，字段明细见 [ddz_daily_game_raw](ddz/ddz_daily_game_raw.md) 对应章节。与 Hive 源的两点实测差异：

- **牌串格式不同**：本表 `card_info.hand_cards` / `bottom_cards` 为**逗号分隔小写**（如 `"9,9,q,6,3,10,10,4,8,a,k,4,2,j,j,10,9,"`，`10`=十、`sj`=小王、`bj`=大王）；Hive 源文档示例为连写大写。跨源解析时注意适配。
- `user_attr` 实测仅含 `bout`（未见 `mode_bout`）。

```json
{"ai_level":{"callflag":0,"doubleflag":0,"robflag":0,"throwtileflag":0,"type":0},"card_info":{"bottom_cards":"2,sj,k,","card_id":0,"hand_cards":"9,9,q,6,3,10,10,4,8,a,k,4,2,j,j,10,9,","shuffle_type":0},"card_power":{"card_power":-14,"card_power_final":-14,"cost_time":797,"is_pass":true,"shuffle_times":1},"user_attr":{"bout":1243}}
```

> JSON 各分类及子字段可能缺失，查询时用 `get_json_int` / `get_json_string` 并配合 `IFNULL` 处理。

## 使用示例

### 1. 查某日某玩法对局明细（必带 dt + game_id）

```sql
SELECT resultguid, uid, `role`, role_name, result_id, magnification,
       basedeposit, olddeposit, end_deposit, depositdiff, fee, timecost
FROM tcy_dwd.dwd_game_combatgains_si
WHERE dt = 20260916
  AND game_id = 105
  AND app_id = 1880053
  AND robot != 1            -- 仅真人（同局机器人登记在 1880105 下）
LIMIT 100;
```

### 2. 摸底单日 game_id 分布

```sql
SELECT game_id, COUNT(*) AS row_cnt, COUNT(DISTINCT uid) AS uid_cnt
FROM tcy_dwd.dwd_game_combatgains_si
WHERE dt = 20260916
GROUP BY game_id
ORDER BY row_cnt DESC;
```

### 3. 核查各玩法落行结构（服务费行 / 结算行）

```sql
SELECT game_id,
       CASE WHEN result_id IS NULL THEN 'fee_row(result_id NULL)' ELSE 'settle_row' END AS row_type,
       COUNT(*) AS cnt
FROM tcy_dwd.dwd_game_combatgains_si
WHERE dt = 20260916
  AND game_id IN (53, 105, 521)
GROUP BY game_id,
         CASE WHEN result_id IS NULL THEN 'fee_row(result_id NULL)' ELSE 'settle_row' END
ORDER BY game_id, row_type;
```

## 字段使用注意

1. **性能红线**：查询必须同时指定 `dt`（int yyyyMMdd 分区）和 `game_id`，避免全平台扫描。`app_id` / `app_code` / `from_app_id` / `from_app_code` / `group_id` / `game_id` 六列建有 Bitmap 索引。
2. **别查错源**：53（三人斗地主）本项目走 Hive `fact_game_combatgains`，521/105 走本表，详见 [identifier-map](../../docs/knowledge/identifier-map.md)。
3. **货币两套体系**：银子玩法用 deposit 系字段，积分玩法用 score 系字段；`depositdiff` / `scorediff` **含服务费**，净输赢 = `depositdiff + fee`（实测验证：底分 80 × 倍数 3 + 服务费 100 = depositdiff −340）。
4. **落行结构差异**：直接统计局数/人数前先跑「使用示例 3」确认该玩法是否存在服务费行；105/521 的分析须先做双行合并。
5. **保留字列名**：`role`、`date`、`time`、`system` 为保留字/通用词，SQL 中建议加反引号（`` `role` `` 坑见 [game-combat-analysis](../../docs/knowledge/game-combat-analysis.md) §五）。
6. **房间等级/玩法判定**：查 [dq_game_room_config](../config/dq_game_room_config.md)，勿用底分或 play_mode 反推；本表 `game_type` / `play_type` / `small_game_id` / `rules` 口径未确认，仅作参考。
7. **机器人过滤**：`robot = 1` 为机器人，真人分析加 `robot != 1`；105 共服对局中机器人登记在 app_id=1880105 下，同局 app_id 混杂是常态。
8. **跨天对局**：`dt` 为事件落分区日期，同一 resultguid 可能跨 dt；下游管道用 `MIN(dt) OVER (PARTITION BY resultguid)` 归一（见 [srddz](srddz/srddz_daily_game_raw.md) / [crazyddz](crazyddz/crazyddz_daily_game_raw.md) 更新 SQL）。
9. **主键去重**：PRIMARY KEY 模型，同主键重复事件会被覆盖去重，无需自行防重。
10. **共服对局保留整局**：抽取某 app 数据时用 `has_target_app` 窗口标记（触及目标 app_id 即保留整个 resultguid），套路见 srddz / crazyddz 更新 SQL。

## 建表语句

`SHOW CREATE TABLE` 实录（2026-09-17）：

```sql
CREATE TABLE `dwd_game_combatgains_si` (
  `trigger_id` varchar(65533) NOT NULL COMMENT "触发ID",
  `time_unix` bigint(20) NOT NULL COMMENT "事件时间戳(ms)",
  `uid` bigint(20) NOT NULL COMMENT "用户ID",
  `dt` int(11) NOT NULL COMMENT "分区",
  `game_id` int(11) NOT NULL COMMENT "游戏ID",
  `room` int(11) NOT NULL COMMENT "房间ID",
  `date` int(11) NOT NULL COMMENT "事件日期(yyyyMMdd)",
  `time` int(11) NULL COMMENT "事件时间(HHmmss)",
  `resultguid` varchar(65533) NULL COMMENT "局结果GUID",
  `game_code` varchar(65533) NULL COMMENT "游戏代码",
  `game_name` varchar(65533) NULL COMMENT "游戏名称",
  `startguid` varchar(65533) NULL COMMENT "开局GUID",
  `tableno` int(11) NULL COMMENT "桌号",
  `chairno` int(11) NULL COMMENT "椅子号",
  `from_app_id` bigint(20) NULL COMMENT "来源AppID",
  `from_app_code` varchar(65533) NULL COMMENT "来源App代码",
  `basescore` bigint(20) NULL COMMENT "底分",
  `oldscore` bigint(20) NULL COMMENT "旧积分",
  `scorediff` bigint(20) NULL COMMENT "积分差值",
  `basedeposit` bigint(20) NULL COMMENT "基础存款",
  `olddeposit` bigint(20) NULL COMMENT "旧存款",
  `depositdiff` bigint(20) NULL COMMENT "存款差值",
  `experience` int(11) NULL COMMENT "经验值",
  `timecost` int(11) NULL COMMENT "耗时(秒)",
  `bout` int(11) NULL COMMENT "局次",
  `breakoff` int(11) NULL COMMENT "断线标记",
  `win` int(11) NULL COMMENT "赢标记",
  `loss` int(11) NULL COMMENT "输标记",
  `standoff` int(11) NULL COMMENT "平标记",
  `fee` int(11) NULL COMMENT "服务费",
  `cut` int(11) NULL COMMENT "抽水",
  `user_type` int(11) NULL COMMENT "用户类型",
  `channel_id` int(11) NULL COMMENT "渠道ID",
  `channel_name` varchar(65533) NULL COMMENT "渠道名称",
  `group_id` int(11) NULL COMMENT "分组ID",
  `system` varchar(65533) NULL COMMENT "系统",
  `history_win` int(11) NULL COMMENT "历史赢",
  `history_loss` int(11) NULL COMMENT "历史输",
  `history_standoff` int(11) NULL COMMENT "历史平",
  `small_game_id` int(11) NULL COMMENT "小玩法ID",
  `wait_time` bigint(20) NULL COMMENT "等待耗时(-1表示无)",
  `app_code` varchar(65533) NULL COMMENT "应用代码",
  `magnification` int(11) NULL COMMENT "倍率(含特例53)",
  `deposit_limit` varchar(65533) NULL COMMENT "押注上限",
  `robot` int(11) NULL COMMENT "机器人标记",
  `safebox_deposit` bigint(20) NULL COMMENT "保险箱存款",
  `rules` varchar(65533) NULL COMMENT "规则原串",
  `game_type` varchar(65533) NULL COMMENT "游戏类型",
  `special_cards` varchar(65533) NULL COMMENT "特殊牌原串",
  `start_cards` varchar(65533) NULL COMMENT "起手牌原串",
  `role` int(11) NULL COMMENT "角色ID",
  `bankruptcy` int(11) NULL COMMENT "破产标记",
  `end_deposit` bigint(20) NULL COMMENT "结束存款",
  `rules_cards_name` varchar(65533) NULL COMMENT "规则牌型名称",
  `special_cards_name` varchar(65533) NULL COMMENT "特殊牌型名称",
  `start_cards_name` varchar(65533) NULL COMMENT "起手牌名称",
  `score_fee` bigint(20) NULL COMMENT "积分费用",
  `silver_extra` bigint(20) NULL COMMENT "银币额外值(如存在)",
  `silver_balance` bigint(20) NULL COMMENT "银币余额(如存在)",
  `result_name` varchar(65533) NULL COMMENT "结果名称",
  `app_id` int(11) NULL COMMENT "应用ID",
  `app_name` varchar(65533) NULL COMMENT "应用名称",
  `from_app_name` varchar(65533) NULL COMMENT "来源App名称",
  `robot_name` varchar(65533) NULL COMMENT "机器人/玩家",
  `role_name` varchar(65533) NULL COMMENT "角色名称",
  `result_id` int(11) NULL COMMENT "结果ID(1胜2负3平)",
  `stash_score_balance` bigint(20) NULL COMMENT "仓库积分余额",
  `stash_deposit_balance` bigint(20) NULL COMMENT "仓库存款余额",
  `magnification_stacked` int(11) NULL COMMENT "叠加倍率",
  `end_score` bigint(20) NULL COMMENT "结束积分",
  `afk_turn_cnt` bigint(20) NULL COMMENT "挂机局数",
  `turn_cnt` bigint(20) NULL COMMENT "总局数",
  `ip` varchar(65533) NULL COMMENT "IP",
  `package_type_id` int(11) NULL COMMENT "安装包类型ID",
  `from_app_vers` varchar(65533) NULL COMMENT "来源版本",
  `app_vers` varchar(65533) NULL COMMENT "客户端版本",
  `game_vers` varchar(65533) NULL COMMENT "游戏版本",
  `os_type_id` int(11) NULL COMMENT "系统类型ID",
  `promotion_code` varchar(65533) NULL COMMENT "推广码",
  `fpid` varchar(65533) NULL COMMENT "设备FPID",
  `device_idf` varchar(65533) NULL COMMENT "设备IDF",
  `ct_open_id` varchar(65533) NULL COMMENT "开放平台ID",
  `package_name` varchar(65533) NULL COMMENT "包名",
  `ct_app_id` varchar(65533) NULL COMMENT "CT应用ID",
  `screen_resolution` varchar(65533) NULL COMMENT "分辨率",
  `os_name` varchar(65533) NULL COMMENT "操作系统名称",
  `device_name` varchar(65533) NULL COMMENT "设备名称",
  `language` varchar(65533) NULL COMMENT "语言",
  `carrier_id` int(11) NULL COMMENT "运营商ID",
  `external_app_id` varchar(65533) NULL COMMENT "外部APP ID",
  `click_id` varchar(65533) NULL COMMENT "广告点击ID",
  `user_agent` varchar(65533) NULL COMMENT "User-Agent",
  `network_type_id` int(11) NULL COMMENT "网络类型ID",
  `enter_app_type_id` int(11) NULL COMMENT "进入App类型ID",
  `media_sdk` varchar(65533) NULL COMMENT "媒体SDK",
  `sub_package_id` varchar(65533) NULL COMMENT "子包ID",
  `di_sdk_vers` varchar(65533) NULL COMMENT "DI SDK版本",
  `storage_unix` bigint(20) NULL COMMENT "入仓时间戳(ms)",
  `collect_points` bigint(20) NULL COMMENT "抓分数",
  `mirrorguid` varchar(65533) NULL COMMENT "镜像对局ID",
  `play_type` varchar(65533) NULL COMMENT "对局类型",
  `room_currency_lower` bigint(20) NULL COMMENT "房间货币下限",
  `room_currency_upper` bigint(20) NULL COMMENT "房间货币上限",
  `total_kills` bigint(20) NULL COMMENT "总击杀数",
  `magnification_subdivision` varchar(65533) NULL COMMENT "倍数细分",
  `robot_provider` int(11) NULL COMMENT "机器人提供方",
  `extend_content` varchar(65533) NULL COMMENT "扩展信息",
  INDEX i_app_id (`app_id`) USING BITMAP COMMENT '',
  INDEX i_app_code (`app_code`) USING BITMAP COMMENT '',
  INDEX i_from_app_id (`from_app_id`) USING BITMAP COMMENT '',
  INDEX i_from_app_code (`from_app_code`) USING BITMAP COMMENT '',
  INDEX i_group_id (`group_id`) USING BITMAP COMMENT '',
  INDEX i_game_id (`game_id`) USING BITMAP COMMENT ''
) ENGINE=OLAP
PRIMARY KEY(`trigger_id`, `time_unix`, `uid`, `dt`, `game_id`, `room`)
COMMENT "OLAP"
PARTITION BY (`dt`)
DISTRIBUTED BY HASH(`uid`, `game_id`, `room`)
ORDER BY(`time_unix`, `uid`, `game_id`, `package_type_id`, `room`, `resultguid`)
PROPERTIES (
    "bloom_filter_columns" = "uid",
    "compression" = "ZSTD",
    "datacache.enable" = "true",
    "enable_async_write_back" = "false",
    "enable_persistent_index" = "true",
    "persistent_index_type" = "CLOUD_NATIVE",
    "replication_num" = "3",
    "storage_volume" = "builtin_storage_volume"
);
```

## 表数据流向

```text
游戏服务端对局事件
            ↓  数仓摄入（DWD）
tcy_dwd.dwd_game_combatgains_si      （全平台对局日志，约 200 个 game_id）      ← 本表
            ↓  按玩法抽取（game_id=521，has_target_app 保整局）
tcy_temp.crazyddz_daily_game_raw     （疯狂斗地主原始对局表）
            ↓  按玩法抽取（game_id=105，同上）
tcy_temp.srddz_daily_game_raw        （四人斗地主原始对局表）

hive_catalog_cdh5.dwd.fact_game_combatgains   （Hive 同构源，53 走此路，不经本表）
            ↓  按玩法抽取（game_id=53）
tcy_temp.ddz_daily_game_raw          （三人斗地主原始对局表）
```

> **文档版本**：v1.0
> **创建时间**：2026-09-17
> **更新说明**：
>
> - v1.0：初始版本。表结构与示例值来自 `SHOW CREATE TABLE` 及 dt=20260916 抽样实测；落行结构（53 无服务费行、105 一人两行、521 多轮）为当日全量 GROUP BY 实证。
