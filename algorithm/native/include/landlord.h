#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <cmath>
#include <limits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <functional>
#include <numeric>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include <cstdint>

namespace landlord {

// ============================================================
// 1. Rank — 斗地主牌面点数
// ============================================================

constexpr int RANK_COUNT = 15;
constexpr int STRAIGHT_RANK_COUNT = 12;

enum class Rank : int {
    THREE      = 0,
    FOUR       = 1,
    FIVE       = 2,
    SIX        = 3,
    SEVEN      = 4,
    EIGHT      = 5,
    NINE       = 6,
    TEN        = 7,
    JACK       = 8,
    QUEEN      = 9,
    KING       = 10,
    ACE        = 11,
    TWO        = 12,
    SMALL_JOKER = 13,
    BIG_JOKER  = 14
};

inline int rankIndex(Rank r) { return static_cast<int>(r); }

// ============================================================
// ScoringConfig — 牌力评分配置（从配置文件加载）
// ============================================================

struct ScoringConfig {
    int rankBaseValues[15];
    int pairCoeff, pairOffset;
    int tripleCoeff, tripleOffset;
    int tripleWithSingleOffset, tripleWithPairOffset;
    double wingRankFactor;
    int straightLenCoeff, consecutivePairsLenCoeff;
    int planeTripleOffset, planeLenCoeff;
    int planeWithSinglesTripleOffset, planeWithPairsTripleOffset;
    int planeWithWingsLenCoeff;
    int quadBaseCoeff;
    int quadWithTwoSinglesOffset, quadWithTwoPairsOffset;
    int bombCoeff, bombOffset;
    int rocketScore;
    double penaltyPerCombo;
    int bonusBigJoker, bonusSmallJoker, bonusTwoPairs, bonusBombOrRocket;
    double ehsScale;
};

inline ScoringConfig defaultScoringConfig() {
    ScoringConfig c;
    int rbv[] = { -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 4, 6, 10, 15, 18 };
    for (int i = 0; i < 15; ++i) c.rankBaseValues[i] = rbv[i];
    c.pairCoeff = 2; c.pairOffset = 2;
    c.tripleCoeff = 3; c.tripleOffset = 5;
    c.tripleWithSingleOffset = 8; c.tripleWithPairOffset = 10;
    c.wingRankFactor = 0.5;
    c.straightLenCoeff = 3; c.consecutivePairsLenCoeff = 4;
    c.planeTripleOffset = 5; c.planeLenCoeff = 6;
    c.planeWithSinglesTripleOffset = 8; c.planeWithPairsTripleOffset = 10;
    c.planeWithWingsLenCoeff = 8;
    c.quadBaseCoeff = 2;
    c.quadWithTwoSinglesOffset = 10; c.quadWithTwoPairsOffset = 14;
    c.bombCoeff = 2; c.bombOffset = 35; c.rocketScore = 60;
    c.penaltyPerCombo = 8.0;
    c.bonusBigJoker = 5; c.bonusSmallJoker = 3; c.bonusTwoPairs = 4; c.bonusBombOrRocket = 5;
    c.ehsScale = 40.0;
    return c;
}

inline ScoringConfig& scoringConfig() {
    static ScoringConfig cfg = defaultScoringConfig();
    return cfg;
}

inline void setScoringConfig(const ScoringConfig& c) {
    scoringConfig() = c;
}

inline bool loadScoringConfigFromFile(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f.good()) return false;
    ScoringConfig c = defaultScoringConfig();
    std::string line;
    while (std::getline(f, line)) {
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        while (!key.empty() && (key[0] == ' ' || key[0] == '\t')) key.erase(0, 1);
        while (!key.empty() && (key[key.size()-1] == ' ' || key[key.size()-1] == '\t')) key.erase(key.size()-1);
        while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0, 1);
        while (!val.empty() && (val[val.size()-1] == ' ' || val[val.size()-1] == '\t')) val.erase(val.size()-1);
        if (key.empty() || key[0] == '#') continue;
        if (key == "landlord.scoring.rank-base-values" || key == "rank-base-values") {
            std::vector<int> vals;
            std::string v = val;
            for (std::size_t i = 0; i < v.size(); ) {
                std::size_t j = v.find(',', i);
                if (j == std::string::npos) j = v.size();
                std::string part = v.substr(i, j - i);
                while (!part.empty() && part[0] == ' ') part.erase(0, 1);
                if (!part.empty()) {
                    vals.push_back(std::atoi(part.c_str()));
                }
                i = j + 1;
            }
            if (vals.size() == 15) {
                for (int i = 0; i < 15; ++i) c.rankBaseValues[i] = vals[i];
            }
        } else if (key == "landlord.scoring.pair-coeff" || key == "pair-coeff") { c.pairCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.pair-offset" || key == "pair-offset") { c.pairOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.triple-coeff" || key == "triple-coeff") { c.tripleCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.triple-offset" || key == "triple-offset") { c.tripleOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.triple-with-single-offset" || key == "triple-with-single-offset") { c.tripleWithSingleOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.triple-with-pair-offset" || key == "triple-with-pair-offset") { c.tripleWithPairOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.wing-rank-factor" || key == "wing-rank-factor") { c.wingRankFactor = std::atof(val.c_str()); }
        else if (key == "landlord.scoring.straight-len-coeff" || key == "straight-len-coeff") { c.straightLenCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.consecutive-pairs-len-coeff" || key == "consecutive-pairs-len-coeff") { c.consecutivePairsLenCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.plane-triple-offset" || key == "plane-triple-offset") { c.planeTripleOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.plane-len-coeff" || key == "plane-len-coeff") { c.planeLenCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.plane-with-singles-triple-offset" || key == "plane-with-singles-triple-offset") { c.planeWithSinglesTripleOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.plane-with-pairs-triple-offset" || key == "plane-with-pairs-triple-offset") { c.planeWithPairsTripleOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.plane-with-wings-len-coeff" || key == "plane-with-wings-len-coeff") { c.planeWithWingsLenCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.quad-base-coeff" || key == "quad-base-coeff") { c.quadBaseCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.quad-with-two-singles-offset" || key == "quad-with-two-singles-offset") { c.quadWithTwoSinglesOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.quad-with-two-pairs-offset" || key == "quad-with-two-pairs-offset") { c.quadWithTwoPairsOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.bomb-coeff" || key == "bomb-coeff") { c.bombCoeff = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.bomb-offset" || key == "bomb-offset") { c.bombOffset = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.rocket-score" || key == "rocket-score") { c.rocketScore = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.penalty-per-combo" || key == "penalty-per-combo") { c.penaltyPerCombo = std::atof(val.c_str()); }
        else if (key == "landlord.scoring.bonus-big-joker" || key == "bonus-big-joker") { c.bonusBigJoker = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.bonus-small-joker" || key == "bonus-small-joker") { c.bonusSmallJoker = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.bonus-two-pairs" || key == "bonus-two-pairs") { c.bonusTwoPairs = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.bonus-bomb-or-rocket" || key == "bonus-bomb-or-rocket") { c.bonusBombOrRocket = std::atoi(val.c_str()); }
        else if (key == "landlord.scoring.ehs-scale" || key == "ehs-scale") { c.ehsScale = std::atof(val.c_str()); }
    }
    setScoringConfig(c);
    return true;
}

inline int rankBaseValue(Rank r) {
    return scoringConfig().rankBaseValues[rankIndex(r)];
}

inline constexpr bool isStraightRank(Rank r) {
    return r != Rank::TWO && r != Rank::SMALL_JOKER && r != Rank::BIG_JOKER;
}

constexpr std::array<Rank, STRAIGHT_RANK_COUNT> STRAIGHT_RANKS = {
    Rank::THREE, Rank::FOUR, Rank::FIVE, Rank::SIX,
    Rank::SEVEN, Rank::EIGHT, Rank::NINE, Rank::TEN,
    Rank::JACK, Rank::QUEEN, Rank::KING, Rank::ACE
};

constexpr std::array<Rank, RANK_COUNT> ALL_RANKS = {
    Rank::THREE, Rank::FOUR, Rank::FIVE, Rank::SIX,
    Rank::SEVEN, Rank::EIGHT, Rank::NINE, Rank::TEN,
    Rank::JACK, Rank::QUEEN, Rank::KING, Rank::ACE,
    Rank::TWO, Rank::SMALL_JOKER, Rank::BIG_JOKER
};

// ============================================================
// 2. Suit — 花色
// ============================================================

enum class Suit { SPADE, HEART, CLUB, DIAMOND, NONE };

// ============================================================
// 3. ComboType — 斗地主牌型
// ============================================================

enum class ComboType {
    SINGLE,
    PAIR,
    TRIPLE,
    TRIPLE_WITH_SINGLE,
    TRIPLE_WITH_PAIR,
    STRAIGHT,
    CONSECUTIVE_PAIRS,
    PLANE,
    PLANE_WITH_SINGLES,
    PLANE_WITH_PAIRS,
    QUAD_WITH_TWO_SINGLES,
    QUAD_WITH_TWO_PAIRS,
    BOMB,
    ROCKET
};

// ============================================================
// 4. Card — 单张牌
// ============================================================

struct Card {
    Rank rank;
    Suit suit;
    int baseValue() const { return rankBaseValue(rank); }
    bool operator==(const Card& o) const { return rank == o.rank && suit == o.suit; }
    bool operator!=(const Card& o) const { return !(*this == o); }
};

// ============================================================
// 5. Combo — 牌型组合
// ============================================================

struct Combo {
    ComboType type;
    std::vector<Rank> mainRanks;
    std::vector<Rank> wingRanks;

