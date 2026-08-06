#pragma once
// optimal_split.h —— 斗地主「搜索式全局最优拆牌器」
//
// 目标（字典序）：(1) 最小组合数 n；(2) 等 n 中最大 Σscore(combo)。
//   - min-n 居首：用户定义「手数 = 最小组合数」；纯 max-score 会被否（它拒绝组低对子，
//     如两张 3：对子分 −12 < 两单牌 0，误判 n=2），污染手数/单牌。
//   - max-Σscore 破平：§1.2「全局牌力最优」——6666+7..K 下 BOMB(6)+STRAIGHT(7..K)(55)
//     胜过 STRAIGHT(6..K)+TRIPLE(6)(24)，炸弹不被长顺吞没。
//
// 算法：记忆化穷举 DFS。状态 = 15 点数计数向量；memo key = base-5 打包(uint64)。
//   每步取「最低非空点 r」，枚举 r 的所有消费方式（作主位 + 作翼位）——保证完备且不重复。
//   复用 landlord.h 的 Combo/Card/Rank/ComboType 与 DefaultComboScoringStrategy::score。
//
// 仅用于「指标期」拆牌；不影响线上发牌/配牌/洗牌管线（MakeDealByCfg/SpliteCard 等）。
// 复用同一手牌时 controlBonus 与拆牌无关，正确地被排除于优化、由 harness 指标期的
// calcTotalHandScore 加回。

#include "../include/landlord.h"

#include <vector>
#include <unordered_map>
#include <array>
#include <cstdint>
#include <utility>
#include <cmath>

