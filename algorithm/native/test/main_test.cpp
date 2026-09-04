#include "landlord.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <set>
#include <stdexcept>
#include <type_traits>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            throw std::runtime_error("CHECK failed: " #expression); \
        } \
    } while (false)

using namespace landlord;

static_assert(!std::is_copy_constructible<ShuffleDealStrategy>::value,
              "ShuffleDealStrategy must not copy its borrowed scorer pointer");
static_assert(!std::is_copy_assignable<ShuffleDealStrategy>::value,
              "ShuffleDealStrategy must not copy-assign its borrowed scorer pointer");
static_assert(!std::is_move_constructible<ShuffleDealStrategy>::value,
              "ShuffleDealStrategy must not move its borrowed scorer pointer");
static_assert(!std::is_move_assignable<ShuffleDealStrategy>::value,
              "ShuffleDealStrategy must not move-assign its borrowed scorer pointer");

void testRankValues() {
    CHECK(rankBaseValue(Rank::THREE) == -7);
    CHECK(rankBaseValue(Rank::TEN) == 0);
    CHECK(rankBaseValue(Rank::ACE) == 6);
    CHECK(rankBaseValue(Rank::TWO) == 10);
    CHECK(rankBaseValue(Rank::BIG_JOKER) == 18);
    CHECK(isStraightRank(Rank::ACE));
    CHECK(!isStraightRank(Rank::TWO));
    CHECK(!isStraightRank(Rank::BIG_JOKER));
    std::cout << "[PASS] testRankValues" << std::endl;
}

void testDeckSize() {
    auto deck = Deck::fullDeck();
    CHECK(deck.size() == 54);
    std::set<std::pair<int, int>> uniqueCards;
    for (const auto& card : deck) {
        uniqueCards.insert({static_cast<int>(card.rank), static_cast<int>(card.suit)});
    }
    CHECK(uniqueCards.size() == 54);
    int jokerCount = 0;
    for (auto& c : deck) {
        if (c.suit == Suit::NONE) jokerCount++;
    }
    CHECK(jokerCount == 2);
    std::cout << "[PASS] testDeckSize" << std::endl;
}

void testComboFactories() {
    DefaultComboScoringStrategy scorer;

    auto s = Combo::single(Rank::ACE);
    CHECK(s.type == ComboType::SINGLE);
    CHECK(s.length() == 1);
    CHECK(std::abs(0.0 - scorer.score(s)) < 0.001);

    auto p = Combo::pair(Rank::ACE);
    CHECK(p.type == ComboType::PAIR);
    CHECK(p.length() == 2);
    CHECK(std::abs(14.0 - scorer.score(p)) < 0.001);

    auto t = Combo::triple(Rank::ACE);
    CHECK(t.type == ComboType::TRIPLE);
    CHECK(t.length() == 3);
    CHECK(std::abs(23.0 - scorer.score(t)) < 0.001);

    auto b = Combo::bomb(Rank::KING);
    CHECK(b.type == ComboType::BOMB);
    CHECK(b.length() == 4);
    CHECK(std::abs(43.0 - scorer.score(b)) < 0.001);

    auto r = Combo::rocket();
    CHECK(r.type == ComboType::ROCKET);
    CHECK(r.length() == 2);
    CHECK(std::abs(60.0 - scorer.score(r)) < 0.001);

    auto st = Combo::straight({Rank::THREE, Rank::FOUR, Rank::FIVE, Rank::SIX, Rank::SEVEN});
    CHECK(st.type == ComboType::STRAIGHT);
    CHECK(st.length() == 5);
    CHECK(std::abs(15.0 - scorer.score(st)) < 0.001);

    auto consecutivePairs = Combo::consecutivePairs({Rank::THREE, Rank::FOUR, Rank::FIVE});
    CHECK(consecutivePairs.type == ComboType::CONSECUTIVE_PAIRS);
    CHECK(consecutivePairs.length() == 6);
    CHECK(std::abs(12.0 - scorer.score(consecutivePairs)) < 0.001);

    auto tw = Combo::tripleWithSingle(Rank::ACE, Rank::THREE);
    CHECK(tw.type == ComboType::TRIPLE_WITH_SINGLE);
    CHECK(tw.length() == 4);

    auto tp = Combo::tripleWithPair(Rank::ACE, Rank::THREE);
    CHECK(tp.type == ComboType::TRIPLE_WITH_PAIR);
    CHECK(tp.length() == 5);

    auto pl = Combo::plane({Rank::THREE, Rank::FOUR});
    CHECK(pl.type == ComboType::PLANE);
    CHECK(pl.length() == 6);

    auto q2s = Combo::quadWithTwoSingles(Rank::KING, Rank::THREE, Rank::FOUR);
    CHECK(q2s.type == ComboType::QUAD_WITH_TWO_SINGLES);
    CHECK(q2s.length() == 6);

    auto q2p = Combo::quadWithTwoPairs(Rank::KING, Rank::THREE, Rank::FOUR);
    CHECK(q2p.type == ComboType::QUAD_WITH_TWO_PAIRS);
    CHECK(q2p.length() == 8);

    std::cout << "[PASS] testComboFactories" << std::endl;
}

