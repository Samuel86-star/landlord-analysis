# 标识符维度速查（app_code / group_id / channel_id / game_id）

> 最易混淆的就是这几个维度。先认清维度归属，再谈分析。

## 一、三个独立维度，别混

| 维度 | 字段 | 含义 | 例子 |
| ---- | ---- | ---- | ---- |
| 应用/包体 | `app_code` | 同一 app 下的客户端包体/引擎版本 | zgda / zgdx / zgde |
| 设备平台 | `group_id`（分端 ID） | iOS / 安卓 / 其他 | 8,88 / 6,66,33,44,77,99 / 其他 |
| 渠道号 | `channel_id` | 投放/分发渠道（另一个维度） | 1001, 2001... |

- **`app_id = 1880053`**（斗地主 app），其下多个 app_code 并存。
- 用户说"zgdx 用户 / zgde 用户"，几乎都指 **`app_code`**，不是 `channel_id`。

## 二、app_code 目录（截至 2026-07）

| app_code | 客户端 | 平台（group_id） | 备注 |
| -------- | ------ | ---------------- | ---- |
| `zgda` | Cocos-Lua | 多平台 | `group_id=1` 为内部测试号（需剔除，见 [data-gotchas.md](data-gotchas.md) §8） |
| `zgdx` | Cocos-Creator | 安卓 + iOS（8,88） | — |
| `zgde` | 新包体 | 56（非 iOS / 非标准安卓，平台待确认） | — |

> app_code 会随包体迭代增减；遇到没见过的 app_code，用下文「平台分桶」SQL 自查 group_id 分布。

## 三、group_id → 平台映射

```sql
CASE
    WHEN group_id IN (8, 88) THEN 'iOS'
    WHEN group_id IN (6, 66, 33, 44, 77, 99) THEN 'Android'
    ELSE CONCAT('other:', group_id)          -- 如 zgde=56、测试号=1
END AS platform
```

- 对局表用 `group_id`；登录表 `dws_dq_daily_login` 用 `most_freq_group_id`；注册表 `dws_dq_app_daily_reg` 用 `reg_group_id`。**同一套枚举值**。

## 四、game_id → 玩法 → DWS 分析表 → raw 源

> 分析一律查 DWS（矫正层），raw 仅 fallback；详见 [game-combat-analysis.md](game-combat-analysis.md) §零。

| game_id | 玩法 | DWS 分析表（优先） | raw 源表（fallback） | 上游源 |
| ------- | ---- | ------------------ | -------------------- | ------ |
| 53 | 三人斗地主（ddz） | `tcy_temp.dws_ddz_daily_game` | `ddz_daily_game_raw` | Hive `fact_game_combatgains` |
| 521 | 疯狂斗地主（crazyddz） | `tcy_temp.dws_crazyddz_daily_game` | `crazyddz_daily_game_raw` | SR 内表 `dwd_game_combatgains_si` |
| 105 | 四人斗地主（srddz） | `tcy_temp.dws_srddz_daily_game` | `srddz_daily_game_raw` | SR 内表，仅 room 927/928/930 |

> 识别口诀：`crazy/疯狂/510K`→521；`sr/四斗/srddz`→105；`ddz/三人`→53。53 上游来自 Hive，521/105 来自 SR 内表，**别查错源**。详见 [CLAUDE.md](../../CLAUDE.md) 游戏 ID 映射表。
