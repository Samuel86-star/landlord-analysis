# MakeDealByCfg 做牌函数 & makedeal.json 配置说明

## 一、MakeDealByCfg 函数概述

`MakeDealByCfg` 是斗地主的**做牌（控牌）函数**，在发牌后对牌局进行干预，通过调整牌的分配来控制玩家手牌质量，使游戏体验更可控。

- 函数位置：`common/zgdatbl.cpp:3945`
- 配置文件：`makedeal.json`（由 `common/ConfigManagerSys.h` 中 `#define MAKEDEAL_CONFIG` 定义）
- 支持两种做牌模式：**Type 0**（老版全局统一做牌）和 **Type 1**（新版按玩家身份区分做牌）

---

## 二、MAKEDEALCFG 结构体（配置映射）

来源：`common/zgdareq.h:397-411`

```cpp
typedef struct _tagMakeDealCfg{
    int nMakeDealType;              // 做牌策略操作类型 0=老版，1=新版
    int nBeginMakeNum;              // 开始做牌的牌数
    int nBeginSelectBanker;         // 开始选取底牌的牌位置（仅Type=1）
    int nFirstChairHandCount;       // 庄家需要做牌的手数阈值(大于)
    int nFirstChairBombCount;       // 庄家需要做牌的炸弹数量(小于)
    int nFirstChairBigCardsCount;   // 庄家需要做牌的大牌2王数量(小于)
    int nOtherChairHandCount;       // 闲家需要做牌的手数阈值(大于)
    int nOtherChairBombCount;       // 闲家需要做牌的炸弹数量(小于)
    int nOtherChairBigCardsCount;   // 闲家需要做牌的大牌2王数量(小于)
    int nTargetValue;               // 做牌目标牌力值
    int nTargetRound;               // 做牌目标手数
    std::vector<int> arrCouPaiStrategy; // 拼牌做牌策略
    int nReserved[4];               // 保留，第一位存BigCardsTo(2王分配给指定椅子上限)
}MAKEDEALCFG, *LPMAKEDEALCFG;
```

### 字段与 JSON 键名对照表

| 字段 | JSON键名 | 类型 | 说明 |
|------|----------|------|------|
| nMakeDealType | MakeDealType | int | 做牌策略类型：**0** = 老版做牌（全局统一），**1** = 新版做牌（按玩家身份区分） |
| nBeginMakeNum | BeginMakeNum | int | 开始做牌的牌数（前N张牌固定分发，不做调整） |
| nBeginSelectBanker | BeginSelectBanker | int | 开始选取底牌的牌位置（仅Type=1生效，第N+1张作为底牌） |
| nFirstChairHandCount | FirstChairHandCount | int | 庄家（地主）需要做牌的手数阈值（大于此值才做牌） |
| nFirstChairBombCount | FirstChairBombCount | int | 庄家炸弹数量阈值（小于此值才做牌） |
| nFirstChairBigCardsCount | FirstChairBigCardsCount | int | 庄家大牌（2/王）数量阈值（小于此值才做牌） |
| nOtherChairHandCount | OtherChairHandCount | int | 闲家手数阈值（大于此值才做牌） |
| nOtherChairBombCount | OtherChairBombCount | int | 闲家炸弹阈值（小于此值才做牌） |
| nOtherChairBigCardsCount | OtherChairBigCardsCount | int | 闲家大牌阈值（小于此值才做牌） |
| nTargetValue | TargetValue | float→int | 做牌目标牌力值。正数时乘以MaxCardsValue(106)，负数时乘以MinCardsValue的绝对值 |
| nTargetRound | TargetRound | int | 做牌目标手数（手数>此值或牌力<此值时触发做牌） |
| nReserved[0] | BigCardsTo | int | 大牌（2/王）分配给指定椅子的上限数量，超过则不再匹配大牌 |
| arrCouPaiStrategy | CouPaiStrategy | int[][] | 拼牌策略优先级数组，从多个子数组中随机选一组 |

---

## 三、做牌模式详解

### 3.1 Type 0：老版做牌（全局统一）

**流程：**

1. **固定发牌阶段**：前 `BeginMakeNum` 张牌按原顺序分发给3个座位
   - `cards[i*3]` → 座位0, `cards[i*3+1]` → 座位1, `cards[i*3+2]` → 座位2

2. **统计手牌质量**：对已发的牌统计每个座位的 `手数(nHandCount)`、`炸弹数(nBombCount)`、`大牌数(nBigCardsCount)`

3. **判断是否需要做牌**：
   - **庄家**：手数 > FirstChairHandCount 且 炸弹 < FirstChairBombCount 且 大牌 < FirstChairBigCardsCount → 做牌
   - **闲家**：手数 > OtherChairHandCount 且 炸弹 < OtherChairBombCount 且 大牌 < OtherChairBigCardsCount → 做牌