    int length() const {
        switch (type) {
            case ComboType::SINGLE:                return 1;
            case ComboType::PAIR:                  return 2;
            case ComboType::TRIPLE:                return 3;
            case ComboType::TRIPLE_WITH_SINGLE:    return 4;
            case ComboType::TRIPLE_WITH_PAIR:      return 5;
            case ComboType::STRAIGHT:              return static_cast<int>(mainRanks.size());
            case ComboType::CONSECUTIVE_PAIRS:     return static_cast<int>(mainRanks.size()) * 2;
            case ComboType::PLANE:                 return static_cast<int>(mainRanks.size()) * 3;
            case ComboType::PLANE_WITH_SINGLES:    return static_cast<int>(mainRanks.size()) * 3 + static_cast<int>(wingRanks.size());
            case ComboType::PLANE_WITH_PAIRS:      return static_cast<int>(mainRanks.size()) * 3 + static_cast<int>(wingRanks.size()) * 2;
            case ComboType::QUAD_WITH_TWO_SINGLES: return 6;
            case ComboType::QUAD_WITH_TWO_PAIRS:   return 8;
            case ComboType::BOMB:                  return 4;
            case ComboType::ROCKET:                return 2;
        }
        return 0;
    }

    static Combo single(Rank r) {
        return { ComboType::SINGLE, {r}, {} };
    }
    static Combo pair(Rank r) {
        return { ComboType::PAIR, {r}, {} };
    }
    static Combo triple(Rank r) {
        return { ComboType::TRIPLE, {r}, {} };
    }
    static Combo tripleWithSingle(Rank tri, Rank s) {
        return { ComboType::TRIPLE_WITH_SINGLE, {tri}, {s} };
    }
    static Combo tripleWithPair(Rank tri, Rank p) {
        return { ComboType::TRIPLE_WITH_PAIR, {tri}, {p} };
    }
    static Combo straight(const std::vector<Rank>& ranks) {
        return { ComboType::STRAIGHT, ranks, {} };
    }
    static Combo consecutivePairs(const std::vector<Rank>& ranks) {
        return { ComboType::CONSECUTIVE_PAIRS, ranks, {} };
    }
    static Combo plane(const std::vector<Rank>& triRanks) {
        return { ComboType::PLANE, triRanks, {} };
    }
    static Combo planeWithSingles(const std::vector<Rank>& triRanks, const std::vector<Rank>& singles) {
        return { ComboType::PLANE_WITH_SINGLES, triRanks, singles };
    }
    static Combo planeWithPairs(const std::vector<Rank>& triRanks, const std::vector<Rank>& pairRanks) {
        return { ComboType::PLANE_WITH_PAIRS, triRanks, pairRanks };
    }
    static Combo quadWithTwoSingles(Rank quad, Rank s1, Rank s2) {
        return { ComboType::QUAD_WITH_TWO_SINGLES, {quad}, {s1, s2} };
    }
    static Combo quadWithTwoPairs(Rank quad, Rank p1, Rank p2) {
        return { ComboType::QUAD_WITH_TWO_PAIRS, {quad}, {p1, p2} };
    }
    static Combo bomb(Rank r) {
        return { ComboType::BOMB, {r}, {} };
    }
    static Combo rocket() {
        return { ComboType::ROCKET, {Rank::BIG_JOKER, Rank::SMALL_JOKER}, {} };
    }
};

// ============================================================
// 6. Deck — 牌堆
// ============================================================

namespace Deck {
    constexpr int DECK_SIZE   = 54;
    constexpr int HAND_SIZE   = 17;
    constexpr int BOTTOM_SIZE = 3;

    inline std::vector<Card> fullDeck() {
        constexpr Suit SUITS[] = { Suit::SPADE, Suit::HEART, Suit::CLUB, Suit::DIAMOND };
        std::vector<Card> deck;
        deck.reserve(DECK_SIZE);
        for (auto r : ALL_RANKS) {
            if (r == Rank::SMALL_JOKER || r == Rank::BIG_JOKER) {
                deck.push_back({r, Suit::NONE});
            } else {
                for (auto s : SUITS) {
                    deck.push_back({r, s});
                }
            }
        }
        return deck;
    }
}

// ============================================================
// 7. DealData — 发牌数据
// ============================================================

struct DealData {
    std::vector<Card> handCards[3];
    std::vector<Card> bottomCards;

