# 742 / 420 发牌核心逻辑源码逆向分析

> 分析对象：经典玩法房 **742** 与 **420**（不展开其他房间/玩法）。
> 源码：`algorithm/native/previous/zgdatbl.cpp`、`zgdatbl.h`、`MakeDealHelper.cpp`、`MakeDealHelper.h`、`ConfigManagerSys.h`、`makedeal.json`，以及仓库内 `MakeDealByCfg配置说明.md`。
> 整理日期：2026-08-05。所有结论均带 `文件:行号` 举证。

## 核心定性

742 与 420 当前都跑 `MakeDealType=1`（Type1，`MakeDeal_ComposeCard` 拼牌）。两房**对机器人和普通真人一视同仁**（机器人策略 `robot742`/`robot420` 与房间策略 `new`/`new2` 逐字段相同，均无炸码 13）；但存在一层**针对"新手真人"的偏向发牌**——局数 ≤ 阈值的真人走 `newuser` 策略（**含炸码 13，主动凑炸送好牌**），这是两房唯一的"不平等"。此外当桌面只有 1 个真人时，还有更上游的"脚本手牌 + 强制当庄"新手保护层。

---

# 问题 A：`makedeal.json` 字段含义与代码映射

## A.1 742 / 420 实际读取的 JSON 节点路径

发牌入口 `StartDeal()` 先判定是否做牌（`zgdatbl.cpp:1348-1357`），判定依据是 `MakeDeal[roomID]` 是否非空：

```
MAKEDEAL_CONFIG == "makedeal.json"          // ConfigManagerSys.h:10
MakeDeal["742"] = "new"                      // makedeal.json:8
MakeDeal["420"] = "new2"                     // makedeal.json:7
```

进入 `MakeDealByCfg()`（`zgdatbl.cpp:3894`）后，`GetMakeDealCfg()`（`zgdatbl.cpp:4116`）按**身份**解析出实际生效的策略名。对 742 / 420，一次发牌最多触达以下 4 个层级：

| 身份（742 / 420） | 解析出的策略名 | JSON 节点路径 | 是否存在房间级覆盖 |
|---|---|---|---|
| 房间默认（普通真人） | `new` / `new2` | `MakeDealStrategy.new` / `.new2` | 无 `new742`/`new2420` → 落到通用 `new`/`new2` |
| 机器人 | `robot742` / `robot420` | `MakeDealStrategy.robot742` / `.robot420` | **有**，且内容 = `new`/`new2` |
| 新手真人（局数≤阈值） | `newuser` | `MakeDealStrategy.newuser` | 无 `newuser742` → 落到通用 `newuser` |
| 全局参数 | — | `MakeDealCommonArgs`（MaxCardsValue=121/MinCardsValue=16）、`PlayerMakeDealRule.RoomRule["742"|"420"]` | — |

> 解析优先级（`GetMakeDealCfg` 4124-4152）：指定策略名时，先查 `<策略名>+<roomID>`（如 `robot742`），命中则用之，否则回退通用 `<策略名>`（如 `robot`）。**正是因为 `robot742`/`robot420` 存在且等于房间策略，742/420 的机器人才不会被套用通用 `robot`（含炸码 13 的"控水"档）。**

## A.2 字段对照表（JSON 字段 → C++ 变量 → 含义 → 算法用途）

`MAKEDEALCFG` 结构体定义见 `MakeDealByCfg配置说明.md:18-32`（源自 `common/zgdareq.h:397-411`），由 `GetMakeDealCfg` 在 `zgdatbl.cpp:4155-4183` 逐字段灌入。