4. **执行做牌(DoMakeDeal)**：从剩余牌堆中按优先级（2/王 → 炸弹 → 三条 → 顺子 → 连对 → 对子）匹配缺牌，替换空位

5. **填充剩余牌**：未匹配到的空位从剩余牌堆中顺序取牌填充

**前置条件**（全部满足才进入做牌逻辑）：
```
0 < BeginMakeNum < CARDS_PER_CHAIR(17)
FirstChairHandCount  != -1
FirstChairBombCount   != -1
FirstChairBigCardsCount != -1
OtherChairHandCount   != -1
OtherChairBombCount   != -1
OtherChairBigCardsCount != -1
```

### 3.2 Type 1：新版做牌（按玩家身份区分）

**流程：**

1. **为每个玩家分配不同策略**：
   - 机器人 → 使用 `"robot"` 策略（查找 `"robot{RoomID}"` → `"robot"`）
   - 新用户（局数 < NewUserBout） → 使用 `"newuser"` 策略
   - 普通玩家 → 使用房间默认策略

2. **逐牌分发循环（17+1轮）**：
   - `p < BeginMakeNum`：从剩余牌堆头部顺序取牌（不做调整）
   - `BeginMakeNum <= p < BeginSelectBanker`：**拼牌阶段**
     - 拆分手牌为牌型组（`SpliteCard`）
     - 计算牌力值（`CalHandCardValue`）和手数
     - 如果 `手数 > 目标手数 || 牌力 < 目标值`，则调用 `MakeDeal_ComposeCard` 从剩余牌中拼出需要的牌
   - `p == BeginSelectBanker`：从剩余牌堆尾部选取一张作为**底牌**
   - `p > BeginSelectBanker`：从剩余牌堆尾部顺序取牌（不做调整）

3. **输出**：最终17张手牌 + 3张底牌

**参数校验**（每个玩家独立校验）：
```
0 <= BeginMakeNum <= CARDS_PER_CHAIR(17)
0 <= BeginSelectBanker <= CARDS_PER_CHAIR(17)
```
校验不通过则直接 return，不做牌。

---

## 四、makedeal.json 配置文件结构

```json
{
  "MakeDeal": {
    "<RoomID>": "策略名"
  },

  "MakeDealStrategy": {
    "default": {
      "MakeDealType": 1,
      "BeginMakeNum": 5,
      "BeginSelectBanker": 12,
      "FirstChairHandCount": 7,
      "FirstChairBombCount": 1,
      "FirstChairBigCardsCount": 1,
      "OtherChairHandCount": 8,
      "OtherChairBombCount": 0,
      "OtherChairBigCardsCount": 1,
      "BigCardsTo": 2,
      "TargetValue": 0.3,
      "TargetRound": 5,
      "CouPaiStrategy": [
        [13, 6, 4, 5, 2, 1],
        [13, 6, 4, 1, 5, 2]
      ]
    },
    "default10001": {},
    "robot": {},
    "robot10001": {},
    "newuser": {},
    "newuser10001": {}
  },

  "MakeDealCommonArgs": {
    "MaxCardsValue": 106,
    "MinCardsValue": 25,
    "GroupDataExp": {
      "1":  [{"C": 1.0, "M": 1}],
      "2":  [{"C": 1.0, "M": 1}],
      "3":  [{"C": 1.0, "M": 1}],
      "4":  [{"C": 1.0, "M": 1}],
      "5":  [{"C": 1.0, "M": 1}],
      "6":  [{"C": 1.0, "M": 1}],
      "7":  [{"C": 1.0, "M": 1}],
      "8":  [{"C": 1.0, "M": 1}],
      "9":  [{"C": 1.0, "M": 1}],
      "10": [{"C": 1.0, "M": 1}],
      "11": [{"C": 1.0, "M": 1}],
      "12": [{"C": 1.0, "M": 1}],
      "13": [{"C": 1.0, "D": 2}],
      "14": [{"C": 20.0}]
    }
  },

  "PlayerMakeDealRule": {
    "IsEnable": 1,
    "RoomRule": {
      "<RoomID>": {
        "NewUserBout": 10
      }
    }
  }
}
```

### 各节点说明

| 节点 | 说明 |
|------|------|
| `MakeDeal` | 房间与策略名的映射。键为房间ID字符串，值为策略名 |
| `MakeDealStrategy` | 所有做牌策略定义。键为策略名，值为策略参数 |
| `MakeDealCommonArgs` | 做牌通用参数，包括牌力值计算基数和各牌型价值公式 |
| `PlayerMakeDealRule` | 新用户做牌规则，控制哪些房间启用、新用户局数阈值 |