    const std::vector<Card>& getHandCards(int seat) const {
        return handCards[seat];
    }
    std::vector<Card>& getHandCards(int seat) {
        return handCards[seat];
    }
    const std::vector<Card>& getBottomCards() const {
        return bottomCards;
    }
};

// ============================================================
// 8. EvaluatedDealData — 带牌力评估的发牌结果
// ============================================================

struct EvaluatedDealData : DealData {
    double handStrength[3] = {0, 0, 0};
    double ehs[3]          = {0, 0, 0};
    bool   reshuffled      = false;
    int    reshuffleCnt    = 0;
};

// 将 V_total 压缩到 [0,1] 区间的近似 EHS 指标（Sigmoid），仅用于日志与分析
inline double normalizeHandStrength(double score) {
    double scale = scoringConfig().ehsScale;
    if (scale <= 0 || std::isnan(scale) || std::isinf(scale)) scale = 40.0;
    double x = score / scale;
    double e = std::exp(-x);
    return 1.0 / (1.0 + e);
}

// ============================================================
// 9. HandCardUtils — 手牌工具
// ============================================================

namespace HandCardUtils {
    inline std::array<int, RANK_COUNT> buildRankCounts(const std::vector<Card>& cards) {
        std::array<int, RANK_COUNT> count{};
        for (const auto& c : cards) {
            count[rankIndex(c.rank)]++;
        }
        return count;
    }
}

// ============================================================
// 10. DefaultComboScoringStrategy — 单个牌型计分
// ============================================================

class DefaultComboScoringStrategy {
public:
    inline double score(const Combo& c) const {
        switch (c.type) {
            case ComboType::SINGLE:
                return 0;
            case ComboType::PAIR:
                return scoreMainRank(c.mainRanks, 0, scoringConfig().pairCoeff, scoringConfig().pairOffset);
            case ComboType::TRIPLE:
                return scoreMainRank(c.mainRanks, 0, scoringConfig().tripleCoeff, scoringConfig().tripleOffset);
            case ComboType::TRIPLE_WITH_SINGLE:
                return scoreTripleWithWing(c, scoringConfig().tripleWithSingleOffset, false);
            case ComboType::TRIPLE_WITH_PAIR:
                return scoreTripleWithWing(c, scoringConfig().tripleWithPairOffset, true);
            case ComboType::STRAIGHT:
                return scoreStraightOrPairs(c.mainRanks, true, scoringConfig().straightLenCoeff);
            case ComboType::CONSECUTIVE_PAIRS:
                return scoreStraightOrPairs(c.mainRanks, false, scoringConfig().consecutivePairsLenCoeff);
            case ComboType::PLANE:
                return scorePlane(c.mainRanks, scoringConfig().planeTripleOffset, scoringConfig().planeLenCoeff);
            case ComboType::PLANE_WITH_SINGLES:
                return scorePlaneWithWings(c, scoringConfig().planeWithSinglesTripleOffset, false);
            case ComboType::PLANE_WITH_PAIRS:
                return scorePlaneWithWings(c, scoringConfig().planeWithPairsTripleOffset, true);
            case ComboType::QUAD_WITH_TWO_SINGLES:
                return scoreQuadWithWings(c, scoringConfig().quadWithTwoSinglesOffset, false);
            case ComboType::QUAD_WITH_TWO_PAIRS:
                return scoreQuadWithWings(c, scoringConfig().quadWithTwoPairsOffset, true);
            case ComboType::BOMB:
                return scoreMainRank(c.mainRanks, 0, scoringConfig().bombCoeff, scoringConfig().bombOffset);
            case ComboType::ROCKET:
                return static_cast<double>(scoringConfig().rocketScore);
        }
        return 0;
    }

private:
    static double scoreMainRank(const std::vector<Rank>& mainRanks, int idx, int coeff, int offset) {
        if (idx >= static_cast<int>(mainRanks.size())) return 0;
        return rankBaseValue(mainRanks[idx]) * coeff + offset;
    }

    static double scoreWingRank(const std::vector<Rank>& wings, int idx) {
        if (idx >= static_cast<int>(wings.size())) return 0;
        return std::max(0.0, rankBaseValue(wings[idx]) * scoringConfig().wingRankFactor);
    }

    // 对子翼加分 = max(0, 基础分 + 1)，直接对齐 PRD §4.1.2「翼牌为对子时 翼牌加分 = max(0, 基础分 + 1)」字面口径，
    // 不随评分配置(pairCoeff/pairOffset/wingRankFactor)漂移。单牌翼仍用 scoreWingRank(= 基础分 × wingRankFactor)。
    static double scorePairWingRank(const std::vector<Rank>& wings, int idx) {
        if (idx >= static_cast<int>(wings.size())) return 0;
        return std::max(0.0, rankBaseValue(wings[idx]) + 1.0);
    }

    static double scoreTripleWithWing(const Combo& c, int tripleOffset, bool pairWing) {
        double main = scoreMainRank(c.mainRanks, 0, scoringConfig().tripleCoeff, tripleOffset);
        double wing = pairWing ? scorePairWingRank(c.wingRanks, 0) : scoreWingRank(c.wingRanks, 0);
        return main + wing;
    }

    static double scoreStraightOrPairs(const std::vector<Rank>& mainRanks, bool clampNegative, int lenCoeff) {
        if (mainRanks.empty()) return 0;
        double sum = 0;
        for (std::size_t i = 0; i < mainRanks.size(); ++i) {
            double v = static_cast<double>(rankBaseValue(mainRanks[i]));
            sum += clampNegative ? std::max(0.0, v) : v;
        }
        return std::max(0.0, sum) + static_cast<int>(mainRanks.size()) * lenCoeff;
    }

    static double scorePlane(const std::vector<Rank>& mainRanks, int tripleOffset, int lenCoeff) {
        if (mainRanks.empty()) return 0;
        const ScoringConfig& cfg = scoringConfig();
        double sum = 0;
        for (std::size_t i = 0; i < mainRanks.size(); ++i) {
            sum += rankBaseValue(mainRanks[i]) * cfg.tripleCoeff + tripleOffset;
        }
        return sum + static_cast<int>(mainRanks.size()) * lenCoeff;
    }

    static double scorePlaneWithWings(const Combo& c, int tripleOffset, bool pairWing) {
        if (c.mainRanks.empty()) return 0;
        const ScoringConfig& cfg = scoringConfig();
        double sum = 0;
        for (std::size_t i = 0; i < c.mainRanks.size(); ++i) {
            sum += rankBaseValue(c.mainRanks[i]) * cfg.tripleCoeff + tripleOffset;
        }
        sum += static_cast<int>(c.mainRanks.size()) * cfg.planeWithWingsLenCoeff;
        for (int i = 0; i < static_cast<int>(c.wingRanks.size()); ++i) {
            sum += pairWing ? scorePairWingRank(c.wingRanks, i) : scoreWingRank(c.wingRanks, i);
        }
        return sum;
    }

    static double scoreQuadWithWings(const Combo& c, int quadOffset, bool pairWing) {
        double main = scoreMainRank(c.mainRanks, 0, scoringConfig().quadBaseCoeff, quadOffset);
        double w1 = pairWing ? scorePairWingRank(c.wingRanks, 0) : scoreWingRank(c.wingRanks, 0);
        double w2 = pairWing ? scorePairWingRank(c.wingRanks, 1) : scoreWingRank(c.wingRanks, 1);
        return main + w1 + w2;
    }
};

// ============================================================
// 11. DefaultHandCardsScoringStrategy — 整手牌牌力评分
// ============================================================

class DefaultHandCardsScoringStrategy {
public:
    explicit DefaultHandCardsScoringStrategy(const DefaultComboScoringStrategy& cs)
        : comboScorer_(cs) {}
    DefaultHandCardsScoringStrategy() = default;

