package com.mamba.landlord.algorithm;

import com.mamba.landlord.algorithm.scoring.strategy.DefaultComboScoringStrategy;
import com.mamba.landlord.algorithm.scoring.strategy.DefaultHandCardsScoringStrategy;
import com.mamba.landlord.algorithm.splitter.DefaultComboExtractor;
import com.mamba.landlord.algorithm.splitter.DefaultSplitterFactory;
import com.mamba.landlord.algorithm.utils.HandCardUtils;
import com.mamba.landlord.core.model.Card;
import com.mamba.landlord.core.model.Combo;
import com.mamba.landlord.core.model.ComboType;
import com.mamba.landlord.core.model.Deck;
import com.mamba.landlord.core.model.DealData;
import com.mamba.landlord.core.model.Rank;
import com.mamba.landlord.core.model.Suit;
import com.mamba.landlord.core.properties.ScoringStrategyProperties;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

/**
 * 边界条件与潜在 Bug 的单元测试。
 * 覆盖 null、空集合、非法索引、除零、异常配置等场景。
 */
class BoundaryBugTest {

    @Test
    @DisplayName("ComboScoring: null Combo 应抛异常")
    void comboScoringNullCombo() {
        DefaultComboScoringStrategy scorer = new DefaultComboScoringStrategy();
        assertThrows(IllegalArgumentException.class, () -> scorer.score(null));
    }

    @Test
    @DisplayName("ComboScoring: 空 mainRanks 的牌型应返回 0 或安全值")
    void comboScoringEmptyMainRanks() {
        DefaultComboScoringStrategy scorer = new DefaultComboScoringStrategy();
        Combo emptyPair = new Combo(ComboType.PAIR, Collections.emptyList(), null);
        assertEquals(0.0, scorer.score(emptyPair), 0.001);

        Combo emptyBomb = new Combo(ComboType.BOMB, Collections.emptyList(), null);
        assertEquals(0.0, scorer.score(emptyBomb), 0.001);
    }

    @Test
    @DisplayName("HandScoring: null handCards 返回 0")
    void handScoringNullHandCards() {
        DefaultHandCardsScoringStrategy scorer = new DefaultHandCardsScoringStrategy();
        assertEquals(0.0, scorer.calcTotalHandScore(null, List.of(Combo.single(Rank.ACE))));
    }

    @Test
    @DisplayName("HandScoring: null combos 返回 0")
    void handScoringNullCombos() {
        DefaultHandCardsScoringStrategy scorer = new DefaultHandCardsScoringStrategy();
        assertEquals(0.0, scorer.calcTotalHandScore(List.of(new Card(Rank.ACE, Suit.SPADE)), null));
    }

    @Test
    @DisplayName("HandScoring: 空 handCards 返回 0")
    void handScoringEmptyHandCards() {
        DefaultHandCardsScoringStrategy scorer = new DefaultHandCardsScoringStrategy();
        assertEquals(0.0, scorer.calcTotalHandScore(Collections.emptyList(), List.of(Combo.single(Rank.ACE))));
    }

    @Test
    @DisplayName("HandScoring: 空 combos 返回 0")
    void handScoringEmptyCombos() {
        DefaultHandCardsScoringStrategy scorer = new DefaultHandCardsScoringStrategy();
        assertEquals(0.0, scorer.calcTotalHandScore(List.of(new Card(Rank.ACE, Suit.SPADE)), Collections.emptyList()));
    }

    @Test
    @DisplayName("HandCardUtils: null handCards 应抛异常")
    void handCardUtilsNullHandCards() {
        assertThrows(IllegalArgumentException.class, () -> HandCardUtils.buildRankCounts(null));
    }

    @Test
    @DisplayName("HandCardUtils: 空 handCards 返回全 0 数组")
    void handCardUtilsEmptyHandCards() {
        int[] count = HandCardUtils.buildRankCounts(Collections.emptyList());
        assertNotNull(count);
        assertEquals(Rank.values().length, count.length);
        for (int c : count) assertEquals(0, c);
    }

    @Test
    @DisplayName("DefaultSplitterFactory: null handCards 应抛异常")
    void splitterFactoryNullHandCards() {
        assertThrows(IllegalArgumentException.class, () -> DefaultSplitterFactory.extractAllCombos(null));
    }

    @Test
    @DisplayName("DefaultSplitterFactory: 空 handCards 应返回空组合列表")
    void splitterFactoryEmptyHandCards() {
        List<Combo> combos = DefaultSplitterFactory.extractAllCombos(Collections.emptyList());
        assertNotNull(combos);
        assertTrue(combos.isEmpty());
    }

    @Test
    @DisplayName("DefaultComboExtractor: null handCards 应抛异常")
    void comboExtractorNullHandCards() {
        DefaultComboExtractor extractor = new DefaultComboExtractor();
        assertThrows(IllegalArgumentException.class, () -> extractor.extractAllCombos(null));
    }