---

## 五、策略查找优先级

`GetMakeDealCfg`（`common/zgdatbl.cpp:4167`）的策略查找顺序：

### 5.1 指定策略名（如 `"robot"`、`"newuser"`）

```
1. 查找 "策略名+RoomID"（如 "robot10001"）
2. 不存在 → 查找 "策略名"（如 "robot"）
3. 都不存在 → 回退 "default"
```

### 5.2 默认查找（无指定策略名）

```
1. 从 MakeDeal[RoomID] 获取策略名
2. 查找 "策略名+RoomID"（加房间后缀优先）
3. 不存在 → 查找 "策略名"
4. 都不存在 → 回退 "default"
```

---

## 六、CouPaiStrategy 拼牌策略值含义

CouPaiStrategy 中的数字对应 `CardGroupType` 枚举（`zgdasvr/MakeDealHelper.h:26-44`）：

| 值 | 枚举名 | 牌型 | 拼牌逻辑 |
|----|--------|------|----------|
| 13 | cgBOMB_CARD | 炸弹 | 从三条拼成炸弹（三条+1张同值牌） |
| 6 | cgTHREE_LINE | 飞机 | 从三条+对子拼成飞机（三条+相邻对子+1张） |
| 4 | cgSINGLE_LINE | 顺子 | 从单牌+对子拼成顺子（缺1张的5连） |
| 5 | cgDOUBLE_LINE | 连对 | 从单牌+对子拼成连对（缺1张的3连对） |
| 3 | cgTHREE | 三条 | 从对子拼成三条（对子+1张同值牌） |
| 2 | cgDOUBLE | 对子 | 从单牌拼成对子（单牌+1张同值牌） |
| 1 | cgSINGLE | 单牌 | 无法拼牌时发一张缺少的单牌 |

数组中数字的排列顺序即为**拼牌优先级**，优先匹配前面的牌型。

**CouPaiStrategy 是二维数组**：外层包含多个子数组，运行时随机选择一个子数组使用。这提供了策略多样性，避免每局拼牌模式完全相同。

---

## 七、牌力值(TargetValue)计算

### 7.1 目标值转换

- **正数**（如0.3）：`nTargetValue = 0.3 × MaxCardsValue(106) = 31.8 → 取整31`
- **负数**（如-0.2）：`nTargetValue = -(-0.2 × MinCardsValue_abs) = -5 → 绝对值5`，用于设定负牌力阈值

### 7.2 牌型组价值计算

每个牌型组的价值通过 `GroupDataExp` 配置的公式计算：

```json
{"C": 系数, "M": 指数}  →  value = C × MaxCard^M
{"C": 系数, "D": 对数底} →  value = C × log_D(MaxCard)
```

可叠加多条公式（数组中有多个元素时累加）。

### 7.3 手牌总牌力计算

`CalHandCardValue`（`zgdasvr/MakeDealHelper.cpp:1661`）：

1. 将手牌拆分为牌型组（`SpliteCard`）
2. 按牌型组价值从高到低排序
3. 统计所有牌型组的总价值
4. **扣减**：三条/飞机可带的低值单牌和对子不计入手数（它们被带出不算独立出牌）
5. 最终：`nHandCardAveValue` = 扣减后的总价值

### 7.4 牌力分布参考

| 牌力范围 | 牌力等级 |
|----------|----------|
| < 10 | 差牌 |
| 10 ~ 14 | 一般 |
| 15 ~ 19 | 好牌 |
| >= 20 | 极好牌（有炸弹/王炸） |

---

## 八、CardLay（牌值分布表）详解

`SK_LAYOUT_NUM` = 16，索引含义如下：

| 索引 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|------|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 牌值 | - | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | J | Q | K | A | 小王 | 大王 |

- 索引0不使用
- 索引1~13对应3~A（2对应索引1，因为2是斗地主最大单牌）
- 索引14=小王，15=大王

`SK_GetCardIndexEx(cardID, 0)` 映射规则：
- cardID 0-51（普通牌）：`cardID % 13` → 0对应2(索引1)，1对应3(索引2)，...，12对应A(索引13)
- cardID 52（小王）：索引14
- cardID 53（大王）：索引15

日志显示顺序（按 nOrder 数组）：`B(?) S(小王) 2 A K Q J 10 9 8 7 6 5 4 3`

---

## 九、cards 数组格式

`cards[]` 是54张牌的输入数组，格式为**交错排列**：

```
cards[0],  cards[1],  cards[2]   → 座位0、1、2的第1张牌
cards[3],  cards[4],  cards[5]   → 座位0、1、2的第2张牌
...
cards[48], cards[49], cards[50]  → 座位0、1、2的第17张牌
cards[51], cards[52], cards[53]  → 3张底牌
```