| JSON 字段 | C++ 变量 | 业务含义 | 在 742/420（Type1）中的具体算法用途 |
|---|---|---|---|
| `MakeDealType` | `nMakeDealType` | 做牌引擎选择 | `=1` → 走 `MakeDealByCfg` 的 Type1 分支（`zgdatbl.cpp:3993`），调 `MakeDeal_ComposeCard`；`=0` 才走 `DoMakeDeal` 菜单 |
| `BeginMakeNum` | `nBeginMakeNum` | 前 N 张纯随机、不干预 | Type1 循环里 `p < nBeginMakeNum` 直接随机发牌（`zgdatbl.cpp:4026-4031`）。742=11、420=14 |
| `BeginSelectBanker` | `nBeginSelectBanker` | 干预结束/抓底牌的牌位 | `p < nBeginSelectBanker` 才拼牌（4032）；`p == nBeginSelectBanker` 抓 1 张底牌（4046-4053）。742=15、420=17 |
| `CouPaiStrategy` | `arrCouPaiStrategy` | 拼牌牌型优先级（随机选一组） | `MakeDeal_ComposeCard` 按此顺序尝试组牌（`MakeDealHelper.cpp:849`）。码 `13`=把三张补成炸弹（852-869）。**742/420 的 `new`/`new2`/`robot742`/`robot420` 均不含 13 → 不主动凑炸** |
| `TargetValue` | `nTargetValue` | 牌力干预门槛（×MaxCardsValue 缩放） | `nTargetValue = TargetValue × 121`（4165-4172）；当 `手牌牌力 < nTargetValue` 时 `bNeedMakeDeal=true`（4041）。742/420=999×121→极大 → **永远干预** |
| `TargetRound` | `nTargetRound` | 手数干预门槛 | `手数 > nTargetRound` 也触发干预（4041）。742/420=10（几乎不单独触发） |
| `BigCardsTo` | `nReserved[0]` | 单椅 2/王张数上限 | **仅 Type0 生效**：`Match2OrKingCardType` 里 `nCardLay[1]+[14]+[15] >= nReserved[0]` 即停止往该椅塞 2/王（`zgdatbl.h:969`）。Type1 不读 |
| `FirstChairHandCount` / `BombCount` / `BigCardsCount` | `nFirstChair*` | 庄家做牌阈值（手数>/炸弹</大牌<） | **仅 Type0 生效**：`DoMakeDeal` 第一轮据此判断庄家是否需要做牌（`zgdatbl.cpp:4208-4210`）。Type1 不读 |
| `OtherChairHandCount` / `BombCount` / `BigCardsCount` | `nOtherChair*` | 闲家做牌阈值 | 同上，闲家门槛（4225-4227）。Type1 不读 |

> **关键结论**：在 Type1（742/420 实际运行的模式）下，真正参与运算的只有 **`MakeDealType / BeginMakeNum / BeginSelectBanker / CouPaiStrategy / TargetValue / TargetRound`** 这 6 个字段；`BigCardsTo` 与六个 `First/OtherChair*` 是 Type0 遗留字段，**在 742/420 里完全惰性**（写进结构体但不被任何 Type1 代码读取）。这也是两房"庄闲大牌差≈0"的根因——Type1 根本没有 Type0 那套"系统性给庄家塞 2/王"的逻辑。

## A.3 配置→代码 关联（`GetMakeDealCfg` 解析流）

```
GetMakeDealCfg(pCfg, MakeDealStrategy="")           // zgdatbl.cpp:4116
 ├─ 若传入策略名（"robot"/"newuser"）：
 │    候选 = 策略名 + roomID  (如 "robot742")        // 4127
 │    若 MakeDealStrategy["robot742"] 存在 → 用之   // 4128 命中
 │    否则回退 MakeDealStrategy["robot"]            // 4131
 ├─ 若未传策略名（取房间默认）：
 │    name = MakeDeal["742"] = "new"                // 4137-4139
 │    若 MakeDealStrategy["new742"] 存在 → 用之     // 4140（742 不存在→回退）
 │    否则用 "new"
 ├─ 逐字段灌入 MAKEDEALCFG（4155-4174）
 ├─ TargetValue × MaxCardsValue(121) → nTargetValue  // 4165-4172
 └─ CouPaiStrategy 多组中 rand() 选一组 → arrCouPaiStrategy  // 4177-4183
```

---

# 问题 B：真人 vs 机器人 发牌路由

## B.1 身份识别