    inline double calcTotalHandScore(const std::vector<Card>& handCards,
                                     const std::vector<Combo>& combos) const {
        if (handCards.empty() || combos.empty()) return 0.0;
        int n = static_cast<int>(combos.size());
        double comboSum = 0.0;
        for (const auto& c : combos) {
            comboSum += comboScorer_.score(c);
        }
        double penalty = (n - 1) * scoringConfig().penaltyPerCombo;
        // 控制牌加成（含炸弹/王炸）基于全部手牌计算，与拆牌方式无关（PRD §4.1.3）
        int controlBonus = computeHandControlBonus(handCards);
        return comboSum - penalty + controlBonus;
    }

private:
    DefaultComboScoringStrategy comboScorer_;

    // Control_Bonus：基于全部手牌计算，与拆牌方式无关（PRD §4.1.3）。
    // 大王/小王/双2 + 每个持有炸弹(张数>=4) + 持有王炸(大小王齐)。
    static int computeHandControlBonus(const std::vector<Card>& handCards) {
        const ScoringConfig& cfg = scoringConfig();
        int bonus = 0;
        bool hasBigJoker = false, hasSmallJoker = false;
        int twoCount = 0;
        std::array<int, RANK_COUNT> rankCount{};
        for (std::size_t i = 0; i < handCards.size(); ++i) {
            Rank r = handCards[i].rank;
            if (r == Rank::BIG_JOKER)   hasBigJoker = true;
            if (r == Rank::SMALL_JOKER) hasSmallJoker = true;
            if (r == Rank::TWO)         twoCount++;
            rankCount[rankIndex(r)]++;
        }
        if (hasBigJoker)   bonus += cfg.bonusBigJoker;
        if (hasSmallJoker) bonus += cfg.bonusSmallJoker;
        if (twoCount >= 2) bonus += cfg.bonusTwoPairs;
        for (int rc : rankCount) {
            if (rc >= 4) bonus += cfg.bonusBombOrRocket;
        }
        if (hasBigJoker && hasSmallJoker) bonus += cfg.bonusBombOrRocket;
        return bonus;
    }
};

// ============================================================
// 12. SplitStrategyDecisionProperties — 拆牌策略决策配置
// ============================================================

struct SplitStrategyDecisionProperties {
    int strongStraightLen              = 7;
    int strongConsecutivePairsLen      = 5;
    int weakPlaneLenMax                = 1;
    int fewTriplesStraightFirst        = 1;
    int planePresentLenMin             = 2;
    int straightLenBreakThreshold      = 6;
    int consecutivePairsLenBreakThreshold = 4;
    int manyTriplesMin                 = 3;
    int manyBombsMin                   = 2;
    int longStraightLenMin             = 5;
    int fewTriplesLongStraight         = 2;
    int shortStraightLenMax            = 5;
};

// ============================================================
// 13. ShuffleStrategyDecisionProperties — 发牌过滤配置
// ============================================================

struct ShuffleStrategyDecisionProperties {
    bool   enabled                     = true;
    double lowerThreshold              = -std::numeric_limits<double>::infinity();
    double upperThreshold              =  std::numeric_limits<double>::infinity();
    double maxSpread                   =  std::numeric_limits<double>::infinity();
    double maxPotentialLandlordScore   =  std::numeric_limits<double>::infinity();
    double maxLandlordAdvantage        =  std::numeric_limits<double>::infinity();
    int    maxSinglesPerHand           = 0;
    int    maxBombsPerHand             = 0;
    double thresholdRelaxStep          = 0.15;
    int    maxReshuffleTimes           = 5;
    std::string version                = "dealing_filter_v2";
};

// ============================================================
// 14. 全局配置持有者
// ============================================================

inline SplitStrategyDecisionProperties& splitConfig() {
    static SplitStrategyDecisionProperties instance;
    return instance;
}

inline ShuffleStrategyDecisionProperties& shuffleConfig() {
    static ShuffleStrategyDecisionProperties instance;
    return instance;
}

// ============================================================
// 15. AbstractHandSplitter — 拆牌抽象基类
// ============================================================

class AbstractHandSplitter {
public:
    virtual ~AbstractHandSplitter() = default;

    inline std::vector<Combo> extractAllCombos(const std::vector<Card>& handCards) {
        auto count = HandCardUtils::buildRankCounts(handCards);
        return extractAllCombos(handCards, count);
    }

    virtual std::vector<Combo> extractAllCombos(const std::vector<Card>& handCards,
                                                std::array<int, RANK_COUNT>& rankCounts) = 0;

protected:
    static constexpr int MIN_STRAIGHT_LEN          = 5;
    static constexpr int MIN_CONSECUTIVE_PAIRS_LEN = 3;
    static constexpr int MIN_PLANE_LEN             = 2;

    struct Segment { int startIdx; int maxLen; };

    inline bool findLongestConsecutiveSegment(const std::array<int, RANK_COUNT>& count,
                                             int minCardsPerRank, int minLen,
                                             Segment& out) const {
        int maxLen = 0;
        int startIdx = -1;
        for (int i = 0; i < STRAIGHT_RANK_COUNT; ++i) {
            int len = 0;
            while (i + len < STRAIGHT_RANK_COUNT) {
                Rank r = STRAIGHT_RANKS[i + len];
                if (count[rankIndex(r)] >= minCardsPerRank) {
                    ++len;
                } else {
                    break;
                }
            }
            if (len >= minLen && len > maxLen) {
                maxLen = len;
                startIdx = i;
            }
        }
        if (maxLen < minLen) return false;
        out.startIdx = startIdx;
        out.maxLen = maxLen;
        return true;
    }

    inline bool extractStraights(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        Segment seg;
        if (!findLongestConsecutiveSegment(count, 1, MIN_STRAIGHT_LEN, seg)) return false;
        std::vector<Rank> ranks;
        ranks.reserve(seg.maxLen);
        for (int j = 0; j < seg.maxLen; ++j) {
            Rank r = STRAIGHT_RANKS[seg.startIdx + j];
            ranks.push_back(r);
            count[rankIndex(r)]--;
        }
        combos.push_back(Combo::straight(ranks));
        return true;
    }

    inline bool extractConsecutivePairs(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        Segment seg;
        if (!findLongestConsecutiveSegment(count, 2, MIN_CONSECUTIVE_PAIRS_LEN, seg)) return false;
        std::vector<Rank> ranks;
        ranks.reserve(seg.maxLen);
        for (int j = 0; j < seg.maxLen; ++j) {
            Rank r = STRAIGHT_RANKS[seg.startIdx + j];
            ranks.push_back(r);
            count[rankIndex(r)] -= 2;
        }
        combos.push_back(Combo::consecutivePairs(ranks));
        return true;
    }

    inline bool tryExtractRocket(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        int bigIdx   = rankIndex(Rank::BIG_JOKER);
        int smallIdx = rankIndex(Rank::SMALL_JOKER);
        if (count[bigIdx] >= 1 && count[smallIdx] >= 1) {
            combos.push_back(Combo::rocket());
            count[bigIdx]--;
            count[smallIdx]--;
            return true;
        }
        return false;
    }

    inline void extractAllBombs(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        for (auto r : ALL_RANKS) {
            if (r == Rank::SMALL_JOKER || r == Rank::BIG_JOKER) continue;
            int idx = rankIndex(r);
            while (count[idx] >= 4) {
                combos.push_back(Combo::bomb(r));
                count[idx] -= 4;
            }
        }
    }