每张牌的ID范围：
- 0-51：普通牌（4花色 × 13点）
- 52：小王
- 53：大王

---

## 十、CalcHandCardsCount 手牌统计详解

来源：`common/zgdatbl.h:769-813`

该函数统计一手牌的关键质量指标，做牌判断依赖这些指标：

| 输出参数 | 含义 | 计算方式 |
|----------|------|----------|
| nHandCount | 手数（需几次出完） | 按优先级合并牌型后统计 |
| nBombCount | 炸弹数量 | 4张同值 + 王炸 |
| nBigCardCount | 大牌数量 | `nCardLay[1]`(2的数量) + `nCardLay[14]`(小王) + `nCardLay[15]`(大王) |

统计优先级顺序（从上到下依次消耗牌）：

1. `Calc2KingHandCount` — 王炸
2. `CalcBombHandCount` — 炸弹
3. `CalcABTThreeHandCount` — 飞机带翅膀
4. `CalcABTCoupleHandCount` — 连对
5. `CalcABTHandCount` — 顺子
6. `CalcThreeHandCount` — 三条
7. `CalcCoupleHandCount` — 对子
8. `CalcSingleHandCount` — 单牌

---

## 十一、DoMakeDeal 做牌执行逻辑详解

来源：`common/zgdatbl.cpp:4238-4339`

### 11.1 判断是否对该座位做牌的条件

| 座位 | 第1轮条件（i==0） | 第2轮起条件（i>0） |
|------|-------------------|-------------------|
| 庄家(m_nBanker) | 手数 > FirstChairHandCount 且 炸弹 < FirstChairBombCount 且 大牌 < FirstChairBigCardsCount | 无条件做牌 |
| 闲家1(下家) | 手数 > OtherChairHandCount 且 炸弹 < OtherChairBombCount 且 大牌 < OtherChairBigCardsCount | 无条件做牌 |
| 闲家2(再下家) | 手数 > OtherChairHandCount 且 炸弹 < OtherChairBombCount 且 大牌 < OtherChairBigCardsCount | 无条件做牌 |

**额外前提**：如果该座位是机器人，需 INI 配置 `RobotNeedMakeDeal` 对应房间为 TRUE 才做牌。

**含义**：第1轮（i==0）需要同时满足三个条件才做牌，第2轮起（i>0）只要进入了做牌流程就无条件继续。这确保了：如果第1轮判断需要做牌，后续轮次会持续补充直到所有空位填满或匹配失败。

### 11.2 做牌匹配优先级（Type 0 固定顺序）

`MatchFirstChairCards` / `MatchOtherChairCards` 的匹配顺序：

1. `Match2OrKingCardType` — 匹配2或王（受 BigCardsTo 限制）
2. `MatchBombCardType` — 匹配炸弹
3. `MatchThreeCardType` — 匹配三条
4. `MatchABTCardType` — 匹配顺子
5. `MatchABTCoupleCardType` — 匹配连对
6. `MatchCoupleCardType` — 匹配对子

每轮只匹配一个空位，匹配到后立即 break，进入下一轮。如果所有匹配都失败则 return（不再尝试后续空位）。

---

## 十二、Type 0 做牌六种匹配函数详解

所有匹配函数从**剩余牌堆(pReserveCards)**中寻找合适的牌填入**目标座位的空位(-1)**。

### 12.1 Match2OrKingCardType — 匹配2或王

- **前提**：该座位已有的2+小王+大王数量 < `BigCardsTo`（否则跳过，不再分配大牌）
- **逻辑**：遍历剩余牌堆，找第一张是2(索引1)、小王(索引14)或大王(索引15)的牌
- **作用**：给弱牌座位补大牌控制力

### 12.2 MatchBombCardType — 匹配炸弹

- **前提**：目标座位有3张同值牌（nCardLay[i]==3），且范围3~A（索引2~13）
- **逻辑**：在剩余牌堆中找第4张同值牌，凑成炸弹
- **作用**：给有3条的座位补齐第4张，形成炸弹

### 12.3 MatchThreeCardType — 匹配三条

- **前提**：目标座位有2张同值牌（nCardLay[i]==2），范围2~A（索引1~13）
- **逻辑**：在剩余牌堆中找第3张同值牌，凑成三条
- **作用**：给有对子的座位补齐第3张，形成三条

### 12.4 MatchABTCardType — 匹配顺子

- **前提**：5张连续牌中缺1张，且其余4张各自1~3张
- **逻辑**：从3(索引2)到10(索引9)起始，检查连续5个位置的牌分布，如果恰好缺1张且有4张存在，则从剩余牌堆中找缺的那张
- **过滤**：排除"2对+2单牌"且两对间距<=2的情况（拆对不值得）
- **作用**：补齐5连顺子的缺牌

