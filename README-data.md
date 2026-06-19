# 源数据说明 (Data Documentation)

## 目录结构

```text
landlord-analysis/
├── README-data.md      # 本数据说明文件（数据字段、应用/游戏映射）
├── CLAUDE.md           # 项目协作指南（规范 + 目录结构 + DDL 规则等）
├── analysis/           # 数据分析工作区（plan/data/result）
├── starrocks/          # StarRocks 数仓能力（account/game/currency/config/retention）
├── py/                 # 回填脚本集 ★ 日常运维入口
│   ├── daily_backfill.py     # 每日初始化（15 张表）
│   ├── daily_retention.py    # 留存回扫（35 天）
│   ├── batch_insert_*.py     # 18 个单表回填脚本
│   ├── sr_exec.py            # StarRocks 客户端
│   └── README.md             # ← 脚本使用说明，团队从这里入手
└── ops/                # 运维操作手册（daily_data_ops.md / troubleshooting.md）
```

> **首次接触本项目？** 先读 [py/README.md](py/README.md)，了解日常如何用两条命令完成数据初始化。

## 数据字段说明

### 全局字段

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
| ----- | ---- | ---- | ------ | ------- |
| uid | int | 玩家唯一标识 ID | 123456789 | 是 |
| app_id | int | 应用 ID | 1880053 | 是 |
| app_code | varchar(32) | 应用code | zgda（可以跟group_id结合一起区分客户端开发语言） | 否 |
| game_id | int | 游戏 ID（部分表包含） | 1 | 否 |
| group_id | int | 平台分组 ID（区分 PC/APP/小游戏） | 6 | 是 |
| channel_id | int | 渠道号 | 1001 | 是 |

**应用与游戏映射说明：**

- `app_id = 1880053`：代表斗地主游戏应用
- `game_id`：代表具体的斗地主游戏，作用与 `app_id` 相似，用于标识游戏类型
- 注意：`game_id` 字段仅在部分表中存在

**平台分组说明 (group_id)：**

- `PC 端`：`group_id not in (6,66,8,88,55,69,0,56,68,33,44,77,99)`
- `APP 端`：`group_id in (6,66,33,44,77,99)` 为安卓，`group_id in (8,88)` 为 iOS
- `小游戏`：`group_id = 56`

**应用与客户端开发语言说明：**

- `PC端`：`group_id not in (6,66,8,88,55,69,0,56,68,33,44,77,99)`时，客户端开发语言为MFC
- `app端`：`group_id in (6,66,33,44,77,99,8,88)`且`app_code=zgdx`时，客户端开发语言为cocos creator
- `app端`：`group_id in (6,66,33,44,77,99,8,88)`且`app_code=zgda`时，客户端开发语言为cocos lua
- `小游戏`：`group_id = 56`时，客户端开发语言为creator

**渠道分类说明：**

源数据表中仅记录 `channel_id`（渠道号），需通过关联维表获取渠道分类信息：

```sql
-- 获取 channel_id 对应的渠道分类
SELECT
    t1.channel_id,
    t2.channel_category_id,
    t2.channel_category_name,
    t2.channel_category_tag_id
FROM tcy_dim.dim_channel_singletag_dict t1
INNER JOIN hive_catalog_cdh5.dim.dim_channel_category t2
    ON t1.channel_type_id = t2.channel_type_id;
```

**渠道分类标签 (channel_category_tag_id)：**

- `1`：官方
- `2`：渠道
- `3`：小游戏

## 数据表说明

### 原始数据表（上游源表，无独立 md，本文件即为字段说明）

| 表名 | 说明 | 描述 |
| ---- | ---- | ---- |
| `hive_catalog_cdh5.dm.olap_tcy_userapp_d_p_login1st` | Hive 源表 | 游戏用户首次注册登录信息表（dws_dq_daily_reg 的上游）|
| `tcy_dwd.dwd_tcy_userlogin_si` | SR DWD 表 | 玩家登录日志信息表（dws_dq_daily_login 的上游）|
| `hive_catalog_cdh5.dwd.fact_game_combatgains` | Hive 源表 | 玩家游戏对局战绩日志（ddz_daily_game_raw 的上游）|
| `tcy_dwd.dwd_game_combatgains_si` | SR DWD 表 | 全量游戏对局日志（crazyddz_daily_game_raw 的上游）|
| `hive_catalog_cdh5.dwd.fact_gtpl_prop_detail` | Hive 源表 | 道具流水日志（dws_prop_log 的上游）|
| `tcy_dwd.dwd_silver_si` | SR DWD 表 | 玩家银子变动日志（dws_dq_silver_logs 的上游，详见 [dwd_silver_si.md](starrocks/currency/dwd_silver_si.md)）|

### 维度表

