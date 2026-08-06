# extracted/ — 斗地主发牌独立模拟器（Standalone Harness）

把线上 `previous/zgdatbl.cpp/.h` + `MakeDealHelper.cpp/.h` 里**发牌/配牌/拆牌/洗牌/随机**的真实 C++
逻辑 1:1 原样剥离，去掉网络/DB/CGameTable 上帝类依赖，编译为**独立可执行**，用于 100% 物理级精确的
发牌概率统计。这是**线上发牌的唯一可信真值来源**（贪心拆牌的 Python 模拟只适合方向性探索）。

## 文件

| 文件 | 说明 |
|---|---|
| `harness.cpp` | 单文件模拟器：发牌逻辑逐字剥离 + 极简 JSON + 极简 Table/Player/Config stub + JSONL 输出 |
| `stats.py` | 聚合 harness 产出的 `*.jsonl`，打印炸弹/手数/大牌/做牌类型分布（按真人/机器人拆分） |
| `makedeal.json` | **不在此目录**——运行时 `--cfg ../previous/makedeal.json` 指向只读快照里的原配置 |
| `*.jsonl` | harness 运行产出（样本数据，已 gitignore，可再生） |

## 编译

需要 MSVC（本机已装 VS2022 Community + MSVC 14.44）。也可用 g++。

```powershell
# MSVC（vcvarsall + cl；/utf-8 必须，否则中文按 GBK 误读报 C2001）
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
cmd /c "`"$vcvars`" x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Feharness.exe harness.cpp"
```
```bash
# g++
g++ -std=c++14 -O2 harness.cpp -o harness
```

## 运行

```bash
# 742、3 真人、10 万局，输出 JSONL
./harness --room 742 --reals 3 -n 100000 --seed 1 --cfg ../previous/makedeal.json > out_742_3.jsonl
```

| 参数 | 含义 | 默认 |
|---|---|---|
| `--room <id>` | 房间号（742/420） | 742 |
| `--reals <1\|2\|3>` | 真人玩家数（其余位机器人） | 3 |
| `-n <count>` | 模拟局数 | 100000 |
| `--seed <uint>` | 洗牌种子基数（每局 = seed + deal*7919） | 0 |
| `--cfg <path>` | makedeal.json 路径 | makedeal.json |

## 输出格式（JSON Lines，一行一局）

```json
{"deal":0,"room":742,"reals":3,"banker":2,
 "seats":[
   {"seat":0,"is_robot":false,"hand":[53,6,...17张],"bombs":0,"handcount":6,"bigcards":2,"makedeal":3,
    "opt_hands":6,"value":14,"singles":3,"split_bombs":0,"gtypes":{"0":3,"1":1,...},"val_f":14.3},
   {"seat":1,...},{"seat":2,...}],
 "gap_val":0.5,"gap_bomb":0.0,"spread":12.3,"bottom":[0,16,50]}
