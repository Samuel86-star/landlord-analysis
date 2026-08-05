# 斗地主发牌核心 · 技术质量审计报告

> 审计对象：`algorithm/native/previous/zgdatbl.cpp`（11406 行）/ `zgdatbl.h` / `MakeDealHelper.cpp` / `ConfigManagerSys.*` / `makedeal.json`
> 审计日期：2026-08-05。所有扣分项带 `文件:行号` 举证。
> 关联：[742/420 发牌核心逻辑源码逆向分析](./742-420-reverse-analysis.md)。

## 1. 综合评分面板 (Scorecard Summary)

| 评估维度 | 满分 | 实际得分 | 核心扣分点摘要 |
| :--- | :--- | :--- | :--- |
| 代码架构与可维护性 | 30 | **18** | 11406 行 `CGameTable` 上帝类；Type0/Type1 双引擎挤在 `MakeDealByCfg`；玩家人数分支散落 5+ 函数；Type0 菜单序硬编码、策略名靠字符串拼接 |
| 配置文件设计与解耦度 | 25 | **16** | Type1 已配置驱动+ZMQ 热加载（亮点）；但 `TargetValue:999` 哨兵、`CouPaiStrategy` 裸码 13、`BigCardsTo→nReserved[0]` 命名断层；零校验（缺字段静默归 0、空数组直接除零） |
| 性能、并发与随机性 | 25 | **14** | 全局 `rand()`/`srand(time(NULL))` 每局重播种、非线程安全、不可复现；深链 `Json::Value` 在热重载时无读写保护；每局重解析 JSON 无缓存 |
| 控盘扩展与可审计性 | 20 | **13** | 新房加 JSON 即可（好）；但发牌明细日志被 `#ifdef _MAKEDEALINFO` 编译期屏蔽，线上不可回溯；seed 不落盘 |
| **综合总分** | **100** | **61** | **评级：存在较大技术债务** |

> 一句话：**能跑、Type1 已是配置驱动+热加载、742 有线上实测验证**——但它是典型的"长期叠加的遗产代码"：随机源过时且线程不安全、关键日志在 release 被编译掉、配置缺校验存在除零崩溃路径，离"可审计的控盘系统"还有明显差距。

---

## 2. 详细扣分项与代码/配置举证 (Detailed Deductions)

### 维度一：代码架构与可维护性（18 / 30，扣 12）

| 扣分 | 举证 |
|---|---|
| **-4 上帝类** | `zgdatbl.cpp` 单文件 **11406 行**、`CGameTable` 一个类约 150 个方法，把"发牌、牌型识别、叫庄、赖子、机器人接线、结算上报"全揉在一起，无模块边界。 |
| **-3 硬编码** | Type0 做牌菜单顺序写死在 `MatchFirstChairCards`（`zgdatbl.cpp:4299-4315`：2王→炸→三→顺→连对→对），改顺序必须重编；策略名靠字符串拼接 `"robot"+roomID`（`GetMakeDealCfg` `zgdatbl.cpp:4127`、`4140`），约定不在代码里体现；magic `7937`（`:23`）、`1001`/`101`（`:1108`/`:1142`）、layout 下标 `1/14/15`（`zgdatbl.h:969`、`:607`）裸用。 |
| **-2 双引擎挤一函数** | `MakeDealByCfg`（`:3894-4080`，186 行）用 `if(nMakeDealType==0)...else if(==1)` 把 Type0（`DoMakeDeal` 菜单）和 Type1（`MakeDeal_ComposeCard` 拼牌）两套引擎塞进一个函数，嵌套 3 层循环。 |
| **-2 玩家数分支未集中** | "1/2/3 真人"的分支逻辑分散在 `IsNeedMakeDealForNovice`（`:1848`，靠 `nPlayerForNoRobot` 数人头 `:1860`）、`CalcBanker`（`:871` 判 `GetRobotCount()==2`）、`MakeDealByCfg` Type1 逐玩家路由（`:3996`）——没有统一的"桌型→策略"决策点。 |
| **-1 双配置系统割裂** | 行为同时依赖 `makedeal.json` 和 INI：`RobotNeedMakeDeal`（`:4196`）、`RobotSpecialAuctionMode`（`:865`）、`CrazyMode`/`RazzMode`（`:1326`/`:1332`）走 `GetPrivateProfileInt`，与 JSON 配置两套体系，排查需跨两个文件。 |

### 维度二：配置文件设计与解耦度（16 / 25，扣 9）

