# 源数据说明 (Data Documentation)

## 目录结构

```
landlord-analysis/
├── README-data.md      # 本数据说明文件
├── raw/                # 原始数据目录
├── dws/                # 维度表层目录
├── processed/          # 处理后的数据目录
└── docs/               # 相关文档目录
```

## 数据字段说明

### 全局字段

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
|-------|------|------|--------|---------|
| uid   | bigint | 玩家唯一标识 ID | 123456789 | 是 |
| app_id | bigint | 应用 ID | 1880053 | 是 |
| game_id | int | 游戏 ID（部分表包含） | 1 | 否 |
| group_id | bigint | 平台分组 ID（区分 PC/APP/小游戏） | 6 | 是 |
| channel_id | bigint | 渠道号 | 1001 | 是 |

**应用与游戏映射说明：**
- `app_id = 1880053`：代表斗地主游戏应用
- `game_id`：代表具体的斗地主游戏，作用与 `app_id` 相似，用于标识游戏类型
- 注意：`game_id` 字段仅在部分表中存在

**平台分组说明 (group_id)：**
- `PC 端`：`group_id not in (6,66,8,88,55,69,0,56,68,33,44,77,99)`
- `APP 端`：`group_id in (6,66,33,44,77,99)` 为安卓，`group_id in (8,88)` 为 iOS
- `小游戏`：`group_id = 56`

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

### 原始数据表 (raw/)

| 表名 | 说明文件 | 描述 |
|------|----------|------|
| `olap_tcy_userapp_d_p_login1st` | [olap_tcy_userapp_d_p_login1st.md](raw/olap_tcy_userapp_d_p_login1st.md) | 游戏用户首次注册登录信息表 |
| `dwd_tcy_userlogin_si` | [dwd_tcy_userlogin_si.md](raw/dwd_tcy_userlogin_si.md) | 玩家登录日志信息表 |
| `dwd_game_combat_si` | [dwd_game_combat_si.md](raw/dwd_game_combat_si.md) | 玩家游戏对局战绩日志信息表 |

### 维度表 (dws/)

| 表名 | 说明文件 | 描述 |
|------|----------|------|
| `dws_channel_category_map` | [dws_channel_category_map.md](dws/dws_channel_category_map.md) | 渠道号与渠道分类映射表 |

### 处理后数据表 (processed/)

（待补充）

## 版本历史

| 版本 | 日期 | 修改内容 | 修改人 |
|-----|------|---------|--------|
| v1.0 | 2026-04-07 | 初始版本 | - |