| 表名 | 说明文件 | 描述 |
| ---- | -------- | ---- |
| `dws_channel_category_map` | [config/dq_channel_category_map.md](starrocks/config/dq_channel_category_map.md) | 渠道号与渠道分类映射表 |
| `dq_currency_op_config` | [config/dq_currency_op_config.md](starrocks/config/dq_currency_op_config.md) | 货币操作配置（结算类型） |
| `dq_currency_guid_config` | [config/dq_currency_guid_config.md](starrocks/config/dq_currency_guid_config.md) | 货币奖池配置 |
| `dq_fin_flow_scene_dict` | [config/dq_fin_flow_scene_dict.md](starrocks/config/dq_fin_flow_scene_dict.md) | 金流场景字典 |
| `dq_prop_config` | [config/dq_prop_config.md](starrocks/config/dq_prop_config.md) | 道具配置 |

### DWS 中间表

> 各表完整字段定义、建表 SQL、回填方式见 [starrocks/](starrocks/) 下对应 md，及 [ops/daily_data_ops.md](ops/daily_data_ops.md)。

| 表名 | 说明文件 | 描述 |
| ---- | -------- | ---- |
| `dws_dq_daily_reg` | [account/dws_dq_daily_reg.md](starrocks/account/dws_dq_daily_reg.md) | 用户注册信息表 |
| `dws_dq_daily_login` | [account/dws_dq_daily_login.md](starrocks/account/dws_dq_daily_login.md) | 用户每日登录多维度聚合表 |
| `dws_dq_app_daily_reg` | [account/dws_dq_app_daily_reg.md](starrocks/account/dws_dq_app_daily_reg.md) | APP 端每日注册用户宽表 |
| `ddz_daily_game_raw` | [game/ddz/ddz_daily_game_raw.md](starrocks/game/ddz/ddz_daily_game_raw.md) | 对局原始字段表（Hive→SR）|
| `crazyddz_daily_game_raw` | [game/crazyddz/crazyddz_daily_game_raw.md](starrocks/game/crazyddz/crazyddz_daily_game_raw.md) | 疯狂斗地主原始字段表（SR 抽取）|
| `dws_ddz_daily_game` | [game/ddz/dws_ddz_daily_game.md](starrocks/game/ddz/dws_ddz_daily_game.md) | 对局战绩统一字段表 |
| `dws_crazyddz_daily_game` | [game/crazyddz/dws_crazyddz_daily_game.md](starrocks/game/crazyddz/dws_crazyddz_daily_game.md) | 疯狂斗地主对局战绩表 |
| `dws_app_game_active` | [game/dws_app_game_active.md](starrocks/game/dws_app_game_active.md) | APP 端每日游戏活跃用户表 |
| `dws_app_gamemode_active` | [game/dws_app_gamemode_active.md](starrocks/game/dws_app_gamemode_active.md) | APP 端每日游戏活跃用户×玩法表 |
| `dws_app_silvergame_stat` | [game/dws_app_silvergame_stat.md](starrocks/game/dws_app_silvergame_stat.md) | 银子玩法每日统计（金流+参与度）|
| `dws_app_scoregame_stat` | [game/dws_app_scoregame_stat.md](starrocks/game/dws_app_scoregame_stat.md) | 积分玩法每日统计（参与度+胜负）|
| `dws_app_allgame_stat` | [game/dws_app_allgame_stat.md](starrocks/game/dws_app_allgame_stat.md) | 全玩法体验统计（按玩法拆分）|
| `dws_app_daily_allgame_stat` | [game/dws_app_daily_allgame_stat.md](starrocks/game/dws_app_daily_allgame_stat.md) | 全玩法日聚合（uid×dt 降维）|
| `dws_ddz_firstday_game` | [game/ddz/dws_ddz_firstday_game.md](starrocks/game/ddz/dws_ddz_firstday_game.md) | 首日对局明细表 |
| `dws_dq_silver_logs` | [currency/dws_dq_silver_logs.md](starrocks/currency/dws_dq_silver_logs.md) | 斗地主银子变动日志表 |
| `dws_prop_log` | [currency/dws_prop_log.md](starrocks/currency/dws_prop_log.md) | 游戏道具流水日志表 |
| `dws_app_firstday_game_stat` | [retention/dws_app_firstday_game_stat.md](starrocks/retention/dws_app_firstday_game_stat.md) | 首日游戏指标宽表 |
| `dws_app_retention_flag` | [retention/dws_app_retention_flag.md](starrocks/retention/dws_app_retention_flag.md) | 留存 flag 表（d1/d3/d7/d14/d30）|

> 历史遗留：旧文档里的 `dws_ddz_app_game_stat` 已改名为 `dws_app_allgame_stat`（详见各表 md 版本说明）。

---
