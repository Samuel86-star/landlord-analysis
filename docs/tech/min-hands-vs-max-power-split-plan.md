# 最少手数 vs 牌力最大拆牌 等价性验证 —— 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用一个「牌力最大拆牌」穷举器（power-DFS）验证 `optimalSplit`（字典序 min-n → max Σscore）是否恒达全局最大牌力。

**Architecture:** 复用 `optimal_split.h` 的 `optdetail` 枚举/编解码，新增独立 memo 的 power-DFS，目标为 `max Σ(score − penaltyPerCombo)`；在约简空间与 `optimalSplit` 的约简牌力比较，`diff > 1e-6` 即反例。

**Tech Stack:** C++11（MSVC `/std:c++14`）、CMake、复用 `landlord.h` / `optimal_split.h`。详见规格 `docs/tech/min-hands-vs-max-power-split.md`。

## Global Constraints

- `penaltyPerCombo = 8.0`（`config/scoring.properties`，与数仓 `card_power` 同口径）；config 在程序入口 `loadScoringConfigFromFile("config/scoring.properties")` 一次，运行中不变。
- 源码 UTF-8：MSVC 必带 `/utf-8`，否则中文注释触发 C2001。
- 程序须从 `algorithm/native/` 目录运行（`loadScoringConfigFromFile` 用相对路径 `config/scoring.properties`）。
- power memo 与 `optimalSplit` 的 memo 物理隔离，互不污染。
- 不触碰线上发牌/配牌/洗牌管线（`harness.cpp` / `MakeDealByCfg` 等只读参照）。

---

## File Structure

- Create: `algorithm/native/extracted/optimal_split_power.h` —— power-DFS：`maxPowerValue` / `optimalSplitByPower` / `clearPowerMemo`。
- Create: `algorithm/native/extracted/power_split_test.cpp` —— power-DFS 单元测试（已知值 + 支配性 + 还原一致性）。
- Create: `algorithm/native/extracted/verify_split_vs_power.cpp` —— 验证 harness（随机 N 手对比 + 反例 dump）。
- Modify: `algorithm/native/CMakeLists.txt` —— 加 `split_vs_power` target。
- Output: `algorithm/native/extracted/divergence_17.jsonl`、`divergence_20.jsonl`。

---

## Task 1: power-DFS 头文件（TDD）

**Files:**

- Create: `algorithm/native/extracted/optimal_split_power.h`
- Test: `algorithm/native/extracted/power_split_test.cpp`

**Interfaces:**

- Consumes: `landlord::optdetail::{enumMoves, consumeBuf, pow5, packCounts}`（来自 `optimal_split.h`）、`landlord::{Combo, Card, scoringConfig, DefaultComboScoringStrategy, HandCardUtils}`（来自 `landlord.h`）。
- Produces: `landlord::optpower::maxPowerValue(const std::vector<Card>&) -> double`、`landlord::optpower::optimalSplitByPower(const std::vector<Card>&) -> std::vector<Combo>`、`landlord::optpower::clearPowerMemo()`。

- [ ] **Step 1: 写失败测试 `power_split_test.cpp`**