void testComboScoring() {
    DefaultComboScoringStrategy scorer;

    CHECK(scorer.score(Combo::single(Rank::ACE)) == 0.0);

    // PAIR: baseValue * 2 + 2
    // ACE baseValue = 6, so 6*2+2 = 14
    double pairScore = scorer.score(Combo::pair(Rank::ACE));
    CHECK(std::abs(pairScore - 14.0) < 0.001);

    // TRIPLE: baseValue * 3 + 5
    // ACE: 6*3+5 = 23
    double tripleScore = scorer.score(Combo::triple(Rank::ACE));
    CHECK(std::abs(tripleScore - 23.0) < 0.001);

    // BOMB: baseValue * 2 + 35
    // KING baseValue = 4, so 4*2+35 = 43
    double bombScore = scorer.score(Combo::bomb(Rank::KING));
    CHECK(std::abs(bombScore - 43.0) < 0.001);

    // ROCKET: 60
    double rocketScore = scorer.score(Combo::rocket());
    CHECK(std::abs(rocketScore - 60.0) < 0.001);

    // 三带一对（对子翼 = 基础分+1）：AAA+22 → 6*3+10 + (10+1) = 28+11 = 39
    double tripleWithPairScore = scorer.score(Combo::tripleWithPair(Rank::ACE, Rank::TWO));
    CHECK(std::abs(tripleWithPairScore - 39.0) < 0.001);

    std::cout << "[PASS] testComboScoring" << std::endl;
}

void testHandScoring() {
    DefaultHandCardsScoringStrategy handScorer;

    std::vector<Card> hand = {
        {Rank::ACE, Suit::SPADE}, {Rank::ACE, Suit::HEART}, {Rank::ACE, Suit::CLUB},
        {Rank::THREE, Suit::SPADE}
    };
    std::vector<Combo> combos = { Combo::tripleWithSingle(Rank::ACE, Rank::THREE) };

    double score = handScorer.calcTotalHandScore(hand, combos);
    // tripleWithSingle: 6*3+8 + max(0, -7*0.5) = 26 + 0 = 26
    // N=1, penalty = 0
    // control bonus: no joker, no 2s >= 2, no bomb
    // V_total = 26
    CHECK(std::abs(score - 26.0) < 0.001);

    // 持有炸弹的控制加成与拆牌无关：8888 全拆成单牌（无 BOMB 组合），仍应 +5
    std::vector<Card> hand2 = {
        {Rank::EIGHT, Suit::SPADE}, {Rank::EIGHT, Suit::HEART},
        {Rank::EIGHT, Suit::CLUB},  {Rank::EIGHT, Suit::DIAMOND},
        {Rank::THREE, Suit::SPADE}, {Rank::FOUR, Suit::HEART}
    };
    std::vector<Combo> combos2 = {
        Combo::single(Rank::EIGHT), Combo::single(Rank::EIGHT),
        Combo::single(Rank::EIGHT), Combo::single(Rank::EIGHT),
        Combo::single(Rank::THREE), Combo::single(Rank::FOUR)
    };
    double score2 = handScorer.calcTotalHandScore(hand2, combos2);
    // comboSum=0, N=6 -> penalty=(6-1)*8=40, control=+5(held bomb) -> V_total = -35
    CHECK(std::abs(score2 - (-35.0)) < 0.001);

    std::cout << "[PASS] testHandScoring" << std::endl;
}