- 判据是玩家对象的 `m_nUserType == USER_TYPE_ROBOT`，封装为 `CPlayer::IsRoboter()`（`zgdatbl.cpp` 多处调用：875、1736、1868、3998…）。
- 桌级聚合：`GetRobotCount()` 遍历三家计数机器人（`zgdatbl.cpp:10028-10039`），`IsRobotTable()` = 机器人 > 0（`10018-10026`）。

## B.2 742 / 420 是否存在不平等发牌？——存在，但**偏向新手真人**，不偏向机器人

证据在 `MakeDealByCfg` 的 Type1 分支（`zgdatbl.cpp:3993-4017`），按玩家身份三选一分配策略：

```cpp
for (int i = 0; i < 3; i++) {
    if (m_ptrPlayers[i]->IsRoboter())
        GetMakeDealCfg(&userdealCfg[i], "robot");        // 3998-4001 → 解析到 robot742/robot420
    else if (IsNeedMakeDealByUserBoutInfo(m_ptrPlayers[i]))
        GetMakeDealCfg(&userdealCfg[i], "newuser");      // 4003-4006 → 新手真人
    else
        userdealCfg[i] = dealCfg;                        // 4008-4010 → 房间默认 new/new2
}
```

逐身份对比（742/420 实际生效值）：

| 玩家身份 | 生效策略 | CouPaiStrategy | BeginMakeNum / Select | TargetValue | 是否凑炸(13) | 性质 |
|---|---|---|---|---|---|---|
| 机器人 | `robot742` / `robot420` | `[4,5,3,6]` / `[4,6,5,2,3]` | 11/15、14/17 | 999 | **否** | **= 房间策略，无差异** |
| 普通真人（局数>阈值） | `new` / `new2` | 同上 | 同上 | 999 | 否 | 与机器人完全相同 |
| **新手真人（局数≤阈值）** | **`newuser`** | **`[13,6,3,4,5,2]`** | **5 / 17** | **0.9** | **是（首码即 13）** | **送好牌：从第 6 张就干预、主动凑炸** |

> 新手阈值来自 `PlayerMakeDealRule.RoomRule`：**420 = NewUserBout 3**（`makedeal.json:395-398`）、**742 = NewUserBout 5**（`399-402`）。判定 `IsNeedMakeDealByUserBoutInfo`（`zgdatbl.cpp:4083-4113`）：玩家 `m_nBout ≤ NewUserBout` 即走 `newuser`。

**两条不平等的发牌机制（举证）：**

1. **`newuser` 送好牌（Type1 内，按玩家）**——`MakeDeal_ComposeCard` 在 `bNeedMakeDeal=true` 时按 `CouPaiStrategy` 顺序组牌，命中 `13` 即"三张+剩余第 4 张 → 升级为炸弹"（`MakeDealHelper.cpp:852-869`）。`newuser` 策略首码就是 13、`BeginMakeNum=5`（极早干预）、`TargetValue=0.9×121=108`（牌力门槛高，几乎总触发）。**结论：局数 ≤3（420）/≤5（742）的真人会被主动凑出炸弹、拿到强牌；同桌机器人和老玩家走无 13 策略，不被送炸。方向是优待新手真人。**

2. **机器人并未被优待，反而被"拉平"**——通用 `robot` 策略（`makedeal.json:123-137`）本含炸码 13（`[6,13,3,4,5,2]`）、`TargetValue=0.55`，是给机器人凑炸的"控水"档；但 742/420 配了 `robot742`/`robot420` 覆盖（138-167），把它**改成了与真人相同的无 13 策略**。所以这两房**机器人 = 普通真人**，没有机器人偏强。

> 反观其他没配 `robot<room>` 的经典房（如仍跑 `old2`/`default`），机器人会落回通用 `robot`（含 13），那才是"机器人被送炸"的形态——但**那不在 742/420 范围内**。

---

# 问题 C：不同真人人数的发牌分支矩阵

桌型由 `GetRobotCount()` 决定（`10028`）。发牌上游 `StartDeal()`（`zgdatbl.cpp:1310`）里有两层"人数相关"的前置干预，其后才是 `MakeDealByCfg`。