```cpp
// power_split_test.cpp —— 牌力最大拆牌器(power-DFS)校验
// 编译：vcvarsall x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fepower_split_test.exe power_split_test.cpp
#include "../include/landlord.h"
#include "optimal_split.h"
#include "optimal_split_power.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

using namespace landlord;

static double sumScore(const std::vector<Combo>& cs) {
    DefaultComboScoringStrategy sc; double t = 0; for (auto& c : cs) t += sc.score(c); return t;
}
static std::vector<Card> handFromRanks(std::initializer_list<Rank> rs) {
    std::vector<Card> h; for (auto r : rs) h.push_back({r, Suit::NONE}); return h;
}
// 拆分是否恰好消费整手牌（rank 计数对齐）
static bool splitCoversHand(const std::vector<Card>& hand, const std::vector<Combo>& split) {
    auto hc = HandCardUtils::buildRankCounts(hand);
    std::array<int, 15> sc{};
    std::array<std::pair<int, int>, 15> buf;
    for (auto& m : split) {
        int nb = optdetail::consumeBuf(m, buf);
        for (int i = 0; i < nb; ++i) sc[buf[i].first] += buf[i].second;
    }
    for (int i = 0; i < 15; ++i) if (hc[i] != sc[i]) return false;
    return true;
}

int main() {
    int fails = 0;
    double ppc = scoringConfig().penaltyPerCombo;

    // ---- 已知值：rocket-only -> maxPower = 60 - 8 = 52 ----
    {
        auto h = handFromRanks({Rank::SMALL_JOKER, Rank::BIG_JOKER});
        double mp = optpower::maxPowerValue(h);
        bool pass = std::fabs(mp - 52.0) < 1e-6;
        if (!pass) ++fails;
        printf("[%s] rocket-only maxPower=%.4f (期望 52)\n", pass ? "PASS" : "FAIL", mp);
    }
    // ---- 已知值：§1.2 {6666+7..K} -> maxPower = 55 - 16 = 39 ----
    {
        auto h = handFromRanks({Rank::SIX,Rank::SIX,Rank::SIX,Rank::SIX,
                                Rank::SEVEN,Rank::EIGHT,Rank::NINE,Rank::TEN,
                                Rank::JACK,Rank::QUEEN,Rank::KING});
        double mp = optpower::maxPowerValue(h);
        bool pass = std::fabs(mp - 39.0) < 1e-6;
        if (!pass) ++fails;
        printf("[%s] §1.2 bomb+straight maxPower=%.4f (期望 39)\n", pass ? "PASS" : "FAIL", mp);
    }

    // ---- 支配性 + 还原一致性（2000 随机手）----
    std::mt19937 rng(777);
    auto deck = Deck::fullDeck();
    int domFail = 0, coverFail = 0, consistFail = 0;
    for (int t = 0; t < 2000; ++t) {
        std::shuffle(deck.begin(), deck.end(), rng);
        int size = 17 + (t % 2) * 3;  // 17 与 20 交替
        std::vector<Card> h(deck.begin(), deck.begin() + size);

        auto opt = optimalSplit(h);
        double reduced_opt = sumScore(opt) - ppc * (int)opt.size();
        double mp = optpower::maxPowerValue(h);
        if (!(mp >= reduced_opt - 1e-9)) ++domFail;            // maxPower 恒 >= optimalSplit 约简值

        auto pw = optpower::optimalSplitByPower(h);
        if (!splitCoversHand(h, pw)) ++coverFail;              // 还原拆分须恰好消费手牌
        double reduced_pw = sumScore(pw) - ppc * (int)pw.size();
        if (std::fabs(reduced_pw - mp) > 1e-6) ++consistFail;  // 还原拆分约简值 == maxPower
    }
    bool randPass = (domFail == 0 && coverFail == 0 && consistFail == 0);
    if (!randPass) ++fails;
    printf("[%s] 随机2000手：支配违例=%d 还原未覆盖=%d 一致性违例=%d\n",
           randPass ? "PASS" : "FAIL", domFail, coverFail, consistFail);

    printf("\n汇总: %d 项失败\n", fails);
    return fails ? 1 : 0;
}
```

- [ ] **Step 2: 编译确认测试失败（power-DFS 尚未实现）**

Run（从 `algorithm/native/extracted/`，需 vcvarsall）：

```bash
cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fepower_split_test.exe power_split_test.cpp
```

Expected: 编译错误——`optimal_split_power.h` 不存在 / `optpower::maxPowerValue` 未定义。

- [ ] **Step 3: 实现 `optimal_split_power.h`**