    inline bool extractPlanes(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        Segment seg;
        if (!findLongestConsecutiveSegment(count, 3, MIN_PLANE_LEN, seg)) return false;
        std::vector<Rank> triRanks;
        triRanks.reserve(seg.maxLen);
        for (int j = 0; j < seg.maxLen; ++j) {
            Rank r = STRAIGHT_RANKS[seg.startIdx + j];
            triRanks.push_back(r);
            count[rankIndex(r)] -= 3;
        }

        auto singles = collectSingles(count);
        auto pairs   = collectPairs(count);

        if (seg.maxLen <= static_cast<int>(pairs.size())) {
            std::vector<Rank> wingPairs(pairs.begin(), pairs.begin() + seg.maxLen);
            for (auto r : wingPairs) count[rankIndex(r)] -= 2;
            combos.push_back(Combo::planeWithPairs(triRanks, wingPairs));
        } else if (seg.maxLen <= static_cast<int>(singles.size())) {
            std::vector<Rank> wingSingles(singles.begin(), singles.begin() + seg.maxLen);
            for (auto r : wingSingles) count[rankIndex(r)] -= 1;
            combos.push_back(Combo::planeWithSingles(triRanks, wingSingles));
        } else {
            combos.push_back(Combo::plane(triRanks));
        }
        return true;
    }

    inline bool extractQuadsWithWings(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        for (auto r : ALL_RANKS) {
            if (r == Rank::SMALL_JOKER || r == Rank::BIG_JOKER) continue;
            int idx = rankIndex(r);
            if (count[idx] < 4) continue;

            auto pairs   = collectPairs(count);
            auto singles = collectSingles(count);

            if (static_cast<int>(pairs.size()) >= 2) {
                Rank p1 = pairs[0], p2 = pairs[1];
                combos.push_back(Combo::quadWithTwoPairs(r, p1, p2));
                count[idx] -= 4;
                count[rankIndex(p1)] -= 2;
                count[rankIndex(p2)] -= 2;
                return true;
            }
            if (static_cast<int>(singles.size()) >= 2) {
                Rank s1 = singles[0], s2 = singles[1];
                combos.push_back(Combo::quadWithTwoSingles(r, s1, s2));
                count[idx] -= 4;
                count[rankIndex(s1)] -= 1;
                count[rankIndex(s2)] -= 1;
                return true;
            }
        }
        return false;
    }

    inline void extractTriples(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        for (auto r : ALL_RANKS) {
            int idx = rankIndex(r);
            if (count[idx] < 3) continue;

            auto pairs   = collectPairs(count);
            auto singles = collectSingles(count);

            if (!pairs.empty()) {
                Rank p = pairs[0];
                combos.push_back(Combo::tripleWithPair(r, p));
                count[idx] -= 3;
                count[rankIndex(p)] -= 2;
            } else if (!singles.empty()) {
                Rank s = singles[0];
                combos.push_back(Combo::tripleWithSingle(r, s));
                count[idx] -= 3;
                count[rankIndex(s)] -= 1;
            } else {
                combos.push_back(Combo::triple(r));
                count[idx] -= 3;
            }
        }
    }

    inline void extractAllBareTriples(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        for (auto r : ALL_RANKS) {
            int idx = rankIndex(r);
            while (count[idx] >= 3) {
                combos.push_back(Combo::triple(r));
                count[idx] -= 3;
            }
        }
    }

    inline void extractAllPairs(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        for (auto r : ALL_RANKS) {
            int idx = rankIndex(r);
            while (count[idx] >= 2) {
                combos.push_back(Combo::pair(r));
                count[idx] -= 2;
            }
        }
    }

    inline void extractAllSingles(std::array<int, RANK_COUNT>& count, std::vector<Combo>& combos) {
        for (auto r : ALL_RANKS) {
            int idx = rankIndex(r);
            while (count[idx] >= 1) {
                combos.push_back(Combo::single(r));
                count[idx] -= 1;
            }
        }
    }

    inline std::vector<Rank> collectSingles(const std::array<int, RANK_COUNT>& count) const {
        std::vector<Rank> list;
        for (auto r : ALL_RANKS) {
            int c = count[rankIndex(r)];
            for (int i = 0; i < c; ++i) list.push_back(r);
        }
        std::sort(list.begin(), list.end(), [](Rank a, Rank b) {
            return rankBaseValue(a) < rankBaseValue(b);
        });
        return list;
    }

    inline std::vector<Rank> collectPairs(const std::array<int, RANK_COUNT>& count) const {
        std::vector<Rank> list;
        for (auto r : ALL_RANKS) {
            if (count[rankIndex(r)] >= 2) list.push_back(r);
        }
        std::sort(list.begin(), list.end(), [](Rank a, Rank b) {
            return rankBaseValue(a) < rankBaseValue(b);
        });
        return list;
    }
};

// ============================================================
// 16. PlaneBombPrioritizedSplitter — 飞机/炸弹优先拆牌
// ============================================================

class PlaneBombPrioritizedSplitter : public AbstractHandSplitter {
public:
    std::vector<Combo> extractAllCombos(const std::vector<Card>& /*handCards*/,
                                        std::array<int, RANK_COUNT>& count) override {
        std::vector<Combo> combos;
        combos.reserve(20);

        tryExtractRocket(count, combos);
        extractAllBombs(count, combos);
        while (extractPlanes(count, combos)) {}
        while (extractQuadsWithWings(count, combos)) {}
        while (extractStraights(count, combos)) {}
        while (extractConsecutivePairs(count, combos)) {}
        extractTriples(count, combos);
        extractAllPairs(count, combos);
        extractAllSingles(count, combos);

        return combos;
    }
};

// ============================================================
// 17. StraightPrioritizedSplitter — 顺子/连对优先拆牌
// ============================================================

class StraightPrioritizedSplitter : public AbstractHandSplitter {
public:
    std::vector<Combo> extractAllCombos(const std::vector<Card>& /*handCards*/,
                                        std::array<int, RANK_COUNT>& count) override {
        std::vector<Combo> combos;
        combos.reserve(20);

        tryExtractRocket(count, combos);
        while (extractStraights(count, combos)) {}
        while (extractConsecutivePairs(count, combos)) {}
        extractAllBareTriples(count, combos);
        extractAllPairs(count, combos);
        extractAllSingles(count, combos);
        extractAllBombs(count, combos);

        return combos;
    }
};

// ============================================================
// 18. DefaultSplitterFactory — 拆牌算法工厂
// ============================================================

class DefaultSplitterFactory {
public:
    static inline std::vector<Combo> extractAllCombos(const std::vector<Card>& handCards) {
        auto count = HandCardUtils::buildRankCounts(handCards);
        return getSplitter(count).extractAllCombos(handCards, count);
    }

    static inline AbstractHandSplitter& getSplitter(std::array<int, RANK_COUNT>& rankCounts) {
        if (chooseStrategy(rankCounts))
            return straightSplitter();
        return planeBombSplitter();
    }

    static inline AbstractHandSplitter& getSplitter(const std::vector<Card>& handCards) {
        auto count = HandCardUtils::buildRankCounts(handCards);
        return getSplitter(count);
    }

    static inline bool chooseStrategy(const std::array<int, RANK_COUNT>& handCardsCnt) {
        bool confident;
        return chooseStrategyWithConfidence(handCardsCnt, &confident);
    }