### 12.5 MatchABTCoupleCardType — 匹配连对

- **前提**：3连对缺1张（即3个连续值中有2个有2张，1个只有1张）
- **逻辑**：从3(索引2)到Q(索引11)起始，检查连续3个位置，如果有2个值各有2张、1个值只有1张，则从剩余牌堆补缺值
- **作用**：补齐3连对的缺牌

### 12.6 MatchCoupleCardType — 匹配对子

- **前提**：目标座位有1张某值牌（nCardLay[i]==1），范围2~A（索引1~13）
- **逻辑**：在剩余牌堆中找第2张同值牌
- **作用**：给有单牌的座位补对子

**匹配顺序固定**：2/王 → 炸弹 → 三条 → 顺子 → 连对 → 对子，不可配置（Type 0 模式）。仅 Type 1 的 CouPaiStrategy 可配置拼牌优先级。

---

## 十三、CopyMatchedCardID 匹配结果写入

来源：`common/zgdatbl.cpp:4410-4427`

```cpp
BOOL CGameTable::CopyMatchedCardID(int &nPreCardID, int nCardLay[],
                                    int nMatchedCardID, int nReserveCards[], int nReserveCount)
```

- 在剩余牌堆中查找匹配到的 CardID
- 找到后：将剩余牌堆中该位置置为 -1（已使用），将目标座位空位填入该牌，更新 CardLay 计数
- 返回 TRUE 表示匹配成功，FALSE 表示匹配失败（牌不在剩余堆中）

---

## 十四、m_nMakeDealTypes 做牌类型标记

该数组记录每个座位实际使用了哪种做牌方式，用于日志和监控：

| 值 | 含义 | 设置位置 |
|----|------|----------|
| 0 | 未做牌 | 初始值（`memset` 清零） |
| 1 | 做牌已启用（配置层面） | `IsNeedMakeDealForNovice` 返回TRUE时 |
| 2 | 老版做牌（Type 0）实际执行了做牌 | `MatchFirstChairCards` / `MatchOtherChairCards` 执行后 |
| 3 | 新版做牌（Type 1）拼牌阶段执行了做牌 | `MakeDeal_ComposeCard` 执行后 |

---

## 十五、MakeDealByCfg2 与 MakeDealByCfg 的区别

`MakeDealByCfg2`（`common/zgdatbl.cpp:11469`）是 `MakeDealByCfg` 的改进版本，逻辑基本相同，主要区别：

| 对比项 | MakeDealByCfg | MakeDealByCfg2 |
|--------|---------------|----------------|
| 内存管理 | `new/delete` 动态分配 | 栈数组 `int pReserveCards[TOTAL_CARDS]`，更安全 |
| 调用场景 | 旧版洗牌流程，只做一轮 | 新版洗牌流程，配合 `isGoodCards` 多轮重洗 |
| 牌力验证 | 无 | 配合 `ShuffleDealStrategy.isGoodCards` 验证做牌结果 |

---

## 十六、新版洗牌流程(isGoodCards)与MakeDealByCfg2的关系

`MakeDealByCfg2` 配合 `ShuffleDealStrategy.isGoodCards` 构成**多轮重洗机制**（`common/zgdatbl.cpp:1441-1461`）：

```
循环最多 cfg.maxReshuffleTimes 次，总耗时不超过1秒：
  1. 随机洗牌 (SvrXygRandomSort)
  2. 做牌 (MakeDealByCfg2)
  3. 检查牌力 (isGoodCards)
  4. 如果达标(isOk=true) → 退出循环，使用当前牌
  5. 如果不达标 → 继续洗牌重试
```

这意味着新版流程不仅做牌，还验证做牌结果是否满足全局牌力要求，避免做牌后牌力仍然失衡。

旧版流程（`MakeDealByCfg`）只做一轮，不做验证。

---

## 十七、发牌完整调用链

发牌入口函数中（`common/zgdatbl.cpp:1384-1467`）有三条分支：

| 分支 | 条件 | 行为 |
|------|------|------|
| 新手保护 | `!m_bIsProtected && IsNeedMakeDealForNovice()` | 先定庄 → 从文件读取固定好牌 → 设置保护标记 |
| 固定牌局 | `IsNeedMakeDealForNoShuff(RoomID)` | 使用预设牌序，不做牌 |
| 正常做牌 | 其余情况 | 定庄 → 洗牌 → `MakeDealByCfg`/`MakeDealByCfg2` → 做牌 |

### 17.1 IsNeedMakeDealForNovice 判断条件