| 扣分 | 举证 |
|---|---|
| **-3 magic number / 语义混淆** | `TargetValue:999`（`makedeal.json:104/119`）是"永远干预"的哨兵，但代码里 `999×121=120879`（`zgdatbl.cpp:4165-4168`）——字段把"比例(0.0-1.0)"和"绝对哨兵(999)"混在一个 float 里，运维不读代码根本不知道 999=∞；`CouPaiStrategy` 用裸码 `[4,6,5,2,3]`，JSON 里没有 `13=BOMB` 的枚举名，配错（如写成 `23`）静默落空。 |
| **-2 命名断层** | `BeginSelectBanker` 名字误导（实为"干预停止位+底牌槽"，非"选庄"）；`BigCardsTo` 存进 `nReserved[0]`（`zgdatbl.cpp:4164`），JSON 名与结构体槽位毫无关联，可追溯性差。 |
| **-2 非纯配置驱动** | Type1 的 `CouPaiStrategy/BeginMakeNum/TargetValue` 确实热加载（`ConfigManagerSys.cpp:35-47` ZMQ pub/sub 推送更新）——这是亮点；但 Type0 菜单序、`IsRoboter→robot`/`bout≤NewUserBout→newuser` 的**路由规则本身**写死在 C++（`:3998`/`:4003`），加新身份（如"回流玩家"）必须改代码。 |
| **-2 零合法性校验** | `GetMakeDealCfg` 对每个字段直接 `.asInt()`（`:4155-4174`），缺字段 → JsonCpp 静默返回 0 → `BeginMakeNum=0`/`TargetValue=0` 行为悄悄改变，无告警；`BeginMakeNum < BeginSelectBanker ≤ 17` 的顺序关系未校验（只各自查 `≤CARDS_PER_CHAIR`，`:4012-4016`）；无 schema、reload 时 `ParseJsonConfig` 只查 JSON 语法不查业务（`ConfigManagerSys.cpp:56-67`）。 |

### 维度三：性能、并发与随机性（14 / 25，扣 11）⚠️ 最大短板

| 扣分 | 举证 |
|---|---|
| **-7 随机源过时且不安全** | 洗牌 `SvrXygRandomSort`（`zgdatbl.h:593-602`）= `srand(seed)` 每次重播种 + `rand()%s` 随机键排序（非 Fisher-Yates）。问题三连：① **每次调用都 `srand` 重播种**（`:595`、`:23`、`:4177`），且种子是秒级 `time(NULL)`/`GetTickCount()` → **同一秒开局的多个桌次序列高度相关、可预测**；② `rand()` 是全局状态（MSVC 非 thread-local），多线程并发发牌**线程间互踩状态**；③ `s = length*1000 = 54000 > RAND_MAX(32767)`（`:597`/`:599`），`rand()%54000` 只能取到 `[0,32767]`，上半区永远不产生。**自家的 Python 模拟器都改用了 `std::mt19937`+`std::shuffle`（`include/landlord.h:1046-1047`）**，反证线上是遗产实现。 |
| **-2 并发读写竞态** | 顶层容器 `m_jsoncfgobjmgr` 是 `threadsafe_unordered_map`（`ConfigManagerSys.h:45`，且有明确告警"不要用返回迭代器" `:221`）——这点做得对；但发牌代码**直接深链遍历** `m_jsoncfgobjmgr[MAKEDEAL_CONFIG]["MakeDealStrategy"][name]["Field"]`（`zgdatbl.cpp:4155` 等几十处），而热重载时 `ProcessData` 直接整体替换该 `Json::Value`（`ConfigManagerSys.cpp:40`）→ **reader 遍历期间 value 被并发替换**，无读写锁保护，存在读-释放/迭代器失效风险。 |
| **-2 每局重解析无缓存** | `GetMakeDealCfg` 单次做 20+ 次链式 JSON 查找（`:4155-4183`），Type1 每局最多调用 4 次（房间 1 + 最多 3 个逐玩家，`:3898`/`:4001`/`:4006`），`arrCouPaiStrategy` 这个 `std::vector` 每次重建（`:4182`）→ 高并发开局下重复 JSON 树遍历 + 堆分配，无按 `(roomID,策略名)` 缓存。 |

### 维度四：控盘扩展与可审计性（13 / 20，扣 7）

| 扣分 | 举证 |
|---|---|
| **-4 线上无发牌审计日志** | 所有做牌明细日志被 `#ifdef _MAKEDEALINFO` 包裹（`zgdatbl.cpp:3944`/`3974`/`4256`、`3877`/`3878`）→ **release 编译时全部消失**。线上出"发牌不公/概率异常"客诉时，只剩 `m_nMakeDealTypes[]`（`:512`，每椅 0/1/2/3 的粗粒度标记）上报，**无法回溯当局具体怎么凑的牌**。 |
| **-2 不可复现** | 洗牌种子 `GetTickCount()+tokenID*10+socket`（`:1404`）计算了但**不落盘**；且 `srand` 重播种 + 全局状态使结果本就不可复现 → 即便加日志也无法重放某一局。 |
| **-1 扩展半开放** | 新房加 `MakeDeal[room]` + `MakeDealStrategy.<name>` 即可（好）；但加"新玩法的做牌规则"（如新的 CouPai 码、新的桌型路由）要改 `MakeDeal_ComposeCard` 的 switch 或 `MakeDealByCfg` 的路由——扩展点不在配置而在代码。 |