    static inline bool chooseStrategyWithConfidence(const std::array<int, RANK_COUNT>& handCardsCnt,
                                                    bool* confidentOut) {
        const auto& c = splitConfig();
        int maxStraightLen          = calcMaxConsecutiveLength(handCardsCnt, ComboType::STRAIGHT);
        int maxConsecutivePairsLen  = calcMaxConsecutiveLength(handCardsCnt, ComboType::CONSECUTIVE_PAIRS);
        int maxPlaneLen             = calcMaxConsecutiveLength(handCardsCnt, ComboType::PLANE);
        int numTriples              = calcSatisfiedRankCnt(handCardsCnt, ComboType::TRIPLE);
        int numBombs                = calcSatisfiedRankCnt(handCardsCnt, ComboType::BOMB);

        if ((maxStraightLen >= c.strongStraightLen || maxConsecutivePairsLen >= c.strongConsecutivePairsLen)
                && maxPlaneLen <= c.weakPlaneLenMax && numTriples <= c.fewTriplesStraightFirst) {
            if (confidentOut) *confidentOut = true;
            return true;
        }
        if (maxPlaneLen >= c.planePresentLenMin
                && (maxStraightLen >= c.straightLenBreakThreshold || maxConsecutivePairsLen >= c.consecutivePairsLenBreakThreshold)) {
            if (confidentOut) *confidentOut = true;
            return false;
        }
        if ((maxStraightLen >= c.straightLenBreakThreshold || maxConsecutivePairsLen >= c.consecutivePairsLenBreakThreshold)
                && numTriples <= c.fewTriplesStraightFirst) {
            if (confidentOut) *confidentOut = false;
            return true;
        }
        if ((numTriples >= c.manyTriplesMin || numBombs >= c.manyBombsMin)
                && maxStraightLen < c.straightLenBreakThreshold && maxConsecutivePairsLen < c.consecutivePairsLenBreakThreshold) {
            if (confidentOut) *confidentOut = false;
            return false;
        }
        if (maxStraightLen >= c.longStraightLenMin && numTriples <= c.fewTriplesLongStraight) {
            if (confidentOut) *confidentOut = false;
            return true;
        }
        if (maxPlaneLen >= c.planePresentLenMin && maxStraightLen <= c.shortStraightLenMax) {
            if (confidentOut) *confidentOut = false;
            return false;
        }
        if (confidentOut) *confidentOut = false;
        return false;
    }

    static PlaneBombPrioritizedSplitter& planeBombSplitter() {
        static PlaneBombPrioritizedSplitter instance;
        return instance;
    }
    static StraightPrioritizedSplitter& straightSplitter() {
        static StraightPrioritizedSplitter instance;
        return instance;
    }

private:
    static constexpr int MIN_CARDS_STRAIGHT           = 1;
    static constexpr int MIN_CARDS_PAIRS              = 2;
    static constexpr int MIN_CARDS_TRIPLE_OR_PLANE    = 3;
    static constexpr int MIN_CARDS_BOMB               = 4;

    static int minCardsForType(ComboType ct) {
        switch (ct) {
            case ComboType::STRAIGHT:          return MIN_CARDS_STRAIGHT;
            case ComboType::CONSECUTIVE_PAIRS: return MIN_CARDS_PAIRS;
            case ComboType::PLANE:
            case ComboType::TRIPLE:            return MIN_CARDS_TRIPLE_OR_PLANE;
            case ComboType::BOMB:              return MIN_CARDS_BOMB;
            default:                           return MIN_CARDS_STRAIGHT;
        }
    }

    static int calcMaxConsecutiveLength(const std::array<int, RANK_COUNT>& handCardsCnt, ComboType comboType) {
        int need = minCardsForType(comboType);
        int maxVal = 0;
        for (int i = 0; i < STRAIGHT_RANK_COUNT; ++i) {
            int len = 0;
            while (i + len < STRAIGHT_RANK_COUNT) {
                Rank r = STRAIGHT_RANKS[i + len];
                if (handCardsCnt[rankIndex(r)] >= need) {
                    ++len;
                } else {
                    break;
                }
            }
            if (len > maxVal) maxVal = len;
        }
        return maxVal;
    }

    static int calcSatisfiedRankCnt(const std::array<int, RANK_COUNT>& handCardsCnt, ComboType comboType) {
        int need = minCardsForType(comboType);
        int n = 0;
        for (auto r : STRAIGHT_RANKS) {
            if (handCardsCnt[rankIndex(r)] >= need) ++n;
        }
        return n;
    }
};

// ============================================================
// 19. DefaultComboExtractor — 双路径拆牌取最优
// ============================================================

class DefaultComboExtractor {
public:
    explicit DefaultComboExtractor(const DefaultHandCardsScoringStrategy& scorer)
        : scorer_(&scorer) {}
    DefaultComboExtractor() : scorer_(nullptr) {}

    inline std::vector<Combo> extractAllCombos(const std::vector<Card>& handCards) const {
        if (!scorer_) {
            return DefaultSplitterFactory::extractAllCombos(handCards);
        }

        auto count = HandCardUtils::buildRankCounts(handCards);
        bool confident = false;
        bool useStraight = DefaultSplitterFactory::chooseStrategyWithConfidence(count, &confident);

        if (confident) {
            if (useStraight)
                return DefaultSplitterFactory::straightSplitter().extractAllCombos(handCards, count);
            return DefaultSplitterFactory::planeBombSplitter().extractAllCombos(handCards, count);
        }

        auto countB = count;
        auto combosA = DefaultSplitterFactory::planeBombSplitter().extractAllCombos(handCards, count);
        auto combosB = DefaultSplitterFactory::straightSplitter().extractAllCombos(handCards, countB);

        double scoreA = scorer_->calcTotalHandScore(handCards, combosA);
        double scoreB = scorer_->calcTotalHandScore(handCards, combosB);

        return scoreA >= scoreB ? std::move(combosA) : std::move(combosB);
    }

private:
    const DefaultHandCardsScoringStrategy* scorer_;
};

// ============================================================
// 20. ShuffleDealStrategy — 洗牌 + 发牌 + 重洗逻辑
// ============================================================

class ShuffleDealStrategy {
public:
    ShuffleDealStrategy()
        : handScorer_()
        , comboExtractor_(handScorer_)
        , rng_(std::random_device{}()) {}

    explicit ShuffleDealStrategy(std::uint32_t seed)
        : handScorer_()
        , comboExtractor_(handScorer_)
        , rng_(seed) {}

    ShuffleDealStrategy(const ShuffleDealStrategy&) = delete;
    ShuffleDealStrategy& operator=(const ShuffleDealStrategy&) = delete;
    ShuffleDealStrategy(ShuffleDealStrategy&&) = delete;
    ShuffleDealStrategy& operator=(ShuffleDealStrategy&&) = delete;

    inline std::vector<Card> shuffle() {
        auto deck = Deck::fullDeck();
        std::shuffle(deck.begin(), deck.end(), rng_);
        return deck;
    }

    inline DealData dealCards(const std::vector<Card>& shuffledCards) {
        DealData dd;
        dd.handCards[0].assign(shuffledCards.begin(),
                               shuffledCards.begin() + Deck::HAND_SIZE);
        dd.handCards[1].assign(shuffledCards.begin() + Deck::HAND_SIZE,
                               shuffledCards.begin() + Deck::HAND_SIZE * 2);
        dd.handCards[2].assign(shuffledCards.begin() + Deck::HAND_SIZE * 2,
                               shuffledCards.begin() + Deck::HAND_SIZE * 3);
        dd.bottomCards.assign(shuffledCards.begin() + Deck::HAND_SIZE * 3,
                              shuffledCards.end());
        return dd;
    }