```cpp
#pragma once
// optimal_split_power.h —— 「牌力值最大拆牌」穷举器
// 目标：max over all splits of Σ( score(combo) − penaltyPerCombo )。
//   calcTotalHandScore = Σscore − ppc·(n−1) + controlBonus；controlBonus 仅依赖整手牌、
//   ppc 为常量，故 argmax(牌力) ≡ argmax Σ(score − ppc)。约简空间比较即可，不必算 controlBonus。
// 复用 optimal_split.h 的 optdetail 枚举/编解码；独立 memo，与 optimalSplit 不串。
#include "optimal_split.h"

#include <vector>
#include <unordered_map>
#include <array>
#include <cstdint>
#include <cmath>

namespace landlord {
namespace optpower {

inline std::unordered_map<uint64_t, double>& powerMemoMap() {
    static std::unordered_map<uint64_t, double> m;
    return m;
}
inline void clearPowerMemo() { powerMemoMap().clear(); }

// best(key) = max_m [ score(m) − ppc + best(key − m) ]；base: 空 = 0
inline double solvePower(uint64_t key, const std::array<int, 15>& c) {
    int r = -1;
    for (int i = 0; i < 15; ++i) if (c[i] > 0) { r = i; break; }
    if (r < 0) return 0.0;
    auto it = powerMemoMap().find(key);
    if (it != powerMemoMap().end()) return it->second;

    static DefaultComboScoringStrategy scorer;          // 无状态，读全局 scoringConfig()
    const double ppc = scoringConfig().penaltyPerCombo;

    std::vector<std::pair<uint64_t, Combo>> moves;
    moves.reserve(64);
    optdetail::enumMoves(c, moves);

    double best = -1e18;
    std::array<std::pair<int, int>, 15> buf;
    std::array<int, 15> c2;
    const auto& p = optdetail::pow5();
    for (auto& mv : moves) {
        int nb = optdetail::consumeBuf(mv.second, buf);
        c2 = c;
        bool ok = true;
        uint64_t key2 = key;
        for (int i = 0; i < nb; ++i) {
            c2[buf[i].first] -= buf[i].second;
            if (c2[buf[i].first] < 0) { ok = false; break; }
            key2 -= (uint64_t)buf[i].second * p[buf[i].first];
        }
        if (!ok) continue;
        double val = (scorer.score(mv.second) - ppc) + solvePower(key2, c2);
        if (val > best + 1e-12) best = val;
    }
    powerMemoMap()[key] = best;
    return best;
}

// 最大约简牌力 Σ(score − ppc)
inline double maxPowerValue(const std::vector<Card>& hand) {
    auto c = HandCardUtils::buildRankCounts(hand);
    uint64_t key = optdetail::packCounts(c);
    return solvePower(key, c);
}

// 还原一个 argmax 拆分（仅在命中反例时调用以展示）
inline std::vector<Combo> optimalSplitByPower(const std::vector<Card>& hand) {
    auto c = HandCardUtils::buildRankCounts(hand);
    uint64_t key = optdetail::packCounts(c);
    solvePower(key, c);  // 填 memo

    static DefaultComboScoringStrategy scorer;
    const double ppc = scoringConfig().penaltyPerCombo;

    std::vector<Combo> out;
    std::array<int, 15> cur = c;
    uint64_t ck = key;
    std::array<std::pair<int, int>, 15> buf;
    const auto& p = optdetail::pow5();
    while (true) {
        int r = -1;
        for (int i = 0; i < 15; ++i) if (cur[i] > 0) { r = i; break; }
        if (r < 0) break;
        std::vector<std::pair<uint64_t, Combo>> moves;
        moves.reserve(64);
        optdetail::enumMoves(cur, moves);
        double bestVal = -1e18;
        uint64_t bestMc = UINT64_MAX;
        Combo bestCombo = Combo::single((Rank)r);
        for (auto& mv : moves) {
            int nb = optdetail::consumeBuf(mv.second, buf);
            std::array<int, 15> c2 = cur;
            bool ok = true;
            uint64_t key2 = ck;
            for (int i = 0; i < nb; ++i) {
                c2[buf[i].first] -= buf[i].second;
                if (c2[buf[i].first] < 0) { ok = false; break; }
                key2 -= (uint64_t)buf[i].second * p[buf[i].first];
            }
            if (!ok) continue;
            auto it = powerMemoMap().find(key2);
            double rest = (it != powerMemoMap().end()) ? it->second : solvePower(key2, c2);
            double val = (scorer.score(mv.second) - ppc) + rest;
            if (val > bestVal + 1e-12 || (std::fabs(val - bestVal) <= 1e-12 && mv.first < bestMc)) {
                bestVal = val; bestMc = mv.first; bestCombo = mv.second;
            }
        }
        out.push_back(bestCombo);
        int nb = optdetail::consumeBuf(bestCombo, buf);
        for (int i = 0; i < nb; ++i) {
            cur[buf[i].first] -= buf[i].second;
            ck -= (uint64_t)buf[i].second * p[buf[i].first];
        }
    }
    return out;
}

} // namespace optpower
} // namespace landlord
```

