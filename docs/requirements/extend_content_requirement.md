# 需求：对局战绩 JSON 字段结构说明

## 背景

斗地主对局战绩表 `hive_catalog_cdh5.dwd.fact_game_combatgains` 中，存在两个 JSON 扩展字段：

| 字段名 | 类型 | 说明 |
| ---- | ---- | ---- |
| `magnification_subdivision` | varchar(512) | 倍数细分（公共倍数 + 行为倍数） |
| `extend_content` | varchar(512) | 扩展信息（牌信息 + 牌力值 + 用户属性 + AI 等级） |

本文档明确这两个 JSON 字段的结构规范，作为数据生产/消费双方的对齐依据。

---

## 一、magnification_subdivision（倍数细分）

`magnification_subdivision` 为 JSON 字段，包含**公共倍数**（`public_bet`）和**行为倍数**（`behavior_bet`）。

### public_bet（公共倍数）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.public_bet.initial_bet` | tinyint | 初始倍数 |
| `$.public_bet.grab_landlord_bet` | tinyint | 抢地主倍数：3=无人抢，6=1人抢，12=2人抢，24=3人抢 |
| `$.public_bet.bomb_bet` | int | 炸弹个数：1=无炸弹，否则 bomb_bet/2 为炸弹个数（打出炸弹数，非持有炸弹数） |
| `$.public_bet.complete_victory_bet` | tinyint | 春天/反春标记：1=无，2=春天或反春 |

### behavior_bet（行为倍数）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.behavior_bet.landlord` | tinyint | 地主加倍：1=不加倍，2=加倍，4=超级加倍 |
| `$.behavior_bet.farmer1` | tinyint | 农民1加倍：1=不加倍，2=加倍，4=超级加倍 |
| `$.behavior_bet.farmer2` | tinyint | 农民2加倍：1=不加倍，2=加倍，4=超级加倍 |

### 示例值

```json
{
  "public_bet": {
    "initial_bet": 1,
    "grab_landlord_bet": 6,
    "bomb_bet": 2,
    "complete_victory_bet": 1
  },
  "behavior_bet": {
    "landlord": 2,
    "farmer1": 1,
    "farmer2": 4
  }
}
```

### 倍数计算逻辑

- **公共倍数** = `initial_bet × grab_landlord_bet × 2^(bomb_bet/2) × complete_victory_bet`
  - `bomb_bet = 1` 时无炸弹（`2^0 = 1`），否则炸弹个数为 `bomb_bet / 2`
- **个人倍数**：
  - **农民** = 公共倍数 × 地主加倍 × 自己加倍（`magnification_stacked`）
  - **地主** = 公共倍数 × 自己加倍 × 农民1加倍 + 公共倍数 × 自己加倍 × 农民2加倍

---

## 二、extend_content（扩展信息）

### card_info（牌信息）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.card_info.hand_cards` | string | 手牌 |
| `$.card_info.bottom_cards` | string | 底牌 |
| `$.card_info.shuffle_type` | int | 配牌类型: 0=随机发牌，201=新手保护机器人，202=充值保护机器人匹配，203=老用户每日前N，204=房间前N次触发机器人保护，205=低保次数触发机器人保护，206=连输保护机器人匹配，207=连输银两触发机器人匹配，默认为0 |
| `$.card_info.card_id` | int | 牌库编号（默认0，0为随机牌；>0时代表有配牌，对应牌库编号） |

### card_power（牌力值）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.card_power.card_power` | int | 初始牌分 |
| `$.card_power.card_power_final` | int | 算上底牌后的牌分 |
| `$.card_power.cost_time` | int | 重洗牌花费时间（ms） |
| `$.card_power.is_pass` | boolean | 是否重洗成功 |
| `$.card_power.shuffle_times` | tinyint | 重洗次数：-1=未开启重洗，0=未重洗过，1=重洗过1次，依此类推 |

### user_attr（用户属性）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.user_attr.bout` | int | 用户历史局数（斗地主全局下首局时记录为0） |
| `$.user_attr.mode_bout` | int | 用户该玩法的历史局数（该玩法首局时记录为0） |

### ai_level（AI 等级详细字段）

| JSON 路径 | 类型 | 说明 |
| ---- | ---- | ---- |
| `$.ai_level.type` | tinyint | 机器人类型：0=逻辑机器人，1=算法机器人 |
| `$.ai_level.callflag` | tinyint | 叫地主强度：1=强化叫地主，2=监督叫地主，默认0 |
| `$.ai_level.robflag` | tinyint | 抢地主强度：1=强化抢地主，2=监督抢地主，默认0 |
| `$.ai_level.doubleflag` | tinyint | 加倍强度：1=强化加倍，2=监督加倍，默认0 |
| `$.ai_level.throwtileflag` | tinyint | 打牌强度：1=强化打牌，2=中等监督打牌，3=多风格打牌，默认0 |

## 示例值

```json
{
  "card_info": {
    "hand_cards": "3455677888999JJJQQQKKKAAA222",
    "bottom_cards": "345",
    "shuffle_type": 201,
    "card_id": 1001
  },
  "card_power": {
    "card_power": 15,
    "card_power_final": 22,
    "cost_time": 0,
    "is_pass": false,
    "shuffle_times": 0
  },
  "user_attr": {
    "bout": 2,
    "mode_bout": 1
  },
  "ai_level": {
    "type": 1,
    "callflag": 1,
    "robflag": 1,
    "doubleflag": 2,
    "throwtileflag": 1
  }
}
```

## 字段缺失说明

- **magnification_subdivision**：各子字段可能缺失，查询时建议使用 `get_json_int` 函数提取，并配合 `IFNULL` 处理 NULL 值。
- **extend_content**：各顶级分类（`card_info` / `card_power` / `user_attr` / `ai_level`）及子字段**并非 100% 存在**，可能单独缺失或部分缺失。查询时建议使用 `get_json_int` / `get_json_string` 函数提取，并配合 `IFNULL` / `COALESCE` 处理 NULL 值。

## 验收要点

请研发同学确认以下事项：

### magnification_subdivision

1. 两个顶级分类（`public_bet` / `behavior_bet`）的命名是否准确
2. 各子字段的 JSON 路径、类型、含义是否准确（特别是枚举值含义）
3. 倍数计算逻辑是否与实际代码一致

### extend_content

1. 四个顶级分类（`card_info` / `card_power` / `user_attr` / `ai_level`）的命名是否准确
2. 各子字段的 JSON 路径、类型、含义是否准确（特别是枚举值含义）
3. 字段缺失的场景说明是否完整（哪些情况下哪些字段会缺失）
4. 是否还存在未列出的子字段或扩展分类

---

> **文档版本**：v1.0
> **创建时间**：2026-06-02
> **关联表**：`hive_catalog_cdh5.dwd.fact_game_combatgains`