- `PlayerMakeDealRule.IsEnable == 1`
- 该房间有 `RoomRule` 配置
- 桌上恰好只有1个真人玩家（2个机器人陪1个新手）
- 新手局数 <= `NewUserBout` 阈值

### 17.2 IsNeedMakeDealByUserBoutInfo 判断条件

来源：`common/zgdatbl.cpp:4134-4164`

- `PlayerMakeDealRule.IsEnable == 1`
- 该房间有 `RoomRule` 配置
- 玩家局数(`m_nBout`) <= `NewUserBout` 阈值

**区别**：`IsNeedMakeDealForNovice` 要求桌上只有1个真人（新手保护场景），`IsNeedMakeDealByUserBoutInfo` 只看单个玩家局数（Type 1 做牌中判断是否使用 newuser 策略）。

### 17.3 新手保护机制

为低局数新手从预设文件（`ReadNoviceCardsFromFile`）中读取经过精心编排的好牌，确保新手有良好体验。设置 `m_bIsProtected` 标记后，后续不再重复触发。

---

## 十八、INI 配置补充（zgdasvr.ini）

做牌还依赖 INI 文件中的 `RobotNeedMakeDeal` 配置（`common/zgdatbl.cpp:4247-4251`）：

```ini
[RobotNeedMakeDeal]
<RoomID>=1    ; 1=机器人也做牌, 0=机器人不做牌（默认1）
```

当某个座位是机器人时，只有该配置为 TRUE 才会对机器人座位执行做牌。设为 0 可让机器人保持随机牌力，降低做牌对机器人胜率的影响。

---

## 十九、MakeDeal_ComposeCard 拼牌核心函数

来源：`zgdasvr/MakeDealHelper.cpp:771-1475`

### 19.1 函数签名

```cpp
ComposeCardResult MakeDeal_ComposeCard(
    LPMAKEDEALCFG pCfg,                          // 做牌配置
    IN OUT std::vector<CardGroupData>& CardGroupDatas,  // 当前手牌的牌型拆分
    IN OUT std::vector<int>& RemainCards,        // 剩余牌堆
    IN bool bNeedMakeDeal = true                 // 是否需要做牌（false=仅保证基本出牌能力）
);
```

### 19.2 返回值 ComposeCardResult

```cpp
struct ComposeCardResult {
    bool bRet;                      // 是否拼牌成功
    std::string ComposeCardGroupType;  // 拼出的牌型名称
    int ComposeCardGroupCardCount;    // 拼出牌型的牌数
    int nRemoveCardID;               // 从剩余牌堆中取走的牌ID
};
```

### 19.3 拼牌逻辑

**当 bNeedMakeDeal == false（不需要做牌，仅保证基本出牌能力）：**

1. 遍历手牌牌型组，找单牌 → 尝试从剩余牌堆补同值牌凑成对子
2. 找对子 → 尝试补同值牌凑成三条
3. 如果以上都没找到，发一张手牌中没有的新单牌

**当 bNeedMakeDeal == true（需要做牌）：**

按 `CouPaiStrategy` 数组中的优先级依次尝试拼牌：

1. **cgBOMB_CARD(13)**：从三条+剩余牌堆中同值牌凑炸弹
2. **cgTHREE_LINE(6)**：从三条+相邻对子+剩余牌凑飞机，并尝试延伸
3. **cgSINGLE_LINE(4)**：从单牌/对子+剩余牌凑顺子（缺1张的5连），并尝试延伸
4. **cgDOUBLE_LINE(5)**：从单牌+相邻对子+剩余牌凑连对，并尝试延伸
5. **cgTHREE(3)**：从对子+剩余牌凑三条
6. **cgDOUBLE(2)**：从单牌+剩余牌凑对子

**兜底逻辑**（所有策略都未拼成）：

1. 如果手牌没有小王/大王，优先发小王/大王
2. 否则发一张手牌中缺少的牌值对应的单牌

---

## 二十、完整 makedeal.json 配置示例