## C.1 三种桌型的分支差异

| 维度 | 3 真人（0 机器人） | 2 真人 + 1 机器人 | **1 真人 + 2 机器人** |
|---|---|---|---|
| `IsRobotTable()` | FALSE | TRUE | TRUE |
| 庄家决定 `CalcBanker`（`:858`） | `CalcBankerChairBefore` 随机（`:887`） | 随机（除非 INI 开 `RobotSpecialAuctionMode`） | **`isFixBankerToSoleRealPlayer=TRUE` 触发 → 强制把唯一的真人设为庄**（`:871-884`，遍历找 `m_nUserType!=ROBOT` 的椅子） |
| 新手脚本牌层 `IsNeedMakeDealForNovice`（`:1848`） | 不触发（要求恰好 1 个非机器人，`:1874`） | 不触发 | **可能触发**：唯一真人的 `m_nBout ≤ NewUserBout` 且配了 `ManualMakeDealBout`（742/420 均配=1）→ `ReadNoviceCardsFromFile()` 直接发**预制好牌**，置 `m_bIsProtected=TRUE`，**跳过 `MakeDealByCfg`**（`:1382-1389`） |
| Type1 按身份策略（`MakeDealByCfg`） | 3 个真人均按局数判 `newuser`/`new`/`new2` | 真人按局数判、机器人 `robot742`/`robot420` | 未被脚本牌接管时：真人按局数判、2 个机器人均 `robot742`/`robot420` |
| 是否"针对真人克制发牌" | 否（无机器人，无对手定向） | 否（机器人策略 = 真人策略，无克制） | **否，方向相反**——1 真人桌要么发脚本好牌、要么强制真人当庄，是**保护该真人**，不是克制 |

> 关键澄清"机器人的牌从哪来"：Type1 的发牌循环（`zgdatbl.cpp:4022-4062`）是**三家共用同一个剩余牌堆 `arrReserveCards`**（`:4020`，由开局那副随机牌扣除已发部分），按椅子轮发，每张从牌堆头/尾取出。**机器人不会单独生成一副牌**，它和真人争抢同一副残牌；差异只在"它这张牌触发拼牌时用 `robot742` 策略去残牌里挑哪张"。

## C.2 一次发牌的完整生命周期（742/420，Type1）

```
StartDeal()                                          // zgdatbl.cpp:1310
 │
 ├─ bMakeDeal = (MakeDeal["742"|"420"] 非空) = TRUE  // 1348-1357
 │
 ├─【新手保护前置门】IsNeedMakeDealForNovice()        // 1382
 │     条件：恰好 1 真人 且 该真人 m_nBout ≤ NewUserBout 且 有 ManualMakeDealBout
 │     命中 → CalcBanker(TRUE) 强制真人当庄(1384) → ReadNoviceCardsFromFile() 发预制牌(1385)
 │            → m_bIsProtected=TRUE，【下面整段 MakeDealByCfg 被跳过】(1389)
 │
 ├─ CalcBanker(FALSE)  决定庄家                        // 1391（1真人+2机器人且未走脚本时也会因 INI 强制真人当庄）
 ├─ SvrXygRandomSort(card, 54, seed)  洗整副牌        // 1404
 │
 └─ if (bMakeDeal) MakeDealByCfg(card, 54)            // 1418-1419
        │
        ├─ GetMakeDealCfg(dealCfg)  → 房间默认策略 new/new2  // 3898
        │
        ├─【Type1 分支】(nMakeDealType==1)             // 3993
        │   for 每个椅子 k∈{0,1,2}:                   // 3996
        │     IsRoboter? → robot742/robot420          // 3998
        │     else 局数≤阈值? → newuser               // 4003
        │     else → 房间 new/new2                    // 4010
        │
        ├─ arrReserveCards = 整副牌（共用残牌堆）      // 4020
        │
        └─ for p = 0..17, for k = 0..2:               // 4022 三家轮发
              ├─ p < BeginMakeNum        → 残牌头随机发 1 张     // 4026
              ├─ p < BeginSelectBanker   → SpliteCard拆牌 →       // 4032-4041
              │     MakeDeal_ComposeCard(cfg, 牌型, 残牌,
              │         手数>TargetRound || 牌力<TargetValue)
              │       └─ 按 CouPaiStrategy 顺序组牌；          // MakeDealHelper.cpp:849
              │          命中 13 且有三张+残牌第4张 → 升级成炸  // 852-869
              │     取拼出的那张牌入 hand
              ├─ p == BeginSelectBanker  → 抓 1 张底牌           // 4046
              └─ else                    → 残牌尾随机发 1 张     // 4054
        │
        └─ 回写 cards[]（17×3 手牌 + 3 底牌）         // 4071-4078
   ─────────────────────────────────────────
   回到 StartDeal：把 cards 分发到三家手牌 + 公共底牌区   // 1421+
```