---

## 3. Top 3 高危风险点 (Top Technical Debts)

**①【崩溃】配置写错直接除零，崩发牌线程** —— `zgdatbl.cpp:4178-4179`
```cpp
int nCouPaiStrategyCount = ...["CouPaiStrategy"].size();   // 缺字段/空数组 → 0
int nCouPaiStrategySelectIndex = rand() % nCouPaiStrategyCount;  // rand()%0 → 整数除零 SIGFPE
```
任何新房间/新策略块漏配 `CouPaiStrategy` 或写成 `[]`，发牌线程当场崩溃。无任何防御。

**②【公平性/合规】PRNG 可预测 + 线程不安全 + 不可复现** —— `zgdatbl.h:593-602`、`zgdatbl.cpp:23/4177`
`srand(time(NULL))` 每局重播种 + 全局 `rand()`：同秒多桌序列相关、理论上可预测；多线程并发发牌互踩全局随机状态，分布被污染。对一个涉及真实计分的牌类游戏，"发牌可预测"是**客诉与合规级**风险，且因 `srand` 重播种导致**事后完全无法复现**，无法自证清白。

**③【客诉/长跑】线上发牌不可追溯 + Type0 内存泄漏** —— `zgdatbl.cpp:3944`(ifdef) + `:3971`(return 前漏 delete)
发牌明细日志 release 被编译掉 → 客诉来了查无此局；同时 Type0 路径里 `int *pReserveCards = new int[...]`（`:3907`）在 `DoMakeDeal` 返回 FALSE 时 `return`（`:3971`）**未 `delete[]`** → 长跑内存单调增长（742/420 走 Type1 不受影响，但所有 old2/Type0 房中招）。

---

## 4. 重构优化建议与范例 (Refactoring Suggestions)

针对得分最低的**维度三（随机性）**与**维度二（校验/缓存）**各给一处 Before/After。

### 重构 A：随机源——弃用 `rand/srand`，改 `thread_local mt19937` + Fisher-Yates

> 解决维度三的 -7（最大单笔扣分）与风险②。

**Before**（`zgdatbl.h:593-602` + `zgdatbl.cpp:4177-4179`）：
```cpp
// 随机键排序洗牌，srand 每次重播种，rand()%54000 超 RAND_MAX
inline void SvrXygRandomSort(int array[], int length, int seed) {
    srand(seed);                                  // 全局状态、每局重播种
    int* value = new int[length];
    int s = length * 1000;                        // 54*1000=54000 > 32767
    for (int i = 0; i < length; i++) value[i] = rand() % s;
    SvrReversalMoreByValue(array, value, length); // 非均匀：碰撞 + RAND_MAX 截断
    delete[] value;
}
// GetMakeDealCfg 里：
srand(time(NULL));                                // 又一次污染全局状态
int idx = rand() % nCouPaiStrategyCount;          // 且 nCouPaiStrategyCount==0 时除零
```

**After**：
```cpp
// 1) 线程局部、高质量 PRNG，每个线程独立状态、不被互踩
inline std::mt19937_64& ThreadRng() {
    thread_local std::mt19937_64 rng{std::random_device{}()};
    return rng;
}

// 2) 标准 Fisher-Yates，无偏、无碰撞、无堆分配
inline void SvrXygRandomSort(int array[], int length, int /*seed 不再需要*/) {
    for (int i = length - 1; i > 0; --i) {
        std::uniform_int_distribution<int> dist(0, i);
        std::swap(array[i], array[dist(ThreadRng())]);
    }
}

// 3) CouPaiStrategy 选组：顺便修除零
if (nCouPaiStrategyCount <= 0) {                  // 防御，缺配走默认、告警而非崩溃
    UwlLogFile("[MakeDeal] CouPaiStrategy empty for %s, fallback", strKey);
    pCfg->arrCouPaiStrategy = {4, 6, 5, 3};       // 安全默认
} else {
    std::uniform_int_distribution<int> dist(0, nCouPaiStrategyCount - 1);
    int idx = dist(ThreadRng());
    // ... 填 arrCouPaiStrategy
}
```
**收益**：去掉全局 `rand/srand`（线程安全 + 可统计均匀）、洗牌变 O(n) 无偏无堆分配、同时堵住风险①除零。

