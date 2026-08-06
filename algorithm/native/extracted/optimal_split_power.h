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