    inline double calcHandStrengthScores(const std::vector<Card>& handCards) {
        if (handCards.empty()) return 0.0;
        auto combos = comboExtractor_.extractAllCombos(handCards);
        if (combos.empty()) return 0.0;
        return handScorer_.calcTotalHandScore(handCards, combos);
    }

    inline std::array<double, 3> calcAllSeatHandStrength(const DealData& dealData) {
        std::array<double, 3> result{};
        for (int seat = 0; seat < 3; ++seat) {
            const auto& hc = dealData.getHandCards(seat);
            if (!hc.empty()) result[seat] = calcHandStrengthScores(hc);
        }
        return result;
    }

    inline double calcPotentialLandlordScore(const DealData& dealData) {
        const auto& bottom = dealData.getBottomCards();
        double maxScore = -std::numeric_limits<double>::infinity();
        for (int seat = 0; seat < 3; ++seat) {
            std::vector<Card> combined(dealData.getHandCards(seat));
            combined.insert(combined.end(), bottom.begin(), bottom.end());
            double s = calcHandStrengthScores(combined);
            if (s > maxScore) maxScore = s;
        }
        return maxScore;
    }

    inline double calcMaxLandlordAdvantage(const DealData& dealData,
                                           const std::array<double, 3>& seatScores) {
        const auto& bottom = dealData.getBottomCards();
        double maxAdv = -std::numeric_limits<double>::infinity();
        for (int landlordSeat = 0; landlordSeat < 3; ++landlordSeat) {
            std::vector<Card> combined(dealData.getHandCards(landlordSeat));
            combined.insert(combined.end(), bottom.begin(), bottom.end());
            double landlordScore = calcHandStrengthScores(combined);

            double farmerSum = 0;
            for (int s = 0; s < 3; ++s) {
                if (s != landlordSeat) farmerSum += seatScores[s];
            }
            double adv = landlordScore - farmerSum / 2.0;
            if (adv > maxAdv) maxAdv = adv;
        }
        return maxAdv;
    }

    using StructureFeatures = std::array<std::array<int, 2>, 3>;

    inline StructureFeatures calcAllStructureFeatures(const DealData& dealData) {
        StructureFeatures features{};
        for (int seat = 0; seat < 3; ++seat) {
            auto combos = comboExtractor_.extractAllCombos(dealData.getHandCards(seat));
            int singles = 0, bombs = 0;
            for (const auto& c : combos) {
                if (c.type == ComboType::SINGLE) ++singles;
                if (c.type == ComboType::BOMB || c.type == ComboType::ROCKET) ++bombs;
            }
            features[seat][0] = singles;
            features[seat][1] = bombs;
        }
        return features;
    }

    inline EvaluatedDealData shuffleAndDeal() {
        const auto& config = shuffleConfig();
        if (!config.enabled) {
            return shuffleAndDealSimple();
        }

        int maxReshuffleTimes           = config.maxReshuffleTimes;
        double lowerThreshold           = config.lowerThreshold;
        double upperThreshold           = config.upperThreshold;
        double maxSpreadVal             = config.maxSpread;
        double maxPotentialLandlordVal  = config.maxPotentialLandlordScore;
        double maxLandlordAdvVal        = config.maxLandlordAdvantage;
        int    maxSingles               = config.maxSinglesPerHand;
        int    maxBombs                 = config.maxBombsPerHand;
        double relaxStep                = config.thresholdRelaxStep;

        auto shuffledCards = shuffle();
        auto dealData      = dealCards(shuffledCards);

        auto seatScores             = calcAllSeatHandStrength(dealData);
        double potentialLandlord    = calcPotentialLandlordScore(dealData);
        double landlordAdv          = calcMaxLandlordAdvantage(dealData, seatScores);
        auto structureFeats         = calcAllStructureFeatures(dealData);

        int reshuffleCnt    = 0;

        while (reshuffleCnt < maxReshuffleTimes
               && isExtreme(seatScores, potentialLandlord, landlordAdv,
                            structureFeats,
                            relaxThreshold(lowerThreshold, reshuffleCnt, relaxStep),
                            relaxThreshold(upperThreshold, reshuffleCnt, relaxStep),
                            relaxThreshold(maxSpreadVal, reshuffleCnt, relaxStep),
                            maxPotentialLandlordVal, maxLandlordAdvVal,
                            maxSingles, maxBombs)) {

            ++reshuffleCnt;
            shuffledCards = shuffle();
            dealData      = dealCards(shuffledCards);

            seatScores          = calcAllSeatHandStrength(dealData);
            potentialLandlord   = calcPotentialLandlordScore(dealData);
            landlordAdv         = calcMaxLandlordAdvantage(dealData, seatScores);
            structureFeats      = calcAllStructureFeatures(dealData);
        }

        EvaluatedDealData result;
        for (int i = 0; i < 3; ++i) {
            result.handCards[i] = std::move(dealData.handCards[i]);
            result.handStrength[i] = seatScores[i];
            result.ehs[i] = normalizeHandStrength(seatScores[i]);
        }
        result.bottomCards  = std::move(dealData.bottomCards);
        result.reshuffled   = reshuffleCnt > 0;
        result.reshuffleCnt = reshuffleCnt;
        return result;
    }

private:
    DefaultHandCardsScoringStrategy handScorer_;
    DefaultComboExtractor           comboExtractor_;
    std::mt19937                    rng_;

    inline EvaluatedDealData shuffleAndDealSimple() {
        auto shuffledCards = shuffle();
        auto dealData      = dealCards(shuffledCards);
        auto seatScores    = calcAllSeatHandStrength(dealData);

        EvaluatedDealData result;
        for (int i = 0; i < 3; ++i) {
            result.handCards[i] = std::move(dealData.handCards[i]);
            result.handStrength[i] = seatScores[i];
            result.ehs[i] = normalizeHandStrength(seatScores[i]);
        }
        result.bottomCards  = std::move(dealData.bottomCards);
        result.reshuffled   = false;
        result.reshuffleCnt = 0;
        return result;
    }

    static double relaxThreshold(double baseValue, int reshuffleCnt, double relaxStep) {
        if (reshuffleCnt <= 0 || relaxStep <= 0) return baseValue;
        return baseValue * (1.0 + reshuffleCnt * relaxStep);
    }

    static bool isExtreme(const std::array<double, 3>& scores,
                          double potentialLandlordScore,
                          double landlordAdvantage,
                          const StructureFeatures& structureFeatures,
                          double lower, double upper, double maxSpreadVal,
                          double maxPotentialLandlord,
                          double maxLandlordAdv,
                          int maxSingles, int maxBombs) {
        double mn = std::min({scores[0], scores[1], scores[2]});
        double mx = std::max({scores[0], scores[1], scores[2]});

        if (mn < lower || mx > upper || (mx - mn) > maxSpreadVal) return true;
        if (potentialLandlordScore > maxPotentialLandlord) return true;
        if (landlordAdvantage > maxLandlordAdv) return true;

        if (maxSingles > 0 || maxBombs > 0) {
            for (const auto& feature : structureFeatures) {
                if (maxSingles > 0 && feature[0] > maxSingles) return true;
                if (maxBombs > 0  && feature[1] > maxBombs)    return true;
            }
        }
        return false;
    }

};

// ============================================================
// 21. DealDistributionSampler — 发牌分布采样器
// ============================================================

struct DealSampleResult {
    std::vector<double> minScores;
    std::vector<double> maxScores;
    std::vector<double> spreads;
    std::vector<double> potentialLandlordScores;
    std::vector<double> landlordAdvantages;
    std::vector<double> maxSingles;
    std::vector<double> maxBombs;

