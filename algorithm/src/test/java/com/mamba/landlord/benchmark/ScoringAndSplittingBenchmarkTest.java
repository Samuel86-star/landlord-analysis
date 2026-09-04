package com.mamba.landlord.benchmark;

import com.mamba.landlord.algorithm.scoring.strategy.DefaultComboScoringStrategy;
import com.mamba.landlord.algorithm.scoring.strategy.DefaultHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.splitter.DefaultComboExtractor;
import com.mamba.landlord.algorithm.splitter.DefaultSplitterFactory;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.DealData;
import com.mamba.landlord.core.model.Deck;
import com.mamba.landlord.core.model.Rank;
import com.mamba.landlord.core.model.Suit;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertTrue;

/**
 * 评分与拆牌环节的 Benchmark 测试。
 * 分别对 Combo 评分、整手牌评分、拆牌、双路径提取进行吞吐量测试。
 */
@Tag("benchmark")
class ScoringAndSplittingBenchmarkTest {

    private static final Logger log = LoggerFactory.getLogger(ScoringAndSplittingBenchmarkTest.class);

    private DefaultComboScoringStrategy comboScorer;
    private DefaultHandCardsScoringStrategy handScorer;
    private DefaultComboExtractor comboExtractor;
    private List<Card> sampleHand;
    private List<Combo> sampleCombos;

    @BeforeEach
    void setUp() {
        comboScorer = new DefaultComboScoringStrategy();
        handScorer = new DefaultHandCardsScoringStrategy();
        comboExtractor = new DefaultComboExtractor(handScorer);

        sampleHand = new ArrayList<>(Deck.copyFullDeckCards()).subList(0, 17);
        sampleCombos = comboExtractor.extractAllCombos(sampleHand);
    }

    @Test
    void benchmarkComboScoring() {
        int iterations = 500_000;
        List<Combo> combos = List.of(
            Combo.single(Rank.ACE),
            Combo.pair(Rank.KING),
            Combo.triple(Rank.TWO),
            Combo.straight(List.of(Rank.THREE, Rank.FOUR, Rank.FIVE, Rank.SIX, Rank.SEVEN)),
            Combo.bomb(Rank.ACE),
            Combo.rocket()
        );

        long start = System.nanoTime();
        double sum = 0;
        for (int i = 0; i < iterations; i++) {
            for (Combo c : combos) {
                sum += comboScorer.score(c);
            }
        }
        long elapsedNs = System.nanoTime() - start;
        double opsPerSec = iterations * combos.size() * 1e9 / elapsedNs;

        log.info("[Benchmark] ComboScoring: iterations={}, combosPerIter={}, totalOps={}, elapsedMs={}, opsPerSec={}",
            iterations, combos.size(), iterations * combos.size(), elapsedNs / 1_000_000, String.format("%.0f", opsPerSec));
        assertTrue(opsPerSec > 100_000, "Combo scoring should exceed 100k ops/sec");
    }

    @Test
    void benchmarkHandScoring() {
        int iterations = 100_000;

        long start = System.nanoTime();
        double sum = 0;
        for (int i = 0; i < iterations; i++) {
            sum += handScorer.calcTotalHandScore(sampleHand, sampleCombos);
        }
        long elapsedNs = System.nanoTime() - start;
        double opsPerSec = iterations * 1e9 / elapsedNs;

        log.info("[Benchmark] HandScoring: iterations={}, elapsedMs={}, opsPerSec={}",
            iterations, elapsedNs / 1_000_000, String.format("%.0f", opsPerSec));
        assertTrue(opsPerSec > 10_000, "Hand scoring should exceed 10k ops/sec");
    }