---

# 一句话收口（742/420）

- **配置定位**：`MakeDeal.742="new"` / `MakeDeal.420="new2"`；机器人用 `robot742`/`robot420`（=房间策略）；新手真人用 `newuser`；外加 `PlayerMakeDealRule.RoomRule` 与 `MakeDealCommonArgs`。
- **真人不平等**：有，但只针对**新手真人**（局数≤3/≤5）——走含炸码 13 的 `newuser` 主动送好牌；机器人和普通真人在 742/420 **完全同策略、无差异**。
- **人数分支**：3 真人=纯 Type1 随机+拼牌；2 真人+1 机=同上、机器人无定向；**1 真人+2 机=可能触发"脚本好牌+强制真人当庄"的新手保护**，否则走 Type1。**没有任何分支"给机器人发克制真人的牌"。**

---

## 附：关键函数索引

| 函数 | 位置 | 作用 |
|---|---|---|
| `StartDeal` | zgdatbl.cpp:1310 | 发牌总入口；新手前置门 + 洗牌 + 调 `MakeDealByCfg` |
| `IsNeedMakeDealForNovice` | zgdatbl.cpp:1848 | 1真人桌新手脚本牌判定（恰好1非机器人 + 局数≤NewUserBout + ManualMakeDealBout） |
| `ReadNoviceCardsFromFile` | zgdatbl.cpp:1731 | 发预制好牌，置 `m_bIsProtected`，跳过 `MakeDealByCfg` |
| `CalcBanker` | zgdatbl.cpp:858 | 决定庄家；`isFixBankerToSoleRealPlayer`/`RobotSpecialAuctionMode` + 2机器人 → 强制真人当庄 |
| `MakeDealByCfg` | zgdatbl.cpp:3894 | 配置驱动做牌；Type0 走 `DoMakeDeal`，Type1 走 `MakeDeal_ComposeCard` |
| `GetMakeDealCfg` | zgdatbl.cpp:4116 | 按身份解析策略名（`robot742`/`newuser`/`new`…）并灌入 `MAKEDEALCFG` |
| `IsNeedMakeDealByUserBoutInfo` | zgdatbl.cpp:4083 | Type1 新手判定（`m_nBout ≤ NewUserBout` → 用 `newuser`） |
| `DoMakeDeal` | zgdatbl.cpp:4187 | Type0 做牌；`RobotNeedMakeDeal` ini 门控 + 首轮阈值判断 |
| `MatchFirstChairCards` | zgdatbl.cpp:4291 | Type0 庄家菜单：2王→炸→三→顺→连对→对（硬编码，一轮） |
| `MatchOtherChairCards` | zgdatbl.cpp:4324 | Type0 闲家菜单（同上） |
| `Match2OrKingCardType` | zgdatbl.h:967 | Type0 给椅塞 2/王，受 `BigCardsTo`(`nReserved[0]`) 上限 |
| `MakeDeal_ComposeCard` | MakeDealHelper.cpp:771 | Type1 拼牌核心；策略含 13 时三张+第4张成炸（852-869） |
| `GetRobotCount` / `IsRobotTable` | zgdatbl.cpp:10028 / 10018 | 机器人计数 / 是否机器人桌 |
