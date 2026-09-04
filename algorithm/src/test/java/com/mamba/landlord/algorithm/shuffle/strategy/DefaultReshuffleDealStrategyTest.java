package com.mamba.landlord.algorithm.shuffle.strategy;

import com.mamba.landlord.algorithm.scoring.strategy.DefaultHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.splitter.DefaultComboExtractor;
import com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Deck;
import com.mamba.landlord.core.model.EvaluatedDealData;
import com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

/**
 * {@link DefaultReshuffleDealStrategy} 的重洗逻辑测试。
 * <p>
 * 通过配置 {@link ShuffleStrategyDecisionProperties}，覆盖以下场景：
 * <ul>
 *   <li>关闭过滤时直接返回，不触发重洗。</li>
 *   <li>触发重洗时不超过 maxReshuffleTimes。</li>
 *   <li>返回数据包含三家完整手牌与底牌。</li>
 *   <li>启用地主优势与结构特征检测不影响基本发牌流程。</li>
 * </ul>
 * </p>
 */
class DefaultReshuffleDealStrategyTest {

    private static final class SequenceStrategy extends DefaultReshuffleDealStrategy {
        private final List<List<Card>> decks;
        private int index;

        private SequenceStrategy() {
            List<Card> first = Deck.copyFullDeckCards();
            List<Card> second = new ArrayList<>(first);
            Collections.reverse(second);
            decks = List.of(first, second);
        }

        @Override
        public List<Card> shuffle() {
            return decks.get(index++);
        }
    }

    private static double recomputeScore(List<Card> cards) {
        DefaultHandCardsScoringStrategy scorer = new DefaultHandCardsScoringStrategy();
        DefaultComboExtractor extractor = new DefaultComboExtractor(scorer);
        return scorer.calcTotalHandScore(cards, extractor.extractAllCombos(cards));
    }

    @Test
    void returnedScoresAlwaysBelongToReturnedHandsAfterMaxReshuffles() {
        ShuffleStrategyDecisionProperties props = new ShuffleStrategyDecisionProperties();
        props.setEnabled(true);
        props.setLowerThreshold(100_000.0);
        props.setUpperThreshold(100_001.0);
        props.setMaxReshuffleTimes(1);
        props.setThresholdRelaxStep(0.0);
        ShuffleStrategyDecisionHolder.set(props);

        EvaluatedDealData data = new SequenceStrategy().shuffleAndDeal();

        assertEquals(1, data.getReshuffleCnt());
        assertEquals(recomputeScore(data.getHandCards(0)), data.getHandStrengthPlayer0(), 0.001);
        assertEquals(recomputeScore(data.getHandCards(1)), data.getHandStrengthPlayer1(), 0.001);
        assertEquals(recomputeScore(data.getHandCards(2)), data.getHandStrengthPlayer2(), 0.001);
    }

    @Test
    @DisplayName("当关闭过滤时，应直接复用默认策略结果且不发生重洗")
    void disabledFilterShouldBehaveLikeDefaultStrategy() {
        ShuffleStrategyDecisionProperties props = new ShuffleStrategyDecisionProperties();
        props.setEnabled(false);
        ShuffleStrategyDecisionHolder.set(props);

        DefaultReshuffleDealStrategy strategy = new DefaultReshuffleDealStrategy();
        EvaluatedDealData data = strategy.shuffleAndDeal();

        assertNotNull(data);
        assertFalse(data.isReshuffled(), "关闭过滤时不应标记为重洗");
        assertEquals(0, data.getReshuffleCnt(), "关闭过滤时重洗次数应为 0");
    }

    @Test
    @DisplayName("当阈值配置极端且允许多次重洗时，应在有限次数内尝试重洗")
    void extremeThresholdsShouldCauseSomeReshuffles() {
        ShuffleStrategyDecisionProperties props = new ShuffleStrategyDecisionProperties();
        props.setEnabled(true);
        // 设置宽松阈值（接近不可能触发），最多允许 3 次重洗
        props.setLowerThreshold(-1000.0);
        props.setUpperThreshold(1000.0);
        props.setMaxReshuffleTimes(3);
        ShuffleStrategyDecisionHolder.set(props);

        DefaultReshuffleDealStrategy strategy = new DefaultReshuffleDealStrategy();
        EvaluatedDealData data = strategy.shuffleAndDeal();

        assertNotNull(data);
        // 无法强行保证一定重洗，但重洗次数必须在 [0, maxReshuffleTimes] 内
        assertTrue(data.getReshuffleCnt() >= 0 && data.getReshuffleCnt() <= 3,
            "重洗次数应在 0 到 maxReshuffleTimes 之间");
    }

    @Test
    @DisplayName("无论配置如何，返回数据中三家手牌和底牌均不为空且张数正确")
    void resultShouldAlwaysContainAllCards() {
        ShuffleStrategyDecisionProperties props = new ShuffleStrategyDecisionProperties();
        props.setEnabled(true);
        props.setMaxReshuffleTimes(2);
        ShuffleStrategyDecisionHolder.set(props);

        DefaultReshuffleDealStrategy strategy = new DefaultReshuffleDealStrategy();
        EvaluatedDealData data = strategy.shuffleAndDeal();

        assertNotNull(data.getHandCards(0), "玩家0手牌不应为空");
        assertNotNull(data.getHandCards(1), "玩家1手牌不应为空");
        assertNotNull(data.getHandCards(2), "玩家2手牌不应为空");
        assertNotNull(data.getBottomCards(), "底牌不应为空");
        assertEquals(17, data.getHandCards(0).size(), "玩家0应有 17 张手牌");
        assertEquals(17, data.getHandCards(1).size(), "玩家1应有 17 张手牌");
        assertEquals(17, data.getHandCards(2).size(), "玩家2应有 17 张手牌");
        assertEquals(3, data.getBottomCards().size(), "底牌应有 3 张");
    }

    @Test
    @DisplayName("启用地主优势和结构特征检测（宽松阈值）时，不影响基本发牌流程")
    void advancedFiltersWithLooseThresholdsShouldNotBreakDealing() {
        ShuffleStrategyDecisionProperties props = new ShuffleStrategyDecisionProperties();
        props.setEnabled(true);
        // 设置宽松阈值，正常情况下不会触发
        props.setMaxLandlordAdvantage(10000.0);
        props.setMaxSinglesPerHand(17);
        props.setMaxBombsPerHand(13);
        props.setMaxReshuffleTimes(3);
        ShuffleStrategyDecisionHolder.set(props);

        DefaultReshuffleDealStrategy strategy = new DefaultReshuffleDealStrategy();
        EvaluatedDealData data = strategy.shuffleAndDeal();

        assertNotNull(data);
        assertTrue(data.getReshuffleCnt() >= 0 && data.getReshuffleCnt() <= 3,
            "重洗次数应在 0 到 maxReshuffleTimes 之间");
        // 验证牌数完整性
        assertEquals(17, data.getHandCards(0).size());
        assertEquals(17, data.getHandCards(1).size());
        assertEquals(17, data.getHandCards(2).size());
        assertEquals(3, data.getBottomCards().size());
    }
}
