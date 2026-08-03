#include "landlord.h"
#include <cassert>
#include <iostream>
#include <cmath>
#include <chrono>

using namespace landlord;

void testRankValues() {
    assert(rankBaseValue(Rank::THREE) == -7);
    assert(rankBaseValue(Rank::TEN) == 0);
    assert(rankBaseValue(Rank::ACE) == 6);
    assert(rankBaseValue(Rank::TWO) == 10);
    assert(rankBaseValue(Rank::BIG_JOKER) == 18);
    assert(isStraightRank(Rank::ACE));
    assert(!isStraightRank(Rank::TWO));
    assert(!isStraightRank(Rank::BIG_JOKER));
    std::cout << "[PASS] testRankValues" << std::endl;
}

void testDeckSize() {
    auto deck = Deck::fullDeck();
    assert(deck.size() == 54);
    int jokerCount = 0;
    for (auto& c : deck) {
        if (c.suit == Suit::NONE) jokerCount++;
    }
    assert(jokerCount == 2);
    std::cout << "[PASS] testDeckSize" << std::endl;
}

void testComboFactories() {
    auto s = Combo::single(Rank::THREE);
    assert(s.type == ComboType::SINGLE);
    assert(s.length() == 1);

    auto p = Combo::pair(Rank::ACE);
    assert(p.type == ComboType::PAIR);
    assert(p.length() == 2);

    auto b = Combo::bomb(Rank::KING);
    assert(b.type == ComboType::BOMB);
    assert(b.length() == 4);

    auto r = Combo::rocket();
    assert(r.type == ComboType::ROCKET);
    assert(r.length() == 2);

    auto st = Combo::straight({Rank::THREE, Rank::FOUR, Rank::FIVE, Rank::SIX, Rank::SEVEN});
    assert(st.type == ComboType::STRAIGHT);
    assert(st.length() == 5);

    auto tw = Combo::tripleWithSingle(Rank::ACE, Rank::THREE);
    assert(tw.type == ComboType::TRIPLE_WITH_SINGLE);
    assert(tw.length() == 4);

    auto tp = Combo::tripleWithPair(Rank::ACE, Rank::THREE);
    assert(tp.type == ComboType::TRIPLE_WITH_PAIR);
    assert(tp.length() == 5);

    auto pl = Combo::plane({Rank::THREE, Rank::FOUR});
    assert(pl.type == ComboType::PLANE);
    assert(pl.length() == 6);

    auto q2s = Combo::quadWithTwoSingles(Rank::KING, Rank::THREE, Rank::FOUR);
    assert(q2s.type == ComboType::QUAD_WITH_TWO_SINGLES);
    assert(q2s.length() == 6);

    auto q2p = Combo::quadWithTwoPairs(Rank::KING, Rank::THREE, Rank::FOUR);
    assert(q2p.type == ComboType::QUAD_WITH_TWO_PAIRS);
    assert(q2p.length() == 8);

    std::cout << "[PASS] testComboFactories" << std::endl;
}

void testComboScoring() {
    DefaultComboScoringStrategy scorer;

    assert(scorer.score(Combo::single(Rank::ACE)) == 0.0);

    // PAIR: baseValue * 2 + 2
    // ACE baseValue = 6, so 6*2+2 = 14
    double pairScore = scorer.score(Combo::pair(Rank::ACE));
    assert(std::abs(pairScore - 14.0) < 0.001);

    // TRIPLE: baseValue * 3 + 5
    // ACE: 6*3+5 = 23
    double tripleScore = scorer.score(Combo::triple(Rank::ACE));
    assert(std::abs(tripleScore - 23.0) < 0.001);

    // BOMB: baseValue * 2 + 35
    // KING baseValue = 4, so 4*2+35 = 43
    double bombScore = scorer.score(Combo::bomb(Rank::KING));
    assert(std::abs(bombScore - 43.0) < 0.001);

    // ROCKET: 60
    double rocketScore = scorer.score(Combo::rocket());
    assert(std::abs(rocketScore - 60.0) < 0.001);

    // 三带一对（对子翼 = 基础分+1）：AAA+22 → 6*3+10 + (10+1) = 28+11 = 39
    double tripleWithPairScore = scorer.score(Combo::tripleWithPair(Rank::ACE, Rank::TWO));
    assert(std::abs(tripleWithPairScore - 39.0) < 0.001);

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
    assert(std::abs(score - 26.0) < 0.001);

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
    assert(std::abs(score2 - (-35.0)) < 0.001);

    std::cout << "[PASS] testHandScoring" << std::endl;
}

void testDealCards() {
    ShuffleDealStrategy strategy;
    auto shuffled = strategy.shuffle();
    assert(shuffled.size() == 54);

    auto deal = strategy.dealCards(shuffled);
    assert(deal.handCards[0].size() == 17);
    assert(deal.handCards[1].size() == 17);
    assert(deal.handCards[2].size() == 17);
    assert(deal.bottomCards.size() == 3);

    int total = 0;
    for (int i = 0; i < 3; i++) total += deal.handCards[i].size();
    total += deal.bottomCards.size();
    assert(total == 54);

    std::cout << "[PASS] testDealCards" << std::endl;
}

void testShuffleAndDeal() {
    shuffleConfig().enabled = false;
    ShuffleDealStrategy strategy;
    auto result = strategy.shuffleAndDeal();

    assert(result.handCards[0].size() == 17);
    assert(result.handCards[1].size() == 17);
    assert(result.handCards[2].size() == 17);
    assert(result.bottomCards.size() == 3);
    assert(!result.reshuffled);
    assert(result.reshuffleCnt == 0);

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
        assert(result.handCards[0].size() == 17);
    }

    std::cout << "[PASS] testShuffleAndDealWithReshuffle (totalReshuffles=" << totalReshuffles << ")" << std::endl;
}

void testConfigLoad() {
    bool ok = loadScoringConfigFromFile("config/scoring.properties");
    assert(ok && "config file should load");
    assert(rankBaseValue(Rank::THREE) == -7);
    assert(rankBaseValue(Rank::ACE) == 6);
    DefaultComboScoringStrategy scorer;
    assert(std::abs(scorer.score(Combo::rocket()) - 60.0) < 0.001);
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
    assert(score == 0.0);
    std::cout << "[PASS] testBoundaryEmptyHand" << std::endl;
}

void testBoundaryEhsScaleZero() {
    ScoringConfig cfg = defaultScoringConfig();
    cfg.ehsScale = 0.0;
    setScoringConfig(cfg);

    shuffleConfig().enabled = false;
    ShuffleDealStrategy strategy;
    auto result = strategy.shuffleAndDeal();

    assert(!std::isnan(result.ehs[0]) && !std::isnan(result.ehs[1]) && !std::isnan(result.ehs[2]));
    assert(result.ehs[0] >= 0 && result.ehs[0] <= 1);
    assert(result.ehs[1] >= 0 && result.ehs[1] <= 1);
    assert(result.ehs[2] >= 0 && result.ehs[2] <= 1);

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

    benchmarkComboScoring();
    benchmarkShuffleAndDeal();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}