```json
{
  "MakeDeal": {
    "10001": "normal",
    "10002": "easy",
    "10003": "hard"
  },

  "MakeDealStrategy": {
    "default": {
      "MakeDealType": 1,
      "BeginMakeNum": 5,
      "BeginSelectBanker": 12,
      "FirstChairHandCount": 7,
      "FirstChairBombCount": 1,
      "FirstChairBigCardsCount": 1,
      "OtherChairHandCount": 8,
      "OtherChairBombCount": 0,
      "OtherChairBigCardsCount": 1,
      "BigCardsTo": 2,
      "TargetValue": 0.3,
      "TargetRound": 5,
      "CouPaiStrategy": [
        [13, 6, 4, 5, 2, 1],
        [13, 4, 6, 2, 5, 1]
      ]
    },

    "normal10001": {
      "MakeDealType": 1,
      "BeginMakeNum": 6,
      "BeginSelectBanker": 13,
      "FirstChairHandCount": 6,
      "FirstChairBombCount": 1,
      "FirstChairBigCardsCount": 2,
      "OtherChairHandCount": 7,
      "OtherChairBombCount": 1,
      "OtherChairBigCardsCount": 1,
      "BigCardsTo": 2,
      "TargetValue": 0.4,
      "TargetRound": 5,
      "CouPaiStrategy": [
        [13, 6, 4, 5, 3, 2, 1]
      ]
    },

    "easy": {
      "MakeDealType": 1,
      "BeginMakeNum": 5,
      "BeginSelectBanker": 11,
      "FirstChairHandCount": 8,
      "FirstChairBombCount": 0,
      "FirstChairBigCardsCount": 2,
      "OtherChairHandCount": 9,
      "OtherChairBombCount": 0,
      "OtherChairBigCardsCount": 2,
      "BigCardsTo": 3,
      "TargetValue": 0.5,
      "TargetRound": 4,
      "CouPaiStrategy": [
        [13, 13, 6, 4, 5, 3, 2, 1]
      ]
    },

    "robot": {
      "MakeDealType": 1,
      "BeginMakeNum": 3,
      "BeginSelectBanker": 10,
      "FirstChairHandCount": 10,
      "FirstChairBombCount": 0,
      "FirstChairBigCardsCount": 3,
      "OtherChairHandCount": 10,
      "OtherChairBombCount": 0,
      "OtherChairBigCardsCount": 3,
      "BigCardsTo": 1,
      "TargetValue": 0.1,
      "TargetRound": 7,
      "CouPaiStrategy": [
        [1, 2, 3]
      ]
    },

    "newuser": {
      "MakeDealType": 1,
      "BeginMakeNum": 5,
      "BeginSelectBanker": 12,
      "FirstChairHandCount": 7,
      "FirstChairBombCount": 0,
      "FirstChairBigCardsCount": 1,
      "OtherChairHandCount": 8,
      "OtherChairBombCount": 0,
      "OtherChairBigCardsCount": 1,
      "BigCardsTo": 3,
      "TargetValue": 0.5,
      "TargetRound": 4,
      "CouPaiStrategy": [
        [13, 6, 4, 5, 3, 2, 1]
      ]
    }
  },

  "MakeDealCommonArgs": {
    "MaxCardsValue": 106,
    "MinCardsValue": 25,
    "GroupDataExp": {
      "1":  [{"C": 1.0, "M": 1}],
      "2":  [{"C": 1.0, "M": 1}],
      "3":  [{"C": 1.0, "M": 1}],
      "4":  [{"C": 1.0, "M": 1}],
      "5":  [{"C": 1.0, "M": 1}],
      "6":  [{"C": 1.0, "M": 1}],
      "7":  [{"C": 1.0, "M": 1}],
      "8":  [{"C": 1.0, "M": 1}],
      "9":  [{"C": 1.0, "M": 1}],
      "10": [{"C": 1.0, "M": 1}],
      "11": [{"C": 1.0, "M": 1}],
      "12": [{"C": 1.0, "M": 1}],
      "13": [{"C": 1.0, "D": 2}],
      "14": [{"C": 20.0}]
    }
  },

  "PlayerMakeDealRule": {
    "IsEnable": 1,
    "RoomRule": {
      "10001": { "NewUserBout": 10 },
      "10002": { "NewUserBout": 5 }
    }
  }
}
```

---

## 二十一、配置调优指南

### 21.1 让庄家牌更强

| 目标 | 调整方向 |
|------|----------|
| 减少手数 | 降低 `FirstChairHandCount`，使更多牌局触发做牌 |
| 增加炸弹概率 | 降低 `FirstChairBombCount` |
| 增加大牌(2/王) | 降低 `FirstChairBigCardsCount`，提高 `BigCardsTo` |
| 提高拼牌质量 | 提高 `TargetValue`（如0.4→0.6），降低 `TargetRound` |
| 拼牌偏重炸弹 | CouPaiStrategy 中把 13 放在前面，或重复出现 |
| 拼牌偏重顺子 | CouPaiStrategy 中把 4 放在前面 |

### 21.2 让机器人少受做牌影响

| 目标 | 调整方向 |
|------|----------|
| 给机器人弱策略 | 配置 `robot` 策略：高 HandCount 阈值、低 BigCardsTo、高 TargetRound |
| 完全不做牌 | `robot` 策略设 `BeginMakeNum=0` 或所有阈值设为极端值 |
| INI层面禁用 | `zgdasvr.ini` 中 `[RobotNeedMakeDeal]` 对应房间设为 0 |

### 21.3 保护新用户

