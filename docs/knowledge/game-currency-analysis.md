# 游戏银子金流分析口径

> 用于“净消耗、服务费、运营支出、充值、三端对账”等任务。默认采用公司后台口径；需要评估真实经济成本时，单独说明礼券兑换差异。

## 一、数据源与核心公式

- 数据源：`tcy_temp.dws_dq_silver_logs`。
- 必带条件：`app_id = 1880053` 和目标 `dt` 分区。
- 该表已经限定 `game_id = 53` 的斗地主银子流水，不包含积分玩法。
- 净消耗 = 赚的银子 - 赠送的银子。
- 充值属于 `settlement_type = 2`，单独统计，不进入净消耗。

赚的银子：

- `settlement_type = 1 AND op_id = 300501`：取 `silver_deposit`，即服务费。
- 其他 `settlement_type = 1`：取 `-silver_amount`。

赠送的银子：

- 基础口径取 `settlement_type = 0` 的 `silver_amount`。
- 后台对账口径中，APP 排除 `op_id = 590602`（礼券换入银子）。
- 真实经济成本口径中，礼券换入银子仍是运营支出，应计入成本。输出时必须注明采用哪种口径。

## 二、平台划分与机器人过滤

| 平台 | `group_id` |
| ---- | ---------- |
| APP | `6, 66, 33, 44, 77, 99, 8, 88` |
| 小游戏 | `56` |
| PC | 不在 APP、小游戏及排除组 `55, 69, 0, 68` 中 |

后台 PC 口径排除以下组合：

```sql
COALESCE(group_id, -1) = 1
AND COALESCE(app_code, '') = 'zgda'
AND COALESCE(channel_id, -1) = 0
```

该组合不是普通 PC 流量。2026-09-06 对账验证：808 个 UID、19,744 条服务费流水在经典对局表中全部为 `robot = 1`，服务费合计 19,095,040。它是机器人服务费，不属于公司从真实玩家获得的银子。

不要把所有 `group_id = 1 AND app_code = 'zgda'` 都删除：其他 `channel_id` 中存在正常 PC 流量。

## 三、后台对账标准 SQL

执行前仅替换日期。

```sql
WITH platform_flows AS (
    SELECT
        CASE
            WHEN group_id IN (6, 66, 33, 44, 77, 99, 8, 88) THEN 'APP'
            WHEN group_id = 56 THEN '小游戏'
            WHEN group_id NOT IN (6, 66, 33, 44, 77, 99, 8, 88, 56, 55, 69, 0, 68) THEN 'PC'
            ELSE '其他排除端'
        END AS platform,
        settlement_type,
        op_id,
        silver_amount,
        silver_deposit
    FROM tcy_temp.dws_dq_silver_logs
    WHERE app_id = 1880053
      AND dt = '2026-09-06'
      AND NOT (
          COALESCE(group_id, -1) = 1
          AND COALESCE(app_code, '') = 'zgda'
          AND COALESCE(channel_id, -1) = 0
      )
),
platform_summary AS (
    SELECT
        platform,
        SUM(
            CASE
                WHEN settlement_type = 1 AND op_id = 300501 THEN silver_deposit
                WHEN settlement_type = 1 THEN -silver_amount
                ELSE 0
            END
        ) AS earned_silver,
        SUM(
            CASE
                WHEN settlement_type = 0
                 AND NOT (platform = 'APP' AND COALESCE(op_id, -1) = 590602)
                THEN silver_amount
                ELSE 0
            END
        ) AS gifted_silver,
        SUM(CASE WHEN settlement_type = 1 AND op_id = 300501 THEN silver_deposit ELSE 0 END) AS service_fee,
        SUM(CASE WHEN settlement_type = 2 THEN silver_amount ELSE 0 END) AS recharge_silver
    FROM platform_flows
    GROUP BY platform
)
SELECT
    platform,
    earned_silver,
    gifted_silver,
    earned_silver - gifted_silver AS net_consumption,
    service_fee,
    recharge_silver
FROM platform_summary
WHERE platform IN ('PC', 'APP', '小游戏')
ORDER BY CASE platform WHEN 'PC' THEN 1 WHEN 'APP' THEN 2 ELSE 3 END;
```

真实经济成本口径只需删除赠送银子 CASE 中对 APP `op_id = 590602` 的排除条件。

## 四、已验证基线

| 日期 | PC 净消耗 | APP 净消耗 | 小游戏净消耗 |
| ---- | ---------: | ----------: | ---------------: |
| 2026-09-05 | 44,624,906 | 43,662,212 | -212,737 |
| 2026-09-06 | 42,662,356 | 39,911,508 | -264,086 |

以上均与公司后台一致。若相同日期复算不一致，先检查 DWS 分区是否被重新回填，再按下列顺序排查。

## 五、对账排查顺序

1. 确认只查 `tcy_temp.dws_dq_silver_logs`、`app_id = 1880053` 和单日分区。
2. 确认服务费使用 `silver_deposit`，不要用 `silver_amount` 或 `silver_diff`。
3. 确认 APP 后台口径排除了 `op_id = 590602`。
4. 确认 PC 只排除 `group_id = 1 / zgda / channel_id = 0`，没有扩大到整个 `group_id = 1`。
5. 按 `platform × settlement_type × op_id` 聚合差额，优先找能与后台差值精确相等的操作项。
6. 所有排除条件使用 `COALESCE` 保证 NULL 安全；`NOT (字段 = 值)` 会误过滤字段为 NULL 的正常流水。

## 六、常见误区

- `settlement_type` 是公司经营分类，不是玩家账户正负号；不要直接对 `silver_amount` 全表求和。
- 服务费流水同时包含输赢与服务费字段，净消耗中的服务费必须取 `silver_deposit`。
- `settlement_type = 4` 是机器人专用金流，但机器人对局产生的服务费仍可能记为 `settlement_type = 1`，所以还要结合身份属性过滤。
- “与后台一致”和“真实经济成本”不是永远相同；礼券兑换就是已知差异项。