void testDealCards() {
    ShuffleDealStrategy strategy;
    auto shuffled = strategy.shuffle();
    CHECK(shuffled.size() == 54);

    auto deal = strategy.dealCards(shuffled);
    CHECK(deal.handCards[0].size() == 17);
    CHECK(deal.handCards[1].size() == 17);
    CHECK(deal.handCards[2].size() == 17);
    CHECK(deal.bottomCards.size() == 3);

    int total = 0;
    for (int i = 0; i < 3; i++) total += deal.handCards[i].size();
    total += deal.bottomCards.size();
    CHECK(total == 54);

    std::cout << "[PASS] testDealCards" << std::endl;
}

void testShuffleAndDeal() {
    shuffleConfig().enabled = false;
    ShuffleDealStrategy strategy;
    auto result = strategy.shuffleAndDeal();

    CHECK(result.handCards[0].size() == 17);
    CHECK(result.handCards[1].size() == 17);
    CHECK(result.handCards[2].size() == 17);
    CHECK(result.bottomCards.size() == 3);
    CHECK(!result.reshuffled);
    CHECK(result.reshuffleCnt == 0);

    std::cout << "[PASS] testShuffleAndDeal" << std::endl;
}

void testShuffleAndDealWithReshuffle() {
    auto& cfg = shuffleConfig();
    cfg.enabled = true;
    cfg.lowerThreshold = -68.0;
    cfg.upperThreshold = 74.0;
    cfg.maxSpread = 113.0;
    cfg.maxReshuffleTimes = 5;

    ShuffleDealStrategy strategy;

    int totalReshuffles = 0;
    for (int i = 0; i < 100; i++) {
        auto result = strategy.shuffleAndDeal();
        totalReshuffles += result.reshuffleCnt;
        CHECK(result.handCards[0].size() == 17);
    }

    std::cout << "[PASS] testShuffleAndDealWithReshuffle (totalReshuffles=" << totalReshuffles << ")" << std::endl;
}

void testConfigLoad() {
    bool ok = loadScoringConfigFromFile("config/scoring.properties");
    CHECK(ok && "config file should load");
    CHECK(rankBaseValue(Rank::THREE) == -7);
    CHECK(rankBaseValue(Rank::ACE) == 6);
    DefaultComboScoringStrategy scorer;
    CHECK(std::abs(scorer.score(Combo::rocket()) - 60.0) < 0.001);
    std::cout << "[PASS] testConfigLoad" << std::endl;
}

