# 重构 A 对照验证：遗产 `SvrXygRandomSort` vs 新法 MT19937+Fisher-Yates

> 脚本：[`shuffle_prng_compare.py`](./shuffle_prng_compare.py)　运行：`py -3 -u shuffle_prng_compare.py -n 50000 --tables 200 --seconds 3600 --K 4`
> 关联：审计报告 [`docs/makedeal-strategies/makedeal-code-quality-audit.md`](../../docs/makedeal-strategies/makedeal-code-quality-audit.md)【维度三】与风险②。
> 背景：C++ 目标态（重构 A）已在 [`include/landlord.h:1044-1049`](./include/landlord.h) 落地（`thread_local std::mt19937` + `std::shuffle`）；本机无 C++ 工具链，故在 Python 侧忠实移植并对照。

## 验证方法（忠实移植，不美化不夸大）

- **Before（遗产）**：逐行移植 `zgdatbl.h:593-602` 的 `SvrXygRandomSort`
  - MSVC `rand()` LCG：`state=(state*214013+2531011) mod 2^32; return (state>>16)&0x7fff`，`RAND_MAX=32767`
  - `srand(seed)` 每次重播种；每张牌键 = `rand()%(length*1000)`（54 张时 s=54000 **> RAND_MAX** → `[32768,53999]` 永不产生）
  - 按键稳定排序（并列时保持原序，近似 `SvrReversalMoreByValue` 对并列键的位置依赖）
- **After（新法）**：`random.shuffle`（= MT19937 + Fisher-Yates），等价 `landlord.h` 目标态
- 指标 C 复刻生产里 `srand(time(NULL)); rand()%K` 模式（`zgdatbl.cpp:4177-4179` CouPaiStrategy 选组）

## 结果（N=50000 局 / 指标 C：每秒 200 桌 × 3600 秒 × K=4 组）

| 指标 | 遗产 `SvrXygRandomSort` | 新法 MT19937+FY | 说明 |
|---|---|---|---|
| **A 单局均匀性** χ²/df（df=2809，越接近 1 越均匀） | **0.905**（χ²=2542.5） | **1.025**（χ²=2880.4） | 两者都接近均匀；遗产略欠分散（见下） |
| **B 随机键碰撞率**（≥1 次并列的局占比） | **4.37%** | **0%** | 与理论 C(54,2)/32768≈0.0437 对/局 吻合 |
| **C 同秒最大组占比 / distinct 组** | **100% / 1.00** | **28.68% / 4.00** | 遗产：同秒所有桌同一组、且可预测；新法 ~1/K 均匀分散 |

## 结论（与审计一致）

1. **单局内两者都接近均匀（A）**——所以遗产法不是"每局明显有偏"，不能拿单局卡方去吓人。遗产的轻微不均来自键碰撞（B，~4.4% 局）与低熵连续播种下的流相关性（A 的欠分散暗示：相邻 seed 的 LCG 首输出相关）。
2. **真正的代差在 C**：`srand(time(NULL))` 每次重播种使**同一秒内所有桌的 `rand()` 流完全相同且是当前秒的确定函数** → 200 桌 100% 选到同一个 CouPaiStrategy 组，跨桌可预测。MT19937 + `random_device` 高熵播种彻底消除该碰撞（最大组占比 ≈1/K、distinct≈K）。
3. 这正是审计【风险②】（PRNG 可预测 + 不可复现）的实测佐证；目标态已在 `landlord.h` 就位，线上 `previous/zgdatbl.*` 待替换。