    @Test
    @DisplayName("DefaultComboExtractor: 空 handCards 应返回空组合列表")
    void comboExtractorEmptyHandCards() {
        DefaultComboExtractor extractor = new DefaultComboExtractor();
        List<Combo> combos = extractor.extractAllCombos(Collections.emptyList());
        assertNotNull(combos);
        assertTrue(combos.isEmpty());
    }

    @Test
    @DisplayName("DealData: 非法 seat 应抛异常")
    void dealDataInvalidSeat() {
        List<Card> hand = Deck.copyFullDeckCards().subList(0, 17);
        DealData data = DealData.builder()
            .handCardsPlayer0(hand)
            .handCardsPlayer1(hand)
            .handCardsPlayer2(hand)
            .bottomCards(Deck.copyFullDeckCards().subList(51, 54))
            .build();
        assertThrows(IllegalArgumentException.class, () -> data.getHandCards(-1));
        assertThrows(IllegalArgumentException.class, () -> data.getHandCards(3));
    }

    @Test
    @DisplayName("dealCards: 牌数不足 54 应抛异常")
    void dealCardsWrongSize() {
        var strategy = new com.mamba.landlord.algorithm.shuffle.strategy.DefaultShuffleDealStrategy();
        List<Card> shortDeck = Deck.copyFullDeckCards().subList(0, 50);
        assertThrows(IllegalArgumentException.class, () -> strategy.dealCards(shortDeck));
    }

    @Test
    @DisplayName("dealCards: null 应抛异常")
    void dealCardsNull() {
        var strategy = new com.mamba.landlord.algorithm.shuffle.strategy.DefaultShuffleDealStrategy();
        assertThrows(IllegalArgumentException.class, () -> strategy.dealCards(null));
    }

    @Test
    @DisplayName("ScoringStrategyProperties: getRankBaseValue 非法 index 应抛异常")
    void scoringPropertiesInvalidIndex() {
        ScoringStrategyProperties props = new ScoringStrategyProperties();
        assertThrows(IndexOutOfBoundsException.class, () -> props.getRankBaseValue(-1));
        assertThrows(IndexOutOfBoundsException.class, () -> props.getRankBaseValue(15));
    }

    @Test
    @DisplayName("ScoringStrategyProperties: getRankBaseValue 合法 index 不抛异常")
    void scoringPropertiesValidIndex() {
        ScoringStrategyProperties props = new ScoringStrategyProperties();
        assertDoesNotThrow(() -> props.getRankBaseValue(0));
        assertEquals(-7, props.getRankBaseValue(0));
        assertEquals(18, props.getRankBaseValue(14));
    }

    @Test
    @DisplayName("ScoringStrategyProperties: 非法 rankBaseValues 字符串应抛异常")
    void scoringPropertiesMalformedRankBaseValues() {
        ScoringStrategyProperties props = new ScoringStrategyProperties();
        props.setRankBaseValues("1,2,3");
        assertThrows(IllegalStateException.class, () -> props.getRankBaseValue(0));
    }

    @Test
    @DisplayName("DefaultSplitterFactory: rankCounts 长度不足应抛异常")
    void splitterFactoryShortRankCounts() {
        int[] shortCounts = new int[5];
        assertThrows(ArrayIndexOutOfBoundsException.class,
            () -> DefaultSplitterFactory.getSplitter(shortCounts));
    }

    @Test
    @DisplayName("EHS: ehsScale=0 时发牌流程应不产生 NaN")
    void ehsScaleZeroNoNan() {
        ScoringStrategyProperties props = new ScoringStrategyProperties();
        props.setEhsScale(0.0);
        com.mamba.landlord.core.holder.ScoringStrategyHolder.set(props);

        com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties shuffleProps =
            new com.mamba.landlord.core.properties.ShuffleStrategyDecisionProperties();
        shuffleProps.setEnabled(false);
        com.mamba.landlord.core.holder.ShuffleStrategyDecisionHolder.set(shuffleProps);

        var strategy = new com.mamba.landlord.algorithm.shuffle.strategy.DefaultReshuffleDealStrategy();
        var result = strategy.shuffleAndDeal();

        assertFalse(Double.isNaN(result.getEhsPlayer0()));
        assertFalse(Double.isNaN(result.getEhsPlayer1()));
        assertFalse(Double.isNaN(result.getEhsPlayer2()));
        assertTrue(result.getEhsPlayer0() >= 0 && result.getEhsPlayer0() <= 1);
        assertTrue(result.getEhsPlayer1() >= 0 && result.getEhsPlayer1() <= 1);
        assertTrue(result.getEhsPlayer2() >= 0 && result.getEhsPlayer2() <= 1);
    }
}