void benchmarkComboScoring() {
    DefaultComboScoringStrategy scorer;
    std::vector<Combo> combos = {
        Combo::single(Rank::ACE),
        Combo::pair(Rank::KING),
        Combo::triple(Rank::TWO),
        Combo::straight({Rank::THREE, Rank::FOUR, Rank::FIVE, Rank::SIX, Rank::SEVEN}),
        Combo::bomb(Rank::ACE),
        Combo::rocket()
    };
    const int iterations = 500000;
    auto start = std::chrono::high_resolution_clock::now();
    double sum = 0;
    for (int i = 0; i < iterations; ++i) {
        for (size_t j = 0; j < combos.size(); ++j) {
            sum += scorer.score(combos[j]);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double opsPerSec = iterations * combos.size() * 1000.0 / elapsedMs;
    std::cout << "[Benchmark] ComboScoring: " << iterations << " iters, " << combos.size()
              << " combos/iter, " << static_cast<int>(opsPerSec) << " ops/sec" << std::endl;
}

void benchmarkShuffleAndDeal() {
    shuffleConfig().enabled = false;
    ShuffleDealStrategy strategy;
    const int iterations = 5000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        strategy.shuffleAndDeal();
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double opsPerSec = iterations * 1000.0 / elapsedMs;
    std::cout << "[Benchmark] ShuffleAndDeal: " << iterations << " iters, "
              << static_cast<int>(opsPerSec) << " ops/sec" << std::endl;
}

void testBoundaryEmptyHand() {
    DefaultHandCardsScoringStrategy handScorer;
    std::vector<Card> emptyHand;
    std::vector<Combo> emptyCombos;
    double score = handScorer.calcTotalHandScore(emptyHand, emptyCombos);
    CHECK(score == 0.0);
    std::cout << "[PASS] testBoundaryEmptyHand" << std::endl;
}

void testBoundaryEhsScaleZero() {
    ScoringConfig cfg = defaultScoringConfig();
    cfg.ehsScale = 0.0;
    setScoringConfig(cfg);

    shuffleConfig().enabled = false;
    ShuffleDealStrategy strategy;
    auto result = strategy.shuffleAndDeal();

    CHECK(!std::isnan(result.ehs[0]) && !std::isnan(result.ehs[1]) && !std::isnan(result.ehs[2]));
    CHECK(result.ehs[0] >= 0 && result.ehs[0] <= 1);
    CHECK(result.ehs[1] >= 0 && result.ehs[1] <= 1);
    CHECK(result.ehs[2] >= 0 && result.ehs[2] <= 1);

    loadScoringConfigFromFile("config/scoring.properties");
    std::cout << "[PASS] testBoundaryEhsScaleZero" << std::endl;
}

void testSplitting() {
    ShuffleDealStrategy strategy;
    for (int i = 0; i < 50; i++) {
        auto shuffled = strategy.shuffle();
        auto deal = strategy.dealCards(shuffled);
        for (int seat = 0; seat < 3; seat++) {
            double score = strategy.calcHandStrengthScores(deal.getHandCards(seat));
            (void)score;
        }
    }
    std::cout << "[PASS] testSplitting" << std::endl;
}

void testSeededShuffleIsReproducible() {
    ShuffleDealStrategy first(7);
    ShuffleDealStrategy second(7);
    CHECK(first.shuffle() == second.shuffle());
}

void testReturnedScoresMatchReturnedHands() {
    auto& cfg = shuffleConfig();
    cfg.enabled = true;
    cfg.lowerThreshold = 100000.0;
    cfg.upperThreshold = 100001.0;
    cfg.maxReshuffleTimes = 1;
    cfg.thresholdRelaxStep = 0.0;

    ShuffleDealStrategy strategy(7);
    auto result = strategy.shuffleAndDeal();

    CHECK(result.reshuffleCnt == 1);
    for (int seat = 0; seat < 3; ++seat) {
        double recomputed = strategy.calcHandStrengthScores(result.handCards[seat]);
        CHECK(std::abs(recomputed - result.handStrength[seat]) < 0.001);
    }
}

int main() {
    testRankValues();
    testConfigLoad();
    testBoundaryEmptyHand();
    testBoundaryEhsScaleZero();
    testDeckSize();
    testComboFactories();
    testComboScoring();
    testHandScoring();
    testDealCards();
    testShuffleAndDeal();
    testShuffleAndDealWithReshuffle();
    testSplitting();
    testSeededShuffleIsReproducible();
    testReturnedScoresMatchReturnedHands();

    benchmarkComboScoring();
    benchmarkShuffleAndDeal();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