| 目标 | 调整方向 |
|------|----------|
| 启用新用户规则 | `PlayerMakeDealRule.IsEnable = 1` |
| 定义新用户局数 | `RoomRule.{RoomID}.NewUserBout` 设高值 |
| 给新用户强牌 | 配置 `newuser` 策略：低 HandCount、高 TargetValue、低 TargetRound |
| 增加拼牌强度 | CouPaiStrategy 包含更多牌型（如 `[13, 13, 6, 4, 5, 3, 2, 1]`） |

### 21.4 牌力公式调优(GroupDataExp)

| 牌型 | 默认公式 | 调优建议 |
|------|----------|----------|
| 单牌(1) | C×MaxCard^1 | 降低C值可减少单牌权重，使拼牌更倾向凑组合 |
| 对子(2) | C×MaxCard^1 | 同上 |
| 三条(3) | C×MaxCard^1 | 三条是拼牌基础，一般不动 |
| 顺子(4) | C×MaxCard^1 | 提高C值可提高顺子权重，使拼牌更倾向凑顺子 |
| 连对(5) | C×MaxCard^1 | 同上 |
| 飞机(6) | C×MaxCard^1 | 飞机权重高可鼓励拼飞机 |
| 炸弹(13) | C×log2(MaxCard) | 对数增长较慢，可改用指数公式提高高牌炸弹权重 |
| 王炸(14) | C=20.0 | 固定值，一般不动 |

---

## 二十二、调试与日志

### 22.1 编译宏

- `_MAKEDEALINFO`：启用做牌详细日志，输出每轮做牌前后的牌分布对比
- `_DEBUG` 或 `_RS125`：启用洗牌次数、牌力、耗时等统计日志

### 22.2 日志输出格式

做牌前后牌分布对比日志格式：
```
    B  S  2  A  K  Q  J  10 9  8  7  6  5  4  3
    0  1  2  1  1  0  1  1  2  1  1  2  1  1  1  ← 座位0
    0  0  1  1  1  2  1  1  0  1  1  1  2  1  1  ← 座位1
    0  1  1  1  1  1  1  1  1  1  1  1  1  1  1  ← 座位2
```

被做牌修改的位置用 `@` 标记（如 `@2` 表示该位置从原值被做牌修改为2）。

### 22.3 洗牌统计日志

```
ShuffleTimes:N/M, nFirstPower:[P0,P1,P2] nSingles:[S0,S1,S2] nBomb:[B0,B1,B2]
PotentialLandlord:X, LandlordAdv:Y, IsPass:Z, CostTime:T
```

| 字段 | 含义 |
|------|------|
| ShuffleTimes | 当前重洗次数/最大次数 |
| nFirstPower | 3个座位的牌力值 |
| nSingles | 3个座位的单牌数 |
| nBomb | 3个座位的炸弹数 |
| PotentialLandlord | 潜在庄家优势值 |
| LandlordAdv | 庄家优势值 |
| IsPass | 是否通过牌力检查 |
| CostTime | 本轮耗时(ms) |

---

## 二十三、源码文件索引

| 文件 | 关键内容 |
|------|----------|
| `common/zgdatbl.cpp:3945` | `MakeDealByCfg` 函数实现 |
| `common/zgdatbl.cpp:11469` | `MakeDealByCfg2` 函数实现 |
| `common/zgdatbl.cpp:4167` | `GetMakeDealCfg` 配置读取函数 |
| `common/zgdatbl.cpp:4238` | `DoMakeDeal` 做牌执行函数 |
| `common/zgdatbl.cpp:4342` | `MatchFirstChairCards` 庄家匹配函数 |
| `common/zgdatbl.cpp:4375` | `MatchOtherChairCards` 闲家匹配函数 |
| `common/zgdatbl.cpp:4134` | `IsNeedMakeDealByUserBoutInfo` 新用户判断 |
| `common/zgdatbl.cpp:1900` | `IsNeedMakeDealForNovice` 新手保护判断 |
| `common/zgdatbl.h:769` | `CalcHandCardsCount` 手牌统计函数 |
| `common/zgdatbl.h:816-989` | 6种匹配函数（Match*） |
| `common/zgdareq.h:397` | `MAKEDEALCFG` 结构体定义 |
| `common/ConfigManagerSys.h:10` | `MAKEDEAL_CONFIG` 宏定义 |
| `zgdasvr/MakeDealHelper.h` | `ComposeCardResult`、`CardGroupType` 枚举、辅助函数声明 |
| `zgdasvr/MakeDealHelper.cpp:771` | `MakeDeal_ComposeCard` 拼牌核心函数 |
| `zgdasvr/MakeDealHelper.cpp:1661` | `CalHandCardValue` 牌力计算函数 |