- [ ] **Step 4: 编译并运行测试，确认全 PASS**

Run（从 `algorithm/native/extracted/`）：

```bash
cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fepower_split_test.exe power_split_test.cpp
power_split_test.exe
```

Expected: 4 项全 `[PASS]`，`汇总: 0 项失败`，退出码 0。

- [ ] **Step 5: 提交**

```bash
git add algorithm/native/extracted/optimal_split_power.h algorithm/native/extracted/power_split_test.cpp
git commit -m "feat(split): 牌力最大拆牌 power-DFS (max Σ(score−ppc)) + 单测"
```

---

## Task 2: 验证 harness + 小样本冒烟

**Files:**

- Create: `algorithm/native/extracted/verify_split_vs_power.cpp`

**Interfaces:**

- Consumes: `optimalSplit`（`optimal_split.h`）、`optpower::maxPowerValue` / `optpower::optimalSplitByPower`（Task 1）、`landlord.h` 评分工具。

- [ ] **Step 1: 写 `verify_split_vs_power.cpp`**

```cpp
// verify_split_vs_power.cpp —— 最少手数拆牌 vs 牌力最大拆牌 等价性验证
// 编译：vcvarsall x64 && cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fesplit_vs_power.exe verify_split_vs_power.cpp
// 运行：须在 algorithm/native/ 目录（相对路径 config/scoring.properties）
#include "../include/landlord.h"
#include "optimal_split.h"
#include "optimal_split_power.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <random>
#include <algorithm>

using namespace landlord;

static double sumScore(const std::vector<Combo>& cs) {
    DefaultComboScoringStrategy sc; double t = 0; for (auto& c : cs) t += sc.score(c); return t;
}
static const char* rk(Rank r) {
    switch (r) { case Rank::TWO: return "2"; case Rank::SMALL_JOKER: return "s"; case Rank::BIG_JOKER: return "b";
        default: static char buf[2];
            // 3..A 用 3,4,..,9,T,J,Q,K,A
            switch (r) { case Rank::TEN: return "T"; case Rank::JACK: return "J"; case Rank::QUEEN: return "Q";
                case Rank::KING: return "K"; case Rank::ACE: return "A";
                default: buf[0] = '3' + (int)r; buf[1] = 0; return buf; } }
}
static const char* tp(ComboType t) {
    switch (t) { case ComboType::SINGLE: return "S"; case ComboType::PAIR: return "P"; case ComboType::TRIPLE: return "T";
        case ComboType::TRIPLE_WITH_SINGLE: return "T1"; case ComboType::TRIPLE_WITH_PAIR: return "T2";
        case ComboType::STRAIGHT: return "ST"; case ComboType::CONSECUTIVE_PAIRS: return "CP"; case ComboType::PLANE: return "PL";
        case ComboType::PLANE_WITH_SINGLES: return "PL1"; case ComboType::PLANE_WITH_PAIRS: return "PL2";
        case ComboType::QUAD_WITH_TWO_SINGLES: return "Q1"; case ComboType::QUAD_WITH_TWO_PAIRS: return "Q2";
        case ComboType::BOMB: return "B"; case ComboType::ROCKET: return "R"; }
    return "?";
}
static std::string handStr(const std::vector<Card>& h) {
    auto cc = HandCardUtils::buildRankCounts(h);
    std::string s; for (int i = 14; i >= 0; --i) for (int k = 0; k < cc[i]; ++k) s += rk((Rank)i);
    return s;
}
static std::string splitStr(const std::vector<Combo>& cs) {
    std::string s;
    for (auto& c : cs) {
        s += tp(c.type); s += "[";
        for (auto r : c.mainRanks) s += rk(r);
        s += "]";
        if (!c.wingRanks.empty()) { s += "+"; for (auto r : c.wingRanks) s += rk(r); }
        s += " ";
    }
    return s;
}

// 对单一手牌规模跑 N 手，返回分歧数；反例写 jsonl
static long runSize(int size, long N, unsigned seed, double ppc, const char* outPath) {
    std::mt19937 rng(seed);
    auto deck = Deck::fullDeck();
    FILE* f = std::fopen(outPath, "w");
    long diverge = 0; double maxDiff = 0.0; long nBigger = 0;
    for (long t = 0; t < N; ++t) {
        std::shuffle(deck.begin(), deck.end(), rng);
        std::vector<Card> h(deck.begin(), deck.begin() + size);

        auto opt = optimalSplit(h);
        double S_opt = sumScore(opt);
        double reduced_opt = S_opt - ppc * (double)opt.size();

        double mp = optpower::maxPowerValue(h);
        double diff = mp - reduced_opt;
        if (!(mp >= reduced_opt - 1e-9)) {
            std::printf("[SANITY-FAIL] maxPower < reduced_opt! size=%d t=%ld diff=%.6f\n", size, t, diff);
        }
        if (diff > maxDiff) maxDiff = diff;
        if (diff > 1e-6) {
            ++diverge;
            auto pw = optpower::optimalSplitByPower(h);
            double S_pw = sumScore(pw);
            int n_opt = (int)opt.size(), n_pw = (int)pw.size();
            if (n_pw > n_opt) ++nBigger;  // 机制验证：最大牌力拆用更多手数
            if (f) std::fprintf(f,
                "{\"t\":%ld,\"hand\":\"%s\",\"opt\":\"%s\",\"power\":\"%s\","
                "\"n_opt\":%d,\"n_pw\":%d,\"S_opt\":%.4f,\"S_pw\":%.4f,"
                "\"reduced_opt\":%.4f,\"maxPower\":%.4f,\"diff\":%.6f}\n",
                t, handStr(h).c_str(), splitStr(opt).c_str(), splitStr(pw).c_str(),
                n_opt, n_pw, S_opt, S_pw, reduced_opt, mp, diff);
        }
    }
    if (f) std::fclose(f);
    std::printf("size=%2d  N=%ld  分歧=%ld (%.4f%%)  n_pw>n_opt=%ld  maxDiff=%.6f  -> %s\n",
                size, N, diverge, 100.0 * diverge / N, nBigger, maxDiff, outPath);
    return diverge;
}

int main() {
    loadScoringConfigFromFile("config/scoring.properties");
    double ppc = scoringConfig().penaltyPerCombo;
    std::printf("penaltyPerCombo=%.2f\n", ppc);

    const long N = (std::getenv("VSP_N") ? std::atol(std::getenv("VSP_N")) : 50000);
    long d17 = runSize(17, N, 12345, ppc, "extracted/divergence_17.jsonl");
    long d20 = runSize(20, N, 12345, ppc, "extracted/divergence_20.jsonl");

    std::printf("\n===== 结论 =====\n");
    if (d17 == 0 && d20 == 0) std::printf("0 分歧：最少手数拆牌恒达最大牌力（命题成立）\n");
    else std::printf("存在分歧（17:%ld, 20:%ld）：命题不成立，见 divergence_*.jsonl\n", d17, d20);
    return 0;
}
```