    @Test
    void benchmarkSinglePathSplitting() {
        int iterations = 50_000;

        long start = System.nanoTime();
        int comboCount = 0;
        for (int i = 0; i < iterations; i++) {
            List<Combo> combos = DefaultSplitterFactory.extractAllCombos(sampleHand);
            comboCount += combos.size();
        }
        long elapsedNs = System.nanoTime() - start;
        double opsPerSec = iterations * 1e9 / elapsedNs;

        log.info("[Benchmark] SinglePathSplitting: iterations={}, elapsedMs={}, opsPerSec={}, avgCombosPerHand={}",
            iterations, elapsedNs / 1_000_000, String.format("%.0f", opsPerSec), comboCount * 1.0 / iterations);
        assertTrue(opsPerSec > 2_000, "Single-path splitting should exceed 2k ops/sec");
    }

    @Test
    void benchmarkDualPathExtraction() {
        int iterations = 20_000;

        long start = System.nanoTime();
        for (int i = 0; i < iterations; i++) {
            comboExtractor.extractAllCombos(sampleHand);
        }
        long elapsedNs = System.nanoTime() - start;
        double opsPerSec = iterations * 1e9 / elapsedNs;

        log.info("[Benchmark] DualPathExtraction: iterations={}, elapsedMs={}, opsPerSec={}",
            iterations, elapsedNs / 1_000_000, String.format("%.0f", opsPerSec));
        assertTrue(opsPerSec > 500, "Dual-path extraction should exceed 500 ops/sec");
    }

    @Test
    void benchmarkFullShuffleAndDeal() {
        com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties props =
            new com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties();
        props.setEnabled(false);
        com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder.set(props);

        com.mamba.landlord.algorithm.shuffle.strategy.DefaultReshuffleDealStrategy strategy =
            new com.mamba.landlord.algorithm.shuffle.strategy.DefaultReshuffleDealStrategy();

        int iterations = 5_000;
        long start = System.nanoTime();
        int reshuffles = 0;
        for (int i = 0; i < iterations; i++) {
            reshuffles += strategy.shuffleAndDeal().getReshuffleCnt();
        }
        long elapsedNs = System.nanoTime() - start;
        double opsPerSec = iterations * 1e9 / elapsedNs;

        log.info("[Benchmark] FullShuffleAndDeal: iterations={}, elapsedMs={}, opsPerSec={}, totalReshuffles={}",
            iterations, elapsedNs / 1_000_000, String.format("%.0f", opsPerSec), reshuffles);
        assertTrue(opsPerSec > 100, "Full shuffle+deal should exceed 100 ops/sec");
    }

    /**
     * 分解 FullShuffleAndDeal 各环节耗时，定位瓶颈。
     */
    @Test
    void benchmarkFullShuffleAndDealBreakdown() {
        var props = new com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties();
        props.setEnabled(false);
        com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder.set(props);

        var strategy = new com.mamba.landlord.algorithm.shuffle.strategy.DefaultReshuffleDealStrategy();
        int iterations = 10_000;

        long tShuffle = 0, tDeal = 0, tExtractAndScore = 0;
        List<Card> deck = null;
        DealData dealData = null;

        for (int i = 0; i < iterations; i++) {
            long s = System.nanoTime();
            deck = strategy.shuffle();
            tShuffle += System.nanoTime() - s;

            s = System.nanoTime();
            dealData = strategy.dealCards(deck);
            tDeal += System.nanoTime() - s;
        }

        for (int seat = 0; seat < 3; seat++) {
            List<Card> hand = dealData.getHandCards(seat);
            for (int i = 0; i < iterations; i++) {
                long s = System.nanoTime();
                List<Combo> combos = comboExtractor.extractAllCombos(hand);
                handScorer.calcTotalHandScore(hand, combos);
                tExtractAndScore += System.nanoTime() - s;
            }
        }

        long total = tShuffle + tDeal + tExtractAndScore;
        log.info("[Benchmark] Breakdown ({} iters): shuffle={}ms ({}%), deal={}ms ({}%), extract+score={}ms ({}%)",
            iterations,
            String.format("%.1f", tShuffle / 1e6), String.format("%.1f", 100.0 * tShuffle / total),
            String.format("%.1f", tDeal / 1e6), String.format("%.1f", 100.0 * tDeal / total),
            String.format("%.1f", tExtractAndScore / 1e6), String.format("%.1f", 100.0 * tExtractAndScore / total));
    }
}