```

字段：`banker`=庄家座位（首叫位，非地主、不吃底牌）；每家 `hand`(17 张 cardid，stride-3 取自 `card[c+3*i]`)、
`bombs`(持有炸弹=quads+rocket，**与线上 `bomb_cnt` 对齐**)、`handcount`(Type0 手数口径，对照)、`bigcards`(2/王张数)、`makedeal`(做牌类型 0/1/2/3)；
**指标期拆牌字段**（来自 `optimal_split.h` 搜索式最优拆牌，min-combo→max-Σscore）：`opt_hands`(人均最优手数)、`value`/`val_f`(牌力，int/float)、`singles`(单牌数)、`split_bombs`(拆牌炸弹，偏低仅参考)、`gtypes`(牌型直方图)。
局级：`gap_val`/`gap_bomb`(庄家−闲家均值，17 张口径)、`spread`(三座位牌力极差)、`bottom`(3 张底牌，不并入任何座位)。

> 指标口径与统计方法见 [`docs/knowledge/makedeal-simulation.md`](../../../docs/knowledge/makedeal-simulation.md)。
> 扫描/打分/TOP20 用 `sweep.py`，单配置聚合用 `anchor_check.py`，最优拆牌校验用 `split_test.cpp`。

## 聚合统计

```bash
py -3 stats.py out_742_3.jsonl out_420_3.jsonl
# 或单文件
py -3 stats.py out_742_3.jsonl
```

打印每组配置的真人/机器人炸弹均值与分布、手数、大牌、做牌类型分布、庄家为真人比率。

## 忠实度（1:1 verbatim 部分）

`SvrXygRandomSort`/`SvrReversalMoreByValue`、`MakeDealByCfg`(Type0+Type1)、`DoMakeDeal`、
`MatchFirst/OtherChairCards`+6 个 `Match*`、`MakeDeal_ComposeCard`(全分支含 3/4/13/14 位置特判；mc==4 连对 `6*prov*2` 笔误已修，见审计 B5)、
`SpliteCard`+`GetBestCardType`+`get_MaxHandCardValue`(递归最优拆牌)、`get_GroupData`(读 JSON GroupDataExp)、
`CalHandCardValue`、`CalcHandCardsCount`+8 个 `Calc*HandCount`、`GetMakeDealCfg`(含 robot/newuser/room 路由)、
`CalcBanker`——逐字照抄。`GetMakeDealCfg` 原 `srand(time(NULL))` 已删（审计 B3，偏离 1:1）；洗牌 `rand()` 与 `s=54000>RAND_MAX` 截断原样保留（审计 B4）。

**仅 stub 部分**（不发牌逻辑）：`CPlayer`/`CGameTable` 精简到只留发牌所需成员；`CConfigManagerSys` 换成
`std::map`；`GetPrivateProfileInt` 走默认值；`UwlLogFile` no-op；`_T/TCHAR/CString/_stprintf` 映射到标准 C++；
极简递归下降 JSON 解析器（替代 JsonCpp，零外部依赖）。

## 已知校准结论（N=10000/组，seed=1）

| 房间(策略) | 桌型 | 真人炸弹 | 真人手数 |
|---|---|---|---|
| 742 (new `[4,5,3,6]` b11 sel15) | 3真人 | 0.137 | 6.26 |
| 420 (new2 `[4,6,5,2,3]` b14 sel17) | 3真人 | 0.138 | 6.37 |

- 742/420 炸弹率**几乎相同**（0.137 vs 0.138）——线上真值**否定了**之前贪心模拟里的"对(2)顶炸悬崖"（那是贪心拆牌的伪影）。
- 742 持有炸弹 0.137 与线上实测 **打出** `bomb_bet` 单家 ≈0.13 自洽（持有略高于打出）→ harness 忠实。

## 局限

- 洗牌种子：线上用 `GetTickCount()+tokenID*10+socket`，harness 用 `seed+deal*7919`（可复现、每局不同）。
  种子源不同 → **具体某局不可复现线上**；但**概率分布忠实**（同一 shuffle 算法），统计结论有效。
- 新手脚本牌层（`ReadNoviceCardsFromFile`）未含（需外部预制牌文件）；newuser **策略层**已含。
- 配置加载后不热更新（沙盒静态读 `makedeal.json`）。

## 审计发现与修复（2026-08 静态审计）

对 `harness.cpp` 做了一轮纯逻辑/边界审计，区分为 **A 类（线上共有）** 与 **B 类（harness stub 引入）**。处理见下表——A 类中 **B3 经确认按「修」处理（标注「偏离 1:1」，仅影响策略选型随机源，不动洗牌/发牌算法体）；B4 保留线上洗牌 bias 以守住「线上真值」定位**。**742/420 标定结论不受影响。**

| 编号 | 类别 | 处理 | 说明 |
|---|---|---|---|
| B2 | B·已修 | 代码 | `m_nMakeDealTypes` 原为未初始化裸数组（stub 丢失了线上 `CGameTable` 的构造清零），Type0 首局 / Match 未命中时读出垃圾或跨局残留，污染 `makedeal` 字段。已加类内 `={0,0,0}` + 每局循环重置。**仅动该元数据字段，不碰发牌算法体**；Type1(742/420) 因 compose 阶段必写全 3 位为 3，输出字节不变。 |
| B1 | A·加固 | 代码 | `GetMakeDealCfg` 中 `rand() % nCouPaiStrategyCount` 在 `CouPaiStrategy` 缺失/为空时整数除零崩溃（线上既有缺陷）。已加 `if (nCouPaiStrategyCount<=0) return;`，仅畸形配置触发，正常配置零影响，不破坏 1:1 统计等价性。 |
| B3 | A·已修(偏离1:1) | 代码 | `GetMakeDealCfg` 原有 `srand(time(NULL))`（秒级粒度）：线上每局间隔秒级无碍，但 harness 紧循环同 1 秒内数千局会选同一 `CouPaiStrategy`，致多策略房间（old2/robot/level1-6 等）策略分层偏斜。**已删除该 srand**，改由每局 `SvrXygRandomSort` 的 `srand(shuffleSeed)` 提供按局变化的随机源 → 更贴合线上「每局策略独立」的统计意图。**742(`new`)/420(`new2`) 单策略，输出不变。** 属对线上字面代码的偏离，不动洗牌/发牌算法体。 |
| B4 | A·保留 | 文档 | `SvrXygRandomSort` 中 `s=length*1000=54000>RAND_MAX(32767)`，`rand()%54000≡rand()`，洗牌键值域压缩、碰撞处保原序，存在轻微系统性洗牌偏置。**属线上既有行为，原样保留**——改它会改变洗牌分布、使已标定真人炸弹率 0.137 失效并动摇「线上真值来源」定位。 |
| B5 | A·已修 | 代码 | `MakeDeal_ComposeCard` 连对 mc==4 第二分支 `6*prov*2`（应为 `6+prov*2`）原笔误，已修正。该 `nCount` 不进 `CalHandCardValue`（在其之前重拆）、不被 Type1 下游消费，**修正前后零输出影响**，仅统一元数据。 |
| B6 | B·已修 | 代码 | harness `DoMakeDeal` 原删去了线上储备耗尽 `return FALSE` 兜底，**已恢复**。该分支不可达（储备牌数恰好 = 总空槽数，精确耗尽、永不超取），零输出影响，恢复后重新对齐线上 1:1。 |