### 重构 B：配置层——校验 + 缓存，把"每局重解析 + 静默归零"变成"启动校验 + 命中缓存"

> 解决维度二的 -2（零校验）与维度三的 -2（每局重解析），并缓解深链并发竞态。

**Before**（`zgdatbl.cpp:4116-4184`）：每局 4 次调用 × 20+ 次链式 `["..."].asInt()`，缺字段静默 0、无范围校验、无缓存、深链在重载时裸读。

**After**（思路：解析一次→校验→缓存进 `unordered_map<strategyKey, MAKEDEALCFG>`，发牌只读快照）：
```cpp
struct MakeDealCfgCache {
    ddz::threadsafe_unordered_map<std::string, MAKEDEALCFG> cache;
    // 配置版本号，ZMQ 重载时 ProcessData 里 bump，触发懒重建
};

MAKEDEALCFG MakeDealCfgLoader::Load(const std::string& roomID,
                                    const std::string& strategyName) {
    const std::string key = roomID + "/" + strategyName;
    MAKEDEALCFG out;
    if (cfgCache.cache.find(key, out)) return out;      // 命中缓存，O(1)

    Json::Value snap;
    if (!CConfigManagerSys::GetConfigByName(MAKEDEAL_CONFIG, snap))  // 用官方 lookup，勿深链
        return Default();

    const Json::Value& s = ResolveStrategy(snap, strategyName, roomID); // 内含 robot742 回退
    out.nMakeDealType      = s.get("MakeDealType", 1).asInt();
    out.nBeginMakeNum      = s.get("BeginMakeNum", 0).asInt();
    out.nBeginSelectBanker = s.get("BeginSelectBanker", 17).asInt();
    // ...

    // 业务校验（当前代码完全缺失）
    if (out.nBeginMakeNum < 0 || out.nBeginMakeNum > 17
        || out.nBeginSelectBanker < out.nBeginMakeNum           // 顺序关系
        || out.nBeginSelectBanker > 17) {
        UwlLogFile("[MakeDeal] INVALID cfg %s: bN=%d sel=%d -> fallback",
                   key.c_str(), out.nBeginMakeNum, out.nBeginSelectBanker);
        return Default();
    }
    // CouPaiStrategy 必须非空且码值合法 {2,3,4,5,6,13}，否则风险①除零

    cfgCache.cache.insert(key, out);
    return out;
}
```
**收益**：每局发牌从"4×20 次链式 JSON 查找"降到"1 次哈希查缓存"；缺配/越界从"静默归 0 / 除零崩溃"变成"告警 + 安全默认"；深链读改为单次快照，规避重载竞态。

> 附加（维度四，低成本高收益）：把 `#ifdef _MAKEDEALINFO` 的编译期开关改成**运行时日志开关 + 必落盘字段**：每局记录 `roomID/boutID/seed/三家 m_nMakeDealTypes/最终牌型指纹`。这是堵住风险③、让发牌客诉"可回溯"的最小改动。

---

## 附：重构 A 模拟器侧对照验证（已实测）

重构 A 的 C++ 目标态已在 `algorithm/native/include/landlord.h:1044-1049`（`ShuffleDealStrategy::shuffle`，`thread_local std::mt19937` + `std::shuffle`）就位。本机随后已定位 MSVC（VS2022 Community）并据此把线上发牌逻辑 1:1 剥离为独立模拟器 [`algorithm/native/extracted/harness.cpp`](../../algorithm/native/extracted/README.md)（含递归最优拆牌，线上真值）；此处的**随机源对照验证**因聚焦 `rand/srand` 行为本身，仍在 Python 侧忠实移植遗产 `SvrXygRandomSort` 完成：脚本 [`algorithm/native/shuffle_prng_compare.py`](../../algorithm/native/shuffle_prng_compare.py)、结果与方法学 [`shuffle_prng_compare_README.md`](../../algorithm/native/shuffle_prng_compare_README.md)。

实测头条（N=50000 局；指标 C 每秒 200 桌×3600 秒×K=4 组）：

| 指标 | 遗产 `rand/srand` | 新法 MT19937+FY |
|---|---|---|
| 单局均匀性 χ²/df（df=2809，≈1 为均匀） | 0.905 | 1.025 |
| 随机键碰撞率（≥1 次并列的局占比） | **4.37%** | 0% |
| 同秒最大组占比 / distinct 组（`srand(time(NULL))` 模式） | **100% / 1** | 28.7% / 4 |

**结论**：单局内两者都接近均匀；代差在跨桌——`srand(time(NULL))` 使同一秒所有桌随机流完全相同且可预测（同秒 100% 选到同一 CouPaiStrategy 组），新法各桌独立不可预测。实测佐证风险②。