    DealSampleResult(std::vector<double> minS, std::vector<double> maxS,
                     std::vector<double> spr, std::vector<double> pls,
                     std::vector<double> la,  std::vector<double> ms,
                     std::vector<double> mb)
        : minScores(sortedCopy(std::move(minS)))
        , maxScores(sortedCopy(std::move(maxS)))
        , spreads(sortedCopy(std::move(spr)))
        , potentialLandlordScores(sortedCopy(std::move(pls)))
        , landlordAdvantages(sortedCopy(std::move(la)))
        , maxSingles(sortedCopy(std::move(ms)))
        , maxBombs(sortedCopy(std::move(mb)))
    {}

    double minScorePercentile(double p) const { return percentile(minScores, p); }
    double maxScorePercentile(double p) const { return percentile(maxScores, p); }
    double spreadPercentile(double p) const { return percentile(spreads, p); }
    double potentialLandlordPercentile(double p) const { return percentile(potentialLandlordScores, p); }
    double landlordAdvantagePercentile(double p) const { return percentile(landlordAdvantages, p); }
    double maxSinglesPercentile(double p) const { return percentile(maxSingles, p); }
    double maxBombsPercentile(double p) const { return percentile(maxBombs, p); }

    void printRecommendedThresholds() const {
        std::cout << "===== DealDistributionSampler 推荐阈值 =====" << std::endl;
        std::cout << "# 手牌牌力下阈值（min 的 P5）      → lower-threshold="
                  << std::fixed << std::setprecision(1) << minScorePercentile(0.05) << std::endl;
        std::cout << "# 手牌牌力上阈值（max 的 P95）     → upper-threshold="
                  << std::fixed << std::setprecision(1) << maxScorePercentile(0.95) << std::endl;
        std::cout << "# 极差上限（spread 的 P95）        → max-spread="
                  << std::fixed << std::setprecision(1) << spreadPercentile(0.95) << std::endl;
        std::cout << "# 潜在地主牌力上限（P95）          → max-potential-landlord-score="
                  << std::fixed << std::setprecision(1) << potentialLandlordPercentile(0.95) << std::endl;
        std::cout << "# 地主优势上限（P95）              → max-landlord-advantage="
                  << std::fixed << std::setprecision(1) << landlordAdvantagePercentile(0.95) << std::endl;
        std::cout << "# 单家单牌数上限（maxSingles P95） → max-singles-per-hand="
                  << std::fixed << std::setprecision(0) << maxSinglesPercentile(0.95) << std::endl;
        std::cout << "# 单家炸弹数上限（maxBombs P99）   → max-bombs-per-hand="
                  << std::fixed << std::setprecision(0) << maxBombsPercentile(0.99) << std::endl;
        std::cout << "============================================" << std::endl;
    }

    void printDetailedDistribution() const {
        constexpr double ps[] = {0.05, 0.10, 0.25, 0.50, 0.75, 0.90, 0.95, 0.99};
        std::cout << "===== DealDistributionSampler 详细分布 =====" << std::endl;
        logRow("minScore（最差一家）", minScores, ps);
        logRow("maxScore（最强一家）", maxScores, ps);
        logRow("spread（极差）      ", spreads, ps);
        logRow("potentialLandlord  ", potentialLandlordScores, ps);
        logRow("landlordAdvantage  ", landlordAdvantages, ps);
        logRow("maxSingles（单牌数）", maxSingles, ps);
        logRow("maxBombs（炸弹数） ", maxBombs, ps);
        std::cout << "============================================" << std::endl;
    }

    static double percentile(const std::vector<double>& sorted, double p) {
        if (sorted.empty()) return 0.0;
        double idx = p * (static_cast<double>(sorted.size()) - 1);
        int lo = static_cast<int>(std::floor(idx));
        int hi = static_cast<int>(std::ceil(idx));
        if (lo == hi) return sorted[lo];
        double frac = idx - lo;
        return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
    }

private:
    static std::vector<double> sortedCopy(std::vector<double> v) {
        std::sort(v.begin(), v.end());
        return v;
    }

    template <size_t N>
    static void logRow(const char* label, const std::vector<double>& data, const double (&ps)[N]) {
        std::ostringstream ss;
        ss << label << "：";
        for (auto p : ps) {
            ss << " P" << std::fixed << std::setprecision(0) << (p * 100)
               << "=" << std::fixed << std::setprecision(1) << percentile(data, p);
        }
        std::cout << ss.str() << std::endl;
    }
};

class DealDistributionSampler {
public:
    explicit DealDistributionSampler(ShuffleDealStrategy& strategy)
        : strategy_(strategy) {}

    inline DealSampleResult sample(int sampleSize) {
        std::vector<double> minScores, maxScores, spreadsVec;
        std::vector<double> potentialLandlordScores, landlordAdvantages;
        std::vector<double> maxSinglesList, maxBombsList;

        minScores.reserve(sampleSize);
        maxScores.reserve(sampleSize);
        spreadsVec.reserve(sampleSize);
        potentialLandlordScores.reserve(sampleSize);
        landlordAdvantages.reserve(sampleSize);
        maxSinglesList.reserve(sampleSize);
        maxBombsList.reserve(sampleSize);

        for (int i = 0; i < sampleSize; ++i) {
            auto shuffled  = strategy_.shuffle();
            auto dealData  = strategy_.dealCards(shuffled);
            auto seatScores = strategy_.calcAllSeatHandStrength(dealData);

            double mn = std::min({seatScores[0], seatScores[1], seatScores[2]});
            double mx = std::max({seatScores[0], seatScores[1], seatScores[2]});

            minScores.push_back(mn);
            maxScores.push_back(mx);
            spreadsVec.push_back(mx - mn);

            double potentialLandlord = strategy_.calcPotentialLandlordScore(dealData);
            potentialLandlordScores.push_back(potentialLandlord);

            double advantage = strategy_.calcMaxLandlordAdvantage(dealData, seatScores);
            landlordAdvantages.push_back(advantage);

            auto features = strategy_.calcAllStructureFeatures(dealData);
            int mxSingles = 0, mxBombs = 0;
            for (int seat = 0; seat < 3; ++seat) {
                if (features[seat][0] > mxSingles) mxSingles = features[seat][0];
                if (features[seat][1] > mxBombs)   mxBombs   = features[seat][1];
            }
            maxSinglesList.push_back(static_cast<double>(mxSingles));
            maxBombsList.push_back(static_cast<double>(mxBombs));
        }

        return DealSampleResult(
            std::move(minScores), std::move(maxScores), std::move(spreadsVec),
            std::move(potentialLandlordScores), std::move(landlordAdvantages),
            std::move(maxSinglesList), std::move(maxBombsList)
        );
    }

private:
    ShuffleDealStrategy& strategy_;
};

} // namespace landlord