- [ ] **Step 2: 编译**

Run（从 `algorithm/native/`）：

```bash
cl /nologo /utf-8 /EHsc /std:c++14 /O2 /Fesplit_vs_power.exe extracted\verify_split_vs_power.cpp
```

Expected: 生成 `split_vs_power.exe`，无错误。

- [ ] **Step 3: 小样本冒烟（N=1000）**

Run（从 `algorithm/native/`）：

```bash
VSP_N=1000 ./split_vs_power.exe   # PowerShell: $env:VSP_N=1000; .\split_vs_power.exe
```

Expected: 打印两行 `size=17` / `size=20` 汇总；`divergence_17.jsonl` / `divergence_20.jsonl` 生成（可能为空文件）；无 `[SANITY-FAIL]`。

- [ ] **Step 4: 提交**

```bash
git add algorithm/native/extracted/verify_split_vs_power.cpp
git commit -m "feat(verify): 最少手数 vs 牌力最大拆牌 对比 harness"
```

---

## Task 3: CMake target + 全量运行 + 结论归档

**Files:**

- Modify: `algorithm/native/CMakeLists.txt`
- Update: `docs/tech/min-hands-vs-max-power-split.md`（结论段落）

**Interfaces:**

- Consumes: Task 1（power-DFS）、Task 2（harness）。