namespace landlord {
namespace optdetail {

inline const std::array<uint64_t, 15>& pow5() {
    static std::array<uint64_t, 15> p = [] {
        std::array<uint64_t, 15> a{};
        uint64_t v = 1;
        for (int i = 0; i < 15; ++i) { a[i] = v; v *= 5ULL; }
        return a;
    }();
    return p;
}

inline uint64_t packCounts(const std::array<int, 15>& c) {
    const auto& p = pow5();
    uint64_t k = 0;
    for (int i = 0; i < 15; ++i) k += (uint64_t)c[i] * p[i];
    return k;
}

// ---- moveCode 编/解码（28 位：type|mainStart|mainLen|wingMask）----
inline uint64_t encodeMove(int type, int mainStart, int mainLen, uint32_t wingMask) {
    return ((uint64_t)(type & 0xF) << 24)
         | ((uint64_t)(mainStart & 0x1F) << 19)
         | ((uint64_t)(mainLen & 0xF) << 15)
         | (uint64_t)(wingMask & 0x7FFF);
}
inline int      mcType(uint64_t mc)      { return (int)((mc >> 24) & 0xF); }
inline int      mcMainStart(uint64_t mc) { return (int)((mc >> 19) & 0x1F); }
inline int      mcMainLen(uint64_t mc)   { return (int)((mc >> 15) & 0xF); }
inline uint32_t mcWingMask(uint64_t mc)  { return (uint32_t)(mc & 0x7FFF); }

inline uint64_t encodeMove(const Combo& m) {
    uint32_t wm = 0;
    for (auto r : m.wingRanks) wm |= (1u << (int)r);
    int ms = m.mainRanks.empty() ? 0 : (int)m.mainRanks[0];
    int ml = (int)m.mainRanks.size();
    return encodeMove((int)m.type, ms, ml, wm);
}

inline Combo decodeMove(uint64_t mc) {
    int type = mcType(mc);
    int ms = mcMainStart(mc);
    int ml = mcMainLen(mc);
    uint32_t wm = mcWingMask(mc);
    std::vector<Rank> wings;
    for (int i = 0; i < 15; ++i) if (wm & (1u << i)) wings.push_back((Rank)i);
    auto rangeRanks = [](int s, int L) {
        std::vector<Rank> r; r.reserve(L);
        for (int i = 0; i < L; ++i) r.push_back((Rank)(s + i));
        return r;
    };
    switch ((ComboType)type) {
        case ComboType::SINGLE:                return Combo::single((Rank)ms);
        case ComboType::PAIR:                  return Combo::pair((Rank)ms);
        case ComboType::TRIPLE:                return Combo::triple((Rank)ms);
        case ComboType::TRIPLE_WITH_SINGLE:    return Combo::tripleWithSingle((Rank)ms, wings[0]);
        case ComboType::TRIPLE_WITH_PAIR:      return Combo::tripleWithPair((Rank)ms, wings[0]);
        case ComboType::STRAIGHT:              return Combo::straight(rangeRanks(ms, ml));
        case ComboType::CONSECUTIVE_PAIRS:     return Combo::consecutivePairs(rangeRanks(ms, ml));
        case ComboType::PLANE:                 return Combo::plane(rangeRanks(ms, ml));
        case ComboType::PLANE_WITH_SINGLES:    return Combo::planeWithSingles(rangeRanks(ms, ml), wings);
        case ComboType::PLANE_WITH_PAIRS:      return Combo::planeWithPairs(rangeRanks(ms, ml), wings);
        case ComboType::QUAD_WITH_TWO_SINGLES: return Combo::quadWithTwoSingles((Rank)ms, wings[0], wings[1]);
        case ComboType::QUAD_WITH_TWO_PAIRS:   return Combo::quadWithTwoPairs((Rank)ms, wings[0], wings[1]);
        case ComboType::BOMB:                  return Combo::bomb((Rank)ms);
        case ComboType::ROCKET:                return Combo::rocket();
    }
    return Combo::single((Rank)ms);
}

// 把 Combo 的消费填入 buf(rank,count)，返回项数
inline int consumeBuf(const Combo& m, std::array<std::pair<int, int>, 15>& buf) {
    int n = 0;
    auto add = [&](int rk, int cnt) { buf[n++] = {rk, cnt}; };
    switch (m.type) {
        case ComboType::SINGLE:                add((int)m.mainRanks[0], 1); break;
        case ComboType::PAIR:                  add((int)m.mainRanks[0], 2); break;
        case ComboType::TRIPLE:                add((int)m.mainRanks[0], 3); break;
        case ComboType::TRIPLE_WITH_SINGLE:    add((int)m.mainRanks[0], 3); add((int)m.wingRanks[0], 1); break;
        case ComboType::TRIPLE_WITH_PAIR:      add((int)m.mainRanks[0], 3); add((int)m.wingRanks[0], 2); break;
        case ComboType::STRAIGHT:              for (auto r : m.mainRanks) add((int)r, 1); break;
        case ComboType::CONSECUTIVE_PAIRS:     for (auto r : m.mainRanks) add((int)r, 2); break;
        case ComboType::PLANE:                 for (auto r : m.mainRanks) add((int)r, 3); break;
        case ComboType::PLANE_WITH_SINGLES:    for (auto r : m.mainRanks) add((int)r, 3); for (auto r : m.wingRanks) add((int)r, 1); break;
        case ComboType::PLANE_WITH_PAIRS:      for (auto r : m.mainRanks) add((int)r, 3); for (auto r : m.wingRanks) add((int)r, 2); break;
        case ComboType::QUAD_WITH_TWO_SINGLES: add((int)m.mainRanks[0], 4); add((int)m.wingRanks[0], 1); add((int)m.wingRanks[1], 1); break;
        case ComboType::QUAD_WITH_TWO_PAIRS:   add((int)m.mainRanks[0], 4); add((int)m.wingRanks[0], 2); add((int)m.wingRanks[1], 2); break;
        case ComboType::BOMB:                  add((int)m.mainRanks[0], 4); break;
        case ComboType::ROCKET:                add((int)Rank::SMALL_JOKER, 1); add((int)Rank::BIG_JOKER, 1); break;
    }
    return n;
}

// 从 pool 选 k 的所有组合，cb(vector<int> 选中元素)
template <class F>
void forEachCombination(const std::vector<int>& pool, int k, F&& cb) {
    int n = (int)pool.size();
    if (k <= 0 || k > n) {
        if (k == 0) { std::vector<int> empty; cb(empty); }
        return;
    }
    std::vector<int> idx(k);
    for (int i = 0; i < k; ++i) idx[i] = i;
    std::vector<int> chosen(k);
    while (true) {
        for (int i = 0; i < k; ++i) chosen[i] = pool[idx[i]];
        cb(chosen);
        int i = k - 1;
        while (i >= 0 && idx[i] == n - k + i) --i;
        if (i < 0) break;
        ++idx[i];
        for (int j = i + 1; j < k; ++j) idx[j] = idx[j - 1] + 1;
    }
}

// 枚举最低非空点 r 的所有消费方式（主位 + 翼位）
inline void enumMoves(const std::array<int, 15>& c,
                      std::vector<std::pair<uint64_t, Combo>>& moves) {
    int r = -1;
    for (int i = 0; i < 15; ++i) if (c[i] > 0) { r = i; break; }
    if (r < 0) return;

    auto emit = [&](const Combo& m) { moves.push_back({encodeMove(m), m}); };

    std::vector<int> sing, pai;
    for (int i = 0; i < 15; ++i) { if (c[i] >= 1) sing.push_back(i); if (c[i] >= 2) pai.push_back(i); }
    auto singExcluding = [&](const std::vector<int>& excl) {
        std::vector<int> out;
        for (int x : sing) { bool skip = false; for (int e : excl) if (x == e) { skip = true; break; } if (!skip) out.push_back(x); }
        return out;
    };
    auto paiExcluding = [&](const std::vector<int>& excl) {
        std::vector<int> out;
        for (int x : pai) { bool skip = false; for (int e : excl) if (x == e) { skip = true; break; } if (!skip) out.push_back(x); }
        return out;
    };

    // ---------- 主位（r 是 combo 最低主点）----------
    emit(Combo::single((Rank)r));                                   // SINGLE
    if (c[r] >= 2) emit(Combo::pair((Rank)r));                      // PAIR
    if (c[r] >= 3) {
        emit(Combo::triple((Rank)r));                               // TRIPLE
        for (int w : singExcluding({r})) emit(Combo::tripleWithSingle((Rank)r, (Rank)w));   // 三带一
        for (int p : paiExcluding({r})) emit(Combo::tripleWithPair((Rank)r, (Rank)p));      // 三带二
    }
    if (c[r] >= 4) {
        emit(Combo::bomb((Rank)r));                                 // BOMB
        auto sq = singExcluding({r});
        forEachCombination(sq, 2, [&](const std::vector<int>& ch) {                 // 四带二单
            emit(Combo::quadWithTwoSingles((Rank)r, (Rank)ch[0], (Rank)ch[1]));
        });
        auto pq = paiExcluding({r});
        forEachCombination(pq, 2, [&](const std::vector<int>& ch) {                 // 四带二对
            emit(Combo::quadWithTwoPairs((Rank)r, (Rank)ch[0], (Rank)ch[1]));
        });
    }
    if (r <= 11) {  // 顺/连对/飞机只在点数 3..A（索引 0..11）
        int Ls = 1; while (r + Ls <= 11 && c[r + Ls] >= 1) ++Ls;     // 单顺最大长度
        for (int len = 5; len <= Ls; ++len) {
            std::vector<Rank> rr; for (int i = 0; i < len; ++i) rr.push_back((Rank)(r + i));
            emit(Combo::straight(rr));
        }
        int Lp = 1; while (r + Lp <= 11 && c[r + Lp] >= 2) ++Lp;     // 连对最大长度
        for (int len = 3; len <= Lp; ++len) {
            std::vector<Rank> rr; for (int i = 0; i < len; ++i) rr.push_back((Rank)(r + i));
            emit(Combo::consecutivePairs(rr));
        }
        int Lf = 1; while (r + Lf <= 11 && c[r + Lf] >= 3) ++Lf;     // 飞机最大长度
        for (int len = 2; len <= Lf && len <= 6; ++len) {
            std::vector<Rank> main; for (int i = 0; i < len; ++i) main.push_back((Rank)(r + i));
            std::vector<int> excl; for (int i = 0; i < len; ++i) excl.push_back(r + i);
            emit(Combo::plane(main));                               // 飞机（不带翼）
            auto sPool = singExcluding(excl);
            forEachCombination(sPool, len, [&](const std::vector<int>& ch) {         // 飞机带单
                std::vector<Rank> w; for (int x : ch) w.push_back((Rank)x);
                emit(Combo::planeWithSingles(main, w));
            });
            auto pPool = paiExcluding(excl);
            forEachCombination(pPool, len, [&](const std::vector<int>& ch) {         // 飞机带对
                std::vector<Rank> w; for (int x : ch) w.push_back((Rank)x);
                emit(Combo::planeWithPairs(main, w));
            });
        }
    }

    // ---------- 翼位（r 被更高主位吸收为翼）----------
    // 三带一：r 作单翼
    for (int t = r + 1; t < 15; ++t) if (c[t] >= 3) emit(Combo::tripleWithSingle((Rank)t, (Rank)r));
    // 三带二：r 作对翼
    if (c[r] >= 2) for (int t = r + 1; t < 15; ++t) if (c[t] >= 3) emit(Combo::tripleWithPair((Rank)t, (Rank)r));
    // 四带二单：r 作单翼之一，q 更高四张
    for (int q = r + 1; q < 15; ++q) if (c[q] >= 4) {
        auto sq = singExcluding({q, r});
        for (int w2 : sq) emit(Combo::quadWithTwoSingles((Rank)q, (Rank)r, (Rank)w2));
    }
    // 四带二对：r 作对翼之一
    if (c[r] >= 2) for (int q = r + 1; q < 15; ++q) if (c[q] >= 4) {
        auto pq = paiExcluding({q, r});
        for (int p2 : pq) emit(Combo::quadWithTwoPairs((Rank)q, (Rank)r, (Rank)p2));
    }
    // 飞机带单：r 作单翼之一，飞机起点 t>r
    for (int t = r + 1; t <= 11; ++t) {
        int Lf = 1; while (t + Lf <= 11 && c[t + Lf] >= 3) ++Lf;
        for (int len = 2; len <= Lf && len <= 6; ++len) {
            std::vector<Rank> main; for (int i = 0; i < len; ++i) main.push_back((Rank)(t + i));
            std::vector<int> excl; for (int i = 0; i < len; ++i) excl.push_back(t + i);
            excl.push_back(r);
            auto sPool = singExcluding(excl);
            if ((int)sPool.size() >= len - 1) forEachCombination(sPool, len - 1, [&](const std::vector<int>& ch) {
                std::vector<Rank> w; w.push_back((Rank)r); for (int x : ch) w.push_back((Rank)x);
                emit(Combo::planeWithSingles(main, w));
            });
        }
    }
    // 飞机带对：r 作对翼之一
    if (c[r] >= 2) for (int t = r + 1; t <= 11; ++t) {
        int Lf = 1; while (t + Lf <= 11 && c[t + Lf] >= 3) ++Lf;
        for (int len = 2; len <= Lf && len <= 6; ++len) {
            std::vector<Rank> main; for (int i = 0; i < len; ++i) main.push_back((Rank)(t + i));
            std::vector<int> excl; for (int i = 0; i < len; ++i) excl.push_back(t + i);
            excl.push_back(r);
            auto pPool = paiExcluding(excl);
            if ((int)pPool.size() >= len - 1) forEachCombination(pPool, len - 1, [&](const std::vector<int>& ch) {
                std::vector<Rank> w; w.push_back((Rank)r); for (int x : ch) w.push_back((Rank)x);
                emit(Combo::planeWithPairs(main, w));
            });
        }
    }
    // 王炸：r==小王且有大王（r 必为最低非空 ⇒ 0..12 空）
    if (r == (int)Rank::SMALL_JOKER && c[(int)Rank::BIG_JOKER] >= 1) emit(Combo::rocket());
}

struct MemoEntry { int n; double score; uint64_t moveCode; };

inline std::unordered_map<uint64_t, MemoEntry>& memoMap() {
    static std::unordered_map<uint64_t, MemoEntry> m;
    return m;
}

inline void clearMemo() { memoMap().clear(); }

const double NEG_INF = -1e18;

inline MemoEntry solve(uint64_t key, const std::array<int, 15>& c) {
    int r = -1;
    for (int i = 0; i < 15; ++i) if (c[i] > 0) { r = i; break; }
    if (r < 0) return {0, 0.0, 0};

    auto it = memoMap().find(key);
    if (it != memoMap().end()) return it->second;

    static DefaultComboScoringStrategy scorer;  // 无状态，读全局 scoringConfig()

    std::vector<std::pair<uint64_t, Combo>> moves;
    moves.reserve(64);
    enumMoves(c, moves);

    MemoEntry best{100000, NEG_INF, UINT64_MAX};
    std::array<std::pair<int, int>, 15> buf;
    std::array<int, 15> c2;
    const auto& p = pow5();

    for (auto& mv : moves) {
        int nb = consumeBuf(mv.second, buf);
        c2 = c;
        bool ok = true;
        uint64_t key2 = key;
        for (int i = 0; i < nb; ++i) {
            c2[buf[i].first] -= buf[i].second;
            if (c2[buf[i].first] < 0) { ok = false; break; }
            key2 -= (uint64_t)buf[i].second * p[buf[i].first];
        }
        if (!ok) continue;
        MemoEntry ch = solve(key2, c2);
        int n = ch.n + 1;
        double sc = ch.score + scorer.score(mv.second);
        bool better = false;
        if (n < best.n) better = true;
        else if (n == best.n) {
            if (sc > best.score + 1e-9) better = true;
            else if (std::fabs(sc - best.score) <= 1e-9 && mv.first < best.moveCode) better = true;
        }
        if (better) best = {n, sc, mv.first};
    }

    memoMap()[key] = best;
    return best;
}

} // namespace optdetail

// 全局最优拆牌
inline std::vector<Combo> optimalSplit(const std::vector<Card>& hand) {
    auto c = HandCardUtils::buildRankCounts(hand);
    uint64_t key = optdetail::packCounts(c);
    optdetail::solve(key, c);  // 填充 memo

    std::vector<Combo> out;
    std::array<int, 15> cur = c;
    uint64_t ck = key;
    std::array<std::pair<int, int>, 15> buf;
    const auto& p = optdetail::pow5();
    while (true) {
        int r = -1;
        for (int i = 0; i < 15; ++i) if (cur[i] > 0) { r = i; break; }
        if (r < 0) break;
        auto it = optdetail::memoMap().find(ck);
        if (it == optdetail::memoMap().end()) break;  // 不应发生
        Combo m = optdetail::decodeMove(it->second.moveCode);
        out.push_back(m);
        int nb = optdetail::consumeBuf(m, buf);
        for (int i = 0; i < nb; ++i) {
            cur[buf[i].first] -= buf[i].second;
            ck -= (uint64_t)buf[i].second * p[buf[i].first];
        }
    }
    return out;
}

// 改 scoringConfig 后调用（清 memo）
inline void clearOptimalSplitMemo() { optdetail::clearMemo(); }

} // namespace landlord
