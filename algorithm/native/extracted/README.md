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
   {"seat":0,"is_robot":false,"hand":[53,6,...17张],"bombs":0,"handcount":6,"bigcards":2,"makedeal":3},
   {"seat":1,...},{"seat":2,...}],
 "bottom":[0,16,50]}
```

字段：`banker`=庄家座位；每家 `hand`(17 张 cardid，stride-3 取自 `card[c+3*i]`)、`bombs`(持有炸弹=quads+rocket)、
`handcount`(Type0 手数口径)、`bigcards`(2/王张数)、`makedeal`(做牌类型 0/1/2/3)；`bottom`(3 张底牌)。

## 聚合统计

```bash
py -3 stats.py out_742_3.jsonl out_420_3.jsonl
# 或单文件
py -3 stats.py out_742_3.jsonl
```

打印每组配置的真人/机器人炸弹均值与分布、手数、大牌、做牌类型分布、庄家为真人比率。

## 忠实度（1:1 verbatim 部分）

`SvrXygRandomSort`/`SvrReversalMoreByValue`、`MakeDealByCfg`(Type0+Type1)、`DoMakeDeal`、
`MatchFirst/OtherChairCards`+6 个 `Match*`、`MakeDeal_ComposeCard`(全分支含 3/4/13/14 位置特判与原笔误)、
`SpliteCard`+`GetBestCardType`+`get_MaxHandCardValue`(递归最优拆牌)、`get_GroupData`(读 JSON GroupDataExp)、
`CalHandCardValue`、`CalcHandCardsCount`+8 个 `Calc*HandCount`、`GetMakeDealCfg`(含 robot/newuser/room 路由)、
`CalcBanker`——全部逐字照抄，含 `rand()`/`srand()` 与 `s=54000>RAND_MAX` 截断。

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
