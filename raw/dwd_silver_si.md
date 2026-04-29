# 玩家银子变动日志表说明

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_dwd` |
| 表名 | `dwd_silver_si` |
| 全名 | `tcy_dwd.dwd_silver_si` |
| 类型 | 原始数据表 |
| 描述 | 玩家银子变动日志表，记录用户银子收支明细 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 | 是否必填 |
| ----- | ---- | ---- | ------ | ------- |
| dt | int | 日期（格式：YYYYMMDD） | 20260429 | 是 |
| uid | bigint | 玩家 ID | 123456789 | 是 |
| game_id | int | 游戏 ID | 53 | 是 |
| game_code | varchar(65533) | 游戏代号 | zgda | 是 |
| app_id | int | 应用 ID | 1880053 | 是 |
| op_id | int | 操作 ID | 1001 | 是 |
| op_name | varchar(65533) | 操作名称 | 对局输赢 | 是 |
| op_type_id | int | 操作类型 ID | 1 | 是 |
| op_type_name | varchar(65533) | 操作类型名称 | 游戏 | 是 |
| silver_diff | int | 对局银两变化（含服务费） | 500 | 是 |
| silver_deposit | int | 银两变化或服务费 | 500 | 是 |
| silver_amount | int | 银两变化（不含服务费） | 400 | 是 |
| silver_balance | bigint | 银子余额 | 10000 | 是 |
| silver_initial | bigint | 银子初始值 | 9500 | 是 |
| group_id | int | 大厅组号 | 6 | 是 |
| channel_id | bigint | 渠道号 | 1001 | 是 |
| source_guid | varchar(65533) | 关联奖池配置 ID | abc123 | 否 |
| date_time | datetime | 时间 | 2026-04-29 10:30:00 | 是 |

## 全局字段说明

以下全局字段在本表中同样适用，详细说明可参考 [README-data.md](../README-data.md)：

| 字段名 | 类型 | 说明 |
| ------ | ---- | ---- |
| uid | bigint | 玩家唯一标识 ID |
| game_id | int | 游戏 ID（53 = 斗地主游戏） |
| app_id | int | 应用 ID（1880053 = 斗地主游戏应用） |
| game_code | varchar(65533) | 游戏代号 |
| group_id | int | 大厅组号（区分 PC/APP/小游戏） |
| channel_id | bigint | 渠道号 |

**分端说明 (group_id)：**

- `PC 端`：`group_id not in (6,66,8,88,55,69,0,56,68,33,44,77,99)`
- `APP 端`：`group_id in (6,66,33,44,77,99)` 为安卓，`group_id in (8,88)` 为 iOS
- `小游戏`：`group_id = 56`

**渠道分类说明：**

- 本表仅记录 `channel_id`（渠道号），不包含渠道分类信息
- 需关联 `tcy_temp.dws_channel_category_map` 表获取渠道分类
- 渠道分类标签：`1`=官方，`2`=渠道，`3`=小游戏

## 字段详解

### 银子变化相关

| 字段 | 说明 |
| ---- | ---- |
| `silver_diff` | 对局银两变化（含服务费），正数表示收入，负数表示支出 |
| `silver_deposit` | 银两变化或服务费，用于区分变化金额和服务费 |
| `silver_amount` | 银两变化（不含服务费），反映实际输赢 |
| `silver_balance` | 银子余额，操作后的当前银子数量 |
| `silver_initial` | 银子初始值，操作前的银子数量 |

### 操作类型相关

| 字段 | 说明 |
| ---- | ---- |
| `op_id` | 操作 ID，标识具体操作 |
| `op_name` | 操作名称，如对局输赢、任务奖励、充值等 |
| `op_type_id` | 操作类型 ID，分类标识 |
| `op_type_name` | 操作类型名称，如游戏、任务、充值等 |

## 使用示例

### 1. 查询某日银子变动明细

```sql
SELECT
    dt,
    uid,
    date_time,
    op_name,
    op_type_name,
    silver_diff,
    silver_balance
FROM tcy_dwd.dwd_silver_si
WHERE dt = 20260429
  AND app_id = 1880053
ORDER BY date_time DESC;
```

### 2. 查询用户银子变动汇总

```sql
SELECT
    uid,
    op_type_name,
    COUNT(*) AS op_count,
    SUM(silver_diff) AS total_diff,
    SUM(silver_amount) AS total_amount
FROM tcy_dwd.dwd_silver_si
WHERE dt BETWEEN 20260420 AND 20260429
  AND app_id = 1880053
GROUP BY uid, op_type_name;
```

### 3. 关联对局数据分析银子变化

```sql
SELECT
    s.uid,
    s.date_time,
    s.silver_diff,
    s.silver_balance,
    g.result_id,
    g.magnification
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.dws_ddz_daily_game g
    ON s.uid = g.uid
    AND DATE(s.date_time) = g.dt
WHERE s.dt = 20260429
  AND s.app_id = 1880053
  AND s.op_type_name = '游戏';
```

## 注意事项

1. 查询时需使用 `app_id` 字段进行过滤（如：`app_id = 1880053`）
2. 查询时间段时使用 `dt` 字段，注意 `dt` 为 int 类型的日期（如：20260429）
3. `silver_diff` 包含服务费，`silver_amount` 不包含服务费，分析实际输赢时应使用 `silver_amount`
4. `silver_balance` 为操作后的余额，`silver_initial` 为操作前的余额