- [ ] **Step 1: 加 CMake target**

在 `algorithm/native/CMakeLists.txt` 末尾追加：

```cmake
add_executable(split_vs_power extracted/verify_split_vs_power.cpp)
```

- [ ] **Step 2: 全量运行（N=50000）**

Run（从 `algorithm/native/`）：

```bash
$env:VSP_N = "50000"; .\split_vs_power.exe
```

Expected: `size=17 N=50000` 与 `size=20 N=50000` 两行汇总 + 结论块；记录分歧数/率/maxDiff。

- [ ] **Step 3: 归档结论到规格文档**

根据运行结果，在 `docs/tech/min-hands-vs-max-power-split.md` 末尾追加「## 八、验证结果（YYYY-MM-DD）」段落：填入 17/20 分歧数与率、`maxDiff`；若存在反例，摘 1–3 条典型（手牌 + 两套拆分 + `n_opt`/`n_pw`）并给出机制解释（`n_pw > n_opt` 且 `Σscore` 增量 `> ppc·Δn`）。

- [ ] **Step 4: 提交**

```bash
git add algorithm/native/CMakeLists.txt docs/tech/min-hands-vs-max-power-split.md
git commit -m "chore(verify): split_vs_power CMake target + 全量结论归档"
```

---

## Self-Review

- **Spec coverage**：规格 §4.1（power-DFS 头）→ Task 1；§4.2（harness）→ Task 2；§4.3 正确性（支配/独立 memo/约简比较）→ Task 1 测试覆盖支配性与还原一致性；§5 参数（N=50000、17/20、seed 12345、`scoring.properties`）→ Task 2/3；§6 编译（cl + CMake）→ Task 2/3；§7 判定 → Task 3 Step 3。无遗漏。
- **Placeholder scan**：无 TBD/TODO；所有代码块为完整可编译代码；已知值 52/39 已手算确认。
- **Type consistency**：`optpower::maxPowerValue` / `optimalSplitByPower` / `clearPowerMemo` 在 Task 1 定义、Task 2 消费，签名一致；`optdetail::consumeBuf` 复用自 `optimal_split.h`，参数 `std::array<std::pair<int,int>,15>&` 一致。
